/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kmem.c	1.3	99/11/20 SMI"

#include <mdb/mdb_param.h>
#include <mdb/mdb_modapi.h>
#include <sys/cpuvar.h>
#include <sys/kmem_impl.h>
#include <sys/vmem_impl.h>
#include <sys/machelf.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/sysmacros.h>
#include <alloca.h>

#include "kmem.h"

#define	dprintf(x) if (mdb_debug_level) { \
	mdb_printf("kmem debug: ");  \
	/*CSTYLED*/\
	mdb_printf x ;\
}

#define	KMEM_ALLOCATED			0x1
#define	KMEM_FREE			0x2
#define	KMEM_BUFCTL			0x4

static int mdb_debug_level = 0;
int kmem_content_maxsave;

/*ARGSUSED*/
int
kmem_init_walkers(uintptr_t addr, const kmem_cache_t *c, void *ignored)
{
	mdb_walker_t w;
	char descr[64];

	(void) mdb_snprintf(descr, sizeof (descr),
	    "walk the %s cache", c->cache_name);

	w.walk_name = c->cache_name;
	w.walk_descr = descr;
	w.walk_init = kmem_walk_init;
	w.walk_step = kmem_walk_step;
	w.walk_fini = kmem_walk_fini;
	w.walk_init_arg = (void *)addr;

	if (mdb_add_walker(&w) == -1)
		mdb_warn("failed to add %s walker", c->cache_name);

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
kmem_debug(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_debug_level ^= 1;

	mdb_printf("kmem: debugging is now %s\n",
	    mdb_debug_level ? "on" : "off");

	return (DCMD_OK);
}

typedef struct {
	uintptr_t kcw_first;
	uintptr_t kcw_current;
} kmem_cache_walk_t;

int
kmem_cache_walk_init(mdb_walk_state_t *wsp)
{
	kmem_cache_walk_t *kcw;
	kmem_cache_t c;
	uintptr_t cp;
	GElf_Sym sym;

	if (mdb_lookup_by_name("kmem_null_cache", &sym) == -1) {
		mdb_warn("couldn't find kmem_null_cache");
		return (WALK_ERR);
	}

	cp = (uintptr_t)sym.st_value;

	if (mdb_vread(&c, sizeof (kmem_cache_t), cp) == -1) {
		mdb_warn("couldn't read cache at %p", cp);
		return (WALK_ERR);
	}

	kcw = mdb_alloc(sizeof (kmem_cache_walk_t), UM_SLEEP);

	kcw->kcw_first = cp;
	kcw->kcw_current = (uintptr_t)c.cache_next;
	wsp->walk_data = kcw;

	return (WALK_NEXT);
}

int
kmem_cache_walk_step(mdb_walk_state_t *wsp)
{
	kmem_cache_walk_t *kcw = wsp->walk_data;
	kmem_cache_t c;
	int status;

	if (mdb_vread(&c, sizeof (kmem_cache_t), kcw->kcw_current) == -1) {
		mdb_warn("couldn't read cache at %p", kcw->kcw_current);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(kcw->kcw_current, &c, wsp->walk_cbdata);

	if ((kcw->kcw_current = (uintptr_t)c.cache_next) == kcw->kcw_first)
		return (WALK_DONE);

	return (status);
}

void
kmem_cache_walk_fini(mdb_walk_state_t *wsp)
{
	kmem_cache_walk_t *kcw = wsp->walk_data;
	mdb_free(kcw, sizeof (kmem_cache_walk_t));
}

int
kmem_cpu_cache_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL) {
		mdb_warn("kmem_cpu_cache doesn't support global walks");
		return (WALK_ERR);
	}

	if (mdb_layered_walk("cpu", wsp) == -1) {
		mdb_warn("couldn't walk 'cpu'");
		return (WALK_ERR);
	}

	wsp->walk_data = (void *)wsp->walk_addr;

	return (WALK_NEXT);
}

int
kmem_cpu_cache_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t caddr = (uintptr_t)wsp->walk_data;
	const cpu_t *cpu = wsp->walk_layer;
	kmem_cpu_cache_t cc;

	caddr += cpu->cpu_cache_offset;

	if (mdb_vread(&cc, sizeof (kmem_cpu_cache_t), caddr) == -1) {
		mdb_warn("couldn't read kmem_cpu_cache at %p", caddr);
		return (WALK_ERR);
	}

	return (wsp->walk_callback(caddr, &cc, wsp->walk_cbdata));
}

int
kmem_slab_walk_init(mdb_walk_state_t *wsp)
{
	uintptr_t caddr = wsp->walk_addr;
	kmem_cache_t c;

	if (caddr == NULL) {
		mdb_warn("kmem_slab doesn't support global walks\n");
		return (WALK_ERR);
	}

	if (mdb_vread(&c, sizeof (c), caddr) == -1) {
		mdb_warn("couldn't read kmem_cache at %p", caddr);
		return (WALK_ERR);
	}

	wsp->walk_data =
	    (void *)(caddr + offsetof(kmem_cache_t, cache_nullslab));
	wsp->walk_addr = (uintptr_t)c.cache_freelist;

	return (WALK_NEXT);
}

int
kmem_slab_walk_step(mdb_walk_state_t *wsp)
{
	kmem_slab_t s;
	uintptr_t addr = wsp->walk_addr;
	uintptr_t saddr = (uintptr_t)wsp->walk_data;
	uintptr_t caddr = saddr - offsetof(kmem_cache_t, cache_nullslab);

	if (addr == saddr)
		return (WALK_DONE);

	if (mdb_vread(&s, sizeof (s), addr) == -1) {
		mdb_warn("failed to read slab at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	if ((uintptr_t)s.slab_cache != caddr) {
		mdb_warn("slab %p isn't in cache %p (in cache %p)\n",
		    addr, caddr, s.slab_cache);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)s.slab_next;

	return (wsp->walk_callback(addr, &s, wsp->walk_cbdata));
}

int
kmem_cache(uintptr_t addr, uint_t flags, int ac, const mdb_arg_t *argv)
{
	kmem_cache_t c;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("kmem_cache", "kmem_cache", ac, argv) == -1) {
			mdb_warn("can't walk kmem_cache");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (DCMD_HDRSPEC(flags))
		mdb_printf("%-?s %-25s %4s %6s %8s %8s\n", "ADDR", "NAME",
		    "FLAG", "CFLAG", "BUFSIZE", "BUFTOTL");

	if (mdb_vread(&c, sizeof (c), addr) == -1) {
		mdb_warn("couldn't read kmem_cache at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%0?p %-25s %04x %06x %8d %8d\n", addr, c.cache_name,
	    c.cache_flags, c.cache_cflags, c.cache_bufsize, c.cache_buftotal);

	return (DCMD_OK);
}

static int
addrcmp(const void *lhs, const void *rhs)
{
	uintptr_t p1 = *((uintptr_t *)lhs);
	uintptr_t p2 = *((uintptr_t *)rhs);

	if (p1 < p2)
		return (-1);
	if (p1 > p2)
		return (1);
	return (0);
}

static int
bufctlcmp(const kmem_bufctl_audit_t **lhs, const kmem_bufctl_audit_t **rhs)
{
	const kmem_bufctl_audit_t *bcp1 = *lhs;
	const kmem_bufctl_audit_t *bcp2 = *rhs;

	if (bcp1->bc_timestamp > bcp2->bc_timestamp)
		return (-1);

	if (bcp1->bc_timestamp < bcp2->bc_timestamp)
		return (1);

	return (0);
}

#define	KMW_ADD(ptr) { \
	if (kmw->kmw_ndx == kmw->kmw_size) { \
		mdb_free(kmw->kmw_buf, kmw->kmw_size); \
		kmw->kmw_size <<= 1; \
		kmw->kmw_buf = mdb_zalloc(kmw->kmw_size * \
		    sizeof (uintptr_t), UM_SLEEP); \
	} \
	kmw->kmw_buf[kmw->kmw_ndx++] = (uintptr_t)(ptr); \
}

#define	READMAG_ROUNDS(rounds) { \
	if (mdb_vread(mp, magbsize, (uintptr_t)kmp) == -1) { \
		mdb_warn("couldn't read magazine at %p", kmp); \
		goto out1; \
	} \
	for (i = 0; i < rounds; i++) { \
		if (type & KMEM_FREE) { \
			kmem_buftag_t tag; \
			uintptr_t taddr; \
			if (!(type & KMEM_BUFCTL)) { \
				KMW_ADD(mp->mag_round[i]); \
				continue; \
			} \
			/* LINTED - alignment */ \
			taddr = (uintptr_t)KMEM_BUFTAG(cp, mp->mag_round[i]); \
			if (mdb_vread(&tag, sizeof (tag), taddr) == -1) { \
				mdb_warn("couldn't read buftag at %p", taddr); \
				continue; \
			} \
			KMW_ADD((uintptr_t)tag.bt_bufctl); \
			continue; \
		} \
		maglist[magcnt++] = mp->mag_round[i]; \
		if (magcnt == magmax) { \
			mdb_warn("%d magazines exceeds fudge factor", magcnt); \
			goto out1; \
		} \
	} \
}

typedef struct kmem_walk {
	uintptr_t *kmw_buf;
	int kmw_size;
	int kmw_ndx;
} kmem_walk_t;

