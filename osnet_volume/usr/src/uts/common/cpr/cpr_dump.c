/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_dump.c	1.73	99/12/04 SMI"

/*
 * Fill in and write out the cpr state file
 *	1. Allocate and write headers, ELF and cpr dump header
 *	2. Allocate bitmaps according to phys_install
 *	3. Tag kernel pages into corresponding bitmap
 *	4. Write bitmaps to state file
 *	5. Write actual physical page data to state file
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/memlist.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/cpr.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/panic.h>

/* Local defines and variables */
#define	BTOb(bytes)	((bytes) << 3)		/* Bytes to bits, log2(NBBY) */
#define	bTOB(bits)	((bits) >> 3)		/* bits to Bytes, log2(NBBY) */

static u_int cpr_pages_tobe_dumped;
static u_int cpr_regular_pgs_dumped;

static int cpr_dump_regular_pages(vnode_t *);
static int cpr_count_upages(char *, bitfunc_t);
static int cpr_compress_and_write(vnode_t *, u_int, pfn_t, pgcnt_t);
int cpr_flush_write(vnode_t *);

int cpr_contig_pages(vnode_t *, int);
pgcnt_t cpr_count_kpages(char *, bitfunc_t);
pgcnt_t cpr_count_volatile_pages(char *, bitfunc_t);

void cpr_clear_bitmaps();

extern int i_cpr_dump_setup(vnode_t *);
extern int i_cpr_blockzero(u_char *, u_char **, int *, vnode_t *);
extern int cpr_test_mode;

ctrm_t cpr_term;

u_char *cpr_buf;
size_t cpr_buf_size;

u_char *cpr_pagedata;
size_t cpr_pagedata_size;

static u_char *cpr_wptr;	/* keep track of where to write to next */
static int cpr_file_bn;		/* cpr state-file block offset */
static int cpr_disk_writes_ok;
static size_t cpr_dev_space = 0;

u_char cpr_pagecopy[CPR_MAXCONTIG * MMU_PAGESIZE];

#define	WITHIN_SEG(addr, size, segp) \
	(((addr) >= (segp)->s_base) && \
	((((addr) + (size)) <= ((segp)->s_base + (segp)->s_size))))


/*
 * Allocate pages for buffers used in writing out the statefile
 */
static int
cpr_alloc_bufs(void)
{
	char *allocerr;

	allocerr = "cpr: Unable to allocate memory for cpr buffer";
	cpr_buf_size = mmu_btopr(CPRBUFSZ);
	cpr_buf = kmem_alloc(mmu_ptob(cpr_buf_size), KM_NOSLEEP);
	if (cpr_buf == NULL) {
		cmn_err(CE_WARN, allocerr);
		return (ENOMEM);
	}

	cpr_pagedata_size = (CPR_MAXCONTIG + 1);
	cpr_pagedata = kmem_alloc(mmu_ptob(cpr_pagedata_size), KM_NOSLEEP);
	if (cpr_pagedata == NULL) {
		kmem_free(cpr_buf, mmu_ptob(cpr_buf_size));
		cpr_buf = NULL;
		cmn_err(CE_WARN, allocerr);
		return (ENOMEM);
	}

	return (0);
}


/*
 * Allocate bitmaps according to phys_install memlist in the kernel.
 * Two types of bitmaps will be created, REGULAR_BITMAP and
 * VOLATILE_BITMAP. REGULAR_BITMAP contains kernel and some user pages
 * that need to be resumed. VOLATILE_BITMAP contains data used during
 * cpr_dump. Since the contents of volatile pages are no longer needed
 * after statefile is dumped to disk, they will not be saved. However
 * they need to be claimed during resume so that the resumed kernel
 * can free them. That's why we need two types of bitmaps. Remember
 * they can not be freed during suspend because they are used during
 * the process of dumping the statefile.
 */
