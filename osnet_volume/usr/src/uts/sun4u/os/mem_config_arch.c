/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mem_config_arch.c	1.5	98/08/12 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <sys/mem_config.h>

/*ARGSUSED*/
int
arch_kphysm_del_span_ok(pfn_t base, pgcnt_t npgs)
{
	ASSERT(npgs != 0);
	return (0);
}

/*ARGSUSED*/
int
arch_kphysm_relocate(pfn_t base, pgcnt_t npgs)
{
	ASSERT(npgs != 0);
	return (ENOTSUP);
}

int
arch_kphysm_del_supported(void)
{
	return (1);
}
