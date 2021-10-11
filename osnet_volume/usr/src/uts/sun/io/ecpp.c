/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ecpp.c	2.56	99/10/08 SMI"

/*
 * IEEE 1284 Parallel Port Device Driver
 *
 * Todo:
 * abort handling
 * postio
 * kstat
 */


#include <sys/param.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/termio.h>
#include <sys/termios.h>
#include <sys/cmn_err.h>
#include <sys/stropts.h>
#include <sys/debug.h>
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/strtty.h>
#include <sys/eucioctl.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/conf.h>		/* req. by dev_ops flags MTSAFE etc. */
#include <sys/modctl.h>		/* for modldrv */
#include <sys/stat.h>		/* ddi_create_minor_node S_IFCHR */
#include <sys/open.h>		/* for open params.	 */
#include <sys/uio.h>		/* for read/write */

#include <sys/ecppreg.h>	/* hw description */
#include <sys/ecppio.h>		/* ioctl description */
#include <sys/ecppvar.h>	/* driver description */

/*
* For debugging, allocate space for the trace buffer
*/
#if defined(POSTRACE)
struct postrace postrace_buffer[NPOSTRACE+1];
struct postrace *postrace_ptr;
int postrace_count;
#endif

#ifndef ECPP_REFERENCE_CODE
#define	ECPP_REFERENCE_CODE 0
#endif

#ifndef ECPP_DEBUG
#define	ECPP_DEBUG 0
#endif	/* ECPP_DEBUG */
static	int ecpp_debug = ECPP_DEBUG;

/* driver entry point fn definitions */
static int 	ecpp_open(queue_t *, dev_t *, int, int, cred_t *);
static int	ecpp_close(queue_t *, int, cred_t *);
static uint_t 	ecpp_isr(caddr_t);

/* configuration entry point fn definitions */
static int 	ecpp_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	ecpp_attach(dev_info_t *, ddi_attach_cmd_t);
static int	ecpp_detach(dev_info_t *, ddi_detach_cmd_t);

/* configuration support routines */
static void	ecpp_get_props(struct ecppunit *);
static void	set_interrupt_characteristic(struct ecppunit *pp);
static void	ecpp_check_periph(struct ecppunit *);

/* Streams Routines */
static int	ecpp_wput(queue_t *, mblk_t *);
static int	ecpp_wsrv(queue_t *);
static void	ecpp_flush(struct ecppunit *, int);
static void	ecpp_start(queue_t *, mblk_t *, caddr_t, size_t);

/* ioctl handling */
static void	ecpp_putioc(queue_t *, mblk_t *);
static void	ecpp_srvioc(queue_t *, mblk_t *);
static void 	ecpp_ack_ioctl(queue_t *, mblk_t *);
static void 	ecpp_nack_ioctl(queue_t *, mblk_t *, int);
static void 	ecpp_copyin(queue_t *, mblk_t *, void *, size_t, uint_t);
static void 	ecpp_copyout(queue_t *, mblk_t *, void *, size_t, uint_t);

/* dma routines */
static uint8_t	ecpp_stop_dma(struct ecppunit *);
static uint8_t	ecpp_reset_unbind_dma(struct ecppunit *);
static void	ecpp_putback_untransfered(struct ecppunit *, void *, uint_t);
static uint8_t	ecpp_setup_dma_resources(struct ecppunit *, caddr_t, size_t);
static uint8_t	ecpp_init_dma_xfer(struct ecppunit *, queue_t *,
			caddr_t, size_t);

/* pio routines */
static void	ecpp_pio_writeb(struct ecppunit *);
static void	ecpp_xfer_cleanup(struct ecppunit *);
static uint8_t	ecpp_prep_pio_xfer(struct ecppunit *, queue_t *,
			caddr_t, size_t);

/* superio programming */
static void	ecpp_remove_reg_maps(struct ecppunit *);
static uint8_t	ecpp_config_superio(struct ecppunit *);
static uchar_t	ecpp_reset_port_regs(struct ecppunit *);
static void	set_chip_pio(struct ecppunit *);
static void	ecpp_xfer_timeout(void *);
static void	ecpp_fifo_timer(void *);
static void	ecpp_wsrv_timer(void *);
static uchar_t	dcr_write(struct ecppunit *, uint8_t);
static uchar_t	ecr_write(struct ecppunit *, uint8_t);
static uchar_t	ecpp_check_status(struct ecppunit *);
static void	write_config_reg(struct ecppunit *, uint8_t, uint8_t);
static uint8_t	read_config_reg(struct ecppunit *, uint8_t);

/* IEEE 1284 phase transitions */
static int	ecpp_terminate_phase(struct ecppunit *);
static uchar_t 	ecpp_idle_phase(struct ecppunit *);
static int	ecp_forward2reverse(struct ecppunit *);
static int	ecp_reverse2forward(struct ecppunit *);
static uchar_t	read_nibble_backchan(struct ecppunit *);
static void 	reset_to_centronics(struct ecppunit *);

/* reverse transfers */
static uint_t	ecpp_peripheral2host(struct ecppunit *pp);
static uchar_t	ecp_peripheral2host(struct ecppunit *pp);
static uchar_t	nibble_peripheral2host(struct ecppunit *pp, uint8_t *);

/* IEEE 1284 mode transitions */
static void 	ecpp_default_negotiation(struct ecppunit *);
static int 	ecpp_mode_negotiation(struct ecppunit *, uchar_t);
static int	ecp_negotiation(struct ecppunit *);
static int	nibble_negotiation(struct ecppunit *);

/* debugging functions */
static void	ecpp_error(dev_info_t *, char *, ...);
static uchar_t	ecpp_get_error_status(uchar_t);


/* DMAC defintions */
#define	DCSR_INIT_BITS  DCSR_INT_EN | DCSR_EN_CNT | DCSR_CSR_DRAIN \
		| ecpp_burstsize | DCSR_TCI_DIS | DCSR_EN_DMA

#define	DCSR_INIT_BITS2 DCSR_INT_EN | DCSR_EN_CNT | DCSR_CSR_DRAIN \
		| ecpp_burstsize | DCSR_EN_DMA

/* Stream  defaults */
#define	IO_BLOCK_SZ	1024*128		/* DMA or PIO */
#define	ECPPHIWAT	32 * 1024  * 6
#define	ECPPLOWAT	32 * 1024  * 4

/* Loop timers */
#define	ECPP_REG_WRITE_MAX_LOOP	100 /* cpu is faster than superio */
#define	ECPP_ISR_MAX_DELAY	30  /* DMAC slow PENDING status */

#define	FIFO_DRAIN_PERIOD 250000 /* 250 ms */


static struct ecpp_transfer_parms default_xfer_parms = {
	60,		/* write timeout 60 seconds */
	ECPP_CENTRONICS	/* supported mode */
};


static void    *ecppsoft_statep;
static int	ecpp_burstsize = DCSR_BURST_1 | DCSR_BURST_0;

/* Pport statistics */
static int ecpp_reattempt_high = 0;	/* max times isr has looped */
static int ecpp_cf_isr;		/* # of check_status failures since open */
static int ecpp_joblen;		/* # of bytes xfer'd since open */

static int ecpp_isr_max_delay = ECPP_ISR_MAX_DELAY;
static int ecpp_def_timeout = 90;  /* left in for 2.7 compatibility */

struct module_info ecppinfo = {
	/* id, name, min pkt siz, max pkt siz, hi water, low water */
	42, "ecpp", 0, IO_BLOCK_SZ, ECPPHIWAT, ECPPLOWAT
};

static struct qinit ecpp_rinit = {
	putq, NULL, ecpp_open, ecpp_close, NULL, &ecppinfo, NULL
};

static struct qinit ecpp_wint = {
	ecpp_wput, ecpp_wsrv, ecpp_open, ecpp_close, NULL, &ecppinfo, NULL
};

struct streamtab ecpp_str_info = {
	&ecpp_rinit, &ecpp_wint, NULL, NULL
};

static struct cb_ops ecpp_cb_ops = {
	nodev,			/* cb_open */
	nodev,			/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&ecpp_str_info,	/* cb_stream */
	(int)(D_NEW | D_MP)	/* cb_flag */
};

/*
 * Declare ops vectors for auto configuration.
 */
struct dev_ops  ecpp_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	ecpp_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	ecpp_attach,		/* devo_attach */
	ecpp_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&ecpp_cb_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	nulldev			/* devo_power */
};

extern struct mod_ops mod_driverops;

static struct modldrv ecppmodldrv = {
	&mod_driverops,		/* type of module - driver */
	"pport driver:ecpp 2.56 99/10/08",
	&ecpp_ops,
};

static struct modlinkage ecppmodlinkage = {
	MODREV_1,
	&ecppmodldrv,
	0
};

int
_init(void)
{
	int    error;

	if ((error = mod_install(&ecppmodlinkage)) == 0) {
		(void) ddi_soft_state_init(&ecppsoft_statep,
		    sizeof (struct ecppunit), 1);
	}

	return (error);
}

int
_fini(void)
{
	int    error;

	if ((error = mod_remove(&ecppmodlinkage)) == 0)
		ddi_soft_state_fini(&ecppsoft_statep);

	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&ecppmodlinkage, modinfop));
}

static int
ecpp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int	instance;
	char	name[16];
	struct	ecppunit *pp;
	struct ddi_device_acc_attr attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;

	PTRACEINIT();		/* initialize tracing */

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME: {
		int current_mode;

		if (!(pp = ddi_get_soft_state(ecppsoft_statep, instance)))
			return (DDI_FAILURE);

		/*
		 * Initialize the chip and restore current mode
		 */
		current_mode = pp->current_mode;
		(void) ecpp_config_superio(pp);
		(void) ecpp_reset_port_regs(pp);
		ecpp_check_periph(pp);
		set_interrupt_characteristic(pp);
		(void) ecpp_mode_negotiation(pp, current_mode);

		mutex_enter(&pp->umutex);
		pp->suspended = FALSE;
		mutex_exit(&pp->umutex);

		return (DDI_SUCCESS);
	}
	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(ecppsoft_statep, instance) != 0)
		goto failed;

	pp = ddi_get_soft_state(ecppsoft_statep, instance);

	if (ddi_regs_map_setup(dip, 1, (caddr_t *)&pp->c_reg, 0,
			sizeof (struct config_reg), &attr,
			&pp->c_handle) != DDI_SUCCESS) {
		ecpp_error(dip, "ecpp_attach failed to map c_reg\n");
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&pp->i_reg, 0,
			sizeof (struct info_reg), &attr, &pp->i_handle)
			!= DDI_SUCCESS) {
		ecpp_error(dip, "ecpp_attach failed to map i_reg\n");
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 0, (caddr_t *)&pp->f_reg, 0x400,
			sizeof (struct fifo_reg), &attr, &pp->f_handle)
			!= DDI_SUCCESS) {
		ecpp_error(dip, "ecpp_attach failed to map f_reg\n");
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}

	if (ddi_regs_map_setup(dip, 2, (caddr_t *)&pp->dmac, 0,
			sizeof (struct cheerio_dma_reg), &attr,
			&pp->d_handle) != DDI_SUCCESS) {
		ecpp_error(dip, "ecpp_attach failed to map dmac\n");
		ecpp_remove_reg_maps(pp);
		return (DDI_FAILURE);
	}
	pp->attr.dma_attr_version = DMA_ATTR_V0;
	pp->attr.dma_attr_addr_lo = 0x00000000ull;
	pp->attr.dma_attr_addr_hi = 0xfffffffeull;
	pp->attr.dma_attr_count_max = 0xffffff;
	pp->attr.dma_attr_align = 1;
	pp->attr.dma_attr_burstsizes = 0x74;
	pp->attr.dma_attr_minxfer = 1;
	pp->attr.dma_attr_maxxfer = 0xffff;
	pp->attr.dma_attr_seg = 0xffff;
	pp->attr.dma_attr_sgllen = 1;
	pp->attr.dma_attr_granular = 1;

	if (ddi_dma_alloc_handle(dip, &pp->attr, DDI_DMA_DONTWAIT,
	    NULL, &pp->dma_handle) != DDI_SUCCESS)
		goto failed;
	pp->dip = dip;
	pp->msg = (mblk_t *)NULL;

	/* add interrupts */

	if (ddi_get_iblock_cookie(dip, 0,
	    &pp->ecpp_trap_cookie) != DDI_SUCCESS)  {
		ecpp_error(dip, "ddi_get_iblock_cookie FAILED \n");
		goto failed;
	}

	mutex_init(&pp->umutex, NULL, MUTEX_DRIVER,
	    (void *)pp->ecpp_trap_cookie);

	cv_init(&pp->pport_cv, NULL, CV_DRIVER, NULL);

	if (ddi_add_intr(dip, 0, &pp->ecpp_trap_cookie, NULL, ecpp_isr,
	    (caddr_t)pp) != DDI_SUCCESS) {
		ecpp_error(pp->dip, "ecpp_attach failed to add hard intr\n");
		goto remlock;
	}

	(void) sprintf(name, "ecpp%d", instance);

	if (ddi_create_minor_node(dip, name, S_IFCHR, instance, NULL,
	    NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(dip, NULL);
		goto remhardintr;
	}

	ddi_report_dev(dip);
	pp->ioblock = (caddr_t)kmem_alloc(IO_BLOCK_SZ, KM_SLEEP);

	ecpp_get_props(pp);

	if (ecpp_config_superio(pp) == FAILURE) {
		ecpp_error(pp->dip, "attach failed.\n");
		goto remhardintr;
	}

	ecpp_error(pp->dip, "attach ok.\n");

	return (DDI_SUCCESS);

remhardintr:
	ddi_remove_intr(dip, (uint_t)0, pp->ecpp_trap_cookie);

remlock:
	mutex_destroy(&pp->umutex);
	cv_destroy(&pp->pport_cv);

failed:
	ecpp_error(dip, "ecpp_attach:failed.\n");
	ecpp_remove_reg_maps(pp);

	return (DDI_FAILURE);
}

static int
ecpp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance;
	struct ecppunit *pp;

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		if (!(pp = ddi_get_soft_state(ecppsoft_statep, instance)))
			return (DDI_FAILURE);

		mutex_enter(&pp->umutex);
		ASSERT(!pp->suspended);

		pp->suspended = TRUE;	/* prevent new transfers */

		/*
		 * Wait if there's any activity on the port
		 */
		if ((pp->e_busy == ECPP_BUSY) || (pp->e_busy == ECPP_FLUSH)) {
			(void) cv_timedwait(&pp->pport_cv, &pp->umutex,
			    ddi_get_lbolt() +
			    SUSPEND_TOUT * drv_usectohz(1000000));
			if ((pp->e_busy == ECPP_BUSY) ||
			    (pp->e_busy == ECPP_FLUSH)) {
				mutex_exit(&pp->umutex);
				ecpp_error(pp->dip,
					"ecpp_detach: suspend timeout\n");
				return (DDI_FAILURE);
			}
		}

		mutex_exit(&pp->umutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (!(pp = ddi_get_soft_state(ecppsoft_statep, instance))) {
		ecpp_error(pp->dip,
		    "ecpp_detach: instance not initialized\n");
		return (DDI_FAILURE);
	}

	if (pp->dma_handle != NULL) {
		ecpp_error(pp->dip, "ecpp_detach\n");
		ddi_dma_free_handle(&pp->dma_handle);
	}

	ddi_remove_minor_node(dip, NULL);

	ddi_remove_intr(dip, (uint_t)0, pp->ecpp_trap_cookie);

	cv_destroy(&pp->pport_cv);

	mutex_destroy(&pp->umutex);

	ecpp_remove_reg_maps(pp);

	kmem_free(pp->ioblock, IO_BLOCK_SZ);

	return (DDI_SUCCESS);

}

/*
 * ecpp_get_props() reads ecpp.conf for user defineable tuneables.
 * If the file or a particular variable is not there, a default value
 * is assigned.
 */

