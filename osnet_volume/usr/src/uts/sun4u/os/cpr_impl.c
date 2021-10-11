/*
 * Copyright (c) 1994-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_impl.c	1.85	99/10/19 SMI"

/*
 * Platform specific implementation code
 */

#define	SUNDDI_IMPL

#include <sys/types.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>
#include <sys/iommu.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/intreg.h>
#include <sys/pte.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/as.h>
#include <sys/cpr.h>
#include <sys/vmem.h>
#include <sys/clock.h>
#include <sys/kmem.h>
#include <sys/panic.h>
#include <vm/seg_kmem.h>
#include <sys/cpu_module.h>
#include <sys/callb.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/stack.h>
#include <sys/fs/ufs_fs.h>
#include <sys/memlist.h>
#include <sys/bootconf.h>
#include <sys/debug/debug.h>

/* for exporting the mapping info to prom */
#include <sys/spitasi.h>

extern	void cpr_clear_bitmaps(void);
extern	void i_cpr_read_maps(caddr_t, u_int);
extern	void kadb_arm(void);
extern	void kadb_format(caddr_t *);

static	int i_cpr_storage_desc_alloc(csd_t **, pgcnt_t *, csd_t **, int);
static	void i_cpr_storage_desc_init(csd_t *, pgcnt_t, csd_t *);
static	caddr_t i_cpr_storage_data_alloc(pgcnt_t, pgcnt_t *, int);
static	int cpr_dump_sensitive(vnode_t *, csd_t *);

pgcnt_t	i_cpr_count_sensitive_kpages(char *, bitfunc_t);
void	i_cpr_storage_free(void);

extern void *i_cpr_data_page;
extern int cpr_test_mode;
extern int cprboot_magic;
extern char cpr_default_path[];
extern int itlb_entries, dtlb_entries;
extern caddr_t textva, datava;

static char *kadb_defer_word;
static size_t kadb_defer_word_strlen;

static struct cpr_map_info cpr_prom_retain[CPR_PROM_RETAIN_CNT];
caddr_t cpr_vaddr = NULL;

static	u_int sensitive_pages_saved;
static	u_int sensitive_size_saved;

caddr_t	i_cpr_storage_data_base;
caddr_t	i_cpr_storage_data_end;
csd_t *i_cpr_storage_desc_base;
csd_t *i_cpr_storage_desc_end;		/* one byte beyond last used descp */
csd_t *i_cpr_storage_desc_last_used;	/* last used descriptor */
caddr_t sensitive_write_ptr;		/* position for next storage write */

size_t	i_cpr_sensitive_bytes_dumped;
pgcnt_t	i_cpr_sensitive_pgs_dumped;
pgcnt_t	i_cpr_storage_data_sz;		/* in pages */
pgcnt_t	i_cpr_storage_desc_pgcnt;	/* in pages */

u_short	cpr_mach_type = CPR_MACHTYPE_4U;
static	csu_md_t m_info;


#define	MAX_STORAGE_RETRY	3
#define	MAX_STORAGE_ALLOC_RETRY	3
#define	INITIAL_ALLOC_PCNT	40	/* starting allocation percentage */
#define	INTEGRAL		100	/* to get 1% precision */

#define	EXTRA_RATE		2	/* add EXTRA_RATE% extra space */
#define	EXTRA_DESCS		10

#define	CPR_NO_STORAGE_DESC	1
#define	CPR_NO_STORAGE_DATA	2

#define	SET_ORIG_CIF		0
#define	SET_TMP_CIF		1
#define	UNLINK_CIF_WRAPPER	0
#define	LINK_CIF_WRAPPER	1


/*
 * CPR miscellaneous support routines
 */
#define	cpr_open(path, mode,  vpp)	(vn_open(path, UIO_SYSSPACE, \
		mode, 0600, vpp, CRCREAT, 0))
#define	cpr_rdwr(rw, vp, basep, cnt)	(vn_rdwr(rw, vp,  (caddr_t)(basep), \
		cnt, 0LL, UIO_SYSSPACE, 0, (rlim64_t)MAXOFF_T, CRED(), \
		(ssize_t *)NULL))

/*
 * definitions for saving/restoring prom pages
 */
static void	*ppage_buf;
static pgcnt_t	ppage_count;
static pfn_t	*pphys_list;
static size_t	pphys_list_size;


/*
 * special handling for tlb info
 */
#define	WITHIN_NUCLEUS(va, base) \
	(((caddr_t)(va) >= (base)) && \
	(((caddr_t)(va) + MMU_PAGESIZE) <= ((base) + MMU_PAGESIZE4M)))

#define	CPR_MAX_TI		3	/* max saved tlb info */
#define	CPR_INIT_TI		0x1
#define	CPR_CLEAR_TI		0x2
#define	CPR_SAVE_TI		0x4

/* private struct for sun4u cpr */
struct cpr_tlb_info {
	caddr_t vaddr;
	struct sun4u_tlb *md_array;
};

static struct cpr_tlb_info tlb_info[CPR_MAX_TI];
static struct cpr_tlb_info *ctp;


/*
 * set/save cookies from original and tmp proms
 */
void
i_cpr_swap_cookie(int action)
{
	extern func_t i_cpr_orig_cif, i_cpr_tmp_cif, cif_handler;

	if (action == SET_TMP_CIF) {
		i_cpr_orig_cif = cif_handler;
		cif_handler = i_cpr_tmp_cif;
	} else
		cif_handler = i_cpr_orig_cif;
}


/*
 * link-in a cif wrapper after switching to kernel traps
 * or unlink after replacing the original prom pages
 */
void
i_cpr_splice(int action)
{
	extern func_t i_cpr_real_cif, cif_handler;
	extern int i_cpr_cif_wrapper();

	if (action == LINK_CIF_WRAPPER) {
		i_cpr_real_cif = cif_handler;
		cif_handler = i_cpr_cif_wrapper;
	} else
		cif_handler = i_cpr_real_cif;
}


/*
 * launch slave cpus into kernel text, pause them,
 * and restore the original prom pages
 */
void
i_cpr_mp_setup(void)
{
	extern int restart_other_cpu(int, boolean_t);
	extern struct scb trap_table;
	cpu_t *cp;

	/*
	 * reset cpu_ready_set so x_calls work properly
	 */
	CPUSET_ZERO(cpu_ready_set);
	CPUSET_ADD(cpu_ready_set, getprocessorid());

	/*
	 * setup cif to use cookie from the new/tmp prom;
	 * setup tmp handling for calling prom services
	 */
	i_cpr_swap_cookie(SET_TMP_CIF);
	i_cpr_splice(LINK_CIF_WRAPPER);

	/*
	 * switch to kernel trap table
	 */
	prom_set_traptable(&trap_table);

	if (ncpus > 1) {
		sfmmu_init_tsbs();

		DEBUG1(errp("MP startup...\n"));
		mutex_enter(&cpu_lock);
		/*
		 * All of the slave cpus are not ready at this time,
		 * yet the cpu structures have various cpu_flags set;
		 * clear cpu_flags and mutex_ready.
		 */
		for (cp = CPU->cpu_next; cp != CPU; cp = cp->cpu_next) {
			cp->cpu_flags = 0;
			cp->cpu_m.mutex_ready = 0;
		}

		for (cp = CPU->cpu_next; cp != CPU; cp = cp->cpu_next)
			(void) restart_other_cpu(cp->cpu_id, B_TRUE);

		pause_cpus(NULL);
		mutex_exit(&cpu_lock);
		DEBUG1(errp("MP paused...\n"));
	}

	/*
	 * unlink the cif wrapper and reset the cif handler
	 * to the original prom's cookie.  note: no prom_xxx
	 * calls until after prom pages are restored.
	 */
	i_cpr_splice(UNLINK_CIF_WRAPPER);
	i_cpr_swap_cookie(SET_ORIG_CIF);

	(void) i_cpr_prom_pages(CPR_PROM_RESTORE);
}


