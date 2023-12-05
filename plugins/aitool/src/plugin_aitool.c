#include "aitool/db.h"

#include "vh/db.h"
#include "vh/frame_data.h"
#include "vh/fs.h"
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

struct pane
{
    GtkSpinButton* game_offset;
    GtkLabel* frame;
    GtkLabel* hash40;
    GtkLabel* string;
};

struct center
{
    /* These are loaded from the video player plugin */
    GtkWidget* video_canvas;

    struct rect* drag_rect;
    struct point drag_start;
    struct point drag_offset;

    unsigned drag_resize : 1;
};

struct plugin_ctx
{
    GTypeModule* type_module;
    struct db_interface* dbi;
    struct db* db;

    struct aidb_interface* aidbi;
    struct aidb* aidb;

    int64_t game_offset;
    int game_id;
    int video_id;

    struct frame_data fdata;

    /* These are loaded from the video player plugin */
    struct plugin_lib video_plugin;
    void* video_ctx;

    struct gfx gfx;
    struct center center;
    struct pane pane;
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

static int aidb_init_refs = 0;
static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));

    ctx->type_module = type_module;
    ctx->dbi = dbi;
    ctx->db = db;

    if (aidb_init_refs++ == 0)
        if (aidb_init() < 0)
        {
            aidb_init_refs--;
            goto init_aidb_failed;
        }

    ctx->aidbi = aidb("sqlite3");
    ctx->aidb = ctx->aidbi->open("aitool.db");
    if (ctx->aidb == NULL)
        goto open_aidb_failed;
    if (ctx->aidbi->migrate_to(ctx->aidb, 1) < 0)
        goto migrate_aidb_failed;

    frame_data_init(&ctx->fdata);

    ctx->video_plugin.handle = NULL;
    ctx->video_ctx = NULL;
    ctx->center.video_canvas = NULL;

    if (plugins_scan(on_scan_plugin, ctx) <= 0)
        goto load_video_plugin_failed;

    return ctx;

load_video_plugin_failed:
migrate_aidb_failed:
    ctx->aidbi->close(ctx->aidb);
open_aidb_failed:
    if (--aidb_init_refs == 0)
        aidb_deinit();
init_aidb_failed:
    mem_free(ctx);
    return NULL;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    ctx->video_plugin.i->destroy(type_module, ctx->video_ctx);
    plugin_unload(&ctx->video_plugin);

    ctx->aidbi->close(ctx->aidb);
    if (--aidb_init_refs == 0)
        aidb_deinit();

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
    int scale = gtk_widget_get_scale_factor(ctx->center.video_canvas);
    int canvas_width = gtk_widget_get_width(ctx->center.video_canvas) * scale;
    int canvas_height = gtk_widget_get_height(ctx->center.video_canvas) * scale;
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
    int scale = gtk_widget_get_scale_factor(ctx->center.video_canvas);
    int canvas_width = gtk_widget_get_width(ctx->center.video_canvas) * scale;
    int canvas_height = gtk_widget_get_height(ctx->center.video_canvas) * scale;
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
    struct center* c = &ctx->center;
    struct point dp = {
        (int)x,
        (int)y
    };

    if (c->drag_rect == NULL)
        return;
    dp = scale_canvas_to_texture(ctx, dp);

    if (c->drag_resize == 0)
    {
        c->drag_rect->center.x = c->drag_start.x + c->drag_offset.x + dp.x;
        c->drag_rect->center.y = c->drag_start.y + c->drag_offset.y + dp.y;
    }
    else
    {
        c->drag_rect->center.x = c->drag_start.x + c->drag_offset.x + (dp.x - c->drag_offset.x) / 2;
        c->drag_rect->center.y = c->drag_start.y + c->drag_offset.y + (dp.y - c->drag_offset.y) / 2;
        c->drag_rect->dims.x = abs(dp.x - c->drag_offset.x);
        c->drag_rect->dims.y = abs(dp.y - c->drag_offset.y);
    }
}

static void
drag_place_begin(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    struct center* c = &ctx->center;
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
            c->drag_resize = 1;
            c->drag_start = p;
            c->drag_offset.x = r->center.x - p.x - r->dims.x / 2 * signx;
            c->drag_offset.y = r->center.y - p.y - r->dims.y / 2 * signy;
            c->drag_rect = r;
            drag_update_rect(0, 0, ctx);
            gtk_gl_area_queue_render(GTK_GL_AREA(c->video_canvas));
            return;
        }
    VEC_END_EACH

    c->drag_resize = 1;
    c->drag_start = p;
    c->drag_offset.x = 0;
    c->drag_offset.y = 0;
    c->drag_rect = vec_emplace(&ctx->gfx.rects);
    c->drag_rect->center = p;
    c->drag_rect->dims.x = 10;
    c->drag_rect->dims.y = 10;
    gtk_gl_area_queue_render(GTK_GL_AREA(c->video_canvas));
}

