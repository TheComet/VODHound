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
    {{ 1, -1}},
    {{-1,  1}}
};
static const char* attr_bindings[] = {
    "vPosition",
    NULL
};
static const char* vs_quad = "\
precision mediump float;\n\
attribute vec2 vPosition;\n\
varying vec2 fTexCoord;\n\
void main() {\
    // map [-1, 1] to texture coordinate [0, 1]\
    fTexCoord = vPosition * 2.0 - 1.0;\n\
    gl_Position = vec4(vPosition, 0.0, 1.0);\n\
}";
static const char* fs_yuv420 = "\
precision mediump float;\n\
varying vec2 fTexCoord;\n\
uniform sampler2D sTex0;\n\
void main() {\n\
    vec3 col0 = texture2D(sTex0, fTexCoord);\n\
    gl_FragColor = vec4(col0, 1.0);\n\
}";

typedef struct Context3D
{
    Display* display;
    GLXContext gl;
    Window window;
    GLuint vbo;
    GLuint tex[8];
    GLuint program;
    GLuint sTex[8];
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
        return IUP_NOERROR;
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
        return IUP_NOERROR;
    }
    log_dbg("GLX extensions: %s\n", glxstr);

    if (!ExtensionExistsInString(glxstr, "GLX_ARB_create_context_profile") ||
        !glXCreateContextAttribsARB)
    {
        const char* error = "GLX does not support GLX_ARB_create_context_profile";
        iupAttribSet(ih, "ERROR", error);
        iupAttribSetStr(ih, "LASTERROR", error);
        return IUP_NOERROR;
    }

    // Install an X error handler so the application won't exit if GL 3.0
    // context allocation fails.
    //
    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    ctxErrorOccurred = 0;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);

    int ctx_flags = 0;
    int profile_mask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
#if defined(DEBUG)
    //ctx_flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
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

    // Restore the original error handler
    XSetErrorHandler( oldHandler );

    if (ctxErrorOccurred || !ctx->gl)
    {
        log_err("Failed to create an OpenGL context\n");
        return IUP_NOERROR;
    }

    // Verifying that context is a direct context
    if (!glXIsDirect(ctx->display, ctx->gl))
        log_dbg( "Indirect GLX rendering context obtained\n" );
    else
        log_dbg( "Direct GLX rendering context obtained\n" );

    // set context
    if (!glXMakeCurrent(ctx->display, ctx->window, ctx->gl)) {
        glXDestroyContext(ctx->display, ctx->gl);
    }

    /* Set up quad mesh */
    glGenBuffers(1, &ctx->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Prepare background textures */
    glGenTextures(1, &ctx->tex[0]);
    glBindTexture(GL_TEXTURE_2D, ctx->tex[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    ctx->program = gl_load_shader(vs_quad, fs_yuv420, attr_bindings, NULL);
    ctx->sTex[0] = glGetUniformLocation(ctx->program, "sTex0");

    return IUP_NOERROR;
}

static void DestroyGLXContext(Context3D* ctx)
{
    glXDestroyContext(ctx->display, ctx->gl);
}

static int CanvasResize_CB(Ihandle* ih, int width, int height)
{
    log_dbg("resize cb\n");
    glViewport(0, 0, width, height);
    return IUP_DEFAULT;
}

static int Redraw_CB(Ihandle* ih, float x, float y)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");

    glXMakeCurrent(ctx->display, ctx->window, ctx->gl);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));

    glUseProgram(ctx->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->tex[0]);
    glUniform1i(ctx->sTex[0], 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    IupGfxSwapBuffers(ih);

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

    /* Need the X11 display and window handle */
    ctx->display = iupdrvGetDisplay();
    if (ctx->display == NULL)
        return IUP_NOERROR;

    ctx->window = (Window)iupAttribGet(ih, "XWINDOW"); /* check first in the hash table, can be defined by the IupFileDlg */
    if (!ctx->window)
        ctx->window = (Window)IupGetAttribute(ih, "XWINDOW");  /* works only after mapping the IupCanvas */
    if (!ctx->window)
        return IUP_NOERROR;

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

void iupdrvGfxSetTexture(Ihandle* ih, int id, const char* value)
{

}

/* ------------ Public API ------------------------------------------------- */

int IupGfxIsCurrent(Ihandle* ih)
{
    (void)ih;
    return 1;
}

void IupGfxMakeCurrent(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    glXMakeCurrent(ctx->display, ctx->window, ctx->gl);
}

void IupGfxSwapBuffers(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
    glXSwapBuffers(ctx->display, ctx->window);
}
