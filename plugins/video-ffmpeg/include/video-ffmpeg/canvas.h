struct canvas;

struct canvas*
canvas_create(void);

void
canvas_destroy(struct canvas* canvas);

void*
canvas_get_native_handle(struct canvas* canvas);

void
canvas_main_loop(struct canvas* canvas);
