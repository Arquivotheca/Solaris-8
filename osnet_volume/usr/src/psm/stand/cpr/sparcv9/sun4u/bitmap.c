/*
 * Copyright (c) 1993-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bitmap.c	1.2	99/11/05 SMI"

#include <sys/types.h>
#include <sys/cpr.h>
#include <sys/fs/ufs_fs.h>
#include <sys/prom_plat.h>
#include "cprboot.h"


/*
 * max space for a copy of physavail data
 * prop size is usually 80 to 128 bytes
 */
#define	PA_BUFSIZE	2048


/*
 * file scope
 */
static void *cb_physavail;
static char pabuf[PA_BUFSIZE];
static caddr_t high_virt;

static caddr_t bitmap;
static int bitmap_bits;
static int bitmap_size;
static int bitmap_alloc_size;
static int tracking_init;


/*
 * if we run out of storage space, recycle any
 * tmp buffer pages that were already restored
 */
static void
free_tmp_pages(void)
{
	static size_t off;

	for (off = 0; off < sfile.buf_offset; off += MMU_PAGESIZE) {
		if (SF_DIFF_PPN(off)) {
			clrbit(bitmap, SF_BUF_PPN(off));
			SF_STAT_INC(recycle);
		}
	}
}


/*
 * scan the physavail list for a page
 * that doesn't clash with the kernel
 */
pfn_t
find_apage(void)
{
	static arange_t *arp;
	static long bitno;
	int rescan;

	if (arp == NULL) {
		arp = cb_physavail;
		bitno = arp->high;
	}

	/*
	 * when a page is found, set a bit so the page wont be found
	 * by another scan; if the scan reaches the end, free any
	 * tmp buf pages and rescan a second time from the top
	 */
	for (rescan = 0; rescan < 2; rescan++) {
		for (; arp->high; bitno = (++arp)->high) {
			for (; bitno >= (long)arp->low; bitno--) {
				if (isclr(bitmap, bitno)) {
					setbit(bitmap, bitno);
					return ((pfn_t)bitno--);
				}
			}
		}
		if (tracking_init == 0)
			break;
		free_tmp_pages();
		arp = cb_physavail;
		bitno = arp->high;
	}

	prom_printf("\n%s: ran out of available/free pages!\n%s\n",
	    prog, rsvp);
	prom_exit_to_mon();

	/* NOTREACHED */
	return (PFN_INVALID);
}


/*
 * reserve virt range, find available phys pages,
 * and map-in each phys starting at vaddr
 */
static caddr_t
map_free_phys(caddr_t vaddr, size_t size, char *name)
{
	int pages, ppn, err;
	physaddr_t phys;
	caddr_t virt;
	char *str;

	str = "map_free_phys";
	virt = prom_claim_virt(size, vaddr);
	CB_VPRINTF(("\n%s: claim vaddr 0x%x, size 0x%x, ret 0x%x\n",
	    str, vaddr, size, virt));
	if (virt != vaddr) {
		prom_printf("\n%s: cant reserve (0x%p - 0x%p) for \"%s\"\n",
		    str, vaddr, vaddr + size, name);
		return (virt);
	}

	for (pages = mmu_btop(size); pages--; virt += MMU_PAGESIZE) {
		/*
		 * map virt page to free phys
		 */
		ppn = find_apage();
		phys = PN_TO_ADDR(ppn);
		err = prom_map_phys(-1, MMU_PAGESIZE, virt, phys);
		if (err || verbose) {
			prom_printf("    map virt 0x%p, phys 0x%lx, "
			    "ppn 0x%lx, ret %d\n", virt, phys, ppn, err);
		}
		if (err)
			return ((caddr_t)ERR);
	}

	return (vaddr);
}


/*
 * check bitmap desc and relocate bitmap data
 * to pages isolated from the kernel
 *
 * sets globals:
 *	bitmap
 *	bitmap_bits
 *	bitmap_size
 *	bitmap_alloc_size
 *	high_virt
 */
