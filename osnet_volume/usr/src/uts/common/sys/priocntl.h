/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PRIOCNTL_H
#define	_SYS_PRIOCNTL_H

#pragma ident	"@(#)priocntl.h	1.14	98/02/24 SMI"	/* from SVR4 1.6 */

#include <sys/types.h>
#include <sys/procset.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	PC_VERSION	1	/* First version of priocntl */

#define	priocntl(idtype, id, cmd, arg)\
	__priocntl(PC_VERSION, idtype, id, cmd, arg)

#define	priocntlset(psp, cmd, arg)\
	__priocntlset(PC_VERSION, psp, cmd, arg)

#ifdef __STDC__
extern long	__priocntl(int, idtype_t, id_t, int, caddr_t);
extern long	__priocntlset(int, procset_t *, int, caddr_t);
#else
extern long	__priocntl(), __priocntlset();
#endif	/* __STDC__ */

/*
 * The following are the possible values of the command
 * argument for the priocntl system call.
 */

#define	PC_GETCID	0	/* Get class ID */
#define	PC_GETCLINFO	1	/* Get info about a configured class */
#define	PC_SETPARMS	2	/* Set scheduling parameters */
#define	PC_GETPARMS	3	/* Get scheduling parameters */
#define	PC_ADMIN	4	/* Scheduler administration (used by */
				/*   dispadmin(1M), not for general use) */
#define	PC_GETPRIRANGE	5	/* Get global priority range for a class */
				/* posix.4 scheduling, not for general use */

#define	PC_CLNULL	-1

#define	PC_CLNMSZ	16
#define	PC_CLINFOSZ	(32 / sizeof (int))
#define	PC_CLPARMSZ	(32 / sizeof (int))

typedef struct pcinfo {
	id_t	pc_cid;			/* class id */
	char	pc_clname[PC_CLNMSZ];	/* class name */
	int	pc_clinfo[PC_CLINFOSZ];	/* class information */
} pcinfo_t;

typedef struct pcparms {
	id_t	pc_cid;			    /* process class */
	int	pc_clparms[PC_CLPARMSZ];    /* class specific parameters */
} pcparms_t;

/*
 * The following is used by the library librt for
 * posix.4 scheduler interfaces and is not for general use.
 */

typedef struct pcpri {
	id_t	pc_cid;			/* process class */
	pri_t	pc_clpmax;		/* class global priority max */
	pri_t	pc_clpmin;		/* class global priority min */
} pcpri_t;

/*
 * The following is used by the dispadmin(1M) command for
 * scheduler administration and is not for general use.
 */

#ifdef _SYSCALL32
/* Data structure for ILP32 clients */
typedef struct pcadmin32 {
	id32_t		pc_cid;
	caddr32_t	pc_cladmin;
} pcadmin32_t;
#endif	/* _SYSCALL32 */

typedef struct pcadmin {
	id_t	pc_cid;
	caddr_t	pc_cladmin;
} pcadmin_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PRIOCNTL_H */
