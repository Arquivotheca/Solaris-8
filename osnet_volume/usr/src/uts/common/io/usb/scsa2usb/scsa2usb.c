/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)scsa2usb.c	1.6	99/12/02 SMI"

/*
 * scsa2usb bridge nexus driver
 */

#if defined(lint) && !defined(DEBUG)
#define	DEBUG	1
#endif

#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>

#include <sys/scsi/scsi.h>
#include <sys/sunndi.h>
#include <sys/esunddi.h>
#include <sys/callb.h>
#include <sys/usb/clients/mass_storage/usb_bulkonly.h>
#include <sys/usb/scsa2usb/scsa2usb.h>

/*
 * Function Prototypes
 */
static int	scsa2usb_attach(dev_info_t *, ddi_attach_cmd_t);
static int	scsa2usb_detach(dev_info_t *, ddi_detach_cmd_t);
static void	scsa2usb_cleanup(scsa2usb_state_t *);
static void	scsa2usb_create_luns(scsa2usb_state_t *);

static int	scsa2usb_is_usb(dev_info_t *);
static int	scsa2usb_open_usb_pipes(scsa2usb_state_t *);
static void	scsa2usb_close_usb_pipes(scsa2usb_state_t *, uint_t);
static void	scsa2usb_close_usb_pipes_cb(usb_opaque_t, int, uint_t);

static void	scsa2usb_bulk_only_state_machine(scsa2usb_state_t *, mblk_t *);
static int	scsa2usb_bulk_only_cb(usb_pipe_handle_t, usb_opaque_t,
		    mblk_t *);
static int	scsa2usb_bulk_only_ex_cb(usb_pipe_handle_t, usb_opaque_t,
		    uint_t, mblk_t *, uint_t);

static void	scsa2usb_pipes_reset_cb(usb_opaque_t, int, uint_t);
static int	scsa2usb_reset_recovery(scsa2usb_state_t *);
static int	scsa2usb_reset_pipes(scsa2usb_state_t *);
static int	scsa2usb_default_pipe_cb(usb_pipe_handle_t, usb_opaque_t,
		    mblk_t *);
static int	scsa2usb_default_pipe_ex_cb(usb_pipe_handle_t, usb_opaque_t,
		    uint_t, mblk_t *, uint_t);
static void	scsa2usb_handle_stall_reset_cleanup(scsa2usb_state_t *);

static int	scsa2usb_clear_ept_stall(scsa2usb_state_t *, uint_t, char *);

static int	scsa2usb_handle_scsi_cmd_sub_class(scsa2usb_state_t *,
		    scsa2usb_cmd_t *, struct scsi_pkt *);
static int	scsa2usb_bulk_only_transport(scsa2usb_state_t *,
		    scsa2usb_cmd_t *);
static int	scsa2usb_rw_transport(scsa2usb_state_t *, struct scsi_pkt *);

static mblk_t	*scsa2usb_cbw_build(scsa2usb_state_t *, scsa2usb_cmd_t *);
static mblk_t	*scsa2usb_bp_to_mblk(scsa2usb_state_t *);

static int	scsa2usb_handle_data_start(scsa2usb_state_t *,
		    scsa2usb_cmd_t *);
static int	scsa2usb_handle_data_done(scsa2usb_state_t *, scsa2usb_cmd_t *,
		    mblk_t *);
static int	scsa2usb_handle_status_start(scsa2usb_state_t *);
static int	scsa2usb_handle_csw_result(scsa2usb_state_t *, mblk_t *);
static void	scsa2usb_pkt_completion(scsa2usb_state_t *);
static void	scsa2usb_device_not_responding(scsa2usb_state_t *);
static int	scsa2usb_set_timeout(scsa2usb_state_t *, int);

static int	scsa2usb_cpr_resume(dev_info_t *, scsa2usb_state_t *);
static int	scsa2usb_cpr_suspend(dev_info_t *, scsa2usb_state_t *);
static boolean_t scsa2usb_cpr_callb(void *, int);
static boolean_t scsa2usb_panic_callb(void *, int);

static int	scsa2usb_scsi_tgt_probe(struct scsi_device *, int (*)(void));
static int	scsa2usb_scsi_tgt_init(dev_info_t *, dev_info_t *,
		    scsi_hba_tran_t *, struct scsi_device *);
static void	scsa2usb_tran_tgt_free(dev_info_t *, dev_info_t *,
		    scsi_hba_tran_t *, struct scsi_device *);
static struct	scsi_pkt *scsa2usb_scsi_init_pkt(struct scsi_address *,
		    struct scsi_pkt *, struct buf *, int, int,
		    int, int, int (*)(), caddr_t);
static void	scsa2usb_scsi_destroy_pkt(struct scsi_address *,
		    struct scsi_pkt *);
static int	scsa2usb_scsi_start(struct scsi_address *, struct scsi_pkt *);
static void	scsa2usb_prepare_pkt(scsa2usb_state_t *,
		    struct scsi_pkt *, scsa2usb_cmd_t *);
static int	scsa2usb_scsi_abort(struct scsi_address *, struct scsi_pkt *);
static int	scsa2usb_scsi_reset(struct scsi_address *, int);
static int	scsa2usb_scsi_getcap(struct scsi_address *, char *, int);
static int	scsa2usb_scsi_setcap(struct scsi_address *, char *, int, int);
static int	scsa2usb_scsi_quiesce(dev_info_t *);
static int	scsa2usb_scsi_unquiesce(dev_info_t *);

/* event handling */
static void	scsa2usb_register_events(scsa2usb_state_t *);
static void	scsa2usb_unregister_events(scsa2usb_state_t *);
static int	scsa2usb_check_same_device(dev_info_t *, scsa2usb_state_t *);
static int	scsa2usb_connect_event_callback(dev_info_t *,
		    ddi_eventcookie_t, void *, void *);
static int	scsa2usb_disconnect_event_callback(dev_info_t *,
		    ddi_eventcookie_t, void *, void *);

/* scsi pkt callback handling */
static void	scsa2usb_queue_cb(scsa2usb_state_t *, scsa2usb_cmd_t *);
static void	scsa2usb_callback(void *);
static void	scsa2usb_drain_callback();

/* PM handling */
static void 	scsa2usb_create_pm_components(dev_info_t *, scsa2usb_state_t *,
		    usb_device_descr_t *);
static void 	scsa2usb_device_idle(scsa2usb_state_t *);
static int 	scsa2usb_check_power(scsa2usb_state_t *);
static int	scsa2usb_pwrlvl0(scsa2usb_state_t *);
static int	scsa2usb_pwrlvl1(scsa2usb_state_t *);
static int	scsa2usb_pwrlvl2(scsa2usb_state_t *);
static int	scsa2usb_pwrlvl3(scsa2usb_state_t *);
static int 	scsa2usb_power(dev_info_t *, int comp, int level);


/* cmd decoding */
static char *scsa2usb_cmds[] = {
	"\000tur",
	"\001rezero",
	"\003rqsense",
	"\004format",
	"\014cartprot",
	"\022inquiry",
	"\026tranlba",
	"\030fmtverify",
	"\032modesense",
	"\033start",
	"\035snddiag",
	"\036doorlock",
	"\043formatcap",
	"\045readcap",
	"\050read10",
	"\052write10",
	"\053seek10",
	"\056writeverify",
	"\057verify",
	"\076readlong",
	"\077writelong",
	"\0125modeselect",
	"\0132modesense",
	"\05read12"
	"\052write12",
	NULL
};


/* global variables */
static void *scsa2usb_state;				/* for soft state */
static scsa2usb_cb_info_t scsa2usb_cb;			/* for callbacks */
static boolean_t scsa2usb_sync_message = B_TRUE;	/* for syncing */
static size_t scsa2usb_max_bulk_xfer_size = SCSA2USB_MAX_BULK_XFER;

static int scsa2usb_pm_enable = 1;

/* debugging information */
#ifdef DEBUG
static kmutex_t scsa2usb_dump_mutex;
static void	scsa2usb_dump(uint_t, usb_opaque_t);
#endif	/* DEBUG */


/* for debug messages */
static uint_t	scsa2usb_errmask = (uint_t)DPRINT_MASK_ALL;
static uint_t	scsa2usb_errlevel = USB_LOG_L2;
static uint_t	scsa2usb_instance_debug = (uint_t)-1;
static uint_t	scsa2usb_show_label = USB_ALLOW_LABEL;

#ifdef	SCSA2USB_TEST
/*
 * Test 13 cases. (See USB Mass Storage Class - Bulk Only Transport).
 * We are not covering test cases 1, 6, and 12 as these are the "good"
 * test cases and are tested as part of the normal drive access operations.
 *
 * NOTE: This is for testing only. It will be replaced by a uscsi test.
 */
static int scsa2usb_test_case_2 = 0;
static int scsa2usb_test_case_3 = 0;
static int scsa2usb_test_case_4 = 0;
static int scsa2usb_test_case_5 = 0;
static int scsa2usb_test_case_7 = 0;
static int scsa2usb_test_case_8 = 0;
static int scsa2usb_test_case_9 = 0;
static int scsa2usb_test_case_10 = 0;
static int scsa2usb_test_case_11 = 0;
static int scsa2usb_test_case_13 = 0;

static void	scsa2usb_test_mblk(scsa2usb_state_t *, boolean_t);
#endif	/* SCSA2USB_TEST */


/* modloading support */
static struct dev_ops scsa2usb_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	scsa2usb_attach,	/* attach */
	scsa2usb_detach,	/* detach */
	nodev,			/* reset */
	NULL,			/* driver operations */
	NULL,			/* bus operations */
	scsa2usb_power		/* power */
};

static struct modldrv modldrv = {
	&mod_driverops,			/* Module type. This one is a driver */
	"SCSA to USB Driver 1.6",	/* Name of the module. */
	&scsa2usb_ops,			/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};


int
_init(void)
{
	int rval;

	if (((rval = ddi_soft_state_init(&scsa2usb_state,
	    sizeof (scsa2usb_state_t), SCSA2USB_INITIAL_ALLOC)) != 0)) {

		return (rval);
	}

	if ((rval = scsi_hba_init(&modlinkage)) != 0) {
		ddi_soft_state_fini(&scsa2usb_state);

		return (rval);
	}

	mutex_init(&scsa2usb_cb.c_mutex, NULL, MUTEX_DRIVER, NULL);

#ifdef DEBUG
	mutex_init(&scsa2usb_dump_mutex, NULL, MUTEX_DRIVER, NULL);
#endif	/* DEBUG */

	if ((rval = mod_install(&modlinkage)) != 0) {
		scsi_hba_fini(&modlinkage);
		ddi_soft_state_fini(&scsa2usb_state);
		mutex_destroy(&scsa2usb_cb.c_mutex);
#ifdef DEBUG
		mutex_destroy(&scsa2usb_dump_mutex);
#endif	/* DEBUG */

		return (rval);
	}

	return (rval);
}


int
_fini(void)
{
	int	rval;

	if ((rval = mod_remove(&modlinkage)) == 0) {
		scsi_hba_fini(&modlinkage);
		ddi_soft_state_fini(&scsa2usb_state);
		mutex_destroy(&scsa2usb_cb.c_mutex);
#ifdef DEBUG
		mutex_destroy(&scsa2usb_dump_mutex);
#endif	/* DEBUG */
	}

	return (rval);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * scsa2usb_attach:
 *	Attach driver
 *	Invoke scsi_hba_tran_alloc
 *	Invoke scsi_hba_attach_setup
 *	Get the serialno of the device
 *	Open bulk pipes and
 *	Create disk child(ren)
 *	Register events
 *	Register for USBA dump
 *	Create cpr/panic callbacks
 */
static int
scsa2usb_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int			i, rval;
	int			instance = ddi_get_instance(dip);
	size_t			usb_config_length;	/* config descr len */
	uchar_t			*usb_config;
	scsi_hba_tran_t		*tran;			/* scsi transport */
	scsa2usb_state_t	*scsa2usbp;
	usb_device_descr_t	*usb_dev_descr;
	usb_config_descr_t	usb_cfg_descr;		/* config descr */

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, NULL,
	    "scsa2usb_attach: dip = 0x%p", dip);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		scsa2usbp = ddi_get_soft_state(scsa2usb_state, instance);
		return (scsa2usb_cpr_resume(dip, scsa2usbp));
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, NULL,
		    "scsa2usb_attach: failed");
		return (DDI_FAILURE);
	}

	/* sd disk relies on usb property to do bp_mapin() */
	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip, "usb", 1) !=
	    DDI_PROP_SUCCESS) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, NULL,
		    "scsa2usb%d: cannot create usb property", instance);

		return (DDI_FAILURE);
	}

	/* Allocate softc information */
	if (ddi_soft_state_zalloc(scsa2usb_state, instance) != DDI_SUCCESS) {
		ddi_prop_remove_all(dip);

		return (DDI_FAILURE);
	}

	/* get soft state space and initialize */
	scsa2usbp = (scsa2usb_state_t *)ddi_get_soft_state(scsa2usb_state,
				    instance);
	if (scsa2usbp == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, NULL,
		    "scsa2usb%d: bad soft state", instance);

		ddi_prop_remove_all(dip);

		return (DDI_FAILURE);
	}

	/* initialize mutex */
	mutex_init(&scsa2usbp->scsa2usb_mutex, NULL, MUTEX_DRIVER, NULL);

	/* bump ref count on cb info *before* first call to scsa2usb_cleanup */
	mutex_enter(&scsa2usb_cb.c_mutex);
	scsa2usb_cb.c_ref_count++;
	mutex_exit(&scsa2usb_cb.c_mutex);

	/* enter mutex to keep scsa2usb_cleanup() happy */
	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* allocate a log handle for debug/error messages */
	scsa2usbp->scsa2usb_log_handle = usb_alloc_log_handle(dip, "s2u",
				&scsa2usb_errlevel,
				&scsa2usb_errmask, &scsa2usb_instance_debug,
				&scsa2usb_show_label, 0);

	/*
	 * If scsa2usb_intfc_num == -1 then this instance is responsible
	 * for the entire device. use scsa2usb_intfc_num 0
	 * for now, scsa2usb driver will only bind to interfaces so this
	 * code isn't really needed
	 */
	scsa2usbp->scsa2usb_intfc_num = usb_get_interface_number(dip);
	if (scsa2usbp->scsa2usb_intfc_num == -1) {
		scsa2usbp->scsa2usb_intfc_num = 0;
	}

	scsa2usbp->scsa2usb_dip = dip;
	scsa2usbp->scsa2usb_instance = instance;

	/* Obtain the cooked device descriptor */
	usb_dev_descr = usb_get_dev_descr(dip);

	/* Obtain the raw configuration descriptor */
	usb_config = usb_get_raw_config_data(dip, &usb_config_length);
	if ((rval = usb_parse_configuration_descr(usb_config, usb_config_length,
	    &usb_cfg_descr, USB_CONF_DESCR_SIZE)) != USB_CONF_DESCR_SIZE) {

		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "usb_parsed_configuration_descr failed rval = %d", rval);

		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	/* Obtain the interface descriptor and print it */
	if ((rval = usb_parse_interface_descr(usb_config, usb_config_length,
	    scsa2usbp->scsa2usb_intfc_num, 0, &scsa2usbp->scsa2usb_intfc_descr,
	    USB_IF_DESCR_SIZE)) != USB_IF_DESCR_SIZE) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "usb_parse_interface_desc failed rval = %d", rval);

		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	/* check here for protocol and subclass supported by this driver  */
	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol) {
	case MS_ISD_1999_SILICON_PROTOCOL:
	case MS_BULK_ONLY_PROTOCOL:
		break;
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "unsupported protocol = %x",
		    scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol);
		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceSubClass) {
	case MS_SCSI_SUB_CLASS:
		break;
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "unsupported subclass = %x",
		    scsa2usbp->scsa2usb_intfc_descr.bInterfaceSubClass);
		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	/* Read all the endpoints */
	for (i = 0; i < scsa2usbp->scsa2usb_intfc_descr.bNumEndpoints; i++) {
		usb_endpoint_descr_t ept;

		if ((rval = usb_parse_endpoint_descr(usb_config,
		    usb_config_length,
		    scsa2usbp->scsa2usb_intfc_num, 0,
		    i,				/* endpoint # */
		    &ept,			/* endpoint */
		    USB_EPT_DESCR_SIZE)) != USB_EPT_DESCR_SIZE) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "usb_parse_endpoint_descr failed rval = %d", rval);

			scsa2usb_cleanup(scsa2usbp);

			return (DDI_FAILURE);
		}

		switch (ept.bmAttributes & USB_EPT_ATTR_MASK) {
		case USB_EPT_ATTR_BULK:
			if ((ept.bEndpointAddress & USB_EPT_DIR_MASK) ==
			    USB_EPT_DIR_OUT) {	/* Bulk Out ept */
				(void) bcopy((caddr_t)&ept,
					&scsa2usbp->scsa2usb_bulkout_ept,
					sizeof (usb_endpoint_descr_t));
			} else if ((ept.bEndpointAddress & USB_EPT_DIR_MASK) ==
			    USB_EPT_DIR_IN) {	/* Bulk In ept */
				(void) bcopy((caddr_t)&ept,
					&scsa2usbp->scsa2usb_bulkin_ept,
					sizeof (usb_endpoint_descr_t));
			}
			break;
		case USB_EPT_ATTR_INTR:	/* Interrupt endpoint */
			(void) bcopy(&ept, &scsa2usbp->scsa2usb_intr_ept,
				sizeof (usb_endpoint_descr_t));
			break;
		default:
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb: Error in ept descr");

			scsa2usb_cleanup(scsa2usbp);

			return (DDI_FAILURE);
		}
	}

	/* Get the serial number */
	if (usb_dev_descr->iSerialNumber) {
		char	buf[SERIAL_NUM_LEN];

		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		if ((rval = usb_get_string_descriptor(dip, USB_LANG_ID,
				usb_dev_descr->iSerialNumber, buf,
				SERIAL_NUM_LEN)) != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "usb_get_string_descr failed, rval = %d", rval);

			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			scsa2usb_cleanup(scsa2usbp);

			return (DDI_FAILURE);
		}

		mutex_enter(&scsa2usbp->scsa2usb_mutex);

		if (strlen(buf)) {
			USB_DPRINTF_L4(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "Serial Number = %s", buf);

			(void) strncpy((char *)scsa2usbp->scsa2usb_serial_no,
				(char *)buf,
				sizeof (scsa2usbp->scsa2usb_serial_no));
		}
	}

	/*
	 * Allocate a transport structure
	 */
	tran = scsi_hba_tran_alloc(dip, SCSI_HBA_CANSLEEP);
	scsa2usbp->scsa2usb_tran = tran;

	/*
	 * initialize transport structure
	 */
	tran->tran_hba_private		= scsa2usbp;
	tran->tran_tgt_private		= NULL;
	tran->tran_tgt_init		= scsa2usb_scsi_tgt_init;
	tran->tran_tgt_probe		= scsa2usb_scsi_tgt_probe;
	tran->tran_tgt_free		= scsa2usb_tran_tgt_free;
	tran->tran_start		= scsa2usb_scsi_start;
	tran->tran_abort		= scsa2usb_scsi_abort;
	tran->tran_reset		= scsa2usb_scsi_reset;
	tran->tran_getcap		= scsa2usb_scsi_getcap;
	tran->tran_setcap		= scsa2usb_scsi_setcap;
	tran->tran_init_pkt		= scsa2usb_scsi_init_pkt;
	tran->tran_destroy_pkt		= scsa2usb_scsi_destroy_pkt;
	tran->tran_dmafree		= NULL;
	tran->tran_sync_pkt		= NULL;
	tran->tran_reset_notify		= NULL;
	tran->tran_get_bus_addr		= NULL;
	tran->tran_get_name		= NULL;
	tran->tran_quiesce		= scsa2usb_scsi_quiesce;
	tran->tran_unquiesce		= scsa2usb_scsi_unquiesce;
	tran->tran_bus_reset		= NULL;
	tran->tran_add_eventcall	= NULL;
	tran->tran_get_eventcookie	= NULL;
	tran->tran_post_event		= NULL;
	tran->tran_remove_eventcall	= NULL;

	/*
	 * get dma attributes from parent nexus,
	 * needed for attaching with SCSA
	 */
	scsa2usbp->scsa2usb_dma_attr = usb_get_hc_dma_attr(dip);

	/* register with SCSA as an HBA */
	if (scsi_hba_attach_setup(dip, scsa2usbp->scsa2usb_dma_attr, tran, 0)) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsi_hba_attach_setup failed");

		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	scsa2usbp->scsa2usb_flags |= SCSA2USB_FLAGS_HBA_ATTACH_SETUP;

	/* open pipes and set scsa2usb_flags */
	if (scsa2usb_open_usb_pipes(scsa2usbp) == USB_FAILURE) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "error opening pipes");

		scsa2usb_cleanup(scsa2usbp);

		return (DDI_FAILURE);
	}

	/* look up reset delay, currently not used though */
	scsa2usbp->scsa2usb_reset_delay = ddi_prop_get_int(DDI_DEV_T_ANY,
	    dip, 0, "scsi-reset-delay", SCSI_DEFAULT_RESET_DELAY);
	if (scsa2usbp->scsa2usb_reset_delay == 0) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "reset delay must be non-zero");
		scsa2usbp->scsa2usb_reset_delay = SCSI_DEFAULT_RESET_DELAY;
	}

	/*
	 * check the number of luns and create children
	 */
	scsa2usb_create_luns(scsa2usbp);

	/* set default block size. updated after read cap cmd */
	scsa2usbp->scsa2usb_lbasize = DEV_BSIZE;

	/* register for connect/disconnect events */
	scsa2usb_register_events(scsa2usbp);

