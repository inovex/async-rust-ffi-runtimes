#pragma once

#include <boost/asio/io_context.hpp>
#include <cstdint>
#include <memory>
#include <vector>

#include "ffi/future.h"

namespace asyncrt {

using PollStatus = ::PollStatus;

/** This is just a more convenient wrapper around FfiFuture.
 */
template <typename T>
class Future {
public:
    Future(::FfiFuture<T> f) : m_ffi_future{std::move(f)} {}
    Future(Future const&) = delete;

    Future(Future&& other) : m_ffi_future{other.m_ffi_future} {
        other.m_ffi_future.fut_ptr = nullptr;
    }

    ~Future() {
        if (m_ffi_future.fut_ptr != nullptr) {
            m_ffi_future.drop_fn(m_ffi_future.fut_ptr);
        }
    }

    Future& operator=(Future const&) = delete;

    Future& operator=(Future&& other) {
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

    static ::FfiWakerBase const* clone(::FfiWakerBase const* self) {
        return static_cast<Waker const*>(self)->clone_impl();
    }

    static void wake(::FfiWakerBase const* self) { static_cast<Waker const*>(self)->wake_impl(); }

    static void wake_by_ref(::FfiWakerBase const* self) {
        static_cast<Waker const*>(self)->wake_by_ref_impl();
    }

    static void drop(::FfiWakerBase const* self) { static_cast<Waker const*>(self)->drop_impl(); }

private:
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
    bool poll(Executor& executor);

    uint64_t get_id() const noexcept { return m_id; }

    ::FfiContext* get_context() noexcept { return &m_context; }

private:
    uint64_t m_id;
    Waker m_waker;
    ::FfiContext m_context;
};

/**
 * Stores a future and its callback for later execution.
 */
template <typename T, typename F>
class Task : public detail::TaskBase {
public:
    Task(Future<T> future, F&& callback, Executor& executor, uint64_t id)
        : TaskBase{executor, id}, m_future{std::move(future)}, m_callback{std::move(callback)} {}

protected:
    PollStatus poll_impl(Executor& executor) override {
        auto poll = m_future.poll(get_context());
        if (poll.status == PollStatus::Ready) {
            m_callback(poll.value);
        }
        return poll.status;
    }

private:
    Future<T> m_future;
    F m_callback;
};

}  // namespace detail

class Executor {
public:
    Executor(boost::asio::io_context& ioCtx);

    template <typename T, typename F>
    void await(Future<T> future, F&& callback) {
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

namespace detail {

template <typename T, typename F>
class FutureImpl {
public:
    FutureImpl(F&& f) : m_func{std::forward<F>(f)} {}

    static ::FfiPoll<T> poll(void* self, ::FfiContext* ctx) {
        return static_cast<FutureImpl*>(self)->poll_impl(ctx);
    }

    static void drop(void* self) {
        auto* p = static_cast<FutureImpl*>(self);
        delete p;
    }

private:
    ::FfiPoll<T> poll_impl(::FfiContext* /* ctx */) {
        return ::FfiPoll<T>{
            .status = ::PollStatus::Ready,
            .value = m_func(),
        };
    }

    F m_func;
};

}  // namespace detail

template <typename T, typename F>
::FfiFuture<T> make_future(F&& f) {
    auto* future = new detail::FutureImpl<T, F>{std::forward<F>(f)};
    return ::FfiFuture<T>{
        future,
        &detail::FutureImpl<T, F>::poll,
        &detail::FutureImpl<T, F>::drop,
    };
}

}  // namespace asyncrt
