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

#ifndef	_SYS_UN_H
#define	_SYS_UN_H

#pragma ident	"@(#)un.h	1.9	96/07/12 SMI"	/* UCB 7.1 6/4/86 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _SA_FAMILY_T
#define	_SA_FAMILY_T
typedef	unsigned short sa_family_t;
#endif

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
	sa_family_t	sun_family;		/* AF_UNIX */
	char		sun_path[108];		/* path name (gag) */
};

#ifdef _KERNEL
int	unp_discard();
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_UN_H */
