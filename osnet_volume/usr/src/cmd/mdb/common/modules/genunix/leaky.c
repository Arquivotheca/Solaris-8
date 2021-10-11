/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)leaky.c	1.3	99/11/20 SMI"

#include <mdb/mdb_param.h>
#include <mdb/mdb_modapi.h>

#include <sys/fs/ufs_inode.h>
#include <sys/kmem_impl.h>
#include <sys/vmem_impl.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <vm/seg_vn.h>
#include <vm/as.h>
#include <vm/seg_map.h>
#include <alloca.h>
#include <stdio.h>
#include "leaky.h"

#define	LK_BUFCTLHSIZE	127
#define	LK_MARKED(b) ((uintptr_t)(b) & 1)
#define	LK_MARK(b) ((b) |= 1)

/*
 * Possible values for lk_state.
 */
#define	LK_CLEAN	0	/* No outstanding mdb_alloc()'s */
#define	LK_SWEEPING	1	/* Potentially some outstanding mdb_alloc()'s */
#define	LK_DONE		2	/* All mdb_alloc()'s complete */
#define	LK_CLEANING	3	/* Currently cleaning prior mdb_alloc()'s */

static volatile int lk_state;

typedef struct leak_mtab {
	uintptr_t lkm_base;
	uintptr_t lkm_limit;
	uintptr_t lkm_bufctl;
} leak_mtab_t;

typedef struct leak_state {
	uintptr_t *lks_buf;
	size_t lks_ndx;
	size_t lks_nptrs;
	struct leak_state *lks_next;
} leak_state_t;

typedef struct leak_beans {
	int lkb_dups;
	int lkb_follows;
	int lkb_misses;
	int lkb_dismissals;
	int lkb_deepest;
	size_t lkb_resident;
} leak_beans_t;

typedef struct leak_bufctl {
	struct leak_bufctl *lkb_hash_next;
	struct leak_bufctl *lkb_next;
	kmem_bufctl_audit_t lkb_bc;
	uintptr_t lkb_addr;
	int lkb_dups;
} leak_bufctl_t;

typedef struct leak_walk {
	int lkw_ndx;
	leak_bufctl_t *lkw_current;
	leak_bufctl_t *lkw_hash_next;
	kmem_bufctl_audit_t lkw_bc;
} leak_walk_t;

static leak_mtab_t *lk_mtab;
static leak_state_t *lk_free_state;
static int lk_nbuffers;
static leak_beans_t lk_beans;
static leak_bufctl_t *lk_bufctl[LK_BUFCTLHSIZE];
static leak_bufctl_t **lk_sorted = NULL;
static hrtime_t lk_begin;
static uint_t lk_verbose = FALSE;
static size_t lk_leaks;

static void
leaky_verbose(char *str, uint64_t stat)
{
	if (lk_verbose == FALSE)
		return;

	mdb_printf("findleaks: ");

	if (str == NULL) {
		mdb_printf("\n");
		return;
	}

	mdb_printf("%*s => %lld\n", 30, str, stat);
}

static void
leaky_verbose_perc(char *str, uint64_t stat, uint64_t total)
{
	double perc = (double)stat / (double)total * (double)100.0;
	char c[256];

	if (lk_verbose == FALSE)
		return;

	sprintf(c, "findleaks: %*s => %-13lld (%2.1f",
	    30, str, stat, perc);
	mdb_printf("%s%%)\n", c);
}

static void
leaky_verbose_begin()
{
	lk_begin = gethrtime();
}

static void
leaky_verbose_end()
{
	hrtime_t ts = gethrtime() - lk_begin;

	if (lk_verbose == FALSE)
		return;

	mdb_printf("findleaks: %*s => %lld seconds\n",
	    30, "elapsed wall time", ts / (hrtime_t)NANOSEC);
}

/*ARGSUSED*/
static int
leaky_mtab(uintptr_t addr, const kmem_bufctl_audit_t *bcp, leak_mtab_t **lmp)
{
	leak_mtab_t *lm = (*lmp)++;

	lm->lkm_base = (uintptr_t)bcp->bc_addr;
	lm->lkm_bufctl = addr;

	return (WALK_NEXT);
}