#ifdef	DEBUG
	mutex_enter(&scsa2usb_dump_mutex);

	/* Initialize the dump support */
	scsa2usbp->scsa2usb_dump_ops = usba_alloc_dump_ops();
	scsa2usbp->scsa2usb_dump_ops->usb_dump_ops_version =
			USBA_DUMP_OPS_VERSION_0;
	scsa2usbp->scsa2usb_dump_ops->usb_dump_func = scsa2usb_dump;
	scsa2usbp->scsa2usb_dump_ops->usb_dump_cb_arg = (usb_opaque_t)scsa2usbp;
	scsa2usbp->scsa2usb_dump_ops->usb_dump_order = USB_DUMPOPS_OTHER_ORDER;
	usba_dump_register(scsa2usbp->scsa2usb_dump_ops);
	mutex_exit(&scsa2usb_dump_mutex);
#endif	/* DEBUG */

	/*
	 * In case the system panics, the sync command flushes
	 * dirty FS pages or buffers. This would cause a hang
	 * in USB.
	 * The reason for the failure is that we enter
	 * polled mode (interrupts disabled) and OHCI/UHCI get stuck
	 * trying to execute bulk requests
	 * The panic_callback registered below provides a warning
	 * that a panic has occurred and from that point onwards, we
	 * complete each request successfully and immediately. This
	 * will fake successful syncing so at least the rest of the
	 * filesystems complete syncing.
	 */
	scsa2usbp->scsa2usb_panic_info = kmem_zalloc(sizeof (scsa2usb_cpr_t),
					KM_SLEEP);
	mutex_init(&scsa2usbp->scsa2usb_panic_info->lockp,
					NULL, MUTEX_DRIVER, NULL);
	scsa2usbp->scsa2usb_panic_info->statep = scsa2usbp;
	scsa2usbp->scsa2usb_panic_info->cpr.cc_lockp =
					&scsa2usbp->scsa2usb_panic_info->lockp;
	scsa2usbp->scsa2usb_panic_info->cpr.cc_id =
					callb_add(scsa2usb_panic_callb,
					(void *)scsa2usbp->scsa2usb_panic_info,
					CB_CL_PANIC, "scsa2usb");

	/*
	 * The cpr_callback added below gives us a chance to recover from
	 * similar condition under CPR.
	 */
	scsa2usbp->scsa2usb_cpr_info = kmem_zalloc(sizeof (scsa2usb_cpr_t),
					KM_SLEEP);
	mutex_init(&scsa2usbp->scsa2usb_cpr_info->lockp,
					NULL, MUTEX_DRIVER, NULL);
	scsa2usbp->scsa2usb_cpr_info->statep = scsa2usbp;
	scsa2usbp->scsa2usb_cpr_info->cpr.cc_lockp =
					&scsa2usbp->scsa2usb_cpr_info->lockp;
	scsa2usbp->scsa2usb_cpr_info->cpr.cc_id = callb_add(scsa2usb_cpr_callb,
					(void *)scsa2usbp->scsa2usb_cpr_info,
					CB_CL_CPR_DAEMON, "scsa2usb");

	scsa2usbp->scsa2usb_dev_state = USB_DEV_ONLINE;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	/* enable PM */
	if (scsa2usb_pm_enable) {
		scsa2usb_create_pm_components(dip, scsa2usbp, usb_dev_descr);
	}

	/* report device */
	ddi_report_dev(dip);

	return (DDI_SUCCESS);
}


/*
 * check the number of luns but continue if the check fails,
 * create child nodes for each lun
 */
static void
scsa2usb_create_luns(scsa2usb_state_t *scsa2usbp)
{
	int		i, lun, rval;
	char		name[MAX_COMPAT_NAMES][16];
	char		*compatible[MAX_COMPAT_NAMES];	/* compatible names */
	mblk_t		*data;
	uint_t		completion_reason;
	dev_info_t	*dip = scsa2usbp->scsa2usb_dip;
	dev_info_t	*sd_dip;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	/* Set n_luns to 1 by default (for floppies and other devices) */
	scsa2usbp->scsa2usb_n_luns = 1;

	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol) {
	case MS_ISD_1999_SILICON_PROTOCOL:
		break;
	case MS_BULK_ONLY_PROTOCOL:
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		if ((rval = usb_pipe_sync_device_ctrl_receive(
				scsa2usbp->scsa2usb_default_pipe,
				GET_MAX_LUN,
				USB_DEV_REQ_TYPE_CLASS |
				USB_DEV_REQ_RECIPIENT_INTERFACE |
				USB_DEV_REQ_DEVICE_TO_HOST,
				0,
				scsa2usbp->scsa2usb_intfc_num,
				1,
				&data,
				&completion_reason,
				USB_FLAGS_ENQUEUE)) != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "get max lun not supported, resetting pipe");

			rval = usb_pipe_reset(scsa2usbp->scsa2usb_default_pipe,
				USB_FLAGS_SLEEP, NULL, NULL);
			ASSERT(rval == USB_SUCCESS);

			mutex_enter(&scsa2usbp->scsa2usb_mutex);
		} else {
			ASSERT(completion_reason == 0);
			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			/*
			 * This check ensures that we have valid
			 * data returned back. Otherwise we assume
			 * that device supports only one LUN.
			 */
			if ((data->b_wptr - data->b_rptr) != 1) {
				USB_DPRINTF_L0(DPRINT_MASK_SCSA,
				    scsa2usbp->scsa2usb_log_handle,
				    "device reported incorrect luns "
				    "(adjusting to 1)");
			} else {
				/* Set n_luns to value returned from device */
				scsa2usbp->scsa2usb_n_luns = *data->b_rptr;

				/*
				 * In case a device returns incorrect LUNs
				 * which are more than 15 or negative or 0;
				 * we assume 1.
				 */
				if ((scsa2usbp->scsa2usb_n_luns >=
				    SCSA2USB_MAX_TARGETS) ||
				    (scsa2usbp->scsa2usb_n_luns <= 0)) {
					USB_DPRINTF_L0(DPRINT_MASK_SCSA,
					    scsa2usbp->scsa2usb_log_handle,
					    "device reported %d luns "
					    "(adjusting to 1)",
					    scsa2usbp->scsa2usb_n_luns);

					scsa2usbp->scsa2usb_n_luns = 1;
				}
			}

			SCSA2USB_FREE_MSG(data);	/* Free data */
		}
		break;
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "assuming one lun supported for 0x%p", scsa2usbp);
		break;
	}

	/*
	 * create disk child for each lun
	 */
	for (lun = 0; lun < scsa2usbp->scsa2usb_n_luns; lun++) {
		ndi_devi_alloc_sleep(dip, "disk",
		    (dnode_t)DEVI_SID_NODEID, &sd_dip);

		/* attach target & lun properties */
		rval = ndi_prop_update_int(DDI_DEV_T_NONE, sd_dip, "target", 0);
		if (rval != DDI_PROP_SUCCESS) {
			continue;
		}

		rval = ndi_prop_update_int(DDI_DEV_T_NONE, sd_dip, "lun", lun);
		if (rval != DDI_PROP_SUCCESS) {
			continue;
		}

		/* add compatible name property, one for now */
		for (i = 0; i < MAX_COMPAT_NAMES; i++) {
			compatible[i] = name[i];
		}
		/* make sd driver the compatible name */
		(void) strcpy(name[0], "sd");
		(void) strcpy(name[1], "sgen");

		rval = ndi_prop_update_string_array(DDI_DEV_T_NONE, sd_dip,
		    "compatible", (char **)compatible, MAX_COMPAT_NAMES);
		if (rval != DDI_PROP_SUCCESS) {
			continue;
		}

		/*
		 * add property "usb" so we always verify that it is
		 * our child
		 */
		rval = ndi_prop_create_boolean(DDI_DEV_T_NONE, sd_dip, "usb");
		if (rval != DDI_PROP_SUCCESS) {
			continue;
		}

		rval = ndi_devi_online(sd_dip, 0);
		ASSERT(rval == NDI_SUCCESS);

		scsa2usbp->scsa2usb_target_dip[lun] = sd_dip;
	}
}


static int
scsa2usb_is_usb(dev_info_t *dip)
{
	if (dip) {
		return (ddi_prop_exists(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "usb"));
	}
	return (0);
}


/*
 * scsa2usb_detach:
 *	detach or suspend driver instance
 */
static int
scsa2usb_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	scsi_hba_tran_t	*tran;
	scsa2usb_state_t *scsa2usbp;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ASSERT(tran != NULL);

	scsa2usbp = (scsa2usb_state_t *)tran->tran_hba_private;
	ASSERT(scsa2usbp);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_detach: dip = 0x%p, cmd = %d", dip, cmd);

	switch (cmd) {
	case DDI_DETACH:
		/* assume that the NDI framework detached the children */
		mutex_enter(&scsa2usbp->scsa2usb_mutex);

		if (SCSA2USB_BUSY(scsa2usbp)) {
			/*
			 * We might be in reset. If so, wait for
			 * the reset to finish.
			 */
			if (SCSA2USB_IN_RESET(scsa2usbp)) {
				USB_DPRINTF_L2(DPRINT_MASK_SCSA,
				    scsa2usbp->scsa2usb_log_handle,
				    "scsa2usb_detach: device in reset");

				/*
				 * If we are in reset wait till we finish
				 */
				while (SCSA2USB_IN_RESET(scsa2usbp)) {
					USB_DPRINTF_L4(DPRINT_MASK_SCSA,
					    scsa2usbp->scsa2usb_log_handle,
					    "scsa2usb_cleanup: "
					    "SCSA2USB_IN_RESET flags set");
					mutex_exit(&scsa2usbp->scsa2usb_mutex);
					delay(10);
					mutex_enter(&scsa2usbp->scsa2usb_mutex);
				}
			} else {
				mutex_exit(&scsa2usbp->scsa2usb_mutex);

				return (DDI_FAILURE);
			}
		}

		scsa2usb_cleanup(scsa2usbp);

		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		return (scsa2usb_cpr_suspend(dip, scsa2usbp));

	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_detach: Cpr/pm not supported");

		return (DDI_FAILURE);
	}
}


/*
 * This callback routine is called when there is a system panic.
 */
/* ARGSUSED */
static boolean_t
scsa2usb_panic_callb(void *arg, int code)
{
	scsa2usb_cpr_t *cpr_infop;
	scsa2usb_state_t *scsa2usbp;
	struct scsi_pkt *pkt;

	_NOTE(NO_COMPETING_THREADS_NOW);
	cpr_infop = (scsa2usb_cpr_t *)arg;
	scsa2usbp = (scsa2usb_state_t *)cpr_infop->statep;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_panic_callb: code=%d", code);

	pkt = scsa2usbp->scsa2usb_cur_pkt;

	/*
	 * If we return error here, "sd" prints tons of error
	 * messages and could retry the same pkt over and over again.
	 * The sync recovery isn't "smooth" in that case. By faking
	 * a success return, instead,  we force sync to complete.
	 */
	if (pkt) {
		/*
		 * Do not print the "no sync" warning here. it will
		 * then be displayed before we actually start syncing
		 * Also we don't replace this code with a call to
		 * scsa2usb_pkt_completion().
		 * NOTE: mutexes are disabled during panic.
		 */
		pkt->pkt_reason = CMD_CMPLT;
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		SCSA2USB_UPDATE_PKT_STATE(pkt, PKT2CMD(pkt));
		SCSA2USB_RESET_CUR_PKT(scsa2usbp);
		if (pkt->pkt_comp) {
			pkt->pkt_comp(pkt);
		}
	}

	/* check the callback queue */
	scsa2usb_callback((void *) NULL);

	_NOTE(COMPETING_THREADS_NOW);

	return (B_TRUE);
}


/*
 * This callback routine is called just before threads are frozen
 * or unfrozen
 */
/* ARGSUSED */
static boolean_t
scsa2usb_cpr_callb(void *arg, int code)
{
	scsa2usb_cpr_t *cpr_infop = (scsa2usb_cpr_t *)arg;
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)cpr_infop->statep;

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_cpr_callb: code=%d flag=0x%x",
	    code, scsa2usbp->scsa2usb_flags);

	/* are we suspending? for resume there is nothing to do */
	if (!SCSA2USB_CHK_CPR(scsa2usbp)) {
		int devi_stillreferenced(dev_info_t *);
		int lun, listcnt;
		dev_info_t *child_dip;
		major_t major;
		struct devnames *dnp;

		/*
		 * if any of the children are open, fail suspend since
		 * we cannot support syncing when threads are frozen
		 */
		for (lun = 0; lun < scsa2usbp->scsa2usb_n_luns; lun++) {
			child_dip = scsa2usbp->scsa2usb_target_dip[lun];
			major = ddi_name_to_major(
					ddi_binding_name(child_dip));
			dnp = &(devnamesp[major]);

			LOCK_DEV_OPS(&(dnp->dn_lock));
			e_ddi_enter_driver_list(dnp, &listcnt);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));

			if (child_dip &&
			    (devi_stillreferenced(child_dip) ==
			    DEVI_REFERENCED)) {
				USB_DPRINTF_L0(DPRINT_MASK_SCSA,
				    scsa2usbp->scsa2usb_log_handle,
				    "device is busy and cannot be "
				    "suspended, \n"
				    "Please close device");

				LOCK_DEV_OPS(&(dnp->dn_lock));
				e_ddi_exit_driver_list(dnp, listcnt);
				UNLOCK_DEV_OPS(&(dnp->dn_lock));
				mutex_exit(&scsa2usbp->scsa2usb_mutex);

				return (B_FALSE);
			}
			LOCK_DEV_OPS(&(dnp->dn_lock));
			e_ddi_exit_driver_list(dnp, listcnt);
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
		}

		/* we can suspend */
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (B_TRUE);
}


/*
 * scsa2usb_cleanup:
 *	cleanup whatever attach has setup
 */
static void
scsa2usb_cleanup(scsa2usb_state_t *scsa2usbp)
{
	int lun;
	dev_info_t *dip = scsa2usbp->scsa2usb_dip;
	int rval;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_cleanup called");

	ASSERT(scsa2usbp != NULL);

	/*
	 * wait till the callback thread is done if this is the last
	 * instance.
	 */
	mutex_enter(&scsa2usb_cb.c_mutex);
	if (--(scsa2usb_cb.c_ref_count) == 0) {
		while (scsa2usb_cb.c_cb_active) {
			mutex_exit(&scsa2usb_cb.c_mutex);
			delay(10);
			mutex_enter(&scsa2usb_cb.c_mutex);
		}
	}

	mutex_exit(&scsa2usb_cb.c_mutex);
	if (scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_HBA_ATTACH_SETUP) {
		(void) scsi_hba_detach(dip);
		scsi_hba_tran_free(scsa2usbp->scsa2usb_tran);
	}

	if (scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) {
		scsa2usb_close_usb_pipes(scsa2usbp, USB_FLAGS_SLEEP);
	}


#ifdef	DEBUG
	mutex_enter(&scsa2usb_dump_mutex);
	if (scsa2usbp->scsa2usb_dump_ops) {
		usba_dump_deregister(scsa2usbp->scsa2usb_dump_ops);
		usba_free_dump_ops(scsa2usbp->scsa2usb_dump_ops);
	}
	mutex_exit(&scsa2usb_dump_mutex);
#endif	/* DEBUG */

	ddi_remove_minor_node(dip, NULL);

	ddi_prop_remove_all(dip);

	/*
	 * If not disconnected but offlined, destroy children here
	 * If disconnected, the framework will destroy the children
	 */
	if (!(scsa2usbp->scsa2usb_dev_state == USB_DEV_DISCONNECTED)) {
		/* free the child dips */
		for (lun = 0; lun < scsa2usbp->scsa2usb_n_luns; lun++) {
			if (scsa2usbp->scsa2usb_target_dip[lun]) {
				mutex_exit(&scsa2usbp->scsa2usb_mutex);
				rval = ndi_devi_free(
					scsa2usbp->scsa2usb_target_dip[lun]);
				ASSERT(rval == NDI_SUCCESS);
				mutex_enter(&scsa2usbp->scsa2usb_mutex);
			}
		}
	}

	/* Cancel the registered callbacks */
	if (scsa2usbp->scsa2usb_cpr_info) {
		SCSA2USB_CANCEL_CB(scsa2usbp->scsa2usb_cpr_info->cpr.cc_id);
		mutex_destroy(&scsa2usbp->scsa2usb_cpr_info->lockp);
		scsa2usbp->scsa2usb_cpr_info->statep = NULL;
		kmem_free(scsa2usbp->scsa2usb_cpr_info,
		    sizeof (scsa2usb_cpr_t));
	}

	if (scsa2usbp->scsa2usb_panic_info) {
		SCSA2USB_CANCEL_CB(
		    scsa2usbp->scsa2usb_panic_info->cpr.cc_id);
		mutex_destroy(&scsa2usbp->scsa2usb_panic_info->lockp);
		scsa2usbp->scsa2usb_panic_info->statep = NULL;
		kmem_free(scsa2usbp->scsa2usb_panic_info,
		    sizeof (scsa2usb_cpr_t));
	}

	/* unregister for connect/disconnect events */
	scsa2usb_unregister_events(scsa2usbp);

	usb_free_log_handle(scsa2usbp->scsa2usb_log_handle);

	if (scsa2usbp->scsa2usb_pm) {
		kmem_free(scsa2usbp->scsa2usb_pm, sizeof (scsa2usb_power_t));
	}

	_NOTE(LOCK_RELEASED_AS_SIDE_EFFECT(&scsa2usbp->scsa2usb_mutex));
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	mutex_destroy(&scsa2usbp->scsa2usb_mutex);

	ddi_soft_state_free(scsa2usb_state, ddi_get_instance(dip));
}


