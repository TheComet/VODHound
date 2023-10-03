#pragma once

#include "vh/config.h"

C_BEGIN

struct plugin;

VH_PUBLIC_API struct plugin*
plugin_create(const char* name);

VH_PUBLIC_API void
plugin_destroy(struct plugin* plugin);

C_END