static int
leaky_seg(uintptr_t addr, const vmem_seg_t *seg, leak_mtab_t **lmp)
{
	leak_mtab_t *lm = (*lmp)++;

	lm->lkm_base = seg->vs_start;
	lm->lkm_limit = seg->vs_end;
	lm->lkm_bufctl = addr + 1;

	return (WALK_NEXT);
}

static int
leaky_vmem(uintptr_t addr, const vmem_t *vmem, leak_mtab_t **lmp)
{
	if (strcmp(vmem->vm_name, "kmem_oversize") != 0)
		return (WALK_NEXT);

	if (mdb_pwalk("vmem_alloc", (mdb_walk_cb_t)leaky_seg, lmp, addr) == -1)
		mdb_warn("can't walk vmem for kmem_oversize (%p)", addr);

	return (WALK_DONE);
}

static int
leaky_interested(const kmem_cache_t *c)
{
	int flags = KMF_AUDIT | KMF_DEADBEEF | KMF_BUFTAG;
	vmem_t vmem;

	if ((c->cache_flags & flags) != flags)
		return (0);

	if (mdb_vread(&vmem, sizeof (vmem), (uintptr_t)c->cache_arena) == -1) {
		mdb_warn("cannot read arena %p for cache '%s'",
		    (uintptr_t)c->cache_arena, c->cache_name);
		return (0);
	}

	/*
	 * If this cache isn't allocating from the kmem_default vmem arena,
	 * we're not interested.
	 */
	if (strcmp(vmem.vm_name, "kmem_default") != 0)
		return (0);

	return (1);
}

