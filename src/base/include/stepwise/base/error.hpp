#pragma once

#include <string>
#include <string_view>

namespace stepwise {

enum class ErrorCode {
    InvalidArgument,
    InvalidState,
    NotFound,
    AlreadyExists,
    Os,
    Protocol,
};

class Error {
public:
    Error(ErrorCode code, std::string message);

    static Error from_errno(std::string_view context, int error_number);

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};

}  // namespace stepwise
