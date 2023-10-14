#pragma once

#include "vh/config.h"

C_BEGIN

VH_PUBLIC_API int
vh_init(void);

VH_PUBLIC_API void
vh_deinit(void);

VH_PUBLIC_API int
vh_threadlocal_init(void);

VH_PUBLIC_API void
vh_threadlocal_deinit(void);

C_END