static void
ecpp_get_props(struct ecppunit *pp)
{
	char *prop_ptr;

	/*
	 * If fast_centronics is TRUE, non-compliant IEEE 1284
	 * peripherals ( Centronics peripherals) will operate in DMA mode.
	 * Transfers betwee main memory and the device will be via DMA;
	 * peripheral handshaking will be conducted by superio logic.
	 * If ecpp can not read the variable correctly fast_centronics will
	 * be set to FALSE.  In this case, transfers and handshaking
	 * will be conducted by PIO for Centronics devices.
	 */
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, pp->dip, 0,
		"fast-centronics", &prop_ptr) == DDI_PROP_SUCCESS) {
		if (strcmp(prop_ptr, "true") == 0)
			pp->fast_centronics = TRUE;
		else
			pp->fast_centronics = FALSE;

		ddi_prop_free(prop_ptr);
	}
	else
		pp->fast_centronics = FALSE;

	/*
	 * If fast-1284-compatible is set to TRUE, when ecpp communicates
	 * with IEEE 1284 compliant peripherals, data transfers between
	 * main memory and the parallel port will be conducted by DMA.
	 * Handshaking between the port and peripheral will be conducted
	 * by superio logic.  This is the default characteristic.  If
	 * fast-1284-compatible is set to FALSE, transfers and handshaking
	 * will be conducted by PIO.
	 */

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, pp->dip, 0,
		"fast-1284-compatible", &prop_ptr) == DDI_PROP_SUCCESS) {
		if (strcmp(prop_ptr, "true") == 0)
			pp->fast_compat = TRUE;
		else
			pp->fast_compat = FALSE;

		ddi_prop_free(prop_ptr);
	}
	else
		pp->fast_compat = TRUE;


	/*
	 * Some centronics peripherals require the nInit signal to be
	 * toggled to reset the device.  If centronics_init_seq is set
	 * to TRUE, ecpp will toggle the nInit signal upon every ecpp_open().
	 * Applications have the opportunity to toggle the nInit signal
	 * with ioctl(2) calls as well.  The default is to set it to FALSE.
	 */
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, pp->dip, 0,
		"centronics-init-seq", &prop_ptr) == DDI_PROP_SUCCESS) {

		if (strcmp(prop_ptr, "true") == 0)
			pp->init_seq = TRUE;
		else
			pp->init_seq = FALSE;

		ddi_prop_free(prop_ptr);
	}
	else
		pp->init_seq = FALSE;

	/*
	 * If one of the centronics status signals are in an erroneous
	 * state, ecpp_wsrv() will be reinvoked centronics-retry ms to
	 * check if the status is ok to transfer.  If the property is not
	 * found, wsrv_retry will be set to CENTRONICS_RETRY ms.
	 */
	pp->wsrv_retry = ddi_prop_get_int(DDI_DEV_T_ANY, pp->dip, 0,
			"centronics-retry", CENTRONICS_RETRY);

	/*
	 * In PIO mode, ecpp_isr() will loop for wait for the busy signal
	 * to be deasserted before transferring the next byte. wait_for_busy
	 * is specificied in microseconds.  If the property is not found
	 * ecpp_isr() will wait for a maximum of WAIT_FOR_BUSY us.
	 */
	pp->wait_for_busy = ddi_prop_get_int(DDI_DEV_T_ANY, pp->dip, 0,
			"centronics-wait-for-busy", WAIT_FOR_BUSY);

	/*
	 * In PIO mode, centronics transfers must hold the data signals
	 * for a data_setup_time milliseconds before the strobe is asserted.
	 */
	pp->data_setup_time = ddi_prop_get_int(DDI_DEV_T_ANY, pp->dip, 0,
			"centronics-data-setup-time", DATA_SETUP_TIME);

	/*
	 * In PIO mode, centronics transfers asserts the strobe signal
	 * for a period of strobe_pulse_width milliseconds.
	 */
	pp->strobe_pulse_width = ddi_prop_get_int(DDI_DEV_T_ANY, pp->dip, 0,
			"centronics-strobe-pulse-width", STROBE_PULSE_WIDTH);

	/*
	 * Upon a transfer the peripheral, ecpp waits write_timeout seconds
	 * for the transmission to complete.
	 */
	pp->xfer_parms.write_timeout = ddi_prop_get_int(DDI_DEV_T_ANY,
		pp->dip, 0, "ecpp-transfer-timeout", ecpp_def_timeout);


	ecpp_error(pp->dip, "ecpp_get_prop:fast_centronics=%x,fast-1284=%x\n",
		pp->fast_centronics, pp->fast_compat);
	ecpp_error(pp->dip, "ecpp_get_prop:wsrv_retry=%d,wait_for_busy=%d\n",
		pp->wsrv_retry, pp->wait_for_busy);

	ecpp_error(pp->dip, "ecpp_get_prop:data_setup=%d,strobe_pulse=%d\n",
		pp->data_setup_time, pp->strobe_pulse_width);

	ecpp_error(pp->dip, "ecpp_get_prop:transfer-timeout=%d\n",
		pp->xfer_parms.write_timeout);

}


int
ecpp_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t	dev = (dev_t)arg;
	struct ecppunit *pp;
	int	instance, ret;

#if defined(lint)
	dip = dip;
#endif
	instance = getminor(dev);

	switch (infocmd) {
		case DDI_INFO_DEVT2DEVINFO:
			pp = (struct ecppunit *)
				ddi_get_soft_state(ecppsoft_statep, instance);
			*result = pp->dip;
			ret = DDI_SUCCESS;
			break;
		case DDI_INFO_DEVT2INSTANCE:
			*result = (void *)instance;
			ret = DDI_SUCCESS;
			break;
		default:
			ret = DDI_FAILURE;
			break;
	}

	return (ret);
}

/*ARGSUSED2*/
static int
ecpp_open(queue_t *q, dev_t *dev, int flag, int sflag, cred_t *credp)
{
	struct ecppunit *pp;
	int		instance;
	struct stroptions *sop;
	mblk_t		*mop;

	instance = getminor(*dev);
	if (instance < 0)
		return (ENXIO);
	pp = (struct ecppunit *)ddi_get_soft_state(ecppsoft_statep, instance);

	if (pp == NULL)
		return (ENXIO);

	mutex_enter(&pp->umutex);

	if ((pp->oflag == TRUE)) {
		ecpp_error(pp->dip, "ecpp open failed");
		mutex_exit(&pp->umutex);
		return (EBUSY);
	}

	pp->oflag = TRUE;

	mutex_exit(&pp->umutex);

	/* initialize state variables */
	pp->error_status = ECPP_NO_1284_ERR;
	pp->xfer_parms = default_xfer_parms;	/* structure assignment */

	pp->current_mode = ECPP_CENTRONICS;
	pp->backchannel = ECPP_CENTRONICS;
	pp->current_phase = ECPP_PHASE_PO;
	pp->port = ECPP_PORT_DMA;
	pp->instance = instance;
	pp->timeout_error = 0;
	pp->need_idle_state = FALSE;
	pp->no_more_fifo_timers = FALSE;
	pp->wsrv_timer = FALSE;
	pp->saved_dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	pp->ecpp_drain_counter = 0;
	pp->dma_cancelled = FALSE;
	pp->io_mode = ECPP_DMA;

	ecpp_joblen = 0;
	ecpp_cf_isr = 0;

	/* clear the state flag */
	pp->e_busy = ECPP_IDLE;

	if (!(mop = allocb(sizeof (struct stroptions), BPRI_MED))) {
		return (EAGAIN);
	}

	q->q_ptr = WR(q)->q_ptr = (caddr_t)pp;

	mop->b_datap->db_type = M_SETOPTS;
	mop->b_wptr += sizeof (struct stroptions);

	/*
	 * if device is open with O_NONBLOCK flag set, let read(2) return 0
	 * if no data waiting to be read.  Writes will block on flow control.
	 */
	sop = (struct stroptions *)mop->b_rptr;
	sop->so_flags = SO_HIWAT | SO_LOWAT | SO_NDELON;
	sop->so_hiwat = ECPPHIWAT;
	sop->so_lowat = ECPPLOWAT;

	/* enable the stream */
	qprocson(q);

	(void) putnext(q, mop);

	pp->readq = RD(q);
	pp->writeq = WR(q);
	pp->msg = (mblk_t *)NULL;

	if (ecpp_reset_port_regs(pp) == FAILURE)
		return (ENXIO);

	ecpp_check_periph(pp);

	ecpp_default_negotiation(pp);

	set_interrupt_characteristic(pp);

	/* check that phases are correct for corresponding mode */
	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
		if (pp->current_phase != ECPP_PHASE_C_IDLE) {
			ecpp_error(pp->dip, "ecpp_open:C: phase=%x\n",
				pp->current_phase);
		}
		break;
	case ECPP_NIBBLE_MODE:
		/*
		 * If back channel data must be read before interrupts
		 * are enabled and ecpp_open() completes.  ecpp_open()
		 * must leave the pport in ECPP_PHASE_NIBT_REVIDLE if
		 * this is a Nibble device.
		 */

		if (read_nibble_backchan(pp) == FAILURE) {
			ecpp_error(pp->dip,
				"ecpp_open: read_nibble_backchan failed.");
		}
		break;
	case ECPP_ECP_MODE:
		if (pp->current_phase != ECPP_PHASE_ECP_FWD_IDLE) {
			ecpp_error(pp->dip, "ecpp_open:E: phase=%x\n",
				pp->current_phase);
		}
		break;
	default:
		ecpp_error(pp->dip, "ecpp_open: incorrect current_mode=%x\n",
			pp->current_mode);
	}


	ecpp_error(pp->dip, "ecpp_open: current_mode=%x, current_phase=%x\n",
		pp->current_mode, pp->current_phase);
	ecpp_error(pp->dip, "ecpp_open: ecr=%x, dsr=%x, dcr=%x\n",
		PP_GETB(pp->f_handle, &pp->f_reg->ecr),
		PP_GETB(pp->i_handle, &pp->i_reg->dsr),
		PP_GETB(pp->i_handle, &pp->i_reg->dcr));
	return (0);
}

/*ARGSUSED1*/
static int
ecpp_close(queue_t *q, int flag, cred_t *cred_p)
{
	struct ecppunit *pp;
	uchar_t break_out;

	pp = (struct ecppunit *)q->q_ptr;

	ecpp_error(pp->dip, "ecpp_close: entering ...\n");

	break_out = FALSE;

	mutex_enter(&pp->umutex);

	/* wait until all output activity has ceased, or SIG */
	do {
		/*
		 * ecpp_close() will continue to loop until the
		 * queue has been drained or if the thread
		 * has received a SIG.  Typically, when the queue
		 * has data, the port will be ECPP_BUSY.  However,
		 * after a dma completes and before the wsrv
		 * starts the next transfer, the port may be IDLE.
		 * In this case, ecpp_close() will loop within this
		 * while(qsize) segment.  Since, ecpp_wsrv() runs
		 * at software interupt level, this shouldn't loop
		 * very long.
		 */

		/* stop closing during active DMA */
		while (pp->e_busy == ECPP_BUSY) {
			ecpp_error(pp->dip, "ecpp_close: ECPP_BUSY\n");
			if (!cv_wait_sig(&pp->pport_cv, &pp->umutex)) {
				ecpp_error(pp->dip,
					"ecpp_close:B: received SIG\n");
				/*
				 * Returning from a signal such as
				 * SIGTERM or SIGKILL
				 */
				ecpp_flush(pp, FWRITE);
				break_out = TRUE;
				break;
			} else
				ecpp_error(pp->dip, "ecpp_close:B: non-sig\n");

		}
		ecpp_error(pp->dip, "ecpp_close:BUSY: done\n");

		/* If a SIG was sent to this thread, do no block */
		if (break_out == TRUE) break;

		/* stop closing if bad status */
		while (pp->e_busy == ECPP_ERR) {
			ecpp_error(pp->dip, "ecpp_close: ECPP_ERR\n");
			if (!cv_wait_sig(&pp->pport_cv, &pp->umutex)) {
				ecpp_error(pp->dip,
					"ecpp_close:E: received SIG\n");

				/*
				 * Returning from a signal such as
				 * SIGTERM or SIGKILL
				 */
				ecpp_flush(pp, FWRITE);
				break_out = TRUE;
				break;
			} else
				ecpp_error(pp->dip, "ecpp_close:E: non-sig\n");
		}
		/* If a SIG was sent to this thread, do no block */
		if (break_out == TRUE) break;

	} while (qsize(q) || pp->e_busy != ECPP_IDLE);

	qprocsoff(q);

	ecpp_error(pp->dip,
		"ecpp_close:ecpp_joblen=%d, ecpp_cf_isr=%d,qsize(q) =%d\n",
		ecpp_joblen, ecpp_cf_isr, qsize(q));

	(void) untimeout(pp->timeout_id);
	(void) untimeout(pp->fifo_timer_id);
	(void) untimeout(pp->wsrv_timer_id);

	/* set link to Compatible mode */
	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
		if (pp->io_mode == ECPP_DMA)
			set_chip_pio(pp);

		if (pp->current_phase != ECPP_PHASE_C_FWD_DMA) {
			ecpp_error(pp->dip, "ecpp_close: phase=%x\n",
				pp->current_phase);
		}
		break;
	case ECPP_DIAG_MODE:
		if (pp->current_phase != ECPP_PHASE_C_FWD_DMA) {
			ecpp_error(pp->dip, "ecpp_close: phase=%x\n",
				pp->current_phase);
		}
		break;
	case ECPP_NIBBLE_MODE:
		if (pp->current_phase != ECPP_PHASE_NIBT_REVIDLE) {
			ecpp_error(pp->dip, "ecpp_close: nibble bad");
		} else if (ecpp_terminate_phase(pp) == FAILURE) {
			ecpp_error(pp->dip,
				"ecpp_close: nibble ecpp_terminate bad");
			reset_to_centronics(pp);
			pp->current_mode = ECPP_FAILURE_MODE;
		}
		break;
	case ECPP_ECP_MODE:
		if (pp->current_phase != ECPP_PHASE_ECP_FWD_IDLE) {
			ecpp_error(pp->dip, "ecpp_close: ecp bad");
		} else if (ecpp_terminate_phase(pp) == FAILURE) {
			ecpp_error(pp->dip,
				"ecpp_close: ecp ecpp_terminate bad");
			reset_to_centronics(pp);
			pp->current_mode = ECPP_FAILURE_MODE;
		}
		break;
	default:
		ecpp_error(pp->dip, "ecpp_close: bad current_mode");
	}

	pp->oflag = FALSE;
	q->q_ptr = WR(q)->q_ptr = NULL;
	pp->msg = (mblk_t *)NULL;

	ecpp_error(pp->dip, "ecpp_close: ecr=%x, dsr=%x, dcr=%x\n",
		PP_GETB(pp->f_handle, &pp->f_reg->ecr),
		PP_GETB(pp->i_handle, &pp->i_reg->dsr),
		PP_GETB(pp->i_handle, &pp->i_reg->dcr));

	mutex_exit(&pp->umutex);

	PTRACE(ecpp_close, 'SOLC', pp);
	return (0);
}

static void
set_interrupt_characteristic(struct ecppunit *pp)
{
	/*
	 * This routine should distinquish between superio type
	 * when such properites are added to the PROM.  For now,
	 * assumes PC87332.
	 *
	 * Routine must be called after ecpp_default_negotiation()
	 * where io_mode is set.
	 */
	if (pp->io_mode == ECPP_PIO && pp->current_mode != ECPP_DIAG_MODE)
		write_config_reg(pp, PCR, 0x04);
	else
		write_config_reg(pp, PCR, 0x14);
}


/*
 * standard put procedure for ecpp
 */
