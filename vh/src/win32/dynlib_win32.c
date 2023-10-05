#include "vh/dynlib.h"
#include "vh/log.h"

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

int
dynlib_add_path(const char* path)
{
    /* This function does not appear to add duplicates so it's safe to call it
     * multiple times */
    if (!SetDllDirectoryA(path))
    {
        char* error = last_error_create();
        log_err("Failed to add DLL search path: %s: %s", path, error);
        last_error_free(error);
        return -1;
    }
    return 0;
}

void*
dynlib_open(const char* file_name)
{
    HMODULE hModule = LoadLibraryA(file_name);
    if (hModule == NULL)
    {
        char* error = last_error_create();
        log_err("Failed to load shared library '%s': %s\n", file_name, error);
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