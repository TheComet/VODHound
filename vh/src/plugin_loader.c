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

static int match_plugin_symbols(struct str_view str, const void* param)
{
    return cstr_starts_with(str, "plugin");
}

int
plugin_scan(struct strlist* plugin_names)
{
    struct strlist subdirs;
    struct strlist names;
    struct path file_path;

    strlist_init(&subdirs);
    strlist_init(&names);
    path_init(&file_path);

    /* Scan for all folders/files in plugin directory */
    log_info("Scanning for plugins in %s\n", PLUGIN_DIR.data);
    if (fs_list(&subdirs, PLUGIN_DIR) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&subdirs); ++i)
    {
        /*
         * Search in each subdirectory for a shared library matching the
         * directory's name. For example, if there is a directory "myplugin/",
         * then inside it there should be a "myplugin/myplugin.so/dll" file.
         */
        struct str_view subdir = strlist_get(&subdirs, i);
        if (path_set(&file_path, PLUGIN_DIR) != 0)
            goto error;
        if (path_join(&file_path, subdir) != 0)
            goto error;
        strlist_clear(&names);
        if (fs_list_matching(&names, str_view(file_path.str), match_expected_filename, &subdir) != 0)
            goto error;
        if (strlist_count(&names) == 0)
            continue;

        /*
         * On Windows, it is necessary to add the plugin directory to the DLL
         * search path. Otherwise the plugin won't be able to load its dependencies,
         * if it has any.
         */
        path_terminate(&file_path);
        if (dynlib_add_path(file_path.str.data) != 0)
            continue;

        /*
         * Join path to shared lib, so it becomes e.g. "myplugin/myplugin.so",
         * and try to load it.
         */
        if (path_join(&file_path, strlist_get(&names, 0)) != 0)
            goto error;
        path_terminate(&file_path);
        void* lib = dynlib_open(file_path.str.data);
        if (lib == NULL)
            continue;

        /*
         * Collect all exported symbols that start with "plugin".
         * NOTE: We are re-using the variable "files" here to save on malloc()
         * calls
         */
        strlist_clear(&names);
        if (dynlib_symbol_table_filtered(lib, &names, match_plugin_symbols, NULL) != 0)
        {
            dynlib_close(lib);
            goto error;
        }
        if (strlist_count(&names) == 0)
        {
            dynlib_close(lib);
            continue;
        }

        log_info("+ Found plugin %s\n", file_path.str.data);

        /* Re-use buffer of file_path to null-terminate the symbol name */
        struct str symbol_name = str_take(&file_path.str);
        for (int s = 0; s != (int)strlist_count(&names); ++s)
        {
            if (str_set(&symbol_name, strlist_get(&names, s)) != 0)
            {
                file_path.str = str_take(&symbol_name);
                dynlib_close(lib);
                goto error;
            }
            str_terminate(&symbol_name);

            struct plugin_interface* pi = dynlib_symbol_addr(lib, symbol_name.data);
            if (pi)
                log_info("  * %s by %s: %s\n", pi->name, pi->author, pi->description);
            else
            {
                log_warn("  ! Failed to load symbol '%s': %s\n", dynlib_last_error());
                continue;
            }

            if (strlist_add(plugin_names, cstr_view(pi->name)) != 0)
            {
                file_path.str = str_take(&symbol_name);
                dynlib_close(lib);
                goto error;
            }
        }

        file_path.str = str_take(&symbol_name);
        dynlib_close(lib);
    }

    return 0;

error:
list_dir_failed:
    path_deinit(&file_path);
    strlist_deinit(&names);
    strlist_deinit(&subdirs);
    return -1;
}

int
plugin_load(struct plugin* plugin, struct str_view name)
{
    struct strlist subdirs;
    struct strlist names;
    struct path file_path;

    strlist_init(&subdirs);
    strlist_init(&names);
    path_init(&file_path);

    /* Scan for all folders/files in plugin directory */
    if (fs_list(&subdirs, PLUGIN_DIR) != 0)
        goto list_dir_failed;

    for (int i = 0; i != (int)strlist_count(&subdirs); ++i)
    {
        /*
         * Search in each subdirectory for a shared library matching the
         * directory's name. For example, if there is a directory "myplugin/",
         * then inside it there should be a "myplugin/myplugin.so/dll" file.
         */
        struct str_view subdir = strlist_get(&subdirs, i);
        if (path_set(&file_path, PLUGIN_DIR) != 0)
            goto error;
        if (path_join(&file_path, subdir) != 0)
            goto error;
        strlist_clear(&names);
        if (fs_list_matching(&names, str_view(file_path.str), match_expected_filename, &subdir) != 0)
            goto error;
        if (strlist_count(&names) == 0)
            continue;

        /*
         * On Windows, it is necessary to add the plugin directory to the DLL
         * search path. Otherwise the plugin won't be able to load its dependencies,
         * if it has any.
         */
        path_terminate(&file_path);
        if (dynlib_add_path(file_path.str.data) != 0)
            continue;

        /*
         * Join path to shared lib, so it becomes e.g. "myplugin/myplugin.so",
         * and try to load it.
         */
        if (path_join(&file_path, strlist_get(&names, 0)) != 0)
            goto error;
        path_terminate(&file_path);
        void* lib = dynlib_open(file_path.str.data);
        if (lib == NULL)
            continue;

        /*
         * Collect all exported symbols that start with "plugin".
         * NOTE: We are re-using the variable "files" here to save on malloc()
         * calls
         */
        strlist_clear(&names);
        if (dynlib_symbol_table_filtered(lib, &names, match_plugin_symbols, NULL) != 0)
        {
            dynlib_close(lib);
            goto error;
        }

        /* Re-use buffer of file_path to null-terminate the symbol name */
        struct str symbol_name = str_take(&file_path.str);
        for (int s = 0; s != (int)strlist_count(&names); ++s)
        {
            if (str_set(&symbol_name, strlist_get(&names, s)) != 0)
            {
                file_path.str = str_take(&symbol_name);
                dynlib_close(lib);
                goto error;
            }
            str_terminate(&symbol_name);

            struct plugin_interface* pi = dynlib_symbol_addr(lib, symbol_name.data);
            if (pi == NULL)
            {
                file_path.str = str_take(&symbol_name);
                dynlib_close(lib);
                continue;
            }

            if (cstr_equal(name, pi->name))
            {
                log_info("Loading plugin %s by %s: %s\n", pi->name, pi->author, pi->description);
                plugin->handle = lib;
                plugin->i = pi;
                goto success;
            }
        }

        file_path.str = str_take(&symbol_name);
        dynlib_close(lib);
    }

success:
    path_deinit(&file_path);
    strlist_deinit(&names);
    strlist_deinit(&subdirs);
    return 0;

error:
list_dir_failed :
    path_deinit(&file_path);
    strlist_deinit(&names);
    strlist_deinit(&subdirs);
    return -1;
}

void
plugin_unload(struct plugin* plugin)
{
    struct plugin_interface* pi = plugin->i;
    log_info("Unloading plugin %s by %s: %s\n", pi->name, pi->author, pi->description);
    dynlib_close(plugin->handle);
}
