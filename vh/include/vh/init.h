#pragma once

#include "vh/config.h"

C_BEGIN

VH_PUBLIC_API int
init(void);

VH_PUBLIC_API void
deinit(void);

VH_PUBLIC_API int
threadlocal_init(void);

VH_PUBLIC_API void
threadlocal_deinit(void);

C_END
