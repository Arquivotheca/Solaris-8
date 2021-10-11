/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * WARNING: The ustat system call will become obsolete in the
 * next major release following SVR4. Application code should
 * migrate to the replacement system call statvfs(2).
 */

#ifndef _SYS_USTAT_H
#define	_SYS_USTAT_H

#pragma ident	"@(#)ustat.h	1.11	96/10/27 SMI"	/* SVr4.0 11.8 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct  ustat {
	daddr_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_fname[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

#if defined(_SYSCALL32)

struct	ustat32 {
	daddr32_t	f_tfree;	/* total free */
	ino32_t		f_tinode;	/* total inodes free */
	char		f_fname[6];	/* filsys name */
	char		f_fpack[6];	/* filsys pack name */
};

#endif	/* _SYSCALL32 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_USTAT_H */
