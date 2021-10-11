/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TTYDEV_H
#define	_SYS_TTYDEV_H

#pragma ident	"@(#)ttydev.h	1.7	92/07/14 SMI"	/* UCB 4.3 83/05/18 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Terminal definitions related to underlying hardware.
 */

/*
 * Speeds
 */
#define	B0	0
#define	B50	1
#define	B75	2
#define	B110	3
#define	B134	4
#define	B150	5
#define	B200	6
#define	B300	7
#define	B600	8
#define	B1200	9
#define	B1800	10
#define	B2400	11
#define	B4800	12
#define	B9600	13
#define	B19200	14
#define	B38400	15
#define	EXTA	14
#define	EXTB	15

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_TTYDEV_H */