static int
ecpp_wput(queue_t *q, mblk_t *mp)
{
	struct msgb *nmp;
	struct ecppunit *pp;

	pp = (struct ecppunit *)q->q_ptr;

	if (!mp)
		return (0);

	if ((mp->b_wptr - mp->b_rptr) <= 0) {
		ecpp_error(pp->dip,
			"ecpp_wput:bogus packet recieved mp=%x\n", mp);
		freemsg(mp);
		return (ENOSR);
	}

	switch (DB_TYPE(mp)) {

	case M_DATA:

		/*
		 * This is a quick fix for multiple message block problem,
		 * it will be changed later with better performance code.
		 */

		if (mp->b_cont) {
			/* mblk has scattered data ... do msgpullup */
			nmp = msgpullup(mp, -1);
			freemsg(mp);
			mp = nmp;
			ecpp_error(pp->dip,
			    "ecpp_wput:msgpullup: mp=%x len=%d b_cont=%x\n",
			    mp, mp->b_wptr - mp->b_rptr, mp->b_cont);
		}

		if (canput(q))
			(void) putq(q, mp);
		else {
			ecpp_error(pp->dip,
				"ecpp_wput:wput failed, should not occur\n");
				freemsg(mp);
				return (ENOSR);
		}

		break;

	case M_CTL:

		(void) putq(q, mp);

		break;

	case M_IOCTL:
		ecpp_error(pp->dip, "ecpp_wput:M_IOCTL\n");

		if (pp->current_mode == ECPP_DIAG_MODE &&
		    pp->e_busy == ECPP_BUSY)
			(void) putq(q, mp);
		else
			ecpp_putioc(q, mp);

		break;

	case M_IOCDATA:
	{
		struct copyresp *csp;
			ecpp_error(pp->dip, "ecpp_wput:M_IOCDATA\n");

		csp = (struct copyresp *)mp->b_rptr;

		/*
		 * If copy request failed, quit now
		 */
		if (csp->cp_rval != 0) {
			freemsg(mp);
			return (0);
		}

		switch (csp->cp_cmd) {
		case ECPPIOC_SETPARMS:
		case ECPPIOC_SETREGS:
		case ECPPIOC_SETPORT:
		case ECPPIOC_SETDATA:
			/*
			 * need to retrieve and use the data, but if the
			 * device is busy, wait.
			 */
			(void) putq(q, mp);
			break;

		case ECPPIOC_GETPARMS:
		case ECPPIOC_GETREGS:
		case ECPPIOC_GETPORT:
		case ECPPIOC_GETDATA:
		case BPPIOC_GETERR:
		case BPPIOC_TESTIO:
			/* data transfered to user space okay */
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		break;
	}

	case M_FLUSH:

		ecpp_error(pp->dip, "ecpp_wput:M_FLUSH\n");
		switch (*(mp->b_rptr)) {
			case FLUSHRW:
				mutex_enter(&pp->umutex);
				ecpp_flush(pp, (FREAD | FWRITE));
				mutex_exit(&pp->umutex);
				*(mp->b_rptr) = FLUSHR;
				qreply(q, mp);
				break;
			case FLUSHR:
				mutex_enter(&pp->umutex);
				ecpp_flush(pp, FREAD);
				mutex_exit(&pp->umutex);
				qreply(q, mp);
				break;
			case FLUSHW:
				mutex_enter(&pp->umutex);
				ecpp_flush(pp, FWRITE);
				mutex_exit(&pp->umutex);
				freemsg(mp);
				break;

			default:
			break;
		}
		break;

	default:
		ecpp_error(pp->dip, "ecpp_wput: bad messagetype 0x%x\n",
		    DB_TYPE(mp));
		freemsg(mp);
		break;
	}

	return (0);
}

static uchar_t
ecpp_get_error_status(uchar_t status)
{
	uchar_t pin_status = 0;


	if (!(status & ECPP_nERR)) {
		pin_status |= BPP_ERR_ERR;
	}

	if (status & ECPP_PE) {
		pin_status |= BPP_PE_ERR;
	}

	if (!(status & ECPP_SLCT)) {
		pin_status |= (BPP_ERR_ERR | BPP_SLCT_ERR);
	}

	if (!(status & ECPP_nBUSY)) {
		pin_status |= (BPP_SLCT_ERR);
	}

	return (pin_status);
}

/*
 * ioctl handler for output PUT procedure.
 */
static void
ecpp_putioc(queue_t *q, mblk_t *mp)
{
	struct iocblk	*iocbp;
	struct ecppunit *pp;
	int count = 0;

	pp = (struct ecppunit *)q->q_ptr;

	iocbp = (struct iocblk *)mp->b_rptr;

	PTRACE(ecpp_putioc, 'iocb', iocbp);

	/* I_STR ioctls are invalid */
	if (iocbp->ioc_count != TRANSPARENT) {
		ecpp_nack_ioctl(q, mp, EINVAL);
		return;
	}

	PTRACE(ecpp_putioc, 'bcio', iocbp->ioc_cmd);
	switch (iocbp->ioc_cmd) {
	case ECPPIOC_SETPARMS:
	{
	PTRACE(ecpp_putioc, 'Acio', iocbp->ioc_cmd);
		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (struct ecpp_transfer_parms), iocbp->ioc_flag);
		break;
	}
	case ECPPIOC_GETPARMS:
	{
		caddr_t uaddr;
		struct ecpp_transfer_parms *xfer_parms;

		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont =
			allocb(sizeof (struct ecpp_transfer_parms), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		xfer_parms = (struct ecpp_transfer_parms *)mp->b_cont->b_rptr;
		mutex_enter(&pp->umutex);
		xfer_parms->write_timeout = pp->xfer_parms.write_timeout;
		xfer_parms->mode = pp->xfer_parms.mode = pp->current_mode;
		mutex_exit(&pp->umutex);

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (struct ecpp_transfer_parms);

		ecpp_copyout(q, mp, uaddr,
			sizeof (struct ecpp_transfer_parms), iocbp->ioc_flag);

		break;
	}
	case ECPPIOC_SETREGS:
	{
	PTRACE(ecpp_putioc, 'Ccio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (struct ecpp_regs), iocbp->ioc_flag);
		break;
	}
	case ECPPIOC_GETREGS:
	{
		caddr_t uaddr;
		struct ecpp_regs *rg;

	PTRACE(ecpp_putioc, 'Dcio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont = allocb(sizeof (struct ecpp_regs), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		rg = (struct ecpp_regs *)mp->b_cont->b_rptr;

		mutex_enter(&pp->umutex);

		rg->dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		rg->dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

		ecpp_error(pp->dip, "ECPPIOC_GETREGS: dsr=%x,dcr=%x\n",
		    rg->dsr, rg->dcr);
		mutex_exit(&pp->umutex);

		rg->dsr |= 0x07;  /* bits 0,1,2 must be 1 */
		rg->dcr |= 0xf0;  /* bits 4 - 7 must be 1 */

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (struct ecpp_regs);

		ecpp_copyout(q, mp, uaddr, sizeof (struct ecpp_regs),
			iocbp->ioc_flag);

		break;
	}

	case ECPPIOC_SETPORT:
	case ECPPIOC_SETDATA:
	{
	PTRACE(ecpp_putioc, 'Ecio', iocbp->ioc_cmd);
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/*
		 * each of the commands fetches a byte quantity.
		 */
		ecpp_copyin(q, mp, *(caddr_t *)(void *)mp->b_cont->b_rptr,
			sizeof (uchar_t), iocbp->ioc_flag);
		break;
	}

	case ECPPIOC_GETDATA:
	case ECPPIOC_GETPORT:
	{
		caddr_t uaddr;
		uchar_t *port;

		PTRACE(ecpp_putioc, 'Fcio', iocbp->ioc_cmd);
		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}


		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont = allocb(sizeof (uchar_t), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		port = (uchar_t *)mp->b_cont->b_rptr;

		if (iocbp->ioc_cmd == ECPPIOC_GETPORT)
			*port = pp->port;

		else if (iocbp->ioc_cmd == ECPPIOC_GETDATA) {
			mutex_enter(&pp->umutex);
			while (get_dmac_bcr(pp) != 0 &&
			    pp->e_busy == ECPP_BUSY && count < 10000) {
				drv_usecwait(10);
				PTRACE(ecpp_putioc, 'YSUB', pp->e_busy);
				count++;
			}

			switch (pp->port) {
			case ECPP_PORT_PIO:
				*port = PP_GETB(pp->i_handle,
					&pp->i_reg->ir.datar);
				PTRACE(ecpp_putioc, 'PIO ', *port);
				break;
			case ECPP_PORT_TDMA:
				*port = PP_GETB(pp->f_handle,
					&pp->f_reg->fr.tfifo);
				PTRACE(ecpp_putioc, 'TDMA', *port);
				break;
			default:
				ecpp_nack_ioctl(q, mp, EINVAL);
				break;
			}
			mutex_exit(&pp->umutex);
		} else {
			ecpp_error(pp->dip, "wierd command");
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
			sizeof (uchar_t);

		ecpp_copyout(q, mp, uaddr, sizeof (uchar_t), iocbp->ioc_flag);

		break;
	}

	case BPPIOC_GETERR:
	{
		caddr_t uaddr;
		struct bpp_error_status *bpp_status;

	PTRACE(ecpp_putioc, 'Gcio', iocbp->ioc_cmd);
		/* Get the user buffer address */
		uaddr = *(caddr_t *)(void *)mp->b_cont->b_rptr;

		freemsg(mp->b_cont);

		mp->b_cont =
			allocb(sizeof (struct bpp_error_status), BPRI_MED);

		if (mp->b_cont == NULL) {
			ecpp_nack_ioctl(q, mp, ENOSR);
			break;
		}

		bpp_status = (struct bpp_error_status *)mp->b_cont->b_rptr;
		bpp_status->timeout_occurred = pp->timeout_error;
		bpp_status->bus_error = 0;	/* not used */
		bpp_status->pin_status = ecpp_get_error_status(pp->saved_dsr);

		mp->b_cont->b_wptr = mp->b_cont->b_rptr +
		    sizeof (struct bpp_error_status);

		ecpp_copyout(q, mp, uaddr, sizeof (struct bpp_error_status),
				iocbp->ioc_flag);

		break;
	}

	case BPPIOC_TESTIO:
	{
	PTRACE(ecpp_putioc, 'Hcio', iocbp->ioc_cmd);
		if (!((pp->current_mode == ECPP_CENTRONICS) ||
				(pp->current_mode == ECPP_COMPAT_MODE))) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		mutex_enter(&pp->umutex);

		pp->saved_dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

		if ((pp->saved_dsr & ECPP_PE) ||
		    !(pp->saved_dsr & ECPP_SLCT) ||
		    !(pp->saved_dsr & ECPP_nERR)) {
			ecpp_nack_ioctl(q, mp, EIO);
		} else {
			ecpp_ack_ioctl(q, mp);
		}

		mutex_exit(&pp->umutex);

		break;
	}

	default:
	PTRACE(ecpp_putioc, 'Icio', iocbp->ioc_cmd);
		ecpp_nack_ioctl(q, mp, EINVAL);
		break;
	}
}

static int
ecpp_wsrv(queue_t *q)
{
	struct ecppunit *pp;
	struct msgb *mp, *mp1;
	struct msgb *omp;
	size_t len, total_len, starting, my_ioblock_sz;
	caddr_t my_ioblock;

	pp = (struct ecppunit *)q->q_ptr;

	/* ecpp_error(pp->dip, "ecpp_wsrv\n"); */
	starting = 0;

	mutex_enter(&pp->umutex);

	/* Do not start new transfer if going to suspend */
	if (pp->suspended)
		return (0);

	if (pp->io_mode == ECPP_DMA)
		my_ioblock_sz = 0x8000;  /* 32 KBytes */
	else
		my_ioblock_sz = IO_BLOCK_SZ; /* 128 KBytes */
	/*
	 * if channel is actively doing work, wait till completed
	 */
	if (pp->e_busy == ECPP_BUSY || pp->e_busy == ECPP_FLUSH ||
			get_dmac_bcr(pp) != 0) {
		/*
		 * ecpp_error(pp->dip, "ecpp_wsrv:BUSY:bcr=%d\n",
		 * get_dmac_bcr(pp));
		 */
		mutex_exit(&pp->umutex);
		return (0);
	}

	if (pp->e_busy == ECPP_ERR) {
		if (ecpp_check_status(pp) == FAILURE) {
			if (pp->wsrv_timer == FALSE) {
				ecpp_error(pp->dip,
					"ecpp_wsrv:start wrsv_timer.\n");
				pp->wsrv_timer_id = timeout(ecpp_wsrv_timer,
					(caddr_t)pp,
					    drv_usectohz(pp->wsrv_retry*1000));

			} else
				ecpp_error(pp->dip,
					"ecpp_wsrv: wrsv_timer=TRUE.\n");

			pp->wsrv_timer = TRUE;
			mutex_exit(&pp->umutex);
			return (0);
		} else {
			/*
			 * Peripheral's error condition has corrected.
			 * Notify ecpp_close() in case it is waiting.
			 */
			pp->e_busy = ECPP_IDLE;
			cv_signal(&pp->pport_cv);
		}
	}

	len = total_len = 0;
	my_ioblock = pp->ioblock;

	/*
	 * The following While loop is implemented to gather the
	 * many small writes that the lp subsystem makes and
	 * compile them into one large dma transfer. The len and
	 * total_len variables are a running count of the number of
	 * bytes that have been gathered. They are bcopied to the
	 * dmabuf buffer. my_dmabuf is a pointer that gets incremented
	 * each time we add len to the buffer. the pp->e_busy state
	 * flag is set to E_BUSY as soon as we start gathering packets
	 * because if not there is a possibility that we could get to
	 * to the close routine asynchronously and free up some of the
	 * data that we are currently transferring.
	 */

	while (mp = getq(q)) {
		switch (DB_TYPE(mp)) {
		case M_DATA:
			if (mp->b_cont) {
				/* mblk has scattered data ... do msgpullup */
				mp1 = msgpullup(mp, -1);
				freemsg(mp);
				mp = mp1;
				ecpp_error(pp->dip,
				    "ecpp_wsrv:mpull:mp=%x len=%d b_cont=%x\n",
				    mp, mp->b_wptr - mp->b_rptr, mp->b_cont);
			}


			len = mp->b_wptr - mp->b_rptr;
			if (!total_len && len >= my_ioblock_sz) {
				pp->e_busy = ECPP_BUSY;
				starting ++;
				ecpp_error(pp->dip, "start 1,len=%d\n", len);

				ecpp_start(q, mp, (caddr_t)mp->b_rptr, len);
				mutex_exit(&pp->umutex);
				return (1);
			}

			if (total_len + len <= my_ioblock_sz) {
				pp->e_busy = ECPP_BUSY;
				bcopy((void *)mp->b_rptr,
				    (void *)my_ioblock, len);
				my_ioblock += len;
				total_len += len;
				starting++;
				freemsg(mp);
				omp = (mblk_t *)NULL;
				break;
			} else {
				(void) putbq(q, mp);
				pp->e_busy = ECPP_BUSY;
				starting++;
				ecpp_error(pp->dip,
				    "start 2, total_len=%d\n", total_len);

				ecpp_start(q, omp, pp->ioblock, total_len);
				mutex_exit(&pp->umutex);
				return (1);
			}


		case M_IOCTL:
			ecpp_error(pp->dip, "M_IOCTL.\n");
			mutex_exit(&pp->umutex);

			ecpp_putioc(q, mp);

			mutex_enter(&pp->umutex);

			break;

		case M_IOCDATA:
			ecpp_error(pp->dip, "M_IOCDATA\n");

		{
			struct copyresp *csp;

			csp = (struct copyresp *)mp->b_rptr;

			/*
			 * If copy request failed, quit now
			 */
			if (csp->cp_rval != 0) {
				freemsg(mp);
				break;
			}

			switch (csp->cp_cmd) {
			case ECPPIOC_SETPARMS:
			case ECPPIOC_SETREGS:
			case ECPPIOC_SETPORT:
			case ECPPIOC_SETDATA:

				ecpp_srvioc(q, mp);
				break;
			default:
				ecpp_nack_ioctl(q, mp, EINVAL);
				break;
			}

			break;
		}
		case M_CTL:
			ecpp_error(pp->dip, "M_CTL\n");

			/* This should be a backchannel request. */
			(void) ecpp_peripheral2host(pp);

			/*
			 * If M_CTL was last mblk, we need to
			 * wake up ecpp_close().
			 */

			if (!qsize(pp->writeq))
				cv_signal(&pp->pport_cv);

			break;
		default:
			printf("ecpp: should never get here\n");
			freemsg(mp);
			break;
		}
	}

	ecpp_error(pp->dip, "wsrv...finishing:starting=%x,ebusy=%x\n",
		starting, pp->e_busy);

	if (starting == 0) {
		/* IDLE if xfer_timeout, or FIFO_EMPTY */
		if (pp->e_busy == ECPP_IDLE) {
			if (ecpp_idle_phase(pp) == FAILURE) {
				pp->error_status = ECPP_1284_ERR;
				ecpp_error(pp->dip,
				    "ecpp_wsrv:idle FAILED.\n");
				ecpp_error(pp->dip,
				    "ecpp_wsrv:idle:ecr=%x, dsr=%x, dcr=%x\n",
				    PP_GETB(pp->f_handle, &pp->f_reg->ecr),
				    PP_GETB(pp->i_handle, &pp->i_reg->dsr),
				    PP_GETB(pp->i_handle, &pp->i_reg->dcr));
			}
		} else
			pp->need_idle_state = TRUE;
	}

	if (total_len != 0) {
		pp->e_busy = ECPP_BUSY;
		ecpp_error(pp->dip, "start 3, total_len=%d\n", total_len);

		ecpp_start(q, omp, pp->ioblock, total_len);
	}

	ecpp_error(pp->dip, "wsrv:done.\n");
	mutex_exit(&pp->umutex);
	return (1);
}

/*
 * Ioctl processor for queued ioctl data transfer messages.
 */
static void
ecpp_srvioc(queue_t *q, mblk_t *mp)
{
	struct iocblk	*iocbp;
	struct ecppunit *pp;

	iocbp = (struct iocblk *)mp->b_rptr;
	pp = (struct ecppunit *)q->q_ptr;

	PTRACE(ecpp_srvioc, 'SRVI', pp);

	switch (iocbp->ioc_cmd) {
	case ECPPIOC_SETPARMS:
	{
		struct ecpp_transfer_parms *xferp;

		xferp = (struct ecpp_transfer_parms *)mp->b_cont->b_rptr;


		if (xferp->write_timeout <= 0 ||
				xferp->write_timeout >= ECPP_MAX_TIMEOUT) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		if (!((xferp->mode == ECPP_CENTRONICS) ||
			(xferp->mode == ECPP_COMPAT_MODE) ||
			(xferp->mode == ECPP_NIBBLE_MODE) ||
			(xferp->mode == ECPP_ECP_MODE) ||
			(xferp->mode == ECPP_DIAG_MODE))) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		pp->xfer_parms = *xferp;

		ecpp_error(pp->dip,
			"srvioc: current_mode =%x new mode=%x\n",
			pp->current_mode, pp->xfer_parms.mode);
		if (ecpp_mode_negotiation(pp, pp->xfer_parms.mode) == FAILURE) {
			ecpp_nack_ioctl(q, mp, EPROTONOSUPPORT);
		    } else {
			/*
			 * mode nego was a success.  If nibble mode check
			 * back channel and set into REVIDLE.
			 */
			if (pp->current_mode == ECPP_NIBBLE_MODE) {
				if (read_nibble_backchan(pp) == FAILURE) {
					/*
					 * problems reading the backchannel
					 * returned to centronics;
					 * ioctl fails.
					 */
					ecpp_nack_ioctl(q, mp, EPROTONOSUPPORT);
					break;
				}
			}

			ecpp_ack_ioctl(q, mp);
		}
		if (pp->current_mode != ECPP_DIAG_MODE)
		    pp->port = ECPP_PORT_DMA;
		else
		    pp->port = ECPP_PORT_PIO;

		pp->xfer_parms.mode = pp->current_mode;

		break;
	}

	case ECPPIOC_SETREGS:
	{
		struct ecpp_regs *rg;
		uint8_t dcr;

		rg = (struct ecpp_regs *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* bits 4-7 must be 1 or return EINVAL */
		if ((rg->dcr & 0xf0) != 0xf0) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		/* get the old dcr */
		dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
		/* get the new dcr */
		dcr = (dcr & 0xf0) | (rg->dcr & 0x0f);
		ecpp_error(pp->dip, "ECPPIOC_SETREGS:dcr=%x\n", dcr);
		PP_PUTB(pp->i_handle, &pp->i_reg->dcr, dcr);
		ecpp_ack_ioctl(q, mp);
		break;
	}
	case ECPPIOC_SETPORT:
	{
		uchar_t *port;

		port = (uchar_t *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		PTRACE(ecpp_srvioc, 'SETP', *port);
		switch (*port) {
		case ECPP_PORT_PIO:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_001 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			pp->port = *port;
			PTRACE(ecpp_srvioc, ' OIP', pp);
			ecpp_ack_ioctl(q, mp);
			break;

		case ECPP_PORT_TDMA:
			/* change to mode 110 */
			PP_PUTB(pp->f_handle, & pp->f_reg->ecr,
				(ECPP_DMA_ENABLE | ECPP_INTR_MASK
				| ECR_mode_110 | ECPP_FIFO_EMPTY));
			pp->port = *port;
			PTRACE(ecpp_srvioc, 'AMDT', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
		}

		break;
	}
	case ECPPIOC_SETDATA:
	{
		uchar_t *data;

		data = (uchar_t *)mp->b_cont->b_rptr;

		/* must be in diagnostic mode for these commands to work */
		if (pp->current_mode != ECPP_DIAG_MODE) {
			ecpp_nack_ioctl(q, mp, EINVAL);
			break;
		}

		switch (pp->port) {
		case ECPP_PORT_PIO:
			PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, *data);
			PTRACE(ecpp_srvioc, '1OIP', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		case ECPP_PORT_TDMA:
			PP_PUTB(pp->f_handle, &pp->f_reg->fr.tfifo, *data);
			PTRACE(ecpp_srvioc, 'amdt', pp);
			ecpp_ack_ioctl(q, mp);
			break;
		default:
			ecpp_nack_ioctl(q, mp, EINVAL);
		}

		break;
	}

	default:		/* unexpected ioctl type */
		ecpp_nack_ioctl(q, mp, EINVAL);
		break;
	}
}

static void
ecpp_flush(struct ecppunit *pp, int cmd)
{
	queue_t		*q;

	/* mutex must be held */

	if (!(cmd & FWRITE))
		return;

	ecpp_error(pp->dip, "FWRITE FLUSH.\n");
	q = pp->writeq;

	ecpp_error(pp->dip, "ecpp_flush e_busy=%x\n", pp->e_busy);

	/* if there is an ongoing DMA, it needs to be turned off. */
	switch (pp->e_busy) {

	case ECPP_BUSY:
		/*
		 * Change the port status to ECPP_FLUSH to
		 * indicate to ecpp_wsrv that the wq is being flushed.
		 */
		pp->e_busy = ECPP_FLUSH;

		/*
		 * dma_cancelled indicates to ecpp_isr() that we have
		 * turned off the DMA.  Since the mutex is held, ecpp_isr()
		 * may be blocked.  Once ecpp_flush() finishes and ecpp_isr()
		 * gains the mutex, ecpp_isr() will have a _reset_ DMAC.  Most
		 * significantly, the DMAC will be reset after ecpp_isr() was
		 * invoked.  Therefore we need to have a flag "dma_cancelled"
		 * to signify when the described condition has occured.  If
		 * ecpp_isr() notes a dma_cancelled, it will ignore the DMAC csr
		 * and simply claim the interupt.
		 */

		pp->dma_cancelled = TRUE;

		/*
		 * if the bcr is zero, then DMA is complete and
		 * we are waiting for the fifo to drain.  Therefore,
		 * turn off dma.
		 */

		if (get_dmac_bcr(pp))
			if (!ecpp_stop_dma(pp))
				ecpp_error(pp->dip,
					"ecpp_flush: ecpp_stop_dma FAILED.\n");

		/*
		 * If the status of the port is ECPP_BUSY, at this point,
		 * the DMA is stopped by either explicitly above, or by
		 * ecpp_isr() but the FIFO hasn't drained yet.  In either
		 * case, we need to unbind the dma mappings.  Resetting
		 * the DMAC to a known state is wise thing to do at this
		 * point as well.
		 */

		if (!ecpp_reset_unbind_dma(pp))
			ecpp_error(pp->dip,
				"ecpp_flush: ecpp_reset_unbind FAILED.\n");

		/*
		 * The transfer is cleaned up.  There may or may not be data
		 * in the fifo.  We don't care at this point.  Ie. SuperIO may
		 * transfer the remaining bytes in the fifo or not. it doesn't
		 * matter.  All that is important at this stage is that no more
		 * fifo timers are started.
		 */

		pp->no_more_fifo_timers = TRUE;

		/* clean up the ongoing timers */
		pp->about_to_untimeout = 1;
		mutex_exit(&pp->umutex);

		(void) untimeout(pp->timeout_id);

		mutex_enter(&pp->umutex);
		pp->about_to_untimeout = 0;
		break;

	case ECPP_ERR:
		/*
		 * Change the port status to ECPP_FLUSH to
		 * indicate to ecpp_wsrv that the wq is being flushed.
		 */
		pp->e_busy = ECPP_FLUSH;

		/*
		 *  Most likely there are mblks in the queue,
		 *  but the driver can not transmit because
		 *  of the bad port status.  In this case,
		 *  ecpp_flush() should make sure ecpp_wsrv_timer()
		 *  is turned off.
		 */

		if (pp->wsrv_timer == TRUE) {
			pp->about_to_untimeout_wsrvt = 1;
			mutex_exit(&pp->umutex);

			(void) untimeout(pp->wsrv_timer_id);

			mutex_enter(&pp->umutex);
			pp->about_to_untimeout_wsrvt = 0;
			pp->wsrv_timer = FALSE;

		}
		break;

	case ECPP_IDLE:
		/* No work to do. Ready to flush */
		break;
	default:
		ecpp_error(pp->dip,
			"ecpp_flush: illegal state %x\n", pp->e_busy);
	}


	/* Discard all messages on the output queue. */
	flushq(q, FLUSHDATA);

	/* The port is no longer flushing or dma'ing for that matter. */
	pp->e_busy = ECPP_IDLE;

	/* we need tor return the port to idle state */
	if (ecpp_idle_phase(pp) == FAILURE) {
		pp->error_status = ECPP_1284_ERR;
		ecpp_error(pp->dip, "ecpp_flush:idle FAILED.\n");
		ecpp_error(pp->dip, "ecpp_flush:ecr=%x, dsr=%x, dcr=%x\n",
			PP_GETB(pp->f_handle, &pp->f_reg->ecr),
			PP_GETB(pp->i_handle, &pp->i_reg->dsr),
			PP_GETB(pp->i_handle, &pp->i_reg->dcr));
	}

	/* Wake up ecpp_close(). */
	cv_signal(&pp->pport_cv);

}

static void
ecpp_start(queue_t *q, mblk_t *mp, caddr_t addr, size_t len)
{
	struct ecppunit *pp;

	/* MUST be called with mutex held */

	pp = (struct ecppunit *)q->q_ptr;

	ASSERT(pp->e_busy == ECPP_BUSY);

	ecpp_error(pp->dip,
		"ecpp_start:current_mode=%x,current_phase=%x,ecr=%x,len=%d\n",
			pp->current_mode,
			pp->current_phase,
			PP_GETB(pp->f_handle, &pp->f_reg->ecr), len);

	switch (pp->current_mode) {
	case ECPP_CENTRONICS:
	case ECPP_COMPAT_MODE:
		if (pp->io_mode == ECPP_DMA) {
			if (ecpp_init_dma_xfer(pp, q, addr, len) == FAILURE)
			return;

			pp->current_phase = ECPP_PHASE_C_FWD_DMA;
			break;
		}

		/* PIO mode */
		if (ecpp_prep_pio_xfer(pp, q, addr, len) == FAILURE)
			return;

		(void) ecpp_pio_writeb(pp);

		pp->current_phase = ECPP_PHASE_C_FWD_PIO;
		break;


	case ECPP_DIAG_MODE:

		if (ecpp_setup_dma_resources(pp, addr, len) == FAILURE)
			return;

		if (!ecr_write(pp, (ECPP_DMA_ENABLE | ECPP_INTR_MASK
				| ECR_mode_110 | ECPP_FIFO_EMPTY))) {
			ecpp_error(pp->dip,
				"ecpp_start: ECPP_DIAG: failed write: ECR.\n");
		}

		pp->dma_cancelled = FALSE;
		ecpp_joblen = ecpp_joblen + pp->dma_cookie.dmac_size;

		set_dmac_bcr(pp, pp->dma_cookie.dmac_size);
		set_dmac_acr(pp, pp->dma_cookie.dmac_address);
		set_dmac_csr(pp, DCSR_INIT_BITS2);

		break;
	case ECPP_NIBBLE_MODE:
		/*
		 *if backchannel request has occured, it must be
		 * processed now.
		 */

		/*
		 * JC
		 * say if nibble device was opened. some time has passed and
		 * finally wsrv sends data to the device.  The device must
		 * be terminated into COMPAT mode and a transfer take place.
		 * a check should be made to see if a REVINTR has occured.
		 * It should be nearly impossible for this to occur because
		 * the port was in REVIDLE when the intr occurred and
		 * immediately a MSG was created, however in this case a
		 * MDATA msg came in ahead of the MSG.
		 */
#if ECPP_REFERENCE_CODE
		if (pp->current_phase == ECPP_PHASE_NIBT_REVINTR) {
			/*
			 *  Event 20:
			 *  Event 21:
			 */

			/*
			 * JC
			 * this is a problem. An MDATA msg came in
			 * before we completed reading back channel.
			 * may be possible in race condition.
			 */
		}
#endif
		if (pp->current_phase != ECPP_PHASE_NIBT_REVIDLE)
			ecpp_error(pp->dip,
				"ecpp_start: nibble problem: phase=%x\n",
					pp->current_phase);

		(void) ecpp_terminate_phase(pp);

		/*
		 * JC
		 * check mode.  should be COMPAT. put into correct phase,
		 * make sure backchannel is NIBBLE and kick off transfer.
		 */

		ecpp_error(pp->dip, "ecpp_start:N: ecr %x\n",
			PP_GETB(pp->f_handle, &pp->f_reg->ecr));

		if (pp->io_mode == ECPP_DMA) {
			if (ecpp_init_dma_xfer(pp, q, addr, len) == FAILURE)
				return;

			pp->current_phase = ECPP_PHASE_C_FWD_DMA;
		} else {
			if (ecpp_prep_pio_xfer(pp, q, addr, len) == FAILURE)
				return;

			(void) ecpp_pio_writeb(pp);
			pp->current_phase = ECPP_PHASE_C_FWD_PIO;
		}

		break;

	case ECPP_ECP_MODE:
		switch (pp->current_phase) {

		case ECPP_PHASE_ECP_FWD_IDLE:
			break;
		case ECPP_PHASE_ECP_REV_IDLE:
			if (ecp_reverse2forward(pp) == FAILURE)
				reset_to_centronics(pp);
			break;
		default:
			ecpp_error(pp->dip, "ecpp_start: ecp problem\n");
			break;
		}

		if (!ecr_write(pp, (ECR_mode_011 | ECPP_DMA_ENABLE))) {
			ecpp_error(pp->dip,
				"ecpp_start:E:failed to write to dcr.\n");
		}

		pp->dma_cancelled = FALSE;
		ecpp_joblen = ecpp_joblen + pp->dma_cookie.dmac_size;

		set_dmac_bcr(pp, pp->dma_cookie.dmac_size);
		set_dmac_acr(pp, pp->dma_cookie.dmac_address);
		set_dmac_csr(pp, DCSR_INIT_BITS2);

		break;
	}

	pp->msg = mp;

	pp->timeout_id = timeout(ecpp_xfer_timeout, (caddr_t)pp,
		pp->xfer_parms.write_timeout * drv_usectohz(1000000));

	ecpp_error(pp->dip, "ecpp_start:len=%d, ecr=%x, dsr=%x, dcr=%x\n",
		len,
		PP_GETB(pp->f_handle, &pp->f_reg->ecr),
		PP_GETB(pp->i_handle, &pp->i_reg->dsr),
		PP_GETB(pp->i_handle, &pp->i_reg->dcr));
}


static uint8_t
ecpp_prep_pio_xfer(struct ecppunit *pp, queue_t *q, caddr_t addr, size_t len)
{
	uint8_t dcr;

	/*
	 * Transfer a PIO "block" a byte at a time.
	 * The block is starts at addr, and it's
	 * end is marked by pp->last_byte.
	 */

	pp->next_byte = addr;
	pp->last_byte = (caddr_t)((ulong_t)addr + len);

	/* pport must be in PIO mode */
	if (!ecr_write(pp, (ECR_mode_000 | ECPP_INTR_MASK | ECPP_INTR_SRV)))
		ecpp_error(pp->dip, "ecpp_prep_pio_xfer:failed w/ECR.\n");

	if (ecpp_check_status(pp) == FAILURE) {
		/*
		 * if status signals are bad, do not start PIO,
		 * put everything back on the queue.
		 */
		ecpp_error(pp->dip,
			"ecpp_prep_pio_xfer:suspend PIO len=%d\n", len);

		if (pp->msg != NULL) {
			/*
			 * this circumstance we want to copy the
			 * untransfered section of msg to a new mblk,
			 * then free the orignal one.
			 */
			ecpp_putback_untransfered(pp,
				(void *)pp->msg->b_rptr, len);
			ecpp_error(pp->dip,
				"ecpp_prep_pio_xfer:len2=%d\n", len);

			freemsg(pp->msg);
			pp->msg = (mblk_t *)NULL;
		} else {
			ecpp_putback_untransfered(pp, pp->ioblock, len);
			ecpp_error(pp->dip,
				"ecpp_prep_pio_xfer: len2=%d\n", len);
		}
		qenable(q);

		return (FAILURE);
	}

	/* Enable SuperIO interrupts on nAck assertion. */
	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if (!dcr_write(pp, (dcr & ~ECPP_INTR_EN)))
		ecpp_error(pp->dip, "ecpp_prep_pio_xfer:failed w/dcr.\n");

	return (SUCCESS);
}

static uint8_t
ecpp_init_dma_xfer(struct ecppunit *pp, queue_t *q, caddr_t addr, size_t len)
{

	if (ecpp_setup_dma_resources(pp, addr, len) == FAILURE)
		return (FAILURE);

	/* pport must be in DMA mode */
	if (!ecr_write(pp, (ECR_mode_010 | ECPP_DMA_ENABLE | ECPP_INTR_MASK)))
		ecpp_error(pp->dip, "ecpp_init_dma_xfer:failed w/ECR.\n");


	if (ecpp_check_status(pp) == FAILURE) {
		/*
		 * if status signals are bad, do not start DMA, but
		 * rather put everything back on the queue.
		 */
		ecpp_error(pp->dip,
			"ecpp_init_dma_xfer: suspending DMA. len=%d\n",
				pp->dma_cookie.dmac_size);

		if (pp->msg != NULL) {
			/*
			 * this circumstance we want to copy the
			 * untransfered section of msg to a new mblk,
			 * then free the orignal one.
			 */
			ecpp_putback_untransfered(pp,
				(void *)pp->msg->b_rptr, len);
			ecpp_error(pp->dip,
				"ecpp_init_dma_xfer:a:len=%d\n", len);

			freemsg(pp->msg);
			pp->msg = (mblk_t *)NULL;
		} else {
			ecpp_putback_untransfered(pp, pp->ioblock, len);
			ecpp_error(pp->dip,
				"ecpp_init_dma_xfer:b:len=%d\n", len);
		}

		if (ddi_dma_unbind_handle(pp->dma_handle) != DDI_SUCCESS)
			ecpp_error(pp->dip,
				"ecpp_init_dma_xfer: unbind FAILURE.\n");
		qenable(q);
		return (FAILURE);
	}

	pp->dma_cancelled = FALSE;

	set_dmac_bcr(pp, pp->dma_cookie.dmac_size);
	set_dmac_acr(pp, pp->dma_cookie.dmac_address);

	ecpp_joblen = ecpp_joblen + pp->dma_cookie.dmac_size;
	set_dmac_csr(pp, DCSR_INIT_BITS2);

	return (SUCCESS);

}

static uint8_t
ecpp_setup_dma_resources(struct ecppunit *pp, caddr_t addr, size_t len)
{
	int cond;

	cond = ddi_dma_addr_bind_handle(pp->dma_handle, NULL,
		addr, len, DDI_DMA_WRITE, DDI_DMA_DONTWAIT, NULL,
		&pp->dma_cookie, &pp->dma_cookie_count);

	switch (cond) {
	case DDI_DMA_MAPPED:
		break;
	case DDI_DMA_PARTIAL_MAP:
		ecpp_error(pp->dip,
			"ecpp_start:DDI_DMA_PARTIAL_MAP:\n");
		return (FAILURE);
	case DDI_DMA_NORESOURCES:
		ecpp_error(pp->dip,
			"ecpp_start:DDI_DMA_NORESOURCES: failure.\n");
		return (FAILURE);
	case DDI_DMA_NOMAPPING:
		ecpp_error(pp->dip,
			"ecpp_start:DDI_DMA_NOMAPPING: failure.\n");
		return (FAILURE);
	case DDI_DMA_TOOBIG:
		ecpp_error(pp->dip,
			"ecpp_start:DDI_DMA_TOOBIG: failure.\n");
		return (FAILURE);
	case DDI_DMA_INUSE:
		ecpp_error(pp->dip,
			"ecpp_start:DDI_DMA_INUSE: failure.\n");
		return (FAILURE);
	default:
		ecpp_error(pp->dip,
			"ddi_dma_addr_bind_handle:unknown rv:failure.\n");
	}

	/* put dmac in a known state */
	RESET_DMAC_CSR(pp);
	set_dmac_bcr(pp, 0);

	return (SUCCESS);
}

static void
ecpp_ack_ioctl(queue_t *q, mblk_t *mp)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCACK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	};

	iocbp = (struct iocblk *)mp->b_rptr;
	iocbp->ioc_error = 0;
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;

	qreply(q, mp);

}

static void
ecpp_nack_ioctl(queue_t *q, mblk_t *mp, int err)
{
	struct iocblk  *iocbp;

	mp->b_datap->db_type = M_IOCNAK;
	mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
	iocbp = (struct iocblk *)mp->b_rptr;
	iocbp->ioc_error = err;

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	qreply(q, mp);
}

/*
 * Set up for a simple copyin of user data, reusing the existing message block.
 * Set the private data field to the user address.
 *
 * This routine supports single-level copyin.
 * More complex user data structures require a better state machine.
 */
static void
ecpp_copyin(queue_t *q, mblk_t *mp, void *addr, size_t len, uint_t mode)
{
	struct copyreq *cqp;

	cqp = (struct copyreq *)(void *)mp->b_rptr;
	mp->b_wptr = mp->b_wptr + sizeof (struct copyreq);
	cqp->cq_addr = addr;
	cqp->cq_size = len;
	cqp->cq_private = (mblk_t *)(void *)addr;
	cqp->cq_flag = mode;
	if (mp->b_cont != NULL) {
		freemsg(mp->b_cont);
		mp->b_cont = NULL;
	}

	mp->b_datap->db_type = M_COPYIN;
	qreply(q, mp);
}


/*
 * Set up for a simple copyout of user data, reusing the existing message block.
 * Set the private data field to -1, signifying the final processing state.
 * Assumes that the output data is already set up in mp->b_cont.
 *
 * This routine supports single-level copyout.
 * More complex user data structures require a better state machine.
 */
static void
ecpp_copyout(queue_t *q, mblk_t *mp, void *addr, size_t len, uint_t mode)
{
	struct copyreq *cqp;

	cqp = (struct copyreq *)(void *)mp->b_rptr;
	mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
	cqp->cq_addr = addr;
	cqp->cq_size = len;
	cqp->cq_private = (mblk_t *)-1;
	cqp->cq_flag = mode;
	mp->b_datap->db_type = M_COPYOUT;
	qreply(q, mp);
}

static void
ecpp_remove_reg_maps(struct ecppunit *pp)
{
	if (pp->c_handle)
		ddi_regs_map_free(&pp->c_handle);
	if (pp->i_handle)
		ddi_regs_map_free(&pp->i_handle);
	if (pp->f_handle)
		ddi_regs_map_free(&pp->f_handle);
	if (pp->d_handle)
		ddi_regs_map_free(&pp->d_handle);
}

uint_t
ecpp_isr(caddr_t arg)
{
	struct ecppunit *pp = (struct ecppunit *)(void *)arg;
	mblk_t *mp;
	uint32_t dcsr;
	uint8_t dsr, dcr;
	int unx_len, cheerio_pend_counter, ecpp_reattempts;

	mutex_enter(&pp->umutex);

	if (pp->dma_cancelled == TRUE) {
		/*
		 * Say the isr occurred while mutex was blocked by
		 * ecpp_close() or M_FLUSH ioctl, both of which call
		 * ecpp_flush().  ecpp_flush() will reset the DMAC.
		 * Once ecpp_isr() regains the mutex, the mutex should
		 * be claimed with no further processing. If another DMA
		 * was kicked off, dma_cancelled is set to FALSE.  Thus,
		 * after ecpp_flush() is invoked, detailed ecpp isr processing
		 * is not possible until ecpp_start() is invoked.
		 */

		ecpp_error(pp->dip, "dma-cancel isr\n");

		pp->dma_cancelled = FALSE;

		mutex_exit(&pp->umutex);
		return (DDI_INTR_CLAIMED);
	}

	/*
	 * the intr is through the motherboard. it is faster than PCI route.
	 * sometimes ecpp_isr() is invoked before cheerio csr is updated.
	 */
	cheerio_pend_counter = ecpp_isr_max_delay;
	dcsr = get_dmac_csr(pp);

	while (!(dcsr & DCSR_INT_PEND) && cheerio_pend_counter-- > 0) {
			drv_usecwait(1);
			dcsr = get_dmac_csr(pp);
	}

	/*
	 * This is a workaround for what seems to be a timing problem
	 * with the delivery of interrupts and CSR updating with the
	 * ebus2 csr, superio and the n_ERR pin from the peripheral.
	 */

	/* delay is not needed for PIO mode */
	if (pp->io_mode == ECPP_DMA || pp->current_mode == ECPP_DIAG_MODE) {
		drv_usecwait(100);
		dcsr = get_dmac_csr(pp);
	}

	if (dcsr & DCSR_INT_PEND) {  /* interrupt is for this device */
		if (dcsr & DCSR_ERR_PEND) {
			ecpp_error(pp->dip, "e\n");
			/* we are expecting a data transfer interrupt */
			ASSERT(pp->e_busy == ECPP_BUSY);
			/*
			 * some kind of DMA error.  Abort transfer and retry.
			 * on the third retry failure, give up.
			 */

			if ((get_dmac_bcr(pp)) != 0) {
				mutex_exit(&pp->umutex);
				ecpp_error(pp->dip,
					"interrupt with bcr != 0");
				mutex_enter(&pp->umutex);
			}

			ecpp_xfer_cleanup(pp);

			if (ddi_dma_unbind_handle(pp->dma_handle) !=
			    DDI_SUCCESS)
				ecpp_error(pp->dip,
				    "ecpp_isr(e): unbind FAILURE \n");

			mutex_exit(&pp->umutex);

			ecpp_error(pp->dip, "transfer count error");

			return (DDI_INTR_CLAIMED);
		}

		if (dcsr & DCSR_TC) {
			ecpp_error(pp->dip, "TC(%x)\n", pp->current_mode);

			/* we are expecting a data transfer interrupt */
			ASSERT(pp->e_busy == ECPP_BUSY);

			if (!ecpp_stop_dma(pp))
				ecpp_error(pp->dip,
					"ecpp_isr: ecpp_stop_dma FAILED.\n");

			ecpp_error(pp->dip, "isr:TC: ecr=%x, dsr=%x, dcr=%x\n",
				PP_GETB(pp->f_handle, &pp->f_reg->ecr),
				PP_GETB(pp->i_handle, &pp->i_reg->dsr),
				PP_GETB(pp->i_handle, &pp->i_reg->dcr));

			ecpp_error(pp->dip,
				"TC:ecpp_joblen=%d\n", ecpp_joblen);

			pp->no_more_fifo_timers = FALSE;
			pp->fifo_timer_id = timeout(ecpp_fifo_timer,
			    (caddr_t)pp, drv_usectohz(FIFO_DRAIN_PERIOD));
			mutex_exit(&pp->umutex);

			return (DDI_INTR_CLAIMED);
		}

		if ((pp->io_mode == ECPP_PIO) &&
			((pp->current_mode == ECPP_CENTRONICS) ||
			pp->current_mode == ECPP_COMPAT_MODE)) {

			/*
			 * Centronics PIO mode.  ecpp_start initiates a
			 * transfer of a data block. ecpp_isr() is generated
			 * upon a peripheral Ack for each byte transfered.
			 * If a non-block-ending byte is transfered,
			 * ecpp_isr() initiates the pio of another byte
			 */
			if (pp->e_busy != ECPP_BUSY)
				ecpp_error(pp->dip,
					"ecpp_isr:C:PIO:e_busy=%x WRONG\n",
						pp->e_busy);

			/*
			 * Turn off SuperIO interrupt capabilities
			 */
			dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

			if (!dcr_write(pp, dcr & ~ECPP_INTR_EN)) {
				ecpp_error(pp->dip,
				"ecpp_isr:pio:failed to write to dcr.\n");
			}

			++ecpp_joblen;

			/* Last byte of the ecpp_start()'s block */
			if (pp->next_byte >= pp->last_byte) {
				ecpp_xfer_cleanup(pp);
				ecpp_error(pp->dip,
				    "isr:PIOe:ecpp_joblen=%d,ecpp_cf_isr=%d,\n",
					ecpp_joblen, ecpp_cf_isr);
			} else {
				/*
				 * Sometimes peripheral is slow
				 * to deassert BUSY after ACK. If so loop.
				 */
				ecpp_reattempts = 0;
				while ((ecpp_check_status(pp) == FAILURE) &&
				    ++ecpp_reattempts < pp->wait_for_busy) {
					if (ecpp_reattempt_high <
							ecpp_reattempts)
						ecpp_reattempt_high =
							ecpp_reattempts;
					drv_usecwait(1);
				}

				if (ecpp_check_status(pp) == FAILURE) {
					++ecpp_cf_isr; /* check status fail */

					ecpp_error(pp->dip,
					    "ecpp_isr:ckeck_status:F:dsr=%x \n",
						PP_GETB(pp->i_handle,
						&pp->i_reg->dsr));

					ecpp_error(pp->dip,
					    "ecpp_isr:PIOF:jl=%d,cf_isr=%d\n",
						ecpp_joblen, ecpp_cf_isr);

					/*
					 * if status signals are bad,
					 * put everything back on the wq.
					 */
					unx_len = pp->last_byte - pp->next_byte;
					ecpp_putback_untransfered(pp,
						pp->next_byte,
						unx_len);
					if (pp->msg != NULL) {
						ecpp_error(pp->dip,
						    "ecppisr:e1:unx_len=%d\n",
							unx_len);
						freemsg(pp->msg);
						pp->msg = (mblk_t *)NULL;
					} else {
						ecpp_error(pp->dip,
						    "ecppisr:e2:unx_len=%d\n",
							unx_len);
					}

					ecpp_xfer_cleanup(pp);
					qenable(pp->writeq);
					mutex_exit(&pp->umutex);
					return (DDI_INTR_CLAIMED);
				}
				pp->e_busy = ECPP_BUSY;

				/* Send out another byte */
				(void) ecpp_pio_writeb(pp);
			}

			mutex_exit(&pp->umutex);
			return (DDI_INTR_CLAIMED);

		}

		/* does peripheral need attention? */
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		if ((dsr & ECPP_nERR) == 0) {
			ecpp_error(pp->dip, "E\n");

			/*
			 * dma breaks in ecp mode if the next three lines
			 * are not there.
			 */
			drv_usecwait(20);	/* 20us */

			/*
			 * mask the interrupt so that it can be
			 * handled later.
			 */
			OR_SET_BYTE_R(pp->f_handle,
				&pp->f_reg->ecr, ECPP_INTR_MASK);

			/*
			 * dma breaks in ecp mode if the next three lines
			 * are not there.
			 */
			drv_usecwait(6);	/* 6us */

			ecpp_error(pp->dip,
				"ecpp_isr:E:crnt_mode=%x,crnt_phase=%x\n",
					pp->current_mode, pp->current_phase);

			/*
			 * JC
			 * It's not clear to me that the DMA_ENABLE should
			 * be set. it should be in REVIDLE now.  Otherwise
			 * how was an intr generated?
			 */
			switch (pp->current_mode) {
			case ECPP_ECP_MODE:
				PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECPP_INTR_MASK | ECR_mode_011 |
				ECPP_DMA_ENABLE));
				break;
			case ECPP_NIBBLE_MODE:
				PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECPP_INTR_MASK | ECR_mode_001));
				pp->current_phase = ECPP_PHASE_NIBT_REVINTR;
				break;
			default:
				PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECPP_INTR_MASK | ECR_mode_001 |
				ECPP_DMA_ENABLE));
				pp->current_phase = FAILURE_PHASE;

			}

			set_dmac_csr(pp, DCSR_INIT_BITS2);

			mutex_exit(&pp->umutex);

			mp = allocb(sizeof (int), BPRI_MED);

			if (mp == NULL) {
				ecpp_error(pp->dip,
					"lost backchannel\n");
			} else {
					mp->b_datap->db_type = M_CTL;
				*(int *)mp->b_rptr = ECPP_BACKCHANNEL;
					mp->b_wptr = mp->b_rptr + sizeof (int);
				if ((pp->oflag == TRUE)) {
					(void) putbq(pp->writeq, mp);
					qenable(pp->writeq);
				}
			}
			return (DDI_INTR_CLAIMED);
		}

		printf("ecpp_isr: IP, but interrupt not for us\n");

	} else {
		printf("ecpp_isr: interrupt not for us.dcsr=%x\n", dcsr);
	}

	mutex_exit(&pp->umutex);

	ecpp_error(pp->dip, "isr:UNCL: ecr=%x, dsr=%x, dcr=%x\n",
		PP_GETB(pp->f_handle, &pp->f_reg->ecr),
		PP_GETB(pp->i_handle, &pp->i_reg->dsr),
		PP_GETB(pp->i_handle, &pp->i_reg->dcr));
	ecpp_error(pp->dip, "mode=%x, phase=%x",
		pp->current_mode, pp->current_phase);

	return (DDI_INTR_UNCLAIMED);
}

