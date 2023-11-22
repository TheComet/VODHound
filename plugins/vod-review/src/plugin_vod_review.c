#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>

static void
overlay_dummy_set_layer(GtkWidget* canvas, int idx, const void* data) { (void)canvas; (void)idx; (void)data; }
static void
overlay_dummy_get_size(GtkWidget* canvas, int* w, int* h) { (void)canvas; *w = 0; *h = 0; }

static void
overlay_gtk_gl_area_set_layer(GtkWidget* gfxcanvas, int idx, const void* data)
{
    /*
    char attr[9];
    snprintf(attr, 9, "TEXRGBA%d", idx);
    IupSetAttribute(gfxcanvas, attr, data);*/
}

static void
overlay_gtk_gl_area_get_size(GtkWidget* gfxcanvas, int* w, int* h)
{
    /*
    struct str_view size = cstr_view(IupGetAttribute(gfxcanvas, "TEXSIZE"));
    if (str_dec_to_int(str_left_of(size, 'x'), w) != 0)
        *w = 0;
    if (str_dec_to_int(str_right_of(size, 'x'), h) != 0)
        *h = 0;*/
}

struct overlay_interface
{
    void (*set_layer)(GtkWidget* gfxcanvas, int idx, const void* data);
    void (*get_canvas_size)(GtkWidget* gfxcanvas, int* w, int* h);
};

struct plugin_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct plugin_lib video_plugin;
    struct plugin_ctx* video_ctx;
    GtkWidget* video_ui;
    GtkWidget* ui;

    GtkWidget* controls;
    struct overlay_interface overlay;
};

static int try_load_video_driver_plugin(struct plugin_ctx* ctx)
{
    struct str_view class_name;

    ctx->video_ctx = ctx->video_plugin.i->create(ctx->dbi, ctx->db);
    if (ctx->video_ctx == NULL)
        goto create_video_ctx_failed;

    if (ctx->video_plugin.i->ui_center == NULL)
        goto plugin_has_no_ui_interface;

    ctx->video_ui = ctx->video_plugin.i->ui_center->create(ctx->video_ctx);
    if (ctx->video_ui == NULL)
        goto create_video_ui_failed;

    class_name = cstr_view(
        G_OBJECT_CLASS_NAME(
            G_OBJECT_GET_CLASS(ctx->video_ui)));
    if (cstr_equal(class_name, "GtkGLArea"))
    {
        ctx->overlay.get_canvas_size = overlay_gtk_gl_area_get_size;
        ctx->overlay.set_layer = overlay_gtk_gl_area_set_layer;
        return 0;
    }

    log_warn("Video plugin '%s' uses unsupported class '%s'. Overlays won't work.\n",
            ctx->video_plugin.i->info->name, class_name.data);

unsupported_video_ui:
    ctx->video_plugin.i->ui_center->destroy(ctx->video_ctx, ctx->video_ui);
    ctx->video_ui = NULL;
create_video_ui_failed:
plugin_has_no_ui_interface:
    ctx->video_plugin.i->destroy(ctx->video_ctx);
    ctx->video_ctx = NULL;
create_video_ctx_failed:
    return -1;
}

static int on_scan_plugin_prefer_ffmpeg(struct plugin_lib lib, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("FFmpeg Video Player"), lib.i->info->name))
    {
        ctx->video_plugin = lib;
        if (try_load_video_driver_plugin(ctx) == 0)
            return 1;
        return -1;
    }

    return 0;
}

static int on_scan_plugin_any_video_driver(struct plugin_lib lib, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("video driver"), lib.i->info->category))
    {
        ctx->video_plugin = lib;
        if (try_load_video_driver_plugin(ctx) == 0)
            return 1;
        return -1;
    }

    return 0;
}

