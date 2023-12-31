#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "vh/mem.h"
#include "vh/mfile.h"
#include "vh/utf8.h"

static char* last_error;
const char*
mfile_last_error(void)
{
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&last_error,
        0,
        NULL);
    return last_error;
}
void
mfile_last_error_free(void)
{
    if (last_error)
        LocalFree(last_error);
    last_error = NULL;
}

int
mfile_map_read(struct mfile* mf, const char* utf8_filename)
{
    HANDLE hFile;
    LARGE_INTEGER liFileSize;
    HANDLE mapping;
    wchar_t* utf16_filename;

    utf16_filename = utf8_to_utf16(utf8_filename, (int)strlen(utf8_filename));
    if (utf16_filename == NULL)
        goto utf16_conv_failed;

    /* Try to open the file */
    hFile = CreateFileW(
        utf16_filename,         /* File name */
        GENERIC_READ,           /* Read only */
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,                   /* Default security */
        OPEN_EXISTING,          /* File must exist */
        FILE_ATTRIBUTE_NORMAL,  /* Default attributes */
        NULL);                  /* No attribute template */
    if (hFile == INVALID_HANDLE_VALUE)
        goto open_failed;

    /* Determine file size in bytes */
    if (!GetFileSizeEx(hFile, &liFileSize))
        goto get_file_size_failed;
    if (liFileSize.QuadPart > (1ULL << 32) - 1)  /* mf->size is an int */
        goto get_file_size_failed;

    mapping = CreateFileMapping(
        hFile,                 /* File handle */
        NULL,                  /* Default security attributes */
        PAGE_READONLY,         /* Read only (or copy on write, but we don't write) */
        0, 0,                  /* High/Low size of mapping. Zero means entire file */
        NULL);                 /* Don't name the mapping */
    if (mapping == NULL)
        goto create_file_mapping_failed;

    mf->address = MapViewOfFile(
        mapping,               /* File mapping handle */
        FILE_MAP_COPY,         /* Read-only view of file */
        0, 0,                  /* High/Low offset of where the mapping should begin in the file */
        0);                    /* Length of mapping. Zero means entire file */
    if (mf->address == NULL)
        goto map_view_failed;

    /* The file mapping isn't required anymore */
    CloseHandle(mapping);
    CloseHandle(hFile);
    utf_free(utf16_filename);

    mem_track_allocation(mf->address);
    mf->size = liFileSize.QuadPart;

    return 0;

    map_view_failed            : CloseHandle(mapping);
    create_file_mapping_failed :
    get_file_size_failed       : CloseHandle(hFile);
    open_failed                : utf_free(utf16_filename);
    utf16_conv_failed          : return -1;
}

void mfile_unmap(struct mfile* mf)
{
    mem_track_deallocation(mf->address);
    UnmapViewOfFile(mf->address);
}
