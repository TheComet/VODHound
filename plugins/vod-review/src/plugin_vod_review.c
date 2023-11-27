#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>


struct plugin_ctx
{
    GTypeModule* type_module;
    struct db_interface* dbi;
    struct db* db;

    /* These are loaded from the video player plugin */
    struct plugin_lib video_plugin;
    struct plugin_ctx* video_ctx;
    GtkWidget* video_canvas;
};

static int on_scan_plugin(struct plugin_lib lib, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("FFmpeg Video Player"), lib.i->info->name))
    {
        ctx->video_plugin = lib;
        ctx->video_ctx = ctx->video_plugin.i->create(ctx->type_module, ctx->dbi, ctx->db);
        if (ctx->video_ctx == NULL)
        {
            log_err("Failed to load FFmpeg video player plugin!\n");
            return -1;
        }
        return 1;  /* Success, stop iterating */
    }

    return 0;
}

static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct strlist plugins;
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    ctx->type_module = type_module;
    ctx->dbi = dbi;
    ctx->db = db;

    ctx->video_plugin.handle = NULL;
    ctx->video_ctx = NULL;
    ctx->video_canvas = NULL;

    if (plugins_scan(on_scan_plugin, ctx) <= 0)
        goto load_video_plugin_failed;

    return ctx;

load_video_plugin_failed:
    mem_free(ctx);
    return NULL;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->destroy(type_module, ctx->video_ctx);
    plugin_unload(&ctx->video_plugin);

    mem_free(ctx);
}

static void
on_realize(GtkGLArea* area, struct plugin_ctx* ctx)
{
}

static void
on_unrealize(GtkGLArea* area, struct plugin_ctx* ctx)
{
}

static gboolean
on_render(GtkWidget* widget, GdkGLContext* context, struct plugin_ctx* ctx)
{
    return FALSE;  /* continue propagating signal to other listeners */
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
    GtkAdjustment* adj;
    GtkWidget* slider;
    GtkWidget* time;
    GtkWidget* play;
    GtkWidget* seekb;
    GtkWidget* seekf;
    GtkWidget* controls;
    GtkWidget* ui;

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

    ctx->video_canvas = ctx->video_plugin.i->ui_center->create(ctx->video_ctx);
    g_signal_connect(ctx->video_canvas, "realize", G_CALLBACK(on_realize), ctx);
    g_signal_connect(ctx->video_canvas, "unrealize", G_CALLBACK(on_unrealize), ctx);
    g_signal_connect(ctx->video_canvas, "render", G_CALLBACK(on_render), ctx);
    gtk_widget_set_vexpand(ctx->video_canvas, TRUE);

    ui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(ui), ctx->video_canvas);
    gtk_box_append(GTK_BOX(ui), controls);

    return g_object_ref_sink(ui);
}
static void ui_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    ctx->video_plugin.i->ui_center->destroy(ctx->video_ctx, ctx->video_canvas);
    g_object_unref(ui);
}

static struct ui_center_interface ui = {
    ui_create,
    ui_destroy
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    return ctx->video_plugin.i->video->open_file(ctx->video_ctx, file_name, pause);
}
static void video_close(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->close(ctx->video_ctx);
}
static void video_clear(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->clear(ctx->video_ctx);
}
static int video_is_open(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->is_open(ctx->video_ctx);
}
static void video_play(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->play(ctx->video_ctx);
}
static void video_pause(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->pause(ctx->video_ctx);
}
static void video_step(struct plugin_ctx* ctx, int frames)
{
    ctx->video_plugin.i->video->step(ctx->video_ctx, frames);
}
static int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den)
{
    return ctx->video_plugin.i->video->seek(ctx->video_ctx, offset, num, den);
}
static uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    return ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
}
static uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den)
{
    return ctx->video_plugin.i->video->duration(ctx->video_ctx, num, den);
}
static void video_dimensions(const struct plugin_ctx* ctx, int* width, int* height)
{
    ctx->video_plugin.i->video->dimensions(ctx->video_ctx, width, height);
}
static int video_is_playing(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->is_playing(ctx->video_ctx);
}
static void video_set_volume(struct plugin_ctx* ctx, int percent)
{
    ctx->video_plugin.i->video->set_volume(ctx->video_ctx, percent);
}
static int video_volume(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->volume(ctx->video_ctx);
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
    video_is_playing,
    video_offset,
    video_duration,
    video_dimensions,
    video_set_volume,
    video_volume
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
