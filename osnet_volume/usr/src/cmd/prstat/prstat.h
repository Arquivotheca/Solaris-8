/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRSTAT_H
#define	_PRSTAT_H

#pragma ident	"@(#)prstat.h	1.3	99/11/03 SMI"

#include <sys/sysmacros.h>
#include <sys/time.h>
#include <procfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * FRC2PCT macro is used to convert 16-bit binary fractions in the range
 * 0.0 to 1.0 with binary point to the right of the high order bit
 * (i.e. 1.0 == 0x8000) to percentage value.
 */

#define	FRC2PCT(pp)	(((float)(pp))/0x8000*100)

#define	TIME2NSEC(__t)\
(hrtime_t)(((hrtime_t)__t.tv_sec * (hrtime_t)NANOSEC) + (hrtime_t)__t.tv_nsec)
#define	TIME2SEC(__t)\
(hrtime_t)(__t.tv_sec)

/*
 * List of available output modes
 */

#define	OPT_PSINFO	0x0001		/* read process's data from "psinfo" */
#define	OPT_LWPS	0x0002		/* report about all lwps */
#define	OPT_USERS	0x0004		/* report about most active users */
#define	OPT_USAGE	0x0008		/* read process's data from "usage" */
#define	OPT_REALTIME	0x0010		/* real-time scheduling class flag */
#define	OPT_MSACCT	0x0020		/* microstate accounting flag */
#define	OPT_TERMCAP	0x0040		/* use termcap data to move cursor */
#define	OPT_SPLIT	0x0080		/* split-screen mode flag */
#define	OPT_TTY		0x0100		/* report results to tty or file */
#define	OPT_FULLSCREEN	0x0200		/* full-screen mode flag */
#define	OPT_USEHOME	0x0400		/* use 'home' to move cursor up */

/*
 * Flags to keep track of microstate accounting status for lwps
 */

#define	MSACCT_IS_SET	0x0001
#define	MSACCT_WAS_SET	0x0002
#define	MSACCT_CANT_SET	0x0004

/*
 * Linked list of structures for every process or lwp in the system
 */

typedef struct lwp_info {
	psinfo_t	li_info;	/* data read from psinfo file */
	prusage_t	li_usage;	/* data read from usage file */
	ulong_t		li_key;		/* value of the key for this lwp */
	int		li_msacct;	/* microstate accounting state */
	int		li_alive;	/* flag for alive lwps */
	float		li_usr;		/* user level CPU time */
	float		li_sys;		/* system call CPU time */
	float		li_trp;		/* other system trap CPU time */
	float		li_tfl;		/* text page fault sleep time */
	float		li_dfl;		/* data page fault sleep time */
	float		li_lck;		/* user lock wait sleep time */
	float		li_slp;		/* all other sleep time */
	float		li_lat;		/* wait-cpu (latency) time */
	ulong_t		li_vcx;		/* voluntary context switches */
	ulong_t		li_icx;		/* involuntary context switches */
	ulong_t		li_scl;		/* system calls */
	ulong_t		li_sig;		/* received signals */
	struct lwp_info *li_next;	/* pointer to next lwp */
	struct lwp_info *li_prev;	/* pointer to previous lwp */
} lwp_info_t;

/*
 * Linked list of structures for every user in the system
 */

typedef struct ulwp_info {
	uid_t		ui_uid;		/* user id */
	uint_t		ui_nproc;	/* number of user's processes */
	size_t		ui_size;	/* user's memory usage */
	size_t		ui_rssize;	/* user's resident set size */
	ulong_t		ui_time;	/* user's cpu time (in secs) */
	float		ui_pctcpu;	/* user's overall cpu usage */
	float		ui_pctmem;	/* user's overall memory usage */
	ulong_t		ui_key;		/* user's sort key value */
	struct ulwp_info *ui_next;	/* pointer to next user */
	struct ulwp_info *ui_prev;	/* pointer to previous user */
} ulwp_info_t;

/*
 * Command line options
 */

typedef	struct optdesc {
	int		o_interval;	/* interval between updates */
	int		o_nprocs;	/* number of lines to display */
	int		o_nusers;	/* number of users to display */
	int		o_count;	/* number of iterations */
	int		o_outpmode;	/* selected output mode */
	int		o_sortorder;	/* +1 ascending, -1 descending */
} optdesc_t;

extern void lwp_clear();
extern void ulwp_clear();
extern void curses_off();

#ifdef	__cplusplus
}
#endif

#endif	/* _PRSTAT_H */
