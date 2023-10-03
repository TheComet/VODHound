#include "rf/dynlib.h"
#include "rf/log.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static char*
last_error_create(void)
{
    char* str;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&str,
        0,
        NULL);
    return str;
}
static void
last_error_free(char* str)
{
    LocalFree(str);
}

void*
dynlib_open(const char* file_name)
{
    HMODULE hModule = LoadLibraryA(file_name);
    if (hModule == NULL)
    {
        char* error = last_error_create();
        rf_log_err("Failed to load shared library '%s': %s\n", file_name, error);
        last_error_free(error);
    }
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
