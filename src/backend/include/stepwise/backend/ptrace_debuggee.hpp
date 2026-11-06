#pragma once

#include <optional>

#include <stepwise/backend/debuggee.hpp>

namespace stepwise::backend {

class TracedProcess {
public:
    explicit TracedProcess(Pid pid) noexcept : pid_(pid) {}
    ~TracedProcess();

    TracedProcess(const TracedProcess&) = delete;
    TracedProcess& operator=(const TracedProcess&) = delete;

    [[nodiscard]] Pid pid() const noexcept { return *pid_; }

    void mark_reaped() noexcept { pid_.reset(); }

private:
    std::optional<Pid> pid_;
};

class PtraceDebuggee final : public Debuggee {
public:
    PtraceDebuggee() = default;

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
    Result<DebugEvent> resume(int request, std::optional<Signal> deliver);
    Result<DebugEvent> wait_for_event();

    std::optional<TracedProcess> tracee_;
};

}  // namespace stepwise::backend
