/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

/*
 * this atrocity is necessary on sparc because registers modified
 * by the child get propagated back to the parent via the window
 * save/restore mechanism.
 */

#ifndef _VFORK_H
#define _VFORK_H

#ident	"@(#)vfork.h	1.2	90/10/12 SMI"

#ifdef	__STDC__
extern pid_t vfork(void);
#else
extern int vfork();
#endif

#ifdef sparc
#pragma unknown_control_flow(vfork)
#endif

#endif /*!_VFORK_H*/
