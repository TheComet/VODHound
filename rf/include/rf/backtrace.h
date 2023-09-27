#pragma once

#define RF_BACKTRACE_SIZE 64

#include "rf/config.h"

C_BEGIN

RF_PRIVATE_API int
rf_backtrace_init(void);

RF_PRIVATE_API void
rf_backtrace_deinit(void);

/*!
 * @brief Generates a backtrace.
 * @param[in] size The maximum number of frames to walk.
 * @return Returns an array of char* arrays.
 * @note The returned array must be freed manually with FREE(returned_array).
 */
RF_PRIVATE_API char**
rf_backtrace_get(int* size);

RF_PRIVATE_API void
rf_backtrace_free(char** bt);

C_END
