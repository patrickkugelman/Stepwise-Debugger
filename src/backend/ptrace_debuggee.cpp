#include <stepwise/backend/ptrace_debuggee.hpp>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace stepwise::backend {
namespace {

static_assert(std::is_same_v<pid_t, Pid::rep>);

class UniqueFd {
public:
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    ~UniqueFd() { reset(); }

    UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

    void reset() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
};

struct Pipe {
    UniqueFd read_end;
    UniqueFd write_end;
};

Result<Pipe> make_cloexec_pipe() {
    std::array<int, 2> fds{};
    if (::pipe2(fds.data(), O_CLOEXEC) != 0) {
        return Error::from_errno("pipe2", errno);
    }
    return Pipe{UniqueFd{fds[0]}, UniqueFd{fds[1]}};
}

// child side, between fork and exec: async-signal-safe calls only.
// errors go back to the parent through the cloexec pipe
[[noreturn]] void exec_child(const char* path, char* const* argv, int error_fd, bool stdout_to_stderr) {
    const int persona = ::personality(0xffffffff);
    if (persona != -1 && ::personality(static_cast<unsigned long>(persona) | ADDR_NO_RANDOMIZE) != -1 &&
        (!stdout_to_stderr || ::dup2(STDERR_FILENO, STDOUT_FILENO) != -1) &&
        ::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != -1) {
        ::execv(path, argv);
    }
    const int error_number = errno;
    [[maybe_unused]] const auto written = ::write(error_fd, &error_number, sizeof error_number);
    ::_exit(127);
}

Result<void> read_exec_status(int fd, const std::string& program) {
    int child_errno = 0;
    ssize_t count = 0;
    do {
        count = ::read(fd, &child_errno, sizeof child_errno);
    } while (count == -1 && errno == EINTR);

    if (count == -1) {
        return Error::from_errno("read exec status", errno);
    }
    if (count == 0) {
        return {};
    }
    if (count != sizeof child_errno) {
        return Error{ErrorCode::Protocol, "short read on exec status pipe"};
    }
    return Error::from_errno(std::format("cannot start '{}'", program), child_errno);
}

struct RegisterMapping {
    std::uint64_t Registers::*ours;
    unsigned long long user_regs_struct::*kernel;
};

constexpr auto register_mappings = std::to_array<RegisterMapping>({
    {&Registers::rax, &user_regs_struct::rax},
    {&Registers::rbx, &user_regs_struct::rbx},
    {&Registers::rcx, &user_regs_struct::rcx},
    {&Registers::rdx, &user_regs_struct::rdx},
    {&Registers::rsi, &user_regs_struct::rsi},
    {&Registers::rdi, &user_regs_struct::rdi},
    {&Registers::rbp, &user_regs_struct::rbp},
    {&Registers::rsp, &user_regs_struct::rsp},
    {&Registers::r8, &user_regs_struct::r8},
    {&Registers::r9, &user_regs_struct::r9},
    {&Registers::r10, &user_regs_struct::r10},
    {&Registers::r11, &user_regs_struct::r11},
    {&Registers::r12, &user_regs_struct::r12},
    {&Registers::r13, &user_regs_struct::r13},
    {&Registers::r14, &user_regs_struct::r14},
    {&Registers::r15, &user_regs_struct::r15},
    {&Registers::rip, &user_regs_struct::rip},
    {&Registers::eflags, &user_regs_struct::eflags},
    {&Registers::orig_rax, &user_regs_struct::orig_rax},
    {&Registers::cs, &user_regs_struct::cs},
    {&Registers::ss, &user_regs_struct::ss},
    {&Registers::ds, &user_regs_struct::ds},
    {&Registers::es, &user_regs_struct::es},
    {&Registers::fs, &user_regs_struct::fs},
    {&Registers::gs, &user_regs_struct::gs},
    {&Registers::fs_base, &user_regs_struct::fs_base},
    {&Registers::gs_base, &user_regs_struct::gs_base},
});

Error no_process_error() {
    return Error{ErrorCode::InvalidState, "no traced process"};
}

Result<UniqueFd> open_process_memory(Pid pid, int flags) {
    const std::string path = std::format("/proc/{}/mem", pid.value());
    UniqueFd fd{::open(path.c_str(), flags | O_CLOEXEC)};
    if (fd.get() == -1) {
        return Error::from_errno(std::format("open {}", path), errno);
    }
    return fd;
}

}  // namespace

