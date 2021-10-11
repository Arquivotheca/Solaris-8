/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kma.c	1.12	99/09/13 SMI"

/*
 * This file contains code for the crash function kmastat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include <sys/vmem_impl.h>
#include <sys/kstat.h>
#include <sys/elf.h>
#include "crash.h"

static void	kmainit(), prkmastat();

static void kmause(void *kaddr, void *buf,
		size_t size, kmem_bufctl_audit_t *bcp);

static Sym *kmem_null_cache_sym, *vmem_list_sym;

/* get arguments for kmastat function */
int
getkmastat()
{
	int c;

	kmainit();
	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	(void) redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	prkmastat();
	return (0);
}

typedef struct datafmt {
	char	*hdr1;
	char	*hdr2;
	char	*fmt;
	int	width;
} datafmt_t;

static char *dashes = "----------------------------------------------------";

static datafmt_t kmemfmt[] = {
	{	"cache",	"name",		"%-25s ",	-25,	},
	{	"buf",		"size",		"%6u ",		6,	},
	{	"buf",		"in use",	"%6u ",		6,	},
	{	"buf",		"total",	"%6u ",		6,	},
	{	"memory",	"in use",	"%9u ",		9,	},
	{	"alloc",	"succeed",	"%9u ",		9,	},
	{	"alloc",	"fail",		"%5u ",		5,	},
	{	NULL,		NULL,		NULL,		0,	}
};

static datafmt_t vmemfmt[] = {
	{	"arena",	"name",		"%-25s ",	-25,	},
	{	"memory",	"in use",	"%9llu ",	9,	},
	{	"memory",	"total",	"%10llu ",	10,	},
	{	"memory",	"import",	"%9llu ",	9,	},
	{	"alloc",	"succeed",	"%9llu ",	9,	},
	{	"alloc",	"fail",		"%5llu ",	5,	},
	{	NULL,		NULL,		NULL,		0,	}
};

#define	ABS(x)		((x) < 0 ? -(x) : (x))
#define	DENOM(x)	((int64_t)(x) <= 0 ? 1 : (x))

typedef struct vmem_dir {
	struct vmem_dir	*vd_parent;
	struct vmem_dir	*vd_sibling;
	struct vmem_dir	*vd_children;
	vmem_t		vd_arena;
	int		vd_depth;
	int		vd_alloc;
	int		vd_alloc_fail;
	int		vd_meminuse;
} vmem_dir_t;

static vmem_dir_t vmem_root;

static void
vmem_dirwalk(vmem_dir_t *root, void (*func)(vmem_dir_t *))
{
	vmem_dir_t *vdp;

	func(root);
	for (vdp = root->vd_children; vdp != NULL; vdp = vdp->vd_sibling)
		vmem_dirwalk(vdp, func);
}

static vmem_dir_t *
vmem_lookup(vmem_dir_t *root, uint32_t id)
{
	vmem_dir_t *vdp, *targ;

	if (root->vd_arena.vm_id == id)
		return (root);
	for (vdp = root->vd_children; vdp != NULL; vdp = vdp->vd_sibling)
		if ((targ = vmem_lookup(vdp, id)) != NULL)
			return (targ);
	return (NULL);
}

static void
vmem_insert(vmem_t *vmp)
{
	vmem_dir_t *vdp, **vdpp, *parent, *targ;

	targ = malloc(sizeof (vmem_dir_t));

	bzero(targ, sizeof (vmem_dir_t));
	bcopy(vmp, &targ->vd_arena, sizeof (vmem_t));
	parent = vmem_lookup(&vmem_root, vmp->vm_kstat.vk_source_id.value.ui32);
	vdpp = &parent->vd_children;
	while ((vdp = *vdpp) != NULL)
		vdpp = &vdp->vd_sibling;
	*vdpp = targ;
	targ->vd_parent = parent;
	targ->vd_depth = parent->vd_depth + 1;
}

