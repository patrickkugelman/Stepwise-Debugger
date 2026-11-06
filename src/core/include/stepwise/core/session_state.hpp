#pragma once

#include <string_view>

#include <stepwise/base/result.hpp>

namespace stepwise::core {

enum class SessionState { NotStarted, Stopped, Running, Exited };

[[nodiscard]] std::string_view to_string(SessionState state) noexcept;

class SessionStateMachine {
public:
    [[nodiscard]] SessionState current() const noexcept { return current_; }
    [[nodiscard]] bool can_transition_to(SessionState next) const noexcept;

    Result<void> transition_to(SessionState next);

private:
    SessionState current_{SessionState::NotStarted};
};

}  // namespace stepwise::core
