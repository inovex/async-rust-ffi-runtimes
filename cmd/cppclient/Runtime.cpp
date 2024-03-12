#include "Runtime.hpp"

#include <sstream>

#include <iostream>  // TODO: debugging only

namespace asyncrt {
namespace detail {

::FfiWakerVTable g_wakerImplVTable{
    &Waker::clone,
    &Waker::wake,
    &Waker::wake_by_ref,
    &Waker::drop,
};

Waker::Waker(Executor& executor, TaskBase& task)
    : FfiWakerBase{&g_wakerImplVTable}, m_Executor{executor}, m_task{task} {
    std::cout << "+++ [C] waker " << reinterpret_cast<void*>(this) << " created" << std::endl;
}

Waker::~Waker() {
    std::cout << "+++ [C] waker " << reinterpret_cast<void*>(this) << " deleted" << std::endl;
}

FfiWakerBase const* Waker::clone_impl() const {
    auto const* p = new Waker(*this);
    std::cout << "+++ [C] clone waker " << reinterpret_cast<void const*>(this) << ": "
              << reinterpret_cast<void const*>(p) << std::endl;
    return p;
}

void Waker::wake_impl() const {
    std::cout << "+++ [C] wake waker " << reinterpret_cast<void const*>(this) << std::endl;
    m_Executor.ready(m_task);
}

void Waker::wake_by_ref_impl() const {
    std::cout << "+++ [C] wake_by_ref waker " << reinterpret_cast<void const*>(this) << std::endl;
    m_Executor.ready(m_task);
}

void Waker::drop_impl() const {
    std::cout << "+++ [C] drop waker " << reinterpret_cast<void const*>(this) << std::endl;
    delete this;
}

TaskBase::TaskBase(Executor& executor, uint64_t id)
    : m_id{id}, m_waker{make_drop_ptr<Waker>(executor, *this)}, m_context{m_waker.get()} {
    std::cout << "+++ [C] created task " << id << "                  " << " with context "
              << &m_context << ", waker " << reinterpret_cast<void*>(m_waker.get()) << ", vtable "
              << reinterpret_cast<void const*>(m_waker->vtable) << ", wake func "
              << reinterpret_cast<void const*>(m_waker->vtable->wake) << std::endl;
}

TaskBase::~TaskBase() {
    std::cout << "+++ [C] TaskBase::~TaskBase()" << std::endl;
}

bool TaskBase::poll(Executor& executor) {
    std::cout << "+++ [C] scheduling task " << m_id << std::endl;
    auto status = poll_impl(executor);
    switch (status) {
    case PollStatus::Ready:
        std::cout << "+++ [C] task " << m_id << " finished" << std::endl;
        break;
    case PollStatus::Pending:
        std::cout << "+++ [C] task " << m_id << " pending" << std::endl;
        break;
    case PollStatus::Panicked:
        std::cout << "+++ [C] task " << m_id << " panicked" << std::endl;
        break;
    }
    return status != PollStatus::Pending;
}

}  // namespace detail

Executor::Executor(boost::asio::io_context& ioCtx) : m_ioctx{ioCtx} {}

void Executor::ready(detail::TaskBase& task) {
    std::cout << "+++ [C] task " << task.get_id() << " became ready" << std::endl;
    // use post instead of dispatch, because the AsyncFuture may hold a lock
    // this should probably be redesigned, but it works for now
    m_ioctx.post([this, &task]() {
        bool done = task.poll(*this);
        if (done) {
            std::cout << "+++ [C] removing task " << task.get_id() << " from runtime" << std::endl;
            auto task_id = task.get_id();
            auto begin = std::begin(m_tasks);
            auto end = std::end(m_tasks);
            auto iter = std::find_if(
                begin, end, [task_id](auto const& task) { return task->get_id() == task_id; });
            if (iter == end) {
                std::stringstream s;
                s << "task " << task_id << " not known";
                throw std::logic_error{s.str()};
            }
            m_tasks.erase(iter);
        }
    });
}

}  // namespace asyncrt
