/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)qsort.c	1.14	98/08/13 SMI"	/* SVr4.0 1.9	*/

/*LINTLIBRARY*/

/*
 * qsort.c:
 * Our own version of the system qsort routine which is faster by an average
 * of 25%, with lows and highs of 10% and 50%.
 * The THRESHold below is the insertion sort threshold, and has been adjusted
 * for records of size 48 bytes.
 * The MTHREShold is where we stop finding a better median.
 */

#include "synonyms.h"
#include <mtlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <synch.h>

/*
 * These are knobs for tuning insertion sort.
 */
#define		THRESH		14		/* threshold for insertion */
#define		MTHRESH		20		/* threshold for median */

static void qst(char *, char *, size_t, int (*)(const void *, const void *),
	size_t, size_t);

/*
 * qsort:
 * First, set up some global parameters for qst to share.  Then, quicksort
 * with qst(), and then a cleanup insertion sort ourselves.  Sound simple?
 * It's not...
 */
void
qsort(void *base, size_t n, size_t qsz, int (*qcmp)(const void *, const void *))
{
	char c, *i, *j, *lo, *hi;
	char *min, *max;
	size_t thresh;		/* THRESHold in chars */
	size_t mthresh;		/* MTHRESHold in chars */

	if (n < 2)
		return;

	thresh = qsz * THRESH;
	mthresh = qsz * MTHRESH;
	max = (char *)base + n * qsz;

	if (n >= THRESH) {
		qst(base, max, qsz, qcmp, thresh, mthresh);
		hi = (char *)base + thresh;
	} else {
		hi = max;
	}
	/*
	 * First put smallest element, which must be in the first THRESH, in
	 * the first position as a sentinel.  This is done just by searching
	 * the first THRESH elements (or the first n if n < THRESH), finding
	 * the min, and swapping it into the first position.
	 */
	for (j = lo = base; (lo += qsz) < hi; /* CSTYLED */)
		if (qcmp(j, lo) > 0)
			j = lo;
	if (j != base) {
		/* swap j into place */
		for (i = (char *)base, hi = (char *)base + qsz;
			i < hi; /* CSTYLED */) {
			c = *j;
			*j++ = *i;
			*i++ = c;
		}
	}
	/*
	 * With our sentinel in place, we now run the following hyper-fast
	 * insertion sort.  For each remaining element, min, from [1] to [n-1],
	 * set hi to the index of the element AFTER which this one goes.
	 * Then, do the standard insertion sort shift on a character at a time
	 * basis for each element in the array.
	 */
	for (min = base; (hi = min += qsz) < max; /* CSTYLED */) {
		while (qcmp(hi -= qsz, min) > 0)
			/* empty loop */;
		if ((hi += qsz) != min) {
			for (lo = min + qsz; --lo >= min; /* CSTYLED */) {
				c = *lo;
				for (i = j = lo; (j -= qsz) >= hi; i = j)
					*i = *j;
				*i = c;
			}
		}
	}
}

/*
 * qst:
 * Do a quicksort
 * First, find the median element, and put that one in the first place as the
 * discriminator.  (This "median" is just the median of the first, last and
 * middle elements).  (Using this median instead of the first element is a big
 * win).  Then, the usual partitioning/swapping, followed by moving the
 * discriminator into the right place.  Then, figure out the sizes of the two
 * partions, do the smaller one recursively and the larger one via a repeat of
 * this code.  Stopping when there are less than THRESH elements in a partition
 * and cleaning up with an insertion sort (in our caller) is a huge win.
 * All data swaps are done in-line, which is space-losing but time-saving.
 * (And there are only three places where this is done).
 */


