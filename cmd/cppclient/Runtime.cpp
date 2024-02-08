#include "Runtime.hpp"

#include <iostream>

namespace asyncrt {

FfiWakerVTable g_wakerImplVTable{
    &Waker::clone,
    &Waker::wake,
    &Waker::wake_by_ref,
    &Waker::drop,
};

Task::Task(::FfiFuture future, Executor& executor, uint64_t id)
    : m_future{future}, m_waker{new Waker{executor, *this}}, m_id{id} {}

Task::~Task() {
    m_future.drop_fn(m_future.fut_ptr);
}

FfiPoll Task::poll() {
    std::cout << "polling task " << m_id << std::endl;
    FfiContext ctx{m_waker.get()};
    return m_future.poll_fn(m_future.fut_ptr, &ctx);
}

Waker::Waker(Executor& executor, Task& task)
    : FfiWakerBase{&g_wakerImplVTable},
      m_Executor{executor},
      m_task{task} {}

FfiWakerBase const* Waker::Clone() const {
    std::cout << "WakerImpl::Clone() called" << std::endl;
    return new Waker(*this);
}

void Waker::wake_impl() const {
    std::cout << "WakerImpl::Wake() called" << std::endl;
    m_Executor.ready(m_task);
}

void Waker::wake_by_ref_impl() const {
    std::cout << "WakerImpl::WakeByRef() called" << std::endl;
    m_Executor.ready(m_task);
}

void Waker::drop_impl() const {
    std::cout << "WakerImpl::Drop() called" << std::endl;
    delete this;
}

Executor::Executor(boost::asio::io_context& ioCtx) : m_ioctx{ioCtx} {}

void Executor::await(::FfiFuture future) {
    auto task = std::make_shared<Task>(std::move(future), *this, m_last_task_id++);
    bool done = schedule(*task);
    if (!done) {
        m_tasks.emplace_back(std::move(task));
    }
}

bool Executor::schedule(Task& task) {
    std::cout << "scheduling task " << task.get_id() << std::endl;
    auto poll = task.poll();
    bool done = false;
    std::cout << "poll=" << static_cast<int>(poll.status) << std::endl;
    switch (poll.status) {
    case PollStatus::Ready:
        std::cout << "task " << task.get_id() << " finished" << std::endl;
        done = true;
        break;
    case PollStatus::Pending:
        std::cout << "task " << task.get_id() << " pending" << std::endl;
        break;
    case PollStatus::Panicked:
        std::cout << "task " << task.get_id() << " panicked" << std::endl;
        done = true;
        break;
    }
    return done;
}

void Executor::ready(Task& task) {
    m_ioctx.dispatch([this, &task]() {
        bool done = schedule(task);
        if (done) {
            std::cout << "removing task " << task.get_id() << " from runtime" << std::endl;
            auto taskId = task.get_id();
            auto begin = std::begin(m_tasks);
            auto end = std::end(m_tasks);
            auto iter = std::find_if(begin, end, [taskId](auto const& task) {
                    return task->get_id() == taskId;
                });
            if (iter == end) {
                std::cerr << "error: task " << taskId << " not known" <<
                    std::endl;
            }
            m_tasks.erase(iter);
        }
    });
}

} // namespace asyncrt
