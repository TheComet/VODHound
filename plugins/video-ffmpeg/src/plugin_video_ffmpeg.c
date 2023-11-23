#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/decoder.h"
#include "video-ffmpeg/gfx.h"

#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>

struct plugin_ctx
{
    struct decoder decoder;
    GtkWidget* canvas;
};

static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);

    gl_canvas_register_type_internal(type_module);

    return ctx;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    mem_free(ctx);
    //g_type_module_unuse(type_module);
}

static GtkWidget* ui_create(struct plugin_ctx* ctx)
{
    ctx->canvas = gl_canvas_new();
    mem_track_allocation(ctx->canvas);
    return g_object_ref_sink(ctx->canvas);
}
static void ui_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    mem_track_deallocation(ui);
    g_object_unref(ui);
}

static struct ui_center_interface ui = {
    ui_create,
    ui_destroy
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    int ret = decoder_open_file(&ctx->decoder, file_name, pause);
    if (ret == 0 && ctx->canvas)
    {
        int w, h;
        decode_next_frame(&ctx->decoder);
        decoder_frame_size(&ctx->decoder, &w, &h);

        /* TODO Create GdkGLContext and start rendering thread */
        /*
        snprintf(buf, 22, "%dx%d", w, h);
        IupSetAttribute(ctx->canvas, "TEXSIZE", buf);
        IupSetAttribute(ctx->canvas, "TEXRGBA", decoder_rgb24_data(&ctx->decoder));
        IupRedraw(ctx->canvas, 0);*/
    }

    return ret;
}
static void video_close(struct plugin_ctx* ctx)
{
    decoder_close(&ctx->decoder);
}
static void video_clear(struct plugin_ctx* ctx)
{
    /*
    IupSetAttribute(ctx->canvas, "TEXRGBA", NULL);
    IupRedraw(ctx->canvas, 0);*/
}
static int video_is_open(const struct plugin_ctx* ctx) { return decoder_is_open(&ctx->decoder); }
static void video_play(struct plugin_ctx* ctx) {}
static void video_pause(struct plugin_ctx* ctx) {}
static void video_step(struct plugin_ctx* ctx, int frames) { decode_next_frame(&ctx->decoder); }
static int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den) { return decoder_seek_near_keyframe(&ctx->decoder, offset); }
static uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den) { return 0; }
static uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den) { return 0; }
static int video_is_playing(const struct plugin_ctx* ctx) { return 0; }
static void video_set_volume(struct plugin_ctx* ctx, int percent) {}
static int video_volume(const struct plugin_ctx* ctx) { return 0; }
static const char* video_graphics_backend(const struct plugin_ctx* ctx) { return "gl"; }
static int video_add_render_callback(struct plugin_ctx* ctx,
    void (*on_render)(int width, int height, void* user_data),
    void* user_data)
{
    return 0;
}

static struct video_player_interface controls = {
    video_open_file,
    video_close,
    video_clear,
    video_is_open,
    video_play,
    video_pause,
    video_step,
    video_seek,
    video_offset,
    video_duration,
    video_is_playing,
    video_set_volume,
    video_volume,
    video_graphics_backend,
    video_add_render_callback
};

static struct plugin_info info = {
    "FFmpeg Video Player",
    "video driver",
    "TheComet",
    "@TheComet93",
    "Decodes videos using FFmpeg libraries."
};

PLUGIN_API struct plugin_interface vh_plugin = {
    PLUGIN_VERSION,
    0,
    &info,
    create,
    destroy,
    &ui,
    NULL,
    NULL,
    &controls
};