static int
cpr_alloc_bitmaps(void)
{
	pgcnt_t map_bits;
	size_t map_bytes;
	cbd_t *dp;

	/*
	 * create bitmaps according to physmax
	 */
	map_bits = physmax + 1;
	map_bytes = bTOB(map_bits);

	DEBUG1(errp("bitmap bytes 0x%x, phys page range (0x0 - 0x%lx)\n",
	    map_bytes, physmax));

	dp = &CPR->c_bitmap_desc;
	dp->cbd_reg_bitmap = (cpr_ptr)kmem_alloc(map_bytes, KM_NOSLEEP);
	if (dp->cbd_reg_bitmap == NULL)
		return (ENOMEM);
	dp->cbd_vlt_bitmap = (cpr_ptr)kmem_alloc(map_bytes, KM_NOSLEEP);
	if (dp->cbd_vlt_bitmap == NULL) {
		kmem_free((void *)dp->cbd_reg_bitmap, map_bytes);
		dp->cbd_reg_bitmap = NULL;
		return (ENOMEM);
	}

	dp->cbd_magic = CPR_BITMAP_MAGIC;
	dp->cbd_spfn = 0;
	dp->cbd_epfn = physmax;
	dp->cbd_size = map_bytes;

	return (0);
}


/*
 * CPR dump header contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. kernel return pc & ppn for resume
 *	3. current thread info
 *	4. debug level and test mode
 *	5. number of bitmaps allocated
 *	6. number of page records
 */
static int
cpr_write_header(vnode_t *vp)
{
	extern u_short cpr_mach_type;
	struct memlist *pmem;
	struct cpr_dump_desc cdump;
	pgcnt_t tot_physpgs, bitmap_pages;
	pgcnt_t kpages, vpages, upages;

	cdump.cdd_magic = (u_int)CPR_DUMP_MAGIC;
	cdump.cdd_version = CPR_VERSION;
	cdump.cdd_machine = cpr_mach_type;
	cdump.cdd_debug = cpr_debug;
	cdump.cdd_test_mode = cpr_test_mode;
	cdump.cdd_bitmaprec = 1;

	cpr_clear_bitmaps();

	/*
	 * Remember how many pages we plan to save to statefile.
	 * This information will be used for sanity checks.
	 * Untag those pages that will not be saved to statefile.
	 */
	kpages = cpr_count_kpages(REGULAR_BITMAP, cpr_setbit);
	vpages = cpr_count_volatile_pages(REGULAR_BITMAP, cpr_clrbit);
	upages = cpr_count_upages(REGULAR_BITMAP, cpr_setbit);
	cdump.cdd_dumppgsize = kpages - vpages + upages;
	cpr_pages_tobe_dumped = cdump.cdd_dumppgsize;
	DEBUG7(errp(
	    "\ncpr_write_header: kpages %ld - vpages %ld + upages %ld = %d\n",
	    kpages, vpages, upages, cdump.cdd_dumppgsize));

	/*
	 * Some pages contain volatile data (cpr_buf and storage area for
	 * sensitive kpages), which are no longer needed after the statefile
	 * is dumped to disk.  We have already untagged them from regular
	 * bitmaps.  Now tag them into the volatile bitmaps.  The pages in
	 * volatile bitmaps will be claimed during resume, and the resumed
	 * kernel will free them.
	 */
	(void) cpr_count_volatile_pages(VOLATILE_BITMAP, cpr_setbit);

	/*
	 * Find out how many pages of bitmap are needed to represent
	 * the physical memory.
	 */
	tot_physpgs = 0;
	memlist_read_lock();
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		tot_physpgs += mmu_btop(pmem->size);
	}
	memlist_read_unlock();
	bitmap_pages = mmu_btop(PAGE_ROUNDUP(bTOB(tot_physpgs)));

	/*
	 * Export accurate statefile size for statefile allocation retry.
	 * statefile_size = all the headers + total pages +
	 * number of pages used by the bitmaps.
	 * Roundup will be done in the file allocation code.
	 */
	STAT->cs_nocomp_statefsz = sizeof (cdd_t) + sizeof (cmd_t) +
		(sizeof (cbd_t) * cdump.cdd_bitmaprec) +
		(sizeof (cpd_t) * cdump.cdd_dumppgsize) +
		mmu_ptob(cdump.cdd_dumppgsize + bitmap_pages);

	/*
	 * If the estimated statefile is not big enough,
	 * go retry now to save un-necessary operations.
	 */
	if (!(CPR->c_flags & C_COMPRESSING) &&
		(STAT->cs_nocomp_statefsz > STAT->cs_est_statefsz)) {
		if (cpr_debug & (LEVEL1 | LEVEL7))
		    errp("cpr_write_header: STAT->cs_nocomp_statefsz > "
			"STAT->cs_est_statefsz\n");
		return (ENOSPC);
	}

	/* now write cpr dump descriptor */
	return (cpr_write(vp, (caddr_t)&cdump, sizeof (cdd_t)));
}


