/** \file
 * \brief IupBackgroundBox control
 *
 * See Copyright Notice in "iup.h"
 */

#include "iup.h"
#include "iupgfx.h"

#include "iup_object.h"
#include "iup_register.h"
#include "iup_str.h"

#include <stddef.h>

int iupdrvGfxInit(void);
void iupdrvGfxDeInit(void);
void iupdrvGfxCanvasInitClass(Iclass* ic);
int iupdrvGfxCanvasGetTexWidth(Ihandle* ih);
int iupdrvGfxCanvasGetTexHeight(Ihandle* ih);
void iupdrvGfxCanvasSetTexSize(Ihandle* ih, int width, int height);
void iupdrvGfxCanvasSetTexRGBA(Ihandle* ih, int id, const char* value);

static char* iGfxCanvasGetTexWidth(Ihandle* ih)
{
    return iupStrReturnInt(iupdrvGfxCanvasGetTexWidth(ih));
}

static char* iGfxCanvasGetTexHeight(Ihandle* ih)
{
    return iupStrReturnInt(iupdrvGfxCanvasGetTexHeight(ih));
}

static char* iGfxCanvasGetTexSize(Ihandle* ih)
{
    int w = iupdrvGfxCanvasGetTexWidth(ih);
    int h = iupdrvGfxCanvasGetTexHeight(ih);
    return iupStrReturnIntInt(w, h, 'x');
}

static int iGfxCanvasSetTexSize(Ihandle* ih, const char* value)
{
    int w, h;
    iupStrToIntInt(value, &w, &h, 'x');
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    iupdrvGfxCanvasSetTexSize(ih, w, h);
    return 0;
}

static int iGfxCanvasSetTexRGBA(Ihandle* ih, int id, const char* value)
{
    iupdrvGfxCanvasSetTexRGBA(ih, id, value);
    return 0;
}

static Iclass* i3DCanvasNewClass(void)
{
    Iclass* ic = iupClassNew(iupRegisterFindClass("canvas"));

    ic->name = "gfxcanvas";
    ic->cons = "GfxCanvas";
    ic->format = "a"; /* one ACTION callback name */
    ic->nativetype = IUP_TYPECANVAS;
    ic->childtype = IUP_CHILDNONE;
    ic->is_interactive = 1;
    ic->has_attrib_id = 1;

    ic->New = i3DCanvasNewClass;

    iupClassRegisterAttribute(ic, "ERROR", NULL, NULL, NULL, NULL, IUPAF_READONLY | IUPAF_NO_INHERIT);
    iupClassRegisterAttribute(ic, "TEXSIZE", iGfxCanvasGetTexSize, iGfxCanvasSetTexSize, "1x1", "1x1", IUPAF_NO_SAVE | IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);
    iupClassRegisterAttribute(ic, "TEXWIDTH", iGfxCanvasGetTexWidth, NULL, NULL, NULL, IUPAF_READONLY | IUPAF_NO_SAVE | IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);
    iupClassRegisterAttribute(ic, "TEXHEIGHT", iGfxCanvasGetTexHeight, NULL, NULL, NULL, IUPAF_READONLY | IUPAF_NO_SAVE | IUPAF_NOT_MAPPED | IUPAF_NO_INHERIT);
    iupClassRegisterAttributeId(ic, "TEXRGBA", NULL, iGfxCanvasSetTexRGBA, IUPAF_WRITEONLY | IUPAF_NO_INHERIT);

    iupdrvGfxCanvasInitClass(ic);

    return ic;
}

static int a = 0;
Ihandle* IupGfxCanvas(const char *action)
{
    a++;
    void* params[2];
    params[0] = (void*)action;
    params[1] = NULL;
    return IupCreatev("gfxcanvas", params);
}

void IupGfxOpen(void)
{
    if (!IupIsOpened())
        return;

    if (!IupGetGlobal("_IUP_GFXCANVAS_OPEN"))
    {
        if (iupdrvGfxInit() != 0)
            return;
        iupRegisterClass(i3DCanvasNewClass());
        IupSetGlobal("_IUP_GFXCANVAS_OPEN", "1");
    }
}

void IupGfxClose(void)
{
    iupdrvGfxDeInit();
    Iclass* ic = iupRegisterFindClass("3dcanvas");
    if (ic)
        iupUnRegisterClass(ic);
}