static void ecpp_pio_writeb(struct ecppunit *pp) {

	uchar_t	dcr;

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

	if (!dcr_write(pp, (dcr | ECPP_INTR_EN))) {
		ecpp_error(pp->dip, "ecpp_start:CEN:failed to write to dcr.\n");
	}

	/* send the next byte */
	PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, *(pp->next_byte++));

	drv_usecwait(pp->data_setup_time);
	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

	/* Now Assert (neg logic) nStrobe */
	if (!dcr_write(pp, (dcr | ECPP_STB))) {
		ecpp_error(pp->dip,
			"ecpp_pio_writeb:1:failed to write to dcr.\n");
	}

	/* Turn on Cheerio intterupts (non-TC) */
	set_dmac_csr(pp, DCSR_INT_EN | DCSR_TCI_DIS);

	drv_usecwait(pp->strobe_pulse_width);

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

	if (!dcr_write(pp, (dcr & ~ECPP_STB))) {
		ecpp_error(pp->dip,
			"ecpp_pio_writeb:2:failed to write to dcr.\n");
	}
}

static void ecpp_xfer_cleanup(struct ecppunit *pp)
{

	/*
	 * Transfer clean-up.
	 * 1: Shut down the DMAC
	 * 2: stop the transfer timer
	 * 3: If last mblk in queue, signal to close()
	 *    & return to idle state.
	 */

	/* Reset Cheerio DMAC */
	RESET_DMAC_CSR(pp);
	set_dmac_bcr(pp, 0);

	/* Stop the transfer timeout timer */
	if (pp->timeout_id) {
		pp->about_to_untimeout = 1;
		mutex_exit(&pp->umutex);

		(void) untimeout(pp->timeout_id);

		mutex_enter(&pp->umutex);
		pp->about_to_untimeout = 0;
	}

	/*
	 * if we did not use the ioblock, the mblk that
	 * was used should be freed.
	 */

	if (pp->msg != NULL) {
		freemsg(pp->msg);
		pp->msg = (mblk_t *)NULL;
	}

	/* The port is no longer active */
	pp->e_busy = ECPP_IDLE;


	/* If the wq is not empty, wake up the scheduler. */
	if (qsize(pp->writeq))
		qenable(pp->writeq);
	else {
		/*
		 * No more data in wq,
		 * signal ecpp_close().
		 */
		cv_signal(&pp->pport_cv);
	}
}

