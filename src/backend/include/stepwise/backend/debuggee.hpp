#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <stepwise/backend/registers.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/base/types.hpp>

namespace stepwise::backend {

struct LaunchSpec {
    std::string program;
    std::vector<std::string> args;
    bool stdout_to_stderr{false};
};

struct ProcessStopped {
    Signal signal;
};

struct ProcessExited {
    int exit_code;
};

struct ProcessKilled {
    Signal signal;
};

using DebugEvent = std::variant<ProcessStopped, ProcessExited, ProcessKilled>;

class Debuggee {
public:
    virtual ~Debuggee() = default;

    Debuggee(const Debuggee&) = delete;
    Debuggee& operator=(const Debuggee&) = delete;

    virtual Result<Pid> launch(const LaunchSpec& spec) = 0;

    virtual Result<DebugEvent> cont(std::optional<Signal> deliver) = 0;
    virtual Result<DebugEvent> step(std::optional<Signal> deliver) = 0;

    virtual Result<Registers> read_registers() = 0;
    virtual Result<void> write_registers(const Registers& registers) = 0;

    virtual Result<void> read_memory(Address address, std::span<std::byte> out) = 0;
    virtual Result<void> write_memory(Address address, std::span<const std::byte> data) = 0;

    virtual Result<std::uint64_t> peek(Address address) = 0;
    virtual Result<void> poke(Address address, std::uint64_t word) = 0;

    virtual Result<Address> fault_address() = 0;

protected:
    Debuggee() = default;
};

}  // namespace stepwise::backend
