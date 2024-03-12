#pragma once

#include <memory>
#include <utility>

namespace asyncrt {

#if 0
template <typename D>
class DropPtr {
public:
    explicit DropPtr(D* ptr) : m_ptr{ptr} {}

    DropPtr(DropPtr const&) = delete;

    DropPtr(DropPtr&& other) : m_ptr{other.m_ptr} {
        other.m_ptr = nullptr;
    }

    ~DropPtr() {
        drop();
    }

    DropPtr& operator=(DropPtr const&) = delete;

    DropPtr& operator=(DropPtr&& other) {
        drop();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }

    D* release() noexcept {
        auto* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    void reset(D* ptr = nullptr) noexcept {
        drop();
        m_ptr = ptr;
    }

    D* get() const noexcept {
        return m_ptr;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    D& operator*() const noexcept {
        return *m_ptr;
    }

    D* operator->() const noexcept {
        return m_ptr;
    }

private:
    void drop() {
        if (m_ptr != nullptr) {
            m_ptr->drop(m_ptr);
            m_ptr = nullptr;
        }
    }

    D* m_ptr;
};
#endif

namespace detail {

template <typename D>
void drop(D* ptr) {
    ptr->vtable->drop(ptr);
}

}  // namespace detail

template <typename D>
using DropPtr = std::unique_ptr<D, void (*)(D*)>;

template <typename D>
DropPtr<D> make_drop_ptr_from_raw(D* ptr) {
    return DropPtr<D>{ptr, detail::drop};
}

template <typename D, typename... Args>
DropPtr<D> make_drop_ptr(Args&&... args) {
    return DropPtr<D>{new D{std::forward<Args>(args)...}, detail::drop};
}

}  // namespace asyncrt
