#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
enum class PollStatus : uint8_t {
#else
enum PollStatus {
#endif
    Ready,
    Pending,
    Panicked,
};

struct FfiPoll {
    enum PollStatus status;
    void* value;
};

struct FfiFuture {
    void *fut_ptr;
    struct FfiPoll (*poll_fn)(void *, struct FfiContext *);
    void (*drop_fn)(void *);
};

#ifdef __cplusplus
}
#endif
