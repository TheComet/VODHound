#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

struct plugin
{
	int foo;
};

static int
start(uint32_t version)
{
	return 0;
}

static void
stop(void)
{

}

static struct plugin*
create(void)
{
	return MALLOC(sizeof(struct plugin));
}

static void
destroy(struct plugin* plugin)
{
	FREE(plugin);
}

struct plugin_interface plugin = {
	PLUGIN_VERSION,
	start,
	stop,
	{
		{ create, destroy, NULL, NULL,
		  "FFMpeg Video",
		  "video",
		  "TheComet",
		  "@TheComet93",
		  "Plays videos" },

		{ NULL }
	}
};
