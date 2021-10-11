/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MODE_H
#define	_SYS_MODE_H

#pragma ident	"@(#)mode.h	1.10	98/01/06 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * REQUIRES sys/stat.h
 * REQUIRES sys/vnode.h
 */

/*
 * Conversion between vnode types/modes and encoded type/mode as
 * seen by stat(2) and mknod(2).
 */
extern enum vtype	iftovt_tab[];
extern ushort_t		vttoif_tab[];
#define	IFTOVT(M)	(iftovt_tab[((M) & S_IFMT) >> 12])
#define	VTTOIF(T)	(vttoif_tab[(int)(T)])
#define	MAKEIMODE(T, M)	(VTTOIF(T) | ((M) & ~S_IFMT))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MODE_H */
