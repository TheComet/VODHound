#include "vh/dynlib.h"
#include "vh/log.h"
#include "vh/plugin_loader.h"
#include "vh/plugin.h"
#include "vh/str.h"
#include "vh/fs.h"

#include <stddef.h>

#define PLUGIN_DIR "share/VODHound/plugins"

static int match_dynlib_extension(struct str_view str)
{
    return cstr_ends_with(str, ".so") ||
        cstr_ends_with(str, ".dll") ||
        cstr_ends_with(str, ".Dll") ||
        cstr_ends_with(str, ".DLL");
}

int
plugin_scan(struct strlist* names)
{
    struct strlist plugin_files;
    struct path fname;

    path_init(&fname);
    strlist_init(&plugin_files);

    if (fs_dir_files_matching(&plugin_files, PLUGIN_DIR, match_dynlib_extension) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&plugin_files); ++i)
    {
        if (path_set(&fname, cstr_view(PLUGIN_DIR)) != 0)
            goto error;
        if (path_join(&fname, strlist_get(&plugin_files, i)) != 0)
            goto error;
        if (path_terminate(&fname) != 0)
            goto error;

        void* lib = dynlib_open(fname.str.data);
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
    strlist_deinit(&plugin_files);
    path_deinit(&fname);
    return -1;
}

int
plugin_load(struct plugin* plugin, struct str_view name)
{
    struct strlist plugin_files;
    struct path fname;

    path_init(&fname);
    strlist_init(&plugin_files);

    if (fs_dir_files_matching(&plugin_files, PLUGIN_DIR, match_dynlib_extension) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&plugin_files); ++i)
    {
        if (path_set(&fname, cstr_view(PLUGIN_DIR)) != 0)
            goto error;
        if (path_join(&fname, strlist_get(&plugin_files, i)) != 0)
            goto error;
        if (path_terminate(&fname) != 0)
            goto error;
        log_dbg("%s\n", fname.str.data);

        void* lib = dynlib_open(fname.str.data);
        if (lib == NULL)
            continue;

        struct plugin_interface* pi = dynlib_lookup_symbol(lib, "plugin");
        if (pi == NULL)
        {
            dynlib_close(lib);
            continue;
        }

        if (cstr_cmp(name, pi->name) == 0)
        {
            plugin->handle = lib;
            plugin->i = pi;
            goto success;
        }

        dynlib_close(lib);
    }

success:
    strlist_deinit(&plugin_files);
    path_deinit(&fname);
    return 0;

error:
list_dir_failed : 
    strlist_deinit(&plugin_files);
    path_deinit(&fname);
    return -1;
}

void
plugin_unload(struct plugin* plugin)
{
    dynlib_close(plugin->handle);
}
