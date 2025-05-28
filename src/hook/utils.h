#ifndef DOBBY_INJECT_UTILS_H
#define DOBBY_INJECT_UTILS_H

#include <unistd.h>
#include <dobby.h>
#include <sys/mman.h>

#include <android/log.h>
#include <unwind.h>
#include <dlfcn.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGOPEN 1
#define LOG_TAG    "[Dobby_Inject]"
#if(LOGOPEN == 1)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) NULL
#define LOGE(...) NULL
#define LOGD(...) NULL
#endif

#ifdef __cplusplus
}
#endif

#define uintval(p)              reinterpret_cast<uintptr_t>(p)
#define ptr(p)                  (reinterpret_cast<void *>(p))
#define align_up(x, n)          (((x) + ((n) - 1)) & ~((n) - 1))
#define align_down(x, n)        ((x) & -(n))
#define page_size               getpagesize()
#define page_align(n)           align_up(static_cast<uintptr_t>(n), page_size)
#define ptr_align(x)            ptr(align_down(reinterpret_cast<uintptr_t>(x), page_size))
#define make_rwx(p, n)          ::mprotect(ptr_align(p), \
                                            page_align(uintval(p) + (n)) != page_align(uintval(p)) \
                                                ? page_align(n) + page_size : page_align(n),       \
                                            PROT_READ | PROT_WRITE | PROT_EXEC)

inline void* InlineHook(void* target, void* hooker) {
    make_rwx(target, page_size);
    void* origin_call;
    if (DobbyHook(target, hooker, &origin_call) == 0) {
        return origin_call;
    } else {
        return nullptr;
    }
}

struct StackState {
    void** frames;
    int max_depth;
    int count;
};

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
    StackState* state = (StackState*)arg;
    if (state->count >= state->max_depth) {
        return _URC_END_OF_STACK;
    }

    void* pc = (void*)_Unwind_GetIP(context);
    if (pc) {
        state->frames[state->count++] = pc;
    }

    return _URC_NO_REASON;
}

inline void print_callstack() {
    void* frames[64];
    StackState state = {frames, 64, 0};
    _Unwind_Backtrace(unwind_callback, &state);

    LOGD("Callstack (depth %d):", state.count);
    for (int i = 0; i < state.count; i++) {
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_sname) {
            LOGD("#%d: %p <%s+%lu> (%s)", i, frames[i], info.dli_sname,
                 (unsigned long)((uintptr_t)frames[i] - (uintptr_t)info.dli_saddr),
                 info.dli_fname);
        } else {
            LOGD("#%d: %p", i, frames[i]);
        }
    }
}

#endif
