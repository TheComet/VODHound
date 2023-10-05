#pragma once

#include "vh/config.h"

C_BEGIN

struct thread
{
    void* handle;
};
struct mutex
{
    void* handle;
};

VH_PUBLIC_API int
thread_start(struct thread* t, void* (*func)(void*), void* args);

VH_PUBLIC_API int
thread_join(struct thread t, int timeout_ms);

VH_PUBLIC_API void
thread_kill(struct thread t);

VH_PUBLIC_API void
mutex_init(struct mutex* m);

VH_PUBLIC_API void
mutex_init_recursive(struct mutex* m);

VH_PUBLIC_API void
mutex_deinit(struct mutex m);

VH_PUBLIC_API void
mutex_lock(struct mutex m);

/*!
 * \brief Attempts to lock a mutex.
 * \param[in] m Mutex
 * \return Returns non-zero if the lock was acquired. Zero if the lock was not
 * acquired.
 */
VH_PUBLIC_API int
mutex_trylock(struct mutex m);

VH_PUBLIC_API void
mutex_unlock(struct mutex m);
