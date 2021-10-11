/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)simex.h	1.1	96/12/06 SMI"

/* Format of a simulated extended precision power of ten.  These 
 * numbers contain 71 bits; the first 70 bits are obtained by taking
 * the ceiling at the 70th bit.  The idea is roughly to compensate
 * for truncation errors during multiplication.  The 71st bit is whatever
 * actually belongs there.  For obtaining 64 bits of accuracy, exactly
 * how the 71st bit is handled is not critical.
 *
 * Each number comes with a binary exponent.
 */

struct simex {
	short signif[5];
	short expo;
};

/*
 * This table contains, in simulated extended format, 1e320, 1e288,
 * 1e256, 1e224, etc., descending by factors of 1e32.
 * The lowest power in the table is 1e-352.
 */
extern struct simex _bigpow[];

/*
 * This table contains 1e16, 1e15, 1e14, etc., down to 1e-15.
 */

extern struct simex _litpow[];

