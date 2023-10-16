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

struct fs_list_ctx
{
    struct path file_path;
    struct str_view subdir;
    void* lib;
    int (*on_plugin)(struct plugin plugin, void* user);
    void* on_plugin_user;
};

static int on_symbol(const char* sym, void* user)
{
    struct plugin plugin;
    struct fs_list_ctx* ctx = user;
    if (!cstr_starts_with(cstr_view(sym), "vh_plugin"))
        return 0;

    plugin.handle = ctx->lib;
    plugin.i = dynlib_symbol_addr(ctx->lib, sym);
    if (plugin.i)
    {
        log_dbg("  * %s by %s: %s\n", plugin.i->name, plugin.i->author, plugin.i->description);
        int ret = ctx->on_plugin(plugin, ctx->on_plugin_user);
        if (ret > 0)
            log_info("Loading plugin %s by %s: %s\n", plugin.i->name, plugin.i->author, plugin.i->description);
        return ret;
    }

    return 0;  /* Continue */
}

static int on_filename(const char* name, void* user)
{
    struct fs_list_ctx* ctx = user;
    struct str_view fname = cstr_view(name);

    /* Only consider files that have the same name as their parent directy */
    if (!str_equal(str_remove_file_ext(fname), ctx->subdir))
        return 0;

    /* PDB files on Windows */
#if defined(_DEBUG)
    if (cstr_ends_with(fname, ".pdb"))
        return 0;
#endif

    /*
     * Join path to shared lib and try to load it.
     * share/VODHound/plugins/subdir/subdir.so
     */
    if (path_join(&ctx->file_path, fname) != 0)
        return -1;
    path_terminate(&ctx->file_path);

    ctx->lib = dynlib_open(ctx->file_path.str.data);
    if (ctx->lib == NULL)
    {
        log_err("! Failed to load plugin %s\n", ctx->file_path.str.data);
        path_dirname(&ctx->file_path);
        return 0;  /* Try to continue*/
    }

    log_dbg("+ Found plugin %s\n", ctx->file_path.str.data);
    path_dirname(&ctx->file_path);

    int ret = dynlib_symbol_table(ctx->lib, on_symbol, ctx);

    /*
     * If the callback returns positive it means they want to use the plugin.
     * Ownership of the library handle is transferred to them. Stop iterating.
     */
    if (ret <= 0)
        dynlib_close(ctx->lib);

    return ret;
}

static int on_subdir(const char* subdir, void* user)
{
    struct fs_list_ctx* ctx = user;
    ctx->subdir = cstr_view(subdir);

    /*
     * Search in each subdirectory for a shared library matching the
     * directory's name. For example, if there is a directory "myplugin/",
     * then inside it there should be a "myplugin/myplugin.so/dll" file.
     */
    if (path_set(&ctx->file_path, PLUGIN_DIR) != 0)    /* share/VODHound/plugins */
        return -1;
    if (path_join(&ctx->file_path, ctx->subdir) != 0)  /* share/VODHound/plugins/subdir */
        return -1;

    /*
     * It is necessary to add the plugin directory to the DLL/library search
     * path. Otherwise the plugin won't be able to load its dependencies, if
     * it has any.
     */
    path_terminate(&ctx->file_path);
    if (dynlib_add_path(ctx->file_path.str.data) != 0)
        return 0;  /* Try to continue */

    return fs_list(str_view(ctx->file_path.str), on_filename, ctx);
}

int
plugins_scan(int (*on_plugin)(struct plugin plugin, void* user), void* user)
{
    int ret;
    struct fs_list_ctx ctx;

    path_init(&ctx.file_path);
    ctx.on_plugin = on_plugin;
    ctx.on_plugin_user = user;

    /* Scan for all folders/files in plugin directory */
    log_dbg("Scanning for plugins in %s\n", PLUGIN_DIR.data);
    ret = fs_list(PLUGIN_DIR, on_subdir, &ctx);

    path_deinit(&ctx.file_path);
    return ret;
}

struct plugin_load_ctx
{
    struct plugin* plugin;
    struct str_view name;
};

static int plugin_load_on_plugin(struct plugin plugin, void* user)
{
    struct plugin_load_ctx* ctx = user;
    if (cstr_equal(ctx->name, plugin.i->name))
    {
        *ctx->plugin = plugin;
        return 1;
    }
    return 0;
}

int
plugin_load(struct plugin* plugin, struct str_view name)
{
    struct plugin_load_ctx ctx = { plugin, name };
    if (plugins_scan(plugin_load_on_plugin, &ctx) != 1)
        return -1;
    return 0;
}

void
plugin_unload(struct plugin* plugin)
{
    struct plugin_interface* pi = plugin->i;
    log_info("Unloading plugin %s by %s: %s\n", pi->name, pi->author, pi->description);
    dynlib_close(plugin->handle);
}
