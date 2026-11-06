#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <stepwise/backend/debuggee.hpp>

namespace stepwise::backend {

namespace fake_opcode {
inline constexpr std::byte inc_rax{0x40};
inline constexpr std::byte nop{0x90};
inline constexpr std::byte int3{0xCC};
inline constexpr std::byte ret{0xC3};
}  // namespace fake_opcode

struct FakeRegion {
    Address base;
    std::vector<std::byte> bytes;
};

struct FakeProgram {
    Address entry;
    std::vector<FakeRegion> regions;
};

[[nodiscard]] FakeProgram demo_fake_program();

class FakeDebuggee final : public Debuggee {
public:
    explicit FakeDebuggee(FakeProgram program);

    Result<Pid> launch(const LaunchSpec& spec) override;
    Result<DebugEvent> cont(std::optional<Signal> deliver) override;
    Result<DebugEvent> step(std::optional<Signal> deliver) override;
    Result<Registers> read_registers() override;
    Result<void> write_registers(const Registers& registers) override;
    Result<void> read_memory(Address address, std::span<std::byte> out) override;
    Result<void> write_memory(Address address, std::span<const std::byte> data) override;
    Result<std::uint64_t> peek(Address address) override;
    Result<void> poke(Address address, std::uint64_t word) override;
    Result<Address> fault_address() override;

private:
    struct Live {
        Registers registers;
        std::vector<FakeRegion> memory;
    };

    Result<DebugEvent> run(bool single_step, std::optional<Signal> deliver);
    FakeRegion* locate(Address address, std::size_t length);

    FakeProgram program_;
    std::optional<Live> live_;
    std::optional<Address> fault_address_;
};

}  // namespace stepwise::backend