static void
show_vmem(vmem_dir_t *vdp)
{
	datafmt_t *dfp = vmemfmt;
	vmem_kstat_t *vkp = &vdp->vd_arena.vm_kstat;
	char name[50];

	if (vdp == &vmem_root)
		return;

	memset(name, ' ', sizeof (name));
	sprintf(name + 4 * (vdp->vd_depth - 1), vdp->vd_arena.vm_name);

	(void) fprintf(fp, (dfp++)->fmt, name);
	(void) fprintf(fp, (dfp++)->fmt, vkp->vk_mem_inuse.value.ui64);
	(void) fprintf(fp, (dfp++)->fmt, vkp->vk_mem_total.value.ui64);
	(void) fprintf(fp, (dfp++)->fmt, vkp->vk_mem_import.value.ui64);
	(void) fprintf(fp, (dfp++)->fmt, vkp->vk_alloc.value.ui64);
	(void) fprintf(fp, (dfp++)->fmt, vkp->vk_fail.value.ui64);

	(void) fprintf(fp, "\n");
}

static void
show_kmem(kmem_cache_t *cp, kmem_cache_stat_t *kcs)
{
	datafmt_t *dfp = kmemfmt;
	vmem_dir_t *vdp;
	vmem_t vm;
	size_t meminuse;

	kvm_read(kd, (intptr_t)cp->cache_arena, (char *)&vm, sizeof (vm));
	vdp = vmem_lookup(&vmem_root, vm.vm_id);

	meminuse = (kcs->kcs_slab_create - kcs->kcs_slab_destroy) *
	    kcs->kcs_slab_size;

	vdp->vd_meminuse += meminuse;
	vdp->vd_alloc += kcs->kcs_alloc;
	vdp->vd_alloc_fail += kcs->kcs_alloc_fail;

	(void) fprintf(fp, (dfp++)->fmt, cp->cache_name);
	(void) fprintf(fp, (dfp++)->fmt, kcs->kcs_buf_size);
	(void) fprintf(fp, (dfp++)->fmt, kcs->kcs_buf_total -
	    kcs->kcs_buf_avail);
	(void) fprintf(fp, (dfp++)->fmt, kcs->kcs_buf_total);
	(void) fprintf(fp, (dfp++)->fmt, meminuse);
	(void) fprintf(fp, (dfp++)->fmt, kcs->kcs_alloc);
	(void) fprintf(fp, (dfp++)->fmt, kcs->kcs_alloc_fail);

	(void) fprintf(fp, "\n");
}

static void
show_vmem_totals(vmem_dir_t *vdp)
{
	char ttl[40];

	if (vdp->vd_alloc != 0) {
		sprintf(ttl, "Total [%s]", vdp->vd_arena.vm_name);
		(void) fprintf(fp, "%-25s %6s %6s %6s %9u %9u %5u\n",
		    ttl, "", "", "",
		    vdp->vd_meminuse, vdp->vd_alloc, vdp->vd_alloc_fail);
	}
}

