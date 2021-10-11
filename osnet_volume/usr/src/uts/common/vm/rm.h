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
 *	Copyright (c) 1986-1990,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_VM_RM_H
#define	_VM_RM_H

#pragma ident	"@(#)rm.h	1.24	98/01/06 SMI"
/*	From:	SVr4.0	"kernel:vm/rm.h	1.8"			*/

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL
/*
 * VM - Resource Management.
 */

void	rm_outofanon(void);
void	rm_outofhat(void);
size_t	rm_asrss(struct as *);
size_t	rm_assize(struct as *);
ushort_t rm_pctmemory(struct as *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_RM_H */
