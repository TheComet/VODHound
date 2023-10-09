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
dynlib_symbol_addr(void* handle, const char* name)
{
    HMODULE hModule = (HMODULE)handle;
    return (void*)GetProcAddress(hModule, name);
}

static PIMAGE_EXPORT_DIRECTORY
get_exports_directory(HMODULE hModule)
{
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hModule;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)
        ((BYTE*)hModule + dos_header->e_lfanew);
    if (header->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    if (header->OptionalHeader.NumberOfRvaAndSizes <= 0)
        return 0;

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)
        ((BYTE*)hModule +
            header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    if (exports->AddressOfNames == 0)
        return 0;
    return exports;
}

int
dynlib_symbol_count(void* handle)
{
    HMODULE hModule = (HMODULE)handle;
    PIMAGE_EXPORT_DIRECTORY exports = get_exports_directory(hModule);
    if (exports == NULL)
        return 0;
    return exports->NumberOfNames;
}

const char*
dynlib_symbol_at(void* handle, int idx)
{
    HMODULE hModule = (HMODULE)handle;
    PIMAGE_EXPORT_DIRECTORY exports = get_exports_directory(hModule);
    if (exports == NULL)
        return 0;

    char** names = (char**)((BYTE*)hModule + exports->AddressOfNames);
    char** func = (char**)((BYTE*)hModule + exports->AddressOfFunctions);
    return (const char*)names[idx];
}

int
dynlib_string_count(void* handle)
{
    const char* symbol;
    int table_size = 0;
    HMODULE hModule = (HMODULE)handle;

    /* Index starts at 1 */
    while (LoadStringA(hModule, table_size + 1, (char*)&symbol, 0) > 0)
        table_size++;

    return 0;
}

struct str_view
dynlib_string_at(void* handle, int idx)
{
    const char* symbol;
    int len;
    HMODULE hModule = (HMODULE)handle;

    /* Index starts at 1 */
    len = LoadStringA(hModule, idx + 1, (char*)&symbol, 0);
    return cstr_view2(symbol, len);
}
