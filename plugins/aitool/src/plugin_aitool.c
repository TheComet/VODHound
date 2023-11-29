#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>

#include <epoxy/gl.h>

struct vertex
{
    GLfloat pos[2];
};

static const struct vertex quad_vertices[6] = {
    {{-1, -1}},
    {{-1,  1}},
    {{ 1, -1}},
    {{ 1,  1}},
};
static const char* attr_bindings[] = {
    "v_position",
    NULL
};
static const char* vs_quad = "\
//precision mediump float;\n\
attribute vec2 v_position;\n\
varying vec2 f_texcoord;\n\
uniform vec2 u_aspect;\n\
uniform vec2 u_offset;\n\
void main() {\
    // map [-1, 1] to texture coordinate [0, 1]\n\
    f_texcoord = v_position * vec2(1.0, -1.0) * 0.5 + 0.5;\n\
    f_texcoord = (f_texcoord + u_offset) * u_aspect;\n\
    gl_Position = vec4(v_position, 0.0, 1.0);\n\
}";
static const char* fs_quad = "\
//precision mediump float;\n\
varying vec2 f_texcoord;\n\
uniform vec2 u_center;\n\
uniform vec2 u_dims;\n\
uniform vec2 u_pixsize;\n\
\n\
float band(float t, float start, float end, float blur) {\n\
    float step1 = smoothstep(start-blur, start+blur, t);\n\
    float step2 = smoothstep(end+blur, end-blur, t);\n\
    return step1 * step2;\n\
}\n\
\n\
float line(float t, float pos, float blur) {\n\
    return band(t, pos-blur*0.5, pos+blur*0.5, blur);\n\
}\n\
\n\
float line_cross(vec2 uv, vec2 center, vec2 blur) {\n\
    float line1 = line(uv.x, center.x, blur.x);\n\
    float line2 = line(uv.y, center.y, blur.y);\n\
    return line1 + line2;\n\
}\n\
\n\
float rect(vec2 uv, vec2 center, vec2 dims, vec2 blur) {\n\
    float band1 = band(uv.x, center.x-dims.x/2.0, center.x+dims.x/2.0, blur.x);\n\
    float band2 = band(uv.y, center.y-dims.y/2.0, center.y+dims.y/2.0, blur.y);\n\
    return band1 * band2;\n\
}\n\
\n\
void main() {\n\
    float rect = rect(f_texcoord, u_center, u_dims, u_pixsize);\n\
    float cr = rect * line_cross(f_texcoord, u_center, u_pixsize);\n\
    gl_FragColor = vec4(1.0, 0.8, 0.5, (rect + cr) * 0.5);\n\
}";

struct point
{
    int x, y;
};

struct rect
{
    struct point center;
    struct point dims;
};

struct gfx
{
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLuint u_aspect;
    GLuint u_offset;
    GLuint u_center;
    GLuint u_dims;
    GLuint u_pixsize;

    struct vec rects;
};

static GLuint gl_load_shader_type(const GLchar* code, GLenum type)
{
    GLuint shader;
    GLint compiled;

    shader = glCreateShader(type);
    if (shader == 0)
    {
        log_err("GL Error %d: glCreateShader() failed\n", glGetError());
        return 0;
    }

    glShaderSource(shader, 1, &code, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        log_err("GL Error %d: glCompileShader() failed\n", glGetError());
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* error = mem_alloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, error);
            log_err("%.*s\n", info_len, error);
            mem_free(error);
        }

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint gl_load_shader(const GLchar* vs, const GLchar* fs, const char* attribute_bindings[])
{
    int i;
    GLuint program;
    GLint linked;

    program = glCreateProgram();
    if (program == 0)
    {
        log_err("GL Error %d: glCreateProgram() failed\n", glGetError());
        goto create_program_failed;
    }

    GLuint vs_shader = gl_load_shader_type(vs, GL_VERTEX_SHADER);
    if (vs_shader == 0)
        goto load_vs_shader_failed;
    glAttachShader(program, vs_shader);
    glDeleteShader(vs_shader);

    GLuint fs_shader = gl_load_shader_type(fs, GL_FRAGMENT_SHADER);
    if (fs_shader == 0)
        goto load_fs_shader_failed;
    glAttachShader(program, fs_shader);
    glDeleteShader(fs_shader);

    for (i = 0; attribute_bindings[i]; ++i)
        glBindAttribLocation(program, i, attribute_bindings[i]);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        log_err("GL Error %d: glLinkProgram() failed\n", glGetError());
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char* error = mem_alloc(info_len);
            glGetProgramInfoLog(program, info_len, NULL, error);
            log_err("%.*s\n", info_len, error);
            mem_free(error);
            goto link_program_failed;
        }
    }

    return program;

    link_program_failed   :
    load_vs_shader_failed :
    load_fs_shader_failed : glDeleteProgram(program);
    create_program_failed : return 0;
}

