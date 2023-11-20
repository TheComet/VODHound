#pragma once

#include "vh/config.h"
#include "vh/str.h"
#include "vh/vec.h"

C_BEGIN

struct plugin_lib
{
    void* handle;
    struct plugin_interface* i;
};

VH_PUBLIC_API int
plugins_scan(int (*on_plugin)(struct plugin_lib plugin, void* user), void* user);

VH_PUBLIC_API int
plugin_load(struct plugin_lib* plugin, struct str_view name);

VH_PUBLIC_API void
plugin_unload(struct plugin_lib* plugin);

C_END
