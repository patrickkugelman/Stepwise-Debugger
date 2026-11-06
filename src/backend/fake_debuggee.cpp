#include <stepwise/backend/fake_debuggee.hpp>

#include <algorithm>
#include <csignal>
#include <cstring>
#include <format>
#include <utility>

namespace stepwise::backend {
namespace {

constexpr Pid fake_pid{1000};
constexpr std::uint64_t initial_stack_pointer = 0x7fffffffe000;
constexpr std::uint64_t initial_eflags = 0x202;

Error no_process_error() {
    return Error{ErrorCode::InvalidState, "no traced process"};
}

Error unmapped_error(std::string_view verb, Address address) {
    return Error{ErrorCode::Os,
                 std::format("cannot {} memory at {:#x}: Input/output error", verb, address.value())};
}

}  // namespace

FakeProgram demo_fake_program() {
    using namespace fake_opcode;
    return FakeProgram{
        Address{0x401000},
        {
            FakeRegion{Address{0x401000},
                       {inc_rax, inc_rax, inc_rax, inc_rax, inc_rax, nop, nop, nop, ret}},
            FakeRegion{Address{0x404000}, std::vector<std::byte>(64)},
        },
    };
}

FakeDebuggee::FakeDebuggee(FakeProgram program) : program_(std::move(program)) {}

Result<Pid> FakeDebuggee::launch(const LaunchSpec&) {
    if (live_) {
        return Error{ErrorCode::InvalidState, "a process is already being traced"};
    }
    Registers registers;
    registers.rip = program_.entry.value();
    registers.rsp = initial_stack_pointer;
    registers.eflags = initial_eflags;
    live_ = Live{registers, program_.regions};
    fault_address_.reset();
    return fake_pid;
}

Result<DebugEvent> FakeDebuggee::cont(std::optional<Signal> deliver) {
    return run(false, deliver);
}

Result<DebugEvent> FakeDebuggee::step(std::optional<Signal> deliver) {
    return run(true, deliver);
}

Result<Registers> FakeDebuggee::read_registers() {
    if (!live_) {
        return no_process_error();
    }
    return live_->registers;
}

Result<void> FakeDebuggee::write_registers(const Registers& registers) {
    if (!live_) {
        return no_process_error();
    }
    live_->registers = registers;
    return {};
}

FakeRegion* FakeDebuggee::locate(Address address, std::size_t length) {
    for (FakeRegion& region : live_->memory) {
        if (address < region.base) {
            continue;
        }
        const std::uint64_t offset = address.value() - region.base.value();
        if (offset <= region.bytes.size() && length <= region.bytes.size() - offset) {
            return &region;
        }
    }
    return nullptr;
}

Result<void> FakeDebuggee::read_memory(Address address, std::span<std::byte> out) {
    if (!live_) {
        return no_process_error();
    }
    const FakeRegion* region = locate(address, out.size());
    if (region == nullptr) {
        return unmapped_error("read", address);
    }
    const auto offset = static_cast<std::size_t>(address.value() - region->base.value());
    std::copy_n(region->bytes.begin() + static_cast<std::ptrdiff_t>(offset), out.size(),
                out.begin());
    return {};
}

Result<void> FakeDebuggee::write_memory(Address address, std::span<const std::byte> data) {
    if (!live_) {
        return no_process_error();
    }
    FakeRegion* region = locate(address, data.size());
    if (region == nullptr) {
        return unmapped_error("write", address);
    }
    const auto offset = static_cast<std::size_t>(address.value() - region->base.value());
    std::ranges::copy(data, region->bytes.begin() + static_cast<std::ptrdiff_t>(offset));
    return {};
}

Result<std::uint64_t> FakeDebuggee::peek(Address address) {
    std::uint64_t word = 0;
    if (auto read = read_memory(address, std::as_writable_bytes(std::span{&word, 1})); !read) {
        return read.error();
    }
    return word;
}

Result<void> FakeDebuggee::poke(Address address, std::uint64_t word) {
    return write_memory(address, std::as_bytes(std::span{&word, 1}));
}

Result<Address> FakeDebuggee::fault_address() {
    if (!live_ || !fault_address_) {
        return Error{ErrorCode::InvalidState, "the process is not stopped on a fault"};
    }
    return *fault_address_;
}

Result<DebugEvent> FakeDebuggee::run(bool single_step, std::optional<Signal> deliver) {
    if (!live_) {
        return no_process_error();
    }
    if (deliver && *deliver != Signal{SIGTRAP}) {
        live_.reset();
        return ProcessKilled{*deliver};
    }

    Registers& registers = live_->registers;
    while (true) {
        std::byte opcode{};
        if (!read_memory(Address{registers.rip}, std::span<std::byte>{&opcode, 1})) {
            fault_address_ = Address{registers.rip};
            return ProcessStopped{Signal{SIGSEGV}};
        }

        if (opcode == fake_opcode::int3) {
            registers.rip += 1;
            return ProcessStopped{Signal{SIGTRAP}};
        }
        if (opcode == fake_opcode::ret) {
            const int code = static_cast<int>(registers.rax & 0xff);
            live_.reset();
            return ProcessExited{code};
        }
        if (opcode == fake_opcode::inc_rax) {
            registers.rax += 1;
        } else if (opcode != fake_opcode::nop) {
            return ProcessStopped{Signal{SIGILL}};
        }
        registers.rip += 1;

        if (single_step) {
            return ProcessStopped{Signal{SIGTRAP}};
        }
    }
}

}  // namespace stepwise::backend