struct plugin_ctx
{
    GTypeModule* type_module;
    struct db_interface* dbi;
    struct db* db;

    struct gfx gfx;
    struct rect* drag_rect;
    struct point drag_start;
    struct point drag_offset;

    /* These are loaded from the video player plugin */
    struct plugin_lib video_plugin;
    struct plugin_ctx* video_ctx;
    GtkWidget* video_canvas;

    unsigned drag_resize : 1;
};

static int on_scan_plugin(struct plugin_lib lib, void* user)
{
    struct plugin_ctx* ctx = user;

    if (cstr_equal(cstr_view("FFmpeg Video Player"), lib.i->info->name))
    {
        ctx->video_plugin = lib;
        ctx->video_ctx = ctx->video_plugin.i->create(ctx->type_module, ctx->dbi, ctx->db);
        if (ctx->video_ctx == NULL)
        {
            log_err("Failed to load FFmpeg video player plugin!\n");
            return -1;
        }
        return 1;  /* Success, stop iterating */
    }

    return 0;
}

static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct strlist plugins;
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    ctx->type_module = type_module;
    ctx->dbi = dbi;
    ctx->db = db;

    ctx->video_plugin.handle = NULL;
    ctx->video_ctx = NULL;
    ctx->video_canvas = NULL;

    if (plugins_scan(on_scan_plugin, ctx) <= 0)
        goto load_video_plugin_failed;

    return ctx;

load_video_plugin_failed:
    mem_free(ctx);
    return NULL;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->destroy(type_module, ctx->video_ctx);
    plugin_unload(&ctx->video_plugin);

    mem_free(ctx);
}