int
kmem_walk_init_common(mdb_walk_state_t *wsp, int type)
{
	kmem_walk_t *kmw;
	int ncpus, csize, i, cpu;
	kmem_cache_t *cp;
	kmem_slab_t s, *sp, *end;
	kmem_bufctl_t bc, *bcp;

	int magsize, magmax, magbsize;
	int magcnt = 0;
	kmem_magazine_t *kmp, *mp;
	char *valid, *ubase;
	void **maglist = NULL;
	uint_t chunksize, slabsize;
	int status = -1;
	uintptr_t bufp, addr = wsp->walk_addr;

	if (addr == NULL) {
		mdb_warn("kmem walk doesn't support global walks\n");
		return (WALK_ERR);
	}

	dprintf(("walking %p\n", addr));

	/*
	 * First we need to figure out how many CPUs are configured in the
	 * system to know how much to slurp out.
	 */
	mdb_readvar(&ncpus, "ncpus");

	csize = KMEM_CACHE_SIZE(ncpus);
	cp = alloca(csize);

	if (mdb_vread(cp, csize, addr) == -1) {
		mdb_warn("couldn't read cache at addr %p", addr);
		goto out2;
	}

	dprintf(("buf total is %d\n", cp->cache_buftotal));

	if (cp->cache_buftotal == 0) {
		wsp->walk_data = NULL;
		return (WALK_NEXT);
	}

	/*
	 * This is a little weak.  If NOTOUCH is set, and we're walking
	 * free buffers, we'll just punt.
	 */
	if ((cp->cache_cflags & KMC_NOTOUCH) && (type & KMEM_FREE)) {
		wsp->walk_data = NULL;
		return (WALK_NEXT);
	}

	/*
	 * There are several places where we need to go buffer hunting:
	 * the per-CPU loaded magazine, the per-CPU spare full magazine,
	 * the full magazine list (at the depot) and finally the slab
	 * layer.
	 *
	 * In terms of buffers in magazines, we have the number of
	 * magazines on the cache_fmag_list plus at most two magazines per
	 * CPU (the loaded and the spare).  Toss in 100 magazines as a
	 * fudge factor in case this is live (the number "100" comes from
	 * the same fudge factor in crash(1M)).
	 */
	magsize = cp->cache_magazine_size;
	magmax = (cp->cache_fmag_total + 2 * ncpus + 100) * magsize;
	magbsize = sizeof (kmem_magazine_t) + (magsize - 1) * sizeof (void *);
	mp = alloca(magbsize);

	if (type & KMEM_ALLOCATED)
		maglist = mdb_alloc(magmax * sizeof (void *), UM_SLEEP);

	wsp->walk_data = kmw = mdb_zalloc(sizeof (kmem_walk_t), UM_SLEEP);
	kmw->kmw_size = cp->cache_buftotal;
	kmw->kmw_buf = mdb_zalloc(kmw->kmw_size * sizeof (uintptr_t), UM_SLEEP);

	/*
	 * First up: the magazines in the depot (i.e. on the cache_fmag_list).
	 */
	for (kmp = cp->cache_fmag_list; kmp != NULL; ) {
		/* LINTED - alignment (in READMAG_ROUNDS) */
		READMAG_ROUNDS(magsize);
		kmp = mp->mag_next;

		if (kmp == cp->cache_fmag_list)
			break; /* cache_fmag_list loop detected */
	}

	dprintf(("cache_fmag_list done\n"));

	/*
	 * Now whip through the CPUs, snagging the loaded magazines
	 * and full spares.
	 */
	for (cpu = 0; cpu < ncpus; cpu++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu];

		dprintf(("reading cpu cache %p\n", ccp));

		if (ccp->cc_rounds > 0 &&
		    (kmp = ccp->cc_loaded_mag) != NULL)
			/* LINTED - alignment (in READMAG_ROUNDS) */
			READMAG_ROUNDS(ccp->cc_rounds);

		if ((kmp = ccp->cc_full_mag) != NULL)
			/* LINTED - alignment (in READMAG_ROUNDS) */
			READMAG_ROUNDS(magsize);
	}

	dprintf(("cpu magazines done\n"));

	/*
	 * We have all of the buffers from the magazines;  now sort them
	 * in order to be able to bsearch on them later.
	 */
	qsort(maglist, magcnt, sizeof (void *), addrcmp);

	/*
	 * If this is a KMF_HASH cache, we'll walk the hash table instead
	 * of walking through the slab layer.
	 */
	if ((cp->cache_flags & KMF_HASH) && (type & KMEM_ALLOCATED)) {
		kmem_bufctl_t **hash;
		int nelems = cp->cache_hash_mask + 1;
		int hsize = nelems * sizeof (uintptr_t);
		uintptr_t haddr = (uintptr_t)cp->cache_hash_table;
		void *buf;

		hash = mdb_alloc(hsize, UM_SLEEP);

		if (mdb_vread(hash, hsize, haddr) == -1) {
			mdb_warn("failed to read hash table at %p", haddr);
			goto hash_out;
		}

		for (i = 0; i < nelems; i++) {
			dprintf(("hash ndx %d\n", i));
			for (bcp = hash[i]; bcp != NULL; bcp = bc.bc_next) {
				dprintf(("bcp %p\n", bcp));
				if (mdb_vread(&bc, sizeof (bc),
				    (uintptr_t)bcp) == -1) {
					mdb_warn("failed to read hashed bufctl "
					    "at ndx %d (%p)", i, hash[i]);
					goto hash_out;
				}

				/*
				 * We have a buffer which has been allocated
				 * out of the global layer.  We need to make
				 * sure that it's not actually sitting in a
				 * magazine before we report it as an allocated
				 * buffer.
				 */
				buf = bc.bc_addr;

				if (!(cp->cache_flags & KMF_NOMAGAZINE) &&
				    bsearch(&buf, maglist, magcnt,
				    sizeof (void *), addrcmp) != NULL)
					continue;

				KMW_ADD(type & KMEM_BUFCTL ?
				    (uintptr_t)bcp : (uintptr_t)buf);
			}
		}

		kmw->kmw_ndx = 0;
		status = 0;
hash_out:
		mdb_free(hash, hsize);
		goto out1;
	}

	/*
	 * Now we need to walk through the slabs
	 */
	chunksize = cp->cache_chunksize;
	slabsize = cp->cache_slabsize;

	valid = mdb_alloc(slabsize / cp->cache_bufsize, UM_SLEEP);
	ubase = mdb_alloc(slabsize + sizeof (kmem_bufctl_t), UM_SLEEP);

	sp = cp->cache_nullslab.slab_next;
	end = (kmem_slab_t *)
	    ((uintptr_t)&((kmem_cache_t *)0)->cache_nullslab + addr);

	dprintf(("starting with slab %p\n", sp));

	while (sp != end) {
		int chunks;
		char *kbase;
		void *buf, *ubuf;

		dprintf(("reading slab %p\n", sp));

		if (mdb_vread(&s, sizeof (s), (uintptr_t)sp) == -1) {
			mdb_warn("failed to read kmem_slab_t at %p", sp);
			status = 0;
			goto out;
		}

		if ((uintptr_t)s.slab_cache != addr) {
			/*
			 * We've somehow become derailed; kick out.
			 */
			mdb_warn("slab %p isn't in cache %p (in cache %p)\n",
			    sp, addr, s.slab_cache);
			status = 0;
			goto out;
		}

		chunks = s.slab_chunks;
		kbase = s.slab_base;

		dprintf(("kbase is %p\n", kbase));

		if (mdb_vread(ubase, chunks * chunksize,
		    (uintptr_t)s.slab_base) == -1) {
			mdb_warn("failed to read slab at %p", s.slab_base);
			goto out;
		}

		/*
		 * Set the valid map.
		 */
		memset(valid, 1, chunks);

		/*
		 * Now we're going to actually walk through the slab;
		 * we know that any buffer we find here isn't valid.
		 */
		bcp = s.slab_head;

		dprintf(("refcnt is %d; chunks is %d\n",
		    s.slab_refcnt, chunks));

		if (s.slab_refcnt == chunks && bcp != NULL) {
			dprintf(("slab %p in cache %p has refcnt == chunks "
			    "but head %p\n", sp, addr, bcp));
		}

		if (s.slab_refcnt < chunks && bcp == NULL) {
			mdb_warn("slab %p in cache %p has refcnt < chunks "
			    "but NULL head\n", sp, addr);
		}

		for (i = s.slab_refcnt; i < chunks; i++) {
			uint_t ndx;

			dprintf(("bcp is %p\n", bcp));

			if (bcp == NULL) {
				mdb_warn("slab %p in cache %p has NULL bcp\n",
				    sp, addr);
				break;
			}

			if (cp->cache_flags & KMF_HASH) {
				if (mdb_vread(&bc, sizeof (bc),
				    (uintptr_t)bcp) == -1) {
					mdb_warn("failed to read bufctl"
					    "ptr at %p", bcp);
					break;
				}
				buf = bc.bc_addr;
			} else {
				/*
				 * Otherwise the buffer is in the slab which
				 * we've read in;  we just need to determine
				 * its offset in the slab to find the
				 * kmem_bufctl_t.
				 */
				bc = *((kmem_bufctl_t *)
				    ((uintptr_t)bcp - (uintptr_t)kbase +
				    (uintptr_t)ubase));

				buf = (void *)((uintptr_t)bcp -
				    cp->cache_offset);
			}

			ndx = ((uintptr_t)buf - (uintptr_t)kbase) / chunksize;

			if (ndx > slabsize / cp->cache_bufsize) {
				/*
				 * This is very wrong; we have managed to find
				 * a buffer in the slab which shouldn't
				 * actually be here.  Emit a warning, and
				 * try to continue.
				 */
				mdb_warn("buf %p is out of range for "
				    "slab %p, cache %p\n", buf, sp, addr);
			} else {
				/*
				 * If this is a valid address, then we know
				 * that we have found a buffer on the slab's
				 * freelist; tip the valid bit low
				 */
				valid[ndx] = 0;
			}

			bcp = bc.bc_next;
		}

		for (i = 0; i < chunks; i++) {
			buf = (char *)kbase + i * chunksize;
			ubuf = (char *)ubase + i * chunksize;

			if (type & KMEM_ALLOCATED) {
				if (!valid[i])
					continue;

				if (!(cp->cache_flags & KMF_NOMAGAZINE) &&
				    bsearch(&buf, maglist, magcnt,
				    sizeof (void *), addrcmp) != NULL)
					continue;
			} else {
				if (valid[i])
					continue;
			}

			/*
			 * This buffer must be allocated;  it's
			 * not in the slab layer, and it's not hanging
			 * out in a magazine.
			 */
			if (type & KMEM_BUFCTL) {
				kmem_buftag_t *tag;

				if (!(cp->cache_flags & KMF_AUDIT))
					continue;

				/* LINTED - alignment */
				tag = KMEM_BUFTAG(cp, ubuf);
				bufp = (uintptr_t)tag->bt_bufctl;
			} else {
				bufp = (uintptr_t)buf;
			}
			KMW_ADD(bufp);
		}
lap:
		sp = s.slab_next;
	}

	kmw->kmw_ndx = 0;
	status = 0;

out:
	mdb_free(valid, slabsize / cp->cache_bufsize);
	mdb_free(ubase, slabsize + sizeof (kmem_bufctl_t));
out1:
	if (status == -1) {
		mdb_free(kmw->kmw_buf, kmw->kmw_size * sizeof (uintptr_t));
		mdb_free(kmw, sizeof (kmem_walk_t));
	}

	if (maglist != NULL)
		mdb_free(maglist, magmax * sizeof (void *));
out2:
	return (status);
}


/*ARGSUSED*/
int
kmem_walk_all(uintptr_t addr, const kmem_cache_t *c, mdb_walk_state_t *wsp)
{
	if (mdb_pwalk(wsp->walk_data, wsp->walk_callback,
	    wsp->walk_cbdata, addr) == -1)
		return (WALK_DONE);

	return (WALK_NEXT);
}

#define	KMEM_WALK_ALL(name, wsp) { \
	wsp->walk_data = (name); \
	if (mdb_walk("kmem_cache", (mdb_walk_cb_t)kmem_walk_all, wsp) == -1) \
		return (WALK_ERR); \
	return (WALK_DONE); \
}

int
kmem_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_arg != NULL)
		wsp->walk_addr = (uintptr_t)wsp->walk_arg;

	if (wsp->walk_addr == NULL)
		KMEM_WALK_ALL("kmem", wsp);
	return (kmem_walk_init_common(wsp, KMEM_ALLOCATED));
}

int
bufctl_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL)
		KMEM_WALK_ALL("bufctl", wsp);
	return (kmem_walk_init_common(wsp, KMEM_ALLOCATED|KMEM_BUFCTL));
}