/*
 * scsa2usb_open_usb_pipes:
 *	set up a pipe policy
 *	open usb bulk pipes
 */
static int
scsa2usb_open_usb_pipes(scsa2usb_state_t *scsa2usbp)
{
	int			rval;
	usb_pipe_policy_t	*policy, dflt_pipe_policy;	/* policies */
	size_t			sz;

	ASSERT(scsa2usbp);
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_open_usb_pipes: dip = 0x%p flag = 0x%x",
	    scsa2usbp->scsa2usb_dip, scsa2usbp->scsa2usb_flags);

	if (!(scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED)) {

		/*
		 * one pipe policy for all pipes except default pipe
		 */
		policy = &scsa2usbp->scsa2usb_pipe_policy;
		policy->pp_version = USB_PIPE_POLICY_V_0;
		policy->pp_callback_arg = (void *)scsa2usbp;
		policy->pp_timeout_value = SCSA2USB_BULK_PIPE_TIMEOUT;
		policy->pp_callback = scsa2usb_bulk_only_cb;
		policy->pp_exception_callback = scsa2usb_bulk_only_ex_cb;

		USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_open_usb_pipes: opening pipes");

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		/* Open the USB bulk-in pipe */
		if ((rval = usb_pipe_open(scsa2usbp->scsa2usb_dip,
		    &scsa2usbp->scsa2usb_bulkin_ept, policy, USB_FLAGS_SLEEP,
		    &scsa2usbp->scsa2usb_bulkin_pipe)) != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_open: bulk/in pipe open "
			    " failed rval = %d", rval);

			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			return (USB_FAILURE);
		}

		/* Open the bulk-out pipe  using the same policy */
		if ((rval = usb_pipe_open(scsa2usbp->scsa2usb_dip,
		    &scsa2usbp->scsa2usb_bulkout_ept, policy, USB_FLAGS_SLEEP,
		    &scsa2usbp->scsa2usb_bulkout_pipe)) != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_open: bulk/out pipe open"
			    " failed rval = %d", rval);

			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			return (USB_FAILURE);
		}

		/* Open the default pipe */
		bzero(&dflt_pipe_policy, sizeof (usb_pipe_policy_t));
		dflt_pipe_policy.pp_version = USB_PIPE_POLICY_V_0;
		dflt_pipe_policy.pp_callback = scsa2usb_default_pipe_cb;
		dflt_pipe_policy.pp_exception_callback =
						scsa2usb_default_pipe_ex_cb;
		dflt_pipe_policy.pp_callback_arg = (void *)scsa2usbp;
		dflt_pipe_policy.pp_timeout_value = USB_PIPE_TIMEOUT;

		if (usb_pipe_open(scsa2usbp->scsa2usb_dip, NULL,
		    &dflt_pipe_policy, USB_FLAGS_OPEN_EXCL,
		    &scsa2usbp->scsa2usb_default_pipe) != USB_SUCCESS) {

			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_open: default pipe open "
			    "failed rval = %d", rval);

			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			return (USB_FAILURE);
		}

		/* get the max transfer size of the bulk pipe */
		if (usb_pipe_bulk_transfer_size(scsa2usbp->scsa2usb_dip,
		    &sz) == USB_SUCCESS) {
			mutex_enter(&scsa2usbp->scsa2usb_mutex);
			scsa2usbp->scsa2usb_max_bulk_xfer_size =
				min(sz, scsa2usb_max_bulk_xfer_size);
		} else {
			mutex_enter(&scsa2usbp->scsa2usb_mutex);
			scsa2usbp->scsa2usb_max_bulk_xfer_size =
				min(DEV_BSIZE, scsa2usb_max_bulk_xfer_size);
		}

		USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_open_usb_pipes: max bulk transfer size = %x",
		    scsa2usbp->scsa2usb_max_bulk_xfer_size);

		/* Set the pipes opened flag */
		scsa2usbp->scsa2usb_flags |= SCSA2USB_FLAGS_PIPES_OPENED;

		scsa2usbp->scsa2usb_pipe_state = SCSA2USB_PIPE_NORMAL;

		/* Set the state to NONE */
		scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_NONE;
	}

	return (USB_SUCCESS);
}


/*
 * scsa2usb_close_usb_pipes:
 *	close all pipes, synchronously or asynchronously, this must succeed
 */
static void
scsa2usb_close_usb_pipes(scsa2usb_state_t *scsa2usbp, uint_t flags)
{
	int rval = USB_SUCCESS;
	void (*cb)(usb_opaque_t, int, uint_t);
	usb_opaque_t arg;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_close_usb_pipes: scsa2usb_state = 0x%p", scsa2usbp);

	ASSERT(scsa2usbp);
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	if (((scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) == 0)) {

		return;
	}

	if ((flags & USB_FLAGS_SLEEP) == 0) {
		cb = scsa2usb_close_usb_pipes_cb;
		arg = (opaque_t)scsa2usbp;
		scsa2usbp->scsa2usb_pipe_state = SCSA2USB_PIPE_CLOSING;
	} else {
		cb = NULL;
		arg = NULL;
	}

	/* to avoid races, reset the flag first */
	scsa2usbp->scsa2usb_flags &= ~SCSA2USB_FLAGS_PIPES_OPENED;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	rval = usb_pipe_close(&scsa2usbp->scsa2usb_bulkout_pipe,
		flags, cb, arg);
	ASSERT(rval == USB_SUCCESS);

	rval = usb_pipe_close(&scsa2usbp->scsa2usb_bulkin_pipe,
		flags, cb, arg);
	ASSERT(rval == USB_SUCCESS);

	rval = usb_pipe_close(&scsa2usbp->scsa2usb_default_pipe,
		flags, cb, arg);
	ASSERT(rval == USB_SUCCESS);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	if (flags & USB_FLAGS_SLEEP) {
		scsa2usbp->scsa2usb_pipe_state = SCSA2USB_PIPE_NORMAL;
	}
}


/*
 * scsa2usb_cpr_suspend
 *	close pipes if there is no activity
 */
/* ARGSUSED */
static int
scsa2usb_cpr_suspend(dev_info_t *dip, scsa2usb_state_t *scsa2usbp)
{
	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_cpr_suspend:");

	if (!scsa2usbp) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, NULL,
		    "scsa2usb_cpr_resume: !scsa2usbp");

		return (DDI_SUCCESS);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/*
	 * our children should have been suspended first but a recovery
	 * might still be in progress
	 */
	if (SCSA2USB_BUSY(scsa2usbp)) {
		USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_cpr_suspend: I/O active on mass storage");

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (DDI_FAILURE);
	}

	scsa2usbp->scsa2usb_dev_state = USB_DEV_CPR_SUSPEND;

	/*
	 * Close all USB pipes that are open
	 */
	scsa2usb_close_usb_pipes(scsa2usbp, USB_FLAGS_SLEEP);

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (DDI_SUCCESS);
}


/*
 * scsa2usb_cpr_resume
 *	just reopen the pipes
 */
/* ARGSUSED */
static int
scsa2usb_cpr_resume(dev_info_t *dip, scsa2usb_state_t *scsa2usbp)
{
	if (scsa2usbp == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, NULL,
		    "scsa2usb_cpr_resume: !scsa2usbp");

		return (DDI_SUCCESS);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* raise power */
	(void) scsa2usb_check_power(scsa2usbp);

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	/* Check for the same device */
	if (scsa2usb_check_same_device(dip, scsa2usbp) == USB_FAILURE) {
		/* change the flags to active */
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
		scsa2usbp->scsa2usb_dev_state = USB_DEV_DISCONNECTED;
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (DDI_SUCCESS);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	(void) scsa2usb_open_usb_pipes(scsa2usbp);
	scsa2usbp->scsa2usb_dev_state = USB_DEV_ONLINE;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (DDI_SUCCESS);
}


/*
 * SCSA entry points:
 *
 * scsa2usb_scsi_tgt_probe:
 * scsa functions are exported by means of the transport table
 * Issue a probe to get the inquiry data.
 */
/* ARGSUSED */
static int
scsa2usb_scsi_tgt_probe(struct scsi_device *sd, int (*waitfunc)(void))
{
	scsi_hba_tran_t *tran;
	scsa2usb_state_t *scsa2usbp;
	dev_info_t *dip = ddi_get_parent(sd->sd_dev);

	ASSERT(dip);

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ASSERT(tran != NULL);
	scsa2usbp = (scsa2usb_state_t *)tran->tran_hba_private;
	ASSERT(scsa2usbp);

	/* if device is disconnected (ie. pipes closed), fail immediately */
	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (SCSIPROBE_FAILURE);
	}
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_tgt_probe: scsi_device = 0x%p", sd);

	return (scsi_hba_probe(sd, waitfunc));
}


/*
 * scsa2usb_scsi_tgt_init:
 *	check whether we created this child ourselves
 */
/* ARGSUSED */
static int
scsa2usb_scsi_tgt_init(dev_info_t *dip, dev_info_t *cdip,
    scsi_hba_tran_t *tran, struct scsi_device *sd)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)
					tran->tran_hba_private;

	/* is this our child? */
	if (scsa2usb_is_usb(cdip)) {

		return (DDI_SUCCESS);
	} else {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_tgt_init: rejecting %s%d",
		    ddi_driver_name(cdip), ddi_get_instance(cdip));

		return (DDI_FAILURE);
	}
}


/*
 * scsa2usb_tran_tgt_free:
 *	Do nothing.
 */
/* ARGSUSED */
static void
scsa2usb_tran_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
	scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	/* nothing to do */
}


/*
 * scsa2usb_scsi_init_pkt:
 *	Set up the scsi_pkt for transport. Also initialize
 *	scsa2usb_cmd struct for the transport.
 *	NOTE: We do not do any DMA setup here as USBA framework
 *	does that for us.
 */
static struct scsi_pkt *
scsa2usb_scsi_init_pkt(struct scsi_address *ap,
    struct scsi_pkt *pkt, struct buf *bp, int cmdlen, int statuslen,
    int tgtlen, int flags, int (*callback)(), caddr_t arg)
{
	scsa2usb_cmd_t	 *cmd;
	scsa2usb_state_t *scsa2usbp;

	ASSERT(callback == NULL_FUNC || callback == SLEEP_FUNC);

	scsa2usbp = (scsa2usb_state_t *)ADDR2SCSA2USB(ap);

	/*
	 * allocate a pkt if none already allocated
	 */
	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* Print sync message */
	if (ddi_in_panic()) {
		SCSA2USB_PRINT_SYNC_MSG(scsa2usb_sync_message, scsa2usbp);
		/* continue so caller will not hang or complain */
	}

	if (pkt == NULL) {
		pkt = scsi_hba_pkt_alloc(scsa2usbp->scsa2usb_dip, ap, cmdlen,
				statuslen, tgtlen, sizeof (scsa2usb_cmd_t),
				callback, arg);
		if (pkt == NULL) {
			mutex_exit(&scsa2usbp->scsa2usb_mutex);

			return (NULL);
		}

		cmd = PKT2CMD(pkt);
		cmd->cmd_pkt		= pkt; /* back link to pkt */
		cmd->cmd_scblen		= statuslen;
		cmd->cmd_cdblen		= (uchar_t)cmdlen;
		cmd->cmd_tag		= scsa2usbp->scsa2usb_tag++;
		cmd->cmd_bp		= bp;
		pkt->pkt_scbp		= (opaque_t)&cmd->cmd_scb;

	} else {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb: pkt != NULL");

		/* nothing to do */
	}


	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_init_pkt: ap = 0x%p pkt: 0x%p\n\t"
	    "bp = 0x%p cmdlen = %x stlen = 0x%x tlen = 0x%x flags = 0x%x",
	    ap, pkt, bp, cmdlen, statuslen, tgtlen, flags);

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (pkt);
}


/*
 * scsa2usb_scsi_destroy_pkt:
 *	We are done with the packet. Get rid of it.
 */
static void
scsa2usb_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	scsa2usb_state_t *scsa2usbp = ADDR2SCSA2USB(ap);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_destroy_pkt");

	scsi_hba_pkt_free(ap, pkt);
}


/*
 * scsa2usb_scsi_start:
 *	For each command being issued, build up the CDB
 *	and call scsi_transport to issue the command. This
 *	function is based on the assumption that USB allows
 *	a subset of SCSI commands. Other SCSI commands we fail.
 */
