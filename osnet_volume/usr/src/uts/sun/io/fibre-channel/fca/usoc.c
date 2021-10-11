/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Sun SOC+ FCA driver
 */

#pragma ident	"@(#)usoc.c	1.10	99/12/10 SMI"

/*
 * usoc - Universal Serial Optical Channel host adapter driver.
 */
#include <sys/types.h>
#include <sys/varargs.h>
#include <sys/devops.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/fcntl.h>

#include <sys/cmn_err.h>
#include <sys/stropts.h>
#include <sys/kmem.h>

#include <sys/errno.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/thread.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/autoconf.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/modctl.h>

#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/disp.h>
#include <sys/taskq.h>

#include <sys/fibre-channel/fc.h>

#include <sys/fibre-channel/fca/usoc_cq_defs.h>
#include <sys/fibre-channel/fca/usocmap.h>
#include <sys/fibre-channel/fca/usocreg.h>
#include <sys/fibre-channel/fca/usocio.h>
#include <sys/fibre-channel/fca/usocvar.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/ethernet.h>
#include <vm/seg_kmem.h>

char _depends_on[] = "misc/fctl";

/*
 * Default usoc dma limits
 */
static ddi_dma_lim_t default_usoclim = {
	(ulong_t)0, (ulong_t)0xffffffff, (uint_t)0xffffffff,
	DEFAULT_BURSTSIZE | BURST32 | BURST64, 1, (25*1024)
};

/*
 * ddi dma attributes
 */
static struct ddi_dma_attr usoc_dma_attr = {
	DMA_ATTR_V0,			/* version */
	(unsigned long long)0,		/* addr_lo */
	(unsigned long long)0xffffffff,	/* addr_hi */
	(unsigned long long)0xffffffff,	/* count max */
	(unsigned long long)4,		/* align */
	DEFAULT_BURSTSIZE | BURST32 | BURST64, 	/* burst size */
	1,				/* minxfer */
	(unsigned long long)0xffffffff,	/* maxxfer */
	(unsigned long long)0xffffffff,	/* seg */
	1,				/* sgllen */
	4,				/* granularity */
	0				/* flags */
};

/*
 * DDI access attributes.  These aren't really universal, since they
 * define sparc/sBus attributes.
 */
static struct ddi_device_acc_attr usoc_acc_attr = {
	(ushort_t)DDI_DEVICE_ATTR_V0,	/* version */
	(uchar_t)DDI_STRUCTURE_BE_ACC,	/* endian flags */
	(uchar_t)DDI_STRICTORDER_ACC	/* data order */
};

/*
 * Fill in the FC Transport structure, as defined in the Fibre Channel
 * Transport Programmming Guide.
 */
static fc_fca_tran_t usoc_tran = {
	FCTL_FCA_MODREV_1,		/* Version 1 */
	2,				/* numerb of ports */
	sizeof (usoc_pkt_priv_t),	/* pkt size */
	1024,				/* max cmds */
	&default_usoclim,		/* DMA limits */
	0,				/* iblock, to be filled in later */
	&usoc_dma_attr,			/* dma attributes */
	&usoc_acc_attr,			/* access atributes */
	usoc_bind_port,
	usoc_unbind_port,
	usoc_init_pkt,
	usoc_uninit_pkt,
	usoc_transport,
	usoc_get_cap,
	usoc_set_cap,
	usoc_getmap,
	usoc_transport,
	usoc_ub_alloc,
	usoc_ub_free,
	usoc_ub_release,
	usoc_external_abort,
	usoc_reset,
	usoc_port_manage
};

/*
 * Table used for setting the burst size in the soc+ config register
 */
static int usoc_burst32_table[] = {
	USOC_CR_BURST_4,
	USOC_CR_BURST_4,
	USOC_CR_BURST_4,
	USOC_CR_BURST_4,
	USOC_CR_BURST_16,
	USOC_CR_BURST_32,
	USOC_CR_BURST_64
};

/*
 * Table for setting the burst size for 64-bit sbus mode in soc+'s CR
 */
static int usoc_burst64_table[] = {
	(USOC_CR_BURST_8 << 8),
	(USOC_CR_BURST_8 << 8),
	(USOC_CR_BURST_8 << 8),
	(USOC_CR_BURST_8 << 8),
	(USOC_CR_BURST_8 << 8),
	(USOC_CR_BURST_32 << 8),
	(USOC_CR_BURST_64 << 8),
	(USOC_CR_BURST_128 << 8)
};

/*
 * Tables used to define the sizes of the Circular Queues
 *
 * To conserve DVMA/IOPB space, we make some of these queues small...
 */
static int usoc_req_entries[] = {
	USOC_SMALL_CQ_ENTRIES,		/* Maintenance commands */
	USOC_MAX_CQ_ENTRIES,		/* aborts, High priority commands */
	USOC_MAX_CQ_ENTRIES,		/* Most commands */
	0				/* Not currently used */
};

static int usoc_rsp_entries[] = {
	USOC_MAX_CQ_ENTRIES,		/* Solicited  "USOC_OK" responses */
	USOC_SMALL_CQ_ENTRIES,		/* Unsolicited sequences */
	0,				/* Not currently used */
	0				/* Not currently used */
};

/*
 * Bus ops vector
 */
static struct bus_ops usoc_bus_ops = {
	BUSO_REV,		/* rev */
	nullbusmap,		/* int (*bus_map)() */
	0,			/* ddi_intrspec_t (*bus_get_intrspec)(); */
	0,			/* int (*bus_add_intrspec)(); */
	0,			/* void	(*bus_remove_intrspec)(); */
	i_ddi_map_fault,	/* int (*bus_map_fault)() */
	ddi_dma_map,		/* int (*bus_dma_map)() */
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,		/* int (*bus_dma_ctl)() */
	0,			/* int (*bus_ctl)() */
	ddi_bus_prop_op		/* int (*bus_prop_op*)() */
};

/*
 * CB ops vector.  Used for administration only.
 */
static struct cb_ops usoc_cb_ops = {
	usoc_open,			/* int (*cb_open)() */
	usoc_close,			/* int (*cb_close)() */
	nodev,				/* int (*cb_strategy)() */
	nodev,				/* int (*cb_print)() */
	nodev,				/* int (*cb_dump)() */
	nodev,				/* int (*cb_read)() */
	nodev,				/* int (*cb_write)() */
	usoc_ioctl,			/* int (*cb_ioctl)() */
	nodev,				/* int (*cb_devmap)() */
	nodev,				/* int (*cb_mmap)() */
	nodev,				/* int (*cb_segmap)() */
	nochpoll,			/* int (*cb_chpoll)() */
	ddi_prop_op,			/* int (*cb_prop_op)() */
	0,				/* struct streamtab *cb_str */
	D_MP | D_NEW | D_HOTPLUG,	/* cb_flag */
	CB_REV,				/* rev */
	nodev,				/* int (*cb_aread)() */
	nodev				/* int (*cb_awrite)() */
};

/*
 * Soc driver ops structure.
 */
static struct dev_ops usoc_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt */
	usoc_getinfo,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	usoc_attach,		/* attach */
	usoc_detach,		/* detach */
	nodev,			/* reset */
	&usoc_cb_ops,		/* driver operations */
	&usoc_bus_ops		/* bus operations */
};

/*
 * usoc status translation table:
 *
 * The P_RJT/F_RJT/P_BSY/F_BSY typically arrive with USOC_OK as the
 * usoc_status many a times in which case the Link Service response
 * is fully decoded and comprehensively understood. Since the usoc
 * firmware doesn't have a mechanism to pass reason/expln/action
 * in addition to the status, the driver wittingly sets the reason
 * to vendor unique.
 */
usoc_xlat_error_t usoc_error_table[] = {

{ USOC_OK, 		FC_PKT_SUCCESS, 	0 },
{ USOC_P_RJT,		FC_PKT_NPORT_RJT,	FC_REASON_FCA_UNIQUE },
{ USOC_F_RJT,		FC_PKT_FABRIC_RJT,	FC_REASON_FCA_UNIQUE },
{ USOC_P_BSY,		FC_PKT_NPORT_BSY,	FC_REASON_FCA_UNIQUE },
{ USOC_F_BSY,		FC_PKT_FABRIC_BSY,	FC_REASON_FCA_UNIQUE },
{ USOC_ONLINE, 		FC_PKT_LOCAL_RJT, 	FC_REASON_NO_CONNECTION },
{ USOC_OFFLINE,		FC_PKT_PORT_OFFLINE,	FC_REASON_OFFLINE },
{ USOC_TIMEOUT, 	FC_PKT_TIMEOUT,		FC_REASON_FCA_UNIQUE },
{ USOC_OVERRUN,		FC_PKT_TRAN_ERROR,	FC_REASON_OVERRUN },
{ USOC_OLDPORT_ONLINE, 	FC_PKT_LOCAL_RJT,	FC_REASON_NO_CONNECTION },
{ USOC_UNKNOWN_CQ_TYPE,	FC_PKT_LOCAL_RJT, 	FC_REASON_UNSUPPORTED },
{ USOC_MAX_XCHG_EXCEEDED, FC_PKT_LOCAL_RJT, 	FC_REASON_QFULL },
{ USOC_LOOP_ONLINE, 	FC_PKT_LOCAL_RJT, 	FC_REASON_NO_CONNECTION },
{ USOC_BAD_SEG_CNT, 	FC_PKT_LOCAL_RJT, 	FC_REASON_ILLEGAL_REQ },
{ USOC_BAD_XID, 	FC_PKT_LOCAL_RJT, 	FC_REASON_BAD_XID },
{ USOC_XCHG_BUSY, 	FC_PKT_TRAN_ERROR, 	FC_REASON_XCHG_BSY },
{ USOC_BAD_POOL_ID, 	FC_PKT_LOCAL_RJT, 	FC_REASON_ILLEGAL_REQ },
{ USOC_INSUFFICIENT_CQES, FC_PKT_LOCAL_RJT, 	FC_REASON_QFULL },
{ USOC_ALLOC_FAIL,	FC_PKT_LOCAL_RJT, 	FC_REASON_NOMEM },
{ USOC_BAD_SID,		FC_PKT_LOCAL_RJT, 	FC_REASON_BAD_SID },
{ USOC_NO_SEQ_INIT,	FC_PKT_TRAN_ERROR,	FC_REASON_NO_SEQ_INIT },
{ USOC_ABORTED,		FC_PKT_SUCCESS, 	FC_REASON_ABORTED },
{ USOC_ABORT_FAILED,	FC_PKT_TRAN_ERROR, 	FC_REASON_ABORT_FAILED },
{ USOC_DIAG_BUSY,	FC_PKT_LOCAL_RJT, 	FC_REASON_DIAG_BUSY },
{ USOC_DIAG_ILL_RQST,	FC_PKT_LOCAL_RJT,	FC_REASON_ILLEGAL_REQ },
{ USOC_INCOMPLETE_DMA_ERROR, FC_PKT_LOCAL_RJT, 	FC_REASON_DMA_ERROR },
{ USOC_FC_CRC_ERROR,	FC_PKT_TRAN_ERROR,	FC_REASON_CRC_ERROR },
{ USOC_OPEN_FAIL,	FC_PKT_LOCAL_RJT,	FC_REASON_FCAL_OPN_FAIL }

};

/*
 * Driver private variables.
 */
static void *usoc_soft_state_p = NULL;
static clock_t	usoc_period;

static kmutex_t	usoc_global_lock;
static uchar_t	usoc_xrambuf[0x40000];
static int	usoc_core_taken = 0;
static uint32_t usoc_lfd_interval = USOC_LFD_INTERVAL; /* secs */
static int usoc_use_unsol_intr_new = 0;
static uint32_t usoc_dbg_drv_hdn = USOC_DDH_NOERR;

#ifdef	DEBUG

static int usoc_core_flags = USOC_CORE_ON_ABORT_TIMEOUT |
	USOC_CORE_ON_BAD_TOKEN | USOC_CORE_ON_BAD_ABORT |
	USOC_CORE_ON_LOW_THROTTLE;

static	int usoc_debug = 0;
static	int usoc_of_timeouts = 0;	/* for curiosity */

#define	DEBUGF(level, args)\
	if (usoc_debug >= (level)) cmn_err args;

#else

static int usoc_core_flags = 0;

#define	DEBUGF(level, args)

#endif

/*
 * Firmware related externs
 */
extern uint32_t usoc_ucode[];
extern uint32_t usoc_ucode_end;
extern uint32_t usoc_fw_len;

extern struct mod_ops mod_driverops;

#define	USOC_NAME	"Sun SOC+ Fibre Channel Device Driver v1.10"

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	USOC_NAME,
	&usoc_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};


/*
 * This is the module initialization/completion routines
 */
int
_init(void)
{
	int stat;

	stat = ddi_soft_state_init(&usoc_soft_state_p,
	    sizeof (usoc_state_t), USOC_INIT_ITEMS);
	if (stat != 0) {
		return (stat);
	}

	fc_fca_init(&usoc_ops);

	stat = mod_install(&modlinkage);
	if (stat != 0) {
		ddi_soft_state_fini(&usoc_soft_state_p);
	} else {
		mutex_init(&usoc_global_lock, NULL, MUTEX_DRIVER, NULL);
		usoc_period = drv_usectohz(USOC_WATCH_TIMER);
	}

	return (stat);
}


int
_fini(void)
{
	int stat;

	/* remove the module */
	if ((stat = mod_remove(&modlinkage)) != 0) {
		return (stat);
	}

	/* destroy the soft state structure */
	ddi_soft_state_fini(&usoc_soft_state_p);
	mutex_destroy(&usoc_global_lock);

	return (stat);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Function: usoc_attach
 *
 * Arguments:
 *   dip: the dev_info pointer for the instance
 *   cmd: DDI_ATTACH, DDI_RESUME or DDI_PMRESUME
 *
 * Return values:
 *   DDI_SUCCESS:  The module was successfully attached or resumed.
 *   DDI_FAILURE:  The module did not successfully attach
 *
 * This function ensures a valid SOC+ card is installed and minimally
 * functional.  It then allocates sturctures needed for the instance,
 * including mutexes, condition variables and various single-use
 * structures.  Registers and on-board xram is mapped to DVMA space,
 * firmware is downloaded, interrupts are enabled,a nd the firmware is
 * allowed to run.  Device nodes are created for administrative interfaces.
 *
 * If the attach fails, all resources are freed and a failure condition
 * is returned.
 */
static int
usoc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int			instance;
	usoc_state_t		*usocp;
	struct ether_addr	ourmacaddr;
	usoc_port_t		*porta, *portb;
	char			buf[MAXPATHLEN];
	char			*cptr, *wwn;
	int			y;
	int			i, j;
	int			burstsize;
	short			s;

	instance = ddi_get_instance(dip);

	if (cmd == DDI_RESUME) {
		return (usoc_doresume(dip));
	} else if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);
	}

	DEBUGF(4, (CE_CONT, "usoc%d entering attach\n", instance));

	if (ddi_intr_hilevel(dip, 0)) {
		/*
		 * Interrupt number '0' is a high-level interrupt.
		 * At this point you either add a special interrupt
		 * handler that triggers a soft interrupt at a lower level,
		 * or - more simply and appropriately here - you just
		 * fail the attach.
		 */
		cmn_err(CE_WARN,
		    "!usoc%d attach : hilevel interrupt unsupported",
		    instance);
		return (DDI_FAILURE);
	}

	/* Allocate soft state. */
	if (ddi_soft_state_zalloc(usoc_soft_state_p, instance)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "!usoc%d attach: alloc soft state",
			instance);
		return (DDI_FAILURE);
	}
	DEBUGF(4, (CE_CONT, "usoc%d attach: allocated soft state\n",
		instance));

	/*
	 * Initialize the state structure.
	 */
	usocp = ddi_get_soft_state(usoc_soft_state_p, instance);
	if (usocp == (usoc_state_t *)NULL) {
		cmn_err(CE_WARN, "!usoc%d attach: bad soft state",
			instance);
		return (DDI_FAILURE);
	}
	DEBUGF(4, (CE_CONT, "usoc%d attach: usoc soft state ptr=0x%p \n",
		instance, (void *)usocp));

	usocp->usoc_dip = dip;
	usocp->usoc_instance = instance;

	porta = &usocp->usoc_port_state[0];
	portb = &usocp->usoc_port_state[1];

	/* Get the full path name for displaying error messages */
	cptr = ddi_pathname(dip, buf);
	(void) strcpy(usocp->usoc_name, cptr);

	/* set up the port-specific info */
	porta->sp_unsol_callb = NULL;
	portb->sp_unsol_callb = NULL;
	porta->sp_statec_callb = NULL;
	portb->sp_statec_callb = NULL;
	porta->sp_port = 0;
	portb->sp_port = 1;
	porta->sp_board = usocp;
	portb->sp_board = usocp;

	/*
	 * Get our Node wwn and calculate port wwns
	 *
	 * If there's no Node WWN property, we use this machine's
	 * mac address as the WWN.
	 * Alignment problems result in some convoluted copying techniques.
	 * The Network Address Authority is set to IEEE in this case.
	 */
	if ((ddi_prop_op(DDI_DEV_T_ANY, dip,
	    PROP_LEN_AND_VAL_ALLOC, DDI_PROP_DONTPASS |
	    DDI_PROP_CANSLEEP, "wwn", (caddr_t)&wwn, &i) != DDI_SUCCESS) ||
		(i < USOC_WWN_SIZE) ||
		(bcmp(wwn, "00000000", USOC_WWN_SIZE) == 0)) {
		(void) localetheraddr((struct ether_addr *)NULL, &ourmacaddr);

		bcopy((caddr_t)&ourmacaddr, (caddr_t)&s, sizeof (short));
		usocp->usoc_n_wwn.w.wwn_hi = s;
		bcopy((caddr_t)&ourmacaddr+2,
			(caddr_t)&usocp->usoc_n_wwn.w.wwn_lo, sizeof (uint_t));
		usocp->usoc_n_wwn.w.naa_id = NAA_ID_IEEE;
		usocp->usoc_n_wwn.w.nport_id = 0;
	} else {
		bcopy((caddr_t)wwn, (caddr_t)&usocp->usoc_n_wwn, USOC_WWN_SIZE);
		kmem_free(wwn, i);
	}

	for (i = 0; i < USOC_WWN_SIZE; i++) {
		(void) sprintf(&usocp->usoc_stats.node_wwn[i << 1],
			"%02x", usocp->usoc_n_wwn.raw_wwn[i]);
	}

	DEBUGF(4, (CE_CONT, "usoc%d attach: node wwn: 0x%08x%08x\n", instance,
	    *(uint32_t *)usocp->usoc_n_wwn.raw_wwn,
	    *(uint32_t *)&usocp->usoc_n_wwn.raw_wwn[4]));

	/*
	 * Assign the port WorldWide Names.  These are derived from
	 * the node WWN.
	 * XXX we imbed the instance number in the port WWN.
	 * we probably shouldn't
	 */
	bcopy((caddr_t)&usocp->usoc_n_wwn, (caddr_t)&porta->sp_p_wwn,
		sizeof (la_wwn_t));
	bcopy((caddr_t)&usocp->usoc_n_wwn, (caddr_t)&portb->sp_p_wwn,
		sizeof (la_wwn_t));
	porta->sp_p_wwn.w.naa_id = NAA_ID_IEEE_EXTENDED;
	portb->sp_p_wwn.w.naa_id = NAA_ID_IEEE_EXTENDED;
	porta->sp_p_wwn.w.nport_id = instance*2;
	portb->sp_p_wwn.w.nport_id = instance*2+1;

	for (i = 0; i < USOC_WWN_SIZE; i++) {
		(void) sprintf(&usocp->usoc_stats.port_wwn[0][i << 1],
			"%02x", porta->sp_p_wwn.raw_wwn[i]);
		(void) sprintf(&usocp->usoc_stats.port_wwn[1][i << 1],
			"%02x", portb->sp_p_wwn.raw_wwn[i]);
	}

	DEBUGF(4, (CE_CONT, "usoc%d attach: porta wwn: 0x%08x%08x\n", instance,
	    *(uint32_t *)porta->sp_p_wwn.raw_wwn,
	    *(uint32_t *)&porta->sp_p_wwn.raw_wwn[4]));

	DEBUGF(4, (CE_CONT, "usoc%d attach: portb wwn: 0x%08x%08x\n", instance,
	    *(uint32_t *)portb->sp_p_wwn.raw_wwn,
	    *(uint32_t *)&portb->sp_p_wwn.raw_wwn[4]));

	DEBUGF(4, (CE_CONT, "usoc%d attach: allocated transport structs\n",
		instance));

	/*
	 * Map the external ram and registers for SOC+.
	 * Note: Soc+ sbus host adapter provides 3 register definition
	 * but on-board Soc+'s  may have only one register definition.
	 */
	if ((ddi_dev_nregs(dip, &i) == DDI_SUCCESS) && (i == 1)) {

		/* Map XRAM */
		if (ddi_regs_map_setup(dip, 0, (caddr_t *)&usocp->usoc_xrp, 0,
		    USOC_XRAM_SIZE, &usoc_acc_attr, &usocp->usoc_xrp_acchandle)
			!= DDI_SUCCESS) {
			usocp->usoc_xrp = NULL;
			cmn_err(CE_WARN, "usoc(%d):attach: unable to map"
			    "XRAM", instance);
			goto fail;
		}

		DEBUGF(4, (CE_CONT, "usoc%d attach: mapped xram %p\n",
		    instance, (void *)(usocp->usoc_xrp)));

		/* Map registers */
		if (ddi_regs_map_setup(dip, 0, (caddr_t *)&usocp->usoc_rp,
		    USOC_XRAM_SIZE, sizeof (usoc_reg_t), &usoc_acc_attr,
		    &usocp->usoc_rp_acchandle) != DDI_SUCCESS) {
			usocp->usoc_rp = NULL;
			cmn_err(CE_WARN, "usoc(%d): attach: unable to map"
			    " registers", instance);
			goto fail;
		}

		DEBUGF(4, (CE_CONT, "usoc%d attach: mapped regs %p\n",
		    instance, (void *)(usocp->usoc_rp)));
	} else {
		/* Map EEPROM */
		if (ddi_regs_map_setup(dip, 0, (caddr_t *)&usocp->usoc_eeprom,
		    0, 0, &usoc_acc_attr, &usocp->usoc_eeprom_acchandle)
			!= DDI_SUCCESS) {
			usocp->usoc_eeprom = NULL;
			cmn_err(CE_WARN, "usoc(%d): attach: unable to map"
			    " eeprom", instance);
			goto fail;
		}

		DEBUGF(4, (CE_CONT, "usoc%d attach: mapped eeprom %p\n",
		    instance, (void *)(usocp->usoc_eeprom)));

		/* Map XRAM */
		if (ddi_regs_map_setup(dip, 1, (caddr_t *)&usocp->usoc_xrp, 0,
		    0, &usoc_acc_attr, &usocp->usoc_xrp_acchandle) !=
		    DDI_SUCCESS) {
			usocp->usoc_xrp = NULL;
			cmn_err(CE_WARN, "usoc(%d): attach: unable to"
			    " map XRAM", instance);
			goto fail;
		}

		DEBUGF(4, (CE_CONT, "usoc%d attach: mapped xram %p\n",
		    instance, (void *)(usocp->usoc_xrp)));

		/* Map registers */
		if (ddi_regs_map_setup(dip, 2, (caddr_t *)&usocp->usoc_rp, 0,
		    0, &usoc_acc_attr, &usocp->usoc_rp_acchandle) !=
		    DDI_SUCCESS) {
			usocp->usoc_xrp = NULL;
			cmn_err(CE_WARN, "usoc(%d): attach: unable to map"
			    " registers", instance);
			goto fail;
		}

		DEBUGF(4, (CE_CONT, "usoc%d attach: mapped regs %p\n",
		    instance, (void *)(usocp->usoc_rp)));
	}

	/*
	 * Check to see we really have a SOC+ Host Adapter card installed
	 */
	DEBUGF(4, (CE_CONT, "usoc%d attach: tried reading CSR reg, val %08x\n",
	    instance, USOC_RD32(usocp->usoc_rp_acchandle,
	    &usocp->usoc_rp->usoc_csr.w)));

	/* now that we have our registers mapped make sure soc+ reset */
	usoc_disable(usocp);

	/* try defacing a spot in XRAM and check if it stayes defaced */
	USOC_WR32(usocp->usoc_xrp_acchandle, usocp->usoc_xrp + USOC_XRAM_UCODE,
	    0xdefaced);

	y = USOC_RD32(usocp->usoc_xrp_acchandle, usocp->usoc_xrp +
	    USOC_XRAM_UCODE);

	DEBUGF(4, (CE_CONT, "usoc%d attach: read xram\n", instance));

	if (y != 0xdefaced) {
		cmn_err(CE_WARN, "usoc(%d): attach: read/write mismatch"
		    " in XRAM", instance);
		goto fail;
	}

	/*
	 * Set the request and response queue pointers to the USOC
	 * XRAM CQ Descriptor locations. We will use the
	 * usocp->usoc_xrp_acchandle to access these XRAM locations.
	 */
	usocp->usoc_xram_reqp = (usoc_cq_t *)(usocp->usoc_xrp +
	    USOC_XRAM_REQ_DESC);
	usocp->usoc_xram_rspp = (usoc_cq_t *)(usocp->usoc_xrp +
	    USOC_XRAM_RSP_DESC);

	/*
	 * Initialize kstat information
	 */
	if ((usocp->usoc_ksp = kstat_create("usoc", instance, "statistics",
	    "controller", KSTAT_TYPE_RAW, sizeof (struct usoc_stats),
	    KSTAT_FLAG_VIRTUAL)) == NULL) {
		cmn_err(CE_WARN, "usoc(%d): attach: to create kstats",
		    instance);
	} else {
		usocp->usoc_stats.version = 2;
		(void) strcpy(usocp->usoc_stats.drvr_name, USOC_NAME);
		usocp->usoc_stats.pstats[0].port = 0;
		usocp->usoc_stats.pstats[1].port = 1;
		usocp->usoc_ksp->ks_data = (void *)&usocp->usoc_stats;
		kstat_install(usocp->usoc_ksp);
	}

	/*
	 * Get iblock cookies to initialize mutexes
	 */
	if (ddi_get_iblock_cookie(dip, 0, &usocp->usoc_iblkc) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "usoc(%d): attach: unable to"
		    " get iblock cookie", instance);
		goto fail;
	}

	usocp->usoc_task_handle = taskq_create("usoc_timeout",
	    USOC_NUM_TIMEOUT_THREADS, MINCLSYSPRI,
	    USOC_MIN_PACKETS, USOC_MAX_PACKETS, 0);

	mutex_init(&usocp->usoc_time_mtx, NULL, MUTEX_DRIVER,
	    (void *)usocp->usoc_iblkc);

	/* initialize the interrupt mutex */
	mutex_init(&usocp->usoc_k_imr_mtx, NULL, MUTEX_DRIVER,
	    (void *)usocp->usoc_iblkc);

	mutex_init(&usocp->usoc_board_mtx, NULL, MUTEX_DRIVER,
	    (void *)usocp->usoc_iblkc);

	mutex_init(&usocp->usoc_fault_mtx, NULL, MUTEX_DRIVER,
	    (void *)usocp->usoc_iblkc);

	/*
	 * Board mutex is now initialized. So Initialize the ID Free list
	 */
	usoc_init_ids(usocp);

	/* init the port mutexes */
	mutex_init(&porta->sp_mtx, NULL, MUTEX_DRIVER, usocp->usoc_iblkc);
	cv_init(&porta->sp_cv, NULL, CV_DRIVER, NULL);
	mutex_init(&portb->sp_mtx, NULL, MUTEX_DRIVER, usocp->usoc_iblkc);
	cv_init(&portb->sp_cv, NULL, CV_DRIVER, NULL);
	DEBUGF(4, (CE_CONT, "usoc%d: attach: inited port mutexes and cvs\n",
		instance));

	/* get local copy of service params */
	USOC_REP_RD32(usocp->usoc_xrp_acchandle,
	    usocp->usoc_service_params,
	    usocp->usoc_xrp + USOC_XRAM_SERV_PARAMS, USOC_SVC_LENGTH);
	DEBUGF(4, (CE_CONT, "usoc%d: attach: got service params\n", instance));

	usocp->usoc_throttle = USOC_MAX_THROTTLE;

	/*
	 * Allocate request and response queues and init their mutexs.
	 */
	for (i = 0; i < USOC_N_CQS; i++) {
		if (usoc_cqalloc_init(usocp, i) != FC_SUCCESS) {
			goto fail;
		}
	}
	DEBUGF(4, (CE_CONT, "usoc%d: attach: allocated cqs\n", instance));

	/*
	 * Adjust the burst size we'll use.
	 */
	burstsize = ddi_dma_burstsizes(usocp->usoc_request[0].skc_dhandle);
	DEBUGF(4, (CE_CONT, "usoc%d: attach: burstsize = 0x%x\n",
		instance, burstsize));
	j = burstsize & BURSTSIZE_MASK;
	for (i = 0; usoc_burst32_table[i] != USOC_CR_BURST_64; i++)
		if (!(j >>= 1)) break;

	usocp->usoc_cfg = (usocp->usoc_cfg & ~USOC_CR_SBUS_BURST_SIZE_MASK)
		| usoc_burst32_table[i];

	if (ddi_dma_set_sbus64(usocp->usoc_request[0].skc_dhandle,
	    usoc_dma_attr.dma_attr_burstsizes | BURST128) == DDI_SUCCESS) {

		DEBUGF(4, (CE_CONT, "usoc%d: enabled 64 bit sbus\n", instance));

		usocp->usoc_cfg |= USOC_CR_SBUS_ENHANCED;
		burstsize = ddi_dma_burstsizes(usocp->usoc_request[0].
		    skc_dhandle);

		DEBUGF(4, (CE_CONT, "usoc%d: attach: 64bit burstsize = 0x%x\n",
		    instance, burstsize));

		j = burstsize & BURSTSIZE_MASK;
		for (i = 0; usoc_burst64_table[i] != (USOC_CR_BURST_128 << 8);
		    i++) {
			if (!(j >>= 1)) {
				break;
			}
		}
		usocp->usoc_cfg = (usocp->usoc_cfg &
		    ~USOC_CR_SBUS_BURST_SIZE_64BIT_MASK) |
		    usoc_burst64_table[i];
	}

	/*
	 * Install the interrupt routine.
	 */
	if (ddi_add_intr(dip, (uint_t)0, NULL, NULL, usoc_intr,
	    (caddr_t)usocp) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "usoc(%d): attach: unable"
		    " to install interrupt handler", instance);
		goto fail;
	}

	/* Set a flag, so that detach will call ddi_remove_intr() */
	usocp->usoc_intr_added = 1;

	DEBUGF(4, (CE_CONT, "usoc%d: attach: set config reg %x\n",
		instance, usocp->usoc_cfg));

	/*
	 * create the minor nodes for these port instances
	 */
	if (ddi_create_minor_node(dip, USOC_PORTA_NAME, S_IFCHR,
		instance*N_USOC_NPORTS, USOC_NT_PORT, 0) != DDI_SUCCESS)
		goto fail;

	if (ddi_create_minor_node(dip, USOC_PORTB_NAME, S_IFCHR,
		instance*N_USOC_NPORTS+1, USOC_NT_PORT, 0) != DDI_SUCCESS)
		goto fail;
	/*
	 * finish filling out the tran structure and give it to the transport
	 */
	usoc_tran.fca_iblock = usocp->usoc_iblkc;

	(void) fc_fca_attach(dip, &usoc_tran);

	/*
	 * start the SOC+ sequencer
	 */
	if (usoc_start(usocp) != FC_SUCCESS) {
		goto fail;
	}

	DEBUGF(4, (CE_CONT, "usoc%d: attach: soc+ started\n", instance));

	ddi_report_dev(dip);

	DEBUGF(2, (CE_CONT, "usoc%d: attach O.K.\n\n", instance));
	usocp->usoc_watch_tid = timeout(usoc_watchdog, (void *)usocp,
	    usoc_period);

	/*
	 * Allow DDI_SUSPEND and DDI_RESUME
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
	    "pm-hardware-state", "needs-suspend-resume",
	    strlen("needs-suspend-resume") + 1);

	return (DDI_SUCCESS);
fail:
	DEBUGF(4, (CE_CONT, "usoc%d: attach: DDI_FAILURE\n", instance));

	/* Make sure usoc reset */
	usoc_disable(usocp);

	/* let detach do the dirty work */
	(void) usoc_dodetach(dip);

	return (DDI_FAILURE);
}


static int
usoc_doresume(dev_info_t *dip)
{
	int		i;
	usoc_state_t	*usocp;

	/* Get the soft state  */
	if ((usocp = ddi_get_soft_state(usoc_soft_state_p,
	    ddi_get_instance(dip))) == NULL) {
		return (DDI_FAILURE);
	}

	mutex_enter(&usocp->usoc_k_imr_mtx);
	if (usocp->usoc_shutdown == 0) {
		mutex_exit(&usocp->usoc_k_imr_mtx);
		return (DDI_SUCCESS);
	}
	usocp->usoc_shutdown = 0;
	mutex_exit(&usocp->usoc_k_imr_mtx);

	/*
	 * The timeout ID during DDI_SUSPEND was preserved
	 * just to be able to know whether a timeout should
	 * be fired during DDI_RESUME.
	 */
	if (usocp->usoc_watch_tid) {
		usocp->usoc_watch_tid = timeout(usoc_watchdog,
		    (void *)usocp, usoc_period);
	}

	for (i = 0; i < USOC_N_CQS; i++) {
		if (usocp->usoc_request[i].deferred_intr_timeoutid) {
			usocp->usoc_request[i].deferred_intr_timeoutid =
			    timeout(usoc_deferred_intr, (caddr_t)usocp,
			    drv_usectohz(10000));
		}

		if (usocp->usoc_response[i].deferred_intr_timeoutid) {
			usocp->usoc_response[i].deferred_intr_timeoutid =
			    timeout(usoc_deferred_intr, (caddr_t)usocp,
			    drv_usectohz(10000));
		}
	}

	(void) usoc_force_reset(usocp, 1, 0);

	return (DDI_SUCCESS);
}


/*
 * Function: usoc_detach
 *
 * Arguments:
 *   dip: the dev_info pointer for the instance
 *   cmd: DDI_DETACH, DDI_SUSPEND or DDI_PM_SUSPEND
 *
 * Return values:
 *   DDI_SUCCESS:  The module was successfully attached or resumed.
 *   DDI_FAILURE:  The module did not successfully attach
 *
 * Responsible for ensuring all resources are freed the SOC card is
 * disabled.
 *
 * The suspend functions are not yet implemented.
 */
static int
usoc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd == DDI_DETACH) {
		return (usoc_dodetach(dip));
	} else if (cmd == DDI_SUSPEND) {
		return (usoc_dosuspend(dip));
	}

	return (DDI_FAILURE);
}


/*
 * Function: usoc_dodetach
 *
 * Arguments:
 *   dip: the dev_info pointer for the instance
 *
 * Return values:
 *   DDI_SUCCESS:  The module was successfully attached or resumed.
 *   DDI_FAILURE:  The module did not successfully attach
 *
 * Destroys all mutex and condition variables associated with this
 * instance.  Request and response quese are destroyed, buffer pools
 * are deallocated.
 *
 * Register and xram space is unmapped and interrupts are disabled.
 *
 */
static int
usoc_dodetach(dev_info_t *dip)
{
	int		i;
	usoc_state_t	*usocp;
	usoc_port_t	*portp;
	uint32_t	delay_count;

	/* Get the soft state struct. */
	if ((usocp = ddi_get_soft_state(usoc_soft_state_p,
	    ddi_get_instance(dip))) == 0) {
		return (DDI_FAILURE);
	}

	ddi_prop_remove_all(dip);

	if (usocp->usoc_watch_tid) {
		(void) untimeout(usocp->usoc_watch_tid);
		usocp->usoc_watch_tid = NULL;
	}

	delay_count = 0;
	while ((usocp->usoc_lfd_pending ||
	    (usoc_dbg_drv_hdn == USOC_DDH_FAIL_LATENT_DETACH)) &&
	    delay_count < USOC_LFD_WAIT_COUNT) {
		delay_count++;
		delay(drv_usectohz(USOC_LFD_WAIT_DELAY));
	}

	if (usocp->usoc_lfd_pending ||
	    (usoc_dbg_drv_hdn == USOC_DDH_FAIL_LATENT_DETACH)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "usoc_detach: Latent Fault Detection Pending");

		return (DDI_FAILURE);
	}

	usoc_disable(usocp);

	/*
	 * Free usoc data buffer pool
	 */
	if (usocp->usoc_pool_dhandle || usocp->usoc_pool) {
		if (usocp->usoc_pool_dhandle) {
			(void) ddi_dma_unbind_handle(usocp->usoc_pool_dhandle);
		}

		if (usocp->usoc_pool) {
			ddi_dma_mem_free(&usocp->usoc_pool_acchandle);
			usocp->usoc_pool = NULL;
		}

		if (usocp->usoc_pool_dhandle) {
			ddi_dma_free_handle(&usocp->usoc_pool_dhandle);
			usocp->usoc_pool_dhandle = NULL;
		}
	}

	/*
	 * tell those above that we're going away now
	 */
	(void) fc_fca_detach(dip);

	/*
	 * Remove minor nodes for both ports
	 */
	ddi_remove_minor_node(dip, NULL);

	/* Remove soc+ interrupt */
	if (usocp->usoc_intr_added) {
		ddi_remove_intr(dip, 0, usocp->usoc_iblkc);
		DEBUGF(2, (CE_CONT,
		    "usoc%d: detach: Removed SOC+ interrupt from ddi\n",
		    usocp->usoc_instance));
	}

	for (i = 0; i < USOC_N_CQS; i++) {
		if (usocp->usoc_request[i].skc_dhandle ||
		    usocp->usoc_request[i].skc_cq_raw) {

			if (usocp->usoc_request[i].skc_dhandle) {
				(void) ddi_dma_unbind_handle(
				    usocp->usoc_request[i].skc_dhandle);
			}

			if (usocp->usoc_request[i].skc_cq_raw) {
				ddi_dma_mem_free(
				    &usocp->usoc_request[i].skc_acchandle);
				usocp->usoc_request[i].skc_cq_raw = NULL;
				usocp->usoc_request[i].skc_cq = NULL;
			}


			if (usocp->usoc_request[i].skc_dhandle) {
				ddi_dma_free_handle(
				    &usocp->usoc_request[i].skc_dhandle);
				usocp->usoc_request[i].skc_dhandle = NULL;
			}
		}

		if (usocp->usoc_response[i].skc_dhandle ||
		    usocp->usoc_response[i].skc_cq_raw) {

			if (usocp->usoc_response[i].skc_dhandle) {
				(void) ddi_dma_unbind_handle(
				    usocp->usoc_response[i].skc_dhandle);
			}

			if (usocp->usoc_response[i].skc_cq_raw) {
				ddi_dma_mem_free(
				    &usocp->usoc_response[i].skc_acchandle);
				usocp->usoc_response[i].skc_cq_raw = NULL;
				usocp->usoc_response[i].skc_cq = NULL;
			}

			if (usocp->usoc_response[i].skc_dhandle) {
				ddi_dma_free_handle(
				    &usocp->usoc_response[i].skc_dhandle);
				usocp->usoc_response[i].skc_dhandle = NULL;
			}
		}

		if (usocp->usoc_request[i].deferred_intr_timeoutid) {
			(void) untimeout(usocp->
				usoc_request[i].deferred_intr_timeoutid);

			usocp->usoc_request[i].deferred_intr_timeoutid = NULL;
		}

		if (usocp->usoc_response[i].deferred_intr_timeoutid) {
			(void) untimeout(usocp->
				usoc_response[i].deferred_intr_timeoutid);

			usocp->usoc_response[i].deferred_intr_timeoutid = NULL;
		}

		mutex_destroy(&usocp->usoc_request[i].skc_mtx);
		mutex_destroy(&usocp->usoc_response[i].skc_mtx);

		cv_destroy(&usocp->usoc_request[i].skc_cv);
		cv_destroy(&usocp->usoc_response[i].skc_cv);
	}

	for (i = 0; i < N_USOC_NPORTS; i++) {
		portp = &usocp->usoc_port_state[i];

		cv_destroy(&portp->sp_cv);
		mutex_destroy(&portp->sp_mtx);
	}

	mutex_destroy(&usocp->usoc_time_mtx);
	mutex_destroy(&usocp->usoc_board_mtx);
	mutex_destroy(&usocp->usoc_k_imr_mtx);
	mutex_destroy(&usocp->usoc_fault_mtx);

	if (usocp->usoc_task_handle) {
		taskq_destroy(usocp->usoc_task_handle);
	}

	if (usocp->usoc_ksp != NULL) {
		kstat_delete(usocp->usoc_ksp);
	}

	/* Release register maps */
	/* Unmap EEPROM */
	if (usocp->usoc_eeprom != NULL) {
		ddi_regs_map_free(&usocp->usoc_eeprom_acchandle);
	}

	/* Unmap XRAM */
	if (usocp->usoc_xrp != NULL) {
		ddi_regs_map_free(&usocp->usoc_xrp_acchandle);
	}

	/* Unmap registers */
	if (usocp->usoc_rp != NULL) {
		ddi_regs_map_free(&usocp->usoc_rp_acchandle);
	}

	ddi_soft_state_free(usoc_soft_state_p, ddi_get_instance(dip));

	return (DDI_SUCCESS);
}


