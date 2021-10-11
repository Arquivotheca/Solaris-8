/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isp.c	1.205	99/11/28 SMI"

/*
 * isp - Emulex/QLogic Intelligent SCSI Processor driver for
 *	ISP 1000 and 1040A
 */

#if defined(lint) && !defined(DEBUG)
#define	DEBUG	1
#endif

#ifdef DEBUG
#define	ISPDEBUG
static int ispdebug = 0;
static int isp_enable_brk_fatal = 0;
static int isp_watch_disable = 0; /* Disable watchdog for debug */
#include <sys/debug.h>
#endif	/* DEBUG */

static int isp_timeout_debug = 0;
static int isp_state_debug = 0;
static int isp_debug_mbox = 0;
static int isp_debug_ars = 0;


#include <sys/note.h>
#include <sys/modctl.h>
#include <sys/pci.h>
#include <sys/scsi/scsi.h>

#include <sys/scsi/adapters/ispmail.h>
#include <sys/scsi/adapters/ispvar.h>
#include <sys/scsi/adapters/ispreg.h>
#include <sys/scsi/adapters/ispcmd.h>

#include <sys/scsi/impl/scsi_reset_notify.h>

#include <sys/varargs.h>

/*
 * NON-ddi compliant include files
 */
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/kstat.h>


/*
 * the values of the following variables are used to initialize
 * the cache line size and latency timer registers in the PCI
 * configuration space.  variables are used instead of constants
 * to allow tuning.
 */
static int isp_conf_cache_linesz = 0x10;	/* 64 bytes */
static int isp_conf_latency_timer = 0x40;	/* 64 PCI cycles */

/*
 * Starting risc code address for ISP1040 and ISP1000.
 */
static unsigned short isp_risc_code_addr = 0x1000;

/*
 * patch in case of hw problems
 */
static int isp_burst_sizes_limit = 0xff;

/*
 * ISP firmware download options:
 *	ISP_DOWNLOAD_FW_OFF		=> no download (WARNING)
 *	ISP_DOWNLOAD_FW_IF_NEWER	=>
 *		download if f/w level > current f/w level
 *	ISP_DOWNLOAD_FW_ALWAYS		=> always download
 *
 * WARNING: running with "download off" will probably cause the driver/chip
 * to blow up, since our driver uses mailbox commands the std. firmware
 * does not have
 */
int isp_download_fw = ISP_DOWNLOAD_FW_ALWAYS;

/*
 * Firmware related externs
 */
extern ushort_t isp_sbus_risc_code[];
extern ushort_t isp_1040_risc_code[];
extern ushort_t isp_sbus_risc_code_length;
extern ushort_t isp_1040_risc_code_length;

#ifdef OLDTIMEOUT
/*
 * non-ddi compliant
 * lbolt is used for packet timeout handling
 */
extern volatile clock_t	lbolt;
#endif

/*
 * Hotplug support
 * Leaf ops (hotplug controls for target devices)
 */
#ifdef	ISPDEBUG_FW
static int isp_new_fw(dev_t dev, struct uio *uio, cred_t *cred_p);
#endif	/* ISPDEBUG_FW */

#ifdef	ISPDEBUG_IOCTL

/*
 * for allowing ioctls for debugging
 */

static int isp_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

#ifndef	ISP_RELOAD_FIRMWARE
#define	ISP_RELOAD_FIRMWARE		(('i' << 8) | 0x01)
#endif

#ifndef	ISP_PRINT_DEVQ_STATS
#define	ISP_PRINT_DEVQ_STATS		(('i' << 8) | 0x02)
#endif

#ifndef	ISP_RESET_TARGET
#define	ISP_RESET_TARGET		(('i' << 8) | 0x03)
#endif

#ifndef ISP_PRINT_SLOTS
#define	ISP_PRINT_SLOTS			(('i'<<8)|0x05)
#endif



static int	isp_debug_ioctl = 0;

#endif	/* ISPDEBUG_IOCTL */

static struct cb_ops isp_cb_ops = {
	scsi_hba_open,
	scsi_hba_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
#ifdef ISPDEBUG_FW
	isp_new_fw,	/* write -- replace F/W image */
#else
	nodev,		/* write */
#endif
#ifdef	ISPDEBUG_IOCTL
	isp_ioctl,	/* ioctl */
#else
	scsi_hba_ioctl,	/* ioctl */
#endif
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,
	D_NEW | D_MP | D_HOTPLUG,
	CB_REV,		/* rev */
	nodev,		/* int (*cb_aread)() */
	nodev		/* int (*cb_awrite)() */
};

/*
 * dev_ops functions prototypes
 */
static int isp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int isp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int isp_dr_detach(dev_info_t *dip);

/*
 * Function prototypes
 *
 * SCSA functions exported by means of the transport table
 */
static int isp_scsi_tgt_probe(struct scsi_device *sd,
    int (*waitfunc)(void));
static int isp_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd);

static int isp_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt);
static int isp_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt);
static int isp_scsi_reset(struct scsi_address *ap, int level);
static int isp_scsi_getcap(struct scsi_address *ap, char *cap, int whom);
static int isp_scsi_setcap(struct scsi_address *ap, char *cap, int value,
    int whom);
static struct scsi_pkt *isp_scsi_init_pkt(struct scsi_address *ap,
    struct scsi_pkt *pkt, struct buf *bp, int cmdlen, int statuslen,
    int tgtlen, int flags, int (*callback)(), caddr_t arg);
static void isp_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt);
static void isp_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pkt);
static void isp_scsi_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pkt);
static int isp_scsi_reset_notify(struct scsi_address *ap, int flag,
    void (*callback)(caddr_t), caddr_t arg);
static int isp_scsi_quiesce(dev_info_t *dip);
static int isp_scsi_unquiesce(dev_info_t *dip);

static void isp_i_send_marker(struct isp *isp, short mod, ushort_t tgt,
    ushort_t lun);

static void isp_i_add_marker_to_list(struct isp *isp, short mod, ushort_t tgt,
    ushort_t lun);


/*
 * isp interrupt handlers
 */
static uint_t isp_intr(caddr_t arg);

/*
 * internal functions
 */
static int isp_i_commoncap(struct scsi_address *ap, char *cap, int val,
    int tgtonly, int doset);
static int isp_i_updatecap(struct isp *isp, int start_tgt, int end_tgt);
static void isp_i_update_props(struct isp *isp, int tgt, ushort_t cap,
    ushort_t synch);
static void isp_i_update_this_prop(struct isp *isp, char *property,
    int value, int size, int flag);
static void isp_i_update_sync_prop(struct isp *isp, struct isp_cmd *sp);
static void isp_i_initcap(struct isp *isp, int start_tgt, int end_tgt);

static void isp_i_watch();
static void isp_i_watch_isp(struct isp *isp);
#ifdef OLDTIMEOUT
static void isp_i_old_watch_isp(struct isp *isp, clock_t local_lbolt);
static int isp_i_is_fatal_timeout(struct isp *isp, struct isp_cmd *sp);
#endif
static void isp_i_fatal_error(struct isp *isp, int flags);

static void isp_i_empty_waitQ(struct isp *isp);
static int isp_i_start_cmd(struct isp *isp, struct isp_cmd *sp);
static int isp_i_find_freeslot(struct isp *isp);
static int isp_i_polled_cmd_start(struct isp *isp, struct isp_cmd *sp);
static void isp_i_call_pkt_comp(struct isp_cmd *head);
static void isp_i_handle_arq(struct isp *isp, struct isp_cmd *sp);
static int isp_i_handle_mbox_cmd(struct isp *isp);

static void isp_i_qflush(struct isp *isp,
    ushort_t start_tgt, ushort_t end_tgt);
static int isp_i_reset_interface(struct isp *isp, int action);
static int isp_i_reset_init_chip(struct isp *isp);
static int isp_i_set_marker(struct isp *isp,
    short mod, short tgt, short lun);

static void isp_i_log(struct isp *isp, int level, char *fmt, ...);
static void isp_i_print_state(int level, struct isp *isp);

static int isp_i_async_event(struct isp *isp, short event);
static int isp_i_response_error(struct isp *isp, struct isp_response *resp);

static void isp_i_mbox_cmd_init(struct isp *isp,
    struct isp_mbox_cmd *mbox_cmdp, uchar_t n_mbox_out, uchar_t n_mbox_in,
    ushort_t reg0, ushort_t reg1, ushort_t reg2,
    ushort_t reg3, ushort_t reg4, ushort_t reg5);
static int isp_i_mbox_cmd_start(struct isp *isp,
    struct isp_mbox_cmd *mbox_cmdp);
static void isp_i_mbox_cmd_complete(struct isp *isp);

static int isp_i_download_fw(struct isp *isp,
    ushort_t risc_addrp, ushort_t *fw_addrp, ushort_t fw_len);
static int isp_i_alive(struct isp *isp);
static int isp_i_handle_qfull_cap(struct isp *isp,
	ushort_t start, ushort_t end, int val,
	int flag_get_set, int flag_cmd);

static int isp_i_pkt_alloc_extern(struct isp *isp, struct isp_cmd *sp,
	int cmdlen, int tgtlen, int statuslen, int kf);
static void isp_i_pkt_destroy_extern(struct isp *isp, struct isp_cmd *sp);
static int isp_quiesce_bus(struct isp  *isp);
static int isp_unquiesce_bus(struct isp  *isp);
static int isp_mailbox_all(struct isp *isp, uchar_t n_mbox_out,
	uchar_t n_mbox_in, uint16_t reg0, uint16_t reg1, uint16_t reg2,
	uint16_t reg3, uint16_t reg4, uint16_t reg5, uint16_t targets);
static void isp_i_check_waitQ(struct isp *isp);
static int isp_i_outstanding(struct isp *isp);
#ifdef ISPDEBUG
static void isp_i_test(struct isp *isp, struct isp_cmd *sp);
#endif

static void	isp_i_update_queue_space(struct isp *isp);


/*
 * kmem cache constuctor and destructor
 */
static int isp_kmem_cache_constructor(void * buf, void *cdrarg, int kmflags);
static void isp_kmem_cache_destructor(void * buf, void *cdrarg);

/* for updating the max no. of LUNs for a tgt */
static void isp_update_max_luns(struct isp *isp, int tgt);


/*
 * waitQ macros, refer to comments on waitQ below
 */
#define	ISP_CHECK_WAITQ(isp)				\
	mutex_enter(ISP_WAITQ_MUTEX(isp));		\
	isp_i_empty_waitQ(isp);

#define	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp)		\
	ISP_CHECK_WAITQ(isp)				\
	mutex_exit(ISP_REQ_MUTEX(isp));			\
	mutex_exit(ISP_WAITQ_MUTEX(isp));
/*
 * mutex for protecting variables shared by all instances of the driver
 */
static kmutex_t isp_global_mutex;

/*
 * mutex for protecting isp_log_buf which is shared among instances.
 */
static kmutex_t isp_log_mutex;

/*
 * readers/writer lock to protect the integrity of the softc structure
 * linked list while being traversed (or updated).
 */
static krwlock_t isp_global_rwlock;

/*
 * Local static data
 */
static void *isp_state = NULL;
static struct isp *isp_head;	/* for linking softc structures */
static struct isp *isp_tail;	/* for linking softc structures */
static int isp_scsi_watchdog_tick; /* isp_i_watch() interval in sec */
static int isp_tick;		/* isp_i_watch() interval in HZ */
static timeout_id_t isp_timeout_id;	/* isp_i_watch() timeout id */
static int isp_selection_timeout = 250;
static int timeout_initted = 0;	/* isp_i_watch() timeout status */
static	char	isp_log_buf[256]; /* buffer used in isp_i_log */


/*
 * default isp dma attr structure describes device
 * and DMA engine specific attributes/constrains necessary
 * to allocate DMA resources for ISP device.
 *
 * we currently don't support PCI 64-bit addressing supported by
 * 1040A card. 64-bit addressing allows 1040A to operate in address
 * spaces greater than 4 gigabytes.
 */
static ddi_dma_attr_t dma_ispattr = {
	DMA_ATTR_V0,				/* dma_attr_version */
	(unsigned long long)0,			/* dma_attr_addr_lo */
	(unsigned long long)0xffffffffULL,	/* dma_attr_addr_hi */
	(unsigned long long)0x00ffffff,		/* dma_attr_count_max */
	(unsigned long long)1,			/* dma_attr_align */
	DEFAULT_BURSTSIZE | BURST32 | BURST64 | BURST128,
						/* dma_attr_burstsizes */
	1,					/* dma_attr_minxfer */
	(unsigned long long)0x00ffffff,		/* dma_attr_maxxfer */
	(unsigned long long)0xffffffffULL,	/* dma_attr_seg */
	1,					/* dma_attr_sgllen */
	512,					/* dma_attr_granular */
	0					/* dma_attr_flags */
};

/*
 * warlock directives
 */
_NOTE(MUTEX_PROTECTS_DATA(isp_global_mutex, timeout_initted))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_hotplug_mutex, isp::isp_softstate))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_hotplug_mutex, isp::isp_cv))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_hotplug_mutex, isp::isp_hotplug_waiting))
_NOTE(MUTEX_PROTECTS_DATA(isp_global_mutex, isp::isp_next))
_NOTE(MUTEX_PROTECTS_DATA(isp_global_mutex, isp_timeout_id))
_NOTE(MUTEX_PROTECTS_DATA(isp_global_mutex, isp_head isp_tail))
_NOTE(MUTEX_PROTECTS_DATA(isp_log_mutex, isp_log_buf))

_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", isp_response))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", isp_request))
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", scsi_arq_status))
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", buf scsi_pkt isp_cmd scsi_cdb))
_NOTE(SCHEME_PROTECTS_DATA("stable data", scsi_device scsi_address))
_NOTE(SCHEME_PROTECTS_DATA("protected by mutexes or no competing threads",
	isp_biu_regs isp_mbox_regs isp_sxp_regs isp_risc_regs))

_NOTE(DATA_READABLE_WITHOUT_LOCK(isp_tick))
_NOTE(DATA_READABLE_WITHOUT_LOCK(ispdebug))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_watchdog_tick))
_NOTE(DATA_READABLE_WITHOUT_LOCK(scsi_reset_delay scsi_hba_tran))

/*
 * autoconfiguration routines.
 */
static struct dev_ops isp_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	isp_attach,		/* attach */
	isp_detach,		/* detach */
	nodev,			/* reset */
	&isp_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	NULL			/* no power management */
};

char _depends_on[] = "misc/scsi";

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module. This one is a driver */
	"ISP SCSI HBA Driver 1.205", /* Name of the module. */
	&isp_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

int
_init(void)
{
	int ret;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	ret = ddi_soft_state_init(&isp_state, sizeof (struct isp),
	    ISP_INITIAL_SOFT_SPACE);
	if (ret != 0)
		return (ret);

	mutex_init(&isp_global_mutex, NULL, MUTEX_DRIVER, NULL);
	rw_init(&isp_global_rwlock, NULL, RW_DRIVER, NULL);
	mutex_init(&isp_log_mutex, NULL, MUTEX_DRIVER, NULL);

	if ((ret = scsi_hba_init(&modlinkage)) != 0) {
		rw_destroy(&isp_global_rwlock);
		mutex_destroy(&isp_global_mutex);
		mutex_destroy(&isp_log_mutex);
		ddi_soft_state_fini(&isp_state);
		return (ret);
	}

	ret = mod_install(&modlinkage);
	if (ret != 0) {
		scsi_hba_fini(&modlinkage);
		rw_destroy(&isp_global_rwlock);
		mutex_destroy(&isp_global_mutex);
		mutex_destroy(&isp_log_mutex);
		ddi_soft_state_fini(&isp_state);
	}

	return (ret);
}

/*
 * nexus drivers are currently not unloaded so this routine is really redundant
 */
int
_fini(void)
{
	int ret;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	if ((ret = mod_remove(&modlinkage)) != 0)
		return (ret);

	scsi_hba_fini(&modlinkage);

	rw_destroy(&isp_global_rwlock);
	mutex_destroy(&isp_global_mutex);
	mutex_destroy(&isp_log_mutex);

	ddi_soft_state_fini(&isp_state);

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);
	return (mod_info(&modlinkage, modinfop));
}


static int
isp_scsi_tgt_probe(struct scsi_device *sd, int (*waitfunc)(void))
{
	dev_info_t dip = ddi_get_parent(sd->sd_dev);
	int rval = SCSIPROBE_FAILURE;
	scsi_hba_tran_t *tran;
	struct isp *isp;
	int tgt = sd->sd_address.a_target;


	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ASSERT(tran != NULL);
	isp = TRAN2ISP(tran);

	/*
	 * force renegotiation since inquiry cmds do not always
	 * cause check conditions
	 */
	ISP_MUTEX_ENTER(isp);
	/* no use continuing if the LUN is larger than we support */
	if (sd->sd_address.a_lun >= isp->isp_max_lun[tgt]) {
		isp_i_log(isp, CE_WARN,
		    "probe request for LUN %d denied: max LUN %d",
		    sd->sd_address.a_lun, isp->isp_max_lun[tgt] - 1);
		ISP_MUTEX_EXIT(isp);
		return (rval);
	}
	(void) isp_i_updatecap(isp, tgt, tgt);
	ISP_MUTEX_EXIT(isp);

	rval = scsi_hba_probe(sd, waitfunc);

	/*
	 * the scsi-options precedence is:
	 *	target-scsi-options		highest
	 * 	device-type-scsi-options
	 *	per bus scsi-options
	 *	global scsi-options		lowest
	 */
	ISP_MUTEX_ENTER(isp);
	/* does this target exist ?? */
	if (rval == SCSIPROBE_EXISTS) {
		/* are options defined for this tgt ?? */
		if ((isp->isp_target_scsi_options_defined & (1 << tgt)) ==
		    0) {
			int options;

			if ((options = scsi_get_device_type_scsi_options(dip,
			    sd, -1)) != -1) {
				/* dev-specific options were found */
				isp->isp_target_scsi_options[tgt] = options;
				isp_i_initcap(isp, tgt, tgt);
				(void) isp_i_updatecap(isp, tgt, tgt);

				/* log scsi-options for this LUN */
				isp_i_log(isp, CE_WARN,
				    "?target%x-scsi-options = 0x%x", tgt,
				    isp->isp_target_scsi_options[tgt]);
			}

			/* update max LUNs for this tgt */
			isp_update_max_luns(isp, tgt);

		}
	}
	ISP_MUTEX_EXIT(isp);

	ISP_DEBUG(isp, SCSI_DEBUG, "target%x-scsi-options= 0x%x\n",
		tgt, isp->isp_target_scsi_options[tgt]);

	return (rval);
}


/*
 * update max luns for this target
 *
 * called with isp mutex held
 */
static void
isp_update_max_luns(struct isp *isp, int tgt)
{
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	switch (SCSI_OPTIONS_NLUNS(isp->isp_target_scsi_options[tgt])) {
	case SCSI_OPTIONS_NLUNS_32:
		isp->isp_max_lun[tgt] = SCSI_32LUNS_PER_TARGET;
		break;
	case SCSI_OPTIONS_NLUNS_16:
		isp->isp_max_lun[tgt] = SCSI_16LUNS_PER_TARGET;
		break;
	case SCSI_OPTIONS_NLUNS_8:
		isp->isp_max_lun[tgt] = NLUNS_PER_TARGET;
		break;
	case SCSI_OPTIONS_NLUNS_1:
		isp->isp_max_lun[tgt] = SCSI_1LUN_PER_TARGET;
		break;
	case SCSI_OPTIONS_NLUNS_DEFAULT:
		isp->isp_max_lun[tgt] = (uchar_t)ISP_NLUNS_PER_TARGET;
		break;
	default:
		/* do something sane and print a warning */
		isp_i_log(isp, CE_WARN,
		    "unknown scsi-options value for max luns: using %d",
		    isp->isp_max_lun[tgt]);
		break;
	}

}


/*ARGSUSED*/
static int
isp_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	int		res = DDI_FAILURE;
	scsi_hba_tran_t	*tran;
	struct isp	*isp;
	int		tgt;


	ASSERT(hba_dip != NULL);
	tran = (scsi_hba_tran_t *)ddi_get_driver_private(hba_dip);
	ASSERT(tran != NULL);
	isp = TRAN2ISP(tran);
	ASSERT(isp != NULL);

	ASSERT(sd != NULL);
	tgt = sd->sd_address.a_target;

	mutex_enter(ISP_RESP_MUTEX(isp));
	if ((tgt < NTARGETS_WIDE) &&
	    (sd->sd_address.a_lun < isp->isp_max_lun[tgt])) {
		res = DDI_SUCCESS;
	}
	mutex_exit(ISP_RESP_MUTEX(isp));

	return (res);
}


/*
 * Attach isp host adapter.  Allocate data structures and link
 * to isp_head list.  Initialize the isp and we're
 * on the air.
 */
