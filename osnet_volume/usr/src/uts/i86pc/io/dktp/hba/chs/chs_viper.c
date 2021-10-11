/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)chs_viper.c	1.9	99/07/14 SMI"

#include "chs.h"
#include <sys/esunddi.h>

/*ARGSUSED*/
static bool_t
viper_probe(dev_info_t	*dip,
			int		*regp,
			int		 len,
			int		*pidp,
			int		 pidlen,
			bus_t		 bus_type,
			bool_t		 probing)
{
	unsigned short	vendorid;
	unsigned short	deviceid;
	unsigned char	iline;
	ddi_acc_handle_t handle;
	int 		rval;

	if (bus_type != BUS_TYPE_PCI) {
		return (FALSE);
	}

	if (pci_config_setup(dip, &handle) != DDI_SUCCESS) {
		return (FALSE);
	}

	vendorid = pci_config_getw(handle, PCI_CONF_VENID);
	deviceid = pci_config_getw(handle, PCI_CONF_DEVID);
	iline = pci_config_getb(handle, PCI_CONF_ILINE);

	pci_config_teardown(&handle);

	rval = FALSE;
	switch (vendorid) {
	case 0x1014:
		switch (deviceid) {
		case 0x002E:
			rval = TRUE;
		}
	}


	if (iline < 1) {
		cmn_err(CE_CONT,
		    "?viper_probe: disabled\n");
		return (FALSE);
	}


	if (probing) {
		if (rval == TRUE)
			cmn_err(CE_CONT,
			    "?viper_probe() vendor=0x%x device=0x%x\n",
			    vendorid, deviceid);
		else
			MDBG4(("viper_probe: Not found\n"));
	}

	MDBG4(("viper_probe: okay\n"));
	return (rval);
}



/*ARGSUSED*/
static void
viper_reset(chs_t *chsp)
{
	MDBG1(("viper_reset: Software reset completed\n"));
}



/*
 * Check the whichbit in regoffset reg to see if it is set or not
 * within timeframe seconds.
 * If check is not 0, then it returns TRUE if the requested bit
 * was set within the time frame, otherwise returns FALSE.
 * If check is 0, it returns TRUE if the requested bit is
 * reset during that timeframe, otherwise returns FALSE.
 */

/*ARGSUSED*/
static bool_t
Check_regbit(chs_t *chsp, int regoffset, int whichbit,  int timeframe,
		int check)
{
	int i = 0;
	int militimeframe = timeframe * 1000;

	/* checks every mili second  */
	while (i < militimeframe) {
		if (check) {
			if (ddi_io_getb(chsp->handle,
			    REG8(chsp->reg, regoffset)) & whichbit) {
				return (TRUE);
			}
		} else {
			if ((ddi_io_getb(chsp->handle,
			    REG8(chsp->reg, regoffset)) & whichbit) == 0) {
				return (TRUE);
			}
		}

		drv_usecwait(1000);
		i++;
	}
	return (FALSE);
}



/*ARGSUSED*/
void
viper_regs_free(chs_t *chs)
{
#ifndef PCI_DDI_EMULATION
	ddi_regs_map_free(&chs->handle);
#endif
}


/*ARGSUSED*/
static bool_t
viper_init(chs_t 		*chsp,
		dev_info_t	*dip)
{
	ddi_acc_handle_t handle;
	int reset_counter = 0;
	uchar_t postmajor, postminor, bcs, ecs;
	caddr_t statusq;
	caddr_t secondcmdblkp;
	paddr_t secondcmdblkphys;
	paddr_t statusq_phys;
	int i;
	int reset_pass = FALSE;



#ifndef PCI_DDI_EMULATION
	static ddi_device_acc_attr_t attr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,	/* not portable */
		DDI_STRICTORDER_ACC
	};


	if (ddi_regs_map_setup(dip, chsp->rnum, (caddr_t *)&chsp->reg,
	    (offset_t)0, (offset_t)0, &attr, &chsp->handle) !=
	    DDI_SUCCESS) {
		MDBG1(("viper_init: no ioaddr\n"));
		return (FALSE);
	}