int
cb_set_bitmap(void)
{
	cbd_t bitmap_desc, *bdp;
	caddr_t newvirt;
	char *str;

	str = "cb_set_bitmap";
	CB_VPRINTF((ent_fmt, str, entry));

	SF_DCOPY(bitmap_desc);
	bdp = &bitmap_desc;
	if (bdp->cbd_magic != CPR_BITMAP_MAGIC) {
		prom_printf("%s: bad magic 0x%x, expect 0x%x\n",
		    str, bdp->cbd_magic, CPR_BITMAP_MAGIC);
		return (ERR);
	}

	bitmap = SF_DATA();
	bitmap_bits = bdp->cbd_size * NBBY;
	bitmap_size = bdp->cbd_size;
	bitmap_alloc_size = PAGE_ROUNDUP(bitmap_size);
	if (verbose || CPR_DBG(7)) {
		prom_printf("%s: bitmap 0x%p, bits 0x%x, "
		    "size 0x%x, alloc 0x%x\n",
		    str, bitmap, bitmap_bits,
		    bitmap_size, bitmap_alloc_size);
	}

	/*
	 * reserve new space for the bitmap and copy to new pages;
	 * clear any trailing space and point bitmap to the new pages
	 */
	high_virt = (caddr_t)CB_HIGH_VIRT;
	newvirt = map_free_phys(high_virt, bitmap_alloc_size, "bitmap");
	if (newvirt != high_virt)
		return (ERR);
	high_virt += bitmap_alloc_size;
	bcopy(bitmap, newvirt, bitmap_size);
	if (bitmap_alloc_size > bitmap_size)
		bzero(newvirt + bitmap_size, bitmap_alloc_size - bitmap_size);
	bitmap = newvirt;
	if (verbose || CPR_DBG(7))
		prom_printf("%s: bitmap now 0x%p\n", str, bitmap);

	/* advance past the bitmap data */
	SF_ADV(bitmap_size);

	return (0);
}


/*
 * create a new stack for cprboot;
 * this stack is used to avoid clashes with kernel pages and
 * to avoid exceptions while remapping cprboot virt pages
 */
int
cb_get_newstack(void)
{
	caddr_t newstack;

	CB_VENTRY(cb_get_newstack);
	newstack = map_free_phys((caddr_t)CB_STACK_VIRT,
	    CB_STACK_SIZE, "new stack");
	if (newstack != (caddr_t)CB_STACK_VIRT)
		return (ERR);
	return (0);
}


/*
 * since kernel phys pages span most of the installed memory range,
 * some statefile buffer pages will likely clash with the kernel
 * and need to be moved before kernel pages are restored; a list
 * of buf phys page numbers is created here and later updated as
 * buf pages are moved
 *
 * sets globals:
 *	sfile.buf_map
 *	tracking_init
 */
int
cb_tracking_setup(void)
{
	pfn_t ppn, lppn;
	uint_t *imap;
	caddr_t newvirt;
	size_t size;
	int pages;

	CB_VENTRY(cb_tracking_setup);

	pages = mmu_btop(sfile.size);
	size = PAGE_ROUNDUP(pages * sizeof (uint_t));
	newvirt = map_free_phys(high_virt, size, "buf tracking");
	if (newvirt != high_virt)
		return (ERR);
	sfile.buf_map = (uint_t *)newvirt;
	high_virt += size;

	/*
	 * create identity map of sfile.buf phys pages
	 */
	imap = sfile.buf_map;
	lppn = sfile.low_ppn + pages;
	for (ppn = sfile.low_ppn; ppn < lppn; ppn++, imap++)
		*imap = (uint_t)ppn;
	tracking_init = 1;

	return (0);
}


/*
 * get "available" prop from /memory node
 *
 * sets globals:
 *	cb_physavail
 */
int
cb_get_physavail(void)
{
	char *str, *pdev, *mem_prop;
	int len, glen, need, space;
	dnode_t mem_node;
	physaddr_t phys;
	arange_t *arp;
	pphav_t *pap;
	size_t size;
	pfn_t ppn;
	int err;

	str = "cb_get_physavail";
	CB_VPRINTF((ent_fmt, str, entry));

	/*
	 * first move cprboot pages off the physavail list
	 */
	size = PAGE_ROUNDUP((uintptr_t)_end) - (uintptr_t)_start;
	ppn = cpr_vatopfn((caddr_t)_start);
	phys = PN_TO_ADDR(ppn);
	err = prom_claim_phys(size, phys);
	CB_VPRINTF(("    text/data claim (0x%lx - 0x%lx) = %d\n",
	    ppn, ppn + mmu_btop(size) - 1, err));
	if (err)
		return (ERR);

	pdev = "/memory";
	mem_node = prom_finddevice(pdev);
	if (mem_node == OBP_BADNODE) {
		prom_printf("%s: cant find \"%s\" node\n", str, pdev);
		return (ERR);
	}
	mem_prop = "available";

	/*
	 * prop data is treated as a struct array;
	 * check for prop length plus 1 struct size
	 */
	len = prom_getproplen(mem_node, mem_prop);
	need = len + sizeof (*pap);
	space = sizeof (pabuf);
	CB_VPRINTF(("    %s node 0x%x, len %d\n", pdev, mem_node, len));
	if (len == -1 || need > space) {
		prom_printf("\n%s: bad \"%s\" length %d, min %d, max %d\n",
		    str, mem_prop, len, need, space);
		return (ERR);
	}

	/*
	 * read-in prop data and clear 1 struct size
	 * trailing the data for use as an array terminator
	 */
	glen = prom_getprop(mem_node, mem_prop, pabuf);
	if (glen != len) {
		prom_printf("\n%s: %s,%s: expected len %d, got %d\n",
		    str, mem_node, mem_prop, len, glen);
		return (ERR);
	}
	bzero(&pabuf[len], sizeof (*pap));

	/*
	 * convert the physavail list in place
	 * from (phys_base, phys_size) to (low_ppn, high_ppn)
	 */
	if (verbose)
		prom_printf("\nphysavail list:\n", str);
	cb_physavail = pabuf;
	for (pap = cb_physavail, arp = cb_physavail; pap->size; pap++, arp++) {
		arp->low = ADDR_TO_PN(pap->base);
		arp->high = arp->low + mmu_btop(pap->size) - 1;
		if (verbose) {
			prom_printf("  %d: (0x%lx - 0x%lx),\tpages %d\n",
			    (int)(arp - (arange_t *)cb_physavail),
			    arp->low, arp->high, (arp->high - arp->low + 1));
		}
	}

	return (0);
}