/*
 * end marker for jumpback page;
 * this symbol is used to check the size of i_cpr_resume_setup()
 * and the above text.  For simplicity, the Makefile needs to
 * link i_cpr_resume_setup.o and cpr_impl.o consecutively.
 */
void
i_cpr_end_jumpback(void)
{
}


/*
 * when a page is outside the nucleus, record the vaddr and a pointer
 * to m_info dtte/itte; this data is later used during suspend to save
 * new ttes and during resume to flush tlb entries
 */
static void
i_cpr_record_ti(void *vaddr, caddr_t nbase, struct sun4u_tlb *md_array)
{
	ASSERT(nbase == datava || nbase == textva);
	if (WITHIN_NUCLEUS(vaddr, nbase))
		return;

	ASSERT(ctp < &tlb_info[CPR_MAX_TI]);
	ctp->vaddr = (caddr_t)((uintptr_t)vaddr & MMU_PAGEMASK);
	ctp->md_array = md_array;
	ctp++;
}


/*
 * search for the highest unused tlb index and a free slot in the
 * m_info itte/dtte array and add a new entry; cprboot will later
 * install these as locked tlb entries.  instead of prom mappings,
 * resume will use these tlb entries to translate kpages until
 * switching back to the kernel trap table
 */
static void
i_cpr_add_ttes(void)
{
	extern int utsb_dtlb_ttenum;
	struct cpr_tlb_info *ti_tail;
	struct sun4u_tlb *stp, *st_tail;
	int high_index;
	tte_t tte;
	pfn_t ppn;
	uint_t rw;

	/*
	 * for data ttes, we have to select an index below
	 * utsb_dtlb_ttenum since the sfmmu code will
	 * overwrite any dtlb entry at that index
	 */
	ti_tail = &tlb_info[CPR_MAX_TI];
	for (ctp = tlb_info; ctp < ti_tail && ctp->vaddr; ctp++) {
		ASSERT(ctp->md_array == m_info.dtte ||
		    ctp->md_array == m_info.itte);
		if (ctp->md_array == m_info.dtte) {
			high_index = utsb_dtlb_ttenum;
			rw = TTE_HWWR_INT;
		} else {
			high_index = itlb_entries;
			rw = 0;
		}

		ppn = va_to_pfn(ctp->vaddr);
		tte.tte_inthi = TTE_VALID_INT | TTE_PFN_INTHI(ppn);
		tte.tte_intlo = TTE_PFN_INTLO(ppn) | TTE_LCK_INT |
		    TTE_CP_INT | TTE_CV_INT | TTE_PRIV_INT | rw;

		st_tail = ctp->md_array + CPR_MAX_TLB;
		for (stp = ctp->md_array; stp < st_tail; stp++) {
			if (stp->index && stp->index < high_index)
				high_index = stp->index;
			if (stp->va_tag)
				continue;
			high_index--;
			stp->tte = tte;
			stp->va_tag = (cpr_ptr)ctp->vaddr;
			stp->index = high_index;
			break;
		}
	}
}


/*
 * (action == CPR_INIT_TI)
 * init tlb info; the vaddr data needs to be recorded before
 * saving sensitive pages to be available during resume,
 * otherwise the data would appear as NULLs
 *
 * (action == CPR_CLEAR_TI)
 * during resume, use the vaddr data to
 * flush dtlb/itlb entries setup by cprboot
 *
 * (action == CPR_SAVE_TI)
 * save vaddr and tte data to dtte/itte arrays
 */
static void
i_cpr_tlb_info(int action)
{
	struct cpr_tlb_info *ti_tail;

	if (action == CPR_INIT_TI) {
		bzero(tlb_info, sizeof (tlb_info));
		ctp = tlb_info;
		i_cpr_record_ti((void *)i_cpr_resume_setup,
		    textva, m_info.itte);
		i_cpr_record_ti(&i_cpr_data_page, datava, m_info.dtte);
		i_cpr_record_ti(curthread, datava, m_info.dtte);
	} else if (action == CPR_CLEAR_TI) {
		ti_tail = &tlb_info[CPR_MAX_TI];
		for (ctp = tlb_info; ctp < ti_tail && ctp->vaddr; ctp++)
			vtag_flushpage(ctp->vaddr, KCONTEXT);
	} else if (action == CPR_SAVE_TI)
		i_cpr_add_ttes();
}


/* ARGSUSED */
void
i_cpr_vtag_xcall(uint64_t arg1, uint64_t arg2)
{
	i_cpr_tlb_info(CPR_CLEAR_TI);
}


/*
 * start paused slave cpus and clear tmp tlb entries
 */
void
i_cpr_machdep_setup(void)
{
	u_int pil, reset_pil;

	if (ncpus > 1) {
		DEBUG1(errp("MP restarted...\n"));
		mutex_enter(&cpu_lock);
		start_cpus();
		mutex_exit(&cpu_lock);

		pil = getpil();
		if (pil < XCALL_PIL)
			reset_pil = 0;
		else {
			reset_pil = 1;
			setpil(XCALL_PIL - 1);
		}
		xc_some(cpu_ready_set, i_cpr_vtag_xcall, 0, 0);
		if (reset_pil)
			setpil(pil);
	} else
		i_cpr_tlb_info(CPR_CLEAR_TI);
}


/*
 * Stop all interrupt activities in the system
 */
void
i_cpr_stop_intr(void)
{
	(void) spl7();
}

/*
 * Set machine up to take interrupts
 */
void
i_cpr_enable_intr(void)
{
	(void) spl0();
}

/*
 * save valid, locked tlb entries read with rd_func;
 * these will be reinstalled by cprboot on all cpus
 */
static void
i_cpr_get_tlb_data(struct sun4u_tlb *tlbp, int maxent,
    void (*rd_func)(u_int, tte_t *, caddr_t *, int *))
{
	int cnt, ent, ctxnum;
	caddr_t addr;
	tte_t tte;

	cnt = 0;
	for (ent = maxent - 1; ent >= 0; ent--) {
		(*rd_func)(ent, &tte, &addr, &ctxnum);
		if (TTE_IS_VALID(&tte) && TTE_IS_LOCKED(&tte) &&
		    (addr == textva || addr == datava ||
		    (enable_bigktsb && addr >= ktsb_base &&
		    addr < ktsb_base + ktsb_sz))) {
			ASSERT(cnt < CPR_MAX_TLB);
			tlbp->tte = tte;
			tlbp->va_tag = (cpr_ptr)addr;
			tlbp->index = ent;
			tlbp++;
			cnt++;
		}
	}
}


