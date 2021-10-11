/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986, 1987, 1988, 1989, 1990, 1995  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef	_VM_SEG_ENUM_H
#define	_VM_SEG_ENUM_H

#pragma ident	"@(#)seg_enum.h	1.3	95/12/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These enumerations are needed in both <vm/seg.h> and
 * <sys/vnode.h> in order to declare function prototypes.
 */

/*
 * Fault information passed to the seg fault handling routine.
 * The F_SOFTLOCK and F_SOFTUNLOCK are used by software
 * to lock and unlock pages for physical I/O.
 */
enum fault_type {
	F_INVAL,		/* invalid page */
	F_PROT,			/* protection fault */
	F_SOFTLOCK,		/* software requested locking */
	F_SOFTUNLOCK		/* software requested unlocking */
};

/*
 * Lock information passed to the seg pagelock handling routine.
 */
enum lock_type {
	L_PAGELOCK,		/* lock pages */
	L_PAGEUNLOCK,		/* unlock pages */
	L_PAGERECLAIM		/* reclaim pages */
};

/*
 * seg_rw gives the access type for a fault operation
 */
enum seg_rw {
	S_OTHER,		/* unknown or not touched */
	S_READ,			/* read access attempted */
	S_WRITE,		/* write access attempted */
	S_EXEC,			/* execution access attempted */
	S_CREATE		/* create if page doesn't exist */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_ENUM_H */
