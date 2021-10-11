/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)subr.c	1.8	96/06/17 SMI"	/* SVr4.0 1.5	*/
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<limits.h>
#include	<sys/time.h>
#include	<sys/procset.h>
#include	<sys/priocntl.h>

#include "priocntl.h"

/*
 * Utility functions for priocntl command.
 */

/* VARARGS1 */
void
fatalerr(format, a1, a2, a3, a4, a5)
char	*format;
{
	fprintf(stderr, format, a1, a2, a3, a4, a5);
	exit(1);
}


/*
 * Structure defining idtypes known to the priocntl command
 * along with the corresponding names and a liberal guess
 * of the max number of procs sharing any given ID of that type.
 * The idtype values themselves are defined in <sys/procset.h>.
 */
static struct idtypes {
	idtype_t	idtype;
	char		*idtypnm;
	int		maxprocsperid;
} idtypes [] = {
	{ P_PID,	"pid",		1 },
	{ P_PPID,	"ppid",		200 },
	{ P_PGID,	"pgid",		500 },
	{ P_SID,	"sid",		1024 },
	{ P_CID,	"class",	1024 },
	{ P_UID,	"uid",		1024 },
	{ P_GID,	"gid",		1024 },
	{ P_ALL,	"all",		1024 }
};

#define	IDCNT	(sizeof(idtypes)/sizeof(struct idtypes))


int
str2idtyp(idtypnm, idtypep)
char		*idtypnm;
idtype_t	*idtypep;
{
	register struct idtypes	*curp;
	register struct idtypes	*endp;

	for (curp = idtypes, endp = &idtypes[IDCNT]; curp < endp; curp++) {
		if (strcmp(curp->idtypnm, idtypnm) == 0) {
			*idtypep = curp->idtype;
			return(0);
		}
	}
	return(-1);
}


int
idtyp2str(idtype, idtypnm)
idtype_t	idtype;
char		*idtypnm;
{
	register struct idtypes	*curp;
	register struct idtypes	*endp;

	for (curp = idtypes, endp = &idtypes[IDCNT]; curp < endp; curp++) {
		if (idtype == curp->idtype) {
			strcpy(idtypnm, curp->idtypnm);
			return(0);
		}
	}
	return(-1);
}


/*
 * Given an idtype, return a very liberal guess of the max number of
 * processes sharing any given ID of that type.
 */
int
idtyp2maxprocs(idtype)
idtype_t	idtype;
{
	register struct idtypes	*curp;
	register struct idtypes	*endp;

	for (curp = idtypes, endp = &idtypes[IDCNT]; curp < endp; curp++) {
		if (idtype == curp->idtype)
			return(curp->maxprocsperid);
	}
	return(-1);
}

	
/*
 * Compare two IDs for equality.
 */
int
idcompar(id1p, id2p)
id_t	*id1p;
id_t	*id2p;
{
	if (*id1p == *id2p)
		return(0);
	else
		return(-1);
}


id_t
clname2cid(clname)
char	*clname;
{
	pcinfo_t	pcinfo;

	strcpy(pcinfo.pc_clname, clname);
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		return((id_t)-1);
	return(pcinfo.pc_cid);
}


int
getmyid(idtype, idptr)
idtype_t	idtype;
id_t		*idptr;
{
	pcparms_t	pcparms;

	switch(idtype) {

	case P_PID:
		*idptr = (id_t)getpid();
		break;

	case P_PPID:
		*idptr = (id_t)getppid();
		break;

	case P_PGID:
		*idptr = (id_t)getpgrp();
		break;

	case P_SID:
		*idptr = (id_t)getsid(getpid());
		break;

	case P_CID:
		pcparms.pc_cid = PC_CLNULL;
		if (priocntl(P_PID, P_MYID, PC_GETPARMS,
		    (caddr_t)&pcparms) == -1)
			return(-1);
		*idptr = pcparms.pc_cid;
		break;

	case P_UID:
		*idptr = (id_t)getuid();
		break;

	case P_GID:
		*idptr = (id_t)getgid();
		break;

	default:
		return(-1);
	}
	return(0);
}


int
getmyidstr(idtype, idstr)
idtype_t	idtype;
char		*idstr;
{
	pcparms_t	pcparms;
	pcinfo_t	pcinfo;

	switch(idtype) {

	case P_PID:
		itoa((long)getpid(), idstr);
		break;

	case P_PPID:
		itoa((long)getppid(), idstr);
		break;

	case P_PGID:
		itoa((long)getpgrp(), idstr);
		break;
	case P_SID:
		itoa((long)getsid(getpid()), idstr);
		break;

	case P_CID:
		if (priocntl(P_PID, P_MYID, PC_GETPARMS,
		    (caddr_t)&pcparms) == -1)
			return(-1);
		pcinfo.pc_cid = pcparms.pc_cid;
		if (priocntl(0, 0, PC_GETCLINFO, (caddr_t)&pcinfo) == -1)
			return(-1);
		strcpy(idstr, pcinfo.pc_clname);
		break;

	case P_UID:
		itoa((long)getuid(), idstr);
		break;

	case P_GID:
		itoa((long)getgid(), idstr);
		break;

	default:
		return(-1);
	}
	return(0);
}


/*
 * itoa() and reverse() taken almost verbatim from K & R Chapter 3.
 */
