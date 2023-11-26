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
    GtkWidget* canvas;
    struct gfx* gfx;
    struct decoder decoder;
};

static void
on_realize(GtkGLArea* area, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    gtk_gl_area_make_current(area);
    ctx->gfx = gfx_gl.create();
}

static void
on_unrealize(GtkGLArea* area, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    gtk_gl_area_make_current(area);
    gfx_gl.destroy(ctx->gfx);
    ctx->gfx = NULL;
}

static gboolean
on_render(GtkWidget* widget, GdkGLContext* context, void* user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    int scale = gtk_widget_get_scale_factor(widget);
    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);
    gfx_gl.render(ctx->gfx, width * scale, height * scale);
    return TRUE;  /* continue propegating signal to other listeners */
}

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
}

static GtkWidget* ui_create(struct plugin_ctx* ctx)
{
    ctx->canvas = gtk_gl_area_new();
    if (ctx->canvas == NULL)
        return NULL;

    g_signal_connect(ctx->canvas, "realize", G_CALLBACK(on_realize), ctx);
    g_signal_connect(ctx->canvas, "unrealize", G_CALLBACK(on_unrealize), ctx);
    g_signal_connect(ctx->canvas, "render", G_CALLBACK(on_render), ctx);

    return g_object_ref_sink(ctx->canvas);
}
static void ui_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    g_object_unref(ui);
    ctx->canvas = NULL;
}

static struct ui_center_interface ui = {
    ui_create,
    ui_destroy
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    int ret = decoder_open_file(&ctx->decoder, file_name, pause);
    if (ret == 0 && ctx->gfx)
    {
        int w, h;
        decode_next_frame(&ctx->decoder);
        decoder_frame_size(&ctx->decoder, &w, &h);
        gfx_gl.set_frame(ctx->gfx, w, h, decoder_rgb24_data(&ctx->decoder));
        gtk_gl_area_queue_render(GTK_GL_AREA(ctx->canvas));

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
