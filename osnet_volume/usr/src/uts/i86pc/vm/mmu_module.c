/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mmu_module.c	1.11	99/06/02 SMI"

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <vm/seg_spt.h>
#include <sys/var.h>
#include <sys/x86_archext.h>
#include <vm/faultcode.h>

#include <sys/t_lock.h>
#include <sys/memlist.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#include <sys/vm_machparam.h>
#include <sys/tss.h>
#include <sys/vnode.h>
#include <vm/anon.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#define	SUNDDI_IMPL	/* so sunddi.h will not redefine splx() et al */
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/cred.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <vm/seg_vn.h>
#include <sys/bootconf.h> /* XXX the memlist stuff belongs in memlist_plat.h */

/*
 * External Routines:
 */

void 		hat_init(void) {}
/* ARGSUSED */
struct hat 	*hat_alloc(struct as *as) { return NULL; }
/* ARGSUSED */
void 		hat_free_start(struct hat *hat) {}
/* ARGSUSED */
void 		hat_free_end(struct hat *hat) {}
/* ARGSUSED */
int 		hat_dup(struct hat *hat, struct hat *hat_c, caddr_t addr,
		    size_t len, uint_t flags) { return 0; }
/* ARGSUSED */
void		hat_swapin(struct hat *hat) {}
/* ARGSUSED */
void		hat_swapout(struct hat *hat) {}
/* ARGSUSED */
void		hat_map(struct hat *hat, caddr_t addr,
			size_t len, uint_t flags) {}
/* ARGSUSED */
int		hat_pageunload(struct page *pp, uint_t forceflag) { return 0; }
/* ARGSUSED */
uint_t		hat_pagesync(struct page *pp, uint_t clearflag) { return 0; }
/* ARGSUSED */
void		hat_sync(struct hat *hat, caddr_t addr, size_t len,
			uint_t clearflag) {}
/* ARGSUSED */
void		hat_memload(struct hat *hat, caddr_t addr, page_t *pp,
			uint_t attr, uint_t flags) {}
/* ARGSUSED */
void		hat_devload(struct hat *hat, caddr_t addr,
			size_t len, ulong_t pf, uint_t attr, int flags) {}
/* ARGSUSED */
void		hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
			page_t **ppa, uint_t attr, uint_t flags) {}
/* ARGSUSED */
void		hat_unlock(struct hat *hat, caddr_t addr, size_t len) {}
/* ARGSUSED */
void		hat_chgprot(struct hat *hat, caddr_t addr, size_t len,
			uint_t attr) {}
/* ARGSUSED */
void		hat_chgattr(struct hat *hat, caddr_t addr, size_t len,
			uint_t attr) {}
/* ARGSUSED */
void		hat_chgattr_pagedir(struct hat *hat, caddr_t addr, size_t len,
			uint_t attr) {}
/* ARGSUSED */
void		hat_setattr(struct hat *hat, caddr_t addr, size_t len,
			uint_t attr) {}
/* ARGSUSED */
void		hat_clrattr(struct hat *hat, caddr_t addr, size_t len,
			uint_t attr) {}
/* ARGSUSED */
uint_t		hat_getattr(struct hat *hat, caddr_t addr,
			uint_t *attr) { return 0; }
/* ARGSUSED */
ssize_t		hat_getpagesize(struct hat *hat, caddr_t addr) { return 0; }
/* ARGSUSED */
void		hat_page_setattr(page_t *pp, uint_t flag) {}
/* ARGSUSED */
void		hat_page_clrattr(page_t *pp, uint_t flag) {}
/* ARGSUSED */
uint_t		hat_page_getattr(page_t *pp, uint_t flag) { return 0; }
/* ARGSUSED */
void		hat_unload(struct hat *hat, caddr_t addr, size_t len,
			uint_t flags) {}
/* ARGSUSED */
ulong_t		hat_getpfnum(struct hat *hat, caddr_t addr) { return 0; }
/* ARGSUSED */
ulong_t		hat_page_getshare(page_t *pp) { return 0; }
/* ARGSUSED */
int		hat_probe(struct hat *hat, caddr_t addr) { return 0; }
/* ARGSUSED */
int		hat_share(struct hat *hat, caddr_t addr, struct hat *hat_s,
			caddr_t srcaddr, size_t size) { return 0; }
/* ARGSUSED */
void		hat_unshare(struct hat *hat, caddr_t addr, size_t size) {}
/* ARGSUSED */
size_t		hat_get_mapped_size(struct hat *hat) { return 0; }
/* ARGSUSED */
int		hat_stats_enable(struct hat *hat) { return 0; }
/* ARGSUSED */
void		hat_stats_disable(struct hat *hat) {}
/* ARGSUSED */
void		hat_load_mmuctx(kthread_t *t) {}
/* ARGSUSED */
void		hat_setup4thread(kthread_t *t) {}
/* ARGSUSED */
void		hat_setup(struct hat *hat, int allocflag) {}
/* ARGSUSED */
void		hat_enter(struct hat *hat) {}
/* ARGSUSED */
void		hat_exit(struct hat *hat) {}
/* ARGSUSED */
int		hat_supported(enum hat_features feature,
			void *arg) { return 0; }
/* ARGSUSED */
faultcode_t	hat_softlock(struct	hat *hat, caddr_t addr,
			size_t *lenp, page_t **ppp, uint_t flags) { return 0; }