/*
 * CPR dump tail record contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. all misc info that needs to be passed to cprboot or resumed kernel
 */
static int
cpr_write_terminator(vnode_t *vp)
{
	cpr_term.magic = (u_int)CPR_TERM_MAGIC;
	cpr_term.va = (cpr_ptr)&cpr_term;
	cpr_term.pfn = (cpr_ext)va_to_pfn(&cpr_term);

	/* count the last one (flush) */
	cpr_term.real_statef_size = STAT->cs_real_statefsz +
		btod(cpr_wptr - cpr_buf) * DEV_BSIZE;

	DEBUG9(errp("cpr_dump: Real Statefile Size: %d\n",
		STAT->cs_real_statefsz));

	cpr_tod_get(&cpr_term.tm_shutdown);

	return (cpr_write(vp, (caddr_t)&cpr_term, sizeof (cpr_term)));
}

/*
 * write bitmap desc, regular and volatile bitmap to the state file.
 */
static int
cpr_write_bitmap(vnode_t *vp)
{
	uchar_t *rmap, *vmap, *dst, *tail;
	int bytes, error;
	size_t size;
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	if (error = cpr_write(vp, (caddr_t)dp, sizeof (*dp)))
		return (error);

	/*
	 * merge regular and volatile bitmaps into tmp space
	 * and write to disk
	 */
	rmap = (uchar_t *)dp->cbd_reg_bitmap;
	vmap = (uchar_t *)dp->cbd_vlt_bitmap;
	for (size = dp->cbd_size; size; size -= bytes) {
		bytes = min(size, sizeof (cpr_pagecopy));
		tail = &cpr_pagecopy[bytes];
		for (dst = cpr_pagecopy; dst < tail; dst++)
			*dst = *rmap++ | *vmap++;
		if (error = cpr_write(vp, (caddr_t)cpr_pagecopy, bytes))
			return (error);
	}

	return (0);
}


static int
cpr_write_statefile(vnode_t *vp)
{
	u_int error = 0;
	extern	int	i_cpr_check_pgs_dumped();
	void flush_windows();
	pgcnt_t spages;
	char *str;

	flush_windows();

	/*
	 * to get an accurate view of kas, we need to untag sensitive
	 * pages *before* dumping them because the disk driver makes
	 * allocations and changes kas along the way.  The remaining
	 * pages referenced in the bitmaps are dumped out later as
	 * regular kpages.
	 */
	str = "cpr_write_statefile:";
	spages = i_cpr_count_sensitive_kpages(REGULAR_BITMAP, cpr_clrbit);
	DEBUG7(errp("%s untag %ld sens pages\n", str, spages));

	/*
	 * now it's OK to call a driver that makes allocations
	 */
	cpr_disk_writes_ok = 1;

	/*
	 * now write out the clean sensitive kpages
	 * according to the sensitive descriptors
	 */
	error = i_cpr_dump_sensitive_kpages(vp);
	if (error) {
		DEBUG7(errp("%s cpr_dump_sensitive_kpages() failed!\n", str));
		return (error);
	}

	/*
	 * cpr_dump_regular_pages() counts cpr_regular_pgs_dumped
	 */
	error = cpr_dump_regular_pages(vp);
	if (error) {
		DEBUG7(errp("%s cpr_dump_regular_pages() failed!\n", str));
		return (error);
	}

	/*
	 * sanity check to verify the right number of pages were dumped
	 */
	error = i_cpr_check_pgs_dumped(cpr_pages_tobe_dumped,
	    cpr_regular_pgs_dumped);

	if (error) {
		errp("\n%s page count mismatch!\n", str);
#ifdef DEBUG
		if (cpr_test_mode)
			debug_enter(NULL);
#endif
	}

	return (error);
}


