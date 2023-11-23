#pragma once

#include <gtk/gtk.h>

#define GL_TYPE_CANVAS (gl_canvas_get_type())
G_DECLARE_FINAL_TYPE(GLCanvas, gl_canvas, GL, CANVAS, GtkWidget);

void
gl_canvas_register_type_internal(GTypeModule* type_module);

GtkWidget*
gl_canvas_new(void);
