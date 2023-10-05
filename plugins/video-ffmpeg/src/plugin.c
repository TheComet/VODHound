#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

struct plugin_data
{
	int foo;
};

static struct plugin_data*
create(void)
{
	return mem_alloc(sizeof(struct plugin_data));
}

static void
destroy(struct plugin_data* plugin)
{
	mem_free(plugin);
}

void* ui_create(struct plugin_data* plugin) { return NULL; }
void ui_destroy(struct plugin_data* plugin, void* ui) {}

struct ui_interface ui = {
	ui_create,
	ui_destroy
};

int video_open_file(struct plugin_data* plugin, const char* file_name, int pause) { return -1; }
void video_close(struct plugin_data* plugin) {}
int video_is_open(struct plugin_data* plugin) { return 0; }
void video_play(struct plugin_data* plugin) {}
void video_pause(struct plugin_data* plugin) {}
void video_step(struct plugin_data* plugin, int frames) {}
void video_seek(struct plugin_data* plugin, uint64_t offset, int num, int den) {}
uint64_t video_offset(struct plugin_data* plugin, int num, int den) { return 0; }
uint64_t video_duration(struct plugin_data* plugin, int num, int den) { return 0; }
int video_is_playing(struct plugin_data* plugin) { return 0; }
void video_set_volume(struct plugin_data* plugin, int percent) {}
int video_volume(struct plugin_data* plugin) { return 0; }

struct video_player_interface controls = {
	video_open_file,
	video_close,
	video_is_open,
	video_play,
	video_pause,
	video_step,
	video_seek,
	video_offset,
	video_duration,
	video_is_playing,
	video_set_volume,
	video_volume
};

PLUGIN_API struct plugin_interface plugin = {
	PLUGIN_VERSION,
	0,
	create, destroy, &ui, &controls,
	"FFmpeg Video Player",
	"video",
	"TheComet",
	"@TheComet93",
	"Decodes videos using FFmpeg libraries and displays them on an OpenGL 3.3 surface."
};
