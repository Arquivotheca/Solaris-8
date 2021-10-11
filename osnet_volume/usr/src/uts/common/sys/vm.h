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
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VM_H
#define	_SYS_VM_H

#pragma ident	"@(#)vm.h	2.26	98/11/25 SMI"

#include <sys/vmparam.h>
#include <sys/vmmeter.h>
#include <sys/vmsystm.h>
#include <sys/sysmacros.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)
#include <sys/vnode.h>

void	setupclock(int);
void	pageout(void);
void	cv_signal_pageout(void);
int	queue_io_request(struct vnode *, u_offset_t);

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VM_H */