static int
scsa2usb_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	scsa2usb_cmd_t *cmd = PKT2CMD(pkt);
	scsa2usb_state_t *scsa2usbp = ADDR2SCSA2USB(ap);
	struct buf *bp = cmd->cmd_bp;
	int rval;

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	if (!scsa2usbp->scsa2usb_msg_count) {
		USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_start:\n\t"
		    "bp = 0x%p ap = 0x%p pkt = 0x%p flag = 0x%x\n\tcdb0 = 0x%x "
		    "dev_state = 0x%x pkt_state = 0x%x flags = 0x%x "
		    "pipe_state = 0x%x", bp, ap, pkt, pkt->pkt_flags,
		    pkt->pkt_cdbp[0], scsa2usbp->scsa2usb_dev_state,
		    scsa2usbp->scsa2usb_pkt_state, scsa2usbp->scsa2usb_flags,
		    scsa2usbp->scsa2usb_pipe_state);
	}

	/*
	 * if we are in panic, we are in polled mode, so we can just
	 * accept the request and return
	 * if we fail this request, the rest of the file systems do not
	 * get synced
	 */
	if (ddi_in_panic()) {
		extern int do_polled_io;

		ASSERT(do_polled_io);
		scsa2usb_prepare_pkt(scsa2usbp, pkt, cmd);
		SCSA2USB_PRINT_SYNC_MSG(scsa2usb_sync_message, scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_ACCEPT);
	}

	/* if device is disconnected, set pkt_reason to incomplete */
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {
		if (!scsa2usbp->scsa2usb_msg_count++) {
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "device not accessible");
		}

		/*
		 * if we return incomplete, sd will retry
		 * a few times and give up. the user can
		 * then disconnect/reconnect to get the device
		 * back
		 */
		scsa2usb_prepare_pkt(scsa2usbp, pkt, cmd);
		scsa2usb_device_not_responding(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_ACCEPT);
	}

	/* any packet currently in transport or pipe_state not normal? */
	if (SCSA2USB_BUSY(scsa2usbp)) {
		if (!scsa2usbp->scsa2usb_msg_count++) {
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "transport busy, cur_pkt = 0x%p, pipe_state = 0x%x"
			    " pkt_state = 0x%x",
			    scsa2usbp->scsa2usb_cur_pkt,
			    scsa2usbp->scsa2usb_pipe_state,
			    scsa2usbp->scsa2usb_pkt_state);
		}

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_BUSY);
	}

	/* are we fully powered up */
	if (scsa2usb_check_power(scsa2usbp) != USB_SUCCESS) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_BUSY);
	}

	/*
	 * if the device is quiesced, let the target queue up requests and
	 * retry later
	 */
	if (scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_QUIESCED) {
		if (!scsa2usbp->scsa2usb_msg_count++) {
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle, "bus quiesced");
		}

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_BUSY);
	}

	/* we cannot do polling, this should not happen */
	if (pkt->pkt_flags & FLAG_NOINTR) {
		if (!scsa2usbp->scsa2usb_msg_count++) {
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "NOINTR packet: opcode = 0%x", pkt->pkt_cdbp[0]);
		}

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (TRAN_BADPKT);
	}

	/*
	 * prepare packet and process (subclass dependent)
	 */
	scsa2usb_prepare_pkt(scsa2usbp, pkt, cmd);
	if (scsa2usbp->scsa2usb_msg_count) {
		if (scsa2usbp->scsa2usb_msg_count > 1) {
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "the last message repeated %d times",
			    scsa2usbp->scsa2usb_msg_count);
		}
		scsa2usbp->scsa2usb_msg_count = 0;
	}

	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceSubClass) {
	case MS_SCSI_SUB_CLASS:
		rval = scsa2usb_handle_scsi_cmd_sub_class(scsa2usbp, cmd, pkt);
		break;
	default:
		rval = TRAN_FATAL_ERROR;
		break;
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


/*
 * scsa2usb_prepare_pkt:
 *	initialize some fields of the pkt and cmd
 *	(the pkt may have been resubmitted/retried)
 */
static void
scsa2usb_prepare_pkt(scsa2usb_state_t *scsa2usbp,
    struct scsi_pkt *pkt, scsa2usb_cmd_t *cmd)
{
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	pkt->pkt_reason = CMD_CMPLT;	/* Set reason to pkt_complete */
	pkt->pkt_state = 0;		/* Reset next three fields */
	pkt->pkt_statistics = 0;
	pkt->pkt_resid = 0;
	*(pkt->pkt_scbp) = STATUS_GOOD;	/* Set status to good */
	scsa2usbp->scsa2usb_cur_pkt = pkt;

	if (cmd) {
		cmd->cmd_timeout = pkt->pkt_time;
		cmd->cmd_xfercount = 0;		/* Reset the fields */
		cmd->cmd_total_xfercount = 0;
		cmd->cmd_lba = 0;
		cmd->cmd_done = 0;
		cmd->cmd_dir = 0;
		cmd->cmd_offset = 0;
		cmd->cmd_actual_len = cmd->cmd_cdblen;
	}
}


/*
 * prepare a scsa2usb_cmd and submit for transport
 */
static int
scsa2usb_handle_scsi_cmd_sub_class(scsa2usb_state_t *scsa2usbp,
    scsa2usb_cmd_t *cmd, struct scsi_pkt *pkt)
{
	int	rval;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_scsi_cmd_sub_class: cmd = 0x%p pkt = 0x%p",
	    cmd, pkt);

	bzero((void *)&cmd->cmd_cdb, SCSI_CDB_SIZE);

	cmd->cmd_cdb[SCSA2USB_OPCODE] = pkt->pkt_cdbp[0];   /* Set the opcode */
	cmd->cmd_cdb[SCSA2USB_LUN] = pkt->pkt_cdbp[1] >> 5;

	/*
	 * decode and convert the packet
	 * for most cmds, we can bcopy the cdb
	 * NOTE: not all these are needed for scsi
	 */
	switch (pkt->pkt_cdbp[0]) {
	case SCMD_DOORLOCK:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		break;

	case SCMD_FORMAT:

		/* if parameter list is specified */
		if (pkt->pkt_cdbp[1] & 0x10) {
			cmd->cmd_xfercount = 4;
			cmd->cmd_dir = CBW_DIR_OUT;
			cmd->cmd_actual_len = CDB_GROUP0;
		}

		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		break;

	case SCMD_INQUIRY:
		cmd->cmd_dir = CBW_DIR_IN;
		cmd->cmd_actual_len = CDB_GROUP0;
		cmd->cmd_cdb[SCSA2USB_LBA_2] =
		    cmd->cmd_xfercount = cmd->cmd_bp->b_bcount;
		if (cmd->cmd_bp->b_bcount > sizeof (struct scsi_inquiry)) {
			cmd->cmd_cdb[SCSA2USB_LBA_2] =
			    cmd->cmd_xfercount = sizeof (struct scsi_inquiry);
		}
		break;

	case SCMD_READ_CAPACITY:
		cmd->cmd_dir = CBW_DIR_IN;
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_xfercount = sizeof (scsa2usb_read_cap_t);
		break;

	/* SCMD_READ/SCMD_WRITE are converted to G1 cmds */
	case SCMD_READ:
	case SCMD_WRITE:
	case READ_LONG_CMD:
	case SCMD_READ_G1:
	case SCMD_WRITE_G1:
	case WRITE_LONG_CMD:

		return (scsa2usb_rw_transport(scsa2usbp, pkt));

	case SCMD_REQUEST_SENSE:
		cmd->cmd_dir = CBW_DIR_IN;
		cmd->cmd_xfercount = pkt->pkt_cdbp[4];
		cmd->cmd_cdb[SCSA2USB_LBA_2] = pkt->pkt_cdbp[4];
		cmd->cmd_actual_len = CDB_GROUP0;
		break;

	case SCMD_START_STOP:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		break;

	case SCMD_TEST_UNIT_READY:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		break;

	case SCMD_SDIAG:
		/*
		 * Needed by zip protocol to reset the device
		 */
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	case SCMD_VERIFY:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_actual_len = CDB_GROUP1;
		/* no data xfer */
		break;

	case SCMD_WRITE_VERIFY:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_dir = CBW_DIR_OUT;
		cmd->cmd_xfercount = (pkt->pkt_cdbp[7] << 8) | pkt->pkt_cdbp[8];
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	case SCMD_REZERO_UNIT:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	/*
	 * Next four commands do not have a SCSI equivalent
	 * These are listed in the ATAPI Zip specs.
	 */
	case READ_FORMAT_CAP_CMD:
		cmd->cmd_cdb[SCSA2USB_OPCODE] = READ_FORMAT_CAP_CMD;
		cmd->cmd_dir = CBW_DIR_IN;
		cmd->cmd_cdb[SCSA2USB_LEN_0] = pkt->pkt_cdbp[7];
		cmd->cmd_cdb[SCSA2USB_LEN_1] = pkt->pkt_cdbp[8];
		cmd->cmd_xfercount = (pkt->pkt_cdbp[7] << 8) | pkt->pkt_cdbp[8];
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	case TRANSLATE_LBA_CMD:
		cmd->cmd_dir = CBW_DIR_IN;
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_xfercount = (pkt->pkt_cdbp[7] << 8) | pkt->pkt_cdbp[8];
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	case CARTRIDGE_PROT_CMD:
		cmd->cmd_dir = CBW_DIR_OUT;
		/*
		 * If the length of the password is odd bytes, the ATAPI
		 * drive ignores the last byte. There is a known bug that
		 * passing odd bytes CDB or len to ATAPI may confuse the
		 * drive. Hence we are reducing the lenght by 1 here.
		 */
		cmd->cmd_xfercount = pkt->pkt_cdbp[4] & ~1;
		cmd->cmd_cdb[SCSA2USB_LBA_2] = pkt->pkt_cdbp[4];
		cmd->cmd_cdb[SCSA2USB_LBA_2] &= ~1;	/* Make it even */
		cmd->cmd_cdb[SCSA2USB_LUN] = pkt->pkt_cdbp[1];
		cmd->cmd_actual_len = CDB_GROUP0;
		break;

	case FORMAT_VERIFY_CMD:
		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		cmd->cmd_actual_len = CDB_GROUP1;
		break;

	/*
	 * Fake following two commands as drive doesn't support it.
	 * These are needed by format command.
	 */
	case SCMD_RESERVE:
	case SCMD_RELEASE:
		/* reset fields in case the pkt was resubmitted or not clean */
		scsa2usb_prepare_pkt(scsa2usbp, pkt, NULL);
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		scsa2usb_pkt_completion(scsa2usbp);

		return (TRAN_ACCEPT);

	/* just pass seek, mode sense, and mode select cmds */
	case SCMD_SEEK:
	case SCMD_MODE_SENSE:
	case SCMD_MODE_SELECT:
	case SCMD_MODE_SENSE_G1:
	case SCMD_MODE_SELECT_G1:
	default:
		/*
		 * an unknown command may be a uscsi cmd which we
		 * should let go thru without mapping
		 */
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_start: opcode = 0x%x", pkt->pkt_cdbp[0]);

		bcopy(pkt->pkt_cdbp, &cmd->cmd_cdb, cmd->cmd_cdblen);
		if (cmd->cmd_bp) {
			if (cmd->cmd_bp->b_flags & B_READ) {
				cmd->cmd_dir = CBW_DIR_IN;
			} else {
				cmd->cmd_dir = CBW_DIR_OUT;
			}
			cmd->cmd_xfercount = cmd->cmd_bp->b_bcount;
		}

		break;
	} /* end of switch */

	cmd->cmd_total_xfercount = cmd->cmd_xfercount;

	/* Set the timeout value as per command request */
	if (scsa2usb_set_timeout(scsa2usbp, pkt->pkt_time) != USB_SUCCESS) {
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_start: set pipe timeout failed");

		return (TRAN_FATAL_ERROR);
	}

	/* transport the cmd, protocol dependent */
	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol) {
	case MS_ISD_1999_SILICON_PROTOCOL:
	case MS_BULK_ONLY_PROTOCOL:
		rval = scsa2usb_bulk_only_transport(scsa2usbp, cmd);
		break;
	default:
		rval = TRAN_FATAL_ERROR;
	}

	return (rval);
}


/*
 * scsa2usb_scsi_abort:
 *	Issue SCSI abort command. This function is a NOP.
 */
/* ARGSUSED */
static int
scsa2usb_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)ADDR2SCSA2USB(ap);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_abort");

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* if device is disconnected (ie. pipes closed), fail immediately */
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (0);
	}

	/* no abort actions implemented yet */

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (0);
}


/*
 * scsa2usb_scsi_reset:
 *	device reset may turn the device into a brick and bus reset
 *	is not applicable.
 *	We return success, always.
 */
/* ARGSUSED */
static int
scsa2usb_scsi_reset(struct scsi_address *ap, int level)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)ADDR2SCSA2USB(ap);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_reset: ap = 0x%p, level = %d",
	    ap, level);

	return (1);
}


/*
 * scsa2usb_scsi_getcap:
 *	Get SCSI capabilities.
 */
/* ARGSUSED */
static int
scsa2usb_scsi_getcap(struct scsi_address *ap, char *cap, int whom)
{
	int rval = -1;
	uint_t cidx;
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)ADDR2SCSA2USB(ap);
	ASSERT(scsa2usbp);

	if (cap == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_getcap: invalid arg, "
		    "cap = 0x%p whom = %d", cap, whom);

		return (rval);
	}

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_getcap: cap = %s", cap);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* if device is disconnected (ie. pipes closed), fail immediately */
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (rval);
	}

	cidx =	scsi_hba_lookup_capstr(cap);
	switch (cidx) {
	case SCSI_CAP_GEOMETRY:
		rval = ((10 << 16) | 258);
		break;
	case SCSI_CAP_DMA_MAX:
		rval = scsa2usbp->scsa2usb_max_bulk_xfer_size;
		break;
	case SCSI_CAP_SCSI_VERSION:
		rval = SCSI_VERSION_3;
		break;
	case SCSI_CAP_INTERCONNECT_TYPE:
		rval = INTERCONNECT_USB;
		break;
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_getcap: unsupported cap = %s", cap);
		break;
	}

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_getcap: cap = %s, returned = %d", cap, rval);

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


/*
 * scsa2usb_scsi_setcap:
 *	Set SCSI capablities.
 */
/* ARGSUSED */
static int
scsa2usb_scsi_setcap(struct scsi_address *ap, char *cap, int value, int whom)
{
	int rval = -1; /* default is cap undefined */
	uint_t cidx;
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)ADDR2SCSA2USB(ap);
	ASSERT(scsa2usbp);

	if (cap == NULL || whom == 0) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_setcap: invalid arg");

		return (rval);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* if device is disconnected (ie. pipes closed), fail immediately */
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (rval);
	}

	cidx =	scsi_hba_lookup_capstr(cap);
	switch (cidx) {
	case SCSI_CAP_SECTOR_SIZE:
		if (value) {
			scsa2usbp->scsa2usb_secsz = value;
		}
		break;
	case SCSI_CAP_TOTAL_SECTORS:
		if (value) {
			scsa2usbp->scsa2usb_totalsec = value;
		}
		break;
	case SCSI_CAP_DMA_MAX:
	case SCSI_CAP_SCSI_VERSION:
	case SCSI_CAP_INTERCONNECT_TYPE:
		/* supported but not settable */
		rval = 0;
		break;
	default:
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_scsi_setcap: unsupported cap = %s", cap);
		break;
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


/*
 * scsa2usb_scsi_quiesce:
 *	Quiesce SCSI bus.
 */
static int
scsa2usb_scsi_quiesce(dev_info_t *dip)
{
	scsi_hba_tran_t *tran;
	scsa2usb_state_t *scsa2usbp;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if ((tran == NULL) || ((scsa2usbp = TRAN2SCSA2USB(tran)) == NULL)) {

		return (-1);
	}

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_quiesce: dip = 0x%p", dip);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	if (scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_QUIESCED) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (0);
	}

	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (-1);
	}

	/* Set the SCSA2USB_FLAGS_QUIESCE flag so scsi_start returns busy */
	scsa2usbp->scsa2usb_flags |= SCSA2USB_FLAGS_QUIESCED;

	/* Check if we are currently in transport and drain */
	while (SCSA2USB_BUSY(scsa2usbp)) {

		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		delay(10);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
	}

	scsa2usb_close_usb_pipes(scsa2usbp, USB_FLAGS_SLEEP);
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (0);
}


/*
 * scsa2usb_scsi_unquiesce:
 *	Unquiesce SCSI bus.
 */
static int
scsa2usb_scsi_unquiesce(dev_info_t *dip)
{
	scsi_hba_tran_t *tran;
	scsa2usb_state_t *scsa2usbp;

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	if ((tran == NULL) || ((scsa2usbp = TRAN2SCSA2USB(tran)) == NULL)) {

		return (-1);
	}

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_scsi_unquiesce: dip = 0x%p scsa2usbp: 0x%p",
	    dip, scsa2usbp);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	if ((scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) == 0) {
		if (scsa2usb_open_usb_pipes(scsa2usbp)) {
			mutex_exit(&scsa2usbp->scsa2usb_mutex);

			return (-1);
		}
	}

	/* reset the SCSA2USB_FLAGS_QUIESCE flag */
	scsa2usbp->scsa2usb_flags &= ~SCSA2USB_FLAGS_QUIESCED;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (0);
}


/*
 * scsa2usb_bulk_only_transport:
 *	Handle issuing commands to a Bulk Only device.
 *
 *	returns TRAN_* values and not USB_SUCCESS/FAILURE
 */
static int
scsa2usb_bulk_only_transport(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd)
{
	int	rval;
	uint_t	flags = USB_FLAGS_SHORT_XFER_OK|USB_FLAGS_ENQUEUE;
	mblk_t	*usb_command;
	uchar_t *c = (uchar_t *)&cmd->cmd_cdb;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	ASSERT(cmd->cmd_xfercount <= scsa2usbp->scsa2usb_max_bulk_xfer_size);

	if ((usb_command = scsa2usb_cbw_build(scsa2usbp, cmd)) == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bulk_only_transport: error no memory");

		return (TRAN_BUSY);
	}

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bulk_only_transport:\n\tcmd = 0x%p opcode=%s\n\t"
	    "len: %x %x %x %x \n\t"
	    "cdb: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", cmd,
	    scsi_cname(cmd->cmd_cdb[SCSA2USB_OPCODE], scsa2usb_cmds),
	    *(usb_command->b_rptr + 8), *(usb_command->b_rptr + 9),
	    *(usb_command->b_rptr + 0xa), *(usb_command->b_rptr + 0xb),
	    c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8],
	    c[9], c[10], c[11], c[12], c[13], c[14], c[15]);

	/*
	 * Send a Bulk Command Block Wrapper to the device
	 */
	scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_XFER_SEND_CBW;
	mutex_exit(&scsa2usbp->scsa2usb_mutex);
	if ((rval = usb_pipe_send_bulk_data(scsa2usbp->scsa2usb_bulkout_pipe,
	    usb_command, flags)) != USB_SUCCESS) {

		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bulk_only_transport: sent Bulk cmd = 0x%x "
		    " rval = %d", cmd->cmd_cdb[SCSA2USB_OPCODE], rval);

		mutex_enter(&scsa2usbp->scsa2usb_mutex);

		scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_NONE;

		/* lack of better return code */
		return (TRAN_FATAL_ERROR);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	return (TRAN_ACCEPT);
}


/*
 * scsa2usb_rw_transport:
 *	Handle splitting READ and WRITE requests to the
 *	device to a size that the host controller allows.
 *
 *	returns TRAN_* values and not USB_SUCCESS/FAILURE
 */
static int
scsa2usb_rw_transport(scsa2usb_state_t *scsa2usbp, struct scsi_pkt *pkt)
{
	scsa2usb_cmd_t *cmd = PKT2CMD(pkt);
	int lba, dir, opcode;
	size_t len, xfer_count;
	int rval;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_rw_transport:");

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	opcode = pkt->pkt_cdbp[0];
	switch (pkt->pkt_cdbp[0]) {
	case SCMD_READ:
		/*
		 * Note that READ/WRITE(6) are not supported by the drive.
		 * convert it into a 10 byte read/write.
		 */
		lba = (pkt->pkt_cdbp[2] << 8) + pkt->pkt_cdbp[3];
		len = pkt->pkt_cdbp[4];
		opcode = SCMD_READ_G1;	/* Overwrite it w/ byte 10 cmd val */
		dir = CBW_DIR_IN;
		break;
	case SCMD_WRITE:
		lba = (pkt->pkt_cdbp[2] << 8) + pkt->pkt_cdbp[3];
		len = pkt->pkt_cdbp[4];
		opcode = SCMD_WRITE_G1;	/* Overwrite it w/ byte 10 cmd val */
		dir = CBW_DIR_OUT;
		break;
	case SCMD_READ_G1:
	case READ_LONG_CMD:
		lba = (pkt->pkt_cdbp[2] << 24) + (pkt->pkt_cdbp[3] << 16) +
			(pkt->pkt_cdbp[4] << 8) +  pkt->pkt_cdbp[5];
		len = (pkt->pkt_cdbp[7] << 8) + pkt->pkt_cdbp[8];
		dir = CBW_DIR_IN;
		break;
	case SCMD_WRITE_G1:
	case WRITE_LONG_CMD:
		lba = (pkt->pkt_cdbp[2] << 24) + (pkt->pkt_cdbp[3] << 16) +
			(pkt->pkt_cdbp[4] << 8) +  pkt->pkt_cdbp[5];
		len = (pkt->pkt_cdbp[7] << 8) + pkt->pkt_cdbp[8];
		dir = CBW_DIR_OUT;
		break;
	}

	xfer_count = len * scsa2usbp->scsa2usb_lbasize;

	ASSERT((xfer_count % DEV_BSIZE) == 0);
	cmd->cmd_total_xfercount = xfer_count;
	cmd->cmd_lba = lba;

	/* reduce xfer count if necessary */
	if (xfer_count > scsa2usbp->scsa2usb_max_bulk_xfer_size) {
		xfer_count = scsa2usbp->scsa2usb_max_bulk_xfer_size;
		len = xfer_count/scsa2usbp->scsa2usb_lbasize;
	}

	cmd->cmd_xfercount = xfer_count;

	cmd->cmd_cdb[SCSA2USB_OPCODE] = (uchar_t)opcode;
	cmd->cmd_cdb[SCSA2USB_LBA_0] = lba >> 24;
	cmd->cmd_cdb[SCSA2USB_LBA_1] = lba >> 16;
	cmd->cmd_cdb[SCSA2USB_LBA_2] = lba >> 8;
	cmd->cmd_cdb[SCSA2USB_LBA_3] = (uchar_t)lba;

	cmd->cmd_cdb[SCSA2USB_LEN_0] = len >> 8;
	cmd->cmd_cdb[SCSA2USB_LEN_1] = len;
	cmd->cmd_actual_len = CDB_GROUP1;
	cmd->cmd_dir = (uchar_t)dir;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "lba = 0x%x len = 0x%x xfercount = 0x%x total = 0x%x",
	    lba, len, cmd->cmd_xfercount, cmd->cmd_total_xfercount);

	/* select transport */
	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol) {
	case MS_ISD_1999_SILICON_PROTOCOL:
	case MS_BULK_ONLY_PROTOCOL:
		rval = scsa2usb_bulk_only_transport(scsa2usbp, cmd);
		break;
	default:
		rval = TRAN_FATAL_ERROR;
	}

	return (rval);
}


/*
 * scsa2usb_setup_next_xfer:
 *	For READs and WRITEs we split up the transfer in terms
 *	of OHCI understood units. This function handles the split
 *	transfers.
 */
static int
scsa2usb_setup_next_xfer(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd)
{
	int lba = cmd->cmd_lba;
	int len = min(scsa2usbp->scsa2usb_max_bulk_xfer_size,
			cmd->cmd_total_xfercount);
	int rval;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	if (ddi_in_panic()) {

		return (USB_FAILURE);
	}

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_setup_next_xfer: opcode = 0x%x total count = 0x%x",
	    cmd->cmd_cdb[SCSA2USB_OPCODE], cmd->cmd_total_xfercount);

	ASSERT(cmd->cmd_total_xfercount > 0);
	ASSERT((len % scsa2usbp->scsa2usb_lbasize) == 0);

	cmd->cmd_xfercount = len;
	len = len/scsa2usbp->scsa2usb_lbasize;
	lba += scsa2usbp->scsa2usb_max_bulk_xfer_size/
				scsa2usbp->scsa2usb_lbasize;

	cmd->cmd_cdb[SCSA2USB_LBA_0] = lba >> 24;
	cmd->cmd_cdb[SCSA2USB_LBA_1] = lba >> 16;
	cmd->cmd_cdb[SCSA2USB_LBA_2] = lba >> 8;
	cmd->cmd_cdb[SCSA2USB_LBA_3] = (uchar_t)lba;

	cmd->cmd_cdb[SCSA2USB_LEN_0] = len >> 8;
	cmd->cmd_cdb[SCSA2USB_LEN_1] = (uchar_t)len;

	cmd->cmd_lba = lba;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_setup_next_xfer:\n\t"
	    "lba = 0x%x len = 0x%x xfercount = 0x%x total = 0x%x",
	    lba, len, cmd->cmd_xfercount, cmd->cmd_total_xfercount);

	/* select transport */
	switch (scsa2usbp->scsa2usb_intfc_descr.bInterfaceProtocol) {
	case MS_ISD_1999_SILICON_PROTOCOL:
	case MS_BULK_ONLY_PROTOCOL:
		rval = scsa2usb_bulk_only_transport(scsa2usbp, cmd);
		break;
	default:
		rval = TRAN_FATAL_ERROR;
	}

	if (rval != TRAN_ACCEPT) {
		scsa2usbp->scsa2usb_cur_pkt->pkt_reason = CMD_TRAN_ERR;
		rval = USB_FAILURE;
	} else {
		rval = USB_SUCCESS;
	}

	return (rval);
}


