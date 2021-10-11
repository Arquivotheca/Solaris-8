/*
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)ast.c	1.10	97/06/30 SMI"

#include "ast.h"
#include <sys/types.h>
#include <sys/bootdef.h>
#include <sys/bootinfo.h>
#include <sys/salib.h>

extern caddr_t	rm_malloc(size_t size, u_int align, caddr_t virt);
extern void	rm_free(caddr_t, size_t);

int
AstInit(struct bootmem *mrp)
{
	ebi_iiSig	*sig;
	EBI_II		calltab;
	ulong 		blocks, i;
	long 		slot;
	memoryBlockInfo	binfo;
	ulong		*ebip;
	ulong		*calltabp;
	IOInfoTable	*ioinfodata;
	ulong		*mmiospace;
	struct bootmem	*origmrp = mrp;
	extern int goany(void);

	sig = (ebi_iiSig *)(((char *)REAL_TO_LIN(BIOS_SEG, 0)) +
					EBI_II_SIGNATURE);

	if (strncmp(sig->sig, "EBI2", 4))
		return (-1);

#ifdef BOOT_DEBUG
	printf("This is an AST Manhattan with ");
#endif

	ebip = (ulong *)(REAL_TO_LIN(sig->seg, sig->off));
	calltabp = (ulong *)&calltab;

	for (i = 0; i < (sizeof (EBI_II) / sizeof (*ebip)); i++)
		*calltabp++ = *ebip++ + (ulong)(REAL_TO_LIN(BIOS_SEG, 0));

	slot = -1;
	if (((*calltab.GetNumSlots)((ulong *)&slot)) != OK) {
		printf("AstInit: Failed getting number of slots.\n");
		(void) goany();
		return (-1);
	}
	ioinfodata = (IOInfoTable *)
	    rm_malloc((slot+1)*sizeof (*ioinfodata), 0, 0);

	mmiospace = (ulong *)rm_malloc((slot+1)*sizeof (ulong), 0, 0);
	if ((!ioinfodata) || (!mmiospace)) {
		if (ioinfodata)
			rm_free((caddr_t)ioinfodata,
			    (slot+1)*sizeof (*ioinfodata));
		if (mmiospace)
			rm_free((caddr_t)mmiospace, (slot+1)*sizeof (ulong));
		printf("AstInit: Failed allocating ioinfo table\n");
		(void) goany();
		return (-1);
	}
	if ((*calltab.GetMMIOTable)(ioinfodata) != OK) {
		printf("AstInit: Failed getting ioinfo table\n");
		(void) goany();
		return (-1);
	}

	for (i = 0; i < slot; i++) {
		if (!ioinfodata[i].length)
			continue;
		if (ioinfodata[i].flags & ALLOCATE_RAM) {
#ifdef BOOT_DEBUG
			printf(" Allocating mem slot: %d length: %d\n",
						i, ioinfodata[i].length);
#endif
			mmiospace[i] =
			    (long)rm_malloc(ioinfodata[i].length, 0, 0);
		} else
			mmiospace[i] = ioinfodata[i].address.low;
	}
	if (((*calltab.InitEBI)(mmiospace)) != OK) {
		printf("AstInit: Failed initting EBI\n");
		(void) goany();
		return (-1);
	}
	if (((*calltab.GetNumMemBlocks)(mmiospace, &blocks)) != OK) {
		printf("Failed getting memblock info\n");
		(void) goany();
		return (-1);
	}
#ifdef BOOT_DEBUG
	procs = 0;
	(*calltab.GetNumProcs)(mmiospace, &procs);
	printf(" with %d memory blocks and %d processors.\n", blocks, procs);
#endif
	for (i = 0; i < blocks; i++) {
		if ((*calltab.GetMemBlockInfo)(mmiospace, &binfo, i) != OK)
			return (-1);

#ifdef BOOT_DEBUG
		printf("              Start: 0x%x  Size: 0x%x %d KB\n",
				binfo.blockStartAddr.low, binfo.blockSize,
				binfo.blockSize/1024);
#endif
		/*
		 * find the place to insert the memory, this assumes
		 * the list from Solaris is already sorted
		 */
		for (mrp = origmrp; mrp->extent > 0; mrp++)
			if (binfo.blockStartAddr.low <= mrp->base)
				break;

		/*
		 * If Ast reports a memory block that crosses the 16Mb boundary,
		 * it has to be broken into 2 chunks, the 1st chunk ends at
		 * 16Mb and the 2nd chunk starts at 16Mb.  This process is
		 * needed because when it returns back to memtest, the entry
		 * that starts below 16Mb will be clipped at ext_sum - cmos
		 * extended memory sum which is 64Mb in this case. Thus, all
		 * the memory above 64Mb will be lost.
		 */
		if ((binfo.blockStartAddr.low < MEM16M) &&
		    ((binfo.blockStartAddr.low+binfo.blockSize) > MEM16M)) {
			mrp->base = binfo.blockStartAddr.low;
			mrp->extent = MEM16M - mrp->base;
			mrp->flags = B_MEM_EXPANS;
			mrp++;
			mrp->base = MEM16M;
			mrp->extent = binfo.blockSize - (mrp-1)->extent;
			mrp->flags = B_MEM_EXPANS;
			continue;
		}
		/*
		 * now add it to the list for Solaris - if it's already there
		 * then we just skip this entry.  if we can add or subtract
		 * from the current entry, we do that.  if it needs an
		 * entirely new entry, then we'll need to shuffle everything
		 * after it to keep the list sorted.
		 */
		else if (mrp->base == binfo.blockStartAddr.low) {
			if (mrp->extent != binfo.blockSize)
				mrp->extent = binfo.blockSize;
			continue;
		}

		/*
		 * shuffle any needed entries to add ours in a sorted manner.
		 * actually, since Solaris gives us 3 entries (0-640, 1MB-16MB,
		 * and 16MB on up), we'll never need to shuffle - we can just
		 * add to the end.
		 */
		mrp->base = binfo.blockStartAddr.low;
		mrp->extent = binfo.blockSize;
		mrp->flags = BF_IGNCMOSRAM;
	}
#ifdef BOOT_DEBUG
	(void) goany();
#endif
	mrp++;
	mrp->extent = 0;
	if (((*calltab.DeInitEBI)(mmiospace)) != OK) {
		printf("Failed de-initting EBI\n");
		return (-1);
	}
	for (i = 0; i < slot; i++) {
		if (!ioinfodata[i].length)
			continue;
		if (ioinfodata[i].flags & ALLOCATE_RAM)
			rm_free((caddr_t)mmiospace[i], ioinfodata[i].length);
	}
	rm_free((caddr_t)mmiospace, (slot+1)*sizeof (ulong));
	rm_free((caddr_t)ioinfodata, (slot+1)*sizeof (*ioinfodata));

	return (0);
}