#endif

	/* Initializing the viper chip */

	/* step1: */
	if (pci_config_setup(dip, &handle) != DDI_SUCCESS) {
		viper_regs_free(chsp);
		return (FALSE);
	}

	/* Enable bus master, parity */
	/* IO access should already be on */
	pci_config_putw(handle, PCI_CONF_COMM,
	    pci_config_getw(handle, PCI_CONF_COMM)  |
		PCI_COMM_IO | PCI_COMM_PARITY_DETECT | PCI_COMM_ME);

	pci_config_teardown(&handle);


	/* Initialization for I/O Processing */

	reset_counter = 0;
	/* step3: */
step3:

	while ((reset_counter < 2) && (reset_pass != TRUE)) {
		reset_pass = TRUE;
		reset_counter++;

		ddi_io_putb(chsp->handle, REG8(chsp->reg, SCPR_REG),
		    VIPER_RESET_ADAPTER);
		drv_usecwait(100);

		/*
		 * the next two putb's are not in spec but IBM told
		 * us to add it
		 */

		ddi_io_putb(chsp->handle, REG8(chsp->reg, SCPR_REG),	0);

		/* step4 */
		if (Check_regbit(chsp, HIST_REG, VIPER_HIST_GHI, 45, 1) ==
		    FALSE) {
			reset_pass = FALSE;
			continue;
		}

		/* step5: */
		postmajor = ddi_io_getb(chsp->handle, REG8(chsp->reg,
				ISPR_REG));

		/* step6: */
		ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG),
		    VIPER_HIST_GHI);
		/* step7: */

		if (Check_regbit(chsp, HIST_REG, VIPER_HIST_GHI, 45, 1) ==
		    FALSE) {
			reset_pass = FALSE;
			continue;
		}

		/* step8: */
		postminor = ddi_io_getb(chsp->handle,
				REG8(chsp->reg, ISPR_REG));

		/* step9: */
		ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG),
		    VIPER_HIST_GHI);

		/* step10: */
		if (!(postmajor & 0x80)) {
			cmn_err(CE_WARN, " viper_init failed, "
				"POST major code 0x%x, POST minor code 0x%x",
					postmajor, postminor);
			reset_pass = FALSE;
			break;
		}
		/* step11: */
		if (!(postmajor & 0x40)) {
			cmn_err(CE_WARN, "viper_init: SCSI Bus Problem");
			cmn_err(CE_WARN, " viper_init:, "
				"POST major code 0x%x, POST minor code 0x%x",
					postmajor, postminor);
		}
		/* step12: */
		if (Check_regbit(chsp, HIST_REG, VIPER_HIST_GHI, 240, 1) ==
		    FALSE) {
			reset_pass = FALSE;
			continue;
		}
		/* step13: */
		bcs = ddi_io_getb(chsp->handle, REG8(chsp->reg, ISPR_REG));

		/* step14: */
		ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG),
		    VIPER_HIST_GHI);

		/* step15: */

		if (Check_regbit(chsp, HIST_REG, VIPER_HIST_GHI, 240, 1) ==
		    FALSE) {
			reset_pass = FALSE;
			continue;
		}
		/* step16: */
		ecs = ddi_io_getb(chsp->handle, REG8(chsp->reg, ISPR_REG));

		/* step17: */
		ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG),
		    VIPER_HIST_GHI | VIPER_HIST_EI);

		/* step18: */
		if (Check_regbit(chsp, CBSP_REG, VIPER_OP_PENDING, 240, 0)
								== FALSE) {
			reset_pass = FALSE;
			continue;
		}
	}

	if (reset_pass == FALSE) {
		MDBG1(("viper_init: Defective Board\n"));
		MDBG1(("viper_init: Initialization failed\n"));
		ddi_io_putb(chsp->handle,
			REG8(chsp->reg, HIST_REG), VIPER_HIST_GHI);
		viper_regs_free(chsp);
		return (FALSE);
	}

	/*
	 * step 19 and 20 will be together because if we set everything
	 * in CCCR and then read CCCR to enable ILE, semaphone bit will
	 * be set to 0 on the read.
	 */

	/* Set up CCCR, enable bus master */
	ddi_io_putl(chsp->handle, REG32(chsp->reg, CCCR_REG), 0x10);

	/* step21: */
	ddi_io_putb(chsp->handle, REG8(chsp->reg, SCPR_REG),
	    VIPER_ENABLE_BUS_MASTER);

	/* step22: */
	/* Setup status queue */
	if (ddi_iopb_alloc(dip, (ddi_dma_lim_t *)0,
	    (((sizeof (viper_statusq_element)) *
	    VIPER_STATUS_QUEUE_NUM) +
	    (sizeof (chs_cmd_t)) + sizeof (chs_cmd_t)),
	    (caddr_t *)&statusq) == DDI_FAILURE) {

		MDBG1(("viper_init: status queue allocation failed"));
		ddi_io_putb(chsp->handle,
			REG8(chsp->reg, HIST_REG), VIPER_HIST_GHI);
		viper_regs_free(chsp);
		return (FALSE);
	}

	/* Zero out the status queue elements */
	for (i = 0;
		i < ((sizeof (viper_statusq_element) * VIPER_STATUS_QUEUE_NUM) +
			(sizeof (chs_cmd_t)) + sizeof (chs_cmd_t));
									i++) {
		*(statusq + i) = 0;
}

	statusq_phys = CHS_KVTOP((uchar_t *)statusq);

	/* setup status queue start, head, tail and end registers on the chip */

	ddi_io_putl(chsp->handle, REG32(chsp->reg, SQSR_REG),
					(ulong_t)(statusq_phys));

	ddi_io_putl(chsp->handle, REG32(chsp->reg, SQHR_REG),
		(ulong_t)(statusq_phys + sizeof (viper_statusq_element)));

	ddi_io_putl(chsp->handle, REG32(chsp->reg, SQTR_REG),
						(ulong_t)statusq_phys);

	ddi_io_putl(chsp->handle, REG32(chsp->reg, SQER_REG),
			(ulong_t)(statusq_phys +
				(sizeof (viper_statusq_element) *
						VIPER_STATUS_QUEUE_NUM)));


	MDBG8(("viper_init: statusq head is %p", (void*)statusq));

	chsp->vsq = statusq;
	chsp->psq = statusq_phys;
	chsp->sqtr = (paddr_t)statusq_phys;
	chsp->sqhr =
		(paddr_t)(statusq_phys + sizeof (viper_statusq_element));


	/* step23: */
	/* would also disable interrupts */
	ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG), VIPER_HIST_GHI);

	/* step24: */
	if (!((bcs == 0x0F) && (ecs == 0))) {
		cmn_err(CE_WARN, "viper_init: Raid Configuration Problem\n");
	}


	chsp->vipercmdblkp = statusq +
		((sizeof (viper_statusq_element)) * VIPER_STATUS_QUEUE_NUM);

	secondcmdblkp =
		(caddr_t)((uchar_t *)chsp->vipercmdblkp + sizeof (chs_cmd_t));

	secondcmdblkphys = CHS_KVTOP((uchar_t *)secondcmdblkp);

	*((uchar_t *)chsp->vipercmdblkp + 20) = VIPER_CMDBLK_BYTE20;
	*((uchar_t *)chsp->vipercmdblkp + 21) = VIPER_CMDBLK_BYTE21;
	*((uchar_t *)chsp->vipercmdblkp + 22) = VIPER_CMDBLK_BYTE22;
	*((uchar_t *)chsp->vipercmdblkp + 23) = VIPER_CMDBLK_BYTE23;

	/*
	 * Keep the physical address of this command block in chs unit
	 * so that we do not need to convert each time we want to send a
	 * command
	 */
	chsp->vipercmdphys = (paddr_t)(statusq_phys +
				(sizeof (viper_statusq_element) *
						VIPER_STATUS_QUEUE_NUM));


	*((uchar_t *)secondcmdblkp) = CHS_DAC_NOOP;
	*((uchar_t *)secondcmdblkp + 1) = CHS_NEPTUNE_CMDID;

	*((uchar_t *)secondcmdblkp + 20) = VIPER_CMDBLK_BYTE20;
	*((uchar_t *)secondcmdblkp + 21) = VIPER_CMDBLK_BYTE21;
	*((uchar_t *)secondcmdblkp + 22) = VIPER_CMDBLK_BYTE22;
	*((uchar_t *)secondcmdblkp + 23) = VIPER_CMDBLK_BYTE23;

	*((ulong_t *)((uchar_t *)chsp->vipercmdblkp + 16)) =
					(ulong_t)secondcmdblkphys;
	*((ulong_t *)((uchar_t *)secondcmdblkp + 16)) =
					(ulong_t)secondcmdblkphys;

	MDBG1(("viper_init: Init completed\n"));
	return (TRUE);
}


