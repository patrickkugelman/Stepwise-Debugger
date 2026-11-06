#include <stepwise/backend/signal_name.hpp>

#include <cstring>
#include <format>

namespace stepwise::backend {

std::string signal_name(Signal signal) {
    if (const char* abbreviation = ::sigabbrev_np(signal.value()); abbreviation != nullptr) {
        return std::format("SIG{}", abbreviation);
    }
    return std::format("signal {}", signal.value());
}

std::string signal_description(Signal signal) {
    if (const char* description = ::sigdescr_np(signal.value()); description != nullptr) {
        return description;
    }
    return std::format("signal {}", signal.value());
}

}  // namespace stepwise::backend