static void
qst(char *base, char *max, size_t qsz, int (*qcmp)(const void *, const void *),
	size_t thresh, size_t mthresh)
{
	char c, *i, *j, *jj;
	size_t ii;
	char *mid, *tmp;
	size_t lo, hi;
	/*
	 * Note: (LP64 work) offset-aligned copy/swap could be done with the
	 * size of 8-byte structure alignment, but we don't want to penalize the
	 * case with structure size in multiple of 4 bytes, not 8 bytes.
	 * Future enhancement for 64-bit version: create aligned64 as the third
	 * case:
	 * int aligned64 = ((uintptr_t)base & (sizeof (long) - 1)) == 0 &&
	 *		((qsz & (sizeof (long) - 1)) == 0);
	 */
	int aligned = ((uintptr_t)base & (sizeof (int) - 1)) == 0 &&
				((qsz & (sizeof (int) - 1)) == 0);

	/*
	 * these are all of the variables that are used before set
	 * after a recursive call returns
	 */
	struct _stack {
		char *base, *max;
		size_t lo, hi;
		char *i, *j;
	} stack[64], *sp;
	int stack_resume = 0;
	int stackp = 0;
	long loops;
	long cnt;

	if (aligned)
		loops = qsz / sizeof (int); /* word copies... so 4 */

top:
	/*
	 * At the top here, lo is the number of characters of elements in the
	 * current partition.  (Which should be max - base).
	 * Find the median of the first, last, and middle element and make
	 * that the middle element.  Set j to largest of first and middle.
	 * If max is larger than that guy, then it's that guy, else compare
	 * max with loser of first and take larger.  Things are set up to
	 * prefer the middle, then the first in case of ties.
	 */
	lo = max - base;	/* number of elements as chars */
	do	{
		mid = i = base + qsz * ((lo / qsz) >> 1);
		if (lo >= mthresh) {
			j = (qcmp((jj = base), i) > 0 ? jj : i);
			if (qcmp(j, (tmp = max - qsz)) > 0) {
				/* switch to first loser */
				j = (j == jj ? i : jj);
				if (qcmp(j, tmp) < 0)
					j = tmp;
			}
			if (j != i) {
				if (aligned) {
					int temp;
					int * t1 = (int *)i, * t2 = (int *)j;
					cnt = loops;

					while (cnt--) {
						temp = *t1;
						*t1++ = *t2;
						*t2++ = temp;
					}
				} else {	/* character by character */
					ii = qsz;
					do	{
						c = *i;
						*i++ = *j;
						*j++ = c;
					} while (--ii);
				}
			}
		}
		/*
		 * Semi-standard quicksort partitioning/swapping
		 */
		for (i = base, j = max - qsz; /* empty */; /* empty */) {
			while (i < mid && qcmp(i, mid) <= 0)
				i += qsz;

			while (j > mid) {
				if (qcmp(mid, j) <= 0) {
					j -= qsz;
					continue;
				}
				tmp = i + qsz;	/* value of i after swap */
				if (i == mid) {
					/* j <-> mid, new mid is j */
					mid = jj = j;
				} else {
					/* i <-> j */
					jj = j;
					j -= qsz;
				}
				goto swap;
			}
			if (i == mid) {
				break;
			} else {
				/* i <-> mid, new mid is i */
				jj = mid;
				tmp = mid = i;	/* value of i after swap */
				j -= qsz;
			}
		swap:
			if (aligned) {
				int temp;
				int * t1 = (int *)i, * t2 = (int *)jj;
				cnt = loops;

				while (cnt--) {
					temp = *t1;
					*t1++ = *t2;
					*t2++ = temp;
				}
			} else {	/* character by character */
				ii = qsz;
				do	{
					c = *i;
					*i++ = *jj;
					*jj++ = c;
				} while (--ii);
			}
			i = tmp;
		}
		/*
		 * Look at sizes of the two partitions, do the smaller
		 * one first by recursion, then do the larger one by
		 * making sure lo is its size, base and max are update
		 * correctly, and branching back.  But only repeat
		 * (recursively or by branching) if the partition is
		 * of at least size THRESH.
		 */
		i = (j = mid) + qsz;
		if ((lo = j - base) <= (hi = max - i)) {
			if (lo >= thresh) {
				sp = &stack[stackp];
				sp->base = base;
				sp->max = max;
				sp->lo = lo;
				sp->hi = hi;
				sp->i = i;
				sp->j = j;
				stack_resume &= ~(1 << stackp);
				stackp++;
				max = j;
				goto top;
			}
		resume0:
			base = i;
			lo = hi;
		} else {
			if (hi >= thresh) {
				sp = &stack[stackp];
				sp->base = base;
				sp->max = max;
				sp->lo = lo;
				sp->hi = hi;
				sp->i = i;
				sp->j = j;
				stack_resume |= (1 << stackp);
				stackp++;
				base = i;
				goto top;
			}
		resume1:
			max = j;
		}
	} while (lo >= thresh);

	if (stackp) {
		stackp--;
		sp = &stack[stackp];
		base = sp->base;
		max = sp->max;
		lo = sp->lo;
		hi = sp->hi;
		i = sp->i;
		j = sp->j;
		if (stack_resume & (1 << stackp))
			goto resume1;
		else
			goto resume0;
	}
}
