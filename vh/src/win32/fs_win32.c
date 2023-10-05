#include "vh/fs.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static int match_all(struct str_view str, const void* param) { (void)str; (void)param;  return 1; }

void
path_set_take(struct path* path, struct str str)
{
    path->str = str;
    str_replace_char(&path->str, '/', '\\');
}

int
path_set(struct path* path, struct str_view str)
{
    if (str_set(&path->str, str) != 0)
        return -1;
    str_replace_char(&path->str, '/', '\\');
    return 0;
}

int
path_join(struct path* path, struct str_view trailing)
{
    if (path->str.len && path->str.data[path->str.len - 1] != '/'
            && path->str.data[path->str.len - 1] != '\\')
        if (cstr_append(&path->str, "\\") != 0)
            return -1;
    if (str_append(&path->str, trailing) != 0)
        return -1;
    str_replace_char(&path->str, '/', '\\');
    return 0;
}

int
fs_list(struct strlist* out, struct str_view path)
{
    return fs_list_matching(out, path, match_all, NULL);
}

int
fs_list_matching(
    struct strlist* out,
    struct str_view path,
    int (*match)(struct str_view str, const void* param),
    const void* param)
{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffd;
    DWORD dwError;
    struct path correct_path;

    path_init(&correct_path);
    if (path_set(&correct_path, path) != 0)
        goto str_set_failed;
    /* Using cstr_view2() here so correct_path is null terminated */
    if (path_join(&correct_path, cstr_view2("*", 2)) != 0)
        goto first_file_failed;

    hFind = FindFirstFileA(correct_path.str.data, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        goto first_file_failed;

    do
    {
        struct str_view fname = cstr_view(ffd.cFileName);
        if (cstr_equal(fname, ".") || cstr_equal(fname, ".."))
            continue;
        if (match(fname, param))
            if (strlist_add(out, fname) != 0)
                goto error;
    } while (FindNextFile(hFind, &ffd) != 0);

    dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
        goto error;

    FindClose(hFind);
    path_deinit(&correct_path);

    return 0;

    error             : FindClose(hFind);
    first_file_failed : path_deinit(&correct_path);
    str_set_failed    : return -1;
}