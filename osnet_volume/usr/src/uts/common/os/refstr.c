/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)refstr.c	1.1	99/03/31 SMI"

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/refstr.h>
#include <sys/refstr_impl.h>

refstr_t *
refstr_alloc(const char *str)
{
	refstr_t *rsp;
	size_t size = sizeof (rsp->rs_size) + sizeof (rsp->rs_refcnt) +
		strlen(str) + 1;

	ASSERT(size <= UINT32_MAX);
	rsp = kmem_alloc(size, KM_SLEEP);
	rsp->rs_size = (uint32_t)size;
	rsp->rs_refcnt = 1;
	(void) strcpy(rsp->rs_string, str);
	return (rsp);
}

const char *
refstr_value(refstr_t *rsp)
{
	return ((const char *)rsp->rs_string);
}

void
refstr_hold(refstr_t *rsp)
{
	atomic_add_32(&rsp->rs_refcnt, 1);
}

void
refstr_rele(refstr_t *rsp)
{
	if (atomic_add_32_nv(&rsp->rs_refcnt, -1) == 0)
		kmem_free(rsp, (size_t)rsp->rs_size);
}
