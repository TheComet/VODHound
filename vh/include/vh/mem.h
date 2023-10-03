#pragma once

#include "vh/config.h"
#include <stdint.h>

#if defined(VH_MEMORY_DEBUGGING)
#   define MALLOC   mem_malloc
#   define FREE     mem_free
#   define REALLOC  mem_realloc
#else
#   include <stdlib.h>
#   define MALLOC   malloc
#   define FREE     free
#   define REALLOC  realloc
#endif

C_BEGIN

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
VH_PRIVATE_API size_t
mem_threadlocal_deinit(void);

#if defined(VH_MEMORY_DEBUGGING)

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PUBLIC_API void*
mem_malloc(size_t size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PUBLIC_API void*
mem_realloc(void* ptr, size_t new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
VH_PRIVATE_API void
mem_free(void*);

#endif

VH_PUBLIC_API size_t
mem_get_num_allocs(void);

VH_PUBLIC_API size_t
mem_get_memory_usage(void);

VH_PRIVATE_API void
mem_mutated_string_and_hex_dump(const void* data, size_t size_in_bytes);

C_END