/*ARGSUSED*/
static int
isp_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	char buf[64];
	int id;
	int mutex_initted = 0;
	int interrupt_added = 0;
	int bound_handle = 0;
	struct isp *isp;
	int instance;
	struct isp_regs_off isp_reg_off;
	scsi_hba_tran_t *tran = NULL;
	ddi_device_acc_attr_t dev_attr;
	size_t rlen;
	uint_t count;
	struct isp *s_isp, *l_isp;
	ddi_dma_attr_t tmp_dma_attr;
	char *prop_template = "target%x-scsi-options";
	char prop_str[32];
	int rval;
	int i;
	/*
	 * in case we are running without OBP (like Qlogic 1040a card)
	 */
	static int isp_pci_no_obp = 0;
	int	dt_len = 0;


	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		dev_attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
		dev_attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
		break;

	case DDI_RESUME:
		tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		if (!tran) {
			return (DDI_FAILURE);
		}
		isp = TRAN2ISP(tran);
		if (!isp) {
			return (DDI_FAILURE);
		}

		/*
		 * the downloaded firmware on the card will be erased by
		 * the power cycle and a new download is needed.
		 */
		ISP_MUTEX_ENTER(isp);

		if (isp->isp_shutdown) {
			/*
			 * If the isp was broken there's no point bringing it
			 * back up.
			 */
			isp->isp_suspended = 0;
			ISP_MUTEX_EXIT(isp);
		} else {
			if (isp->isp_bus == ISP_SBUS) {
				rval = isp_i_download_fw(isp,
					isp_risc_code_addr, isp_sbus_risc_code,
					isp_sbus_risc_code_length);
			} else {
				rval = isp_i_download_fw(isp,
					isp_risc_code_addr, isp_1040_risc_code,
					isp_1040_risc_code_length);
			}
			if (rval) {
				ISP_MUTEX_EXIT(isp);
				return (DDI_FAILURE);
			}
			isp->isp_suspended = 0;
			if (isp_i_reset_interface(isp,
			    ISP_RESET_BUS_IF_BUSY)) {
				ISP_MUTEX_EXIT(isp);
				return (DDI_FAILURE);
			}
			mutex_exit(ISP_RESP_MUTEX(isp));
			ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
			mutex_enter(&isp_global_mutex);
			if (isp_timeout_id == 0) {
				isp_timeout_id =
					timeout(isp_i_watch, NULL, isp_tick);
				timeout_initted = 1;
			}
			mutex_exit(&isp_global_mutex);
		}

		return (DDI_SUCCESS);

	default:
		isp_i_log(NULL, CE_WARN,
		    "isp%d: Cmd != DDI_ATTACH/DDI_RESUME", instance);
		return (DDI_FAILURE);
	}

	/*
	 * Since we know that some instantiations of this device can
	 * be plugged into slave-only SBus slots, check to see whether
	 * this is one such.
	 */
	if (ddi_slaveonly(dip) == DDI_SUCCESS) {
		isp_i_log(NULL, CE_WARN,
		    "isp%d: Device in slave-only slot, unused",
		    instance);
		return (DDI_FAILURE);
	}

	if (ddi_intr_hilevel(dip, 0)) {
		/*
		 * Interrupt number '0' is a high-level interrupt.
		 * At this point you either add a special interrupt
		 * handler that triggers a soft interrupt at a lower level,
		 * or - more simply and appropriately here - you just
		 * fail the attach.
		 */
		isp_i_log(NULL, CE_WARN,
		    "isp%d: Device is using a hilevel intr, unused",
		    instance);
		return (DDI_FAILURE);
	}

	/*
	 * Allocate isp data structure.
	 */
	if (ddi_soft_state_zalloc(isp_state, instance) != DDI_SUCCESS) {
		isp_i_log(NULL, CE_WARN, "isp%d: Failed to alloc soft state",
		    instance);
		return (DDI_FAILURE);
	}

	isp = (struct isp *)ddi_get_soft_state(isp_state, instance);
	if (isp == NULL) {
		isp_i_log(NULL, CE_WARN, "isp%d: Bad soft state", instance);
		ddi_soft_state_free(isp_state, instance);
		return (DDI_FAILURE);
	}

	/*
	 * get device type of parent to figure out which bus we are on
	 */
	dt_len = sizeof (prop_str);
	if (ddi_prop_op(DDI_DEV_T_ANY, ddi_get_parent(dip),
	    PROP_LEN_AND_VAL_BUF, DDI_PROP_DONTPASS | DDI_PROP_CANSLEEP,
	    "device_type", prop_str,
	    &dt_len) != DDI_SUCCESS) {
		/* must be (an older?) SPARC/SBUS system */
		isp->isp_bus = ISP_SBUS;
	} else {
		prop_str[dt_len] = '\0';
		if (strcmp("pci", prop_str) == 0) {
			isp->isp_bus = ISP_PCI;
		} else {
			isp->isp_bus = ISP_SBUS;
		}
	}

	/*
	 * set up as much bus-specific stuff as we can here
	 */
	if (isp->isp_bus == ISP_SBUS) {

		ISP_DEBUG(isp, SCSI_DEBUG, "isp bus is ISP_SBUS");

		isp->isp_reg_number = ISP_SBUS_REG_NUMBER;
		dev_attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
		isp_reg_off.isp_biu_regs_off = ISP_BUS_BIU_REGS_OFF;
		isp_reg_off.isp_mbox_regs_off = ISP_SBUS_MBOX_REGS_OFF;
		isp_reg_off.isp_sxp_regs_off = ISP_SBUS_SXP_REGS_OFF;
		isp_reg_off.isp_risc_regs_off = ISP_SBUS_RISC_REGS_OFF;

	} else {

		ISP_DEBUG(isp, SCSI_DEBUG, "isp bus is ISP_PCI");

		isp->isp_reg_number = ISP_PCI_REG_NUMBER;
		dev_attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
		isp_reg_off.isp_biu_regs_off = ISP_BUS_BIU_REGS_OFF;
		isp_reg_off.isp_mbox_regs_off = ISP_PCI_MBOX_REGS_OFF;
		isp_reg_off.isp_sxp_regs_off = ISP_PCI_SXP_REGS_OFF;
		isp_reg_off.isp_risc_regs_off = ISP_PCI_RISC_REGS_OFF;
		/*
		 * map in pci config space
		 */
		if (pci_config_setup(dip, &isp->isp_pci_config_acc_handle) !=
		    DDI_SUCCESS) {
			isp_i_log(isp, CE_WARN,
			    "Unable to map pci config registers");
			ddi_soft_state_free(isp_state, instance);
			return (DDI_FAILURE);
		}
	}

	/*
	 * map in device registers
	 */

	if (ddi_regs_map_setup(dip, isp->isp_reg_number,
	    (caddr_t *)&isp->isp_biu_reg, isp_reg_off.isp_biu_regs_off,
	    sizeof (struct isp_biu_regs),
	    &dev_attr, &isp->isp_biu_acc_handle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Unable to map biu registers");
		goto fail;
	}

	if (ddi_regs_map_setup(dip, isp->isp_reg_number,
	    (caddr_t *)&isp->isp_mbox_reg, isp_reg_off.isp_mbox_regs_off,
	    sizeof (struct isp_mbox_regs),
	    &dev_attr, &isp->isp_mbox_acc_handle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Unable to map mbox registers");
		goto fail;
	}

	if (ddi_regs_map_setup(dip, isp->isp_reg_number,
	    (caddr_t *)&isp->isp_sxp_reg, isp_reg_off.isp_sxp_regs_off,
	    sizeof (struct isp_sxp_regs),
	    &dev_attr, &isp->isp_sxp_acc_handle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Unable to map sxp registers");
		goto fail;
	}

	if (ddi_regs_map_setup(dip, isp->isp_reg_number,
	    (caddr_t *)&isp->isp_risc_reg, isp_reg_off.isp_risc_regs_off,
	    sizeof (struct isp_risc_regs),
	    &dev_attr, &isp->isp_risc_acc_handle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Unable to map risc registers");
		goto fail;
	}

	isp->isp_cmdarea = NULL;
	tmp_dma_attr = dma_ispattr;

	if (ddi_dma_alloc_handle(dip, &tmp_dma_attr,
	    DDI_DMA_SLEEP, NULL, &isp->isp_dmahandle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Cannot alloc dma handle");
		goto fail;
	}

	if (ddi_dma_mem_alloc(isp->isp_dmahandle, (size_t)ISP_QUEUE_SIZE,
	    &dev_attr, DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, (caddr_t *)&isp->isp_cmdarea, &rlen,
	    &isp->isp_dma_acc_handle) != DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "Cannot alloc cmd area");
		goto fail;
	}
	if (ddi_dma_addr_bind_handle(isp->isp_dmahandle,
	    NULL, isp->isp_cmdarea, (size_t)ISP_QUEUE_SIZE,
	    DDI_DMA_RDWR|DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &isp->isp_dmacookie,
	    &count) != DDI_DMA_MAPPED) {
		isp_i_log(isp, CE_WARN, "Cannot bind cmd area");
		goto fail;
	}
	bound_handle++;
	bzero(isp->isp_cmdarea, ISP_QUEUE_SIZE);
	isp->isp_request_dvma = isp->isp_dmacookie.dmac_address;
	isp->isp_request_base = (struct isp_request *)isp->isp_cmdarea;

	isp->isp_response_dvma =
		isp->isp_request_dvma + (ISP_MAX_REQUESTS *
		sizeof (struct isp_request));
	isp->isp_response_base = (struct isp_response *)
		((intptr_t)isp->isp_request_base +
		(ISP_MAX_REQUESTS * sizeof (struct isp_request)));
	isp->isp_request_in = isp->isp_request_out = 0;
	isp->isp_response_in = isp->isp_response_out = 0;

	/*
	 * for reset throttling -- when this is set then requests
	 * will be put on the wait queue -- protected by the
	 * wait queue mutex
	 */
	isp->isp_in_reset = 0;

	/*
	 * get cookie so we can initialize the mutexes
	 */
	if (ddi_get_iblock_cookie(dip, (uint_t)0, &isp->isp_iblock)
	    != DDI_SUCCESS) {
		goto fail;
	}

	/*
	 * Allocate a transport structure
	 */
	tran = scsi_hba_tran_alloc(dip, SCSI_HBA_CANSLEEP);

	isp->isp_tran		= tran;
	isp->isp_dip		= dip;

	tran->tran_hba_private	= isp;
	tran->tran_tgt_private	= NULL;
	tran->tran_tgt_init	= isp_scsi_tgt_init;
	tran->tran_tgt_probe	= isp_scsi_tgt_probe;
	tran->tran_tgt_free	= NULL;

	tran->tran_start	= isp_scsi_start;
	tran->tran_abort	= isp_scsi_abort;
	tran->tran_reset	= isp_scsi_reset;
	tran->tran_getcap	= isp_scsi_getcap;
	tran->tran_setcap	= isp_scsi_setcap;
	tran->tran_init_pkt	= isp_scsi_init_pkt;
	tran->tran_destroy_pkt	= isp_scsi_destroy_pkt;
	tran->tran_dmafree	= isp_scsi_dmafree;
	tran->tran_sync_pkt	= isp_scsi_sync_pkt;
	tran->tran_reset_notify = isp_scsi_reset_notify;
	tran->tran_get_bus_addr	= NULL;
	tran->tran_get_name	= NULL;
	tran->tran_quiesce	= isp_scsi_quiesce;
	tran->tran_unquiesce	= isp_scsi_unquiesce;
	tran->tran_bus_reset	= NULL;
	tran->tran_add_eventcall	= NULL;
	tran->tran_get_eventcookie	= NULL;
	tran->tran_post_event		= NULL;
	tran->tran_remove_eventcall	= NULL;


	/*
	 * find the clock frequency of chip
	 */
	isp->isp_clock_frequency =
		ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
			"clock-frequency", -1);

	/*
	 * For PCI-ISP if the clock frequency property does not exist then
	 * assume that the card does not have Fcode. This will also work
	 * for cPCI cards because though the cPCI deamon will create
	 * SUNW,isp node, it will not execute the FCode and hence no
	 * clock-frequency property.
	 */
	if ((isp->isp_clock_frequency == -1) && (isp->isp_bus == ISP_PCI)) {
		isp_pci_no_obp = 1;
	}

	if (isp_pci_no_obp) {
		isp->isp_clock_frequency = 60 * 1000000;
		isp_download_fw = ISP_DOWNLOAD_FW_ALWAYS;
	}


	if (isp->isp_clock_frequency <= 0) {
		isp_i_log(isp, CE_WARN,
		    "Can't determine clock frequency of chip");
		goto fail;
	}
	/*
	 * convert from Hz to MHz, making  sure to round to the nearest MHz.
	 */
	isp->isp_clock_frequency = (isp->isp_clock_frequency + 500000)/1000000;
	ISP_DEBUG(isp, SCSI_DEBUG, "isp clock frequency=%x MHz",
		isp->isp_clock_frequency);

	/*
	 * find scsi host id property
	 */
	id = ddi_getprop(DDI_DEV_T_ANY, dip, 0, "scsi-initiator-id", -1);
	if ((id != scsi_host_id) && (id >= 0) && (id < NTARGETS_WIDE)) {
		isp_i_log(isp, CE_NOTE, "initiator SCSI ID now %d", id);
		isp->isp_initiator_id = (uchar_t)id;
	} else {
		isp->isp_initiator_id = (uchar_t)scsi_host_id;
	}

	isp->isp_scsi_tag_age_limit =
		ddi_getprop(DDI_DEV_T_ANY, dip, 0, "scsi-tag-age-limit",
		    scsi_tag_age_limit);
	ISP_DEBUG(isp, SCSI_DEBUG, "isp scsi_tage_age_limit=%d, global=%d",
	    isp->isp_scsi_tag_age_limit, scsi_tag_age_limit);
	if (isp->isp_scsi_tag_age_limit != scsi_tag_age_limit) {
		isp_i_log(isp, CE_NOTE, "scsi-tag-age-limit=%d",
		    isp->isp_scsi_tag_age_limit);
	}

	isp->isp_scsi_reset_delay =
		ddi_getprop(DDI_DEV_T_ANY, dip, 0, "scsi-reset-delay",
		    scsi_reset_delay);
	ISP_DEBUG(isp, SCSI_DEBUG, "isp scsi_reset_delay=%d, global=%d",
	    isp->isp_scsi_reset_delay, scsi_reset_delay);
	if (isp->isp_scsi_reset_delay != scsi_reset_delay) {
		isp_i_log(isp, CE_NOTE, "scsi-reset-delay=%d",
		    isp->isp_scsi_reset_delay);
	}

	/*
	 * find the burstsize and reduce ours if necessary
	 * If no burst size found, select a reasonable default.
	 */
	tmp_dma_attr.dma_attr_burstsizes &=
		(ddi_dma_burstsizes(isp->isp_dmahandle) &
		isp_burst_sizes_limit);
	isp->isp_burst_size = tmp_dma_attr.dma_attr_burstsizes;


	ISP_DEBUG(isp, SCSI_DEBUG, "ispattr burstsize=%x",
		isp->isp_burst_size);

	if (isp->isp_burst_size == -1) {
		isp->isp_burst_size = DEFAULT_BURSTSIZE | BURST32 | BURST64;
		ISP_DEBUG(isp, SCSI_DEBUG, "Using default burst sizes, 0x%x",
		    isp->isp_burst_size);
	} else {
		isp->isp_burst_size &= BURSTSIZE_MASK;
		ISP_DEBUG(isp, SCSI_DEBUG, "burst sizes= 0x%x",
			isp->isp_burst_size);
	}

	/*
	 * set the threshold for the dma fifo
	 */
	if (isp->isp_burst_size & BURST128) {
		if (isp->isp_bus == ISP_SBUS) {
			isp_i_log(isp, CE_WARN, "Wrong burst size for SBus");
			goto fail;
		}
		isp->isp_conf1_fifo = ISP_PCI_CONF1_FIFO_128;
	} else if (isp->isp_burst_size & BURST64) {
		if (isp->isp_bus == ISP_SBUS) {
			isp->isp_conf1_fifo = ISP_SBUS_CONF1_FIFO_64;
		} else {
			isp->isp_conf1_fifo = ISP_PCI_CONF1_FIFO_64;
		}
	} else if (isp->isp_burst_size & BURST32) {
		if (isp->isp_bus == ISP_SBUS) {
			isp->isp_conf1_fifo = ISP_SBUS_CONF1_FIFO_32;
		} else {
			isp->isp_conf1_fifo = ISP_PCI_CONF1_FIFO_32;
		}
	} else if (isp->isp_burst_size & BURST16) {
		if (isp->isp_bus == ISP_SBUS) {
			isp->isp_conf1_fifo = ISP_SBUS_CONF1_FIFO_16;
		} else {
			isp->isp_conf1_fifo = ISP_PCI_CONF1_FIFO_16;
		}
	} else if (isp->isp_burst_size & BURST8) {
		if (isp->isp_bus == ISP_SBUS) {
			isp->isp_conf1_fifo = ISP_SBUS_CONF1_FIFO_8 |
				ISP_SBUS_CONF1_BURST8;
		} else {
			isp_i_log(isp, CE_WARN, "Wrong burst size for PCI");
			goto fail;
		}
	}

	if (isp->isp_conf1_fifo) {
		isp->isp_conf1_fifo |= ISP_BUS_CONF1_BURST_ENABLE;
	}

	ISP_DEBUG(isp, SCSI_DEBUG, "isp_conf1_fifo=0x%x", isp->isp_conf1_fifo);

	/*
	 * Attach this instance of the hba
	 */
	if (scsi_hba_attach_setup(dip, &tmp_dma_attr, tran, 0) !=
	    DDI_SUCCESS) {
		isp_i_log(isp, CE_WARN, "SCSI HBA attach failed");
		goto fail;
	}

	/*
	 * if scsi-options property exists, use it;
	 * otherwise use the global variable
	 */
	isp->isp_scsi_options =
		ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "scsi-options",
		    SCSI_OPTIONS_DR);
	ISP_DEBUG(isp, SCSI_DEBUG, "isp scsi_options=%x",
		isp->isp_scsi_options);

	/*
	 * if target<n>-scsi-options property exists, use it;
	 * otherwise use the isp_scsi_options
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		(void) sprintf(prop_str, prop_template, i);
		isp->isp_target_scsi_options[i] = ddi_prop_get_int(
			DDI_DEV_T_ANY, dip, 0, prop_str, -1);
		if (isp->isp_target_scsi_options[i] != -1) {
			isp_i_log(isp, CE_NOTE,
				"?target%x-scsi-options = 0x%x",
				i, isp->isp_target_scsi_options[i]);
			isp->isp_target_scsi_options_defined |= 1 << i;
		} else {
			isp->isp_target_scsi_options[i] =
				isp->isp_scsi_options;
		}

		ISP_DEBUG(isp, SCSI_DEBUG,
		    "isp target%d-scsi-options=%x, isp scsi_options=%x", i,
		    isp->isp_target_scsi_options[i], isp->isp_scsi_options);

		/*
		 * set default max luns per target
		 *
		 * Note: this max should really depend on the SCSI type
		 * of the target, i.e. SCSI-2 would only get 8 LUNs max,
		 * SCSI-1 one LUN, etc.  But, historyically, this adapter
		 * driver has always handled 32 as a default
		 */
		isp->isp_max_lun[i] = (ushort_t)ISP_NLUNS_PER_TARGET;
	}

	/*
	 * initialize the "need to send a marker" list to "none needed"
	 */
	isp->isp_marker_in = isp->isp_marker_out = 0;	 /* no markers */
	isp->isp_marker_free = ISP_MI_SIZE - 1;		 /* all empty */

	/*
	 * initialize the mbox sema
	 */
	sema_init(ISP_MBOX_SEMA(isp), 1, NULL, SEMA_DRIVER, isp->isp_iblock);

	/*
	 * initialize the wait queue mutex
	 */
	mutex_init(ISP_WAITQ_MUTEX(isp), NULL, MUTEX_DRIVER, isp->isp_iblock);

	/*
	 * initialize intr mutex/cv
	 */
	mutex_init(ISP_INTR_MUTEX(isp), NULL, MUTEX_DRIVER, isp->isp_iblock);
	cv_init(ISP_INTR_CV(isp), NULL, CV_DRIVER, isp->isp_iblock);
	isp->isp_in_intr = 0;

	/*
	 * mutexes to protect the isp request and response queue
	 */
	mutex_init(ISP_REQ_MUTEX(isp), NULL, MUTEX_DRIVER, isp->isp_iblock);
	mutex_init(ISP_RESP_MUTEX(isp), NULL, MUTEX_DRIVER, isp->isp_iblock);

	/*
	 * Initialize the conditional variable for quiescing the bus
	 */
	cv_init(ISP_CV(isp), NULL, CV_DRIVER, NULL);
	/*
	 * Initialize mutex for hotplug support.
	 */
	mutex_init(ISP_HOTPLUG_MUTEX(isp), NULL, MUTEX_DRIVER,
	    isp->isp_iblock);

	mutex_initted = 1;

	if (ddi_add_intr(dip, (uint_t)0,
	    (ddi_iblock_cookie_t *)&isp->isp_iblock,
	    (ddi_idevice_cookie_t *)0,
	    isp_intr,
	    (caddr_t)isp)) {
		isp_i_log(isp, CE_WARN, "Cannot add intr");
		goto fail;
	}
	interrupt_added = 1;

	/*
	 * kstat_intr support
	 */
	(void) sprintf(buf, "isp%d", instance);
	isp->isp_kstat = kstat_create("isp", instance, buf, "controller",
	    KSTAT_TYPE_INTR, 1, KSTAT_FLAG_PERSISTENT);
	if (isp->isp_kstat != NULL) {
		kstat_install(isp->isp_kstat);
	}

	/*
	 * link all isp's, for isp_intr_loop but also debugging
	 */
	rw_enter(&isp_global_rwlock, RW_WRITER);
	isp->isp_next = NULL;

	if (isp_head) {
		isp_tail->isp_next = isp;
		isp_tail = isp;
	} else {
		isp_head = isp_tail = isp;
	}
	rw_exit(&isp_global_rwlock);

	/*
	 * set up watchdog per all isp's
	 */
	mutex_enter(&isp_global_mutex);
	if (isp_timeout_id == 0) {
		ASSERT(timeout_initted == 0);
		isp_scsi_watchdog_tick =
		    ddi_getprop(DDI_DEV_T_ANY, dip, 0, "scsi-watchdog-tick",
		    scsi_watchdog_tick);
		if (isp_scsi_watchdog_tick != scsi_watchdog_tick) {
			isp_i_log(isp, CE_NOTE, "scsi-watchdog-tick=%d",
			    isp_scsi_watchdog_tick);
		}
		/*
		 * The isp_scsi_watchdog_tick should not be less than
		 * the pkt_time otherwise we will induce spurious timeouts.
		 */
		if (isp_scsi_watchdog_tick < ISP_DEFLT_WATCHDOG_SECS) {
			isp_scsi_watchdog_tick = ISP_DEFLT_WATCHDOG_SECS;
		}
		isp_tick =
		    drv_usectohz((clock_t)isp_scsi_watchdog_tick * 1000000);
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "isp_scsi_watchdog_tick=%d, isp_tick=%d",
		    isp_scsi_watchdog_tick, isp_tick);
		isp_timeout_id = timeout(isp_i_watch, NULL, isp_tick);
		timeout_initted = 1;
	}

	mutex_exit(&isp_global_mutex);

	ISP_MUTEX_ENTER(isp);

	/*
	 * create kmem cache for packets
	 */
	(void) sprintf(buf, "isp%d_cache", instance);
	isp->isp_kmem_cache = kmem_cache_create(buf,
		EXTCMDS_SIZE, 8, isp_kmem_cache_constructor,
		isp_kmem_cache_destructor, NULL, (void *)isp, NULL, 0);

	/*
	 * Download the ISP firmware that has been linked in
	 * We need the mutexes here to avoid assertion failures in
	 * the mbox cmds
	 */
	if (isp->isp_bus == ISP_SBUS) {
		rval = isp_i_download_fw(isp, isp_risc_code_addr,
			isp_sbus_risc_code, isp_sbus_risc_code_length);
	} else {
		rval = isp_i_download_fw(isp, isp_risc_code_addr,
			isp_1040_risc_code, isp_1040_risc_code_length);
	}
	if (rval) {
		ISP_MUTEX_EXIT(isp);
		goto fail;
	}

	/*
	 * Initialize the default Target Capabilites and Sync Rates
	 */
	isp_i_initcap(isp, 0, NTARGETS_WIDE - 1);

	/*
	 * reset isp and initialize capabilities
	 * Do NOT reset the bus since that will cause a reset delay
	 * which adds substantially to the boot time.
	 */
	/*
	 * if there is no obp, then when you just reboot
	 * the machine without power cycle, the disks still
	 * have the old parameters (e.g. wide) because no reset is sent
	 * on the bus.
	 */
	if (isp_pci_no_obp) {
		if (isp_i_reset_interface(isp, ISP_FORCE_RESET_BUS)) {
			ISP_MUTEX_EXIT(isp);
			goto fail;
		}
	} else {
		if (isp_i_reset_interface(isp, ISP_RESET_BUS_IF_BUSY)) {
			ISP_MUTEX_EXIT(isp);
			goto fail;
		}
	}
	ISP_MUTEX_EXIT(isp);

	ddi_report_dev(dip);

	isp_i_log(isp, CE_NOTE,
	    "?Firmware Version: v%d.%02d.%d, Customer: %d, Product: %d",
	    MSB(isp->isp_maj_min_rev), LSB(isp->isp_maj_min_rev),
	    isp->isp_subminor_rev,
	    MSB(isp->isp_cust_prod), LSB(isp->isp_cust_prod));

	return (DDI_SUCCESS);

fail:
	isp_i_log(isp, CE_WARN, "Unable to attach");
	if (isp->isp_kmem_cache) {
		kmem_cache_destroy(isp->isp_kmem_cache);
	}
	if (isp->isp_cmdarea) {
		if (bound_handle) {
			(void) ddi_dma_unbind_handle(isp->isp_dmahandle);
		}
		ddi_dma_mem_free(&isp->isp_dma_acc_handle);
	}
	mutex_enter(&isp_global_mutex);
	if (timeout_initted && (isp == isp_head) && (isp == isp_tail)) {
		timeout_id_t tid = isp_timeout_id;
		timeout_initted = 0;
		isp_timeout_id = 0;
		mutex_exit(&isp_global_mutex);
		(void) untimeout(tid);
	} else {
		mutex_exit(&isp_global_mutex);
	}
	if (interrupt_added) {
		ddi_remove_intr(dip, (uint_t)0, isp->isp_iblock);
	}

	/*
	 * kstat_intr support
	 */
	if (isp->isp_kstat) {
		kstat_delete(isp->isp_kstat);
	}

	if (mutex_initted) {
		mutex_destroy(ISP_WAITQ_MUTEX(isp));
		mutex_destroy(ISP_REQ_MUTEX(isp));
		mutex_destroy(ISP_RESP_MUTEX(isp));
		mutex_destroy(ISP_HOTPLUG_MUTEX(isp));
		sema_destroy(ISP_MBOX_SEMA(isp));
		cv_destroy(ISP_CV(isp));
		mutex_destroy(ISP_INTR_MUTEX(isp));
		cv_destroy(ISP_INTR_CV(isp));
	}
	if (isp->isp_dmahandle) {
		ddi_dma_free_handle(&isp->isp_dmahandle);
	}

	rw_enter(&isp_global_rwlock, RW_WRITER);
	for (l_isp = s_isp = isp_head; s_isp != NULL;
	    s_isp = s_isp->isp_next) {
		if (s_isp == isp) {
			if (s_isp == isp_head) {
				isp_head = isp->isp_next;
				if (isp_tail == isp) {
					isp_tail = NULL;
				}
			} else {
				if (isp_tail == isp) {
					isp_tail = l_isp;
				}
				l_isp->isp_next = isp->isp_next;
			}
			break;
		}
		l_isp = s_isp;
	}
	rw_exit(&isp_global_rwlock);

	/* Note: there are no failure paths before mutex_initted is set */
	if (mutex_initted) {
		(void) scsi_hba_detach(dip);
	}
	if (tran) {
		scsi_hba_tran_free(tran);
	}
	if (isp->isp_bus == ISP_PCI && isp->isp_pci_config_acc_handle) {
		pci_config_teardown(&isp->isp_pci_config_acc_handle);
	}
	if (isp->isp_biu_acc_handle) {
		ddi_regs_map_free(&isp->isp_biu_acc_handle);
	}
	if (isp->isp_mbox_acc_handle) {
		ddi_regs_map_free(&isp->isp_mbox_acc_handle);
	}
	if (isp->isp_sxp_acc_handle) {
		ddi_regs_map_free(&isp->isp_sxp_acc_handle);
	}
	if (isp->isp_risc_acc_handle) {
		ddi_regs_map_free(&isp->isp_risc_acc_handle);
	}
	ddi_soft_state_free(isp_state, instance);
	return (DDI_FAILURE);
}


/*ARGSUSED*/
static int
isp_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct isp		*isp, *nisp;
	scsi_hba_tran_t		*tran;
	int			slot;

	switch (cmd) {
	case DDI_DETACH:
		return (isp_dr_detach(dip));

	case DDI_SUSPEND:
		tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		if (!tran) {
			return (DDI_FAILURE);
		}
		isp = TRAN2ISP(tran);
		if (!isp) {
			return (DDI_FAILURE);
		}

		/*
		 * Prevent any new I/O from occuring and then check for any
		 * outstanding I/O.  We could put in a delay, but since all
		 * target drivers should have been suspended before we were
		 * called there should not be any pending commands.
		 */
		ISP_MUTEX_ENTER(isp);
		isp->isp_suspended = 1;
		for (slot = 0; slot < ISP_MAX_SLOTS; slot++) {
			if (isp->isp_slots[slot].slot_cmd) {
				isp->isp_suspended = 0;
				ISP_MUTEX_EXIT(isp);
				return (DDI_FAILURE);
			}
		}
		ISP_MUTEX_EXIT(isp);

		mutex_enter(ISP_WAITQ_MUTEX(isp));
		if (isp->isp_waitq_timeout != 0) {
			timeout_id_t tid = isp->isp_waitq_timeout;
			isp->isp_waitq_timeout = 0;
			mutex_exit(ISP_WAITQ_MUTEX(isp));
			(void) untimeout(tid);
		} else {
			mutex_exit(ISP_WAITQ_MUTEX(isp));
		}
		rw_enter(&isp_global_rwlock, RW_WRITER);
		for (nisp = isp_head; nisp; nisp = nisp->isp_next) {
			if (!nisp->isp_suspended) {
				rw_exit(&isp_global_rwlock);
				return (DDI_SUCCESS);
			}
		}
		mutex_enter(&isp_global_mutex);
		rw_exit(&isp_global_rwlock);
		if (isp_timeout_id != 0) {
			timeout_id_t tid = isp_timeout_id;
			isp_timeout_id = 0;
			timeout_initted = 0;
			mutex_exit(&isp_global_mutex);
			(void) untimeout(tid);
		} else {
			mutex_exit(&isp_global_mutex);
		}
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}


static int
isp_dr_detach(dev_info_t *dip)
{
	struct isp		*isp, *nisp, *tisp;
	scsi_hba_tran_t		*tran;
	int			instance = ddi_get_instance(dip);


	if (!(tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip))) {
		return (DDI_FAILURE);
	}

	isp = TRAN2ISP(tran);
	if (!isp) {
		return (DDI_FAILURE);
	}

	/*
	 * deallocate reset notify callback list
	 */
	scsi_hba_reset_notify_tear_down(isp->isp_reset_notify_listf);

	/*
	 * Force interrupts OFF and remove handler
	 */
	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_icr,
	    ISP_BUS_ICR_DISABLE_ALL_INTS);
	ddi_remove_intr(dip, (uint_t)0, isp->isp_iblock);

	/*
	 * kstat_intr support
	 */
	if (isp->isp_kstat) {
		kstat_delete(isp->isp_kstat);
	}

	/*
	 * Remove device instance from the global linked list
	 */
	rw_enter(&isp_global_rwlock, RW_WRITER);
	for (nisp = tisp = isp_head; nisp;
	    tisp = nisp, nisp = nisp->isp_next) {
		if (nisp == isp)
			break;
	}
	ASSERT(nisp);

	if (nisp == isp_head) {
		isp_head = tisp = isp->isp_next;
	} else {
		tisp->isp_next = isp->isp_next;
	}
	if (nisp == isp_tail) {
		isp_tail = tisp;
	}
	rw_exit(&isp_global_rwlock);

	/*
	 * If active, CANCEL watch thread.
	 */
	mutex_enter(&isp_global_mutex);
	if (timeout_initted && (isp_head == NULL)) {
		timeout_id_t tid = isp_timeout_id;
		timeout_initted = 0;
		isp_timeout_id = 0;
		mutex_exit(&isp_global_mutex);
		(void) untimeout(tid);
	} else {
		mutex_exit(&isp_global_mutex);
	}

	/*
	 * Release miscellaneous device resources
	 */
	if (isp->isp_kmem_cache) {
		kmem_cache_destroy(isp->isp_kmem_cache);
	}

	if (isp->isp_cmdarea) {
		(void) ddi_dma_unbind_handle(isp->isp_dmahandle);
		ddi_dma_mem_free(&isp->isp_dma_acc_handle);
	}

	if (isp->isp_dmahandle)
		ddi_dma_free_handle(&isp->isp_dmahandle);

	if (isp->isp_bus == ISP_PCI && isp->isp_pci_config_acc_handle) {
		pci_config_teardown(&isp->isp_pci_config_acc_handle);
	}
	if (isp->isp_biu_acc_handle) {
		ddi_regs_map_free(&isp->isp_biu_acc_handle);
	}
	if (isp->isp_mbox_acc_handle) {
		ddi_regs_map_free(&isp->isp_mbox_acc_handle);
	}
	if (isp->isp_sxp_acc_handle) {
		ddi_regs_map_free(&isp->isp_sxp_acc_handle);
	}
	if (isp->isp_risc_acc_handle) {
		ddi_regs_map_free(&isp->isp_risc_acc_handle);
	}

	/*
	 * Remove device MT locks
	 */
	mutex_destroy(ISP_WAITQ_MUTEX(isp));
	mutex_destroy(ISP_REQ_MUTEX(isp));
	mutex_destroy(ISP_RESP_MUTEX(isp));
	mutex_destroy(ISP_HOTPLUG_MUTEX(isp));
	sema_destroy(ISP_MBOX_SEMA(isp));
	cv_destroy(ISP_CV(isp));
	mutex_destroy(ISP_INTR_MUTEX(isp));
	cv_destroy(ISP_CV(isp));

	/*
	 * Remove properties created during attach()
	 */
	ddi_prop_remove_all(dip);

	/*
	 * Delete the DMA limits, transport vectors and remove the device
	 * links to the scsi_transport layer.
	 * 	-- ddi_set_driver_private(dip, NULL)
	 */
	(void) scsi_hba_detach(dip);

	/*
	 * Free the scsi_transport structure for this device.
	 */
	scsi_hba_tran_free(tran);

	isp->isp_dip = (dev_info_t *)NULL;
	isp->isp_tran = (scsi_hba_tran_t *)NULL;

	ddi_soft_state_free(isp_state, instance);

	return (DDI_SUCCESS);
}

/*
 * Hotplug functions for the driver.
 */
static int
isp_scsi_quiesce(dev_info_t *dip)
{
	struct isp *isp;
	scsi_hba_tran_t *tran;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);

	if ((tran == NULL) || ((isp = TRAN2ISP(tran)) == NULL)) {
		return (-1);
	}

	return (isp_quiesce_bus(isp));
}

static int
isp_scsi_unquiesce(dev_info_t *dip)
{
	struct isp *isp;
	scsi_hba_tran_t *tran;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);

	if ((tran == NULL) || ((isp = TRAN2ISP(tran)) == NULL)) {
		return (-1);
	}

	return (isp_unquiesce_bus(isp));
}

static int
isp_quiesce_bus(struct isp *isp)
{
	int result;
	int outstanding, oldoutstanding = 0;
	uint_t bus_state;
	clock_t delay;
	dev_info_t *self = (dev_info_t *)isp->isp_dip;

	mutex_enter(ISP_HOTPLUG_MUTEX(isp));
	if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS &&
		(bus_state == BUS_QUIESCED)) {
		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		return (EALREADY);
	}

	if ((isp->isp_softstate & ISP_SS_DRAINING) == 0) {
		isp->isp_softstate |= ISP_SS_DRAINING;

		/*
		 * For each LUN send the stop queue mailbox command down.
		 */

		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		if (isp_mailbox_all(isp, 2, 3, ISP_MBOX_CMD_STOP_QUEUE, 0, 0,
		    0, 0, 0, NTARGETS_WIDE) != 0) {
			/* the stop-queue failed: start it back up again */
			(void) isp_mailbox_all(isp, 2, 3,
			    ISP_MBOX_CMD_START_QUEUE, 0, 0, 0, 0, 0,
			    NTARGETS_WIDE);
			mutex_enter(ISP_HOTPLUG_MUTEX(isp));
			isp->isp_softstate &= ~ISP_SS_DRAINING;
			isp->isp_softstate |= ISP_SS_DRAIN_ERROR;
			cv_broadcast(ISP_CV(isp));
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (-1);
		}

		/* Save the outstanding command count */
		if ((oldoutstanding = outstanding =
			isp_i_outstanding(isp)) == 0) {
			/* That was fast */
			mutex_enter(ISP_HOTPLUG_MUTEX(isp));
			isp->isp_softstate &= ~ISP_SS_DRAINING;
			isp->isp_softstate |= ISP_SS_QUIESCED;
			(void) ndi_set_bus_state(self, BUS_QUIESCED);
			cv_broadcast(ISP_CV(isp));
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (0);
		}
		mutex_enter(ISP_HOTPLUG_MUTEX(isp));
	}

	/*
	 * Here's how this works.  Pay attention as it's a bit complicated.
	 *
	 * The first thread (isp->isp_hotplug_waiting == 0) in goes into the
	 * poll loop and runs the show. Any additional threads
	 * (isp->isp_hotplug_waiting !=0) go into a deep sleep.  Every
	 * ISP_BUS_DRAIN_TIME the polling thread wakes up and checks if the
	 * drain has completed.  If it has, it cleans up, clears the
	 * ISP_SS_DRAINING flag, wakes up all the other threads, and returns
	 * success.
	 *
	 * If the poll thread wakes up due to an interrupt, it wakes up the
	 * next thread and returns EINTR.  The next thread takes over the
	 * polling duties.  If there is no next thread, it aborts the operation
	 * and re-starts the ISP chip.  */

	if (isp->isp_hotplug_waiting++)
		result = cv_wait_sig(ISP_CV(isp), ISP_HOTPLUG_MUTEX(isp));

	if (isp->isp_softstate & ISP_SS_DRAINING) {
		(void) drv_getparm(LBOLT, &delay);
		delay += ISP_BUS_DRAIN_TIME * drv_usectohz(1000000);
		result = cv_timedwait_sig(ISP_CV(isp),
			ISP_HOTPLUG_MUTEX(isp), delay);
		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		while ((result) == -1 &&
			(outstanding = isp_i_outstanding(isp)) != 0) {
			mutex_enter(ISP_HOTPLUG_MUTEX(isp));
			/* Timeout -- re-poll */
			if (outstanding == oldoutstanding &&
				oldoutstanding != 0) {
				/*
				 * If nothing at all changed for a whole
				 * ISP_BUS_DRAIN_TIME, something must be
				 * hosed so abort everyone
				 */
				mutex_exit(ISP_HOTPLUG_MUTEX(isp));
				(void) isp_mailbox_all(isp, 2, 3,
				    ISP_MBOX_CMD_START_QUEUE, 0, 0, 0, 0, 0,
					NTARGETS_WIDE);
				mutex_enter(ISP_HOTPLUG_MUTEX(isp));
				isp->isp_softstate &= ~ISP_SS_DRAINING;
				isp->isp_softstate |= ISP_SS_DRAIN_ERROR;
				cv_broadcast(ISP_CV(isp));
				isp->isp_hotplug_waiting --;
				mutex_exit(ISP_HOTPLUG_MUTEX(isp));
				(void) isp_i_check_waitQ(isp);
				return (EIO);
			}
			oldoutstanding = outstanding;
			(void) drv_getparm(LBOLT, &delay);
			delay += ISP_BUS_DRAIN_TIME * drv_usectohz(1000000);
			result = cv_timedwait_sig(ISP_CV(isp),
				ISP_HOTPLUG_MUTEX(isp), delay);
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		}
		mutex_enter(ISP_HOTPLUG_MUTEX(isp));
		if (result && outstanding == 0) {
			/* Done.  Wake up the others */
			isp->isp_hotplug_waiting --;
			isp->isp_softstate &= ~ISP_SS_DRAINING;
			isp->isp_softstate |= ISP_SS_QUIESCED;
			(void) ndi_set_bus_state(self, BUS_QUIESCED);
			cv_broadcast(ISP_CV(isp));
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (0);
		}
	}


	if (--isp->isp_hotplug_waiting == 0) {
		/* Last one out cleans up. */
		if (result == 0) {
			/*
			 * quiesce has been interrupted and no waiters.
			 * Restart the queues after reseting ISP_SS_DRAINING;
			 */
			ISP_DEBUG(isp, SCSI_DEBUG,
				"isp_quiesce: abort QUIESCE\n");
			isp->isp_softstate &= ~ISP_SS_DRAINING;
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			(void) isp_mailbox_all(isp, 2, 3,
				ISP_MBOX_CMD_START_QUEUE, 0, 0, 0, 0, 0,
				NTARGETS_WIDE);
			(void) isp_i_check_waitQ(isp);
			return (EINTR);
		} else if (isp->isp_softstate & ISP_SS_DRAIN_ERROR) {
			isp->isp_softstate &= ~ISP_SS_DRAIN_ERROR;
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (EINTR);
		}
	} else {
		if (result == 0) {
			/* Interrupted but others waiting, wake a replacement */
			cv_signal(ISP_CV(isp));
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (EINTR);
		} else if (isp->isp_softstate & ISP_SS_DRAIN_ERROR) {
			mutex_exit(ISP_HOTPLUG_MUTEX(isp));
			return (EIO);
		}
	}
	mutex_exit(ISP_HOTPLUG_MUTEX(isp));
	return (0);
}

