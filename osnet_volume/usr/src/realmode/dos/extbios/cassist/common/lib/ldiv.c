/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Minimal C library for Solars x86 realmode drivers, long divide:
 *
 *    This file contains a variant of the ANSI "ldiv" routine which delivers
 *    the quotent and remainder of a single integer division operation.  While
 *    "ldiv" was originally intended to enhance performance (where two long
 *    division operations would otherwise be needed), it turns out to be
 *    especially useful for realmode drivers since they don't have access
 *    to the Microsoft C library's internal long division routine.
 *
 *    This routine differs from the ANSI version in one significant way:
 *    We always assume that the divisor has no more than 15 significant bits!
 */

#ident "<@(#)ldiv.c	1.3	95/08/14	SMI>"
#include <stdlib.h>

ldiv_t
ldiv(long over, long under)
{
	/*
	 *  Long division with remainder:
	 *
	 *    Because the divisor is guaranteed to be less than the dividend
	 *    (i.e, quotent will be at least 1), results may be interpreted as
	 *    either signed or unsigned!
	 */

	static ldiv_t result;

	_asm {
		/*
		 *  Do the real work in assembler language (you do remember how
		 *  long division works don't you?).  We can take some short-
		 *  cuts here because we know:
		 *
		 *      a)  The divisor is only one digit (base 65536)
		 *      b)  The dividend is only two digits!
		 */

		mov   ax, word ptr [over+2]
		xor   dx, dx
		div   word ptr [under]
		mov   word ptr [result+2], ax
		mov   ax, word ptr [over]
		div   word ptr [under]
		mov   word ptr [result+4], dx
		mov   word ptr [result], ax
		mov   ax, dx
		cwd
		mov   word ptr [result+6], dx
	}

	return (result);
}