/*
 * scsa2usb_bp_to_mblk:
 *	Convert a bp to mblk_t. USBA framework understands
 *	mblk_t.
 */
static mblk_t *
scsa2usb_bp_to_mblk(scsa2usb_state_t *scsa2usbp)
{
	size_t		size;
	mblk_t		*mp;
	struct buf	*bp;
	scsa2usb_cmd_t	*cmd = PKT2CMD(scsa2usbp->scsa2usb_cur_pkt);

	ASSERT(scsa2usbp->scsa2usb_cur_pkt);
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	bp = cmd->cmd_bp;

	if (bp->b_bcount > 0) {
		size = ((bp->b_bcount > cmd->cmd_xfercount) ?
				cmd->cmd_xfercount : bp->b_bcount);
	} else {

		return (NULL);
	}

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bp_to_mblk:\n\tbp = 0x%p pkt = 0x%p off = 0x%x sz = %d",
	    bp, scsa2usbp->scsa2usb_cur_pkt, cmd->cmd_offset, size);

	if ((mp = allocb(sizeof (char) * size, BPRI_HI)) == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bp_to_mblk: allocb failed");

		return (NULL);
	}

	bcopy(bp->b_un.b_addr + cmd->cmd_offset, mp->b_rptr, size);
	mp->b_wptr += size;
	cmd->cmd_offset += size;

	return (mp);
}


/*
 * scsa2usb_cbw_build:
 *	Build a CBW request packet. This
 *	packet is transported to the device
 */
static mblk_t *
scsa2usb_cbw_build(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd)
{
	int	i;
	int	len;
	mblk_t	*mp;
	uchar_t dir, *cdb = (uchar_t *)(&cmd->cmd_cdb);

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	if ((mp = allocb(USB_BULK_CBWCMD_LEN, BPRI_HI)) == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_cbw_to_mblk: allocb failed");

		return (NULL);
	}

	*mp->b_wptr++ = CBW_MSB(CBW_SIGNATURE);	/* CBW Signature */;
	*mp->b_wptr++ = CBW_MID1(CBW_SIGNATURE);
	*mp->b_wptr++ = CBW_MID2(CBW_SIGNATURE);
	*mp->b_wptr++ = CBW_LSB(CBW_SIGNATURE);
	*mp->b_wptr++ = CBW_LSB(cmd->cmd_tag);	/* CBW Tag */
	*mp->b_wptr++ = CBW_MID2(cmd->cmd_tag);
	*mp->b_wptr++ = CBW_MID1(cmd->cmd_tag);
	*mp->b_wptr++ = CBW_MSB(cmd->cmd_tag);

	dir = cmd->cmd_dir;
	len = cmd->cmd_xfercount;
#ifdef	SCSA2USB_TEST
	if (scsa2usb_test_case_2 && (cdb[0] == SCMD_READ_CAPACITY)) {
		/* Host expects no data. The device wants data. Hn < Di */
		scsa2usb_test_case_2 = len = 0;
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 2: Hn < Di cdb: 0x%x len: 0x%x", cdb[0], len);
	}

	if (scsa2usb_test_case_3 && (cmd->cmd_dir == CBW_DIR_OUT)) {
		/* Host expects no data. The device wants data. Hn < Do */
		if (cdb[0] == SCMD_WRITE_G1) {
			scsa2usb_test_case_3 = len = 0;
			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "TEST 3: Hn < Do cdb: 0x%x len:%x", cdb[0], len);
		}
	}

	if (scsa2usb_test_case_4 && (cdb[0] == SCMD_READ_G1)) {
		cdb[0] = 0x5e;
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 4: Hi > Dn: changed cdb to 0x%x", cdb[0]);
		scsa2usb_test_case_4 = 0;
	}

	if (scsa2usb_test_case_7 && (cmd->cmd_cdb[0] == SCMD_READ_G1)) {
		len -= 0x10;
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 7: Hi < Di cdb: 0x%x len: 0x%x", cdb[0], len);
		scsa2usb_test_case_7 = 0;
	}

	if (scsa2usb_test_case_8 && (cdb[0] == SCMD_READ_G1)) {
		dir = (dir == CBW_DIR_IN) ? CBW_DIR_OUT : dir;
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 8: Hi <> Do cdb: 0x%x dir: 0x%x", cdb[0], dir);
	}

	if (scsa2usb_test_case_9 && (cdb[0] == SCMD_WRITE_G1)) {
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 9: Ho <> Di (%x)", cdb[0]);
		cdb[SCSA2USB_LEN_0] = cdb[SCSA2USB_LEN_1] = 0;
		scsa2usb_test_case_9 = 0;
	}

	if (scsa2usb_test_case_10 && (cdb[0] == SCMD_WRITE_G1)) {
		dir = (dir == CBW_DIR_OUT) ? CBW_DIR_IN : dir;
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "TEST 10: Ho <> Di cdb: 0x%x dir: 0x%x", cdb[0], dir);
	}

	/*
	 * This case occues when the device intends to receive
	 * more data from the host than the host sends.
	 */
	if (scsa2usb_test_case_13) {
		if ((cdb[0] == SCMD_WRITE_G1) || (cdb[0] == SCMD_READ_G1)) {
			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb: TEST 13: Ho < Do");

			len -= 30;
			scsa2usb_test_case_13 = 0;
		}
	}
#endif	/* SCSA2USB_TEST */

	*mp->b_wptr++ = CBW_MSB(len);		/* Transfer Length */
	*mp->b_wptr++ = CBW_MID1(len);
	*mp->b_wptr++ = CBW_MID2(len);
	*mp->b_wptr++ = CBW_LSB(len);

	*mp->b_wptr++ = dir;			/* Transfer Direction */
	*mp->b_wptr++ = cmd->cmd_pkt->pkt_address.a_lun;	/* Lun # */
	*mp->b_wptr++ = cmd->cmd_actual_len;			/* CDB Len */

	/* Copy the CDB out */
	for (i = 0; i < CBW_CDB_LEN; i++) {
		*mp->b_wptr++ = *cdb++;
	}

	return (mp);
}


/*
 * Pipe Callback Handlers:
 *
 * scsa2usb_close_usb_pipes_cb:
 *	callback for closing all usb pipes. if all pipes are closed, do
 *	the pkt completion if needed
 */
/*ARGSUSED*/
static void
scsa2usb_close_usb_pipes_cb(usb_opaque_t arg, int error_code, uint_t flags)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_close_usb_pipes_cb: scsa2usb_state = 0x%p", scsa2usbp);

	ASSERT(error_code == 0);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	ASSERT(scsa2usbp->scsa2usb_pipe_state != SCSA2USB_PIPE_NORMAL);

	/* all pipes closed? */
	if ((scsa2usbp->scsa2usb_bulkout_pipe == NULL) &&
	    (scsa2usbp->scsa2usb_bulkin_pipe == NULL) &&
	    (scsa2usbp->scsa2usb_default_pipe == NULL)) {

		scsa2usbp->scsa2usb_pipe_state = SCSA2USB_PIPE_NORMAL;

		if (scsa2usbp->scsa2usb_cur_pkt) {
			SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
			scsa2usbp->scsa2usb_cur_pkt->pkt_reason = CMD_TRAN_ERR;
			scsa2usb_pkt_completion(scsa2usbp);
		}
	}
	mutex_exit(&scsa2usbp->scsa2usb_mutex);
}


/*
 *
 * scsa2usb_pipes_reset_cb:
 *	pipe reset callbacks. If all callbacks have been done,
 *	complete the packet
 */
static void
scsa2usb_pipes_reset_cb(usb_opaque_t arg, int error, uint_t flags)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/*
	 * the system has paniced: give up.
	 */
	if (ddi_in_panic()) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return;
	}

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_pipes_reset_cb: "
	    "error = 0x%x, flags = 0x%x, pipe_state = 0x%x pkt_state = 0x%x",
	    error, flags,
	    scsa2usbp->scsa2usb_pipe_state, scsa2usbp->scsa2usb_pkt_state);
	ASSERT(error == 0);

	if (SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp)) {
		scsa2usb_handle_stall_reset_cleanup(scsa2usbp);
	} else {
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);
}


/*
 * scsa2usb_bulk_only_ex_cb:
 *	Exception callback routine for bulk only devices.
 *	Resets the pipe. If there is any data, destroy it.
 *	Check state machine comments below describing actions on exceptions
 */
/*ARGSUSED*/
static int
scsa2usb_bulk_only_ex_cb(usb_pipe_handle_t pipe,
	usb_opaque_t arg, uint_t completion_reason, mblk_t *data, uint_t flag)
{
	int		rval = USB_SUCCESS;
	int		in_ept_addr, out_ept_addr;
	boolean_t	data_handled;
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;
	scsa2usb_cmd_t *cmd;
	usb_pipe_handle_t ph;

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	ph = (usb_pipe_handle_t)scsa2usbp->scsa2usb_default_pipe;

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bulk_only_ex_cb:\n\t"
	    "reason = 0x%x flag = 0x%x data = 0x%p state = 0x%x ph = 0x%p",
	    completion_reason, flag, data, scsa2usbp->scsa2usb_pkt_state, ph);

	/*
	 * the system has paniced. try recovery asap.
	 */
	if (ddi_in_panic()) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	/*
	 * the device is hosed? recover.
	 */
	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	/*
	 * If !scsa2usb_cur_pkt, return.
	 */
	if (!scsa2usbp->scsa2usb_cur_pkt) {
		SCSA2USB_FREE_MSG(data);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	cmd = PKT2CMD(scsa2usbp->scsa2usb_cur_pkt);

	data_handled = B_FALSE;
	out_ept_addr = scsa2usbp->scsa2usb_bulkout_ept.bEndpointAddress;
	in_ept_addr = scsa2usbp->scsa2usb_bulkin_ept.bEndpointAddress;

	/* Handle stalls here. Implement the 13 cases here as well */
	switch (completion_reason) {
	case USB_CC_STALL:
	{
		switch (scsa2usbp->scsa2usb_pkt_state) {
		case SCSA2USB_PKT_XFER_SEND_CBW:
			/*
			 * a stall on CBW xfer indicates a bad command
			 * has been submitted. we have to reset the device
			 * and clear stall + pipes afterwards
			 */
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_bulk_only_ex_cb: bad command");

			SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
			if (scsa2usbp->scsa2usb_cur_pkt) {
				scsa2usbp->scsa2usb_cur_pkt->pkt_reason =
							CMD_TRAN_ERR;
			}

			rval = scsa2usb_reset_recovery(scsa2usbp);
			ASSERT(rval == USB_SUCCESS);
			break;
		case SCSA2USB_PKT_XFER_DATA:
		{
			/*
			 * a stall after xfer data indicates a data
			 * over or underrun
			 * clearing the stall is sufficient. continue
			 * with status afterwards
			 *
			 * if the data phase doesn't complete then
			 * we end up getting a non-null mblk.
			 * We need to re-adjust the cmd_total_xfercount
			 * value to reflect the true (residue).
			 * Anyway, this "data" needs to be discarded?
			 */
			switch (cmd->cmd_dir)
			case CBW_DIR_IN:
				/*
				 * We manipulate cmd->cmd_total_xfercount
				 * value in scsa2usb_handle_data_done(). In
				 * scsa2usb_handle_csw_result() we compare the
				 * residue with cmd->cmd_total_xfercount. If
				 * these don't match, we do a reset recovery.
				 *
				 * That reset recovery is unnecessary. So,
				 * here manipulate cmd->cmd_total_xfercount
				 * value so that when we are in
				 * scsa2usb_handle_csw_result(), we don't do
				 * a reset recovery.
				 */
				if (data &&
				    ((data->b_wptr - data->b_rptr) > 0)) {
					cmd->cmd_total_xfercount +=
						(data->b_wptr - data->b_rptr);
					data_handled = B_TRUE;
				}

				/* handle data received so far */
				(void) scsa2usb_handle_data_done(scsa2usbp,
				    cmd, data);

				/* no more data to transfer after this */
				cmd->cmd_done = 1;

				/* Set state to handle CSW_1 case */
				scsa2usbp->scsa2usb_pkt_state =
						SCSA2USB_PKT_RECEIVE_CSW_1;

				/* start recovery */
				scsa2usbp->scsa2usb_pipe_state |=
						SCSA2USB_PIPE_BULK_IN_RESET;

				rval = scsa2usb_clear_ept_stall(scsa2usbp,
					in_ept_addr, "bulkin");
				ASSERT(rval == USB_SUCCESS);

				break;
			case CBW_DIR_OUT:
				/* Set state to handle CSW_1 case */
				scsa2usbp->scsa2usb_pkt_state =
						SCSA2USB_PKT_RECEIVE_CSW_1;
				/* start recovery */
				scsa2usbp->scsa2usb_pipe_state |=
						SCSA2USB_PIPE_BULK_OUT_RESET;

				rval = scsa2usb_clear_ept_stall(scsa2usbp,
					out_ept_addr, "bulkout");
				ASSERT(rval == USB_SUCCESS);
				break;
		}
		case SCSA2USB_PKT_PROCESS_CSW_1:
			/*
			 * a stall on status: just clear it and retry once more
			 */
			scsa2usbp->scsa2usb_pkt_state =
					SCSA2USB_PKT_RECEIVE_CSW_2;
			scsa2usbp->scsa2usb_pipe_state |=
					SCSA2USB_PIPE_BULK_IN_RESET;
			rval = scsa2usb_clear_ept_stall(scsa2usbp,
					in_ept_addr, "bulkin");
			ASSERT(rval == USB_SUCCESS);
			break;
		case SCSA2USB_PKT_PROCESS_CSW_2:
			/*
			 * a 2nd stall on receiving status. we are hosed
			 */
			SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
			rval = scsa2usb_reset_recovery(scsa2usbp);
			ASSERT(rval == USB_SUCCESS);

		} /* end of switch on stall handling */
	}
	break;

	case USB_CC_TIMEOUT:
		scsa2usbp->scsa2usb_cur_pkt->pkt_reason = CMD_TIMEOUT;
		scsa2usbp->scsa2usb_cur_pkt->pkt_statistics |=
					STAT_TIMEOUT;
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		rval = scsa2usb_reset_pipes(scsa2usbp);
		ASSERT(rval == USB_SUCCESS);

		break;
	case USB_CC_DEV_NOT_RESP:
		SCSA2USB_FREE_MSG(data);	/* Free data */
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	default:
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		scsa2usbp->scsa2usb_cur_pkt->pkt_reason = CMD_TRAN_ERR;

		/* start reset recovery to at least clear the pipes */
		rval = scsa2usb_reset_recovery(scsa2usbp);
		ASSERT(rval == USB_SUCCESS);
		break;
	}

	/*
	 * In data phase we xferred data. Need to adjust length here.
	 */
	if (data && (data_handled == B_FALSE)) {
		uint_t len = (data->b_wptr - data->b_rptr);
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bulk_only_ex_cb: data len = 0x%x total = 0x%x",
		    len, cmd->cmd_total_xfercount);

		if (cmd->cmd_total_xfercount && (len > 0)) {
			cmd->cmd_total_xfercount -= len;
		}
	}

	SCSA2USB_FREE_MSG(data);	/* Free data */
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (USB_SUCCESS);
}


/*
 * scsa2usb_bulk_only_cb:
 *	Callback function for the bulk pipes.
 *	It implements the state machine for each phase of the transport
 */
/*ARGSUSED*/
static int
scsa2usb_bulk_only_cb(usb_pipe_handle_t pipe, usb_opaque_t arg, mblk_t *data)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bulk_only_cb:\n\tph = 0x%p, arg = 0x%p, data = 0x%p",
	    pipe, arg, data);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	if (SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp)) {
		scsa2usb_bulk_only_state_machine(scsa2usbp, data);
	} else {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (USB_SUCCESS);
}


/*
 * handle callbacks on stall and reset pipe
 */
static void
scsa2usb_handle_stall_reset_cleanup(scsa2usb_state_t *scsa2usbp)
{
	int	rval;
	usb_pipe_handle_t ph = NULL;
	uint_t	ept_addr;
	char	*what;

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_stall_reset_cleanup:\n\t"
	    "pkt_state = 0x%x, pipe_state = 0x%x",
	    scsa2usbp->scsa2usb_pkt_state, scsa2usbp->scsa2usb_pipe_state);

	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);

		/* nothing to do */
		return;
	}

	/*
	 * clear stalls, one at the time
	 * we reset the flag first so if it fails, we are hosed
	 */
	if (scsa2usbp->scsa2usb_pipe_state &
	    SCSA2USB_PIPE_BULK_IN_CLEAR_STALL) {
		scsa2usbp->scsa2usb_pipe_state &=
					~SCSA2USB_PIPE_BULK_IN_CLEAR_STALL;
		scsa2usbp->scsa2usb_pipe_state |= SCSA2USB_PIPE_BULK_IN_RESET;
		ph = scsa2usbp->scsa2usb_bulkin_pipe;
		ept_addr = scsa2usbp->scsa2usb_bulkin_ept.bEndpointAddress;
		what = "bulkin";
	} else if (scsa2usbp->scsa2usb_pipe_state &
	    SCSA2USB_PIPE_BULK_OUT_CLEAR_STALL) {
		scsa2usbp->scsa2usb_pipe_state &=
					~SCSA2USB_PIPE_BULK_OUT_CLEAR_STALL;
		scsa2usbp->scsa2usb_pipe_state |= SCSA2USB_PIPE_BULK_OUT_RESET;
		ept_addr = scsa2usbp->scsa2usb_bulkout_ept.bEndpointAddress;
		ph = scsa2usbp->scsa2usb_bulkout_pipe;
		what = "bulkout";
	}

	if (ph) {
		/*
		 * initiate clearing stalls.
		 * wait for the callback to clear more stalls or
		 * start pipe resets
		 */
		rval = scsa2usb_clear_ept_stall(scsa2usbp, ept_addr, what);
		ASSERT(rval == USB_SUCCESS);

	} else if (scsa2usbp->scsa2usb_pipe_state &
	    SCSA2USB_PIPE_BULK_IN_RESET) {
		ph = scsa2usbp->scsa2usb_bulkin_pipe;
		scsa2usbp->scsa2usb_pipe_state &= ~SCSA2USB_PIPE_BULK_IN_RESET;
		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		rval = usb_pipe_reset(ph, 0, scsa2usb_pipes_reset_cb,
							(void *)scsa2usbp);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_stall_reset_cleanup: bulk in pipe reset "
		    "rval = 0x%d", rval);

	} else	if (scsa2usbp->scsa2usb_pipe_state &
	    SCSA2USB_PIPE_BULK_OUT_RESET) {
		ph = scsa2usbp->scsa2usb_bulkout_pipe;
		scsa2usbp->scsa2usb_pipe_state &= ~SCSA2USB_PIPE_BULK_OUT_RESET;
		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		rval = usb_pipe_reset(ph, 0, scsa2usb_pipes_reset_cb,
							(void *)scsa2usbp);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_stall_reset_cleanup: bulk out pipe reset "
		    "rval = 0x%d", rval);

	} else if (scsa2usbp->scsa2usb_pipe_state == SCSA2USB_PIPE_NORMAL) {
		/*
		 * if pipe state is normal then we can continue with the state
		 * machine
		 */
		scsa2usb_bulk_only_state_machine(scsa2usbp, (mblk_t *)NULL);
	}
}


