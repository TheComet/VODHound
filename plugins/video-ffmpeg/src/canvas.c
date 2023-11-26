#include "video-ffmpeg/canvas.h"
#include "video-ffmpeg/gfx.h"

#include "vh/init.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/thread.h"

enum
{
    RESIZE,

    LAST_SIGNAL
};

struct _GLCanvas
{
    GtkWidget parent_instance;
    GdkGLContext* context;

    struct thread thread;
    struct mutex mutex;
    int width, height;
    unsigned request_stop : 1;
};

struct _GLCanvasClass
{
    GtkWidgetClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE(GLCanvas, gl_canvas, GTK_TYPE_WIDGET);

static void*
render_thread(void* args)
{
    struct gfx_interface* gfxi;
    struct gfx* gfx;
    GLCanvas* canvas = args;

    vh_threadlocal_init();

    log_dbg("Render thread\n");
    gdk_gl_context_make_current(canvas->context);
    gfx = gfx_create();

    mutex_lock(canvas->mutex);
    while (!canvas->request_stop)
    {
        int width = canvas->width;
        int height = canvas->height;
        mutex_unlock(canvas->mutex);

        gfx_render(gfx, width, height);

        mutex_lock(canvas->mutex);
    }
    mutex_unlock(canvas->mutex);

    gfx_destroy(gfx);
    vh_threadlocal_deinit();

    return NULL;
}

static void
gl_canvas_realize(GtkWidget* widget)
{
    GdkGLContext* context;
    GError* error = NULL;
    GLCanvas* canvas = GL_CANVAS(widget);

    GTK_WIDGET_CLASS(gl_canvas_parent_class)->realize(widget);

    context = gdk_surface_create_gl_context(
        gtk_native_get_surface(
            gtk_widget_get_native(GTK_WIDGET(canvas))), &error);
    if (error)
    {
        g_clear_object(&context);
        g_clear_error(&error);
        return;
    }

    gdk_gl_context_set_allowed_apis(context, GDK_GL_API_GL);
    gdk_gl_context_realize(context, &error);
    if (error)
    {
        g_clear_object(&context);
        g_clear_error(&error);
        return;
    }

    canvas->context = context;
    canvas->request_stop = 0;
    thread_start(&canvas->thread, render_thread, canvas);
}

static void
gl_canvas_unrealize(GtkWidget* widget)
{
    GLCanvas* self = GL_CANVAS(widget);
    if (self->context)
    {
        mutex_lock(self->mutex);
            self->request_stop = 1;
        mutex_unlock(self->mutex);
        thread_join(self->thread, 0);

        gdk_gl_context_make_current(self->context);
        if (self->context == gdk_gl_context_get_current())
            gdk_gl_context_clear_current();

        g_clear_object(&self->context);
    }

    GTK_WIDGET_CLASS(gl_canvas_parent_class)->unrealize(GTK_WIDGET(self));
}

static void
gl_canvas_init(GLCanvas* self)
{
    mutex_init(&self->mutex);
    mem_track_allocation(self);
}

static void
gl_canvas_finalize(GObject* obj)
{
    GLCanvas* self = GL_CANVAS(obj);
    mutex_deinit(self->mutex);
    mem_track_deallocation(obj);
    G_OBJECT_CLASS(gl_canvas_parent_class)->finalize(obj);
}

static void
gl_canvas_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    GLCanvas* self = GL_CANVAS(widget);
    log_dbg("Resize: %dx%d\n", width, height);
    mutex_lock(self->mutex);
        self->width = width;
        self->height = height;
    mutex_unlock(self->mutex);
    GTK_WIDGET_CLASS(gl_canvas_parent_class)->size_allocate(widget, width, height, baseline);
}

static void
gl_canvas_class_init(GLCanvasClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);

    object_class->finalize = gl_canvas_finalize;

    widget_class->realize = gl_canvas_realize;
    widget_class->unrealize = gl_canvas_unrealize;
    widget_class->size_allocate = gl_canvas_size_allocate;
}

static void
gl_canvas_class_finalize(GLCanvasClass* class)
{
}

void
gl_canvas_register_type_internal(GTypeModule* type_module)
{
    gl_canvas_register_type(type_module);

    /*
     * Have to re-initialize static variables explicitly, because GTK doesn't
     * support types registered by modules to be unloaded, but we do it anyway.
     */
    gpointer class = g_type_class_peek(GL_TYPE_CANVAS);
    if (class)
        gl_canvas_parent_class = g_type_class_peek_parent(class);
}

GtkWidget*
gl_canvas_new(void)
{
    GLCanvas* canvas = g_object_new(GL_TYPE_CANVAS, NULL);
    return GTK_WIDGET(canvas);
}
