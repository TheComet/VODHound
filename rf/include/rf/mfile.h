#pragma once

#include "rf/config.h"

C_BEGIN

struct rf_mfile
{
    void* address;
    int size;
};

int
rf_mfile_map(struct rf_mfile* mf, const char* file_name);

void
rf_mfile_unmap(struct rf_mfile* mf);

C_END
