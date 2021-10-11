/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Note that the guard is carefully crafted here.
 */

#ifndef _SYS_IOCTL_H

#pragma ident	"@(#)sgtty.h	1.2	97/06/09 SMI"	/* SVr4.0 1.1	*/

#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern int stty(int, struct sgttyb *);
extern int gtty(int, struct sgttyb *);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_IOCTL_H */
