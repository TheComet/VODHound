#include "vh/fs.h"

#include <sys/types.h>
#include <dirent.h>

static int match_all(struct str_view str) { (void)str;  return 1; }

void
path_set_take(struct path* path, struct str str)
{
    path->str = str;
    str_replace_char(&path->str, '\\', '/');
}

int
path_set(struct path* path, struct str_view str)
{
    if (str_set(&path->str, str) != 0)
        return -1;
    str_replace_char(&path->str, '\\', '/');
    return 0;
}

int
path_join(struct path* path, struct str_view trailing)
{
    if (path->str.len && path->str.data[path->str.len - 1] != '/'
            && path->str.data[path->str.len - 1] != '\\')
        if (cstr_append(&path->str, "/") != 0)
            return -1;
    if (str_append(&path->str, trailing) != 0)
        return -1;
    str_replace_char(&path->str, '\\', '/');
    return 0;
}

int
fs_dir_files(struct strlist* out, const char* path)
{
    return fs_dir_files_matching(out, path, match_all);
}

int
fs_dir_files_matching(struct strlist* out, const char* path, int (*match)(struct str_view str))
{
    DIR* dp;
    struct dirent* ep;
    struct path correct_path;

    path_init(&correct_path);
    if (path_set(&correct_path, cstr_view(path)) != 0)
        goto str_set_failed;
    if (path_terminate(&correct_path) != 0)
        goto first_file_failed;

    dp = opendir(correct_path.str.data);
    if (!dp)
        goto first_file_failed;

    while ((ep = readdir(dp)) != NULL)
    {
        if (ep->d_type == DT_REG)
        {
            struct str_view fname = cstr_view(ep->d_name);
            if (match(fname))
                if (strlist_add(out, fname) != 0)
                    goto error;
        }
    }

    closedir(dp);
    path_deinit(&correct_path);

    return 0;

    error             : closedir(dp);
    first_file_failed : path_deinit(&correct_path);
    str_set_failed    : return -1;
}
