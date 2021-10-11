/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bitmap.c	1.17	98/06/12 SMI"

/*
 * Operations on bitmaps of arbitrary size
 * A bitmap is a vector of 1 or more ulongs.
 * The user of the package is responsible for range checks and keeping
 * track of sizes.
 */

#include <sys/types.h>
#include <sys/bitmap.h>
#include <sys/debug.h>		/* ASSERT */

/*
 * Return index of first available bit in denoted bitmap, or -1 for
 * failure.  Size is the cardinality of the bitmap; that is, the
 * number of bits.
 * No side-effects.  In particular, does not update bitmap.
 * Caller is responsible for range checks.
 */
index_t
bt_availbit(ulong *bitmap, size_t nbits)
{
	index_t	maxword;	/* index of last in map */
	index_t	wx;		/* word index in map */

	/*
	 * Look for a word with a bit off.
	 * Subtract one from nbits because we're converting it to a
	 * a range of indices.
	 */
	nbits -= 1;
	maxword = nbits >> BT_ULSHIFT;
	for (wx = 0; wx <= maxword; wx++)
		if (bitmap[wx] != ~0)
			break;

	if (wx <= maxword) {
		/*
		 * Found a word with a bit off.  Now find the bit in the word.
		 */
		index_t	bx;	/* bit index in word */
		index_t	maxbit; /* last bit to look at */
		ulong		word;
		ulong		bit;

		maxbit = wx == maxword ? nbits & BT_ULMASK : BT_NBIPUL - 1;
		word = bitmap[wx];
		bit = 1;
		for (bx = 0; bx <= maxbit; bx++, bit <<= 1) {
			if (!(word & bit)) {
				return (wx << BT_ULSHIFT | bx);
			}
		}
	}
	return (-1);
}


/*
 * Find highest order bit that is on, and is within or below
 * the word specified by wx.
 */
int
bt_gethighbit(ulong *mapp, int wx)
{
	ulong word;

	while ((word = mapp[wx]) == 0) {
		wx--;
		if (wx < 0) {
			return (-1);
		}
	}
	return (wx << BT_ULSHIFT | (highbit(word) - 1));
}


/*
 * Search the bitmap for a consecutive pattern of 1's.
 * Search starts at position pos1.
 * Returns 1 on success and 0 on failure.
 * Side effects.
 * Returns indices to the first bit (pos1)
 * and the last bit (pos2) in the pattern.
 */
int
bt_range(ulong *bitmap, size_t *pos1, size_t *pos2, size_t nbits)
{
	size_t inx;

	for (inx = *pos1; inx < nbits; inx++)
		if (BT_TEST(bitmap, inx))
			break;

	if (inx == nbits)
		return (0);

	*pos1 = inx;

	for (; inx < nbits; inx++)
		if (!BT_TEST(bitmap, inx))
			break;

	*pos2 = inx - 1;

	return (1);
}

/*
 * Find highest one bit set.
 *	Returns bit number + 1 of highest bit that is set, otherwise returns 0.
 * High order bit is 31 (or 63 in _LP64 kernel).
 */
int
highbit(ulong i)
{
	register int h = 1;

	if (i == 0)
		return (0);
#ifdef _LP64
	if (i & 0xffffffff00000000ul) {
		h += 32; i >>= 32;
	}
#endif
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
}

/*
 * Find lowest one bit set.
 *	Returns bit number + 1 of lowest bit that is set, otherwise returns 0.
 * Low order bit is 0.
 */
int
lowbit(ulong i)
{
	register int h = 1;

	if (i == 0)
		return (0);

#ifdef _LP64
	if (!(i & 0xffffffff)) {
		h += 32; i >>= 32;
	}
#endif
	if (!(i & 0xffff)) {
		h += 16; i >>= 16;
	}
	if (!(i & 0xff)) {
		h += 8; i >>= 8;
	}
	if (!(i & 0xf)) {
		h += 4; i >>= 4;
	}
	if (!(i & 0x3)) {
		h += 2; i >>= 2;
	}
	if (!(i & 0x1)) {
		h += 1;
	}
	return (h);
}


/*
 * get the lowest bit in the range of 'start' and 'stop', inclusive.
 * I.e., if caller calls bt_getlowbit(map, X, Y), any value between X and Y,
 * including X and Y can be returned.
 * Neither start nor stop is required to align with word boundaries.
 * If a bit is set in the range, the bit position is returned; otherwise,
 * a -1 is returned.
 */
int
bt_getlowbit(ulong* map, size_t start, size_t stop)
{
	ulong		word;
	int		wx = start >> BT_ULSHIFT;
	index_t		startbits = start & BT_ULMASK;
	int		limit;
	index_t		limitbits;

	if (start > stop) {
		return (-1);
	}

	/*
	 * The range between 'start' and 'stop' can be very large, and the
	 * '1' bits in this range can be sparse.
	 * For performance reason, the underlying implementation operates
	 * on words, not on bits.
	 */
	word = map[wx];
	if (startbits) {
		/*
		 * Since the start is not aligned on word boundary, we
		 * need to patch the unwanted low order bits with 0's before
		 * operating on the first bitmap word.
		 */
		word = word & (BT_ULMAXMASK << startbits);
	}

	/*
	 * the end of range has similar problems. If the last word is not
	 * aligned, delay examing the last word.
	 */
	if ((limitbits = stop & BT_ULMASK) == BT_ULMASK) {
		limit = stop >> BT_ULSHIFT;
	} else {
		limit = (stop >> BT_ULSHIFT) - 1;
	}

	while ((word == 0) && (wx <= limit)) {
		word = map[++wx];
	}
	if (wx > limit) {
		if (limitbits == BT_ULMASK) {
			return (-1);
		} else {
			/*
			 * take care the partial word by patch the high order
			 * bits with 0's. Here we dealing with the case that
			 * the last word of the bitmap is not aligned.
			 */
			ASSERT(limitbits < BT_ULMASK);
			word = word & (~(BT_ULMAXMASK << limitbits + 1));
			/*
			 * examine the partial word.
			 */
			if (word == 0) {
				return (-1);
			} else {
				return ((wx << BT_ULSHIFT) |
					(lowbit(word) - 1));
			}
		}
	} else {
		return ((wx << BT_ULSHIFT) | (lowbit(word) - 1));
	}
}
