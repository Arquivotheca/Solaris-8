/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Putil.c	1.1	99/03/23 SMI"

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#include "Pcontrol.h"
#include "Putil.h"

/*
 * Place the new element on the list prior to the existing element.
 */
void
list_link(void *new, void *existing)
{
	list_t *p = new;
	list_t *q = existing;

	if (q) {
		p->list_forw = q;
		p->list_back = q->list_back;
		q->list_back->list_forw = p;
		q->list_back = p;
	} else {
		p->list_forw = p->list_back = p;
	}
}

/*
 * Unchain the specified element from a list.
 */
void
list_unlink(void *old)
{
	list_t *p = old;

	if (p->list_forw != p) {
		p->list_back->list_forw = p->list_forw;
		p->list_forw->list_back = p->list_back;
	}
	p->list_forw = p->list_back = p;
}

/*
 * Routines to manipulate sigset_t, fltset_t, or sysset_t.  These routines
 * are provided as equivalents for the <sys/procfs.h> macros prfillset,
 * premptyset, praddset, and prdelset.  These functions are preferable
 * because they are not macros which rely on using sizeof (*sp), and thus
 * can be used to create common code to manipulate event sets.  The set
 * size must be passed explicitly, e.g. : prset_fill(&set, sizeof (set));
 */
void
prset_fill(void *sp, size_t size)
{
	size_t i = size / sizeof (uint32_t);

	while (i != 0)
		((uint32_t *)sp)[--i] = (uint32_t)0xFFFFFFFF;
}

void
prset_empty(void *sp, size_t size)
{
	size_t i = size / sizeof (uint32_t);

	while (i != 0)
		((uint32_t *)sp)[--i] = (uint32_t)0;
}

void
prset_add(void *sp, size_t size, uint_t flag)
{
	if (flag - 1 < 32 * size / sizeof (uint32_t))
		((uint32_t *)sp)[(flag - 1) / 32] |= 1U << ((flag - 1) % 32);
}

void
prset_del(void *sp, size_t size, uint_t flag)
{
	if (flag - 1 < 32 * size / sizeof (uint32_t))
		((uint32_t *)sp)[(flag - 1) / 32] &= ~(1U << ((flag - 1) % 32));
}

int
prset_ismember(void *sp, size_t size, uint_t flag)
{
	return ((flag - 1 < 32 * size / sizeof (uint32_t)) &&
	    (((uint32_t *)sp)[(flag - 1) / 32] & (1U << ((flag - 1) % 32))));
}

/*
 * If _libproc_debug is set, printf the debug message to stderr
 * with an appropriate prefix.
 */
/*PRINTFLIKE1*/
void
dprintf(const char *format, ...)
{
	if (_libproc_debug) {
		va_list alist;

		va_start(alist, format);
		(void) fputs("libproc DEBUG: ", stderr);
		(void) vfprintf(stderr, format, alist);
		va_end(alist);
	}
}
