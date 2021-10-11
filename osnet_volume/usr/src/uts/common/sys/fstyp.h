/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FSTYP_H
#define	_SYS_FSTYP_H

#pragma ident	"@(#)fstyp.h	1.10	96/02/07 SMI"	/* SVr4.0 11.6	*/

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	FSTYPSZ
#define	FSTYPSZ		16	/* max size of fs identifier */
#endif

/*
 * Opcodes for the sysfs() system call.
 */
#define	GETFSIND	1	/* translate fs identifier to fstype index */
#define	GETFSTYP	2	/* translate fstype index to fs identifier */
#define	GETNFSTYP	3	/* return the number of fstypes */

#if defined(__STDC__) && !defined(_KERNEL)
int sysfs(int, ...);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FSTYP_H */