/* ARGSUSED */
faultcode_t	hat_pageflip(struct hat *hat, caddr_t addr_to,
			caddr_t kaddr, size_t *lenp, page_t **pp_to,
			page_t **pp_from) { return 0; }
/* ARGSUSED */
void		i486mmu_mlist_exit(struct page *pp) {}
/* ARGSUSED */
ulong_t		hat_getkpfnum(caddr_t addr) { return 0; }
/* ARGSUSED */
void		hat_reserve(struct as *as, caddr_t addr, size_t size) {}
/* ARGSUSED */
int		hat_kill_procs(page_t *pp, caddr_t	addr) { return 0; }
/* ARGSUSED */
void		mmu_loadcr3(struct cpu *cpup, void *arg) {}
/* ARGSUSED */
pfn_t		va_to_pfn(void *vaddr) { return 0; }
/* ARGSUSED */
void		hat_kern_setup(void) {}
/* ARGSUSED */
void		lomem_init(void) {}
/* ARGSUSED */
caddr_t		lomem_alloc(uint_t nbytes, ddi_dma_attr_t *attr,
			int align, int cansleep) { return 0; }
/* ARGSUSED */
void		lomem_free(caddr_t kaddr) {}
/* ARGSUSED */
caddr_t		i86devmap(pfn_t pf, pgcnt_t npf, uint_t prot) { return 0; }
/* ARGSUSED */
page_t		*page_numtopp_alloc(pfn_t pfnum) { return 0; }
/* ARGSUSED */
int		islomembuf(void *buf) { return 0; }
/* ARGSUSED */
int		kadb_map(caddr_t vaddr, uint64_t new,
			uint64_t *old) { return 0; }



/* ARGSUSED */
uint_t		page_num_pagesizes(void) { return 0; }
/* ARGSUSED */
size_t		page_get_pagesize(uint_t n) { return 0; }
/* ARGSUSED */
faultcode_t	pagefault(caddr_t addr, enum fault_type type,
			enum seg_rw rw, int iskernel) { return 0; }
/* ARGSUSED */
void		 map_addr(caddr_t *addrp, size_t len, offset_t off,
			int align, uint_t flags) {}
/* ARGSUSED */
void		 map_addr_proc(caddr_t	*addrp, size_t	len, offset_t	off,
			int align, caddr_t userlimit, struct proc *p) {}
/* ARGSUSED */
int		valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen,
			int dir) { return 0; }
/* ARGSUSED */
int		valid_usr_range(caddr_t addr, size_t len, uint_t prot,
			struct as *as, caddr_t userlimit)
			{ return (RANGE_BADADDR); }
/* ARGSUSED */
int		pf_is_memory(pfn_t pf) {return 0; }
/* ARGSUSED */
int		pagelist_num(ulong_t pfn) { return 0; }
/* ARGSUSED */
void		page_list_sub(int list, page_t *pp) {}
/* ARGSUSED */
void		page_list_add(int list, page_t *pp, int where) {}
/* ARGSUSED */
page_t		*page_get_freelist(struct vnode *vp, u_offset_t off,
			struct seg *seg, caddr_t vaddr, size_t size,
			uint_t flags, void *resv)
			{ return 0; }
/* ARGSUSED */
page_t		*page_get_cachelist(struct vnode *vp, u_offset_t off,
			struct seg *seg, caddr_t vaddr, uint_t flags,
			void *resv)
			{ return 0; }
/* ARGSUSED */
void		page_coloring_init(void) {}
/* ARGSUSED */
int		bp_color(struct buf *bp) { return (0); }
/* ARGSUSED */
page_t		*page_get_anylist(struct vnode *vp, u_offset_t off,
			struct as *as, caddr_t vaddr, size_t size, uint_t flags,
			ddi_dma_attr_t *dma_attr) { return 0; }
/* ARGSUSED */
page_t		*page_create_io(struct vnode *vp, u_offset_t off,
			uint_t bytes, uint_t flags, struct as *as,
			caddr_t	vaddr, ddi_dma_attr_t *mattr)
			{ return 0; }
/* ARGSUSED */
page_t		*page_get_replacement_page(page_t *like_pp) { return NULL; }

/* ARGSUSED */
int		platform_page_relocate(page_t **target,
			page_t **replacement)
			{ return -1; }
/* ARGSUSED */
int		pageout_init(void (*proc)(), proc_t *pp,
			pri_t pri) { return 1; }
/* ARGSUSED */
void		setup_kernel_page_directory(struct cpu *cpup) {}
/* ARGSUSED */
void 		setup_vaddr_for_ppcopy(struct cpu *cpup) {}
/* ARGSUSED */
void 		ppcopy(page_t *pp1, page_t *pp2) {}
/* ARGSUSED */
void 		pagezero(page_t *pp, uint_t offset, uint_t len) {}
/* ARGSUSED */
void 		clear_bootpde(struct cpu *cpup) {}
/* ARGSUSED */
void 		mmu_setup_kvseg(pfn_t pfn) {}
/* ARGSUSED */
void 		psm_pageinit(machpage_t *pp, uint32_t pnum) {}
/* ARGSUSED */
void 		hat_map_kernel_stack_local(caddr_t saddr, caddr_t eaddr) {}
/* ARGSUSED */
void 		mmu_init(void) {}
/* ARGSUSED */
int		cpuid2nodeid(int cpun) { return (0); }
/* ARGSUSED */
void *		kmem_node_alloc(size_t size, int flg, int node) { return (0); }

/* ARGSUSED */
void 		post_startup_mmu_initialization(void) {}