/*
 * record cpu nodes and ids
 */
static void
i_cpr_save_cpu_info(void)
{
	struct sun4u_cpu_info *scip;
	cpu_t *cp;

	scip = m_info.sci;
	cp = CPU;
	do {
		ASSERT(scip < &m_info.sci[NCPU]);
		scip->cpu_id = cp->cpu_id;
		scip->node = cpunodes[cp->cpu_id].nodeid;
		scip++;
	} while ((cp = cp->cpu_next) != CPU);
}


/*
 * Write necessary machine dependent information to cpr state file,
 * eg. sun4u mmu ctx secondary for the current running process (cpr) ...
 */
int
i_cpr_write_machdep(vnode_t *vp)
{
	extern u_int getpstate(), getwstate();
	extern u_int i_cpr_tstack_size;
	const char ustr[] = ": unix-tte 2drop false ;";
	uintptr_t tinfo;
	label_t *ltp;
	cmd_t cmach;
	char *fmt;
	int rc;

	/*
	 * ustr[] is used as temporary forth words during
	 * slave startup sequence, see sfmmu_mp_startup()
	 */

	cmach.md_magic = (u_int) CPR_MACHDEP_MAGIC;
	cmach.md_size = sizeof (m_info) + sizeof (ustr) +
	    kadb_defer_word_strlen;

	if (rc = cpr_write(vp, (caddr_t)&cmach, sizeof (cmach))) {
		cmn_err(CE_WARN, "cpr: Failed to write descriptor.");
		return (rc);
	}

	bzero(&m_info, sizeof (m_info));

	/*
	 * save machdep info
	 */
	m_info.ksb = (uint32_t)STACK_BIAS;
	m_info.kpstate = (uint16_t)getpstate();
	m_info.kwstate = (uint16_t)getwstate();
	DEBUG1(errp("stack bias 0x%x, pstate 0x%x, wstate 0x%x\n",
	    m_info.ksb, m_info.kpstate, m_info.kwstate));

	ltp = &ttolwp(curthread)->lwp_qsav;
	m_info.qsav_pc = (cpr_ext)ltp->val[0];
	m_info.qsav_sp = (cpr_ext)ltp->val[1];

	/*
	 * We used to save the current secondary context as well.
	 * But a change to the hat layer to simulate one TSB per process
	 * can cause a panic if we do that now.  The problem is that
	 * if we suspend the system without powering off (uadmin 3 8),
	 * the secondary context does not get reinitialized in the HW.
	 * The hat layer has an optimization which avoids setting a new
	 * context if it is the same as the current HW context.  It also
	 * skips the step to lock the new TSB location into the DTLB.
	 * This causes a DTLB miss at trap level 2 because the TSB
	 * is not mapped (bug 4026209).  Force the hat to do all the steps.
	 */
	m_info.mmu_ctx_sec = INVALID_CONTEXT;
	m_info.mmu_ctx_pri = sfmmu_getctx_pri();

	i_cpr_get_tlb_data(m_info.itte, itlb_entries, itlb_rd_entry);
	i_cpr_get_tlb_data(m_info.dtte, dtlb_entries, dtlb_rd_entry);
	i_cpr_tlb_info(CPR_SAVE_TI);

	tinfo = (uintptr_t)curthread;
	m_info.thrp = (cpr_ptr)tinfo;

	tinfo = (uintptr_t)i_cpr_resume_setup;
	m_info.func = (cpr_ptr)tinfo;

	/*
	 * i_cpr_data_page is comprised of a 4K stack area and a few
	 * trailing data symbols; the page is shared by the prom and
	 * kernel during resume.  the stack size is recorded here
	 * and used by cprboot to set %sp
	 */
	tinfo = (uintptr_t)&i_cpr_data_page;
	m_info.tmp_stack = (cpr_ptr)tinfo;
	m_info.tmp_stacksize = i_cpr_tstack_size;

	m_info.test_mode = cpr_test_mode;

	i_cpr_save_cpu_info();

	if (rc = cpr_write(vp, (caddr_t)&m_info, sizeof (m_info))) {
		cmn_err(CE_WARN, "cpr: Failed to write machdep info.");
		return (rc);
	}

	fmt = "cpr: error writing %s forth info";
	if (rc = cpr_write(vp, (caddr_t)ustr, sizeof (ustr)))
		cmn_err(CE_WARN, fmt, "unix-tte");

	if (kadb_defer_word_strlen) {
		if (rc = cpr_write(vp,
		    kadb_defer_word, kadb_defer_word_strlen))
			cmn_err(CE_WARN, fmt, "kadb");
	}

	return (rc);
}


/*
 * Save miscellaneous information which needs to be written to the
 * state file.  This information is required to re-initialize
 * kernel/prom handshaking.
 */
void
i_cpr_save_machdep_info()
{
	DEBUG5(errp("jumpback size = 0x%lx\n",
	    (uintptr_t)&i_cpr_end_jumpback -
	    (uintptr_t)i_cpr_resume_setup));

	/*
	 * Verify the jumpback code all falls in one page.
	 */
	if (((uintptr_t)&i_cpr_end_jumpback & MMU_PAGEMASK) !=
	    ((uintptr_t)i_cpr_resume_setup & MMU_PAGEMASK))
		cmn_err(CE_PANIC, "cpr: jumpback code exceeds one page.\n");

	/*
	 * If kadb is running, get a copy of the strings which it
	 * exports to the prom.
	 */
	kadb_defer_word_strlen = 0;
	kadb_defer_word = NULL;
	kadb_format(&kadb_defer_word);
	if (kadb_defer_word != NULL)
		kadb_defer_word_strlen = strlen(kadb_defer_word) + 1;
}


void
i_cpr_set_tbr()
{
}

/*
 * The bootcpu is the lowest numbered one on sun4u.
 */
processorid_t
i_cpr_find_bootcpu()
{
	int i;

	ASSERT(MUTEX_HELD(&cpu_lock));

	for (i = 0; i < NCPU; i++) {
		cpu_t *cp;

		cp = cpu[i];
		if (cp != NULL && (cp->cpu_flags & CPU_EXISTS))
			return (cp->cpu_id);
	}
	cmn_err(CE_PANIC, "i_cpr_find_bootcpu(): failed");
	/*NOTREACHED*/
}

/*
 * Return the virtual address of the mapping area
 */
caddr_t
i_cpr_map_setup(void)
{
	/*
	 * Allocate a virtual memory range spanned by an hmeblk.
	 * This would be 8 hments or 64k bytes.  Starting VA
	 * must be 64k (8-page) aligned.
	 */
	cpr_vaddr = vmem_xalloc(heap_arena,
	    mmu_ptob(NHMENTS), mmu_ptob(NHMENTS),
	    0, 0, NULL, NULL, VM_NOSLEEP);
	return (cpr_vaddr);
}

/*
 * create tmp locked tlb entries for a group of phys pages;
 *
 * i_cpr_mapin/i_cpr_mapout should always be called in pairs,
 * otherwise would fill up a tlb with locked entries
 */
