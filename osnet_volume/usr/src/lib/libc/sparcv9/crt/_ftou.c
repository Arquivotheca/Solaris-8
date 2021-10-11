/*
 * Copyright (c) 1987-1996 Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)_ftou.c	1.1	96/11/22 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include "libc.h"

unsigned
__dtou(double d)
{
	/* Convert double to unsigned. */

	int id;

	/*
	 * id = d is correct if 0 <= d < 2**31, and good enough if d is NaN
	 * or d < 0 or d >= 2**32.  Otherwise, since the result (int) d of
	 * converting 2**31 <= d < 2**32 is unknown, adjust d before the
	 * conversion.
	 */

	if (d >= 2147483648.0)
		id = 0x80000000 | (int) (d - 2147483648.0);
	else
		id = (int) d;
	return ((unsigned) id);
}

unsigned
__ftou(float d)
{
	/* Convert float to unsigned. */

	int id;
	/*
	 * id = d is correct if 0 <= d < 2**31, and good enough if d is NaN
	 * or d < 0 or d >= 2**32.  Otherwise, since the result (int) d of
	 * converting 2**31 <= d < 2**32 is unknown, adjust d before the
	 * conversion.
	 */

	if (d >= 2147483648.0)
		id = 0x80000000 | (int) (d - 2147483648.0);
	else
		id = (int) d;
	return ((unsigned) id);
}
