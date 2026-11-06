#include <stepwise/core/breakpoint_manager.hpp>

#include <algorithm>
#include <format>
#include <span>
#include <utility>

namespace stepwise::core {
namespace {

constexpr std::byte int3_opcode{0xCC};

Error not_found(BreakpointId id) {
    return Error{ErrorCode::NotFound, std::format("no breakpoint number {}", id.value())};
}

}  // namespace

BreakpointManager::Slot* BreakpointManager::find_slot(BreakpointId id) noexcept {
    const auto match = std::ranges::find_if(slots_, [id](const Slot& slot) { return slot.info.id == id; });
    return match == slots_.end() ? nullptr : &*match;
}

const Breakpoint* BreakpointManager::find(BreakpointId id) const noexcept {
    const auto match = std::ranges::find_if(slots_, [id](const Slot& slot) { return slot.info.id == id; });
    return match == slots_.end() ? nullptr : &match->info;
}

Result<BreakpointId> BreakpointManager::add(Address address, std::optional<Condition> condition) {
    const auto duplicate =
        std::ranges::find_if(slots_, [address](const Slot& slot) { return slot.info.address == address; });
    if (duplicate != slots_.end()) {
        return Error{ErrorCode::AlreadyExists,
                     std::format("breakpoint {} already exists at {:#x}", duplicate->info.id.value(),
                                 address.value())};
    }
    const BreakpointId id{next_id_++};
    slots_.push_back(Slot{Breakpoint{id, address, true, 0, std::move(condition)}, std::nullopt});
    return id;
}

Result<void> BreakpointManager::remove(BreakpointId id) {
    const auto match = std::ranges::find_if(slots_, [id](const Slot& slot) { return slot.info.id == id; });
    if (match == slots_.end()) {
        return not_found(id);
    }
    slots_.erase(match);
    return {};
}

Result<void> BreakpointManager::set_enabled(BreakpointId id, bool enabled) {
    Slot* slot = find_slot(id);
    if (slot == nullptr) {
        return not_found(id);
    }
    slot->info.enabled = enabled;
    return {};
}

std::vector<Breakpoint> BreakpointManager::list() const {
    std::vector<Breakpoint> result;
    result.reserve(slots_.size());
    for (const Slot& slot : slots_) {
        result.push_back(slot.info);
    }
    return result;
}

bool BreakpointManager::has_enabled_at(Address address) const noexcept {
    return std::ranges::any_of(slots_, [address](const Slot& slot) {
        return slot.info.enabled && slot.info.address == address;
    });
}

std::optional<BreakpointId> BreakpointManager::inserted_at(Address address) const noexcept {
    for (const Slot& slot : slots_) {
        if (slot.saved && slot.info.address == address) {
            return slot.info.id;
        }
    }
    return std::nullopt;
}

void BreakpointManager::set_temporary(std::optional<Address> address) noexcept {
    temporary_ = address;
}

bool BreakpointManager::temporary_inserted_at(Address address) const noexcept {
    return temporary_inserted_ && temporary_ == address;
}

std::uint64_t BreakpointManager::record_hit(BreakpointId id) {
    Slot* slot = find_slot(id);
    return slot == nullptr ? 0 : ++slot->info.hit_count;
}

Result<void> BreakpointManager::patch(backend::Debuggee& debuggee, Address address, std::byte& original) {
    if (auto read = debuggee.read_memory(address, std::span<std::byte>{&original, 1}); !read) {
        return read;
    }
    return debuggee.write_memory(address, std::span<const std::byte>{&int3_opcode, 1});
}

Result<void> BreakpointManager::insert_all(backend::Debuggee& debuggee) {
    for (Slot& slot : slots_) {
        if (!slot.info.enabled || slot.saved) {
            continue;
        }
        std::byte original{};
        if (auto patched = patch(debuggee, slot.info.address, original); !patched) {
            static_cast<void>(remove_all(debuggee));
            return Error{patched.error().code(),
                         std::format("cannot insert breakpoint {} at {:#x}: {}", slot.info.id.value(),
                                     slot.info.address.value(), patched.error().message())};
        }
        slot.saved = original;
    }

    if (temporary_ && !temporary_inserted_) {
        // a user breakpoint is already patched here; patching again
        // would save 0xCC as the original byte
        if (!has_enabled_at(*temporary_)) {
            std::byte original{};
            if (auto patched = patch(debuggee, *temporary_, original); !patched) {
                static_cast<void>(remove_all(debuggee));
                return Error{patched.error().code(),
                             std::format("cannot insert internal breakpoint at {:#x}: {}",
                                         temporary_->value(), patched.error().message())};
            }
            temporary_saved_ = original;
        }
        temporary_inserted_ = true;
    }
    return {};
}

Result<void> BreakpointManager::remove_all(backend::Debuggee& debuggee) {
    std::optional<Error> first_failure;
    const auto restore = [&](Address address, std::byte original) {
        auto restored = debuggee.write_memory(address, std::span<const std::byte>{&original, 1});
        if (!restored && !first_failure) {
            first_failure = restored.error();
        }
    };

    for (Slot& slot : slots_) {
        if (slot.saved) {
            const std::byte original = *slot.saved;
            slot.saved.reset();
            restore(slot.info.address, original);
        }
    }
    if (temporary_saved_) {
        const std::byte original = *temporary_saved_;
        temporary_saved_.reset();
        restore(*temporary_, original);
    }
    temporary_inserted_ = false;

    if (first_failure) {
        return *first_failure;
    }
    return {};
}

void BreakpointManager::forget_inserted() noexcept {
    for (Slot& slot : slots_) {
        slot.saved.reset();
    }
    temporary_saved_.reset();
    temporary_inserted_ = false;
}

}  // namespace stepwise::core
