#pragma once

typedef struct _GtkWidget GtkWidget;
struct gfx;

struct gfx_interface
{
    struct gfx* (*create)(GtkWidget* parent);
    void (*destroy)(struct gfx* gfx, GtkWidget* parent);
};

extern struct gfx_interface gfx_gl;
