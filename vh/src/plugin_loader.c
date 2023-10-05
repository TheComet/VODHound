#include "vh/dynlib.h"
#include "vh/log.h"
#include "vh/plugin_loader.h"
#include "vh/plugin.h"
#include "vh/str.h"
#include "vh/fs.h"

#include <stddef.h>

#define PLUGIN_DIR_ "share/VODHound/plugins"
static struct str_view PLUGIN_DIR = {
    PLUGIN_DIR_,
    sizeof(PLUGIN_DIR_) - 1
};

static int match_expected_filename(struct str_view str, const void* param)
{
    const struct str_view* expected = param;
    return str_equal(str_remove_file_ext(str), *expected);
}

int
plugin_scan(struct strlist* names)
{
    struct strlist subdirs;
    struct strlist files;
    struct path file_path;

    strlist_init(&subdirs);
    strlist_init(&files);
    path_init(&file_path);

    if (fs_list(&subdirs, PLUGIN_DIR) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&subdirs); ++i)
    {
        struct str_view fname = strlist_get(&subdirs, i);
        if (path_set(&file_path, PLUGIN_DIR) != 0)
            goto error;
        if (path_join(&file_path, fname) != 0)
            goto error;
        if (fs_list_matching(&files, str_view(file_path.str), match_expected_filename, &fname) != 0)
            goto error;
        if (strlist_count(&files) == 0)
            continue;

        path_terminate(&file_path);
        if (dynlib_add_path(file_path.str.data) != 0)
            continue;

        if (path_join(&file_path, strlist_get(&files, 0)) != 0)
            goto error;
        path_terminate(&file_path);

        void* lib = dynlib_open(file_path.str.data);
        if (lib == NULL)
            continue;

        struct plugin_interface* pi = dynlib_lookup_symbol(lib, "plugin");
        if (pi == NULL)
        {
            dynlib_close(lib);
            continue;
        }

        if (strlist_add(names, cstr_view(pi->name)) != 0)
        {
            dynlib_close(lib);
            goto error;
        }

        dynlib_close(lib);
    }

    return 0;

error:
list_dir_failed:
    path_deinit(&file_path);
    strlist_deinit(&files);
    strlist_deinit(&subdirs);
    return -1;
}

int
plugin_load(struct plugin* plugin, struct str_view name)
{
    struct strlist subdirs;
    struct strlist files;
    struct path file_path;

    strlist_init(&subdirs);
    strlist_init(&files);
    path_init(&file_path);

    if (fs_list(&subdirs, PLUGIN_DIR) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&subdirs); ++i)
    {
        struct str_view fname = strlist_get(&subdirs, i);
        if (path_set(&file_path, PLUGIN_DIR) != 0)
            goto error;
        if (path_join(&file_path, fname) != 0)
            goto error;
        if (fs_list_matching(&files, str_view(file_path.str), match_expected_filename, &fname) != 0)
            goto error;
        if (strlist_count(&files) == 0)
            continue;

        path_terminate(&file_path);
        if (dynlib_add_path(file_path.str.data) != 0)
            continue;

        if (path_join(&file_path, strlist_get(&files, 0)) != 0)
            goto error;
        path_terminate(&file_path);

        log_dbg("%s\n", file_path.str.data);

        void* lib = dynlib_open(file_path.str.data);
        if (lib == NULL)
            continue;

        struct plugin_interface* pi = dynlib_lookup_symbol(lib, "plugin");
        if (pi == NULL)
        {
            dynlib_close(lib);
            continue;
        }

        if (cstr_equal(name, pi->name))
        {
            plugin->handle = lib;
            plugin->i = pi;
            goto success;
        }

        dynlib_close(lib);
    }

success:
    path_deinit(&file_path);
    strlist_deinit(&files);
    strlist_deinit(&subdirs);
    return 0;

error:
list_dir_failed : 
    path_deinit(&file_path);
    strlist_deinit(&files);
    strlist_deinit(&subdirs);
    return -1;
}

void
plugin_unload(struct plugin* plugin)
{
    dynlib_close(plugin->handle);
}
