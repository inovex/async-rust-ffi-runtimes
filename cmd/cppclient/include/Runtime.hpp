#pragma once

#include "Drop.hpp"
#include "ffi/future.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <boost/asio/io_context.hpp>

#include <iostream>

namespace asyncrt {

using PollStatus = ::PollStatus;

// A future which is passed from Rust to C++
template <typename T>
class RustFuture {
public:
    RustFuture(::FfiFuture<T> f) : m_ffi_future{std::move(f)} {}
    RustFuture(RustFuture const&) = delete;

    RustFuture(RustFuture&& other) : m_ffi_future{other.m_ffi_future} {
        other.m_ffi_future.fut_ptr = nullptr;
    }

    ~RustFuture() {
        if (m_ffi_future.fut_ptr != nullptr) {
            m_ffi_future.drop_fn(m_ffi_future.fut_ptr);
        }
    }

    RustFuture& operator=(RustFuture const&) = delete;

    RustFuture& operator=(RustFuture&& other) {
        m_ffi_future.fut_ptr = other.m_ffi_future.fut_ptr;
        other.m_ffi_future.fut_ptr = nullptr;
    }

    ::FfiPoll<T> poll(::FfiContext* context) {
        return m_ffi_future.poll_fn(m_ffi_future.fut_ptr, context);
    }

private:
    ::FfiFuture<T> m_ffi_future;
};

class Executor;

namespace detail {

class TaskBase;

class Waker : public ::FfiWakerBase {
public:
    Waker(Executor& executor, TaskBase& task);
    ~Waker();

    Waker& operator=(Waker const&) = delete;
    Waker& operator=(Waker&&) = delete;

    static ::FfiWakerBase const* clone(::FfiWakerBase const* self) {
        return static_cast<Waker const*>(self)->clone_impl();
    }

    static void wake(::FfiWakerBase const* self) { static_cast<Waker const*>(self)->wake_impl(); }

    static void wake_by_ref(::FfiWakerBase const* self) {
        static_cast<Waker const*>(self)->wake_by_ref_impl();
    }

    static void drop(::FfiWakerBase const* self) { static_cast<Waker const*>(self)->drop_impl(); }

private:
    Waker(Waker const&) = default;
    Waker(Waker&&) = default;

    ::FfiWakerBase const* clone_impl() const;
    void wake_impl() const;
    void wake_by_ref_impl() const;
    void drop_impl() const;

    Executor& m_Executor;
    TaskBase& m_task;
};

class TaskBase {
protected:
    TaskBase(Executor& executor, uint64_t id);
    virtual ~TaskBase();

    virtual PollStatus poll_impl(Executor& executor) = 0;

public:
    [[nodiscard]] bool poll(Executor& executor);

    uint64_t get_id() const noexcept { return m_id; }

    ::FfiContext* get_context() noexcept { return &m_context; }

private:
    uint64_t m_id;
    DropPtr<Waker> m_waker;
    ::FfiContext m_context;
};

/**
 * Stores a future and its callback for later execution.
 */
template <typename T, typename F>
class Task : public detail::TaskBase {
public:
    Task(RustFuture<T> future, F&& callback, Executor& executor, uint64_t id)
        : TaskBase{executor, id}, m_future{std::move(future)}, m_callback{std::move(callback)} {}

protected:
    [[nodiscard]] PollStatus poll_impl(Executor& executor) override {
        auto poll = m_future.poll(get_context());
        if (poll.status == PollStatus::Ready) {
            m_callback(poll.value);
        }
        return poll.status;
    }

private:
    RustFuture<T> m_future;
    F m_callback;
};

}  // namespace detail

class Executor {
public:
    Executor(boost::asio::io_context& ioCtx);

    template <typename T, typename F>
    void await(RustFuture<T> future, F&& callback) {
        auto task = std::make_shared<detail::Task<T, F>>(
            std::move(future), std::forward<F>(callback), *this, m_last_task_id++);
        auto done = task->poll(*this);
        if (!done) {
            m_tasks.emplace_back(std::move(task));
        }
    }

    // used by the task when it was woken
    void ready(detail::TaskBase& task);

private:
    std::vector<std::shared_ptr<detail::TaskBase>> m_tasks{};
    boost::asio::io_context& m_ioctx;
    uint64_t m_last_task_id = 0;
};

template <typename T>
::FfiPoll<T> make_poll_status(PollStatus poll_status) {
    if (poll_status == PollStatus::Ready) {
        throw std::invalid_argument{"poll status must not be ready without value"};
    }
    return ::FfiPoll<T>{
        .status = poll_status,
    };
}

template <typename T>
::FfiPoll<T> make_poll_status(T&& value) {
    return ::FfiPoll<T>{
        .status = PollStatus::Ready,
        .value = std::forward<T>(value),
    };
}

namespace detail {

template <typename T, typename F>
class FutureImpl {
public:
    FutureImpl(F&& f) : m_func{std::forward<F>(f)} {}

    static ::FfiPoll<T> poll(void* self, ::FfiContext* context) {
        std::cout << "+++ [C] called  CppFuture " << self << " with context "
                  << reinterpret_cast<void*>(context) << ", waker "
                  << reinterpret_cast<void const*>(context->waker) << ", vtable "
                  << reinterpret_cast<void const*>(context->waker->vtable) << ", wake func "
                  << reinterpret_cast<void const*>(context->waker->vtable->wake) << std::endl;
        return static_cast<FutureImpl*>(self)->poll_impl(context);
    }

    static void drop(void* self) {
        auto* p = static_cast<FutureImpl*>(self);
        delete p;
    }

private:
    ::FfiPoll<T> poll_impl(::FfiContext* ctx) {
        try {
            return m_func(ctx);
        } catch (...) {
            return make_poll_status<T>(PollStatus::Panicked);
        }
    }

    F m_func;
};

}  // namespace detail

template <typename T, typename F>
::FfiFuture<T> make_cpp_future(F&& f) {
    auto* future = new detail::FutureImpl<T, F>{std::forward<F>(f)};
    std::cout << "+++ [C] CPP FutureImpl    " << reinterpret_cast<void*>(future) << std::endl;
    return ::FfiFuture<T>{
        future,
        &detail::FutureImpl<T, F>::poll,
        &detail::FutureImpl<T, F>::drop,
    };
}

}  // namespace asyncrt
