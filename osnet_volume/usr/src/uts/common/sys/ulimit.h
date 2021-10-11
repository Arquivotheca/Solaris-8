/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

#ifndef	_SYS_ULIMIT_H
#define	_SYS_ULIMIT_H

#pragma ident	"@(#)ulimit.h	1.10	93/10/15 SMI"	/* SVr4.0 1.5 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following are codes which can be
 * passed to the ulimit system call.
 */

#if defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE)
#define	UL_GFILLIM	UL_GETFSIZE	/* get file limit */
#define	UL_SFILLIM	UL_SETFSIZE	/* set file limit */
#define	UL_GMEMLIM	3		/* get process size limit */
#define	UL_GDESLIM	4		/* get file descriptor limit */
#define	UL_GTXTOFF	64		/* get text offset */
#endif

/*
 * The following are symbolic constants required for
 * X/Open Conformance.   They are the equivalents of
 * the constants above.
 */

#define	UL_GETFSIZE	1	/* get file limit */
#define	UL_SETFSIZE	2	/* set file limit */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ULIMIT_H */
