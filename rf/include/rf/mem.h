#pragma once

#include "rf/config.h"
#include <stdint.h>

#if !defined(RF_MEMORY_DEBUGGING)
#   include <stdlib.h>
#   define rf_malloc   malloc
#   define rf_free     free
#   define rf_realloc  realloc
#endif

C_BEGIN

/*!
 * @brief Initializes memory tracking for the current thread. Must be
 * called for every thread. This is called from cs_threadlocal_init().
 *
 * In release mode this does nothing. In debug mode it will initialize
 * memory reports and backtraces, if enabled.
 */
RF_PRIVATE_API int
rf_mem_threadlocal_init(void);

/*!
 * @brief De-initializes memory tracking for the current thread. This is called
 * from cs_threadlocal_deinit().
 *
 * In release mode this does nothing. In debug mode this will output the memory
 * report and print backtraces, if enabled.
 * @return Returns the number of memory leaks.
 */
RF_PRIVATE_API size_t
rf_mem_threadlocal_deinit(void);

/*!
 * @brief Does the same thing as a normal call to malloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
RF_PUBLIC_API void*
rf_malloc(size_t size);

/*!
 * @brief Does the same thing as a normal call to realloc(), but does some
 * additional work to monitor and track down memory leaks.
 */
RF_PUBLIC_API void*
rf_realloc(void* ptr, size_t new_size);

/*!
 * @brief Does the same thing as a normal call to fee(), but does some
 * additional work to monitor and track down memory leaks.
 */
RF_PRIVATE_API void
rf_free(void*);

RF_PUBLIC_API size_t
rf_mem_get_num_allocs(void);

RF_PUBLIC_API size_t
rf_mem_get_memory_usage(void);

RF_PRIVATE_API void
rf_mem_mutated_string_and_hex_dump(const void* data, size_t size_in_bytes);

C_END
