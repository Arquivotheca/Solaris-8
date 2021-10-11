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
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VMPARAM_H
#define	_SYS_VMPARAM_H

#pragma ident	"@(#)vmparam.h	2.35	98/06/03 SMI"

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/vm_machparam.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	SSIZE		4096		/* initial stack size */
#define	SINCR		4096		/* increment of stack */

/*
 * USRSTACK is the top (end) of the user stack.
 */
#define	USRSTACK	USERLIMIT
#define	USRSTACK32	USERLIMIT32

/*
 * Implementation architecture independent sections of the kernel use
 * this section.
 */
#if (defined(_KERNEL) || defined(_KMEMUSER)) && !defined(_MACHDEP)

#if defined(_KERNEL) && !defined(_ASM)
extern const unsigned int	_diskrpm;
extern const unsigned long	_dsize_limit;
extern const unsigned long	_ssize_limit;
extern const unsigned long	_pgthresh;
extern const unsigned int	_maxslp;
extern const unsigned long	_maxhandspreadpages;
#endif	/* defined(_KERNEL) && !defined(_ASM) */

#define	DISKRPM		_diskrpm
#define	DSIZE_LIMIT	_dsize_limit
#define	SSIZE_LIMIT	_ssize_limit
#define	PGTHRESH	_pgthresh
#define	MAXSLP		_maxslp
#define	MAXHANDSPREADPAGES	_maxhandspreadpages

#endif	/* (defined(_KERNEL) || defined(_KMEMUSER)) && !defined(_MACHDEP) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VMPARAM_H */
