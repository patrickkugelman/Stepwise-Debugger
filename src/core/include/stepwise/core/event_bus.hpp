#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <stepwise/core/events.hpp>

namespace stepwise::core {

class EventBus;

class [[nodiscard]] Subscription {
public:
    Subscription(EventBus& bus, std::uint64_t id) noexcept : bus_(&bus), id_(id) {}
    ~Subscription();

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) noexcept
        : bus_(std::exchange(other.bus_, nullptr)), id_(other.id_) {}
    Subscription& operator=(Subscription&& other) noexcept;

private:
    EventBus* bus_;
    std::uint64_t id_;
};

class EventBus {
public:
    using Handler = std::function<void(const Event&)>;

    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    Subscription subscribe(Handler handler);
    void publish(const Event& event) const;

private:
    friend class Subscription;

    struct Entry {
        std::uint64_t id;
        Handler handler;
    };

    void unsubscribe(std::uint64_t id) noexcept;

    std::vector<Entry> entries_;
    std::uint64_t next_id_{0};
};

}  // namespace stepwise::core