/*
 * default pipe exception callback
 */
/* ARGSUSED */
static int
scsa2usb_default_pipe_ex_cb(usb_pipe_handle_t pipe, usb_opaque_t arg,
	uint_t completion_reason, mblk_t *data, uint_t flag)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_default_pipe_ex_cb: pipe = 0x%p, data = 0x%p cr = 0x%x",
	    pipe, (void *)data, completion_reason);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp)) ||
	    (completion_reason == USB_CC_DEV_NOT_RESP)) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	scsa2usbp->scsa2usb_pipe_state |= SCSA2USB_PIPE_DEFAULT_RESET;

	/* we are hosed so complete after the recovery */
	SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (scsa2usb_default_pipe_cb(pipe, arg, data));
}


/*
 * scsa2usb_default_pipe_cb:
 *	callback on the default pipe after clearing a stall on a pipe.
 *	reset pipe
 *	no need to do a pkt completion here. there should always be
 *	some pipe reset callbacks after this.
 */
static int
scsa2usb_default_pipe_cb(usb_pipe_handle_t pipe, usb_opaque_t arg, mblk_t *data)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/*
	 * the system has paniced. try recovery asap.
	 */
	if (ddi_in_panic()) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	if (!(SCSA2USB_DEVICE_ACCESS_OK(scsa2usbp))) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		scsa2usb_close_usb_pipes(scsa2usbp, 0);
		scsa2usb_device_not_responding(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (USB_SUCCESS);
	}

	if (scsa2usbp->scsa2usb_pipe_state & SCSA2USB_PIPE_DEV_RESET) {

		/* NOTE: is there a need to delay accessing the device? */
		scsa2usbp->scsa2usb_pipe_state &= ~SCSA2USB_PIPE_DEV_RESET;
	}

	USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_default_pipe_cb: pipe = 0x%p, data = 0x%p state = 0x%x",
	    pipe, (void *)data, scsa2usbp->scsa2usb_pipe_state);

	/* we must have a non-zero pipe state */
	ASSERT(scsa2usbp->scsa2usb_pipe_state);

	/* reset default pipe first if it needs it */
	if (scsa2usbp->scsa2usb_pipe_state & SCSA2USB_PIPE_DEFAULT_RESET) {
		int rval;
		usb_pipe_handle_t ph = scsa2usbp->scsa2usb_default_pipe;

		scsa2usbp->scsa2usb_pipe_state &= ~SCSA2USB_PIPE_DEFAULT_RESET;

		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		rval = usb_pipe_reset(ph, 0, scsa2usb_pipes_reset_cb,
							(void *)scsa2usbp);
		ASSERT(rval == USB_SUCCESS);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_stall_reset_cleanup: default pipe reset "
		    "rval = 0x%d", rval);
	} else {
		scsa2usb_handle_stall_reset_cleanup(scsa2usbp);
	}

	SCSA2USB_FREE_MSG(data);	/* Free data */

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (USB_SUCCESS);
}


/*
 * scsa2usb_bulk_only_state_machine:
 *
 * scsa2usb_state_machine() only handles the normal transitions
 * or continuation after clearing stalls or error recovery.
 * the bulk exception callback handles the exception cases.
 *
 * there are two variables: scsa2usb_pkt_state and scsa2usb_pipe_state.
 * the transport is controlled by pkt_state, the error recovery by pipe_state.
 * state transitions are made in callbacks
 * this state machine implements the notorious "13 cases".
 *
 *
 *  SCSA2USB_PKT_NONE:
 *	prepare valid CBW and transport it on bulk-out pipe
 *	new pkt state is SCSA2USB_PKT_XFER_SEND_CBW
 *
 *
 * SCSA2USB_PKT_XFER_SEND_CBW:
 *	if stall on bulkout:
 *		set pkt_reason to CMD_TRAN_ERR
 *		new pkt state is SCSA2USB_PKT_DO_COMP
 *		reset recovery
 *
 *	else if other exception
 *		set pkt_reason to CMD_TRAN_ERR
 *		reset recovery
 *
 *	else if no data:
 *		new pkt state is SCSA2USB_PKT_RECEIVE_CSW_1
 *		setup receiving status on bulkin
 *
 *	else if data in:
 *		new pkt state is SCSA2USB_PKT_XFER_DATA
 *		setup data in on bulkin
 *
 *	else if data out:
 *		new pkt state is SCSA2USB_PKT_XFER_DATA
 *		setup data out on bulkout
 *
 * SCSA2USB_PKT_XFER_DATA: (in)
 *	copy data transferred so far, no more data to transfer
 *
 *	if stall on bulkin pipe
 *		new pkt state is SCSA2USB_PKT_RECEIVE_CSW_1
 *		new pipe state is SCSA2USB_PIPE_BULK_IN_RESET
 *		terminate data transfers, set cmd_done
 *		clear stall on bulkin
 *
 *	else if other exception
 *		set pkt_reason to CMD_TRAN_ERR
 *		new pkt state is SCSA2USB_PKT_DO_COMP
 *		reset recovery
 *
 *	else
 *		new state is SCSA2USB_PKT_PROCESS_CSW_1
 *		setup receiving status on bulkin
 *
 *
 * SCSA2USB_PKT_XFER_DATA: (out)
 *	if stall on bulkout pipe
 *		new pkt state is SCSA2USB_PKT_RECEIVE_CSW_1
 *		new pipe state is SCSA2USB_PIPE_BULK_OUT_RESET
 *		terminate data transfers, set cmd_done
 *		clear stall on bulkout
 *
 *	else if other exception
 *		set pkt_reason to CMD_TRAN_ERR
 *		new pkt state is SCSA2USB_PKT_DO_COMP
 *		reset recovery
 *
 *	else
 *		new pkt state is SCSA2USB_PKT_PROCESS_CSW_1
 *		setup receiving status on bulkin
 *
 *
 * SCSA2USB_PKT_RECEIVE_CSW_1: (first attempt)
 *	new pkt state is SCSA2USB_PKT_PROCESS_CSW_1
 *	setup receiving status on bulkin
 *
 *
 *  SCSA2USB_PKT_RECEIVE_CSW_2: (2nd attempt)
 *	new pkt state is SCSA2USB_PKT_PROCESS_CSW_2
 *	setup receiving status on bulkin
 *
 * SCSA2USB_PKT_PROCESS_CSW_2:
 *	if stall
 *		new pkt state is SCSA2USB_PKT_DO_COMP
 *		reset recovery, we are hosed.
 *	else
 *		goto check CSW
 *
 *  SCSA2USB_PKT_PROCESS_CSW_1:
 *	if stall
 *		new pkt state is SCSA2USB_PKT_RECEIVE_CSW_2
 *		clear stall on bulkin
 *
 *	else check CSW
 *		- check length equals 13
 *		- check signature
 *		- matching tag
 *		- check status is less than or equal to 2
 *		- check residue is less than or equal to data length
 *			adjust residue based on if we got valid data
 *			back in exception callback handler.
 *		if not OK
 *			new pkt state is SCSA2USB_PKT_DO_COMP
 *			set pkt reason CMD_TRAN_ERR
 *			reset recovery, we are hosed
 *
 *		else if phase error
 *			new pkt state is SCSA2USB_PKT_DO_COMP
 *			set pkt reason CMD_TRAN_ERR
 *			reset recovery
 *
 *		else if (status < 2)
 *			if status is equal to 1
 *				set check condition
 *
 *			if residue
 *				calculate residue from data xferred and
 *					DataResidue
 *				set pkt_residue
 *
 *			goto  SCSA2USB_PKT_DO_COMP
 *
 * SCSA2USB_PKT_DO_COMP:
 *	set new pkt state to SCSA2USB_PKT_NONE
 *	do callback
 *
 * On each callback we check whether the device has been disconnected
 * or not responding. If so, we close the pipes and do the callback.
 *
 * The reset recovery walks sequentially thru device reset, clearing
 * stalls and reset pipes. When the reset recovery completes we return
 * to the state machine.
 * Clearing stalls clears the stall condition, resets the pipe, and
 * then returns to the state machine.
 */
/*ARGSUSED*/
static void
scsa2usb_bulk_only_state_machine(scsa2usb_state_t *scsa2usbp, mblk_t *data)
{
	struct scsi_pkt *pkt;
	scsa2usb_cmd_t	*cmd;
	int rval = USB_SUCCESS;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bulk_only_state_machine:\n\t"
	    "pkt state = 0x%x pipe state = 0x%x",
	    scsa2usbp->scsa2usb_pkt_state, scsa2usbp->scsa2usb_pipe_state);

	pkt = scsa2usbp->scsa2usb_cur_pkt;
	if (!pkt) {

		return;
	}

	/*
	 * the system has paniced. try recovery asap.
	 */
	if (ddi_in_panic()) {
		SCSA2USB_FREE_MSG(data);	/* Free data */
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		scsa2usb_pkt_completion(scsa2usbp);

		return;
	}

	cmd = PKT2CMD(pkt);	/* Set cmd */

	switch (scsa2usbp->scsa2usb_pkt_state) {
	default:
	case SCSA2USB_PKT_NONE:
		break;
	case SCSA2USB_PKT_XFER_SEND_CBW:
		/*
		 * We should set state after data transfer is started
		 * as the I/O to the bulk-in/out pipe may fail.
		 * By setting the state earlier we are lying about the
		 * the current state.
		 */
		if (cmd->cmd_xfercount) {
			scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_XFER_DATA;
			rval = scsa2usb_handle_data_start(scsa2usbp, cmd);
		} else {
			scsa2usbp->scsa2usb_pkt_state =
						SCSA2USB_PKT_PROCESS_CSW_1;
			rval = scsa2usb_handle_status_start(scsa2usbp);
		}
		break;
	case SCSA2USB_PKT_XFER_DATA:
		rval = scsa2usb_handle_data_done(scsa2usbp, cmd, data);
		if (rval == USB_SUCCESS) {
			scsa2usbp->scsa2usb_pkt_state =
						SCSA2USB_PKT_PROCESS_CSW_1;
			rval = scsa2usb_handle_status_start(scsa2usbp);
		}
		break;

	/*
	 * We maintain two states for receiving CSW, as there is a chance
	 * of being stuck in an infinite loop servicing exception callbacks
	 * and handling the infamous 13 cases.
	 * By having two states we get a chance to recover from it.
	 */
	case SCSA2USB_PKT_RECEIVE_CSW_1:
		scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_PROCESS_CSW_1;
		rval = scsa2usb_handle_status_start(scsa2usbp);
		break;
	case SCSA2USB_PKT_RECEIVE_CSW_2:
		scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_PROCESS_CSW_2;
		rval = scsa2usb_handle_status_start(scsa2usbp);
		break;
	case SCSA2USB_PKT_PROCESS_CSW_1:
	case SCSA2USB_PKT_PROCESS_CSW_2:
		rval = scsa2usb_handle_csw_result(scsa2usbp, data);

		if ((rval == USB_SUCCESS) && (pkt->pkt_reason == CMD_CMPLT) &&
		    (cmd->cmd_xfercount != 0) && !cmd->cmd_done) {
			rval = scsa2usb_setup_next_xfer(scsa2usbp, cmd);
		}
	} /* end of switch */

	SCSA2USB_FREE_MSG(data);	/* Free data */

	/* complete the packet if we are done or encountered error */
	if ((rval != USB_SUCCESS) ||
	    (scsa2usbp->scsa2usb_pkt_state == SCSA2USB_PKT_DO_COMP)) {
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		if (ddi_in_panic()) {
			pkt->pkt_reason = CMD_CMPLT;
			scsa2usb_pkt_completion(scsa2usbp);

			return;
		}

		if ((rval != USB_SUCCESS) && (pkt->pkt_reason != CMD_CMPLT)) {
			pkt->pkt_reason = CMD_TRAN_ERR;
		}

		scsa2usb_pkt_completion(scsa2usbp);
	}

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_bulk_only_state_machine: rval = %d", rval);
}


/*
 * State machine handlers:
 *
 * scsa2usb_handle_data_start:
 *	Initiate the data xfer. It could be IN/OUT direction.
 *	NOTE: We call this function only if there is xfercount.
 */
static int
scsa2usb_handle_data_start(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd)
{
#ifdef	SCSA2USB_TEST
	uint_t	flags = 0;
#else
	uint_t	flags = USB_FLAGS_SHORT_XFER_OK|USB_FLAGS_ENQUEUE;
#endif	/* SCSA2USB_TEST */
	int rval = USB_SUCCESS;
	mblk_t	*write_mblk;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_data_start: cmd = 0x%p, opcode = %s", cmd,
	    scsi_cname(cmd->cmd_cdb[SCSA2USB_OPCODE], scsa2usb_cmds));

	switch (cmd->cmd_dir) {
	case CBW_DIR_IN:
#ifdef	SCSA2USB_TEST
		/*
		 * This cases occurs when the host expects to receive
		 * more data than the device actually transfers. Hi > Di
		 */
		if (scsa2usb_test_case_5) {
			mutex_exit(&scsa2usbp->scsa2usb_mutex);
			rval = usb_pipe_receive_bulk_data(
				    scsa2usbp->scsa2usb_bulkin_pipe,
				    cmd->cmd_xfercount - 1, flags);
			rval = usb_pipe_receive_bulk_data(
				    scsa2usbp->scsa2usb_bulkin_pipe,
				    cmd->cmd_xfercount + 2, flags);
			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "TEST 5: Hi > Di: rval = 0x%x", rval);
			scsa2usb_test_case_5 = 0;
			break;
		}

		/*
		 * This happens when the host expects to send data to the
		 * device while the device intends to send data to the host.
		 */
		if (scsa2usb_test_case_8 && (cmd->cmd_cdb[0] == SCMD_READ_G1)) {
			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "TEST 8: Hi <> Do: Step 2");
			scsa2usb_test_mblk(scsa2usbp, B_TRUE);
			scsa2usb_test_case_8 = 0;
			break;
		}
#endif	/* SCSA2USB_TEST */

		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		rval = usb_pipe_receive_bulk_data(
		    scsa2usbp->scsa2usb_bulkin_pipe, cmd->cmd_xfercount, flags);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);

		USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_data_start: bulk in data, cmd = %s "
		    "rval = %d len = %x",
		    scsi_cname(cmd->cmd_cdb[SCSA2USB_OPCODE], scsa2usb_cmds),
		    rval, cmd->cmd_xfercount);
		break;

	case CBW_DIR_OUT:
#ifdef	SCSA2USB_TEST
		/*
		 * This happens when the host expects to receive data
		 * from the device while the device intends to receive
		 * data from the host.
		 */
		if (scsa2usb_test_case_10 &&
		    (cmd->cmd_cdb[0] == SCMD_WRITE_G1)) {
			mutex_exit(&scsa2usbp->scsa2usb_mutex);
			rval = usb_pipe_receive_bulk_data(
			    scsa2usbp->scsa2usb_bulkin_pipe, CSW_LEN, flags);
			mutex_enter(&scsa2usbp->scsa2usb_mutex);

			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "TEST 10: Ho <> Di: done rval = 0x%x",  rval);
			scsa2usb_test_case_10 = 0;
			break;
		}
#endif	/* SCSA2USB_TEST */

		write_mblk = scsa2usb_bp_to_mblk(scsa2usbp);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		rval = usb_pipe_send_bulk_data(
		    scsa2usbp->scsa2usb_bulkout_pipe, write_mblk, flags);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);

#ifdef	SCSA2USB_TEST
		if (scsa2usb_test_case_11) {
			/*
			 * Host expects to send data to the device and
			 * device doesn't expect to receive any data
			 */
			USB_DPRINTF_L1(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle, "TEST 11: Ho > Do");

			scsa2usb_test_mblk(scsa2usbp, B_FALSE);
			scsa2usb_test_case_11 = 0;
		}
#endif	/* SCSA2USB_TEST */

		USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_data_start: bulk out data "
		    " rval = %d cmd = %s", rval,
		    scsi_cname(cmd->cmd_cdb[SCSA2USB_OPCODE], scsa2usb_cmds));
		break;
	}

	return (rval);
}


/*
 * scsa2usb_handle_data_done:
 *	This function handles the completion of the data xfer.
 *	It also massages the inquiry data. This function may
 *	also be called after a stall.
 */
static int
scsa2usb_handle_data_done(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd,
	mblk_t *data)
{
	struct	buf	*bp;
	int	rval = USB_SUCCESS;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_data_done:\n\tcmd = 0x%p data = 0x%p len = 0x%x",
	    cmd, data, (data ? (data->b_wptr - data->b_rptr) : 0));

	if (data)  {
		int len = data->b_wptr - data->b_rptr;
		int off = 0;
		uchar_t	*p;
		scsa2usb_read_cap_t *cap;

		switch (cmd->cmd_cdb[SCSA2USB_OPCODE]) {
		case SCMD_INQUIRY:
			bp = cmd->cmd_bp;
			off = len - SCSA2USB_SERIAL_LEN;

			/* Copy the inquiry data */
			bcopy(data->b_rptr, bp->b_un.b_addr, off);

			/* Copy the additional serial number */
			if ((off + SCSA2USB_SERIAL_LEN) <= len) {
				bcopy(scsa2usbp->scsa2usb_serial_no,
				    bp->b_un.b_addr + off, SCSA2USB_SERIAL_LEN);
			}
			cmd->cmd_done = 1;
			break;

		case SCMD_READ_CAPACITY:
			cap = (scsa2usb_read_cap_t *)data->b_rptr;

			/* Figure out the logical block size */
			if (len >= sizeof (struct scsa2usb_read_cap)) {
				scsa2usbp->scsa2usb_lbasize =
					SCSA2USB_INT(
					    cap->scsa2usb_read_cap_blen3,
					    cap->scsa2usb_read_cap_blen2,
					    cap->scsa2usb_read_cap_blen1,
					    cap->scsa2usb_read_cap_blen0);

			}
			cmd->cmd_done = 1;
			goto copy_data;

		case SCMD_REQUEST_SENSE:
			p = data->b_rptr;
			USB_DPRINTF_L4(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "rqsense data: len = %d, data:\n\t"
			    "%x %x %x %x %x %x %x %x %x %x\n\t"
			    "%x %x %x %x %x %x %x %x %x %x", len,
			    p[0], p[1], p[2], p[3], p[4],
			    p[5], p[6], p[7], p[8], p[9],
			    p[10], p[11], p[12], p[13], p[14],
			    p[15], p[16], p[17], p[18], p[19]);

			cmd->cmd_done = 1;
			/* FALLTHROUGH */

		default:
copy_data:
			if (cmd->cmd_dir == CBW_DIR_IN) {
				bp = cmd->cmd_bp;
				len = min(len, bp->b_bcount);
				bcopy(data->b_rptr,
				    bp->b_un.b_addr + cmd->cmd_offset, len);
			}
			cmd->cmd_total_xfercount -= len;
			if (cmd->cmd_total_xfercount == 0) {
				cmd->cmd_done = 1;
			}
			cmd->cmd_offset += len;

			/* short xfer? */
			if (len < cmd->cmd_xfercount) {
				cmd->cmd_done = 1;
			}
			break;
		}

	} else {
		if (cmd->cmd_dir == CBW_DIR_OUT) {
			/* assume that all data was xferred */
			cmd->cmd_total_xfercount -= cmd->cmd_xfercount;
			if (cmd->cmd_total_xfercount == 0) {
				cmd->cmd_done = 1;
			}
		}
	}

	return (rval);
}


