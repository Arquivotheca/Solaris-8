/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uparm.h	1.11	92/07/14 SMI"	/* from SVr4.0 1.3.2.1 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 */

extern char *libpath();

#ifdef preSVr4
#ifdef USG
# define LIBPATH          "/usr/lib/mailx"
#else
# define LIBPATH          "/usr/lib"
#endif
#else
# define LIBPATH          "/usr/share/lib/mailx"
#endif
#define	LOCALEPATH	"/usr/lib/locale"