static void
drag_move_begin(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    struct center* c = &ctx->center;
    struct point p = {
        (int)x,
        (int)y
    };
    p =  pos_canvas_to_texture(ctx, p);

    VEC_FOR_EACH(&ctx->gfx.rects, struct rect, r)
        if (p.x > r->center.x - r->dims.x && p.x < r->center.x + r->dims.x &&
            p.y > r->center.y - r->dims.y && p.y < r->center.y + r->dims.y)
        {
            c->drag_resize = 0;
            c->drag_start = p;
            c->drag_offset.x = r->center.x - p.x;
            c->drag_offset.y = r->center.y - p.y;
            c->drag_rect = r;
            return;
        }
    VEC_END_EACH

    /* Nothing was selected, disable dragging/resizing */
    c->drag_rect = NULL;
}

static void
drag_update(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    drag_update_rect(x, y, ctx);
    gtk_gl_area_queue_render(GTK_GL_AREA(ctx->center.video_canvas));
}

static void
drag_end(GtkGestureDrag* gesture, double x, double y, struct plugin_ctx* ctx)
{
    struct center* c = &ctx->center;
    drag_update_rect(x, y, ctx);

    if (c->drag_rect)
        if (c->drag_rect->dims.x < 10 || c->drag_rect->dims.y < 10)
            vec_erase_index(&ctx->gfx.rects,
                vec_find(&ctx->gfx.rects, c->drag_rect));

    gtk_gl_area_queue_render(GTK_GL_AREA(c->video_canvas));
}

static gboolean
shortcut_prev_frame(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    ctx->video_plugin.i->video->step(ctx->video_ctx, -1);
    return TRUE;
}

static gboolean
shortcut_next_frame(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    ctx->video_plugin.i->video->step(ctx->video_ctx, 1);
    return TRUE;
}

static gboolean
shortcut_prev_frame_adj(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    struct video_player_interface* vi = ctx->video_plugin.i->video;
    void* vctx = ctx->video_ctx;

    int64_t offset = vi->offset(vctx, 1, 60) - ctx->game_offset;
    ctx->game_offset--;
    vi->seek(vctx, ctx->game_offset + offset, 1, 60);

    ctx->dbi->game.set_frame_offset(ctx->db, ctx->game_id, ctx->video_id, ctx->game_offset);

    return TRUE;
}

static gboolean
shortcut_next_frame_adj(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    struct plugin_ctx* ctx = user_pointer;
    struct video_player_interface* vi = ctx->video_plugin.i->video;
    void* vctx = ctx->video_ctx;

    int64_t offset = vi->offset(vctx, 1, 60) - ctx->game_offset;
    ctx->game_offset++;
    vi->seek(vctx, ctx->game_offset + offset, 1, 60);

    ctx->dbi->game.set_frame_offset(ctx->db, ctx->game_id, ctx->video_id, ctx->game_offset);

    return TRUE;
}

static gboolean
shortcut_prev_motion(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    int i;
    uint64_t motion;
    struct plugin_ctx* ctx = user_pointer;
    struct video_player_interface* vi = ctx->video_plugin.i->video;
    void* vctx = ctx->video_ctx;

    int player_idx = 1;
    const uint64_t* motions = ctx->fdata.motion[player_idx];

    i = (int)vi->offset(vctx, 1, 60) - ctx->game_offset;
    if (i >= ctx->fdata.frame_count)
        i = ctx->fdata.frame_count - 1;
    if (i <= 0)
        return TRUE;

    /* Find previous motion */
    i--;
    motion = motions[i];
    while (i != 0 && motion == motions[i])
        i--;
    i++;

    vi->seek(vctx, ctx->game_offset + i, 1, 60);

    return TRUE;
}

static gboolean
shortcut_next_motion(GtkWidget* widget, GVariant* unused, gpointer user_pointer)
{
    int i;
    uint64_t motion;
    struct plugin_ctx* ctx = user_pointer;
    struct video_player_interface* vi = ctx->video_plugin.i->video;
    void* vctx = ctx->video_ctx;

    int player_idx = 1;
    const uint64_t* motions = ctx->fdata.motion[player_idx];

    i = (int)vi->offset(vctx, 1, 60) - ctx->game_offset;
    if (i >= ctx->fdata.frame_count - 1)
        return TRUE;
    if (i < 0)
        i = 0;

    /* Find next motion */
    motion = motions[i];
    do i++;
    while (i != ctx->fdata.frame_count && motion == motions[i]);

    vi->seek(vctx, ctx->game_offset + i, 1, 60);

    return TRUE;
}

static void
add_shortcuts(struct plugin_ctx* ctx, GtkWidget* ui)
{
    GtkEventController* controller;
    GtkShortcutTrigger* trigger;
    GtkShortcutAction* action;
    GtkShortcut* shortcut;

    controller = gtk_shortcut_controller_new();
    gtk_shortcut_controller_set_scope(
        GTK_SHORTCUT_CONTROLLER(controller),
        GTK_SHORTCUT_SCOPE_GLOBAL);
    gtk_widget_add_controller(ui, controller);

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_h, GDK_CONTROL_MASK),
            gtk_callback_action_new(shortcut_prev_frame, ctx, NULL)));

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_l, GDK_CONTROL_MASK),
            gtk_callback_action_new(shortcut_next_frame, ctx, NULL)));

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_j, GDK_CONTROL_MASK),
            gtk_callback_action_new(shortcut_prev_frame_adj, ctx, NULL)));

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_k, GDK_CONTROL_MASK),
            gtk_callback_action_new(shortcut_next_frame_adj, ctx, NULL)));

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_h, GDK_SHIFT_MASK),
            gtk_callback_action_new(shortcut_prev_motion, ctx, NULL)));

    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        gtk_shortcut_new(
            gtk_keyval_trigger_new(GDK_KEY_l, GDK_SHIFT_MASK),
            gtk_callback_action_new(shortcut_next_motion, ctx, NULL)));
}