TracedProcess::~TracedProcess() {
    if (!pid_) {
        return;
    }
    ::kill(pid_->value(), SIGKILL);
    while (::waitpid(pid_->value(), nullptr, 0) == -1 && errno == EINTR) {
    }
}

Result<Pid> PtraceDebuggee::launch(const LaunchSpec& spec) {
    if (tracee_) {
        return Error{ErrorCode::InvalidState, "a process is already being traced"};
    }

    auto pipe = make_cloexec_pipe();
    if (!pipe) {
        return pipe.error();
    }

    std::vector<std::string> arguments{spec.program};
    arguments.insert(arguments.end(), spec.args.begin(), spec.args.end());
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid == -1) {
        return Error::from_errno("fork", errno);
    }
    if (pid == 0) {
        exec_child(spec.program.c_str(), argv.data(), pipe.value().write_end.get(),
                   spec.stdout_to_stderr);
    }

    tracee_.emplace(Pid{pid});
    pipe.value().write_end.reset();

    if (auto status = read_exec_status(pipe.value().read_end.get(), spec.program); !status) {
        tracee_.reset();
        return status.error();
    }

    auto event = wait_for_event();
    if (!event) {
        tracee_.reset();
        return event.error();
    }
    const auto* stop = std::get_if<ProcessStopped>(&event.value());
    if (stop == nullptr || stop->signal != Signal{SIGTRAP}) {
        tracee_.reset();
        return Error{ErrorCode::Protocol,
                     std::format("'{}' did not stop at exec as expected", spec.program)};
    }

    if (::ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_EXITKILL) == -1) {
        const int error_number = errno;
        tracee_.reset();
        return Error::from_errno("PTRACE_SETOPTIONS", error_number);
    }
    return Pid{pid};
}

Result<DebugEvent> PtraceDebuggee::cont(std::optional<Signal> deliver) {
    return resume(PTRACE_CONT, deliver);
}

Result<DebugEvent> PtraceDebuggee::step(std::optional<Signal> deliver) {
    return resume(PTRACE_SINGLESTEP, deliver);
}

Result<Registers> PtraceDebuggee::read_registers() {
    if (!tracee_) {
        return no_process_error();
    }
    user_regs_struct kernel_regs{};
    if (::ptrace(PTRACE_GETREGS, tracee_->pid().value(), nullptr, &kernel_regs) == -1) {
        return Error::from_errno("PTRACE_GETREGS", errno);
    }
    Registers registers;
    for (const auto& [ours, kernel] : register_mappings) {
        registers.*ours = kernel_regs.*kernel;
    }
    return registers;
}

Result<void> PtraceDebuggee::write_registers(const Registers& registers) {
    if (!tracee_) {
        return no_process_error();
    }
    const pid_t pid = tracee_->pid().value();
    user_regs_struct kernel_regs{};
    if (::ptrace(PTRACE_GETREGS, pid, nullptr, &kernel_regs) == -1) {
        return Error::from_errno("PTRACE_GETREGS", errno);
    }
    for (const auto& [ours, kernel] : register_mappings) {
        kernel_regs.*kernel = registers.*ours;
    }
    if (::ptrace(PTRACE_SETREGS, pid, nullptr, &kernel_regs) == -1) {
        return Error::from_errno("PTRACE_SETREGS", errno);
    }
    return {};
}