static int
usoc_dosuspend(dev_info_t *dip)
{
	int		i;
	usoc_state_t	*usocp;
	uint32_t	delay_count;

	/* Get the soft state  */
	if ((usocp = ddi_get_soft_state(usoc_soft_state_p,
	    ddi_get_instance(dip))) == NULL) {
		return (DDI_FAILURE);
	}

	if (usocp->usoc_watch_tid) {
		(void) untimeout(usocp->usoc_watch_tid);
	}

	delay_count = 0;
	while ((usocp->usoc_lfd_pending ||
	    (usoc_dbg_drv_hdn == USOC_DDH_FAIL_LATENT_SUSPEND)) &&
	    delay_count < USOC_LFD_WAIT_COUNT) {
		delay_count++;
		delay(drv_usectohz(USOC_LFD_WAIT_DELAY));
	}

	if (usocp->usoc_lfd_pending) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "usoc_suspend: Latent Fault Detection"
		    " Pending");

		if (usocp->usoc_watch_tid) {
			usocp->usoc_watch_tid = timeout(usoc_watchdog,
			    (void *)usocp, usoc_period);
		}
		return (DDI_FAILURE);
	}

	for (i = 0; i < USOC_N_CQS; i++) {
		if (usocp->usoc_request[i].deferred_intr_timeoutid) {
			(void) untimeout(usocp->
				usoc_request[i].deferred_intr_timeoutid);
		}

		if (usocp->usoc_response[i].deferred_intr_timeoutid) {
			(void) untimeout(usocp->
				usoc_response[i].deferred_intr_timeoutid);
		}
	}
	(void) usoc_force_reset(usocp, 0, 0);

	return (DDI_SUCCESS);
}


/*
 * Function: usoc_bind_port
 *
 * Arguments:
 *   dip: the dev_info pointer for the instance
 *   port_info: pointer to info handed back to the transport
 *   bind info: pointer to info from the transport
 *
 * Return values:
 *   a port handle for this port on the SOC+, NULL for failure
 *
 * Called by the transport layer to register the callback functions and to
 * get the port information like login paramters, WWN, portid etc...
 */
static opaque_t
usoc_bind_port(dev_info_t *dip, fc_fca_port_info_t *port_info,
    fc_fca_bind_info_t *bind_info)
{
	usoc_state_t	*usocp;
	usoc_port_t	*port_statep;
	ddi_devstate_t	devstate;

	usocp = ddi_get_soft_state(usoc_soft_state_p, ddi_get_instance(dip));

	/* Make sure the port number is out of range */
	if (bind_info->port_num >= N_USOC_NPORTS) {
		port_info->pi_error = FC_OUTOFBOUNDS;
		return (NULL);
	}

	port_statep = &usocp->usoc_port_state[bind_info->port_num];

	mutex_enter(&port_statep->sp_mtx);

	/* Make sure the port is already bound to the transport */

	if (port_statep->sp_status & PORT_BOUND) {
		port_info->pi_error = FC_ALREADY;
		mutex_exit(&port_statep->sp_mtx);
		return (NULL);
	}

	if (USOC_DEVICE_BAD(usocp, devstate)) {
		DEBUGF(2, (CE_WARN, "!usoc%d: bind portnum %x. "
		    "devstate %s(%d)", usocp->usoc_instance,
		    bind_info->port_num, USOC_DEVICE_STATE(devstate),
		    devstate));

		port_info->pi_error = FC_OFFLINE;
		mutex_exit(&port_statep->sp_mtx);
		return (NULL);
	}

	/*
	 * XXX - need to figure out if the port is populated
	 */

	/* Keep track of the trasnport port handle and callback functions */
	port_statep->sp_status |= PORT_BOUND;
	port_statep->sp_tran_handle = bind_info->port_handle;
	port_statep->sp_statec_callb = bind_info->port_statec_cb;
	port_statep->sp_unsol_callb = bind_info->port_unsol_cb;

	/*
	 * since usoc doesn't discover the topology; let's ask
	 * the transport to do its thing. i.e set the topology
	 * to unknown
	 */
	port_info->pi_topology = FC_TOP_UNKNOWN;

	/*
	 * Figure out the stae of the port and inform the transport
	 */
	if (port_statep->sp_status & PORT_ONLINE_LOOP) {
		port_info->pi_port_state = FC_STATE_LOOP;
	} else if (port_statep->sp_status & PORT_ONLINE) {
		port_info->pi_port_state = FC_STATE_ONLINE;
	} else {
		port_info->pi_port_state = FC_STATE_OFFLINE;
	}

	/* Hope the transport knows validity of this is state dependent */
	port_info->pi_s_id.port_id = port_statep->sp_src_id;

	/*
	 * the trasnport needs a copy of the common service parameters
	 * for this port.  The transport can get any updates through the
	 * the getcap entry point.
	 */
	usoc_wcopy((uint_t *)usocp->usoc_service_params,
		(uint_t *)(&port_info->pi_login_params.common_service),
		USOC_SVC_LENGTH);

	port_info->pi_login_params.node_ww_name = usocp->usoc_n_wwn;
	port_info->pi_login_params.nport_ww_name = port_statep->sp_p_wwn;

	/*
	 * Until we absolutely support class 1 and class2
	 * let's not announce supporting these classes
	 */
	if ((port_info->pi_login_params.class_1.data[0]) & 0x80) {
		port_info->pi_login_params.class_1.data[0] &= ~0x80;
	}

	if ((port_info->pi_login_params.class_2.data[0]) & 0x80) {
		port_info->pi_login_params.class_2.data[0] &= ~0x80;
	}
	mutex_exit(&port_statep->sp_mtx);

	return ((opaque_t)port_statep);
}


/*
 * Function: usoc_unbind_port
 *
 * Arguments:
 *   portp: the port handle returned from usoc_bind_port
 *
 * Return values:
 *   None.
 *
 * Mark the port as unbound.  Clear callback functions and packet init
 * counter.
 *
 */
static void
usoc_unbind_port(opaque_t portp)
{
	usoc_port_t *port_statep = (usoc_port_t *)portp;

	/*
	 * Don't check devstate. Allow unbind even if device is down/offline
	 */
	mutex_enter(&port_statep->sp_mtx);
	port_statep->sp_status &= ~PORT_BOUND;
	port_statep->sp_statec_callb = NULL;
	port_statep->sp_unsol_callb = NULL;

	/*
	 * XXX - handle unsolicited buffers still allocated
	 */
	mutex_exit(&port_statep->sp_mtx);
}


/* ARGSUSED */
static int
usoc_init_pkt(opaque_t portp, fc_packet_t *pkt, int sleep)
{
	usoc_port_t *port_statep = (usoc_port_t *)portp;
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;

	mutex_enter(&port_statep->sp_mtx);
	port_statep->sp_pktinits++;
	if ((!(priv->spp_flags & PACKET_INTERNAL_PACKET) &&
	    !(port_statep->sp_status & PORT_BOUND)) || priv == NULL) {
		mutex_exit(&port_statep->sp_mtx);
		return (FC_UNBOUND);
	}

	mutex_exit(&port_statep->sp_mtx);

	priv->spp_portp = port_statep;
	priv->spp_flags = PACKET_VALID | PACKET_DORMANT;
	mutex_init(&priv->spp_mtx, NULL, MUTEX_DRIVER,
		(void *)port_statep->sp_board->usoc_iblkc);
	priv->spp_next = NULL;
	priv->spp_packet = pkt;

	return (FC_SUCCESS);
}


static int
usoc_uninit_pkt(opaque_t portp, fc_packet_t *pkt)
{
	usoc_port_t *port_statep = (usoc_port_t *)portp;
	usoc_pkt_priv_t *priv =
		(usoc_pkt_priv_t *)pkt->pkt_fca_private;

	mutex_enter(&port_statep->sp_mtx);
	if (!(priv->spp_flags & PACKET_INTERNAL_PACKET) &&
	    !(port_statep->sp_status & PORT_BOUND)) {
		mutex_exit(&port_statep->sp_mtx);
		return (FC_UNBOUND);
	}

	ASSERT((priv->spp_flags & PACKET_IN_PROCESS) == 0);

	if (priv->spp_portp != port_statep ||
	    (priv->spp_flags & PACKET_VALID) == 0 ||
	    (priv->spp_flags & PACKET_DORMANT) == 0) {
		mutex_exit(&port_statep->sp_mtx);
		return (FC_BADPACKET);
	}

	mutex_destroy(&priv->spp_mtx);
	priv->spp_flags = 0;
	priv->spp_next = NULL;
	ASSERT(port_statep->sp_pktinits > 0);
	port_statep->sp_pktinits--;
	mutex_exit(&port_statep->sp_mtx);

	return (FC_SUCCESS);
}


/*ARGSUSED*/
/*
 * int
 * usoc_getinfo() - Given the device number, return the devinfo
 * 	pointer or the instance number.  Note: this routine must be
 * 	successful on DDI_INFO_DEVT2INSTANCE even before attach.
 */
usoc_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
	void **result)
{
	int instance;
	usoc_state_t *usocp;

	instance = getminor((dev_t)arg) / 2;

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		usocp = ddi_get_soft_state(usoc_soft_state_p, instance);
		if (usocp)
			*result = usocp->usoc_dip;
		else
			*result = NULL;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		break;

	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}


/*ARGSUSED*/
usoc_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	int 		instance = getminor(*devp)/2;
	usoc_state_t	*usocp;
	usoc_port_t	*port_statep;
	int		port;

	usocp = ddi_get_soft_state(usoc_soft_state_p, instance);
	if (usocp == NULL) {
		return (ENXIO);
	}

	/* Allow only previliged users to access the port */
	if (drv_priv(cred_p)) {
		return (EPERM);
	}

	port = getminor(*devp) % 2;
	port_statep = &usocp->usoc_port_state[port];

	mutex_enter(&port_statep->sp_mtx);
	/* Check if port is already open for exclusive access */
	if (port_statep->sp_status & PORT_EXCL) {
		mutex_exit(&port_statep->sp_mtx);
		return (EBUSY);
	}

	if (flag & FEXCL) {
		/* Check if exclusive open can be allowed */
		if (port_statep->sp_status & PORT_OPEN) {
			mutex_exit(&port_statep->sp_mtx);
			return (EBUSY);
		}
		port_statep->sp_status |= PORT_EXCL;
	}
	port_statep->sp_status |= PORT_OPEN;
	mutex_exit(&port_statep->sp_mtx);

	DEBUGF(2, (CE_CONT,
	    "usoc%d: open of port %d flag %x\n", usocp->usoc_instance,
	    port, flag));

	return (0);
}


/*ARGSUSED*/
usoc_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	int 		instance = getminor(dev)/2;
	usoc_state_t	*usocp = ddi_get_soft_state(usoc_soft_state_p,
			    instance);
	usoc_port_t	*port_statep;
	int		port;

	port = getminor(dev)%2;
	port_statep = &usocp->usoc_port_state[port];

	mutex_enter(&port_statep->sp_mtx);
	if ((port_statep->sp_status & PORT_OPEN) == 0) {
		mutex_exit(&port_statep->sp_mtx);
		return (ENODEV);
	}
	port_statep->sp_status &= ~(PORT_OPEN | PORT_EXCL);
	mutex_exit(&port_statep->sp_mtx);
	DEBUGF(2, (CE_CONT,
	    "usoc%d: clsoe of port %d\n", instance, port));
	return (0);
}


/*ARGSUSED*/
usoc_ioctl(dev_t dev,
    int cmd, intptr_t arg, int mode, cred_t *cred_p, int *rval_p)
{
	int 	instance = getminor(dev)/2;
	usoc_state_t	*usocp =
			    ddi_get_soft_state(usoc_soft_state_p, instance);
	int		port;
	usoc_port_t	*port_statep;
	int 		i;
	uint_t		r;
	uint32_t	j;
	int		offset;
	int 		retval = FC_SUCCESS;
	dev_info_t	*dip;
	char		*buffer, tmp[10];
	struct usoc_fm_version ver;

	if (usocp == NULL)
		return (ENXIO);

	DEBUGF(4, (CE_CONT, "usoc%d ioctl: got command %x\n", instance, cmd));
	port = getminor(dev)%2;
	port_statep = &usocp->usoc_port_state[port];

	switch (cmd) {
		case USOCIO_FCODE_MCODE_VERSION: {
			ddi_devstate_t	devstate;

			/* Check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}

			if (copyin((caddr_t)arg, (caddr_t)&ver,
			    sizeof (struct usoc_fm_version)) == -1)
				return (EFAULT);
			dip = usocp->usoc_dip;
			if (ddi_prop_op(DDI_DEV_T_ANY, dip,
			    PROP_LEN_AND_VAL_ALLOC, DDI_PROP_DONTPASS |
			    DDI_PROP_CANSLEEP, "version", (caddr_t)&buffer,
			    &i) != DDI_SUCCESS) {
				return (EIO);
			}
			if (i < ver.fcode_ver_len) {
				ver.fcode_ver_len = i;
			}
			if (copyout((caddr_t)buffer, (caddr_t)ver.fcode_ver,
			    ver.fcode_ver_len) == -1) {
				kmem_free((caddr_t)buffer, i);
				return (EFAULT);
			}
			kmem_free((caddr_t)buffer, i);

			if (usocp->usoc_eeprom) {
				for (i = 0; i < USOC_N_CQS; i++) {
					mutex_enter(
					    &usocp->usoc_request[i].skc_mtx);
					mutex_enter(
					    &usocp->usoc_response[i].skc_mtx);
				}

				i = j = USOC_RD32(usocp->usoc_rp_acchandle,
				    &usocp->usoc_rp->usoc_cr.w);
				j = (j & ~USOC_CR_EEPROM_BANK_MASK) | (3 << 16);
				USOC_WR32(usocp->usoc_rp_acchandle,
				    &usocp->usoc_rp->usoc_cr.w, j);
				if (ver.prom_ver_len > 10)
					ver.prom_ver_len = 10;
				USOC_REP_RD(usocp->usoc_eeprom_acchandle,
				    tmp, usocp->usoc_eeprom + (unsigned)0xfff6,
				    10);
				USOC_WR32(usocp->usoc_rp_acchandle,
				    &usocp->usoc_rp->usoc_cr.w, i);
				for (i = USOC_N_CQS-1; i >= 0; i--) {
				    mutex_exit(&usocp->usoc_request[i].skc_mtx);
					mutex_exit(
					    &usocp->usoc_response[i].skc_mtx);
				}
				if (copyout((caddr_t)tmp, (caddr_t)ver.prom_ver,
				    ver.prom_ver_len) == -1)
					return (EFAULT);
			} else {
				ver.prom_ver_len = 0;
			}
			ver.mcode_ver_len = 0;
			if (copyout((caddr_t)&ver, (caddr_t)arg,
			    sizeof (struct usoc_fm_version)) == -1)
				return (EFAULT);
			break;
		}

		case USOCIO_LOADUCODE: {
			ddi_devstate_t	devstate;

			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}
			mutex_enter(&usocp->usoc_k_imr_mtx);
			usoc_disable(usocp);
			mutex_exit(&usocp->usoc_k_imr_mtx);

			if (copyin((caddr_t)arg, (caddr_t)usoc_ucode,
			    0x10000) == -1) {
				return (EFAULT);
			}

			if (usoc_force_reset(usocp, 1, 0) != FC_SUCCESS) {
				return (EIO);
			}
			break;
		}

		case USOCIO_DUMPXRAM: {
			ddi_devstate_t	devstate;

			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}
			for (i = 0; i < USOC_N_CQS; i++) {
				mutex_enter(&usocp->usoc_request[i].skc_mtx);
				mutex_enter(&usocp->usoc_response[i].skc_mtx);
			}
			buffer = (char *)kmem_zalloc(0x10000, KM_SLEEP);
			ASSERT(buffer != NULL);
			for (i = 0; i < 4; i++) {
				offset = arg+(0x10000 * i);

				j = USOC_RD32(usocp->usoc_rp_acchandle,
				    &usocp->usoc_rp->usoc_cr.w);
				j = (j & ~USOC_CR_EXTERNAL_RAM_BANK_MASK) |
				    (i << 24);
				USOC_WR32(usocp->usoc_rp_acchandle,
				    &usocp->usoc_rp->usoc_cr.w, j);

				USOC_REP_RD32(usocp->usoc_xrp_acchandle,
				    buffer, usocp->usoc_xrp, 0x10000);
				(void) copyout((caddr_t)buffer,
					(caddr_t)offset, 0x10000);
			}

			j = USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_cr.w);

			j &= ~USOC_CR_EXTERNAL_RAM_BANK_MASK;

			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_cr.w, j);

			kmem_free(buffer, 0x10000);

			for (i = USOC_N_CQS-1; i >= 0; i--) {
				mutex_exit(&usocp->usoc_request[i].skc_mtx);
				mutex_exit(&usocp->usoc_response[i].skc_mtx);
			}
			break;
		}
#ifdef DEBUG
		case USOCIO_DUMPXRAMBUF:
			(void) copyout((caddr_t)usoc_xrambuf, (caddr_t)arg,
			    0x40000);
			mutex_enter(&usoc_global_lock);
			usoc_core_taken = 0;
			mutex_exit(&usoc_global_lock);
			break;
#endif
		case USOCIO_GETMAP:
			if (usoc_dump_xram_buf((void *)port_statep->sp_board) !=
			    FC_SUCCESS) {
				retval = FC_FAILURE;
				break;
			}
			retval = FC_SUCCESS;
			break;

		case USOCIO_BYPASS_DEV:
			retval = usoc_bypass_dev((void *)port_statep, arg);
			break;

		case USOCIO_FORCE_LIP:
			retval = usoc_force_lip(port_statep, 0);
			break;

		case USOCIO_FORCE_OFFLINE:
			retval = usoc_force_offline(port_statep, 0);
			break;

		case USOCIO_LOOPBACK_INTERNAL:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_INT_LOOP);
			if (copyout((caddr_t)&r, (caddr_t)arg, sizeof (uint_t))
			    == -1)
				return (EFAULT);
			break;

		case USOCIO_LOOPBACK_MANUAL:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_EXT_LOOP);
			if (copyout((caddr_t)&r, (caddr_t)arg, sizeof (uint_t))
			    == -1)
				return (EFAULT);
			break;

		case USOCIO_NO_LOOPBACK:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_REM_LOOP);
			if (copyout((caddr_t)&r, (caddr_t)arg, sizeof (uint_t))
			    == -1)
				return (EFAULT);
			break;

		case USOCIO_DIAG_NOP:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_NOP);
			if (copyout((caddr_t)&r, (caddr_t)arg, sizeof (uint_t))
			    == -1)
				return (EFAULT);
			break;

		case USOCIO_DIAG_XRAM:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_XRAM_TEST);
			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_SOC:
			retval = usoc_diag_request(port_statep, &r,
				USOC_DIAG_SOC_TEST);
			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_HCB:
			retval = usoc_diag_request(port_statep, &r,
			    USOC_DIAG_HCB_TEST);

			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_SOCLB:
			retval = usoc_diag_request(port_statep,
			    &r, USOC_DIAG_SOCLB_TEST);
			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_SRDSLB:
			retval = usoc_diag_request(port_statep, &r,
			    USOC_DIAG_SRDSLB_TEST);

			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_EXTLB:
			retval = usoc_diag_request(port_statep, &r,
			    USOC_DIAG_EXTOE_TEST);
			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_DIAG_RAW:
			if (copyin((caddr_t)arg, (caddr_t)&i,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			retval = usoc_diag_request(port_statep, &r, (uint_t)i);

			if (copyout((caddr_t)&r, (caddr_t)arg,
			    sizeof (uint_t)) == -1) {
				return (EFAULT);
			}
			break;

		case USOCIO_ADD_POOL: {
			struct usoc_add_pool addp;

			if (ddi_copyin((caddr_t)arg, (caddr_t)&addp,
			    sizeof (struct usoc_add_pool), mode)) {
				return (EFAULT);
			}

			addp.pool_fc4type |= FC_TYPE_IOCTL;
			retval = usoc_ub_alloc(port_statep, NULL,
				addp.pool_buf_size, 0, addp.pool_fc4type);

			if (retval != FC_SUCCESS) {
				DEBUGF(7, (CE_WARN,
				    "usoc%d: add pool failed: 0x%x\n",
				    instance, retval));
				return (EIO);
			}
			DEBUGF(7, (CE_NOTE,
			    "usoc%d: pool for FC4 0x%x setup\n",
			    instance, addp.pool_fc4type));
			break;
		}

		case USOCIO_DELETE_POOL: {
			struct usoc_delete_pool delp;
			uint64_t	*dp;
			uint32_t	sz;
			ddi_devstate_t	devstate;

			/* check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}

			if (ddi_copyin((caddr_t)arg, (caddr_t)&delp,
			    sizeof (struct usoc_delete_pool), mode)) {
				return (EFAULT);
			}
			sz = (delp.pool_buf_count * sizeof (uint64_t));
			dp = (uint64_t *)kmem_zalloc(sz, KM_SLEEP);
			if (ddi_copyin((caddr_t)delp.pool_tokens,
			    (caddr_t)dp, sz, mode)) {
				kmem_free(dp, sz);
				return (EFAULT);
			}
			retval = usoc_ub_free(port_statep,
					delp.pool_buf_count, dp);

			kmem_free(dp, sz);
			if (retval != FC_SUCCESS) {
				DEBUGF(7, (CE_WARN,
				    "usoc%d: delete pool failed: 0x%x\n",
				    instance, retval));
				return (EIO);
			}
			DEBUGF(7, (CE_NOTE,
			    "usoc%d: pool for successfully deleted\n",
			    instance));
			break;
		}

		case USOCIO_ADD_BUFFER: {
			struct usoc_add_buffers addbp;
			uint64_t	*ap;
			uint32_t	sz;
			uint64_t *tptr = NULL;
			ddi_devstate_t	devstate;

			/* check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}

			if (ddi_copyin((void *)arg, (void *)&addbp,
			    sizeof (struct usoc_add_buffers), mode)) {
				return (EFAULT);
			}
			tptr = addbp.pool_tokens;
			sz = (addbp.pool_buf_count * sizeof (uint64_t));
			ap = (uint64_t *)kmem_zalloc(sz, KM_SLEEP);
			retval = usoc_add_ubuf(port_statep, addbp.pool_id,
				ap, 0, &addbp.pool_buf_count);

			if (retval != FC_SUCCESS) {
				DEBUGF(7, (CE_WARN,
				    "usoc%d: add buffer failed: 0x%x\n",
				    instance, retval));
				kmem_free(ap, sz);
				return (EIO);
			} else {
				if (ddi_copyout((void *)&addbp, (void *)arg,
				    sizeof (struct usoc_add_buffers), mode)) {
					kmem_free(ap, sz);
					return (EFAULT);
				}
				if (ddi_copyout((void *)ap,
				    (void *)tptr, sz, mode)) {
					kmem_free(ap, sz);
					return (EFAULT);
				}
				DEBUGF(7, (CE_NOTE,
				    "usoc%d 0x%x bufs added to pool 0x%x\n",
				    instance, addbp.pool_buf_count,
				    addbp.pool_id));
			}
			kmem_free(ap, sz);
			break;
		}

		case USOCIO_SEND_FRAME: {
			struct usoc_send_frame	sftp;
			usoc_unsol_buf_t	*ubufp;
			uint32_t		poolid;
			uint32_t		sz;
			ddi_devstate_t	devstate;

			/* check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}

			if (ddi_copyin((void *)arg, (void *)&sftp,
			    sizeof (struct usoc_send_frame), mode)) {
				return (EFAULT);
			}
			poolid = sftp.sft_pool_id;

			mutex_enter(&port_statep->sp_mtx);
			ubufp = port_statep->usoc_unsol_buf;
			while (ubufp != NULL) {
				if (ubufp->pool_id == poolid) {
					/* found */
					break;
				}
				ubufp = ubufp->pool_next;
			}
			mutex_exit(&port_statep->sp_mtx);

			if (ubufp == NULL) {
				/* Not found !! */
				DEBUGF(7, (CE_WARN, "usoc%d: Buf pool with "
				    "poolid 0x%x not found\n",
				    instance, poolid));
				return (EINVAL);
			}
			sz = ubufp->pool_buf_size;

			(void) usoc_data_out(port_statep, &sftp, sz, ubufp);
			break;
		}

		case USOCIO_RCV_FRAME: {
			uint32_t		type;
			uint32_t		poolid;
			struct usoc_rcv_frame 	urcvp;
			usoc_unsol_buf_t	*ubufp;
			caddr_t			rcv_data;
			ddi_devstate_t		devstate;

			/* check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}

			if (ddi_copyin((void *)arg, (void *)&urcvp,
			    sizeof (struct usoc_rcv_frame), mode)) {
				return (EFAULT);
			}
			type = urcvp.rcv_type;
			rcv_data = urcvp.rcv_buf;
			poolid = USOC_GET_POOLID_FROM_TYPE(type);

			mutex_enter(&port_statep->sp_mtx);
			ubufp = port_statep->usoc_unsol_buf;
			while (ubufp != NULL) {
				if (ubufp->pool_id == poolid) {
					/* found */
					break;
				}
				ubufp = ubufp->pool_next;
			}
			mutex_exit(&port_statep->sp_mtx);

			if (ubufp == NULL) {
				/* Not found !! */
				DEBUGF(7, (CE_WARN, "usoc%d: Buf pool with "
				    "poolid 0x%x not found\n",
				    instance, poolid));
				return (EINVAL);
			}
			if (ubufp->pool_buf_size != urcvp.rcv_size) {
				/* Mismatched sizes !! */
				DEBUGF(7, (CE_WARN, "usoc%d buffers size "
				    "mismatch 0x%x: 0x%x \n",
				    instance, ubufp->pool_buf_size,
				    urcvp.rcv_size));
				return (EINVAL);
			}
			mutex_enter(&port_statep->sp_unsol_mtx);
wait:
			port_statep->sp_unsol_buf = (fc_unsol_buf_t *)0;
			while (port_statep->sp_unsol_buf == NULL) {
				cv_wait(&port_statep->sp_unsol_cv,
					&port_statep->sp_unsol_mtx);
			}

			if (port_statep->sp_unsol_buf->ub_frame.type == type) {

				mutex_exit(&port_statep->sp_unsol_mtx);
				if (ddi_copyout((void *)
				    port_statep->sp_unsol_buf->ub_buffer,
				    (void *)rcv_data,
				    urcvp.rcv_size, mode)) {
					DEBUGF(7, (CE_WARN,
					    "usoc%d: unsolicited data copyout "
					    "error: 0x%x\n", instance, retval));
					return (EFAULT);
				}
			} else {
				goto wait;
			}
			return (0);
		}

		case USOCIO_GET_LESB: {
			ddi_devstate_t		devstate;
			/* check if device is marked bad */
			if (USOC_DEVICE_BAD(usocp, devstate)) {
				return (EIO);
			}
			if (usoc_getrls(port_statep, (caddr_t)arg, 0)
			    != FC_SUCCESS) {
				return (EFAULT);
			}
			return (0);
		}

		default:
			return (ENOTTY);
	}

	switch (retval) {
		case FC_SUCCESS:
			return (0);

		case FC_NOMEM:
			return (ENOMEM);

		case USOC_STATUS_DIAG_BUSY:
			return (EALREADY);

		case USOC_STATUS_DIAG_INVALID:
			return (EINVAL);

		default:
			return (EIO);
	}
}


/*
 * Function name : usoc_disable()
 *
 * Return Values :  none
 *
 * Description	 : Reset the soc+
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * Note:  before calling this, the interface should be locked down
 * so that it is guaranteed that no other threads are accessing
 * the hardware.
 */
static	void
usoc_disable(usoc_state_t *usocp)
{
#ifdef	DEBUG
	int i;
#endif
	/* Don't touch the hardware if the registers aren't mapped */
	if (!usocp->usoc_rp)
		return;

	usocp->usoc_k_imr = 0;
	USOC_WR32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_imr, 0);
	USOC_WR32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_csr.w,
	    USOC_CSR_SOFT_RESET);
#ifdef	DEBUG
	i = USOC_RD32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_csr.w);
#endif
	DEBUGF(9, (CE_CONT, "csr.w = %x\n", i));
}


/*
 * static int
 * usoc_cqalloc_init() - Inialize the circular queue tables.
 *	Also, init the locks that are associated with the tables.
 *
 *	Returns:	FC_SUCCESS, if able to init properly.
 *			FC_FAILURE, if unable to init properly.
 */

static int
usoc_cqalloc_init(usoc_state_t *usocp, uint32_t index)
{
	uint32_t cq_size;
	size_t real_len;
	uint_t ccount;
	usoc_kcq_t *cqp;
	int	req_bound = 0, rsp_bound = 0;

	/*
	 * Initialize the Request and Response Queue locks.
	 */

	mutex_init(&usocp->usoc_request[index].skc_mtx, "request.mtx",
		MUTEX_DRIVER, (void *)usocp->usoc_iblkc);
	mutex_init(&usocp->usoc_response[index].skc_mtx, "response.mtx",
		MUTEX_DRIVER, (void *)usocp->usoc_iblkc);
	cv_init(&usocp->usoc_request[index].skc_cv, "request.cv", CV_DRIVER,
		(void *)NULL);
	cv_init(&usocp->usoc_response[index].skc_cv, "response.cv", CV_DRIVER,
		(void *)NULL);

	/* Allocate DVMA resources for the Request Queue. */
	cq_size = usoc_req_entries[index] * sizeof (cqe_t);
	if (cq_size) {
		cqp = &usocp->usoc_request[index];

		if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		    DDI_DMA_DONTWAIT, NULL,
		    &cqp->skc_dhandle) != DDI_SUCCESS) {

			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: failed to"
			    " allocate dma handle");
			goto fail;
		}

		if (ddi_dma_mem_alloc(cqp->skc_dhandle,
		    cq_size + USOC_CQ_ALIGN, &usoc_acc_attr,
		    DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL,
		    (caddr_t *)&cqp->skc_cq_raw, &real_len,
		    &cqp->skc_acchandle) != DDI_SUCCESS) {

			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: failed to"
			    " allocate dma space");
		}

		if (real_len < (cq_size + USOC_CQ_ALIGN)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: failed to"
			    " allocate complete dma space");

			goto fail;
		}

		cqp->skc_cq = (cqe_t *)(((uintptr_t)cqp->skc_cq_raw +
			(uintptr_t)USOC_CQ_ALIGN - 1) &
			((uintptr_t)(~(USOC_CQ_ALIGN-1))));

		if (ddi_dma_addr_bind_handle(cqp->skc_dhandle,
		    (struct as *)NULL, (caddr_t)cqp->skc_cq, cq_size,
		    DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		    NULL, &cqp->skc_dcookie, &ccount) != DDI_DMA_MAPPED) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: bind of dma"
			    " handle failed");

			goto fail;
		}

		req_bound = 1;
		if (ccount != 1) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: bind of dma"
			    " handle returned multiple cookies");
			goto fail;
		}
	} else {
		usocp->usoc_request[index].skc_cq_raw = NULL;
		usocp->usoc_request[index].skc_cq = (cqe_t *)NULL;
		usocp->usoc_request[index].skc_dhandle = 0;
	}

	/* Allocate DVMA resources for the response Queue. */
	cq_size = usoc_rsp_entries[index] * sizeof (cqe_t);
	if (cq_size) {
		cqp = &usocp->usoc_response[index];

		if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		    DDI_DMA_DONTWAIT, NULL, &cqp->skc_dhandle) != DDI_SUCCESS) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: alloc of dma"
			    " handle failed");
			goto fail;
		}

		if (ddi_dma_mem_alloc(cqp->skc_dhandle, cq_size + USOC_CQ_ALIGN,
		    &usoc_acc_attr, DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT, NULL,
		    (caddr_t *)&cqp->skc_cq_raw, &real_len,
		    &cqp->skc_acchandle) != DDI_SUCCESS) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: alloc of dma"
			    " space failed");
			goto fail;
		}

		if (real_len < (cq_size + USOC_CQ_ALIGN)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: alloc of complete"
			    " dma space failed");
			goto fail;
		}

		cqp->skc_cq = (cqe_t *)(((uintptr_t)cqp->skc_cq_raw +
		    (uintptr_t)USOC_CQ_ALIGN - 1) &
		    ((uintptr_t)(~(USOC_CQ_ALIGN-1))));

		if (ddi_dma_addr_bind_handle(cqp->skc_dhandle,
		    (struct as *)NULL, (caddr_t)cqp->skc_cq, cq_size,
		    DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		    NULL, &cqp->skc_dcookie, &ccount) != DDI_DMA_MAPPED) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: bind of dma"
			    " handle failed");
			goto fail;
		}

		rsp_bound = 1;
		if (ccount != 1) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "cq alloc: bind of dma"
			    "handle returned multiple cookies");
			goto fail;
		}
	} else {
		usocp->usoc_response[index].skc_cq_raw = NULL;
		usocp->usoc_response[index].skc_cq = (cqe_t *)NULL;
		usocp->usoc_response[index].skc_dhandle = 0;
	}

	/*
	 * Initialize the queue pointers
	 */
	usoc_cqinit(usocp, index);

	return (FC_SUCCESS);

fail:
	if (usocp->usoc_request[index].skc_dhandle) {
		if (req_bound) {
			(void) ddi_dma_unbind_handle(
			    usocp->usoc_request[index].skc_dhandle);
		}
		ddi_dma_free_handle(&usocp->usoc_request[index].skc_dhandle);
	}

	if (usocp->usoc_request[index].skc_cq_raw) {
		ddi_dma_mem_free(&usocp->usoc_request[index].skc_acchandle);
	}

	if (usocp->usoc_response[index].skc_dhandle) {
		if (rsp_bound) {
			(void) ddi_dma_unbind_handle(
			    usocp->usoc_response[index].skc_dhandle);
		}
		ddi_dma_free_handle(&usocp->usoc_response[index].skc_dhandle);
	}

	if (usocp->usoc_response[index].skc_cq_raw) {
		ddi_dma_mem_free(&usocp->usoc_response[index].skc_acchandle);
	}

	usocp->usoc_request[index].skc_cq_raw = NULL;
	usocp->usoc_request[index].skc_cq = NULL;
	usocp->usoc_response[index].skc_cq_raw = NULL;
	usocp->usoc_response[index].skc_cq = NULL;

	mutex_destroy(&usocp->usoc_request[index].skc_mtx);
	mutex_destroy(&usocp->usoc_response[index].skc_mtx);

	cv_destroy(&usocp->usoc_request[index].skc_cv);
	cv_destroy(&usocp->usoc_response[index].skc_cv);

	return (FC_FAILURE);
}


/*
 * usoc_cqinit() - initializes the driver's circular queue pointers, etc.
 */

static void
usoc_cqinit(usoc_state_t *usocp, uint32_t index)
{
	usoc_kcq_t *kcq_req = &usocp->usoc_request[index];
	usoc_kcq_t *kcq_rsp = &usocp->usoc_response[index];

	/*
	 * Initialize the Request and Response Queue pointers
	 */
	kcq_req->skc_seqno = 1;
	kcq_rsp->skc_seqno = 1;
	kcq_req->skc_in = 0;
	kcq_rsp->skc_in = 0;
	kcq_req->skc_out = 0;
	kcq_rsp->skc_out = 0;
	kcq_req->skc_last_index = usoc_req_entries[index] - 1;
	kcq_rsp->skc_last_index = usoc_rsp_entries[index] - 1;
	kcq_req->skc_full = 0;
	kcq_rsp->deferred_intr_timeoutid = NULL;

	kcq_req->skc_xram_cqdesc = (usocp->usoc_xram_reqp + index);
	kcq_rsp->skc_xram_cqdesc = (usocp->usoc_xram_rspp + index);

	/*  Clear out memory we have allocated */
	if (kcq_req->skc_cq != NULL) {
		uint_t	i;

		for (i = 0; i < (usoc_req_entries[index] * sizeof (cqe_t));
		    i += sizeof (uint32_t)) {
			USOC_WR32(kcq_req->skc_acchandle,
			    (caddr_t)kcq_req->skc_cq + i, 0);
		}
	}

	if (kcq_rsp->skc_cq != NULL) {
		uint_t	i;

		for (i = 0; i < (usoc_rsp_entries[index] * sizeof (cqe_t));
		    i += 4) {
			USOC_WR32(kcq_rsp->skc_acchandle,
			    (caddr_t)kcq_rsp->skc_cq + i, 0);
		}
	}
}


