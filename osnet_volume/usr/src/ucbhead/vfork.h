/*
 * Copyright (c) 1986-1997 by Sun Microsystems, Inc.
 */

/*
 * this atrocity is necessary on sparc because registers modified
 * by the child get propagated back to the parent via the window
 * save/restore mechanism.
 */

#ifndef _vfork_h
#define	_vfork_h

#pragma ident	"@(#)vfork.h	1.4	97/06/17 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern int vfork(void);
#else
extern int vfork();
#endif

#if defined(sparc) || defined(__sparcv9)
#pragma unknown_control_flow(vfork)
#endif

#ifdef __cplusplus
}
#endif

#endif /* !_vfork_h */
