/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ncakmem.c	1.3	99/10/07 SMI"

#define	VMEM

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/tuneable.h>
#include <sys/kmem.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>
#include <vm/as.h>

#include "osvers.h"
#include "ncakmem.h"

#ifdef	VMEM

#if defined(i386)
#define	VMEM_MAX_CACHE	256
#else	/* defined(i386) */
#define	VMEM_MAX_CACHE	128
#endif	/* defined(i386) */

#if	SunOS <= SunOS_5_7

static struct kmem_backend *nca_vmem_backend;
static struct kmem_cache *nca_vmem_cache[VMEM_MAX_CACHE + 1];

static void *
nca_vmem_getpages(size_t npages, uint_t flag)
{
	uintptr_t a;

	if (flag & KM_NOSLEEP) {
		if ((a = rmalloc(kernelmap, npages)) == 0)
			return (0);
	} else {
		a = rmalloc_wait(kernelmap, npages);
	}
	return (kmxtob(a));
}

static void
nca_vmem_freepages(void *pages, size_t npages)
{
	rmfree(kernelmap, npages, btokmx(pages));
}

static void *
nca_vmem_alloc(size_t npages, int flag)
{
	if (npages > VMEM_MAX_CACHE) {
		return (kmem_backend_alloc(nca_vmem_backend, ptob(npages),
						flag));
	} else {
		return (kmem_cache_alloc(nca_vmem_cache[npages], flag));
	}
}

static void
nca_vmem_free(void *addr, size_t npages)
{
	if (npages > VMEM_MAX_CACHE) {
		kmem_backend_free(nca_vmem_backend, addr, ptob(npages));
	} else {
		kmem_cache_free(nca_vmem_cache[npages], addr);
	}
}

void
nca_vmem_init(void)
{
	int np;

	nca_vmem_backend = kmem_backend_create("nca_vmem_backend",
		nca_vmem_getpages, nca_vmem_freepages, PAGESIZE,
						KMEM_CLASS_OTHER);

	for (np = 1; np <= VMEM_MAX_CACHE; np++) {
		char name[64];
		(void) sprintf(name, "nca_vmem_alloc_%d", ptob(np));
		nca_vmem_cache[np] = kmem_cache_create(name, ptob(np), PAGESIZE,
			NULL, NULL, NULL, NULL, nca_vmem_backend, KMC_NOTOUCH);
	}
}

void
nca_vmem_fini(void)
{
	int np;

	for (np = 1; np <= VMEM_MAX_CACHE; np++)
		kmem_cache_destroy(nca_vmem_cache[np]);

	kmem_backend_destroy(nca_vmem_backend);
}

#elif	SunOS >= SunOS_5_8

static vmem_t *nca_vmem_cache[VMEM_MAX_CACHE + 1];

static void *
nca_vmem_alloc(size_t npages, int flag)
{
	int	ix = npages;

	if (npages > VMEM_MAX_CACHE)
		ix = 0;
	return (vmem_alloc(nca_vmem_cache[ix], npages << PAGESHIFT,
	    flag & KM_VMFLAGS));
}

static void
nca_vmem_free(void *addr, size_t npages)
{
	int	ix = npages;

	if (npages > VMEM_MAX_CACHE)
		ix = 0;
	vmem_free(nca_vmem_cache[ix], addr, npages << PAGESHIFT);
}

static void *
nca_vmem_xalloc(vmem_t *vmp, size_t size, int vmflag)
{
	return (vmem_xalloc(vmp, size, PAGESIZE, 0, 0, NULL, NULL, vmflag));
}

void
nca_vmem_init(void)
{
	int np;
	char name[64];

	for (np = 0; np <= VMEM_MAX_CACHE; np++) {
		if (np == 0) {
			(void) sprintf(name, "nca_vmem_alloc_oversized");
		} else {
			(void) sprintf(name, "nca_vmem_alloc_%d", ptob(np));
		}
		nca_vmem_cache[np] = vmem_create(name, NULL, 0, PAGESIZE,
		    nca_vmem_xalloc, vmem_free, heap_arena, 0, VM_SLEEP);
	}
}

void
nca_vmem_fini(void)
{
	int np;

	for (np = 0; np <= VMEM_MAX_CACHE; np++)
		vmem_destroy(nca_vmem_cache[np]);
}

#endif	/* SunOS */

#else	/* VMEM */

void nca_vmem_init(void) {}

void nca_vmem_fini(void) {}

#endif	/* VMEM */

