#include "video-ffmpeg/decoder.h"
#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

#include "iup.h"
#include "iupgfx.h"

#include <string.h>

struct plugin_ctx
{
    Ihandle* canvas;
    struct decoder decoder;
    struct gfx* gfx;
};

static struct plugin_ctx*
create(void)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);
    return ctx;
}

static void
destroy(struct plugin_ctx* ctx)
{
    mem_free(ctx);
}

Ihandle* ui_create(struct plugin_ctx* ctx)
{
    if (ctx->canvas)
        return NULL;

    ctx->canvas = IupGfxCanvas(NULL);
    if (ctx->canvas == NULL)
        return NULL;

    ctx->gfx = gfx_create(ctx->canvas);
    if (ctx->gfx == NULL)
    {
        IupDestroy(ctx->canvas);
        return NULL;
    }

    return ctx->canvas;
}
void ui_destroy(struct plugin_ctx* ctx, Ihandle* ui)
{
    if (ctx->canvas != ui)
        return;

    gfx_destroy(ctx->gfx, ctx->canvas);
    IupDestroy(ctx->canvas);
}

struct ui_interface ui = {
    ui_create,
    ui_destroy
};

int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    int ret = decoder_open_file(&ctx->decoder, file_name, pause);
    if (ret == 0 && ctx->canvas)
    {
        decode_next_frame(&ctx->decoder);
        IupSetAttribute(ctx->canvas, "TEXSIZE", "960x540");
        IupSetAttribute(ctx->canvas, "TEXRGBA", decoder_rgb24_data(&ctx->decoder));
        IupRedraw(ctx->canvas, 0);
    }

    return ret;
}
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
    "Decodes videos using FFmpeg libraries."
};
