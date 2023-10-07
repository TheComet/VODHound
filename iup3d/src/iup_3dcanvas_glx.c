#include "iup.h"
#include "iupcbs.h"
#include "iup3d.h"

#include "vh/mem.h"
#include "vh/log.h"

#include "iup_attrib.h"
#include "iup_class.h"

typedef struct Context3D
{
    void* window;
} Context3D;

static int CreateGLXContext(Ihandle* ih, Context3D* ctx)
{
    return IUP_NOERROR;
}

static void DestroyGLXContext(Context3D* ctx)
{
}

static int CanvasDefaultResize_CB(Ihandle* ih, int width, int height)
{
    return IUP_DEFAULT;
}

static int CanvasCreateMethod(Ihandle* ih, void** params)
{
    Context3D* ctx;
    (void)params;

    ctx = mem_alloc(sizeof *ctx);
    iupAttribSet(ih, "_IUP_GLXCONTEXT", (char*)ctx);

    IupSetCallback(ih, "RESIZE_CB", (Icallback)CanvasDefaultResize_CB);

    return IUP_NOERROR;
}

static int CanvasMapMethod(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");

    /* get a device context */
    ctx->window = iupAttribGet(ih, "HWND"); /* check first in the hash table, can be defined by the IupFileDlg */
    if (!ctx->window)
        ctx->window = IupGetAttribute(ih, "HWND");  /* works for Win32 and GTK, only after mapping the IupCanvas */
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
    iupAttribSet(ih, "_IUP_GLXCONTEXT", NULL);;
}

void iupdrv3DCanvasInitClass(Iclass* ic)
{
    ic->Create = CanvasCreateMethod;
    ic->Map = CanvasMapMethod;
    ic->UnMap = CanvasUnMapMethod;
    ic->Destroy = CanvasDestroyMethod;
}

int Iup3DIsCurrent(Ihandle* ih)
{
    (void)ih;
    return 1;
}

void Iup3DMakeCurrent(Ihandle* ih)
{
    (void)ih;
}

void Iup3DSwapBuffers(Ihandle* ih)
{
    Context3D* ctx = (Context3D*)iupAttribGet(ih, "_IUP_GLXCONTEXT");
}
