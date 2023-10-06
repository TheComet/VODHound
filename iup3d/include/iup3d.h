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

void Iup3DOpen(void);
void Iup3DClose(void);

Ihandle *Iup3DCanvas(const char *action);

void Iup3DMakeCurrent(Ihandle* ih);
int Iup3DIsCurrent(Ihandle* ih);
void Iup3DSwapBuffers(Ihandle* ih);

#ifdef __cplusplus
}
#endif

#endif
