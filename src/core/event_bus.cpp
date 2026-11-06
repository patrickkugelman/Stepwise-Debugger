#include <stepwise/core/event_bus.hpp>

#include <algorithm>

namespace stepwise::core {

Subscription::~Subscription() {
    if (bus_ != nullptr) {
        bus_->unsubscribe(id_);
    }
}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        if (bus_ != nullptr) {
            bus_->unsubscribe(id_);
        }
        bus_ = std::exchange(other.bus_, nullptr);
        id_ = other.id_;
    }
    return *this;
}

Subscription EventBus::subscribe(Handler handler) {
    const std::uint64_t id = next_id_++;
    entries_.push_back(Entry{id, std::move(handler)});
    return Subscription{*this, id};
}

void EventBus::publish(const Event& event) const {
    const std::vector<Entry> snapshot = entries_;
    for (const Entry& entry : snapshot) {
        entry.handler(event);
    }
}

void EventBus::unsubscribe(std::uint64_t id) noexcept {
    std::erase_if(entries_, [id](const Entry& entry) { return entry.id == id; });
}

}  // namespace stepwise::core