static GtkWidget* ui_center_create(struct plugin_ctx* ctx)
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

    ctx->center.video_canvas = ctx->video_plugin.i->ui_center->create(ctx->video_ctx);
    g_signal_connect(ctx->center.video_canvas, "realize", G_CALLBACK(on_realize), ctx);
    g_signal_connect(ctx->center.video_canvas, "unrealize", G_CALLBACK(on_unrealize), ctx);
    g_signal_connect(ctx->center.video_canvas, "render", G_CALLBACK(on_render), ctx);
    gtk_widget_set_vexpand(ctx->center.video_canvas, TRUE);

    ui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(ui), ctx->center.video_canvas);
    gtk_box_append(GTK_BOX(ui), controls);

    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(ctx->center.video_canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_place_begin), ctx);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), ctx);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), ctx);

    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(ctx->center.video_canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_move_begin), ctx);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), ctx);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), ctx);

    add_shortcuts(ctx, ctx->center.video_canvas);

    return g_object_ref_sink(ui);
}
static void ui_center_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    ctx->video_plugin.i->ui_center->destroy(ctx->video_ctx, ctx->center.video_canvas);
    g_object_unref(ui);
}

static struct ui_center_interface ui_center = {
    ui_center_create,
    ui_center_destroy
};

static GtkWidget* ui_pane_create(struct plugin_ctx* ctx)
{
    GtkWidget* label;
    GtkWidget* game_offset;
    GtkWidget* vbox;
    GtkWidget* ui;

    ui = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    label = gtk_label_new("Frame Offset:");
    game_offset = gtk_spin_button_new_with_range(-32768, 32767, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(game_offset), 0);

    vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_box_append(GTK_BOX(vbox), game_offset);
    gtk_box_append(GTK_BOX(ui), vbox);

    vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    label = gtk_label_new("Frame:");
    gtk_box_append(GTK_BOX(vbox), label);
    label = gtk_label_new("0");
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_box_append(GTK_BOX(ui), vbox);

    vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    label = gtk_label_new("Motion:");
    gtk_box_append(GTK_BOX(vbox), label);
    label = gtk_label_new("idle");
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_box_append(GTK_BOX(ui), vbox);

    return g_object_ref_sink(ui);
}

static void ui_pane_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    g_object_unref(ui);
}

static struct ui_pane_interface ui_pane = {
    ui_pane_create,
    ui_pane_destroy
};

static int on_game_video(int video_id, const char* file_name, const char* path_hint, int64_t frame_offset, void* user)
{
    struct plugin_ctx* ctx = user;
    ctx->video_id = video_id;
    ctx->game_offset = frame_offset;
    return 1;
}

static void replay_select(struct plugin_ctx* ctx, const int* game_ids, int count)
{
    /* Figure out where in the video the game starts. We use this to seek correctly */
    ctx->game_offset = 0;  /* default */
    ctx->video_id = -1;
    ctx->game_id = game_ids[0];
    ctx->dbi->game.get_videos(ctx->db, game_ids[0], on_game_video, ctx);

    frame_data_load(&ctx->fdata, game_ids[0]);
    
}
static void replay_clear(struct plugin_ctx* ctx)
{
    if (frame_data_is_loaded(&ctx->fdata))
        frame_data_free(&ctx->fdata);
}

static struct replay_interface replay = {
    replay_select,
    replay_clear
};

static int video_open_file(struct plugin_ctx* ctx, const char* file_path)
{
    return ctx->video_plugin.i->video->open_file(ctx->video_ctx, file_path);
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
static int video_step(struct plugin_ctx* ctx, int frames)
{
    return ctx->video_plugin.i->video->step(ctx->video_ctx, frames);
}
static int video_seek(struct plugin_ctx* ctx, int64_t offset, int num, int den)
{
    int64_t game_offset = ctx->game_offset * 60 * num / den;
    return ctx->video_plugin.i->video->seek(ctx->video_ctx, offset + game_offset, num, den);
}
static int64_t video_offset(const struct plugin_ctx* ctx, int num, int den)
{
    int64_t offset = ctx->video_plugin.i->video->offset(ctx->video_ctx, num, den);
    int64_t game_offset = ctx->game_offset * 60 * num / den;
    return offset - game_offset;
}
static int64_t video_duration(const struct plugin_ctx* ctx, int num, int den)
{
    /* TODO duration of game, not of video */
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
    &ui_center,
    &ui_pane,
    &replay,
    &controls
};