static struct plugin_ctx*
create(struct db_interface* dbi, struct db* db)
{
    struct strlist plugins;
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    ctx->video_ctx = NULL;
    ctx->video_ui = NULL;
    ctx->overlay.get_canvas_size = overlay_dummy_get_size;
    ctx->overlay.set_layer = overlay_dummy_set_layer;

    if (plugins_scan(on_scan_plugin_prefer_ffmpeg, ctx) <= 0)
    {
        log_warn("Trying all possible video driver plugins...\n");
        if (plugins_scan(on_scan_plugin_any_video_driver, ctx) <= 0)
        {
            log_err("Failed to find a suitable video driver plugin. Videos cannot be loaded.\n");
            ctx->overlay.get_canvas_size = overlay_dummy_get_size;
            ctx->overlay.set_layer = overlay_dummy_set_layer;
        }
    }

    return ctx;
}

static void
destroy(struct plugin_ctx* ctx)
{
    if (ctx->video_ui)
    {
        ctx->video_plugin.i->ui_center->destroy(ctx->video_ctx, ctx->video_ui);
        ctx->video_plugin.i->destroy(ctx->video_ctx);
        plugin_unload(&ctx->video_plugin);
    }

    mem_free(ctx);
}

static void ui_add_timeline(struct plugin_ctx* ctx)
{
    /*
    Ihandle* slider = IupVal("HORIZONTAL");
    IupSetAttribute(slider, "EXPAND", "HORIZONTAL");

    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, slider);
    IupAppend(ctx->controls, IupFill());

    IupMap(slider);
    IupRefresh(ctx->controls);*/
}

static GtkWidget* ui_create(struct plugin_ctx* ctx)
{
    GtkWidget* slider;
    GtkWidget* time;
    GtkWidget* play;
    GtkWidget* seekb;
    GtkWidget* seekf;
    GtkWidget* controls;
    GtkAdjustment* adj;
    GtkWidget* video_canvas;

    adj = gtk_adjustment_new(0, 0, 100, 0.1, 1, 0);

    slider = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_widget_set_hexpand(slider, TRUE);

    time = gtk_label_new("00:00:00 / 00:00:00");
    play = gtk_button_new();
    seekb = gtk_button_new();
    seekf = gtk_button_new();

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(controls), play);
    gtk_box_append(GTK_BOX(controls), seekb);
    gtk_box_append(GTK_BOX(controls), seekf);
    gtk_box_append(GTK_BOX(controls), slider);
    gtk_box_append(GTK_BOX(controls), time);

    video_canvas = ctx->video_ui ? ctx->video_ui : gtk_image_new();
    gtk_widget_set_vexpand(video_canvas, TRUE);

    ctx->ui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(ctx->ui), video_canvas);
    gtk_box_append(GTK_BOX(ctx->ui), controls);

    return g_object_ref_sink(ctx->ui);
}
static void ui_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    /*if (ctx->video_ui)
        IupDetach(ctx->video_ui);*/

    g_object_unref(ui);
}

static struct ui_center_interface ui = {
    ui_create,
    ui_destroy
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->open_file(ctx->video_ctx, file_name, pause);
    return -1;
}
static void video_close(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->close(ctx->video_ctx);
}
static void video_clear(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->clear(ctx->video_ctx);
}
static int video_is_open(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->is_open(ctx->video_ctx);
    return 0;
}
static void video_play(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->play(ctx->video_ctx);
}
static void video_pause(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->pause(ctx->video_ctx);
}
static void video_step(struct plugin_ctx* ctx, int frames)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->step(ctx->video_ctx, frames);
}
static int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->seek(ctx->video_ctx, offset, num, den);
    return 0;
}
static uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
    return 0;
}
static uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->duration(ctx->video_ctx, num, den);
    return 0;
}
static int video_is_playing(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->is_playing(ctx->video_ctx);
    return 0;
}
static void video_set_volume(struct plugin_ctx* ctx, int percent)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->set_volume(ctx->video_ctx, percent);
}
static int video_volume(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->volume(ctx->video_ctx);
    return 0;
}
static const char* video_graphics_backend(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->graphics_backend(ctx->video_ctx);
    return "null";
}
static int video_add_render_callback(struct plugin_ctx* ctx,
    void (*on_render)(int width, int height, void* user_data),
    void* user_data)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->add_render_callback(ctx->video_ctx, on_render, user_data);
    return -1;
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
    "VOD Review",
    "video",
    "TheComet",
    "@TheComet93",
    "Tool for reviewing videos."
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
