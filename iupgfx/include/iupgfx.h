/** \file
 * \brief OpenGL canvas for Iup.
 *
 * See Copyright Notice in "iup.h"
 */

#ifndef __IUP3D_H
#define __IUP3D_H

#ifdef __cplusplus
extern "C" {
#endif

/* Attributes */

/* Attribute values */
#ifndef IUP_YES
#define IUP_YES    "YES"
#endif
#ifndef IUP_NO
#define IUP_NO    "NO"
#endif

void IupGfxOpen(void);
void IupGfxClose(void);

Ihandle *IupGfxCanvas(const char *action);

void IupGfxMakeCurrent(Ihandle* ih);
int IupGfxIsCurrent(Ihandle* ih);
void IupGfxSwapBuffers(Ihandle* ih);

#ifdef __cplusplus
}
#endif

#endif
