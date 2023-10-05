#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/decoder.h"
#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

struct plugin_ctx
{
    struct canvas* canvas;
    struct decoder decoder;
    struct gfx* gfx;
};

static struct plugin_ctx*
create(void)
{
    return mem_alloc(sizeof(struct plugin_ctx));
}

static void
destroy(struct plugin_ctx* ctx)
{
    mem_free(ctx);
}

void* ui_create(struct plugin_ctx* ctx)
{
    ctx->canvas = canvas_create();
    if (ctx->canvas == NULL)
        return NULL;

    ctx->gfx = gfx_create(ctx->canvas);
    if (ctx->gfx == NULL)
    {
        canvas_destroy(ctx->canvas);
        return NULL;
    }

    return canvas_get_native_handle(ctx->canvas);
}
void ui_destroy(struct plugin_ctx* ctx, void* ui)
{
    if (ui == canvas_get_native_handle(ctx->canvas))
    {
        gfx_destroy(ctx->gfx, ctx->canvas);
        canvas_destroy(ctx->canvas);
    }
}

void ui_main(struct plugin_ctx* ctx, void* ui)
{
    if (ui == canvas_get_native_handle(ctx->canvas))
        canvas_main_loop(ctx->canvas);
}

struct ui_interface ui = {
    ui_create,
    ui_destroy,
    ui_main
};

int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause) { return decoder_open_file(&ctx->decoder, file_name, pause); }
void video_close(struct plugin_ctx* ctx) { decoder_close(&ctx->decoder); }
int video_is_open(struct plugin_ctx* ctx) { return 0; }
void video_play(struct plugin_ctx* ctx) {}
void video_pause(struct plugin_ctx* ctx) {}
void video_step(struct plugin_ctx* ctx, int frames) { decode_next_frame(&ctx->decoder); }
int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den) { return decoder_seek_near_keyframe(&ctx->decoder, offset); }
uint64_t video_offset(struct plugin_ctx* ctx, int num, int den) { return 0; }
uint64_t video_duration(struct plugin_ctx* ctx, int num, int den) { return 0; }
int video_is_playing(struct plugin_ctx* ctx) { return 0; }
void video_set_volume(struct plugin_ctx* ctx, int percent) {}
int video_volume(struct plugin_ctx* ctx) { return 0; }

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