static int
usoc_start(usoc_state_t *usocp)
{
	if (!usocp) {
		return (FC_FAILURE);
	}

	if (usoc_download_ucode(usocp, usoc_fw_len) != FC_SUCCESS) {
		return (FC_FAILURE);
	}

	if (usoc_init_cq_desc(usocp) != FC_SUCCESS) {
		return (FC_FAILURE);
	}

	if (usoc_init_wwn(usocp) != FC_SUCCESS) {
		return (FC_FAILURE);
	}

	if (usocp->usoc_throttle < USOC_THROTTLE_THRESHOLD) {
		usocp->usoc_throttle = USOC_MAX_THROTTLE;
	}
	usocp->usoc_spurious_sol_intrs = 0;
	usocp->usoc_spurious_unsol_intrs = 0;

	mutex_enter(&usocp->usoc_k_imr_mtx);
	usocp->usoc_shutdown = 0;
	mutex_exit(&usocp->usoc_k_imr_mtx);

	usocp->usoc_port_state[0].sp_status &= ~(PORT_CHILD_INIT);
	usocp->usoc_port_state[1].sp_status &= ~(PORT_CHILD_INIT);
	usocp->usoc_port_state[0].sp_status |= PORT_OFFLINE;
	usocp->usoc_port_state[1].sp_status |= PORT_OFFLINE;

	if (usoc_enable(usocp) != FC_SUCCESS) {
		return (FC_FAILURE);
	}

	if (usoc_establish_pool(usocp, 1) != FC_SUCCESS) {
		/* disable usoc, since there is a failure after usoc_enable */
		usoc_disable(usocp);
		return (FC_FAILURE);
	}

	if (usoc_add_pool_buffer(usocp, 1) != FC_SUCCESS) {
		/* disable usoc, since there is a failure after usoc_enable */
		usoc_disable(usocp);
		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}

static void
usoc_doreset(usoc_state_t *usocp)
{
	int		i;
	usoc_port_t	*port_statep;

	for (i = 0; i < USOC_N_CQS; i++) {
		mutex_enter(&usocp->usoc_request[i].skc_mtx);
		mutex_enter(&usocp->usoc_response[i].skc_mtx);
	}

	mutex_enter(&usocp->usoc_k_imr_mtx);
	usoc_disable(usocp);

	for (i = 0; i < N_USOC_NPORTS; i++) {
		port_statep = &usocp->usoc_port_state[i];

		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_status &= ~(PORT_STATUS_MASK |
		    PORT_LILP_PENDING | PORT_LIP_PENDING |
		    PORT_ABORT_PENDING | PORT_BYPASS_PENDING |
		    PORT_ELS_PENDING | PORT_OUTBOUND_PENDING | PORT_ONLINE);
		port_statep->sp_status |= PORT_OFFLINE;
		mutex_exit(&port_statep->sp_mtx);
	}

	for (i = 0; i < USOC_N_CQS; i++) {
		usoc_cqinit(usocp, i);
	}

	mutex_exit(&usocp->usoc_k_imr_mtx);

	for (i = USOC_N_CQS-1; i >= 0; i--) {
		mutex_exit(&usocp->usoc_response[i].skc_mtx);
		mutex_exit(&usocp->usoc_request[i].skc_mtx);
	}

	/* flush all the commands pending with the driver */
	usoc_flush_all(usocp);

	for (i = 0; i < N_USOC_NPORTS; i++) {
		ddi_devstate_t devstate;

		if (!USOC_DEVICE_BAD(usocp, devstate)) {
			if (usocp->usoc_port_state[i].sp_statec_callb) {
				usocp->usoc_port_state[i].sp_statec_callb(
				    usocp->usoc_port_state[i].sp_tran_handle,
				    FC_STATE_RESET);
			}
		}
	}
}


/*
 * Function name : usoc_download_ucode ()
 *
 * Return Values :
 *
 * Description	 : Copies firmware from code that has been linked into
 *		   the usoc module into the soc+'s XRAM.  Prints the date
 *		   string
 *
 */
static int
usoc_download_ucode(usoc_state_t *usocp, uint_t fw_len)
{
	uint32_t date_str[16];

	/* Copy the firmware image */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle, &usoc_ucode,
	    usocp->usoc_xrp, fw_len);

	/* Check if access handle fault occured */
	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "download ucode.0 access handle fault");
		return (FC_FAILURE);
	}

	/* Get the date string from the firmware image */
	USOC_REP_RD32(usocp->usoc_xrp_acchandle, date_str,
	    usocp->usoc_xrp + USOC_XRAM_FW_DATE_STR, sizeof (date_str));

	date_str[sizeof (date_str) / sizeof (uint_t) - 1] = 0;

	/* Check if access handle fault occured */
	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "download microcode-1 access handle fault");
		return (FC_FAILURE);
	}

	if (*(caddr_t)date_str != '\0') {
		usoc_display(usocp, USOC_PORT_ANY, CE_NOTE, USOC_LOG_ONLY,
		    NULL, "host adapter fw date code: %s", (caddr_t)date_str);
		(void) strcpy(usocp->usoc_stats.fw_revision, (char *)date_str);
	} else {
		usoc_display(usocp, USOC_PORT_ANY, CE_NOTE, USOC_LOG_ONLY,
		    NULL, "host adapter fw date code: <not available>");
		(void) strcpy(usocp->usoc_stats.fw_revision, "<Not Available>");
	}

	return (FC_SUCCESS);
}


/*
 * Function name : usoc_init_cq_desc()
 *
 * Return Values : none
 *
 * Description	 : Initializes the request and response queue
 *		   descriptors in the SOC+'s XRAM
 *
 * Context	 : Should only be called during initialiation when
 *		   the SOC+ is reset.
 */
static int
usoc_init_cq_desc(usoc_state_t *usocp)
{
	usoc_cq_t	que_desc[USOC_N_CQS];
	uint32_t	i;

	/*
	 * Finish CQ table initialization and give the descriptor
	 * table to the soc+.  Note that we don't use all of the queues
	 * provided by the hardware, but we make sure we initialize the
	 * quantities in the unused fields in the hardware to zeroes.
	 */

	/*
	 * Do request queues
	 */
	for (i = 0; i < USOC_N_CQS; i++) {
	    if (usoc_req_entries[i]) {
		que_desc[i].cq_address =
		    (uint32_t)usocp->usoc_request[i].skc_dcookie.dmac_address;
		que_desc[i].cq_last_index = usoc_req_entries[i] - 1;
	    } else {
		que_desc[i].cq_address = (uint32_t)0;
		que_desc[i].cq_last_index = 0;
	    }
	    que_desc[i].cq_in = 0;
	    que_desc[i].cq_out = 0;
	    que_desc[i].cq_seqno = 1; /* required by SOC+ microcode */
	}

	/* copy to XRAM */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle,	/* access handle */
	    que_desc,				/* pointer to kernel copy */
	    usocp->usoc_xram_reqp,		/* pointer to xram location */
	    (USOC_N_CQS * sizeof (usoc_cq_t)));

	/* Check if access handle fault occured */
	if (USOC_DDH_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle,
		USOC_DDH_XRP_INITCQ_ACCHDL_FAULT)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "init_cq_desc-0 access handle fault");
		return (FC_FAILURE);
	}

	/*
	 * Do response queues
	 */
	for (i = 0; i < USOC_N_CQS; i++) {
		if (usoc_rsp_entries[i]) {
		    que_desc[i].cq_last_index = usoc_rsp_entries[i] - 1;
		    que_desc[i].cq_address =
			(uint32_t)usocp->usoc_response[i].
				skc_dcookie.dmac_address;

		} else {
		    que_desc[i].cq_address = 0;
		    que_desc[i].cq_last_index = 0;
		}
	}

	/* copy to XRAM */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle,	/* access handle */
	    que_desc,				/* pointer to kernel copy */
	    usocp->usoc_xram_rspp,		/* pointer to xram location */
	    (USOC_N_CQS * sizeof (usoc_cq_t)));

	/* Check if access handle fault occured */
	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "init_cq_desc-1 access handle fault");

		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}


static int
usoc_init_wwn(usoc_state_t *usocp)
{
	/* copy the node wwn to xram */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle, &usocp->usoc_n_wwn,
	    (usocp->usoc_xrp + USOC_XRAM_NODE_WWN), sizeof (la_wwn_t));

	/* copy port a's wwn to xram */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle,
	    &usocp->usoc_port_state[0].sp_p_wwn,
	    (usocp->usoc_xrp + USOC_XRAM_PORTA_WWN), sizeof (la_wwn_t));

	/* copy port b's wwn to xram */
	USOC_REP_WR32(usocp->usoc_xrp_acchandle,
	    &usocp->usoc_port_state[1].sp_p_wwn,
	    (usocp->usoc_xrp + USOC_XRAM_PORTB_WWN), sizeof (la_wwn_t));

	/*
	 * need to avoid deadlock by assuring no other thread grabs both of
	 * these at once
	 */
	mutex_enter(&usocp->usoc_port_state[0].sp_mtx);
	mutex_enter(&usocp->usoc_port_state[1].sp_mtx);
	USOC_REP_RD32(usocp->usoc_xrp_acchandle,
	    usocp->usoc_service_params,
	    (usocp->usoc_xrp + USOC_XRAM_SERV_PARAMS), USOC_SVC_LENGTH);
	mutex_exit(&usocp->usoc_port_state[1].sp_mtx);
	mutex_exit(&usocp->usoc_port_state[0].sp_mtx);

	/*
	 * check for access handle fault. If a fault occured, then
	 * subsequent reads/writes would have done nothing.
	 */
	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "WWN init access handle fault");

		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}


static int
usoc_enable(usoc_state_t *usocp)
{
	DEBUGF(2, (CE_CONT, "usoc%d: enable:\n", usocp->usoc_instance));

	USOC_WR32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_cr.w,
	    usocp->usoc_cfg);

	USOC_WR32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_csr.w,
	    USOC_CSR_USOC_TO_HOST);

	usocp->usoc_k_imr = (uint32_t)USOC_CSR_USOC_TO_HOST |
	    USOC_CSR_SLV_ACC_ERR;

	USOC_WR32(usocp->usoc_rp_acchandle, &usocp->usoc_rp->usoc_imr,
	    usocp->usoc_k_imr);

	/*
	 * Check for access handle fault. If fault was detected, then
	 * subsequent writes using access handles would not have done
	 * anything.
	 */
	if (USOC_DDH_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle,
	    USOC_DDH_ENABLE_ACCHDL_FAULT)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "usoc_enable: access handle fault");

		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}


/*
 * static int
 * usoc_establish_pool() - this routine tells the SOC+ of a buffer pool
 *	to place LINK ctl application data as it arrives.
 *
 *	Returns:
 *		FC_SUCCESS, upon establishing the pool.
 *		FC_FAILURE, if unable to establish the pool.
 */

static int
usoc_establish_pool(usoc_state_t *usocp, uint32_t poolid)
{
	usoc_pool_request_t	*prq;
	int			result;

	if ((prq = (usoc_pool_request_t *)kmem_zalloc(
	    sizeof (usoc_pool_request_t), KM_NOSLEEP)) == NULL) {
			return (FC_FAILURE);
	}
	/*
	 * Fill in the request structure.
	 */
	prq->spr_usoc_hdr.sh_request_token = 1;
	prq->spr_usoc_hdr.sh_flags = USOC_FC_HEADER | USOC_UNSOLICITED |
		USOC_NO_RESPONSE;
	prq->spr_usoc_hdr.sh_class = 0;
	prq->spr_usoc_hdr.sh_seg_cnt = 1;
	prq->spr_usoc_hdr.sh_byte_cnt = 0;

	prq->spr_pool_id = poolid;
	prq->spr_header_mask = USOCPR_MASK_RCTL;
	prq->spr_buf_size = USOC_POOL_SIZE;
	prq->spr_n_entries = 0;			/* as per SOC+ spec */

	prq->spr_fc_frame_hdr.r_ctl = R_CTL_ELS_REQ;
	prq->spr_fc_frame_hdr.d_id = 0;
	prq->spr_fc_frame_hdr.s_id = 0;
	prq->spr_fc_frame_hdr.type = 0;
	prq->spr_fc_frame_hdr.f_ctl = 0;
	prq->spr_fc_frame_hdr.seq_id = 0;
	prq->spr_fc_frame_hdr.df_ctl = 0;
	prq->spr_fc_frame_hdr.seq_cnt = 0;
	prq->spr_fc_frame_hdr.ox_id = 0;
	prq->spr_fc_frame_hdr.rx_id = 0;
	prq->spr_fc_frame_hdr.ro = 0;

	prq->spr_cqhdr.cq_hdr_count = 1;
	prq->spr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_POOL;
	prq->spr_cqhdr.cq_hdr_flags = 0;
	prq->spr_cqhdr.cq_hdr_seqno = 0;

	/* Enque the request. */
	result = usoc_cq_enque(usocp, NULL, (cqe_t *)prq, CQ_REQUEST_0,
	    NULL, NULL, 0);

	kmem_free((void *)prq, sizeof (usoc_pool_request_t));

	if (result == FC_SUCCESS) {
		mutex_enter(&usocp->usoc_board_mtx);
		usocp->usoc_ncmds--;
		mutex_exit(&usocp->usoc_board_mtx);
	} else {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "usoc_establish_pool: usoc_cq_enque failed");
	}

	return (result);

}


/*
 * static int
 * usoc_add_pool_buffer() - this routine tells the SOC+ to add one buffer
 *	to an established pool of buffers
 *
 *	Returns:
 *		DDI_SUCCESS, upon establishing the pool.
 *		DDI_FAILURE, if unable to establish the pool.
 */
static int
usoc_add_pool_buffer(usoc_state_t *usocp, uint32_t poolid)
{
	usoc_data_request_t	*drq;
	int			result;
	size_t			real_len;
	int			bound = 0;
	uint_t			ccount;

	if ((drq = (usoc_data_request_t *)
	    kmem_zalloc(sizeof (usoc_data_request_t), KM_NOSLEEP)) == NULL) {
		return (FC_FAILURE);
	}

	/* Allocate DVMA resources for the buffer pool */
	if (usocp->usoc_pool_dhandle == NULL) {
		if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		    DDI_DMA_DONTWAIT, NULL, &usocp->usoc_pool_dhandle) !=
		    DDI_SUCCESS) {
			goto fail;
		}
	}

	if (usocp->usoc_pool == NULL) {
		if (ddi_dma_mem_alloc(usocp->usoc_pool_dhandle,
		    USOC_POOL_SIZE, &usoc_acc_attr, DDI_DMA_CONSISTENT,
		    DDI_DMA_DONTWAIT, NULL, (caddr_t *)&usocp->usoc_pool,
		    &real_len, &usocp->usoc_pool_acchandle) != DDI_SUCCESS) {
			goto fail;
		}

		if (real_len < USOC_POOL_SIZE) {
			goto fail;
		}

		if (ddi_dma_addr_bind_handle(usocp->usoc_pool_dhandle,
		    (struct as *)NULL, (caddr_t)usocp->usoc_pool, real_len,
		    DDI_DMA_READ | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		    NULL, &usocp->usoc_pool_dcookie, &ccount) !=
		    DDI_DMA_MAPPED) {
			goto fail;
		}
		bound = 1;

		if (ccount != 1) {
			goto fail;
		}
	} else {
		bound = 1;
	}

	/*
	 * Fill in the request structure.
	 */
	drq->sdr_usoc_hdr.sh_request_token = poolid;
	drq->sdr_usoc_hdr.sh_flags = USOC_UNSOLICITED | USOC_NO_RESPONSE;
	drq->sdr_usoc_hdr.sh_class = 3;
	drq->sdr_usoc_hdr.sh_seg_cnt = 1;
	drq->sdr_usoc_hdr.sh_byte_cnt = 0;

	drq->sdr_dataseg[0].fc_base =
	    (uint32_t)usocp->usoc_pool_dcookie.dmac_address;
	drq->sdr_dataseg[0].fc_count = USOC_POOL_SIZE;
	drq->sdr_dataseg[1].fc_base = 0;
	drq->sdr_dataseg[1].fc_count = 0;
	drq->sdr_dataseg[2].fc_base = 0;
	drq->sdr_dataseg[2].fc_count = 0;
	drq->sdr_dataseg[3].fc_base = 0;
	drq->sdr_dataseg[3].fc_count = 0;
	drq->sdr_dataseg[4].fc_base = 0;
	drq->sdr_dataseg[4].fc_count = 0;
	drq->sdr_dataseg[5].fc_base = 0;
	drq->sdr_dataseg[5].fc_count = 0;

	drq->sdr_cqhdr.cq_hdr_count = 1;
	drq->sdr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_BUFFER;
	drq->sdr_cqhdr.cq_hdr_flags = 0;
	drq->sdr_cqhdr.cq_hdr_seqno = 0;

	/* Transport the request. */
	result = usoc_cq_enque(usocp, NULL, (cqe_t *)drq, CQ_REQUEST_0,
	    NULL, NULL, 0);

	kmem_free((void *)drq, sizeof (usoc_data_request_t));

	if (result == FC_SUCCESS) {
		mutex_enter(&usocp->usoc_board_mtx);
		usocp->usoc_ncmds--;
		mutex_exit(&usocp->usoc_board_mtx);
	} else {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "usoc_add_pool_buffer: usoc_cq_enque failed");
	}

	return (result);

fail:
	usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
	    "usoc_add_pool_buffer: Buffer pool DVMA alloc failed");

	if (usocp->usoc_pool_dhandle) {
		if (bound) {
			(void) ddi_dma_unbind_handle(usocp->usoc_pool_dhandle);
		}
		ddi_dma_free_handle(&usocp->usoc_pool_dhandle);
	}

	if (usocp->usoc_pool) {
		ddi_dma_mem_free(&usocp->usoc_pool_acchandle);
	}

	usocp->usoc_pool = NULL;
	usocp->usoc_pool_dhandle = NULL;

	return (FC_FAILURE);
}


static int
usoc_transport(opaque_t portp, fc_packet_t *pkt)
{
	usoc_port_t	*port_statep = (usoc_port_t *)portp;
	usoc_state_t	*usocp = port_statep->sp_board;
	int		internal;
	int		retval;
	int		intr_mode, req_q_no;
	usoc_pkt_priv_t	*priv =
		(usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_request_t	*sp;
	ddi_devstate_t	devstate;

	if (USOC_DEVICE_BAD(usocp, devstate)) {
		pkt->pkt_state = FC_PKT_PORT_OFFLINE;
		return (FC_TRANSPORT_ERROR);
	}

	if (!(priv->spp_flags & PACKET_INTERNAL_PACKET) &&
	    !(port_statep->sp_status & PORT_BOUND)) {
		return (FC_UNBOUND);
	}

	if ((priv == NULL) || (priv->spp_portp != port_statep) ||
		!(priv->spp_flags & PACKET_VALID))
		return (FC_BADPACKET);

	sp = &priv->spp_sr;

	if (!(priv->spp_flags & PACKET_DORMANT)) {
		pkt->pkt_state = FC_PKT_LOCAL_RJT;
		pkt->pkt_reason = FC_REASON_PKT_BUSY;
		return (FC_TRANSPORT_ERROR);
	}

	if (!(priv->spp_flags & PACKET_INTERNAL_PACKET) &&
	    (port_statep->sp_status & PORT_OFFLINE)) {
		pkt->pkt_state = FC_PKT_PORT_OFFLINE;
		return (FC_OFFLINE);
	}

	/*
	 * Assume that any internal packet has already set up the
	 * usoc_request structure
	 */
	mutex_enter(&priv->spp_mtx);
	priv->spp_flags &= ~PACKET_NO_CALLBACK;
	internal = (priv->spp_flags & PACKET_INTERNAL_PACKET) ? 1 : 0;
	if (internal == 0) {
		sp->sr_usoc_hdr.sh_flags = USOC_FC_HEADER | USOC_RESP_HEADER;
		if (port_statep->sp_port == 1) {
			sp->sr_usoc_hdr.sh_flags |= USOC_PORT_B;
		}

		sp->sr_cqhdr.cq_hdr_flags = 0;
		sp->sr_cqhdr.cq_hdr_seqno = 0;

		switch (FC_TRAN_CLASS(pkt->pkt_tran_flags)) {
		case FC_TRAN_CLASS1:
			sp->sr_usoc_hdr.sh_class = 1;
			break;

		case FC_TRAN_CLASS2:
			sp->sr_usoc_hdr.sh_class = 2;
			break;

		case FC_TRAN_CLASS3:
			/* FALLTHROUGH */
		default:
			sp->sr_usoc_hdr.sh_class = 3;
			break;
		}

		switch (pkt->pkt_tran_type) {
		case FC_PKT_FCP_READ:
		case FC_PKT_FCP_WRITE:
			sp->sr_usoc_hdr.sh_byte_cnt = pkt->pkt_datalen;
			sp->sr_dataseg[0].fc_base =
				(uint32_t)pkt->pkt_cmd_cookie.dmac_address;
			sp->sr_dataseg[0].fc_count = pkt->pkt_cmdlen;
			sp->sr_dataseg[1].fc_base =
				(uint32_t)pkt->pkt_resp_cookie.dmac_address;
			sp->sr_dataseg[1].fc_count = pkt->pkt_rsplen;
			sp->sr_dataseg[2].fc_base =
				(uint32_t)pkt->pkt_data_cookie.dmac_address;
			sp->sr_dataseg[2].fc_count = pkt->pkt_datalen;
			sp->sr_usoc_hdr.sh_seg_cnt = 3;
			sp->sr_cqhdr.cq_hdr_count = 1;
			if (pkt->pkt_tran_type == FC_PKT_FCP_READ) {
				sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_IO_READ;
			} else {
				sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_IO_WRITE;
			}
			break;

		case FC_PKT_EXCHANGE:
			sp->sr_usoc_hdr.sh_byte_cnt = pkt->pkt_cmdlen;
			sp->sr_dataseg[0].fc_base =
				(uint32_t)pkt->pkt_cmd_cookie.dmac_address;
			sp->sr_dataseg[0].fc_count = pkt->pkt_cmdlen;
			sp->sr_dataseg[1].fc_base =
				(uint32_t)pkt->pkt_resp_cookie.dmac_address;
			sp->sr_dataseg[1].fc_count = pkt->pkt_rsplen;
			sp->sr_dataseg[2].fc_base = 0;
			sp->sr_dataseg[2].fc_count = 0;
			sp->sr_usoc_hdr.sh_seg_cnt = 2;
			sp->sr_cqhdr.cq_hdr_count = 1;
			sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_SIMPLE;
			break;

		case FC_PKT_INBOUND:
			sp->sr_usoc_hdr.sh_byte_cnt = pkt->pkt_rsplen;
			sp->sr_dataseg[0].fc_base =
				(uint32_t)pkt->pkt_resp_cookie.dmac_address;
			sp->sr_dataseg[0].fc_count = pkt->pkt_rsplen;
			sp->sr_dataseg[1].fc_base = 0;
			sp->sr_dataseg[1].fc_count = 0;
			sp->sr_dataseg[2].fc_base = 0;
			sp->sr_dataseg[2].fc_count = 0;
			sp->sr_usoc_hdr.sh_seg_cnt = 1;
			sp->sr_cqhdr.cq_hdr_count = 1;
			sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_INBOUND;
			break;

		case FC_PKT_OUTBOUND:
			sp->sr_usoc_hdr.sh_byte_cnt = pkt->pkt_cmdlen;
			sp->sr_dataseg[0].fc_base =
				(uint32_t)pkt->pkt_cmd_cookie.dmac_address;
			sp->sr_dataseg[0].fc_count = pkt->pkt_cmdlen;
			sp->sr_dataseg[1].fc_base = 0;
			sp->sr_dataseg[1].fc_count = 0;
			sp->sr_dataseg[2].fc_base = 0;
			sp->sr_dataseg[2].fc_count = 0;
			sp->sr_usoc_hdr.sh_seg_cnt = 1;
			sp->sr_cqhdr.cq_hdr_count = 1;
			sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_OUTBOUND;
			break;

		case FC_PKT_NOP:
			sp->sr_usoc_hdr.sh_byte_cnt = 0;
			sp->sr_dataseg[0].fc_base = 0;
			sp->sr_dataseg[0].fc_count = 0;
			sp->sr_dataseg[1].fc_base = 0;
			sp->sr_dataseg[1].fc_count = 0;
			sp->sr_dataseg[2].fc_base = 0;
			sp->sr_dataseg[2].fc_count = 0;
			sp->sr_usoc_hdr.sh_seg_cnt = 0;
			sp->sr_cqhdr.cq_hdr_count = 1;
			sp->sr_cqhdr.cq_hdr_type = CQ_TYPE_NOP;
			break;

		default:
			pkt->pkt_state = FC_PKT_LOCAL_RJT;
			pkt->pkt_reason = FC_REASON_UNSUPPORTED;
			return (FC_TRANSPORT_ERROR);
		}

		usoc_wcopy((uint_t *)&pkt->pkt_cmd_fhdr,
		    (uint_t *)&sp->sr_fc_frame_hdr, sizeof (fc_frame_hdr_t));

		req_q_no = (pkt->pkt_tran_flags & FC_TRAN_HI_PRIORITY) ?
		    CQ_REQUEST_1 : CQ_REQUEST_2;
	} else {
		req_q_no = CQ_REQUEST_0;
	}

	priv->spp_flags &= ~PACKET_DORMANT;
	priv->spp_flags |= PACKET_IN_TRANSPORT;
	mutex_exit(&priv->spp_mtx);

	intr_mode = (pkt->pkt_tran_flags & FC_TRAN_NO_INTR) ? 0 : 1;

	pkt->pkt_state = FC_PKT_SUCCESS;
	pkt->pkt_resp_resid = 0;
	pkt->pkt_data_resid = 0;

	DEBUGF(4, (CE_CONT, "usoc%d: transport: packet=%p intr_mode=%d\n",
	    usocp->usoc_instance, (void *)pkt, intr_mode));

	/* start timer, if it is not a polled command */
	if (intr_mode) {
		usoc_timeout(usocp, usoc_pkt_timeout, pkt);
	}

	if (pkt->pkt_cmdlen) {
		USOC_SYNC_FOR_DEV(pkt->pkt_cmd_dma, 0, 0);
	}

	if (pkt->pkt_datalen && pkt->pkt_tran_type == FC_PKT_FCP_WRITE) {
		USOC_SYNC_FOR_DEV(pkt->pkt_data_dma, 0, 0);
	}

	if ((retval = usoc_cq_enque(usocp, port_statep, (cqe_t *)sp, req_q_no,
	    pkt, pkt, 0)) != FC_SUCCESS) {
		ASSERT(retval == USOC_UNAVAIL);

		pkt->pkt_state = FC_PKT_PORT_OFFLINE;

		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~PACKET_IN_TRANSPORT;
		priv->spp_flags |= PACKET_DORMANT;
		mutex_exit(&priv->spp_mtx);

		if (intr_mode) {
			usoc_untimeout(usocp, priv);
		}

		return (FC_TRANSPORT_ERROR);
	}

	retval = FC_SUCCESS;

	if (!intr_mode) {
		retval = usoc_finish_polled_cmd(usocp, pkt);
	}

	return (retval);
}


/*
 * Function name : usoc_cq_enque()
 *
 * Return Values :
 *		FC_SUCCESS, if able to que the entry.
 *		USOC_QFULL, if queue full & sleep not set
 *		USOC_UNAVAIL if this port down
 *
 * Description	 : Enqueues an entry into the solicited request
 *		   queue
 *
 * Context	:
 */

/*ARGSUSED*/
static int
usoc_cq_enque(usoc_state_t *usocp, usoc_port_t *port_statep, cqe_t *cqe,
    int rqix, fc_packet_t *pkt, fc_packet_t *token, int mtxheld)
{
	usoc_kcq_t	*kcq;
	cqe_t		*sp;
	uint_t		bitmask, wmask;
	uchar_t		out, s_out;
	usoc_pkt_priv_t	*priv;
	int		internal;
	ddi_devstate_t	devstate;

	kcq = &usocp->usoc_request[rqix];

	bitmask = USOC_CSR_1ST_H_TO_S << rqix;
	wmask = USOC_CSR_USOC_TO_HOST | bitmask;

	if (usocp->usoc_shutdown) {
		return (USOC_UNAVAIL);
	}

	/*
	 * We check for usoc_state before adding to the request cq, so
	 * there is no need to check here
	 */

	if (pkt) {
		priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;

		mutex_enter(&priv->spp_mtx);
		internal = (priv->spp_flags & PACKET_INTERNAL_PACKET) ? 1 : 0;
		ASSERT((priv->spp_flags & PACKET_IN_OVERFLOW_Q) == 0);
		mutex_exit(&priv->spp_mtx);
	} else {
		internal = 0;
	}

	/*
	 * Grab lock for request queue.
	 */
	if (!mtxheld) {
		mutex_enter(&kcq->skc_mtx);
	}

	/*
	 * If the request CQ is full then put the request on the overflow
	 * queue; The QFULL condition is typically cleared when a REQUEST
	 * QUEUE slot is freed and the host is interrupted by the firmware.
	 */
	do {
		mutex_enter(&usocp->usoc_board_mtx);
		if ((!internal && (usocp->usoc_ncmds >=
		    usocp->usoc_throttle)) || kcq->skc_full) {
			mutex_exit(&usocp->usoc_board_mtx);
			/*
			 * If soc's queue full, then wait for an interrupt
			 * when a slot is freed; Put the packet in the
			 * overflow queue for deferred submission
			 */
			if (pkt) {
				DEBUGF(4, (CE_WARN, "usoc%d: transport: add "
				    "overflow pkt %p qfull %x ncmd %x "
				    "throttle %x", usocp->usoc_instance,
				    (void *)pkt, kcq->skc_full,
				    usocp->usoc_ncmds, usocp->usoc_throttle));
				usoc_add_overflow_Q(kcq, priv);
				usocp->usoc_stats.qfulls++;

				if (!mtxheld) {
					mutex_exit(&kcq->skc_mtx);
				}

				if (kcq->skc_full) {
					/*
					 * Under heavy load, it was observed
					 * that the firmware forgot to
					 * interrupt when a request queue
					 * slot was emptied - So keep
					 * reminding it
					 */
					mutex_enter(&usocp->usoc_k_imr_mtx);
					usocp->usoc_k_imr |= bitmask;
					USOC_WR32(usocp->usoc_rp_acchandle,
					    &usocp->usoc_rp->usoc_imr,
					    usocp->usoc_k_imr);
					mutex_exit(&usocp->usoc_k_imr_mtx);

					/* Check for reg access handle fault */
					if (USOC_ACCHDL_FAULT(usocp,
					    usocp->usoc_rp_acchandle)) {

						usoc_display(usocp,
						    USOC_PORT_ANY, CE_WARN,
						    USOC_LOG_ONLY, NULL,
						    "cq_enque.0 register"
						    " access handle fault");
					}
				}
				return (FC_SUCCESS);
			}

			if (!mtxheld) {
				mutex_exit(&kcq->skc_mtx);
			}

			return (USOC_QFULL);
		}
		mutex_exit(&usocp->usoc_board_mtx);

		if (((kcq->skc_in + 1) & kcq->skc_last_index) ==
		    (out = kcq->skc_out)) {
			/*
			 * get SOC+'s copy of out to update our copy of out
			 */
			s_out = USOC_REQUESTQ_INDEX(rqix,
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_reqp.w));

			/* check for access handle fault */
			if (USOC_DDH_ACCHDL_FAULT(usocp,
			    usocp->usoc_rp_acchandle,
			    USOC_DDH_ENQUE1_ACCHDL_FAULT)) {

				usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
				    USOC_LOG_ONLY, NULL,
				    "cq_enque.1 access handle fault");

				/* set skc_full to zero, to end the loop */
				kcq->skc_full = 0;
			} else {
				kcq->skc_out = out = s_out;

				/* if soc+'s que still full set flag */
				kcq->skc_full = ((((kcq->skc_in + 1) &
				    kcq->skc_last_index) == out)) ?
				    USOC_SKC_FULL : 0;
			}
		}

	} while (kcq->skc_full);


	/* if device is marked bad, then return failure */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		if (!mtxheld) {
			mutex_exit(&kcq->skc_mtx);
		}

		DEBUGF(4, (CE_WARN, "usoc%d: usoc_cq_enque.1 "
		    "devstate %s(%d) pkt %p", usocp->usoc_instance,
		    USOC_DEVICE_STATE(devstate), devstate, (void *)pkt));

		return (USOC_UNAVAIL);
	}

	/* Allocate 32 bit ID for the cq entry */
	if (token) {
		uint32_t	id;

		if ((id = usoc_alloc_id(usocp, token)) == USOC_INVALID_ID) {
			if (!mtxheld) {
				mutex_exit(&kcq->skc_mtx);
			}

			usoc_display(usocp, USOC_PORT_ANY, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "cq_enque: out of free"
			    " IDs for cq_entry");

			return (USOC_UNAVAIL);
		}
		DEBUGF(3, (CE_CONT, "usoc%d cq_enque: alloc id %x\n",
		    usocp->usoc_instance, id));

		((usoc_request_t *)cqe)->sr_usoc_hdr.sh_request_token = id;
	} else {
		mutex_enter(&usocp->usoc_board_mtx);
		usocp->usoc_ncmds++;
		mutex_exit(&usocp->usoc_board_mtx);
	}

	sp = &(kcq->skc_cq[kcq->skc_in]);
	cqe->cqe_hdr.cq_hdr_seqno = kcq->skc_seqno;

	/*
	 * Give the entry to the USOC.
	 * prepared cqe entry needs to be copied to request queue.
	 * Use ddi_rep_put32() instead of using simple bcopy. It
	 * won't cause much overhead.
	 */
	USOC_REP_WR32(kcq->skc_acchandle, cqe, sp, sizeof (cqe_t));
	USOC_SYNC_FOR_DEV(kcq->skc_dhandle, (int)((caddr_t)sp -
	    (caddr_t)kcq->skc_cq), sizeof (cqe_t));

	/* Check for access-handle/dma-handle failures */
	if (USOC_DDH_ACCHDL_FAULT(usocp, kcq->skc_acchandle,
	    USOC_DDH_ENQUE2_ACCHDL_FAULT)) {
		/* Let lock go for request queue. */
		if (!mtxheld) {
			mutex_exit(&kcq->skc_mtx);
		}

		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "cq_enque.0 pkt: %p kernel CQ access handle fault",
		    (void *)pkt);

		if (token) {
			usoc_free_id_for_pkt(usocp, token);
		} else {
			mutex_enter(&usocp->usoc_board_mtx);
			usocp->usoc_ncmds--;
			mutex_exit(&usocp->usoc_board_mtx);
		}

		return (USOC_UNAVAIL);
	}

	if (USOC_DMAHDL_FAULT(usocp, kcq->skc_dhandle)) {
		/* Let lock go for request queue. */
		if (!mtxheld) {
			mutex_exit(&kcq->skc_mtx);
		}

		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "cq_enque.0 pkt %p kernel CQ DMA handle fault",
		    (void *)pkt);

		if (token) {
			usoc_free_id_for_pkt(usocp, token);
		} else {
			mutex_enter(&usocp->usoc_board_mtx);
			usocp->usoc_ncmds--;
			mutex_exit(&usocp->usoc_board_mtx);
		}
		return (USOC_UNAVAIL);
	}

	/*
	 * Mark the packet as PACKET_IN_USOC. This enables the timeout
	 * processing for the packet.
	 */
	if (token) {
		priv = (usoc_pkt_priv_t *)token->pkt_fca_private;

		mutex_enter(&priv->spp_mtx);
		priv->spp_flags |= PACKET_IN_USOC;
		mutex_exit(&priv->spp_mtx);
	}

	/*
	 * Update request queue index
	 */
	kcq->skc_in++;
	if ((kcq->skc_in & kcq->skc_last_index) == 0) {
		kcq->skc_in = 0;
		kcq->skc_seqno++;
	}

	/*
	 * Ring SOC+'s doorbell to process this Queue.
	 */
	USOC_WR32(usocp->usoc_rp_acchandle,
	    &usocp->usoc_rp->usoc_csr.w, wmask | (kcq->skc_in << 24));

	/* Let lock go for request queue. */
	if (!mtxheld) {
		mutex_exit(&kcq->skc_mtx);
	}

	return (FC_SUCCESS);
}


void
usoc_pkt_timeout(fc_packet_t *pkt)
{
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_state_t	*usocp;
	int	intr_mode = (pkt->pkt_tran_flags & FC_TRAN_NO_INTR) ? 0 : 1;
	int	sent;

	mutex_enter(&priv->spp_mtx);
	ASSERT(priv->spp_flags & PACKET_IN_TRANSPORT);
	priv->spp_flags &= ~PACKET_IN_TIMEOUT;
	priv->spp_flags &= ~PACKET_IN_TRANSPORT;
	priv->spp_flags |= PACKET_INTERNAL_ABORT;
	sent = (priv->spp_flags & PACKET_IN_OVERFLOW_Q) ? 0 : 1;
	usocp = priv->spp_portp->sp_board;
	mutex_exit(&priv->spp_mtx);

	mutex_enter(&usocp->usoc_k_imr_mtx);
	if (usocp->usoc_shutdown) {
		mutex_exit(&usocp->usoc_k_imr_mtx);

		if (sent) {
			usoc_free_id_for_pkt(usocp, pkt);
		} else {
			int	qindex, retval;

			/* remove packet from in overflow queue */
			mutex_enter(&priv->spp_mtx);
			if (priv->spp_flags & PACKET_INTERNAL_PACKET) {
				qindex = CQ_REQUEST_0;
			} else if (pkt->pkt_tran_flags & FC_TRAN_HI_PRIORITY) {
				qindex = CQ_REQUEST_1;
			} else {
				qindex = CQ_REQUEST_2;
			}
			mutex_exit(&priv->spp_mtx);

			retval = usoc_remove_overflow_Q(usocp, pkt, qindex);
			/* force a debug kernel to panic */
			ASSERT(retval == FC_SUCCESS);
		}

		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~PACKET_INTERNAL_ABORT;
		priv->spp_flags |= PACKET_DORMANT;
		ASSERT((priv->spp_flags & PACKET_IN_PROCESS) == 0);
		mutex_exit(&priv->spp_mtx);

		pkt->pkt_state = FC_PKT_PORT_OFFLINE;
		pkt->pkt_reason = FC_REASON_OFFLINE;
		usoc_updt_pkt_stat_for_devstate(pkt);

		mutex_enter(&priv->spp_mtx);
		ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
		    PACKET_INTERNAL_PACKET | PACKET_VALID |
		    PACKET_NO_CALLBACK)) == 0);

		if (USOC_PKT_COMP(pkt, priv)) {
			mutex_exit(&priv->spp_mtx);
			pkt->pkt_comp(pkt);
		} else {
			priv->spp_flags &= ~PACKET_NO_CALLBACK;
			mutex_exit(&priv->spp_mtx);
		}
	} else {
		mutex_exit(&usocp->usoc_k_imr_mtx);

		if (usoc_abort_cmd((opaque_t)priv->spp_portp, pkt,
		    intr_mode) != FC_SUCCESS) {
			pkt->pkt_state = FC_PKT_LOCAL_RJT;
			pkt->pkt_reason = FC_REASON_ABORT_FAILED;
			usoc_updt_pkt_stat_for_devstate(pkt);

			mutex_enter(&priv->spp_mtx);
			ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
			    PACKET_INTERNAL_PACKET | PACKET_VALID |
			    PACKET_NO_CALLBACK)) == 0);

			if (USOC_PKT_COMP(pkt, priv)) {
				mutex_exit(&priv->spp_mtx);
				pkt->pkt_comp(pkt);
			} else {
				priv->spp_flags &= ~PACKET_NO_CALLBACK;
				mutex_exit(&priv->spp_mtx);
			}
		}
	}
}


