/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _PKGDEV_H
#define	_PKGDEV_H

#pragma ident	"@(#)pkgdev.h	1.9	96/04/25 SMI"	/* SVr4.0 1.2.1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

struct pkgdev {
	int			rdonly;
	int			mntflg;
	longlong_t 	capacity; /* number of 512-blocks on device */
	char		*name;
	char		*dirname;
	char		*pathname;
	char		*mount;
	char		*fstyp;
	char		*cdevice;
	char		*bdevice;
	char		*norewind;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _PKGDEV_H */
