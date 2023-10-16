#include "iup.h"
#include "iupcbs.h"
#include "iupgfx.h"

#include "iup_attrib.h"
#include "iup_class.h"
#include "iup_drvinfo.h"

#include "gl/shader.h"

#include "vh/mem.h"
#include "vh/log.h"

#include <X11/Xlib.h>
#include <GL/glx.h>

#include <string.h>

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
uniform sampler2D s_texture0;\n\
void main() {\n\
    vec3 col0 = texture2D(s_texture0, f_texcoord);\n\
    gl_FragColor = vec4(col0, 1.0);\n\
}";

typedef struct Context3D
{
    Display* display;
    GLXContext gl;
    Window window;
    GLuint vbo;
    GLuint texture[8];
    GLuint program;
    GLuint u_aspect;
    GLuint u_offset;
    GLuint s_texture[8];

    int canvas_width, canvas_height;
    int texture_width, texture_height;
} Context3D;

typedef GLXContext (*glXCreateContextAttribsARBProc)(
        Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int ExtensionExistsInString(const char* strings, const char* extension)
{
    int len = strlen(extension);
    const char* p = strings;
    while (p)
    {
        p = strstr(p, extension);
        if (p == NULL)
            break;
        if ((p == extension || p[-1] == ' ') && (p[len] == '\0' || p[len] == ' '))
            return 1;
        p += len;
    }

    return 0;
}

static int ctxErrorOccurred = 0;
static int ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
    ctxErrorOccurred = 1;
    return 0;
}
static int CreateGLXContext(Ihandle* ih, Context3D* ctx)
{
    int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_DEPTH_SIZE, 1,
        None
    };

    int fbcount;
    GLXFBConfig* fbconfigs = glXChooseFBConfig(ctx->display, DefaultScreen(ctx->display), attribs, &fbcount);
    if (fbconfigs == NULL)
    {
        const char* error = "Failed to retrieve a framebuffer config";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        goto choose_fbconfig_failed;
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddress(
            (const GLubyte*)"glXCreateContextAttribsARB");

    const char* glxstr = glXQueryExtensionsString(ctx->display, DefaultScreen(ctx->display));
    if (!glxstr)
    {
        const char* error = "GLX did not advertise any extensions";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        goto query_extension_string_failed;
    }

    if (!ExtensionExistsInString(glxstr, "GLX_ARB_create_context_profile") ||
        !glXCreateContextAttribsARB)
    {
        const char* error = "GLX does not support GLX_ARB_create_context_profile";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        goto ARB_create_context_profile_doesnt_exist;
    }

    /*
     * Install an X error handler so the application won't exit if GL 3.0
     * context allocation fails.
     *
     * Note this error handler is global.  All display connections in all threads
     * of a process use the same error handler, so be sure to guard against other
     * threads issuing X commands while this code is running.
     */
    ctxErrorOccurred = 0;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);

    int ctx_flags = 0;
    int profile_mask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
#if defined(DEBUG)
    ctx_flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
#endif

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        GLX_CONTEXT_PROFILE_MASK_ARB, profile_mask,
        GLX_CONTEXT_FLAGS_ARB, ctx_flags,
        None
    };

    ctx->gl = glXCreateContextAttribsARB(ctx->display, *fbconfigs, 0, True, context_attribs);
    XSync(ctx->display, False);

    /* Restore the original error handler */
    XSetErrorHandler(oldHandler);

    if (ctxErrorOccurred || !ctx->gl)
    {
        const char* error = "Failed to create an OpenGL context";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        goto create_context_failed;
    }

    // set context
    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
    {
        const char* error = "Failed to set OpenGL context as current";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        goto set_context_failed;
    }

    /* Set up quad mesh */
    glGenBuffers(1, &ctx->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    XFree(fbconfigs);

    return IUP_NOERROR;

set_context_failed                      : glXDestroyContext(ctx->display, ctx->gl);
create_context_failed                   :
ARB_create_context_profile_doesnt_exist :
query_extension_string_failed           : XFree(fbconfigs);
choose_fbconfig_failed                  : return IUP_NOERROR;
}