/*
 * If an abort command is in over flow Queue, it gets flushed out first
 * through the unsolicited interrupt of USOC_OFFLINE and the packet is
 * completed via the below function; Subsequently the microcode starts
 * returning all the commands posted to it, including the one that was
 * trying to be aborted (but the abort didn't go to the microcode, rather
 * got stuck in the overflow Queue). The race condition is somewhat
 * handled in usoc_intr_solicited by looking at the packet flags -
 * which if set to DORMANT, or INTERNAL_ABORT, will just drop it and
 * move on to the next one.
 *
 * The above works well with FCP sitting atop transport, which in
 * conjunction with ssd, moves the packet into its overflow queue, so
 * the addresses (pkt pointers) remain valid for a reasonably good
 * time, so when the original packet returns from the microcode
 * (with OFFLINE as the cause). This is going to have nastier problems
 * if the packet gets freed in the mean time by upper layers.
 */
void
usoc_abort_done(fc_packet_t *abort_pkt)
{
	usoc_pkt_priv_t 	*priv;
	fc_packet_t 		*pkt;
	usoc_state_t		*usocp;
#ifdef	LIP_ON_ABORT_FAILURE
	usoc_port_t		*port_statep;
#endif /* LIP_ON_ABORT_FAILURE */
	usoc_pkt_priv_t		*abort_priv =
		(usoc_pkt_priv_t *)abort_pkt->pkt_fca_private;

	mutex_enter(&abort_priv->spp_mtx);
	abort_priv->spp_flags &= ~PACKET_IN_TIMEOUT;
	abort_priv->spp_flags |= PACKET_DORMANT;
	usocp = abort_priv->spp_portp->sp_board;
	mutex_exit(&abort_priv->spp_mtx);

	pkt = (fc_packet_t *)abort_pkt->pkt_ulp_private;
	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
#ifdef	LIP_ON_ABORT_FAILURE
	port_statep = priv->spp_portp;
#endif /* LIP_ON_ABORT_FAILURE */

	mutex_enter(&priv->spp_mtx);
	priv->spp_flags &= ~PACKET_INTERNAL_ABORT;
	priv->spp_flags &= ~PACKET_IN_ABORT;

	ASSERT((priv->spp_flags & PACKET_IN_TRANSPORT) == 0);

	if (priv->spp_flags & PACKET_RETURNED) {
		priv->spp_flags &= ~PACKET_RETURNED;
		abort_pkt->pkt_state = FC_PKT_SUCCESS;
		abort_pkt->pkt_reason = FC_REASON_ABORTED;
	} else {
		mutex_exit(&priv->spp_mtx);
		/*
		 * Original packet has not completed during the abort process
		 * and it was not aborted from the overflow queue.
		 * So free the ID
		 */
		usoc_free_id_for_pkt(usocp, priv->spp_packet);
		mutex_enter(&priv->spp_mtx);
	}
	priv->spp_flags |= PACKET_DORMANT;
	mutex_exit(&priv->spp_mtx);

#ifdef	LIP_ON_ABORT_FAILURE
	if (abort_pkt->pkt_state != FC_PKT_SUCCESS &&
	    abort_pkt->pkt_state != FC_PKT_PORT_OFFLINE) {
		if (usoc_reset_link(port_statep, 1) != FC_SUCCESS) {
			usoc_display(usocp, USOC_PRIV_TO_PORTNUM(priv),
			    CE_WARN, USOC_LOG_ONLY, NULL,
			    "abort failure.. Link reset failed");
		}
	}
#endif /* LIP_ON_ABORT_FAILURE */

	ASSERT((abort_priv->spp_flags & ~(PACKET_DORMANT |
	    PACKET_INTERNAL_PACKET | PACKET_VALID |
	    PACKET_NO_CALLBACK)) == 0);

	pkt->pkt_state = FC_PKT_TIMEOUT;
	pkt->pkt_reason = abort_pkt->pkt_reason;

	usoc_packet_free(abort_pkt);

	mutex_enter(&priv->spp_mtx);

	ASSERT((priv->spp_flags & ~(PACKET_DORMANT | PACKET_INTERNAL_PACKET |
	    PACKET_VALID | PACKET_NO_CALLBACK)) == 0);

	if (USOC_PKT_COMP(pkt, priv)) {
		mutex_exit(&priv->spp_mtx);
		pkt->pkt_comp(pkt);
	} else {
		priv->spp_flags &= ~PACKET_NO_CALLBACK;
		mutex_exit(&priv->spp_mtx);
	}
}


void
usoc_abort_timeout(fc_packet_t *abort_pkt)
{
	int			noreset = 0, intr_mode;
	uint32_t 		d_id;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t 	*priv;
	usoc_pkt_priv_t 	*abort_priv;
	usoc_state_t		*usocp;
	usoc_port_t		*port_statep;

	abort_priv = (usoc_pkt_priv_t *)abort_pkt->pkt_fca_private;
	usocp = abort_priv->spp_portp->sp_board;

	pkt = (fc_packet_t *)abort_pkt->pkt_ulp_private;
	d_id = pkt->pkt_cmd_fhdr.d_id;
	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	port_statep = priv->spp_portp;

	DEBUGF(2, (CE_WARN, "abort timeout: opkt=%p: s_id=%x, d_id=%x, type=%x"
	    " f_ctl=%x, seq_id=%x, df_ctl=%x, seq_cnt=%x, ox_id=%x, rx_id=%x"
	    " ro=%x", (void *)pkt, pkt->pkt_cmd_fhdr.s_id,
	    pkt->pkt_cmd_fhdr.d_id, pkt->pkt_cmd_fhdr.type,
	    pkt->pkt_cmd_fhdr.f_ctl, pkt->pkt_cmd_fhdr.seq_id,
	    pkt->pkt_cmd_fhdr.df_ctl, pkt->pkt_cmd_fhdr.seq_cnt,
	    pkt->pkt_cmd_fhdr.ox_id, pkt->pkt_cmd_fhdr.rx_id,
	    pkt->pkt_cmd_fhdr.ro));

	DEBUGF(2, (CE_WARN, "abort timeout: opkt tran flags:%x: priv_flags %x "
	    " timeout: %x", pkt->pkt_tran_flags, priv->spp_flags,
	    pkt->pkt_timeout));

	DEBUGF(2, (CE_WARN, "abort timeout: apkt=%p: s_id=%x, d_id=%x, type=%x"
	    " f_ctl=%x, seq_id=%x, df_ctl=%x, seq_cnt=%x, ox_id=%x, rx_id=%x"
	    " ro=%x", (void *)abort_pkt, abort_pkt->pkt_cmd_fhdr.s_id,
	    abort_pkt->pkt_cmd_fhdr.d_id,
	    abort_pkt->pkt_cmd_fhdr.type,
	    abort_pkt->pkt_cmd_fhdr.f_ctl,
	    abort_pkt->pkt_cmd_fhdr.seq_id,
	    abort_pkt->pkt_cmd_fhdr.df_ctl,
	    abort_pkt->pkt_cmd_fhdr.seq_cnt,
	    abort_pkt->pkt_cmd_fhdr.ox_id,
	    abort_pkt->pkt_cmd_fhdr.rx_id,
	    abort_pkt->pkt_cmd_fhdr.ro));

	DEBUGF(2, (CE_WARN, "abort timeout: apkt tran flags:%x: priv_flags %x "
	    " timeout: %x", abort_pkt->pkt_tran_flags, abort_priv->spp_flags,
	    abort_pkt->pkt_timeout));

	mutex_enter(&usocp->usoc_time_mtx);
	mutex_enter(&priv->spp_mtx);
	priv->spp_flags &= ~PACKET_INTERNAL_ABORT;
	priv->spp_flags &= ~PACKET_IN_ABORT;

	intr_mode = (pkt->pkt_tran_flags & FC_TRAN_NO_INTR) ? 0 : 1;

	if (priv->spp_flags & PACKET_RETURNED ||
	    (!intr_mode && usocp->usoc_shutdown)) {
		if (!(priv->spp_flags & PACKET_RETURNED)) {
			mutex_exit(&priv->spp_mtx);
			usoc_free_id_for_pkt(usocp, pkt);
			mutex_enter(&priv->spp_mtx);
		} else {
			priv->spp_flags &= ~PACKET_RETURNED;
		}
		priv->spp_flags |= PACKET_DORMANT;

		pkt->pkt_state = FC_PKT_TIMEOUT;
		pkt->pkt_reason = FC_REASON_ABORTED;

		ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
		    PACKET_INTERNAL_PACKET | PACKET_VALID |
		    PACKET_NO_CALLBACK)) == 0);

		if (USOC_PKT_COMP(pkt, priv)) {
			mutex_exit(&priv->spp_mtx);
			mutex_exit(&usocp->usoc_time_mtx);
			pkt->pkt_comp(pkt);
		} else {
			priv->spp_flags &= ~PACKET_NO_CALLBACK;
			mutex_exit(&priv->spp_mtx);
			mutex_exit(&usocp->usoc_time_mtx);
		}
		noreset++;
	} else {
		priv->spp_flags |= PACKET_IN_TRANSPORT;
		ASSERT(priv->spp_flags & PACKET_IN_USOC);
		mutex_exit(&priv->spp_mtx);
		/*
		 * Restart the pkt watchdog timer. The pkt should
		 * eventually get returned by virtue of the reset link
		 * done below.
		 */
		if (intr_mode) {
			usoc_timeout_held(usocp, usoc_pkt_timeout, pkt);

			DEBUGF(2, (CE_CONT, "usoc%d: abort timeout D_ID=%x;"
			    " pkt=%p, abort_pkt=%p\n", usocp->usoc_instance,
			    pkt->pkt_cmd_fhdr.d_id, (void *)pkt,
			    (void *)abort_pkt));
		}
		mutex_exit(&usocp->usoc_time_mtx);
	}

	mutex_enter(&abort_priv->spp_mtx);

	ASSERT(abort_priv->spp_flags & PACKET_IN_TRANSPORT);

	if (abort_priv->spp_flags & PACKET_INTERNAL_ABORT) {
		abort_priv->spp_flags &= ~PACKET_INTERNAL_ABORT;
	}

	if (abort_priv->spp_flags & PACKET_IN_OVERFLOW_Q) {
		mutex_exit(&abort_priv->spp_mtx);
		(void) usoc_remove_overflow_Q(usocp, abort_pkt, CQ_REQUEST_1);
		mutex_enter(&abort_priv->spp_mtx);
	} else {
		/*
		 * abort_pkt was sent to usoc and has timed out. So
		 * free the ID.
		 */
		mutex_exit(&abort_priv->spp_mtx);
		usoc_free_id_for_pkt(usocp, abort_priv->spp_packet);
		mutex_enter(&abort_priv->spp_mtx);
	}

	abort_priv->spp_flags &= ~PACKET_IN_TIMEOUT;
	abort_priv->spp_flags |= PACKET_DORMANT;
	abort_priv->spp_flags &= ~PACKET_IN_TRANSPORT;

	mutex_exit(&abort_priv->spp_mtx);

	usoc_take_core(usocp, USOC_CORE_ON_ABORT_TIMEOUT);

	usoc_display(usocp, USOC_PRIV_TO_PORTNUM(abort_priv), CE_WARN,
	    USOC_LOG_ONLY, NULL, "abort timeout D_ID=0x%x", d_id);

	if (noreset == 0) {
		if (usoc_reset_link(port_statep, 1) != FC_SUCCESS) {
			usoc_display(usocp, USOC_PRIV_TO_PORTNUM(abort_priv),
			    CE_WARN, USOC_LOG_ONLY, NULL,
			    "abort timeout; Link reset failed");
		}

		if (!intr_mode) {
			int ntries = 0;
			/*
			 * wait for microcode to return packet
			 * if packet is still pending
			 */
			mutex_enter(&priv->spp_mtx);
			while (!(priv->spp_flags & PACKET_DORMANT) &&
			    ntries++ < 6) {
				mutex_exit(&priv->spp_mtx);
				delay(drv_usectohz(500000));
				mutex_enter(&priv->spp_mtx);
			}
			mutex_exit(&priv->spp_mtx);

			if (ntries >= 6) {
				(void) usoc_force_reset(usocp, 1, 1);

				usoc_free_id_for_pkt(usocp, pkt);

				mutex_enter(&priv->spp_mtx);
				priv->spp_flags &= ~PACKET_IN_TRANSPORT;
				priv->spp_flags |= PACKET_DORMANT;
				pkt->pkt_state = FC_PKT_TIMEOUT;
				pkt->pkt_reason = FC_REASON_ABORTED;

				ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
				    PACKET_INTERNAL_PACKET | PACKET_VALID |
				    PACKET_NO_CALLBACK)) == 0);

				priv->spp_flags &= ~PACKET_NO_CALLBACK;
				mutex_exit(&priv->spp_mtx);
			}
		}
	}

	ASSERT((abort_priv->spp_flags & ~(PACKET_DORMANT |
	    PACKET_INTERNAL_PACKET | PACKET_VALID | PACKET_NO_CALLBACK)) == 0);

	usoc_packet_free(abort_pkt);
}


static uint_t
usoc_lilp_map(usoc_port_t *port_statep, uint32_t bufid, uint_t polled)
{
	fc_packet_t		*pkt;
	usoc_pkt_priv_t	*priv;
	usoc_data_request_t	*sdr;
	int			retval;

	if ((pkt = usoc_packet_alloc(port_statep, polled))
	    == (fc_packet_t *)NULL)
		return (FC_NOMEM);

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	sdr = (usoc_data_request_t *)&priv->spp_sr;
	if (port_statep->sp_port)
	    sdr->sdr_usoc_hdr.sh_flags = USOC_PORT_B;
	sdr->sdr_usoc_hdr.sh_seg_cnt = 1;
	sdr->sdr_usoc_hdr.sh_byte_cnt = 132;
	sdr->sdr_dataseg[0].fc_base = bufid;
	sdr->sdr_dataseg[0].fc_count = 132;
	sdr->sdr_cqhdr.cq_hdr_count = 1;
	sdr->sdr_cqhdr.cq_hdr_type = CQ_TYPE_REPORT_MAP;

	pkt->pkt_timeout = USOC_LILP_TIMEOUT;
	retval = usoc_doit(port_statep, pkt, polled, PORT_LILP_PENDING, NULL);

	usoc_packet_free(pkt);
	return (retval);
}


static uint_t
usoc_local_rls(usoc_port_t *port_statep, uint32_t bufid, uint_t polled)
{
	fc_packet_t		*pkt;
	usoc_pkt_priv_t	*priv;
	usoc_data_request_t	*sdr;
	int			retval;

	if ((pkt = usoc_packet_alloc(port_statep, polled)) ==
	    (fc_packet_t *)NULL) {
		return (FC_NOMEM);
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	sdr = (usoc_data_request_t *)&priv->spp_sr;

	if (port_statep->sp_port) {
	    sdr->sdr_usoc_hdr.sh_flags = USOC_PORT_B;
	}

	sdr->sdr_usoc_hdr.sh_seg_cnt = 1;
	sdr->sdr_usoc_hdr.sh_byte_cnt = sizeof (fc_rls_acc_t);
	sdr->sdr_dataseg[0].fc_base = bufid;
	sdr->sdr_dataseg[0].fc_count = sizeof (fc_rls_acc_t);
	sdr->sdr_cqhdr.cq_hdr_count = 1;
	sdr->sdr_cqhdr.cq_hdr_type = CQ_TYPE_REPORT_LESB;

	pkt->pkt_timeout = USOC_RLS_TIMEOUT;
	retval = usoc_doit(port_statep, pkt, polled, PORT_RLS_PENDING, NULL);

	usoc_packet_free(pkt);
	return (retval);
}


static int
usoc_force_lip(usoc_port_t *port_statep, uint_t polled)
{
	fc_packet_t		*pkt;
	usoc_cmdonly_request_t	*scr;
	usoc_pkt_priv_t		*priv;
	usoc_state_t		*usocp = port_statep->sp_board;
	int			retval;
	ddi_devstate_t	devstate;

	if (USOC_DEVICE_BAD(usocp, devstate)) {
		return (FC_FAILURE);
	}

	usocp->usoc_stats.pstats[port_statep->sp_port].lips++;

	if ((pkt = usoc_packet_alloc(port_statep, 0)) == NULL) {
		return (FC_NOMEM);
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	scr = (usoc_cmdonly_request_t *)&priv->spp_sr;
	if (port_statep->sp_port) {
		scr->scr_usoc_hdr.sh_flags = USOC_PORT_B;
	}
	scr->scr_cqhdr.cq_hdr_count = 1;
	scr->scr_cqhdr.cq_hdr_type = CQ_TYPE_REQUEST_LIP;

	pkt->pkt_timeout = USOC_LIP_TIMEOUT;

	retval = usoc_doit(port_statep, pkt, polled, PORT_LIP_PENDING, NULL);
	if (retval != FC_SUCCESS) {
		caddr_t fc_error;

		(void) fc_fca_error(retval, &fc_error);

		usoc_display(usocp, port_statep->sp_port, CE_WARN,
		    USOC_LOG_ONLY, NULL, "Force lip failed: %s,", fc_error);

		if (usoc_force_reset(usocp, 1, 1) != FC_SUCCESS) {
			retval = FC_FAILURE;
		}
	}

	usoc_packet_free(pkt);

	return (retval);
}


/*
 * Entry point for external aborts.
 */
/* ARGSUSED */
static int
usoc_external_abort(opaque_t portp, fc_packet_t *pkt, int sleep)
{
	int 		rval;
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_port_t 	*port_statep = (usoc_port_t *)portp;
	usoc_state_t	*usocp = port_statep->sp_board;
	int		intr_mode;

try_again:
	mutex_enter(&priv->spp_mtx);
	if (!(priv->spp_flags & PACKET_IN_TRANSPORT) &&
	    !(priv->spp_flags & PACKET_IN_ABORT)) {
		priv->spp_flags &= ~PACKET_NO_CALLBACK;
		mutex_exit(&priv->spp_mtx);
		return (FC_ABORT_FAILED);
	}
	priv->spp_flags |= PACKET_NO_CALLBACK;

	if (priv->spp_flags & PACKET_IN_INTR ||
	    priv->spp_flags & PACKET_IN_ABORT ||
	    priv->spp_flags & PACKET_IN_TIMEOUT ||
	    priv->spp_flags & PACKET_INTERNAL_ABORT) {
		if (sleep != KM_SLEEP) {
			priv->spp_flags &= ~PACKET_NO_CALLBACK;
			mutex_exit(&priv->spp_mtx);
			return (FC_ABORT_FAILED);
		}
		mutex_exit(&priv->spp_mtx);
		delay(drv_usectohz(1000000));
		goto try_again;
	}

	priv->spp_flags &= ~PACKET_IN_TRANSPORT;
	priv->spp_flags |= PACKET_IN_ABORT;	/* to avoid a race condition */
	mutex_exit(&priv->spp_mtx);

	if ((pkt->pkt_tran_flags & FC_TRAN_NO_INTR) == 0) {
		usoc_untimeout(usocp, priv);
	}

	intr_mode = (sleep == KM_SLEEP) ? 0 : 1;

	rval = usoc_abort_cmd((opaque_t)priv->spp_portp, pkt, intr_mode);
	if (sleep == KM_SLEEP) {
		pkt->pkt_state = FC_PKT_LOCAL_RJT;

		if (rval == FC_SUCCESS) {
			ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
			    PACKET_INTERNAL_PACKET | PACKET_VALID |
			    PACKET_NO_CALLBACK)) == 0);
			pkt->pkt_reason = FC_REASON_ABORTED;
		} else {
			pkt->pkt_reason = FC_REASON_ABORT_FAILED;
		}

		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~PACKET_NO_CALLBACK;
		mutex_exit(&priv->spp_mtx);

	} else if (rval != FC_SUCCESS) {
		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~PACKET_NO_CALLBACK;
		mutex_exit(&priv->spp_mtx);
	}

	DEBUGF(2, (CE_WARN, "usoc(%d): external abort completed; pkt=%p,"
	    " result=%x\n", usocp->usoc_instance, (void *)pkt, rval));

	return (rval);
}


static int
usoc_abort_cmd(opaque_t portp, fc_packet_t *pkt, int intr_mode)
{
	usoc_port_t 		*port_statep = (usoc_port_t *)portp;
	usoc_cmdonly_request_t	*scr;
	usoc_state_t		*usocp = port_statep->sp_board;
	usoc_pkt_priv_t		*priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_pkt_priv_t		*abort_priv;
	usoc_cmdonly_request_t	*abort_scr;
	fc_packet_t		*abort_pkt;
	int			retval;
	uint32_t		csr;
	int			qindex;
	clock_t 		pkt_ticks, ticker, t;

	usocp->usoc_stats.pstats[port_statep->sp_port].abts++;
	scr = (usoc_cmdonly_request_t *)&priv->spp_sr;

	if ((abort_pkt = usoc_packet_alloc(port_statep, 0)) ==
	    (fc_packet_t *)NULL) {
		return (FC_NOMEM);
	}

	abort_priv = (usoc_pkt_priv_t *)abort_pkt->pkt_fca_private;
	abort_scr = (usoc_cmdonly_request_t *)&abort_priv->spp_sr;

	abort_pkt->pkt_ulp_private = (opaque_t)pkt;
	abort_pkt->pkt_timeout = USOC_ABORT_TIMEOUT;	/* 20 seconds */

	mutex_enter(&priv->spp_mtx);
	abort_scr->scr_usoc_hdr.sh_byte_cnt =
	    scr->scr_usoc_hdr.sh_request_token;
	priv->spp_flags |= PACKET_IN_ABORT;
#ifdef DEBUG
	priv->spp_abort_pkt = abort_pkt;
#endif
	mutex_exit(&priv->spp_mtx);

	mutex_enter(&abort_priv->spp_mtx);
	abort_priv->spp_flags &= ~PACKET_DORMANT;
	abort_priv->spp_flags |= PACKET_IN_TRANSPORT;
	mutex_exit(&abort_priv->spp_mtx);

	abort_scr->scr_cqhdr.cq_hdr_count = 1;
	abort_scr->scr_cqhdr.cq_hdr_type = CQ_TYPE_REQUEST_ABORT;

	if (port_statep->sp_port) {
		abort_scr->scr_usoc_hdr.sh_flags = USOC_PORT_B;
	}

	abort_pkt->pkt_state = FC_PKT_FAILURE;

	mutex_enter(&priv->spp_mtx);
	if (priv->spp_flags & PACKET_IN_OVERFLOW_Q) {
		if (priv->spp_flags & PACKET_INTERNAL_PACKET) {
			qindex = CQ_REQUEST_0;
		} else if (pkt->pkt_tran_flags & FC_TRAN_HI_PRIORITY) {
			qindex = CQ_REQUEST_1;
		} else {
			qindex = CQ_REQUEST_2;
		}
		mutex_exit(&priv->spp_mtx);

		retval = usoc_remove_overflow_Q(usocp, pkt, qindex);
		if (retval == FC_SUCCESS) {
			int do_callback;

			pkt->pkt_state = FC_PKT_TIMEOUT;
			pkt->pkt_reason = FC_REASON_ABORTED;

			mutex_enter(&priv->spp_mtx);
			if (USOC_PKT_COMP(pkt, priv)) {
				do_callback = 1;
			} else {
				priv->spp_flags &= ~PACKET_NO_CALLBACK;
				do_callback = 0;
			}
			mutex_exit(&priv->spp_mtx);

			abort_pkt->pkt_state = FC_PKT_SUCCESS;
			abort_pkt->pkt_reason = FC_REASON_ABORTED;

			if (intr_mode && do_callback) {
				/*
				 * Throw the abort packet on the timeout
				 * list to be called into usoc_abort_done
				 * from a different context; Just a Hack.
				 */
				abort_pkt->pkt_comp = usoc_abort_done;
				abort_pkt->pkt_timeout = 0;

				mutex_enter(&abort_priv->spp_mtx);
				abort_priv->spp_flags &= ~PACKET_IN_TRANSPORT;
				mutex_exit(&abort_priv->spp_mtx);
				/*
				 * Mark the packet as RETURNED. This will make
				 * sure that usoc_abort_done does not try to
				 * free the ID.
				 */
				mutex_enter(&priv->spp_mtx);
				priv->spp_flags |= PACKET_RETURNED;
				mutex_exit(&priv->spp_mtx);

				usoc_timeout(usocp, usoc_abort_done, abort_pkt);
			} else {
				mutex_enter(&priv->spp_mtx);
				priv->spp_flags &= ~PACKET_INTERNAL_ABORT;
				priv->spp_flags &= ~PACKET_IN_ABORT;
				priv->spp_flags &= ~PACKET_NO_CALLBACK;
				priv->spp_flags |= PACKET_DORMANT;
				mutex_exit(&priv->spp_mtx);

				mutex_enter(&abort_priv->spp_mtx);
				abort_priv->spp_flags &= ~PACKET_IN_TRANSPORT;
				abort_priv->spp_flags |= PACKET_DORMANT;
				mutex_exit(&abort_priv->spp_mtx);

				ASSERT((abort_priv->spp_flags &
				    ~(PACKET_DORMANT | PACKET_INTERNAL_PACKET |
				    PACKET_VALID | PACKET_NO_CALLBACK)) == 0);

				usoc_packet_free(abort_pkt);
			}
			return (FC_SUCCESS);
		}

		/* force a debug kernel to panic */
		ASSERT(retval == FC_SUCCESS);
	}
	mutex_exit(&priv->spp_mtx);

	if (intr_mode) {
		abort_pkt->pkt_comp = usoc_abort_done;
		abort_pkt->pkt_tran_flags = FC_TRAN_INTR;
		usoc_timeout(usocp, usoc_abort_timeout, abort_pkt);
	} else {
		abort_pkt->pkt_tran_flags = FC_TRAN_NO_INTR;
	}

	if ((retval = usoc_cq_enque(usocp, port_statep, (cqe_t *)abort_scr,
	    CQ_REQUEST_1, abort_pkt, abort_pkt, 0)) != FC_SUCCESS) {
		ASSERT(retval == USOC_UNAVAIL);

		pkt->pkt_state = FC_PKT_PORT_OFFLINE;
		DEBUGF(2, (CE_WARN, "usoc: abort_cmd: usoc_cq_enque "
		    "failed: %x", retval));

		mutex_enter(&abort_priv->spp_mtx);
		abort_priv->spp_flags &= ~PACKET_IN_TRANSPORT;
		abort_priv->spp_flags |= PACKET_DORMANT;
		mutex_exit(&abort_priv->spp_mtx);

		if (intr_mode) {
			usoc_untimeout(usocp, abort_priv);
		}

		/*
		 * abort_cmd could not be sent, as usoc is unavailable.
		 * Original pkt will never complete. So free the ID.
		 */
		usoc_free_id_for_pkt(usocp, pkt);

		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~(PACKET_IN_ABORT |
		    PACKET_INTERNAL_ABORT);
		priv->spp_flags |= PACKET_DORMANT;
		mutex_exit(&priv->spp_mtx);

		ASSERT((abort_priv->spp_flags & ~(PACKET_DORMANT |
		    PACKET_INTERNAL_PACKET | PACKET_VALID |
		    PACKET_NO_CALLBACK)) == 0);

		usoc_packet_free(abort_pkt);

		return (FC_TRANSPORT_ERROR);
	}

	if (intr_mode) {
		return (FC_SUCCESS);
	}


	/* Wait for completion of abort command */
	ASSERT(pkt->pkt_timeout > 0);
	pkt_ticks = drv_usectohz(pkt->pkt_timeout * 1000 * 1000);
	(void) drv_getparm(LBOLT, &ticker);

	mutex_enter(&abort_priv->spp_mtx);
	while ((abort_priv->spp_flags & PACKET_IN_PROCESS) != 0) {

		mutex_exit(&abort_priv->spp_mtx);
		delay(drv_usectohz(USOC_NOINTR_POLL_DELAY_TIME));

		(void) drv_getparm(LBOLT, &t);
		if ((ticker + pkt_ticks) < t || usocp->usoc_shutdown) {
			mutex_enter(&abort_priv->spp_mtx);
			if (abort_priv->spp_flags & PACKET_IN_TRANSPORT) {
				mutex_exit(&abort_priv->spp_mtx);

				usoc_display(usocp, port_statep->sp_port,
				    CE_WARN, USOC_LOG_ONLY, NULL,
				    "abort timeout D_ID=0x%x for a polled"
				    " command", pkt->pkt_cmd_fhdr.d_id);

				usoc_abort_timeout(abort_pkt);

				return (FC_TRANSPORT_ERROR);
			} else {
				/* Packet had just completed */
				mutex_exit(&abort_priv->spp_mtx);
			}
		}

		csr = USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_csr.w);

		/* Check for register access handle fault */
		if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle)) {
			usoc_display(usocp, port_statep->sp_port, CE_WARN,
			    USOC_LOG_ONLY, NULL, "abort_cmd for a polled"
			    " pkt(%p); access handle fault", (void *)pkt);

		} else if ((USOC_INTR_CAUSE(usocp, csr)) & USOC_CSR_RSP_QUE_0) {
			(void) usoc_intr_solicited(usocp, 0);
		}

		mutex_enter(&abort_priv->spp_mtx);
	}
	mutex_exit(&abort_priv->spp_mtx);

	mutex_enter(&priv->spp_mtx);
	priv->spp_flags &= ~(PACKET_IN_ABORT | PACKET_INTERNAL_ABORT |
	    PACKET_NO_CALLBACK);
	if (priv->spp_flags & PACKET_RETURNED) {
		priv->spp_flags &= ~PACKET_RETURNED;
	} else {
		mutex_exit(&priv->spp_mtx);
		usoc_free_id_for_pkt(usocp, pkt);
		mutex_enter(&priv->spp_mtx);
	}
	priv->spp_flags |= PACKET_DORMANT;
	mutex_exit(&priv->spp_mtx);

	ASSERT((abort_priv->spp_flags & ~(PACKET_DORMANT |
	    PACKET_INTERNAL_PACKET | PACKET_VALID |
	    PACKET_NO_CALLBACK)) == 0);

	usoc_packet_free(abort_pkt);

	return (FC_SUCCESS);
}


static int
usoc_bypass_dev(usoc_port_t *port_statep, uint_t dest)
{
	fc_packet_t		*pkt;
	usoc_cmdonly_request_t	*scr;
	usoc_pkt_priv_t	*priv;
	int			retval;

	if ((pkt = usoc_packet_alloc(port_statep, 0))
	    == (fc_packet_t *)NULL)
		return (FC_NOMEM);

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	scr = (usoc_cmdonly_request_t *)&priv->spp_sr;
	if (port_statep->sp_port)
	    scr->scr_usoc_hdr.sh_flags = USOC_PORT_B;
	scr->scr_usoc_hdr.sh_byte_cnt = dest;
	scr->scr_cqhdr.cq_hdr_count = 1;
	scr->scr_cqhdr.cq_hdr_type = CQ_TYPE_BYPASS_DEV;

	pkt->pkt_timeout = USOC_BYPASS_TIMEOUT;

	retval = usoc_doit(port_statep, pkt, 0, PORT_BYPASS_PENDING, NULL);

	usoc_packet_free(pkt);
	return (retval);
}


static void
usoc_take_core(usoc_state_t *usocp, int core_flags)
{
	caddr_t		mesg;
	int		force;

	force = core_flags & USOC_FORCE_CORE;
	core_flags &= ~USOC_FORCE_CORE;

	mutex_enter(&usoc_global_lock);
	if (((usoc_core_taken == 0) || (force != 0)) &&
	    (usoc_core_flags & core_flags)) {
		usoc_core_taken = 1;
		mutex_exit(&usoc_global_lock);

		switch (core_flags) {
		case USOC_CORE_ON_ABORT_TIMEOUT:
			mesg = "Taking core on abort timeout";
			break;

		case USOC_CORE_ON_BAD_TOKEN:
			mesg = "Taking core on bad token";
			break;

		case USOC_CORE_ON_BAD_ABORT:
			mesg = "Taking core on abort failure";
			break;

		case USOC_CORE_ON_BAD_UNSOL:
			mesg = "Taking core on invalid unsol cq entry";
			break;

		case USOC_CORE_ON_LOW_THROTTLE:
			mesg = "Taking core on low throttle";
			break;

		case USOC_CORE_ON_SEND_1:
			mesg = "Taking core before transporting outbound frame";
			break;

		case USOC_CORE_ON_SEND_2:
			mesg = "Taking core after transporting outbound frame";
			break;

		default:
			mesg = "Taking core, reason = UNKNOWN";
			break;
		}

		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, mesg);

		if (usoc_dump_xram_buf(usocp) != FC_SUCCESS) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "Take core failed");
		}
	} else {
		mutex_exit(&usoc_global_lock);
	}
}


static void
usoc_task_force_reload(void *arg)
{
	usoc_state_t	*usocp = (usoc_state_t *)arg;

	usoc_take_core(usocp, USOC_CORE_ON_LOW_THROTTLE);
	(void) usoc_force_reset(usocp, 1, 1);

	mutex_enter(&usocp->usoc_board_mtx);
	usocp->usoc_reload_pending = 0;
	usocp->usoc_throttle = USOC_MAX_THROTTLE;
	mutex_exit(&usocp->usoc_board_mtx);
}


/*ARGSUSED*/
static int
usoc_force_reset(usoc_state_t *usocp, int restart, int flag)
{
	if (restart) {
		if (usocp->usoc_port_state[0].sp_statec_callb) {
			(usocp->usoc_port_state[0].sp_statec_callb)
			    (usocp->usoc_port_state[0].sp_tran_handle,
			    FC_STATE_RESET_REQUESTED);
		}

		if (usocp->usoc_port_state[1].sp_statec_callb) {
			(usocp->usoc_port_state[1].sp_statec_callb)
			    (usocp->usoc_port_state[1].sp_tran_handle,
			    FC_STATE_RESET_REQUESTED);
		}
	}

	mutex_enter(&usocp->usoc_k_imr_mtx);
	if (usocp->usoc_shutdown) {
		mutex_exit(&usocp->usoc_k_imr_mtx);
		return (FC_SUCCESS);
	} else {
		usocp->usoc_shutdown = 1;
		mutex_exit(&usocp->usoc_k_imr_mtx);
	}

	/*
	 * If card reset is being done as a recovery procedure, and
	 * if it is happening too often, then go ahead and report
	 * the device as faulty. If we don't do this, the we might get into
	 * infinite number of reset recoveries.
	 */
	if (flag) {
		if (usocp->usoc_rec_resets < USOC_MAX_REC_RESETS) {
			if ((usocp->usoc_ticker - usocp->usoc_reset_rec_time) <
			    USOC_MIN_REC_RESET_TIME) {
				usocp->usoc_rec_resets++;
			} else {
				usocp->usoc_rec_resets = 0;
			}
			usocp->usoc_reset_rec_time = usocp->usoc_ticker;
		} else {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "too many card resets");

			(void) USOC_REPORT_FAULT(usocp);

			return (FC_FAILURE);
		}
	} else {
		/*
		 * Clear reset counter, because this reset for a valid
		 * purpose.
		 */
		usocp->usoc_reset_rec_time = usocp->usoc_ticker;
		usocp->usoc_rec_resets = 0;
	}

	usocp->usoc_stats.resets++;
	usoc_doreset(usocp);

	if (restart) {
		if (usoc_start(usocp) == FC_SUCCESS) {
			usoc_reestablish_ubs(&usocp->usoc_port_state[0]);
			usoc_reestablish_ubs(&usocp->usoc_port_state[1]);
		} else {
			/*
			 * Should we free all DMA resources ? Wait until
			 * we get another chance - with another reset
			 * probably ?
			 */
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "failed to start f/w");

			return (FC_FAILURE);
		}
	}
	return (FC_SUCCESS);
}


/*
 * static unsigned int
 * usoc_intr() - this is the interrupt routine for the USOC. Process all
 *	possible incoming interrupts from the usoc device.
 */
static unsigned int
usoc_intr(caddr_t arg)
{
	usoc_state_t			*usocp = (usoc_state_t *)arg;
	register volatile usoc_reg_t	*usocreg = usocp->usoc_rp;
	volatile unsigned		csr;
	volatile int			cause = 0;
	int				j, dispatch;
	ddi_devstate_t			devstate;
	int				retval, status_sol, status_unsol;

	dispatch = 0;
	status_sol = status_unsol = DDI_INTR_UNCLAIMED;

	/* Save the time for the latent fault detection */
	usocp->usoc_alive_time = usocp->usoc_ticker;

	/* Check if device is marked bad */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "interrupt after device was reported faulty");

		return (DDI_INTR_UNCLAIMED);
	}

	csr = USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_csr.w);

	if (USOC_DDH_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle,
	    USOC_DDH_RP_INTR_ACCHDL_FAULT)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "interrupt: rp access handle fault");

		return (DDI_INTR_UNCLAIMED);
	}

	cause = (int)USOC_INTR_CAUSE(usocp, csr);

	DEBUGF(2, (CE_CONT, "usoc%d: intr: csr: 0x%x cause: 0x%x\n",
	    usocp->usoc_instance, csr, cause));

	if (!cause) {
		return (DDI_INTR_UNCLAIMED);
	}

	while (cause) {
		/*
		 * Process the unsolicited messages first in case
		 * there are some high priority async events that
		 * we should act on.
		 */
		if (cause & USOC_CSR_RSP_QUE_1) {
			if (usoc_dbg_drv_hdn != USOC_DDH_SPURIOUS_UNSOL) {
				status_sol = usoc_intr_unsolicited(usocp, 1);
			}
			DEBUGF(4, (CE_CONT, "usoc%d intr: did unsolicited\n",
			    usocp->usoc_instance));

			if (status_sol == DDI_INTR_CLAIMED) {
				usocp->usoc_spurious_unsol_intrs = 0;
			} else {
				usocp->usoc_spurious_unsol_intrs++;
			}
			if (usocp->usoc_spurious_unsol_intrs >=
			    USOC_MAX_SPURIOUS_INTR) {
				break;
			}
		}

		if (cause & USOC_CSR_RSP_QUE_0) {
			if (usoc_dbg_drv_hdn != USOC_DDH_SPURIOUS_SOL) {
				status_unsol = usoc_intr_solicited(usocp, 0);
			}

			DEBUGF(4, (CE_CONT, "usoc%d intr: did solicited\n",
			    usocp->usoc_instance));

			if (status_unsol == DDI_INTR_CLAIMED) {
				usocp->usoc_spurious_sol_intrs = 0;
			} else {
				usocp->usoc_spurious_sol_intrs++;
			}
			if (usocp->usoc_spurious_sol_intrs >=
			    USOC_MAX_SPURIOUS_INTR) {
				break;
			}
		}

		if ((cause & USOC_CSR_HOST_TO_USOC) != 0) {
			usoc_monitor(usocp);
			dispatch++;
		}

		csr = USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_csr.w);
		cause = (int)USOC_INTR_CAUSE(usocp, csr);
		DEBUGF(4, (CE_CONT, "usoc%d intr: did "
		    " request queues\n", usocp->usoc_instance));
	}

	mutex_enter(&usocp->usoc_board_mtx);
	if ((usocp->usoc_spurious_unsol_intrs >= USOC_MAX_SPURIOUS_INTR ||
	    usocp->usoc_spurious_sol_intrs >= USOC_MAX_SPURIOUS_INTR) &&
	    (usocp->usoc_reload_pending == 0)) {
		usocp->usoc_reload_pending = 1;
		mutex_exit(&usocp->usoc_board_mtx);

		/* Too many spurious interrupts */
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
		    "too many spurious intrs. unsolicited %x solicited"
		    " %x. Force reset", usocp->usoc_spurious_unsol_intrs,
		    usocp->usoc_spurious_sol_intrs);

		if (taskq_dispatch(usocp->usoc_task_handle,
		    usoc_task_force_reload, usocp, KM_NOSLEEP) != 0) {
			/* taskq dispatch was successful */
			usocp->usoc_spurious_sol_intrs = 0;
			usocp->usoc_spurious_unsol_intrs = 0;
		} else {
			/* taskq dispatch was failed */
			mutex_enter(&usocp->usoc_board_mtx);
			usocp->usoc_reload_pending = 0;
			mutex_exit(&usocp->usoc_board_mtx);
		}
	} else {
		mutex_exit(&usocp->usoc_board_mtx);
	}

	/* Check if we got a valid interrupt */
	if (status_sol == DDI_INTR_CLAIMED || dispatch ||
	    status_unsol == DDI_INTR_CLAIMED) {
		retval = DDI_INTR_CLAIMED;
	} else {
		retval = DDI_INTR_UNCLAIMED;
	}

	if (!dispatch) {
		for (j = 0; j < USOC_N_CQS; j++) {
			usoc_kcq_t	*kcq = &usocp->usoc_request[j];

			if (kcq->skc_overflowh != NULL) {
				dispatch++;
				break;
			}
		}

		if (dispatch) {
			(void) taskq_dispatch(usocp->usoc_task_handle,
			    usoc_monitor, usocp, KM_NOSLEEP);
		}
	}

	return (retval);
}

