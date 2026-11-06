#include <stepwise/core/events.hpp>

namespace stepwise::core {

std::string_view to_string(StopReason reason) noexcept {
    switch (reason) {
        case StopReason::Entry:
            return "entry";
        case StopReason::SingleStep:
            return "single step";
        case StopReason::LineStep:
            return "step";
        case StopReason::Breakpoint:
            return "breakpoint";
        case StopReason::Trap:
            return "trap";
        case StopReason::Signal:
            return "signal";
    }
    return "unknown";
}

}  // namespace stepwise::core
