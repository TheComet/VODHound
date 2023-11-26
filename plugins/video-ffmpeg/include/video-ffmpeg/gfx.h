#pragma once

struct gfx;

struct gfx* gfx_create(void);
void gfx_destroy(struct gfx* gfx);
void gfx_set_frame(struct gfx* gfx, int width, int height, const void* rgb24);
void gfx_render(struct gfx* gfx, int canvas_width, int canvas_height);
