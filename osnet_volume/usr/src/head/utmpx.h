/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
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
 *	(c) 1986,1987,1988,1989,1996,1997 Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _UTMPX_H
#define	_UTMPX_H

#pragma ident	"@(#)utmpx.h	1.16	97/08/23 SMI"	/* SVr4.0 1.4	*/

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utmp.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	_UTMPX_FILE	"/var/adm/utmpx"
#define	_WTMPX_FILE	"/var/adm/wtmpx"
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	UTMPX_FILE	_UTMPX_FILE
#define	WTMPX_FILE	_WTMPX_FILE
#endif

#define	ut_name	ut_user
#define	ut_xtime ut_tv.tv_sec

/*
 * This data structure describes the utmpx entries returned by
 * the getutxent(3c) family of APIs.  It does not (necessarily)
 * correspond to the contents of the utmpx or wtmpx files.
 *
 * Applications should only interact with this subsystem via
 * the getutxent(3c) family of APIs.
 */
struct utmpx {
	char	ut_user[32];		/* user login name */
	char	ut_id[4];		/* inittab id */
	char	ut_line[32];		/* device name (console, lnxx) */
	pid_t	ut_pid;			/* process id */
	short	ut_type;		/* type of entry */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
	struct exit_status ut_exit;	/* process termination/exit status */
#else
	struct ut_exit_status ut_exit;	/* process termination/exit status */
#endif
	struct timeval ut_tv;		/* time entry was made */
	int	ut_session;		/* session ID, used for windowing */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
	int	pad[5];			/* reserved for future use */
#else
	int	__pad[5];		/* reserved for future use */
#endif
	short	ut_syslen;		/* significant length of ut_host */
					/*   including terminating null */
	char	ut_host[257];		/* remote host name */
};

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#include <sys/types32.h>
#include <inttypes.h>

/*
 * This data structure describes the utmp *file* contents using
 * fixed-width data types.  It should only be used by the implementation.
 *
 * Applications should use the getutxent(3c) family of routines to interact
 * with this database.
 */

struct futmpx {
	char	ut_user[32];		/* user login name */
	char	ut_id[4];		/* inittab id */
	char	ut_line[32];		/* device name (console, lnxx) */
	pid32_t	ut_pid;			/* process id */
	int16_t ut_type;		/* type of entry */
	struct {
		int16_t	e_termination;	/* process termination status */
		int16_t	e_exit;		/* process exit status */
	} ut_exit;			/* exit status of a process */
	struct timeval32 ut_tv;		/* time entry was made */
	int32_t	ut_session;		/* session ID, user for windowing */
	int32_t	pad[5];			/* reserved for future use */
	int16_t ut_syslen;		/* significant length of ut_host */
	char	ut_host[257];		/* remote host name */
};

#define	MOD_WIN		10

/*	Define and macro for determing if a normal user wrote the entry */
/*	and marking the utmpx entry as a normal user */
#define	NONROOT_USRX	2
#define	nonuserx(utx)	((utx).ut_exit.e_exit == NONROOT_USRX ? 1 : 0)
#define	setuserx(utx)	((utx).ut_exit.e_exit = NONROOT_USRX)

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if defined(__STDC__)

extern void endutxent(void);
extern struct utmpx *getutxent(void);
extern struct utmpx *getutxid(const struct utmpx *);
extern struct utmpx *getutxline(const struct utmpx *);
extern struct utmpx *pututxline(const struct utmpx *);
extern void setutxent(void);

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int utmpxname(const char *);
extern struct utmpx *makeutx(const struct utmpx *);
extern struct utmpx *modutx(const struct utmpx *);
extern void getutmp(const struct utmpx *, struct utmp *);
extern void getutmpx(const struct utmp *, struct utmpx *);
extern void updwtmp(const char *, struct utmp *);
extern void updwtmpx(const char *, struct utmpx *);
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#else /* __STDC__ */

extern void endutxent();
extern struct utmpx *getutxent();
extern struct utmpx *getutxid();
extern struct utmpx *getutxline();
extern struct utmpx *pututxline();
extern void setutxent();

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
extern int utmpxname();
extern struct utmpx *makeutx();
extern struct utmpx *modutx();
extern void getutmp();
extern void getutmpx();
extern void updwtmp();
extern void updwtmpx();
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _UTMPX_H */