void
i_cpr_mapin(caddr_t vaddr, u_int pages, pfn_t ppn)
{
	tte_t tte;

	for (; pages--; ppn++, vaddr += MMU_PAGESIZE) {
		tte.tte_inthi = TTE_VALID_INT | TTE_PFN_INTHI(ppn);
		tte.tte_intlo = TTE_PFN_INTLO(ppn) | TTE_LCK_INT |
		    TTE_CP_INT | TTE_CV_INT | TTE_PRIV_INT | TTE_HWWR_INT;
		sfmmu_dtlb_ld(vaddr, KCONTEXT, &tte);
	}
}

void
i_cpr_mapout(caddr_t vaddr, u_int pages)
{
	for (; pages--; vaddr += MMU_PAGESIZE)
		vtag_flushpage(vaddr, KCONTEXT);
}

/*
 * We're done using the mapping area; release virtual space
 */
void
i_cpr_map_destroy(void)
{
	vmem_free(heap_arena, cpr_vaddr, mmu_ptob(NHMENTS));
	cpr_vaddr = NULL;
}

/* ARGSUSED */
void
i_cpr_handle_xc(int flag)
{
}


/*
 * This function takes care of pages which are not in kas or need to be
 * taken care of in a special way.  For example, panicbuf pages are not
 * in kas and their pages are allocated via prom_retain().
 */
pgcnt_t
i_cpr_count_special_kpages(char *bitmap, bitfunc_t bitfunc)
{
	struct cpr_map_info *pri, *tail;
	pgcnt_t pages, total = 0;
	pfn_t pfn;

	/*
	 * Save information about prom retained panicbuf pages
	 */
	if (bitfunc == cpr_setbit) {
		pri = &cpr_prom_retain[CPR_PANICBUF];
		pri->virt = (cpr_ptr)panicbuf;
		pri->phys = va_to_pa(panicbuf);
		pri->size = sizeof (panicbuf);
	}

	/*
	 * Go through the prom_retain array to tag those pages.
	 */
	tail = &cpr_prom_retain[CPR_PROM_RETAIN_CNT];
	for (pri = cpr_prom_retain; pri < tail; pri++) {
		pages = mmu_btopr(pri->size);
		for (pfn = ADDR_TO_PN(pri->phys); pages--; pfn++) {
			if (pf_is_memory(pfn)) {
				if (bitfunc == cpr_setbit) {
					if ((*bitfunc)(pfn, bitmap) == 0)
						total++;
				} else
					total++;
			}
		}
	}

	return (total);
}


/*
 * Free up memory-related resources here.  We start by freeing buffers
 * allocated during suspend initialization.  Also, free up the mapping
 * resources allocated in cpr_init().
 */
void
i_cpr_free_memory_resources(void)
{
	(void) i_cpr_prom_pages(CPR_PROM_FREE);
	i_cpr_map_destroy();
	i_cpr_storage_free();
}


/*
 * Derived from cpr_write_statefile().
 * Save the sensitive pages to the storage area and do bookkeeping
 * using the sensitive descriptors. Each descriptor will contain no more
 * than CPR_MAXCONTIG amount of contiguous pages to match the max amount
 * of pages that statefile gets written to disk at each write.
 * XXX The CPR_MAXCONTIG can be changed to the size of the compression
 * scratch area.
 */
static int
i_cpr_save_to_storage(void)
{
	sensitive_size_saved = 0;
	sensitive_pages_saved = 0;
	sensitive_write_ptr = i_cpr_storage_data_base;
	return (cpr_contig_pages(NULL, SAVE_TO_STORAGE));
}


/*
 * This routine allocates space to save the sensitive kernel pages,
 * i.e. kernel data nucleus, kvalloc and kvseg segments.
 * It's assumed that those segments are the only areas that can be
 * contaminated by memory allocations during statefile dumping.
 * The space allocated here contains:
 * 	A list of descriptors describing the saved sensitive pages.
 * 	The storage area for saving the compressed sensitive kernel pages.
 * Since storage pages are allocated from segkmem, they need to be
 * excluded when saving.
 */
int
i_cpr_save_sensitive_kpages(void)
{
	static const char pages_fmt[] = "\n%s %s allocs\n"
	    "	spages %ld, vpages %ld, diff %ld\n";
	int retry_cnt;
	int error = 0;
	pgcnt_t pages, spages, vpages;
	caddr_t	addr;
	char *str;

	/*
	 * Tag sensitive kpages. Allocate space for storage descriptors
	 * and storage data area based on the resulting bitmaps.
	 * Note: The storage space will be part of the sensitive
	 * segment, so we need to tag kpages here before the storage
	 * is actually allocated just so their space won't be accounted
	 * for. They will not be part of the statefile although those
	 * pages will be claimed by cprboot.
	 */
	cpr_clear_bitmaps();

	spages = i_cpr_count_sensitive_kpages(REGULAR_BITMAP, cpr_setbit);
	vpages = cpr_count_volatile_pages(REGULAR_BITMAP, cpr_clrbit);
	pages = spages - vpages;

	str = "i_cpr_save_sensitive_kpages:";
	DEBUG7(errp(pages_fmt, "before", str, spages, vpages, pages));

	/*
	 * Allocate space to save the clean sensitive kpages
	 */
	for (retry_cnt = 0; retry_cnt < MAX_STORAGE_ALLOC_RETRY; retry_cnt++) {
		/*
		 * Alloc on first pass or realloc if we are retrying because
		 * of insufficient storage for sensitive pages
		 */
		if (retry_cnt == 0 || error == ENOMEM) {
			if (i_cpr_storage_data_base) {
				kmem_free(i_cpr_storage_data_base,
				    mmu_ptob(i_cpr_storage_data_sz));
				i_cpr_storage_data_base = NULL;
				i_cpr_storage_data_sz = 0;
			}
			addr = i_cpr_storage_data_alloc(pages,
			    &i_cpr_storage_data_sz, retry_cnt);
			if (addr == NULL) {
				DEBUG7(errp(
				    "\n%s can't allocate data storage space!\n",
				    str));
				return (ENOMEM);
			}
			i_cpr_storage_data_base = addr;
			i_cpr_storage_data_end =
			    addr + mmu_ptob(i_cpr_storage_data_sz);
		}

		/*
		 * Allocate on first pass, only realloc if retry is because of
		 * insufficient descriptors, but reset contents on each pass
		 * (desc_alloc resets contents as well)
		 */
		if (retry_cnt == 0 || error == -1) {
			error = i_cpr_storage_desc_alloc(
			    &i_cpr_storage_desc_base, &i_cpr_storage_desc_pgcnt,
			    &i_cpr_storage_desc_end, retry_cnt);
			if (error != 0)
				return (error);
		} else {
			i_cpr_storage_desc_init(i_cpr_storage_desc_base,
			    i_cpr_storage_desc_pgcnt, i_cpr_storage_desc_end);
		}

		/*
		 * We are ready to save the sensitive kpages to storage.
		 * We cannot trust what's tagged in the bitmaps anymore
		 * after storage allocations.  Clear up the bitmaps and
		 * retag the sensitive kpages again.  The storage pages
		 * should be untagged.
		 */
		cpr_clear_bitmaps();

		spages =
		    i_cpr_count_sensitive_kpages(REGULAR_BITMAP, cpr_setbit);
		vpages = cpr_count_volatile_pages(REGULAR_BITMAP, cpr_clrbit);

		DEBUG7(errp(pages_fmt, "after ", str,
		    spages, vpages, spages - vpages));

		/*
		 * Returns 0 on success, -1 if too few descriptors, and
		 * ENOMEM if not enough space to save sensitive pages
		 */
		DEBUG1(errp("compressing pages to storage...\n"));
		error = i_cpr_save_to_storage();
		if (error == 0) {
			/* Saving to storage succeeded */
			DEBUG1(errp("compressed %d pages\n",
			    sensitive_pages_saved));
			break;
		} else if (error == -1)
			DEBUG1(errp("%s too few descriptors\n", str));
	}
	if (error == -1)
		error = ENOMEM;
	return (error);
}