static int
isp_unquiesce_bus(struct isp *isp)
{
	dev_info_t *self = (dev_info_t *)isp->isp_dip;
	uint_t bus_state;

	mutex_enter(ISP_HOTPLUG_MUTEX(isp));
	if (isp->isp_softstate & ISP_SS_DRAINING) {
		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		/* EBUSY would be more appropriate */
		return (EIO);
	}
	if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS &&
		bus_state == BUS_ACTIVE) {
		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		return (EALREADY);
	}
	mutex_exit(ISP_HOTPLUG_MUTEX(isp));
	(void) isp_mailbox_all(isp, 2, 3, ISP_MBOX_CMD_START_QUEUE, 0,
		0, 0, 0, 0, NTARGETS_WIDE);
	mutex_enter(ISP_HOTPLUG_MUTEX(isp));
	isp->isp_softstate &= ~ISP_SS_QUIESCED;
	(void) ndi_set_bus_state(self, BUS_ACTIVE);
	mutex_exit(ISP_HOTPLUG_MUTEX(isp));
	(void) isp_i_check_waitQ(isp);
	ISP_DEBUG(isp, SCSI_DEBUG, "isp_unquiesce: bus has been UNQUIESCED\n");
	return (0);
}

/*
 * Get the number of executing commands from the ISP.
 */
static int
isp_i_outstanding(struct isp *isp)
{
	struct isp_mbox_cmd mbox_cmd;
	int cmd_count;

	ISP_MUTEX_ENTER(isp);
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 1,
		ISP_MBOX_CMD_GET_ISP_STAT, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd))
		cmd_count = -1;
	else
		cmd_count = mbox_cmd.mbox_in[2];
	ISP_MUTEX_EXIT(isp);
	return (cmd_count);
}


/*
 * send mailbox cmd to all targets requested
 *
 * (Note: always called with NTARGETS_WIDE targets)
 */
/*ARGSUSED*/
static int
isp_mailbox_all(struct isp *isp, uchar_t n_mbox_out, uchar_t n_mbox_in,
    ushort_t reg0, ushort_t reg1, ushort_t reg2, ushort_t reg3,
    ushort_t reg4, ushort_t reg5, ushort_t targets)
{
	ushort_t i, j;
	struct isp_mbox_cmd mbox_cmd;


	ISP_MUTEX_ENTER(isp);

	for (i = 0; i < targets; i++) {
		ushort_t	luns = isp->isp_max_lun[i];

		for (j = 0; j < luns; j++) {
			isp_i_mbox_cmd_init(isp, &mbox_cmd, n_mbox_out,
			    n_mbox_in, reg0, (i << 8) | j,
			    reg2, reg3, reg4, reg5);
			if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
				ISP_MUTEX_EXIT(isp);
				return (-1);
			}
		}
	}

	ISP_MUTEX_EXIT(isp);

	return (0);
}


/*
 * Function name : isp_i_download_fw ()
 *
 * Return Values : 0  on success.
 *		   -1 on error.
 *
 * Description	 : Uses the request and response queue iopb memory for dma.
 *		   Verifies that fw fits in iopb memory.
 *		   Verifies fw checksum.
 *		   Copies firmware to iopb memory.
 *		   Sends mbox cmd to ISP to (down) Load RAM.
 *		   After command is done, resets ISP which starts it
 *			executing from new f/w.
 *
 * Context	 : Can be called ONLY from user context.
 *		 : NOT MT-safe.
 *		 : Driver must be in a quiescent state.
 */
static int
isp_i_download_fw(struct isp *isp,
    ushort_t risc_addr, ushort_t *fw_addrp, ushort_t fw_len)
{
	int rval			= -1;
	int fw_len_bytes		= (int)fw_len *
					    sizeof (unsigned short);
	ushort_t checksum		= 0;
	int found			= 0;
	char *string			= " Firmware  Version ";
	int string_len = strlen(string);
	char *startp;
	char buf[10];
	int length;
	int major_rev, minor_rev;
	struct isp_mbox_cmd mbox_cmd;
	ushort_t i;


	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_download_fw_start: risc = 0x%x fw = 0x%x, fw_len =0x%x",
	    risc_addr, fw_addrp, fw_len);

	/*
	 * if download is not necessary just return good status
	 */
	if (isp_download_fw == ISP_DOWNLOAD_FW_OFF) {
		goto done;
	}

	/*
	 * Since we use the request and response queue iopb
	 * we check to see if f/w will fit in this memory.
	 * This iopb memory presently is 32k and the f/w is about
	 * 13k but check the headers for definite values.
	 */
	if (fw_len_bytes > ISP_QUEUE_SIZE) {
		isp_i_log(isp, CE_WARN,
		    "Firmware (0x%x) should be < 0x%x bytes",
		    fw_len_bytes, ISP_QUEUE_SIZE);
		goto fail;
	}

	/*
	 * verify checksum equals zero
	 */
	for (i = 0; i < fw_len; i++) {
		checksum += fw_addrp[i];
	}
	if (checksum != 0) {
		isp_i_log(isp, CE_WARN, "Firmware checksum incorrect");
		goto fail;
	}

	/*
	 * get new firmware version numbers
	 *
	 * XXX: this searches *THE WHOLE FIRMWARE* for the rev string!
	 */
	startp = (char *)fw_addrp;
	length = fw_len_bytes;
	while (length > string_len) {
		if (strncmp(startp, string, string_len) == 0) {
			found = 1;
			break;
		}
		startp++;
		length--;
	}

	if (!found) {
		goto done;
	}

	startp += strlen(string);
	(void) strncpy(buf, startp, 5);
	buf[2] = buf[5] = '\0';
	startp = buf;
	major_rev = stoi(&startp);
	startp++;
	minor_rev = stoi(&startp);

	ISP_DEBUG(isp, SCSI_DEBUG, "New f/w: major = %d minor = %d",
	    major_rev, minor_rev);

	/*
	 * reset and initialize isp chip
	 */
	if (isp_i_reset_init_chip(isp) != 0) {
		goto fail;
	}

	/*
	 * if we want to download only if we have newer version, we
	 * assume that there is already some firmware in the RAM that
	 * chip can use.
	 *
	 * in case we want to always download, we don't depend on having
	 * anything in the RAM and start from ROM firmware.
	 *
	 */
	if (isp_download_fw == ISP_DOWNLOAD_FW_IF_NEWER) {
		/*
		 * start ISP Ram firmware up
		 */
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 6,
		    ISP_MBOX_CMD_START_FW, risc_addr,
		    0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto download;
		}

		/*
		 * set clock rate
		 */
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
		    ISP_MBOX_CMD_SET_CLOCK_RATE, isp->isp_clock_frequency,
		    0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto download;
		}

		/*
		 * get ISP Ram firmware version numbers
		 */
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 6,
		    ISP_MBOX_CMD_ABOUT_PROM, 0, 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto download;
		}

		isp_i_log(isp, CE_NOTE, "?On-board Firmware Version: v%d.%02d",
		    mbox_cmd.mbox_in[1], mbox_cmd.mbox_in[2]);

		if (major_rev < (int)mbox_cmd.mbox_in[1] ||
		    minor_rev <= (int)mbox_cmd.mbox_in[2]) {
			goto done;
		}

download:
		/*
		 * Send mailbox cmd to stop ISP from executing the Ram
		 * firmware and drop to executing the ROM firmware.
		 */
		ISP_DEBUG(isp, SCSI_DEBUG, "Stop Firmware");
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 6, ISP_MBOX_CMD_STOP_FW,
		    0, 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			isp_i_log(isp, CE_WARN, "Stop firmware failed");
			goto fail;
		}
	}

	/*
	 * copy firmware to iopb memory that was allocated for queues.
	 * XXX this assert is not quite right, area is a little smaller
	 */
	ASSERT(fw_len_bytes <= ISP_QUEUE_SIZE);
	ISP_COPY_OUT_DMA_16(isp, fw_addrp, isp->isp_request_base, fw_len);

	/*
	 * sync memory
	 */
	(void) ddi_dma_sync(isp->isp_dmahandle, (off_t)0, (size_t)fw_len_bytes,
	    DDI_DMA_SYNC_FORDEV);

	ISP_DEBUG(isp, SCSI_DEBUG, "Load Ram");
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 5, 1, ISP_MBOX_CMD_LOAD_RAM,
	    risc_addr, MSW(isp->isp_request_dvma),
	    LSW(isp->isp_request_dvma), fw_len, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		isp_i_log(isp, CE_WARN, "Load ram failed");
		isp_i_print_state(CE_NOTE, isp);
		goto fail;
	}

	/*
	 * reset the ISP chip so it starts with the new firmware
	 */
	ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_RESET);
	drv_usecwait(ISP_CHIP_RESET_BUSY_WAIT_TIME);
	ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_RELEASE);

	/*
	 * Start ISP firmware up.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 6,
	    ISP_MBOX_CMD_START_FW, risc_addr,
	    0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	/*
	 * set clock rate
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
	    ISP_MBOX_CMD_SET_CLOCK_RATE, isp->isp_clock_frequency,
	    0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	/*
	 * get ISP Ram firmware version numbers
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 6,
	    ISP_MBOX_CMD_ABOUT_PROM, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	isp->isp_maj_min_rev = mbox_cmd.mbox_in[1];
	isp->isp_subminor_rev = mbox_cmd.mbox_in[2];
	isp->isp_cust_prod = mbox_cmd.mbox_in[3];

	ISP_DEBUG(isp, SCSI_DEBUG,
	    "Downloaded f/w: major=%d minor=%d subminor=%d",
	    MSB(isp->isp_maj_min_rev), LSB(isp->isp_maj_min_rev),
	    isp->isp_subminor_rev);

done:
	rval = 0;

fail:
	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_download_fw: 0x%x 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox0),
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox1),
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox2),
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox3),
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox4),
	    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox5));

	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_download_fw_end: rval = %d", rval);

	bzero((caddr_t)isp->isp_request_base, ISP_QUEUE_SIZE);

	return (rval);
}

/*
 * Function name : isp_i_initcap
 *
 * Return Values : NONE
 * Description	 : Initializes the default target capabilites and
 *		   Sync Rates.
 *
 * Context	 : Called from the user thread through attach.
 *
 */
static void
isp_i_initcap(struct isp *isp, int start_tgt, int end_tgt)
{
	int i;
	ushort_t cap, synch;

	for (i = start_tgt; i <= end_tgt; i++) {
		cap = 0;
		synch = 0;
		if (isp->isp_target_scsi_options[i] & SCSI_OPTIONS_DR) {
			cap |= ISP_CAP_DISCONNECT;
		}
		if (isp->isp_target_scsi_options[i] & SCSI_OPTIONS_PARITY) {
			cap |= ISP_CAP_PARITY;
		}
		if (isp->isp_target_scsi_options[i] & SCSI_OPTIONS_SYNC) {
			cap |= ISP_CAP_SYNC;
			if (isp->isp_target_scsi_options[i] &
				SCSI_OPTIONS_FAST20) {
				synch = ISP_20M_SYNC_PARAMS;
			} else if (isp->isp_target_scsi_options[i] &
			    SCSI_OPTIONS_FAST) {
				synch = ISP_10M_SYNC_PARAMS;
			} else {
				synch = ISP_5M_SYNC_PARAMS;
			}
		}
		isp->isp_cap[i] = cap;
		isp->isp_synch[i] = synch;
	}
	ISP_DEBUG(isp, SCSI_DEBUG, "tgt(s) %d-%d: default cap=0x%x, sync=0x%x",
	    start_tgt, end_tgt, cap, synch);
}


/*
 * Function name : isp_i_commoncap
 *
 * Return Values : TRUE - capability exists  or could be changed
 *		   FALSE - capability does not exist or could not be changed
 *		   value - current value of capability
 * Description	 : sets a capability for a target or all targets
 *		   or returns the current value of a capability
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
/*
 * SCSA host adapter get/set capability routines.
 * isp_scsi_getcap and isp_scsi_setcap are wrappers for isp_i_commoncap.
 */
static int
isp_i_commoncap(struct scsi_address *ap, char *cap,
    int val, int tgtonly, int doset)
{
	struct isp *isp = ADDR2ISP(ap);
	uchar_t tgt = ap->a_target;
	int cidx;
	int i;
	int rval = FALSE;
	int update_isp = 0;
	ushort_t	start, end;

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	if (cap == (char *)0) {
		ISP_DEBUG(isp, SCSI_DEBUG, "isp_i_commoncap: invalid arg");
		return (rval);
	}

	cidx = scsi_hba_lookup_capstr(cap);
	if (cidx == -1) {
		return (UNDEFINED);
	}

	ISP_MUTEX_ENTER(isp);

	/*
	 * Process setcap request.
	 */
	if (doset) {
		/*
		 * At present, we can only set binary (0/1) values
		 */
		switch (cidx) {
		case SCSI_CAP_DMA_MAX:
		case SCSI_CAP_MSG_OUT:
		case SCSI_CAP_PARITY:
		case SCSI_CAP_UNTAGGED_QING:
		case SCSI_CAP_LINKED_CMDS:
		case SCSI_CAP_RESET_NOTIFICATION:
			/*
			 * None of these are settable via
			 * the capability interface.
			 */
			break;
		case SCSI_CAP_DISCONNECT:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_DR) == 0) {
				break;
			} else if (tgtonly) {
				if (val) {
					isp->isp_cap[tgt] |=
					    ISP_CAP_DISCONNECT;
				} else {
					isp->isp_cap[tgt] &=
					    ~ISP_CAP_DISCONNECT;
				}
			} else {
				for (i = 0; i < NTARGETS_WIDE; i++) {
					if (val) {
						isp->isp_cap[i] |=
						    ISP_CAP_DISCONNECT;
					} else {
						isp->isp_cap[i] &=
						    ~ISP_CAP_DISCONNECT;
					}
				}
			}
			rval = TRUE;
			update_isp++;
			break;
		case SCSI_CAP_SYNCHRONOUS:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_SYNC) == 0) {
				break;
			} else if (tgtonly) {
				if (val) {
					isp->isp_cap[tgt] |= ISP_CAP_SYNC;
				} else {
					isp->isp_cap[tgt] &= ~ISP_CAP_SYNC;
				}
			} else {
				for (i = 0; i < NTARGETS_WIDE; i++) {
					if (val) {
						isp->isp_cap[i] |=
						    ISP_CAP_SYNC;
					} else {
						isp->isp_cap[i] &=
						    ~ISP_CAP_SYNC;
					}
				}
			}
			rval = TRUE;
			update_isp++;
			break;
		case SCSI_CAP_TAGGED_QING:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_DR) == 0 ||
			    (isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_TAG) == 0) {
				break;
			} else if (tgtonly) {
				if (val) {
					isp->isp_cap[tgt] |= ISP_CAP_TAG;
				} else {
					isp->isp_cap[tgt] &= ~ISP_CAP_TAG;
				}
			} else {
				for (i = 0; i < NTARGETS_WIDE; i++) {
					if (val) {
						isp->isp_cap[i] |= ISP_CAP_TAG;
					} else {
						isp->isp_cap[i] &= ~ISP_CAP_TAG;
					}
				}
			}
			rval = TRUE;
			update_isp++;
			break;
		case SCSI_CAP_WIDE_XFER:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_WIDE) == 0) {
				break;
			} else if (tgtonly) {
				if (val) {
					isp->isp_cap[tgt] |= ISP_CAP_WIDE;
				} else {
					isp->isp_cap[tgt] &= ~ISP_CAP_WIDE;
				}
			} else {
				for (i = 0; i < NTARGETS_WIDE; i++) {
					if (val) {
						isp->isp_cap[i] |=
						    ISP_CAP_WIDE;
					} else {
						isp->isp_cap[i] &=
						    ~ISP_CAP_WIDE;
					}
				}
			}
			rval = TRUE;
			update_isp++;
			break;
		case SCSI_CAP_INITIATOR_ID:
			if (val < NTARGETS_WIDE) {
				struct isp_mbox_cmd mbox_cmd;

				isp->isp_initiator_id = (ushort_t)val;

				/*
				 * set Initiator SCSI ID
				 */
				isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
				    ISP_MBOX_CMD_SET_SCSI_ID,
				    isp->isp_initiator_id,
				    0, 0, 0, 0);
				if (isp_i_mbox_cmd_start(isp, &mbox_cmd) == 0) {
					rval = TRUE;
				}
			}
			break;
		case SCSI_CAP_ARQ:
			if (tgtonly) {
				if (val) {
					isp->isp_cap[tgt] |= ISP_CAP_AUTOSENSE;
				} else {
					isp->isp_cap[tgt] &= ~ISP_CAP_AUTOSENSE;
				}
			} else {
				for (i = 0; i < NTARGETS_WIDE; i++) {
					if (val) {
						isp->isp_cap[i] |=
						    ISP_CAP_AUTOSENSE;
					} else {
						isp->isp_cap[i] &=
						    ~ISP_CAP_AUTOSENSE;
					}
				}
			}
			rval = TRUE;
			update_isp++;
			break;

		case SCSI_CAP_QFULL_RETRIES:
			if (tgtonly) {
				start = end = tgt;
			} else {
				start = 0;
				end = NTARGETS_WIDE;
			}
			rval = isp_i_handle_qfull_cap(isp, start,
				end,
				val, ISP_SET_QFULL_CAP,
				SCSI_CAP_QFULL_RETRIES);
			break;
		case SCSI_CAP_QFULL_RETRY_INTERVAL:
			if (tgtonly) {
				start = end = (ushort_t)tgt;
			} else {
				start = 0;
				end = NTARGETS_WIDE;
			}
			rval = isp_i_handle_qfull_cap(isp, start,
				end,
				val, ISP_SET_QFULL_CAP,
				SCSI_CAP_QFULL_RETRY_INTERVAL);
			break;

		default:
			ISP_DEBUG(isp, SCSI_DEBUG,
			    "isp_i_setcap: unsupported cap \"%s\" (%d)",
			    cap, cidx);
			rval = UNDEFINED;
			break;
		}

		ISP_DEBUG(isp, SCSI_DEBUG,
		"set cap: cap=%s,val=0x%x,tgtonly=0x%x,doset=0x%x,rval=%d\n",
		    cap, val, tgtonly, doset, rval);

		/*
		 * now update the isp, if necessary
		 */
		if ((rval == TRUE) && update_isp) {
			int start_tgt, end_tgt;

			if (tgtonly) {
				start_tgt = end_tgt = tgt;
				isp->isp_prop_update |= 1 << tgt;
			} else {
				start_tgt = 0;
				end_tgt = NTARGETS_WIDE;
				isp->isp_prop_update = 0xffff;
			}
			if (isp_i_updatecap(isp, start_tgt, end_tgt)) {
				/*
				 * if we can't update the capabilities
				 * in the isp, we are hosed
				 */
				isp->isp_shutdown = 1;
				rval = FALSE;
			}
		}

	/*
	 * Process getcap request.
	 */
	} else {
		switch (cidx) {
		case SCSI_CAP_DMA_MAX:
			rval = (int)dma_ispattr.dma_attr_maxxfer;
			break;
		case SCSI_CAP_MSG_OUT:
			rval = TRUE;
			break;
		case SCSI_CAP_DISCONNECT:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_DR) == 0) {
				break;
			} else if (tgtonly &&
			    (isp->isp_cap[tgt] & ISP_CAP_DISCONNECT) == 0) {
				break;
			}
			rval = TRUE;
			break;
		case SCSI_CAP_SYNCHRONOUS:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_SYNC) == 0) {
				break;
			} else if (tgtonly &&
			    (isp->isp_cap[tgt] & ISP_CAP_SYNC) == 0) {
				break;
			}
			rval = TRUE;
			break;
		case SCSI_CAP_WIDE_XFER:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_WIDE) == 0) {
				break;
			} else if (tgtonly &&
			    (isp->isp_cap[tgt] & ISP_CAP_WIDE) == 0) {
				break;
			}
			rval = TRUE;
			break;
		case SCSI_CAP_TAGGED_QING:
			if ((isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_DR) == 0 ||
			    (isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_TAG) == 0) {
				break;
			} else if (tgtonly &&
			    (isp->isp_cap[tgt] & ISP_CAP_TAG) == 0) {
				break;
			}
			rval = TRUE;
			break;
		case SCSI_CAP_UNTAGGED_QING:
			rval = TRUE;
			break;
		case SCSI_CAP_PARITY:
			if (isp->isp_target_scsi_options[tgt] &
				SCSI_OPTIONS_PARITY) {
				rval = TRUE;
			}
			break;
		case SCSI_CAP_INITIATOR_ID:
			rval = isp->isp_initiator_id;
			break;
		case SCSI_CAP_ARQ:
			if (isp->isp_cap[tgt] & ISP_CAP_AUTOSENSE) {
				rval = TRUE;
			}
			break;
		case SCSI_CAP_LINKED_CMDS:
			break;
		case SCSI_CAP_RESET_NOTIFICATION:
			rval = TRUE;
			break;
		case SCSI_CAP_QFULL_RETRIES:
			rval = isp_i_handle_qfull_cap(isp, tgt,
				tgt,
				0, ISP_GET_QFULL_CAP,
				SCSI_CAP_QFULL_RETRIES);
			break;
		case SCSI_CAP_QFULL_RETRY_INTERVAL:
			rval = isp_i_handle_qfull_cap(isp, tgt,
				tgt,
				0, ISP_GET_QFULL_CAP,
				SCSI_CAP_QFULL_RETRY_INTERVAL);
			break;
		default:
			ISP_DEBUG(isp, SCSI_DEBUG,
			    "isp_scsi_getcap: unsupported cap \"%s\" (%d)",
			    cap, cidx);
			rval = UNDEFINED;
			break;
		}
		ISP_DEBUG2(isp, SCSI_DEBUG,
		"get cap: cap=%s,val=0x%x,tgtonly=0x%x,doset=0x%x,rval=%d\n",
		    cap, val, tgtonly, doset, rval);
	}
	ISP_MUTEX_EXIT(isp);

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	return (rval);
}

/*
 * Function name : isp_scsi_getcap(), isp_scsi_setcap()
 *
 * Return Values : see isp_i_commoncap()
 * Description	 : wrappers for isp_i_commoncap()
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_scsi_getcap(struct scsi_address *ap, char *cap, int whom)
{
	int e;
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_GETCAP_START,
	    "isp_scsi_getcap_start");
	e = isp_i_commoncap(ap, cap, 0, whom, 0);
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_GETCAP_END,
	    "isp_scsi_getcap_end");
	return (e);
}

static int
isp_scsi_setcap(struct scsi_address *ap, char *cap, int value, int whom)
{
	int e;
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_SETCAP_START,
	    "isp_scsi_setcap_start");
	e = isp_i_commoncap(ap, cap, value, whom, 1);
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_SETCAP_END,
	    "isp_scsi_setcap_end");
	return (e);
}

/*
 * Function name : isp_i_updatecap()
 *
 * Return Values : -1	failed.
 *		    0	success
 *
 * Description	 : sync's the isp target parameters with the desired
 *		   isp_caps for the specified target range
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_i_updatecap(struct isp *isp, int start_tgt, int end_tgt)
{
	ushort_t cap, synch;
	struct isp_mbox_cmd mbox_cmd;
	int i;
	int rval = -1;

	i = start_tgt;

	do {
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
		    ISP_MBOX_CMD_GET_TARGET_CAP,
		    (ushort_t)(i << 8), 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		    goto fail;
		}

		cap   = mbox_cmd.mbox_in[2];
		synch = mbox_cmd.mbox_in[3];

		ISP_DEBUG2(isp, SCSI_DEBUG,
	"updatecap:tgt=%d:cap=0x%x,isp_cap=0x%x,synch=0x%x,isp_synch=0x%x",
		    i, cap, isp->isp_cap[i], synch, isp->isp_synch[i]);

		/*
		 * enable or disable ERRSYNC
		 */
		if (isp->isp_cap[i] & (ISP_CAP_WIDE | ISP_CAP_SYNC)) {
			isp->isp_cap[i] |= ISP_CAP_ERRSYNC;
		} else {
			isp->isp_cap[i] &= ~ISP_CAP_ERRSYNC;
		}

		/*
		 * Set isp cap if different from ours.
		 */
		if (isp->isp_cap[i] != cap ||
		    isp->isp_synch[i] != synch) {
			ISP_DEBUG(isp, SCSI_DEBUG,
	"Setting Target %d, new cap=0x%x (was 0x%x), synch=0x%x (was 0x%x)",
			    i, isp->isp_cap[i], cap, isp->isp_synch[i], synch);
			isp_i_mbox_cmd_init(isp, &mbox_cmd, 4, 4,
			    ISP_MBOX_CMD_SET_TARGET_CAP, (ushort_t)(i << 8),
			    isp->isp_cap[i], isp->isp_synch[i], 0, 0);
			if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
				goto fail;
			}
		}
	} while (++i < end_tgt);

	rval = 0;

fail:
	return (rval);
}


/*
 * Function name : isp_i_update_sync_prop()
 *
 * Description	 : called  when isp reports renegotiation
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_i_update_sync_prop(struct isp *isp, struct isp_cmd *sp)
{
	ushort_t cap, synch;
	struct isp_mbox_cmd mbox_cmd;
	int target = TGT(sp);

	ISP_DEBUG(isp, SCSI_DEBUG,
	    "tgt %d.%d: Negotiated new rate", TGT(sp), LUN(sp));

	/*
	 * Get new rate from ISP and save for later
	 * chip resets or scsi bus resets.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
	    ISP_MBOX_CMD_GET_TARGET_CAP,
	    (ushort_t)(target << 8), 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		return;
	}

	cap   = mbox_cmd.mbox_in[2];
	synch = mbox_cmd.mbox_in[3];

	if ((cap & ISP_CAP_WIDE) == 0) {
		if (isp->isp_backoff & (1 << target)) {
			isp_i_log(isp, CE_WARN,
			" Target %d disabled wide SCSI mode", target);
			isp->isp_backoff &= ~(1 << target);
		}
	}

	ISP_DEBUG(isp, SCSI_DEBUG,
	"tgt=%d: cap=0x%x, isp_cap=0x%x, synch=0x%x, isp_synch=0x%x",
	    target, cap, isp->isp_cap[target], synch,
	    isp->isp_synch[target]);

	isp->isp_cap[target]   = cap;
	isp->isp_synch[target] = synch;
	isp->isp_prop_update |= 1 << target;
}


/*
 * Function name : isp_i_update_props()
 *
 * Description	 : Creates/modifies/removes a target sync mode speed,
 *		   wide, and TQ properties
 *		   If offset is 0 then asynchronous mode is assumed and the
 *		   property is removed, if it existed.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_i_update_props(struct isp *isp, int tgt, ushort_t cap, ushort_t synch)
{
	char	property[32];
	int	xfer_speed = 0;
	int	offset = ((int)synch >> 8) & 0xff;
	int	flag;

	(void) sprintf(property, "target%x-sync-speed", tgt);

	if (synch) {
		if (cap & ISP_CAP_WIDE) {
			/* double xfer speed if wide has been enabled */
			xfer_speed = (1000 * 1000)/(((int)synch & 0xff) * 2);
		} else {
			xfer_speed = (1000 * 1000)/(((int)synch & 0xff) * 4);
		}
	}
	isp_i_update_this_prop(isp, property, xfer_speed,
	    sizeof (xfer_speed), offset);


	(void) sprintf(property, "target%x-TQ", tgt);
	flag = cap & ISP_CAP_TAG;
	isp_i_update_this_prop(isp, property, 0, 0, flag);

	(void) sprintf(property, "target%x-wide", tgt);
	flag = cap & ISP_CAP_WIDE;
	isp_i_update_this_prop(isp, property, 0, 0, flag);
}

/*
 * Creates/modifies/removes a property
 */
