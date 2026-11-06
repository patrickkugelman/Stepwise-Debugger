#pragma once

#include <compare>
#include <cstdint>

namespace stepwise {

template <class Tag, class Rep>
class StrongValue {
public:
    using rep = Rep;

    constexpr explicit StrongValue(Rep value) noexcept : value_(value) {}

    [[nodiscard]] constexpr Rep value() const noexcept { return value_; }

    friend constexpr auto operator<=>(const StrongValue&, const StrongValue&) = default;

private:
    Rep value_;
};

using Address = StrongValue<struct AddressTag, std::uint64_t>;
using Pid = StrongValue<struct PidTag, std::int32_t>;
using Signal = StrongValue<struct SignalTag, std::int32_t>;
using BreakpointId = StrongValue<struct BreakpointIdTag, std::uint32_t>;

}  // namespace stepwise