/*
 * Estimate how much memory we will need to save
 * the sensitive pages with compression.
 */
static caddr_t
i_cpr_storage_data_alloc(pgcnt_t pages, pgcnt_t *alloc_pages, int retry_cnt)
{
	size_t est_size, alloc_pcnt;
	caddr_t addr;
	char *str;

	str = "i_cpr_storage_data_alloc:";
	if (retry_cnt == 0) {
		/*
		 * common compression ratio is about 3:1
		 * initial storage allocation is estimated at 40%
		 * to cover the majority of cases
		 */
		alloc_pcnt = INITIAL_ALLOC_PCNT;
		est_size = (mmu_ptob(pages) * alloc_pcnt) / INTEGRAL;
		DEBUG7(errp("%s sensitive pages: %ld\n", str, pages));
		DEBUG7(errp("%s initial est pages: %ld, alloc %ld%%\n",
		    str, mmu_btopr(est_size), alloc_pcnt));
	} else {
		/*
		 * calculate the prior compression percentage (x100)
		 * from the last attempt to save sensitive pages
		 */
		ASSERT(sensitive_pages_saved != 0);
		alloc_pcnt = (sensitive_size_saved * INTEGRAL) /
		    mmu_ptob(sensitive_pages_saved);
		/*
		 * new estimated storage size is based on the
		 * prior compression percentage + 5% for each retry:
		 * bytes * (prior_pcnt + [5%, 10%])
		 */
		alloc_pcnt += (retry_cnt * 5);
		est_size = (mmu_ptob(pages) * alloc_pcnt) / INTEGRAL;
		DEBUG7(errp("%s Retry est pages: %d, alloc %d%%\n",
		    str, est_size, alloc_pcnt));
	}

	*alloc_pages = mmu_btopr(est_size);
	addr = kmem_alloc(mmu_ptob(*alloc_pages), KM_NOSLEEP);
	DEBUG7(errp("%s alloc %d pgs = 0x%x\n", str, *alloc_pages));
	return (addr);
}


void
i_cpr_storage_free(void)
{
	/* Free descriptors */
	if (i_cpr_storage_desc_base) {
		kmem_free(i_cpr_storage_desc_base,
		    mmu_ptob(i_cpr_storage_desc_pgcnt));
		i_cpr_storage_desc_base = NULL;
		i_cpr_storage_desc_pgcnt = 0;
	}


	/* Data storage */
	if (i_cpr_storage_data_base) {
		kmem_free(i_cpr_storage_data_base,
		    mmu_ptob(i_cpr_storage_data_sz));
		i_cpr_storage_data_base = NULL;
		i_cpr_storage_data_sz = 0;
	}
}


/*
 * This routine is derived from cpr_compress_and_write().
 * 1. Do bookkeeping in the descriptor for the contiguous sensitive chunk.
 * 2. Compress and save the clean sensitive pages into the storage area.
 */
int
i_cpr_compress_and_save(int chunks, pfn_t spfn, pgcnt_t pages)
{
	extern uchar_t *cpr_compress_pages(cpd_t *, pgcnt_t, int);
	extern caddr_t i_cpr_storage_data_end;
	u_int remaining, datalen;
	uint32_t test_usum;
	u_char *datap;
	csd_t *descp;
	cpd_t cpd;
	int error;

	/*
	 * Fill next empty storage descriptor
	 */
	descp = i_cpr_storage_desc_base + chunks - 1;
	if (descp >= i_cpr_storage_desc_end) {
		DEBUG1(errp("ran out of descriptors, base 0x%p, chunks %d, "
		    "end 0x%p, descp 0x%p\n", i_cpr_storage_desc_base, chunks,
		    i_cpr_storage_desc_end, descp));
		return (-1);
	}
	ASSERT(descp->csd_dirty_spfn == (u_int)-1);
	i_cpr_storage_desc_last_used = descp;

	descp->csd_dirty_spfn = spfn;
	descp->csd_dirty_npages = pages;

	i_cpr_mapin(CPR->c_mapping_area, pages, spfn);

	/*
	 * try compressing pages and copy cpd fields
	 * pfn is copied for debug use
	 */
	cpd.cpd_pfn = spfn;
	datap = cpr_compress_pages(&cpd, pages, C_COMPRESSING);
	datalen = cpd.cpd_length;
	descp->csd_clean_compressed = (cpd.cpd_flag & CPD_COMPRESS);
#ifdef DEBUG
	descp->csd_usum = cpd.cpd_usum;
	descp->csd_csum = cpd.cpd_csum;
#endif

	error = 0;

	/*
	 * Save the raw or compressed data to the storage area pointed to by
	 * sensitive_write_ptr. Make sure the storage space is big enough to
	 * hold the result. Otherwise roll back to increase the storage space.
	 */
	descp->csd_clean_sva = (cpr_ptr)sensitive_write_ptr;
	descp->csd_clean_sz = datalen;
	if ((sensitive_write_ptr + datalen) < i_cpr_storage_data_end) {
		bcopy(datap, sensitive_write_ptr, datalen);
		sensitive_size_saved += datalen;
		sensitive_pages_saved += descp->csd_dirty_npages;
		sensitive_write_ptr += datalen;
	} else {
		remaining = (i_cpr_storage_data_end - sensitive_write_ptr);
		DEBUG1(errp("i_cpr_compress_and_save: The storage "
		    "space is too small!\ngot %d, want %d\n\n",
		    remaining, (remaining + datalen)));
#ifdef	DEBUG
		/*
		 * Check to see if the content of the sensitive pages that we
		 * just copied have changed during this small time window.
		 */
		test_usum = checksum32(CPR->c_mapping_area, mmu_ptob(pages));
		descp->csd_usum = cpd.cpd_usum;
		if (test_usum != descp->csd_usum) {
			DEBUG1(errp("\nWARNING: i_cpr_compress_and_save: "
			    "Data in the range of pfn 0x%x to pfn "
			    "0x%x has changed after they are saved "
			    "into storage.", spfn, (spfn + pages - 1)));
		}
#endif
		error = ENOMEM;
	}

	i_cpr_mapout(CPR->c_mapping_area, pages);
	return (error);
}


