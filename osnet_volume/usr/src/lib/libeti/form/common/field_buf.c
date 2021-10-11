/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)field_buf.c	1.5	97/09/17 SMI" /* SVr4.0 1.4 */

/*LINTLIBRARY*/

#include <sys/types.h>
#include "utility.h"

int
set_field_buffer(FIELD *f, int n, char *v)
{
	char *p;
	char *x;
	size_t s;
	int err = 0;
	int	len;
	int	size;

	if (!f || !v || n < 0 || n > f->nbuf)
		return (E_BAD_ARGUMENT);

	len = (int)  strlen(v);
	size = BufSize(f);

	if (Status(f, GROWABLE) && len > size)
		if (!_grow_field(f, (len - size - 1)/GrowSize(f) + 1))
			return (E_SYSTEM_ERROR);

	x = Buffer(f, n);
	s = BufSize(f);
	p = memccpy(x, v, '\0', s);

	if (p)
		(void) memset(p - 1, ' ', (size_t) (s - (p - x) + 1));

	if (n == 0) {
		if (_sync_field(f) != E_OK)
			++err;
		if (_sync_linked(f) != E_OK)
			++err;
	}
	return (err ? E_SYSTEM_ERROR : E_OK);
}

char *
field_buffer(FIELD *f, int n)
{
/*
 * field_buffer may not be accurate on the current field unless
 * called from within the check validation function or the
 * form/field init/term functions.
 * field_buffer is always accurate on validated fields.
 */

	if (f && n >= 0 && n <= f -> nbuf)
		return (Buffer(f, n));
	else
		return ((char *) 0);
}