/*
 * creates the CPR state file, the following sections are
 * written out in sequence:
 *    - writes the cpr dump header
 *    - writes the memory usage bitmaps
 *    - writes the platform dependent info
 *    - writes the remaining user pages
 *    - writes the kernel pages
 */
int
cpr_dump(vnode_t *vp)
{
	int error;

	if (cpr_buf == NULL) {
		ASSERT(cpr_pagedata == NULL);
		if (error = cpr_alloc_bufs())
			return (error);
	}
	/* point to top of internal buffer */
	cpr_wptr = cpr_buf;

	/* initialize global variables used by the write operation */
	cpr_file_bn = 0;
	cpr_dev_space = 0;

	/* allocate bitmaps */
	if (CPR->c_bitmap_desc.cbd_reg_bitmap == NULL) {
		ASSERT(CPR->c_bitmap_desc.cbd_vlt_bitmap == NULL);
		if (error = cpr_alloc_bitmaps()) {
			cmn_err(CE_WARN, "cpr: cant allocate bitmaps");
			return (error);
		}
	}

	if (error = i_cpr_prom_pages(CPR_PROM_SAVE))
		return (error);

	if (error = i_cpr_dump_setup(vp))
		return (error);

	/*
	 * set internal cross checking; we dont want to call
	 * a disk driver that makes allocations until after
	 * sensitive pages are saved
	 */
	cpr_disk_writes_ok = 0;

	/*
	 * 1253112: heap corruption due to memory allocation when dumpping
	 *	    statefile.
	 * Theoretically on Sun4u only the kernel data nucleus, kvalloc and
	 * kvseg segments can be contaminated should memory allocations happen
	 * during sddump, which is not supposed to happen after the system
	 * is quiesced. Let's call the kernel pages that tend to be affected
	 * 'sensitive kpages' here. To avoid saving inconsistent pages, we
	 * will allocate some storage space to save the clean sensitive pages
	 * aside before statefile dumping takes place. Since there may not be
	 * much memory left at this stage, the sensitive pages will be
	 * compressed before they are saved into the storage area.
	 */
	if (error = i_cpr_save_sensitive_kpages()) {
		DEBUG7(errp("cpr_dump: save_sensitive_kpages failed!\n"));
		return (error);
	}

	/*
	 * since all cpr allocations are done (space for sensitive kpages,
	 * bitmaps, cpr_buf), kas is stable, and now we can accurately
	 * count regular and sensitive kpages.
	 */
	if (error = cpr_write_header(vp)) {
		DEBUG7(errp("cpr_dump: cpr_write_header() failed!\n"));
		return (error);
	}

	if (error = i_cpr_write_machdep(vp))
		return (error);

	if (error = i_cpr_blockzero(cpr_buf, &cpr_wptr, NULL, NULL))
		return (error);

	if (error = cpr_write_bitmap(vp))
		return (error);

	if (error = cpr_write_statefile(vp)) {
		DEBUG7(errp("cpr_dump: cpr_write_statefile() failed!\n"));
		return (error);
	}

	if (error = cpr_write_terminator(vp))
		return (error);

	if (error = cpr_flush_write(vp))
		return (error);

	if (error = i_cpr_blockzero(cpr_buf, &cpr_wptr, &cpr_file_bn, vp))
		return (error);

	return (0);
}


/*
 * cpr_walk() is called many 100x with a range within kvseg;
 * a page-count from each range is accumulated at arg->pages
 */
static void
cpr_walk(void *arg, void *base, size_t size)
{
	struct cpr_walkinfo *cwip = arg;

	cwip->pages += cpr_count_pages(base, size, cwip->bitmap, cwip->bitfunc);
	cwip->size += size;
	cwip->ranges++;
}


/*
 * faster scan of kvseg using vmem_walk() to visit allocated ranges
 */
