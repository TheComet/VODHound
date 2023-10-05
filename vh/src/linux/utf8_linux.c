#include "vh/utf8.h"

FILE*
utf8_fopen_wb(const char* utf8_filename, int len)
{
    (void)len;
    return fopen(utf8_filename, "wb");
}

int
utf8_remove(const char* utf8_filename, int len)
{
    (void)len;
    return remove(utf8_filename);
}