page_t **
kmem_phys_alloc(size_t len, int flags)
{
	page_t *pp, **ppa;
	pgcnt_t npages = btopr(len);
	size_t ppalen;
	size_t i;

	ppalen = (npages + 1) * sizeof (struct page_t *);

	if ((ppa = (page_t **)kmem_zalloc(ppalen, flags)) == NULL) {
		return (NULL);
	}

	mutex_enter(&freemem_lock);
	while (availrmem < tune.t_minarmem + npages) {
		if (flags & KM_NOSLEEP) {
			mutex_exit(&freemem_lock);
			kmem_free((void *)ppa, ppalen);
			return (NULL);
		}
		mutex_exit(&freemem_lock);
		page_needfree(npages);
		kmem_reap();
		delay(hz >> 2);
		page_needfree(-(spgcnt_t)npages);
		mutex_enter(&freemem_lock);
	}
	availrmem -= npages;
	mutex_exit(&freemem_lock);

	ppa[0] = (page_t *)npages;
	for (i = 1; i <= npages; i++) {

		if (!page_create_wait(1, flags))
			goto out;

#if	SunOS <= SunOS_2_7
		if ((pp = page_get_freelist(&kvp, (u_offset_t)0, &kas, NULL,
		    PAGESIZE, flags)) == NULL) {
			if ((pp = page_get_cachelist(&kvp, (u_offset_t)0, &kas,
			    NULL, flags)) == NULL) {
				goto out;
			}
		}
#else
	{
		struct seg kseg;

		kseg.s_as = &kas;
		if ((pp = page_get_freelist(&kvp, (u_offset_t)0, &kseg, NULL,
		    PAGESIZE, flags, NULL)) == NULL) {
			if ((pp = page_get_cachelist(&kvp, (u_offset_t)0, &kseg,
			    NULL, flags, NULL)) == NULL) {
				goto out;
			}
		}
	}
#endif
		PP_CLRFREE(pp);
		ppa[i] = pp;
		page_downgrade(pp);
	}

	return (ppa);
out:
	for (i = 1; ppa[i] != NULL && i <= npages; i++) {
		page_free(ppa[i], 0);
	}

	page_create_putback(npages - i);
	kmem_free((void *)ppa, ppalen);

	mutex_enter(&freemem_lock);
	availrmem += npages;
	mutex_exit(&freemem_lock);

	return ((page_t **)NULL);
}

void
kmem_phys_free(page_t **ppa)
{
	pgcnt_t npages = (pgcnt_t)ppa[0];
	size_t ppalen = (npages + 1) * sizeof (struct page_t *);
	size_t i;

	for (i = 1; i <= npages; i++) {
		if (! page_tryupgrade(ppa[i])) {
			page_unlock(ppa[i]);
			/*CONSTCOND*/
			while (1) {
				if (page_lock(ppa[i], SE_EXCL,
						NULL, P_RECLAIM)) {
					break;
				}
			}
		}
		page_free(ppa[i], 0);
	}

	kmem_free((void *)ppa, ppalen);

	mutex_enter(&freemem_lock);
	availrmem += npages;
	mutex_exit(&freemem_lock);
}

void *
kmem_phys_mapin(page_t **ppa, int flags)
{
	pgcnt_t npages = (pgcnt_t)ppa[0];
	size_t i;
	caddr_t addr;
	void *base;

#ifdef	VMEM

	if ((base = nca_vmem_alloc(npages, flags)) == NULL) {
		return (NULL);
	}

#else	/* VMEM */

	if (flags & KM_NOSLEEP) {
		if ((a = rmalloc(kernelmap, npages)) == 0) {
			return (NULL);
		}
	} else {
		a = rmalloc_wait(kernelmap, npages);
	}

	base = kmxtob(a);

#endif	/* VMEM */

	addr = base;

	for (i = 1; i <= npages; i++) {
		hat_memload(kas.a_hat, addr, ppa[i],
			(PROT_ALL & ~PROT_USER) | HAT_NOSYNC, HAT_LOAD_LOCK);
		addr += MMU_PAGESIZE;
	}

	return (base);
}

void
kmem_phys_mapout(page_t **ppa, void *base)
{
	pgcnt_t npages = (pgcnt_t)ppa[0];
	size_t i;

	for (i = 1; i <= npages; i++) {
		if (! page_tryupgrade(ppa[i])) {
			page_unlock(ppa[i]);
			/*CONSTCOND*/
			while (1) {
				if (page_lock(ppa[i], SE_EXCL,
					NULL, P_RECLAIM)) {
					break;
				}
			}
		}
		(void) hat_pageunload(ppa[i], HAT_FORCE_PGUNLOAD);
		page_downgrade(ppa[i]);
	}

#ifdef	VMEM

	nca_vmem_free(base, npages);

#else	/* VMEM */

	rmfree(kernelmap, npages, (u_long)btokmx(base));

#endif	/* VMEM */
}