/*ARGSUSED*/
static void
viper_uninit(chs_t 		*chsp,
		dev_info_t	*dip)
{
	/* reset the card */
	int reset_counter = 0;
	int reset_pass = FALSE;

	while ((reset_counter < 2) && (reset_pass != TRUE)) {
		reset_pass = TRUE;
		reset_counter++;

		ddi_io_putb(chsp->handle, REG8(chsp->reg, SCPR_REG),
		    VIPER_RESET_ADAPTER);
		drv_usecwait(100);

		ddi_io_putb(chsp->handle, REG8(chsp->reg, SCPR_REG),
			0);

		/* step4 */
		if (Check_regbit(chsp, HIST_REG, VIPER_HIST_GHI, 45, 1)
								== FALSE) {
			reset_pass = FALSE;
			continue;
		} else {
			break;
		}
	}


	if (reset_pass == FALSE) {
		cmn_err(CE_WARN,
			"viper_uninit: Resetting adapter unsuccessful\n");
		return;
	}

	/* free the status queue */
	ddi_iopb_free(chsp->vsq);
	viper_regs_free(chsp);
	MDBG1(("viper_uninit: Uninit completed\n"));
}


/* enabled the interrupt */
static void
viper_enable(chs_t *chsp)
{

	ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG), VIPER_HIST_EI);

	MDBG1(("viper_enable: Enable completed\n"));
}

