#include "rf/dynlib.h"
#include "rf/log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void*
dynlib_open(const char* file_name)
{
    HMODULE hModule = LoadLibraryA(file_name);
    if (hModule == NULL)
        rf_log_err("Failed to load shared library '%s': %s\n", file_name, dlerror());
    return (void*)hModule;
}

void
dynlib_close(void* handle)
{
    HMODULE hModule = (HMODULE)handle;
    FreeLibrary(hModule);
}

void*
dynlib_lookup_symbol(void* handle, const char* name)
{
    HMODULE hModule = (HMODULE)handle;
    return (void*)GetProcAddress(hModule, name);
}
