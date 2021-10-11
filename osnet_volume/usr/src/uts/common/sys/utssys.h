/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_UTSSYS_H
#define	_SYS_UTSSYS_H

#pragma ident	"@(#)utssys.h	1.10	96/12/03 SMI"	/* SVr4.0 1.1 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions related to the utssys() system call.
 */

/*
 * "commands" of utssys
 */
#define	UTS_UNAME	0x0	/* obsolete */
#define	UTS_USTAT	0x2	/* 1 was umask */
#define	UTS_FUSERS	0x3

/*
 * Flags to UTS_FUSERS
 */
#define	F_FILE_ONLY	0x1
#define	F_CONTAINED	0x2

/*
 * structure yielded by UTS_FUSERS
 */
typedef struct f_user {
	pid_t	fu_pid;
	int	fu_flags;	/* see below */
	uid_t	fu_uid;
} f_user_t;

/*
 * fu_flags values
 */
#define	F_CDIR		0x1
#define	F_RDIR		0x2
#define	F_TEXT		0x4
#define	F_MAP		0x8
#define	F_OPEN		0x10
#define	F_TRACE		0x20
#define	F_TTY		0x40

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_UTSSYS_H */
