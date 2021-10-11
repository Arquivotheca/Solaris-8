/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_BITMAP_H
#define	_SYS_BITMAP_H

#pragma ident	"@(#)bitmap.h	1.16	99/04/13 SMI"	/* SVr4.0 1.6	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Operations on bitmaps of arbitrary size
 * A bitmap is a vector of 1 or more ulong_t's.
 * The user of the package is responsible for range checks and keeping
 * track of sizes.
 */

#ifdef _LP64
#define	BT_ULSHIFT	6 /* log base 2 of BT_NBIPUL, to extract word index */
#else
#define	BT_ULSHIFT	5 /* log base 2 of BT_NBIPUL, to extract word index */
#endif

#define	BT_NBIPUL	(1 << BT_ULSHIFT)	/* n bits per ulong_t */
#define	BT_ULMASK	(BT_NBIPUL - 1)		/* to extract bit index */

#ifdef _LP64
#define	BT_ULMAXMASK	0xffffffffffffffff	/* used by bt_getlowbit */
#else
#define	BT_ULMAXMASK	0xffffffff
#endif

/*
 * bitmap is a ulong_t *, bitindex an index_t
 *
 * The macros BT_WIM and BT_BIW internal; there is no need
 * for users of this package to use them.
 */

/*
 * word in map
 */
#define	BT_WIM(bitmap, bitindex) \
	((bitmap)[(bitindex) >> BT_ULSHIFT])
/*
 * bit in word
 */
#define	BT_BIW(bitindex) \
	(1UL << ((bitindex) & BT_ULMASK))

/*
 * These are public macros
 *
 * BT_BITOUL == n bits to n ulong_t's
 */
#define	BT_BITOUL(nbits) \
	(((nbits) + BT_NBIPUL - 1l) / BT_NBIPUL)
#define	BT_TEST(bitmap, bitindex) \
	((BT_WIM((bitmap), (bitindex)) & BT_BIW(bitindex)) ? 1 : 0)
#define	BT_SET(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) |= BT_BIW(bitindex); }
#define	BT_CLEAR(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) &= ~BT_BIW(bitindex); }


#if defined(_KERNEL) && !defined(_ASM)
/*
 * return next available bit index from map with specified number of bits
 */
extern index_t	bt_availbit(ulong_t *bitmap, size_t nbits);
/*
 * find the highest order bit that is on, and is within or below
 * the word specified by wx
 */
extern int	bt_gethighbit(ulong_t *mapp, int wx);
extern int	bt_range(ulong_t *bitmap, size_t *pos1, size_t *pos2,
			size_t nbits);
/*
 * Find highest and lowest one bit set.
 *	Returns bit number + 1 of bit that is set, otherwise returns 0.
 * Low order bit is 0, high order bit is 31.
 */
extern int	highbit(ulong_t);
extern int	lowbit(ulong_t);
extern int	bt_getlowbit(ulong_t *bitmap, size_t start, size_t stop);
#endif	/* _KERNEL && !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BITMAP_H */
