/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ACCT_H
#define	_SYS_ACCT_H

#pragma ident	"@(#)acct.h	1.17	97/08/12 SMI"	/* SVr4.0 11.9	*/

#include <sys/types.h>
#include <sys/types32.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Accounting structures
 */

typedef	ushort_t comp_t;		/* "floating point" */
		/* 13-bit fraction, 3-bit exponent  */

/* SVR4 acct structure */
struct acct {
	char	ac_flag;		/* Accounting flag */
	char	ac_stat;		/* Exit status */
	uid32_t	ac_uid;			/* Accounting user ID */
	gid32_t	ac_gid;			/* Accounting group ID */
	dev32_t	ac_tty;			/* control typewriter */
	time32_t ac_btime;		/* Beginning time */
	comp_t	ac_utime;		/* acctng user time in clock ticks */
	comp_t	ac_stime;		/* acctng system time in clock ticks */
	comp_t	ac_etime;		/* acctng elapsed time in clock ticks */
	comp_t	ac_mem;			/* memory usage */
	comp_t	ac_io;			/* chars transferred */
	comp_t	ac_rw;			/* blocks read or written */
	char	ac_comm[8];		/* command name */
};

/*
 * Account commands will use this header to read SVR3
 * accounting data files.
 */

struct o_acct {
	char	ac_flag;		/* Accounting flag */
	char	ac_stat;		/* Exit status */
	o_uid_t	ac_uid;			/* Accounting user ID */
	o_gid_t	ac_gid;			/* Accounting group ID */
	o_dev_t	ac_tty;			/* control typewriter */
	time32_t ac_btime;		/* Beginning time */
	comp_t	ac_utime;		/* acctng user time in clock ticks */
	comp_t	ac_stime;		/* acctng system time in clock ticks */
	comp_t	ac_etime;		/* acctng elapsed time in clock ticks */
	comp_t	ac_mem;			/* memory usage */
	comp_t	ac_io;			/* chars transferred */
	comp_t	ac_rw;			/* blocks read or written */
	char	ac_comm[8];		/* command name */
};

#if !defined(_KERNEL)
#if defined(__STDC__)
extern int acct(const char *);
#else
extern int acct();
#endif
#endif /* !defined(_KERNEL) */

#if defined(_KERNEL)
void	acct(char);
int	sysacct(char *);
#endif

#define	AFORK	0001		/* has executed fork, but no exec */
#define	ASU	0002		/* used super-user privileges */
#ifdef SUN_SRC_COMPAT
#define	ACOMPAT	0004		/* used compatibility mode (VAX) */
#define	ACORE	0010		/* dumped core */
#define	AXSIG	0020		/* killed by a signal */
#endif /* SUN_SRC_COMPAT */
#define	AEXPND	0040		/* expanded acct structure */
#define	ACCTF	0300		/* record type: 00 = acct */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ACCT_H */