static void	reverse();

/*
 * itoa(): Convert n to characters in s.
 */
void
itoa(n, s)
long	n;
char	*s;
{
	long	i, sign;

	if ((sign = n) < 0)	/* record sign */
		n = -n;		/* make sign positive */
	i = 0;
	do {	/* generate digits in reverse order */
		s[i++] = n % 10 + '0';	/* get next digit */
	} while ((n /= 10) > 0);	/* delete it */
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}


/*
 * reverse(): Reverse string s in place.
 */
static void
reverse(s)
char	*s;
{
	int	c, i, j;

	for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = (char)c;
	}
}


/*
 * The following routine was removed from libc (libc/port/gen/hrtnewres.c).
 * It has also been added to disadmin, so if you fix it here, you should
 * also probably fix it there. In the long term, this should be recoded to
 * not be hrt'ish.
 */

/*	Convert interval expressed in htp->hrt_res to new_res.
**
**	Calculate: (interval * new_res) / htp->hrt_res  rounding off as
**		specified by round.
**
**	Note:	All args are assumed to be positive.  If
**	the last divide results in something bigger than
**	a long, then -1 is returned instead.
*/

_hrtnewres(htp, new_res, round)
register hrtimer_t *htp;
register ulong new_res;
long round;
{
	register long  interval;
	longlong_t	dint;
	longlong_t	dto_res;
	longlong_t	drem;
	longlong_t	dfrom_res;
	longlong_t	prod;
	longlong_t	quot;
	register long	numerator;
	register long	result;
	ulong		modulus;
	ulong		twomodulus;
	long		temp;

	if (htp->hrt_res <= 0 || new_res <= 0 ||
			new_res > NANOSEC || htp->hrt_rem < 0)
		return(-1);

	if (htp->hrt_rem >= htp->hrt_res) {
		htp->hrt_secs += htp->hrt_rem / htp->hrt_res;
		htp->hrt_rem = htp->hrt_rem % htp->hrt_res;
	}

	interval = htp->hrt_rem;
	if (interval == 0) {
		htp->hrt_res = new_res;
		return(0);
	}

	/*	Try to do the calculations in single precision first
	**	(for speed).  If they overflow, use double precision.
	**	What we want to compute is:
	**
	**		(interval * new_res) / hrt->hrt_res
	*/

	numerator = interval * new_res;

	if (numerator / new_res  ==  interval) {

		/*	The above multiply didn't give overflow since
		**	the division got back the original number.  Go
		**	ahead and compute the result.
		*/

		result = numerator / htp->hrt_res;

		/*	For HRT_RND, compute the value of:
		**
		**		(interval * new_res) % htp->hrt_res
		**
		**	If it is greater than half of the htp->hrt_res,
		**	then rounding increases the result by 1.
		**
		**	For HRT_RNDUP, we increase the result by 1 if:
		**
		**		result * htp->hrt_res != numerator
		**
		**	because this tells us we truncated when calculating
		**	result above.
		**
		**	We also check for overflow when incrementing result
		**	although this is extremely rare.
		*/

		if (round == HRT_RND) {
			modulus = numerator - result * htp->hrt_res;
			if ((twomodulus = 2 * modulus) / 2 == modulus) {

				/*
				 * No overflow (if we overflow in calculation
				 * of twomodulus we fall through and use
				 * double precision).
				 */
				if (twomodulus >= htp->hrt_res) {
					temp = result + 1;
					if (temp - 1 == result)
						result++;
					else
						return(-1);
				}
				htp->hrt_res = new_res;
				htp->hrt_rem = result;
				return(0);
			}
		} else if (round == HRT_RNDUP) {
			if (result * htp->hrt_res != numerator) {
				temp = result + 1;
				if (temp - 1 == result)
					result++;
				else
					return(-1);
			}
			htp->hrt_res = new_res;
			htp->hrt_rem = result;
			return(0);
		} else {	/* round == HRT_TRUNC */
			htp->hrt_res = new_res;
			htp->hrt_rem = result;
			return(0);
		}
	}

	/*	We would get overflow doing the calculation is
	**	single precision so do it the slow but careful way.
	**
	**	Compute the interval times the resolution we are
	**	going to.
	*/

	dint = interval;
	dto_res = new_res;
	prod = dint * dto_res;

	/*	For HRT_RND the result will be equal to:
	**
	**		((interval * new_res) + htp->hrt_res / 2) / htp->hrt_res
	**
	**	and for HRT_RNDUP we use:
	**
	**		((interval * new_res) + htp->hrt_res - 1) / htp->hrt_res
	**
	** 	This is a different but equivalent way of rounding.
	*/

	if (round == HRT_RND) {
		drem = htp->hrt_res / 2;
		prod = prod + drem;
	} else if (round == HRT_RNDUP) {
		drem = htp->hrt_res - 1;
		prod = prod + drem;
	}

	dfrom_res = htp->hrt_res;
	quot = prod / dfrom_res;

	/*	If the quotient won't fit in a long, then we have
	**	overflow.  Otherwise, return the result.
	*/

	if (quot > UINT_MAX) {
		return(-1);
	} else {
		htp->hrt_res = new_res;
		htp->hrt_rem = (int) quot;
		return(0);
	}
}
