#include "vh/dynlib.h"
#include "vh/log.h"

#include <dlfcn.h>

void*
dynlib_open(const char* file_name)
{
    void* handle = dlopen(file_name, RTLD_LAZY);
    if (handle == NULL)
        log_err("Failed to load shared library '%s': %s\n", file_name, dlerror());
    return handle;
}

void
dynlib_close(void* handle)
{
    dlclose(handle);
}

void*
dynlib_lookup_symbol(void* handle, const char* name)
{
    return dlsym(handle, name);
}
