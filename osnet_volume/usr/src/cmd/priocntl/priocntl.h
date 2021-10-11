/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)priocntl.h	1.7	92/07/14 SMI"	/* SVr4.0 1.3	*/

#define	NPIDS	1024	/* max number of pids we pipe to class specific cmd */
#define	NIDS	1024	/* max number of id arguments we handle */

#define	BASENMSZ	16
#define	CSOPTSLN	128	/* max length of class specific opts string */

/*
 * The command string for the sub-command must be big enough for the
 * path, the class specific options, and plenty of space for arguments.
 */
#define	SUBCMDSZ	512

extern void	fatalerr(), itoa();
extern int	str2idtyp(), idtyp2str(), idtyp2maxprocs(), idcompar();
extern int	getmyid(), getmyidstr();
extern id_t	clname2cid();
extern char	*basename();

/*
 * The following is an excerpt from <sys/hrtcntl.h>. HRT timers are not
 * supported by SunOS (which will support the POSIX definition). Priocntl
 * uses the hrt routine _hrtnewres because it coincidentally does the
 * right thing. These defines allow this routine to be locally included
 * in priocntl (rather than exported in libc). This should be improved in
 * the long term.
 */

#define HRT_TRUNC	0	/* Round results down.	*/
#define HRT_RND		1	/* Round results (rnd up if fractional	*/
				/*   part >= .5 otherwise round down).	*/
#define	HRT_RNDUP	2	/* Always round results up.	*/

/*
 *	Structure used to represent a high-resolution time-of-day
 *	or interval.
 */

typedef struct hrtimer {
	ulong	hrt_secs;	/* Seconds.				*/
	long	hrt_rem;	/* A value less than a second.		*/
	ulong	hrt_res;	/* The resolution of hrt_rem.		*/
} hrtimer_t;
