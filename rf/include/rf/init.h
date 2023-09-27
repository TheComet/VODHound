#pragma once

#include "rf/config.h"

C_BEGIN

RF_PUBLIC_API int
rf_init(void);

RF_PUBLIC_API void
rf_deinit(void);

RF_PUBLIC_API int
rf_threadlocal_init(void);

RF_PUBLIC_API void
rf_threadlocal_deinit(void);

C_END
