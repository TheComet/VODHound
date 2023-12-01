#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/decoder.h"
#include "video-ffmpeg/gfx.h"

#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/vec.h"

#include <libavutil/rational.h>

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>

struct plugin_ctx
{
    struct gfx* gfx;
    struct decoder decoder;

    struct vec canvas_list;

    int64_t game_start_ts;
    AVRational game_start_time_base;
};

static void
on_realize(GtkGLArea* area, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    gtk_gl_area_make_current(area);
    ctx->gfx = gfx_create();
}

static void
on_unrealize(GtkGLArea* area, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    gtk_gl_area_make_current(area);
    gfx_destroy(ctx->gfx);
    ctx->gfx = NULL;
}

static gboolean
on_render(GtkWidget* widget, GdkGLContext* context, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    int scale = gtk_widget_get_scale_factor(widget);
    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    gfx_render(ctx->gfx, width * scale, height * scale);
    return FALSE;  /* continue propagating signal to other listeners */
}

static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);
    vec_init(&ctx->canvas_list, sizeof(GtkGLArea*));

    gl_canvas_register_type_internal(type_module);

    return ctx;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    vec_deinit(&ctx->canvas_list);
    mem_free(ctx);
}

static void update_canvas(struct vec* canvas_list, struct gfx* gfx, const struct decoder* decoder)
{
    int w, h;
    decoder_frame_size(decoder, &w, &h);
    gfx_set_frame(gfx, w, h, decoder_rgb24_data(decoder));

    VEC_FOR_EACH(canvas_list, GtkGLArea*, pcanvas)
        gtk_gl_area_queue_render(*pcanvas);
    VEC_END_EACH
}

static int video_open_file(struct plugin_ctx* ctx, const char* file_name)
{
    int ret = decoder_open_file(&ctx->decoder, file_name);
    if (ret == 0 && ctx->gfx)
    {
        /* TODO Create GdkGLContext and start rendering thread */
        /*
        snprintf(buf, 22, "%dx%d", w, h);
        IupSetAttribute(ctx->canvas, "TEXSIZE", buf);
        IupSetAttribute(ctx->canvas, "TEXRGBA", decoder_rgb24_data(&ctx->decoder));
        IupRedraw(ctx->canvas, 0);*/
    }

    return ret;
}
static void video_set_game_start(struct plugin_ctx* ctx, int64_t game_start_ts, int num, int den)
{
    ctx->game_start_ts = game_start_ts;
    ctx->game_start_time_base = av_make_q(num, den);
}
static void video_close(struct plugin_ctx* ctx)
{
    decoder_close(&ctx->decoder);
}
static void video_clear(struct plugin_ctx* ctx)
{
    gfx_set_frame(ctx->gfx, 0, 0, NULL);
    VEC_FOR_EACH(&ctx->canvas_list, GtkGLArea*, pcanvas)
        gtk_gl_area_queue_render(*pcanvas);
    VEC_END_EACH
}
static int video_is_open(const struct plugin_ctx* ctx)
{
    return decoder_is_open(&ctx->decoder);
}
static void video_play(struct plugin_ctx* ctx) {}
static void video_pause(struct plugin_ctx* ctx) {}
static void video_step(struct plugin_ctx* ctx, int frames)
{
    decode_next_frame(&ctx->decoder);
    update_canvas(&ctx->canvas_list, ctx->gfx, &ctx->decoder);
}
static int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den)
{ 
    /* The offset passed in is relative to the start timestamp */
    AVRational from = ctx->game_start_time_base;
    AVRational to = av_make_q(num, den);
    int64_t game_start = av_rescale_q(ctx->game_start_ts, from, to);

    if (decoder_seek_near_keyframe(&ctx->decoder, offset + game_start, num, den) < 0)
        return -1;
    if (decode_next_frame(&ctx->decoder) < 0)
        return -1;
    update_canvas(&ctx->canvas_list, ctx->gfx, &ctx->decoder);
    return 0;
}
static int video_is_playing(const struct plugin_ctx* ctx) { return 0; }
static uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    AVRational from = ctx->game_start_time_base;
    AVRational to = av_make_q(num, den);
    uint64_t offset = decoder_offset(&ctx->decoder, num, den);
    int64_t game_start = av_rescale_q(ctx->game_start_ts, from, to);
    return offset > game_start ? offset - game_start : 0;
}
static uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den) { return 0; }
static void video_dimensions(const struct plugin_ctx* ctx, int* width, int* height)
{
    if (decoder_is_open(&ctx->decoder))
        decoder_frame_size(&ctx->decoder, width, height);
    else
    {
        *width = 0;
        *height = 0;
    }
}
static void video_set_volume(struct plugin_ctx* ctx, int percent) {}
static int video_volume(const struct plugin_ctx* ctx) { return 0; }

static struct video_player_interface controls = {
    video_open_file,
    video_set_game_start,
    video_close,
    video_clear,
    video_is_open,
    video_play,
    video_pause,
    video_step,
    video_seek,
    video_is_playing,
    video_offset,
    video_duration,
    video_dimensions,
    video_set_volume,
    video_volume
};

static GtkWidget*
ui_center_create(struct plugin_ctx* ctx)
{
    GtkWidget* canvas = gtk_gl_area_new();
    if (canvas == NULL)
        return NULL;

    g_signal_connect(canvas, "realize", G_CALLBACK(on_realize), ctx);
    g_signal_connect(canvas, "unrealize", G_CALLBACK(on_unrealize), ctx);
    g_signal_connect(canvas, "render", G_CALLBACK(on_render), ctx);

    vec_push(&ctx->canvas_list, &canvas);

    return g_object_ref_sink(canvas);
}
static void
ui_center_destroy(struct plugin_ctx* ctx, GtkWidget* canvas)
{
    vec_erase_index(&ctx->canvas_list,
        vec_find(&ctx->canvas_list, &canvas));

    g_object_unref(canvas);
}

static struct ui_center_interface ui_center = {
    ui_center_create,
    ui_center_destroy
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
    &ui_center,
    NULL,
    NULL,
    &controls
};