/*
 * This routine is derived from cpr_count_kpages().
 * It goes through kernel data nucleus and segkmem segments to select
 * pages in use and mark them in the corresponding bitmap.
 */
pgcnt_t
i_cpr_count_sensitive_kpages(char *bitmap, bitfunc_t bitfunc)
{
	pgcnt_t kdata_cnt = 0, segkmem_cnt = 0;
	extern caddr_t e_moddata;
	extern struct seg kvalloc;
	size_t size;

	/*
	 * Kernel data nucleus pages
	 */
	size = e_moddata - s_data;
	kdata_cnt += cpr_count_pages(s_data, size, bitmap, bitfunc);

	/*
	 * kvseg and kvalloc pages
	 */
	segkmem_cnt += cpr_scan_kvseg(bitmap, bitfunc);
	segkmem_cnt += cpr_count_pages(kvalloc.s_base, kvalloc.s_size,
	    bitmap, bitfunc);

	DEBUG7(errp("\ni_cpr_count_sensitive_kpages:\n"
	    "\tkdata_cnt %ld + segkmem_cnt %ld = %ld pages\n",
	    kdata_cnt, segkmem_cnt, kdata_cnt + segkmem_cnt));

	return (kdata_cnt + segkmem_cnt);
}


pgcnt_t
i_cpr_count_storage_pages(char *bitmap, bitfunc_t bitfunc)
{
	pgcnt_t count = 0;

	if (i_cpr_storage_desc_base) {
		count += cpr_count_pages((caddr_t)i_cpr_storage_desc_base,
		    (size_t)mmu_ptob(i_cpr_storage_desc_pgcnt),
		    bitmap, bitfunc);
	}
	if (i_cpr_storage_data_base) {
		count += cpr_count_pages(i_cpr_storage_data_base,
		    (size_t)mmu_ptob(i_cpr_storage_data_sz),
		    bitmap, bitfunc);
	}
	return (count);
}


/*
 * Derived from cpr_write_statefile().
 * Allocate (or reallocate after exhausting the supply) descriptors for each
 * chunk of contiguous sensitive kpages.
 */
static int
i_cpr_storage_desc_alloc(csd_t **basepp, pgcnt_t *pgsp, csd_t **endpp,
    int retry)
{
	pgcnt_t npages;
	int chunks;
	csd_t	*descp, *end;
	size_t	len;
	char *str = "i_cpr_storage_desc_alloc:";

	/*
	 * On initial allocation, add some extra to cover overhead caused
	 * by the allocation for the storage area later.
	 */
	if (retry == 0) {
		chunks = cpr_contig_pages(NULL, STORAGE_DESC_ALLOC) +
		    EXTRA_DESCS;
		npages = mmu_btopr(sizeof (**basepp) * (pgcnt_t)chunks);
		DEBUG7(errp("%s chunks %d, ", str, chunks));
	} else {
		DEBUG7(errp("%s retry %d: ", str, retry));
		npages = *pgsp + 1;
	}
	/* Free old descriptors, if any */
	if (*basepp)
		kmem_free((caddr_t)*basepp, mmu_ptob(*pgsp));

	descp = *basepp = kmem_alloc(mmu_ptob(npages), KM_NOSLEEP);
	if (descp == NULL) {
		DEBUG7(errp("%s no space for descriptors!\n", str));
		return (ENOMEM);
	}

	*pgsp = npages;
	len = mmu_ptob(npages);
	end = *endpp = descp + (len / (sizeof (**basepp)));
	DEBUG7(errp("npages 0x%x, len 0x%x, items 0x%x\n\t*basepp "
	    "%p, *endpp %p\n", npages, len, (len / (sizeof (**basepp))),
	    *basepp, *endpp));
	i_cpr_storage_desc_init(descp, npages, end);
	return (0);
}

static void
i_cpr_storage_desc_init(csd_t *descp, pgcnt_t npages, csd_t *end)
{
	size_t	len = mmu_ptob(npages);

	/* Initialize the descriptors to something impossible. */
	bzero(descp, len);
#ifdef	DEBUG
	/*
	 * This condition is tested by an ASSERT
	 */
	for (; descp < end; descp++)
		descp->csd_dirty_spfn = (u_int) -1;
#endif
}

int
i_cpr_dump_sensitive_kpages(vnode_t *vp)
{
	int	error = 0;
	u_int	spin_cnt = 0;
	csd_t	*descp;

	/*
	 * These following two variables need to be reinitialized
	 * for each cpr cycle.
	 */
	i_cpr_sensitive_bytes_dumped = 0;
	i_cpr_sensitive_pgs_dumped = 0;

	if (i_cpr_storage_desc_base) {
		for (descp = i_cpr_storage_desc_base;
		    descp <= i_cpr_storage_desc_last_used; descp++) {
			if (error = cpr_dump_sensitive(vp, descp))
				return (error);
			spin_cnt++;
			if ((spin_cnt & 0x5F) == 1)
				cpr_spinning_bar();
		}
		prom_printf(" \b");
	}

	DEBUG7(errp("\ni_cpr_dump_sensitive_kpages: dumped %d\n",
	    i_cpr_sensitive_pgs_dumped));
	return (0);
}


/*
 * 1. Fill the cpr page descriptor with the info of the dirty pages
 *    and
 *    write the descriptor out. It will be used at resume.
 * 2. Write the clean data in stead of the dirty data out.
 *    Note: to save space, the clean data is already compressed.
 */
static int
cpr_dump_sensitive(vnode_t *vp, csd_t *descp)
{
	int error = 0;
	caddr_t datap;
	cpd_t cpd;	/* cpr page descriptor */
	u_int	dirty_spfn;
	u_int dirty_npages, clean_sz;
	caddr_t	clean_sva;
	int	clean_compressed;
	extern u_char cpr_pagecopy[];

	dirty_spfn = descp->csd_dirty_spfn;
	dirty_npages = descp->csd_dirty_npages;
	clean_sva = (caddr_t)descp->csd_clean_sva;
	clean_sz = descp->csd_clean_sz;
	clean_compressed = descp->csd_clean_compressed;

	/* Fill cpr page descriptor. */
	cpd.cpd_magic = (u_int)CPR_PAGE_MAGIC;
	cpd.cpd_pfn = dirty_spfn;
	cpd.cpd_flag = 0;  /* must init to zero */
	cpd.cpd_pages = dirty_npages;

#ifdef	DEBUG
	if ((cpd.cpd_usum = descp->csd_usum) != 0)
		cpd.cpd_flag |= CPD_USUM;
	if ((cpd.cpd_csum = descp->csd_csum) != 0)
		cpd.cpd_flag |= CPD_CSUM;
#endif

	STAT->cs_dumped_statefsz += mmu_ptob(dirty_npages);

	/*
	 * The sensitive kpages are usually saved with compression
	 * unless compression could not reduce the size of the data.
	 * If user choose not to have the statefile compressed,
	 * we need to decompress the data back before dumping it to disk.
	 */
	if (CPR->c_flags & C_COMPRESSING) {
		cpd.cpd_length = clean_sz;
		datap = clean_sva;
		if (clean_compressed)
			cpd.cpd_flag |= CPD_COMPRESS;
	} else {
		if (clean_compressed) {
			cpd.cpd_length = decompress(clean_sva, cpr_pagecopy,
			    clean_sz, mmu_ptob(dirty_npages));
			datap = (caddr_t)cpr_pagecopy;
			ASSERT(cpd.cpd_length == mmu_ptob(dirty_npages));
		} else {
			cpd.cpd_length = clean_sz;
			datap = clean_sva;
		}
		cpd.cpd_csum = 0;
	}

	/* Write cpr page descriptor */
	error = cpr_write(vp, (caddr_t)&cpd, sizeof (cpd));
	if (error) {
		DEBUG7(errp("descp: %x\n", descp));
#ifdef DEBUG
		debug_enter("cpr_dump_sensitive: cpr_write() page "
			"descriptor failed!\n");
#endif
		return (error);
	}

	i_cpr_sensitive_bytes_dumped += sizeof (cpd_t);

	/* Write page data */
	error = cpr_write(vp, (caddr_t)datap, cpd.cpd_length);
	if (error) {
		DEBUG7(errp("error: %x\n", error));
		DEBUG7(errp("descp: %x\n", descp));
		DEBUG7(errp("cpr_write(%x, %x , %x)\n", vp, datap,
			cpd.cpd_length));
#ifdef DEBUG
		debug_enter("cpr_dump_sensitive: cpr_write() data failed!\n");
#endif
		return (error);
	}

	i_cpr_sensitive_bytes_dumped += cpd.cpd_length;
	i_cpr_sensitive_pgs_dumped += dirty_npages;

	return (error);
}


