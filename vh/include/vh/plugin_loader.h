#pragma once

#include "vh/config.h"
#include "vh/str.h"

C_BEGIN

struct plugin
{
	void* handle;
	struct plugin_interface* i;
};

VH_PUBLIC_API int
plugin_scan(struct strlist* names);

VH_PUBLIC_API int
plugin_load(struct plugin* plugin, struct str_view name);

VH_PUBLIC_API void
plugin_unload(struct plugin* plugin);

C_END