pgcnt_t
cpr_scan_kvseg(char *bitmap, bitfunc_t bitfunc)
{
	struct cpr_walkinfo cwinfo;

	bzero(&cwinfo, sizeof (cwinfo));
	cwinfo.bitmap = bitmap;
	cwinfo.bitfunc = bitfunc;
	vmem_walk(heap_arena, VMEM_ALLOC, cpr_walk, &cwinfo);

	if (cpr_debug & LEVEL7) {
		errp("walked %d sub-ranges, total pages %ld\n",
		    cwinfo.ranges, mmu_btop(cwinfo.size));
		cpr_show_range(kvseg.s_base, kvseg.s_size,
		    bitmap, bitfunc, cwinfo.pages);
	}

	return (cwinfo.pages);
}


/*
 * count pages within each segment; special handling is done for kvseg,
 * particularly for sun4u/LP64 where kvseg.s_size is 64 GB; each call
 * to cpr_count_pages() for kvseg was taking 18+ seconds, the newer
 * vmem_walk() method completes within 1 second
 */
pgcnt_t
cpr_count_seg_pages(char *bitmap, bitfunc_t bitfunc)
{
	struct seg *segp;
	pgcnt_t pages;

	pages = 0;
	for (segp = AS_SEGP(&kas, kas.a_segs); segp;
	    segp = AS_SEGP(&kas, segp->s_next)) {
		if (segp != &kvseg)
			pages += cpr_count_pages(segp->s_base,
			    segp->s_size, bitmap, bitfunc);
	}
	pages += cpr_scan_kvseg(bitmap, bitfunc);

	return (pages);
}


/*
 * count kernel pages within kas and any special ranges
 */
pgcnt_t
cpr_count_kpages(char *bitmap, bitfunc_t bitfunc)
{
	pgcnt_t kas_cnt;

	/*
	 * Some pages need to be taken care of differently.
	 * eg: panicbuf pages of sun4m are not in kas but they need
	 * to be saved.  On sun4u, the physical pages of panicbuf are
	 * allocated via prom_retain().
	 */
	kas_cnt = i_cpr_count_special_kpages(bitmap, bitfunc);
	kas_cnt += cpr_count_seg_pages(bitmap, bitfunc);

	DEBUG9(errp("cpr_count_kpages: kas_cnt=%d\n", kas_cnt));
	DEBUG7(errp("\ncpr_count_kpages: %ld pages, 0x%lx bytes\n",
		kas_cnt, mmu_ptob(kas_cnt)));
	return (kas_cnt);
}


/*
 * Deposit the tagged page to the right bitmap.
 */
int
cpr_setbit(pfn_t ppn, char *bitmap)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	ASSERT(ppn >= dp->cbd_spfn && ppn <= dp->cbd_epfn);

	ASSERT(bitmap);
	if (isclr(bitmap, ppn)) {
		setbit(bitmap, ppn);
		return (0);
	}

	/* already mapped */
	return (1);
}

/*
 * Clear the bitmap corresponding to a page frame.
 */
int
cpr_clrbit(pfn_t ppn, char *bitmap)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	ASSERT(ppn >= dp->cbd_spfn && ppn <= dp->cbd_epfn);

	ASSERT(bitmap);
	if (isset(bitmap, ppn)) {
		clrbit(bitmap, ppn);
		return (0);
	}

	/* not mapped */
	return (1);
}


/* ARGSUSED */
int
cpr_nobit(pfn_t ppn, char *bitmap)
{
	return (0);
}


/*
 * Go thru all pages and pick up any page not caught during the invalidation
 * stage. This is also used to save pages with cow lock or phys page lock held
 * (none zero p_lckcnt or p_cowcnt)
 */
static	int
cpr_count_upages(char *bitmap, bitfunc_t bitfunc)
{
	page_t *pp, *page0;
	pgcnt_t dcnt = 0, tcnt = 0;
	pfn_t pfn;

	page0 = pp = page_first();

	do {
#ifdef sparc
		extern struct vnode prom_ppages;
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    pp->p_vnode == &prom_ppages ||
			PP_ISFREE(pp) && PP_ISAGED(pp))
#else
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    PP_ISFREE(pp) && PP_ISAGED(pp))
#endif sparc
			continue;

		pfn = page_pptonum(pp);
		if (pf_is_memory(pfn)) {
			tcnt++;
			if ((*bitfunc)(pfn, bitmap) == 0)
				dcnt++; /* dirty count */
		}
	} while ((pp = page_next(pp)) != page0);

	STAT->cs_upage2statef = dcnt;
	DEBUG9(errp("cpr_count_upages: dirty=%ld total=%ld\n",
		dcnt, tcnt));
	DEBUG7(errp("cpr_count_upages: %ld pages, 0x%lx bytes\n",
		dcnt, mmu_ptob(dcnt)));
	return (dcnt);
}


