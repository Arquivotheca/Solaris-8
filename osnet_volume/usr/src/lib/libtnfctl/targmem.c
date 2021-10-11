/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)targmem.c	1.6	98/01/19 SMI"

/*
 * Function to allocate memory in target process (used by combinations).
 */

#ifndef DEBUG
#define	NDEBUG	1
#endif

#include <assert.h>
#include "tnfctl_int.h"
#include "prb_internals.h"
#include "dbg.h"


/*
 * _tnfctl_targmem_alloc() - allocates memory in the target process.
 */
tnfctl_errcode_t
_tnfctl_targmem_alloc(tnfctl_handle_t *hndl, size_t size, uintptr_t *addr_p)
{
	int			miscstat;
	tnf_memseg_t		memseg;

	assert(hndl->memseg_p != NULL);
	*addr_p = NULL;

	/* read the memseg block from the target process */
	miscstat = hndl->p_read(hndl->proc_p, hndl->memseg_p, &memseg,
		sizeof (memseg));
	if (miscstat)
		return (TNFCTL_ERR_INTERNAL);

	/* if there is memory left, allocate it */
	if ((memseg.min_p + memseg.i_reqsz) <= (memseg.max_p - size)) {
		memseg.max_p -= size;

		miscstat = hndl->p_write(hndl->proc_p, hndl->memseg_p,
			&memseg, sizeof (memseg));
		if (miscstat)
			return (TNFCTL_ERR_INTERNAL);

		*addr_p = (uintptr_t) memseg.max_p;

		DBG_TNF_PROBE_2(_tnfctl_targmem_alloc_1, "libtnfctl",
			"sunw%verbosity 3",
			tnf_long, size_allocated, size,
			tnf_opaque, at_location, *addr_p);

		return (TNFCTL_ERR_NONE);
	} else {
		return (TNFCTL_ERR_INTERNAL);
	}
}
