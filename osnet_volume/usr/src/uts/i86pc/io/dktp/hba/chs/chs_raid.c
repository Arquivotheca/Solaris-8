/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_raid.c	1.4	99/01/11 SMI"

#include "chs.h"

/*
 * The ccb allocated here is used only during chs_attach() to be used
 * only in chs_init_cmd() and will be discarded at the end of instance
 * initialization.
 */

int
chs_ccbinit(register chs_hba_t *const hba)
{
	size_t mem = sizeof (chs_ccb_t);
	chs_ccb_t *ccb;
	register ddi_dma_lim_t *dma_lim;

	ASSERT(hba != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));

	dma_lim = &chs_dac_dma_lim;
	if (CHS_SCSI(hba))
		mem += sizeof (chs_cdbt_t);

	if (ddi_iopb_alloc(hba->dip, dma_lim, mem,
	    (caddr_t *)&ccb) == DDI_FAILURE)
		return (DDI_FAILURE);
	bzero((caddr_t)ccb, mem);			/* needed */

	if (CHS_SCSI(hba))
		ccb->ccb_cdbt = (chs_cdbt_t *)(ccb + 1);

	hba->ccb = ccb;
	ccb->paddr = CHS_KVTOP(ccb);
	return (DDI_SUCCESS);
}

/*
 * Finds MAX_TGT, max number of targets per channel and NCHN, the number
 * of real physical channels on the card and assigns them to chs->max_tgt
 * and chs->nchn respectively.
 *
 * Returns 1 on success and 0 on failure.
 */
int
chs_getnchn_maxtgt(register chs_t *const chs,
			register chs_ccb_t *const ccb)
{
	register int chn;
	register int tgt;
	u_char device_state[36];

	ASSERT(chs != NULL);
	ASSERT(ccb != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));

	/*
	 * Prepare a ccb for a GET-DEVICE-STATE command.
	 * This command is a lot faster than the SCSI INQUIRY command.
	 */

	ccb->type = CHS_DAC_CTYPE2;
	ccb->ccb_opcode = CHS_DAC_GSTAT;

	/*
	 * ccb->ccb_xferpaddr is not used.  However, it is better be
	 * set to some valid paddr, otherwise the controller will
	 * transfer data to some arbitrary physical address which
	 * can be very dangerous.
	 */
	ccb->ccb_xferpaddr = CHS_KVTOP(device_state);

	/*
	 * Let's find MAX_TGT first.  There is at least one channel on
	 * the card and we know it is channel 0.  So we set channel to
	 * 0 and change targets in the reverse order as there is a good
	 * chance that fewer commands need to be sent this way.
	 */
	ccb->ccb_chn = 0;

	for (tgt = MAX_WIDE_SCSI_TGTS; tgt >= 0; tgt--) {
		ccb->ccb_tgt = (u_char)tgt;
		if (chs_init_cmd(chs, ccb) == DDI_FAILURE)
			return (0);
		if (CHS_DAC_CHECK_STATUS(chs, NULL, ccb->ccb_status)
							== CHS_SUCCESS)
			break;			/* found a valid tgt */
	}
	if (!tgt) {
		cmn_err(CE_WARN, "chs_getnchn_maxtgt: MAX_TGT == 0");
		return (0);
	}
	chs->max_tgt = (u_char)(tgt + 1);

	/*
	 * Now let's find NCHN:  We know that target tgt is valid on all
	 * channels so we set the target to be that.  Furthermore, we can
	 * skip channel 0 that we have already looped through.
	 */
	ccb->ccb_tgt = (u_char)tgt;
	for (chn = 1; chn <= CHS_DAC_CHN_NUM; chn++) {
		ccb->ccb_chn = (u_char)chn;
		if (chs_init_cmd(chs, ccb) == DDI_FAILURE)
			return (0);
		if (CHS_DAC_CHECK_STATUS(chs, NULL, ccb->ccb_status)
							& CHS_INVALCHNTGT)
			break;			/* found an invalid chn */
	}
	if (chn == 0xff) {
		cmn_err(CE_WARN, "chs_getnchn_maxtgt: NCHN == "
				"CHS_DAC_CHN_NUM");
		return (0);
	}
	chs->nchn = (u_char)chn;


	MDBG5(("chs_getnchn_maxtgt: nchn %d max_tgt %d",
		chs->nchn, chs->max_tgt));

	return (1);
}