/*
 * try compressing pages based on cflag,
 * and for DEBUG kernels, verify uncompressed data checksum;
 *
 * this routine replaces common code from
 * i_cpr_compress_and_save() and cpr_compress_and_write()
 */
uchar_t *
cpr_compress_pages(cpd_t *dp, pgcnt_t pages, int cflag)
{
	size_t nbytes, clen, len;
	uint32_t test_sum;
	uchar_t *datap;

	nbytes = mmu_ptob(pages);

	/*
	 * set length to the original uncompressed data size;
	 * always init cpd_flag to zero
	 */
	dp->cpd_length = nbytes;
	dp->cpd_flag = 0;

#ifdef	DEBUG
	/*
	 * Make a copy of the uncompressed data so we can checksum it.
	 * Compress that copy so the checksum works at the other end
	 */
	bcopy(CPR->c_mapping_area, cpr_pagecopy, nbytes);
	dp->cpd_usum = checksum32(cpr_pagecopy, nbytes);
	dp->cpd_flag |= CPD_USUM;
	datap = cpr_pagecopy;
#else
	datap = (uchar_t *)CPR->c_mapping_area;
	dp->cpd_usum = 0;
#endif

	/*
	 * try compressing the raw data to cpr_pagedata;
	 * if there was a size reduction: record the new length,
	 * flag the compression, and point to the compressed data.
	 */
	dp->cpd_csum = 0;
	if (cflag) {
		clen = compress(datap, cpr_pagedata, nbytes);
		if (clen < nbytes) {
			dp->cpd_flag |= CPD_COMPRESS;
			dp->cpd_length = clen;
			datap = cpr_pagedata;
#ifdef	DEBUG
			dp->cpd_csum = checksum32(datap, clen);
			dp->cpd_flag |= CPD_CSUM;

			/*
			 * decompress the data back to a scratch area
			 * and compare the new checksum with the original
			 * checksum to verify the compression.
			 */
			bzero(cpr_pagecopy, sizeof (cpr_pagecopy));
			len = decompress(datap, cpr_pagecopy,
			    clen, sizeof (cpr_pagecopy));
			test_sum = checksum32(cpr_pagecopy, len);
			ASSERT(test_sum == dp->cpd_usum);
#endif
		}
	}

	return (datap);
}


/*
 * 1. Prepare cpr page descriptor and write it to file
 * 2. Compress page data and write it out
 */
static int
cpr_compress_and_write(vnode_t *vp, u_int va, pfn_t pfn, pgcnt_t npg)
{
	int error = 0;
	uchar_t *datap;
	cpd_t cpd;	/* cpr page descriptor */
	extern void i_cpr_mapin(caddr_t, u_int, pfn_t);
	extern void i_cpr_mapout(caddr_t, u_int);

	i_cpr_mapin(CPR->c_mapping_area, npg, pfn);

	DEBUG3(errp("mapped-in %d pages, vaddr 0x%p, pfn 0x%x\n",
		npg, CPR->c_mapping_area, pfn));

	/*
	 * Fill cpr page descriptor.
	 */
	cpd.cpd_magic = (u_int)CPR_PAGE_MAGIC;
	cpd.cpd_pfn = pfn;
	cpd.cpd_pages = npg;

	STAT->cs_dumped_statefsz += mmu_ptob(npg);

	datap = cpr_compress_pages(&cpd, npg, CPR->c_flags & C_COMPRESSING);

	/* Write cpr page descriptor */
	error = cpr_write(vp, (caddr_t)&cpd, sizeof (cpd_t));

	/* Write compressed page data */
	error = cpr_write(vp, (caddr_t)datap, cpd.cpd_length);

	/*
	 * Unmap the pages for tlb and vac flushing
	 */
	i_cpr_mapout(CPR->c_mapping_area, npg);

	if (error) {
		DEBUG1(errp("cpr_compress_and_write: vp 0x%p va 0x%x ",
		    vp, va));
		DEBUG1(errp("pfn 0x%lx blk %d err %d\n",
		    pfn, cpr_file_bn, error));
	} else {
		cpr_regular_pgs_dumped += npg;
	}

	return (error);
}