static int
usoc_intr_solicited(usoc_state_t *usocp, uint32_t srq)
{
	usoc_kcq_t		*kcq;
	volatile usoc_kcq_t	*kcqv;
	usoc_response_t		*srp;
	cqe_t			*cqe;
	uint_t			status, i;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;
	usoc_header_t		*shp;
	register volatile usoc_reg_t *usocreg;
	caddr_t			src, dst;
	uchar_t			index_in;
	cq_hdr_t		*cq_hdr;
	ddi_devstate_t		devstate;
	int			retval;

	/* Check if device is marked bad */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "solicited interrupt after device was marked faulty");

		return (DDI_INTR_UNCLAIMED);
	}

	usocreg = usocp->usoc_rp;

	kcq = &usocp->usoc_response[srq];
	kcqv = (volatile usoc_kcq_t *)kcq;

	DEBUGF(4, (CE_CONT, "usoc%d intr_sol: entered\n",
	    usocp->usoc_instance));

	/*
	 * Grab lock for request queue.
	 */
	mutex_enter(&kcq->skc_mtx);

	/*
	 * Process as many response queue entries as we can.
	 */
	cqe = &(kcq->skc_cq[kcqv->skc_out]);

	index_in = USOC_RESPONSEQ_INDEX(srq,
	    USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_rspp.w));

	/* Check for access handle failure */
	if (USOC_DDH_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle,
	    USOC_DDH_SOLINTR_ACCHDL_FAULT)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "solicited interrupt: rspq %x rp access handle fault",
		    srq);

		/* Drop lock for request queue. */
		mutex_exit(&kcq->skc_mtx);
		return (DDI_INTR_UNCLAIMED);
	}

	if (kcqv->skc_out != index_in) {
		retval = DDI_INTR_CLAIMED;
	} else {
		retval = DDI_INTR_UNCLAIMED;
	}

	while (kcqv->skc_out != index_in) {
		/*
		 * Find out where the newest entry
		 * lives in the queue
		 */
		USOC_SYNC_FOR_KERNEL(kcq->skc_dhandle, 0, 0);

		/* Check for response queue access handle fault */
		if (USOC_ACCHDL_FAULT(usocp, kcq->skc_acchandle)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "solicited interrupt:"
			    " rspq %x skc_acchandle fault", srq);

			break;
		}

		/* Check for response queue dma transfer fault */
		if (USOC_DMAHDL_FAULT(usocp, kcq->skc_dhandle)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "solicited interrupt:"
			    " resq %x skc_dma handle fault", srq);

			break;
		}

		/*
		 * Since SOC+ is big endian, access the cq entry directly.
		 * We have checked access handle using ddi calls (Driver
		 * Hardening) to check for bus faults.
		 */
		srp = (usoc_response_t *)cqe;
		shp = &srp->sr_usoc_hdr;
		cq_hdr = &srp->sr_cqhdr;

		/*
		 * It turns out that on faster CPU's we have
		 * a problem where the usoc interrupts us before
		 * the response has been DMA'ed in. This should
		 * not happen but does !!. So to workaround the
		 * problem for now, check the sequence # of the
		 * response. If it does not match with what we
		 * have, we must be reading stale data.
		 */
		if (cq_hdr->cq_hdr_seqno != kcqv->skc_seqno) {
			if (kcq->deferred_intr_timeoutid) {
				mutex_exit(&kcq->skc_mtx);
				return (DDI_INTR_CLAIMED);
			} else {
				kcq->skc_saved_out = kcqv->skc_out;
				kcq->skc_saved_seqno = kcqv->skc_seqno;
				kcq->deferred_intr_timeoutid = timeout(
					usoc_deferred_intr, (caddr_t)usocp,
					drv_usectohz(10000));
				mutex_exit(&kcq->skc_mtx);
				return (DDI_INTR_CLAIMED);
			}
		}

		DEBUGF(3, (CE_CONT, "usoc%d intr_sol: free id %x\n",
		    usocp->usoc_instance, shp->sh_request_token));

		pkt = usoc_free_id(usocp, shp->sh_request_token, 0);
		if ((pkt == (fc_packet_t *)0) || (pkt == (fc_packet_t *)1)) {
			kcqv->skc_out++;
			if ((kcqv->skc_out & kcq->skc_last_index) == 0) {
				kcqv->skc_out = 0;
				kcqv->skc_seqno++;
			}

			if (pkt == (fc_packet_t *)1) {
				mutex_exit(&kcq->skc_mtx);
				usoc_take_core(usocp,
				    USOC_CORE_ON_BAD_TOKEN | USOC_FORCE_CORE);
				mutex_enter(&kcq->skc_mtx);
			}

			goto intr_process;
		}

		priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
		if (priv != NULL) {
			mutex_enter(&priv->spp_mtx);
		}

		if (priv == NULL || !(priv->spp_flags & (PACKET_VALID |
		    PACKET_IN_TRANSPORT)) || (priv->spp_flags &
		    (PACKET_IN_TIMEOUT | PACKET_IN_ABORT |
		    PACKET_INTERNAL_ABORT | PACKET_DORMANT))) {

			if (priv == NULL || priv->spp_flags & PACKET_DORMANT) {
				usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
				    USOC_LOG_ONLY, NULL, "invalid FC packet;"
				    " in, out, seqno = 0x%x, 0x%x, 0x%x;"
				    " pkt = %p\n", kcqv->skc_in, kcqv->skc_out,
				    kcqv->skc_seqno, (void *)pkt);
			} else {
				priv->spp_flags |= PACKET_RETURNED;
			}

			DEBUGF(4, (CE_CONT, "\tsoc CR: 0x%x SAE: 0x%x "
			    " CSR: 0x%x IMR: 0x%x\n",
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_cr.w),
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_sae.w),
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w),
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_imr)));
			/*
			 * Update response queue ptrs and usoc registers.
			 */
			if (priv != NULL) {
				mutex_exit(&priv->spp_mtx);
			}
			kcqv->skc_out++;
			if ((kcqv->skc_out & kcq->skc_last_index) == 0) {
				kcqv->skc_out = 0;
				kcqv->skc_seqno++;
			}
		} else {
			DEBUGF(2, (CE_CONT, "packet 0x%p complete\n",
			    (void *)pkt));
			status = srp->sr_usoc_status;

			usoc_update_pkt_state(pkt, status);

			if (status == USOC_MAX_XCHG_EXCEEDED) {
				mutex_enter(&usocp->usoc_board_mtx);
				usocp->usoc_throttle = usocp->usoc_ncmds - 25;
				if ((usocp->usoc_throttle <
				    USOC_MIN_THROTTLE) &&
				    (usocp->usoc_reload_pending == 0)) {
					usocp->usoc_reload_pending = 1;
					mutex_exit(&usocp->usoc_board_mtx);
					usoc_display(usocp, USOC_PORT_ANY,
					    CE_WARN, USOC_LOG_ONLY, NULL,
					    "throttle %x force reload",
					    usocp->usoc_throttle);

					if (taskq_dispatch(
					    usocp->usoc_task_handle,
					    usoc_task_force_reload, usocp,
					    KM_NOSLEEP) == 0) {
						/* taskq dispatch failed */
						mutex_enter(
						    &usocp->usoc_board_mtx);
						usocp->usoc_reload_pending = 0;
						mutex_exit(
						    &usocp->usoc_board_mtx);
					}
				} else {
					mutex_exit(&usocp->usoc_board_mtx);
				}
			}

			DEBUGF(2, (CE_CONT, "USOC status: 0x%x\n", status));

			priv->spp_flags |= PACKET_IN_INTR;
			ASSERT(priv->spp_flags & PACKET_IN_TRANSPORT);

			mutex_exit(&priv->spp_mtx);
			if ((pkt->pkt_tran_flags & FC_TRAN_NO_INTR) == 0) {
				usoc_untimeout(usocp, priv);
			}
			mutex_enter(&priv->spp_mtx);
			priv->spp_flags &= ~PACKET_IN_INTR;
			priv->spp_flags &= ~PACKET_IN_PROCESS;

			/*
			 * Copy the response frame header (if there is one)
			 * so that the upper levels can use it.  Note that,
			 * for now, we'll copy the header only if there was
			 * some sort of non-OK status, to save the PIO reads
			 * required to get the header from the host adapter's
			 * xRAM.
			 */
			if (((status != USOC_OK) ||
			    (priv->spp_sr.sr_usoc_hdr.sh_flags &
			    USOC_RESP_HEADER)) && (srp->sr_usoc_hdr.sh_flags &
			    USOC_FC_HEADER)) {
				src = (caddr_t)&srp->sr_fc_frame_hdr;
				dst = (caddr_t)&pkt->pkt_resp_fhdr;
				bcopy(src, dst, sizeof (fc_frame_hdr_t));
				i = srp->sr_usoc_hdr.sh_flags & USOC_PORT_B ?
				    1 : 0;
				if (status <= USOC_MAX_STATUS)
					usocp->usoc_stats.pstats[i].
					    status[status]++;
			}
			if (status == USOC_OK) {
				priv->spp_sr.sr_usoc_hdr.sh_byte_cnt =
				    srp->sr_usoc_hdr.sh_byte_cnt;
				/*
				 * perform ddi_dma_sync().
				 * Check for cmd/resp/data dma faults
				 * This avoids the need for the upper layers
				 * to make this check at all places.
				 */
				usoc_finish_xfer(pkt);
			}
			priv->spp_diagcode = (uint32_t)srp->sr_dataseg.fc_base;
			priv->spp_flags |= PACKET_DORMANT;
			ASSERT((priv->spp_flags & PACKET_IN_PROCESS) == 0);

			if ((priv->spp_sr.sr_usoc_hdr.sh_flags &
			    USOC_RESP_HEADER) && (srp->sr_usoc_hdr.sh_flags &
			    USOC_FC_HEADER)) {
				mutex_exit(&priv->spp_mtx);
				/*
				 * Have transport update state, reason,
				 * action, expln in case of errors.
				 */
				(void) fc_fca_update_errors(pkt);
			} else {
				mutex_exit(&priv->spp_mtx);
			}

			/*
			 * Update response queue ptrs and usoc registers.
			 */
			kcqv->skc_out++;
			if ((kcqv->skc_out & kcq->skc_last_index) == 0) {
				kcqv->skc_out = 0;
				kcqv->skc_seqno++;
			}

			/*
			 * Call the completion routine
			 */
			mutex_enter(&priv->spp_mtx);

			ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
			    PACKET_INTERNAL_PACKET | PACKET_VALID |
			    PACKET_NO_CALLBACK)) == 0);

			if (USOC_PKT_COMP(pkt, priv)) {
				void (*func) (void *);

				mutex_exit(&priv->spp_mtx);
				/*
				 * Give up the mutex to avoid a deadlock
				 * with the callback routine
				 */
				func = (void (*) (void *))pkt->pkt_comp;

				mutex_exit(&kcq->skc_mtx);

				if (pkt->pkt_state == FC_PKT_PORT_OFFLINE) {
					func(pkt);
				} else if (taskq_dispatch(
				    usocp->usoc_task_handle, func, pkt,
				    KM_NOSLEEP) == 0) {
					func(pkt);
				}
				mutex_enter(&kcq->skc_mtx);
			} else {
				priv->spp_flags &= ~PACKET_NO_CALLBACK;
				mutex_exit(&priv->spp_mtx);
			}
		}

		if (kcq->skc_cq == NULL) {
			/*
			 * This action averts a potential PANIC scenario
			 * where the SUSPEND code flow grabbed the kcq->skc_mtx
			 * when we let it go, to call our completion routine,
			 * and "initialized" the response queue.  We exit our
			 * processing loop here, thereby averting a PANIC due
			 * to a NULL de-reference from the response queue.
			 *
			 * Note that this is an interim measure that needs
			 * to be revisited when this driver is next revised
			 * for enhanced performance.
			 */
			break;
		}

intr_process:
		/*
		 * We need to re-read the input and output pointers in
		 * case a polling routine should process some entries
		 * from the response queue while we're doing a callback
		 * routine with the response queue mutex dropped.
		 */
		cqe = &(kcq->skc_cq[kcqv->skc_out]);
		index_in = USOC_RESPONSEQ_INDEX(srq,
		    USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_rspp.w));

		/*
		 * Mess around with the hardware if we think we've run out
		 * of entries in the queue, just to make sure we've read
		 * all entries that are available.
		 */
		if (index_in == kcqv->skc_out) {

			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w, ((kcqv->skc_out << 24) |
			    (USOC_CSR_USOC_TO_HOST & ~USOC_CSR_RSP_QUE_0)));
			/* Make sure the csr write has completed */
			i = USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w);
			DEBUGF(9, (CE_CONT, "csr.w = %x\n", i));

			/*
			 * Update our idea of where the host adapter has placed
			 * the most recent entry in the response queue and
			 * resync the response queue
			 */
			index_in = USOC_RESPONSEQ_INDEX(srq,
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_rspp.w));
		}

		/*
		 * Check for access handle failures. If error occured, then
		 * some of the above ddi_get()/ddi_put() would have
		 * resulted in invalid data
		 */
		if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "solicited interrupt.1:"
			    " rspq %x rp access handle fault", srq);

			break;
		}
	}

	/* Drop lock for request queue. */
	mutex_exit(&kcq->skc_mtx);

	return (retval);
}


/*
 * Function name : usoc_intr_unsolicited()
 *
 * Return Values : none
 *
 * Description	 : Processes entries in the unsolicited response
 *		   queue
 *
 *	The SOC+ will give us an unsolicited response

 *	whenever its status changes: OFFLINE, ONLINE,
 *	or in response to a packet arriving from an originator.
 *
 *	When message requests come in they will be placed in our
 *	buffer queue or in the next "inline" packet by the SOC hardware.
 *
 * Context	: Unsolicited interrupts must be masked
 */
static int
usoc_intr_unsolicited(usoc_state_t *usocp, uint32_t urq)
{
	usoc_kcq_t		*kcq;
	volatile usoc_kcq_t	*kcqv;
	volatile cqe_t		*cqe;
	register uchar_t	t_index, t_seqno;
	register volatile usoc_reg_t *usocreg = usocp->usoc_rp;
	volatile cqe_t		*cqe_cont = NULL;
	uint_t			i;
	int			hdr_count;
	uchar_t			index_in;
	ddi_devstate_t		devstate;
	int			retval;

	/* Check if device is marked bad */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "unsolicited interrupt after device was"
		    " reported faulty");

		return (DDI_INTR_UNCLAIMED);
	}

	kcq = &usocp->usoc_response[urq];
	kcqv = (volatile usoc_kcq_t *)kcq;

	/*
	 * Grab lock for response queue.
	 */
	mutex_enter(&kcq->skc_mtx);

	/*
	 * Since SOC+ is big endian, access the cq entry directly.
	 * We have checked access handle using ddi calls (Driver
	 * Hardening) to check for bus faults.
	 */
	cqe = (volatile cqe_t *)&(kcq->skc_cq[kcqv->skc_out]);

	index_in = USOC_RESPONSEQ_INDEX(urq,
	    USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_rspp.w));

	/* Check for access handle failure */
	if (USOC_DDH_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle,
	    USOC_DDH_UNSOLINTR_ACCHDL_FAULT)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "unsolicited interrupt.1: rspq %x rp acchdl"
		    " fault", usocp->usoc_instance, urq);

		/* Drop lock for request queue. */
		mutex_exit(&kcq->skc_mtx);
		return (DDI_INTR_UNCLAIMED);
	}

	if (kcqv->skc_out != index_in) {
		retval = DDI_INTR_CLAIMED;
	} else {
		retval = DDI_INTR_UNCLAIMED;
	}

	while (kcqv->skc_out != index_in) {
		USOC_SYNC_FOR_KERNEL(kcq->skc_dhandle, 0, 0);

		/* Check for response queue access handle fault */
		if (USOC_ACCHDL_FAULT(usocp, kcq->skc_acchandle)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "unsolicited interrupt:"
			    " rspq %x skc_acchandle fault", urq);

			break;
		}

		/* Check for response queue dma transfer fault */
		if (USOC_DMAHDL_FAULT(usocp, kcq->skc_dhandle)) {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "unsolicited interrupt:"
			    " resq %x skc dmahandle fault", urq);

			break;
		}

		/* Check for continuation entries */
		if ((hdr_count = cqe->cqe_hdr.cq_hdr_count) != 1) {
			t_seqno = kcqv->skc_seqno;
			t_index = kcqv->skc_out + hdr_count;

			i = index_in;

			if (kcqv->skc_out > index_in) {
				i += kcq->skc_last_index + 1;
			}

			/*
			 * If we think the continuation entries haven't
			 * yet arrived, try once more before giving up
			 */

			if (i < t_index) {
				USOC_WR32(usocp->usoc_rp_acchandle,
				    &usocreg->usoc_csr.w,
				    ((kcqv->skc_out << 24) |
				    (USOC_CSR_USOC_TO_HOST &
				    ~USOC_CSR_RSP_QUE_1)));

				/* Make sure the csr write has completed */
				i = USOC_RD32(usocp->usoc_rp_acchandle,
				    &usocreg->usoc_csr.w);

				/*
				 * Update our idea of where the host adapter
				 * has placed the most recent entry in the
				 * response queue
				 */
				i = index_in = USOC_RESPONSEQ_INDEX(urq,
				    USOC_RD32(usocp->usoc_rp_acchandle,
				    &usocreg->usoc_rspp.w));

				/* check for access handle failure */
				if (USOC_ACCHDL_FAULT(usocp,
				    usocp->usoc_rp_acchandle)) {
					usoc_display(usocp, USOC_PORT_ANY,
					    CE_WARN, USOC_LOG_ONLY, NULL,
					    "unsolicited interrupt.2 rspq"
					    "  %x rp access handle fault", urq);

					break;
				}

				if (kcqv->skc_out > index_in) {
					i += kcq->skc_last_index + 1;
				}

				/*
				 * Exit if the continuation entries haven't yet
				 * arrived
				 */
				if (i < t_index) {
					break;
				}
			}

			if (t_index > kcq->skc_last_index) {
				t_seqno++;
				t_index &= kcq->skc_last_index;
			}

			/*
			 * SOC+ is big endian, so access the cq entry directly.
			 * We have checked access handle using ddi calls
			 * (Driver Hardening) to check for bus faults.
			 */
			cqe_cont = (volatile cqe_t *)&(kcq->skc_cq[t_index ?
			    t_index - 1 : kcq->skc_last_index]);

			/*
			 * A cq_hdr_count > 2 is illegal; throw
			 * away the response
			 */

			/*
			 * XXX - should probably throw out as many
			 * entries as the hdr_cout tells us there are
			 */
			if (hdr_count != 2) {
				usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
				    USOC_LOG_ONLY, NULL, "too many"
				    " continuation entries");

				DEBUGF(4, (CE_CONT, "usoc%d: soc+ unsolicited"
				    " entry count = %d\n", usocp->usoc_instance,
				    cqe->cqe_hdr.cq_hdr_count));

				if ((++t_index & kcq->skc_last_index) == 0) {
					t_index = 0;
					t_seqno++;
				}

				kcqv->skc_out = t_index;
				kcqv->skc_seqno = t_seqno;

				cqe = &(kcq->skc_cq[kcqv->skc_out]);
				cqe_cont = NULL;
				continue;
			}
		}

		/*
		 * Update unsolicited response queue ptrs
		 */
		kcqv->skc_out++;
		if ((kcqv->skc_out & kcq->skc_last_index) == 0) {
			kcqv->skc_out = 0;
			kcqv->skc_seqno++;
		}

		if (cqe_cont != NULL) {
			kcqv->skc_out++;
			if ((kcqv->skc_out & kcq->skc_last_index) == 0) {
				kcqv->skc_out = 0;
				kcqv->skc_seqno++;
			}
		}

		/*
		 * XXX: Handle unsolicited interrupts as is for now - set
		 * usoc_handle_unsol_intr_new to 0 when things work and
		 * get rid of the old unsol interrupt handling when
		 * unsolicited buffers work.
		 */
		if (!usoc_use_unsol_intr_new) {
			usoc_handle_unsol_intr(usocp, kcq, cqe, cqe_cont);
		} else if (usoc_use_unsol_intr_new) {
			usoc_handle_unsol_intr_new(usocp, kcq, cqe, cqe_cont);
		}

		if (kcq->skc_cq == NULL) {
			/*
			 * This action averts a potential PANIC scenario
			 * where the SUSPEND code flow grabbed the kcq->skc_mtx
			 * when we let it go, to call our completion routine,
			 * and "initialized" the response queue.  We exit our
			 * processing loop here, thereby averting a PANIC due
			 * to a NULL de-reference from the response queue.
			 *
			 * Note that this is an interim measure that needs
			 * to be revisited when this driver is next revised
			 * for enhanced performance.
			 */
			break;
		}

		if (index_in == kcqv->skc_out) {
			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w, ((kcqv->skc_out << 24) |
			    (USOC_CSR_USOC_TO_HOST & ~USOC_CSR_RSP_QUE_1)));

			/* Make sure the csr write has completed */
			i = USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w);

			/* Check for access handle failure */
			if (USOC_ACCHDL_FAULT(usocp,
			    usocp->usoc_rp_acchandle)) {
				usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
				    USOC_LOG_ONLY, NULL,
				    "unsolicited interrupt.3, rspq %x rp"
				    " access handle fault", urq);

				break;
			}
		}
		/*
		 * We need to re-read the input and output pointers in
		 * case a polling routine should process some entries
		 * from the response queue while we're doing a callback
		 * routine with the response queue mutex dropped.
		 */
		cqe = &(kcq->skc_cq[kcqv->skc_out]);
		index_in = USOC_RESPONSEQ_INDEX(urq,
		    USOC_RD32(usocp->usoc_rp_acchandle, &usocreg->usoc_rspp.w));
		cqe_cont = NULL;

		/*
		 * Mess around with the hardware if we think we've run out
		 * of entries in the queue, just to make sure we've read
		 * all entries that are available.
		 */
		if (index_in == kcqv->skc_out) {
			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w, ((kcqv->skc_out << 24) |
			    (USOC_CSR_USOC_TO_HOST & ~USOC_CSR_RSP_QUE_1)));

			/* Make sure the csr write has completed */
			i = USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_csr.w);

			/*
			 * Update our idea of where the host adapter has placed
			 * the most recent entry in the response queue
			 */
			index_in = USOC_RESPONSEQ_INDEX(urq,
			    USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocreg->usoc_rspp.w));

			/* Check for access handle failure */
			if (USOC_ACCHDL_FAULT(usocp,
			    usocp->usoc_rp_acchandle)) {
				usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
				    USOC_LOG_ONLY, NULL, "unsolicited interrupt"
				    " rspq %x rp access handle fault",
				    urq);
				break;
			}
		}
	} /* end of while */

	/* Release lock for response queue */
	mutex_exit(&kcq->skc_mtx);

	return (retval);
}


static void
usoc_handle_unsol_intr(usoc_state_t *usocp, usoc_kcq_t *kcq,
    volatile cqe_t *cqe, volatile cqe_t *cqe_cont)
{
	usoc_port_t		*port_statep;
	usoc_response_t		*srp;
	int			port;
	uint32_t		cb_state;
	int			status;
	ushort_t		flags;
	fc_unsol_buf_t		*ubt = NULL;
	uint32_t		type;
	opaque_t 		tran_handle;
	void 			(*unsol_cb) (opaque_t port_handle,
				    fc_unsol_buf_t *buf, uint32_t type);

	srp = (usoc_response_t *)cqe;
	flags = srp->sr_usoc_hdr.sh_flags;
	port = (flags & USOC_PORT_B) ? 1 : 0;
	port_statep = &usocp->usoc_port_state[port];

	switch (flags & ~USOC_PORT_B) {
	case USOC_UNSOLICITED | USOC_FC_HEADER:
		mutex_enter(&port_statep->sp_mtx);
		if ((port_statep->sp_status & PORT_BOUND) == 0) {
			mutex_exit(&port_statep->sp_mtx);
			break;
		}
		mutex_exit(&port_statep->sp_mtx);

		switch (srp->sr_fc_frame_hdr.r_ctl & R_CTL_ROUTING) {
		case R_CTL_EXTENDED_SVC:
		case R_CTL_DEVICE_DATA:
			/* examine inbound ELS frames */
			ubt = usoc_ub_temp(port_statep, (cqe_t *)cqe,
			    (caddr_t)cqe_cont);

			type = srp->sr_fc_frame_hdr.r_ctl << 8;
			type |= srp->sr_fc_frame_hdr.type;

			if (ubt == NULL) {
				type |= 0x80000000;
			}

			/* do callbacks the transport */
			mutex_exit(&kcq->skc_mtx);

			if ((cqe_cont == NULL) ||
			    ((((usoc_unsol_resp_t *)cqe)->
			    unsol_resp_usoc_hdr.sh_byte_cnt) == 0)) {
				usoc_take_core(usocp, USOC_CORE_ON_BAD_UNSOL);
			}

			mutex_enter(&port_statep->sp_mtx);

			if (port_statep->sp_unsol_callb) {
				tran_handle = port_statep->sp_tran_handle;
				unsol_cb = port_statep->sp_unsol_callb;

				mutex_exit(&port_statep->sp_mtx);
				(*unsol_cb) (tran_handle, ubt, type);
			} else {
				mutex_exit(&port_statep->sp_mtx);
			}

			mutex_enter(&kcq->skc_mtx);
			break;

		case R_CTL_BASIC_SVC:
			usoc_display(usocp, USOC_PORT_ANY, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "unsupported Link Service"
			    " command: 0x%x", srp->sr_fc_frame_hdr.type);
			break;

		default:
			usoc_display(usocp, USOC_PORT_ANY, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "unsupported FC frame"
			    " R_CTL: 0x%x", srp->sr_fc_frame_hdr.r_ctl);
			break;
		}
		break;

	case USOC_STATUS: {
		/*
		 * Note that only the lsbyte of the status
		 * has interesting information
		 */
		status = srp->sr_usoc_status;
		switch (status) {
		case USOC_ONLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel is ONLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status &= ~PORT_IN_LINK_RESET;
			port_statep->sp_status |= PORT_ONLINE;
			port_statep->sp_src_id = 0;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_ONLINE;
			usocp->usoc_stats.pstats[port].onlines++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol:"
			    " ONLINE intr\n", usocp->usoc_instance));

			break;

		case USOC_LOOP_ONLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel Loop is ONLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status |= PORT_ONLINE_LOOP;
			port_statep->sp_status &= ~PORT_IN_LINK_RESET;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_LOOP;
			usocp->usoc_stats.pstats[port].online_loops++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol: "
			    "ONLINE-LOOP intr\n", usocp->usoc_instance));
			break;

		case USOC_OFFLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel is OFFLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status |= PORT_OFFLINE;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_OFFLINE;

			usocp->usoc_stats.pstats[port].offlines++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol:"
			    " OFFLINE intr\n", usocp->usoc_instance));
			break;

		default:
			cb_state = FC_STATE_OFFLINE;
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "unsolicited interrupt: unknown status: 0x%x",
			    status);
			break;
		}
		mutex_exit(&kcq->skc_mtx);

		if (port_statep->sp_statec_callb) {
			port_statep->sp_statec_callb(
			    port_statep->sp_tran_handle, cb_state);
		}

		if (cb_state == FC_STATE_OFFLINE) {
			usoc_flush_of_queues(port_statep);
		}

		mutex_enter(&kcq->skc_mtx);
		break;

	}
	default:
		usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
		    "unsolicited interrupt: unexpected state: flags: 0x%x",
		    flags);

		DEBUGF(4, (CE_CONT, "\tusoc CR: %x SAE: %x CSR:"
		    " %x IMR: %x\n",
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_cr.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_sae.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_csr.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_imr)));
		break;
	}
}


/*
 * usoc_us_els() - This function handles unsolicited extended link
 * service responses received from the soc. Since the unsolicited responses
 * with and without unsolicited buffer implementation are nearly the
 * same (atleast the offsets of the members we access in this routine are
 * exactly the same) - we can use the new usoc_unsol_resp_t structure here.
 */
static fc_unsol_buf_t *
usoc_ub_temp(usoc_port_t *port_statep, cqe_t *cqe, caddr_t payload)
{
	usoc_unsol_resp_t	*urp = (usoc_unsol_resp_t *)cqe;
	fc_unsol_buf_t		*ubt;
	uint32_t		i;
	usoc_ub_priv_t		*ubtp;
	usoc_state_t		*usocp = port_statep->sp_board;

	/*
	 * There should be a CQE continuation entry for all
	 * extended link services
	 */
	if ((payload == NULL) ||
	    ((i = urp->unsol_resp_usoc_hdr.sh_byte_cnt) == 0)) {
		usoc_display(usocp, port_statep->sp_port, CE_WARN,
		    USOC_LOG_ONLY, NULL, "incomplete continuation entry");

		usoc_display(usocp, port_statep->sp_port, CE_WARN,
		    USOC_LOG_ONLY, NULL, "cqe %p payload %p",
		    (void *)cqe, (void *)payload);

		usoc_display(usocp, port_statep->sp_port, CE_NOTE,
		    USOC_LOG_ONLY, NULL, "entry.1 %x %x %x %x %x %x %x %x",
		    ((uint32_t *)cqe)[0], ((uint32_t *)cqe)[1],
		    ((uint32_t *)cqe)[2], ((uint32_t *)cqe)[3],
		    ((uint32_t *)cqe)[4], ((uint32_t *)cqe)[5],
		    ((uint32_t *)cqe)[6], ((uint32_t *)cqe)[7]);

		usoc_display(usocp, port_statep->sp_port, CE_NOTE,
		    USOC_LOG_ONLY, NULL, "entry.2 %x %x %x %x %x %x %x %x",
		    ((uint32_t *)cqe)[8], ((uint32_t *)cqe)[9],
		    ((uint32_t *)cqe)[10], ((uint32_t *)cqe)[11],
		    ((uint32_t *)cqe)[12], ((uint32_t *)cqe)[13],
		    ((uint32_t *)cqe)[14], ((uint32_t *)cqe)[15]);

		if (payload != NULL) {
			uint32_t *p;
			p = (uint32_t *)payload;

			usoc_display(usocp, port_statep->sp_port, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "payload.1 %x %x %x %x %x"
			    " %x %x %x", p[0], p[1], p[2], p[3], p[4], p[5],
			    p[6], p[7]);

			usoc_display(usocp, port_statep->sp_port, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "payload.2 %x %x %x %x %x"
			    " %x %x %x", p[8], p[9], p[10], p[11], p[12],
			    p[13], p[14], p[15]);
		}

		return (NULL);
	}

	/* Quietly impose a maximum byte count */
	if (i > USOC_CQE_PAYLOAD) {
		i = USOC_CQE_PAYLOAD;
	}

	if ((ubt = kmem_zalloc(sizeof (fc_unsol_buf_t), KM_NOSLEEP)) == NULL) {
		return (NULL);
	}

	if ((ubt->ub_fca_private = kmem_zalloc(sizeof (usoc_ub_priv_t),
	    KM_NOSLEEP)) == NULL) {
		kmem_free(ubt, sizeof (fc_unsol_buf_t));
		return (NULL);
	}

	ubtp = (usoc_ub_priv_t *)ubt->ub_fca_private;

	/*
	 * Since the response struct returned to us on INLINE cqe
	 * entries is different from the unsolicited interrupts for
	 * for non INLINE entries - we need to duplicate code here
	 */
	switch (urp->unsol_resp_usoc_hdr.sh_class) {
	case 1:
		ubt->ub_class = FC_TRAN_CLASS1;
		break;

	case 2:
		ubt->ub_class = FC_TRAN_CLASS2;
		break;

	case 3:
		ubt->ub_class = FC_TRAN_CLASS3;
		break;

	default:
		ubt->ub_class = FC_TRAN_CLASS_INVALID;
		break;
	}

	if ((ubt->ub_buffer = kmem_zalloc((size_t)i, KM_NOSLEEP)) == NULL) {
		kmem_free(ubt, sizeof (fc_unsol_buf_t));
		kmem_free(ubt->ub_fca_private, sizeof (usoc_ub_priv_t));
		return (NULL);
	}

	ubt->ub_port_handle = (opaque_t)port_statep;
	ubt->ub_token = (uint64_t)ubt;
	usoc_wcopy((uint_t *)&urp->unsol_resp_fc_frame_hdr,
		(uint_t *)&ubt->ub_frame, sizeof (fc_frame_hdr_t));
	bcopy(payload, (caddr_t)ubt->ub_buffer, i);
	ubt->ub_bufsize = i;
	ubtp->ubp_portp = port_statep;
	ubtp->ubp_flags = UB_VALID | UB_TEMPORARY;

	return (ubt);
}


