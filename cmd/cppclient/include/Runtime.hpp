#pragma once

#include "ffi/future.h"

#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace asyncrt {

class Executor;
class Waker;

class Task {
public:
    Task(::FfiFuture future, Executor& executor, uint64_t id);
    ~Task();

    ::FfiPoll poll();

    uint64_t get_id() const {
        return m_id;
    }

private:
    ::FfiFuture m_future;
    std::unique_ptr<Waker> m_waker;
    uint64_t m_id;
};

class Waker : public ::FfiWakerBase {
public:
    Waker(Executor& executor, Task& task);

    static ::FfiWakerBase const* clone(::FfiWakerBase const* self) {
        return static_cast<Waker const*>(self)->Clone();
    }

    static void wake(::FfiWakerBase const* self) {
        static_cast<Waker const*>(self)->wake_impl();
    }

    static void wake_by_ref(::FfiWakerBase const* self) {
        static_cast<Waker const*>(self)->wake_by_ref_impl();
    }

    static void drop(::FfiWakerBase const* self) {
        static_cast<Waker const*>(self)->drop_impl();
    }

private:
    ::FfiWakerBase const* Clone() const;
    void wake_impl() const;
    void wake_by_ref_impl() const;
    void drop_impl() const;

    Executor& m_Executor;
    Task& m_task;
};

class Executor {
public:
    Executor(boost::asio::io_context& ioCtx);

    void await(::FfiFuture task);
    void ready(Task& task);

private:
    bool schedule(Task& task);

    std::vector<std::shared_ptr<Task>> m_tasks{};
    boost::asio::io_context& m_ioctx;
    uint64_t m_last_task_id = 0;
};

namespace detail {

template<typename F>
class FutureImpl {
public:
    FutureImpl(F&& f) : m_func{std::forward<F>(f)} {}

    static ::FfiPoll poll(void* self, ::FfiContext* ctx) {
        return static_cast<FutureImpl*>(self)->poll_impl(ctx);
    }

    static void drop(void* self) {
        auto* p = static_cast<FutureImpl*>(self);
        delete p;
    }

private:
    ::FfiPoll poll_impl(::FfiContext* /* ctx */) {
        void* p = m_func();
        return ::FfiPoll{::PollStatus::Ready, p};
    }

    F m_func;
};

} // namespace detail

template<typename F>
::FfiFuture make_future(F&& f) {
    auto* future = new detail::FutureImpl{std::forward<F>(f)};
    return ::FfiFuture {
        future,
        &detail::FutureImpl<F>::poll,
        &detail::FutureImpl<F>::drop,
    };
}

} // asyncrt
