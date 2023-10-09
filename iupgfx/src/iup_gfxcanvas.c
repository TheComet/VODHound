/** \file
 * \brief IupBackgroundBox control
 *
 * See Copyright Notice in "iup.h"
 */

#include "iup.h"
#include "iupgfx.h"

#include "iup_object.h"
#include "iup_register.h"

#include <stddef.h>

int iupdrvGfxInit(void);
void iupdrvGfxDeInit(void);
void iupdrvGfxCanvasInitClass(Iclass* ic);
void iupdrvGfxSetTexture(Ihandle* ih, int id, const char* value);

static int iGfxCanvasSetTexture(Ihandle* ih, int id, const char* value)
{
    if (!ih->handle)  /* Do not execute before map */
        return 0;
    iupdrvGfxSetTexture(ih, id, value);
    return 0;
}

static Iclass* i3DCanvasNewClass(void)
{
    Iclass* ic = iupClassNew(iupRegisterFindClass("canvas"));

    ic->name = "3dcanvas";
    ic->cons = "3DCanvas";
    ic->format = "a"; /* one ACTION callback name */
    ic->nativetype = IUP_TYPECANVAS;
    ic->childtype = IUP_CHILDNONE;
    ic->is_interactive = 1;

    ic->New = i3DCanvasNewClass;

    iupClassRegisterAttribute(ic, "ERROR", NULL, NULL, NULL, NULL, IUPAF_READONLY | IUPAF_NO_INHERIT);
    iupClassRegisterAttributeId(ic, "TEXTURE", NULL, iGfxCanvasSetTexture, IUPAF_NOT_MAPPED | IUPAF_WRITEONLY | IUPAF_NO_INHERIT);

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
    return IupCreatev("3dcanvas", params);
}

void IupGfxOpen(void)
{
    if (!IupIsOpened())
        return;

    if (!IupGetGlobal("_IUP_GLCANVAS_OPEN"))
    {
        if (iupdrvGfxInit() != 0)
            return;
        iupRegisterClass(i3DCanvasNewClass());
        IupSetGlobal("_IUP_GLCANVAS_OPEN", "1");
    }
}

void IupGfxClose(void)
{
    iupdrvGfxDeInit();
    Iclass* ic = iupRegisterFindClass("3dcanvas");
    if (ic)
        iupUnRegisterClass(ic);
}