int
cpr_write(vnode_t *vp, caddr_t buffer, int size)
{
	int	error, count;
	caddr_t	fromp = buffer;

	/*
	 * This is a more expensive operation for the char special statefile,
	 * so we only compute it once; We can't precompute it in the ufs
	 * case because we grow the file and try again if it is too small
	 */
	if (cpr_dev_space == 0) {
		if (vp->v_type == VBLK) {
#ifdef _LP64
			/*
			 * for now we check the "Size" property
			 * only on 64-bit systems.
			 */
			cpr_dev_space = cdev_Size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = cdev_size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_Size(vp->v_rdev) *
				    DEV_BSIZE;
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_size(vp->v_rdev) *
				    DEV_BSIZE;
			ASSERT(cpr_dev_space);
#else
			cpr_dev_space = cdev_size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_size(vp->v_rdev) *
				    DEV_BSIZE;
			ASSERT(cpr_dev_space);
#endif /* _LP64 */
		} else {
			cpr_dev_space = 1;	/* not used in this case */
		}
	}

	/*
	 * break the write into multiple part if request is large,
	 * calculate count up to buf page boundary, then write it out.
	 * repeat until done.
	 */

	while (size > 0) {
		count = MIN(size, cpr_buf + CPRBUFSZ - cpr_wptr);

		bcopy(fromp, (caddr_t)cpr_wptr, count);

		cpr_wptr += count;
		fromp += count;
		size -= count;

		if (cpr_wptr < cpr_buf + CPRBUFSZ)
			return (0);	/* buffer not full yet */
		ASSERT(cpr_wptr == cpr_buf + CPRBUFSZ);

		if (vp->v_type == VBLK) {
			if (dtob(cpr_file_bn + CPRBUFS) > cpr_dev_space)
				return (ENOSPC);
		} else {
			if (dbtob(cpr_file_bn+CPRBUFS) > VTOI(vp)->i_size)
				return (ENOSPC);
		}

		do_polled_io = 1;
		DEBUG3(errp("cpr_write: frmp=%x wptr=%x cnt=%x...",
			fromp, cpr_wptr, count));
		/*
		 * cross check, this should not happen!
		 */
		if (cpr_disk_writes_ok == 0) {
			errp("cpr_write: disk write too early!\n");
			return (EINVAL);
		}

		error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, CPRBUFS);
		DEBUG3(errp("done\n"));
		do_polled_io = 0;

		STAT->cs_real_statefsz += CPRBUFSZ;

		if (error) {
			cmn_err(CE_WARN, "cpr_write error %d", error);
			return (error);
		}
		cpr_file_bn += CPRBUFS;	/* Increment block count */
		cpr_wptr = cpr_buf;	/* back to top of buffer */
	}
	return (0);
}


int
cpr_flush_write(vnode_t *vp)
{
	int	nblk;
	int	error;

	/*
	 * Calculate remaining blocks in buffer, rounded up to nearest
	 * disk block
	 */
	nblk = btod(cpr_wptr - cpr_buf);

	do_polled_io = 1;
	error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, nblk);
	do_polled_io = 0;

	cpr_file_bn += nblk;
	if (error)
		DEBUG2(errp("cpr_flush_write: error (%d)\n", error));
	return (error);
}

void
cpr_clear_bitmaps(void)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	bzero(REGULAR_BITMAP, (size_t)dp->cbd_size);
	bzero(VOLATILE_BITMAP, (size_t)dp->cbd_size);
	DEBUG7(errp("\ncleared reg and vlt bitmaps\n"));
}

