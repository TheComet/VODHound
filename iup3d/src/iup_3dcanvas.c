/** \file
 * \brief IupBackgroundBox control
 *
 * See Copyright Notice in "iup.h"
 */

#include "iup.h"
#include "iup3d.h"
#include "iupcbs.h"
#include "iupkey.h"

#include "iup_object.h"
#include "iup_register.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_stdcontrols.h"
#include "iup_layout.h"
#include "iup_drv.h"


void iupdrv3DCanvasInitClass(Iclass* ic);

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

    iupClassRegisterCallback(ic, "SWAPBUFFERS_CB", "");
    iupClassRegisterAttribute(ic, "ERROR", NULL, NULL, NULL, NULL, IUPAF_READONLY | IUPAF_NO_INHERIT);

    iupdrv3DCanvasInitClass(ic);

    return ic;
}

static int a = 0;
Ihandle* Iup3DCanvas(const char *action)
{
    a++;
    void* params[2];
    params[0] = (void*)action;
    params[1] = NULL;
    return IupCreatev("3dcanvas", params);
}

void Iup3DOpen(void)
{
    if (!IupIsOpened())
        return;

    if (!IupGetGlobal("_IUP_GLCANVAS_OPEN"))
    {
        iupRegisterClass(i3DCanvasNewClass());
        IupSetGlobal("_IUP_GLCANVAS_OPEN", "1");
    }
}

void Iup3DClose(void)
{
    Iclass* ic = iupRegisterFindClass("3dcanvas");
    iupUnRegisterClass(ic);
}