int
freemem_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL)
		KMEM_WALK_ALL("freemem", wsp);
	return (kmem_walk_init_common(wsp, KMEM_FREE));
}

int
freectl_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL)
		KMEM_WALK_ALL("freectl", wsp);
	return (kmem_walk_init_common(wsp, KMEM_FREE|KMEM_BUFCTL));
}

int
kmem_walk_step_common(mdb_walk_state_t *wsp, uintptr_t *buf)
{
	kmem_walk_t *kmw = wsp->walk_data;

	if (kmw == NULL)
		return (WALK_DONE);

	do {
		if (kmw->kmw_ndx == kmw->kmw_size)
			return (WALK_DONE);
	} while ((*buf = kmw->kmw_buf[kmw->kmw_ndx++]) == NULL);

	return (WALK_NEXT);
}

int
kmem_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t buf;

	if (kmem_walk_step_common(wsp, &buf) == WALK_DONE)
		return (WALK_DONE);

	return (wsp->walk_callback(buf, NULL, wsp->walk_cbdata));
}

int
bufctl_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t buf;
	kmem_bufctl_audit_t b;

	if (kmem_walk_step_common(wsp, &buf) == WALK_DONE)
		return (WALK_DONE);

	if (mdb_vread(&b, sizeof (b), buf) == -1)
		return (WALK_DONE);

	return (wsp->walk_callback(buf, &b, wsp->walk_cbdata));
}

void
kmem_walk_fini(mdb_walk_state_t *wsp)
{
	kmem_walk_t *kmw = wsp->walk_data;

	if (kmw == NULL)
		return;

	mdb_free(kmw->kmw_buf, kmw->kmw_size * sizeof (uintptr_t));
	mdb_free(kmw, sizeof (kmem_walk_t));
}

typedef struct kmem_log_walk {
	kmem_bufctl_audit_t *klw_base;
	kmem_bufctl_audit_t **klw_sorted;
	kmem_log_header_t klw_lh;
	size_t klw_size;
	size_t klw_maxndx;
	size_t klw_ndx;
} kmem_log_walk_t;

int
kmem_log_walk_init(mdb_walk_state_t *wsp)
{
	uintptr_t lp = wsp->walk_addr;
	kmem_log_walk_t *klw;
	kmem_log_header_t *lhp;
	int maxndx, i, j, k;

	/*
	 * By default (global walk), walk the kmem_transaction_log.  Otherwise
	 * read the log whose kmem_log_header_t is stored at walk_addr.
	 */
	if (lp == NULL && mdb_readvar(&lp, "kmem_transaction_log") == -1) {
		mdb_warn("failed to read 'kmem_transaction_log'");
		return (WALK_ERR);
	}

	if (lp == NULL) {
		mdb_warn("log is disabled\n");
		return (WALK_ERR);
	}

	klw = mdb_zalloc(sizeof (kmem_log_walk_t), UM_SLEEP);
	lhp = &klw->klw_lh;

	if (mdb_vread(lhp, sizeof (kmem_log_header_t), lp) == -1) {
		mdb_warn("failed to read log header at %p", lp);
		mdb_free(klw, sizeof (kmem_log_walk_t));
		return (WALK_ERR);
	}

	klw->klw_size = lhp->lh_chunksize * lhp->lh_nchunks;
	klw->klw_base = mdb_alloc(klw->klw_size, UM_SLEEP);
	maxndx = lhp->lh_chunksize / sizeof (kmem_bufctl_audit_t) - 1;

	if (mdb_vread(klw->klw_base, klw->klw_size,
	    (uintptr_t)lhp->lh_base) == -1) {
		mdb_warn("failed to read log at base %p", lhp->lh_base);
		mdb_free(klw->klw_base, klw->klw_size);
		mdb_free(klw, sizeof (kmem_log_walk_t));
		return (WALK_ERR);
	}

	klw->klw_sorted = mdb_alloc(maxndx * lhp->lh_nchunks *
	    sizeof (kmem_bufctl_audit_t *), UM_SLEEP);

	for (i = 0, k = 0; i < lhp->lh_nchunks; i++) {
		kmem_bufctl_audit_t *chunk = (kmem_bufctl_audit_t *)
		    ((uintptr_t)klw->klw_base + i * lhp->lh_chunksize);

		for (j = 0; j < maxndx; j++)
			klw->klw_sorted[k++] = &chunk[j];
	}

	qsort(klw->klw_sorted, k, sizeof (kmem_bufctl_audit_t *),
	    (int(*)(const void *, const void *))bufctlcmp);

	klw->klw_maxndx = k;
	wsp->walk_data = klw;

	return (WALK_NEXT);
}

int
kmem_log_walk_step(mdb_walk_state_t *wsp)
{
	kmem_log_walk_t *klw = wsp->walk_data;
	kmem_bufctl_audit_t *bcp;

	if (klw->klw_ndx == klw->klw_maxndx)
		return (WALK_DONE);

	bcp = klw->klw_sorted[klw->klw_ndx++];

	return (wsp->walk_callback((uintptr_t)bcp - (uintptr_t)klw->klw_base +
	    (uintptr_t)klw->klw_lh.lh_base, bcp, wsp->walk_cbdata));
}

void
kmem_log_walk_fini(mdb_walk_state_t *wsp)
{
	kmem_log_walk_t *klw = wsp->walk_data;

	mdb_free(klw->klw_base, klw->klw_size);
	mdb_free(klw->klw_sorted, klw->klw_maxndx *
	    sizeof (kmem_bufctl_audit_t *));
	mdb_free(klw, sizeof (kmem_log_walk_t));
}

typedef struct allocdby_bufctl {
	uintptr_t abb_addr;
	hrtime_t abb_ts;
} allocdby_bufctl_t;

typedef struct allocdby_walk {
	const char *abw_walk;
	uintptr_t abw_thread;
	size_t abw_nbufs;
	size_t abw_size;
	allocdby_bufctl_t *abw_buf;
	size_t abw_ndx;
} allocdby_walk_t;

