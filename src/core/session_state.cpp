#include <stepwise/core/session_state.hpp>

#include <format>

namespace stepwise::core {

std::string_view to_string(SessionState state) noexcept {
    switch (state) {
        case SessionState::NotStarted:
            return "NotStarted";
        case SessionState::Stopped:
            return "Stopped";
        case SessionState::Running:
            return "Running";
        case SessionState::Exited:
            return "Exited";
    }
    return "Unknown";
}

bool SessionStateMachine::can_transition_to(SessionState next) const noexcept {
    switch (current_) {
        case SessionState::NotStarted:
        case SessionState::Exited:
            return next == SessionState::Stopped;
        case SessionState::Stopped:
            return next == SessionState::Running;
        case SessionState::Running:
            return next == SessionState::Stopped || next == SessionState::Exited;
    }
    return false;
}

Result<void> SessionStateMachine::transition_to(SessionState next) {
    if (!can_transition_to(next)) {
        return Error{ErrorCode::InvalidState,
                     std::format("invalid session transition: {} -> {}", to_string(current_),
                                 to_string(next))};
    }
    current_ = next;
    return {};
}

}  // namespace stepwise::core
