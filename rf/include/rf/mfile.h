#pragma once

#include "rf/config.h"

C_BEGIN

struct mfile
{
    void* address;
    int size;
};

int
map_file(struct mfile* mf, const char* file_name);

void
unmap_file(struct mfile* mf);

C_END