static void
viper_disable(chs_t *chsp)
{
	ddi_io_putb(chsp->handle, REG8(chsp->reg, HIST_REG), 0);
	MDBG1(("viper_disable: Enable completed\n"));
}


/* returns 0 if not ready */
static uchar_t
viper_cready(chs_t *chsp)
{
	/*
	 * Per IBM, there is a bug in semaphore handling, that can let two
	 * requests succeed at the same time. So we use the SS bit
	 * instead in case the delay for semaphore is very long (longer
	 * that sending the command and exiting the mutex it is in).
	 *
	 */
	return ((ddi_io_getl(chsp->handle, REG32(chsp->reg, CCCR_REG))
							& VIPER_CCCR_SS) == 0);
}

static bool_t
viper_csend(chs_t *chsp,
		void *ccbp)
{

	register ulong_t *ip = (ulong_t *)ccbp;
	register uchar_t *op = (uchar_t *)chsp->vipercmdblkp;
	register int i;

	for (i = 0; i < 4; i++) {
		*((ulong_t *)op + i) = *(ip + i);
	}


	MDBG8(("%x %x %x %x", op[0], op[1], op[2], op[3]));
	MDBG8(("%x %x %x %x", op[4], op[5], op[6], op[7]));
	MDBG8(("%x %x %x %x", op[8], op[9], op[10], op[11]));
	MDBG8(("%x %x %x %x", op[12], op[13], op[14], op[15]));
	MDBG8(("%x %x %x %x", op[16], op[17], op[18], op[19]));
	MDBG8(("%x %x %x %x\n", op[20], op[21], op[22], op[23]));


	ddi_io_putl(chsp->handle, REG32(chsp->reg, CCSAR_REG),
	    chsp->vipercmdphys);

	ddi_io_putl(chsp->handle, REG32(chsp->reg, CCCR_REG),
	    VIPER_CCCR_SS | VIPER_CCCR_ILE | (VIPER_CMD_BYTECNT << 8));

	MDBG1(("viper_send: Send completed\n"));
	return (TRUE);
}

static uchar_t
viper_iready(chs_t *chsp)
{
	MDBG1(("viper_iready: Ipoll completed\n"));
	return ((ddi_io_getb(chsp->handle, REG8(chsp->reg, HIST_REG)) &
		VIPER_HIST_SCE));
}


/*
 * head is the physical address of head of queue, returns the
 * next element after current in status queue
 */
paddr_t
nextelement(paddr_t head, paddr_t current)
{
	return ((paddr_t)((ulong_t)head + sizeof (viper_statusq_element) *
		(((((ulong_t)current - (ulong_t)head) /
		sizeof (viper_statusq_element)) + 1) %
		VIPER_STATUS_QUEUE_NUM)));
}

