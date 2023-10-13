#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include "iup.h"

#include <string.h>
#include <stdio.h>

static void
overlay_dummy_set_layer(Ihandle* canvas, int idx, const void* data) { (void)canvas; (void)idx; (void)data; }
static void
overlay_dummy_get_size(Ihandle* canvas, int* w, int* h) { (void)canvas; *w = 0; *h = 0; }

static void
overlay_gfxcanvas_set_layer(Ihandle* gfxcanvas, int idx, const void* data)
{
    char attr[9];
    snprintf(attr, 9, "TEXRGBA%d", idx);
    IupSetAttribute(gfxcanvas, attr, data);
}

static void
overlay_gfxcanvas_get_size(Ihandle* gfxcanvas, int* w, int* h)
{
    struct str_view size = cstr_view(IupGetAttribute(gfxcanvas, "TEXSIZE"));
    if (str_dec_to_int(str_left_of(size, 'x'), w) != 0)
        *w = 0;
    if (str_dec_to_int(str_right_of(size, 'x'), h) != 0)
        *h = 0;
}

struct overlay_interface
{
    void (*set_layer)(Ihandle* gfxcanvas, int idx, const void* data);
    void (*get_canvas_size)(Ihandle* gfxcanvas, int* w, int* h);
};

struct plugin_ctx
{
    struct plugin video_plugin;
    struct plugin_ctx* video_ctx;
    Ihandle* video_ui;
    Ihandle* ui;

    Ihandle* controls;
    struct overlay_interface overlay;
};

static int try_load_video_driver_plugin(struct plugin_ctx* ctx)
{
    ctx->video_ctx = ctx->video_plugin.i->create();
    if (ctx->video_ctx == NULL)
        goto create_video_ctx_failed;

    if (ctx->video_plugin.i->ui == NULL)
        goto plugin_has_no_ui_interface;

    ctx->video_ui = ctx->video_plugin.i->ui->create(ctx->video_ctx);
    if (ctx->video_ui == NULL)
        goto create_video_ui_failed;

    struct str_view class_name = cstr_view(IupGetClassName(ctx->video_ui));
    if (cstr_equal(class_name, "gfxcanvas"))
    {
        ctx->overlay.get_canvas_size = overlay_gfxcanvas_get_size;
        ctx->overlay.set_layer = overlay_gfxcanvas_set_layer;
    }
    else
    {
        log_err("Video plugin '%s' uses unsupported IUP class '%s'. Overlays won't work.\n", ctx->video_plugin.i->name, class_name.data);
        goto unsupported_video_ui;
    }

    return 0;

    unsupported_video_ui       :
    create_video_ui_failed     : ctx->video_plugin.i->ui->destroy(ctx->video_ctx, ctx->video_ui);
    plugin_has_no_ui_interface : ctx->video_plugin.i->destroy(ctx->video_ctx);
    create_video_ctx_failed    : return -1;
}

static int on_scan_plugin_prefer_ffmpeg(struct plugin plugin, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("FFmpeg Video Player"), plugin.i->name))
    {
        ctx->video_plugin = plugin;
        if (try_load_video_driver_plugin(ctx) == 0)
            return 1;
        return -1;
    }

    return 0;
}

static int on_scan_plugin_any_video_driver(struct plugin plugin, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("video driver"), plugin.i->category))
    {
        ctx->video_plugin = plugin;
        if (try_load_video_driver_plugin(ctx) == 0)
            return 1;
    }

    return 0;
}

static struct plugin_ctx*
create(void)
{
    struct strlist plugins;
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    ctx->video_ctx = NULL;
    ctx->video_ui = NULL;
    ctx->overlay.get_canvas_size = overlay_dummy_get_size;
    ctx->overlay.set_layer = overlay_dummy_set_layer;

    if (plugins_scan(on_scan_plugin_prefer_ffmpeg, ctx) <= 0)
        plugins_scan(on_scan_plugin_any_video_driver, ctx);

    return ctx;
}

static void
destroy(struct plugin_ctx* ctx)
{
    if (ctx->video_ui)
    {
        ctx->video_plugin.i->ui->destroy(ctx->video_ctx, ctx->video_ui);
        ctx->video_plugin.i->destroy(ctx->video_ctx);
        plugin_unload(&ctx->video_plugin);
    }

    mem_free(ctx);
}

Ihandle* ui_create(struct plugin_ctx* ctx)
{
    Ihandle* slider = IupVal("HORIZONTAL");
    IupSetAttribute(slider, "EXPAND", "HORIZONTAL");

    Ihandle* time = IupLabel("00:00:00 / 00:00:00");
    IupSetAttribute(time, "ALIGNMENT", "ACENTER");

    Ihandle* play = IupButton(">", NULL);
    IupSetAttribute(play, "PADDING", "6x");

    Ihandle* seekb = IupButton("<<", NULL);
    IupSetAttribute(seekb, "PADDING", "6x");

    Ihandle* seekf = IupButton(">>", NULL);
    IupSetAttribute(seekf, "PADDING", "6x");

    ctx->controls = IupGridBox(play, seekb, seekf, slider, time, NULL);
    IupSetAttribute(ctx->controls, "ORIENTATION", "HORIZONTAL");
    IupSetAttribute(ctx->controls, "NUMDIV", "5");
    IupSetAttribute(ctx->controls, "NUMDIV", "5");
    IupSetAttribute(ctx->controls, "SIZECOL", "3");  /* Use the slider's height as reference for calculating row heights */

    ctx->ui = IupVbox(
        ctx->video_ui ? ctx->video_ui : IupCanvas(NULL),
        ctx->controls,
        NULL);

    return ctx->ui;
}
void ui_destroy(struct plugin_ctx* ctx, Ihandle* ui)
{
    if (ctx->video_ui)
        IupDetach(ctx->video_ui);

    IupDestroy(ui);
}

struct ui_interface ui = {
    ui_create,
    ui_destroy
};

int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    Ihandle* slider = IupVal("HORIZONTAL");
    IupSetAttribute(slider, "EXPAND", "HORIZONTAL");

    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, slider);
    IupAppend(ctx->controls, IupFill());

    IupMap(slider);

    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->open_file(ctx->video_ctx, file_name, pause);
    return -1;
}
void video_close(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->close(ctx->video_ctx);
}
int video_is_open(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->is_open(ctx->video_ctx);
    return 0;
}
void video_play(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->play(ctx->video_ctx);
}
void video_pause(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->pause(ctx->video_ctx);
}
void video_step(struct plugin_ctx* ctx, int frames)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->step(ctx->video_ctx, frames);
}
int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->seek(ctx->video_ctx, offset, num, den);
    return 0;
}
uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
    return 0;
}
uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->duration(ctx->video_ctx, num, den);
    return 0;
}
int video_is_playing(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->is_playing(ctx->video_ctx);
    return 0;
}
void video_set_volume(struct plugin_ctx* ctx, int percent)
{
    if (ctx->video_ctx)
        ctx->video_plugin.i->video->set_volume(ctx->video_ctx, percent);
}
int video_volume(const struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->volume(ctx->video_ctx);
    return 0;
}

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

PLUGIN_API struct plugin_interface vh_plugin = {
    PLUGIN_VERSION,
    0,
    create, destroy, &ui, &controls,
    "VOD Review",
    "video",
    "TheComet",
    "@TheComet93",
    "Tool for reviewing videos."
};