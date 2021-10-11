/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */
/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vm_meter.c	1.32	98/08/12 SMI"
/*	From:	SVr4.0	"kernel:os/vm_meter.c	1.15"		*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/cpuvar.h>
#include <sys/sysinfo.h>

/*
 * This define represents the number of
 * useful pages transferred per paging i/o operation, under the assumption
 * that half of the total number is actually useful.  However, if there's
 * only one page transferred per operation, we assume that it's useful.
 */

#ifdef	lint
#define	UsefulPagesPerIO	1
#else	/* lint */
#define	UsefulPagesPerIO	nz((MAXBSIZE/PAGESIZE)/2)
#endif	/* lint */

extern int dopageout;
extern int nrunnable;

int	avenrun[3];		/* FSCALED average run queue lengths */

/*
 * high-precision avenrun values.  These are needed to make the regular
 * avenrun values accurate.
 */
static int	hp_avenrun[3];

/* Average new into old with aging factor time */
#define	ave(smooth, cnt, time) \
	smooth = ((time - 1) * (smooth) + (cnt)) / (time)

/*
 * pagein and pageout rates for use by swapper.
 */
ulong pginrate;		/* 5 second average	*/
ulong pgoutrate;	/* 5 second average	*/

static ulong ogpagein;	/* pagein rate a sec ago */
static ulong ogpageout;	/* pageout rate a sec ago */

/*
 * Called once a second to gather statistics.
 */
void
vmmeter(int nrunnable)
{
	static int f[3] = { 1083, 218, 73 };
	int i, q, r;
	cpu_t *cp;
	u_long gpagein, gpageout;

	/*
	 * Compute load average over the last 1, 5, and 15 minutes
	 * (60, 300, and 900 seconds).  The constants in f[3] are for
	 * exponential decay:
	 * (1 - exp(-1/60)) << 16 = 1083,
	 * (1 - exp(-1/300)) << 16 = 218,
	 * (1 - exp(-1/900)) << 16 = 73.
	 */

	/*
	 * a little hoop-jumping to avoid integer overflow
	 */
	for (i = 0; i < 3; i++) {
		q = hp_avenrun[i] >> 16;
		r = hp_avenrun[i] & 0xffff;
		hp_avenrun[i] += (nrunnable - q) * f[i] - ((r * f[i]) >> 16);
		avenrun[i] = hp_avenrun[i] >> (16 - FSHIFT);
	}

	/*
	 * Compute 5 sec and 30 sec average free memory values.
	 */
	ave(avefree, freemem, 5);
	ave(avefree30, freemem, 30);

	/*
	 * Compute the 5 secs average of pageins and pageouts.
	 */
	gpagein = gpageout = 0;

	cp = cpu_list;
	do {
		gpagein += cp->cpu_stat.cpu_vminfo.pgin;
		gpageout += cp->cpu_stat.cpu_vminfo.pgout;
	} while ((cp = cp->cpu_next) != cpu_list);

	if ((gpagein >= ogpagein) && (gpageout >= ogpageout)) {
		ave(pginrate, gpagein - ogpagein, 5);
		ave(pgoutrate, gpageout - ogpageout, 5);
	}

	/*
	 * Save the current pagein/pageout values.
	 */
	ogpagein = gpagein;
	ogpageout = gpageout;

	if (!lotsfree || !dopageout)
		return;

	/*
	 * Decay deficit by the expected number of pages brought in since
	 * the last call (i.e., in the last second).  The calculation
	 * assumes that one half of the pages brought in are actually
	 * useful (see comment above), and that half of the overall
	 * paging i/o activity is pageins as opposed to pageouts (the
	 * trailing factor of 2)  It also assumes that paging i/o is done
	 * in units of MAXBSIZE bytes, which is a dubious assumption to
	 * apply to all file system types.
	 */
	deficit -= MIN(deficit,
	    MAX(deficit / 10, UsefulPagesPerIO * maxpgio / 2));
}
