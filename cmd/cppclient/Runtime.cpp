#include "Runtime.hpp"

#include <iostream>

namespace asyncrt {
namespace detail {

FfiWakerVTable g_wakerImplVTable{
    &Waker::clone,
    &Waker::wake,
    &Waker::wake_by_ref,
    &Waker::drop,
};

Waker::Waker(Executor& executor, TaskBase& task)
    : FfiWakerBase{&g_wakerImplVTable}, m_Executor{executor}, m_task{task} {}

FfiWakerBase const* Waker::clone_impl() const {
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

TaskBase::TaskBase(Executor& executor, uint64_t id)
    : m_id{id}, m_waker{executor, *this}, m_context{&m_waker} {}

TaskBase::~TaskBase() = default;

bool TaskBase::poll(Executor& executor) {
    std::cout << "scheduling task " << m_id << std::endl;
    auto status = poll_impl(executor);
    switch (status) {
    case PollStatus::Ready:
        std::cout << "task " << m_id << " finished" << std::endl;
        break;
    case PollStatus::Pending:
        std::cout << "task " << m_id << " pending" << std::endl;
        break;
    case PollStatus::Panicked:
        std::cout << "task " << m_id << " panicked" << std::endl;
        break;
    }
    return status != PollStatus::Pending;
}

}  // namespace detail

Executor::Executor(boost::asio::io_context& ioCtx) : m_ioctx{ioCtx} {}

void Executor::ready(detail::TaskBase& task) {
    m_ioctx.dispatch([this, &task]() {
        bool done = task.poll(*this);
        if (done) {
            std::cout << "removing task " << task.get_id() << " from runtime" << std::endl;
            auto task_id = task.get_id();
            auto begin = std::begin(m_tasks);
            auto end = std::end(m_tasks);
            auto iter = std::find_if(
                begin, end, [task_id](auto const& task) { return task->get_id() == task_id; });
            if (iter == end) {
                std::cerr << "error: task " << task_id << " not known" << std::endl;
            }
            m_tasks.erase(iter);
        }
    });
}

}  // namespace asyncrt
