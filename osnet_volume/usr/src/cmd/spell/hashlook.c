/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)hashlook.c	1.13	98/07/14 SMI"	/* SVr4.0 1.8	*/

#include <stdlib.h>
#include <stdio.h>
#include "hash.h"
#include "huff.h"

unsigned *table;
int hindex[NI];

#define	B (BYTE * sizeof (unsigned))
#define	L (BYTE * sizeof (long)-1)
#define	MASK (~((unsigned long)1L<<L))

#ifdef pdp11	/* sizeof (unsigned)==sizeof(long)/2 */
#define	fetch(wp, bp)\
	(((((long)wp[0]<<B)|wp[1])<<(B-bp))|(wp[2]>>bp))
#else 		/* sizeof (unsigned)==sizeof(long) */
#define	fetch(wp, bp) ((wp[0] << (B - bp)) | (wp[1] >> bp))
#endif

int
hashlook(char *s)
{
	unsigned long h;
	unsigned long t;
	register bp;
	register unsigned *wp;
	long sum;
	unsigned *tp;

	h = hash(s);
	t = h>>(HASHWIDTH-INDEXWIDTH);
	wp = &table[hindex[t]];
	tp = &table[hindex[t+1]];
	bp = B;
	sum = (long)t<<(HASHWIDTH-INDEXWIDTH);
	for (;;) {
		{
			/*
			 * this block is equivalent to:
			 * bp -= decode((fetch(wp, bp) >> 1) & MASK, &t);
			 */
			long y;
			long v;

			/*
			 * shift 32 on those machines leaves destination
			 * unchanged
			 */
			if (bp == 0)
				y = 0;
			else
				y = wp[0] << (B - bp);
			if (bp < 32)
				y |= (wp[1] >> bp);
			y = (y >> 1) & MASK;
			if (y < cs) {
				t = y >> (long) (L+1-w);
				bp -= w-1;
			} else {
				for (bp -= w, v = v0; y >= qcs;
				    y = (y << 1) & MASK, v += n)
					bp -= 1;
				t = v + (y>> (long)(L-w));
			}
		}
		while (bp <= 0) {
			bp += B;
			wp++;
		}
		if (wp >= tp && (wp > tp||bp < B))
			return (0);
		sum += t;
		if (sum < h)
			continue;
		return (sum == h);
	}
}


int
prime(char *file)
{
	register FILE *f;

#ifdef pdp11	/* because of insufficient address space for buffers */
	fd = dup(0);
	close(0);
	if (open(file, 0) != 0)
		return (0);
	f = stdin;
	if (rhuff(f) == 0 || read(fileno(f), (char *)hindex,
	    NI * sizeof (*hindex)) != NI * sizeof (*hindex) ||
	    (table = (unsigned *)malloc(hindex[NI-1] * sizeof (*table))) == 0 ||
	    read(fileno(f), (char *)table, sizeof (*table) * hindex[NI-1]) !=
	    hindex[NI-1] * sizeof (*table))
		return (0);
	close(0);
	if (dup(fd) != 0)
		return (0);
	close(fd);
#else
	if ((f = fopen(file, "r")) == NULL)
		return (0);
	if (rhuff(f) == 0 ||
	    fread((char *)hindex, sizeof (*hindex),  NI, f) != NI ||
	    (table = (unsigned *)malloc(hindex[NI-1] * sizeof (*table))) == 0 ||
	    fread((char *)table, sizeof (*table), hindex[NI-1], f) !=
	    hindex[NI-1])
		return (0);
	(void) fclose(f);
#endif
	hashinit();
	return (1);
}
