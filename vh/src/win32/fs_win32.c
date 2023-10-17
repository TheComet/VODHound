#include "vh/fs.h"
#include "vh/utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

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

void
path_dirname(struct path* path)
{
    /* Trailling slashes */
    path->str.len--;
    while (path->str.data[path->str.len] == '\\')
        path->str.len--;

    while (path->str.len && path->str.data[path->str.len] != '\\')
        path->str.len--;
}

int
fs_list(struct str_view path, int (*on_entry)(const char* name, void* user), void* user)
{
    struct path correct_path;
    DWORD dwError;
    WIN32_FIND_DATA ffd;
    int ret = 0;
    HANDLE hFind = INVALID_HANDLE_VALUE;

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
        ret = on_entry(ffd.cFileName, user);
        if (ret != 0) goto out;
    } while (FindNextFile(hFind, &ffd) != 0);

    dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
        ret = -1;

    out               : FindClose(hFind);
    first_file_failed : path_deinit(&correct_path);
    str_set_failed    : return ret;
}

int
fs_file_exists(const char* file_path)
{
    return GetFileAttributes(file_path) != INVALID_FILE_ATTRIBUTES;
}

static struct str appdata_dir;

struct str_view
fs_appdata_dir(void)
{
    return str_view(appdata_dir);
}

int
fs_init(void)
{
    char* utf8_path;
    PWSTR path = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &path);
    if (FAILED(hr))
        goto get_folder_failed;

    utf8_path = utf16_to_utf8(path, (int)wcslen(path));
    if (utf8_path == NULL)
        goto utf_conversion_failed;

    appdata_dir.data = utf8_path;
    appdata_dir.len = (int)strlen(utf8_path);

    return 0;

    utf_conversion_failed : CoTaskMemFree(path);
    get_folder_failed     : return -1;
}

void
fs_deinit(void)
{
    utf_free(appdata_dir.data);
}