/*VARARGS*/
static void
ecpp_error(dev_info_t *dip, char *fmt, ...)
{
	static	long	last;
	static	char	*lastfmt;
	char		msg_buffer[255];
	va_list	ap;

	/*
	 * Don't print same error message too often.
	 */
	if ((last == (hrestime.tv_sec & ~1)) && (lastfmt == fmt)) {
		return;
	}
	last = hrestime.tv_sec & ~1;
	lastfmt = fmt;


	va_start(ap, fmt);
	(void) vsprintf(msg_buffer, fmt, ap);
	if (ecpp_debug) {
		cmn_err(CE_CONT, "%s%d: %s", ddi_get_name(dip),
					ddi_get_instance(dip),
					msg_buffer);
	}
	va_end(ap);
}

static void
ecpp_xfer_timeout(void *arg)
{
	struct ecppunit *pp = arg;
	void *unx_addr;
	uint32_t unx_len;

	mutex_enter(&pp->umutex);

	if (pp->about_to_untimeout) {
		mutex_exit(&pp->umutex);
		return;
	}

	/*
	 * If DMAC fails to shut off, continue anyways and attempt
	 * to put untransfered data back on queue.
	 */
	if (!ecpp_stop_dma(pp))
		ecpp_error(pp->dip,
			"ecpp_xfer_timeout: ecpp_stop_dma FAILED.\n");

	if ((pp->io_mode == ECPP_PIO) &&
	    (pp->current_mode == ECPP_CENTRONICS ||
	    pp->current_mode == ECPP_COMPAT_MODE)) {
		/*
		 * PIO mode timeout
		 */
		unx_len = pp->last_byte - pp->next_byte;
		ecpp_error(pp->dip, "xfer_timeout:unx_len=%d\n", unx_len);

		if (unx_len <= 0) {
			pp->timeout_id = 0;
			ecpp_xfer_cleanup(pp);
			mutex_exit(&pp->umutex);
			return;
		} else
			unx_addr = pp->next_byte;
	} else {
		/*
		 * DMA mode timeout
		 */
		unx_len = get_dmac_bcr(pp);
		ecpp_error(pp->dip, "xfer_timeout:bcr=%d\n", unx_len);

		/*
		 * if the bcr is zero, then DMA is complete and
		 * we are waiting for the fifo to drain.  So let
		 * ecpp_fifo_timer() look after the clean up.
		 */
		if (unx_len == 0) {
			/*
			 * set timeout_id to zero, to ensure ecpp_fifo_timer()
			 * will not attempt to untimeout this timer.
			 */
			pp->timeout_id = 0;
			mutex_exit(&pp->umutex);
			return;
		} else {
			if (pp->msg != NULL)
				unx_addr = (caddr_t)pp->msg->b_wptr - unx_len;
			else
				unx_addr = pp->ioblock +
					pp->dma_cookie.dmac_size - unx_len;
		}
	}

	/* Following code is common for PIO and DMA modes */

	ecpp_putback_untransfered(pp, (caddr_t)unx_addr, unx_len);

	if (pp->msg != NULL) {
		ecpp_error(pp->dip, "xfer_timeout:1\n");
		freemsg(pp->msg);
		pp->msg = (mblk_t *)NULL;
	} else {
		/* this is a ioblock type */
		ecpp_error(pp->dip, "xfer_timeout:2\n");
	}

	(void) ecpp_reset_unbind_dma(pp);

	/* mark the error status structure */
	pp->timeout_error = 1;
	pp->e_busy = ECPP_ERR;
	pp->no_more_fifo_timers = TRUE;

	if (qsize(pp->writeq))
		/*
		 * More data to process.
		 * Notify STREAMs scheduler.
		 */
		qenable(pp->writeq);
	else {
		/*
		 * There is no more mblks in the queue.  Return
		 *  to idle state & signal to ecpp_close().
		 */
		if (ecpp_idle_phase(pp) == FAILURE) {
			pp->error_status = ECPP_1284_ERR;
			ecpp_error(pp->dip, "ecpp_xfer_timeout:idle FAILED.\n");
			ecpp_error(pp->dip,
				"ecpp_xfer_timeout:ecr=%x, dsr=%x, dcr=%x\n",
				PP_GETB(pp->f_handle, &pp->f_reg->ecr),
				PP_GETB(pp->i_handle, &pp->i_reg->dsr),
				PP_GETB(pp->i_handle, &pp->i_reg->dcr));
		}

		pp->need_idle_state = FALSE;
		cv_signal(&pp->pport_cv);
	}

	mutex_exit(&pp->umutex);
}