/*
 * When clear == 1: Returns 1 if it was successful at receiving a status,
 * returns 0 if the queue was empty
 * When clear == 2, it means that we are just checking whether or
 * not it was a GHI interrupt/SQO interrupt, if so clear the
 * interrupt and return 1, else return 0
 */
static uchar_t
viper_get_istat(chs_t *chsp,
			void *hw_stat,
			int clear)
{
	register chs_stat_t  *ip = (chs_stat_t *)hw_stat;
	register ushort_t	op = chsp->reg;
	uchar_t *vsqtr;
	uchar_t Isr;

	if (clear == 2) {
		Isr = (uchar_t)ddi_io_getb(chsp->handle,
			REG8(chsp->reg, HIST_REG));
		if (Isr & VIPER_HIST_SQO) {
			cmn_err(CE_CONT, "?viper_get_istat:"
					" status queue full\n");
		}

		if (Isr & (VIPER_HIST_SQO | VIPER_HIST_GHI)) {
			/* clear the SQ0 or GHI interrupt */
			ddi_io_putb(chsp->handle,
				REG8(chsp->reg, HIST_REG),	Isr);
			return (1);
		} else {
			return (0);
		}
	}


	if (nextelement(chsp->psq, chsp->sqtr) ==  chsp->sqhr) {
		if (viper_iready(chsp)) {
			chsp->sqhr = (paddr_t)ddi_io_getl(chsp->handle,
			    REG32(op, SQHR_REG));
		} else {
			return (0);
		}
	}


	if (nextelement(chsp->psq, chsp->sqtr) ==  chsp->sqhr) {
		/* empty */
		return (0);
	}


	chsp->sqtr = nextelement(chsp->psq, chsp->sqtr);
	if (hw_stat) {
		/* copy status bytes */
		/*
		 * convert physical address to virutal, by using the offset
		 * from startof queue
		 */

		vsqtr = (uchar_t *)((ulong_t)chsp->vsq  +
			(ulong_t)chsp->sqtr -
			(ulong_t)chsp->psq);
		ip->reserved = 0;
		ip->stat_id =  *(vsqtr + 1);

		ip->status = *(vsqtr + 3);
		ip->status = (ip->status << 8) | *(vsqtr + 2);
		MDBG8(("got status %x, stat_id %x\n", ip->status, ip->stat_id));
	}


	if (nextelement(chsp->psq, chsp->sqtr) ==  chsp->sqhr) {
		/* queue empty */
		MDBG8(("empty"));
		ddi_io_putl(chsp->handle, REG32(op, SQTR_REG),
			(ulong_t)chsp->sqtr);
	} else {
		MDBG8(("viper_iget_stat: MORE"));
	}

	MDBG1(("viper_istat: Istat completed\n"));
	return (CHS_STATREADY);
}

static int
viper_geometry(chs_t			*chsp,
			struct	scsi_address	*ap,
			ulong_t			blk)
{
	ulong_t			heads, sectors;


	/*
	 * Per IBM, default mode uses 254 and 63, compatibility mode
	 * uses 128 and 32
	 */


	if (((chs_dac_enquiry_viper_t *)chsp->enq)->MiscFlag & 0x08) {
		/* compatibility mode */
		heads = 128;
		sectors = 32;
	} else {
		if (blk <= 0x400000) {
			heads = 128;
			sectors = 32;
		} else {
			heads = 254;
			sectors = 63;
		}
	}

	if (ap)
	    cmn_err(CE_CONT,
		"?chs(%d,%d): reg=0x%x, sectors=%ld heads=%ld sectors=%ld\n",
		ap->a_target, ap->a_lun, chsp->reg, blk, heads,	sectors);
	return (HBA_SETGEOM(heads, sectors));
}



nops_t	viper_nops = {
	"viper",
	viper_probe,
	chs_get_irq_pci,
	chs_xlate_irq_sid,
	chs_get_reg_pci,
	viper_reset,
	viper_init,
	viper_uninit,
	viper_enable,
	viper_disable,
	viper_cready,
	viper_csend,
	viper_iready,
	viper_get_istat,
	viper_geometry,
	chs_iosetup_viper,
	chs_scsi_chkerr_viper,
	chs_getenq_info_viper,
	chs_in_any_sd_viper,
	chs_get_log_drv_info_viper,
	chs_can_physdrv_access_viper,
	chs_dac_check_status_viper,
	chs_get_scsi_item_viper
};