/* print kernel memory allocator statistics */
static void
prkmastat()
{
	kmem_cache_t c, *cp;
	vmem_t vm, *vmp;
	kmem_cache_stat_t kcs;
	datafmt_t *dfp;
	intptr_t kmem_null_cache_addr, vmem_list_addr;

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%*s ", dfp->width, dfp->hdr1);
	(void) fprintf(fp, "\n");

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%*s ", dfp->width, dfp->hdr2);
	(void) fprintf(fp, "\n");

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%.*s ", ABS(dfp->width), dashes);
	(void) fprintf(fp, "\n");

	vmem_list_addr = vmem_list_sym->st_value;
	kvm_read(kd, vmem_list_addr, (char *)&vmp, sizeof (vmp));
	while (vmp != NULL) {
		kvm_read(kd, (intptr_t)vmp, (char *)&vm, sizeof (vm));
		vmem_insert(&vm);
		vmp = vm.vm_next;
	}

	kmem_null_cache_addr = kmem_null_cache_sym->st_value;
	kvm_read(kd, kmem_null_cache_addr, (char *)&c, sizeof (c));
	for (cp = c.cache_next; cp != (kmem_cache_t *)kmem_null_cache_addr;
	    cp = c.cache_next) {
		kvm_read(kd, (intptr_t)cp, (char *)&c, sizeof (c));
		if (kmem_cache_getstats(cp, &kcs) == -1) {
			printf("error reading stats for %s\n", c.cache_name);
			continue;
		}
		show_kmem(&c, &kcs);
	}

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%.*s ", ABS(dfp->width), dashes);
	(void) fprintf(fp, "\n");

	vmem_dirwalk(&vmem_root, show_vmem_totals);

	for (dfp = kmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%.*s ", ABS(dfp->width), dashes);
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%*s ", dfp->width, dfp->hdr1);
	(void) fprintf(fp, "\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%*s ", dfp->width, dfp->hdr2);
	(void) fprintf(fp, "\n");

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%.*s ", ABS(dfp->width), dashes);
	(void) fprintf(fp, "\n");

	vmem_dirwalk(&vmem_root, show_vmem);

	for (dfp = vmemfmt; dfp->hdr1 != NULL; dfp++)
		(void) fprintf(fp, "%.*s ", ABS(dfp->width), dashes);
	(void) fprintf(fp, "\n");
}

/* initialization for namelist symbols */
static void
kmainit()
{
	static int kmainit_done = 0;

	if (kmainit_done)
		return;
	if ((kmem_null_cache_sym = symsrch("kmem_null_cache")) == 0)
		(void) error("kmem_null_cache not in symbol table\n");
	if ((vmem_list_sym = symsrch("vmem_list")) == 0)
		(void) error("vmem_list not in symbol table\n");
	kmainit_done = 1;
}

static int kmafull;
static kmem_cache_t *current_cache;

static int
bccmp(const void *a1, const void *a2)
{
	const kmem_bufctl_audit_t *bc1 = a1;
	const kmem_bufctl_audit_t *bc2 = a2;
	if (bc1->bc_timestamp < bc2->bc_timestamp)
		return (1);
	if (bc1->bc_timestamp > bc2->bc_timestamp)
		return (-1);
	return (0);
}

static hrtime_t newest_bc_time;

static void
showbc(kmem_bufctl_audit_t *bcp)
{
	int i;
	hrtime_t delta = newest_bc_time - bcp->bc_timestamp;
	char name[KMEM_CACHE_NAMELEN + 1];

	if (kvm_read(kd, (ulong_t)&bcp->bc_cache->cache_name, name,
	    sizeof (name)) == -1)
		(void) sprintf(name, "%p", (void *)bcp->bc_cache);

	fprintf(fp,
	    "\nT-%lld.%09lld  addr=%p  %s\n",
	    delta / NANOSEC, delta % NANOSEC, bcp->bc_addr, name);

	for (i = 0; i < bcp->bc_depth; i++) {
		fprintf(fp, "\t ");
		prsymbol(NULL, bcp->bc_stack[i]);
	}
}

/*
 * Print kmem transaction log in reverse-time-sorted order
 */
int
getkmalog()
{
	char *logname = "kmem_transaction_log";
	int c;
	kmem_log_header_t lh, *lhp;
	Sym *sp;
	kmem_bufctl_audit_t *bcbase = NULL;
	kmem_bufctl_audit_t bc, *bcp;
	int i;
	size_t bc_per_chunk;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}

	if (args[optind]) {
		if (strcmp(args[optind], "fail") == 0)
			logname = "kmem_failure_log";
		else if (strcmp(args[optind], "slab") == 0)
			logname = "kmem_slab_log";
		else
			error("no such log\n");
	}

	if ((sp = symsrch(logname)) == NULL)
		error("cannot find log '%s'\n", logname);

	if (kvm_read(kd, sp->st_value, (void *)&lhp, sizeof (lhp)) == -1)
		error("cannot read log header pointer for '%s'\n", logname);

	if (kvm_read(kd, (ulong_t)lhp, (void *)&lh, sizeof (lh)) == -1)
		error("logging not enabled for '%s'\n", logname);

	bcbase = malloc(lh.lh_chunksize * lh.lh_nchunks + sizeof (bc));
	if (bcbase == NULL)
		error("malloc");

	bcp = bcbase;
	bc_per_chunk = lh.lh_chunksize / sizeof (kmem_bufctl_audit_t);
	for (i = 0; i < lh.lh_nchunks; i++) {
		if (kvm_read(kd, (ulong_t)lh.lh_base + i * lh.lh_chunksize,
		    (void *)bcp, lh.lh_chunksize) == -1) {
			perror("kvm_read lh_base");
			goto out;
		}
		bcp += bc_per_chunk;
	}
	bcp->bc_timestamp = 0;

	qsort(bcbase, bc_per_chunk * lh.lh_nchunks + 1, sizeof (bc), bccmp);

	newest_bc_time = bcbase->bc_timestamp;

	for (bcp = bcbase; bcp->bc_timestamp != 0; bcp++)
		showbc(bcp);
out:
	free(bcbase);
	return (0);
}