/*ARGSUSED*/
static fc_packet_t *
usoc_packet_alloc(usoc_port_t *portp, int sleep)
{
	int 			flag;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;

	flag = (sleep == 0) ? KM_NOSLEEP : KM_SLEEP;

	pkt = (fc_packet_t *)kmem_zalloc(sizeof (fc_packet_t), flag);

	if (pkt == (fc_packet_t *)NULL) {
		return (NULL);
	}

	pkt->pkt_fca_private = (opaque_t)kmem_zalloc(
	    sizeof (usoc_pkt_priv_t), flag);

	if (pkt->pkt_fca_private == (opaque_t)NULL) {
		kmem_free(pkt, sizeof (fc_packet_t));
		return (NULL);
	}

	if (usoc_init_pkt((opaque_t)portp, pkt, flag) != FC_SUCCESS) {
		kmem_free(pkt->pkt_fca_private, sizeof (usoc_pkt_priv_t));
		kmem_free(pkt, sizeof (fc_packet_t));
		return (NULL);
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	priv->spp_flags |= PACKET_INTERNAL_PACKET;

	return (pkt);
}


static void
usoc_packet_free(fc_packet_t *pkt)
{
	int		rval;
	usoc_pkt_priv_t	*priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_port_t 	*port_statep = (usoc_port_t *)priv->spp_portp;

	rval = usoc_uninit_pkt((opaque_t)port_statep, pkt);
	if (rval != FC_SUCCESS) {
		ASSERT(0);

		usoc_display(port_statep->sp_board, port_statep->sp_port,
		    CE_WARN, USOC_LOG_ONLY, NULL, "failed to uninit"
		    " packet: error=%d", rval);
	}

	kmem_free((void *)pkt->pkt_fca_private, sizeof (usoc_pkt_priv_t));
	kmem_free((void *)pkt, sizeof (fc_packet_t));
}


static int
usoc_doit(usoc_port_t *port_statep, fc_packet_t *pkt, int polled,
    uint32_t endflag, uint32_t *diagcode)
{
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	int status;

	ASSERT(pkt->pkt_timeout > 0);

	if (polled) {
		pkt->pkt_tran_flags = FC_TRAN_NO_INTR;
	} else {
		ASSERT(endflag != 0);

		pkt->pkt_tran_flags = FC_TRAN_INTR;
		pkt->pkt_comp = usoc_doneit;
	}

	priv->spp_endflag = endflag;

	mutex_enter(&port_statep->sp_mtx);
	port_statep->sp_status |= endflag;
	mutex_exit(&port_statep->sp_mtx);

#ifdef DEBUG
	usoc_take_core(port_statep->sp_board, USOC_CORE_ON_SEND_1);
#endif
	status = usoc_transport((opaque_t)port_statep, pkt);

#ifdef DEBUG
	usoc_take_core(port_statep->sp_board, USOC_CORE_ON_SEND_2);
#endif

	if ((status == FC_SUCCESS) && !polled) {
		mutex_enter(&port_statep->sp_mtx);

		while (port_statep->sp_status & endflag)
		    cv_wait(&port_statep->sp_cv, &port_statep->sp_mtx);

		mutex_exit(&port_statep->sp_mtx);

		if (diagcode) {
			*diagcode = priv->spp_diagcode;
		}
	}
	return (status);
}


static void
usoc_doneit(fc_packet_t *pkt)
{
	usoc_pkt_priv_t	*priv =
		(usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_port_t	*port_statep = priv->spp_portp;

	mutex_enter(&port_statep->sp_mtx);
	port_statep->sp_status &= ~priv->spp_endflag;
	cv_broadcast(&port_statep->sp_cv);
	mutex_exit(&port_statep->sp_mtx);
}


static int
usoc_diag_request(usoc_port_t *port_statep, uint_t *diagcode,
    uint32_t cmd)
{
	fc_packet_t		*pkt;
	usoc_diag_request_t	*sdr;
	usoc_pkt_priv_t	*priv;
	fc_lilpmap_t		map;
	int			retval;

	if (usoc_getmap2(port_statep, (caddr_t)&map, FKIOCTL) == FC_SUCCESS) {
		if (map.lilp_length != 1 && ((port_statep->sp_status &
		    PORT_ONLINE_LOOP) && cmd != USOC_DIAG_REM_LOOP))
		return (FC_FAILURE);
	}
	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL)
		return (FC_NOMEM);

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	sdr = (usoc_diag_request_t *)&priv->spp_sr;
	if (port_statep->sp_port)
	    sdr->sdr_usoc_hdr.sh_flags = USOC_PORT_B;
	sdr->sdr_diag_cmd = cmd;
	sdr->sdr_cqhdr.cq_hdr_count = 1;
	sdr->sdr_cqhdr.cq_hdr_type = CQ_TYPE_DIAGNOSTIC;

	pkt->pkt_timeout = USOC_DIAG_TIMEOUT;

	retval = usoc_doit(port_statep, pkt, 0, PORT_DIAG_PENDING, diagcode);

	usoc_packet_free(pkt);

	return (retval);
}


static int
usoc_force_offline(usoc_port_t *port_statep, uint_t polled)
{
	fc_packet_t		*pkt;
	usoc_pkt_priv_t	*priv;
	usoc_cmdonly_request_t	*scr;
	int			retval;

	if ((pkt = usoc_packet_alloc(port_statep, polled))
	    == (fc_packet_t *)NULL)
		return (FC_NOMEM);

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	scr = (usoc_cmdonly_request_t *)&priv->spp_sr;
	if (port_statep->sp_port)
	    scr->scr_usoc_hdr.sh_flags = USOC_PORT_B;
	scr->scr_cqhdr.cq_hdr_count = 1;
	scr->scr_cqhdr.cq_hdr_type = CQ_TYPE_OFFLINE;

	pkt->pkt_timeout = USOC_OFFLINE_TIMEOUT;

	retval = usoc_doit(port_statep, pkt, 0, PORT_OFFLINE_PENDING, NULL);

	usoc_packet_free(pkt);

	return (retval);
}


static int
usoc_getmap(opaque_t portp, fc_lilpmap_t *map)
{
	return (usoc_getmap2((usoc_port_t *)portp, (caddr_t)map, FKIOCTL));
}


static int
usoc_getmap2(usoc_port_t *port_statep, caddr_t arg, int flags)
{
	ddi_dma_cookie_t	dcookie;
	ddi_dma_handle_t	dhandle = NULL;
	ddi_acc_handle_t	acchandle;
	size_t			real_len, i;
	uint_t			ccount;
	fc_lilpmap_t		*buf = NULL;
	int			retval, bound = 0;
	usoc_state_t		*usocp = port_statep->sp_board;

	/* If no loop, don't go through the trouble */
	if (port_statep->sp_status & PORT_ONLINE) {
		return (FC_OLDPORT);
	}

	if (port_statep->sp_status & PORT_OFFLINE) {
		return (FC_OFFLINE);
	}

	if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		DDI_DMA_SLEEP, NULL, &dhandle) != DDI_SUCCESS)
		goto getmap_fail;

	i = sizeof (fc_lilpmap_t) + 1;

	if (ddi_dma_mem_alloc(dhandle, i, &usoc_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    (caddr_t *)&buf, &real_len, &acchandle) != DDI_SUCCESS)
		goto getmap_fail;

	if (real_len < i) {
		goto getmap_fail;
	}

	if (ddi_dma_addr_bind_handle(dhandle, (struct as *)NULL,
	    (caddr_t)buf, real_len, DDI_DMA_READ | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &dcookie, &ccount) != DDI_DMA_MAPPED)
		goto getmap_fail;

	bound = 1;

	if (ccount != 1) {
		goto getmap_fail;
	}

	retval = usoc_lilp_map((void *)port_statep, dcookie.dmac_address, 0);

	USOC_SYNC_FOR_KERNEL(dhandle, 0, 0);

	if (retval == FC_SUCCESS) {
		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_src_id = USOC_RD16(acchandle,
		    &buf->lilp_myalpa);
		mutex_exit(&port_statep->sp_mtx);

		if (arg) {
			if (ddi_copyout(buf, (caddr_t)arg,
			    sizeof (struct fc_lilpmap), flags) == -1) {
				return (FC_FAILURE);
			}
		}
		retval = FC_SUCCESS;
	} else {
		retval = FC_NO_MAP;
	}

	/*
	 * Check for acc/dma handle faults. If any fault occured, then above
	 * ddi_get()/ddi_copyouy() would have given corrupted data.
	 */
	if (USOC_ACC_HANDLE_BAD(acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "getmap2: bad access handle");

		goto getmap_fail;
	}

	if (USOC_DMA_HANDLE_BAD(dhandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "getmap2: bad dma handle");

		goto getmap_fail;
	}

	(void) ddi_dma_unbind_handle(dhandle);
	ddi_dma_mem_free(&acchandle);
	ddi_dma_free_handle(&dhandle);

	return (FC_SUCCESS);

getmap_fail:
	if (dhandle) {
		if (bound)
			(void) ddi_dma_unbind_handle(dhandle);
		ddi_dma_free_handle(&dhandle);
	}

	if (buf) {
		ddi_dma_mem_free(&acchandle);
	}

	return (FC_NO_MAP);
}


static int
usoc_getrls(usoc_port_t *port_statep, caddr_t arg, int flags)
{
	ddi_dma_cookie_t	dcookie;
	ddi_dma_handle_t	dhandle = NULL;
	ddi_acc_handle_t	acchandle;
	size_t			real_len, len;
	uint_t			ccount;
	fc_lilpmap_t		*buf = NULL;
	int			retval, bound = 0;
	usoc_state_t		*usocp = port_statep->sp_board;

	if (port_statep->sp_status & PORT_OFFLINE) {
		return (FC_OFFLINE);
	}

	if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
	    DDI_DMA_SLEEP, NULL, &dhandle) != DDI_SUCCESS) {
		goto getrls_fail;
	}

	len = sizeof (fc_rls_acc_t);

	if (ddi_dma_mem_alloc(dhandle, len, &usoc_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    (caddr_t *)&buf, &real_len, &acchandle) != DDI_SUCCESS) {
		goto getrls_fail;
	}

	if (real_len < len) {
		goto getrls_fail;
	}

	if (ddi_dma_addr_bind_handle(dhandle, (struct as *)NULL,
	    (caddr_t)buf, real_len, DDI_DMA_READ | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &dcookie, &ccount) != DDI_DMA_MAPPED) {
		goto getrls_fail;
	}

	bound = 1;
	if (ccount != 1) {
		goto getrls_fail;
	}

	retval = usoc_local_rls((void *)port_statep, dcookie.dmac_address, 0);
	if (retval == FC_SUCCESS) {
		if (arg) {
			if (ddi_copyout(buf, (caddr_t)arg, len, flags) == -1) {
				retval = FC_FAILURE;
			}
		}
	}

	(void) ddi_dma_unbind_handle(dhandle);
	ddi_dma_mem_free(&acchandle);
	ddi_dma_free_handle(&dhandle);

	return (retval);

getrls_fail:
	if (dhandle) {
		if (bound) {
			(void) ddi_dma_unbind_handle(dhandle);
		}

		if (buf) {
			ddi_dma_mem_free(&acchandle);
		}

		ddi_dma_free_handle(&dhandle);
	} else {
		ASSERT(buf == NULL && bound == 0);
	}

	return (FC_BADCMD);
}


static	void
usoc_wcopy(uint_t *h_src, uint_t *h_dest, int len)
{
	int	i;
	for (i = 0; i < len/4; i++) {
		*h_dest++ = *h_src++;
	}
}


static void
usoc_deferred_intr(void *arg)
{
	usoc_state_t	*usocp = (usoc_state_t *)arg;
	usoc_kcq_t		*kcq;

	kcq = &usocp->usoc_request[0];
	mutex_enter(&kcq->skc_mtx);
	if ((kcq->skc_out != kcq->skc_saved_out) ||
		(kcq->skc_seqno != kcq->skc_saved_seqno)) {
		kcq->deferred_intr_timeoutid = 0;
		mutex_exit(&kcq->skc_mtx);
		return;
	}
	kcq->deferred_intr_timeoutid = 0;
	mutex_exit(&kcq->skc_mtx);
	(void) usoc_intr_solicited(usocp, 0);
}


static int
usoc_dump_xram_buf(void *arg)
{
	usoc_state_t	*usocp = (usoc_state_t *)arg;
	int		i;
	uint32_t	j;
	ddi_devstate_t	devstate;

	/* Check if device is marked bad */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_NOTE, USOC_LOG_ONLY,
		    NULL, "take core: device was marked faulty");

		return (FC_FAILURE);
	}

	/* Get firmware core in usoc_xrambuf[] */
	for (i = 0; i < USOC_N_CQS; i++) {
		mutex_enter(&usocp->usoc_request[i].skc_mtx);
		mutex_enter(&usocp->usoc_response[i].skc_mtx);
	}
	for (i = 0; i < 4; i++) {
		j = USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_cr.w);
		j = (j & ~USOC_CR_EXTERNAL_RAM_BANK_MASK) | (i<<24);
		USOC_WR32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_cr.w, j);
		USOC_REP_RD32(usocp->usoc_xrp_acchandle,
		    &usoc_xrambuf[i*0x10000], usocp->usoc_xrp, 0x10000);
	}
	for (i = 0; i < USOC_N_CQS; i++) {
		mutex_exit(&usocp->usoc_request[i].skc_mtx);
		mutex_exit(&usocp->usoc_response[i].skc_mtx);
	}

	/*
	 * Check for access handle fault. If fault occured, then some
	 * of the above ddi_get()/ddi_put() routines would not have worked
	 * properly.
	 */
	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "take core. rp access handle fault");

		return (FC_FAILURE);
	}

	if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_xrp_acchandle)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "take core. xrp access handle fault");

		return (FC_FAILURE);
	}

	return (FC_SUCCESS);
}


static int
usoc_port_manage(opaque_t portp, fc_fca_pm_t *pm)
{
	int ret;
	usoc_port_t *port_statep = (usoc_port_t *)portp;

	if (!(port_statep->sp_status & PORT_BOUND)) {
		return (FC_UNBOUND);
	}
	ret = FC_SUCCESS;

	switch (pm->pm_cmd_code) {
	case FC_PORT_BYPASS:
	{
		uint_t *d_id;

		if (pm->pm_cmd_len != sizeof (*d_id)) {
			ret = FC_NOMEM;
			break;
		}
		d_id = (uint_t *)pm->pm_cmd_buf;
		ret = usoc_bypass_dev(port_statep, *d_id);
		break;
	}

	case FC_PORT_UNBYPASS:
		ret = FC_INVALID_REQUEST;
		break;

	case FC_PORT_DIAG:
	{
		uint_t		result;
		uint32_t 	*diag_code;

		if (pm->pm_cmd_len != sizeof (*diag_code)) {
			ret = FC_INVALID_REQUEST;
			break;
		}
		diag_code = (uint32_t *)pm->pm_cmd_buf;

		switch (*diag_code) {
		case USOCIO_DIAG_NOP:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_NOP);

			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_XRAM:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_XRAM_TEST);

			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_SOC:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_SOC_TEST);

			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_HCB:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_HCB_TEST);

			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_SOCLB:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_SOCLB_TEST);
			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_SRDSLB:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_SRDSLB_TEST);
			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_EXTLB:
			ret = usoc_diag_request(port_statep, &result,
			    USOC_DIAG_EXTOE_TEST);
			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;

		case USOCIO_DIAG_RAW:
		{
			uint_t 	*code;
			uint_t	result;

			if (pm->pm_data_len != sizeof (*code)) {
				ret = FC_INVALID_REQUEST;
				break;
			}
			code = (uint_t *)pm->pm_data_buf;
			ret = usoc_diag_request(port_statep,
			    &result, *code);

			if (pm->pm_stat_len == sizeof (result)) {
				*(int *)pm->pm_stat_buf = result;
			}
			break;
		}

		default:
			ret = FC_INVALID_REQUEST;
			break;
		}
		break;
	}

	case FC_PORT_GET_FW_REV:
	{
		int		i;
		uint32_t	j;
		int		value;
		caddr_t		src;
		usoc_state_t 	*usocp;
		ddi_devstate_t	devstate;

		usocp = port_statep->sp_board;

		if (pm->pm_data_len < 10) {
			ret = FC_NOMEM;
			break;
		}

		if (usocp->usoc_eeprom && !USOC_DEVICE_BAD(usocp, devstate)) {
			for (i = 0; i < USOC_N_CQS; i++) {
				mutex_enter(&usocp->usoc_request[i].skc_mtx);
				mutex_enter(&usocp->usoc_response[i].skc_mtx);
			}

			value = j = USOC_RD32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_cr.w);
			j = (j & ~USOC_CR_EEPROM_BANK_MASK) | (3 << 16);
			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_cr.w, j);
			src = usocp->usoc_eeprom + (unsigned)0xFFF6;
			USOC_REP_RD(usocp->usoc_eeprom_acchandle,
			    pm->pm_data_buf, src, 10);
			USOC_WR32(usocp->usoc_rp_acchandle,
			    &usocp->usoc_rp->usoc_cr.w, value);

			for (i = USOC_N_CQS - 1; i >= 0; i--) {
				mutex_exit(&usocp->usoc_response[i].skc_mtx);
				mutex_exit(&usocp->usoc_request[i].skc_mtx);
			}
		} else {
			ret = FC_FAILURE;
		}
		break;
	}

	case FC_PORT_GET_FCODE_REV:
	{
		int		len;
		caddr_t		buffer;
		usoc_state_t 	*board;

		board = port_statep->sp_board;
		if (ddi_prop_op(DDI_DEV_T_ANY, board->usoc_dip,
		    PROP_LEN_AND_VAL_ALLOC, DDI_PROP_DONTPASS |
		    DDI_PROP_CANSLEEP, "version", (caddr_t)&buffer,
		    &len) != DDI_SUCCESS) {
			ret = FC_FAILURE;
			break;
		}
		if (len > pm->pm_data_len) {
			kmem_free((caddr_t)buffer, len);
			ret = FC_NOMEM;
			break;
		}
		bcopy(buffer, pm->pm_data_buf, len);
		kmem_free(buffer, len);
		break;
	}

	case FC_PORT_GET_DUMP_SIZE:
	{
		uint32_t dump_size = 0x40000;

		if (pm->pm_data_len != sizeof (dump_size)) {
			ret = FC_NOMEM;
			break;
		}
		*(uint32_t *)pm->pm_data_buf = dump_size;
		break;
	}

	case FC_PORT_FORCE_DUMP:
		if (usoc_dump_xram_buf((void *)port_statep->sp_board) !=
		    FC_SUCCESS) {
			ret = FC_FAILURE;
		}
		break;

	case FC_PORT_GET_DUMP:
		if (pm->pm_data_len != 0x40000) {
			ret = FC_NOMEM;
			break;
		}
		bcopy((caddr_t)usoc_xrambuf, pm->pm_data_buf, 0x40000);

		mutex_enter(&usoc_global_lock);
		usoc_core_taken = 0;
		mutex_exit(&usoc_global_lock);
		break;

	case FC_PORT_LINK_STATE:
	{
		uint32_t *link_state;

		if (pm->pm_stat_len != sizeof (*link_state)) {
			ret = FC_NOMEM;
			break;
		}

		if (pm->pm_cmd_buf != NULL) {
			/*
			 * Can't look beyond the FCA port.
			 */
			ret = FC_INVALID_REQUEST;
			break;
		}
		link_state = (uint32_t *)pm->pm_stat_buf;

		if (port_statep->sp_status & PORT_OFFLINE) {
			*link_state = FC_STATE_OFFLINE;
		} else if (port_statep->sp_status & PORT_ONLINE_LOOP) {
			*link_state = FC_STATE_LOOP;
		} else if (port_statep->sp_status & PORT_ONLINE) {
			*link_state = FC_STATE_ONLINE;
		} else {
			*link_state = FC_STATE_OFFLINE;
		}
		break;
	}

	case FC_PORT_INITIALIZE:
		break;

	case FC_PORT_DOWNLOAD_FW:
	{
		usoc_state_t 	*usocp;

		if (pm->pm_data_len <= 0 || pm->pm_data_len > usoc_fw_len) {
			ret = FC_INVALID_REQUEST;
			break;
		}
		usocp = port_statep->sp_board;

		mutex_enter(&usocp->usoc_k_imr_mtx);
		usoc_disable(usocp);
		mutex_exit(&usocp->usoc_k_imr_mtx);
		bcopy(pm->pm_data_buf, usoc_ucode, pm->pm_data_len);

		usoc_display(usocp, USOC_PORT_ANY, CE_NOTE, USOC_LOG_ONLY,
		    NULL, "Downloading the microcode len=%lx", pm->pm_data_len);

		if (usoc_download_ucode(usocp, pm->pm_data_len) != FC_SUCCESS) {
			ret = FC_FAILURE;
		}

		if (usoc_force_reset(usocp, 1, 0) != FC_SUCCESS) {
			ret = FC_FAILURE;
		}

		break;
	}

	case FC_PORT_LOOPBACK:
		break;

	case FC_PORT_ERR_STATS:
	{
		if (pm->pm_data_len <= 0 || pm->pm_data_len <
		    sizeof (fc_rls_acc_t)) {
			ret = FC_NOMEM;
			break;
		}
		if (usoc_getrls(port_statep, (caddr_t)pm->pm_data_buf, FKIOCTL)
		    != FC_SUCCESS) {
			ret = FC_FAILURE;
		}

		break;
	}

	default:
		ret = FC_INVALID_REQUEST;
		break;
	}

	return (ret);
}


/* ARGSUSED */
static int
usoc_get_cap(opaque_t portp, char *cap, void *ptr)
{
	int 	rval;

	if (strcmp(cap, FC_CAP_UNSOL_BUF) == 0) {
		int	*num_bufs;

		num_bufs = (int *)ptr;
		*num_bufs = -1;
		rval = FC_CAP_FOUND;
	} else if (strcmp(cap, FC_CAP_POST_RESET_BEHAVIOR) == 0) {
		fc_reset_action_t	*action;

		action = (fc_reset_action_t *)ptr;
		*action = FC_RESET_RETURN_NONE;
		rval = FC_CAP_FOUND;
	} else {
		rval = FC_CAP_ERROR;
	}
	return (rval);
}


/* ARGSUSED */
static int
usoc_set_cap(opaque_t portp, char *cap, void *ptr)
{
	return (FC_CAP_ERROR);
}


static int
usoc_reset(opaque_t portp, uint32_t cmd)
{
	usoc_port_t 	*port_statep = (usoc_port_t *)portp;
	usoc_state_t	*usocp = port_statep->sp_board;
	ddi_devstate_t	devstate;

	if (!(port_statep->sp_status & PORT_BOUND))
		return (FC_UNBOUND);

	/* check if device is marked bad */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY,
		    NULL, "reset devstate %s(%d)", USOC_DEVICE_STATE(devstate),
		    devstate);

		return (FC_FAILURE);
	}

	switch (cmd) {
	case FC_FCA_LINK_RESET:
		return (usoc_reset_link(port_statep, 1));
		/* NOTREACHED */

	case FC_FCA_CORE:
	case FC_FCA_RESET_CORE:
		if (usoc_dump_xram_buf((void *)usocp) != FC_SUCCESS) {
			break;
		}
		if (cmd == FC_FCA_CORE)
			break;
		/* FALLTHROUGH */

	case FC_FCA_RESET:
		if (usoc_force_reset(usocp, 1, 0) == FC_SUCCESS) {
			return (FC_SUCCESS);
		}
		break;

	default:
		return (FC_FAILURE);
	}
	return (FC_FAILURE);
}


static void
usoc_update_pkt_state(fc_packet_t *pkt, uint_t status)
{
	int count;
	int len;

	ASSERT(status != USOC_TIMEOUT);

	len = sizeof (usoc_error_table) / sizeof (usoc_error_table[0]);
	for (count = 0; count < len; count++) {
		if (usoc_error_table[count].usoc_status == status) {
			pkt->pkt_state = usoc_error_table[count].pkt_state;
			pkt->pkt_reason = usoc_error_table[count].pkt_reason;
			return;
		}
	}
	pkt->pkt_state = FC_PKT_LOCAL_RJT;
	pkt->pkt_reason = FC_REASON_FCA_UNIQUE;
}


/*
 * Function name : usoc_report_fault()
 *
 * Return Values : always returns 1, so that it can be used appropriately in
 *                 the macro.
 *
 * Description	 : Report the usocp device as faulty and disable the usoc.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
usoc_report_fault(usoc_state_t *usocp)
{
	ddi_devstate_t	devstate;
	int		i;
	caddr_t		buf;
	usoc_port_t	*port_statep;

	mutex_enter(&usocp->usoc_fault_mtx);

	/* If device is already down, then no need to report fault again */
	if (USOC_DEVICE_BAD(usocp, devstate)) {
		mutex_exit(&usocp->usoc_fault_mtx);

		usoc_display(usocp, USOC_PORT_ANY, CE_NOTE, USOC_LOG_ONLY,
		    NULL, "dev state is already %s(%d)",
		    USOC_DEVICE_STATE(devstate), devstate);

		return (1);
	}

	/*
	 * Disable USOC
	 */
	mutex_enter(&usocp->usoc_k_imr_mtx);
	usocp->usoc_shutdown = 1;
	usoc_disable(usocp);

	for (i = 0; i < N_USOC_NPORTS; i++) {
		port_statep = &usocp->usoc_port_state[i];
		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_status &= ~(PORT_STATUS_MASK |
		    PORT_LILP_PENDING | PORT_LIP_PENDING |
		    PORT_ABORT_PENDING | PORT_BYPASS_PENDING |
		    PORT_ELS_PENDING | PORT_OUTBOUND_PENDING | PORT_ONLINE);
		port_statep->sp_status |= PORT_OFFLINE;
		mutex_exit(&port_statep->sp_mtx);
	}
	mutex_exit(&usocp->usoc_k_imr_mtx);

#ifdef USOC_DRV_HARDENING
	/* Report fault */
	buf = kmem_zalloc(256, KM_NOSLEEP);
	if (buf) {
		(void) sprintf(buf, "usoc(%d): reporting fault",
		    usocp->usoc_instance);
		ddi_dev_report_fault(usocp->usoc_dip, DDI_SERVICE_LOST,
		    DDI_DATAPATH_FAULT, buf);

		kmem_free(buf, 256);
	} else {
		ddi_dev_report_fault(usocp->usoc_dip, DDI_SERVICE_LOST,
		    DDI_DATAPATH_FAULT, "usoc reporting fault");
	}

#endif /* USOC_DRV_HARDENING */

	mutex_exit(&usocp->usoc_fault_mtx);

	usoc_display(usocp, USOC_PORT_ANY, CE_WARN, USOC_LOG_ONLY, NULL,
	    "report fault DDI_SERVICE_LOST. new devstate %x",
	    USOC_GET_DEVSTATE(usocp));

	return (1);
}


static void
usoc_timeout(usoc_state_t *usocp, void (*tfunc) (fc_packet_t *),
    fc_packet_t *pkt)
{
	int		index;
	usoc_pkt_priv_t	*priv = pkt->pkt_fca_private;
	usoc_timetag_t 	*timep = &priv->spp_timetag;

#ifdef	DEBUG
	if (pkt->pkt_timeout == 0) {
		DEBUGF(4, (CE_CONT, "usoc%d: timeout is zero for %p",
		    usocp->usoc_instance, (void *)pkt));
	}
#endif /* DEBUG */

	ASSERT((pkt->pkt_tran_flags & FC_TRAN_NO_INTR) == 0);
	timep->sto_func = tfunc;
	timep->sto_prev = NULL;
	timep->sto_pkt = pkt;
	timep->sto_ticks = usocp->usoc_ticker + pkt->pkt_timeout;
	index = USOC_TIMELIST_HASH(timep->sto_ticks);

	/*
	 * stick the timetag on our timeout list
	 */
	mutex_enter(&usocp->usoc_time_mtx);

	timep->sto_next = usocp->usoc_timelist[index];

	if (usocp->usoc_timelist[index] != NULL) {
		usocp->usoc_timelist[index]->sto_prev = timep;
	}
	usocp->usoc_timelist[index] = timep;

	mutex_exit(&usocp->usoc_time_mtx);
}

static void
usoc_timeout_held(usoc_state_t *usocp, void (*tfunc) (fc_packet_t *),
    fc_packet_t *pkt)
{
	int		index;
	usoc_pkt_priv_t	*priv = pkt->pkt_fca_private;
	usoc_timetag_t 	*timep = &priv->spp_timetag;

#ifdef	DEBUG
	if (pkt->pkt_timeout == 0) {
		DEBUGF(4, (CE_CONT, "usoc%d: timeout is zero for %p",
		    usocp->usoc_instance, (void *)pkt));
	}
#endif /* DEBUG */

	ASSERT((pkt->pkt_tran_flags & FC_TRAN_NO_INTR) == 0);
	timep->sto_func = tfunc;
	timep->sto_prev = NULL;
	timep->sto_pkt = pkt;
	timep->sto_ticks = usocp->usoc_ticker + pkt->pkt_timeout;
	index = USOC_TIMELIST_HASH(timep->sto_ticks);

	/*
	 * stick the timetag on our timeout list
	 */
	ASSERT(MUTEX_HELD(&usocp->usoc_time_mtx));

	timep->sto_next = usocp->usoc_timelist[index];

	if (usocp->usoc_timelist[index] != NULL) {
		usocp->usoc_timelist[index]->sto_prev = timep;
	}
	usocp->usoc_timelist[index] = timep;
}

static void
usoc_untimeout_held(usoc_state_t *usocp, usoc_pkt_priv_t *priv)
{
	usoc_timetag_t 	*timep = &priv->spp_timetag;
	int		index = USOC_TIMELIST_HASH(timep->sto_ticks);

	ASSERT(MUTEX_HELD(&usocp->usoc_time_mtx));

	/*
	 * Take the timetag out of the timeout list
	 */
	if (timep->sto_prev != NULL) {
		timep->sto_prev->sto_next = timep->sto_next;
	} else {
		usocp->usoc_timelist[index] = timep->sto_next;
	}
	if (timep->sto_next != NULL) {
		timep->sto_next->sto_prev = timep->sto_prev;
	}

	timep->sto_func = NULL;
	timep->sto_prev = NULL;
	timep->sto_ticks = 0;
}


static void
usoc_untimeout(usoc_state_t *usocp, usoc_pkt_priv_t *priv)
{
	usoc_timetag_t 	*timep = &priv->spp_timetag;
	int		index = USOC_TIMELIST_HASH(timep->sto_ticks);

	/*
	 * Take the timetag out of the timeout list
	 */
	mutex_enter(&usocp->usoc_time_mtx);

	if (timep->sto_prev != NULL) {
		timep->sto_prev->sto_next = timep->sto_next;
	} else {
		usocp->usoc_timelist[index] = timep->sto_next;
	}
	if (timep->sto_next != NULL) {
		timep->sto_next->sto_prev = timep->sto_prev;
	}

	mutex_exit(&usocp->usoc_time_mtx);

	timep->sto_func = NULL;
	timep->sto_prev = NULL;
	timep->sto_ticks = 0;
}


static void
usoc_watchdog(void *arg)
{
	usoc_state_t	*usocp = (usoc_state_t *)arg;
	usoc_timetag_t	*timep, *tmptp;
	usoc_timetag_t	*list;
	int		index;
	void 		(*func) (void *);

	index = USOC_TIMELIST_HASH(usocp->usoc_ticker++);

	/*
	 * Perform Latent fault detection:
	 *	If board was inactive for a long time, then check for board
	 *	for proper operation.
	 */
	if ((usocp->usoc_ticker - usocp->usoc_alive_time) >
	    usoc_lfd_interval && !usocp->usoc_lfd_pending &&
	    !usocp->usoc_shutdown) {
		usoc_perform_lfd(usocp);
	}

	mutex_enter(&usocp->usoc_k_imr_mtx);
	if (usocp->usoc_shutdown) {
		mutex_exit(&usocp->usoc_k_imr_mtx);
		usoc_flush_all(usocp);
		goto reschedule_watchdog;
	}
	mutex_exit(&usocp->usoc_k_imr_mtx);

	mutex_enter(&usocp->usoc_time_mtx);
	for (list = NULL, timep = usocp->usoc_timelist[index]; timep;
	    timep = tmptp) {
		tmptp = timep->sto_next;

		if (timep->sto_func && timep->sto_ticks &&
		    (usocp->usoc_ticker > timep->sto_ticks)) {
			usoc_pkt_priv_t	*priv;

			priv = timep->sto_pkt->pkt_fca_private;
			ASSERT(priv != NULL);

			mutex_enter(&priv->spp_mtx);
			if ((priv->spp_flags & (PACKET_IN_INTR |
			    PACKET_DORMANT)) || (priv->spp_flags &
			    PACKET_IN_TRANSPORT && !(priv->spp_flags &
			    (PACKET_IN_OVERFLOW_Q | PACKET_IN_USOC))) ||
			    ((priv->spp_portp->sp_status & (PORT_OFFLINE |
			    PORT_IN_LINK_RESET)))) {
				/*
				 * Do not scan packets that are either not
				 * transported or packets that just completed
				 * If the port is OFFLINE, the commands need
				 * not be monitored as they get returned
				 * anyways.
				 */
				mutex_exit(&priv->spp_mtx);
				continue;
			}

#ifdef	DEBUG
			if (priv->spp_flags & PACKET_IN_OVERFLOW_Q) {
				usoc_of_timeouts++;
			}
#endif /* DEBUG */
			priv->spp_flags |= PACKET_IN_TIMEOUT;
			mutex_exit(&priv->spp_mtx);

			timep->sto_tonext = list;
			list = timep;
		}
	}
	mutex_exit(&usocp->usoc_time_mtx);

	for (timep = list; timep != NULL; timep = tmptp) {
		tmptp = timep->sto_tonext;

		ASSERT(timep->sto_pkt != NULL);
		ASSERT(timep->sto_pkt->pkt_fca_private != NULL);
#ifdef	DEBUG
		{
			usoc_pkt_priv_t	*priv;

			priv = timep->sto_pkt->pkt_fca_private;

			mutex_enter(&priv->spp_mtx);
			ASSERT(priv->spp_flags & PACKET_IN_TIMEOUT);
			mutex_exit(&priv->spp_mtx);
		}

		DEBUGF(2, (CE_CONT, "usoc%d: timeout D_ID=%x; pkt_ticks=%x"
		    " usoc ticks=%x, pkt=%p\n", usocp->usoc_instance,
		    timep->sto_pkt->pkt_cmd_fhdr.d_id, timep->sto_ticks,
		    usocp->usoc_ticker, (void *)(timep->sto_pkt)));

#endif /* DEBUG */

		func = (void (*) (void *))timep->sto_func;

		usoc_untimeout(usocp, timep->sto_pkt->pkt_fca_private);

		timep->sto_tonext = NULL;

		if (taskq_dispatch(usocp->usoc_task_handle, func,
		    timep->sto_pkt, KM_NOSLEEP) == 0) {
			func(timep->sto_pkt);
		}
	}

reschedule_watchdog:
	usocp->usoc_watch_tid = timeout(usoc_watchdog, (void *)usocp,
	    usoc_period);
}


static void
usoc_add_overflow_Q(usoc_kcq_t *kcq, usoc_pkt_priv_t *priv)
{
	ASSERT(MUTEX_HELD(&kcq->skc_mtx));

	if (kcq->skc_overflowh) {
		ASSERT(kcq->skc_overflowt != NULL);

		kcq->skc_overflowt->spp_next = priv;
	} else {
		ASSERT(kcq->skc_overflowt == NULL);

		kcq->skc_overflowh = priv;
	}
	kcq->skc_overflowt = priv;

	mutex_enter(&priv->spp_mtx);
	priv->spp_next = NULL;
	priv->spp_flags |= PACKET_IN_OVERFLOW_Q;
	mutex_exit(&priv->spp_mtx);

#ifdef	DEBUG
	{
		usoc_port_t	*port_statep = priv->spp_portp;

		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_of_ncmds++;
		mutex_exit(&port_statep->sp_mtx);
	}
#endif /* DEBUG */
}


static int
usoc_remove_overflow_Q(usoc_state_t *usocp, fc_packet_t *pkt, int index)
{
	int		rval;
	usoc_pkt_priv_t	*last;
	usoc_pkt_priv_t	*cur;
	usoc_kcq_t 	*kcq = &usocp->usoc_request[index];

	rval = FC_FAILURE;

	mutex_enter(&kcq->skc_mtx);
	last = NULL;
	cur = kcq->skc_overflowh;
	while (cur != NULL) {
		if (cur == (usoc_pkt_priv_t *)pkt->pkt_fca_private) {
			mutex_enter(&cur->spp_mtx);
			if (last) {
				last->spp_next = cur->spp_next;
			} else {
				ASSERT(kcq->skc_overflowh == cur);

				kcq->skc_overflowh = cur->spp_next;
			}

			if (cur == kcq->skc_overflowt) {
				ASSERT(cur->spp_next == NULL);

				kcq->skc_overflowt = last;
			}

			cur->spp_next = NULL;
			cur->spp_flags &= ~PACKET_IN_OVERFLOW_Q;
			mutex_exit(&cur->spp_mtx);

			rval = FC_SUCCESS;
			break;
		}
		last = cur;
		cur = cur->spp_next;
	}
	mutex_exit(&kcq->skc_mtx);

#ifdef	DEBUG
	if (cur != NULL) {
		usoc_port_t	*port_statep = cur->spp_portp;

		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_of_ncmds--;
		mutex_exit(&port_statep->sp_mtx);
	}
#endif /* DEBUG */

	return (rval);
}


static void
usoc_check_overflow_Q(usoc_state_t *usocp, int qindex, uint32_t imr_mask)
{
	char		full;
	usoc_kcq_t 	*kcq = &usocp->usoc_request[qindex];
	usoc_pkt_priv_t	*priv;
	usoc_pkt_priv_t	*npriv;
	usoc_pkt_priv_t	*ppriv;

	mutex_enter(&kcq->skc_mtx);
	full = kcq->skc_full;
	kcq->skc_full = 0;

	npriv = kcq->skc_overflowh;
	ppriv = NULL;

	while ((priv = npriv) != NULL) {
		npriv = priv->spp_next;

		mutex_enter(&priv->spp_mtx);
		if (priv->spp_portp->sp_status & (PORT_OFFLINE |
		    PORT_IN_LINK_RESET)) {
			mutex_exit(&priv->spp_mtx);
			break;
		}

		if (priv->spp_flags & (PACKET_IN_TIMEOUT |
		    PACKET_INTERNAL_ABORT | PACKET_IN_ABORT)) {
			/*
			 * About to be timed out. Don't
			 * bother to transport it.
			 */
			mutex_exit(&priv->spp_mtx);
			ppriv = priv;
			continue;
		}

		priv->spp_flags &= ~PACKET_IN_OVERFLOW_Q;
		priv->spp_next = NULL;
		mutex_exit(&priv->spp_mtx);

		if (usoc_cq_enque(usocp, priv->spp_portp,
		    (cqe_t *)&priv->spp_sr, qindex, NULL,
		    priv->spp_packet, 1) != FC_SUCCESS) {
			mutex_enter(&priv->spp_mtx);
			priv->spp_flags |= PACKET_IN_OVERFLOW_Q;
			priv->spp_next = npriv;
			mutex_exit(&priv->spp_mtx);
			break;
		}

		if (ppriv != NULL) {
			ppriv->spp_next = npriv;
		} else {
			ASSERT(priv == kcq->skc_overflowh);
			kcq->skc_overflowh = npriv;
		}

		if (priv == kcq->skc_overflowt) {
			kcq->skc_overflowt = ppriv;
		}
#ifdef	DEBUG
		{
			usoc_port_t	*port_statep = priv->spp_portp;

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_of_ncmds--;
			mutex_exit(&port_statep->sp_mtx);
		}
#endif /* DEBUG */

		/* don't try to flood when throttling */
		if (!full) {
			break;
		}
	}

	if (kcq->skc_overflowh == NULL) {
		kcq->skc_overflowt = NULL;
	}

	if (full && kcq->skc_full == 0 && imr_mask) {
		/* Disable this queue's intrs */
		DEBUGF(2, (CE_CONT, "usoc%d: req queue %d "
		    "overflow cleared\n", usocp->usoc_instance, qindex));

		mutex_enter(&usocp->usoc_k_imr_mtx);
		usocp->usoc_k_imr &= ~imr_mask;
		USOC_WR32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_imr, usocp->usoc_k_imr);
		mutex_exit(&usocp->usoc_k_imr_mtx);
	}

	mutex_exit(&kcq->skc_mtx);
}


static void
usoc_flush_overflow_Q(usoc_state_t *usocp, int port, int qindex)
{
	usoc_kcq_t		*kcq;
	usoc_port_t 		*port_statep;
	usoc_pkt_priv_t		*cur;
	usoc_pkt_priv_t		*next;
	usoc_pkt_priv_t		*prev;
	usoc_pkt_priv_t		*flush_head;

	kcq = &usocp->usoc_request[qindex];

	mutex_enter(&kcq->skc_mtx);

	flush_head = prev = NULL;
	cur = kcq->skc_overflowh;
	while (cur != NULL) {
		port_statep = cur->spp_portp;
		next = cur->spp_next;

		mutex_enter(&cur->spp_mtx);

		ASSERT(cur->spp_flags & PACKET_IN_OVERFLOW_Q);

		if (((cur->spp_flags & (PACKET_IN_TIMEOUT |
		    PACKET_INTERNAL_ABORT | PACKET_IN_ABORT)) == 0) &&
		    port_statep->sp_port == port) {
			cur->spp_flags &= ~PACKET_IN_PROCESS;
			cur->spp_flags |= PACKET_DORMANT;
			cur->spp_flags &= ~PACKET_IN_OVERFLOW_Q;
			mutex_exit(&cur->spp_mtx);

			/* Remove the packet from the timeout list */
			if (!(cur->spp_packet->pkt_tran_flags &
			    FC_TRAN_NO_INTR)) {
				usoc_untimeout(usocp, cur);
			}

			if (prev) {
				prev->spp_next = cur->spp_next;
			} else {
				ASSERT(cur == kcq->skc_overflowh);
				kcq->skc_overflowh = cur->spp_next;
			}

			if (kcq->skc_overflowt == cur) {
				ASSERT(cur->spp_next == NULL);
				kcq->skc_overflowt = prev;
			}

			mutex_enter(&port_statep->sp_mtx);
#ifdef	DEBUG
			port_statep->sp_of_ncmds--;
#endif /* DEBUG */

			if (flush_head) {
				cur->spp_next = flush_head;
			} else {
				cur->spp_next = NULL;
			}
			flush_head = cur;
			mutex_exit(&port_statep->sp_mtx);

		} else {
			mutex_exit(&cur->spp_mtx);
			prev = cur;
		}
		cur = next;
	}
	kcq->skc_full = 0;

#ifdef	DEBUG
	if (kcq->skc_overflowh == NULL) {
		ASSERT(kcq->skc_overflowt == NULL);
	}

	if (kcq->skc_overflowt == NULL) {
		ASSERT(kcq->skc_overflowh == NULL);
	}
#endif /* DEBUG */

	mutex_exit(&kcq->skc_mtx);

	port_statep = &usocp->usoc_port_state[port];
	mutex_enter(&port_statep->sp_mtx);
	if (flush_head) {
		int 		do_callback;
		fc_packet_t	*pkt;

		while ((next = flush_head) != NULL) {
			pkt = next->spp_packet;

			mutex_enter(&next->spp_mtx);
			if (USOC_PKT_COMP(pkt, next)) {
				do_callback = 1;
			} else {
				next->spp_flags &= ~PACKET_NO_CALLBACK;
				do_callback = 0;
			}
			flush_head = next->spp_next;
			next->spp_next = NULL;
			mutex_exit(&next->spp_mtx);

			pkt->pkt_state = FC_PKT_PORT_OFFLINE;
			pkt->pkt_reason = FC_REASON_OFFLINE;

			ASSERT((next->spp_flags & ~(PACKET_DORMANT |
			    PACKET_INTERNAL_PACKET | PACKET_VALID |
			    PACKET_NO_CALLBACK)) == 0);

			if (do_callback) {
				mutex_exit(&port_statep->sp_mtx);
				pkt->pkt_comp(pkt);
				mutex_enter(&port_statep->sp_mtx);
			}
		}
	}

	mutex_exit(&port_statep->sp_mtx);
}


