/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 *	std.h 1.1 88/03/30 inccmd:std.h
 */

#ifndef	_STD_H
#define	_STD_H

#pragma ident	"@(#)std.h	1.7	92/07/14 SMI"	/* SVr4.0 1.1	*/

#ifdef	__cplusplus
extern "C" {
#endif

#define	SYSBSIZE	BSIZE		/* system block size */
#define	SYSBSHIFT	BSHIFT

#define	EFFBSIZE	SYSBSIZE	/* efficient block size */
#define	EFFBSHIFT	SYSBSHIFT

#define	MULWSIZE	2		/* multiplier 'word' */
#define	MULWSHIFT	1
#define	MULLSIZE	4		/* multiplier 'long' */
#define	MULLSHIFT	2
#define	MULBSIZE	512		/* multiplier 'block' */
#define	MULBSHIFT	9
#define	MULKSIZE	1024		/* multiplier 'k' */
#define	MULKSHIFT	10

#define	SYSTOMUL(sysblk)	((sysblk) * (SYSBSIZE / MULBSIZE))

#ifdef	__cplusplus
}
#endif

#endif	/* _STD_H */