/*
 * Sanity check to make sure that we have dumped right amount
 * of pages from different sources to statefile.
 */
int
i_cpr_check_pgs_dumped(u_int pgs_expected, u_int regular_pgs_dumped)
{
	u_int total_pgs_dumped;

	total_pgs_dumped = regular_pgs_dumped + i_cpr_sensitive_pgs_dumped;

	DEBUG7(errp("\ncheck_pgs: reg %d + sens %d = %d, expect %d\n\n",
	    regular_pgs_dumped, i_cpr_sensitive_pgs_dumped,
	    total_pgs_dumped, pgs_expected));

	if (pgs_expected == total_pgs_dumped)
		return (0);

	return (EINVAL);
}


int
i_cpr_reusefini(void)
{
	struct vnode *vp;
	int rc = 0;
	struct cprinfo ci;
	char *bufp;
	size_t sz;

	if (cpr_reusable_mode)
		cpr_reusable_mode = 0;

	if (rc = cpr_open(cpr_default_path, FWRITE, &vp)) {
		if (rc == EROFS) {
			cmn_err(CE_CONT, "cpr: uadmin A_FREEZE AD_REUSEFINI "
			    "(uadmin %d %d)\nmust be done with / mounted "
			    "writeable.\n", A_FREEZE, AD_REUSEFINI);
			return (rc);
		} else {
			cmn_err(CE_WARN, "cpr: "
			    "Failed to open defaults file %s, errno = %d.",
			    cpr_default_path, rc);
			return (rc);
		}
	}

	if (rc = cpr_rdwr(UIO_READ, vp, &ci, sizeof (ci))) {
		cmn_err(CE_WARN, "cpr: Failed reading %s, errno = %d.",
		    cpr_default_path, rc);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (rc);
	}
	if (ci.ci_magic != CPR_DEFAULT_MAGIC) {
		cmn_err(CE_WARN, "cpr: bad magic number in %s, cannot restore "
		    "prom values for %s\n", cpr_default_path,
		    cpr_enumerate_promprops(&bufp, &sz));
		kmem_free(bufp, sz);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (rc);
	}

	/*
	 * clean up prom properties
	 */
	if ((rc = cpr_set_properties(&ci)) != 0)
		return (rc);

	ci.ci_magic = 0;	/* invalidate the disk copy */
	ci.ci_reusable = 0;	/* no longer in reusable mode */

	if (rc = cpr_rdwr(UIO_WRITE, vp, &ci, sizeof (ci)))
		cmn_err(CE_WARN, "cpr: Failed writing %s, errno = %d.",
		    cpr_default_path, rc);
	(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
	VN_RELE(vp);
	return (rc);
}

int
i_cpr_reuseinit(void)
{
	int rc = 0;
	int save = cpr_reusable_mode;
	struct cprinfo ci;

	/*
	 * read needed current openprom info.
	 */
	if (rc = cpr_get_bootinfo(&ci))
		return (rc);

	cpr_reusable_mode = 0;
	/*
	 * We need to validate cprinfo file
	 */
	if (rc = cpr_validate_cprinfo(&ci, 1)) {
		cpr_reusable_mode = save;
		if (rc == EROFS)
			cmn_err(CE_NOTE, "cpr: reuseinit must be performed "
			    "while / is mounted writeable");
		return (rc);
	}

	cpr_reusable_mode = 1;

	return (rc);
}

int
i_cpr_check_cprinfo()
{
	struct cprinfo ci;
	int rc = 0;
	struct vnode *vp;

	if ((rc = cpr_open(cpr_default_path, FREAD, &vp))) {
		if (rc == ENOENT)
			cmn_err(CE_NOTE, "cpr: cprinfo file does not "
			    "exist.  You must run 'uadmin %d %d' "
			    "command while / is mounted writeable,\n"
			    "then reboot and run 'uadmin %d %d' "
			    "to create a reusable statefile",
			    A_FREEZE, AD_REUSEINIT, A_FREEZE,
			    AD_REUSABLE);
		else
			cmn_err(CE_WARN, "cpr: Unable to open %s, errno = %d.",
			    cpr_default_path, rc);
		return (rc);
	}

	if (rc = cpr_rdwr(UIO_READ, vp, &ci, sizeof (ci))) {
		cmn_err(CE_WARN, "cpr: Failed reading %s, errno = %d.",
		    cpr_default_path, rc);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (rc);
	}
	if (ci.ci_magic != CPR_DEFAULT_MAGIC) {
		cmn_err(CE_CONT, "cpr: bad magic number in cprinfo file.\n"
		    "You  must do 'uadmin %d %d' command while / is mounted "
		    "writeable, then reboot and do 'uadmin %d %d' command "
		    "to create a reusable statefile.\n", A_FREEZE, AD_REUSEINIT,
		    A_FREEZE, AD_REUSABLE);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (EINVAL);
	}

	(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
	VN_RELE(vp);
	return (rc);
}

int
i_cpr_reusable_supported(void)
{
	return (1);
}


/*
 * find prom phys pages and alloc space for a tmp copy
 */
static int
i_cpr_find_ppages()
{
	extern struct vnode prom_ppages;
	struct page *pp;
	struct memlist *pmem;
	pgcnt_t npages, pcnt, scnt, vcnt;
	pfn_t ppn, plast, *dst;
	char *bitmap;

	cpr_clear_bitmaps();
	bitmap = REGULAR_BITMAP;

	/*
	 * there should be a page_t for each phys page used by the kernel;
	 * set a bit for each phys page not tracked by a page_t
	 */
	pcnt = 0;
	memlist_read_lock();
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		npages = mmu_btop(pmem->size);
		ppn = mmu_btop(pmem->address);
		for (plast = ppn + npages; ppn < plast; ppn++) {
			if (page_numtopp_nolock(ppn))
				continue;
			setbit(bitmap, ppn);
			pcnt++;
		}
	}
	memlist_read_unlock();

	/*
	 * clear bits for phys pages in each segment
	 */
	scnt = cpr_count_seg_pages(bitmap, cpr_clrbit);

	/*
	 * set bits for phys pages referenced by the prom_ppages vnode;
	 * these pages are mostly comprised of forthdebug words
	 */
	vcnt = 0;
	for (pp = prom_ppages.v_pages; pp; ) {
		if (cpr_setbit(pp->p_offset, bitmap) == 0)
			vcnt++;
		pp = pp->p_vpnext;
		if (pp == prom_ppages.v_pages)
			break;
	}

	/*
	 * total number of prom pages are:
	 * (non-page_t pages - seg pages + vnode pages)
	 */
	ppage_count = pcnt - scnt + vcnt;
	DEBUG1(errp("find_ppages: pcnt %ld - scnt %ld + vcnt %ld = %ld\n",
	    pcnt, scnt, vcnt, ppage_count));

	/*
	 * alloc array of pfn_t to store phys page list
	 */
	pphys_list_size = ppage_count * sizeof (pfn_t);
	pphys_list = kmem_alloc(pphys_list_size, KM_NOSLEEP);
	if (pphys_list == NULL) {
		cmn_err(CE_WARN, "cpr: cant alloc pphys_list");
		return (ENOMEM);
	}

	/*
	 * phys pages referenced in the bitmap should be
	 * those used by the prom; scan bitmap and save
	 * a list of prom phys page numbers
	 */
	dst = pphys_list;
	memlist_read_lock();
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		npages = mmu_btop(pmem->size);
		ppn = mmu_btop(pmem->address);
		for (plast = ppn + npages; ppn < plast; ppn++) {
			if (isset(bitmap, ppn)) {
				ASSERT(dst < (pphys_list + ppage_count));
				*dst++ = ppn;
			}
		}
	}
	memlist_read_unlock();

	/*
	 * allocate space to store prom pages
	 */
	ppage_buf = kmem_alloc(mmu_ptob(ppage_count), KM_NOSLEEP);
	if (ppage_buf == NULL) {
		kmem_free(pphys_list, pphys_list_size);
		pphys_list = NULL;
		cmn_err(CE_WARN, "cpr: cant alloc ppage_buf");
		return (ENOMEM);
	}

	return (0);
}


