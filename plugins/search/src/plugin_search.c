#include "vh/mem.h"
#include "vh/plugin.h"

#include "iup.h"

#include <string.h>
#include <stdio.h>

struct plugin_ctx
{
    Ihandle* canvas;
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

static Ihandle* ui_create(struct plugin_ctx* ctx)
{
    ctx->canvas = IupCanvas(NULL);
    if (ctx->canvas == NULL)
        return NULL;

    return ctx->canvas;
}
static void ui_destroy(struct plugin_ctx* ctx, Ihandle* ui)
{
    IupDestroy(ui);
}

static struct ui_pane_interface ui = {
    ui_create,
    ui_destroy
};

PLUGIN_API struct plugin_interface vh_plugin = {
    PLUGIN_VERSION,
    0,
    "FFmpeg Video Player",
    "video driver",
    "TheComet",
    "@TheComet93",
    "Decodes videos using FFmpeg libraries.",
    create, destroy, NULL, &ui, NULL
};
