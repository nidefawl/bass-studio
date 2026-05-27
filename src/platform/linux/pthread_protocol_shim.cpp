// Symbol interposition for pthread_mutexattr_setprotocol.
//
// JUCE-based LV2 plugins (Vitalium, etc.) create pthread mutexes with
// PTHREAD_PRIO_INHERIT. When such a mutex is locked from a SCHED_OTHER
// thread (priority 0 — i.e. any normal user thread), glibc 2.40+ aborts
// in __pthread_tpp_change_priority because 0 is outside the valid
// SCHED_FIFO range [1..99].
//
// The host does not have CAP_SYS_NICE and cannot legitimately make its
// main thread an RT thread, so the only safe workaround is to neutralise
// the protocol attribute for every mutex in the process: force every
// pthread_mutexattr_setprotocol() call to record PTHREAD_PRIO_NONE.
//
// This file defines a replacement pthread_mutexattr_setprotocol() in the
// main executable's dynamic symbol table (with a versioned alias matching
// glibc's GLIBC_2.4 export, supplied via the accompanying version
// script). The dynamic linker resolves dlopen()'d plugins' references to
// our wrapper first, and our wrapper delegates to the real libc
// implementation with the protocol overridden.
#if defined(__linux__)

#include <dlfcn.h>
#include <pthread.h>

extern "C" {

__attribute__((visibility("default")))
int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr, int /*protocol*/) {
    using Fn = int (*)(pthread_mutexattr_t*, int);
    static Fn real_fn = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, "pthread_mutexattr_setprotocol"));
    if (!real_fn) {
        return 0;
    }
    return real_fn(attr, PTHREAD_PRIO_NONE);
}

} // extern "C"

#endif // __linux__
