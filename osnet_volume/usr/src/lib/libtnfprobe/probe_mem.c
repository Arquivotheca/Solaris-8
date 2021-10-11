/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)probe_mem.c	1.5	94/12/16 SMI"

/*
 * Includes
 */

#include "prb_internals.h"


/*
 * Globals
 *	Memory that prex uses to store combinations in target process
 */

#define	INITMEMSZ 2048

static char	initial_memory[INITMEMSZ];
static tnf_memseg_t	initial_memseg = {
					initial_memory,
					initial_memory + INITMEMSZ,
					DEFAULTMUTEX,
					0
				};

tnf_memseg_t *	__tnf_probe_memseg_p = &initial_memseg;


/*
 * __tnf_probe_alloc() - allocates memory from the global pool
 */

char *
__tnf_probe_alloc(size_t size)
{
	tnf_memseg_t *	memseg_p = __tnf_probe_memseg_p;
	char *		ptr;

	ptr = NULL;

	mutex_lock(&memseg_p->i_lock);

	memseg_p->i_reqsz = size;

	if ((memseg_p->min_p + size) <= memseg_p->max_p) {
	    ptr = memseg_p->min_p;
	    memseg_p->min_p += size;
	}

	memseg_p->i_reqsz = 0;

	mutex_unlock(&memseg_p->i_lock);

	return (ptr);

}   /* end __tnf_probe_alloc */
