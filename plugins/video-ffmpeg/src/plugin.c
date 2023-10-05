#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/decoder.h"
#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

struct plugin_data
{
    struct canvas* canvas;
    struct decoder decoder;
    struct gfx* gfx;
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

void* ui_create(struct plugin_data* plugin)
{
    plugin->canvas = canvas_create();
    if (plugin->canvas == NULL)
        return NULL;

    plugin->gfx = gfx_create(plugin->canvas);
    if (plugin->gfx == NULL)
    {
        canvas_destroy(plugin->canvas);
        return NULL;
    }

    return canvas_get_native_handle(plugin->canvas);
}
void ui_destroy(struct plugin_data* plugin, void* ui)
{
    if (ui == canvas_get_native_handle(plugin->canvas))
    {
        gfx_destroy(plugin->gfx, plugin->canvas);
        canvas_destroy(plugin->canvas);
    }
}

void ui_main(struct plugin_data* plugin, void* ui)
{
    if (ui == canvas_get_native_handle(plugin->canvas))
        canvas_main_loop(plugin->canvas);
}

struct ui_interface ui = {
    ui_create,
    ui_destroy,
    ui_main
};

int video_open_file(struct plugin_data* plugin, const char* file_name, int pause) { return decoder_open_file(&plugin->decoder, file_name, pause); }
void video_close(struct plugin_data* plugin) { decoder_close(&plugin->decoder); }
int video_is_open(struct plugin_data* plugin) { return 0; }
void video_play(struct plugin_data* plugin) {}
void video_pause(struct plugin_data* plugin) {}
void video_step(struct plugin_data* plugin, int frames) { decode_next_frame(&plugin->decoder); }
int video_seek(struct plugin_data* plugin, uint64_t offset, int num, int den) { return decoder_seek_near_keyframe(&plugin->decoder, offset); }
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
