/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hash.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf_hash = _elf_hash


#include "syn.h"


#define	MASK	(~(unsigned long)0<<28)


unsigned long
elf_hash(const char * name)
{
	register unsigned long		g, h = 0;
	register const unsigned char	*nm = (unsigned char *)name;

	while (*nm != '\0') {
		h = (h << 4) + *nm++;
		if ((g = h & MASK) != 0)
			h ^= g >> 24;
		h &= ~MASK;
	}
	return (h);
}
