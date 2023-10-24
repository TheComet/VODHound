#pragma once

#include "vh/config.h"
#include <stdint.h>

#if !defined(VH_MEM_DEBUGGING)
#   include <stdlib.h>
#   define mem_alloc     malloc
#   define mem_free      free
#   define mem_realloc   realloc
#endif

#if defined(_WIN32)
#   include <malloc.h>
static inline int mem_allocated_size(void* p) { return (int)_msize(p); }
#elif defined(__APPLE__)
#   include <malloc/malloc.h>
#   define mem_allocated_size  malloc_size
#else
#   include <malloc.h>
#   define mem_allocated_size  malloc_usable_size
#endif

C_BEGIN

typedef uint32_t mem_size;
typedef int32_t mem_idx;

/*!
 * @brief Initializes memory tracking for the current thread. Must be
 * called for every thread. This is called from cs_threadlocal_init().
 *
 * In release mode this does nothing. In debug mode it will initialize
 * memory reports and backtraces, if enabled.
 */
VH_PRIVATE_API int
mem_threadlocal_init(void);

/*!
 * @brief De-initializes memory tracking for the current thread. This is called
 * from cs_threadlocal_deinit().
 *
 * In release mode this does nothing. In debug mode this will output the memory
 * report and print backtraces, if enabled.
 * @return Returns the number of memory leaks.
 */
VH_PRIVATE_API mem_size
mem_threadlocal_deinit(void);

#if defined(VH_MEM_DEBUGGING)

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PUBLIC_API void*
mem_alloc(mem_size size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PUBLIC_API void*
mem_realloc(void* ptr, mem_size new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PUBLIC_API void
mem_free(void*);

#endif

VH_PUBLIC_API mem_size
mem_get_num_allocs(void);

VH_PUBLIC_API mem_size
mem_get_memory_usage(void);

VH_PRIVATE_API void
mem_mutated_string_and_hex_dump(const void* data, mem_size size_in_bytes);

C_END
