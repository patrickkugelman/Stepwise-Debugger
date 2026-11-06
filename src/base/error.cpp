#include <stepwise/base/error.hpp>

#include <format>
#include <system_error>
#include <utility>

namespace stepwise {

Error::Error(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

Error Error::from_errno(std::string_view context, int error_number) {
    return Error{ErrorCode::Os,
                 std::format("{}: {}", context, std::system_category().message(error_number))};
}

}  // namespace stepwise
