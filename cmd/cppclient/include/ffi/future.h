#pragma once

#include <stdint.h>

struct FfiWakerBase;

struct FfiWakerVTable {
    const struct FfiWakerBase *(*clone)(const struct FfiWakerBase *);
    void (*wake)(const struct FfiWakerBase *);
    void (*wake_by_ref)(const struct FfiWakerBase *);
    void (*drop)(const struct FfiWakerBase *);
};

struct FfiWakerBase {
    const struct FfiWakerVTable *vtable;
};

struct FfiContext {
    const struct FfiWakerBase *waker;
};

enum class PollStatus : uint8_t {
    Ready,
    Pending,
    Panicked,
};

template<typename T>
struct FfiPoll {
    PollStatus status;
    // the value is only present if the PollStatus is Ready,
    // so wrap it in a union
    union {
        T value;
    };
};

template<typename T>
struct FfiFuture {
    void *fut_ptr;
    struct FfiPoll<T> (*poll_fn)(void *, struct FfiContext *);
    void (*drop_fn)(void *);
};
