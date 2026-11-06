#pragma once

#include <concepts>
#include <cstdlib>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <stepwise/base/error.hpp>

namespace stepwise {

template <class T, class E = Error>
class [[nodiscard]] Result {
    static_assert(!std::same_as<T, E>, "value and error types must differ");

public:
    template <class U = T>
        requires(!std::same_as<std::remove_cvref_t<U>, Result> &&
                 !std::same_as<std::remove_cvref_t<U>, E> && std::convertible_to<U, T>)
    Result(U&& value) : storage_(std::in_place_index<0>, std::forward<U>(value)) {}

    Result(E error) : storage_(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept { return storage_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] T& value() & {
        require(ok());
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] const T& value() const& {
        require(ok());
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] T&& value() && {
        require(ok());
        return std::move(*std::get_if<0>(&storage_));
    }

    [[nodiscard]] const E& error() const& {
        require(!ok());
        return *std::get_if<1>(&storage_);
    }

private:
    static void require(bool condition) noexcept {
        if (!condition) {
            std::abort();
        }
    }

    std::variant<T, E> storage_;
};

template <class E>
class [[nodiscard]] Result<void, E> {
public:
    Result() = default;
    Result(E error) : error_(std::move(error)) {}

    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const E& error() const& {
        if (ok()) {
            std::abort();
        }
        return *error_;
    }

private:
    std::optional<E> error_;
};

}  // namespace stepwise