static void
isp_i_update_this_prop(struct isp *isp, char *property,
    int value, int size, int flag)
{
	int	length;

	dev_info_t *dip = isp->isp_dip;

	ISP_DEBUG(isp, SCSI_DEBUG,
		"isp_i_update_this_prop: %s=%x, size=%x, flag=%x",
		property, value, size, flag);
	if (ddi_getproplen(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    property, &length) == DDI_PROP_SUCCESS) {
		if (flag == 0) {
			if (ddi_prop_remove(DDI_DEV_T_NONE, dip, property) !=
			    DDI_PROP_SUCCESS) {
				goto fail;
			}
		} else if (size) {
			if (ddi_prop_modify(DDI_DEV_T_NONE, dip,
			    0, property,
			    (caddr_t)&value, size) != DDI_PROP_SUCCESS) {
				goto fail;
			}
		}
	} else if (flag) {
		if (ddi_prop_create(DDI_DEV_T_NONE, dip, 0, property,
		    (caddr_t)&value, size) != DDI_PROP_SUCCESS) {
			goto fail;
		}
	}
	return;

fail:
	ISP_DEBUG(isp, SCSI_DEBUG,
	    "cannot create/modify/remove %s property\n", property);
}


/*
 * Function name : isp_i_handle_qfull_cap()
 *
 * Return Values : FALSE - if setting qfull capability failed
 *		   TRUE	 - if setting qfull capability succeeded
 *		   -1    - if getting qfull capability succeeded
 *		   value - if getting qfull capability succeeded
 * Description   :
 *			Must called with response and request mutex
 *			held.
 */
static int
isp_i_handle_qfull_cap(struct isp *isp, ushort_t start, ushort_t end,
	int val, int flag_get_set, int flag_retry_interval)
{
	struct isp_mbox_cmd mbox_cmd;
	short rval = 0;
	ushort_t cmd;
	ushort_t value = (ushort_t)val;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));

	if (flag_retry_interval == SCSI_CAP_QFULL_RETRIES) {
		if (flag_get_set == ISP_GET_QFULL_CAP) {
			cmd = ISP_MBOX_CMD_GET_QFULL_RETRIES;
		} else {
			cmd = ISP_MBOX_CMD_SET_QFULL_RETRIES;
			rval = TRUE;
		}

	} else {
		if (flag_get_set == ISP_GET_QFULL_CAP) {
			cmd = ISP_MBOX_CMD_GET_QFULL_RETRY_INTERVAL;
		} else {
			cmd = ISP_MBOX_CMD_SET_QFULL_RETRY_INTERVAL;
			rval = TRUE;
		}
	}
	do {

		isp_i_mbox_cmd_init(isp, &mbox_cmd, 3, 3,
		cmd, (start<< 8), value, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			if (flag_get_set == ISP_SET_QFULL_CAP) {
				rval = FALSE;
			} else {
				rval = -1;
			}
			break;
		}
		if (flag_get_set == ISP_GET_QFULL_CAP) {
			rval = mbox_cmd.mbox_in[2];
		}

	} while (++start < end);

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));

	return ((int)rval);
}



/*
 * (de)allocator for non-std size cdb/pkt_private/status
 */
/*ARGSUSED*/
static int
isp_i_pkt_alloc_extern(struct isp *isp, struct isp_cmd *sp,
	int cmdlen, int tgtlen, int statuslen, int kf)
{
	caddr_t cdbp, scbp, tgt;
	int failure = 0;
	struct scsi_pkt *pkt = CMD2PKT(sp);

	tgt = cdbp = scbp = NULL;
	if (cmdlen > sizeof (sp->cmd_cdb)) {
		if ((cdbp = kmem_zalloc((size_t)cmdlen, kf)) == NULL) {
			failure++;
		} else {
			pkt->pkt_cdbp = (opaque_t)cdbp;
			sp->cmd_flags |= CFLAG_CDBEXTERN;
		}
	}
	if (tgtlen > PKT_PRIV_LEN) {
		if ((tgt = kmem_zalloc(tgtlen, kf)) == NULL) {
			failure++;
		} else {
			sp->cmd_flags |= CFLAG_PRIVEXTERN;
			pkt->pkt_private = tgt;
		}
	}
	if (statuslen > EXTCMDS_STATUS_SIZE) {
		if ((scbp = kmem_zalloc((size_t)statuslen, kf)) == NULL) {
			failure++;
		} else {
			sp->cmd_flags |= CFLAG_SCBEXTERN;
			pkt->pkt_scbp = (opaque_t)scbp;
		}
	}
	if (failure) {
		isp_i_pkt_destroy_extern(isp, sp);
	}
	return (failure);
}


static void
isp_i_pkt_destroy_extern(struct isp *isp, struct isp_cmd *sp)
{
	struct scsi_pkt *pkt = CMD2PKT(sp);


	if (sp->cmd_flags & CFLAG_FREE) {
		cmn_err(CE_PANIC,
		    "isp_scsi_impl_pktfree: freeing free packet");
		_NOTE(NOT_REACHED)
		/* NOTREACHED */
	}
	if (sp->cmd_flags & CFLAG_CDBEXTERN) {
		kmem_free((caddr_t)pkt->pkt_cdbp,
		    (size_t)sp->cmd_cdblen);
	}
	if (sp->cmd_flags & CFLAG_SCBEXTERN) {
		kmem_free((caddr_t)pkt->pkt_scbp,
		    (size_t)sp->cmd_scblen);
	}
	if (sp->cmd_flags & CFLAG_PRIVEXTERN) {
		kmem_free((caddr_t)pkt->pkt_private,
		    (size_t)sp->cmd_privlen);
	}

	sp->cmd_flags = CFLAG_FREE;
	kmem_cache_free(isp->isp_kmem_cache, (void *)sp);
}


/*
 * Function name : isp_scsi_init_pkt
 *
 * Return Values : pointer to scsi_pkt, or NULL
 * Description	 : Called by kernel on behalf of a target driver
 *		   calling scsi_init_pkt(9F).
 *		   Refer to tran_init_pkt(9E) man page
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static struct scsi_pkt *
isp_scsi_init_pkt(struct scsi_address *ap, struct scsi_pkt *pkt,
	struct buf *bp, int cmdlen, int statuslen, int tgtlen,
	int flags, int (*callback)(), caddr_t arg)
{
	int kf;
	int failure = 1;
	struct isp_cmd *sp;
	struct isp *isp = ADDR2ISP(ap);
	struct isp_cmd	*new_cmd = NULL;
#ifdef ISP_TEST_ALLOC_EXTERN
	cdblen *= 3; statuslen *= 3; tgtlen *= 3;
#endif
	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_scsi_init_pkt enter pkt=%x", pkt);

	/*
	 * If we've already allocated a pkt once,
	 * this request is for dma allocation only.
	 * since isp usually has TQ targets with ARQ enabled, always
	 * allocate an extended pkt
	 */
	if (pkt == NULL) {
		/*
		 * First step of isp_scsi_init_pkt:  pkt allocation
		 */
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_PKTALLOC_START,
		    "isp_i_scsi_pktalloc_start");

		kf = (callback == SLEEP_FUNC)? KM_SLEEP: KM_NOSLEEP;
		sp = kmem_cache_alloc(isp->isp_kmem_cache, kf);

		/*
		 * Selective zeroing of the pkt.
		 * Zeroing cmd_pkt, cmd_cdb_un, cmd_pkt_private, and cmd_flags.
		 */
		if (sp) {
			int *p;

			pkt = (struct scsi_pkt *)((uchar_t *)sp +
			    sizeof (struct isp_cmd) + EXTCMDS_STATUS_SIZE);
			sp->cmd_pkt		= pkt;
			pkt->pkt_ha_private	= (opaque_t)sp;
			pkt->pkt_scbp		= (opaque_t)((uchar_t *)sp +
						    sizeof (struct isp_cmd));
			sp->cmd_flags		= 0;
			sp->cmd_cdblen		= cmdlen;
			sp->cmd_scblen		= statuslen;
			sp->cmd_privlen		= tgtlen;
			pkt->pkt_address	= *ap;
			pkt->pkt_comp		= NULL;
			pkt->pkt_flags		= 0;
			pkt->pkt_time		= 0;
			pkt->pkt_resid		= 0;
			pkt->pkt_statistics	= 0;
			pkt->pkt_reason		= 0;
			pkt->pkt_cdbp		= (opaque_t)&sp->cmd_cdb;
			/* zero cdbp and pkt_private */
			p = (int *)pkt->pkt_cdbp;
			*p++	= 0;
			*p++	= 0;
			*p	= 0;
			pkt->pkt_private = (opaque_t)sp->cmd_pkt_private;
			p = (int *)pkt->pkt_private;
			*p++	= 0;
			*p++	= 0;
#ifdef _LP64
			*p++	= 0;
			*p	= 0;
#endif
			failure = 0;
		}

		/*
		 * cleanup or do more allocations
		 */
		if (failure ||
		    (cmdlen > sizeof (sp->cmd_cdb)) ||
		    (tgtlen > PKT_PRIV_LEN) ||
		    (statuslen > EXTCMDS_STATUS_SIZE)) {
			if (failure == 0) {
				failure = isp_i_pkt_alloc_extern(isp, sp,
				    cmdlen, tgtlen, statuslen, kf);
			}
			if (failure) {
				TRACE_0(TR_FAC_SCSI_ISP,
				    TR_ISP_SCSI_PKTALLOC_END,
				    "isp_i_scsi_pktalloc_end (Error)");
				return (NULL);
			}
		}

		new_cmd = sp;
	} else {
		sp = PKT2CMD(pkt);
		new_cmd = NULL;
		if (sp->cmd_flags & (CFLAG_COMPLETED | CFLAG_FINISHED)) {
			sp->cmd_flags &= ~(CFLAG_COMPLETED | CFLAG_FINISHED);
		}
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_PKTALLOC_END,
		    "isp_i_scsi_pktalloc_end");

	/*
	 * Second step of isp_scsi_init_pkt:  dma allocation
	 */
	/*
	 * Here we want to check for CFLAG_DMAVALID because some target
	 * drivers like scdk on x86 can call this routine with
	 * non-zero pkt and without freeing the DMA resources.
	 */
	if (bp && bp->b_bcount != 0 &&
		(sp->cmd_flags & CFLAG_DMAVALID) == 0) {
		int cmd_flags, dma_flags;
		int rval;
		uint_t dmacookie_count;

		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAGET_START,
		    "isp_i_scsi_dmaget_start");

		cmd_flags = sp->cmd_flags;

		/*
		 * Get the host adapter's dev_info pointer
		 */
		if (bp->b_flags & B_READ) {
			cmd_flags &= ~CFLAG_DMASEND;
			dma_flags = DDI_DMA_READ;
		} else {
			cmd_flags |= CFLAG_DMASEND;
			dma_flags = DDI_DMA_WRITE;
		}
		if (flags & PKT_CONSISTENT) {
			cmd_flags |= CFLAG_CMDIOPB;
			dma_flags |= DDI_DMA_CONSISTENT;
		}
		ASSERT(sp->cmd_dmahandle != NULL);
		rval = ddi_dma_buf_bind_handle(sp->cmd_dmahandle, bp, dma_flags,
			callback, arg, &sp->cmd_dmacookie,
			&dmacookie_count);

		if (rval) {
			switch (rval) {
			case DDI_DMA_NORESOURCES:
				bioerror(bp, 0);
				break;
			case DDI_DMA_NOMAPPING:
			case DDI_DMA_BADATTR:
				bioerror(bp, EFAULT);
				break;
			case DDI_DMA_TOOBIG:
			default:
				bioerror(bp, EINVAL);
				break;
			}
			sp->cmd_flags = cmd_flags & ~CFLAG_DMAVALID;
			if (new_cmd) {
				isp_scsi_destroy_pkt(ap, pkt);
			}
			ISP_DEBUG(isp, SCSI_DEBUG,
				"isp_scsi_init_pkt error rval=%x", rval);
			TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAGET_ERROR_END,
			    "isp_i_scsi_dmaget_end (Error)");
			return ((struct scsi_pkt *)NULL);
		}
		sp->cmd_dmacount = bp->b_bcount;
		sp->cmd_flags = cmd_flags | CFLAG_DMAVALID;

		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAGET_END,
		    "isp_i_scsi_dmaget_end");
	}
#ifdef ISPDEBUG
	/* Clear this out so we know when a command has completed. */
	bzero(&sp->cmd_isp_response, sizeof (sp->cmd_isp_response));
#endif
	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_scsi_init_pkt return pkt=%x", pkt);
	return (pkt);
}

/*
 * Function name : isp_scsi_destroy_pkt
 *
 * Return Values : none
 * Description	 : Called by kernel on behalf of a target driver
 *		   calling scsi_destroy_pkt(9F).
 *		   Refer to tran_destroy_pkt(9E) man page
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct isp_cmd *sp = PKT2CMD(pkt);
	struct isp *isp = ADDR2ISP(ap);

	/*
	 * isp_scsi_dmafree inline to make things faster
	 */
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAFREE_START,
	    "isp_scsi_dmafree_start");

	if (sp->cmd_flags & CFLAG_DMAVALID) {
		/*
		 * Free the mapping.
		 */
		(void) ddi_dma_unbind_handle(sp->cmd_dmahandle);
		sp->cmd_flags ^= CFLAG_DMAVALID;
	}
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAFREE_END,
	    "isp_scsi_dmafree_end");

	/*
	 * Free the pkt
	 */
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_PKTFREE_START,
	    "isp_i_scsi_pktfree_start");

	/*
	 * first test the most common case
	 */
	if ((sp->cmd_flags &
	    (CFLAG_FREE | CFLAG_CDBEXTERN | CFLAG_PRIVEXTERN |
	    CFLAG_SCBEXTERN)) == 0) {
		sp->cmd_flags = CFLAG_FREE;
		kmem_cache_free(isp->isp_kmem_cache, (void *)sp);
	} else {
		isp_i_pkt_destroy_extern(isp, sp);
	}
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_PKTFREE_DONE,
	    "isp_i_scsi_pktfree_done");

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_PKTFREE_END,
	    "isp_i_scsi_pktfree_end");
}


/*
 * Function name : isp_scsi_dmafree()
 *
 * Return Values : none
 * Description	 : free dvma resources
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
/*ARGSUSED*/
static void
isp_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct isp_cmd *sp = PKT2CMD(pkt);

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAFREE_START,
	    "isp_scsi_dmafree_start");

	if (sp->cmd_flags & CFLAG_DMAVALID) {
		/*
		 * Free the mapping.
		 */
		(void) ddi_dma_unbind_handle(sp->cmd_dmahandle);
		sp->cmd_flags ^= CFLAG_DMAVALID;
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_DMAFREE_END,
	    "isp_scsi_dmafree_end");

}


/*
 * Function name : isp_scsi_sync_pkt()
 *
 * Return Values : none
 * Description	 : sync dma
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
/*ARGSUSED*/
static void
isp_scsi_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	int i;
	struct isp_cmd *sp = PKT2CMD(pkt);


	if (sp->cmd_flags & CFLAG_DMAVALID) {
		i = ddi_dma_sync(sp->cmd_dmahandle, 0, 0,
		    (sp->cmd_flags & CFLAG_DMASEND) ?
		    DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
		if (i != DDI_SUCCESS) {
			struct isp	*isp = PKT2ISP(pkt);

			isp_i_log(isp, CE_WARN, "sync pkt failed");
		}
	}
}


/*
 * routine for reset notification setup, to register or cancel.
 */
static int
isp_scsi_reset_notify(struct scsi_address *ap, int flag,
void (*callback)(caddr_t), caddr_t arg)
{
	struct isp	*isp = ADDR2ISP(ap);
	return (scsi_hba_reset_notify_setup(ap, flag, callback, arg,
	    ISP_REQ_MUTEX(isp), &isp->isp_reset_notify_listf));
}


/*
 * the waitQ is used when the request mutex is held. requests will go
 * in the waitQ which will be emptied just before releasing the request
 * mutex; the waitQ reduces the contention on the request mutex significantly
 *
 * Note that the waitq mutex is released *after* the request mutex; this
 * closes a small window where we empty the waitQ but before releasing
 * the request mutex, the waitQ is filled again. isp_scsi_start will
 * attempt to get the request mutex after adding the cmd to the waitQ
 * which ensures that after the waitQ is always emptied.
 */
#define	ISP_CHECK_WAITQ_TIMEOUT(isp)					\
	if (isp->isp_waitq_timeout == 0) {				\
		isp->isp_waitq_timeout = timeout(			\
			(void (*)(void*))isp_i_check_waitQ,		\
		    (caddr_t)isp, drv_usectohz((clock_t)1000000));	\
	}

static void
isp_i_check_waitQ(struct isp *isp)
{
	mutex_enter(ISP_REQ_MUTEX(isp));
	mutex_enter(ISP_WAITQ_MUTEX(isp));
	isp->isp_waitq_timeout = 0;
	isp_i_empty_waitQ(isp);
	mutex_exit(ISP_REQ_MUTEX(isp));
	if (isp->isp_waitf) {
		ISP_CHECK_WAITQ_TIMEOUT(isp);
	}
	mutex_exit(ISP_WAITQ_MUTEX(isp));
}

/*
 * Function name : isp_i_empty_waitQ()
 *
 * Return Values : none
 *
 * Description	 : empties the waitQ
 *		   copies the head of the queue and zeroes the waitQ
 *		   calls isp_i_start_cmd() for each packet
 *		   if all cmds have been submitted, check waitQ again
 *		   before exiting
 *		   if a transport error occurs, complete packet here
 *		   if a TRAN_BUSY occurs, then restore waitQ and try again
 *		   later
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_i_empty_waitQ(struct isp *isp)
{
	struct isp_cmd *sp, *head, *tail;
	int rval;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)));

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_EMPTY_WAITQ_START,
	    "isp_i_empty_waitQ_start");

	/* optimization check */
	if (isp->isp_waitf == NULL) {
		/* nothing to dequeue */
		goto exit;
	}

	/*
	 * if we are in the middle of doing a reset then we
	 * we do not want to take pkts off of the waitQ
	 * just yet
	 */
	if (isp->isp_in_reset) {
		ISP_DEBUG(isp, SCSI_DEBUG, "can't dequeue 0x%x (in shutdown)",
		    isp->isp_waitf);
		goto exit;
	}

again:
	/*
	 * walk thru the waitQ and attempt to start the cmd
	 */
	while (isp->isp_waitf != NULL) {
		/*
		 * copy queue head, clear wait queue and release WAITQ_MUTEX
		 */
		head = isp->isp_waitf;
		tail = isp->isp_waitb;
		isp->isp_waitf = isp->isp_waitb = NULL;
		mutex_exit(ISP_WAITQ_MUTEX(isp));

		/*
		 * empty the local list
		 */
		while (head != NULL) {
			struct scsi_pkt *pkt;

			sp = head;
			head = sp->cmd_forw;
			sp->cmd_forw = NULL;

			ISP_DEBUG2(isp, SCSI_DEBUG, "starting waitQ sp=0x%x",
			    (caddr_t)sp);

			/* try to start the command */
			if ((rval = isp_i_start_cmd(isp, sp)) == TRAN_ACCEPT) {
				continue;	/* success: go to next one */
			}

			/* transport of the cmd failed */

			ISP_DEBUG(isp, SCSI_DEBUG,
			    "isp_i_empty_waitQ: transport failed (%x)", rval);

			/*
			 * if the isp could not handle more requests,
			 * (rval was TRAN_BUSY) then
			 * put all requests back on the waitQ before
			 * releasing the REQ_MUTEX
			 * if there was another transport error then
			 * do not put this packet back on the queue
			 * but complete it here
			 */
			if (rval == TRAN_BUSY) {
				sp->cmd_forw = head;
				head = sp;
			}

			mutex_enter(ISP_WAITQ_MUTEX(isp));
			if (isp->isp_waitf != NULL) {
				/*
				 * somebody else has added to waitQ while
				 * we were messing around, so add our queue
				 * to what is now on wiatQ
				 */
				tail->cmd_forw = isp->isp_waitf;
				isp->isp_waitf = head;
			} else {
				/*
				 * waitQ is still empty, so just put our
				 * list back
				 */
				isp->isp_waitf = head;
				isp->isp_waitb = tail;
			}

			if (rval == TRAN_BUSY) {
				/*
				 * request queue was full; try again
				 * 1 sec later
				 */
				ISP_CHECK_WAITQ_TIMEOUT(isp);
				goto exit;
			}

			/*
			 * transport failed, but (rval != TRAN_BUSY)
			 *
			 * set reason and call target completion routine
			 */

			ISP_SET_REASON(sp, CMD_TRAN_ERR);
			ASSERT(sp != NULL);
			pkt = CMD2PKT(sp);
			ASSERT(pkt != NULL);
			if (pkt->pkt_comp != NULL) {
#ifdef ISPDEBUG
				sp->cmd_flags |= CFLAG_FINISHED;
#endif
				/* pkt no longer in transport */
				sp->cmd_flags &= ~CFLAG_IN_TRANSPORT;
				mutex_exit(ISP_WAITQ_MUTEX(isp));
				mutex_exit(ISP_REQ_MUTEX(isp));
				(*pkt->pkt_comp)(pkt);
				mutex_enter(ISP_REQ_MUTEX(isp));
				mutex_enter(ISP_WAITQ_MUTEX(isp));
			}
			goto again;
		}
		mutex_enter(ISP_WAITQ_MUTEX(isp));
	}

exit:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_EMPTY_WAITQ_END,
	    "isp_i_empty_waitQ_end");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)));
}


/*
 * Function name : isp_scsi_start()
 *
 * Return Values : TRAN_FATAL_ERROR	- isp has been shutdown
 *		   TRAN_BUSY		- request queue is full
 *		   TRAN_ACCEPT		- pkt has been submitted to isp
 *					  (or is held in the waitQ)
 * Description	 : init pkt
 *		   check the waitQ and if empty try to get the request mutex
 *		   if this mutex is held, put request in waitQ and return
 *		   if we can get the mutex, start the request
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * XXX: We assume that dvma bounds checking is performed by
 *	the target driver!  Also, that sp is *ALWAYS* valid.
 *
 * Note: No support for > 1 data segment.
 */
static int
isp_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct isp_cmd *sp = PKT2CMD(pkt);
	struct isp *isp;
	int rval = TRAN_ACCEPT;
	int cdbsize;
	struct isp_request *req;


	isp = ADDR2ISP(ap);

	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());

	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_scsi_start %x", sp);
	TRACE_1(TR_FAC_SCSI_ISP, TR_ISP_SCSI_START_START,
	    "isp_scsi_start_start isp = %x", isp);

	/*
	 * if we have a shutdown, return packet
	 */
	if (isp->isp_shutdown) {
		return (TRAN_FATAL_ERROR);
	}

	ISP_DEBUG2(isp, SCSI_DEBUG, "SCSI starting packet, sp=0x%x",
	    (caddr_t)sp);

	/* pkt had better not already be in transport */
	ASSERT(!(sp->cmd_flags & CFLAG_IN_TRANSPORT));
	sp->cmd_flags = (sp->cmd_flags & ~CFLAG_TRANFLAG) | CFLAG_IN_TRANSPORT;
	pkt->pkt_reason = CMD_CMPLT;

	cdbsize = sp->cmd_cdblen;

	/*
	 * set up request in cmd_isp_request area so it is ready to
	 * go once we have the request mutex
	 * XXX do we need to zero each time
	 */
	req = &sp->cmd_isp_request;

	req->req_header.cq_entry_type = CQ_TYPE_REQUEST;
	req->req_header.cq_entry_count = 1;
	req->req_header.cq_flags = 0;
	req->req_header.cq_seqno = 0;
	req->req_reserved = 0;

	ASSERT(ISP_LOOKUP_ID(sp->cmd_id) == sp);

	req->req_token = sp->cmd_id;
	req->req_scsi_id.req_target = TGT(sp);
	req->req_scsi_id.req_lun_trn = LUN(sp);
	req->req_time = pkt->pkt_time;

	ISP_SET_PKT_FLAGS(pkt->pkt_flags, req->req_flags);

	/*
	 * Setup dma transfers data segments.
	 *
	 * NOTE: Only 1 dataseg supported.
	 */
	if (sp->cmd_flags & CFLAG_DMAVALID) {
		/*
		 * Have to tell isp which direction dma transfer is going.
		 */
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_START_DMA_START,
		    "isp_scsi_start");
		pkt->pkt_resid = (size_t)sp->cmd_dmacount;

		if (sp->cmd_flags & CFLAG_CMDIOPB) {
			(void) ddi_dma_sync(sp->cmd_dmahandle, 0, 0,
			    DDI_DMA_SYNC_FORDEV);
		}

		req->req_seg_count = 1;
		req->req_dataseg[0].d_count = sp->cmd_dmacount;
		req->req_dataseg[0].d_base = sp->cmd_dmacookie.dmac_address;
		if (sp->cmd_flags & CFLAG_DMASEND) {
			req->req_flags |= ISP_REQ_FLAG_DATA_WRITE;
		} else {
			req->req_flags |= ISP_REQ_FLAG_DATA_READ;
		}
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_START_DMA_END,
		    "isp_scsi_start");
	} else {
		req->req_seg_count = 0;
		req->req_dataseg[0].d_count = 0;
	}

	ISP_LOAD_REQUEST_CDB(req, sp, cdbsize);

	/*
	 * calculate deadline from pkt_time
	 * Instead of multiplying by 100 (ie. HZ), we multiply by 128 so
	 * we can shift and at the same time have a 28% grace period
	 * we ignore the rare case of pkt_time == 0 and deal with it
	 * in isp_i_watch()
	 */
#ifdef OLDTIMEOUT
	sp->cmd_deadline = lbolt + (pkt->pkt_time * 128);
	sp->cmd_start_time = lbolt;
#endif
	/*
	 * the normal case is a non-polled cmd, so deal with that first
	 */
	if ((pkt->pkt_flags & FLAG_NOINTR) == 0) {
		/*
		 * isp request mutex can be held for a long time; therefore,
		 * if request mutex is held, we queue the packet in a waitQ
		 * Consequently, we now need to check the waitQ before every
		 * release of the request mutex
		 *
		 * if the waitQ is non-empty, add cmd to waitQ to preserve
		 * some order
		 */
		mutex_enter(ISP_WAITQ_MUTEX(isp));
		if (isp->isp_in_reset ||
		    (isp->isp_waitf != NULL) ||
		    (mutex_tryenter(ISP_REQ_MUTEX(isp)) == 0)) {

			ISP_DEBUG2(isp, SCSI_DEBUG,
			    "putting pkt on waitQ sp=0x%x", (caddr_t)sp);

			/*
			 * either we have pkts on the wait queue or
			 * we can't get the request mutex
			 * OR we are in the middle of handling a reset
			 *
			 * in any case we don't have the request
			 * queue mutex but we do have the wait queue
			 * mutex
			 *
			 * so, for whatever reason, we want to put the
			 * supplied packet on the wait queue instead of
			 * on the request queue
			 */
			if (isp->isp_waitf == NULL) {
				/*
				 * there's nothing on the wait queue
				 * and we can't get the request queue,
				 * so put pkt on wait queue
				 */
				isp->isp_waitb = isp->isp_waitf = sp;
				sp->cmd_forw = NULL;
			} else {
				/*
				 * there is something on the
				 * wait queue so put our pkt at its end
				 */
				struct isp_cmd *dp = isp->isp_waitb;
				dp->cmd_forw = isp->isp_waitb = sp;
				sp->cmd_forw = NULL;
			}
			/*
			 * this is really paranoia and shouldn't
			 * be necessary
			 */
			if (!isp->isp_in_reset &&
			    mutex_tryenter(ISP_REQ_MUTEX(isp))) {
				isp_i_empty_waitQ(isp);
				mutex_exit(ISP_REQ_MUTEX(isp));
			}
			mutex_exit(ISP_WAITQ_MUTEX(isp));
		} else {
			/*
			 * no entries on wait queue *and* we were
			 * able to get request queue lock
			 */

			/*
			 * no need to hold this, and releasing it
			 * will give others a chance to put
			 * their request some place, since we now
			 * own the request queue
			 */
			mutex_exit(ISP_WAITQ_MUTEX(isp));

			rval = isp_i_start_cmd(isp, sp);
			if (rval == TRAN_BUSY) {
				/*
				 * put request back at the head of the waitQ
				 */
				mutex_enter(ISP_WAITQ_MUTEX(isp));
				sp->cmd_forw = isp->isp_waitf;
				isp->isp_waitf = sp;
				if (isp->isp_waitb == NULL) {
					isp->isp_waitb = sp;
				}
				mutex_exit(ISP_WAITQ_MUTEX(isp));
				rval = TRAN_ACCEPT;
			}
			ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
#ifndef OLDTIMEOUT
			isp->isp_alive = 1;
#endif
		}
	} else {
		rval = isp_i_polled_cmd_start(isp, sp);
	}

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_START_END, "isp_scsi_start_end");
	return (rval);
}


