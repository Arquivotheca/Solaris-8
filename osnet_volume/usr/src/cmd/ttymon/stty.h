/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_STTY_H
#define	_STTY_H

#pragma ident	"@(#)stty.h	1.11	99/08/11 SMI"	/* SVr4.0 1.3 */

#ifdef	__cplusplus
extern "C" {
#endif

/* Stty flags. */
#define	ASYNC   1
#define	FLOW	2
#define	WINDOW	4
#define	TERMIOS 8
#ifdef	EUC
#define	EUCW	16
#define	CSIW	32
#endif	EUC

/* Path to the locale-specific ldterm.dat file. */
#ifdef	EUC
#define	_LDTERM_DAT_PATH	"/usr/lib/locale/%s/LC_CTYPE/ldterm.dat"
#endif	EUC

#define	MAX_CC	NCCS-1	/* max number of ctrl char fields printed by stty -g */
#define	NUM_MODES 4	/* number of modes printed by stty -g */
#define	NUM_FIELDS NUM_MODES+MAX_CC /* num modes + ctrl char fields (stty -g) */

struct	speeds {
	const char	*string;
	int	speed;
};

struct mds {
	const char	*string;
	long	set;
	long	reset;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _STTY_H */
