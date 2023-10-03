#include "vh/mem.h"
#include "vh/utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdlib.h>

wchar_t*
utf8_to_utf16(const char* utf8, int utf8_bytes)
{
    int utf16_bytes = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, NULL, 0);
    if (utf16_bytes == 0)
        return NULL;

    wchar_t* utf16 = (wchar_t*)malloc((sizeof(wchar_t) + 1) * utf16_bytes);
    if (utf16 == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_bytes, utf16, utf16_bytes) == 0)
    {
        free(utf16);
        return NULL;
    }

    utf16[utf16_bytes] = 0;

    return utf16;
}

void
utf16_free(wchar_t* utf16)
{
    free(utf16);
}

FILE*
fopen_utf8_wb(const char* utf8_filename, int len)
{
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, len);
    if (utf16_filename == NULL)
        return NULL;

    FILE* fp = _wfopen(utf16_filename, L"wb");
    free(utf16_filename);

    return fp;
}

int
remove_utf8(const char* utf8_filename, int len)
{
    wchar_t* utf16_filename = utf8_to_utf16(utf8_filename, len);
    if (utf16_filename == NULL)
        return -1;

    int result = _wremove(utf16_filename);
    free(utf16_filename);
    return result;
}
