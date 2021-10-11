/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*LINTLIBRARY*/
#ident	"@(#)nblk.c	1.3	93/03/31"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>

/*
 * This should not be a constant, but for ufs it is 12, not 10 like for s5.
 */
#define	DIRECT 12	/* Number of logical blocks before indirection */

long
nblk(long size, ulong bsize, ulong frsize)
{
	long	tot, count, count1, d_indirect, t_indirect, ind;
	long	frags = 0;

	if (size == 0)
		return (1);

	/*
	 * Need to keep track of indirect blocks.
	 */

	ind = howmany(bsize, sizeof (daddr_t));
	d_indirect = ind + DIRECT; 			/* double indirection */
	t_indirect = ind * (ind + 1) + DIRECT; 		/* triple indirection */

	tot = howmany(size, bsize);

	if (tot > t_indirect) {
		count1 = (tot - ind * ind - (DIRECT + 1)) / ind;
		count = count1 + count1 / ind + ind + 3;
	} else if (tot > d_indirect) {
		count = (tot - (DIRECT + 1)) / ind + 2;
	} else if (tot > DIRECT) {
		count = 1;
	} else {
		count = 0;
		frags = ((long) frsize > 0) ?
		    roundup(size, frsize) :
		    roundup(size, bsize);
	}

	/* Accounting for the indirect blocks, the total becomes */
	tot += count;

	/*
	 * calculate number of 512 byte blocks, for frag or full block cases.
	 */
	if (!frags)
		tot *= howmany(bsize, DEV_BSIZE);
	else
		tot = howmany(frags, DEV_BSIZE);
	return (tot);
}
