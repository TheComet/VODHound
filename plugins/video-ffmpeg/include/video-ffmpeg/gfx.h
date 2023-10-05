struct canvas;
struct gfx;

struct gfx*
gfx_create(struct canvas* canvas);

void
gfx_destroy(struct gfx* gfx, struct canvas* canvas);