int
cpr_contig_pages(vnode_t *vp, int flag)
{
	int chunks = 0, error = 0;
	pgcnt_t i, j, totbit;
	pfn_t spfn;
	cbd_t *dp;
	u_int	spin_cnt = 0;
	extern	int i_cpr_compress_and_save();

	dp = &CPR->c_bitmap_desc;
	spfn = dp->cbd_spfn;
	totbit = BTOb(dp->cbd_size);
	i = 0; /* Beginning of bitmap */
	j = 0;
	while (i < totbit) {
		while ((j < CPR_MAXCONTIG) && ((j + i) < totbit)) {
			if (isset((char *)dp->cbd_reg_bitmap, j+i))
				j++;
			else /* not contiguous anymore */
				break;
		}

		if (j) {
			chunks++;
			if (flag == SAVE_TO_STORAGE) {
				error = i_cpr_compress_and_save(
				    chunks, spfn + i, j);
				if (error)
					return (error);
			} else if (flag == WRITE_TO_STATEFILE) {
				error = cpr_compress_and_write(vp, 0,
				    spfn + i, j);
				if (error)
					return (error);
				else {
					spin_cnt++;
					if ((spin_cnt & 0x5F) == 1)
						cpr_spinning_bar();
				}
			}
		}

		i += j;
		if (j != CPR_MAXCONTIG) {
			/* Stopped on a non-tagged page */
			i++;
		}

		j = 0;
	}

	if (flag == STORAGE_DESC_ALLOC)
		return (chunks);
	else
		return (0);
}


void
cpr_show_range(caddr_t vaddr, size_t size,
    char *bitmap, bitfunc_t bitfunc, pgcnt_t count)
{
	char *action, *bname;

	bname = (bitmap == REGULAR_BITMAP) ? "regular" : "volatile";
	if (bitfunc == cpr_setbit)
		action = "tag";
	else if (bitfunc == cpr_clrbit)
		action = "untag";
	else
		action = "none";
	errp("range (0x%p, 0x%p), %s bitmap, %s %ld\n",
	    vaddr, vaddr + size, bname, action, count);
}


pgcnt_t
cpr_count_pages(caddr_t sva, size_t size, char *bitmap, bitfunc_t bitfunc)
{
	caddr_t	va, eva;
	pfn_t pfn;
	pgcnt_t count = 0;

	eva = sva + PAGE_ROUNDUP(size);
	for (va = sva; va < eva; va += MMU_PAGESIZE) {
		pfn = va_to_pfn(va);
		if (pfn != PFN_INVALID && pf_is_memory(pfn)) {
			if ((*bitfunc)(pfn, bitmap) == 0)
				count++;
		}
	}

	/*
	 * heap_arena is fragmented into hundreds of allocated ranges
	 * so we skip reporting them here; the entire range of kvseg
	 * is reported once externally; see cpr_scan_kvseg()
	 */
	if ((cpr_debug & LEVEL7) && !WITHIN_SEG(sva, size, &kvseg))
		cpr_show_range(sva, size, bitmap, bitfunc, count);

	return (count);
}


pgcnt_t
cpr_count_volatile_pages(char *bitmap, bitfunc_t bitfunc)
{
	pgcnt_t count = 0;

	if (cpr_buf) {
		count += cpr_count_pages((caddr_t)cpr_buf,
		    mmu_ptob(cpr_buf_size), bitmap, bitfunc);
	}
	if (cpr_pagedata) {
		count += cpr_count_pages((caddr_t)cpr_pagedata,
		    mmu_ptob(cpr_pagedata_size), bitmap, bitfunc);
	}
	count += i_cpr_count_storage_pages(bitmap, bitfunc);

	DEBUG7(errp("cpr_count_vpages: %ld pages, 0x%lx bytes\n",
	    count, mmu_ptob(count)));
	return (count);
}


static int
cpr_dump_regular_pages(vnode_t *vp)
{
	int error;

	cpr_regular_pgs_dumped = 0;
	error = cpr_contig_pages(vp, WRITE_TO_STATEFILE);
	if (!error)
		DEBUG7(errp("cpr_dump_regular_pages() done.\n"));
	return (error);
}