/* DMA routines */

static uint8_t
ecpp_stop_dma(struct ecppunit *pp)
{
	uint8_t ecr;

	/* disable DMA and byte counter */
	AND_SET_LONG_R(pp->d_handle, &pp->dmac->csr,
		~(DCSR_EN_DMA | DCSR_EN_CNT| DCSR_INT_EN));

	/* ACK and disable the TC interrupt */
	OR_SET_LONG_R(pp->d_handle, &pp->dmac->csr,
		DCSR_TC | DCSR_TCI_DIS);

	/* turn off SuperIO's DMA */
	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	if (ecr_write(pp, ecr & ~ECPP_DMA_ENABLE) == FAILURE)
		return (FAILURE);

	/* Disable SuperIO interrupts and DMA */
	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	if (ecr_write(pp, ecr | ECPP_INTR_SRV) == FAILURE)
		return (FAILURE);

	return (SUCCESS);
}

static uint8_t
ecpp_reset_unbind_dma(struct ecppunit *pp)
{
	RESET_DMAC_CSR(pp);
	set_dmac_bcr(pp, 0); /* 4081996 */

	if (pp->io_mode == ECPP_DMA) {
		if (ddi_dma_unbind_handle(pp->dma_handle) != DDI_SUCCESS) {
			ecpp_error(pp->dip,
				"ecpp_reset_clear_dma: unbind FAILURE \n");
			return (FAILURE);
		}
		else
			ecpp_error(pp->dip,
				"ecpp_reset_unbind_dma: unbind OK.\n");
	}
	return (SUCCESS);
}

static void
ecpp_putback_untransfered(struct ecppunit *pp, void *startp, uint_t len)
{

	mblk_t *new_mp;

	ecpp_error(pp->dip, "ecpp_putback_untrans=%d\n", len);

	/*
	 * if the len is zero, then DMA is complete and
	 * we are waiting for the fifo to drain.  So let
	 * ecpp_fifo_timer() look after the clean up.
	 */
	if (len == 0)
		return;

	new_mp = allocb(len, BPRI_MED);
	if (new_mp == NULL) ecpp_error(pp->dip,
		"ecpp_putback_untransfered: allocb FAILURE.\n");

	bcopy(startp, (void *)new_mp->b_rptr, len);

	new_mp->b_datap->db_type = M_DATA;
			new_mp->b_wptr = new_mp->b_rptr + len;

	(void) putbq(pp->writeq, new_mp);

}

/*
 * 1284 utility routines
 */

static uint8_t
read_config_reg(struct ecppunit *pp, uint8_t reg_num)
{
	uint8_t retval;

	mutex_enter(&pp->umutex);

	PP_PUTB(pp->c_handle, &pp->c_reg->index, reg_num);
	retval = PP_GETB(pp->c_handle, &pp->c_reg->data);

	mutex_exit(&pp->umutex);

	return (retval);
}

static void
write_config_reg(struct ecppunit *pp, uint8_t reg_num, uint8_t val)
{
	mutex_enter(&pp->umutex);

	PP_PUTB(pp->c_handle, &pp->c_reg->index, reg_num);
	PP_PUTB(pp->c_handle, &pp->c_reg->data, val);

	/*
	 * second write to this register is needed.  the register behaves as
	 * a fifo.  the first value written goes to the data register.  the
	 * second write pushes the initial value to the register indexed.
	 */

	PP_PUTB(pp->c_handle, &pp->c_reg->data, val);

	mutex_exit(&pp->umutex);
}

/*
 * put the chip in mode zero (forward direction only).  Note that a switch
 * to mode zero must from some of the other modes (010 and 011) must only
 * occur after the FIFO has emptied.
 */
static void
set_chip_pio(struct ecppunit *pp)
{
	uint8_t	dcr;

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN)) {
		ecpp_error(pp->dip, "chip_pio dcr not correct %x\n", dcr);
		PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
			(ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN));
	}

	/* in mode 0, the DCR direction bit is forced to zero */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
	ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/* we are in centronic mode */
	pp->current_mode = ECPP_CENTRONICS;

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_C_IDLE;
}

static int
ecp_negotiation(struct ecppunit *pp)
{
	uint8_t datar, dsr, dcr;
	int ptimeout;

	/* Ecp negotiation */

	/* XXX Failing noe check that we are in idle phase */
#ifdef FCS
	ASSERT(pp->current_phase == ECPP_PHASE_C_IDLE);
#endif
	(void) ecpp_terminate_phase(pp);
	drv_usecwait(1000);

	/* enter negotiation phase */
	pp->current_phase = ECPP_PHASE_NEGO;

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* in mode 0, the DCR direction bit is forced to zero */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN))
		ecpp_error(pp->dip, "ecp_nego: dcr not correct %x\n", dcr);

	/* Event 0: host sets 0x10 on data lines */
	PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, 0x10);

	datar = PP_GETB(pp->i_handle, &pp->i_reg->ir.datar);

	/*
	 * we can read back the contents of the latched data register since
	 * the port is in extended mode
	 */
	if (datar != 0x10)
		ecpp_error(pp->dip, "pport datar not correct %x\n", datar);

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* Event 1: host deassert nSelectin and assert nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/*
	 * Event 2: peripheral asserts nAck, deasserts nFault, asserts
	 * select, asserts Perror, wait for max 35ms.
	 */


	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		dsr &= (ECPP_nACK | ECPP_PE | ECPP_SLCT | ECPP_nERR);
	} while (dsr != ((ECPP_PE | ECPP_SLCT | ECPP_nERR)) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_nERR should be high if peripheral rejects mode,
		 * this isn't a 1284 device.
		 */
		ecpp_error(pp->dip,
			"ecp_negotiation: failed event 2 %x\n", dsr);

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);

		return (FAILURE);
	}

	/*
	 * Event 3: hosts assert nStrobe, latching extensibility value into
	 * peripherals input latch.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX | ECPP_STB);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/*
	 * Event 4: hosts deasserts nStrobe and nAutoFd to acknowledge that
	 * it has recognized an 1284 compatible perripheral.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/*
	 * Event 5 & 6: peripheral deasserts Perror, deasserts Busy,
	 * asserts select if it support ECP mode, then sets nAck,
	 * wait max 35ms
	 */

	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if ((ptimeout == 0) || ((dsr & ECPP_PE) != 0)) {
		/*
		 * something seriously wrong, should just reset the
		 * interface and go back to compatible mode.
		 */
		ecpp_error(pp->dip,
			"ecp_negotiation: timeout event 6 %x\n", dsr);

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);

		return (FAILURE);
	}

	/* ECPP_SLCT should be low if peripheral rejects mode */
	if ((dsr & ECPP_SLCT) == 0) {
		ecpp_error(pp->dip,
			"ecp_negotiation: mode rejected %x\n", dsr);

		/* terminate the mode */
		(void) ecpp_terminate_phase(pp);

		return (FAILURE);
	}

	/* successful negotiation into ECP mode */
	pp->current_mode = ECPP_ECP_MODE;

	/* execute setup phase */
	pp->current_phase = ECPP_PHASE_ECP_SETUP;

	drv_usecwait(1);

	/* Event 30: host asserts nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/* Event 31: peripheral set Perror, wait max 35ms */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_PE) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"ecp_negotiation: failed event 30 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);
	}

	/* interface is now in forward idle phase */
	pp->current_phase = ECPP_PHASE_ECP_FWD_IDLE;

	/* deassert nAutoFd */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	ecpp_error(pp->dip, "ecp_negotiation: ok\n");

	return (SUCCESS);
}

static int
nibble_negotiation(struct ecppunit *pp)
{
	uint8_t datar, dsr, dcr;
	int ptimeout;

	/* Nibble mode negoatiation */

	PTRACE(nibble_negotiation, 'NiNe', pp->current_phase);
	/* check that we are in idle phase */
#ifdef FCS
	ASSERT(pp->current_phase == ECPP_PHASE_C_IDLE);
#endif
	(void) ecpp_terminate_phase(pp);
	drv_usecwait(50);

	/* enter negotiation phase */
	pp->current_phase = ECPP_PHASE_NEGO;

	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
	if ((dcr & ~ECPP_DCR_SET) != (ECPP_nINIT | ECPP_SLCTIN))
		ecpp_error(pp->dip, "nibble_nego: dcr not correct %x\n", dcr);
	ptimeout = 0;


zero:	/* Event 0: host sets 0x00 on data lines */
	PP_PUTB(pp->i_handle, &pp->i_reg->ir.datar, 0x00);

	datar = PP_GETB(pp->i_handle, &pp->i_reg->ir.datar);

	/*
	 * we can read back the contents of the latched data register since
	 * the port is in extended mode
	 */
	if (datar != 0x00) {
		ecpp_error(pp->dip, "pport nib datar not correct %x\n", datar);
				delay(1 * drv_usectohz(1000000));

		++ptimeout;
		if (ptimeout > 10) {
			ecpp_error(pp->dip,
				"pport nib:ptimeout= %d\n", ptimeout);
			return (FAILURE);
		}
		goto zero;
	}
	drv_usecwait(1);	/* Tp(ecp) == 0.5us */

	/* Event 1: host deassert nSelectin and assert nAutoFd */
	if (dcr_write(pp, ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX) == FAILURE) {
		ecpp_error(pp->dip, "Nibble nego:EVENT 1 FAILURE.\n");
			return (FAILURE);
	}
	/*
	 * Event 2: peripheral asserts nAck, deasserts nFault, asserts
	 * select, asserts Perror
	 */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		dsr &= (ECPP_nACK | ECPP_PE | ECPP_SLCT | ECPP_nERR);
	} while (dsr != ((ECPP_PE | ECPP_SLCT | ECPP_nERR)) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_nERR should be high if peripheral rejects mode,
		 * this isn't a 1284 device
		 */
		ecpp_error(pp->dip,
			"nibble_negotiation: failed event 2 DSR:%x DCR:%x\n",
				dsr, PP_GETB(pp->i_handle, &pp->i_reg->dcr));

		pp->current_mode = ECPP_CENTRONICS;

		/* put chip back into compatibility mode */
		set_chip_pio(pp);
		return (FAILURE);
	}

	/*
	 * Event 3: hosts assert nStrobe, latching extensibility value into
	 * peripherals input latch.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX | ECPP_STB);

	drv_usecwait(2);	/* Tp(ecp) = 0.5us */

	/*
	 * Event 4: hosts asserts nStrobe and nAutoFD to acknowledge that
	 * it has recognized an 1284 compatible perripheral.
	 */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/*
	 * Event 6: Check if nAck is High to confirm end of nibble
	 * negotiation.  Wait max 35 ms.  Save status signals to
	 * check later if back channel data is available.
	 */
	ptimeout = 100000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);


	/*
	 * Event 5: For Extensibility Request value of
	 * 0x00 (nibble mode) Select must be low at Event 6.
	 */

	if ((ptimeout == 0) || (dsr & ECPP_SLCT)) {
		/*
		 * something seriously wrong, should just reset the
		 * interface and go back to compatible mode.
		 */
		ecpp_error(pp->dip,
		    "nibble_negotiation: timeout event 5 %x\n", dsr);
		pp->current_mode = ECPP_CENTRONICS;
		/* put chip back into compatibility mode */
		set_chip_pio(pp);
		return (FAILURE);
	}

	/*
	 * If peripheral has data available, PE and nErr will
	 * be set low at Event 5 & 6.
	 */
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	if ((dsr & ECPP_PE) || (dsr & ECPP_nERR))
		pp->current_phase = ECPP_PHASE_NIBT_NAVAIL;
	else
		pp->current_phase = ECPP_PHASE_NIBT_AVAIL;

	ecpp_error(pp->dip, "nibble_negotiation: current_phase=%x\n",
			pp->current_phase);

	/* successful negotiation into Nibble mode */
	pp->current_mode = ECPP_NIBBLE_MODE;
	pp->backchannel = ECPP_NIBBLE_MODE;
		ecpp_error(pp->dip,
		    "nibble_negotiation: ok\n");

	return (SUCCESS);
}

/*
 * Upon conclusion of this routine, ecr mode = 010.
 * If successful, current_mode = ECPP_CENTRONICS, if not ECPP_FAILURE_MODE.
 */
static int
ecpp_terminate_phase(struct ecppunit *pp)
{
	uint8_t dsr;
	int ptimeout;

	PTRACE(ecpp_terminate_phase, 'tmod', pp->current_mode);

	if (((pp->current_mode == ECPP_NIBBLE_MODE) &&
			(pp->current_phase == ECPP_PHASE_NIBT_REVIDLE)) ||
		((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_FWD_IDLE)) ||
		((pp->current_mode == ECPP_CENTRONICS) &&
			(pp->current_phase == ECPP_PHASE_NEGO))) {
		pp->current_phase = ECPP_PHASE_TERM;
	} else {
		ecpp_error(pp->dip, "ecpp_terminate_phase: not needed.\n");
		return (SUCCESS);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* superio in mode 0, the DCR direction bit is forced to zero */
	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/*
	 * This line is needed to make the diag program work
	 * needs investigation as to why we must go to
	 * mode 001 to terminate a phase KML XXX
	 */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/*
	 * 4234981:
	 * FIXME - payne: this is where the Genoa Conformance Tester has a
	 * problem.  Claims we need to Set nSelectIn low, and nAutoFd high
	 * {if it's not already}  SLIN bit <3> is the inverse of it's pin,
	 * Init <2> follows, AFD <1> is the inverse, Strobe <0> is the inverse
	 */

	/* Event 22: hosts asserts select, deasserts nStrobe and nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

	/* Event 23: peripheral deasserts nFault and nBusy */

	/* Event 24: peripheral asserts nAck */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"termination: failed event 24 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		reset_to_centronics(pp);
		return (FAILURE);
	}

	/* check some of the Event 23 flags */
	if ((dsr & (ECPP_nBUSY | ECPP_nERR)) != ECPP_nERR) {
		ecpp_error(pp->dip,
			"termination: failed event 23 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		reset_to_centronics(pp);
		return (FAILURE);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 25: hosts asserts select and nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN | ECPP_AFX);

	/* Event 26: the peripheral puts itself in compatible mode */

	/* Event 27: peripheral deasserts nACK */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"termination: failed event 27 %x\n", dsr);
		pp->current_mode = ECPP_FAILURE_MODE;
		reset_to_centronics(pp);
		return (FAILURE);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 28: hosts deasserts nAutoFd. */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN);

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* we are in centronic mode */

	pp->current_mode = ECPP_COMPAT_MODE;

	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_001);

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_C_IDLE;

/* JC SPECIAL */
	/* Wait until peripheral is no longer busy */
	ptimeout = 100;
	do {
		ptimeout--;
		delay(1 * drv_usectohz(5000));
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		ecpp_error(pp->dip, "terminate phase: BUSY:dsr=%x\n", dsr);
	} while (((dsr & ECPP_nBUSY) == 0) && ptimeout);


	ecpp_error(pp->dip, "termination_phase complete.\n");

	return (SUCCESS);
}