/*
 * Function name : isp_i_start_cmd()
 *
 * Return Values : TRAN_ACCEPT	- request is in the isp request queue
 *		   TRAN_BUSY	- request queue is full
 *
 * Description	 : if there is space in the request queue, copy over request
 *		   enter normal requests in the isp_slots list
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_i_start_cmd(struct isp *isp, struct isp_cmd *sp)
{
	struct isp_request *req;
	short slot;
	struct scsi_pkt *pkt;
	int tgt;


	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_START_CMD_START,
	    "isp_i_start_cmd_start");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(sp != NULL);
	ASSERT(ISP_LOOKUP_ID(sp->cmd_id) == sp);

	pkt = CMD2PKT(sp);
	ASSERT(pkt != NULL);
	tgt = TGT(sp);

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_start_cmd: sp=%x, req_in=%x, pkt_time=%x",
	    (caddr_t)sp, isp->isp_request_in, pkt->pkt_time);

	if (isp->isp_shutdown) {
		return (TRAN_BUSY);
	}

	/*
	 * Check to see how much space is available in the
	 * Request Queue, save this so we do not have to do
	 * a lot of PIOs
	 */
	if (isp->isp_queue_space == 0) {
		isp_i_update_queue_space(isp);

		/*
		 * Check now to see if the queue is still full
		 * Report TRAN_BUSY if we are full
		 */
		if (isp->isp_queue_space == 0) {
			TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_START_CMD_Q_FULL_END,
			    "isp_i_start_cmd_end (Queue Full)");
			return (TRAN_BUSY);
		}
	}

	/*
	 * this flag is defined in firmware source code although
	 * not documented.
	 */
	/* The ability to disable auto request sense per packet */
	if ((sp->cmd_scblen < sizeof (struct scsi_arq_status)) &&
	    (isp->isp_cap[tgt] & ISP_CAP_AUTOSENSE)) {
		ISP_DEBUG2(isp, SCSI_DEBUG,
			"isp_i_start_cmd: disabling ARQ=%x", sp);
		sp->cmd_isp_request.req_flags |= ISP_REQ_FLAG_DISARQ;
	}

	/*
	 * Put I/O request in isp request queue to run.
	 * Get the next request in pointer.
	 */
	ISP_GET_NEXT_REQUEST_IN(isp, req);

	/*
	 * Copy 40 of the  64 byte request into the request queue
	 * (only 1 data seg)
	 */
	ISP_COPY_OUT_REQ(isp, &sp->cmd_isp_request, req);

	/*
	 * Use correct offset and size for syncing
	 */
	(void) ddi_dma_sync(isp->isp_dmahandle,
	    (off_t)(isp->isp_request_in * sizeof (struct isp_request)),
	    (size_t)sizeof (struct isp_request),
	    DDI_DMA_SYNC_FORDEV);

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_START_CMD_AFTER_SYNC,
	    "isp_i_start_cmd_after_sync");

	/*
	 * Find a free slot to store the pkt in for crash protection for
	 * non-polled commands. Polled commands do not have to be kept
	 * track of since the busy wait loops keep track of them.
	 *
	 * We should *ALWAYS* be able to find a free slot; or we're broke!
	 *
	 * we look for a free slot first locally by trying the "next" slot,
	 * and using isp_i_find_freeslot() (which scans the whole slot list)
	 * only if that fails
	 */
	if ((pkt->pkt_flags & FLAG_NOINTR) == 0) {

		mutex_enter(ISP_WAITQ_MUTEX(isp));
		slot = isp->isp_free_slot++;
		if (isp->isp_free_slot >= (ushort_t)ISP_MAX_SLOTS) {
			isp->isp_free_slot = 0;
		}
		if (isp->isp_slots[slot].slot_cmd != NULL) {
			if ((slot = isp_i_find_freeslot(isp)) < 0) {
				/* no free slots! */
				mutex_exit(ISP_WAITQ_MUTEX(isp));
				return (TRAN_BUSY);
			}
		}
		mutex_exit(ISP_WAITQ_MUTEX(isp));

#ifdef OLDTIMEOUT
		/*
		 * Update ISP deadline time for new cmd sent to ISP
		 * There is a race here with isp_i_watch(); if the compiler
		 * rearranges this code, then this may cause some false
		 * timeouts
		 */
		isp->isp_slots[slot].slot_deadline = sp->cmd_deadline;
#endif	/* OLDTIMEOUT */
		sp->cmd_slot = slot;
		isp->isp_slots[slot].slot_cmd = sp;

		ISP_DEBUG2(isp, SCSI_DEBUG, "sp=0x%x put in slot %d",
		    (caddr_t)sp, slot);
	}

	/*
	 * this is ifdefed out because (1) it fails warlock, and,
	 * more importantly, (2) it doesn't work -- this will be
	 * replace soon by code that correctly renegotiates with
	 * the target (see bug id# 4278801)
	 */
#ifdef	ISP_RENEGOT_WORKED
	/* update capabilities if requested by target */
	if (pkt->pkt_flags & FLAG_RENEGOTIATE_WIDE_SYNC) {
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_i_initcap(isp, tgt, tgt);
		(void) isp_i_updatecap(isp, tgt, tgt);
		mutex_exit(ISP_RESP_MUTEX(isp));
	}
#endif

	/*
	 * Tell isp it's got a new I/O request...
	 */
	ISP_SET_REQUEST_IN(isp);
	isp->isp_queue_space--;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_START_CMD_END,
	    "isp_i_start_cmd_end");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());
	return (TRAN_ACCEPT);
}


/*
 * the isp interrupt routine
 */
static uint_t
isp_intr(caddr_t arg)
{
	struct isp_cmd *sp;
	struct isp_cmd *head, *tail;
	ushort_t response_in;
	struct isp_response *resp, *cmd_resp;
	struct isp *isp = (struct isp *)arg;
	struct isp_slot *isp_slot;
	int n;
	off_t offset;
	uint_t sync_size;
	uint_t intr_claimed = DDI_INTR_UNCLAIMED;


	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_intr entry");

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_START, "isp_intr_start");

	ASSERT(isp != NULL);

#ifdef ISP_PERF
	isp->isp_intr_count++;
#endif

	/*
	 * set flag saying we are in interrupt routine -- this is in case
	 * the reset code has reset the chip and assumes that interrupts
	 * have been handled (but they have not)
	 */
	mutex_enter(ISP_INTR_MUTEX(isp));
	ASSERT(isp->isp_in_intr >= 0);
	isp->isp_in_intr++;
	mutex_exit(ISP_INTR_MUTEX(isp));

	while (ISP_INT_PENDING(isp)) {
again:
		/*
		 * head list collects completed packets for callback later
		 */
		head = tail = NULL;

		/*
		 * Assume no mailbox events (e.g. mailbox cmds, asynch
		 * events, and isp dma errors) as common case.
		 */
		mutex_enter(ISP_RESP_MUTEX(isp));

		/*
		 * kstat_intr support
		 */
		if (intr_claimed == DDI_INTR_UNCLAIMED && isp->isp_kstat) {
			ISP_KSTAT_INTR_PTR(isp)->intrs[KSTAT_INTR_HARD]++;
		}

		intr_claimed = DDI_INTR_CLAIMED;

		/*
		 * we can be interrupted for one of three reasons:
		 * - a mailbox command has completed
		 * - a chip-generated async event has happened (e.g.
		 *	external bus reset), or
		 * - one or more pkts have been placed on the response queue
		 *	by the chip
		 * so check here for/handle the first two cases
		 */
		if (ISP_CHECK_SEMAPHORE_LOCK(isp) != 0) {
			mutex_exit(ISP_RESP_MUTEX(isp));
			if (isp_i_handle_mbox_cmd(isp) != ISP_AEN_SUCCESS) {
				goto dun;
			}
			/*
			 * if there was a reset then check the response
			 * queue again
			 */
			goto again;
		}

		/* semaphore lock was zero */

		/*
		 * Workaround for  bugid 1220411 & 4023994.
		 */
		if (isp->isp_mbox.mbox_flags &
		    ISP_MBOX_CMD_FLAGS_Q_NOT_INIT) {
			ISP_CLEAR_RISC_INT(isp);
			mutex_exit(ISP_RESP_MUTEX(isp));
			continue;
		}

		/*
		 * Loop through completion response queue and post
		 * completed pkts.  Check response queue again
		 * afterwards in case there are more and process them
		 * to keep interrupt response time low under heavy load.
		 *
		 * To conserve PIO's, we only update the isp response
		 * out index after draining it.
		 */
		isp->isp_response_in =
			response_in = ISP_GET_RESPONSE_IN(isp);
		ASSERT(!(response_in >= ISP_MAX_RESPONSES));
		ISP_CLEAR_RISC_INT(isp);

		/*
		 * Calculate how many requests there are in the queue
		 * If this is < 0, then we are wrapping around
		 * and syncing the packets need two separate syncs
		 */
		n = response_in - isp->isp_response_out;
		offset = (off_t)((ISP_MAX_REQUESTS *
		    sizeof (struct isp_request)) +
		    (isp->isp_response_out *
			sizeof (struct isp_response)));

		if (n == 1) {
			sync_size =
				((uint_t)sizeof (struct isp_response));
		} else if (n > 0) {
			sync_size =
				n * ((uint_t)sizeof (struct isp_response));
		} else if (n < 0) {
			sync_size =
				(ISP_MAX_REQUESTS - isp->isp_response_out) *
				((uint_t)sizeof (struct isp_response));

			/*
			 * we wrapped around and need an extra sync
			 */
			(void) ddi_dma_sync(isp->isp_dmahandle,
			    (off_t)((ISP_MAX_REQUESTS *
				sizeof (struct isp_request))),
			    response_in *
			    ((uint_t)sizeof (struct isp_response)),
			    DDI_DMA_SYNC_FORKERNEL);

			n = ISP_MAX_REQUESTS - isp->isp_response_out +
				response_in;
		} else {
			goto update;
		}

		(void) ddi_dma_sync(isp->isp_dmahandle,
		    (off_t)offset, sync_size, DDI_DMA_SYNC_FORKERNEL);
		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "sync: n=%d, in=%d, out=%d, offset=%d, size=%d\n",
		    n, response_in, isp->isp_response_out, offset, sync_size);

		while (n-- > 0) {
			uint32_t rptr;

			ISP_GET_NEXT_RESPONSE_OUT(isp, resp);
			ASSERT(resp != NULL);

			ISP_COPY_IN_TOKEN(isp, resp, &rptr);
			sp = ISP_LOOKUP_ID(rptr);
			ASSERT(sp != NULL);
#ifdef ISPDEBUG
			ASSERT((sp->cmd_flags & CFLAG_COMPLETED) == 0);
			ASSERT((sp->cmd_flags & CFLAG_FINISHED) == 0);
			sp->cmd_flags |= CFLAG_FINISHED;
			ISP_DEBUG2(isp, SCSI_DEBUG,
			    "isp_intr %x done, pkt_time=%x", sp,
			    CMD2PKT(sp)->pkt_time);
#endif	/* ISPDEBUG */

			/*
			 * copy over response packet in sp
			 */
			ISP_COPY_IN_RESP(isp, resp,
			    &sp->cmd_isp_response);

			cmd_resp = &sp->cmd_isp_response;

			/*
			 * Paranoia:  This should never happen.
			 */
			if (cmd_resp->resp_header.cq_entry_type ==
			    CQ_TYPE_REQUEST) {
				/*
				 * The firmware had problems w/this
				 * packet and punted.  Forge a reply
				 * in isp_i_response_error() and send
				 * it back to the target driver.
				 */
				cmd_resp->resp_header.cq_entry_type =
					CQ_TYPE_RESPONSE;
				cmd_resp->resp_header.cq_flags |=
					CQ_FLAG_FULL;
			} else if (cmd_resp->resp_header.cq_entry_type !=
			    CQ_TYPE_RESPONSE) {
				isp_i_log(isp, CE_WARN,
				    "invalid response:in=%x, out=%x, mbx5=%x",
				    isp->isp_response_in,
				    isp->isp_response_out,
				    ISP_READ_MBOX_REG(isp,
					&isp->isp_mbox_reg->isp_mailbox5));
				continue;
			}

			/*
			 * check for firmware returning the packet because
			 * the queue has been aborted (and is waiting for a
			 * marker) -- if this happens set both target and
			 * bus reset statistics (since we don't know which
			 * caused it)
			 *
			 * Note: when CMD_RESET returned no other fields of
			 * response are valid according to QLogic
			 */
			if (cmd_resp->resp_reason == CMD_RESET) {
				resp->resp_state = STAT_BUS_RESET << 8;
				ISP_DEBUG(isp, SCSI_DEBUG, "found CMD_RESET");
			}

			/*
			 * Check response header flags.
			 */
			if (cmd_resp->resp_header.cq_flags & CQ_FLAG_ERR_MASK) {
				ISP_DEBUG(isp, SCSI_DEBUG,
				    "flag error (0x%x)",
				    cmd_resp->resp_header.cq_flags &
				    CQ_FLAG_ERR_MASK);
				if (isp_i_response_error(isp,
				    cmd_resp) == ACTION_IGNORE) {
					continue;
				}
			}

			/*
			 * Update isp deadman timer list before
			 * doing the callback and while we are
			 * holding the response mutex.
			 * note that polled cmds are not in
			 * isp_slots list
			 */
			isp_slot = &isp->isp_slots[sp->cmd_slot];
			if (isp_slot->slot_cmd == sp) {
				isp_slot->slot_cmd = NULL;
#ifdef OLDTIMEOUT
				isp_slot->slot_deadline = 0;
#endif
			}

			if (head != NULL) {
				tail->cmd_forw = sp;
				tail = sp;
				tail->cmd_forw = NULL;
			} else {
				tail = head = sp;
				sp->cmd_forw = NULL;
			}

			TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_Q_END,
			    "isp_intr_queue_end");
		}
update:
		ISP_SET_RESPONSE_OUT(isp);

		mutex_exit(ISP_RESP_MUTEX(isp));

		if (head != NULL) {
			isp_i_call_pkt_comp(head);
		}

		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_AGAIN,
		    "isp_intr_again");
	}
#ifndef OLDTIMEOUT
	isp->isp_alive = 1;
#endif

dun:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_END, "isp_intr_end");

	/* signal possible waiting thread that we are done w/interrupt */
	mutex_enter(ISP_INTR_MUTEX(isp));
	if (--isp->isp_in_intr < 0) {
		ISP_DEBUG(isp, SCSI_DEBUG, "error: isp_in_intr < 0");
		isp->isp_in_intr = 0;
	}
	if (isp->isp_in_intr == 0) {
		cv_broadcast(ISP_INTR_CV(isp));
	}
	mutex_exit(ISP_INTR_MUTEX(isp));

	return (intr_claimed);
}


/*
 * Function name : isp_i_call_pkt_comp()
 *
 * Return Values : none
 *
 * Description	 :
 *		   callback into target driver
 *		   argument is a  NULL-terminated list of packets
 *		   copy over stuff from response packet
 *
 * Context	 : Can be called by interrupt thread.
 */

#ifdef ISPDEBUG
static int isp_test_reason = 0;
_NOTE(SCHEME_PROTECTS_DATA("No Mutex Needed", isp_test_reason))
#endif

static void
isp_i_call_pkt_comp(struct isp_cmd *head)
{
	struct isp *isp;
	struct isp_cmd *sp;
	struct scsi_pkt *pkt;
	struct isp_response *resp;
	uchar_t status;
	int 	tgt;


	isp = CMD2ISP(head);

	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());

	while (head != NULL) {
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_CALL_PKT_COMP_START,
		    "isp_i_call_pkt_comp_start");
		sp = head;
		tgt = TGT(sp);
		pkt = CMD2PKT(sp);
		head = sp->cmd_forw;

		ASSERT(sp->cmd_flags & CFLAG_FINISHED);

		resp = &sp->cmd_isp_response;

		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "completing pkt, sp=0x%x, reason=0x%x",
		    (caddr_t)sp, resp->resp_reason);

		status = pkt->pkt_scbp[0] = (uchar_t)resp->resp_scb;
		if (pkt->pkt_reason == CMD_CMPLT) {
			pkt->pkt_reason = (uchar_t)resp->resp_reason;
			if (pkt->pkt_reason > CMD_UNX_BUS_FREE) {
				/*
				 * An underrun is not an error to be
				 * reported inside pkt->pkt_reason as
				 * resid will have this information
				 * for target drivers.
				 */
				if (pkt->pkt_reason == DATA_UNDER_RUN) {
					pkt->pkt_reason = CMD_CMPLT;
				} else if (pkt->pkt_reason == TAG_REJECT) {
					/*
					 * XXX: on SBUS this will never happen
					 * since the firmware doesn't ever
					 * return this -- on queue full it
					 * returns CMD_CMPLT and sense data
					 * of 0x28 (queue full), so isn't
					 * that what we should be checking
					 * for ???
					 */
					pkt->pkt_reason = CMD_TAG_REJECT;
				} else {
					/* catch all */
					pkt->pkt_reason = CMD_TRAN_ERR;
				}
			}
		}

		/*
		 * The packet state is actually the high byte of the
		 * resp_state
		 */
		pkt->pkt_state = resp->resp_state >> 8;
		pkt->pkt_statistics = resp->resp_status_flags;
		pkt->pkt_resid = (size_t)resp->resp_resid;

		if (pkt->pkt_statistics & ISP_STAT_SYNC) {
			isp_i_log(isp, CE_WARN,
			    "Target %d reducing transfer rate", tgt);
			ISP_MUTEX_ENTER(isp);
			isp->isp_backoff |= (1 << tgt);
			ISP_MUTEX_EXIT(isp);
			pkt->pkt_statistics &= ~ISP_STAT_SYNC;
		}
		/*
		 * check for parity errors
		 */
		if (pkt->pkt_statistics & STAT_PERR) {
			isp_i_log(isp, CE_WARN, "Parity Error");
		}
		/*
		 * Check to see if the ISP has negotiated a new sync
		 * rate with the device and store that information
		 * for a latter date
		 */
		if (pkt->pkt_statistics & ISP_STAT_NEGOTIATE) {
			ISP_MUTEX_ENTER(isp);
			isp_i_update_sync_prop(isp, sp);
			pkt->pkt_statistics &= ~ISP_STAT_NEGOTIATE;
			ISP_MUTEX_EXIT(isp);
		}



#ifdef ISPDEBUG
		if ((isp_test_reason != 0) &&
		    (pkt->pkt_reason == CMD_CMPLT)) {
			pkt->pkt_reason = (uchar_t)isp_test_reason;
			if (isp_test_reason == CMD_ABORTED) {
				pkt->pkt_statistics |= STAT_ABORTED;
			}
			if (isp_test_reason == CMD_RESET) {
				pkt->pkt_statistics |=
				    STAT_DEV_RESET | STAT_BUS_RESET;
			}
			isp_test_reason = 0;
		}
		if (pkt->pkt_resid || status ||
		    pkt->pkt_reason) {
			uchar_t *cp;
			char buf[128];
			int i;

			ISP_DEBUG(isp, SCSI_DEBUG,
	"tgt %d.%d: resid=%x,reason=%s,status=%x,stats=%x,state=%x",
				TGT(sp), LUN(sp), pkt->pkt_resid,
				scsi_rname(pkt->pkt_reason),
				(unsigned)status,
				(unsigned)pkt->pkt_statistics,
				(unsigned)pkt->pkt_state);

			cp = (uchar_t *)pkt->pkt_cdbp;
			buf[0] = '\0';
			for (i = 0; i < (int)sp->cmd_cdblen; i++) {
				(void) sprintf(
				    &buf[strlen(buf)], " 0x%x", *cp++);
				if (strlen(buf) > 124) {
					break;
				}
			}
			ISP_DEBUG(isp, SCSI_DEBUG,
			"\tcflags=%x, cdb dump: %s", sp->cmd_flags, buf);

			if (pkt->pkt_reason == CMD_RESET) {
				ASSERT(pkt->pkt_statistics &
				    (STAT_BUS_RESET | STAT_DEV_RESET
					| STAT_ABORTED));
			} else if (pkt->pkt_reason ==
			    CMD_ABORTED) {
				ASSERT(pkt->pkt_statistics &
				    STAT_ABORTED);
			}
		}
		if (pkt->pkt_state & STATE_XFERRED_DATA) {
			if (ispdebug > 1 && pkt->pkt_resid) {
				ISP_DEBUG(isp, SCSI_DEBUG,
				    "%d.%d finishes with %d resid",
				    TGT(sp), LUN(sp), pkt->pkt_resid);
			}
		}
#endif	/* ISPDEBUG */


		/*
		 * was there a check condition and auto request sense?
		 * fake some arqstat fields
		 */
		if ((status != 0) &&
		    ((pkt->pkt_state & (STATE_GOT_STATUS | STATE_ARQ_DONE)) ==
		    (STATE_GOT_STATUS | STATE_ARQ_DONE))) {
			isp_i_handle_arq(isp, sp);
		}


		/*
		 * if data was xferred and this was an IOPB, we need
		 * to do a dma sync
		 */
		if ((sp->cmd_flags & CFLAG_CMDIOPB) &&
		    (pkt->pkt_state & STATE_XFERRED_DATA)) {

			/*
			 * only one segment yet
			 */
			(void) ddi_dma_sync(sp->cmd_dmahandle, 0,
			    (size_t)0, DDI_DMA_SYNC_FORCPU);
		}


		/*
		 * pkt had better be in transport, be finished, and not
		 * be completed
		 */
		ASSERT(sp->cmd_flags & CFLAG_IN_TRANSPORT);
		ASSERT(sp->cmd_flags & CFLAG_FINISHED);
		ASSERT((sp->cmd_flags & CFLAG_COMPLETED) == 0);

		sp->cmd_flags = (sp->cmd_flags & ~CFLAG_IN_TRANSPORT) |
			CFLAG_COMPLETED;

		/*
		 * Call packet completion routine if FLAG_NOINTR is not set.
		 * If FLAG_NOINTR is set turning on CFLAG_COMPLETED in line
		 * above will cause busy wait loop in
		 * isp_i_polled_cmd_start() to exit.
		 */
		if (!(pkt->pkt_flags & FLAG_NOINTR) &&
		    (pkt->pkt_comp != NULL)) {
			ISP_DEBUG2(isp, SCSI_DEBUG,
			    "completing pkt(tgt), sp=0x%x, reason=0x%x",
			    (caddr_t)sp, pkt->pkt_reason);
			(*pkt->pkt_comp)(pkt);
		}

		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_CALL_PKT_COMP_END,
		    "isp_i_call_pkt_comp_end");
	}
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)) == 0 || ddi_in_panic());
}


/*
 * Function name : isp_i_handle_mbox_cmd()
 *
 * Description	 : called from isp_intr() to handle a mbox or async event
 *
 * Context	 : Called by interrupt thread
 */
static int
isp_i_handle_mbox_cmd(struct isp *isp)
{
	int aen = ISP_AEN_FAILURE;
	int16_t event;


	ASSERT(isp != NULL);
	ASSERT(!mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(!mutex_owned(ISP_RESP_MUTEX(isp)));

	event = ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox0);

	/*
	 * process a mailbox event
	 */
#ifdef ISP_PERF
	isp->isp_rpio_count += 1;
#endif
	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_intr: event= 0x%x", event);
	if (event & ISP_MBOX_EVENT_ASYNCH) {
		ISP_MUTEX_ENTER(isp);
		aen = isp_i_async_event(isp, event);
		ISP_MUTEX_EXIT(isp);
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_ASYNC_END,
		    "isp_intr_end (Async Event)");
	} else {
		isp_i_mbox_cmd_complete(isp);
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_INTR_MBOX_END,
		    "isp_intr_end (Mailbox Event)");
	}
	return (aen);
}


/*
 * Function name : isp_i_handle_arq()
 *
 * Description	 : called on an autorequest sense condition, sets up arqstat
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_i_handle_arq(struct isp *isp, struct isp_cmd *sp)
{
	struct isp_response *resp = &sp->cmd_isp_response;
	char status;
	struct scsi_pkt *pkt = CMD2PKT(sp);


	ASSERT(isp != NULL);

	if (sp->cmd_scblen >= sizeof (struct scsi_arq_status)) {
		struct scsi_arq_status *arqstat;

		ISP_DEBUG(isp, SCSI_DEBUG,
		    "tgt %d.%d: auto request sense", TGT(sp), LUN(sp));
		arqstat = (struct scsi_arq_status *)(pkt->pkt_scbp);
		status = pkt->pkt_scbp[0];
		bzero((caddr_t)arqstat, sizeof (struct scsi_arq_status));

		if (isp_debug_ars) {
			struct scsi_extended_sense	*s =
				(struct scsi_extended_sense *)
				resp->resp_request_sense;

			isp_i_log(isp, CE_NOTE,
			    "tgt %d.%d: ARS, stat=0x%x,key=0x%x,add=0x%x/0x%x",
			    TGT(sp), LUN(sp), status,
			    s->es_key, s->es_add_code, s->es_qual_code);
			isp_i_log(isp, CE_CONT,
			    "   cdb 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ...",
			    pkt->pkt_cdbp[0], pkt->pkt_cdbp[1],
			    pkt->pkt_cdbp[2], pkt->pkt_cdbp[3],
			    pkt->pkt_cdbp[4], pkt->pkt_cdbp[5]);
		}

		/*
		 * use same statistics as the original cmd
		 */
		arqstat->sts_rqpkt_statistics = pkt->pkt_statistics;
		arqstat->sts_rqpkt_state =
		    (STATE_GOT_BUS | STATE_GOT_TARGET |
		    STATE_SENT_CMD | STATE_XFERRED_DATA | STATE_GOT_STATUS);
		if (resp->resp_rqs_count <
		    sizeof (struct scsi_extended_sense)) {
			arqstat->sts_rqpkt_resid = (size_t)
				sizeof (struct scsi_extended_sense) -
				resp->resp_rqs_count;
		}
		bcopy((caddr_t)resp->resp_request_sense,
		    (caddr_t)&arqstat->sts_sensedata,
		    sizeof (struct scsi_extended_sense));
		/*
		 * restore status which was wiped out by bzero
		 */
		pkt->pkt_scbp[0] = status;
	} else {
		/*
		 * bad packet; can't copy over ARQ data
		 * XXX need CMD_BAD_PKT
		 */
		ISP_SET_REASON(sp, CMD_TRAN_ERR);
	}
}


/*
 * Function name : isp_i_find_free_slot()
 *
 * Return Values : empty slot  number
 *
 * Description	 : find an empty slot in the isp_slots list
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * called from isp_i_start_cmd() to find a slot for an untagged command
 * iff the "next" slot is already taken
 */
static int
isp_i_find_freeslot(struct isp *isp)
{
	char found = 0;
	int slot;
	int i;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_WAITQ_MUTEX(isp)));

	/*
	 * If slot in use, scan for a free one. Walk thru
	 * isp_slots, starting at current tag
	 * this should rarely happen.
	 */
	ISP_DEBUG2(isp, SCSI_DEBUG, "found in use slot %d",
	    isp->isp_free_slot);
	for (i = 0; i < (ISP_MAX_SLOTS - 1); i++) {
		slot = isp->isp_free_slot++;
		if (isp->isp_free_slot >= (ushort_t)ISP_MAX_SLOTS) {
			/* wrap around */
			isp->isp_free_slot = 0;
		}
		/* is this slot available ?? */
		if (isp->isp_slots[slot].slot_cmd == NULL) {
			/* yes, this slot is available */
			found = 1;
			break;
		}
		/* this slot busy */
		ISP_DEBUG2(isp, SCSI_DEBUG, "found in use slot %d", slot);
	}

	/* did we find a slot ?? */
	if (!found) {
		/*
		 * oh oh: big trouble -- no slot found!!!
		 *
		 * Note: this used to be a CE_PANIC, but that was kind of
		 * heavy-handed
		 */
		isp_i_log(isp, CE_WARN, "isp: no free slots!!");
		slot = -1;
	} else {
		ISP_DEBUG2(isp, SCSI_DEBUG, "found free slot %d", slot);
	}

	return (slot);
}


/*
 * Function name : isp_i_response_error()
 *
 * Return Values : ACTION_CONTINUE
 *		   ACTION_IGNORE
 *
 * Description	 : handle response packet error conditions
 *
 * Context	 : Called by interrupt thread
 */
static int
isp_i_response_error(struct isp *isp, struct isp_response *resp)
{
	struct isp_cmd *sp = ISP_LOOKUP_ID(resp->resp_token);
	int rval;

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESPONSE_ERROR_START,
	    "isp_i_response_error_start");
	/*
	 * we handle "queue full" (which we periodically expect),
	 * and everything else (which we don't)
	 */
	if (resp->resp_header.cq_flags & CQ_FLAG_FULL) {

		ISP_DEBUG(isp, SCSI_DEBUG,
		    "isp_i_response_error: queue full");

		/*
		 * Need to forge request sense of busy.
		 */
		resp->resp_scb = 0x10;		/* BUSY */
		resp->resp_rqs_count = 1;	/* cnt of request sense data */
		resp->resp_reason = CMD_CMPLT;
		resp->resp_state = 0;

		rval = ACTION_CONTINUE;
	} else {

		/*
		 * For bad request pkts, flag error and try again.
		 * This should *NEVER* happen.
		 */
		ISP_SET_REASON(sp, CMD_TRAN_ERR);
		if (resp->resp_header.cq_flags & CQ_FLAG_BADPACKET) {
			isp_i_log(isp, CE_WARN, "Bad request pkt");
		} else if (resp->resp_header.cq_flags & CQ_FLAG_BADHEADER) {
			isp_i_log(isp, CE_WARN, "Bad request pkt header");
		}
		rval = ACTION_CONTINUE;
	}

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESPONSE_ERROR_END,
	    "isp_i_response_error_end");

	return (rval);
}


/*
 * Function name : isp_i_polled_cmd_start()
 *
 * Return Values : TRAN_ACCEPT	if transaction was accepted
 *		   TRAN_BUSY	if I/O could not be started
 *		   TRAN_ACCEPT	if I/O timed out, pkt fields indicate error
 *
 * Description	 : Starts up I/O in normal fashion by calling isp_i_start_cmd().
 *		   Busy waits for I/O to complete or timeout.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_i_polled_cmd_start(struct isp *isp, struct isp_cmd *sp)
{
	int delay_loops;
	int rval;
	struct scsi_pkt *pkt = CMD2PKT(sp);

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RUN_POLLED_CMD_START,
	    "isp_i_polled_cmd_start_start");


	/*
	 * set timeout to SCSI_POLL_TIMEOUT for non-polling
	 * commands that do not have this field set
	 */
	if (pkt->pkt_time == 0) {
		pkt->pkt_time = SCSI_POLL_TIMEOUT;
	}


	/*
	 * try and start up command
	 */
	mutex_enter(ISP_REQ_MUTEX(isp));
	rval = isp_i_start_cmd(isp, sp);
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
	if (rval != TRAN_ACCEPT) {
		goto done;
	}

	/*
	 * busy wait for command to finish ie. till CFLAG_COMPLETED is set
	 */
	delay_loops = ISP_TIMEOUT_DELAY(
	    (pkt->pkt_time + (2 * ISP_GRACE)),
	    ISP_NOINTR_POLL_DELAY_TIME);

	ISP_DEBUG2(isp, SCSI_DEBUG,
	"delay_loops=%d, delay=%d, pkt_time=%x, cdb[0]=%x\n", delay_loops,
	ISP_NOINTR_POLL_DELAY_TIME, pkt->pkt_time,
	*(sp->cmd_pkt->pkt_cdbp));

	while ((sp->cmd_flags & CFLAG_COMPLETED) == 0) {
		drv_usecwait(ISP_NOINTR_POLL_DELAY_TIME);


		if (--delay_loops <= 0) {
			struct isp_response *resp;

			/*
			 * Call isp_scsi_abort()  to abort the I/O
			 * and if isp_scsi_abort fails then
			 * blow everything away
			 */
			if ((isp_scsi_reset(&pkt->pkt_address, RESET_TARGET))
				== FALSE) {
				mutex_enter(ISP_RESP_MUTEX(isp));
				isp_i_fatal_error(isp, 0);
				mutex_exit(ISP_RESP_MUTEX(isp));
			}

			resp = &sp->cmd_isp_response;
			bzero((caddr_t)resp, sizeof (struct isp_response));

			/*
			 * update stats in resp_status_flags
			 * isp_i_call_pkt_comp() copies this
			 * over to pkt_statistics
			 */
			resp->resp_status_flags |=
			    STAT_BUS_RESET | STAT_TIMEOUT;
			resp->resp_reason = CMD_TIMEOUT;
#ifdef ISPDEBUG
			sp->cmd_flags |= CFLAG_FINISHED;
#endif
			isp_i_call_pkt_comp(sp);
			break;
		}
		/*
		 * This call is required to handle the cases when
		 * isp_intr->isp_i_call_pkt_comp->sdintr->
		 * sd_handle_autosense->sd_decode_sense->sd_handle_ua
		 * sd_handle_mchange->scsi_poll. You will be in the intr.
		 */
		if (ISP_INT_PENDING(isp)) {
			(void) isp_intr((caddr_t)isp);
		}

	}
	ISP_DEBUG2(isp, SCSI_DEBUG, "polled cmd done\n");

done:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RUN_POLLED_CMD_END,
	    "isp_i_polled_cmd_start_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	return (rval);
}


/*
 * Function name : isp_i_async_event
 *
 * Return Values : -1	Fatal error occurred
 *		    0	normal return
 * Description	 :
 * An Event of 8002 is a Sys Err in the ISP.  This would require
 *	Chip reset.
 *
 * An Event of 8001 is a external SCSI Bus Reset
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * called w/both mutexes held from interrupt context
 */
static int
isp_i_async_event(struct isp *isp, short event)
{
	int rval = ISP_AEN_SUCCESS;
	int target_lun, target;

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	TRACE_1(TR_FAC_SCSI_ISP, TR_ISP_I_ASYNCH_EVENT_START,
	    "isp_i_async_event_start(event = %d)", event);


	switch (ISP_GET_MBOX_EVENT(event)) {
	case ISP_MBOX_ASYNC_ERR:
		/*
		 * Force the current commands to timeout after
		 * resetting the chip.
		 */
		isp_i_log(isp, CE_WARN, "SCSI Cable/Connection problem.");
		isp_i_log(isp, CE_CONT, "Hardware/Firmware error.");
		ISP_DEBUG(isp, SCSI_DEBUG, "Mbx1/PanicAddr=0x%x, Mbx2=0x%x",
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox1),
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox2));
		ISP_CLEAR_RISC_INT(isp);
		ISP_CLEAR_SEMAPHORE_LOCK(isp);
		mutex_exit(ISP_REQ_MUTEX(isp));
		isp_i_fatal_error(isp,
		    ISP_FIRMWARE_ERROR | ISP_DOWNLOAD_FW_ON_ERR |
		    ISP_FROM_INTR);
		mutex_enter(ISP_REQ_MUTEX(isp));
		rval = ISP_AEN_RESET;
		break;

	case ISP_MBOX_ASYNC_REQ_DMA_ERR:
	case ISP_MBOX_ASYNC_RESP_DMA_ERR:
		/*
		 *  DMA failed in the ISP chip force a Reset
		 */
		isp_i_log(isp, CE_WARN, "DMA Failure (%x)", event);
		ISP_CLEAR_RISC_INT(isp);
		ISP_CLEAR_SEMAPHORE_LOCK(isp);
		mutex_exit(ISP_REQ_MUTEX(isp));
		isp_i_fatal_error(isp, ISP_FORCE_RESET_BUS | ISP_FROM_INTR);
		mutex_enter(ISP_REQ_MUTEX(isp));
		rval = ISP_AEN_RESET;
		break;

	case ISP_MBOX_ASYNC_RESET:
		isp_i_log(isp, CE_WARN, "Received unexpected SCSI Reset");
		/* FALLTHROUGH */
	case ISP_MBOX_ASYNC_OVR_RESET:
		/* FALLTHROUGH */
	case ISP_MBOX_ASYNC_INT_RESET:
		/*
		 * Set a marker to for a internal SCSI BUS reset done
		 * to recover from a timeout.
		 */
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "ISP initiated SCSI BUS Reset or external SCSI Reset");
		ISP_MUTEX_EXIT(isp);
		if (isp_i_set_marker(isp, SYNCHRONIZE_ALL, 0, 0)) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for all targets (async)");
		}
		ISP_MUTEX_ENTER(isp);
		ISP_CLEAR_RISC_INT(isp);
		ISP_CLEAR_SEMAPHORE_LOCK(isp);

		mutex_exit(ISP_REQ_MUTEX(isp));
		(void) scsi_hba_reset_notify_callback(ISP_RESP_MUTEX(isp),
			&isp->isp_reset_notify_listf);
		mutex_enter(ISP_REQ_MUTEX(isp));
		break;

	case ISP_MBOX_ASYNC_INT_DEV_RESET:
		/* Get the target an lun value */
		target_lun = ISP_READ_MBOX_REG(isp,
			&isp->isp_mbox_reg->isp_mailbox1);
		target = (target_lun >> 8) & 0xff;
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "ISP initiated SCSI Device Reset (reason timeout?)");
		ISP_MUTEX_EXIT(isp);
		/* Post the Marker to synchrnise the target */
		if (isp_i_set_marker(isp, SYNCHRONIZE_TARGET, target, 0)) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for target %d (async)",
			    target);
		}
		ISP_MUTEX_ENTER(isp);
		ISP_CLEAR_RISC_INT(isp);
		ISP_CLEAR_SEMAPHORE_LOCK(isp);
		break; /* Leave holding mutex */

	default:
		isp_i_log(isp, CE_NOTE,
		    "mailbox regs(0-5): 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		    event,
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox1),
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox2),
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox3),
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox4),
		    ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox5));
		ISP_CLEAR_RISC_INT(isp);
		ISP_CLEAR_SEMAPHORE_LOCK(isp);
		break;
	}


	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_ASYNCH_EVENT_END,
	    "isp_i_async_event_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	return (rval);
}

/*
 * Function name : isp_i_mbox_cmd_complete ()
 *
 * Return Values : None.
 *
 * Description	 : Sets ISP_MBOX_CMD_FLAGS_COMPLETE flag so busy wait loop
 *		   in isp_i_mbox_cmd_start() exits.
 *
 * Context	 : Can be called by interrupt thread only.
 *
 * Semaphores	: might have both req/resp locks or neither
 */
static void
isp_i_mbox_cmd_complete(struct isp *isp)
{
	uint16_t *mbox_regp;
	int delay_loops;
	uchar_t i;


	ASSERT(isp != NULL);

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_MBOX_CMD_COMPLETE_START,
	    "isp_i_mbox_cmd_complete_start");

	mbox_regp = &isp->isp_mbox_reg->isp_mailbox0;

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_mbox_cmd_complete_start(cmd = 0x%x)",
	    isp->isp_mbox.mbox_cmd.mbox_out[0]);

	/*
	 * Check for completions that are caused by mailbox events
	 * but that do not set the mailbox status bit ie. 0x4xxx
	 * For now only the busy condition is checked, the others
	 * will automatically time out and error.
	 */
	delay_loops = ISP_TIMEOUT_DELAY(ISP_MBOX_CMD_BUSY_WAIT_TIME,
	    ISP_MBOX_CMD_BUSY_POLL_DELAY_TIME);
	while (ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox0) ==
		ISP_MBOX_BUSY) {
		drv_usecwait(ISP_MBOX_CMD_BUSY_POLL_DELAY_TIME);
		if (--delay_loops < 0) {
			isp->isp_mbox.mbox_cmd.mbox_in[0] =
			    ISP_MBOX_STATUS_FIRMWARE_ERR;
			goto fail;
		}
	}



	/*
	 * save away status registers
	 */
	for (i = 0; i < ISP_MAX_MBOX_REGS; i++, mbox_regp++) {
		isp->isp_mbox.mbox_cmd.mbox_in[i] =
			ISP_READ_MBOX_REG(isp, mbox_regp);
	}

fail:
	/*
	 * set flag so that busy wait loop will detect this and return
	 */
	isp->isp_mbox.mbox_flags |= ISP_MBOX_CMD_FLAGS_COMPLETE;
	/*
	 * clear the risc interrupt only no more interrupt are pending
	 * We do not need isp_response_mutex because of isp semaphore
	 * lock is held.
	 */
	_NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(isp->isp_response_out))
	if (((ISP_GET_RESPONSE_IN(isp) - isp->isp_response_out) == 0) ||
		(isp->isp_mbox.mbox_flags & ISP_MBOX_CMD_FLAGS_Q_NOT_INIT) ||
			(isp->isp_mbox.mbox_cmd.mbox_in[0] ==
				ISP_MBOX_STATUS_FIRMWARE_ERR)) {
		ISP_CLEAR_RISC_INT(isp);
	}
	_NOTE(NOW_VISIBLE_TO_OTHER_THREADS(isp->isp_response_out))

	/*
	 * clear the semaphore lock
	 */
	ISP_CLEAR_SEMAPHORE_LOCK(isp);

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_MBOX_CMD_COMPLETE_END,
	    "isp_i_mbox_cmd_complete_end");

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_mbox_cmd_complete_end (cmd = 0x%x)",
	    isp->isp_mbox.mbox_cmd.mbox_out[0]);
}


/*
 * Function name : isp_i_mbox_cmd_init()
 *
 * Return Values : none
 *
 * Description	 : initializes mailbox command
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static void
isp_i_mbox_cmd_init(struct isp *isp, struct isp_mbox_cmd *mbox_cmdp,
    uchar_t n_mbox_out, uchar_t n_mbox_in,
    ushort_t reg0, ushort_t reg1, ushort_t reg2,
    ushort_t reg3, ushort_t reg4, ushort_t reg5)
{

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_mbox_cmd_init r0 = 0x%x r1 = 0x%x r2 = 0x%x",
	    reg0, reg1, reg2);
	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "			 r3 = 0x%x r4 = 0x%x r5 = 0x%x",
	    reg3, reg4, reg5);

	mbox_cmdp->timeout	= ISP_MBOX_CMD_TIMEOUT;
	mbox_cmdp->retry_cnt	= ISP_MBOX_CMD_RETRY_CNT;
	mbox_cmdp->n_mbox_out	= n_mbox_out;
	mbox_cmdp->n_mbox_in	= n_mbox_in;
	mbox_cmdp->mbox_out[0]	= n_mbox_out >= 1 ? reg0 : 0;
	mbox_cmdp->mbox_out[1]	= n_mbox_out >= 2 ? reg1 : 0;
	mbox_cmdp->mbox_out[2]	= n_mbox_out >= 3 ? reg2 : 0;
	mbox_cmdp->mbox_out[3]	= n_mbox_out >= 4 ? reg3 : 0;
	mbox_cmdp->mbox_out[4]	= n_mbox_out >= 5 ? reg4 : 0;
	mbox_cmdp->mbox_out[5]	= n_mbox_out >= 6 ? reg5 : 0;
}


/*
 * Function name : isp_i_mbox_cmd_start()
 *
 * Return Values : 0   if no error
 *		   -1  on error
 *		   Status registers are returned in structure that is passed in.
 *
 * Description	 : Sends mailbox cmd to ISP and busy waits for completion.
 *		   Serializes accesses to the mailboxes.
 *		   Mailbox cmds are used to initialize the ISP, modify default
 *			parameters, and load new firmware.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *		   Requires both the request and response mutex held on entry.
 *		   No other mutexes may be held across this function 8^(.
 *
 */
static int
isp_i_mbox_cmd_start(struct isp *isp, struct isp_mbox_cmd *mbox_cmdp)
{
	ushort_t *mbox_regp;
	char retry_cnt = (char)mbox_cmdp->retry_cnt;
	int delay_loops, rval = 0;
	uchar_t i;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_MBOX_CMD_START_START,
	    "isp_i_mbox_cmd_start_start");

	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_i_mbox_cmd_start_start(cmd = 0x%x)",
	    mbox_cmdp->mbox_out[0]);

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	/*
	 * allow only one thread to access mailboxes
	 * (release mutexes before the sema_p to avoid a deadlock when
	 * another thread needs to do a mailbox cmd and this thread needs
	 * to reenter the mutex again (see below) while holding the semaphore
	 */
	ISP_MUTEX_EXIT(isp);
	sema_p(ISP_MBOX_SEMA(isp));
	ISP_MUTEX_ENTER(isp);

	/*
	 * while waiting for semaphore shutdown flag may get set.
	 * If shutdown flag is set return -1
	 */
	if (isp->isp_shutdown) {
		rval = -1;
		sema_v(ISP_MBOX_SEMA(isp));
		return (rval);
	}

	/* save away mailbox command */
	bcopy((caddr_t)mbox_cmdp, (caddr_t)&isp->isp_mbox.mbox_cmd,
	    sizeof (struct isp_mbox_cmd));

retry:
	mbox_regp = (ushort_t *)&isp->isp_mbox_reg->isp_mailbox0;

	/* write outgoing registers */
	for (i = 0; i < isp->isp_mbox.mbox_cmd.n_mbox_out; i++, mbox_regp++) {
		ISP_WRITE_MBOX_REG(isp, mbox_regp,
			isp->isp_mbox.mbox_cmd.mbox_out[i]);
	}

#ifdef ISP_PERF
	isp->isp_wpio_count += isp->isp_mbox.mbox_cmd.n_mbox_out;
	isp->isp_mail_requests++;
#endif /* ISP_PERF */

	/*
	 * Turn completed flag off indicating mbox command was issued.
	 * Interrupt handler will set flag when done.
	 */
	isp->isp_mbox.mbox_flags &= ~ISP_MBOX_CMD_FLAGS_COMPLETE;

	/* signal isp that mailbox cmd was loaded */
	ISP_REG_SET_HOST_INT(isp);

	/* busy wait for mailbox cmd to be processed. */
	delay_loops = ISP_TIMEOUT_DELAY(mbox_cmdp->timeout,
	    ISP_MBOX_CMD_BUSY_POLL_DELAY_TIME);

	/*
	 * release mutexes, we are now protected by the sema and we don't
	 * want to deadlock with isp_intr()
	 */
	ISP_MUTEX_EXIT(isp);

	while ((isp->isp_mbox.mbox_flags & ISP_MBOX_CMD_FLAGS_COMPLETE) == 0) {

		drv_usecwait(ISP_MBOX_CMD_BUSY_POLL_DELAY_TIME);
		/* if cmd does not complete retry or return error */
		if (--delay_loops <= 0) {
			if (--retry_cnt <= 0) {
				rval = -1;
				goto done;
			} else {
				ISP_MUTEX_ENTER(isp);
				goto retry;
			}
		}

		ISP_MUTEX_ENTER(isp);
		if (ISP_CHECK_SEMAPHORE_LOCK(isp)) {
			ushort_t event = ISP_READ_MBOX_REG(isp,
				&isp->isp_mbox_reg->isp_mailbox0);
#ifdef ISP_PERF
			isp->isp_rpio_count += 1;
#endif
			if (event & ISP_MBOX_EVENT_ASYNCH) {
				/*
				 * if an async event occurs during
				 * fatal error recovery, we are hosed
				 * with a recursive mutex panic
				 */
				switch (ISP_GET_MBOX_EVENT(event)) {
				case ISP_MBOX_ASYNC_ERR:
				case ISP_MBOX_ASYNC_REQ_DMA_ERR:
				case ISP_MBOX_ASYNC_RESP_DMA_ERR:
					sema_v(ISP_MBOX_SEMA(isp));
					break;
				}
				if (isp_i_async_event(isp, event) ==
				    ISP_AEN_RESET) {
					/*
					 * Do not relase the mutexs as
					 * the calling funtion is holding
					 */
					rval = -1;
					return (rval);
				}
			} else {
				isp_i_mbox_cmd_complete(isp);
			}
		}
		ISP_MUTEX_EXIT(isp);
	}

	/*
	 * copy registers saved by isp_i_mbox_cmd_complete()
	 * to mbox_cmdp
	 */
	for (i = 0; i < ISP_MAX_MBOX_REGS; i++) {
	    mbox_cmdp->mbox_in[i] = isp->isp_mbox.mbox_cmd.mbox_in[i];
	}

	if ((mbox_cmdp->mbox_in[0] & ISP_MBOX_STATUS_MASK) !=
				ISP_MBOX_STATUS_OK) {
		rval = 1;
	}

#ifdef ISP_PERF
	isp->isp_rpio_count += isp->isp_mbox.mbox_cmd.n_mbox_in;
#endif

done:
	if ((rval != 0) || isp_debug_mbox) {
		ISP_DEBUG(isp, SCSI_DEBUG, "mbox cmd %s:",
		    (rval ? "failed" : "succeeded"));
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "cmd= 0x%x; 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		    mbox_cmdp->mbox_out[0], mbox_cmdp->mbox_out[1],
		    mbox_cmdp->mbox_out[2], mbox_cmdp->mbox_out[3],
		    mbox_cmdp->mbox_out[4], mbox_cmdp->mbox_out[5]);
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "status= 0x%x; 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		    mbox_cmdp->mbox_in[0], mbox_cmdp->mbox_in[1],
		    mbox_cmdp->mbox_in[2], mbox_cmdp->mbox_in[3],
		    mbox_cmdp->mbox_in[4], mbox_cmdp->mbox_in[5]);
	} else {
		ISP_DEBUG2(isp, SCSI_DEBUG, "mbox cmd succeeded:");
		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "cmd= 0x%x; 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		    mbox_cmdp->mbox_out[0], mbox_cmdp->mbox_out[1],
		    mbox_cmdp->mbox_out[2], mbox_cmdp->mbox_out[3],
		    mbox_cmdp->mbox_out[4], mbox_cmdp->mbox_out[5]);
		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "status= 0x%x; 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		    mbox_cmdp->mbox_in[0], mbox_cmdp->mbox_in[1],
		    mbox_cmdp->mbox_in[2], mbox_cmdp->mbox_in[3],
		    mbox_cmdp->mbox_in[4], mbox_cmdp->mbox_in[5]);
	}

	ISP_MUTEX_ENTER(isp);

	sema_v(ISP_MBOX_SEMA(isp));

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_MBOX_CMD_START_END,
	    "isp_i_mbox_cmd_start_end");

	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_i_mbox_cmd_start_end (cmd = 0x%x)",
	    mbox_cmdp->mbox_out[0]);

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	return (rval);
}


/*
 * Function name : isp_i_watch()
 *
 * Return Values : none
 * Description	 :
 * Isp deadman timer routine.
 * A hung isp controller is detected by failure to complete
 * cmds within a timeout interval (including grace period for
 * isp error recovery).	 All target error recovery is handled
 * directly by the isp.
 *
 * If isp hung, restart by resetting the isp and flushing the
 * crash protection queue (isp_slots) via isp_i_qflush.
 *
 * we check only 1/8 of the slots at the time; this is really only a sanity
 * check on isp so we know when it dropped a packet. The isp performs
 * timeout checking and recovery on the target
 * If the isp dropped a packet then this is a fatal error
 *
 * if lbolt wraps around then those packets will never timeout; there
 * is small risk of a hang in this short time frame. It is cheaper though
 * to ignore this problem since this is an extremely unlikely event
 *
 * Context	 : Can be called by timeout thread.
 */
#ifdef ISPDEBUG
static int isp_test_abort;
static int isp_test_abort_all;
static int isp_test_reset;
static int isp_test_reset_all;
static int isp_test_fatal;
static int isp_debug_enter;
static int isp_debug_enter_count;
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_test_abort))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_test_abort_all))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_test_reset))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_test_reset_all))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_test_fatal))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_debug_enter))
_NOTE(MUTEX_PROTECTS_DATA(isp::isp_response_mutex, isp_debug_enter_count))
#endif


_NOTE(READ_ONLY_DATA(isp::isp_next isp_head))

static void
isp_i_watch()
{
	struct isp *isp = isp_head;
#ifdef OLDTIMEOUT
	clock_t local_lbolt;

	(void) drv_getparm(LBOLT, &local_lbolt);
#endif

	rw_enter(&isp_global_rwlock, RW_READER);
	for (isp = isp_head; isp != NULL; isp = isp->isp_next) {
#ifdef OLDTIMEOUT
		isp_i_old_watch_isp(isp, local_lbolt);
#else
		if (isp->isp_shutdown) {
			continue;
		}
		isp_i_watch_isp(isp);
#endif
	}
	rw_exit(&isp_global_rwlock);

	mutex_enter(&isp_global_mutex);
	/*
	 * If timeout_initted has been cleared then somebody
	 * is trying to untimeout() this thread so no point in
	 * reissuing another timeout.
	 */
	if (timeout_initted) {
		ASSERT(isp_timeout_id);
		isp_timeout_id = timeout((void (*)(void *))isp_i_watch,
			(caddr_t)isp, isp_tick);
	}
	mutex_exit(&isp_global_mutex);
}


#ifdef OLDTIMEOUT

static void
isp_i_old_watch_isp(struct isp *isp, clock_t local_lbolt)
{
	ushort_t slot;
	ushort_t last_slot, max_slot;
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_WATCH_START, "isp_i_watch_start");


#ifdef ISP_PERF
	isp->isp_perf_ticks += isp_scsi_watchdog_tick;

	if (isp->isp_request_count >= 20000) {
		isp_i_log(isp, CE_NOTE,
	"%d reqs/sec (ticks=%d, intr=%d, req=%d, rpio=%d, wpio=%d)",
		    isp->isp_request_count/isp->isp_perf_ticks,
		    isp->isp_perf_ticks,
		    isp->isp_intr_count, isp->isp_request_count,
		    isp->isp_rpio_count, isp->isp_wpio_count);

		isp->isp_request_count = isp->isp_perf_ticks = 0;
		isp->isp_intr_count = 0;
		isp->isp_rpio_count = isp->isp_wpio_count = 0;
	}
#endif	/* ISP_PERF */

	mutex_enter(ISP_RESP_MUTEX(isp));

	last_slot = isp->isp_last_slot_watched;
	max_slot = ISP_MAX_SLOTS;

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "isp_i_watch: lbolt=%d, start_slot=0x%x, max_slot=0x%x\n",
	    local_lbolt, last_slot, max_slot);

	for (slot = last_slot; slot < max_slot; slot++) {
		struct isp_cmd *sp = isp->isp_slots[slot].slot_cmd;
		struct scsi_pkt *pkt = (sp != NULL)? CMD2PKT(sp) : NULL;
#ifdef ISPDEBUG

		/*
		 * This routine will return with holding ISP_RESP_MUTEX
		 */
		if (sp) {
			isp_i_test(isp, sp);
		}
#endif	/* ISPDEBUG */

		ASSERT(slot < (ushort_t)ISP_MAX_SLOTS);

		if (isp->isp_slots[slot].slot_cmd &&
		    (clock_t)isp->isp_slots[slot].slot_deadline -
		    local_lbolt <= 0) {
			struct isp_cmd *sp =
			    isp->isp_slots[slot].slot_cmd;

			if (pkt->pkt_time) {
#ifdef ISPDEBUG
				uchar_t *cp;
				char buf[128];
				int i;

				ISP_DEBUG(isp, SCSI_DEBUG,
				    "deadline=%x, local_lbolt=%x pkt_time=%x",
				    isp->isp_slots[slot].slot_deadline,
				    local_lbolt, pkt->pkt_time);
				ISP_DEBUG(isp, SCSI_DEBUG,
				"tgt %d.%d: sp=%x, pkt_flags=%x",
				TGT(sp), LUN(sp), sp, pkt->pkt_flags);

				cp = (uchar_t *)pkt->pkt_cdbp;
				buf[0] = '\0';
				for (i = 0; i < (int)sp->cmd_cdblen; i++) {
					(void) sprintf(
					    &buf[strlen(buf)], " 0x%x", *cp++);
					if (strlen(buf) > 124) {
						break;
					}
				}
				ISP_DEBUG(isp, SCSI_DEBUG,
				    "\tcflags=%x, cdb dump: %s",
				    sp->cmd_flags, buf);
#endif	/* ISPDEBUG */

				if (isp_i_is_fatal_timeout(isp, sp)) {
					isp_i_log(isp, CE_WARN,
					    "Fatal timeout on target %d.%d",
					    TGT(sp), LUN(sp));
					isp_i_fatal_error(isp,
						ISP_FORCE_RESET_BUS);
					break;
				}
			}
		}
	}
#ifdef ISPDEBUG
	isp_test_abort = 0;
	if (isp_debug_enter && isp_debug_enter_count) {
		debug_enter("isp_i_watch");
		isp_debug_enter = 0;
	}
#endif
	last_slot = max_slot;
	if (last_slot >= (ushort_t)ISP_MAX_SLOTS) {
		last_slot = 0;
	}
	isp->isp_last_slot_watched = last_slot;
	mutex_exit(ISP_RESP_MUTEX(isp));

	/*
	 * Paranoia: just in case we haven`t emptied the waitQ, empty it
	 * here
	 */
	if (mutex_tryenter(ISP_REQ_MUTEX(isp))) {
		ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_WATCH_END, "isp_i_watch_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
}


/*
 * Function name : isp_i_is_fatal_timeout()
 *
 * Return Values : 1 - is really a fatal timeout
 *		   0 - not a fatal timeout yet
 *
 * Description	 : verifies whether the sp has the earliest starting time
 *		   this will give the isp some more time but it is
 *		   still impossible to figure out when exactly a cmd
 *		   is started on the bus
 *
 * Context	 : called from isp_i_watch_isp
 */
static int
isp_i_is_fatal_timeout(struct isp *isp, struct isp_cmd *sp)
{
	ushort_t slot;
	ushort_t max_slot = ISP_MAX_SLOTS;
	clock_t start_time = sp->cmd_start_time;

	ISP_DEBUG(isp, SCSI_DEBUG,
	    "fatal timeout check for %x, start_time=%x, lbolt=%x",
	    sp, start_time, lbolt);

	/*
	 * now go thru all slots and find an earlier starting time
	 * for the same scsi address
	 * if so, then that cmd should timeout first
	 */
	for (slot = 0; slot < max_slot; slot++) {
		struct isp_cmd *ssp = isp->isp_slots[slot].slot_cmd;

		if (ssp) {
			ISP_DEBUG(isp, SCSI_DEBUG,
			"checking %x, start_time=%x, tgt=%x",
			ssp, ssp->cmd_start_time, TGT(ssp));
		}

		if (ssp && (ssp->cmd_start_time < start_time) &&
		    (bcmp((char *)&(CMD2PKT(ssp)->pkt_address),
			(char *)&CMD2PKT(sp)->pkt_address,
			sizeof (struct scsi_address)) == 0)) {
			start_time = ssp->cmd_start_time;
			ISP_DEBUG(isp, SCSI_DEBUG,
			"found older cmd=%x, start_time=%x, timeout=%x",
			    ssp, start_time, CMD2PKT(ssp)->pkt_time);
			return (0);
		}
	}

	/*
	 * we didn't find an older cmd, so we have a real timeout
	 */
	return (1);
}

#else	/* OLDTIMEOUT */

/*
 * called from isp_i_watch(), which is called from watch thread
 */
static void
isp_i_watch_isp(struct isp *isp)
{
	int slot;


	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_WATCH_START, "isp_i_watch_start");

#ifdef ISPDEBUG
	if (isp_watch_disable)
		return;
#endif

#ifdef ISP_PERF
	isp->isp_perf_ticks += isp_scsi_watchdog_tick;

	if (isp->isp_request_count >= 20000) {
		isp_i_log(isp, CE_NOTE,
	"%d reqs/sec (ticks=%d, intr=%d, req=%d, rpio=%d, wpio=%d)",
		    isp->isp_request_count/isp->isp_perf_ticks,
		    isp->isp_perf_ticks,
		    isp->isp_intr_count, isp->isp_request_count,
		    isp->isp_rpio_count, isp->isp_wpio_count);

		isp->isp_request_count = isp->isp_perf_ticks = 0;
		isp->isp_intr_count = 0;
		isp->isp_rpio_count = isp->isp_wpio_count = 0;
	}
#endif /* ISP_PERF */

	if (!(isp->isp_alive) || !(ISP_INT_PENDING(isp))) {
		if (!isp_i_alive(isp)) {
			isp_i_log(isp, CE_WARN, "ISP: Firmware cmd timeout");
			mutex_enter(ISP_RESP_MUTEX(isp));
			isp_i_fatal_error(isp, ISP_FORCE_RESET_BUS);
			mutex_exit(ISP_RESP_MUTEX(isp));
		}
	}

	isp->isp_alive = 0;

#ifdef ISPDEBUG
	mutex_enter(ISP_RESP_MUTEX(isp));
	for (slot = 0; slot < ISP_MAX_SLOTS; slot++) {
		struct isp_cmd *sp = isp->isp_slots[slot].slot_cmd;

		if (sp) {
			isp_i_test(isp, sp);
			break;
		}
	}
	isp_test_abort = 0;
	if (isp_debug_enter && isp_debug_enter_count) {
		debug_enter("isp_i_watch");
		isp_debug_enter = 0;
	}
	mutex_exit(ISP_RESP_MUTEX(isp));
#endif	/* ISPDEBUG */

	if (isp->isp_prop_update) {
		int i;

		ISP_MUTEX_ENTER(isp);

		for (i = 0; i < NTARGETS_WIDE; i++) {
			if (isp->isp_prop_update & (1 << i)) {
				isp_i_update_props(isp, i, isp->isp_cap[i],
					isp->isp_synch[i]);
			}
		}
		isp->isp_prop_update = 0;
		ISP_MUTEX_EXIT(isp);
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_WATCH_END, "isp_i_watch_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
}

#endif	/* OLDTIMEOUT */


/*
 * Function name : isp_i_fatal_error()
 *
 * Return Values :  none
 *
 * Description	 :
 * Isp fatal error recovery:
 * Reset the isp and flush the active queues and attempt restart.
 * This should only happen in case of a firmware bug or hardware
 * death.  Flushing is from backup queue as ISP cannot be trusted.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * called owning response mutex and not owning request mutex
 */
static void
isp_i_fatal_error(struct isp *isp, int flags)
{
	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());

	/*
	 * If shutdown flag is set than no need to do
	 * fatal error recovery.
	 */
	if (isp->isp_shutdown) {
		return;
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_TIMEOUT_START,
	    "isp_i_fatal_error_start");

	isp_i_log(isp, CE_WARN, "Fatal error, resetting interface");

	/*
	 * hold off starting new requests by grabbing the request
	 * mutex
	 */
	mutex_enter(ISP_REQ_MUTEX(isp));

	if (isp_state_debug) {
		isp_i_print_state(CE_NOTE, isp);
	}