static void
on_realize(GtkGLArea* area, struct plugin_ctx* ctx)
{
    gtk_gl_area_make_current(area);

    /* VAO */
    glGenVertexArrays(1, &ctx->gfx.vao);
    glBindVertexArray(ctx->gfx.vao);

    /* Set up quad mesh */
    glGenBuffers(1, &ctx->gfx.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->gfx.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    /* Shader never changes */
    ctx->gfx.program = gl_load_shader(vs_quad, fs_quad, attr_bindings);
    ctx->gfx.u_offset = glGetUniformLocation(ctx->gfx.program, "u_offset");
    ctx->gfx.u_aspect = glGetUniformLocation(ctx->gfx.program, "u_aspect");
    ctx->gfx.u_center = glGetUniformLocation(ctx->gfx.program, "u_center");
    ctx->gfx.u_dims = glGetUniformLocation(ctx->gfx.program, "u_dims");
    ctx->gfx.u_pixsize = glGetUniformLocation(ctx->gfx.program, "u_pixsize");

    /* Rectangles are stored here */
    vec_init(&ctx->gfx.rects, sizeof(struct rect));
}

static void
on_unrealize(GtkGLArea* area, struct plugin_ctx* ctx)
{
    gtk_gl_area_make_current(area);

    vec_deinit(&ctx->gfx.rects);

    glDeleteProgram(ctx->gfx.program);
    glDeleteBuffers(1, &ctx->gfx.vbo);
    glDeleteVertexArrays(1, &ctx->gfx.vao);
}

static gboolean
on_render(GtkWidget* widget, GdkGLContext* context, struct plugin_ctx* ctx)
{
    int texture_width, texture_height;
    int scale = gtk_widget_get_scale_factor(widget);
    int canvas_width = gtk_widget_get_width(widget) * scale;
    int canvas_height = gtk_widget_get_height(widget) * scale;

    ctx->video_plugin.i->video->dimensions(ctx->video_ctx, &texture_width, &texture_height);

    GLfloat offsetx = 0.0, offsety = 0.0, aspectx = 1.0, aspecty = 1.0;
    GLfloat canvas_ar = (GLfloat)canvas_width / (GLfloat)canvas_height;
    GLfloat texture_ar = (GLfloat)texture_width / (GLfloat)texture_height;
    if (canvas_ar > texture_ar)
    {
        offsetx = ((GLfloat)canvas_height * texture_ar - canvas_width) / 2.0 / canvas_width;
        aspectx = canvas_ar / texture_ar;
    }
    else if (canvas_ar < texture_ar)
    {
        offsety = ((GLfloat)canvas_width / texture_ar - canvas_height) / 2.0 / canvas_height;
        aspecty = texture_ar / canvas_ar;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);

    /* bind quad mesh */
    glBindVertexArray(ctx->gfx.vao);

    /* prepare shader */
    glUseProgram(ctx->gfx.program);
    glUniform2f(ctx->gfx.u_offset, offsetx, offsety);
    glUniform2f(ctx->gfx.u_aspect, aspectx, aspecty);
    glUniform2f(ctx->gfx.u_pixsize, 1.0 / canvas_width, 1.0 / canvas_height);

    /* Draw */
    VEC_FOR_EACH(&ctx->gfx.rects, struct rect, r)
        glUniform2f(ctx->gfx.u_center,
            (GLfloat)r->center.x / (GLfloat)texture_width,
            (GLfloat)r->center.y / (GLfloat)texture_height);
        glUniform2f(ctx->gfx.u_dims,
            (GLfloat)r->dims.x / (GLfloat)texture_width,
            (GLfloat)r->dims.y / (GLfloat)texture_height);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    VEC_END_EACH

    /* Unbind */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindVertexArray(0);

    glDisable(GL_BLEND);

    return FALSE;  /* continue propagating signal to other listeners */
}

static void ui_add_timeline(struct plugin_ctx* ctx)
{
    /*
    Ihandle* slider = IupVal("HORIZONTAL");
    IupSetAttribute(slider, "EXPAND", "HORIZONTAL");

    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, IupFill());
    IupAppend(ctx->controls, slider);
    IupAppend(ctx->controls, IupFill());

    IupMap(slider);
    IupRefresh(ctx->controls);*/
}

static struct point
pos_canvas_to_texture(struct plugin_ctx* ctx, struct point p)
{
    int texture_width, texture_height;
    int scale = gtk_widget_get_scale_factor(ctx->video_canvas);
    int canvas_width = gtk_widget_get_width(ctx->video_canvas) * scale;
    int canvas_height = gtk_widget_get_height(ctx->video_canvas) * scale;
    ctx->video_plugin.i->video->dimensions(ctx->video_ctx, &texture_width, &texture_height);

    GLfloat offsetx = 0.0, offsety = 0.0, aspectx = 1.0, aspecty = 1.0;
    GLfloat canvas_ar = (GLfloat)canvas_width / (GLfloat)canvas_height;
    GLfloat texture_ar = (GLfloat)texture_width / (GLfloat)texture_height;
    if (canvas_ar > texture_ar)
    {
        offsetx = ((GLfloat)canvas_height * texture_ar - canvas_width) / 2.0 / canvas_width;
        aspectx = canvas_ar / texture_ar;
    }
    else if (canvas_ar < texture_ar)
    {
        offsety = ((GLfloat)canvas_width / texture_ar - canvas_height) / 2.0 / canvas_height;
        aspecty = texture_ar / canvas_ar;
    }

    p.x = (int)((((GLfloat)p.x/canvas_width + offsetx) * aspectx) * texture_width);
    p.y = (int)((((GLfloat)p.y/canvas_height + offsety) * aspecty) * texture_height);
    return p;
}

static struct point
scale_canvas_to_texture(struct plugin_ctx* ctx, struct point p)
{
    int texture_width, texture_height;
    int scale = gtk_widget_get_scale_factor(ctx->video_canvas);
    int canvas_width = gtk_widget_get_width(ctx->video_canvas) * scale;
    int canvas_height = gtk_widget_get_height(ctx->video_canvas) * scale;
    ctx->video_plugin.i->video->dimensions(ctx->video_ctx, &texture_width, &texture_height);

    GLfloat aspectx = 1.0, aspecty = 1.0;
    GLfloat canvas_ar = (GLfloat)canvas_width / (GLfloat)canvas_height;
    GLfloat texture_ar = (GLfloat)texture_width / (GLfloat)texture_height;
    if (canvas_ar > texture_ar)
        aspectx = canvas_ar / texture_ar;
    else if (canvas_ar < texture_ar)
        aspecty = texture_ar / canvas_ar;

    p.x = (int)((((GLfloat)p.x/canvas_width) * aspectx) * texture_width);
    p.y = (int)((((GLfloat)p.y/canvas_height) * aspecty) * texture_height);
    return p;
}
static void
drag_update_rect(double x, double y, struct plugin_ctx* ctx)
{
    struct point dp = {
        (int)x,
        (int)y
    };

    if (ctx->drag_rect == NULL)
        return;
    dp = scale_canvas_to_texture(ctx, dp);

    if (ctx->drag_resize == 0)
    {
        ctx->drag_rect->center.x = ctx->drag_start.x + ctx->drag_offset.x + dp.x;
        ctx->drag_rect->center.y = ctx->drag_start.y + ctx->drag_offset.y + dp.y;
    }
    else
    {
        ctx->drag_rect->center.x = ctx->drag_start.x + ctx->drag_offset.x + (dp.x - ctx->drag_offset.x) / 2;
        ctx->drag_rect->center.y = ctx->drag_start.y + ctx->drag_offset.y + (dp.y - ctx->drag_offset.y) / 2;
        ctx->drag_rect->dims.x = abs(dp.x - ctx->drag_offset.x);
        ctx->drag_rect->dims.y = abs(dp.y - ctx->drag_offset.y);
    }
}

static void
drag_place_begin(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    struct point p = {
        (int)x,
        (int)y
    };
    p =  pos_canvas_to_texture(ctx, p);

    VEC_FOR_EACH(&ctx->gfx.rects, struct rect, r)
        if (p.x > r->center.x - r->dims.x && p.x < r->center.x + r->dims.x &&
            p.y > r->center.y - r->dims.y && p.y < r->center.y + r->dims.y)
        {
            int signx = p.x > r->center.x ? 1 : -1;
            int signy = p.y > r->center.y ? 1 : -1;
            ctx->drag_resize = 1;
            ctx->drag_start = p;
            ctx->drag_offset.x = r->center.x - p.x - r->dims.x / 2 * signx;
            ctx->drag_offset.y = r->center.y - p.y - r->dims.y / 2 * signy;
            ctx->drag_rect = r;
            drag_update_rect(0, 0, ctx);
            gtk_gl_area_queue_render(GTK_GL_AREA(ctx->video_canvas));
            return;
        }
    VEC_END_EACH

    ctx->drag_resize = 1;
    ctx->drag_start = p;
    ctx->drag_offset.x = 0;
    ctx->drag_offset.y = 0;
    ctx->drag_rect = vec_emplace(&ctx->gfx.rects);
    ctx->drag_rect->center = p;
    ctx->drag_rect->dims.x = 10;
    ctx->drag_rect->dims.y = 10;
    gtk_gl_area_queue_render(GTK_GL_AREA(ctx->video_canvas));
}

static void
drag_move_begin(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    struct point p = {
        (int)x,
        (int)y
    };
    p =  pos_canvas_to_texture(ctx, p);

    VEC_FOR_EACH(&ctx->gfx.rects, struct rect, r)
        if (p.x > r->center.x - r->dims.x && p.x < r->center.x + r->dims.x &&
            p.y > r->center.y - r->dims.y && p.y < r->center.y + r->dims.y)
        {
            ctx->drag_resize = 0;
            ctx->drag_start = p;
            ctx->drag_offset.x = r->center.x - p.x;
            ctx->drag_offset.y = r->center.y - p.y;
            ctx->drag_rect = r;
            return;
        }
    VEC_END_EACH

    /* Nothing was selected, disable dragging/resizing */
    ctx->drag_rect = NULL;
}

static void
drag_update(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    drag_update_rect(x, y, ctx);
    gtk_gl_area_queue_render(GTK_GL_AREA(ctx->video_canvas));
}

static void
drag_end(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    drag_update_rect(x, y, ctx);

    if (ctx->drag_rect)
        if (ctx->drag_rect->dims.x < 10 || ctx->drag_rect->dims.y < 10)
            vec_erase_index(&ctx->gfx.rects,
                vec_find(&ctx->gfx.rects, ctx->drag_rect));

    gtk_gl_area_queue_render(GTK_GL_AREA(ctx->video_canvas));
}

static GtkWidget* ui_create(struct plugin_ctx* ctx)
{
    GtkAdjustment* adj;
    GtkWidget* slider;
    GtkWidget* time;
    GtkWidget* play;
    GtkWidget* seekb;
    GtkWidget* seekf;
    GtkWidget* controls;
    GtkWidget* ui;
    GtkGesture *drag;

    adj = gtk_adjustment_new(0, 0, 100, 0.1, 1, 0);

    slider = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_widget_set_hexpand(slider, TRUE);

    time = gtk_label_new("00:00:00 / 00:00:00");
    play = gtk_button_new();
    seekb = gtk_button_new();
    seekf = gtk_button_new();

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(controls), play);
    gtk_box_append(GTK_BOX(controls), seekb);
    gtk_box_append(GTK_BOX(controls), seekf);
    gtk_box_append(GTK_BOX(controls), slider);
    gtk_box_append(GTK_BOX(controls), time);

    ctx->video_canvas = ctx->video_plugin.i->ui_center->create(ctx->video_ctx);
    g_signal_connect(ctx->video_canvas, "realize", G_CALLBACK(on_realize), ctx);
    g_signal_connect(ctx->video_canvas, "unrealize", G_CALLBACK(on_unrealize), ctx);
    g_signal_connect(ctx->video_canvas, "render", G_CALLBACK(on_render), ctx);
    gtk_widget_set_vexpand(ctx->video_canvas, TRUE);

    ui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(ui), ctx->video_canvas);
    gtk_box_append(GTK_BOX(ui), controls);

    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(ctx->video_canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_place_begin), ctx);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), ctx);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), ctx);

    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(ctx->video_canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_move_begin), ctx);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), ctx);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), ctx);

    return g_object_ref_sink(ui);
}
static void ui_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    ctx->video_plugin.i->ui_center->destroy(ctx->video_ctx, ctx->video_canvas);
    g_object_unref(ui);
}

