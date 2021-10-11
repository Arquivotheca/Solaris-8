/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: NCR 710/810 EISA SCSI HBA       (init.c)
 *
#pragma ident	"@(#)init.c	1.5	97/07/21 SMI"
 *
 */

/*
 * Routines in this file are based on equivalent routines in the Solaris
 * NCR 710/810 driver.  Some have been simplified for use in a single-threaded
 * environment.
 */


#include <types.h>
#include "ncr.h"


void
ncr_saverestore(	ncr_t	*ncrp,
			nrs_t	*nrsp,
			unchar	*regbufp,
			int	 nregs,
			bool_t	 savethem )
{
	IOADDR	ioaddr = ncrp->n_ioaddr;
	unchar	tmp;

	/* save the interesting bits defined by the mask in the nrs_t array */
	if (savethem) {
		while (nregs-- > 0) {
			/* mask off the uninteresting bits */
			*regbufp = inb(ioaddr + nrsp->nr_reg) & nrsp->nr_bits;
			regbufp++;
			nrsp++;
		}
		return;
	}

	/* restore the saved state of the masked bits */
	while (nregs-- > 0) {
		tmp = inb(ioaddr + nrsp->nr_reg) & nrsp->nr_bits;
		outb(ioaddr + nrsp->nr_reg, (tmp | *regbufp));
		regbufp++;
		nrsp++;
	}
}


/*
 * Initialize the Table Indirect pointers for each target, lun
 */
void
ncr_table_init(	ncr_t	*ncrp,
		npt_t	*nptp,
		int	 target,
		int	 lun,
		ulong	 cmdlen,
		struct scsi_pkt *pktp )
{
	NDBG8(("ncr_table_init(%d,%d)\n", target, lun));

	nptp->nt_target = target;
	nptp->nt_lun = lun;
	nptp->nt_selectparm.nt_sdid = NCR_ENCODE_ID(ncrp, (unchar)target);

	nptp->nt_state = NPT_STATE_DONE;

	/* init the clock divisors from hba's reset values, this
	/* defaults to zeros on the 710 */
	/*nptp->nt_selectparm.nt_scntl3 = ncrp->n_scntl3;*/
	nptp->nt_selectparm.nt_scntl3 = 0;
	nptp->nt_selectparm.nt_sxfer = 0;

	nptp->nt_identify[0] = MSG_IDENTIFY;	/* no disconnect */
	nptp->nt_identify[0] |= lun;

	nptp->nt_cmd.count	= cmdlen;
	nptp->nt_cmd.address	= NCR_KVTOP(&nptp->nt_cdb);

	nptp->nt_sendmsg.count	= sizeof(nptp->nt_identify);
	nptp->nt_sendmsg.address= NCR_KVTOP(nptp->nt_identify);

	nptp->nt_rcvmsg.count	= sizeof(nptp->nt_msginbuf);
	nptp->nt_rcvmsg.address	= NCR_KVTOP(nptp->nt_msginbuf);

	nptp->nt_status.count	= sizeof(nptp->nt_statbuf);
	nptp->nt_status.address	= NCR_KVTOP(nptp->nt_statbuf);

	nptp->nt_extmsg.count	= sizeof(nptp->nt_extmsgbuf);
	nptp->nt_extmsg.address	= NCR_KVTOP(nptp->nt_extmsgbuf);

	nptp->nt_syncin.count	= sizeof(nptp->nt_syncibuf);
	nptp->nt_syncin.address	= NCR_KVTOP(nptp->nt_syncibuf);

	nptp->nt_widein.count	= sizeof(nptp->nt_wideibuf);
	nptp->nt_widein.address	= NCR_KVTOP(nptp->nt_wideibuf);

	nptp->nt_syncout.count	= sizeof(nptp->nt_syncobuf);
	nptp->nt_syncout.address= NCR_KVTOP(nptp->nt_syncobuf);

	nptp->nt_errmsg.count	= sizeof(nptp->nt_errmsgbuf);
	nptp->nt_errmsg.address	= NCR_KVTOP(nptp->nt_errmsgbuf);

	nptp->nt_dsa_physaddr	= longaddr((ushort)nptp, myds());
	nptp->nt_pktp = pktp;
}

