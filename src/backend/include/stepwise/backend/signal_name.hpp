#pragma once

#include <string>

#include <stepwise/base/types.hpp>

namespace stepwise::backend {

[[nodiscard]] std::string signal_name(Signal signal);

[[nodiscard]] std::string signal_description(Signal signal);

}  // namespace stepwise::backend
