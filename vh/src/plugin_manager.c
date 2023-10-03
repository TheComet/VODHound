#include "vh/dynlib.h"
#include "vh/log.h"
#include "vh/plugin_manager.h"
#include "vh/plugin.h"
#include "vh/str.h"

#include <sys/types.h>
#include <dirent.h>
#include <stddef.h>

#define PLUGIN_DIR "./share/VODHound/plugins/"

VH_PUBLIC_API struct plugin*
plugin_create(const char* name)
{
    DIR* dp;
    struct dirent* ep;
    char fname[256] = PLUGIN_DIR;
    fname[255] = '\0';

    dp = opendir(PLUGIN_DIR);
    if (!dp)
    {
        log_err("Error while searching for plugins: Directory '" PLUGIN_DIR "' does not exist\n");
        return NULL;
    }

    while ((ep = readdir(dp)) != NULL)
    {
        if (!cstr_ends_with(cstr_view(ep->d_name), ".so"))
            continue;

        /*
         * Need relative path from working directory. Copy at most N-1 bytes
         * as to not overwrite the last '\0' character ini the buffer, as
         * strncpy() does not always put a null terminator at the end of N is
         * reached
         */
        strncpy(fname + sizeof(PLUGIN_DIR) - 1, ep->d_name, 256 - sizeof(PLUGIN_DIR) - 1);
        void* lib = dynlib_open(fname);
        if (lib == NULL)
            continue;

        struct plugin_interface* i = dynlib_lookup_symbol(lib, "plugin");
        if (i == NULL)
        {
            dynlib_close(lib);
            continue;
        }

        for (struct plugin_factory* f = i->factories; f->name; f++)
            log_dbg("%s: %s\n", f->name, f->description);

        dynlib_close(lib);
    }

    closedir(dp);
    return NULL;
}

VH_PUBLIC_API void
plugin_destroy(struct plugin* plugin)
{

}
