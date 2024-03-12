#pragma once
// Future and Promise are modeled after their standard library counterparts, except that they use
// callbacks instead of blocking member functions.
//
// Error handling is not refined and uses the default exceptions with custom text. This should
// be changed for production code.

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

#include <iostream>  // TODO: debugging only

namespace asyncrt {
namespace detail {

template <typename T>
struct SharedState {
    std::mutex mutex{};
    std::optional<T> value{};
    std::optional<std::function<void()>> wait_callback{};
};

}  // namespace detail

template <typename T>
class AsyncFuture {
public:
    AsyncFuture() = default;
    AsyncFuture(std::shared_ptr<detail::SharedState<T>> shared_state)
        : m_shared_state{std::move(shared_state)} {}
    AsyncFuture(AsyncFuture&&) = default;
    AsyncFuture(AsyncFuture const&) = delete;
    ~AsyncFuture() = default;

    AsyncFuture& operator=(AsyncFuture&&) = default;
    AsyncFuture& operator=(AsyncFuture const&) = delete;

    bool valid() const noexcept { return m_shared_state; }

    [[nodiscard]] bool is_ready() const noexcept {
        std::cout << "+++ [C] AsyncFuture::is_ready " << std::flush;
        std::lock_guard lock{m_shared_state->mutex};
        std::cout << m_shared_state->value.has_value() << std::endl;
        return m_shared_state->value.has_value();
    }

    [[nodiscard]] T& value() {
        if (!m_shared_state->value) {
            throw std::logic_error{"future not ready"};
        }
        return *m_shared_state->value;
    }

    [[nodiscard]] T const& value() const {
        if (!m_shared_state->value) {
            throw std::logic_error{"future not ready"};
        }
        return *m_shared_state->value;
    }

    template <typename F>
    void await(F&& f) {
        std::cout << "+++ [C] awaiting future\n";
        std::lock_guard lock{m_shared_state->mutex};
        if (m_shared_state->value.has_value()) {
            std::cout << "+++ [C] value already available" << std::endl;
            f();
        } else {
            std::cout << "+++ [C] setting callback function" << std::endl;
            if (m_shared_state->wait_callback.has_value()) {
                throw std::logic_error{"future is already awaited on"};
            }
            m_shared_state->wait_callback = std::function<void()>(f);
        }
    }

private:
    std::shared_ptr<detail::SharedState<T>> m_shared_state;
};

template <typename T>
class Promise {
public:
    Promise() : m_shared_state{std::make_shared<detail::SharedState<T>>()} {}
    Promise(Promise&&) = default;
    Promise(Promise const&) = delete;
    ~Promise() = default;

    Promise& operator=(Promise&&) = default;
    Promise& operator=(Promise const&) = delete;

    AsyncFuture<T> get_future() {
        if (m_future_created) {
            throw std::logic_error{"future already retrieved"};
        }
        if (!m_shared_state) {
            throw std::logic_error{"promise has no shared state"};
        }
        return AsyncFuture<T>{m_shared_state};
    }

    void set_value(T const& t) {
        if (!m_shared_state) {
            throw std::logic_error{"promise has no shared state"};
        }
        {
            std::lock_guard lock{m_shared_state->mutex};
            if (m_satisfied) {
                throw std::logic_error{"promise already satisfied"};
            }
            std::cout << "+++ [C] storing value in promise" << std::endl;
            m_shared_state->value = t;
            m_satisfied = true;
        }
        if (m_shared_state->wait_callback.has_value()) {
            (*m_shared_state->wait_callback)();
        }
    }

    void set_value(T&& t) {
        if (!m_shared_state) {
            throw std::logic_error{"promise has no shared state"};
        }
        {
            std::lock_guard lock{m_shared_state->mutex};
            if (m_satisfied) {
                throw std::logic_error{"promise already satisfied"};
            }
            std::cout << "+++ [C] storing value in promise" << std::endl;
            m_shared_state->value.emplace(std::forward<T>(t));
            m_satisfied = true;
        }
        if (m_shared_state->wait_callback.has_value()) {
            (*m_shared_state->wait_callback)();
        }
    }

private:
    std::shared_ptr<detail::SharedState<T>> m_shared_state;
    bool m_future_created{false};
    bool m_satisfied{false};
};

}  // namespace asyncrt