static uchar_t
ecp_peripheral2host(struct ecppunit *pp)
{

	uint8_t ecr;
	uint8_t byte;
	mblk_t		*mp;

	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_REV_IDLE))
		ecpp_error(pp->dip, "ecp_peripheral2host okay\n");

	/*
	 * device interrupts should be disabled by the caller and the mode
	 * should be 001, the DCR direction bit is forced to read
	 */

	AND_SET_BYTE_R(pp->i_handle,
			&pp->i_reg->dcr, ~ECPP_INTR_EN);

	AND_SET_BYTE_R(pp->i_handle, &pp->i_reg->dcr, ~ECPP_AFX);

	/* put superio into ECP mode with reverse transfer */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECR_mode_011 | ECPP_INTR_SRV | ECPP_INTR_MASK);

	pp->current_phase = ECPP_PHASE_ECP_REV_XFER;

	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);

	/*
	 * Event 42: Host tristates data bus, peripheral asserts nERR
	 * if data available, usually the status bits (7-0) and requires
	 * two reads since only nibbles are transfered.
	 */

	/* continue reading bytes from the peripheral */
	while ((ecr & ECPP_FIFO_EMPTY) == 0) {
		byte = PP_GETB(pp->f_handle, &pp->f_reg->fr.dfifo);
			PTRACE(ecp_peripheral2host, 'BYTE', byte);
			ecpp_error(pp->dip,
				"ecpp_periph2host: byte=%x(%c)\n", byte, byte);

		if ((mp = allocb(sizeof (uchar_t), BPRI_MED)) == NULL) {
			ecpp_error(pp->dip,
				"ecpp_periph2host: allocb FAILURE.\n");
			return (FAILURE);
		}

		if (canputnext(pp->readq) == 1) {
			mp->b_datap->db_type = M_DATA;
			*(uchar_t *)mp->b_rptr = byte;
			mp->b_wptr = mp->b_rptr + sizeof (uchar_t);
			(void) putnext(pp->readq, mp);
		}
		drv_usecwait(100);
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	}

	/* put superio into PIO mode */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECR_mode_011 | ECPP_INTR_SRV | ECPP_INTR_MASK);

	pp->current_phase = ECPP_PHASE_ECP_REV_IDLE;

	return (SUCCESS);
}

static uchar_t
nibble_peripheral2host(struct ecppunit *pp, uint8_t *byte)
{
	uint8_t dsr, nbyte = 0;
	int ptimeout;

	/* superio in mode 1, the DCR direction bit is forced to read */
	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/* Event 7: host asserts nAutoFd to move to read 1st nibble */
	if (dcr_write(pp, ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX) == FAILURE) {
		ecpp_error(pp->dip,
			"nibble_peripheral2host: Event 7 FAILED.\n");
		return (FAILURE);
	}
	/* Event 8: peripheral puts data on the status lines */

	/* Event 9: peripheral asserts nAck, data available */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout <= 1) {
		ecpp_error(pp->dip,
			"nibble_peripheral2host(1): failed event 9 %x\n", dsr);
		reset_to_centronics(pp);
		return (FAILURE);
	}

	if (dsr & ECPP_nERR)  nbyte = 0x01;
	if (dsr & ECPP_SLCT)  nbyte |= 0x02;
	if (dsr & ECPP_PE)    nbyte |= 0x04;
	if (~dsr & ECPP_nBUSY)nbyte |= 0x08;

	/* Event 10: host deasserts nAutoFd to grab data */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT);

	/* Event 11: peripheral deasserts nAck to finish handshake */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nibble_peripheral2host(1): failed event 11 %x\n",
			dsr);
		reset_to_centronics(pp);
		return (FAILURE);
	}

	drv_usecwait(1);	/* Tp(ecp) = 0.5us */

	/* Event 12: host asserts nAutoFd to move to read 2nd nibble */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	/* Event 8: peripheral puts data on the status lines */

	/* Event 9: peripheral asserts nAck, data available */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_nACK) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nibble_peripheral2host(2): failed event 9 %x\n", dsr);
		reset_to_centronics(pp);
		return (FAILURE);
	}

	if (dsr & ECPP_nERR)	nbyte |= 0x10;
	if (dsr & ECPP_SLCT)	nbyte |= 0x20;
	if (dsr & ECPP_PE)	nbyte |= 0x40;
	if (~dsr & ECPP_nBUSY)	nbyte |= 0x80;

	/* Event 10: host deasserts nAutoFd to grab data */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT);

	/* Event 13: peripheral asserts PE - end of data phase. */

	/* Event 11: peripheral deasserts nAck to finish handshake */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_nACK) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
			"nib_periph2host(2): failed event 11 DSR:%x DCR:%x\n",
				dsr, PP_GETB(pp->i_handle, &pp->i_reg->dcr));
		drv_usecwait(100);
		reset_to_centronics(pp);
		return (FAILURE);
	}
	ecpp_error(pp->dip,
			"nibble_peripheral2host: byte= %x (%c)\n",
				nbyte, nbyte);
	*byte = nbyte;
	return (SUCCESS);
}

/*
 * process data transfers requested by the peripheral
 */
static uint_t
ecpp_peripheral2host(struct ecppunit *pp)
{
	uint8_t dsr;
	uint32_t ptimeout;

	if (pp->current_mode == ECPP_DIAG_MODE)
		return (SUCCESS);

	switch (pp->backchannel) {
	case ECPP_CENTRONICS:
		return (SUCCESS);
	case ECPP_NIBBLE_MODE:
		/*
		 * A nibble rev intr should have just occured. Event 18
		 */

		if (pp->current_phase == ECPP_PHASE_NIBT_REVINTR) {
			/* Handshake to acknowledge rev channel request */

			/*
			 * Event 19: Peripheral sets nAck is set to 1
			 * 		(trailing edge of pulse)
			 */

			ptimeout = 1000;
			do {
				ptimeout--;
				drv_usecwait(35);
				dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
			} while (((dsr & ECPP_nACK) == 0) && ptimeout);

			if (ptimeout == 0) {
				ecpp_error(pp->dip,
					"ecpp_perip2host:Event 19 FAILED %x\n",
						dsr);
				reset_to_centronics(pp);
				return (FAILURE);
			}

			/* Event 20: Host sets nAFX to 1 */
			AND_SET_BYTE_R(pp->i_handle,
				&pp->i_reg->dcr, ~ECPP_AFX);

			/* Event 21: Peripheral sets PE to 0. */
			ptimeout = 1000;
			do {
				ptimeout--;
				drv_usecwait(35);
				dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
			} while ((dsr & ECPP_PE) && ptimeout);

			if (ptimeout == 0) {
				ecpp_error(pp->dip,
					"ecpp_periph2host:Event 21 FAILED %x\n",
						dsr);
				reset_to_centronics(pp);
				return (FAILURE);
			}

			pp->current_phase = ECPP_PHASE_NIBT_REVDATA;

			if (read_nibble_backchan(pp) == FAILURE) {
				/* return to centronics and keep going ? */
				ecpp_error(pp->dip,
					"ecpp_peripheral2host:nibble bad\n");
				return (FAILURE);
			}
			else
				return (SUCCESS);

		}

		return (FAILURE);

	case ECPP_ECP_MODE:
		switch (pp->current_phase) {
		case ECPP_PHASE_ECP_REV_IDLE:
			break;
		case ECPP_PHASE_ECP_FWD_IDLE:
			if (ecp_forward2reverse(pp) == SUCCESS)
				break;
			else
				return (FAILURE);
		default:
			ecpp_error(pp->dip, "ecpp_periph2host: ECP phase");
			break;
		}

		return (ecp_peripheral2host(pp));

	default:
		ecpp_error(pp->dip, "ecpp_peripheraltohost: illegal back");
		return (FAILURE);
	}
}

static int
ecp_forward2reverse(struct ecppunit *pp)
{
	uint8_t dsr, ecr;
	int ptimeout;

	/*
	 * device interrupts should be disabled by the caller, the ecr
	 * should be mode 001
	 */
	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_FWD_IDLE))
		ecpp_error(pp->dip, "ecp_forward2reverse okay\n");

	/*
	 * In theory the fifo should always be empty at this point
	 * however, in doing active printing with periodic queries
	 * to the printer for reverse transfers, the fifo is sometimes
	 * full at this point. Other times in diag mode we have seen
	 * hangs without having an escape counter. The values of
	 * 10000 and 50 usec wait have seemed to make printing more
	 * reliable. Further investigation may be needed here XXXX
	 */

	ptimeout = 10000;
	do {
		ptimeout--;
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		drv_usecwait(100);
	} while (((ecr & ECPP_FIFO_EMPTY) == 0) && ptimeout);

	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);


	/*
	 * Must be in Forward idle state. This line
	 * sets forward idle,
	 * set data lines in high impedance and does
	 * Event 38: host asserts nAutoFd to grab data
	 */

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

	drv_usecwait(3);	/* Tp(ecp) = 0.5us */

	/* Event 39: assert nINIT */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		ECPP_DCR_SET | ECPP_AFX);

	/* Event 40: peripheral deasserts PERR. */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while ((dsr & ECPP_PE) && ptimeout);

	if (ptimeout == 0) {
		/*
		 * ECPP_PE should be low
		 */
		ecpp_error(pp->dip,
			"ecp_forward2reverse: failed event 40 %x\n", dsr);

		/* XX should do error recovery here */
		return (FAILURE);
	}

	OR_SET_BYTE_R(pp->i_handle, &pp->i_reg->dcr, ECPP_REV_DIR);

	pp->current_phase = ECPP_PHASE_ECP_REV_IDLE;

	return (SUCCESS);
}

static int
ecp_reverse2forward(struct ecppunit *pp)
{
	uint8_t dsr, ecr;
	int ptimeout;

	if ((pp->current_mode == ECPP_ECP_MODE) &&
			(pp->current_phase == ECPP_PHASE_ECP_REV_IDLE))
		ecpp_error(pp->dip, "ecp_reverse2forward okay\n");


	/*
	 * National spec says that we must do the following
	 * Make sure that FIFO is empty before we switch modes
	 * make sure that we are in mode 001 before we change
	 * direction
	 */

	ptimeout = 1000;
	do {
		ptimeout--;
		ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
	} while (((ecr & ECPP_FIFO_EMPTY) == 1) && ptimeout);

	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);

	/* Event 47: deassert nInit */
	PP_PUTB(pp->i_handle, &pp->i_reg->dcr, ECPP_DCR_SET | ECPP_nINIT);

	/* Event 48: peripheral deasserts nAck */

	/* Event 49: peripheral asserts PERR. */
	ptimeout = 1000;
	do {
		ptimeout--;
		drv_usecwait(35);
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
	} while (((dsr & ECPP_PE) == 0) && ptimeout);

	if ((ptimeout == 0) || ((dsr & ECPP_nACK) == 0)) {
		/*
		 * ECPP_nACK should be high
		 */
		ecpp_error(pp->dip,
			"ecp_reverse2forward: failed event 49 %x\n", dsr);

		/* XX should do error recovery here */
		return (FAILURE);
	}

	pp->current_phase = ECPP_PHASE_ECP_FWD_IDLE;

	return (SUCCESS);
}

/*
 * Upon conclusion of ecpp_default_negotiation(), backchannel must be set
 * to one of the following values:
 *   ECPP_CENTRONICS - if peripheral does not support 1284
 *   ECPP_ECP_MODE - if peripheral supports 1284 ECP mode.
 *   ECPP_NIBBLE_MODE - if peripheral is only 1284 compatible.
 *
 * Any other values for backchannel are illegal.
 *
 * ecpp_default_negotiation() always returns SUCCESS.
 *
 * The calling routine must hold the mutex.
 * The calling routine must ensure the port is ~BUSY before it
 * calls this routine then mark it as BUSY.
 */
static void
ecpp_default_negotiation(struct ecppunit *pp)
{
	/*
	 * ECP_MODE is removed from default nego for time being.
	 *

	if (ecpp_mode_negotiation(pp, ECPP_ECP_MODE) == SUCCESS) {
		if (pp->current_mode == ECPP_ECP_MODE) {
			pp->backchannel = ECPP_ECP_MODE;
		} else
			ecpp_error(pp->dip, "ecpp_mode_negotiation: unknown");
	} else {
	*/

	if (ecpp_mode_negotiation(pp, ECPP_NIBBLE_MODE) == SUCCESS) {
		/* 1284 compatible device */
		if (pp->fast_compat == TRUE)
			pp->io_mode = ECPP_DMA;
		else
			pp->io_mode = ECPP_PIO;

	} else {
		/* Centronics device */
		if (pp->fast_centronics == TRUE)
			pp->io_mode = ECPP_DMA;
		else
			pp->io_mode = ECPP_PIO;
	}
}

static int
ecpp_mode_negotiation(struct ecppunit *pp, uchar_t newmode)
{

	int rval = 0;
	int current_mode;

	PTRACE(ecpp_mode_negotiation, 'NEGO', pp->current_mode);
	if (pp->current_mode == newmode)
		return (SUCCESS);

	switch (newmode) {
	case ECPP_CENTRONICS:
		switch (pp->current_mode) {
		case ECPP_COMPAT_MODE:
		case ECPP_FAILURE_MODE:
			pp->current_mode = ECPP_CENTRONICS;
			pp->backchannel = ECPP_CENTRONICS;
			return (SUCCESS);
		case ECPP_DIAG_MODE:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_000 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			pp->current_mode = ECPP_CENTRONICS;
			pp->backchannel = ECPP_CENTRONICS;
			return (SUCCESS);
		case ECPP_ECP_MODE:
		case ECPP_NIBBLE_MODE:
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			return (SUCCESS);
		}

		if (ecpp_terminate_phase(pp) == FAILURE)
			set_chip_pio(pp);

		pp->current_mode = ECPP_CENTRONICS;
		pp->backchannel = ECPP_CENTRONICS;

		return (SUCCESS);
	case ECPP_COMPAT_MODE:
		switch (pp->current_mode) {
		case ECPP_DIAG_MODE:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_000 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			if (nibble_negotiation(pp) == SUCCESS) {
				if (ecpp_terminate_phase(pp) == FAILURE) {
					set_chip_pio(pp);
					pp->backchannel = ECPP_CENTRONICS;
					pp->current_mode = ECPP_CENTRONICS;
					return (FAILURE);
				}
				pp->backchannel = ECPP_NIBBLE_MODE;
				pp->current_mode = ECPP_COMPAT_MODE;
				return (SUCCESS);
			}

			pp->current_mode = ECPP_CENTRONICS;
			pp->backchannel = ECPP_CENTRONICS;
			return (FAILURE);

		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
			if (nibble_negotiation(pp) == SUCCESS) {
				if (ecpp_terminate_phase(pp) == FAILURE) {
					set_chip_pio(pp);
					pp->backchannel = ECPP_CENTRONICS;
					pp->current_mode = ECPP_CENTRONICS;
					return (FAILURE);
				}
				pp->backchannel = ECPP_NIBBLE_MODE;
				pp->current_mode = ECPP_COMPAT_MODE;
				return (SUCCESS);
			}

			pp->current_mode = ECPP_CENTRONICS;
			pp->backchannel = ECPP_CENTRONICS;
			return (FAILURE);
		case ECPP_ECP_MODE:
		case ECPP_NIBBLE_MODE:
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			return (SUCCESS);
		}

		if (ecpp_terminate_phase(pp)) {
		    pp->current_mode = newmode;
		    return (SUCCESS);
		} else {
			set_chip_pio(pp);
			pp->current_mode = ECPP_CENTRONICS;
			return (FAILURE);
		}

	case ECPP_NIBBLE_MODE:
		switch (pp->current_mode) {
		case ECPP_DIAG_MODE:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_000 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			break;
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
			break;
		case ECPP_ECP_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}

		ecpp_error(pp->dip, "ecpp_mode_nego(current=%x)\n",
				pp->current_mode);

		if (nibble_negotiation(pp) == FAILURE) {
			pp->backchannel = ECPP_CENTRONICS;
			return (FAILURE);
		}
		else
			return (SUCCESS);

	case ECPP_ECP_MODE:
		current_mode = pp->current_mode;
		switch (pp->current_mode) {
		case ECPP_DIAG_MODE:
			/* put superio into PIO mode */
			PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
				(ECR_mode_000 | ECPP_INTR_MASK
				| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));
			break;
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
			break;
		case ECPP_NIBBLE_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}

		rval = ecp_negotiation(pp);

		/*
		 * JC
		 * This logic should be reviewed.
		 */
		if (rval != SUCCESS) {
			if (current_mode != ECPP_ECP_MODE) {
				(void) ecpp_mode_negotiation(pp, current_mode);
			}
		}
		/*
		 * This delay is necessary for the TI Microlaser Pro 600
		 * printer. It needs to "settle" down before we try to
		 * send dma at it. If this isn't here the printer
		 * just wedges.
		 */
		delay(1 * drv_usectohz(1000000));
		return (rval);

	case ECPP_DIAG_MODE:
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
		case ECPP_FAILURE_MODE:
		case ECPP_COMPAT_MODE:
		case ECPP_DIAG_MODE:
			break;
		case ECPP_NIBBLE_MODE:
		case ECPP_ECP_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE) {
				ecpp_error(pp->dip, "unknown current mode %x\n",
					pp->current_mode);
				/* XX put channel into compatible mode */
			}
			break;
		default:
			ecpp_error(pp->dip, "unknown current mode %x\n",
				pp->current_mode);
			/* XX put channel into compatible mode */
			break;
		}
		/* put superio into PIO mode */
		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			(ECR_mode_001 | ECPP_INTR_MASK
			| ECPP_INTR_SRV | ECPP_FIFO_EMPTY));

		pp->current_mode = ECPP_DIAG_MODE;
		return (SUCCESS);

	default:
		ecpp_error(pp->dip,
			"ecpp_mode_negotiation: mode %d not supported.",
			newmode);
		return (FAILURE);
	}
}