#ifdef ISPDEBUG
	if (isp_enable_brk_fatal) {
		char buf[128];
		char path[128];
		(void) sprintf(buf,
		"isp_i_fatal_error: You can now look at %s",
		    ddi_pathname(isp->isp_dip, path));
		debug_enter(buf);
	}
#endif

	(void) isp_i_reset_interface(isp, flags);

	mutex_exit(ISP_REQ_MUTEX(isp));
	(void) scsi_hba_reset_notify_callback(ISP_RESP_MUTEX(isp),
	    &isp->isp_reset_notify_listf);
	mutex_enter(ISP_REQ_MUTEX(isp));

	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_TIMEOUT_END, "isp_i_fatal_error_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
}


/*
 * Function name : isp_i_qflush()
 *
 * Return Values : none
 * Description	 :
 *	Flush isp queues  over range specified
 *	from start_tgt to end_tgt.  Flushing goes from oldest to newest
 *	to preserve some cmd ordering.
 *	This is used for isp crash recovery as normally isp takes
 *	care of target or bus problems.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * Note: always called for all targets
 */
static void
isp_i_qflush(struct isp *isp, ushort_t start_tgt, ushort_t end_tgt)
{
	struct isp_cmd *sp;
	struct isp_cmd *head, *tail;
	short slot;
	int i, n = 0;
	struct isp_response *resp;

	ASSERT(start_tgt <= end_tgt);
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(!mutex_owned(ISP_WAITQ_MUTEX(isp)));

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_QFLUSH_START,
	    "isp_i_qflush_start");

	ISP_DEBUG(isp, SCSI_DEBUG,
	    "isp_i_qflush: range= %d-%d", start_tgt, end_tgt);

	/*
	 * Flush specified range of active queue entries
	 * (e.g. target nexus).
	 * we allow for just flushing one target, ie start_tgt == end_tgt
	 */
	head = tail = NULL;

	/*
	 * If flushing active queue, start with current free slot
	 * ie. oldest request, to preserve some order.
	 */
	mutex_enter(ISP_WAITQ_MUTEX(isp));
	slot = isp->isp_free_slot;
	mutex_exit(ISP_WAITQ_MUTEX(isp));

	for (i = 0; i < ISP_MAX_SLOTS; i++) {

		sp = isp->isp_slots[slot].slot_cmd;
		if (sp &&
		    (TGT(sp) >= start_tgt) &&
		    (TGT(sp) <= end_tgt)) {
			isp->isp_slots[slot].slot_cmd = NULL;
			resp = &sp->cmd_isp_response;
			bzero((caddr_t)resp,
			    sizeof (struct isp_response));

			/*
			 * update stats in resp_status_flags
			 * isp_i_call_pkt_comp() copies this
			 * over to pkt_statistics
			 */
			resp->resp_status_flags = STAT_BUS_RESET | STAT_ABORTED;
			resp->resp_reason = CMD_RESET;
#ifdef ISPDEBUG
			sp->cmd_flags |= CFLAG_FINISHED;
#endif
			/*
			 * queue up sp
			 * we don't want to do a callback yet
			 * until we have flushed everything and
			 * can release the mutexes
			 */
			n++;
			if (head != NULL) {
				tail->cmd_forw = sp;
				tail = sp;
				tail->cmd_forw = NULL;
			} else {
				tail = head = sp;
				sp->cmd_forw = NULL;
			}
		}

		/*
		 * Wraparound
		 */
		if (++slot >= ISP_MAX_SLOTS) {
			slot = 0;
		}
	}

	/*
	 * XXX we don't worry about the waitQ since we cannot
	 * guarantee order anyway.
	 */
	if (head != NULL) {
		/*
		 * if we would	hold the REQ mutex and	the target driver
		 * decides to do a scsi_reset(), we will get a recursive
		 * mutex failure in isp_i_set_marker
		 */
		ISP_DEBUG(isp, SCSI_DEBUG, "isp_i_qflush: %d flushed", n);
		ISP_MUTEX_EXIT(isp);
		isp_i_call_pkt_comp(head);
		ISP_MUTEX_ENTER(isp);
	}

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_QFLUSH_END,
	    "isp_i_qflush_end");

	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
}


/*
 * Function name : isp_i_set_marker()
 *
 * Return Values : none
 * Description	 :
 * Send marker request to unlock the request queue for a given target/lun
 * nexus.
 *
 * If no marker can be sent (which means no request queue space is available)
 * then keep track of the fact for later and
 * return failure -- isp_i_update_queue_space() will send the marker when
 * it finds more space
 *
 * XXX: Right now this routine expects to be called without the request
 * mutext being held, but then it acquires it.  But in every place its
 * called, the request mutex is alredy held (usually the response mutex
 * is as well).  So this routine should change to expect the request
 * mutex to already be held, and all code calling it should change
 * correspondingly -- this would remove holes where we release the
 * request mutex and call this routine, but then somebody else adds to
 * the request queue before we acquire the mutex again here
 *
 * but part of the problem is that, right now, the last thing this routine
 * does is to move any entries on the waitQ to the request Q and then
 * release the request Q mutex (in a macro), so that would have to change
 * as well, and may have far-reaching consequences
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * called with neither mutex held
 */
/*ARGSUSED*/
static int
isp_i_set_marker(struct isp *isp, short mod, short tgt, short lun)
{

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_SET_MARKER_START,
	    "isp_i_set_marker_start");

	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	mutex_enter(ISP_REQ_MUTEX(isp));

	/*
	 * Check to see how much space is available in the
	 * Request Queue, save this so we do not have to do
	 * a lot of PIOs
	 */
	if (isp->isp_queue_space == 0) {
		isp_i_update_queue_space(isp);

		/*
		 * Check now to see if the queue is still full
		 */
		if (isp->isp_queue_space == 0) {
			/*
			 * oh oh -- still no space for the marker -- keep
			 * track of that for later
			 */
			isp_i_add_marker_to_list(isp, mod, tgt, lun);

			/*
			 * release request mutex and return failure
			 */
			mutex_exit(ISP_REQ_MUTEX(isp));
			return (-1);
		}
	}

	/*
	 * call helper routine to do work of sending marker
	 * (must hold reqQ mutex)
	 */
	isp_i_send_marker(isp, mod, tgt, lun);

	/* move any packets that've been point on the wait Q onto the req Q */
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_SET_MARKER_END,
	    "isp_i_set_marker_end");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	return (0);
}


/*
 * Function name : isp_scsi_abort()
 *
 * Return Values : FALSE	- abort failed
 *		   TRUE		- abort succeeded
 * Description	 :
 * SCSA interface routine to abort pkt(s) in progress.
 * Abort the pkt specified.  If NULL pkt, abort ALL pkts.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct isp *isp = ADDR2ISP(ap);
	struct isp_mbox_cmd mbox_cmd;
	ushort_t	 arg, rval = FALSE;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_ABORT_START,
	    "isp_scsi_abort_start");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	/*
	 * hold off new requests, we need the req mutex anyway for mbox cmds.
	 * the waitQ must be empty since the request mutex was free
	 */
	ISP_MUTEX_ENTER(isp);

	/*
	 * If no space in request queue, return error
	 */
	if (isp->isp_queue_space == 0) {
		isp_i_update_queue_space(isp);

		/*
		 * Check now to see if the queue is still full
		 */
		if (isp->isp_queue_space == 0) {
			ISP_DEBUG(isp, SCSI_DEBUG,
			    "isp_scsi_abort: No space in Queue for Marker");
			goto fail;
		}
	}

	if (pkt) {
		struct isp_cmd *sp = PKT2CMD(pkt);

		ASSERT(ISP_LOOKUP_ID(sp->cmd_id) == sp);
		ISP_DEBUG(isp, SCSI_DEBUG, "aborting pkt 0x%x", (int)pkt);

		arg = ((ushort_t)ap->a_target << 8) | ((ushort_t)ap->a_lun);
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 4, 4,
			ISP_MBOX_CMD_ABORT_IOCB, arg,
			MSW(sp->cmd_id), LSW(sp->cmd_id), 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}

		ISP_MUTEX_EXIT(isp);
	} else {
		ISP_DEBUG(isp, SCSI_DEBUG, "aborting all pkts");

		arg = ((ushort_t)ap->a_target << 8) | ((ushort_t)ap->a_lun);
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
		    ISP_MBOX_CMD_ABORT_DEVICE, arg, 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}

		ISP_MUTEX_EXIT(isp);
		if (isp_i_set_marker(isp, SYNCHRONIZE_NEXUS,
		    (short)ap->a_target, (short)ap->a_lun)) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for target %d LUN %d",
			    ap->a_target, ap->a_lun);
			ISP_MUTEX_ENTER(isp);
			goto fail;
		}
	}

	rval = TRUE;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_ABORT_END,
	    "isp_scsi_abort_end");

	mutex_enter(ISP_REQ_MUTEX(isp));
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);

	return (rval);

fail:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_ABORT_END,
	    "isp_scsi_abort_end");

	mutex_exit(ISP_RESP_MUTEX(isp));
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);

	return (rval);
}


/*
 * Function name : isp_scsi_reset()
 *
 * Return Values : FALSE - reset failed
 *		   TRUE	 - reset succeeded
 * Description	 :
 * SCSA interface routine to perform scsi resets on either
 * a specified target or the bus (default).
 * XXX check waitQ as well
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 */
static int
isp_scsi_reset(struct scsi_address *ap, int level)
{
	struct isp *isp = ADDR2ISP(ap);
	struct isp_mbox_cmd mbox_cmd;
	ushort_t arg;
	int	rval = FALSE;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_RESET_START,
	    "isp_scsi_reset_start");

	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)) == 0 || ddi_in_panic());
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	/*
	 * hold off new requests, we need the req mutex anyway for mbox cmds.
	 * the waitQ must be empty since the request mutex was free
	 */
	ISP_MUTEX_ENTER(isp);

	/*
	 * If no space in request queue, return error
	 */
	if (isp->isp_queue_space == 0) {
		isp_i_update_queue_space(isp);

		/*
		 * Check now to see if the queue is still full
		 */
		if (isp->isp_queue_space == 0) {
			ISP_DEBUG(isp, SCSI_DEBUG,
			    "isp_scsi_abort: No space in Queue for Marker");
			goto fail;
		}
	}

	if (level == RESET_TARGET) {
		ISP_DEBUG(isp, SCSI_DEBUG, "isp_scsi_reset: reset target %d",
		    ap->a_target);

		arg = ((ushort_t)ap->a_target << 8) | ((ushort_t)ap->a_lun);

		isp_i_mbox_cmd_init(isp, &mbox_cmd, 3, 3,
		    ISP_MBOX_CMD_ABORT_TARGET, arg,
		    (ushort_t)(isp->isp_scsi_reset_delay)/1000, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}

		ISP_MUTEX_EXIT(isp);
		if (isp_i_set_marker(isp, SYNCHRONIZE_TARGET,
		    (short)ap->a_target, (short)ap->a_lun)) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for target %d (SCSI reset)",
			    ap->a_target);
			ISP_MUTEX_ENTER(isp);
			goto fail;
		}
		ISP_MUTEX_ENTER(isp);
	} else {

		ISP_DEBUG(isp, SCSI_DEBUG, "isp_scsi_reset: reset bus");

		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
		    ISP_MBOX_CMD_BUS_RESET,
		    (ushort_t)(isp->isp_scsi_reset_delay), 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}

		mutex_exit(ISP_REQ_MUTEX(isp));
		(void) scsi_hba_reset_notify_callback(ISP_RESP_MUTEX(isp),
			&isp->isp_reset_notify_listf);

		mutex_exit(ISP_RESP_MUTEX(isp));

		if (isp_i_set_marker(isp, SYNCHRONIZE_ALL, 0, 0)) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for all targets (SCSI reset)");
			ISP_MUTEX_ENTER(isp);
			goto fail;
		}
		ISP_MUTEX_ENTER(isp);
	}

	rval = TRUE;

fail:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_SCSI_RESET_END,
	    "isp_scsi_reset_end");

	mutex_exit(ISP_RESP_MUTEX(isp));
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);

	return (rval);
}


/*
 * Function name : isp_i_reset_interface()
 *
 * Return Values : 0 - success
 *		  -1 - hw failure
 *
 * Description	 :
 * Master reset routine for hardware/software.	Resets softc struct,
 * isp chip, and scsi bus.  The works!
 * This function is called from isp_attach or
 * from isp_i_fatal_error with response and request mutex held
 *
 * NOTE: it is up to the caller to flush the response queue and isp_slots
 *	 list
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called by interrupt thread.
 *
 * Called with request and response queue mutexes held
 */
static int
isp_i_reset_interface(struct isp *isp, int action)
{
	int i, j;
	struct isp_mbox_cmd mbox_cmd;
	int rval = -1;

	TRACE_1(TR_FAC_SCSI_ISP, TR_ISP_I_RESET_INTERFACE_START,
	    "isp_i_reset_interface_start (action = %x)", action);

	ASSERT(isp != NULL);

	mutex_enter(ISP_WAITQ_MUTEX(isp));
	if (isp->isp_in_reset) {
		mutex_exit(ISP_WAITQ_MUTEX(isp));
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "already reseting -- not again!?");
		TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESET_INTERFACE_END,
		    "isp_i_reset_interface_end_early");
		return (0);	/* fake successful return */
	}
	isp->isp_in_reset++;
	mutex_exit(ISP_WAITQ_MUTEX(isp));

	/*
	 * If a firmware error is seen do not trust the firmware and issue
	 * mailbox commands
	 */
	if ((action & ISP_FIRMWARE_ERROR) != ISP_FIRMWARE_ERROR) {
		/*
		 * Stop all the Queues
		 *
		 * This is not really necessary since we will issue a bus
		 * reset and that should stop any processing of device queues,
		 * but this is cleaner.
		 */
		for (i = 0; i < NTARGETS_WIDE; i++) {
			for (j = 0; j < isp->isp_max_lun[i]; j++) {
				/*
				 * Stop the queue for individual target/lun
				 * combination
				 */
				isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
				    ISP_MBOX_CMD_STOP_QUEUE,
				    (ushort_t)(i << 8) | j, 0, 0, 0, 0);
				if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
					goto pause_risc;
				}
			}
		}

		/*
		 * Reset the SCSI bus to blow away all the commands
		 * under process
		 */
		if (action & ISP_FORCE_RESET_BUS) {
			ISP_DEBUG(isp, SCSI_DEBUG, "isp_scsi_reset: reset bus");
			isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
				ISP_MBOX_CMD_BUS_RESET,
				(ushort_t)(isp->isp_scsi_reset_delay),
				0, 0, 0, 0);
			if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
				isp_i_log(isp, CE_WARN, "reset fails\n");
				goto pause_risc;
			}

			mutex_exit(ISP_REQ_MUTEX(isp));
			(void) scsi_hba_reset_notify_callback(ISP_RESP_MUTEX(
				isp), &isp->isp_reset_notify_listf);
			mutex_enter(ISP_REQ_MUTEX(isp));
			drv_usecwait((clock_t)isp->isp_scsi_reset_delay*1000);
		}
	}

pause_risc:
	/* Put the Risc in pause mode */
	ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_PAUSE);

	/*
	 * we do not want to wait for an interrupt to finish if we
	 * were called from that context
	 */
	if (!(action & ISP_FROM_INTR)) {
		/* wait to ensure no final interrupt(s) being processed */
		ISP_MUTEX_EXIT(isp);
		mutex_enter(ISP_INTR_MUTEX(isp));
		while (isp->isp_in_intr != 0) {
			if (cv_wait_sig(ISP_INTR_CV(isp),
			    ISP_INTR_MUTEX(isp)) == 0) {
				/* interrupted */
				break;
			}
		}
		mutex_exit(ISP_INTR_MUTEX(isp));
		ISP_MUTEX_ENTER(isp);
	}

	/* flush pkts in queue back to target driver */
	isp_i_qflush(isp, 0, (ushort_t)NTARGETS_WIDE - 1);

	/*
	 * put register set in sxp mode
	 */
	if (isp->isp_bus == ISP_PCI) {
		ISP_SET_BIU_REG_BITS(isp, &isp->isp_biu_reg->isp_bus_conf1,
		    ISP_PCI_CONF1_SXP);
	}

	/*
	 * reset and initialize isp chip
	 *
	 * resetting the chip will put it in default risc mode
	 */
	if (isp_i_reset_init_chip(isp)) {
		goto fail;
	}

	if (action & ISP_DOWNLOAD_FW_ON_ERR) {
		/*
		 * If user wants firmware to be downloaded
		 */
		if (isp->isp_bus == ISP_SBUS) {
			rval = isp_i_download_fw(isp, isp_risc_code_addr,
				isp_sbus_risc_code, isp_sbus_risc_code_length);
		} else {
			rval = isp_i_download_fw(isp, isp_risc_code_addr,
				isp_1040_risc_code, isp_1040_risc_code_length);
		}
	}
	if (rval == -1) {
		/*
		 * Either ISP_DOWNLOAD_FW_ON_ERR was not set or
		 * isp_i_download_fw() failed.
		 *
		 * Start ISP firmware up.
		 */
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 6,
		    ISP_MBOX_CMD_START_FW, isp_risc_code_addr,
		    0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}

		/*
		 * set clock rate
		 */
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
		    ISP_MBOX_CMD_SET_CLOCK_RATE, isp->isp_clock_frequency,
		    0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}
	} else {
		rval = -1;
	}

	/*
	 * set Initiator SCSI ID
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
	    ISP_MBOX_CMD_SET_SCSI_ID, isp->isp_initiator_id,
	    0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}


	ISP_DEBUG(isp, SCSI_DEBUG, "Resetting queues");

	/*
	 * Initialize request and response queue indexes.
	 */
	ISP_DEBUG2(isp, SCSI_DEBUG, "setting req(i/o) to 0/0 (was %d/%d)",
	    isp->isp_request_in, isp->isp_request_out);
	isp->isp_request_in = isp->isp_request_out = 0;
	isp->isp_request_ptr = isp->isp_request_base;

	isp->isp_response_in = isp->isp_response_out = 0;
	isp->isp_response_ptr = isp->isp_response_base;

	/*
	 * extra init that should be done
	 */
	isp->isp_queue_space = ISP_MAX_REQUESTS - 1;
	isp->isp_marker_in = isp->isp_marker_out = 0;
	isp->isp_marker_free = ISP_MI_SIZE - 1;

	isp_i_mbox_cmd_init(isp, &mbox_cmd, 5, 5,
	    ISP_MBOX_CMD_INIT_REQUEST_QUEUE, ISP_MAX_REQUESTS,
	    MSW(isp->isp_request_dvma), LSW(isp->isp_request_dvma),
	    isp->isp_request_in, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	isp_i_mbox_cmd_init(isp, &mbox_cmd, 6, 6,
	    ISP_MBOX_CMD_INIT_RESPONSE_QUEUE, ISP_MAX_RESPONSES,
	    MSW(isp->isp_response_dvma), LSW(isp->isp_response_dvma),
	    0, isp->isp_response_out);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}
	isp->isp_mbox.mbox_flags &= ~ISP_MBOX_CMD_FLAGS_Q_NOT_INIT;
	/*
	 * Get ISP Ram firmware version numbers
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 6,
	    ISP_MBOX_CMD_ABOUT_PROM, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	isp->isp_maj_min_rev = mbox_cmd.mbox_in[1];
	isp->isp_subminor_rev = mbox_cmd.mbox_in[2];
	isp->isp_cust_prod = mbox_cmd.mbox_in[3];

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "Firmware Version: major=%d, minor=%d, subminor=%d",
	    MSB(isp->isp_maj_min_rev), LSB(isp->isp_maj_min_rev),
	    isp->isp_subminor_rev);

	/*
	 * Handle isp capabilities adjustments.
	 */
	ISP_DEBUG(isp, SCSI_DEBUG, "Initializing isp capabilities");

	/*
	 * Check and adjust "host id" as required.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 2,
	    ISP_MBOX_CMD_GET_SCSI_ID, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}
	if (mbox_cmd.mbox_in[1] != isp->isp_initiator_id) {
		ISP_DEBUG(isp, SCSI_DEBUG, "id = %d(%d)",
		    isp->isp_initiator_id,
		    mbox_cmd.mbox_in[1]);
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
		    ISP_MBOX_CMD_SET_SCSI_ID, isp->isp_initiator_id,
		    0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}
	}

	/*
	 * Check and adjust "retries" as required.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 3,
	    ISP_MBOX_CMD_GET_RETRY_ATTEMPTS, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}
	if (mbox_cmd.mbox_in[1] != ISP_RETRIES ||
	    mbox_cmd.mbox_in[2] != ISP_RETRY_DELAY) {
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 3, 3,
		    ISP_MBOX_CMD_SET_RETRY_ATTEMPTS, ISP_RETRIES,
		    ISP_RETRY_DELAY, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}
		ISP_DEBUG(isp, SCSI_DEBUG, "retries = %d(%d), delay = %d(%d)",
		    ISP_RETRIES, mbox_cmd.mbox_in[1],
		    ISP_RETRY_DELAY, mbox_cmd.mbox_in[2]);
	}

	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
	    ISP_MBOX_CMD_SET_SEL_TIMEOUT, isp_selection_timeout, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}
	/*
	 * Set and adjust the Data Over run Recovery method. Set to Mode 2
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 2,
	    ISP_MBOX_CMD_SET_DATA_OVR_RECOV_MODE, 2, 0, 0, 0, 0);

	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	/*
	 * Check and adjust "tag age limit" as required.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 2,
	    ISP_MBOX_CMD_GET_AGE_LIMIT, 0, 0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}
	if (mbox_cmd.mbox_in[1] != isp->isp_scsi_tag_age_limit) {
		ISP_DEBUG(isp, SCSI_DEBUG, "tag age = %d(%d)",
		    isp->isp_scsi_tag_age_limit,
		    mbox_cmd.mbox_in[1]);
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 1,
		    ISP_MBOX_CMD_SET_AGE_LIMIT,
		    (ushort_t)isp->isp_scsi_tag_age_limit, 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
			goto fail;
		}
	}

	/*
	 * Check and adjust isp queues as required.
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
		    ISP_MBOX_CMD_GET_DEVICE_QUEUE_PARAMS,
		    (ushort_t)(i << 8), 0, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		    goto fail;
		}
		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "Max Queue Depth = 0x%x, Exec Trottle = 0x%x",
		    mbox_cmd.mbox_in[2], mbox_cmd.mbox_in[3]);
	}

	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
	    ISP_MBOX_CMD_GET_FIRMWARE_STATUS, 0, 0, 0, 0, 0);
	(void) isp_i_mbox_cmd_start(isp, &mbox_cmd);

	ISP_DEBUG(isp, SCSI_DEBUG, "Max # of IOs = 0x%x",
	    mbox_cmd.mbox_in[2]);

	/*
	 * Set the delay after BDR during a timeout.
	 */
	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 6,
		ISP_MBOX_CMD_SET_DELAY_BDR,
		(ushort_t)(isp->isp_scsi_reset_delay)/1000,
		0, 0, 0, 0);
	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		/* Default QLogic firmware dosen't support this.  */
		isp_i_log(isp, CE_NOTE, "Failed to set BDR delay");
	} else {
		ISP_DEBUG(isp, SCSI_DEBUG, "Set BDR delay to %d", mbox_cmd.
			mbox_out[1]);
	}
	/*
	 * Update caps from isp.
	 */
	ISP_DEBUG(isp, SCSI_DEBUG, "Getting isp capabilities");
	if (isp_i_updatecap(isp, 0, NTARGETS_WIDE)) {
		goto fail;
	}

	/* As the firmware is started afresh - reset backoff flag */
	isp->isp_backoff = 0;

	rval = 0;

fail:
	if (rval != 0) {
		ISP_DEBUG(isp, SCSI_DEBUG, "reset interface failed");
		isp->isp_shutdown = 1;
		isp_i_log(isp, CE_WARN, "interface going offline");
		/*
		 * put register set in risc mode in case the
		 * reset didn't complete
		 */
		if (isp->isp_bus == ISP_PCI) {
			ISP_CLR_BIU_REG_BITS(isp,
			    &isp->isp_biu_reg->isp_bus_conf1,
			    ISP_PCI_CONF1_SXP);
		}
		ISP_CLEAR_RISC_INT(isp);
		ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_icr,
			ISP_BUS_ICR_DISABLE_ALL_INTS);
		ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_PAUSE);
		ISP_WRITE_RISC_REG(isp, &isp->isp_risc_reg->isp_risc_psr,
		    ISP_RISC_PSR_FORCE_TRUE | ISP_RISC_PSR_LOOP_COUNT_DONE);
		ISP_WRITE_RISC_REG(isp, &isp->isp_risc_reg->isp_risc_pcr,
			ISP_RISC_PCR_RESTORE_PCR);
		isp_i_qflush(isp, (ushort_t)0, (ushort_t)NTARGETS_WIDE - 1);
	}

	/* keep track of fact we are no longer resetting */
	mutex_enter(ISP_WAITQ_MUTEX(isp));
	isp->isp_in_reset--;
	ASSERT(isp->isp_in_reset >= 0);
	mutex_exit(ISP_WAITQ_MUTEX(isp));

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESET_INTERFACE_END,
	    "isp_i_reset_interface_end");

	return (rval);
}


/*
 * Function name : isp_i_reset_init_chip()
 *
 * Return Values : 0 - success
 *		  -1 - hw failure
 *
 * Description	 :
 * Reset the ISP chip and perform BIU initializations. Also enable interrupts.
 * It is assumed that EXTBOOT will be strobed low after reset.
 *
 * Context	 : Can be called from different kernel process threads.
 *		   Can be called at attach time.
 */
static int
isp_i_reset_init_chip(struct isp *isp)
{
	int delay_loops;
	int rval = -1;
	unsigned short isp_conf_comm;

	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESET_INIT_CHIP_START,
	    "isp_i_reset_init_chip_start");

	ISP_DEBUG2(isp, SCSI_DEBUG, "isp_i_reset_init_chip");

	if (isp->isp_bus == ISP_PCI) {
		/*
		 * we want to respect framework's setting of PCI
		 * configuration space command register and also
		 * want to make sure that all bits interest to us
		 * are properly set in command register.
		 */
		isp_conf_comm = pci_config_get16(
			isp->isp_pci_config_acc_handle,
			PCI_CONF_COMM);
		ISP_DEBUG2(isp, SCSI_DEBUG,
			"PCI conf command register was 0x%x", isp_conf_comm);
		isp_conf_comm |= PCI_COMM_IO | PCI_COMM_MAE | PCI_COMM_ME |
			PCI_COMM_MEMWR_INVAL | PCI_COMM_PARITY_DETECT |
			PCI_COMM_SERR_ENABLE;
		ISP_DEBUG2(isp, SCSI_DEBUG,
		    "PCI conf command register is 0x%x",
		    isp_conf_comm);
		pci_config_put16(isp->isp_pci_config_acc_handle,
			PCI_CONF_COMM, isp_conf_comm);

		/*
		 * set cache line & latency register in pci configuration
		 * space. line register is set in units of 32-bit words.
		 */
		pci_config_put8(isp->isp_pci_config_acc_handle,
			PCI_CONF_CACHE_LINESZ, (uchar_t)isp_conf_cache_linesz);
		pci_config_put8(isp->isp_pci_config_acc_handle,
			PCI_CONF_LATENCY_TIMER,
			(uchar_t)isp_conf_latency_timer);
	}

	/*
	 * reset the isp
	 */
	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_icr,
		ISP_BUS_ICR_SOFT_RESET);
	/*
	 * we need to wait a bit before touching the chip again,
	 * otherwise problems show up running ISP1040A on
	 * fast sun4u machines.
	 */
	drv_usecwait(ISP_CHIP_RESET_BUSY_WAIT_TIME);
	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_cdma_control,
		ISP_DMA_CON_RESET_INT | ISP_DMA_CON_CLEAR_CHAN);
	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_dma_control,
		ISP_DMA_CON_RESET_INT | ISP_DMA_CON_CLEAR_CHAN);

	/*
	 * wait for isp to fire up.
	 */
	delay_loops = ISP_TIMEOUT_DELAY(ISP_SOFT_RESET_TIME,
		ISP_CHIP_RESET_BUSY_WAIT_TIME);
	while (ISP_READ_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_icr) &
		ISP_BUS_ICR_SOFT_RESET) {
		drv_usecwait(ISP_CHIP_RESET_BUSY_WAIT_TIME);
		if (--delay_loops < 0) {
			isp_i_log(isp, CE_WARN, "Chip reset timeout");
			goto fail;
		}
	}

	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_conf1, 0);

	/*
	 * reset the risc processor
	 */

	ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_RESET);
	drv_usecwait(ISP_CHIP_RESET_BUSY_WAIT_TIME);

	/*
	 * initialization biu
	 */
	ISP_SET_BIU_REG_BITS(isp, &isp->isp_biu_reg->isp_bus_conf1,
		isp->isp_conf1_fifo);
	if (isp->isp_conf1_fifo & ISP_BUS_CONF1_BURST_ENABLE) {
		ISP_SET_BIU_REG_BITS(isp, &isp->isp_biu_reg->isp_cdma_conf,
			ISP_DMA_CONF_ENABLE_BURST);
		ISP_SET_BIU_REG_BITS(isp, &isp->isp_biu_reg->isp_dma_conf,
			ISP_DMA_CONF_ENABLE_BURST);
	}
	ISP_WRITE_RISC_REG(isp, &isp->isp_risc_reg->isp_risc_mtr,
		ISP_RISC_MTR_PAGE0_DEFAULT | ISP_RISC_MTR_PAGE1_DEFAULT);
	ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_RELEASE);
	isp->isp_mbox.mbox_flags |= ISP_MBOX_CMD_FLAGS_Q_NOT_INIT;

	if (isp->isp_bus == ISP_PCI) {
		/*
		 * make sure that BIOS is disabled
		 */
		ISP_WRITE_RISC_HCCR(isp, ISP_PCI_HCCR_CMD_BIOS);
	}

	/*
	 * enable interrupts
	 */
	ISP_WRITE_BIU_REG(isp, &isp->isp_biu_reg->isp_bus_icr,
		ISP_BUS_ICR_ENABLE_RISC_INT | ISP_BUS_ICR_ENABLE_ALL_INTS);


	if (isp_state_debug) {
		isp_i_print_state(CE_WARN, isp);
	}

	rval = 0;