/*
 * scsa2usb_handle_status_start:
 *	Receive status data
 */
static int
scsa2usb_handle_status_start(scsa2usb_state_t *scsa2usbp)
{
	int rval = USB_SUCCESS;
#ifdef	SCSA2USB_TEST
	uint_t	flags = 0;
#else
	uint_t	flags = USB_FLAGS_SHORT_XFER_OK|USB_FLAGS_ENQUEUE;
#endif	/* SCSA2USB_TEST */

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_status_start:");

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	/* setup up for receiving CSW */
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	if ((rval = usb_pipe_receive_bulk_data(scsa2usbp->scsa2usb_bulkin_pipe,
	    CSW_LEN, flags)) != USB_SUCCESS) {

		USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_handle_status_start: read error = 0x%x", rval);

		mutex_enter(&scsa2usbp->scsa2usb_mutex);
	} else {
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
	}

	return (USB_SUCCESS);
}


/*
 * scsa2usb_handle_csw_result:
 *	Handle status results
 */
static int
scsa2usb_handle_csw_result(scsa2usb_state_t *scsa2usbp, mblk_t *data)
{
	usb_bulk_csw_t	csw;
	uint_t	signature, tag, residue, status;
	int rval = USB_SUCCESS;
	char *msg;
	struct scsi_pkt *pkt = scsa2usbp->scsa2usb_cur_pkt;
	scsa2usb_cmd_t *cmd = PKT2CMD(pkt);

#ifdef	SCSA2USB_TEST
	if (!data) {

		return (USB_FAILURE);
	}
#endif	/* SCSA2USB_TEST */
	ASSERT(data);
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));
	ASSERT((data->b_wptr - data->b_rptr) == CSW_LEN);

	/* Read into csw */
	bcopy(data->b_rptr, &csw, CSW_LEN);

	status = csw.csw_bCSWStatus;
	signature = SCSA2USB_INT(csw.csw_dCSWSignature3, csw.csw_dCSWSignature2,
			    csw.csw_dCSWSignature1, csw.csw_dCSWSignature0);

	residue = SCSA2USB_INT(csw.csw_dCSWDataResidue3,
			    csw.csw_dCSWDataResidue2,
			    csw.csw_dCSWDataResidue1,
			    csw.csw_dCSWDataResidue0);
	tag = SCSA2USB_INT(csw.csw_dCSWTag3, csw.csw_dCSWTag2,
			    csw.csw_dCSWTag1, csw.csw_dCSWTag0);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "CSW: Signature = 0x%x Status = 0%x Tag = 0x%x Residue = 0x%x",
	    signature, status, tag,  residue);

	/* Check for abnormal errors */
	if ((signature != CSW_SIGNATURE) ||
	    (residue > cmd->cmd_total_xfercount) || (tag != cmd->cmd_tag) ||
	    (status >= CSW_STATUS_PHASE_ERROR)) {

		USB_DPRINTF_L2(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "CSW_ERR: Tag = 0x%x xfercount = 0x%x",
		    cmd->cmd_tag, cmd->cmd_total_xfercount);

		switch (status) {
		case CSW_STATUS_GOOD:
		case CSW_STATUS_FAILED:
			/* handle below */
			break;
		case CSW_STATUS_PHASE_ERROR:
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_handle_csw_result: Phase Error");

			pkt->pkt_reason = CMD_TRAN_ERR;
			SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);

			/*
			 * invoke reset recovery
			 */
			rval = scsa2usb_reset_recovery(scsa2usbp);
			ASSERT(rval == USB_SUCCESS);

			return (USB_FAILURE);
		default:
			USB_DPRINTF_L2(DPRINT_MASK_SCSA,
			    scsa2usbp->scsa2usb_log_handle,
			    "scsa2usb_handle_csw_result: Invalid CSW");

			if (scsa2usbp->scsa2usb_pkt_state ==
				SCSA2USB_PKT_RECEIVE_CSW_2) {

				SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
				pkt->pkt_reason = CMD_TRAN_ERR;
			}

			/*
			 * invoke reset recovery
			 */
			rval = scsa2usb_reset_recovery(scsa2usbp);
			ASSERT(rval == USB_SUCCESS);

			return (USB_SUCCESS);
		} /* end of switch */
	}

	switch (status) {
	case CSW_STATUS_GOOD:
		msg = "CSW GOOD";
		break;
	case CSW_STATUS_FAILED:
		msg = "CSW FAILED";
		/* Set check condition ? */
		*(pkt->pkt_scbp) = STATUS_CHECK;
		cmd->cmd_done = 1;
		break;
	default:
		msg = "Unknown Status";
	} /* end of switch */

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_handle_csw_result: %s", msg);

	/* Set resid */
	if (residue) {
		pkt->pkt_resid = SCSA2USB_RESID(cmd, residue);
	}

	/* we are done and ready to callback */
	SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);

	return (rval);
}


/*
 * scsa2usb_pkt_completion:
 *	Handle pkt completion. Invokes callback, if any.
 *	Since we may be in interrupt context and the target driver
 *	may submit NOINTR pkts in interrupt context, we callback
 *	into the target driver using a callback thread (borrowed
 *	from USBA taskq pool)
 */
static void
scsa2usb_pkt_completion(scsa2usb_state_t *scsa2usbp)
{
	struct scsi_pkt *pkt = scsa2usbp->scsa2usb_cur_pkt;
	scsa2usb_cmd_t *cmd = PKT2CMD(pkt);

	ASSERT(pkt);
	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));
	ASSERT(scsa2usbp->scsa2usb_pkt_state == SCSA2USB_PKT_DO_COMP);

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_pkt_completion:\n\t"
	    "reason = %d, status = %d state = 0x%x stats = 0x%x",
	    pkt->pkt_reason, *(pkt->pkt_scbp), pkt->pkt_state,
	    pkt->pkt_statistics);

	SCSA2USB_RESET_CUR_PKT(scsa2usbp);
	SCSA2USB_UPDATE_PKT_STATE(pkt, PKT2CMD(pkt));

	/* Do immediate callback. Else queue the callback */
	if (pkt->pkt_comp &&
	    ((pkt->pkt_flags & FLAG_IMMEDIATE_CB) || ddi_in_panic())) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		pkt->pkt_comp(pkt);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);

	} else {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);
		scsa2usb_queue_cb(scsa2usbp, cmd);
		mutex_enter(&scsa2usbp->scsa2usb_mutex);
	}


	/* reset the timestamp for PM framework */
	scsa2usb_device_idle(scsa2usbp);
}


static void
scsa2usb_device_not_responding(scsa2usb_state_t *scsa2usbp)
{
	if (scsa2usbp->scsa2usb_cur_pkt) {
		scsa2usbp->scsa2usb_cur_pkt->pkt_reason = CMD_INCOMPLETE;
		/* scsi_poll relies on this */
		scsa2usbp->scsa2usb_cur_pkt->pkt_state = STATE_GOT_BUS;
		SCSA2USB_SET_PKT_DO_COMP_STATE(scsa2usbp);
		scsa2usb_pkt_completion(scsa2usbp);
	}
}


/*
 * scsa2usb_set_timeout:
 *	Set timeout value for the command in the pipe policy.
 */