static uchar_t
ecpp_idle_phase(struct ecppunit *pp)
{

	if (pp->current_mode == ECPP_DIAG_MODE)
		return (SUCCESS);

	switch (pp->backchannel) {
	case ECPP_CENTRONICS:
		switch (pp->current_mode) {
		case ECPP_CENTRONICS:
			return (SUCCESS);
		case ECPP_COMPAT_MODE:
			pp->current_mode = ECPP_CENTRONICS;
			return (SUCCESS);
		case ECPP_NIBBLE_MODE:
		case ECPP_ECP_MODE:
			if (ecpp_terminate_phase(pp) == FAILURE)
				set_chip_pio(pp);
			pp->current_mode = ECPP_CENTRONICS;
		default:
			ecpp_error(pp->dip,
			    "ecpp_idle_phase:illegal current mode:%x\n",
				pp->current_mode);
		}
		return (SUCCESS);
	case ECPP_NIBBLE_MODE:
		/*
		 * The current mode should be COMPAT
		 * The driver needs to move into nibble mode.
		 * If backchannel bytes need to be read they
		 * should be read here and sent upstream.
		 * Afterwards, port should be left in rev-idle
		 * phase.
		 */


		/* in mode 0, the DCR direction bit is forced to zero */
		PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
			ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_000);


		/* Should perform sanity checks .. correct mode, phase */

		if (ecpp_mode_negotiation(pp, ECPP_NIBBLE_MODE) == SUCCESS) {
			ecpp_error(pp->dip,
				"set_idle_phase, nib: ecr=%x, dsr=%x, dcr=%x\n",
					PP_GETB(pp->f_handle, &pp->f_reg->ecr),
					PP_GETB(pp->i_handle, &pp->i_reg->dsr),
					PP_GETB(pp->i_handle, &pp->i_reg->dcr));

			if (read_nibble_backchan(pp) == FAILURE) {
				ecpp_error(pp->dip, "ecpp_idle_phase: nib bad");
				return (FAILURE);
			}
			return (SUCCESS);
		} else {
			ecpp_error(pp->dip,
				"ecpp_idle_phase:nego(nib) FAILED.\n");
			return (FAILURE);
		}

	case ECPP_ECP_MODE:
		switch (pp->current_phase) {
		case ECPP_PHASE_ECP_FWD_IDLE:
		case ECPP_PHASE_ECP_REV_IDLE:
		    PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
			    ECPP_DCR_SET | ECPP_nINIT | ECPP_AFX);

			return (SUCCESS);
		default:
			ecpp_error(pp->dip, "ecpp_idle_phase: ECP phase");
			return (FAILURE);
		}
	default:
		ecpp_error(pp->dip, "ecpp_idle_phase: illegal back");
		return (FAILURE);
	}
}

static uchar_t
read_nibble_backchan(struct ecppunit *pp) {
	uint8_t 		dsr, dcr;
	uint8_t		byte;
	mblk_t		*mp;

	/*
	 * This routine leaves the port in ECPP_PHASE_NIBT_REVIDLE.
	 * The current phase should be NIBT_AVAIL or NIBT_NAVAIL.
	 * If NIBT_AVAIL routine reads backchannel and sends bytes
	 * upstream.  If a 1284 event fails to occur, routine
	 * sets port (current_mode, backchannel) to CENTRONICS and
	 * returns FAILURE.
	 */
	if (pp->current_phase == ECPP_PHASE_NIBT_AVAIL) {
		/*
		 * Event 14: Host tristates data bus, peripheral
		 * asserts nERR if data available, usually the
		 * status bits (7-0) and requires two reads since
		 * only nibbles are transfered.
		 */
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		while ((dsr & ECPP_nERR) == 0) {
			if (nibble_peripheral2host(pp, &byte) == FAILURE) {
				return (FAILURE);
			}
			if ((mp = allocb(sizeof (uchar_t), BPRI_MED)) == NULL) {
				ecpp_error(pp->dip,
					"read_nibble_backchan:allocb:FAILED\n");
				pp->current_phase = FAILURE_PHASE;
				reset_to_centronics(pp);
				return (FAILURE);
			}

			if (canputnext(pp->readq) == 1) {
				mp->b_datap->db_type = M_DATA;
				*(uchar_t *)mp->b_rptr = byte;
				mp->b_wptr = mp->b_rptr + sizeof (uint8_t);
				(void) putnext(pp->readq, mp);
			} else {
				ecpp_error(pp->dip,
				    "read_nibble_backchan:canputnext:FAILED\n");
				pp->current_phase = FAILURE_PHASE;
				reset_to_centronics(pp);
				return (FAILURE);
			}
			dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		}
		pp->current_phase = ECPP_PHASE_NIBT_NAVAIL;
	}

	if (pp->current_phase == ECPP_PHASE_NIBT_NAVAIL) {
		/*
		 * Event 7:  Set into Reverse Idle phase by setting
		 * AFX high.
		 */
		dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);
		PP_PUTB(pp->i_handle, &pp->i_reg->dcr, dcr | ECPP_AFX);
		pp->current_phase = ECPP_PHASE_NIBT_REVIDLE;
		return (SUCCESS);
	} else {
		ecpp_error(pp->dip, "read_nibble_backchan: bad phase %x",
			pp->current_phase);
		pp->current_phase = FAILURE_PHASE;
		reset_to_centronics(pp);
		return (FAILURE);
	}
}


static void
reset_to_centronics(struct ecppunit *pp)
{

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		(ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN | ECPP_AFX));

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		(ECPP_DCR_SET | ECPP_SLCTIN | ECPP_AFX));

	/* pulsewidth for reset >50 us */
	drv_usecwait(75);

	PP_PUTB(pp->i_handle, &pp->i_reg->dcr,
		(ECPP_DCR_SET | ECPP_nINIT | ECPP_SLCTIN | ECPP_AFX));

	/* Let peripheral reset itself. */
	drv_usecwait(75);

	/* in mode 0, the DCR direction bit is forced to zero */
	PP_PUTB(pp->f_handle, &pp->f_reg->ecr,
		ECPP_INTR_SRV | ECPP_INTR_MASK | ECR_mode_000);

	/* we are in centronic mode */
	pp->current_mode = ECPP_CENTRONICS;
	pp->backchannel = ECPP_CENTRONICS;

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_C_IDLE;

	ecpp_error(pp->dip, "reset_to_centronics: complete.\n");
}

static uchar_t
ecr_write(struct ecppunit *pp, uint8_t ecr_byte)
{
	uint8_t current_ecr, res1, res2;
	int i;

	for (i = ECPP_REG_WRITE_MAX_LOOP; i > 0; i--) {
		PP_PUTB(pp->f_handle, &pp->f_reg->ecr, ecr_byte);

		current_ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);
		/* mask off the lower two read-only bits */
		res1 = ecr_byte & 0xFC;
		res2 = current_ecr & 0xFC;

		if (res1 == res2)
			return (SUCCESS);
	}

	return (FAILURE);
}

static uchar_t
dcr_write(struct ecppunit *pp, uint8_t dcr_byte)
{
	uint8_t current_dcr;
	int i;

	for (i = ECPP_REG_WRITE_MAX_LOOP; i > 0; i--) {
		PP_PUTB(pp->i_handle, &pp->i_reg->dcr, dcr_byte);

		current_dcr = PP_GETB(pp->i_handle, &pp->i_reg->dcr);

		if (dcr_byte == current_dcr)
			return (SUCCESS);
	}
	ecpp_error(pp->dip,
		"(%d)dcr_write: dcr written =%x, dcr readback =%x\n",
				i, dcr_byte, current_dcr);

	return (FAILURE);
}


static uint8_t
ecpp_config_superio(struct ecppunit *pp)
{
	uint8_t pmc, fcr;

	pp->current_phase = ECPP_PHASE_INIT;

	/* ECP DMA configuration bit (PMC4) must be set */
	pmc = read_config_reg(pp, PMC);
	if (!(pmc & 0x08))
		write_config_reg(pp, PMC, pmc | 0x08);

	/*
	 * The Parallel Port Multiplexor pins must be driven.
	 * Check to see if FCR3 is zero, if not clear FCR3.
	 */
	fcr = read_config_reg(pp, FCR);
	if (fcr & 0x8)
		write_config_reg(pp, FCR, fcr & 0xF7);

	/*
	 * Bits CTR0-3 must be 0100b (aka DCR) prior to enabling ECP mode
	 * CTR5 can not be cleared in SPP mode, CTR5 will return 1.
	 * "FAILURE" in this case is ok.  Better to use dcr_write()
	 * to ensure reliable writing to DCR.
	 */
	if (dcr_write(pp, ECPP_DCR_SET | ECPP_nINIT) == FAILURE)
		ecpp_error(pp->dip, "ecpp_config_superio: DCR config\n");

	/* enable ECP mode, pulse intr (note that DCR bits 3-0 == 0100b) */
	write_config_reg(pp, PCR, 0x14);

	/* put SuperIO in initial state */
	if (ecr_write(pp, (ECR_mode_000 | ECPP_INTR_MASK |
		ECPP_INTR_SRV | ECPP_FIFO_EMPTY)) == FAILURE)
			ecpp_error(pp->dip, "ecpp_config_superio: ECR\n");

	if (dcr_write(pp, ECPP_DCR_SET | ECPP_SLCTIN | ECPP_nINIT) == FAILURE) {
		ecpp_error(pp->dip, "ecpp_config_superio: w/DCR failed2.\n");
		pp->current_mode = ECPP_FAILURE_MODE;
		return (FAILURE);

	}
	/* we are in centronic mode */
	pp->current_mode = ECPP_CENTRONICS;

	/* in compatible mode with no data transfer in progress */
	pp->current_phase = ECPP_PHASE_C_IDLE;

	return (SUCCESS);
}

/*
 * ecpp_check_periph() waits until the peripheral is in a ready state
 * after it has been selected.
 */

static void
ecpp_check_periph(struct ecppunit *pp)
{
	uint8_t dsr;
	int ptimeout;

	/*
	 * After a printer has undergone a Power-On-Reset sequence
	 * (power toggle) the Busy signal will remain asserted for several
	 * milliseconds until the device is ready.  The host should
	 * assert SelectIn to indicate to the peripheral that it has been
	 * selected. The peripheral should respond by asserting the
	 * Select signal.
	 *
	 * Some Centronics devices require an Init signal pulse to
	 * occur after a POR sequence.   This may be done here or later
	 * through ioctl(2).
	 */


	/* Toggle the nInit signal if configured in ecpp.conf */
	if (pp->init_seq == TRUE) {
		drv_usecwait(2);
		if (dcr_write(pp, ECPP_DCR_SET | ECPP_SLCTIN) == FAILURE)
			ecpp_error(pp->dip,
				"ecpp_check_periph: w/DCR failed\n");
		drv_usecwait(2);
		if (dcr_write(pp, ECPP_DCR_SET | ECPP_SLCTIN | ECPP_nINIT)
			== FAILURE)
		ecpp_error(pp->dip, "ecpp_check_periph: w/DCR1 failed\n");
	}

	/* Wait until peripheral is no longer BUSY */
	ptimeout = 100;
	do {
		ptimeout--;
		delay(1 * drv_usectohz(5000));
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		ecpp_error(pp->dip, "ecpp_check_periph: BUSY:dsr=%x\n", dsr);
	} while (((dsr & ECPP_nBUSY) == 0) && ptimeout);

	if (ptimeout == 0) {
		ecpp_error(pp->dip,
		    "ecpp_check_periph:Busy timeout DSR %x\n", dsr);
	}

	/* Wait until peripheral indicates it has been selected */
	ptimeout = 100;
	do {
		ptimeout--;
		delay(1 * drv_usectohz(5000));
		dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		ecpp_error(pp->dip,
			"ecpp_check_periph: not Sel :dsr=%x\n", dsr);
	} while (((dsr & (ECPP_nBUSY | ECPP_nACK | ECPP_SLCT | ECPP_nERR)) !=
			(ECPP_nBUSY | ECPP_nACK | ECPP_SLCT | ECPP_nERR)) &&
			ptimeout);

}

static uchar_t
ecpp_reset_port_regs(struct ecppunit *pp)
{

	if (ecr_write(pp, (ECR_mode_000 | ECPP_INTR_MASK |
		ECPP_INTR_SRV | ECPP_FIFO_EMPTY)) == FAILURE) {
		return (FAILURE);
	}

	RESET_DMAC_CSR(pp);
	set_dmac_bcr(pp, 0);

	return (SUCCESS);
}

static void
ecpp_fifo_timer(void *arg)
{
	struct ecppunit *pp = arg;
	uint8_t ecr;

	mutex_enter(&pp->umutex);

	/*
	 * If the FIFO timer has been turned off, exit.
	 */
	if (pp->no_more_fifo_timers == TRUE) {
		ecpp_error(pp->dip,
		    "ecpp_fifo_timer: non_more_fifo_timers is TRUE. bye.\n");
		pp->fifo_timer_id = 0;
		mutex_exit(&pp->umutex);
		return;
	}

	/*
	 * If the FIFO is not empty restart timer unless FIFO
	 * doesn't drain in 1 second.
	 *
	 * FIFO_DRAIN_PERIOD is 250ms so i multiple by 4 to have 1 sec.
	 */

	ecr = PP_GETB(pp->f_handle, &pp->f_reg->ecr);

	if ((pp->current_mode != ECPP_DIAG_MODE) &&
		(((ecr & ECPP_FIFO_EMPTY) == 0) ||
		(pp->ecpp_drain_counter >
			(4 * pp->xfer_parms.write_timeout)))) {

		ecpp_error(pp->dip,
			"ecpp_fifo_timer(%d):FIFO not empty:ecr=%x\n",
			pp->ecpp_drain_counter, ecr);

		if (pp->no_more_fifo_timers == FALSE) {
			pp->fifo_timer_id = timeout(ecpp_fifo_timer,
			    (caddr_t)pp, drv_usectohz(FIFO_DRAIN_PERIOD));
			++pp->ecpp_drain_counter;
		}
		mutex_exit(&pp->umutex);
		return;
	}


	/*
	 * In ECPP_DIAG_MODE, TFIFO data will be sitting there
	 * It has to be cleared here.
	 */

	if (pp->current_mode == ECPP_DIAG_MODE) {
		ecpp_error(pp->dip,
			"ecpp_fifo_timer: Clearing TFIFO: ecr=%x\n", ecr);
	} else
		/*
		 * If the FIFO won't drain after 1 second, don't wait
		 * any longer.  Simply continue cleaning up the transfer.
		 */
		if (pp->ecpp_drain_counter > (4 * pp->xfer_parms.write_timeout))
			ecpp_error(pp->dip,
			"ecpp_fifo_timer(%d):clearing FIFO,can't wait:ecr=%x\n",
				pp->ecpp_drain_counter, ecr);
		else
			ecpp_error(pp->dip,
				"ecpp_fifo_timer(%d):FIFO empty:ecr=%x\n",
				pp->ecpp_drain_counter, ecr);

	pp->ecpp_drain_counter = 0;

	/*
	 * Main section of routine:
	 * 1: stop the DMA transfer timer
	 * 2: unbind the DMA mapping
	 * 3: If last mblk in queue, signal to close() & return to idle state
	 */

	/* Stop the DMA transfer timeout timer */
	if (pp->timeout_id) {
		pp->about_to_untimeout = 1;
		mutex_exit(&pp->umutex);

		(void) untimeout(pp->timeout_id);

		mutex_enter(&pp->umutex);
		pp->about_to_untimeout = 0;
	}

	/* data has drained from fifo, it is ok to free dma resource */
	if (pp->io_mode == ECPP_DMA || pp->current_mode == ECPP_DIAG_MODE) {
		if (pp->current_mode != ECPP_DIAG_MODE)
			ASSERT(--pp->dma_cookie_count == 0);
		if (ddi_dma_unbind_handle(pp->dma_handle) != DDI_SUCCESS)
		    ecpp_error(pp->dip, "ecpp_fifo_timer: unbind FAILURE \n");
	}


	/*
	 * if we did not use the dmablock, the mblk that
	 * was used should be freed.
	 */
	if (pp->msg != NULL) {
		freemsg(pp->msg);
		pp->msg = (mblk_t *)NULL;
	}

	/* The port is no longer active */
	pp->e_busy = ECPP_IDLE;


	/* If the wq is not empty, wake up the scheduler. */
	if (qsize(pp->writeq))
		qenable(pp->writeq);
	else {
		/*
		 * No more data in wq, return to idle state,
		 * signal ecpp_close().
		 */
		pp->need_idle_state = TRUE;
		cv_signal(&pp->pport_cv);
	}

	if (pp->need_idle_state == TRUE) {
		if (ecpp_idle_phase(pp) == FAILURE) {
			pp->error_status = ECPP_1284_ERR;
			ecpp_error(pp->dip,
			    "ecpp_fifo_timer:idle FAILED.\n");
			ecpp_error(pp->dip,
			    "ecpp_fifo_timer:ecr=%x, dsr=%x, dcr=%x\n",
			    PP_GETB(pp->f_handle, &pp->f_reg->ecr),
			    PP_GETB(pp->i_handle, &pp->i_reg->dsr),
			    PP_GETB(pp->i_handle, &pp->i_reg->dcr));
		}
		pp->need_idle_state = FALSE;
	}

	mutex_exit(&pp->umutex);

}

static uchar_t
ecpp_check_status(struct ecppunit *pp)
{
	uint8_t dsr;

	dsr = PP_GETB(pp->i_handle, &pp->i_reg->dsr);
		if ((dsr & ECPP_PE) ||
			((dsr & (ECPP_nERR|ECPP_SLCT|ECPP_nBUSY))
				!= (ECPP_nERR|ECPP_SLCT|ECPP_nBUSY))) {
				pp->e_busy = ECPP_ERR;
				return (FAILURE);
		} else {
			cv_signal(&pp->pport_cv);
			return (SUCCESS);
		}
}

static void
ecpp_wsrv_timer(void *arg)
{
	struct ecppunit *pp = arg;

	ecpp_error(pp->dip, "ecpp_wsrv_timer:starting\n");

	mutex_enter(&pp->umutex);

	if (pp->about_to_untimeout_wsrvt) {
		mutex_exit(&pp->umutex);
		return;
	}

	ecpp_error(pp->dip, "ecpp_wsrv_timer:qenabling...\n");
	pp->wsrv_timer = FALSE;
	qenable(pp->writeq);

	mutex_exit(&pp->umutex);
}