static void
usoc_monitor(void *arg)
{
	int i, j;
	usoc_state_t *usocp = (usoc_state_t *)arg;

	for (i = USOC_CSR_1ST_H_TO_S, j = 0; j < 3; j++, i <<= 1) {
		usoc_check_overflow_Q(usocp, j, i);
	}
}


static void
usoc_flush_of_queues(usoc_port_t *port_statep)
{
	usoc_flush_overflow_Q(port_statep->sp_board,
	    port_statep->sp_port, CQ_REQUEST_0);
	usoc_flush_overflow_Q(port_statep->sp_board,
	    port_statep->sp_port, CQ_REQUEST_1);
	usoc_flush_overflow_Q(port_statep->sp_board,
	    port_statep->sp_port, CQ_REQUEST_2);
}


static int
usoc_reset_link(usoc_port_t *port_statep, int polled)
{
	int 		ret;
	usoc_state_t	*usocp = port_statep->sp_board;

	mutex_enter(&usocp->usoc_k_imr_mtx);
	if (usocp->usoc_shutdown) {
		mutex_exit(&usocp->usoc_k_imr_mtx);
		return (FC_SUCCESS);
	}
	mutex_exit(&usocp->usoc_k_imr_mtx);

	mutex_enter(&port_statep->sp_mtx);
	if (port_statep->sp_status & PORT_IN_LINK_RESET) {
		mutex_exit(&port_statep->sp_mtx);
		return (FC_SUCCESS);
	}
	port_statep->sp_status |= PORT_IN_LINK_RESET;
	mutex_exit(&port_statep->sp_mtx);

	if ((port_statep->sp_status & PORT_STATUS_MASK) == PORT_ONLINE) {
		ret = usoc_force_offline(port_statep, polled);
	} else {
		ret = usoc_force_lip(port_statep, polled);
	}

	if (ret != FC_SUCCESS) {
		mutex_enter(&port_statep->sp_mtx);
		port_statep->sp_status &= ~PORT_IN_LINK_RESET;
		mutex_exit(&port_statep->sp_mtx);
	}

	return (ret);
}


static void
usoc_finish_xfer(fc_packet_t *pkt)
{
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	uint32_t	byte_cnt = priv->spp_sr.sr_usoc_hdr.sh_byte_cnt;

	switch (pkt->pkt_tran_type) {
	case FC_PKT_FCP_WRITE:
		if (pkt->pkt_datalen) {
			pkt->pkt_data_resid = byte_cnt;
		}

		DEBUGF(4, (CE_CONT, "usoc: byte_cnt on FCP"
		    " WRITE to D_ID=%x is %x; resp len=%x"
		    " data_len=%x\n", pkt->pkt_cmd_fhdr.d_id, byte_cnt,
		    pkt->pkt_rsplen, pkt->pkt_datalen));
		break;

	default:
		break;

#if	0
	case FC_PKT_FCP_READ:
		/*
		 * Works only for regular SCSI commands to disk;
		 * This required more thought in the microcode
		 */
		if (byte_cnt == 0) {
			break;	/* a SCSI check condition probably */
		}
		/* FALLTHROUGH */

		if (byte_cnt != pkt->pkt_datalen) {
			DEBUGF(4, (CE_CONT, "usoc: FCP read byte_cnt on"
			    " D_ID=%x is %x; resp len=%x data_len=%x\n",
			    pkt->pkt_cmd_fhdr.d_id, byte_cnt,
			    pkt->pkt_rsplen, pkt->pkt_datalen));
		}
		pkt->pkt_data_resid = pkt->pkt_datalen - byte_cnt;
		break;

#endif /* 0 */
	}

	if (pkt->pkt_rsplen) {
		USOC_SYNC_FOR_KERNEL(pkt->pkt_resp_dma, 0, 0);
	}

	if (pkt->pkt_datalen && pkt->pkt_tran_type == FC_PKT_FCP_READ) {
		USOC_SYNC_FOR_KERNEL(pkt->pkt_data_dma, 0, 0);
	}

	/* Check if cmd/resp/data transfers caused any dma faults */
	if (pkt->pkt_cmdlen && USOC_DMA_HANDLE_BAD(pkt->pkt_cmd_dma)) {
		pkt->pkt_state = FC_PKT_LOCAL_RJT;
		pkt->pkt_reason = FC_REASON_DMA_ERROR;
	}

	if (pkt->pkt_rsplen && USOC_DMA_HANDLE_BAD(pkt->pkt_resp_dma)) {
		pkt->pkt_state = FC_PKT_LOCAL_RJT;
		pkt->pkt_reason = FC_REASON_DMA_ERROR;
	}

	if (pkt->pkt_datalen && USOC_DDH_DMA_HANDLE_BAD(pkt->pkt_data_dma,
	    USOC_DDH_PKT_DATA_DMAHDL_FAULT)) {
		pkt->pkt_state = FC_PKT_LOCAL_RJT;
		pkt->pkt_reason = FC_REASON_DMA_ERROR;
	}
}


/* ARGSUSED */
static int
usoc_ub_alloc(opaque_t portp, uint64_t token[], uint32_t size,
    uint32_t *count, uint32_t type)
{
	usoc_add_pool_request_t	*prq = NULL;
	uint32_t		poolid;
	usoc_port_t		*port_statep = (usoc_port_t *)portp;
	usoc_state_t		*usocp = port_statep->sp_board;
	usoc_unsol_buf_t	*ubufp;
	usoc_unsol_buf_t	*new_bufp = NULL;
	usoc_unsol_buf_t	*prev_bufp = NULL;
	int			from_ioctl = 0;	/* kludge */
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;
	int			result = FC_FAILURE;

	/*
	 * hack for ioctl support
	 */
	if (type & FC_TYPE_IOCTL) {
		from_ioctl = 1;
		type &= ~(FC_TYPE_IOCTL);
	}

	/*
	 * Lets validate the request first
	 */
	if (type > 0xFFFF) {
		DEBUGF(7, (CE_WARN, "usoc%d: Invalid FC4 type 0x%x\n",
		    usocp->usoc_instance, type));
		result = FC_FAILURE;
		goto fail;
	}

	if (count && (*count > USOC_MAX_UBUFS)) {
		DEBUGF(7, (CE_WARN, "usoc%d: Too many buffers requested\n",
		    usocp->usoc_instance));
		result = FC_FAILURE;
		goto fail;
	}

	if (!from_ioctl && (token == NULL)) {
		DEBUGF(7, (CE_WARN, "usoc%d: token array NULL or too small\n",
		    usocp->usoc_instance));
		result = FC_FAILURE;
		goto fail;
	}

	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL) {
		DEBUGF(7, (CE_WARN, "usoc_ub_alloc: usoc%d: fc_packet"
		    "alloc failed\n", usocp->usoc_instance));
		result = FC_FAILURE;
		goto fail;
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	prq = (usoc_add_pool_request_t *)&priv->spp_sr;

	/*
	 * Allocate a unique pool id - use the FC4 type as the poolid
	 * for now. We fail any alloc requests for the same FC4 type
	 */
	poolid = USOC_GET_POOLID_FROM_TYPE((type & 0xFFFF));	/* paronia */

	/*
	 * Now that we have the poolid - walk the list of unsolicited
	 * buffers for this instance of usoc and flag any duplicate
	 * buffer pools of the same FC4 type.
	 */
	prev_bufp = NULL;

	mutex_enter(&port_statep->sp_mtx);
	ubufp = port_statep->usoc_unsol_buf;
	while (ubufp) {
		if (ubufp->pool_id == poolid) {
			mutex_exit(&port_statep->sp_mtx);

			DEBUGF(7, (CE_WARN, "usoc%d: buffer pool for FC4"
			    " 0x%x already exists\n", usocp->usoc_instance,
			    type));

			result = FC_FAILURE;
			goto fail;
		}
		prev_bufp = ubufp;
		ubufp = ubufp->pool_next;
	}
	mutex_exit(&port_statep->sp_mtx);

	/*
	 * we shouldn't have a unsol buffer pool for this FC4 type yet
	 */
	ASSERT(ubufp == NULL);

	new_bufp = kmem_zalloc(sizeof (usoc_unsol_buf_t), KM_SLEEP);

	new_bufp->pool_next = NULL;
	new_bufp->pool_id = poolid;
	new_bufp->pool_type = type;
	new_bufp->pool_buf_size = size;
	new_bufp->pool_dma_res_ptr = NULL;
	new_bufp->pool_fc_ubufs = NULL;		/* initted in usoc_add_ubuf */

	/*
	 * set pool nentries to 0 till we actually allocate the buffers
	 * in usoc_add_ubuf
	 */
	new_bufp->pool_nentries = 0;

	/*
	 * Try to create a pool Header mask with just the TYPE field
	 * in the FC Frame header.
	 */
	new_bufp->pool_hdr_mask = UAPR_MASK_TYPE;

	/*
	 * Fill in the Add Pool request structure.
	 */
	prq->uapr_usoc_hdr.sh_request_token = poolid;
	prq->uapr_usoc_hdr.sh_flags = USOC_FC_HEADER | USOC_UNSOLICITED;
	if (port_statep->sp_port) {
		prq->uapr_usoc_hdr.sh_flags |= USOC_PORT_B;
	}
	prq->uapr_usoc_hdr.sh_class = 3;
	prq->uapr_usoc_hdr.sh_seg_cnt = 1;
	prq->uapr_usoc_hdr.sh_byte_cnt = 0;

	prq->uapr_pool_id = poolid;
	prq->uapr_header_mask = UAPR_MASK_TYPE;
	prq->uapr_buf_size = size;

	prq->uapr_fc_frame_hdr.r_ctl = 0;
	prq->uapr_fc_frame_hdr.d_id = 0;
	prq->uapr_fc_frame_hdr.s_id = 0;
	prq->uapr_fc_frame_hdr.type = type;
	prq->uapr_fc_frame_hdr.f_ctl = 0;
	prq->uapr_fc_frame_hdr.seq_id = 0;
	prq->uapr_fc_frame_hdr.df_ctl = 0;
	prq->uapr_fc_frame_hdr.seq_cnt = 0;
	prq->uapr_fc_frame_hdr.ox_id = 0;
	prq->uapr_fc_frame_hdr.rx_id = 0;
	prq->uapr_fc_frame_hdr.ro = 0;

	prq->uapr_cqhdr.cq_hdr_count = 1;
	prq->uapr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_POOL;
	prq->uapr_cqhdr.cq_hdr_flags = 0;
	prq->uapr_cqhdr.cq_hdr_seqno = 0;

	pkt->pkt_timeout = USOC_PKT_TIMEOUT;
	result = usoc_doit(port_statep, pkt, 0, PORT_OUTBOUND_PENDING, NULL);

	/*
	 * XXX: usoc_doit should handle incrmenting (in usoc_transport) and
	 * decrementing (in usoc_intr_solicited) of usoc_ncmds. No need to
	 * increment the cmd count here.
	 */
	if (result != FC_SUCCESS) {
		DEBUGF(7, (CE_WARN,
		    "usoc%d: unable to transport add_pool req packet (%d)\n",
		    usocp->usoc_instance, result));
		result = FC_FAILURE;
		goto fail;
	}

	if (priv->spp_flags & PACKET_IN_PROCESS) {
		DEBUGF(7, (CE_WARN,
		    "usoc%d: usoc_ub_alloc :spp_flags has PACKET_IN_PROCESS\n",
		    usocp->usoc_instance));
		priv->spp_flags &= ~PACKET_IN_PROCESS;
	}

	/*
	 * Add the buffer pool to the list of buffer pools for this
	 * FCA driver instance
	 */

	mutex_enter(&port_statep->sp_mtx);
	if (prev_bufp == NULL) {
		/* This is the first ULP alloced buffer pool */
		port_statep->usoc_unsol_buf = new_bufp;
	} else {
		ASSERT(prev_bufp->pool_next == NULL);
		prev_bufp->pool_next = new_bufp;
	}
	mutex_exit(&port_statep->sp_mtx);

	/* XXX: Add this for ioctl support only For now succeed request */
	if (!from_ioctl) {
		/*
		 * Now add the requested number of buffers to the established
		 * pool
		 */
		if ((result = usoc_add_ubuf(port_statep, poolid, token, size,
		    count)) != FC_SUCCESS) {
			goto fail;
		}
	}

	if (pkt) {
		usoc_packet_free(pkt);
	}

	return (FC_SUCCESS);

fail:
	usoc_display(usocp, port_statep->sp_port, CE_WARN, USOC_LOG_ONLY, NULL,
	    "Add Buffer pool failed");

	mutex_enter(&port_statep->sp_mtx);
	if (new_bufp) {
		if (prev_bufp) {
			prev_bufp->pool_next = NULL;
		} else {
			port_statep->usoc_unsol_buf = NULL;
		}
		kmem_free(new_bufp, sizeof (usoc_unsol_buf_t));
	}
	mutex_exit(&port_statep->sp_mtx);

	if (pkt) {
		usoc_packet_free(pkt);
	}

	return (FC_FAILURE);
}


/* ARGSUSED */
static int
usoc_add_ubuf(usoc_port_t *portp, uint32_t poolid, uint64_t *token,
    uint32_t size, uint32_t *count)
{
	int			result = FC_SUCCESS;
	usoc_unsol_buf_t	*ubufp;
	usoc_state_t		*usocp = portp->sp_board;
	usoc_add_buf_request_t	*drq;
	size_t			real_len;
	uint_t			ccount;
	uint16_t		num_bufs;
	uint16_t		num_alloced = 0;
	int			num_dmasetup = 0;
	int			i, j;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;

	ASSERT(count != NULL);

	if (*count <= 0) {
		DEBUGF(7, (CE_WARN, "usoc%d: No buffers to allocate!!\n",
		    usocp->usoc_instance));
		return (FC_SUCCESS);
	}

	mutex_enter(&portp->sp_mtx);
	ubufp = portp->usoc_unsol_buf;
	while (ubufp != NULL) {
		if (ubufp->pool_id == poolid) {
			/* found */
			break;
		}
		ubufp = ubufp->pool_next;
	}

	if (ubufp == NULL) {
		/* Not found !! */
		DEBUGF(7, (CE_WARN, "usoc%d: Buf pool with poolid 0x%x"
		    " not found\n", usocp->usoc_instance, poolid));
		mutex_exit(&portp->sp_mtx);
		return (FC_FAILURE);
	}

	if (ubufp->pool_nentries != 0) {
		DEBUGF(7, (CE_WARN,
		    "usoc(%d): 0x%x bufs already allocated for poolid 0x%x\n",
		    usocp->usoc_instance, ubufp->pool_nentries, poolid));
		mutex_exit(&portp->sp_mtx);

		return (FC_FAILURE);
	}

	ubufp->pool_nentries = *count;
	ubufp->pool_dma_res_ptr = (struct pool_dma_res *)
	    kmem_zalloc((sizeof (struct pool_dma_res) *
	    ubufp->pool_nentries), KM_SLEEP);

	mutex_exit(&portp->sp_mtx);

	/*
	 * Now that we have established the buffer pool, add buffers
	 * to the allocated pool
	 */

	if ((pkt = usoc_packet_alloc(portp, 0)) == (fc_packet_t *)NULL) {
		DEBUGF(7, (CE_WARN, "usoc_add_ubuf: usoc%d: fc_packet"
		    " alloc failed\n", usocp->usoc_instance));
		result = FC_FAILURE;
		goto cleanup_addbuf;
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	drq = (usoc_add_buf_request_t *)&priv->spp_sr;

	for (i = 0; i <  *count; num_dmasetup = i, i++) {
		struct pool_dma_res *nbp = (ubufp->pool_dma_res_ptr + i);
		/* Allocate DVMA resources for the buffer pool */
		if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		    DDI_DMA_DONTWAIT, NULL,
		    &nbp->pool_dhandle) != DDI_SUCCESS) {
			result = FC_TOOMANY;
			break;
		}

		if (ddi_dma_mem_alloc(nbp->pool_dhandle, ubufp->pool_buf_size,
		    &usoc_acc_attr, DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		    NULL, (caddr_t *)&nbp->pool_buf, &real_len,
		    &nbp->pool_acchandle) != DDI_SUCCESS) {
			(void) ddi_dma_unbind_handle(nbp->pool_dhandle);
			ddi_dma_free_handle(&nbp->pool_dhandle);
			result = FC_TOOMANY;
			break;
		}

		if (real_len < ubufp->pool_buf_size) {
			(void) ddi_dma_unbind_handle(nbp->pool_dhandle);
			ddi_dma_free_handle(&nbp->pool_dhandle);
			ddi_dma_mem_free(&nbp->pool_acchandle);

			result = FC_TOOMANY;
			break;
		}

		if (ddi_dma_addr_bind_handle(nbp->pool_dhandle,
		    (struct as *)NULL, nbp->pool_buf, ubufp->pool_buf_size,
		    DDI_DMA_READ | DDI_DMA_CONSISTENT, DDI_DMA_DONTWAIT,
		    NULL, &nbp->pool_dcookie, &ccount) != DDI_DMA_MAPPED) {
			(void) ddi_dma_unbind_handle(nbp->pool_dhandle);
			ddi_dma_free_handle(&nbp->pool_dhandle);
			ddi_dma_mem_free(&nbp->pool_acchandle);
			result = FC_TOOMANY;
			break;
		}

		if (ccount != 1) {
			result = FC_FAILURE;
			ddi_dma_free_handle(&nbp->pool_dhandle);
			ddi_dma_mem_free(&nbp->pool_acchandle);
			break;
		}
	}

	if (i == *count)
		num_dmasetup = *count;

	/*
	 * check to see if we actually allocated something
	 */
	if (num_dmasetup != *count) {
		goto cleanup_addbuf;
	}

	/*
	 * Since we can only add NUM_USOC_BUFS buffers at a time....
	 */
	num_alloced = 0;
	for (i = 0; i < num_dmasetup; i += NUM_USOC_BUFS) {
		num_bufs = (((num_dmasetup - i) >= NUM_USOC_BUFS) ?
		    NUM_USOC_BUFS : (num_dmasetup - i));

		for (j = 0; j < num_bufs; j++) {
			struct pool_dma_res *nbp =
				(ubufp->pool_dma_res_ptr + i + j);
			drq->uabr_buf_descriptor[j].address =
				(uint32_t)nbp->pool_dcookie.dmac_address;
			drq->uabr_buf_descriptor[j].token = (i + j);
		}

		/*
		 * Fill in the request structure.
		 */
		drq->uabr_usoc_hdr.sh_request_token = poolid;
		drq->uabr_usoc_hdr.sh_flags = USOC_UNSOLICITED;
		if (portp->sp_port) {
			drq->uabr_usoc_hdr.sh_flags |= USOC_PORT_B;
		}
		drq->uabr_usoc_hdr.sh_class = 3;
		drq->uabr_usoc_hdr.sh_seg_cnt = 1;
		drq->uabr_usoc_hdr.sh_byte_cnt = 0;
		drq->uabr_pool_id = poolid;
		drq->uabr_nentries = num_bufs;
		drq->uabr_cqhdr.cq_hdr_count = 1;
		drq->uabr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_BUFFER;
		drq->uabr_cqhdr.cq_hdr_flags = 0;
		drq->uabr_cqhdr.cq_hdr_seqno = 0;

		pkt->pkt_timeout = USOC_PKT_TIMEOUT;
		result =
		    usoc_doit(portp, pkt, 0, PORT_OUTBOUND_PENDING, NULL);

		if (result != FC_SUCCESS) {
			DEBUGF(7, (CE_WARN, "usoc%d: unable to transport "
			    "add_buf req pkt (%d)\n", usocp->usoc_instance,
			    result));

			/*
			 * Cleanup any dma structures for which CQ entries
			 * could not be alloced.
			 */
			if (i > 0) {
				*count = (i + 1);
				num_alloced = (i + 1);
				result = FC_SUCCESS;
				goto cleanup_addbuf;
			} else {
				*count = 0;
				num_alloced = 0;
				result = FC_FAILURE;
				goto cleanup_addbuf;
			}
		}

		if (priv->spp_flags & PACKET_IN_PROCESS) {
			DEBUGF(7, (CE_WARN, "usoc%d: usoc_add_ubuf"
			    " spp_flags has PACKET_IN_PROCESS\n",
			    usocp->usoc_instance));

			priv->spp_flags &= ~PACKET_IN_PROCESS;
		}

		bzero((void *)drq, sizeof (usoc_add_buf_request_t));
		/*
		 * instead of destroying the fc_packet and trying
		 * to allocate it again - it may be better and cleaner
		 * to reuse the existing packet. We will have to
		 * reset the timetag and also some of the flags.
		 */
		usoc_reuse_packet(pkt);

		num_alloced += num_bufs;
	}

	if (pkt) {
		usoc_packet_free(pkt);
	}

	if (num_alloced != num_dmasetup) {
		goto cleanup_addbuf;
	}

	*count = num_alloced;

	/*
	 * Now that we have actually alloced the buffers, let us initialize
	 * the fc_unsol_buf_t list. The unsolicited callbacks will happen
	 * with these fc_unsol_buf_t structures being passed back up to the
	 * ULPs
	 */
	mutex_enter(&portp->sp_mtx);
	ubufp->pool_fc_ubufs =
	    kmem_zalloc((sizeof (fc_unsol_buf_t) * num_alloced), KM_SLEEP);

	for (i = 0; i < num_alloced; i++) {
		fc_unsol_buf_t		*fcbp;
		usoc_ub_priv_t		*ubp;
		struct pool_dma_res 	*nbp = (ubufp->pool_dma_res_ptr + i);
		uint64_t		buf_token;

		buf_token = (uint64_t)((uint64_t)poolid | (uint64_t)i);

		fcbp = (fc_unsol_buf_t *)(ubufp->pool_fc_ubufs + i);
		fcbp->ub_port_handle = portp->sp_tran_handle;
		fcbp->ub_token = (uint64_t)fcbp;
		fcbp->ub_buffer = (caddr_t)(nbp->pool_buf);
		fcbp->ub_bufsize = ubufp->pool_buf_size;
		fcbp->ub_class = FC_TRAN_CLASS3;
		fcbp->ub_port_private = (void *)0;
		fcbp->ub_fca_private = (usoc_ub_priv_t *)kmem_zalloc(
		    sizeof (usoc_ub_priv_t), KM_SLEEP);
		ubp = (usoc_ub_priv_t *)fcbp->ub_fca_private;
		ubp->ubp_ub_token = buf_token;
		ubp->ubp_pool_dma_res = nbp;
		token[i] = (uint64_t)fcbp;
	}

	ubufp->pool_nentries = num_alloced;
	mutex_exit(&portp->sp_mtx);

	if (result == FC_SUCCESS) {
		return (result);
	}

cleanup_addbuf:
	usoc_display(usocp, portp->sp_port, CE_WARN, USOC_LOG_ONLY, NULL,
	    "Buffer pool DVMA alloc failed");

	mutex_enter(&portp->sp_mtx);
	if (ubufp) {
		/*
		 * If we failed while informing the ucode of the bufs,
		 * assume the worst.  Reset the card to clear any ubufs
		 * already entered.
		 *
		 * Since we're just setting these up, we probably don't need
		 * to inform the transport of a restart
		 */
		if (num_alloced) {
			mutex_exit(&portp->sp_mtx);
			(void) usoc_force_reset(usocp, 0, 0);
			mutex_enter(&portp->sp_mtx);
		}
		/*
		 * cleanup all the dma resources
		 */
		if (ubufp->pool_dma_res_ptr) {
			for (i = 0; i < num_dmasetup; i++) {
				struct pool_dma_res *nbp =
				    (ubufp->pool_dma_res_ptr + i);
				if (nbp->pool_dhandle) {
					(void) ddi_dma_unbind_handle(
					    nbp->pool_dhandle);
					ddi_dma_free_handle(&nbp->pool_dhandle);
				}
				if (nbp->pool_buf) {
					ddi_dma_mem_free(&nbp->pool_acchandle);
				}
				token[i] = 0;
			}

			/*
			 * Now free up the dma_resource space
			 */
			kmem_free((struct pool_dma_res *)
			    ubufp->pool_dma_res_ptr,
			    (sizeof (struct pool_dma_res) *
			    ubufp->pool_nentries));
		}
		ubufp->pool_dma_res_ptr = NULL;
		ubufp->pool_nentries = 0;
	}
	mutex_exit(&portp->sp_mtx);

	if (pkt) {
		usoc_packet_free(pkt);
	}

	if (num_alloced) {
		(void) usoc_start(usocp);
	}

	return (FC_FAILURE);
}


/* ARGSUSED */
static int
usoc_ub_release(opaque_t portp, uint32_t count, uint64_t token[])
{
	uint32_t	i;
	usoc_port_t	*port_statep = (usoc_port_t *)portp;
#ifdef DEBUG
	usoc_state_t	*usocp = port_statep->sp_board;
#endif
	fc_unsol_buf_t	*ub;
	usoc_ub_priv_t	*ubp;
	usoc_add_buf_request_t	*drq;
	uint16_t 	num_bufs = 0;
	int		result = FC_SUCCESS;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;

	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL) {
		DEBUGF(7, (CE_WARN, "usoc_ub_release: usoc%d: fc_packet"
		    " alloc failed\n", usocp->usoc_instance));
		result = FC_FAILURE;
		goto fail;
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	drq = (usoc_add_buf_request_t *)&priv->spp_sr;

	for (i = 0; i < count; i++) {
		if (!token[i]) {
			result = FC_UB_BADTOKEN;
			goto fail;
		}

		ub = (fc_unsol_buf_t *)token[i];
		ubp = (usoc_ub_priv_t *)ub->ub_fca_private;
		if (!(ubp->ubp_flags & UB_VALID)) {
			result = FC_UB_BADTOKEN;
			goto fail;
		}

		if (ubp->ubp_flags & UB_TEMPORARY) {
			kmem_free((void *)ub->ub_buffer, ub->ub_bufsize);
			kmem_free((void *)ubp, sizeof (usoc_ub_priv_t));
			kmem_free((void *)ub, sizeof (fc_unsol_buf_t));
		} else {
			struct pool_dma_res *nbp = (struct pool_dma_res *)
			    ubp->ubp_pool_dma_res;

			ubp->ubp_flags |= UB_IN_FCA;
			drq->uabr_buf_descriptor[num_bufs].address =
			    (uint32_t)nbp->pool_dcookie.dmac_address;
			drq->uabr_buf_descriptor[num_bufs++].token =
			    USOC_GET_UCODE_TOKEN(ubp->ubp_ub_token);
			if ((i == (count - 1)) ||
			    (num_bufs == NUM_USOC_BUFS)) {
				/*
				 * Fill in the request structure.
				 */
				drq->uabr_usoc_hdr.sh_request_token =
				    USOC_GET_POOLID_FROM_TOKEN(
				    ubp->ubp_ub_token);
				drq->uabr_usoc_hdr.sh_flags = USOC_UNSOLICITED;
				if (port_statep->sp_port) {
					drq->uabr_usoc_hdr.sh_flags |=
					    USOC_PORT_B;
				}

				drq->uabr_usoc_hdr.sh_class = 3;
				drq->uabr_usoc_hdr.sh_seg_cnt = 1;
				drq->uabr_usoc_hdr.sh_byte_cnt = 0;
				drq->uabr_pool_id =
				USOC_GET_POOLID_FROM_TOKEN(ubp->ubp_ub_token);
				drq->uabr_nentries = num_bufs;
				drq->uabr_cqhdr.cq_hdr_count = 1;
				drq->uabr_cqhdr.cq_hdr_type =
				    CQ_TYPE_ADD_BUFFER;
				drq->uabr_cqhdr.cq_hdr_flags = 0;
				drq->uabr_cqhdr.cq_hdr_seqno = 0;

				/* Transport the request. */
				pkt->pkt_timeout = USOC_PKT_TIMEOUT;
				result = usoc_doit(port_statep, pkt, 0,
					PORT_OUTBOUND_PENDING, NULL);

				if (result != FC_SUCCESS) {
					DEBUGF(7, (CE_WARN, "usoc%d: unable to "
					    "transport add_buf req pkt (%d)\n",
					    usocp->usoc_instance, result));
					result = FC_UB_BADTOKEN;
					goto fail;
				}
				if (priv->spp_flags & PACKET_IN_PROCESS) {
					DEBUGF(7, (CE_WARN, "usoc%d: "
					    "usoc_ub_release :spp_flags "
					    "has PACKET_IN_PROCESS\n",
					    usocp->usoc_instance));
					priv->spp_flags &= ~PACKET_IN_PROCESS;
				}
				num_bufs = 0;
			}
			/*
			 * instead of destroying the fc_packet and trying
			 * to allocate it again - it may be better and cleaner
			 * to reuse the existing packet. We will have to
			 * reset the timetag and also some of the flags.
			 */
			usoc_reuse_packet(pkt);
		}
	}
fail:
	if (pkt) {
		usoc_packet_free(pkt);
	}
	return (result);
}


/* ARGSUSED */
static int
usoc_ub_free(opaque_t portp, uint32_t count, uint64_t token[])
{
	usoc_delete_pool_request_t *dprq = NULL;
	uint64_t		poolid;
	usoc_port_t		*port_statep = (usoc_port_t *)portp;
	usoc_state_t		*usocp = port_statep->sp_board;
	usoc_unsol_buf_t	*ubufp = port_statep->usoc_unsol_buf;
	usoc_unsol_buf_t	*prev_bufp;
	int			i;
	int			result = FC_SUCCESS;
	fc_unsol_buf_t		*fcbp;
	usoc_ub_priv_t		*ubp;
	fc_packet_t		*pkt;
	usoc_pkt_priv_t		*priv;

	if (count == 0) {
		DEBUGF(7, (CE_WARN, "usoc%d: cannot delete zero pools\n",
		    usocp->usoc_instance));
		return (FC_FAILURE);
	}

	if (token == NULL) {
		DEBUGF(7, (CE_WARN, "usoc%d: cannot delete pool without"
		    " token array\n", usocp->usoc_instance));
		return (FC_FAILURE);
	}

	fcbp = (fc_unsol_buf_t *)token[0];
	ubp = (usoc_ub_priv_t *)fcbp->ub_fca_private;
	poolid = USOC_GET_POOLID_FROM_TOKEN(ubp->ubp_ub_token);

	for (i = 1; i < count; i++) {
		fcbp = (fc_unsol_buf_t *)token[i];
		ubp = (usoc_ub_priv_t *)fcbp->ub_fca_private;
		if (token && USOC_GET_POOLID_FROM_TOKEN(
		    ubp->ubp_ub_token) != poolid) {
			DEBUGF(7, (CE_WARN,
			    "usoc%d: All buffers don't seem to belong "
			    "to same pool. Expected 0x%llu: Got 0x%x\n",
			    usocp->usoc_instance, (unsigned long long)poolid,
			    USOC_GET_POOLID_FROM_TOKEN(token[i])));

			return (FC_UB_BADTOKEN);
		}
	}

	/*
	 * All buffers belong to same pool so we go right on
	 */

	mutex_enter(&port_statep->sp_mtx);
	prev_bufp = NULL;
	while (ubufp) {
		if (ubufp->pool_id == poolid) {
			break;
		}
		prev_bufp = ubufp;
		ubufp = ubufp->pool_next;
	}

	if (ubufp == NULL) {
		DEBUGF(7, (CE_WARN, "usoc%d: Buf pool with poolid 0x%llu"
		    " not found\n", usocp->usoc_instance,
		    (unsigned long long)poolid));

		mutex_exit(&port_statep->sp_mtx);
		return (FC_UB_BADTOKEN);
	}

	if (ubufp->pool_nentries != count) {
		mutex_exit(&port_statep->sp_mtx);

		DEBUGF(7, (CE_WARN,
		    "usoc%d: pool has 0x%x bufs but releasing only 0x%x\n",
		    usocp->usoc_instance, ubufp->pool_nentries, count));

		return (FC_FAILURE);
	}

	mutex_exit(&port_statep->sp_mtx);

	/*
	 * First delete the pool and then deallocate the dma
	 * resources
	 */
	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL) {
		DEBUGF(7, (CE_WARN,
		    "usoc_ub_free: usoc%d: fc_packet alloc failed\n",
		    usocp->usoc_instance));

		return (FC_FAILURE);
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	dprq = (usoc_delete_pool_request_t *)&priv->spp_sr;

	dprq->udpr_pool_id = poolid;
	dprq->udpr_usoc_hdr.sh_request_token = poolid;
	dprq->udpr_usoc_hdr.sh_flags = USOC_FC_HEADER | USOC_UNSOLICITED;

	if (port_statep->sp_port) {
		dprq->udpr_usoc_hdr.sh_flags |= USOC_PORT_B;
	}

	dprq->udpr_usoc_hdr.sh_class = 3;
	dprq->udpr_usoc_hdr.sh_seg_cnt = 1;
	dprq->udpr_cqhdr.cq_hdr_count = 1;
	dprq->udpr_cqhdr.cq_hdr_type = CQ_TYPE_DELETE_POOL;
	dprq->udpr_cqhdr.cq_hdr_flags = 0;
	dprq->udpr_cqhdr.cq_hdr_seqno = 0;

	pkt->pkt_timeout = USOC_PKT_TIMEOUT;
	result = usoc_doit(port_statep, pkt, 0, PORT_OUTBOUND_PENDING, NULL);

	if (result != FC_SUCCESS) {
		ddi_devstate_t	devstate;

		/* we are in trouble!! */
		DEBUGF(7, (CE_WARN, "usoc%d: unable to transport add_pool"
		    " req packet (%d)\n", usocp->usoc_instance, result));

		if (!USOC_DEVICE_BAD(usocp, devstate)) {
			if (pkt) {
				usoc_packet_free(pkt);
			}
			return (FC_FAILURE);
		}
	}

	if (priv->spp_flags & PACKET_IN_PROCESS) {
		DEBUGF(7, (CE_WARN,
		    "usoc%d: usoc_ub_free :spp_flags has PACKET_IN_PROCESS\n",
		    usocp->usoc_instance));

		priv->spp_flags &= ~PACKET_IN_PROCESS;
	}

	if (pkt) {
		usoc_packet_free(pkt);
	}

	/*
	 * Now free up the dma resources for this pool
	 */
	mutex_enter(&port_statep->sp_mtx);
	if (ubufp) {
		/*
		 * First cleanup all the dma resources
		 */
		if (ubufp->pool_dma_res_ptr) {
			for (i = 0; i < count; i++) {
				struct pool_dma_res *nbp =
				    (ubufp->pool_dma_res_ptr + i);
				if (nbp->pool_dhandle) {
					(void) ddi_dma_unbind_handle(
					    nbp->pool_dhandle);
					ddi_dma_free_handle(&nbp->pool_dhandle);
				}
				if (nbp->pool_buf) {
					ddi_dma_mem_free(&nbp->pool_acchandle);
				}
			}
			/*
			 * Now free up the dma_resource space
			 */
			kmem_free(ubufp->pool_dma_res_ptr,
			    ((sizeof (struct pool_dma_res) *
			    ubufp->pool_nentries)));

			ubufp->pool_dma_res_ptr = NULL;
		}

		/*
		 * Now free up the fc_unsol_buf array
		 */
		if (ubufp->pool_fc_ubufs) {
			/*
			 * first free up the ub_fca_private area
			 */
			for (i = 0; i < ubufp->pool_nentries; i++) {
				fc_unsol_buf_t *fcbp =
				    (fc_unsol_buf_t *)token[i];
				if (fcbp->ub_fca_private) {
					kmem_free(fcbp->ub_fca_private,
					    sizeof (usoc_ub_priv_t));
				}
				token[i] = 0;
			}
			/*
			 * Now free up the unsol buf array itself
			 */
			kmem_free(ubufp->pool_fc_ubufs,
			    (sizeof (fc_unsol_buf_t) * ubufp->pool_nentries));

			ubufp->pool_fc_ubufs = NULL;
		}

		if (prev_bufp) {
			prev_bufp->pool_next = ubufp->pool_next;
		} else {
			port_statep->usoc_unsol_buf = ubufp->pool_next;
		}

		kmem_free(ubufp, sizeof (usoc_unsol_buf_t));
	}
	mutex_exit(&port_statep->sp_mtx);

	return (FC_SUCCESS);
}


/*
 * This handle unsol intr routine does both inline and unsolicited buffer
 * pool handling. The other unsolicited intr handle routine can be remove
 * once unsolicited buffers are working
 */