Result<void> PtraceDebuggee::read_memory(Address address, std::span<std::byte> out) {
    if (!tracee_) {
        return no_process_error();
    }
    auto fd = open_process_memory(tracee_->pid(), O_RDONLY);
    if (!fd) {
        return fd.error();
    }
    std::size_t done = 0;
    while (done < out.size()) {
        const ssize_t count = ::pread(fd.value().get(), out.data() + done, out.size() - done,
                                      static_cast<off_t>(address.value() + done));
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count == -1) {
            return Error::from_errno(
                std::format("cannot read memory at {:#x}", address.value() + done), errno);
        }
        if (count == 0) {
            return Error{ErrorCode::Os,
                         std::format("cannot read memory at {:#x}: unexpected end of data",
                                     address.value() + done)};
        }
        done += static_cast<std::size_t>(count);
    }
    return {};
}

Result<void> PtraceDebuggee::write_memory(Address address, std::span<const std::byte> data) {
    if (!tracee_) {
        return no_process_error();
    }
    auto fd = open_process_memory(tracee_->pid(), O_WRONLY);
    if (!fd) {
        return fd.error();
    }
    std::size_t done = 0;
    while (done < data.size()) {
        const ssize_t count = ::pwrite(fd.value().get(), data.data() + done, data.size() - done,
                                       static_cast<off_t>(address.value() + done));
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count == -1) {
            return Error::from_errno(
                std::format("cannot write memory at {:#x}", address.value() + done), errno);
        }
        if (count == 0) {
            return Error{ErrorCode::Os,
                         std::format("cannot write memory at {:#x}: no progress",
                                     address.value() + done)};
        }
        done += static_cast<std::size_t>(count);
    }
    return {};
}

Result<std::uint64_t> PtraceDebuggee::peek(Address address) {
    if (!tracee_) {
        return no_process_error();
    }
    // -1 is a valid word, so check errno
    errno = 0;
    const long word = ::ptrace(PTRACE_PEEKDATA, tracee_->pid().value(),
                               reinterpret_cast<void*>(address.value()), nullptr);
    if (word == -1 && errno != 0) {
        return Error::from_errno(std::format("PTRACE_PEEKDATA at {:#x}", address.value()), errno);
    }
    return static_cast<std::uint64_t>(word);
}

Result<void> PtraceDebuggee::poke(Address address, std::uint64_t word) {
    if (!tracee_) {
        return no_process_error();
    }
    if (::ptrace(PTRACE_POKEDATA, tracee_->pid().value(), reinterpret_cast<void*>(address.value()),
                 reinterpret_cast<void*>(word)) == -1) {
        return Error::from_errno(std::format("PTRACE_POKEDATA at {:#x}", address.value()), errno);
    }
    return {};
}

Result<Address> PtraceDebuggee::fault_address() {
    if (!tracee_) {
        return no_process_error();
    }
    siginfo_t info{};
    if (::ptrace(PTRACE_GETSIGINFO, tracee_->pid().value(), nullptr, &info) == -1) {
        return Error::from_errno("PTRACE_GETSIGINFO", errno);
    }
    return Address{reinterpret_cast<std::uintptr_t>(info.si_addr)};
}

Result<DebugEvent> PtraceDebuggee::resume(int request, std::optional<Signal> deliver) {
    if (!tracee_) {
        return no_process_error();
    }
    const auto signal_number = static_cast<std::intptr_t>(deliver ? deliver->value() : 0);
    if (::ptrace(static_cast<__ptrace_request>(request), tracee_->pid().value(), nullptr,
                 reinterpret_cast<void*>(signal_number)) == -1) {
        return Error::from_errno("ptrace resume", errno);
    }
    return wait_for_event();
}

Result<DebugEvent> PtraceDebuggee::wait_for_event() {
    int status = 0;
    while (::waitpid(tracee_->pid().value(), &status, 0) == -1) {
        if (errno != EINTR) {
            return Error::from_errno("waitpid", errno);
        }
    }

    if (WIFSTOPPED(status)) {
        return ProcessStopped{Signal{WSTOPSIG(status)}};
    }

    tracee_->mark_reaped();
    tracee_.reset();
    if (WIFEXITED(status)) {
        return ProcessExited{WEXITSTATUS(status)};
    }
    return ProcessKilled{Signal{WTERMSIG(status)}};
}

}  // namespace stepwise::backend