/* Get System Drive configuration table from VIPER EEPROM. */
int
chs_getconf(register chs_t *const chs,
		register chs_ccb_t *const ccb)
{
	register chs_dac_conf_t *conf;
	size_t mem;

	ASSERT(chs != NULL);
	ASSERT(ccb != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));

	/* Prepare a ccb for a READ-ROM-CONFIGURATION command */

	bzero((caddr_t)&(ccb->cmd), sizeof (ccb->cmd));


	ccb->type = CHS_DAC_CTYPE5;
	ccb->ccb_opcode = CHS_DAC_RDCONFIG;

	mem = sizeof (*conf) - sizeof (chs_dac_tgt_info_t) +
		((chs->nchn * chs->max_tgt) *
		sizeof (chs_dac_tgt_info_t));

	conf = (chs_dac_conf_t *)kmem_zalloc(mem, KM_NOSLEEP);
	if (conf == NULL)
		return (DDI_FAILURE);

	chs->conf = conf;
	ccb->ccb_xferpaddr = CHS_KVTOP(chs->conf);

	if (chs_init_cmd(chs, ccb) == DDI_FAILURE)
		return (DDI_FAILURE);

	if (CHS_DAC_CHECK_STATUS(chs, NULL, ccb->ccb_status)
							!= CHS_SUCCESS) {
		cmn_err(CE_WARN, "chs_getconf: failed to read EEPROM");
		return (DDI_FAILURE);
	}

	MDBG5(("chs_getconf: %d System Drive(s)",
		chs_get_nsd(chs)));

	return (DDI_SUCCESS);
}

/*
 * Issues a CHS_DAC_ENQUIRY command and preserves the received info for
 * future use.  However, the max_cmd field is used immediately to set
 * up chs->ccb_stk.
 */
int
chs_getenquiry(register chs_t *const chs,
		register chs_ccb_t *const ccb)
{
	register chs_dac_enquiry_t *enq;

	ASSERT(chs != NULL);
	ASSERT(ccb != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));

	enq = (chs_dac_enquiry_t *)
		kmem_zalloc(sizeof (chs_dac_enquiry_t) - sizeof (deadinfo) +
		    ((chs->nchn * (chs->max_tgt - 1)) *
		    sizeof (deadinfo)),
		    KM_NOSLEEP);
#if 0
	enq = (chs_dac_enquiry_t *)
		kmem_zalloc(sizeof (chs_dac_enquiry_t), KM_NOSLEEP);
#endif

	if (enq == NULL)
		return (DDI_FAILURE);

	chs->enq	= enq;
	ccb->type	= CHS_DAC_CTYPE5;
	ccb->ccb_opcode	= CHS_DAC_ENQUIRY;
	ccb->ccb_xferpaddr	= CHS_KVTOP(chs->enq);
	if (chs_init_cmd(chs, ccb) == DDI_FAILURE)
		return (DDI_FAILURE);

	CHS_GETENQ_INFO(chs);


	return (DDI_SUCCESS);
}


int
chs_ccb_stkinit(register chs_t *const chs)
{
	u_char stk_size;
	register int i;
	register chs_ccb_stk_t *ccb_stk;

	ASSERT(chs != NULL);
	ASSERT(mutex_owned(&chs_global_mutex));
	ASSERT(chs->flags & CHS_GOT_ENQUIRY);

	/* the extra one flags the end of stack */
	stk_size = chs->max_cmd + 1;

	ccb_stk = (chs_ccb_stk_t *)kmem_zalloc(stk_size * sizeof (*ccb_stk),
	    KM_NOSLEEP);
	if (ccb_stk == NULL)
		return (DDI_FAILURE);
	chs->free_ccb = chs->ccb_stk = ccb_stk;

	/*
	 * Set up the free list:  The next field of every element on
	 * the stack contains the index of the next free element.
	 */
	for (i = 1; i < stk_size; i++, ccb_stk++) {
		ccb_stk->next = (short)i;
		ccb_stk->ccb = NULL;
	}
	/* Indicating the end of free ccb's and the end of stack. */
	ccb_stk->next = CHS_INVALID_CMDID;
	ccb_stk->ccb = NULL;

	/*
	 * chs->free_ccb is guaranteed to point to a free element in
	 * the stack, iff (chs->free_ccb->next != CHS_INVALID_CMDID).
	 * Otherwise there is no free element on the stack and we have
	 * to wait until some element becomes free.
	 *
	 * ccb_stk_t is deliberately chosen to be a struct and not
	 * a union so that the ccb field can be set to NULL for the
	 * free ccb's.  Because chs_intr() has enough checking means
	 * to safe-guard itself from the spurious interrupts.
	 */

	return (DDI_SUCCESS);
}

/*
 * Returns 1 if the target should not be accessed for any reason:
 * Otherwise it returns 0.
 */
int
chs_dont_access(register chs_t *const chs,
		    const u_char chn,
		    const u_char tgt)
{
	unchar dummyraid;

	ASSERT(chs != NULL);
	ASSERT(tgt < chs->max_tgt);
	ASSERT(mutex_owned(&chs_global_mutex) || mutex_owned(&chs->mutex));
	ASSERT(chs->conf != NULL);

	if (tgt == ((chs_dac_conf_viper_t *)(chs->conf))->init_id[chn] ||
		CHS_IN_ANY_SD(chs, chn, tgt, &dummyraid) != FALSE)
		return (1);

	if (CHS_CAN_PHYSDRV_ACCESS(chs, chn, tgt) == FALSE) {
		return (1);
	}
	return (0);
}
