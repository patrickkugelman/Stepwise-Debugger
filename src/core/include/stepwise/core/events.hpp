#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include <stepwise/backend/registers.hpp>
#include <stepwise/base/types.hpp>
#include <stepwise/core/location.hpp>

namespace stepwise::core {

enum class StopReason { Entry, SingleStep, LineStep, Breakpoint, Trap, Signal };

[[nodiscard]] std::string_view to_string(StopReason reason) noexcept;

struct StoppedEvent {
    Pid pid;
    Address pc;
    StopReason reason;
    CodeLocation where;
};

struct BreakpointHitEvent {
    Pid pid;
    BreakpointId id;
    Address pc;
    std::uint64_t hit_count;
};

struct SignalReceivedEvent {
    Pid pid;
    Signal signal;
};

struct CrashReportEvent {
    Pid pid;
    Signal signal;
    std::optional<Address> fault_address;
    backend::Registers registers;
    CodeLocation where;
    std::vector<Frame> backtrace;
};

struct ExitedEvent {
    Pid pid;
    std::variant<int, Signal> status;
};

using Event = std::variant<StoppedEvent, BreakpointHitEvent, SignalReceivedEvent, CrashReportEvent,
                           ExitedEvent>;

}  // namespace stepwise::core