static void
usoc_handle_unsol_intr_new(usoc_state_t *usocp, usoc_kcq_t *kcq,
    volatile cqe_t *cqe, volatile cqe_t *cqe_cont)
{
	usoc_port_t		*port_statep;
	usoc_unsol_resp_t	*urp = (usoc_unsol_resp_t *)cqe;
	int			port;
	uint32_t		cb_state;
	int			status;
	ushort_t		flags;
	opaque_t 		tran_handle;
	void 			(*unsol_cb) (opaque_t port_handle,
				    fc_unsol_buf_t *buf, uint32_t type);

	flags = urp->unsol_resp_usoc_hdr.sh_flags;
	port = (flags & USOC_PORT_B) ? 1 : 0;
	port_statep = &usocp->usoc_port_state[port];

	switch (flags & ~USOC_PORT_B) {
	case USOC_UNSOLICITED | USOC_FC_HEADER:

		mutex_enter(&port_statep->sp_mtx);
		if ((port_statep->sp_status & PORT_BOUND) == 0) {
			mutex_exit(&port_statep->sp_mtx);
			break;
		}
		mutex_exit(&port_statep->sp_mtx);

		switch (urp->unsol_resp_fc_frame_hdr.r_ctl & R_CTL_ROUTING) {

		/* handle unsolicited data */
		case R_CTL_EXTENDED_SVC:
		/* FALLTHROUGH */
		case R_CTL_DEVICE_DATA: {
			uint16_t	tok_num;
			fc_unsol_buf_t	*fcbp;
			struct pool_dma_res *nbp;
			usoc_ub_priv_t	*uprivp;
			usoc_unsol_buf_t *ubufp = port_statep->usoc_unsol_buf;
#ifdef DEBUG
			usoc_state_t	 *usocp = port_statep->sp_board;
#endif
			uint64_t	poolid;
			uint32_t	type;

			/*
			 * Handle inline unsolicited data. We expect all
			 * inline data(ELS or device data) to have the
			 * cqe type field set to CQ_TYPE_INLINE.
			 */
			if (cqe->cqe_hdr.cq_hdr_type == CQ_TYPE_INLINE) {

				DEBUGF(7, (CE_WARN,
				    "usoc%d: Inline Unsol data/els rcvd\n",
				    usocp->usoc_instance));

				fcbp = usoc_ub_temp(port_statep, (cqe_t *)cqe,
				    (caddr_t)cqe_cont);
				type = urp->unsol_resp_fc_frame_hdr.r_ctl << 8;
				type |= urp->unsol_resp_fc_frame_hdr.type;

				if (fcbp == NULL) {
					type |= 0x80000000;
				}

				/* call back into transport */
				goto sendup;
			}

			if (urp->unsol_resp_nentries > 1) {
				DEBUGF(7, (CE_WARN, "usoc%d: more than one "
				    "buf in sequence:0x%x\n",
				    usocp->usoc_instance,
				    urp->unsol_resp_nentries));
			}

			type = (urp->unsol_resp_fc_frame_hdr.type) & 0xFF;
			poolid = USOC_GET_POOLID_FROM_TYPE(type);

			mutex_enter(&port_statep->sp_mtx);
			while (ubufp) {
				if (ubufp->pool_id == poolid) {
					break;
				}
				ubufp = ubufp->pool_next;
			}

			if (ubufp == NULL) {
				mutex_exit(&port_statep->sp_mtx);

				DEBUGF(7, (CE_WARN, "usoc%d: Buf pool with "
				    "poolid 0x%llu not found\n",
				    usocp->usoc_instance,
				    (unsigned long long)poolid));

				break;
			}
			mutex_exit(&port_statep->sp_mtx);

			tok_num = urp->unsol_resp_tokens[0];
			DEBUGF(7, (CE_WARN,
			    "usoc%d: tok_num in unsol response 0x%x\n",
			    usocp->usoc_instance, tok_num));

			ASSERT(tok_num < ubufp->pool_nentries);
			/*
			 * If the token number exceeds the number of
			 * tokens in the pool we will obviously panic -
			 * instead barf a message and ignore this response
			 */
			if (tok_num >= ubufp->pool_nentries) {
				usoc_display(usocp, port, CE_NOTE,
				    USOC_LOG_ONLY, NULL,
				    "Invalid token number in unsolicited "
				    "response : %x\n", tok_num);

				break;
			}
			fcbp = (fc_unsol_buf_t *)
			    (ubufp->pool_fc_ubufs + tok_num);
			nbp = (ubufp->pool_dma_res_ptr + tok_num);

			DEBUGF(7, (CE_WARN,
			    "usoc%d: fcbp in unsol response %p\n",
			    usocp->usoc_instance, (void *)fcbp));

			USOC_SYNC_FOR_KERNEL(nbp->pool_dhandle, 0, 0);

			fcbp->ub_bufsize = urp->unsol_resp_last_cnt;
			uprivp = (usoc_ub_priv_t *)fcbp->ub_fca_private;
			uprivp->ubp_flags = UB_VALID;
			uprivp->ubp_portp = port_statep;

			/* validate the value of class here */
			switch (urp->unsol_resp_usoc_hdr.sh_class) {
			case 1:
				fcbp->ub_class = FC_TRAN_CLASS1;
				break;

			case 2:
				fcbp->ub_class = FC_TRAN_CLASS2;
				break;

			case 3:
				fcbp->ub_class = FC_TRAN_CLASS3;
				break;

			default:
				fcbp->ub_class = FC_TRAN_CLASS_INVALID;
				break;
			}

			fcbp->ub_port_handle = (opaque_t)port_statep;
			usoc_wcopy((uint_t *)&urp->unsol_resp_fc_frame_hdr,
			    (uint_t *)&fcbp->ub_frame, sizeof (fc_frame_hdr_t));

			/*
			 * This part of calling back into the transport
			 * is common for INLINE/non-INLINE cases
			 */
sendup:
			/* do callbacks the transport */
			mutex_exit(&kcq->skc_mtx);

			/*
			 * wake up any thread waiting for unsol
			 * inbound packets - FOR IOCTL only
			 */
			mutex_enter(&port_statep->sp_unsol_mtx);
			port_statep->sp_unsol_buf = fcbp;
			cv_broadcast(&port_statep->sp_unsol_cv);
			mutex_exit(&port_statep->sp_unsol_mtx);

			mutex_enter(&port_statep->sp_mtx);

			if (port_statep->sp_unsol_callb) {
				tran_handle = port_statep->sp_tran_handle;
				unsol_cb = port_statep->sp_unsol_callb;
				mutex_exit(&port_statep->sp_mtx);
				(*unsol_cb) (tran_handle, fcbp, type);
			} else {
				mutex_exit(&port_statep->sp_mtx);
			}

			mutex_enter(&kcq->skc_mtx);
			break;
		}

		case R_CTL_BASIC_SVC:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Unsupported Link Service command: 0x%x",
			    urp->unsol_resp_fc_frame_hdr.type);
			break;

		default:
			usoc_display(usocp, port, CE_WARN, USOC_LOG_ONLY, NULL,
			    "Unsupported FC frame R_CTL: 0x%x",
			    urp->unsol_resp_fc_frame_hdr.r_ctl);
			break;
		}
		break;

	case USOC_STATUS: {
		/*
		 * Note that only the lsbyte of the status
		 * has interesting information
		 */
		status = urp->unsol_resp_status;
		switch (status) {
		case USOC_ONLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel is ONLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status &= ~PORT_IN_LINK_RESET;
			port_statep->sp_status |= PORT_ONLINE;
			port_statep->sp_src_id = 0;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_ONLINE;
			usocp->usoc_stats.pstats[port].onlines++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol:"
			    " ONLINE intr\n", usocp->usoc_instance));

			break;

		case USOC_LOOP_ONLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel Loop is ONLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status |= PORT_ONLINE_LOOP;
			port_statep->sp_status &= ~PORT_IN_LINK_RESET;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_LOOP;
			usocp->usoc_stats.pstats[port].online_loops++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol: "
			    "ONLINE-LOOP intr\n", usocp->usoc_instance));

			break;

		case USOC_OFFLINE:
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "Fibre Channel is OFFLINE");

			mutex_enter(&port_statep->sp_mtx);
			port_statep->sp_status &= ~PORT_STATUS_MASK;
			port_statep->sp_status |= PORT_OFFLINE;
			mutex_exit(&port_statep->sp_mtx);

			cb_state = FC_STATE_OFFLINE;

			usocp->usoc_stats.pstats[port].offlines++;

			DEBUGF(4, (CE_CONT, "usoc%d intr_unsol:"
			    " OFFLINE intr\n", usocp->usoc_instance));

			break;

		default:
			cb_state = FC_STATE_OFFLINE;
			usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
			    "unsolicited interrupt: unknown status: 0x%x",
			    status);
			break;
		}
		mutex_exit(&kcq->skc_mtx);

		if (port_statep->sp_statec_callb) {
			port_statep->sp_statec_callb(
			    port_statep->sp_tran_handle, cb_state);
		}

		if (cb_state == FC_STATE_OFFLINE) {
			usoc_flush_of_queues(port_statep);
		}

		mutex_enter(&kcq->skc_mtx);
		break;

	}
	default:
		usoc_display(usocp, port, CE_NOTE, USOC_LOG_ONLY, NULL,
		    "unsolicited interrupt: unexpected state: flags: 0x%x",
		    flags);

		DEBUGF(4, (CE_CONT, "\tusoc CR: %x SAE: %x CSR:"
		    " %x IMR: %x\n",
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_cr.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_sae.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_csr.w),
		    USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_imr)));
		break;
	}
}


/*
 * reuse internal usoc packets
 */
static void
usoc_reuse_packet(fc_packet_t *pkt)
{
	usoc_pkt_priv_t	*priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_port_t *port_statep = (usoc_port_t *)priv->spp_portp;

	mutex_enter(&port_statep->sp_mtx);
	ASSERT((priv->spp_flags & PACKET_IN_PROCESS) == 0);

	priv->spp_flags = PACKET_VALID | PACKET_DORMANT |
				PACKET_INTERNAL_PACKET;
	priv->spp_packet = pkt;

	mutex_exit(&port_statep->sp_mtx);

	/*
	 * reset the relevant packet state/reason fields
	 */
	pkt->pkt_state = FC_PKT_SUCCESS;
	pkt->pkt_reason = 0;
	pkt->pkt_action = 0;
	pkt->pkt_expln = 0;
}


static void
usoc_memset(caddr_t buf, uint32_t pat, uint32_t size)
{
	int len = size / sizeof (pat);
	int i;
	uint32_t *ptr = (uint32_t *)buf;

	for (i = 0; i < len; i++) {
		*(ptr + i) = pat;
	}
}


static int
usoc_data_out(usoc_port_t *port_statep, struct usoc_send_frame *sftp,
	uint32_t sz, usoc_unsol_buf_t *ubufp)
{
	usoc_state_t		*usocp = port_statep->sp_board;
	fc_packet_t		*pkt;
	usoc_request_t		*srq;
	usoc_pkt_priv_t		*priv;
	int			retval = FC_FAILURE;
	ddi_dma_cookie_t	dcookie;
	ddi_dma_handle_t	dhandle = NULL;
	ddi_acc_handle_t	acchandle;
	size_t			real_len, data_sz;
	caddr_t			send_buf;
	uint_t			ccount;
	int			bound = 0;
	int 			i;
	uint32_t		*buf;
	size_t			len;

	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL) {
		return (retval);
	}

	if (ddi_dma_alloc_handle(usocp->usoc_dip, &usoc_dma_attr,
		DDI_DMA_DONTWAIT, NULL, &dhandle) != DDI_SUCCESS) {
		goto fail;
	}

	data_sz = (size_t)sz;

	if (ddi_dma_mem_alloc(dhandle, data_sz, &usoc_acc_attr,
	    DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    (caddr_t *)&send_buf, &real_len, &acchandle) != DDI_SUCCESS) {
		goto fail;
	}

	if (real_len < data_sz) {
		goto fail;
	}


	if (ddi_dma_addr_bind_handle(dhandle, (struct as *)NULL,
	    (caddr_t)send_buf, real_len, DDI_DMA_WRITE | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &dcookie, &ccount) != DDI_DMA_MAPPED) {
		goto fail;
	}

	bound = 1;
	if (ccount != 1) {
		goto fail;
	}

	usoc_memset(send_buf, sftp->sft_pattern, real_len);

	buf = (uint32_t *)send_buf;
	len = real_len/sizeof (uint32_t);

	for (i = 0; i < len; i++) {
		if (*(buf + i) != sftp->sft_pattern) {
			usoc_display(usocp, port_statep->sp_port, CE_WARN,
			    USOC_LOG_ONLY, NULL,
			    "mistmatch pattern at offset %x expected %x got"
			    " %x\n", i, sftp->sft_pattern, *(buf + i));
		}
	}

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	srq = (usoc_request_t *)&priv->spp_sr;
	/*
	 * use a dummy token. usoc_cq_enqueue allocates a valid ID
	 */
	srq->sr_usoc_hdr.sh_request_token = 1;

	srq->sr_usoc_hdr.sh_flags = USOC_FC_HEADER;
	srq->sr_usoc_hdr.sh_class = 3;
	if (port_statep->sp_port) {
	    srq->sr_usoc_hdr.sh_flags |= USOC_PORT_B;
	}
	srq->sr_usoc_hdr.sh_byte_cnt = sz;
	srq->sr_usoc_hdr.sh_seg_cnt = 1;
	srq->sr_dataseg[0].fc_count = sz;
	srq->sr_dataseg[0].fc_base = (uint32_t)dcookie.dmac_address;
	srq->sr_cqhdr.cq_hdr_count = 1;
	srq->sr_cqhdr.cq_hdr_type = CQ_TYPE_OUTBOUND;
	srq->sr_fc_frame_hdr.d_id = sftp->sft_d_id;
	srq->sr_fc_frame_hdr.r_ctl = R_CTL_DEVICE_DATA | R_CTL_UNSOL_DATA;
	srq->sr_fc_frame_hdr.s_id = port_statep->sp_src_id;
	srq->sr_fc_frame_hdr.type = (ubufp->pool_id >> USOC_TOKEN_SHIFT);
	srq->sr_fc_frame_hdr.f_ctl = F_CTL_FIRST_SEQ;
	srq->sr_fc_frame_hdr.seq_id = 0;
	srq->sr_fc_frame_hdr.df_ctl = 0;
	srq->sr_fc_frame_hdr.seq_cnt = 0;
	srq->sr_fc_frame_hdr.ox_id = 0xffff;
	srq->sr_fc_frame_hdr.rx_id = 0xffff;
	srq->sr_fc_frame_hdr.ro = 0;

	pkt->pkt_timeout = USOC_PKT_TIMEOUT;
	retval = usoc_doit(port_statep, pkt, 0, PORT_OUTBOUND_PENDING, NULL);

	if (priv->spp_flags & PACKET_IN_PROCESS) {
		DEBUGF(7, (CE_WARN,
		    "usoc%d: usoc_data_out: sppflags has PACKET_IN_PROCESS\n",
		    usocp->usoc_instance));
		priv->spp_flags &= ~PACKET_IN_PROCESS;
	}

	(void) ddi_dma_unbind_handle(dhandle);
	ddi_dma_free_handle(&dhandle);
	ddi_dma_mem_free(&acchandle);
	usoc_packet_free(pkt);
	return (retval);

fail:
	if (dhandle) {
		if (bound) {
			(void) ddi_dma_unbind_handle(dhandle);
		}
		ddi_dma_free_handle(&dhandle);
	}
	if (send_buf) {
		ddi_dma_mem_free(&acchandle);
	}
	if (pkt) {
		usoc_packet_free(pkt);
	}

	return (retval);
}


static void
usoc_reestablish_ubs(usoc_port_t *port_statep)
{
	int			i, j;
	int			result;
	int			num_dmasetup = 0;
	uint16_t 		num_bufs = 0;
	usoc_state_t		*usocp = port_statep->sp_board;
	usoc_unsol_buf_t	*ubufp, *ubuf_head, *ubuf_prev;
	usoc_add_pool_request_t	*prq = NULL;
	usoc_add_buf_request_t	*drq = NULL;

	prq = kmem_zalloc(sizeof (*prq), KM_SLEEP);
	drq = kmem_zalloc(sizeof (*drq), KM_SLEEP);

	ASSERT(prq != NULL && drq != NULL);

	mutex_enter(&port_statep->sp_mtx);
	ubufp = port_statep->usoc_unsol_buf;
	port_statep->usoc_unsol_buf = NULL;
	mutex_exit(&port_statep->sp_mtx);

	ubuf_head = ubufp;
	ubuf_prev = NULL;
	while (ubufp) {
		/*
		 * Fill in the Add Pool request structure.
		 */
		prq->uapr_usoc_hdr.sh_request_token = ubufp->pool_id;
		prq->uapr_usoc_hdr.sh_flags = USOC_FC_HEADER |
		    USOC_UNSOLICITED | USOC_NO_RESPONSE;

		if (port_statep->sp_port) {
			prq->uapr_usoc_hdr.sh_flags |= USOC_PORT_B;
		}

		prq->uapr_usoc_hdr.sh_class = 3;
		prq->uapr_usoc_hdr.sh_seg_cnt = 1;
		prq->uapr_usoc_hdr.sh_byte_cnt = 0;

		prq->uapr_pool_id = ubufp->pool_id;
		prq->uapr_header_mask = UAPR_MASK_TYPE;
		prq->uapr_buf_size = ubufp->pool_buf_size;

		prq->uapr_fc_frame_hdr.r_ctl = 0;
		prq->uapr_fc_frame_hdr.d_id = 0;
		prq->uapr_fc_frame_hdr.s_id = 0;
		prq->uapr_fc_frame_hdr.type = ubufp->pool_type;
		prq->uapr_fc_frame_hdr.f_ctl = 0;
		prq->uapr_fc_frame_hdr.seq_id = 0;
		prq->uapr_fc_frame_hdr.df_ctl = 0;
		prq->uapr_fc_frame_hdr.seq_cnt = 0;
		prq->uapr_fc_frame_hdr.ox_id = 0;
		prq->uapr_fc_frame_hdr.rx_id = 0;
		prq->uapr_fc_frame_hdr.ro = 0;

		prq->uapr_cqhdr.cq_hdr_count = 1;
		prq->uapr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_POOL;
		prq->uapr_cqhdr.cq_hdr_flags = 0;
		prq->uapr_cqhdr.cq_hdr_seqno = 0;

		/* Enque the request. */
		result = usoc_cq_enque(usocp, NULL, (cqe_t *)prq,
		    CQ_REQUEST_1, NULL, NULL, 0);

		if (result != FC_SUCCESS) {
			usoc_display(usocp, port_statep->sp_port, CE_WARN,
			    USOC_LOG_ONLY, NULL, "reestablish of Pool"
			    " failed for poolid=%x", ubufp->pool_id);

			ubufp = ubufp->pool_next;
			continue;
		}

		mutex_enter(&usocp->usoc_board_mtx);
		usocp->usoc_ncmds--;
		mutex_exit(&usocp->usoc_board_mtx);

		num_dmasetup = ubufp->pool_nentries;

		for (i = 0; i < num_dmasetup; i += NUM_USOC_BUFS) {
			num_bufs = (((num_dmasetup - i) >=
			    NUM_USOC_BUFS) ? NUM_USOC_BUFS :
			    (num_dmasetup - i));

			for (j = 0; j < num_bufs; j++) {
				struct pool_dma_res *nbp =
				    (ubufp->pool_dma_res_ptr + i + j);

				drq->uabr_buf_descriptor[j].address =
				    (uint32_t)nbp->pool_dcookie.dmac_address;
				drq->uabr_buf_descriptor[j].token = (i + j);
			}

			/*
			 * Fill in the request structure.
			 */
			drq->uabr_usoc_hdr.sh_request_token = ubufp->pool_id;
			drq->uabr_usoc_hdr.sh_flags = USOC_UNSOLICITED |
			    USOC_NO_RESPONSE;

			if (port_statep->sp_port) {
				drq->uabr_usoc_hdr.sh_flags |= USOC_PORT_B;
			}

			drq->uabr_usoc_hdr.sh_class = 3;
			drq->uabr_usoc_hdr.sh_seg_cnt = 1;
			drq->uabr_usoc_hdr.sh_byte_cnt = 0;
			drq->uabr_pool_id = ubufp->pool_id;
			drq->uabr_nentries = num_bufs;
			drq->uabr_cqhdr.cq_hdr_count = 1;
			drq->uabr_cqhdr.cq_hdr_type = CQ_TYPE_ADD_BUFFER;
			drq->uabr_cqhdr.cq_hdr_flags = 0;
			drq->uabr_cqhdr.cq_hdr_seqno = 0;

			/* Transport the request. */
			result = usoc_cq_enque(usocp, NULL, (cqe_t *)drq,
			    CQ_REQUEST_1, NULL, NULL, 0);

			if (result == FC_SUCCESS) {
				mutex_enter(&usocp->usoc_board_mtx);
				usocp->usoc_ncmds--;
				mutex_exit(&usocp->usoc_board_mtx);
			} else {
				/*
				 * Print an UGLY message
				 */
				usoc_display(usocp, port_statep->sp_port,
				    CE_WARN, USOC_LOG_ONLY, NULL,
				    "reestablish of UBs failed for poolid=%x",
				    ubufp->pool_id);
			}
		}

		ubuf_prev = ubufp;
		ubufp = ubufp->pool_next;
	}

	mutex_enter(&port_statep->sp_mtx);
	if (port_statep->usoc_unsol_buf == NULL) {
		port_statep->usoc_unsol_buf = ubuf_head;
	} else if (ubuf_head != NULL) {
		ASSERT(ubuf_prev != NULL);
		ubuf_prev->pool_next = port_statep->usoc_unsol_buf;
		port_statep->usoc_unsol_buf = ubuf_head;
	}
	mutex_exit(&port_statep->sp_mtx);

	kmem_free(prq, sizeof (*prq));
	kmem_free(drq, sizeof (*drq));
}


static void
usoc_flush_all(usoc_state_t *usocp)
{
	int		i, j;
	int		again;
	int		ntry = 0;

flush_commands:
	for (again = 0, i = 0; i < USOC_N_CQS - 1; i++) {
		for (j = 0; j < N_USOC_NPORTS; j++) {
			usoc_kcq_t	*kcq = &usocp->usoc_request[i];

			mutex_enter(&kcq->skc_mtx);

			if (kcq->skc_overflowh) {
				mutex_exit(&kcq->skc_mtx);
				usoc_flush_overflow_Q(usocp, j, i);

				mutex_enter(&kcq->skc_mtx);
				if (kcq->skc_overflowh != NULL) {
					again++;
				}
				mutex_exit(&kcq->skc_mtx);
			} else {
				ASSERT(kcq->skc_overflowt == NULL);
				mutex_exit(&kcq->skc_mtx);
			}
		}
	}

	if (usoc_flush_timelist(usocp)) {
		again++;
	}

	if (again) {
		delay(drv_usectohz(USOC_NOINTR_POLL_DELAY_TIME));
		if (ntry++ < 3) {
			goto flush_commands;
		} else {
			usoc_display(usocp, USOC_PORT_ANY, CE_WARN,
			    USOC_LOG_ONLY, NULL, "couldn't flush all commands");
		}
	}
}


static uint32_t
usoc_flush_timelist(usoc_state_t *usocp)
{
	usoc_timetag_t	*timep, *tmptp;
	usoc_timetag_t	*list;
	usoc_pkt_priv_t	*priv;
	void 		(*func) (void *);
	int		index;
	uint32_t	remain = 0;

	mutex_enter(&usocp->usoc_time_mtx);
	for (list = NULL, index = 0; index <= USOC_TIMELIST_SIZE; index++) {
		timep = usocp->usoc_timelist[index];
		for (; timep != NULL; timep = tmptp) {
			tmptp = timep->sto_next;
			priv = timep->sto_pkt->pkt_fca_private;
			ASSERT(priv != NULL);

			mutex_enter(&priv->spp_mtx);

			if (priv->spp_flags & (PACKET_IN_INTR |
			    PACKET_DORMANT)) {
				mutex_exit(&priv->spp_mtx);
				continue;
			}

			if (priv->spp_flags & PACKET_IN_TIMEOUT) {
				mutex_exit(&priv->spp_mtx);
				remain++;
				continue;
			}

			if (!(priv->spp_flags & PACKET_IN_OVERFLOW_Q) &&
			    !(priv->spp_flags & PACKET_IN_USOC)) {
				mutex_exit(&priv->spp_mtx);
				remain++;
				continue;
			}

			if ((priv->spp_flags & PACKET_IN_TRANSPORT) &&
			    !(priv->spp_flags & PACKET_IN_OVERFLOW_Q)) {
				mutex_exit(&priv->spp_mtx);
				usoc_free_id_for_pkt(usocp, priv->spp_packet);
				mutex_enter(&priv->spp_mtx);
			}

			priv->spp_flags &= ~PACKET_IN_PROCESS;
			priv->spp_flags |= PACKET_DORMANT;

			usoc_untimeout_held(usocp, priv);

			/*
			 * By design, this function is called after
			 * flushing the over flow queues.
			 */
			ASSERT((priv->spp_flags & PACKET_IN_OVERFLOW_Q) == 0);

			mutex_exit(&priv->spp_mtx);

			timep->sto_tonext = list;
			list = timep;
		}
	}
	mutex_exit(&usocp->usoc_time_mtx);

	for (timep = list; timep != NULL; timep = tmptp) {
		tmptp = timep->sto_tonext;

		priv = timep->sto_pkt->pkt_fca_private;

		mutex_enter(&priv->spp_mtx);
		ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
		    PACKET_INTERNAL_PACKET | PACKET_VALID |
		    PACKET_NO_CALLBACK)) == 0);

		if (USOC_PKT_COMP(timep->sto_pkt, priv)) {
			func = (void (*) (void *))timep->sto_pkt->pkt_comp;
			timep->sto_pkt->pkt_state = FC_PKT_PORT_OFFLINE;
			timep->sto_pkt->pkt_reason = FC_REASON_OFFLINE;
		} else {
			priv->spp_flags &= ~PACKET_NO_CALLBACK;
			func = NULL;
		}
		mutex_exit(&priv->spp_mtx);

		timep->sto_tonext = NULL;
		usoc_updt_pkt_stat_for_devstate(timep->sto_pkt);

		if (func) {
			func(timep->sto_pkt);
		}
	}

	return (remain);
}


static uint32_t
usoc_alloc_id(usoc_state_t *usocp, fc_packet_t *token)
{
	uint32_t	id, multiplier;
	usoc_idinfo_t	*idinfo;

	ASSERT(!MUTEX_HELD(&usocp->usoc_board_mtx));

	idinfo = &usocp->usoc_idinfo;

	mutex_enter(&usocp->usoc_board_mtx);
	/* Check if Free list is empty */
	if (idinfo->id_freelist_head == (short)-1 ||
	    usoc_dbg_drv_hdn == USOC_DDH_FAIL_ALLOCID) {
		mutex_exit(&usocp->usoc_board_mtx);
		return (USOC_INVALID_ID);
	}

	/* Get ID from free list */
	id = (uint32_t)idinfo->id_freelist_head;
	idinfo->id_freelist_head = idinfo->id_nextfree[id];

	/* Debug check: Do not step over an already allocated ID */
	ASSERT(idinfo->id_token[id] == (fc_packet_t *)NULL);

	idinfo->id_token[id] = token;	/* Save Token */

	multiplier = (uint32_t)idinfo->id_multiplier[id];

	usocp->usoc_ncmds++;		/* Increment usoc_ncmds */
	mutex_exit(&usocp->usoc_board_mtx);

	/* Compute return value of ID using the multiplier */
	id = id + (multiplier << USOC_ID_SHIFT);	/* multiplication */

	return (id);
}


static fc_packet_t *
usoc_free_id(usoc_state_t *usocp, uint32_t id, int src)
{
	usoc_idinfo_t	*idinfo;
	unsigned short	multiplier;
	fc_packet_t	*token;
	uint32_t	orgid = id;

	ASSERT(!MUTEX_HELD(&usocp->usoc_board_mtx));

	idinfo = &usocp->usoc_idinfo;

	/* We use incremntal IDs. Get multipier and  the actual ID Value. */
	multiplier = (unsigned short)(id >> USOC_ID_SHIFT);	/* division */
	id = id & (USOC_MAX_IDS -1);		/* mod */

	/* Check if this ID is already free */

	mutex_enter(&usocp->usoc_board_mtx);
	if ((multiplier != idinfo->id_multiplier[id]) ||
	    (usoc_dbg_drv_hdn == USOC_DDH_FAIL_FREEID)) {
		mutex_exit(&usocp->usoc_board_mtx);

		if ((multiplier+1) != idinfo->id_multiplier[id]) {
			usoc_display(usocp, USOC_PORT_ANY, CE_NOTE,
			    USOC_LOG_ONLY, NULL, "free_id(%s): bad id"
			    " %ux multiplier %ux cur-multiplier %ux"
			    " cur-id %ux",
			    (src == 0) ? "intr" : "free_pkt_id", id,
			    (uint32_t)multiplier,
			    (uint32_t)idinfo->id_multiplier[id], orgid);
			return ((fc_packet_t *)1);
		}

		return ((fc_packet_t *)0);
	}

	token = idinfo->id_token[id];
	ASSERT(token != NULL);

	idinfo->id_token[id] = (fc_packet_t *)NULL;

	/*
	 * Put the id in the free list. Since we use incremental IDs,
	 * it will take a long time before same ID will be allocated again.
	 * This helps us in catching the bad tokens where usoc completes
	 * same ID twice.
	 */
	idinfo->id_nextfree[id] = idinfo->id_freelist_head;
	idinfo->id_freelist_head = (short)id;
	idinfo->id_multiplier[id]++;	 /* Increment multiplier */

	/* Decrement the usoc_ncmds */
	usocp->usoc_ncmds--;
	ASSERT(usocp->usoc_ncmds >= 0);

#ifdef	DEBUG
	if (usocp->usoc_ncmds == 0) {
		DEBUGF(4, (CE_NOTE, "usoc%d: ncmds is zero now",
		    usocp->usoc_instance));
	}
#endif /* DEBUG */

	mutex_exit(&usocp->usoc_board_mtx);

	/* if pkt is found, then reset the PACKET_IN_USOC bit */
	if (token) {
		usoc_pkt_priv_t	*priv;

		priv = (usoc_pkt_priv_t *)token->pkt_fca_private;
		mutex_enter(&priv->spp_mtx);
		priv->spp_flags &= ~PACKET_IN_USOC;
		mutex_exit(&priv->spp_mtx);
	}

	return (token);
}


static void
usoc_free_id_for_pkt(usoc_state_t *usocp, fc_packet_t *pkt)
{
	uint32_t	id;
	usoc_pkt_priv_t	*priv;

	/* get the id from the cq entry in the pkt private structure */
	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	id = priv->spp_sr.sr_usoc_hdr.sh_request_token;

	DEBUGF(3, (CE_CONT, "usoc%d free_id_for_pkt: free id %x, pkt %p\n",
	    usocp->usoc_instance, id, (void *)pkt));

	(void) usoc_free_id(usocp, id, 1);
}


static void
usoc_init_ids(usoc_state_t *usocp)
{
	usoc_idinfo_t	*idinfo;
	short		i;

	idinfo = &usocp->usoc_idinfo;

	mutex_enter(&usocp->usoc_board_mtx);
	for (i = 0; i < USOC_MAX_IDS; i++) {
		idinfo->id_token[i] = (fc_packet_t *)NULL;
		idinfo->id_multiplier[i] = 0;
		idinfo->id_nextfree[i] = i+1;
	}
	idinfo->id_nextfree[USOC_MAX_IDS-1] = (short)-1;
	idinfo->id_freelist_head = 0;
	mutex_exit(&usocp->usoc_board_mtx);
}


static void
usoc_lfd_done(fc_packet_t *pkt)
{
	usoc_state_t	*usocp;
	usoc_pkt_priv_t	*priv;

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usocp = priv->spp_portp->sp_board;

	if ((pkt->pkt_state == FC_PKT_SUCCESS) && (pkt->pkt_reason == 0) &&
	    (usoc_dbg_drv_hdn != USOC_DDH_FAIL_LATENT_FAULT)) {
		/* latent fault detection is not in progress now */
		usocp->usoc_lfd_pending = 0;

		DEBUGF(6, (CE_CONT, "usoc%d: check_board: latent fault "
		    "detection over", usocp->usoc_instance));

	} else {
		usoc_packet_free(pkt);

		if (usocp->usoc_lfd_pending < USOC_LFD_MAX_RETRIES) {
			usoc_display(usocp, USOC_PRIV_TO_PORTNUM(priv), CE_WARN,
			    USOC_LOG_ONLY, NULL, "latent fault detection"
			    " failed, retry %d", usocp->usoc_lfd_pending);

			/* Retry latent fault detection */
			usoc_perform_lfd(usocp);

		} else {
			usoc_display(usocp, USOC_PRIV_TO_PORTNUM(priv),
			    CE_WARN, USOC_LOG_ONLY, NULL,
			    "latent fault detection failed, reset card."
			    " retry %d", usocp->usoc_lfd_pending);

			/* Reset card and retry latent fault detection */
			if (usoc_force_reset(usocp, 1, 1) == FC_SUCCESS) {
				usocp->usoc_lfd_pending = 0;
				usoc_perform_lfd(usocp);
			} else {
				usocp->usoc_lfd_pending = 0;
			}
		}
	}
}


static void
usoc_perform_lfd(usoc_state_t *usocp)
{
	usoc_port_t	*port_statep;
	fc_packet_t	*pkt;
	usoc_pkt_priv_t	*priv;
	usoc_request_t	*sr;
	int		status;

	/* Issue the command on port 0 */
	port_statep = &usocp->usoc_port_state[0];

	if ((pkt = usoc_packet_alloc(port_statep, 0)) == (fc_packet_t *)NULL) {
		return;
	}
	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;

	/* Initialize the cq entry */
	sr = (usoc_request_t *)&priv->spp_sr;
	sr->sr_usoc_hdr.sh_flags = 0;
	sr->sr_usoc_hdr.sh_seg_cnt = 0;
	sr->sr_usoc_hdr.sh_byte_cnt = 0;
	sr->sr_dataseg[0].fc_base = 0;
	sr->sr_dataseg[0].fc_count = 0;
	sr->sr_cqhdr.cq_hdr_count = 1;
	sr->sr_cqhdr.cq_hdr_type = CQ_TYPE_NOP;

	pkt->pkt_timeout = USOC_PKT_TIMEOUT;
	ASSERT(pkt->pkt_timeout > 0);
	pkt->pkt_tran_flags = FC_TRAN_INTR;
	pkt->pkt_comp = usoc_lfd_done;

	/* Set flag to indicate latent fault detection in progress */
	usocp->usoc_lfd_pending++;

	DEBUGF(6, (CE_CONT, "usoc%d: check_board: Issue cmd for latent fault "
	    "detection\n", usocp->usoc_instance));

	status = usoc_transport((opaque_t)port_statep, pkt);

	if (status != FC_SUCCESS) {
		DEBUGF(6, (CE_CONT, "usoc%d: check_board: transport failed\n",
		    usocp->usoc_instance));
		usocp->usoc_lfd_pending = 0;
	}
}


static void
usoc_updt_pkt_stat_for_devstate(fc_packet_t *pkt)
{
	usoc_pkt_priv_t *priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	usoc_state_t	*usocp;
	ddi_devstate_t	devstate;

	usocp = priv->spp_portp->sp_board;

	if (USOC_DEVICE_BAD(usocp, devstate)) {
		/* Return error, so that upper layers will not retry the cmd */
		pkt->pkt_state = FC_PKT_LOCAL_RJT;
		pkt->pkt_reason = FC_REASON_ABORT_FAILED;
	}
}


static int
usoc_finish_polled_cmd(usoc_state_t *usocp, fc_packet_t *pkt)
{
	int		internal;
	int		intr_mode;
	volatile 	uint32_t csr;
	usoc_pkt_priv_t	*priv;
	clock_t 	pkt_ticks, ticker, t;

	priv = (usoc_pkt_priv_t *)pkt->pkt_fca_private;
	internal = (priv->spp_flags & PACKET_INTERNAL_PACKET) ? 1 : 0;
	intr_mode = (pkt->pkt_tran_flags & FC_TRAN_NO_INTR) ? 0 : 1;

	ASSERT(pkt->pkt_timeout > 0);
	pkt_ticks = drv_usectohz(pkt->pkt_timeout * 1000 * 1000);
	(void) drv_getparm(LBOLT, &ticker);

	mutex_enter(&priv->spp_mtx);
	while ((priv->spp_flags & PACKET_IN_PROCESS) != 0) {
		mutex_exit(&priv->spp_mtx);
		delay(drv_usectohz(USOC_NOINTR_POLL_DELAY_TIME));

		(void) drv_getparm(LBOLT, &t);
		mutex_enter(&priv->spp_mtx);

		if ((priv->spp_flags & PACKET_IN_TRANSPORT) &&
		    ((ticker + pkt_ticks) < t || usocp->usoc_shutdown)) {
			if (internal) {
				priv->spp_flags &= ~PACKET_IN_PROCESS;

				mutex_exit(&priv->spp_mtx);
				usoc_free_id_for_pkt(usocp, pkt);
				pkt->pkt_state = FC_PKT_TIMEOUT;
				pkt->pkt_reason = FC_REASON_ABORTED;

				mutex_enter(&priv->spp_mtx);
				priv->spp_flags |= PACKET_DORMANT;
				mutex_exit(&priv->spp_mtx);

				ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
				    PACKET_INTERNAL_PACKET | PACKET_VALID |
				    PACKET_NO_CALLBACK)) == 0);

				return (FC_TRANSPORT_ERROR);
			}

			priv->spp_flags &= ~PACKET_IN_TRANSPORT;
			priv->spp_flags |= PACKET_INTERNAL_ABORT;
			mutex_exit(&priv->spp_mtx);

			pkt->pkt_state = FC_PKT_TIMEOUT;
			if (usoc_abort_cmd(priv->spp_portp, pkt,
			    intr_mode) != FC_SUCCESS) {
				pkt->pkt_reason = FC_REASON_ABORT_FAILED;
			} else {
				pkt->pkt_reason = FC_REASON_ABORTED;
			}

			ASSERT((priv->spp_flags & ~(PACKET_DORMANT |
			    PACKET_INTERNAL_PACKET | PACKET_VALID |
			    PACKET_NO_CALLBACK)) == 0);

			return (FC_TRANSPORT_ERROR);
		} else {
			mutex_exit(&priv->spp_mtx);
		}

		csr = USOC_RD32(usocp->usoc_rp_acchandle,
		    &usocp->usoc_rp->usoc_csr.w);

		/* Check for register access handle fault */
		if (USOC_ACCHDL_FAULT(usocp, usocp->usoc_rp_acchandle)) {
			usoc_display(usocp, USOC_PRIV_TO_PORTNUM(priv),
			    CE_WARN, USOC_LOG_ONLY, NULL, "transport."
			    " polled packet access handle fault: %p",
			    (void *)pkt);
		} else if ((USOC_INTR_CAUSE(usocp, csr)) & USOC_CSR_RSP_QUE_0) {
			(void) usoc_intr_solicited(usocp, 0);
		}

		mutex_enter(&priv->spp_mtx);
	}
	mutex_exit(&priv->spp_mtx);

	ASSERT((priv->spp_flags & ~(PACKET_DORMANT | PACKET_INTERNAL_PACKET |
	    PACKET_VALID | PACKET_NO_CALLBACK)) == 0);

	return (FC_SUCCESS);
}


/*
 * display a message
 */
static void
usoc_display(usoc_state_t *usocp, int port, int level, int dest,
    fc_packet_t *pkt, const char *fmt, ...)
{
	caddr_t		buf;
	va_list		ap;

	buf = kmem_zalloc(256, KM_NOSLEEP);
	if (buf == NULL) {
		return;
	}

	if (port == USOC_PORT_ANY) {
		(void) sprintf(buf, "usoc(%d): ", usocp->usoc_instance);
	} else {
		(void) sprintf(buf, "usoc(%d), port(%d): ",
		    usocp->usoc_instance, port);
	}

	va_start(ap, fmt);
	(void) vsprintf(buf + strlen(buf), fmt, ap);
	va_end(ap);

	if (pkt) {
		caddr_t	state, reason, action, expln;

		(void) fc_fca_pkt_error(pkt, &state, &reason, &action, &expln);

		(void) sprintf(buf + strlen(buf), "state=%s, reason=%s",
		    state, reason);
	}

	switch (dest) {
	case USOC_CONSOLE_ONLY:
		cmn_err(level, "^%s", buf);
		break;

	case USOC_LOG_ONLY:
		cmn_err(level, "!%s", buf);
		break;

	default:
		cmn_err(level, "%s", buf);
		break;
	}

	kmem_free(buf, 256);
}
