/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers:
 *
 *  Pseudo-random number generator.  Interface is almost like the ANSII "rand"
 *  function, except if function is not seeded via call to srand(), we use
 *  the time of day as the seed.
 */

#ident	"<@(#)rand.c	1.4	95/08/14	SMI>"
#include <stdlib.h>

static int setup = 0;   /* Non-zero once accumulator has been seeded	    */
long _Raccum_;		/* The accumulator				    */

int
rand()
{
	/*
	 *  Random number generator:
	 *
	 *  Slightly better than the old rand(), but not by much!
	 */

	static int pi = 31415927; // A reasonably random string of bits

	if (!setup) _asm {
		/*
		 *  The interface is slightly different from rand() in that if
		 *  caller does not explicitly seed the accumulator (via
		 *  "srand"), we seed it with the low-order bits of the real-
		 *  time clock (ANSI says we should seed with "1" in this case).
		 */

		xor   ax, ax
		int   1Ah
		or    dx, 1
		mov   setup, dx
		mov   word ptr [_Raccum_], dx
	}

	_asm {
		/*
		 *  We have to do the double-precision multiply by hand so as
		 *  to avoid generating a call into the Microsoft C library.
		 */

		mov   ax, word ptr [_Raccum_];	 Pick up low order word and
		mul   pi;			 .. multiply by constant.  Save
		mov   cx, dx;			 .. 32-bit overflow in cx
		mov   word ptr [_Raccum_], ax
		mov   ax, word ptr [_Raccum_+2]; Now mpy high order word and
		mul   pi;			 .. add in overflow from low-
		add   ax, cx;			 .. order word.
		mov   word ptr [_Raccum_+2], ax
	}

	return (((++_Raccum_) >> 9) & 0x7FFF);
}

void
srand(unsigned seed)
{
	/*
	 *  Seeds the random number generator explicitly.
	 */

	_Raccum_ = seed | 1;
	setup = 1;
}