int
allocdby_walk_bufctl(uintptr_t addr, const kmem_bufctl_audit_t *bcp,
    allocdby_walk_t *abw)
{
	if ((uintptr_t)bcp->bc_thread != abw->abw_thread)
		return (WALK_NEXT);

	if (abw->abw_nbufs == abw->abw_size) {
		allocdby_bufctl_t *buf;
		size_t oldsize = sizeof (allocdby_bufctl_t) * abw->abw_size;

		buf = mdb_zalloc(oldsize << 1, UM_SLEEP);

		bcopy(abw->abw_buf, buf, oldsize);
		mdb_free(abw->abw_buf, oldsize);

		abw->abw_size <<= 1;
		abw->abw_buf = buf;
	}

	abw->abw_buf[abw->abw_nbufs].abb_addr = addr;
	abw->abw_buf[abw->abw_nbufs].abb_ts = bcp->bc_timestamp;
	abw->abw_nbufs++;

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
allocdby_walk_cache(uintptr_t addr, const kmem_cache_t *c, allocdby_walk_t *abw)
{
	if (mdb_pwalk(abw->abw_walk, (mdb_walk_cb_t)allocdby_walk_bufctl,
	    abw, addr) == -1) {
		mdb_warn("couldn't walk bufctl for cache %p", addr);
		return (WALK_DONE);
	}

	return (WALK_NEXT);
}

static int
allocdby_cmp(const allocdby_bufctl_t *lhs, const allocdby_bufctl_t *rhs)
{
	if (lhs->abb_ts < rhs->abb_ts)
		return (1);
	if (lhs->abb_ts > rhs->abb_ts)
		return (-1);
	return (0);
}

int
allocdby_walk_init_common(mdb_walk_state_t *wsp, const char *walk)
{
	allocdby_walk_t *abw;

	if (wsp->walk_addr == NULL) {
		mdb_warn("allocdby walk doesn't support global walks\n");
		return (WALK_ERR);
	}

	abw = mdb_zalloc(sizeof (allocdby_walk_t), UM_SLEEP);

	abw->abw_thread = wsp->walk_addr;
	abw->abw_walk = walk;
	abw->abw_size = 128;	/* something reasonable */
	abw->abw_buf =
	    mdb_zalloc(abw->abw_size * sizeof (allocdby_bufctl_t), UM_SLEEP);

	wsp->walk_data = abw;

	if (mdb_walk("kmem_cache",
	    (mdb_walk_cb_t)allocdby_walk_cache, abw) == -1) {
		mdb_warn("couldn't walk kmem_cache");
		allocdby_walk_fini(wsp);
		return (WALK_ERR);
	}

	qsort(abw->abw_buf, abw->abw_nbufs, sizeof (allocdby_bufctl_t),
	    (int(*)(const void *, const void *))allocdby_cmp);

	return (WALK_NEXT);
}

int
allocdby_walk_init(mdb_walk_state_t *wsp)
{
	return (allocdby_walk_init_common(wsp, "bufctl"));
}

int
freedby_walk_init(mdb_walk_state_t *wsp)
{
	return (allocdby_walk_init_common(wsp, "freectl"));
}

int
allocdby_walk_step(mdb_walk_state_t *wsp)
{
	allocdby_walk_t *abw = wsp->walk_data;
	kmem_bufctl_audit_t bc;
	uintptr_t addr;

	if (abw->abw_ndx == abw->abw_nbufs)
		return (WALK_DONE);

	addr = abw->abw_buf[abw->abw_ndx++].abb_addr;

	if (mdb_vread(&bc, sizeof (bc), addr) == -1) {
		mdb_warn("couldn't read bufctl at %p", addr);
		return (WALK_DONE);
	}

	return (wsp->walk_callback(addr, &bc, wsp->walk_cbdata));
}

void
allocdby_walk_fini(mdb_walk_state_t *wsp)
{
	allocdby_walk_t *abw = wsp->walk_data;

	mdb_free(abw->abw_buf, sizeof (allocdby_bufctl_t) * abw->abw_size);
	mdb_free(abw, sizeof (allocdby_walk_t));
}

/*ARGSUSED*/
int
allocdby_walk(uintptr_t addr, const kmem_bufctl_audit_t *bcp, void *ignored)
{
	char c[MDB_SYM_NAMLEN];
	GElf_Sym sym;
	int i;

	mdb_printf("%0?p %12llx ", addr, bcp->bc_timestamp);
	for (i = 0; i < bcp->bc_depth; i++) {
		if (mdb_lookup_by_addr(bcp->bc_stack[i],
		    MDB_SYM_FUZZY, c, sizeof (c), &sym) == -1)
			continue;
		if (strncmp(c, "kmem_", 5) == 0)
			continue;
		mdb_printf("%s+0x%lx\n",
		    c, bcp->bc_stack[i] - (uintptr_t)sym.st_value);
		break;
	}

	return (WALK_NEXT);
}

int
allocdby_common(uintptr_t addr, uint_t flags, const char *w)
{
	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	mdb_printf("%-?s %12s %s\n", "BUFCTL", "TIMESTAMP", "CALLER");

	if (mdb_pwalk(w, (mdb_walk_cb_t)allocdby_walk, NULL, addr) == -1) {
		mdb_warn("can't walk '%s' for %p", w, addr);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
allocdby(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (allocdby_common(addr, flags, "allocdby"));
}

/*ARGSUSED*/
int
freedby(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (allocdby_common(addr, flags, "freedby"));
}

typedef struct whatis {
	uintptr_t w_addr;
	const kmem_cache_t *w_cache;
	const vmem_t *w_vmem;
	int w_found;
	uint_t w_verbose;
	uint_t w_freemem;
	uint_t w_all;
	uint_t w_bufctl;
} whatis_t;

int
whatis_check_ctl(uintptr_t addr, const kmem_bufctl_audit_t *bcp, whatis_t *w)
{
	kmem_cache_t c;
	size_t csize;
	uintptr_t caddr = (uintptr_t)bcp->bc_contents;

	/*
	 * Check the contents.
	 */
	if (w->w_addr < caddr || w->w_addr > caddr + kmem_content_maxsave)
		return (WALK_NEXT);

	if (mdb_vread(&c, sizeof (c), (uintptr_t)bcp->bc_cache) == -1) {
		mdb_warn("couldn't read cache at %p", bcp->bc_cache);
		return (WALK_NEXT);
	}

	csize = MIN(kmem_content_maxsave, c.cache_offset);

	if (w->w_addr < caddr || w->w_addr > caddr + csize)
		return (WALK_NEXT);

	mdb_printf("%p is %p+%p, contents for bufctl %p\n",
	    w->w_addr, caddr, w->w_addr - caddr, addr);
found:
	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

/*ARGSUSED*/
int
whatis_walk_ctl(uintptr_t addr, void *ignored, whatis_t *w)
{
	kmem_bufctl_audit_t bc;

	if (mdb_vread(&bc, sizeof (bc), addr) == -1) {
		mdb_warn("couldn't read bufctl_audit at %p", addr);
		return (WALK_NEXT);
	}

	return (whatis_check_ctl(addr, &bc, w));
}

/*ARGSUSED*/
int
whatis_walk_kmem(uintptr_t addr, void *ignored, whatis_t *w)
{
	if (w->w_addr < addr || w->w_addr >= addr + w->w_cache->cache_bufsize)
		return (WALK_NEXT);

	mdb_printf("%p is %p+%p, %s from %s\n",
	    w->w_addr, addr, w->w_addr - addr,
	    w->w_freemem == FALSE ? "allocated" : "freed",
	    w->w_cache->cache_name);

	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

int
whatis_walk_seg(uintptr_t addr, const vmem_seg_t *vs, whatis_t *w)
{
	if (w->w_addr < vs->vs_start || w->w_addr >= vs->vs_end)
		return (WALK_NEXT);

	if (w->w_bufctl == FALSE) {
		mdb_printf("%p is %p+%p ", w->w_addr,
		    vs->vs_start, w->w_addr - vs->vs_start, w->w_vmem->vm_name);
	} else {
		mdb_printf("%p is %p+%p (vmem_seg %p) ",
		    w->w_addr, vs->vs_start, w->w_addr - vs->vs_start, addr,
		    w->w_vmem->vm_name);
	}

	mdb_printf("%sfrom %s vmem arena\n", w->w_freemem == TRUE ?
	    "freed " : "", w->w_vmem->vm_name);

	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

int
whatis_walk_vmem(uintptr_t addr, const vmem_t *vmem, whatis_t *w)
{
	w->w_vmem = vmem;

	if (w->w_verbose)
		mdb_printf("Searching vmem arena %s%s...\n", vmem->vm_name,
		    w->w_freemem == TRUE ? " for free virtual" : "");

	if (mdb_pwalk(w->w_freemem == TRUE ? "vmem_free" : "vmem_alloc",
	    (mdb_walk_cb_t)whatis_walk_seg, w, addr) == -1)
		mdb_warn("can't walk vmem seg for %p", addr);

	return (w->w_found && w->w_all == FALSE ? WALK_DONE : WALK_NEXT);
}

/*ARGSUSED*/
int
whatis_walk_bufctl(uintptr_t baddr, const kmem_bufctl_t *bcp, whatis_t *w)
{
	uintptr_t addr;

	if (bcp == NULL)
		return (WALK_NEXT);

	addr = (uintptr_t)bcp->bc_addr;

	if (w->w_addr < addr || w->w_addr >= addr + w->w_cache->cache_bufsize)
		return (WALK_NEXT);

	mdb_printf("%p is %p+%p, bufctl %p %s from %s\n",
	    w->w_addr, addr, w->w_addr - addr, baddr,
	    w->w_freemem == FALSE ? "allocated" : "freed",
	    w->w_cache->cache_name);

	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

int
whatis_walk_cache(uintptr_t addr, const kmem_cache_t *c, whatis_t *w)
{
	char *walk = w->w_bufctl == FALSE ?
	    (w->w_freemem == FALSE ? "kmem" : "freemem") :
	    (w->w_freemem == FALSE ? "bufctl" : "freectl");

	w->w_cache = c;

	if (w->w_verbose)
		mdb_printf("Searching %s%s...\n", c->cache_name,
		    w->w_freemem == FALSE ? "" : " for free memory");

	if (mdb_pwalk(walk, w->w_bufctl == FALSE ?
	    (mdb_walk_cb_t)whatis_walk_kmem : (mdb_walk_cb_t)whatis_walk_bufctl,
	    w, addr) == -1) {
		mdb_warn("can't find kmem walker");
		return (WALK_DONE);
	}
	return (WALK_NEXT);
}

int
whatis_walk_touch(uintptr_t addr, const kmem_cache_t *c, whatis_t *w)
{
	if (c->cache_cflags & KMC_NOTOUCH)
		return (WALK_NEXT);

	return (whatis_walk_cache(addr, c, w));
}

int
whatis_walk_notouch(uintptr_t addr, const kmem_cache_t *c, whatis_t *w)
{
	if (!(c->cache_cflags & KMC_NOTOUCH))
		return (WALK_NEXT);

	return (whatis_walk_cache(addr, c, w));
}

int
whatis_walk_thread(uintptr_t addr, const kthread_t *t, whatis_t *w)
{
	if (w->w_addr < (uintptr_t)t->t_stkbase ||
	    w->w_addr > (uintptr_t)t->t_stk)
		return (WALK_NEXT);

	if (t->t_stkbase == NULL)
		return (WALK_NEXT);

	mdb_printf("%p is in thread %p's stack\n", w->w_addr, addr);

	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

int
whatis_walk_modctl(uintptr_t addr, const struct modctl *m, whatis_t *w)
{
	struct module mod;
	char name[MODMAXNAMELEN], *where;
	char c[MDB_SYM_NAMLEN];
	Shdr shdr;
	GElf_Sym sym;

	if (m->mod_mp == NULL)
		return (WALK_NEXT);

	if (mdb_vread(&mod, sizeof (mod), (uintptr_t)m->mod_mp) == -1) {
		mdb_warn("couldn't read modctl %p's module", addr);
		return (WALK_NEXT);
	}

	if (w->w_addr >= (uintptr_t)mod.text &&
	    w->w_addr < (uintptr_t)mod.text + mod.text_size) {
		where = "text segment";
		goto found;
	}

	if (w->w_addr >= (uintptr_t)mod.data &&
	    w->w_addr < (uintptr_t)mod.data + mod.data_size) {
		where = "data segment";
		goto found;
	}

	if (w->w_addr >= (uintptr_t)mod.bss &&
	    w->w_addr < (uintptr_t)mod.bss + mod.bss_size) {
		where = "bss";
		goto found;
	}

	if (mdb_vread(&shdr, sizeof (shdr), (uintptr_t)mod.symhdr) == -1) {
		mdb_warn("couldn't read symbol header for %p's module", addr);
		return (WALK_NEXT);
	}

	if (w->w_addr >= (uintptr_t)mod.symtbl && w->w_addr <
	    (uintptr_t)mod.symtbl + (uintptr_t)mod.nsyms * shdr.sh_entsize) {
		where = "symtab";
		goto found;
	}

	if (w->w_addr >= (uintptr_t)mod.symspace &&
	    w->w_addr < (uintptr_t)mod.symspace + (uintptr_t)mod.symsize) {
		where = "symspace";
		goto found;
	}

	return (WALK_NEXT);

found:
	if (mdb_readstr(name, sizeof (name), (uintptr_t)m->mod_modname) == -1)
		(void) mdb_snprintf(name, sizeof (name), "0x%p", addr);

	mdb_printf("%p is ", w->w_addr);

	/*
	 * If we found this address in a module, then there's a chance that
	 * it's actually a named symbol.  Try the symbol lookup.
	 */
	if (mdb_lookup_by_addr(w->w_addr, MDB_SYM_FUZZY, c, sizeof (c),
	    &sym) != -1 && w->w_addr >= (uintptr_t)sym.st_value &&
	    w->w_addr < (uintptr_t)sym.st_value + sym.st_size) {
		mdb_printf("%s+%lx ", c, w->w_addr - (uintptr_t)sym.st_value);
	}

	mdb_printf("in %s's %s\n", name, where);

	w->w_found++;
	return (w->w_all == TRUE ? WALK_NEXT : WALK_DONE);
}

int
whatis(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	whatis_t w;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	w.w_verbose = FALSE;
	w.w_freemem = FALSE;
	w.w_bufctl = FALSE;
	w.w_all = FALSE;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &w.w_verbose,
	    'a', MDB_OPT_SETBITS, TRUE, &w.w_all,
	    'b', MDB_OPT_SETBITS, TRUE, &w.w_bufctl, NULL) != argc)
		return (DCMD_USAGE);

	w.w_addr = addr;
	w.w_found = 0;

	if (mdb_walk("modctl", (mdb_walk_cb_t)whatis_walk_modctl, &w) == -1) {
		mdb_warn("couldn't find modctl walker");
		return (DCMD_ERR);
	}

	if (w.w_found && w.w_all == FALSE)
		return (DCMD_OK);

	/*
	 * Now search all thread stacks.  Yes, this is a little weak; we
	 * can save a lot of work by first checking to see if the adress
	 * is in segkp vs. segkmem.  But hey, computers are fast.
	 */
	if (mdb_walk("thread", (mdb_walk_cb_t)whatis_walk_thread, &w) == -1) {
		mdb_warn("couldn't find thread walker");
		return (DCMD_ERR);
	}

again:
	if (w.w_found && w.w_all == FALSE)
		return (DCMD_OK);

	if (mdb_walk("kmem_cache",
	    (mdb_walk_cb_t)whatis_walk_touch, &w) == -1) {
		mdb_warn("couldn't find kmem_cache walker");
		return (DCMD_ERR);
	}

	if (w.w_found && w.w_all == FALSE)
		return (DCMD_OK);

	if (mdb_walk("kmem_cache",
	    (mdb_walk_cb_t)whatis_walk_notouch, &w) == -1) {
		mdb_warn("couldn't find kmem_cache walker");
		return (DCMD_ERR);
	}

	if (w.w_found && w.w_all == FALSE)
		return (DCMD_OK);

	if (mdb_walk("vmem_postfix",
	    (mdb_walk_cb_t)whatis_walk_vmem, &w) == -1) {
		mdb_warn("couldn't find vmem_postfix walker");
		return (DCMD_ERR);
	}

	if (w.w_freemem == FALSE) {
		/*
		 * Time to search the free memory.
		 */
		w.w_freemem = TRUE;
		goto again;
	}

	if (w.w_found == 0)
		mdb_printf("%p is unknown\n", addr);

	return (DCMD_OK);
}

typedef struct kmem_log_cpu {
	uintptr_t kmc_low;
	uintptr_t kmc_high;
} kmem_log_cpu_t;

int
kmem_log_walk(uintptr_t addr, const kmem_bufctl_audit_t *b, kmem_log_cpu_t *kmc)
{
	int i;

	for (i = 0; i < NCPU; i++) {
		if (addr >= kmc[i].kmc_low && addr < kmc[i].kmc_high)
			break;
	}

	if (i == NCPU)
		mdb_printf("   ");
	else
		mdb_printf("%3d", i);

	mdb_printf(" %0?p %0?p %16llx %0?p\n", addr, b->bc_addr,
	    b->bc_timestamp, b->bc_thread);

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
kmem_log(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kmem_log_header_t lh;
	kmem_cpu_log_header_t clh;
	uintptr_t lhp, clhp;
	int ncpus;
	uintptr_t *cpu;
	GElf_Sym sym;
	kmem_log_cpu_t *kmc;
	int i;

	if (mdb_readvar(&lhp, "kmem_transaction_log") == -1) {
		mdb_warn("failed to read 'kmem_transaction_log'");
		return (DCMD_ERR);
	}

	if (lhp == NULL) {
		mdb_warn("no kmem transaction log\n");
		return (DCMD_ERR);
	}

	mdb_readvar(&ncpus, "ncpus");

	if (mdb_vread(&lh, sizeof (kmem_log_header_t), lhp) == -1) {
		mdb_warn("failed to read log header at %p", lhp);
		return (DCMD_ERR);
	}

	clhp = lhp + ((uintptr_t)&lh.lh_cpu[0] - (uintptr_t)&lh);

	cpu = mdb_alloc(sizeof (uintptr_t) * NCPU, UM_SLEEP | UM_GC);

	if (mdb_lookup_by_name("cpu", &sym) == -1) {
		mdb_warn("couldn't find 'cpu' array");
		return (DCMD_ERR);
	}

	if (sym.st_size != NCPU * sizeof (uintptr_t)) {
		mdb_warn("expected 'cpu' to be of size %d; found %d\n",
		    NCPU * sizeof (uintptr_t), sym.st_size);
		return (DCMD_ERR);
	}

	if (mdb_vread(cpu, sym.st_size, (uintptr_t)sym.st_value) == -1) {
		mdb_warn("failed to read cpu array at %p", sym.st_value);
		return (DCMD_ERR);
	}

	kmc = mdb_zalloc(sizeof (kmem_log_cpu_t) * NCPU, UM_SLEEP | UM_GC);

	for (i = 0; i < NCPU; i++) {

		if (cpu[i] == NULL)
			continue;

		if (mdb_vread(&clh, sizeof (clh), clhp) == -1) {
			mdb_warn("cannot read cpu %d's log header at %p",
			    i, clhp);
			return (DCMD_ERR);
		}

		kmc[i].kmc_low = clh.clh_chunk * lh.lh_chunksize +
		    (uintptr_t)lh.lh_base;
		kmc[i].kmc_high = (uintptr_t)clh.clh_current;

		clhp += sizeof (kmem_cpu_log_header_t);
	}

	mdb_printf("%3s %-?s %-?s %16s %-?s\n", "CPU", "ADDR", "BUFADDR",
	    "TIMESTAMP", "THREAD");

	if (mdb_walk("kmem_log", (mdb_walk_cb_t)kmem_log_walk, kmc) == -1) {
		mdb_warn("can't find kmem log walker");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

int
bufctl(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kmem_bufctl_audit_t bc;
	uintptr_t caller = NULL, thread = NULL;
	uintptr_t laddr, haddr, baddr = NULL;
	hrtime_t earliest = 0, latest = 0;
	int i, depth;
	char c[MDB_SYM_NAMLEN];
	GElf_Sym sym;

	if (mdb_getopts(argc, argv,
	    'c', MDB_OPT_UINTPTR, &caller,
	    't', MDB_OPT_UINTPTR, &thread,
	    'e', MDB_OPT_UINT64, &earliest,
	    'l', MDB_OPT_UINT64, &latest,
	    'a', MDB_OPT_UINTPTR, &baddr, NULL) != argc)
		return (DCMD_USAGE);

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%-?s %-?s %-12s  %-?s %s\n",
		    "ADDR", "BUFADDR", "TIMESTAMP", "THREAD", "CALLER");
	}

	if (mdb_vread(&bc, sizeof (bc), addr) == -1) {
		mdb_warn("couldn't read bufctl at %p", addr);
		return (DCMD_ERR);
	}

	/*
	 * Guard against bogus bc_depth in case the bufctl is corrupt or
	 * the address does not really refer to a bufctl.
	 */
	depth = MIN(bc.bc_depth, KMEM_STACK_DEPTH);

	if (caller != NULL) {
		laddr = caller;
		haddr = caller + sizeof (caller);

		if (mdb_lookup_by_addr(caller, MDB_SYM_FUZZY, c, sizeof (c),
		    &sym) != -1 && caller == (uintptr_t)sym.st_value) {
			/*
			 * We were provided an exact symbol value; any
			 * address in the function is valid.
			 */
			laddr = (uintptr_t)sym.st_value;
			haddr = (uintptr_t)sym.st_value + sym.st_size;
		}

		for (i = 0; i < depth; i++)
			if (bc.bc_stack[i] >= laddr && bc.bc_stack[i] < haddr)
				break;

		if (i == depth)
			return (DCMD_OK);
	}

	if (thread != NULL && (uintptr_t)bc.bc_thread != thread)
		return (DCMD_OK);

	if (earliest != 0 && bc.bc_timestamp < earliest)
		return (DCMD_OK);

	if (latest != 0 && bc.bc_timestamp > latest)
		return (DCMD_OK);

	if (baddr != 0 && (uintptr_t)bc.bc_addr != baddr)
		return (DCMD_OK);

	mdb_printf("%0?p %0?p %12llx %0?p", addr, bc.bc_addr,
	    bc.bc_timestamp, bc.bc_thread);

	for (i = 0; i < depth; i++) {
		if (mdb_lookup_by_addr(bc.bc_stack[i],
		    MDB_SYM_FUZZY, c, sizeof (c), &sym) == -1)
			continue;
		if (strncmp(c, "kmem_", 5) == 0)
			continue;
		mdb_printf(" %s+0x%lx\n",
		    c, bc.bc_stack[i] - (uintptr_t)sym.st_value);
		break;
	}

	return (DCMD_OK);
}

#define	KMEM_REDZONE_PATTERN \
		((uint32_t)(((uint64_t)KMEM_REDZONE_PATTERN_64) >> 32))

#ifdef _BIG_ENDIAN
/*
 * We need a bbedface pattern.
 */
#define	KMEM_REDZONE_PATTERN_WITH_SIG \
	((uint32_t)(KMEM_REDZONE_PATTERN & 0x00ffffff) | \
	    ((uint32_t)KMEM_REDZONE_BYTE) << 24)
#else
/*
 * We need a feedfabb pattern.
 */
#define	KMEM_REDZONE_PATTERN_WITH_SIG \
	((uint32_t)(KMEM_REDZONE_PATTERN & 0xffffff00) | \
	    ((uint32_t)KMEM_REDZONE_BYTE))
#endif /* _BIG_ENDIAN */

/*
 * Grr... these aren't #defined in the header file
 */
#define	KMEM_MAXBUF	16384
#define	KMEM_SIZE_DECODE(x)	((x) / (UINT_MAX / KMEM_MAXBUF))
#define	KMEM_SIZE_VALID(x)	((x) % (UINT_MAX / KMEM_MAXBUF) == 1)

typedef struct kmem_verify {
	uint64_t *kmv_buf;		/* buffer to read cache contents into */
	size_t kmv_size;		/* number of bytes in kmv_buf */
	int kmv_corruption;		/* > 0 if corruption found. */
	int kmv_besilent;		/* report actual corruption sites */
	struct kmem_cache kmv_cache;	/* the cache we're operating on */
} kmem_verify_t;

/*
 * verify_pattern()
 * 	verify that buf is filled with the pattern pat.
 */
static uint64_t *
verify_pattern(uint64_t *buf, uint64_t *bufend, uint64_t pat)
{
	while ((*buf == pat) && (buf < bufend))
		buf++;
	return (buf);
}

/*
 * verify_buftag()
 * 	check that the two pointers in a buftag xor to a well-known value
 */
static int
verify_buftag(kmem_buftag_t *bt, uintptr_t pat)
{
	uintptr_t chk;
	chk = (uintptr_t)bt->bt_bufctl ^ (uintptr_t)bt->bt_bxstat;

	/*
	 * The buftag bt_bufctl and bt_bxstat should xor to pat
	 */
	if (chk != pat) {
		return (-1);
	}
	return (0);
}

/*
 * verify_free()
 * 	verify the integrity of a free block of memory by checking
 * 	that it is filled with 0xdeadbeef and that it's buftag is sane.
 */
/*ARGSUSED1*/
static int
verify_free(uintptr_t addr, const void *data, void *private)
{
	kmem_verify_t *kmv = (kmem_verify_t *)private;
	uint64_t *buf = kmv->kmv_buf;	/* buf to validate */
	uint64_t *bufend;		/* ptr to start of buf suffix */
	uint32_t buftag0, buftag1;	/* 1st & 2nd 32bits are bit patterns */
	kmem_buftag_t *buftagp;		/* ptr to buftag (bufend) */
	int besilent = kmv->kmv_besilent;
	int cache_flags = kmv->kmv_cache.cache_flags;

	bufend = buf + kmv->kmv_cache.cache_offset / sizeof (uint64_t);
	buftagp = (kmem_buftag_t *)bufend;

	/*
	 * Read the buffer to check.
	 */
	if (mdb_vread(buf, kmv->kmv_size, addr) == -1) {
		if (!besilent)
			mdb_warn("couldn't read %p", addr);
		return (WALK_NEXT);
	}

	/*
	 * bufend is the first address past the end of the buffer, i.e, where
	 * we expect to find the redzone pattern.
	 */
	buftag0 = *(uint32_t *)bufend;
	buftag1 = *((uint32_t *)bufend + 1);

	/*
	 * If the cache is in KMF_LITE mode, just check that the first 64-bits
	 * of the buffer are deadbeef'd, and that the buftag looks sane.
	 * Otherwise, verify the whole pattern.
	 */
	if (cache_flags & KMF_LITE) {
		if (*buf != KMEM_FREE_PATTERN_64) {
			mdb_printf("buffer %p (free) is missing the free "
			    "pattern expected\n", addr);
			goto corrupt;
		}

		/*
		 * The 0xfeedface0xfeedface at the buffer's end has been
		 * overwritten with the former *beginning* of the buffer, so
		 * that constructor/destructor works; thus, there isn't
		 * much else to check about the buffer at this point.
		 */
		return (WALK_NEXT);
	}

	if (cache_flags & KMF_DEADBEEF) {
		buf = verify_pattern(buf, bufend, KMEM_FREE_PATTERN_64);
	} else {
		buf = bufend;
	}

	if (buf < bufend) {
		uintptr_t delta = (uintptr_t)buf - (uintptr_t)kmv->kmv_buf;
		if (!besilent)
			mdb_printf("buffer %p (free) seems corrupted, at %p\n",
			    addr, (uintptr_t)addr + delta);
		goto corrupt;
	} else if (buf == bufend) {
		/*
		 * If the cache doesn't have external bufctls, freelist linkage
		 * overwrites the first word of the redzone, so ignore it...
		 */
		if ((cache_flags & (KMF_HASH | KMF_REDZONE)) == KMF_REDZONE) {
			if (buftag1 != KMEM_REDZONE_PATTERN) {
				if (!besilent)
					mdb_printf("buffer %p (free) seems to "
					    "have a corrupt redzone pattern\n",
					    addr);
				goto corrupt;
			}
		} else {
			if ((buftag0 != KMEM_REDZONE_PATTERN) ||
			    (buftag1 != KMEM_REDZONE_PATTERN)) {
				if (!besilent)
					mdb_printf("buffer %p (free) seems to "
					    "have a corrupt redzone pattern\n",
					    addr);
				goto corrupt;
			}
		}
	}

	/*
	 * confirm bufctl pointer integrity.
	 */
	if (cache_flags & KMF_BUFTAG) {
		if (verify_buftag(buftagp, KMEM_BUFTAG_FREE) == -1) {
			if (!besilent)
				mdb_printf("buffer %p (free) has a corrupt "
				    "bufctl pointer\n", addr);
			goto corrupt;
		}
	}

	return (WALK_NEXT);
corrupt:
	kmv->kmv_corruption++;
	return (WALK_NEXT);
}

/*
 * verify_alloc()
 * 	Verify that the buftag of an allocated buffer makes sense with respect
 * 	to the buffer.
 */
/*ARGSUSED1*/
static int
verify_alloc(uintptr_t addr, const void *data, void *private)
{
	kmem_verify_t *kmv = (kmem_verify_t *)private;
	uint64_t *buf = kmv->kmv_buf;	/* buf to validate */
	uint64_t *bufend;		/* ptr to start of buf suffix */
	uint32_t buftag0, buftag1;	/* 1st & 2nd part are bit patterns */
	kmem_buftag_t *buftagp;
	int looks_ok = 0, size_ok = 1;	/* flags for finding corruption */
	int besilent = kmv->kmv_besilent;
	int cache_flags = kmv->kmv_cache.cache_flags;

	bufend = buf + kmv->kmv_cache.cache_offset / sizeof (uint64_t);
	buftagp = (kmem_buftag_t *)bufend;

	/*
	 * Read the buffer to check.
	 */
	if (mdb_vread(buf, kmv->kmv_size, addr) == -1) {
		if (!besilent)
			mdb_warn("couldn't read %p", addr);
		return (WALK_NEXT);
	}

	/*
	 * There are two cases to handle:
	 * 1. If the buf was alloc'd using kmem_cache_alloc, it will have
	 *    0xfeedfacefeedface at the end of it
	 * 2. If the buf was alloc'd using kmem_alloc, it will have
	 *    0xbb just past the end of the region in use.  At the buftag,
	 *    it will have 0xfeedface (or, if the whole buffer is in use,
	 *    0xfeedface & bb000000 or 0xfeedfacf & 000000bb depending on
	 *    endianness), followed by 32 bits containing the offset of the
	 *    0xbb byte in the buffer.
	 *
	 * Finally, the two 32-bit words that comprise the second half of the
	 * buftag should xor to KMEM_BUFTAG_ALLOC
	 */
	buftag0 = *(uint32_t *)bufend;
	buftag1 = *((uint32_t *)bufend + 1);

	if (buftag0 == KMEM_REDZONE_PATTERN) {
		if (buftag1 == KMEM_REDZONE_PATTERN) {
			looks_ok = 1;
		} else if (KMEM_SIZE_VALID(buftag1)) {
			if (((uint8_t *)buf)[KMEM_SIZE_DECODE(buftag1)] ==
			    KMEM_REDZONE_BYTE) {
				looks_ok = 1;
			}
		} else {
			size_ok = 0;
		}
	} else if (buftag0 == KMEM_REDZONE_PATTERN_WITH_SIG) {
		/*
		 * We already know where the 'bb' is in this case, it's at
		 * buf[bufsize], the first byte of the redzone.
		 */
		if (KMEM_SIZE_VALID(buftag1)) {
			looks_ok = 1;
		} else {
			size_ok = 0;
		}
	}

	if (!size_ok) {
		if (!besilent)
			mdb_printf("buffer %p (allocated) has a corrupt "
			    "redzone size encoding\n", addr);
		goto corrupt;
	}

	if (!looks_ok) {
		if (!besilent)
			mdb_printf("buffer %p (allocated) has a corrupt "
			    "redzone signature\n", addr);
		goto corrupt;
	}

	if ((cache_flags & KMF_LITE) != KMF_LITE) {
		if (verify_buftag(buftagp, KMEM_BUFTAG_ALLOC) == -1) {
			if (!besilent)
				mdb_printf("buffer %p (allocated) has a "
				    "corrupt bufctl pointer\n", addr);
			goto corrupt;
		}
	}

	return (WALK_NEXT);
corrupt:
	kmv->kmv_corruption++;
	return (WALK_NEXT);
}

/*ARGSUSED2*/
int
kmem_verify(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (flags & DCMD_ADDRSPEC) {
		int check_alloc = 0, check_free = 0;
		kmem_verify_t kmv;

		if (mdb_vread(&kmv.kmv_cache, sizeof (kmv.kmv_cache),
			    addr) == -1) {
			mdb_warn("couldn't read kmem_cache %p", addr);
			return (DCMD_ERR);
		}

		kmv.kmv_size = kmv.kmv_cache.cache_offset +
		    sizeof (kmem_buftag_t);
		kmv.kmv_buf = mdb_alloc(kmv.kmv_size, UM_SLEEP | UM_GC);
		kmv.kmv_corruption = 0;

		if ((kmv.kmv_cache.cache_flags & KMF_REDZONE)) {
			check_alloc = 1;
			if (kmv.kmv_cache.cache_flags & KMF_DEADBEEF)
				check_free = 1;
		} else {
			if (!(flags & DCMD_LOOP)) {
				mdb_warn("cache %p (%s) does not have "
				    "redzone checking enabled (%p)\n", addr,
				    kmv.kmv_cache.cache_name);
			}
			return (DCMD_ERR);
		}

		if (flags & DCMD_LOOP) {
			/*
			 * table mode, don't print out every corrupt buffer
			 */
			kmv.kmv_besilent = 1;
		} else {
			mdb_printf("Summary for cache '%s'\n",
			    kmv.kmv_cache.cache_name);
			mdb_inc_indent(2);
			kmv.kmv_besilent = 0;
		}

		if (check_alloc)
			(void) mdb_pwalk("kmem", verify_alloc, &kmv, addr);
		if (check_free)
			(void) mdb_pwalk("freemem", verify_free, &kmv, addr);

		if (flags & DCMD_LOOP) {
			if (kmv.kmv_corruption == 0) {
				mdb_printf("%-*s %?p clean\n",
				    KMEM_CACHE_NAMELEN,
				    kmv.kmv_cache.cache_name, addr);
			} else {
				char *s = "";	/* optional s in "buffer[s]" */
				if (kmv.kmv_corruption > 1)
					s = "s";

				mdb_printf("%-*s %?p %d corrupt buffer%s\n",
				    KMEM_CACHE_NAMELEN,
				    kmv.kmv_cache.cache_name, addr,
				    kmv.kmv_corruption, s);
			}
		} else {
			/*
			 * This is the more verbose mode, when the user has
			 * type addr::kmem_verify.  If the cache was clean,
			 * nothing will have yet been printed. So say something.
			 */
			if (kmv.kmv_corruption == 0)
				mdb_printf("clean\n");

			mdb_dec_indent(2);
		}
	} else {
		/*
		 * If the user didn't specify a cache to verify, we'll walk all
		 * kmem_cache's, specifying ourself as a callback for each...
		 * this is the equivalent of '::walk kmem_cache .::kmem_verify'
		 */
		mdb_printf("%<u>%-*s %-?s %-20s%</b>\n", KMEM_CACHE_NAMELEN,
		    "Cache Name", "Addr", "Cache Integrity");
		(void) (mdb_walk_dcmd("kmem_cache", "kmem_verify", 0, NULL));
	}

	return (DCMD_OK);
}

typedef struct vmem_node {
	struct vmem_node *vn_next;
	struct vmem_node *vn_parent;
	struct vmem_node *vn_sibling;
	struct vmem_node *vn_children;
	uintptr_t vn_addr;
	int vn_marked;
	vmem_t vn_vmem;
} vmem_node_t;

typedef struct vmem_walk {
	vmem_node_t *vw_root;
	vmem_node_t *vw_current;
} vmem_walk_t;

int
vmem_walk_init(mdb_walk_state_t *wsp)
{
	uintptr_t vaddr, paddr;
	vmem_node_t *head = NULL, *root = NULL, *current = NULL, *parent, *vp;
	vmem_walk_t *vw;

	if (mdb_readvar(&vaddr, "vmem_list") == -1) {
		mdb_warn("couldn't read 'vmem_list'");
		return (WALK_ERR);
	}

	while (vaddr != NULL) {
		vp = mdb_zalloc(sizeof (vmem_node_t), UM_SLEEP);
		vp->vn_addr = vaddr;
		vp->vn_next = head;
		head = vp;

		if (vaddr == wsp->walk_addr)
			current = vp;

		if (mdb_vread(&vp->vn_vmem, sizeof (vmem_t), vaddr) == -1) {
			mdb_warn("couldn't read vmem_t at %p", vaddr);
			goto err;
		}

		vaddr = (uintptr_t)vp->vn_vmem.vm_next;
	}

	for (vp = head; vp != NULL; vp = vp->vn_next) {

		if ((paddr = (uintptr_t)vp->vn_vmem.vm_source) == NULL) {
			vp->vn_sibling = root;
			root = vp;
			continue;
		}

		for (parent = head; parent != NULL; parent = parent->vn_next) {
			if (parent->vn_addr != paddr)
				continue;
			vp->vn_sibling = parent->vn_children;
			parent->vn_children = vp;
			vp->vn_parent = parent;
			break;
		}

		if (parent == NULL) {
			mdb_warn("couldn't find %p's parent (%p)\n",
			    vp->vn_addr, paddr);
			goto err;
		}
	}

	vw = mdb_zalloc(sizeof (vmem_walk_t), UM_SLEEP);
	vw->vw_root = root;

	if (current != NULL)
		vw->vw_current = current;
	else
		vw->vw_current = root;

	wsp->walk_data = vw;
	return (WALK_NEXT);
err:
	for (vp = head; head != NULL; vp = head) {
		head = vp->vn_next;
		mdb_free(vp, sizeof (vmem_node_t));
	}

	return (WALK_ERR);
}

int
vmem_walk_step(mdb_walk_state_t *wsp)
{
	vmem_walk_t *vw = wsp->walk_data;
	vmem_node_t *vp;
	int rval;

	if ((vp = vw->vw_current) == NULL)
		return (WALK_DONE);

	rval = wsp->walk_callback(vp->vn_addr, &vp->vn_vmem, wsp->walk_cbdata);

	if (vp->vn_children != NULL) {
		vw->vw_current = vp->vn_children;
		return (rval);
	}

	do {
		vw->vw_current = vp->vn_sibling;
		vp = vp->vn_parent;
	} while (vw->vw_current == NULL && vp != NULL);

	return (rval);
}

/*
 * The "vmem_postfix" walk walks the vmem arenas in post-fix order; all
 * children are visited before their parent.  We perform the postfix walk
 * iteratively (rather than recursively) to allow mdb to regain control
 * after each callback.
 */
int
vmem_postfix_walk_step(mdb_walk_state_t *wsp)
{
	vmem_walk_t *vw = wsp->walk_data;
	vmem_node_t *vp = vw->vw_current;
	int rval;

	/*
	 * If this node is marked, then we know that we have already visited
	 * all of its children.  If the node has any siblings, they need to
	 * be visited next; otherwise, we need to visit the parent.  Note
	 * that vp->vn_marked will only be zero on the first invocation of
	 * the step function.
	 */
	if (vp->vn_marked) {
		if (vp->vn_sibling != NULL)
			vp = vp->vn_sibling;
		else if (vp->vn_parent != NULL)
			vp = vp->vn_parent;
		else {
			/*
			 * We have neither a parent, nor a sibling, and we
			 * have already been visited; we're done.
			 */
			return (WALK_DONE);
		}
	}

	/*
	 * Before we visit this node, visit its children.
	 */
	while (vp->vn_children != NULL && !vp->vn_children->vn_marked)
		vp = vp->vn_children;

	vp->vn_marked = 1;
	vw->vw_current = vp;
	rval = wsp->walk_callback(vp->vn_addr, &vp->vn_vmem, wsp->walk_cbdata);

	return (rval);
}

void
vmem_walk_fini(mdb_walk_state_t *wsp)
{
	vmem_walk_t *vw = wsp->walk_data;
	vmem_node_t *root = vw->vw_root;
	int done;

	if (root == NULL)
		return;

	if ((vw->vw_root = root->vn_children) != NULL)
		vmem_walk_fini(wsp);

	vw->vw_root = root->vn_sibling;
	done = (root->vn_sibling == NULL && root->vn_parent == NULL);
	mdb_free(root, sizeof (vmem_node_t));

	if (done) {
		mdb_free(vw, sizeof (vmem_walk_t));
	} else {
		vmem_walk_fini(wsp);
	}
}

typedef struct vmem_seg_walk {
	uint8_t vsw_type;
	uintptr_t vsw_start;
	uintptr_t vsw_current;
} vmem_seg_walk_t;

/*ARGSUSED*/
int
vmem_seg_walk_common_init(mdb_walk_state_t *wsp, uint8_t type, char *name)
{
	vmem_seg_walk_t *vsw;

	if (wsp->walk_addr == NULL) {
		mdb_warn("vmem_%s does not support global walks\n", name);
		return (WALK_ERR);
	}

	wsp->walk_data = vsw = mdb_alloc(sizeof (vmem_seg_walk_t), UM_SLEEP);

	vsw->vsw_type = type;
	vsw->vsw_start = wsp->walk_addr + offsetof(vmem_t, vm_seg0);
	vsw->vsw_current = vsw->vsw_start;

	return (WALK_NEXT);
}

/*
 * vmem segments can't have type 0 (this should be added to vmem_impl.h).
 */
#define	VMEM_NONE	0

int
vmem_alloc_walk_init(mdb_walk_state_t *wsp)
{
	return (vmem_seg_walk_common_init(wsp, VMEM_ALLOC, "alloc"));
}

int
vmem_free_walk_init(mdb_walk_state_t *wsp)
{
	return (vmem_seg_walk_common_init(wsp, VMEM_FREE, "free"));
}

int
vmem_span_walk_init(mdb_walk_state_t *wsp)
{
	return (vmem_seg_walk_common_init(wsp, VMEM_SPAN, "span"));
}

int
vmem_seg_walk_init(mdb_walk_state_t *wsp)
{
	return (vmem_seg_walk_common_init(wsp, VMEM_NONE, "seg"));
}

int
vmem_seg_walk_step(mdb_walk_state_t *wsp)
{
	vmem_seg_t seg;
	vmem_seg_walk_t *vsw = wsp->walk_data;
	uintptr_t addr = vsw->vsw_current;
	int rval;

	if (mdb_vread(&seg, sizeof (seg), addr) == -1) {
		mdb_printf("couldn't read vmem_seg at %p", addr);
		return (WALK_ERR);
	}

	vsw->vsw_current = (uintptr_t)seg.vs_anext;
	if (vsw->vsw_type != VMEM_NONE && seg.vs_type != vsw->vsw_type) {
		rval = WALK_NEXT;
	} else {
		rval = wsp->walk_callback(addr, &seg, wsp->walk_cbdata);
	}

	if (vsw->vsw_current == vsw->vsw_start)
		return (WALK_DONE);

	return (rval);
}

void
vmem_seg_walk_fini(mdb_walk_state_t *wsp)
{
	vmem_seg_walk_t *vsw = wsp->walk_data;

	mdb_free(vsw, sizeof (vmem_seg_walk_t));
}

#define	VMEM_NAMEWIDTH	22

int
vmem(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	vmem_t v, parent;
	vmem_kstat_t *vkp = &v.vm_kstat;
	uintptr_t paddr;
	int ident = 0;
	char c[VMEM_NAMEWIDTH];

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("vmem", "vmem", argc, argv) == -1) {
			mdb_warn("can't walk vmem");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (DCMD_HDRSPEC(flags))
		mdb_printf("%-?s %-*s %10s %12s %9s %5s\n",
		    "ADDR", VMEM_NAMEWIDTH, "NAME", "INUSE",
		    "TOTAL", "SUCCEED", "FAIL");

	if (mdb_vread(&v, sizeof (v), addr) == -1) {
		mdb_warn("couldn't read vmem at %p", addr);
		return (DCMD_ERR);
	}

	for (paddr = (uintptr_t)v.vm_source; paddr != NULL; ident += 2) {
		if (mdb_vread(&parent, sizeof (parent), paddr) == -1) {
			mdb_warn("couldn't trace %p's ancestry", addr);
			ident = 0;
			break;
		}
		paddr = (uintptr_t)parent.vm_source;
	}

	(void) mdb_snprintf(c, VMEM_NAMEWIDTH, "%*s%s", ident, "", v.vm_name);

	mdb_printf("%0?p %-*s %10llu %12llu %9llu %5llu\n",
	    addr, VMEM_NAMEWIDTH, c,
	    vkp->vk_mem_inuse.value.ui64, vkp->vk_mem_total.value.ui64,
	    vkp->vk_alloc.value.ui64, vkp->vk_fail.value.ui64);

	return (DCMD_OK);
}

/*ARGSUSED*/
int
vmem_seg(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	vmem_seg_t vs;
	uintptr_t *stk = vs.vs_stack;
	uint8_t t;
	GElf_Sym sym;
	char c[MDB_SYM_NAMLEN];
	int i;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags))
		mdb_printf("%?s %4s %?s %?s %s\n", "ADDR", "TYPE",
		    "START", "END", "WHO");

	if (mdb_vread(&vs, sizeof (vs), addr) == -1) {
		mdb_warn("couldn't read vmem_seg at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%0?p %4s %0?p %0?p ", addr,
	    (t = vs.vs_type) == VMEM_ALLOC ? "ALLC" : t == VMEM_FREE ? "FREE" :
	    t == VMEM_SPAN ? "SPAN" : "????", vs.vs_start, vs.vs_end);

	if (vs.vs_depth == 0 || vs.vs_depth > VMEM_STACK_DEPTH)
		goto out;

	for (i = 0; i < vs.vs_depth; i++) {
		if (mdb_lookup_by_addr(stk[i], MDB_SYM_FUZZY,
		    c, sizeof (c), &sym) == -1)
			continue;
		if (strncmp(c, "vmem_", 5) == 0)
			continue;
		break;
	}

	if (stk[i] >= (uintptr_t)sym.st_value)
		mdb_printf(c);
	else
		mdb_printf("%p", stk[i]);
out:
	mdb_printf("\n");
	return (DCMD_OK);
}

/*ARGSUSED*/
static int
showbc(uintptr_t addr, const kmem_bufctl_audit_t *bcp, hrtime_t *newest)
{
	char name[KMEM_CACHE_NAMELEN + 1];
	hrtime_t delta;
	int i, depth;

	if (bcp->bc_timestamp == 0)
		return (WALK_DONE);

	if (*newest == 0)
		*newest = bcp->bc_timestamp;

	delta = *newest - bcp->bc_timestamp;
	depth = MIN(bcp->bc_depth, KMEM_STACK_DEPTH);

	if (mdb_readstr(name, sizeof (name), (uintptr_t)
	    &bcp->bc_cache->cache_name) <= 0)
		(void) mdb_snprintf(name, sizeof (name), "%a", bcp->bc_cache);

	mdb_printf("\nT-%lld.%09lld  addr=%p  %s\n",
	    delta / NANOSEC, delta % NANOSEC, bcp->bc_addr, name);

	for (i = 0; i < depth; i++)
		mdb_printf("\t %a\n", bcp->bc_stack[i]);

	return (WALK_NEXT);
}

int
kmalog(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *logname = "kmem_transaction_log";
	hrtime_t newest = 0;

	if ((flags & DCMD_ADDRSPEC) || argc > 1)
		return (DCMD_USAGE);

	if (argc > 0) {
		if (argv->a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);
		if (strcmp(argv->a_un.a_str, "fail") == 0)
			logname = "kmem_failure_log";
		else if (strcmp(argv->a_un.a_str, "slab") == 0)
			logname = "kmem_slab_log";
		else
			return (DCMD_USAGE);
	}

	if (mdb_readvar(&addr, logname) == -1) {
		mdb_warn("failed to read %s log header pointer");
		return (DCMD_ERR);
	}

	if (mdb_pwalk("kmem_log", (mdb_walk_cb_t)showbc, &newest, addr) == -1) {
		mdb_warn("failed to walk kmem log");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*
 * As the final lure for die-hard crash(1M) users, we provide ::kmausers here.
 * The first piece is a structure which we use to accumulate kmem_cache_t
 * addresses of interest.  The kmc_add is used as a callback for the kmem_cache
 * walker; we either add all caches, or ones named explicitly as arguments.
 */

typedef struct kmclist {
	const char *kmc_name;			/* Name to match (or NULL) */
	uintptr_t *kmc_caches;			/* List of kmem_cache_t addrs */
	int kmc_nelems;				/* Num entries in kmc_caches */
	int kmc_size;				/* Size of kmc_caches array */
} kmclist_t;

static int
kmc_add(uintptr_t addr, const kmem_cache_t *cp, kmclist_t *kmc)
{
	void *p;
	int s;

	if (kmc->kmc_name == NULL ||
	    strcmp(cp->cache_name, kmc->kmc_name) == 0) {
		/*
		 * If we have a match, grow our array (if necessary), and then
		 * add the virtual address of the matching cache to our list.
		 */
		if (kmc->kmc_nelems >= kmc->kmc_size) {
			s = kmc->kmc_size ? kmc->kmc_size * 2 : 256;
			p = mdb_alloc(sizeof (uintptr_t) * s, UM_SLEEP | UM_GC);

			bcopy(kmc->kmc_caches, p,
			    sizeof (uintptr_t) * kmc->kmc_size);

			kmc->kmc_caches = p;
			kmc->kmc_size = s;
		}

		kmc->kmc_caches[kmc->kmc_nelems++] = addr;
		return (kmc->kmc_name ? WALK_DONE : WALK_NEXT);
	}

	return (WALK_NEXT);
}

/*
 * The second piece of ::kmausers is a hash table of allocations.  Each
 * allocation owner is identified by its stack trace and data_size.  We then
 * track the total bytes of all such allocations, and the number of allocations
 * to report at the end.  Once we have a list of caches, we walk through the
 * allocated bufctls of each, and update our hash table accordingly.
 */

typedef struct kmowner {
	struct kmowner *kmo_head;		/* First hash elt in bucket */
	struct kmowner *kmo_next;		/* Next hash elt in chain */
	size_t kmo_signature;			/* Hash table signature */
	uint_t kmo_num;				/* Number of allocations */
	size_t kmo_data_size;			/* Size of each allocation */
	size_t kmo_total_size;			/* Total bytes of allocation */
	int kmo_depth;				/* Depth of stack trace */
	uintptr_t kmo_stack[KMEM_STACK_DEPTH];	/* Stack trace */
} kmowner_t;

typedef struct kmusers {
	const kmem_cache_t *kmu_cache;		/* Current kmem cache */
	kmowner_t *kmu_hash;			/* Hash table of owners */
	int kmu_nelems;				/* Number of entries in use */
	int kmu_size;				/* Total number of entries */
} kmusers_t;

static void
kmu_add(kmusers_t *kmu, const kmem_bufctl_audit_t *bcp,
    size_t size, size_t data_size)
{
	int i, depth = MIN(bcp->bc_depth, KMEM_STACK_DEPTH);
	size_t bucket, signature = data_size;
	kmowner_t *kmo, *kmoend;

	/*
	 * If the hash table is full, double its size and rehash everything.
	 */
	if (kmu->kmu_nelems >= kmu->kmu_size) {
		int s = kmu->kmu_size ? kmu->kmu_size * 2 : 1024;

		kmo = mdb_alloc(sizeof (kmowner_t) * s, UM_SLEEP | UM_GC);
		bcopy(kmu->kmu_hash, kmo, sizeof (kmowner_t) * kmu->kmu_size);
		kmu->kmu_hash = kmo;
		kmu->kmu_size = s;

		kmoend = kmu->kmu_hash + kmu->kmu_size;
		for (kmo = kmu->kmu_hash; kmo < kmoend; kmo++)
			kmo->kmo_head = NULL;

		kmoend = kmu->kmu_hash + kmu->kmu_nelems;
		for (kmo = kmu->kmu_hash; kmo < kmoend; kmo++) {
			bucket = kmo->kmo_signature & (kmu->kmu_size - 1);
			kmo->kmo_next = kmu->kmu_hash[bucket].kmo_head;
			kmu->kmu_hash[bucket].kmo_head = kmo;
		}
	}

	/*
	 * Finish computing the hash signature from the stack trace, and then
	 * see if the owner is in the hash table.  If so, update our stats.
	 */
	for (i = 0; i < depth; i++)
		signature += bcp->bc_stack[i];

	bucket = signature & (kmu->kmu_size - 1);

	for (kmo = kmu->kmu_hash[bucket].kmo_head; kmo; kmo = kmo->kmo_next) {
		if (kmo->kmo_signature == signature) {
			size_t difference = 0;

			difference |= kmo->kmo_data_size - data_size;
			difference |= kmo->kmo_depth - depth;

			for (i = 0; i < depth; i++) {
				difference |= kmo->kmo_stack[i] -
				    bcp->bc_stack[i];
			}

			if (difference == 0) {
				kmo->kmo_total_size += size;
				kmo->kmo_num++;
				return;
			}
		}
	}

	/*
	 * If the owner is not yet hashed, grab the next element and fill it
	 * in based on the allocation information.
	 */
	kmo = &kmu->kmu_hash[kmu->kmu_nelems++];
	kmo->kmo_next = kmu->kmu_hash[bucket].kmo_head;
	kmu->kmu_hash[bucket].kmo_head = kmo;

	kmo->kmo_signature = signature;
	kmo->kmo_num = 1;
	kmo->kmo_data_size = data_size;
	kmo->kmo_total_size = size;
	kmo->kmo_depth = depth;

	for (i = 0; i < depth; i++)
		kmo->kmo_stack[i] = bcp->bc_stack[i];
}

/*
 * When ::kmausers is invoked without the -f flag, we simply update our hash
 * table with the information from each allocated bufctl.
 */
/*ARGSUSED*/
static int
kmause1(uintptr_t addr, const kmem_bufctl_audit_t *bcp, kmusers_t *kmu)
{
	const kmem_cache_t *cp = kmu->kmu_cache;

	kmu_add(kmu, bcp, cp->cache_bufsize, cp->cache_bufsize);
	return (WALK_NEXT);
}

/*
 * When ::kmausers is invoked with the -f flag, we print out the information
 * for each bufctl as well as updating the hash table.
 */
static int
kmause2(uintptr_t addr, const kmem_bufctl_audit_t *bcp, kmusers_t *kmu)
{
	int i, depth = MIN(bcp->bc_depth, KMEM_STACK_DEPTH);
	const kmem_cache_t *cp = kmu->kmu_cache;

	mdb_printf("size %d, addr %p, thread %p, cache %s\n",
	    cp->cache_bufsize, addr, bcp->bc_thread, cp->cache_name);

	for (i = 0; i < depth; i++)
		mdb_printf("\t %a\n", bcp->bc_stack[i]);

	kmu_add(kmu, bcp, cp->cache_bufsize, cp->cache_bufsize);
	return (WALK_NEXT);
}

/*
 * We sort our results by allocation size before printing them.
 */
static int
kmownercmp(const void *lp, const void *rp)
{
	const kmowner_t *lhs = lp;
	const kmowner_t *rhs = rp;

	return (rhs->kmo_total_size - lhs->kmo_total_size);
}

/*
 * The main engine of ::kmausers is relatively straightforward: First we
 * accumulate our list of kmem_cache_t addresses into the kmclist_t. Next we
 * iterate over the allocated bufctls of each cache in the list.  Finally,
 * we sort and print our results.
 */
/*ARGSUSED*/
int
kmausers(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int mem_threshold = 8192;	/* Minimum # bytes for printing */
	int cnt_threshold = 100;	/* Minimum # blocks for printing */
	int audited_caches = 0;		/* Number of KMF_AUDIT caches found */
	int do_all_caches = 1;		/* Do all caches (no arguments) */
	int opt_e = FALSE;		/* Include "small" users */
	int opt_f = FALSE;		/* Print stack traces */

	mdb_walk_cb_t callback = (mdb_walk_cb_t)kmause1;
	kmowner_t *kmo, *kmoend;
	int i, oelems;

	kmclist_t kmc;
	kmusers_t kmu;

	if (flags & DCMD_ADDRSPEC)
		return (DCMD_USAGE);

	bzero(&kmc, sizeof (kmc));
	bzero(&kmu, sizeof (kmu));

	while ((i = mdb_getopts(argc, argv,
	    'e', MDB_OPT_SETBITS, TRUE, &opt_e,
	    'f', MDB_OPT_SETBITS, TRUE, &opt_f, NULL)) != argc) {

		argv += i;	/* skip past options we just processed */
		argc -= i;	/* adjust argc */

		if (argv->a_type != MDB_TYPE_STRING || *argv->a_un.a_str == '-')
			return (DCMD_USAGE);

		oelems = kmc.kmc_nelems;
		kmc.kmc_name = argv->a_un.a_str;
		(void) mdb_walk("kmem_cache", (mdb_walk_cb_t)kmc_add, &kmc);

		if (kmc.kmc_nelems == oelems) {
			mdb_warn("unknown kmem cache: %s\n", kmc.kmc_name);
			return (DCMD_ERR);
		}

		do_all_caches = 0;
		argv++;
		argc--;
	}

	if (opt_e)
		mem_threshold = cnt_threshold = 0;

	if (opt_f)
		callback = (mdb_walk_cb_t)kmause2;

	if (do_all_caches) {
		kmc.kmc_name = NULL; /* match all cache names */
		(void) mdb_walk("kmem_cache", (mdb_walk_cb_t)kmc_add, &kmc);
	}

	for (i = 0; i < kmc.kmc_nelems; i++) {
		uintptr_t cp = kmc.kmc_caches[i];
		kmem_cache_t c;

		if (mdb_vread(&c, sizeof (c), cp) == -1) {
			mdb_warn("failed to read cache at %p", cp);
			continue;
		}

		if (!(c.cache_flags & KMF_AUDIT)) {
			if (!do_all_caches) {
				mdb_warn("KMF_AUDIT is not enabled for %s\n",
				    c.cache_name);
			}
			continue;
		}

		kmu.kmu_cache = &c;
		(void) mdb_pwalk("bufctl", callback, &kmu, cp);
		audited_caches++;
	}

	if (audited_caches == 0 && do_all_caches) {
		mdb_warn("KMF_AUDIT is not enabled for any caches\n");
		return (DCMD_ERR);
	}

	qsort(kmu.kmu_hash, kmu.kmu_nelems, sizeof (kmowner_t), kmownercmp);
	kmoend = kmu.kmu_hash + kmu.kmu_nelems;

	for (kmo = kmu.kmu_hash; kmo < kmoend; kmo++) {
		if (kmo->kmo_total_size < mem_threshold &&
		    kmo->kmo_num < cnt_threshold)
			continue;
		mdb_printf("%lu bytes for %u allocations with data size %lu:\n",
		    kmo->kmo_total_size, kmo->kmo_num, kmo->kmo_data_size);
		for (i = 0; i < kmo->kmo_depth; i++)
			mdb_printf("\t %a\n", kmo->kmo_stack[i]);
	}

	return (DCMD_OK);
}
