/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)zero.c	1.10	99/01/14 SMI"

#include	<sys/types.h>

void
zero(caddr_t addr, size_t len)
{
	int _len = (int)len;

	while (_len-- > 0) {
		/* Align and go faster */
		if (((int)addr & ((sizeof (int) - 1))) == 0) {
			/* LINTED */
			int *w = (int *)addr;
			/* LINTED */
			while (_len > 0) {
				*w++ = 0;
				_len -= sizeof (int);
			}
			return;
		}
		*addr++ = 0;
	}
}