/*ARGSUSED*/
static int
leaky_estimate(uintptr_t addr, const kmem_cache_t *c, int *est)
{
	if (!leaky_interested(c))
		return (WALK_NEXT);

	*est += c->cache_buftotal;

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
leaky_cache(uintptr_t addr, const kmem_cache_t *c, leak_mtab_t **lmp)
{
	leak_mtab_t *lm = *lmp;

	if (!leaky_interested(c))
		return (WALK_NEXT);

	if (mdb_pwalk("bufctl", (mdb_walk_cb_t)leaky_mtab, lmp, addr) == -1) {
		mdb_warn("can't walk kmem for cache %p (%s)", addr,
		    c->cache_name);
		return (WALK_DONE);
	}

	for (; lm < *lmp; lm++)
		lm->lkm_limit = lm->lkm_base + c->cache_bufsize;

	return (WALK_NEXT);
}

static int
leaky_mtabcmp(const void *l, const void *r)
{
	const leak_mtab_t *lhs = (const leak_mtab_t *)l;
	const leak_mtab_t *rhs = (const leak_mtab_t *)r;

	if (lhs->lkm_base < rhs->lkm_base)
		return (-1);
	if (lhs->lkm_base > rhs->lkm_base)
		return (1);

	return (0);
}

static ssize_t
leaky_search(uintptr_t addr)
{
	ssize_t left = 0, right = lk_nbuffers - 1, guess;

	while (right >= left) {
		guess = (right + left) >> 1;

		if (addr < lk_mtab[guess].lkm_base) {
			right = guess - 1;
			continue;
		}

		if (addr > lk_mtab[guess].lkm_limit) {
			left = guess + 1;
			continue;
		}

		return (guess);
	}

	return (-1);
}

static void
leaky_grep(uintptr_t addr, size_t size)
{
	uintptr_t *buf;
	leak_state_t *state = NULL, *new_state;
	size_t nptrs, ndx;
	uintptr_t min = lk_mtab[0].lkm_base;
	uintptr_t max = lk_mtab[lk_nbuffers - 1].lkm_limit;
	int dups = 0, misses = 0, depth = 0, deepest = 0;
	int follows = 0, dismissals = 0;
	size_t resident = 0, max_resident = 0;
	ssize_t mtab_ndx, rval;
	int mask = sizeof (uintptr_t) - 1;

	if (addr == NULL || size == 0)
		return;
push:
	/*
	 * If our address isn't pointer-aligned, we need to align it and
	 * whack the size appropriately.
	 */
	if (addr & mask) {
		size -= addr & mask;
		addr += (mask + 1) - (addr & mask);
	}

	nptrs = size / sizeof (uintptr_t);
	buf = mdb_alloc(size, UM_SLEEP);
	resident += size;
	ndx = 0;

	if (resident > max_resident)
		max_resident = resident;

	if ((rval = mdb_vread(buf, size, addr)) != size) {
		mdb_warn("couldn't read ptr at %p (size %ld); rval is %d",
		    addr, size, rval);
		goto out;
	}
pop:
	for (; ndx < nptrs; ndx++) {
		uintptr_t ptr = buf[ndx];

		if (ptr < min || ptr > max) {
			dismissals++;
			continue;
		}

		if ((mtab_ndx = leaky_search(ptr)) == -1) {
			misses++;
			continue;
		}

		/*
		 * If this buffer is already marked, then we have found it by
		 * an alternate path.  Update the appropriate counter.
		 */
		if (LK_MARKED(addr = lk_mtab[mtab_ndx].lkm_base)) {
			dups++;
			continue;
		}

		/*
		 * We have found a pointer to an unmarked buffer.  We need
		 * to mark it, and descend into the region (which requires
		 * pushing our current state).  To minimize overhead, we
		 * don't actually make a recursive call...
		 */
		follows++;
		if (++depth > deepest)
			deepest = depth;

		size = lk_mtab[mtab_ndx].lkm_limit - addr;
		LK_MARK(lk_mtab[mtab_ndx].lkm_base);

		if (lk_free_state != NULL) {
			new_state = lk_free_state;
			lk_free_state = lk_free_state->lks_next;
		} else {
			new_state = mdb_zalloc(sizeof (leak_state_t), UM_SLEEP);
		}

		new_state->lks_next = state;
		new_state->lks_buf = buf;
		new_state->lks_ndx = ndx + 1;
		new_state->lks_nptrs = nptrs;
		state = new_state;
		goto push;
	}

	/*
	 * If we're here, then we have completed this region.  We need to
	 * free our buffer, and update our "resident" counter accordingly.
	 */
	mdb_free(buf, size);
	resident -= size;

	/*
	 * If we have pushed state, we need to pop it.
	 */
	if (state != NULL) {
		buf = state->lks_buf;
		ndx = state->lks_ndx;
		nptrs = state->lks_nptrs;
		size = nptrs * sizeof (uintptr_t);

		new_state = state->lks_next;
		state->lks_next = lk_free_state;
		lk_free_state = state;
		state = new_state;
		depth--;

		goto pop;
	}
out:
	lk_beans.lkb_dups += dups;
	lk_beans.lkb_dismissals += dismissals;
	lk_beans.lkb_misses += misses;
	lk_beans.lkb_follows += follows;

	if (deepest > lk_beans.lkb_deepest)
		lk_beans.lkb_deepest = deepest;

	if (max_resident > lk_beans.lkb_resident)
		lk_beans.lkb_resident = max_resident;
}

static void
leaky_grep_sparse(uintptr_t addr, size_t size)
{
	uintptr_t lim = addr + size;
	char c;

	for (; addr < lim; addr = (addr & PAGEMASK) + PAGESIZE) {
		if (mdb_vread(&c, sizeof (char), addr) == -1)
			continue;

		leaky_grep(addr, MIN((addr & PAGEMASK) + PAGESIZE, lim) - addr);
	}
}

static void
leaky_mark(uintptr_t addr)
{
	ssize_t ndx;

	if ((ndx = leaky_search(addr)) != -1)
		LK_MARK(lk_mtab[ndx].lkm_base);
}

/*ARGSUSED*/
static int
leaky_modctl(uintptr_t addr, const struct modctl *m, int *ignored)
{
	struct module mod;
	char name[MODMAXNAMELEN];

	if (m->mod_mp == NULL)
		return (WALK_NEXT);

	if (mdb_vread(&mod, sizeof (mod), (uintptr_t)m->mod_mp) == -1) {
		mdb_warn("couldn't read modctl %p's module", addr);
		return (WALK_NEXT);
	}

	if (mdb_readstr(name, sizeof (name), (uintptr_t)m->mod_modname) == -1)
		(void) mdb_snprintf(name, sizeof (name), "0x%p", addr);

	if (strcmp(name, "unix") == 0) {
		/*
		 * If this is "unix", we'll slurp from mod.data all the way
		 * up to econtig.
		 */
		uintptr_t econtig, base = (uintptr_t)mod.data;

		if (mdb_readvar(&econtig, "econtig") == -1) {
			mdb_warn("failed to find 'econtig'");
		} else {
			leaky_grep_sparse(base, econtig - base);
			return (WALK_NEXT);
		}
	}

	leaky_grep((uintptr_t)mod.data, mod.data_size);
	leaky_grep((uintptr_t)mod.bss, mod.bss_size);

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
leaky_kludge(uintptr_t addr, const kmem_cache_t *c, void *i)
{
	/*
	 * Currently, the hash tables for the kmem caches don't come
	 * out of the kmem_internal arena (they are instead kmem_zalloc()'d).
	 * This will be fixed, but for now, we'll scamper through the
	 * caches, marking the hash table pointers.
	 */
	leaky_mark((uintptr_t)c->cache_hash_table);

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
leaky_another_kludge(uintptr_t addr, const vmem_t *v, void *i)
{
	/*
	 * Currently, the hash tables for the vmem arenas don't come
	 * out of the vmem_vmem arena (they are instead kmem_zalloc()'d).
	 * This will be fixed, but for now, we'll scamper through the
	 * vmem arenas, marked the hash table pointers.
	 *
	 * Are you getting the feeling that kmem and vmem were cut (and
	 * pasted) from the same cloth?
	 */
	leaky_mark((uintptr_t)v->vm_hash_table);

	return (WALK_NEXT);
}

static int
leaky_thread(uintptr_t addr, const kthread_t *t, unsigned long *pagesize)
{
	uintptr_t size, base = (uintptr_t)t->t_stkbase;
	uintptr_t stk = (uintptr_t)t->t_stk;

	/*
	 * If this thread isn't in memory, we can't look at its stack.  This
	 * may result in false positives, so we print a warning.
	 */
	if (!(t->t_schedflag & TS_LOAD)) {
		mdb_printf("findleaks: thread %p's stack swapped out; "
		    "false positives possible\n", addr);
		return (WALK_NEXT);
	}

	if (t->t_state != TS_FREE)
		leaky_grep(base, stk - base);

	/*
	 * There is always gunk hanging out between t_stk and the page
	 * boundary.  If this thread structure wasn't kmem allocated,
	 * this will include the thread structure itself.  If the thread
	 * _is_ kmem allocated, we'll be able to get to it via allthreads.
	 */
	size = *pagesize - (stk & (*pagesize - 1));

	leaky_grep(stk, size);

	return (WALK_NEXT);
}

void
leaky_add(uintptr_t addr)
{
	leak_bufctl_t *nlkb, *lkb;
	kmem_bufctl_audit_t *bcp;
	uintptr_t total = 0;
	size_t ndx;
	int i;

	nlkb = mdb_zalloc(sizeof (leak_bufctl_t), UM_SLEEP);
	nlkb->lkb_addr = addr;
	bcp = &nlkb->lkb_bc;

	if (mdb_vread(bcp, sizeof (kmem_bufctl_audit_t), addr) == -1) {
		mdb_warn("couldn't read leaked bufctl at addr %p", addr);
		mdb_free(nlkb, sizeof (leak_bufctl_t));
		return;
	}

	for (i = 0; i < bcp->bc_depth; i++)
		total += bcp->bc_stack[i];

	ndx = total % LK_BUFCTLHSIZE;

	if ((lkb = lk_bufctl[ndx]) == NULL) {
		lk_leaks++;
		lk_bufctl[ndx] = nlkb;
		return;
	}

	for (;;) {
		if (lkb->lkb_bc.bc_depth != bcp->bc_depth)
			goto no_match;

		for (i = 0; i < bcp->bc_depth; i++)
			if (lkb->lkb_bc.bc_stack[i] != bcp->bc_stack[i])
				goto no_match;

		/*
		 * If we're here, we've found a bufctl with a matching
		 * stack; link it in.  Note that the volatile cast assures
		 * that these stores will occur in program order (thus
		 * assuring that we can take an interrupt and still be in
		 * a sane enough state to throw away the data structure later,
		 * in leaky_cleanup()).
		 */
		((volatile leak_bufctl_t *)nlkb)->lkb_next = lkb->lkb_next;
		((volatile leak_bufctl_t *)lkb)->lkb_next = nlkb;
		lkb->lkb_dups++;
		break;

no_match:
		if (lkb->lkb_hash_next == NULL) {
			lkb->lkb_hash_next = nlkb;
			lk_leaks++;
			break;
		}
		lkb = lkb->lkb_hash_next;
	}
}

void
leaky_add_oversize(uintptr_t addr)
{
	mdb_printf("findleaks: Oversize leak at seg %p!\n", addr);
}

void
leaky_caller(const leak_bufctl_t *lkb, char *buf, size_t *offset)
{
	int i;
	const kmem_bufctl_audit_t *bcp = &lkb->lkb_bc;
	GElf_Sym sym;

	for (i = 0; i < bcp->bc_depth; i++) {
		if (mdb_lookup_by_addr(bcp->bc_stack[i],
		    MDB_SYM_FUZZY, buf, MDB_SYM_NAMLEN, &sym) == -1)
			continue;
		if (strncmp(buf, "kmem_", 5) == 0)
			continue;
		*offset = bcp->bc_stack[i] - (uintptr_t)sym.st_value;

		return;
	}

	/*
	 * We're only here if the entire call chain begins with "kmem_";
	 * this shouldn't happen, but we'll just use the top caller.
	 */
	*offset = 0;
}

int
leaky_ctlcmp(const void *l, const void *r)
{
	const leak_bufctl_t *lhs = *((const leak_bufctl_t **)l);
	const leak_bufctl_t *rhs = *((const leak_bufctl_t **)r);
	char lbuf[MDB_SYM_NAMLEN], rbuf[MDB_SYM_NAMLEN];
	size_t loffs, roffs;
	int rval;

	leaky_caller(lhs, lbuf, &loffs);
	leaky_caller(rhs, rbuf, &roffs);

	if (rval = strcmp(lbuf, rbuf))
		return (rval);

	if (loffs < roffs)
		return (-1);

	if (loffs > roffs)
		return (1);

	return (0);
}

void
leaky_sort()
{
	int i, j = 0;
	leak_bufctl_t *lkb;

	if (lk_leaks == 0)
		return;

	lk_sorted = mdb_alloc(lk_leaks * sizeof (leak_bufctl_t *), UM_SLEEP);

	for (i = 0; i < LK_BUFCTLHSIZE; i++) {
		for (lkb = lk_bufctl[i]; lkb != NULL; lkb = lkb->lkb_hash_next)
			lk_sorted[j++] = lkb;
	}

	qsort(lk_sorted, lk_leaks, sizeof (leak_bufctl_t *), leaky_ctlcmp);
}

void
leaky_cleanup()
{
	int i;
	leak_bufctl_t *lkb, *l, *next;
	leak_state_t *state;

	/*
	 * We'll start by cleaning up any garbage free states.  If we're
	 * interrupted in this loop, at worst a state buffer is leaked.
	 */
	while ((state = lk_free_state) != NULL) {
		lk_free_state = state->lks_next;
		mdb_free(state, sizeof (leak_state_t));
	}

	bzero(&lk_beans, sizeof (lk_beans));

	if (lk_state == LK_CLEANING) {
		mdb_warn("interrupted during ::findleaks cleanup; some mdb "
		    " memory will be leaked\n");

		for (i = 0; i < LK_BUFCTLHSIZE; i++)
			lk_bufctl[i] = NULL;

		lk_sorted = NULL;
		lk_state = LK_CLEAN;
		return;
	}

	if (lk_state != LK_SWEEPING)
		return;

	lk_state = LK_CLEANING;

	if (lk_sorted != NULL) {
		mdb_free(lk_sorted, lk_leaks * sizeof (leak_bufctl_t *));
		lk_sorted = NULL;
	}

	for (i = 0; i < LK_BUFCTLHSIZE; i++) {
		for (lkb = lk_bufctl[i]; lkb != NULL; lkb = next) {
			for (l = lkb->lkb_next; l != NULL; l = next) {
				next = l->lkb_next;
				mdb_free(l, sizeof (leak_bufctl_t));
			}
			next = lkb->lkb_hash_next;
			mdb_free(lkb, sizeof (leak_bufctl_t));
		}
		lk_bufctl[i] = NULL;
	}

	lk_state = LK_CLEAN;
}

int
leaky_filter(kmem_bufctl_audit_t *bcp, uintptr_t filter)
{
	int i;
	GElf_Sym sym;
	char c;

	if (filter == NULL)
		return (1);

	for (i = 0; i < bcp->bc_depth; i++) {
		if (bcp->bc_stack[i] == filter)
			return (1);

		if (mdb_lookup_by_addr(bcp->bc_stack[i], MDB_SYM_FUZZY,
		    &c, sizeof (c), &sym) == -1)
			continue;

		if ((uintptr_t)sym.st_value == filter)
			return (1);
	}

	return (0);
}

void
leaky_dump(uintptr_t filter)
{
	int i;
	size_t ttl = 0, bytes = 0, offset;
	char c[MDB_SYM_NAMLEN];

	mdb_printf("%-?s %7s %?s %s\n", "CACHE", "LEAKED", "BUFCTL", "CALLER");

	for (i = 0; i < lk_leaks; i++) {
		leak_bufctl_t *lkb = lk_sorted[i], *l, *old = lkb;
		kmem_bufctl_audit_t *bcp = &lkb->lkb_bc;
		uintptr_t caddr = (uintptr_t)lkb->lkb_bc.bc_cache;
		kmem_cache_t cache;

		if (mdb_vread(&cache, sizeof (cache), caddr) == -1) {
			/*
			 * This _really_ shouldn't happen; we shouldn't
			 * have been able to get this far if this
			 * cache wasn't readable.
			 */
			mdb_warn("can't read cache %p for leaked "
			    "bufctl %p", caddr, lkb->lkb_addr);
			continue;
		}

		for (l = lkb; l != NULL; l = l->lkb_next) {
			if (l->lkb_bc.bc_timestamp < old->lkb_bc.bc_timestamp)
				old = l;
		}

		if (!leaky_filter(bcp, filter))
			continue;

		leaky_caller(lkb, c, &offset);

		mdb_printf("%0?p %7d %0?p %s+0x%p\n", lkb->lkb_bc.bc_cache,
		    lkb->lkb_dups + 1, old->lkb_addr, c, offset);

		ttl += lkb->lkb_dups + 1;
		bytes += (lkb->lkb_dups + 1) * cache.cache_bufsize;
	}

	for (i = 0; i < 70; i++)
		mdb_printf("-");
	mdb_printf("\n");

	mdb_printf("%?s %7ld buffer%s, %ld byte%s\n", "Total", ttl,
	    ttl == 1 ? "" : "s", bytes, bytes == 1 ? "" : "s");
}

void
leaky_platform()
{
#ifdef __i386
	uintptr_t kernelbase, kernelheap;

	/*
	 * No one is pretending this is pretty.  On x86, we need to slurp
	 * the page structures sitting between kernelbase and kernelheap.
	 */
	if (mdb_readvar(&kernelbase, "valloc_base") == -1) {
		mdb_warn("can't read 'valloc_base'");
		return;
	}

	if (mdb_readvar(&kernelheap, "kernelheap") == -1) {
		mdb_warn("can't read 'kernelheap'");
		return;
	}

	leaky_grep_sparse(kernelbase, kernelheap - kernelbase);
#endif
}

#define	LK_REPORT_BEAN(x) leaky_verbose_perc(#x, lk_beans.lkb_##x, total);

/*ARGSUSED*/
int
findleaks(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	int est = 0, i;
	leak_mtab_t *lmp;
	unsigned long ps;
	int total;
	uintptr_t filter = NULL;

	if (!mdb_prop_postmortem) {
		mdb_printf("findleaks: can only be run on a system "
		    "dump; see dumpadm(1M)\n");
		return (DCMD_OK);
	}

	if (flags & DCMD_ADDRSPEC)
		filter = addr;

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &lk_verbose, NULL) != argc)
		return (DCMD_USAGE);

	/*
	 * Clean any previous ::findleaks.
	 */
	leaky_cleanup();

	if (lk_state == LK_DONE)
		goto dump;

	leaky_verbose_begin();

	if (mdb_readvar(&ps, "_pagesize") == -1) {
		mdb_warn("couldn't read '_pagesize'");
		return (DCMD_ERR);
	}

	if (mdb_walk("kmem_cache", (mdb_walk_cb_t)leaky_estimate, &est) == -1) {
		mdb_warn("couldn't walk 'kmem_cache'");
		return (DCMD_ERR);
	}

	if (est == 0) {
		mdb_printf("findleaks: post-mortem leak detection requires "
		    "kmem_flags to be 0xf\n");
		return (DCMD_OK);
	}

	leaky_verbose("maximum buffers", est);

	/*
	 * Now we have an upper bound on the number of buffers.  Allocate
	 * our kmem array.
	 */
	lk_mtab = mdb_zalloc(est * sizeof (leak_mtab_t), UM_SLEEP | UM_GC);
	lmp = lk_mtab;

	if (mdb_walk("vmem", (mdb_walk_cb_t)leaky_vmem, &lmp) == -1) {
		mdb_warn("couldn't walk 'vmem'");
		return (DCMD_ERR);
	}

	if (mdb_walk("kmem_cache", (mdb_walk_cb_t)leaky_cache, &lmp) == -1) {
		mdb_warn("couldn't walk 'kmem_cache'");
		return (DCMD_ERR);
	}

	lk_nbuffers = lmp - lk_mtab;

	qsort(lk_mtab, lk_nbuffers, sizeof (leak_mtab_t), leaky_mtabcmp);

	leaky_verbose("actual buffers", lk_nbuffers);

	if (mdb_walk("kmem_cache", (mdb_walk_cb_t)leaky_kludge, NULL) == -1) {
		mdb_warn("couldn't walk 'kmem_cache'");
		return (DCMD_ERR);
	}

	if (mdb_walk("vmem", (mdb_walk_cb_t)leaky_another_kludge, NULL) == -1) {
		mdb_warn("couldn't walk 'vmem'");
		return (DCMD_ERR);
	}

	if (mdb_walk("modctl", (mdb_walk_cb_t)leaky_modctl, NULL) == -1) {
		mdb_warn("couldn't walk 'modctl'");
		return (DCMD_ERR);
	}

	if (mdb_walk("thread", (mdb_walk_cb_t)leaky_thread, &ps) == -1) {
		mdb_warn("couldn't walk 'thread'");
		return (DCMD_ERR);
	}

	if (mdb_walk("deathrow", (mdb_walk_cb_t)leaky_thread, &ps) == -1) {
		mdb_warn("couldn't walk 'deathrow'");
		return (DCMD_ERR);
	}

	leaky_platform();

	for (i = 0; i < lk_nbuffers; i++) {
		if (lk_mtab[i].lkm_base & 1)
			continue;
		if (lk_mtab[i].lkm_bufctl & 1)
			leaky_add_oversize(lk_mtab[i].lkm_bufctl - 1);
		else
			leaky_add(lk_mtab[i].lkm_bufctl);
	}

	total = lk_beans.lkb_dismissals + lk_beans.lkb_misses +
	    lk_beans.lkb_dups + lk_beans.lkb_follows;

	leaky_verbose(NULL, 0);
	leaky_verbose("potential pointers", total);
	LK_REPORT_BEAN(dismissals);
	LK_REPORT_BEAN(misses);
	LK_REPORT_BEAN(dups);
	LK_REPORT_BEAN(follows);

	leaky_verbose(NULL, 0);
	leaky_verbose("maximum graph depth", lk_beans.lkb_deepest);
	leaky_verbose("maximum bytes resident", lk_beans.lkb_resident);
	leaky_verbose_end();
	leaky_verbose(NULL, 0);

	lk_state = LK_DONE;
	leaky_sort();
dump:
	leaky_dump(filter);
	leaky_cleanup();

	return (DCMD_OK);
}

int
leaky_walk_init(mdb_walk_state_t *wsp)
{
	leak_walk_t *lw;
	uintptr_t total = 0;
	int i;

	if (lk_state != LK_DONE) {
		mdb_warn("::findleaks must be run %sbefore leaks can be"
		    " walked\n", lk_state != LK_CLEAN ? "to completion " : "");
		return (WALK_ERR);
	}

	wsp->walk_data = lw = mdb_zalloc(sizeof (leak_walk_t), UM_SLEEP);

	if (wsp->walk_addr == NULL)
		return (WALK_NEXT);

	if (mdb_vread(&lw->lkw_bc, sizeof (lw->lkw_bc), wsp->walk_addr) == -1) {
		mdb_warn("can't read bufctl at %p", wsp->walk_addr);
		mdb_free(lw, sizeof (lw));
	}

	for (i = 0; i < lw->lkw_bc.bc_depth; i++)
		total += lw->lkw_bc.bc_stack[i];

	lw->lkw_ndx = total % LK_BUFCTLHSIZE;
	lw->lkw_current = lk_bufctl[lw->lkw_ndx];

	if (lw->lkw_current != NULL)
		lw->lkw_hash_next = lw->lkw_current->lkb_hash_next;

	return (WALK_NEXT);
}

int
leaky_walk_step(mdb_walk_state_t *wsp)
{
	leak_walk_t *lw = wsp->walk_data;
	leak_bufctl_t *lk;
	kmem_bufctl_audit_t *bcp = &lw->lkw_bc;
	int i;

	if ((lk = lw->lkw_current) == NULL) {

		if (lw->lkw_hash_next != NULL) {
			lk = lw->lkw_hash_next;
		} else {
			if (wsp->walk_addr)
				return (WALK_DONE);

			while (lk == NULL && lw->lkw_ndx < LK_BUFCTLHSIZE)
				lk = lk_bufctl[lw->lkw_ndx++];

			if (lw->lkw_ndx == LK_BUFCTLHSIZE)
				return (WALK_DONE);
		}

		lw->lkw_hash_next = lk->lkb_hash_next;
	}

	lw->lkw_current = lk->lkb_next;

	if (wsp->walk_addr != NULL) {
		for (i = 0; i < bcp->bc_depth; i++) {
			if (lk->lkb_bc.bc_stack[i] != bcp->bc_stack[i])
				return (WALK_NEXT);
		}
	}

	return (wsp->walk_callback(lk->lkb_addr,
	    &lk->lkb_bc, wsp->walk_cbdata));
}

void
leaky_walk_fini(mdb_walk_state_t *wsp)
{
	leak_walk_t *lw = wsp->walk_data;

	mdb_free(lw, sizeof (leak_walk_t));
}

int
leaky_buf_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_layered_walk("leak", wsp) == -1) {
		mdb_warn("couldn't walk 'leak'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
leaky_buf_walk_step(mdb_walk_state_t *wsp)
{
	kmem_bufctl_audit_t *bc = (kmem_bufctl_audit_t *)wsp->walk_layer;
	uintptr_t addr = (uintptr_t)bc->bc_addr;

	return (wsp->walk_callback(addr, NULL, wsp->walk_cbdata));
}