fail:
	TRACE_0(TR_FAC_SCSI_ISP, TR_ISP_I_RESET_INIT_CHIP_END,
	    "isp_i_reset_init_chip_end");
	return (rval);
}


/*
 * Error logging, printing, and debug print routines
 */

/*VARARGS3*/
static void
isp_i_log(struct isp *isp, int level, char *fmt, ...)
{
	dev_info_t *dip;
	va_list ap;


	ASSERT(mutex_owned((&isp_log_mutex)) == 0 || ddi_in_panic());

	if (isp != NULL) {
		dip = isp->isp_dip;
	} else {
		dip = 0;
	}

	mutex_enter(&isp_log_mutex);
	va_start(ap, fmt);
	(void) vsprintf(isp_log_buf, fmt, ap);
	va_end(ap);

	if (level == CE_WARN) {
		scsi_log(dip, "isp", level, "%s", isp_log_buf);
	} else {
		scsi_log(dip, "isp", level, "%s\n", isp_log_buf);
	}
	mutex_exit(&isp_log_mutex);
}


static void
isp_i_print_state(int level, struct isp *isp)
{
	char buf[128];
	int i;
	char risc_paused = 0;
	struct isp_biu_regs *isp_biu = isp->isp_biu_reg;
	struct isp_risc_regs *isp_risc = isp->isp_risc_reg;


	ASSERT(isp != NULL);

	/* Put isp header in buffer for later messages. */
	isp_i_log(isp, level, "State dump from isp registers and driver:");

	/*
	 * Print isp registers.
	 */
	(void) sprintf(buf,
		"mailboxes(0-5): 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox0),
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox1),
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox2),
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox3),
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox4),
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox5));
	isp_i_log(isp, CE_CONT, buf);

	if (ISP_READ_RISC_HCCR(isp) ||
		ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_sema)) {
		(void) sprintf(buf,
			"hccr= 0x%x, bus_sema= 0x%x", ISP_READ_RISC_HCCR(isp),
			ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_sema));
		isp_i_log(isp, CE_CONT, buf);
	}
	if ((ISP_READ_RISC_HCCR(isp) & ISP_HCCR_PAUSE) == 0) {
		ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_PAUSE);
		risc_paused = 1;
	}

	(void) sprintf(buf,
		"bus: isr= 0x%x, icr= 0x%x, conf0= 0x%x, conf1= 0x%x",
		ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_isr),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_icr),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_conf0),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_bus_conf1));
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
		"cdma: count= %d, addr= 0x%x, status= 0x%x, conf= 0x%x",
		ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_count),
		(ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_addr1) << 16) |
		ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_addr0),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_status),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_conf));
	if ((i = ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_control)) != 0) {
		(void) sprintf(&buf[strlen(buf)], ", control= 0x%x",
			(ushort_t)i);
	}
	if ((i = ISP_READ_BIU_REG(isp, &isp_biu->isp_cdma_fifo_status)) != 0) {
		(void) sprintf(&buf[strlen(buf)], ", fifo_status= 0x%x",
			(ushort_t)i);
	}
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
		"dma: count= %d, addr= 0x%x, status= 0x%x, conf= 0x%x",
		(ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_count_hi) << 16) |
		ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_count_lo),
		(ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_addr1) << 16) |
		ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_addr0),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_status),
		ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_conf));
	if ((i = ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_control)) != 0) {
		(void) sprintf(&buf[strlen(buf)], ", control= 0x%x",
			(ushort_t)i);
	}
	if ((i = ISP_READ_BIU_REG(isp, &isp_biu->isp_dma_fifo_status)) != 0) {
		(void) sprintf(&buf[strlen(buf)], ", fifo_status= 0x%x",
			(ushort_t)i);
	}
	isp_i_log(isp, CE_CONT, buf);

	/*
	 * If the risc isn't already paused, pause it now.
	 */
	if (risc_paused == 0) {
		ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_PAUSE);
		risc_paused = 1;
	}

	(void) sprintf(buf,
	    "risc: R0-R7= 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x 0x%x",
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_acc),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r1),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r2),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r3),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r4),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r5),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r6),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r7));
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
	    "risc: R8-R15= 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x 0x%x",
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r8),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r9),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r10),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r11),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r12),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r13),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r14),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_r15));
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
	    "risc: PSR= 0x%x, IVR= 0x%x, PCR=0x%x, RAR0=0x%x, RAR1=0x%x",
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_psr),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_ivr),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_pcr),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_rar0),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_rar1));
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
	    "risc: LCR= 0x%x, PC= 0x%x, MTR=0x%x, EMB=0x%x, SP=0x%x",
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_lcr),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_pc),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_mtr),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_emb),
	    ISP_READ_RISC_REG(isp, &isp_risc->isp_risc_sp));
	isp_i_log(isp, CE_CONT, buf);

	/*
	 * If we paused the risc, restart it.
	 */
	if (risc_paused) {
		ISP_WRITE_RISC_HCCR(isp, ISP_HCCR_CMD_RELEASE);
	}

	/*
	 * Print isp queue settings out.
	 */
	isp->isp_request_out =
		ISP_READ_MBOX_REG(isp, &isp->isp_mbox_reg->isp_mailbox4);
	(void) sprintf(buf,
	    "request(in/out)= %d/%d, response(in/out)= %d/%d",
	    isp->isp_request_in, isp->isp_request_out,
	    isp->isp_response_in, isp->isp_response_out);
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
	    "request_ptr(current, base)=  0x%x (0x%x)",
	    (int)isp->isp_request_ptr, (int)isp->isp_request_base);
	isp_i_log(isp, CE_CONT, buf);

	(void) sprintf(buf,
	    "response_ptr(current, base)= 0x%x (0x%x)",
	    (int)isp->isp_response_ptr, (int)isp->isp_response_base);
	isp_i_log(isp, CE_CONT, buf);

	if (ISP_READ_BIU_REG(isp, &isp->isp_biu_reg->isp_cdma_addr1) ||
		ISP_READ_BIU_REG(isp, &isp->isp_biu_reg->isp_cdma_addr0)) {
		(void) sprintf(buf,
		    "dvma request_ptr= 0x%x - 0x%x",
		    (int)isp->isp_request_dvma,
		    (int)isp->isp_response_dvma);
		isp_i_log(isp, CE_CONT, buf);

		(void) sprintf(buf,
		    "dvma response_ptr= 0x%x - 0x%x",
		    (int)isp->isp_response_dvma,
		    (int)(isp->isp_request_dvma + ISP_QUEUE_SIZE));
		isp_i_log(isp, CE_CONT, buf);
	}


	/*
	 * period and offset entries.
	 * XXX this is not quite right if target options are different
	 */
	if (isp->isp_scsi_options & SCSI_OPTIONS_SYNC) {
		(void) sprintf(buf, "period/offset:");
		for (i = 0; i < NTARGETS; i++) {
			(void) sprintf(&buf[strlen(buf)], " %d/%d",
			    PERIOD_MASK(isp->isp_synch[i]),
			    OFFSET_MASK(isp->isp_synch[i]));
		}
		isp_i_log(isp, CE_CONT, buf);
		(void) sprintf(buf, "period/offset:");
		for (i = NTARGETS; i < NTARGETS_WIDE; i++) {
			(void) sprintf(&buf[strlen(buf)], " %d/%d",
			    PERIOD_MASK(isp->isp_synch[i]),
			    OFFSET_MASK(isp->isp_synch[i]));
		}
		isp_i_log(isp, CE_CONT, buf);
	}
}


/*
 * Function name : isp_i_alive()
 *
 * Return Values : FALSE - failed
 *		   TRUE	 - succeeded
 */
static int
isp_i_alive(struct isp *isp)
{
	struct isp_mbox_cmd mbox_cmd;
	ushort_t rval = FALSE;
	ushort_t	total_io_completion;
	ushort_t	total_queued_io;
	ushort_t	total_exe_io;
	ushort_t	total_bad_io;
	ushort_t	an_error = FALSE;


	ASSERT(isp != NULL);

	/*
	 * While we are draining commands or quiesceing the bus, the function
	 * should not do any checking.
	 */
	mutex_enter(ISP_HOTPLUG_MUTEX(isp));
	if ((isp->isp_softstate & ISP_SS_DRAINING) ||
		(isp->isp_softstate & ISP_SS_QUIESCED)) {
		mutex_exit(ISP_HOTPLUG_MUTEX(isp));
		return (TRUE);
	}
	mutex_exit(ISP_HOTPLUG_MUTEX(isp));

	isp_i_mbox_cmd_init(isp, &mbox_cmd, 1, 4,
	    ISP_MBOX_CMD_GET_ISP_STAT, 0, 0, 0, 0, 0);
	ISP_MUTEX_ENTER(isp);

	if (isp_i_mbox_cmd_start(isp, &mbox_cmd)) {
		goto fail;
	}

	total_io_completion = mbox_cmd.mbox_in[1];
	total_queued_io = mbox_cmd.mbox_in[3];
	total_exe_io = mbox_cmd.mbox_in[2];
	total_bad_io = 0;

	/*
	 * for bug id# 4218841: we get many errors where total_queue_io is 1,
	 * and total_io_completion and total_exe_io are both 0 -- an error
	 *
	 * in this case, many of the times, that one command is a non-queued
	 * command
	 *
	 * many times the unqueued commands take longer to be processed when
	 * the ISP chip is busy, so we give those commands a little longer
	 * to finish
	 *
	 * (note: if we have queued I/O we should have executing I/O,
	 * regardless of whether any has completed or not)
	 */
	if ((total_io_completion == 0) &&	/* no I/O has completed */
	    (total_queued_io != 0) && (total_exe_io == 0)) {
		/* save knowledge of this for later */
		an_error = TRUE;
		/* assume all queued I/Os are bad for now */
		total_bad_io = total_queued_io;
	}

	if (isp_timeout_debug || an_error) {
		int	i, j;
		int	warn_value = an_error ? CE_WARN : CE_NOTE;

		isp_i_log(isp, warn_value, "Command totals: queued=%d, "
			"completed=%d, executing=%d",
			total_queued_io, total_io_completion, total_exe_io);

		isp_i_log(isp, CE_CONT, "Running commands:");
		for (i = 0; i < ISP_MAX_SLOTS; i++) {
			int		tgt, lun;
			struct isp_cmd	*sp;
			struct scsi_pkt	*pkt;


			if ((sp = isp->isp_slots[i].slot_cmd) == NULL) {
				continue;
			}

			pkt = CMD2PKT(sp);
			ASSERT(pkt != NULL);

			/*
			 * save the isp_cmd information because it may
			 * complete and be deallocated during the
			 * mailbox command
			 */

			tgt = TGT(sp);
			lun = LUN(sp);

			isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
			    ISP_MBOX_CMD_GET_DEVICE_QUEUE_STATE,
			    (ushort_t)((tgt << 8) | lun),
			    0, 0, 0, 0);
			if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
				/*
				 * not surprising to have an error if
				 * the firmware is hosed -- assume all the
				 * rest of the mailbox commands will
				 * also puke and give up -- set an_error,
				 * since we *really* have an error now
				 */
				an_error = TRUE;
				goto give_up;
			}
			isp_i_log(isp, CE_CONT,
			    "isp_cmd %x: target %d "
			    "LUN %d state=%x exe=%d total=%d",
			    sp, tgt, lun,
			    mbox_cmd.mbox_in[1],
			    mbox_cmd.mbox_in[2],
			    mbox_cmd.mbox_in[3]);

			/*
			 * remember how many non-queued cmds we see
			 * (see system.inc in assmebler sources for
			 *  def)
			 */
			if ((total_bad_io != 0) && an_error &&
			    (mbox_cmd.mbox_in[1] == 0x10)) {
				total_bad_io--;
			}
		}

		isp_i_log(isp, CE_CONT, "Device queues:");
		for (i = 0; i < NTARGETS_WIDE; i++) {
			for (j = 0; j < isp->isp_max_lun[i]; j++) {
				isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
				    ISP_MBOX_CMD_GET_DEVICE_QUEUE_STATE,
				    (ushort_t)((i << 8) | j), 0, 0,
				    0, 0);
				if (isp_i_mbox_cmd_start(isp, &mbox_cmd) !=
				    0) {
					/*
					 * as above, give up here
					 */
					an_error = TRUE;
					goto give_up;
				}
				if ((mbox_cmd.mbox_in[2] != 0) ||
				    (mbox_cmd.mbox_in[3] != 0)) {
					isp_i_log(isp, CE_CONT,
					    "target %d LUN %d: "
					    "state=%x exe=%d total=%d",
					    i, j,
					    mbox_cmd.mbox_in[1],
					    mbox_cmd.mbox_in[2],
					    mbox_cmd.mbox_in[3]);
				}
			}
		}
	}

give_up:
	if (an_error && (total_bad_io != 0)) {
		/*
		 * XXX: see bug id# 4218841 - we are trying to not call
		 * a slow untagged cmd a timeout prematurely
		 */
		/* go ahead and call it a fatal error */
		isp_i_log(isp, CE_WARN, "total_queued_io=%d, total_bad_io=%d",
		    total_queued_io, total_bad_io);
	} else {
		rval = TRUE;
	}

fail:
	mutex_exit(ISP_RESP_MUTEX(isp));
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
	return (rval);
}


/*
 * kmem cache constructor and destructor.
 * When constructing, we bzero the isp cmd structure
 * When destructing, just free the dma handle
 */
/*ARGSUSED*/
static int
isp_kmem_cache_constructor(void * buf, void *cdrarg, int kmflags)
{
	struct isp_cmd *sp = buf;
	struct isp *isp = cdrarg;
	ddi_dma_attr_t	tmp_dma_attr = dma_ispattr;

	int  (*callback)(caddr_t) = (kmflags == KM_SLEEP) ? DDI_DMA_SLEEP:
		DDI_DMA_DONTWAIT;

	bzero((caddr_t)sp, EXTCMDS_SIZE);

	tmp_dma_attr.dma_attr_burstsizes = isp->isp_burst_size;

	if ((sp->cmd_id = ISP_GET_ID(sp, kmflags)) == 0) {
		return (-1);
	}
	if (ddi_dma_alloc_handle(isp->isp_dip, &tmp_dma_attr, callback,
		NULL, &sp->cmd_dmahandle) != DDI_SUCCESS) {
		ISP_FREE_ID(sp->cmd_id);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
static void
isp_kmem_cache_destructor(void * buf, void *cdrarg)
{
	struct isp_cmd *sp = buf;
	ISP_FREE_ID(sp->cmd_id);
	if (sp->cmd_dmahandle) {
		ddi_dma_free_handle(&sp->cmd_dmahandle);
	}
}


#ifdef ISPDEBUG

/*
 * for testing
 *
 * called owning response mutex
 */
static void
isp_i_test(struct isp *isp, struct isp_cmd *sp)
{
	struct scsi_pkt *pkt = (sp != NULL)? CMD2PKT(sp) : NULL;
	struct scsi_address ap;

	/*
	 * Get the address from the packet - fill in address
	 * structure from pkt on to the local scsi_address structure
	 */
	ap.a_hba_tran = pkt->pkt_address.a_hba_tran;
	ap.a_target = pkt->pkt_address.a_target;
	ap.a_lun = pkt->pkt_address.a_lun;
	ap.a_sublun = pkt->pkt_address.a_sublun;

	if (isp_test_abort) {
		mutex_exit(ISP_RESP_MUTEX(isp));
		(void) isp_scsi_abort(&ap, pkt);
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_debug_enter_count++;
		isp_test_abort = 0;
	}
	if (isp_test_abort_all) {
		mutex_exit(ISP_RESP_MUTEX(isp));
		(void) isp_scsi_abort(&ap, NULL);
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_debug_enter_count++;
		isp_test_abort_all = 0;
	}
	if (isp_test_reset) {
		mutex_exit(ISP_RESP_MUTEX(isp));
		(void) isp_scsi_reset(&ap, RESET_TARGET);
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_debug_enter_count++;
		isp_test_reset = 0;
	}
	if (isp_test_reset_all) {
		mutex_exit(ISP_RESP_MUTEX(isp));
		(void) isp_scsi_reset(&ap, RESET_ALL);
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_debug_enter_count++;
		isp_test_reset_all = 0;
	}
	if (isp_test_fatal) {
		isp_test_fatal = 0;
		isp_i_fatal_error(isp, ISP_FORCE_RESET_BUS);
		isp_debug_enter_count++;
		isp_test_fatal = 0;
	}
}

#endif	/* ISPDEBUG */


#ifdef	ISPDEBUG_FW
/*
 * If a debug driver is loaded then writing to the devctl node will
 * cause the ISP driver to use whatever is written as new firmware.
 * The entire firmware image MUST be written in one operation.
 * Additional resets will blow away the new firmware.
 *
 * Called with neither request nor response mutexes held
 */
/*ARGSUSED*/
static int
isp_new_fw(dev_t dev, struct uio *uio, cred_t *cred_p)
{
	struct isp *isp;
	minor_t instance;
	unsigned short *newfw = NULL;
	size_t fwlen = 0;
	int rval = 0;


	instance = getminor(dev);
	isp = (struct isp *)ddi_get_soft_state(isp_state, instance);

	if (isp == NULL) {
		return (ENXIO);
	}

	/* Must start at beginning */
	if (uio->uio_loffset != 0) {
		return (EINVAL);
	}

	/* Sanity check the size: card has 128KB max */
	if ((fwlen = uio->uio_iov->iov_len) > 128*1024) {
		return (EINVAL);
	}

	if ((newfw = kmem_alloc(fwlen, KM_SLEEP)) == 0) {
		return (ENOMEM);
	}

	isp_i_log(isp, CE_WARN, "loading %x bytes of firmware", fwlen);

	/* Read it in */
	if ((rval = uiomove(newfw, fwlen, UIO_WRITE, uio)) != 0) {
		goto fail;
	}

	/* Grab this mutexes to prevent any commands from queueing */
	ISP_MUTEX_ENTER(isp);

	/* Blow away the F/W */
	(void) isp_i_reset_interface(isp, ISP_FORCE_RESET_BUS);

	/* Download new F/W */
	rval = isp_i_download_fw(isp, isp_risc_code_addr, newfw, fwlen/2);

	/* Re-init the F/W */
	(void) isp_i_reset_interface(isp, ISP_RESET_BUS_IF_BUSY);

	/* Flush any pending commands (redundant?) */
	isp_i_qflush(isp, (ushort_t)0, (ushort_t)NTARGETS_WIDE - 1);

	mutex_exit(ISP_REQ_MUTEX(isp));
	(void) scsi_hba_reset_notify_callback(ISP_RESP_MUTEX(isp),
	    &isp->isp_reset_notify_listf);
	mutex_enter(ISP_REQ_MUTEX(isp));

	mutex_exit(ISP_RESP_MUTEX(isp));
	ISP_CHECK_WAITQ_AND_EXIT_REQ_MUTEX(isp);
fail:
	kmem_free(newfw, fwlen);

	return (rval);
}

#endif	/* ISPDEBUG_FW */



#ifdef	ISPDEBUG_IOCTL

/*
 * used by ioctl routine to print devQ stats for one tgt/LUN
 *
 * return non-zero for error
 */
static int
isp_i_print_devq(struct isp *isp, int tgt, int lun)
{
	struct isp_mbox_cmd	mbox_cmd;


	isp_i_mbox_cmd_init(isp, &mbox_cmd, 2, 4,
	    ISP_MBOX_CMD_GET_DEVICE_QUEUE_STATE, (tgt << 8)|lun, 0, 0, 0, 0);

	if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
		isp_i_log(isp, CE_NOTE, "error: can't get devQ stats");
		return (1);
	}

	/* any cmds executing here? */
	if ((mbox_cmd.mbox_in[1] == 0) && (mbox_cmd.mbox_in[2] == 0) &&
	    (mbox_cmd.mbox_in[3] == 0)) {
		return (0);
	}

	/* print results and return */
	isp_i_log(isp, CE_CONT, "tgt=%d lun=%d state=0x%x, exe=%d, ttl=%d",
	    tgt, lun, mbox_cmd.mbox_in[1], mbox_cmd.mbox_in[2],
	    mbox_cmd.mbox_in[3]);
	return (0);

}


/*
 * ioctl routine to allow debugging
 */
static int
isp_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp)
{
	int		instance;		/* our minor number */
	struct isp	*isp;			/* our soft state ptr */
	int		rv = 0;			/* return value */


	/* get minor number -- mask off devctl bits */
	instance = (getminor(dev) >> 6) & 0xff;
	if ((isp = (struct isp *)ddi_get_soft_state(isp_state, instance)) ==
	    NULL) {
		isp_i_log(NULL, CE_WARN,
		    "isp: can't get soft state for instance %d", instance);
		goto try_scsa;
	}

	if (isp_debug_ioctl) {
		isp_i_log(isp, CE_NOTE,
		    "NOTE: isp_ioctl(%d.%d, 0x%x, ...): entering",
		    getmajor(dev), getminor(dev),
		    cmd);
	}

	switch (cmd) {

	case ISP_RELOAD_FIRMWARE:
		mutex_enter(ISP_RESP_MUTEX(isp));
		isp_i_fatal_error(isp, ISP_FORCE_RESET_BUS);
		mutex_exit(ISP_RESP_MUTEX(isp));
		break;

	case ISP_PRINT_DEVQ_STATS: {
		int			tgt, lun;


		ISP_MUTEX_ENTER(isp);	/* freeze while looking at it */

		/* print genl device state */
		isp_i_print_state(CE_NOTE, isp);

		/* scan all tgts/luns */
		isp_i_log(isp, CE_NOTE, "DEBUG: devQ stats ...");
		for (tgt = 0; tgt < NTARGETS_WIDE; tgt++) {
			for (lun = 0; lun < 32; lun++) {
				if (isp_i_print_devq(isp, tgt, lun) != 0) {
					rv = 1;
					break;
				}
			}
		}

		ISP_MUTEX_EXIT(isp);		/* unfreeze now we're done */

		break;
	}

	case ISP_RESET_TARGET: {
		int			tgt;
		ushort_t		tgt_lun;
		struct isp_mbox_cmd	mbox_cmd;


		tgt = (int)arg;

		if ((tgt < 0) || (tgt >= NTARGETS_WIDE)) {
			isp_i_log(isp, CE_NOTE,
			    "target out of range: %d (range 0-%d)",
			    tgt, NTARGETS_WIDE);
			return (EINVAL);
		}

		isp_i_log(isp, CE_NOTE, "DEBUG: resetting target %d", tgt);

		tgt_lun = (ushort_t)tgt << 8;

		ISP_MUTEX_ENTER(isp);	/* freeze while looking at it */

		isp_i_mbox_cmd_init(isp, &mbox_cmd, 3, 3,
		    ISP_MBOX_CMD_ABORT_TARGET, tgt_lun,
		    (ushort_t)(isp->isp_scsi_reset_delay)/1000, 0, 0, 0);
		if (isp_i_mbox_cmd_start(isp, &mbox_cmd) != 0) {
			isp_i_log(isp, CE_NOTE, "can't start mbox cmd");
			rv = 1;			/* failure */
		}

		ISP_MUTEX_EXIT(isp);

		if (rv != 0) {
			break;
		}

		if (isp_i_set_marker(isp, SYNCHRONIZE_TARGET, (short)tgt,
		    0) != 0) {
			isp_i_log(isp, CE_WARN,
			    "cannot set marker for target %d (SCSI reset)",
			    tgt);
			rv = 1;			/* failure */
		}
		break;
	}

	case ISP_PRINT_SLOTS: {
		int			i, slot;
		struct isp_cmd		*sp;
		int			cnt = 0;

		mutex_enter(ISP_WAITQ_MUTEX(isp));
		isp_i_log(isp, CE_NOTE,
		    "DEBUG: printing %d slots starting from %d",
		    ISP_MAX_SLOTS, isp->isp_free_slot);

		slot = isp->isp_free_slot;
		mutex_exit(ISP_WAITQ_MUTEX(isp));

		for (i = 0; i < ISP_MAX_SLOTS; i++) {
			if ((sp = isp->isp_slots[slot].slot_cmd) != NULL) {
				cnt++;
				isp_i_log(isp, CE_NOTE, "slot %3d @ 0x%x: ...",
				    slot, (caddr_t)sp);
			}
			if (++slot >= ISP_MAX_SLOTS) {
				slot = 0;	/* wrap around */
			}
		}
		isp_i_log(isp, CE_CONT, "  %d active slot(s) found", cnt);
	}

	default:
try_scsa:
		rv = scsi_hba_ioctl(dev, cmd, arg, mode, credp, rvalp);
		break;
	}

	return (rv);
}

#endif	/* ISPDEBUG_IOCTL */


/*
 * the QLogic chip controls the "request queue out" pointer (and we control
 * the in pointer) -- the isp chip returns the out pointer in mailbox
 * register 4
 *
 * XXX: note that we do not do any handshaking with the chip here to ensure
 * that the value is stable/valid, so instead we hacked up reading the
 * register two times in a row until it returns the same value twice!
 *
 * enter and leave owning request mutex (don't care about response mutex)
 */
static void
isp_i_update_queue_space(struct isp *isp)
{
	ushort_t	old_outp;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	old_outp = ISP_GET_REQUEST_OUT(isp);

	/* debounce request out value */
	while ((isp->isp_request_out = ISP_GET_REQUEST_OUT(isp)) != old_outp) {
		ISP_DEBUG(isp, SCSI_DEBUG,
		    "reqeust out pointer bounced from %d to %d!",
		    old_outp, isp->isp_request_out);
		old_outp = isp->isp_request_out;
	}

	/* update our count of space based on in and out pointers */
	if (isp->isp_request_in == isp->isp_request_out) {
		/* queue is empty so set space to max */
		isp->isp_queue_space = ISP_MAX_REQUESTS - 1;
	} else if (isp->isp_request_in > isp->isp_request_out) {
		/* queue is partly empty/full (w/wraparound) */
		isp->isp_queue_space = (ISP_MAX_REQUESTS - 1) -
		    (isp->isp_request_in - isp->isp_request_out);
	} else {
		/* queue is partly empty/full (no wraparound) */
		isp->isp_queue_space = isp->isp_request_out -
		    isp->isp_request_in - 1;
	}

	/*
	 * send any markers needed before any other I/O (if room)
	 */
	while ((isp->isp_marker_free < (ISP_MI_SIZE - 1)) &&
	    (isp->isp_queue_space > 0)) {
		struct isp_marker_info	*m;

		/* get pointer to this entry */
		m = &isp->isp_markers[isp->isp_marker_out++];
		isp->isp_marker_free++;		/* one more free slot now */

		/* double check assumption */
		ASSERT(isp->isp_marker_free < ISP_MI_SIZE);

		/* keep track of queue space, tracking wrap around */
		if (isp->isp_marker_out >= ISP_MI_SIZE) {
			isp->isp_marker_out = 0;	 /* wrap around */
		}

		/* send marker to chip */
		isp_i_send_marker(isp, m->isp_marker_mode, m->isp_marker_tgt,
		    m->isp_marker_lun);
	}

	ISP_DEBUG2(isp, SCSI_DEBUG,
	    "updated queue space: req(i/o)=%d/%d, now space=%d",
	    isp->isp_request_in, isp->isp_request_out,
	    isp->isp_queue_space);
}


/*
 * this routine is the guts of isp_i_set_marker() -- but, if the request
 * queue space is zero, this routine will *not* try to free new space.  This
 * is so that it can be called from the "get more queue space" thread without
 * recursing
 */
static void
isp_i_send_marker(struct isp *isp, short mod, ushort_t tgt, ushort_t lun)
{
	struct isp_request	*req;
	struct isp_request	req_buf;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));

	/*
	 * create a marker entry by filling in a request struct
	 */
	req_buf.req_header.cq_entry_type = CQ_TYPE_MARKER;
	req_buf.req_header.cq_entry_count = 1;
	req_buf.req_scsi_id.req_target = (uchar_t)tgt;
	req_buf.req_scsi_id.req_lun_trn = (uchar_t)lun;
	req_buf.req_modifier = mod;

	/* get a pointer to the next free reqeust struct */
	ISP_GET_NEXT_REQUEST_IN(isp, req);

	/* copy our request into the request buffer */
	ISP_COPY_OUT_REQ(isp, &req_buf, req);

	/* Tell isp it's got a new I/O request... */
	ISP_SET_REQUEST_IN(isp);

	/* keep track of request Q space available */
	isp->isp_queue_space--;
}


/*
 * add marker info to the cirular queue for this instance
 *
 * Note: we could optimize this addition by skipping adding an entry
 * if a subsuming entry existed already.  For example, we could skip
 * adding an entry to send a marker for target 0 lun 0 if there's alredy
 * an entry to send a marker for target 0 all luns.  But this won't
 * be done since the need to send multiple targets when the request queue
 * is busy seems very unlikely (and hasn't happened in practice).  So
 * optimization is overkill (and may not work as well)
 *
 * this function can be called from interrupt context, so can not sleep
 *
 * return 0 upon success
 *
 * must be called owning the request mutex and not owning the response mutex
 */
static void
isp_i_add_marker_to_list(struct isp *isp, short mode, ushort_t tgt,
    ushort_t lun)
{
	struct isp_marker_info	*m;


	ASSERT(isp != NULL);
	ASSERT(mutex_owned(ISP_REQ_MUTEX(isp)));
	ASSERT(mutex_owned(ISP_RESP_MUTEX(isp)) == 0 || ddi_in_panic());

	/*
	 * we allocate enough room so that we never run out, but just
	 * in case ...
	 */
	if (isp->isp_marker_free == 0) {
		/*
		 * perhaps later just set index to zero and try to go on???
		 */
		isp_i_log(isp, CE_PANIC,
		    "fatal error: no room to save markers, already saving %d",
		    ISP_MI_SIZE - 1);
		_NOTE(NOT_REACHED)
		/*NOTREACHED*/
	}

	/* get pointer to our entry */
	m = &isp->isp_markers[isp->isp_marker_in++];
	isp->isp_marker_free--;			/* one less free slot */

	/* check for wrap around */
	if (isp->isp_marker_in >= (ISP_MI_SIZE - 1)) {
		isp->isp_marker_in = 0;
	}

	/* fill in our entry */
	m->isp_marker_mode = mode;
	m->isp_marker_tgt = tgt;
	m->isp_marker_lun = lun;
}
