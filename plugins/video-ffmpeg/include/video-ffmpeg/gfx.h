#pragma once

struct gfx;

struct gfx_interface
{
    struct gfx* (*create)(void);
    void (*destroy)(struct gfx* gfx);
    void (*set_frame)(struct gfx* gfx, int width, int height, const void* rgb24);
    void (*render)(struct gfx* gfx, int canvas_width, int canvas_height);
};

extern struct gfx_interface gfx_gl;
