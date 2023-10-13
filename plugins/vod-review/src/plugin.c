#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include "iup.h"

#include <string.h>
#include <stdio.h>

struct plugin_ctx
{
    struct plugin video_plugin;
    struct plugin_ctx* video_ctx;
    Ihandle* video_ui;
    Ihandle* ui;

    Ihandle* controls;
};

static void
overlay_set_gfxcanvas(Ihandle* gfxcanvas, int idx, const void* data)
{
    char attr[9];
    snprintf(attr, 9, "TEXRGBA%d", idx);
    IupSetAttribute(gfxcanvas, attr, data);
}

static void
overlay_get_size_gfxcanvas(Ihandle* gfxcanvas, int* w, int* h)
{

    struct str_view size = cstr_view(IupGetAttribute(gfxcanvas, "TEXSIZE"));
    
}

static struct plugin_ctx*
create(void)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    if (plugin_load_category(
                &ctx->video_plugin,
                cstr_view("video driver"),
                cstr_view("FFmpeg Video Player")) != 0)
        goto plugin_load_failed;
    if ((ctx->video_ctx = ctx->video_plugin.i->create()) == NULL)
        goto create_plugin_ctx_failed;

    return ctx;

    create_plugin_ctx_failed : plugin_unload(&ctx->video_plugin);
    plugin_load_failed       : return ctx;
}

static void
destroy(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx)
    {
        ctx->video_plugin.i->destroy(ctx->video_ctx);
        plugin_unload(&ctx->video_plugin);
    }

    mem_free(ctx);
}

Ihandle* ui_create(struct plugin_ctx* ctx)
{
    if (ctx->video_ctx && ctx->video_plugin.i->ui)
        ctx->video_ui = ctx->video_plugin.i->ui->create(ctx->video_ctx);
    if (ctx->video_ui)
    {
        struct str_view class_name = cstr_view(IupGetClassName(ctx->video_ui));
        if (cstr_equal(class_name, "gfxcanvas"))

    }

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
    {
        IupDetach(ctx->video_ui);
        ctx->video_plugin.i->ui->destroy(ctx->video_ctx, ctx->video_ui);
    }

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
int video_is_open(struct plugin_ctx* ctx)
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
uint64_t video_offset(struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
    return 0;
}
uint64_t video_duration(struct plugin_ctx* ctx, int num, int den)
{
    if (ctx->video_ctx)
        return ctx->video_plugin.i->video->duration(ctx->video_ctx, num, den);
    return 0;
}
int video_is_playing(struct plugin_ctx* ctx)
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
int video_volume(struct plugin_ctx* ctx)
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