static struct ui_center_interface ui = {
    ui_create,
    ui_destroy
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_name, int pause)
{
    return ctx->video_plugin.i->video->open_file(ctx->video_ctx, file_name, pause);
}
static void video_close(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->close(ctx->video_ctx);
}
static void video_clear(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->clear(ctx->video_ctx);
}
static int video_is_open(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->is_open(ctx->video_ctx);
}
static void video_play(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->play(ctx->video_ctx);
}
static void video_pause(struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->video->pause(ctx->video_ctx);
}
static void video_step(struct plugin_ctx* ctx, int frames)
{
    ctx->video_plugin.i->video->step(ctx->video_ctx, frames);
}
static int video_seek(struct plugin_ctx* ctx, uint64_t offset, int num, int den)
{
    return ctx->video_plugin.i->video->seek(ctx->video_ctx, offset, num, den);
}
static uint64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    return ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
}
static uint64_t video_duration(const struct plugin_ctx* ctx, int num, int den)
{
    return ctx->video_plugin.i->video->duration(ctx->video_ctx, num, den);
}
static void video_dimensions(const struct plugin_ctx* ctx, int* width, int* height)
{
    ctx->video_plugin.i->video->dimensions(ctx->video_ctx, width, height);
}
static int video_is_playing(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->is_playing(ctx->video_ctx);
}
static void video_set_volume(struct plugin_ctx* ctx, int percent)
{
    ctx->video_plugin.i->video->set_volume(ctx->video_ctx, percent);
}
static int video_volume(const struct plugin_ctx* ctx)
{
    return ctx->video_plugin.i->video->volume(ctx->video_ctx);
}

static struct video_player_interface controls = {
    video_open_file,
    video_close,
    video_clear,
    video_is_open,
    video_play,
    video_pause,
    video_step,
    video_seek,
    video_is_playing,
    video_offset,
    video_duration,
    video_dimensions,
    video_set_volume,
    video_volume
};

static struct plugin_info info = {
    "AI Tool",
    "video",
    "TheComet",
    "@TheComet93",
    "Tool for labelling video footage"
};

PLUGIN_API struct plugin_interface vh_plugin = {
    PLUGIN_VERSION,
    0,
    &info,
    create,
    destroy,
    &ui,
    NULL,
    NULL,
    &controls
};
