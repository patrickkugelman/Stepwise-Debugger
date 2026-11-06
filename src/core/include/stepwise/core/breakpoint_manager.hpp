#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <stepwise/backend/debuggee.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/base/types.hpp>
#include <stepwise/core/condition.hpp>

namespace stepwise::core {

struct Breakpoint {
    BreakpointId id;
    Address address;
    bool enabled;
    std::uint64_t hit_count;
    std::optional<Condition> condition;
};

class BreakpointManager {
public:
    Result<BreakpointId> add(Address address, std::optional<Condition> condition = std::nullopt);
    Result<void> remove(BreakpointId id);
    Result<void> set_enabled(BreakpointId id, bool enabled);

    [[nodiscard]] std::vector<Breakpoint> list() const;
    [[nodiscard]] const Breakpoint* find(BreakpointId id) const noexcept;
    [[nodiscard]] bool has_enabled_at(Address address) const noexcept;

    [[nodiscard]] std::optional<BreakpointId> inserted_at(Address address) const noexcept;

    void set_temporary(std::optional<Address> address) noexcept;
    [[nodiscard]] bool temporary_inserted_at(Address address) const noexcept;

    std::uint64_t record_hit(BreakpointId id);

    Result<void> insert_all(backend::Debuggee& debuggee);
    Result<void> remove_all(backend::Debuggee& debuggee);
    void forget_inserted() noexcept;

private:
    struct Slot {
        Breakpoint info;
        std::optional<std::byte> saved;
    };

    Slot* find_slot(BreakpointId id) noexcept;
    Result<void> patch(backend::Debuggee& debuggee, Address address, std::byte& original);

    std::vector<Slot> slots_;
    std::uint32_t next_id_{1};

    std::optional<Address> temporary_;
    bool temporary_inserted_{false};
    std::optional<std::byte> temporary_saved_;
};

}  // namespace stepwise::core