static void DestroyGLXContext(Context3D* ctx)
{
    if (glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
    {
        if (ctx->program)
            glDeleteProgram(ctx->program);

        for (int i = 0; i != 8; ++i)
            if (ctx->texture[i])
                glDeleteTextures(1, &ctx->texture[i]);

        glDeleteBuffers(1, &ctx->vbo);
    }

    glXDestroyContext(ctx->display, ctx->gl);
}

static int CanvasResize_CB(Ihandle* ih, int width, int height)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
        return IUP_DEFAULT;

    glViewport(0, 0, width, height);
    ctx->canvas_width = width;
    ctx->canvas_height = height;

    return IUP_DEFAULT;
}

static int Redraw_CB(Ihandle* ih, float x, float y)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");

    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
        return IUP_DEFAULT;

    if (ctx->program == 0)
    {
        char* error;
        ctx->program = gl_load_shader(vs_quad, fs_quad, attr_bindings, &error);
        ctx->u_offset = glGetUniformLocation(ctx->program, "u_offset");
        ctx->u_aspect = glGetUniformLocation(ctx->program, "u_aspect");
        ctx->s_texture[0] = glGetUniformLocation(ctx->program, "s_texture0");
        if (ctx->program == 0)
        {
            log_err("Failed to load shader: %s\n", error);
            free(error);
        }
    }

    GLfloat offsetx = 0.0, offsety = 0.0, aspectx = 1.0, aspecty = 1.0;
    GLfloat canvas_ar = (GLfloat)ctx->canvas_width / (GLfloat)ctx->canvas_height;
    GLfloat texture_ar = (GLfloat)ctx->texture_width / (GLfloat)ctx->texture_height;
    if (canvas_ar > texture_ar)
    {
        offsetx = ((GLfloat)ctx->canvas_height * texture_ar - ctx->canvas_width) / 2.0 / ctx->canvas_width;
        aspectx = canvas_ar / texture_ar;
    }
    else if (canvas_ar < texture_ar)
    {
        offsety = ((GLfloat)ctx->canvas_width / texture_ar - ctx->canvas_height) / 2.0 / ctx->canvas_height;
        aspecty = texture_ar / canvas_ar;
    }

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));

    glUseProgram(ctx->program);
    glUniform2f(ctx->u_offset, offsetx, offsety);
    glUniform2f(ctx->u_aspect, aspectx, aspecty);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->texture[0]);
    glUniform1i(ctx->s_texture[0], 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glXSwapBuffers(ctx->display, ctx->window);

    return IUP_DEFAULT;
}

static int CanvasCreateMethod(Ihandle* ih, void** params)
{
    Context3D* ctx;
    (void)params;

    ctx = mem_alloc(sizeof *ctx);
    iupAttribSet(ih, "_IUP_GLXCONTEXT", (char*)ctx);

    IupSetCallback(ih, "RESIZE_CB", (Icallback)CanvasResize_CB);
    IupSetCallback(ih, "ACTION", (Icallback)Redraw_CB);

    return IUP_NOERROR;
}

static int CanvasMapMethod(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    memset(ctx, 0, sizeof *ctx);

    /* Need the X11 display and window handle */
    ctx->display = iupdrvGetDisplay();
    if (ctx->display == NULL)
        return IUP_NOERROR;

    ctx->window = (Window)iupAttribGet(ih, "XWINDOW"); /* check first in the hash table, can be defined by the IupFileDlg */
    if (!ctx->window)
        ctx->window = (Window)IupGetAttribute(ih, "XWINDOW");  /* works only after mapping the IupCanvas */
    if (!ctx->window)
        return IUP_NOERROR;

    /* "0" is a valid uniform value. -1 is the default, invalid value */
    ctx->u_aspect = -1;
    ctx->u_offset = -1;
    for (int i = 0; i != 8; ++i)
        ctx->s_texture[i] = -1;

    return CreateGLXContext(ih, ctx);
}