/*
 * Print "kmem_alloc*" usage with stack traces when KMF_AUDIT is enabled
 */
int
getkmausers()
{
	int c;
	kmem_cache_t *cp, *kma_cache[1000];
	int ncaches, i;
	int mem_threshold = 8192;	/* Minimum # bytes for printing */
	int cnt_threshold = 100;	/* Minimum # blocks for printing */
	int audited_caches = 0;
	int do_all_caches = 0;

	kmafull = 0;
	optind = 1;
	while ((c = getopt(argcnt, args, "efw:")) != EOF) {
		switch (c) {
			case 'e':
				mem_threshold = 0;
				cnt_threshold = 0;
				break;
			case 'f':
				kmafull = 1;
				break;
			case 'w':
				redirect();
				break;
			default:
				longjmp(syn, 0);
		}
	}

	init_owner();

	if (args[optind]) {
		ncaches = 0;
		do {
			cp = kmem_cache_find(args[optind]);
			if (cp == NULL)
				error("Unknown cache: %s\n",
					args[optind]);
			kma_cache[ncaches++] = cp;
			optind++;
		} while (args[optind]);
	} else {
		ncaches = kmem_cache_find_all("", kma_cache, 1000);
		do_all_caches = 1;
	}

	for (i = 0; i < ncaches; i++) {
		kmem_cache_t c;
		cp = kma_cache[i];

		if (kvm_read(kd, (ulong_t)cp, (void *)&c, sizeof (c)) == -1) {
			perror("kvm_read kmem_cache");
			return (-1);
		}

		if (!(c.cache_flags & KMF_AUDIT)) {
			if (!do_all_caches)
				error("KMF_AUDIT is not enabled for %s\n",
					c.cache_name);
			continue;
		}

		current_cache = &c;
		kmem_cache_audit_apply(cp, kmause);
		audited_caches++;
	}

	if (audited_caches == 0 && do_all_caches)
		error("KMF_AUDIT is not enabled for any caches\n");

	print_owner("allocations", mem_threshold, cnt_threshold);
	return (0);
}

/* ARGSUSED */
static void
kmause(void *kaddr, void *buf, size_t size, kmem_bufctl_audit_t *bcp)
{
	int i;

	if (kmafull) {
		fprintf(fp, "size %ld, addr %p, thread %p, cache %s\n",
			size, kaddr, bcp->bc_thread, current_cache->cache_name);
		for (i = 0; i < bcp->bc_depth; i++) {
			fprintf(fp, "\t ");
			prsymbol(NULL, bcp->bc_stack[i]);
		}
	}
	add_owner(bcp, size, size);
}
