#include "video-ffmpeg/canvas.h"

#include "vh/log.h"
#include "vh/mem.h"

struct _GLCanvas
{
    GtkWidget parent_instance;
    GdkGLContext* context;
};

struct _GLCanvasClass
{
    GtkWidgetClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE(GLCanvas, gl_canvas, GTK_TYPE_WIDGET);

static void
gl_canvas_realize(GtkWidget* widget)
{
    GdkGLContext* context;
    GError* error = NULL;
    GLCanvas* canvas = GL_CANVAS(widget);

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
}

static void
gl_canvas_unrealize(GtkWidget* widget)
{
    GLCanvas* canvas = GL_CANVAS(widget);
    log_dbg("gl_canvas_unrealize()\n");
    if (canvas->context)
    {
        gdk_gl_context_make_current(canvas->context);
        /* delete resources */

        if (canvas->context == gdk_gl_context_get_current())
            gdk_gl_context_clear_current();
    }

    g_clear_object(&canvas->context);

    GTK_WIDGET_CLASS(gl_canvas_parent_class)->unrealize(GTK_WIDGET(canvas));
}

static void
gl_canvas_init(GLCanvas* self)
{
    log_dbg("gl_canvas_init()\n");
    mem_track_allocation(self);
}

static void
gl_canvas_finalize(GObject* obj)
{
    log_dbg("gl_canvas_finalize()\n");
    mem_track_deallocation(obj);

    G_OBJECT_CLASS(gl_canvas_parent_class)->finalize(obj);
}

static void
gl_canvas_class_init(GLCanvasClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);

    object_class->finalize = gl_canvas_finalize;

    widget_class->realize = gl_canvas_realize;
    widget_class->unrealize = gl_canvas_unrealize;
    log_dbg("gl_canvas_class_init()\n");
}

static void
gl_canvas_class_finalize(GLCanvasClass* class)
{
    log_dbg("gl_canvas_class_finalize()\n");
}

void
gl_canvas_register_type_internal(GTypeModule* type_module)
{
    gl_canvas_register_type(type_module);
}

GtkWidget*
gl_canvas_new(void)
{
    GLCanvas* canvas = g_object_new(GL_TYPE_CANVAS, NULL);
    return GTK_WIDGET(canvas);
}