/*
 * search for an available phys page,
 * copy the old phys page to the new one
 * and remap the virt page to the new phys
 */
static int
move_page(caddr_t vaddr, pfn_t oldppn)
{
	physaddr_t oldphys, newphys;
	pfn_t newppn;
	int err;

	newppn = find_apage();
	newphys = PN_TO_ADDR(newppn);
	oldphys = PN_TO_ADDR(oldppn);
	CB_VPRINTF(("    remap vaddr 0x%p, old 0x%x/0x%x, new 0x%x/0x%x\n",
	    vaddr, oldppn, oldphys, newppn, newphys));
	phys_xcopy(oldphys, newphys, MMU_PAGESIZE);
	err = prom_remap(MMU_PAGESIZE, vaddr, newphys);
	if (err)
		prom_printf("\nmove_page: remap error\n");
	return (err);
}


/*
 * physically relocate any text/data pages that clash
 * with the kernel; since we're already running on
 * a new stack, the original stack area is skipped
 */
int
cb_relocate(void)
{
	int is_ostk, is_clash, clash_cnt, ok_cnt;
	char *str, *desc, *skip_fmt;
	caddr_t ostk_low, ostk_high;
	caddr_t virt, saddr, eaddr;
	pfn_t ppn;

	str = "cb_relocate";
	CB_VPRINTF((ent_fmt, str, entry));

	ostk_low  = (caddr_t)&estack - CB_STACK_SIZE;
	ostk_high = (caddr_t)&estack - MMU_PAGESIZE;
	saddr = (caddr_t)_start;
	eaddr = (caddr_t)PAGE_ROUNDUP((uintptr_t)_end);

	install_remap();

	skip_fmt = "    skip  vaddr 0x%p, clash=%d, %s\n";
	clash_cnt = ok_cnt = 0;
	ppn = cpr_vatopfn(saddr);

	for (virt = saddr; virt < eaddr; virt += MMU_PAGESIZE, ppn++) {
		is_clash = (isset(bitmap, ppn) != 0);
		if (is_clash)
			clash_cnt++;
		else
			ok_cnt++;

		is_ostk = (virt >= ostk_low && virt <= ostk_high);
		if (is_ostk)
			desc = "orig stack";
		else
			desc = "text/data";

		/*
		 * page logic:
		 *
		 * if (original stack page)
		 *	clash doesn't matter, just skip the page
		 * else (not original stack page)
		 * 	if (no clash)
		 *		setbit to avoid later alloc and overwrite
		 *	else (clash)
		 *		relocate phys page
		 */
		if (is_ostk) {
			CB_VPRINTF((skip_fmt, virt, is_clash, desc));
		} else if (is_clash == 0) {
			CB_VPRINTF((skip_fmt, virt, is_clash, desc));
			setbit(bitmap, ppn);
		} else if (move_page(virt, ppn))
			return (ERR);
	}
	CB_VPRINTF(("%s: total %d, clash %d, ok %d\n",
	    str, clash_cnt + ok_cnt, clash_cnt, ok_cnt));

	/*
	 * free original stack area for reuse
	 */
	ppn = cpr_vatopfn(ostk_low);
	prom_free_phys(CB_STACK_SIZE, PN_TO_ADDR(ppn));
	CB_VPRINTF(("%s: free old stack (0x%lx - 0x%lx)\n",
	    str, ppn, ppn + mmu_btop(CB_STACK_SIZE) - 1));

	return (0);
}
