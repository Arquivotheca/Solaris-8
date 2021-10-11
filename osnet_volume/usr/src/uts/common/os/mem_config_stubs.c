/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mem_config_stubs.c	1.24	98/12/03 SMI"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <sys/mem_config.h>
#include <sys/mem_cage.h>

/*ARGSUSED*/
int
kphysm_add_memory_dynamic(pfn_t base, pgcnt_t npgs)
{
	return (KPHYSM_ENOTSUP);
}

/*ARGSUSED*/
int
kphysm_del_gethandle(memhandle_t *xmhp)
{
	return (KPHYSM_ENOTSUP);
}

/*ARGSUSED*/
int
kphysm_del_span(
	memhandle_t handle,
	pfn_t base,
	pgcnt_t npgs)
{
	return (KPHYSM_EHANDLE);
}

/*ARGSUSED*/
int
kphysm_del_span_query(
	pfn_t base,
	pgcnt_t npgs,
	memquery_t *mqp)
{
	return (KPHYSM_ENOTSUP);
}

/*ARGSUSED*/
int
kphysm_del_release(memhandle_t handle)
{
	return (KPHYSM_EHANDLE);
}

/*ARGSUSED*/
int
kphysm_del_cancel(memhandle_t handle)
{
	return (KPHYSM_EHANDLE);
}

/*ARGSUSED*/
int
kphysm_del_status(
	memhandle_t handle,
	memdelstat_t *mdstp)
{
	return (KPHYSM_EHANDLE);
}

/*ARGSUSED*/
int
kphysm_del_start(
	memhandle_t handle,
	void (*complete)(void *, int),
	void *complete_arg)
{
	return (KPHYSM_EHANDLE);
}

/*ARGSUSED*/
int
kphysm_setup_func_register(kphysm_setup_vector_t *vec, void *arg)
{
	return (0);
}

/*ARGSUSED*/
void
kphysm_setup_func_unregister(kphysm_setup_vector_t *vec, void *arg)
{
}

/* These should be in a platform stubs file. */

/*ARGSUSED*/
int
arch_kphysm_del_span_ok(pfn_t base, pgcnt_t npgs)
{
	return (0);
}

/*ARGSUSED*/
int
arch_kphysm_relocate(pfn_t base, pgcnt_t npgs)
{
	return (ENOTSUP);
}

int
arch_kphysm_del_supported(void)
{
	return (0);
}

/*ARGSUSED*/
int
pfn_is_being_deleted(pfn_t pfnum)
{
	return (0);
}

/* These should be in a platform stubs file. */

int kcage_on;
pgcnt_t kcage_freemem;
pgcnt_t kcage_throttlefree;
pgcnt_t kcage_desfree;
pgcnt_t kcage_needfree;

/*ARGSUSED*/
void
kcage_create_throttle(pgcnt_t npages, int flags)
{
}

void
kcage_cageout_init(void)
{
}

void
kcage_cageout_wakeup()
{
}
void
memlist_read_lock()
{
}

void
memlist_read_unlock()
{
}

void
memlist_write_lock()
{
}

void
memlist_write_unlock()
{
}
