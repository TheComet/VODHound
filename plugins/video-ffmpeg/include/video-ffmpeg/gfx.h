typedef struct Ihandle_ Ihandle;
struct gfx;

struct gfx*
gfx_create(Ihandle* canvas);

void
gfx_destroy(struct gfx* gfx, Ihandle* canvas);