#ifdef SOLARIS
/*
 * The buffer allocated by kmem_zalloc() might consist of 
 * discontiguous physical pages.  Each target structure must
 * be contained within a single physical page because the 
 * structure contains the SCRIPTS Table Indirect words. All
 * the T.I. words (for a particular target) must be contained
 * in a single physical page since they're accessed via the
 * DSA register (which is programmed with the physical address
 * of the base of the target structure.) 
 *  
 * This routine crams as many per target structs into a virtual
 * page as is possible. If there isn't enough room for a whole
 * structure then the pointer is bumped to the next page boundary
 * by the PageAlignPtr() macro.
 */
static bool_t
ncr_target_init( ncr_t *ncrp )
{
	npt_t	**nptpp;
	caddr_t	  memp;
	int	  size;
	int	  numperpage;
	int	  pages;
	int	  target;
	int	  lun;
	u_int	  real_len;

	NDBG1(("ncr_target_init: start\n"));

	/* round the size of the per target struct up to a multiple of four */
	size = roundup(sizeof(npt_t), sizeof(long));
	numperpage = MMU_PAGESIZE / size;

	/* compute number of pages to hold max number of luns */
	pages = ((NTARGETS_WIDE * NLUNS_PER_TARGET) + numperpage - 1) / numperpage;

	/* add one because alloc doesn't return the buffer page aligned */
	pages++;
	ncrp->n_ptsize = ptob(pages);
#if 0 /*  defined(__ppc) */
	if (ddi_mem_alloc(ncrp->n_dip, (ddi_dma_lim_t *)0, ncrp->n_ptsize,
	    KM_NOSLEEP, &memp, &real_len) == DDI_FAILURE) {

#else	/* ! defined(__ppc) */
	if (!(memp = kmem_zalloc(ncrp->n_ptsize, KM_NOSLEEP))) {

#endif	/* defined(__ppc) */
		NDBG1(("ncr_target_init: nomem\n"));
		return (FALSE);
	}

	ncrp->n_ptsave = memp;
	nptpp = &ncrp->n_pt[0];
	for (target = 0; target < NTARGETS_WIDE; target++) {
		for (lun = 0;lun < NLUNS_PER_TARGET;nptpp++, memp += size) {
			/* bump ptr if entry won't fit within current */
			/* physical page */
			memp = PageAlignPtr(memp, size);

			/* setup the per-target struct and Table Indirect */
			/* array for this (target,lun) */
			*nptpp = (npt_t *)memp;
			ncr_table_init(ncrp, *nptpp, target, lun);
			lun++;
		}
	}
	ncr_syncio_reset(ncrp, NULL);
	NDBG1(("ncr_target_init: okay\n"));
	return (TRUE);
}

/*
 * ncr_hba_init()
 *
 *	Set up this HBA's copy of the SCRIPT and initialize
 *	each of its target/luns.
 */
bool_t
ncr_hba_init( ncr_t *ncrp )
{
	int	clock;

	NDBG1(("ncr_hba_init: start: instance=%d\n",
		ddi_get_instance(ncrp->n_dip)));

	ncrp->n_state = NSTATE_IDLE;

	/* initialize the empty FIFO completion queue */
	ncrp->n_donetail = &ncrp->n_doneq;

	/* reset the chip but don't lose the BIOS config values */
	NCR_RESET(ncrp);

#ifdef NCR_DO_BUS_RESET
	/* turn this into a config property */
	NCR_BUS_RESET(ncrp);
#endif

	/* program the chip's registers */
	NCR_INIT(ncrp);

	/* set up the per target structures */
	if (!ncr_target_init(ncrp))
		return (FALSE);

	/* save ptr to HBA's per-target structure which is used by the
	 * SCRIPT while waiting for reconnections after disconnects
	 */
	ncrp->n_hbap = NTL2UNITP(ncrp, ncrp->n_initiatorid, 0);

	NDBG1(("ncr_hba_init: finish\n"));

	return (TRUE);
}

/*
 * ncr_hba_uninit()
 *
 *	Free all the buffers allocated by ncr_hba_init()
 */
void
ncr_hba_uninit( ncr_t *ncrp )
{
	NDBG1(("ncr_hba_uninit\n"));

	/* free the per-targets-array buffer */
	if (ncrp->n_ptsave) {
		kmem_free(ncrp->n_ptsave, ncrp->n_ptsize);
		ncrp->n_ptsave = NULL;
	}
}
#endif
