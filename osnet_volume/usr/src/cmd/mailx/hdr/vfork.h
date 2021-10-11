/*	@(#)vfork.h 1.3 88/08/19 SMI	*/

/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

/*
 * this atrocity is necessary on sparc because registers modified
 * by the child get propagated back to the parent via the window
 * save/restore mechanism.
 */

#ifndef _vfork_h
#define _vfork_h

#ifdef __STDC__
extern pid_t vfork(void);
#else
extern int vfork();
#endif

#ifdef sparc
#pragma unknown_control_flow(vfork)
#endif

#endif /*!_vfork_h*/