/*
 * save prom pages to kmem pages
 */
static void
i_cpr_save_ppages(void)
{
	pfn_t *pphys, *plast;
	caddr_t dst;

	/*
	 * map in each prom page and copy to a kmem page
	 */
	dst = ppage_buf;
	plast = pphys_list + ppage_count;
	for (pphys = pphys_list; pphys < plast; pphys++) {
		i_cpr_mapin(cpr_vaddr, 1, *pphys);
		bcopy(cpr_vaddr, dst, MMU_PAGESIZE);
		i_cpr_mapout(cpr_vaddr, 1);
		dst += MMU_PAGESIZE;
	}

	DEBUG1(errp("saved %d prom pages\n", ppage_count));
}


/*
 * restore prom pages from kmem pages
 */
static void
i_cpr_restore_ppages(void)
{
	pfn_t *pphys, *plast;
	caddr_t src;

	/*
	 * map in each prom page and copy from a kmem page
	 */
	src = ppage_buf;
	plast = pphys_list + ppage_count;
	for (pphys = pphys_list; pphys < plast; pphys++) {
		i_cpr_mapin(cpr_vaddr, 1, *pphys);
		bcopy(src, cpr_vaddr, MMU_PAGESIZE);
		i_cpr_mapout(cpr_vaddr, 1);
		src += MMU_PAGESIZE;
	}

	DEBUG1(errp("restored %d prom pages\n", ppage_count));
}


/*
 * save/restore prom pages or free related allocs
 */
int
i_cpr_prom_pages(int action)
{
	int error;

	if (action == CPR_PROM_SAVE) {
		if (ppage_buf == NULL) {
			ASSERT(pphys_list == NULL);
			if (error = i_cpr_find_ppages())
				return (error);
			i_cpr_save_ppages();
		}
	} else if (action == CPR_PROM_RESTORE) {
		i_cpr_restore_ppages();
	} else if (action == CPR_PROM_FREE) {
		if (pphys_list) {
			ASSERT(pphys_list_size);
			kmem_free(pphys_list, pphys_list_size);
			pphys_list = NULL;
			pphys_list_size = 0;
		}
		if (ppage_buf) {
			ASSERT(ppage_count);
			kmem_free(ppage_buf, mmu_ptob(ppage_count));
			DEBUG1(errp("freed %d prom pages\n", ppage_count));
			ppage_buf = NULL;
			ppage_count = 0;
		}
	}
	return (0);
}


/* ARGSUSED */
int
i_cpr_dump_setup(vnode_t *vp)
{
	i_cpr_tlb_info(CPR_INIT_TI);
	return (0);
}


int
i_cpr_is_supported(void)
{
	char es_prop[] = "energystar-v2";
	dnode_t node;
	int last;

	node = prom_rootnode();
	if (prom_getproplen(node, es_prop) != -1)
		return (1);
	last = strlen(es_prop) - 1;
	es_prop[last] = '3';
	return (prom_getproplen(node, es_prop) != -1);
}


/*
 * the actual size of the statefile data isn't known until after all the
 * compressed pages are written; even the inode size doesn't reflect the
 * data size since there are usually many extra fs blocks.  for recording
 * the actual data size, the first sector of the statefile is copied to
 * a tmp buf, and the copy is later updated and flushed to disk.
 */
int
i_cpr_blockzero(u_char *base, u_char **bufpp, int *blkno, vnode_t *vp)
{
	extern int cpr_flush_write(vnode_t *);
	static u_char cpr_sector[DEV_BSIZE];
	cpr_ext bytes, *dst;

	/*
	 * this routine is called after cdd_t and csu_md_t are copied
	 * to cpr_buf; mini-hack alert: the save/update method creates
	 * a dependency on the combined struct size being >= one sector
	 * or DEV_BSIZE; since introduction in Sol2.7, csu_md_t size is
	 * over 1K bytes and will probably grow with any changes.
	 *
	 * copy when vp is NULL, flush when non-NULL
	 */
	if (vp == NULL) {
		ASSERT((*bufpp - base) >= DEV_BSIZE);
		bcopy(base, cpr_sector, sizeof (cpr_sector));
		return (0);
	} else {
		bytes = dbtob(*blkno);
		dst = &((cdd_t *)cpr_sector)->cdd_filesize;
		bcopy(&bytes, dst, sizeof (bytes));
		bcopy(cpr_sector, base, sizeof (cpr_sector));
		*bufpp = base + sizeof (cpr_sector);
		*blkno = 0;
		DEBUG1(errp("statefile data size: %lld\n\n", bytes));
		return (cpr_flush_write(vp));
	}
}
