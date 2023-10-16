#pragma once

#include "vh/config.h"

C_BEGIN

struct mfile
{
    void* address;
    int size;
};

VH_PUBLIC_API const char*
mfile_last_error(void);

VH_PUBLIC_API void
mfile_last_error_free(void);

VH_PUBLIC_API int
mfile_map(struct mfile* mf, const char* file_name);

VH_PUBLIC_API void
mfile_unmap(struct mfile* mf);

C_END