static int
scsa2usb_set_timeout(scsa2usb_state_t *scsa2usbp, int time)
{
	int rval;
	usb_pipe_handle_t ph;
	usb_pipe_policy_t pp;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_set_timeout_value: new val = 0x%x", time);

	ph = scsa2usbp->scsa2usb_bulkin_pipe;
	mutex_exit(&scsa2usbp->scsa2usb_mutex);
	(void) usb_pipe_get_policy(ph, &pp);

	/* set the new policy */
	if (time <= 0) {
		time = SCSA2USB_BULK_PIPE_TIMEOUT;
	}

	pp.pp_timeout_value = time;

	rval = usb_pipe_set_policy(ph, &pp, USB_FLAGS_SLEEP);
	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


/*
 * scsa2usb_queue_cb:
 *	queue callback in the callback queue and activate
 *	the callback thread is necessary
 */
static void
scsa2usb_queue_cb(scsa2usb_state_t *scsa2usbp, scsa2usb_cmd_t *cmd)
{
	scsa2usb_cmd_t	*dp = NULL;	/* Command */
	struct scsi_pkt *pkt = cmd->cmd_pkt;

	mutex_enter(&scsa2usb_cb.c_mutex);

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_queue_cb:  cmd = %p", cmd);

	/* Insert the command into the queue */
	cmd->cmd_cb = NULL;
	if (scsa2usb_cb.c_qf == NULL) {
		scsa2usb_cb.c_qf = scsa2usb_cb.c_qb = cmd;
	} else {
		/* add to the tail */
		dp = scsa2usb_cb.c_qb;
		scsa2usb_cb.c_qb = cmd;
		dp->cmd_cb = cmd;
	}

	/*
	 * Request USBA to schedule the callback.
	 * If it fails then do the callback immediately.
	 * Also remove the command just inserted into
	 * the queue.
	 * During CPR threads are frozen but we shouldn't get
	 * any callbacks then.
	 */
	if ((!scsa2usb_cb.c_cb_active &&
	    usb_taskq_request(scsa2usb_callback, (void *)cmd, KM_NOSLEEP) ==
	    USB_FAILURE)) {

		USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_queue_cb: direct callback on cmd = 0x%p", cmd);

		/*
		 * Remove the cmd from the queue and
		 * call the pkt_completion function
		 */
		if (dp) {
			/*
			 * We have more than one element in the queue
			 * As per above insertions, dp points to the
			 * last but one element of the queue.
			 */
			scsa2usb_cb.c_qb = dp;
			dp->cmd_cb = NULL;
		} else {
			/*
			 * We've one element in the queue. Reset
			 * the forward and backward pointers in
			 * this case. dp is set to NULL in the
			 * begining of this function.
			 */
			scsa2usb_cb.c_qf = scsa2usb_cb.c_qb = NULL;
		}

		/* Call the pkt_completion function */
		if (pkt && pkt->pkt_comp) {
			mutex_exit(&scsa2usb_cb.c_mutex);
			pkt->pkt_comp(pkt);
			mutex_enter(&scsa2usb_cb.c_mutex);
		}
	}

	mutex_exit(&scsa2usb_cb.c_mutex);
}


/*
 * scsa2usb_callback:
 *	The taskq scheduler wakes up this function everytime
 *	there is work to do. We scan the queue for pending
 *	requests and work on them.
 */
/* ARGSUSED */
static void
scsa2usb_callback(void *arg)
{
	scsa2usb_cmd_t *cmd;

	mutex_enter(&scsa2usb_cb.c_mutex);
	scsa2usb_cb.c_cb_active++;

	/* Read from the start of the list */
	while (scsa2usb_cb.c_qf) {
		cmd = scsa2usb_cb.c_qf;
		scsa2usb_cb.c_qf = cmd->cmd_cb;
		if (scsa2usb_cb.c_qb == cmd) {
			scsa2usb_cb.c_qb = NULL;
		}

		/* Call the pkt_completion function */
		if (cmd->cmd_pkt && cmd->cmd_pkt->pkt_comp) {
			mutex_exit(&scsa2usb_cb.c_mutex);
			cmd->cmd_pkt->pkt_comp((struct scsi_pkt *)cmd->cmd_pkt);
			mutex_enter(&scsa2usb_cb.c_mutex);
		}
	} /* end of while */

	scsa2usb_cb.c_cb_active--;
	mutex_exit(&scsa2usb_cb.c_mutex);
}


/*
 * wait till all callbacks have completed and the thread is
 * active. The caller should ensure that no more callbacks get
 * queued
 */
static void
scsa2usb_drain_callback()
{
	mutex_enter(&scsa2usb_cb.c_mutex);

	while (scsa2usb_cb.c_qf && scsa2usb_cb.c_cb_active) {
		mutex_exit(&scsa2usb_cb.c_mutex);
		delay(1);
		mutex_enter(&scsa2usb_cb.c_mutex);
	}
	mutex_exit(&scsa2usb_cb.c_mutex);
}


/*
 * scsa2usb_reset_recovery:
 *	Reset the USB device in case of errors.
 *	NOTE: This is for bulk only devices.
 */
static int
scsa2usb_reset_recovery(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_reset_recovery called: scsa2usbp = 0x%p", scsa2usbp);

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	scsa2usbp->scsa2usb_pipe_state |=
		SCSA2USB_PIPE_DEV_RESET |
		SCSA2USB_PIPE_BULK_IN_CLEAR_STALL |
		SCSA2USB_PIPE_BULK_OUT_CLEAR_STALL |
		SCSA2USB_PIPE_BULK_IN_RESET | SCSA2USB_PIPE_BULK_OUT_RESET;

	/*
	 * assume that the reset will be successful. if it isn't, retrying
	 * from target driver won't help much
	 */
	if (scsa2usbp->scsa2usb_cur_pkt) {
		scsa2usbp->scsa2usb_cur_pkt->pkt_statistics |= STAT_DEV_RESET;
	}

	/*
	 * Send an async Reset request to the device
	 * We reset the other pipes in the callback
	 */
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	rval = usb_pipe_device_ctrl_send(scsa2usbp->scsa2usb_default_pipe,
	    USB_DEV_REQ_TYPE_CLASS |
	    USB_DEV_REQ_RECIPIENT_INTERFACE,
	    BULK_ONLY_RESET,			/* bRequest */
	    0,					/* wValue */
	    scsa2usbp->scsa2usb_intfc_num,	/* wIndex */
	    0,					/* wLength */
	    NULL,				/* no data */
	    0);

	USB_DPRINTF_L3(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_reset_recovery: Reset done rval = %d", rval);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


static int
scsa2usb_reset_pipes(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_reset_pipes called: scsa2usbp = 0x%p", scsa2usbp);

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	scsa2usbp->scsa2usb_pipe_state |= SCSA2USB_PIPE_BULK_OUT_RESET;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	/*
	 * asynchronously reset the bulkin pipe here and the bulk out
	 * pipe in the callback
	 */
	rval = usb_pipe_reset(scsa2usbp->scsa2usb_bulkin_pipe, 0,
		scsa2usb_pipes_reset_cb, (void *)scsa2usbp);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_reset_pipes: reset done rval = %d", rval);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


/*
 * scsa2usb_clear_ept_stall:
 *	Clear Endpoint stall for Bulk Only devices, asynchronously
 */
/* ARGSUSED */
static int
scsa2usb_clear_ept_stall(scsa2usb_state_t *scsa2usbp, uint_t ept_addr,
    char *what)
{
	int	rval;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	rval = usb_pipe_device_ctrl_send(scsa2usbp->scsa2usb_default_pipe,
	    USB_DEV_REQ_RECIPIENT_ENDPOINT,	/* bmRequestType */
	    USB_REQ_CLEAR_FEATURE,		/* bRequest */
	    0,					/* endpoint staff */
	    ept_addr,				/* Endpoint address */
	    0,					/* wLength */
	    NULL,				/* no data to be sent */
	    USB_FLAGS_ENQUEUE);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "async scsa2usb_clear_ept_stall:\n\t"
	    "clear stall on %s: rval = %d ept = 0x%x", what, rval, ept_addr);

	return (rval);
}


/*
 * event handling
 * NOTE: we should not block in these callbacks
 */
static int
scsa2usb_connect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;


	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_connect_event_callback:\n\t"
	    "dip = 0x%p, cookie = 0x%p, data = 0x%p",
	    (void *)dip, (void *)cookie, bus_impldata);

	/*
	 * if we haven't closed the pipes yet, do it now, otherwise
	 * check on same device will fail
	 */
	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	ASSERT(scsa2usbp->scsa2usb_dev_state == USB_DEV_DISCONNECTED);

	if (scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) {
		scsa2usb_close_usb_pipes(scsa2usbp, USB_FLAGS_SLEEP);
	}
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	if (scsa2usb_check_same_device(dip, scsa2usbp) == USB_FAILURE) {

		return (DDI_EVENT_CLAIMED);
	}

	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	/* NOTE: we should not synchronously open or reset pipes here */
	if ((scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) == 0) {
		(void) scsa2usb_open_usb_pipes(scsa2usbp);
	}

	/* if the children have been removed, recreate them */
	/* NOTE: this may never happen */
	if (ddi_get_child(dip) == NULL) {
		scsa2usb_create_luns(scsa2usbp);
	}

	scsa2usbp->scsa2usb_pkt_state = SCSA2USB_PKT_NONE;
	scsa2usbp->scsa2usb_dev_state = USB_DEV_ONLINE;

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	USB_DPRINTF_L0(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "reinserted device is accessible again");


	return (DDI_EVENT_CLAIMED);
}


/*
 * scsa2usb_disconnect_event_callback
 *	callback for disconnect events
 */
static int
scsa2usb_disconnect_event_callback(dev_info_t *dip, ddi_eventcookie_t cookie,
	void *arg, void *bus_impldata)
{
	scsa2usb_state_t *scsa2usbp = (scsa2usb_state_t *)arg;

	USB_DPRINTF_L4(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_disconnect_event_callback:\n\t"
	    "dip = 0x%p, cookie = 0x%p, data = 0x%p",
	    (void *)dip, (void *)cookie, bus_impldata);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	scsa2usbp->scsa2usb_dev_state = USB_DEV_DISCONNECTED;

	/*
	 * if pipes are still open and no packet active, we can
	 * close the pipes
	 */
	if ((scsa2usbp->scsa2usb_flags & SCSA2USB_FLAGS_PIPES_OPENED) &&
	    (scsa2usbp->scsa2usb_cur_pkt == NULL)) {
		scsa2usb_close_usb_pipes(scsa2usbp, USB_FLAGS_SLEEP);

	} else if (scsa2usbp->scsa2usb_cur_pkt) {

		USB_DPRINTF_L0(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "Disconnected device was busy, please reconnect");

		/*
		 * a packet is active, we can't close the pipes but
		 * will just complete the packet here.
		 * if we can a callback, we close the pipes then and
		 * clean up
		 */
		scsa2usb_device_not_responding(scsa2usbp);
		scsa2usb_drain_callback();
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (DDI_EVENT_CLAIMED);
}


/*
 * scsa2usb_register_events
 *	register events
 */
static void
scsa2usb_register_events(scsa2usb_state_t *scsa2usbp)
{
	int rval;
	ddi_plevel_t level;
	ddi_iblock_cookie_t icookie;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	/* get event cookie, discard level and icookie for now */
	rval = ddi_get_eventcookie(scsa2usbp->scsa2usb_dip,
		DDI_DEVI_REMOVE_EVENT,
		&scsa2usbp->scsa2usb_remove_cookie, &level, &icookie);

	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(scsa2usbp->scsa2usb_dip,
			scsa2usbp->scsa2usb_remove_cookie,
			scsa2usb_disconnect_event_callback,
			(void *)scsa2usbp);

		ASSERT(rval == DDI_SUCCESS);
	}

	rval = ddi_get_eventcookie(scsa2usbp->scsa2usb_dip,
		DDI_DEVI_INSERT_EVENT,
		&scsa2usbp->scsa2usb_insert_cookie, &level, &icookie);
	if (rval == DDI_SUCCESS) {
		rval = ddi_add_eventcall(scsa2usbp->scsa2usb_dip,
			scsa2usbp->scsa2usb_insert_cookie,
			scsa2usb_connect_event_callback,
			(void *)scsa2usbp);

		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * scsa2usb_unregister_events
 *	Unregister events
 */
static void
scsa2usb_unregister_events(scsa2usb_state_t *scsa2usbp)
{
	int rval;

	ASSERT(mutex_owned(&scsa2usbp->scsa2usb_mutex));

	if (scsa2usbp->scsa2usb_remove_cookie) {
		rval = ddi_remove_eventcall(scsa2usbp->scsa2usb_dip,
					scsa2usbp->scsa2usb_remove_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}

	if (scsa2usbp->scsa2usb_insert_cookie) {
		rval = ddi_remove_eventcall(scsa2usbp->scsa2usb_dip,
					scsa2usbp->scsa2usb_insert_cookie);
		ASSERT(rval == DDI_SUCCESS);
	}
}


/*
 * scsa2usb_check_same_device()
 *	check if it is the same device and if not, warn user
 */
static int
scsa2usb_check_same_device(dev_info_t *dip, scsa2usb_state_t *scsa2usbp)
{
	char	*ptr;

	/* Check for the same device */
	if (usb_check_same_device(dip) == USB_FAILURE) {
		if (ptr = usb_get_usbdev_strdescr(dip)) {
			USB_DPRINTF_L0(DPRINT_MASK_ALL,
			    scsa2usbp->scsa2usb_log_handle,
			    "Cannot access device. Please reconnect %s", ptr);
		} else {
			USB_DPRINTF_L0(DPRINT_MASK_ALL,
			    scsa2usbp->scsa2usb_log_handle,
			    "Device is not identical to the "
			    "previous one on this port.\n"
			    "Please disconnect and reconnect");
		}

		return (USB_FAILURE);
	}

	return (USB_SUCCESS);
}


/*
 * PM support
 *
 * create the pm components required for power management
 *
 * we define a black list of devices that do not support
 * PM reliably
 */
static struct {
	uint16_t	idVendor;	/* vendor ID			*/
	uint16_t	idProduct;	/* product ID			*/
	uint16_t	bcdDevice;	/* device release number in bcd */
} pm_blacklist[] = {
	{0x59b, 1, 0x100},		/* zip 100 with flaky USB bridge */
	{0}
};

static void
scsa2usb_create_pm_components(dev_info_t *dip, scsa2usb_state_t *scsa2usbp,
    usb_device_descr_t *descr)
{
	int i = 0;
	scsa2usb_power_t	*pm;

	USB_DPRINTF_L4(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
		"scsa2usb_create_pm_components:");

	/* determine if this device is on the blacklist */
	while (pm_blacklist[i].idVendor != 0) {
		if ((descr->idVendor == pm_blacklist[i].idVendor) &&
		    (descr->idProduct == pm_blacklist[i].idProduct) &&
		    (descr->bcdDevice == pm_blacklist[i].bcdDevice)) {

			USB_DPRINTF_L1(DPRINT_MASK_PM,
			    scsa2usbp->scsa2usb_log_handle,
			    "device cannot be power managed");

			return;
		}
		i++;
	}

	/* Allocate the PM state structure */
	pm = kmem_zalloc(sizeof (scsa2usb_power_t), KM_SLEEP);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	scsa2usbp->scsa2usb_pm = pm;
	pm->scsa2usb_raise_power = B_FALSE;
	pm->scsa2usb_current_power = USB_DEV_OS_FULL_POWER;
	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	if (usb_is_pm_enabled(dip) == USB_SUCCESS) {
		uint_t	pwr_states;

		if (usb_create_pm_components(dip, &pwr_states) == USB_SUCCESS) {
			pm->scsa2usb_pwr_states = (uint8_t)pwr_states;
		}
	}

	USB_DPRINTF_L4(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_create_pm_components: done");
}


static void
scsa2usb_device_idle(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	USB_DPRINTF_L4(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_device_idle:");

	if (scsa2usbp->scsa2usb_pm &&
	    usb_is_pm_enabled(scsa2usbp->scsa2usb_dip) == USB_SUCCESS) {
		rval = pm_idle_component(scsa2usbp->scsa2usb_dip, 0);
		ASSERT(rval == DDI_SUCCESS);
	}
}


static int
scsa2usb_check_power(scsa2usb_state_t *scsa2usbp)
{
	USB_DPRINTF_L4(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_check_power:");

	if (scsa2usbp->scsa2usb_pm == NULL) {

		return (USB_SUCCESS);
	}

	if (scsa2usbp->scsa2usb_pm->scsa2usb_current_power !=
	    (uint8_t)USB_DEV_OS_FULL_POWER) {
		int rval;

		scsa2usbp->scsa2usb_pm->scsa2usb_raise_power = B_TRUE;

		/* reset the timestamp for PM framework */
		scsa2usb_device_idle(scsa2usbp);

		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		rval = pm_raise_power(scsa2usbp->scsa2usb_dip,
			0, USB_DEV_OS_FULL_POWER);
		ASSERT(rval == DDI_SUCCESS);

		mutex_enter(&scsa2usbp->scsa2usb_mutex);
		scsa2usbp->scsa2usb_pm->scsa2usb_raise_power = B_FALSE;
	}

	return (USB_SUCCESS);
}


/*
 * functions to handle power transition for OS levels 0 -> 3
 */
static int
scsa2usb_pwrlvl0(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	switch (scsa2usbp->scsa2usb_dev_state) {
	case USB_DEV_ONLINE:
		/* Issue USB D3 command to the device here */
		rval = usb_set_device_pwrlvl3(scsa2usbp->scsa2usb_dip);
		ASSERT(rval == USB_SUCCESS);

		scsa2usbp->scsa2usb_dev_state = USB_DEV_POWERED_DOWN;
		scsa2usbp->scsa2usb_pm->scsa2usb_current_power =
						USB_DEV_OS_POWER_OFF;

		/* FALLTHRU */
	case USB_DEV_DISCONNECTED:
	case USB_DEV_CPR_SUSPEND:
	case USB_DEV_POWERED_DOWN:
	default:

		return (USB_SUCCESS);
	}
}


/* ARGSUSED */
static int
scsa2usb_pwrlvl1(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	/* Issue USB D2 command to the device here */
	rval = usb_set_device_pwrlvl2(scsa2usbp->scsa2usb_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


/* ARGSUSED */
static int
scsa2usb_pwrlvl2(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	/* Issue USB D1 command to the device here */
	rval = usb_set_device_pwrlvl1(scsa2usbp->scsa2usb_dip);
	ASSERT(rval == USB_SUCCESS);

	return (DDI_FAILURE);
}


static int
scsa2usb_pwrlvl3(scsa2usb_state_t *scsa2usbp)
{
	int	rval;

	/*
	 * PM framework tries to put us in full power
	 * during system shutdown. If we are disconnected
	 * return success anyways
	 */
	if (scsa2usbp->scsa2usb_dev_state != USB_DEV_DISCONNECTED) {
		/* Issue USB D0 command to the device here */
		rval = usb_set_device_pwrlvl0(scsa2usbp->scsa2usb_dip);
		ASSERT(rval == USB_SUCCESS);

		scsa2usbp->scsa2usb_dev_state = USB_DEV_ONLINE;
		scsa2usbp->scsa2usb_pm->scsa2usb_current_power =
						USB_DEV_OS_FULL_POWER;
	}

	return (DDI_SUCCESS);
}


/* power entry point */
/* ARGSUSED */
static int
scsa2usb_power(dev_info_t *dip, int comp, int level)
{
	scsa2usb_state_t	*scsa2usbp;
	scsa2usb_power_t	*pm;
	int	rval = DDI_FAILURE;

	scsa2usbp = (scsa2usb_state_t *)ddi_get_soft_state(scsa2usb_state,
						ddi_get_instance(dip));

	USB_DPRINTF_L3(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_power : Begin scsa2usbp (%p): level = %d",
	    scsa2usbp, level);

	mutex_enter(&scsa2usbp->scsa2usb_mutex);
	if (SCSA2USB_BUSY(scsa2usbp)) {
		USB_DPRINTF_L2(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_power: busy");
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (rval);
	}

	/*
	 * if we are disconnected, return success. Note that if we
	 * return failure, bringing down the system will hang when
	 * PM tries to power up all devices
	 */
	if (scsa2usbp->scsa2usb_dev_state == USB_DEV_DISCONNECTED) {
		USB_DPRINTF_L2(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_power: disconnected");
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (DDI_SUCCESS);
	}

	pm = scsa2usbp->scsa2usb_pm;
	if (pm == NULL) {
		USB_DPRINTF_L2(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_power: pm NULL");
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (rval);
	}

	/* check if we are transitioning to a legal power level */
	if (USB_DEV_PWRSTATE_OK(pm->scsa2usb_pwr_states, level)) {
		USB_DPRINTF_L2(DPRINT_MASK_PM, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_power: illegal power level = %d "
		    "pwr_states: %x", level, pm->scsa2usb_pwr_states);
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (rval);
	}

	/*
	 * If we are about to raise power and we get this call to lower
	 * power, we return failure
	 */
	if ((pm->scsa2usb_raise_power == B_TRUE) &&
		(level < (int)pm->scsa2usb_current_power)) {
		mutex_exit(&scsa2usbp->scsa2usb_mutex);

		return (DDI_FAILURE);
	}

	switch (level) {
	case USB_DEV_OS_POWER_OFF :
		rval = scsa2usb_pwrlvl0(scsa2usbp);
		break;
	case USB_DEV_OS_POWER_1 :
		rval = scsa2usb_pwrlvl1(scsa2usbp);
		break;
	case USB_DEV_OS_POWER_2 :
		rval = scsa2usb_pwrlvl2(scsa2usbp);
		break;
	case USB_DEV_OS_FULL_POWER :
		rval = scsa2usb_pwrlvl3(scsa2usbp);
		break;
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);

	return (rval);
}


#ifdef	DEBUG
/*
 * scsa2usb_dump:
 *	Dump all SCSA2USB related information. This function is
 *	registered with USBA framework.
 */
static void
scsa2usb_dump(uint_t flag, usb_opaque_t arg)
{
	scsa2usb_state_t *scsa2usbp;
	scsa2usb_cmd_t	 *cmd;

	_NOTE(NO_COMPETING_THREADS_NOW);

	mutex_enter(&scsa2usb_dump_mutex);
	scsa2usb_show_label = USB_DISALLOW_LABEL;

	scsa2usbp = (scsa2usb_state_t *)arg;
	if (flag & USB_DUMP_STATE) {
		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "****** scsa2usb%d ****** dip: 0x0x%p",
		    scsa2usbp->scsa2usb_instance, scsa2usbp->scsa2usb_dip);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_dev_state: 0x%x\t\t  scsa2usb_pm: %p",
		    scsa2usbp->scsa2usb_dev_state, scsa2usbp->scsa2usb_pm);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_flags: 0x%x \t\t  scsa2usb_intfc_num: 0x%x",
		    scsa2usbp->scsa2usb_flags, scsa2usbp->scsa2usb_intfc_num);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_tran: 0x%p\t  scsa2usb_cur_pkt: 0x%p",
		    scsa2usbp->scsa2usb_tran, scsa2usbp->scsa2usb_cur_pkt);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_serial_no: 0x%p scsa2usb_target_dip[0]: 0x%p",
		    scsa2usbp->scsa2usb_serial_no,
		    scsa2usbp->scsa2usb_target_dip[0]);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_intr_ept: 0x%p \n\tscsa2usb_bulkin_ept: 0x%p",
		    scsa2usbp->scsa2usb_intr_ept,
		    scsa2usbp->scsa2usb_bulkin_ept);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bulkout_ept: 0x%p "
		    "\n\tscsa2usb_default_pipe: 0x%p",
		    "\n\tscsa2usb_bulkin_pipe: 0x%p",
		    scsa2usbp->scsa2usb_bulkout_ept,
		    scsa2usbp->scsa2usb_default_pipe,
		    scsa2usbp->scsa2usb_bulkin_pipe);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_bulkout_pipe: 0x%p  "
		    "scsa2usb_pipe_policy: 0x%p",
		    scsa2usbp->scsa2usb_bulkout_pipe,
		    scsa2usbp->scsa2usb_pipe_policy);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_pipe_state: 0x%x\t  scsa2usb_intfc_descr:0x%p",
		    scsa2usbp->scsa2usb_pipe_state,
		    scsa2usbp->scsa2usb_intfc_descr);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_dma_attr: 0x%p  scsa2usb_log_handle: 0x%p",
		    scsa2usbp->scsa2usb_dma_attr,
		    scsa2usbp->scsa2usb_log_handle);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_tag: 0x%x \t\t  scsa2usb_pkt_state: 0x%x",
		    scsa2usbp->scsa2usb_tag, scsa2usbp->scsa2usb_pkt_state);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_dump_ops: 0x%p  "
		    "scsa2usb_max_bulk_xfer_size: 0x%x",
		    scsa2usbp->scsa2usb_dump_ops,
		    scsa2usbp->scsa2usb_max_bulk_xfer_size);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_remove_cookie: 0x%p scsa2usb_insert_cookie: 0x%p",
		    scsa2usbp->scsa2usb_remove_cookie,
		    scsa2usbp->scsa2usb_insert_cookie);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_lbasize: 0x%x\t\t  scsa2usb_n_luns: 0x%x",
		    scsa2usbp->scsa2usb_lbasize, scsa2usbp->scsa2usb_n_luns);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_cb: 0x%p\t\t  scsa2usb_reset_delay: %d",
		    scsa2usb_cb, scsa2usbp->scsa2usb_reset_delay);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_panic_info: 0x%x\t  scsa2usb_cpr_info: 0x%x",
		    scsa2usbp->scsa2usb_panic_info,
		    scsa2usbp->scsa2usb_cpr_info);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_totalsec: 0x%x\t\t  scsa2usb_secsz: 0x%x",
		    scsa2usbp->scsa2usb_totalsec, scsa2usbp->scsa2usb_secsz);

		USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
		    scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_msg_count: 0x%x", scsa2usbp->scsa2usb_msg_count);

		if (scsa2usbp->scsa2usb_cur_pkt) {
			cmd = PKT2CMD(scsa2usbp->scsa2usb_cur_pkt);
			if (cmd) {
				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "****** current cmd: 0x%p pkt: 0x%p",
				    cmd, cmd->cmd_pkt);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_xfercount: 0x%x\t\tcmd_dir: 0x%x",
				    cmd->cmd_xfercount, cmd->cmd_dir);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_cdb: 0x%p\t\tcmd_actual_len: 0x%x",
				    cmd->cmd_cdb, cmd->cmd_actual_len);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_cdblen: 0x%x\t\t\tcmd_scblen: 0x%x",
				    cmd->cmd_cdblen, cmd->cmd_scblen);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_tag: 0x%x\t\t\tcmd_bp: 0x%p",
				    cmd->cmd_tag, cmd->cmd_bp);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_timeout: 0x%x\t\tcmd_scb: 0x%p",
				    cmd->cmd_timeout, cmd->cmd_scb);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_total_xfercount: 0x%x"
				    "\tcmd_offset: 0x%x",
				    cmd->cmd_total_xfercount, cmd->cmd_offset);

				USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
				    scsa2usbp->scsa2usb_log_handle,
				    "cmd_done: 0x%x\t\t\tcmd_lba: 0x%x",
				    cmd->cmd_done, cmd->cmd_lba);
			}
		}
	}

	if (flag & USB_DUMP_PIPE_POLICY) {
		usb_pipe_handle_impl_t *ph = (usb_pipe_handle_impl_t *)
					scsa2usbp->scsa2usb_bulkin_pipe;

		if (ph && ph->p_policy) {
			USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
			    scsa2usbp->scsa2usb_log_handle,
			    "***** USB_PIPE_POLICY (scsa2usb_bulkin_pipe) "
			    "*****");
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}

		ph = (usb_pipe_handle_impl_t *)scsa2usbp->scsa2usb_bulkout_pipe;
		if (ph && ph->p_policy) {
			USB_DPRINTF_L3(DPRINT_MASK_DUMPING,
			    scsa2usbp->scsa2usb_log_handle,
			    "***** USB_PIPE_POLICY (scsa2usb_bulkout_pipe) "
			    "*****");
			usba_dump_usb_pipe_policy(ph->p_policy, flag);
		}
	}

	mutex_exit(&scsa2usb_dump_mutex);
	_NOTE(COMPETING_THREADS_NOW);
}
#endif	/* DEBUG */


#ifdef	SCSA2USB_TEST
/*
 * scsa2usb_test_mblk:
 *	This function sends a dummy data mblk_t to simulate
 *	the following test cases: 5 and 11.
 */
static void
scsa2usb_test_mblk(scsa2usb_state_t *scsa2usbp, boolean_t large)
{
	int	i, rval, len = USB_BULK_CBWCMD_LEN;
	mblk_t	*mp;

	/* Create a large mblk */
	if (large == B_TRUE) {
		len = DEV_BSIZE;
	}

	if ((mp = allocb(len, BPRI_LO)) == NULL) {
		USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
		    "scsa2usb_test_mblk: allocb failed");

		return;
	}

	/* fill up the mblk */
	for (i = 0; i < len; i++) {
		*mp->b_wptr++ = (uchar_t)i;
	}

	mutex_exit(&scsa2usbp->scsa2usb_mutex);
	rval = usb_pipe_send_bulk_data(scsa2usbp->scsa2usb_bulkout_pipe, mp, 0);
	mutex_enter(&scsa2usbp->scsa2usb_mutex);

	USB_DPRINTF_L1(DPRINT_MASK_SCSA, scsa2usbp->scsa2usb_log_handle,
	    "scsa2usb_test_mblk: Sent Data Out rval = 0x%x", rval);
}
#endif	/* SCSA2USB_TEST */