static void CanvasUnMapMethod(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    DestroyGLXContext(ctx);
}

static void CanvasDestroyMethod(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    mem_free(ctx);
    iupAttribSet(ih, "_IUP_GLXCONTEXT", NULL);
}

/* ------------ Internal API ----------------------------------------------- */

int iupdrvGfxInit(void)
{
#define LOAD(name) \
    *(void**)&name = glXGetProcAddress((const GLubyte*)#name); \
    if (name == NULL) \
        return -1

    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glAttachShader);
    LOAD(glDeleteShader);

    LOAD(glCreateProgram);
    LOAD(glDeleteProgram);
    LOAD(glGetProgramInfoLog);
    LOAD(glGetProgramiv);
    LOAD(glBindAttribLocation);
    LOAD(glLinkProgram);
    LOAD(glGetUniformLocation);
    LOAD(glDeleteProgram);
    LOAD(glUseProgram);
    LOAD(glUniform1i);
    LOAD(glUniform2f);

    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glDeleteBuffers);

    LOAD(glEnableVertexAttribArray);
    LOAD(glVertexAttribPointer);

#undef LOAD

    return 0;
}

void iupdrvGfxDeInit(void)
{
}

void iupdrvGfxCanvasInitClass(Iclass* ic)
{
    ic->Create = CanvasCreateMethod;
    ic->Map = CanvasMapMethod;
    ic->UnMap = CanvasUnMapMethod;
    ic->Destroy = CanvasDestroyMethod;
}

int iupdrvGfxCanvasGetTexWidth(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    return ctx->texture_width;
}

int iupdrvGfxCanvasGetTexHeight(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    return ctx->texture_height;
}

void iupdrvGfxCanvasSetTexSize(Ihandle* ih, int width, int height)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");

    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
        return;

    ctx->texture_width = width;
    ctx->texture_height = height;

    for (int i = 0; i != 8; ++i)
        if (ctx->texture[i])
        {
            glDeleteTextures(1, &ctx->texture[i]);
            ctx->texture[i] = 0;
            ctx->s_texture[i] = -1;
        }
}

void iupdrvGfxCanvasSetTexRGBA(Ihandle* ih, int id, const char* value)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");

    if (id > 7)
    {
        char buf[64];
        snprintf(buf, 64, "Only support 8 textures (requested index %d)", id);
        iupAttribSet(ih, "ERROR", buf);
        log_err("%s\n", buf);
        return;
    }
    if (id == IUP_INVALID_ID)
    {
        id = 0;
    }
    else if (id < 0)
    {
        log_err("Invalid ID was passed\n");
        return;
    }

    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl))
        return;

    if (value == NULL)
    {
        if (ctx->texture[id])
        {
            glDeleteTextures(1, &ctx->texture[id]);
            ctx->texture[id] = 0;
            ctx->s_texture[id] = -1;
        }
        return;
    }

    if (ctx->texture[id] == 0)
    {
        /* Create a new texture */
        GLfloat border_color[] = {0.1, 0.1, 0.1, 1.0};
        glGenTextures(1, &ctx->texture[id]);
        glBindTexture(GL_TEXTURE_2D, ctx->texture[id]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ctx->texture_width, ctx->texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, value);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {
        /* Update the existing texture */
        glBindTexture(GL_TEXTURE_2D, ctx->texture[id]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ctx->texture_width, ctx->texture_height, GL_RGBA, GL_UNSIGNED_BYTE, value);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    /* The shader has to be recompiled */
    if (ctx->program)
        glDeleteProgram(ctx->program);
    ctx->program = 0;
    ctx->u_aspect = -1;
    ctx->u_offset = -1;
    for (int i = 0; i != 8; ++i)
        ctx->s_texture[i] = -1;
}
