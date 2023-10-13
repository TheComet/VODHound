#pragma once

#include "vh/config.h"
#include "vh/str.h"
#include "vh/vec.h"

C_BEGIN

struct plugin
{
    void* handle;
    struct plugin_interface* i;
};

VH_PUBLIC_API int
plugins_scan(int (*on_plugin)(struct plugin plugin, void* user), void* user);

VH_PUBLIC_API int
plugin_load(struct plugin* plugin, struct str_view name);

VH_PUBLIC_API void
plugin_unload(struct plugin* plugin);

C_END
