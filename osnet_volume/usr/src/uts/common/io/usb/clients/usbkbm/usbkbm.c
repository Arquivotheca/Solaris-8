/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)usbkbm.c	1.14	99/11/05 SMI"


/*
 * USB keyboard input streams module - processes USB keypacket
 * received from HID driver below to either ASCII or event
 * format for windowing system.
 */

#define	KEYMAP_SIZE_VARIABLE
#include <sys/usb/usba.h>
#include <sys/usb/clients/hid/hid.h>
#include <sys/usb/clients/hid/hid_polled.h>
#include <sys/usb/clients/hidparser/hidparser.h>
#include <sys/stropts.h>
#include <sys/kbio.h>
#include <sys/vuid_event.h>
#include <sys/kbd.h>
#include <sys/consdev.h>
#include <sys/kbtrans.h>
#include <sys/usb/clients/usbkbm/usbkbm.h>
#include <sys/beep.h>

/* debugging information */
static uint_t	usbkbm_errmask = (uint_t)PRINT_MASK_ALL;
static uint_t	usbkbm_errlevel = USB_LOG_L2;
static usb_log_handle_t usbkbm_log_handle;

typedef void (*process_key_callback_t)(usbkbm_state_t *, int, enum keystate);

/*
 * Internal Function Prototypes
 */
static void usbkbm_streams_setled(struct kbtrans_hardware *, int);
static void usbkbm_polled_setled(struct kbtrans_hardware *, int);
static boolean_t usbkbm_polled_is_scancode_available(struct kbtrans_hardware *,
			int *, enum keystate *);
static void usbkbm_mctl_send(usbkbm_state_t *, struct iocblk, char *, int);
static void usbkbm_poll_callback(usbkbm_state_t *, int, enum keystate);
static void usbkbm_streams_callback(usbkbm_state_t *, int, enum keystate);
static void usbkbm_unpack_usb_packet(usbkbm_state_t *, process_key_callback_t,
			uchar_t *, int);
static void usbkbm_reioctl(void	*);
static int usbkbm_polled_getchar(struct cons_polledio_arg *);
static boolean_t usbkbm_polled_ischar(struct cons_polledio_arg *);
static void usbkbm_polled_enter(struct cons_polledio_arg *);
static void usbkbm_polled_exit(struct cons_polledio_arg *);
static void usbkbm_mctl_receive(queue_t *, mblk_t *);
static enum kbtrans_message_response usbkbm_ioctl(register queue_t *q,
			register mblk_t *mp);
static int usbkbm_kioccmd(usbkbm_state_t *, register mblk_t *,
			char, size_t *);

/* stream qinit functions defined here */
static int	usbkbm_open(queue_t *, dev_t *, int, int, cred_t *);
static int	usbkbm_close(queue_t *, int, cred_t *);
static void	usbkbm_wput(queue_t *, mblk_t *);
static void	usbkbm_rput(queue_t *, mblk_t *);
static void	usbkbm_get_scancode(usbkbm_state_t *, int *, enum keystate *);

/* Keytable information */
extern struct keyboard keyindex_usb;
extern keymap_entry_t keytab_usb_lc[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_uc[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_cl[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_ag[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_nl[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_ct[KEYMAP_SIZE_USB];
extern keymap_entry_t keytab_usb_up[KEYMAP_SIZE_USB];

/* External Functions */
extern int space_store(char *key, uintptr_t ptr);
extern uintptr_t space_fetch(char *key);
extern void space_free(char *key);

/*
 * Structure to setup callbacks
 */
struct kbtrans_hw_callbacks kbd_usb_callbacks = {
	usbkbm_streams_setled,
	usbkbm_polled_setled,
	usbkbm_polled_is_scancode_available,
};

/*
 * Global Variables
 */

/* This variable saves the LED state across hotplugging. */
static uchar_t  usbkbm_led_state = 0;

static struct streamtab usbkbm_info;
static struct fmodsw fsw = {
	"usbkbm",
	&usbkbm_info,
	D_NEW | D_MP | D_MTPERMOD
};


/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_strmodops;

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"USB keyboard streams 1.14",
	&fsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlstrmod,
	NULL
};


int
_init()
{
	int rval = mod_install(&modlinkage);
	usbkbm_save_state_t *sp;

	if (rval == 0) {
		usbkbm_log_handle = usb_alloc_log_handle(NULL, "usbkbm",
			&usbkbm_errlevel, &usbkbm_errmask, NULL, NULL, 0);

		sp = (usbkbm_save_state_t *)space_fetch("SUNW,usbkbm_state");

		if (sp != NULL) {

			/* Restore LED information */
			usbkbm_led_state = sp->usbkbm_save_led;

			/* Restore abort information */
			keyindex_usb.k_abort1 =
				    sp->usbkbm_save_keyindex.k_abort1;

			keyindex_usb.k_abort2 =
				sp->usbkbm_save_keyindex.k_abort2;

			/* Restore keytables */
			bcopy(sp->usbkbm_save_keyindex.k_normal, keytab_usb_lc,
				USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_shifted, keytab_usb_uc,
				USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_caps, keytab_usb_cl,
				USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_altgraph,
				keytab_usb_ag, USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_numlock, keytab_usb_nl,
				USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_control, keytab_usb_ct,
				USB_KEYTABLE_SIZE);

			bcopy(sp->usbkbm_save_keyindex.k_up, keytab_usb_up,
				USB_KEYTABLE_SIZE);

			kmem_free(sp->usbkbm_save_keyindex.k_normal,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_shifted,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_caps,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_altgraph,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_numlock,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_control,
				USB_KEYTABLE_SIZE);
			kmem_free(sp->usbkbm_save_keyindex.k_up,
				USB_KEYTABLE_SIZE);

			kmem_free(sp, sizeof (usbkbm_save_state_t));
			space_free("SUNW,usbkbm_state");
		}
	}

	return (rval);
}

int
_fini()
{
	usbkbm_save_state_t *sp;
	int sval;
	int rval = mod_remove(&modlinkage);

	if (rval == 0) {

		usb_free_log_handle(usbkbm_log_handle);

		sp = kmem_alloc(sizeof (usbkbm_save_state_t), KM_SLEEP);

		sval = space_store("SUNW,usbkbm_state", (uintptr_t)sp);

		/*
		 * If it's not possible to store the state, return
		 * EBUSY.
		 */
		if (sval != 0) {
			kmem_free(sp, sizeof (usbkbm_save_state_t));
			return (EBUSY);
		}


		/* Save the LED state */
		sp->usbkbm_save_led = usbkbm_led_state;

		/*
		 * Save entries of the keyboard structure that
		 * have changed.
		 */
		sp->usbkbm_save_keyindex.k_abort1 = keyindex_usb.k_abort1;
		sp->usbkbm_save_keyindex.k_abort2 = keyindex_usb.k_abort2;


		/* Allocate space for keytables to be stored */
		sp->usbkbm_save_keyindex.k_normal =
			kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_shifted =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_caps =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_altgraph =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_numlock =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_control =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);
		sp->usbkbm_save_keyindex.k_up =
			    kmem_alloc(USB_KEYTABLE_SIZE, KM_SLEEP);

		/* Copy over the keytables */
		bcopy(keytab_usb_lc, sp->usbkbm_save_keyindex.k_normal,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_uc, sp->usbkbm_save_keyindex.k_shifted,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_cl, sp->usbkbm_save_keyindex.k_caps,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_ag, sp->usbkbm_save_keyindex.k_altgraph,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_nl, sp->usbkbm_save_keyindex.k_numlock,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_ct, sp->usbkbm_save_keyindex.k_control,
			    USB_KEYTABLE_SIZE);

		bcopy(keytab_usb_up, sp->usbkbm_save_keyindex.k_up,
			    USB_KEYTABLE_SIZE);
	}

	return (rval);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Module qinit functions
 */

static struct module_info usbkbm_minfo = {
	0,		/* module id number */
	"usbkbm",	/* module name */
	0,		/* min packet size accepted */
	INFPSZ,		/* max packet size accepted */
	2048,		/* hi-water mark */
	128		/* lo-water mark */
	};

/* read side for key data and ioctl replies */
static struct qinit usbkbm_rinit = {
	(int (*)())usbkbm_rput,
	(int (*)())NULL,		/* service not used */
	usbkbm_open,
	usbkbm_close,
	(int (*)())NULL,
	&usbkbm_minfo
	};

/* write side for ioctls */
static struct qinit usbkbm_winit = {
	(int (*)())usbkbm_wput,
	(int (*)())NULL,
	usbkbm_open,
	usbkbm_close,
	(int (*)())NULL,
	&usbkbm_minfo
	};

static struct streamtab usbkbm_info = {
	&usbkbm_rinit,
	&usbkbm_winit,
	NULL,		/* for muxes */
	NULL,		/* for muxes */
};

/*
 * usbkbm_open :
 *	Open a keyboard
 */
/* ARGSUSED */
static int
usbkbm_open(queue_t *q, dev_t *devp, int oflag, int sflag, cred_t *crp)
{
	usbkbm_state_t	*usbkbmd;
	struct iocblk	mctlmsg;
	uint32_t	packet_size;
	int		error;

	packet_size = 0;

	if (q->q_ptr) {
	    USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
		"usbkbm_open already opened");

	    return (0); /* already opened */
	}

	/*
	 * Only allow open requests to succeed for super-users.  This
	 * necessary to prevent users from pushing the "usbkbm" module again
	 * on the stream associated with /dev/kbd.
	 */
	if (drv_priv(crp)) {

		return (EPERM);
	}


	switch (sflag) {

	case MODOPEN:
		break;

	case CLONEOPEN:
		USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
			"usbkbm_open: Clone open not supported");

		return (EINVAL);
	}

	/* allocate usb keyboard state structure */

	usbkbmd = kmem_zalloc(sizeof (usbkbm_state_t), KM_SLEEP);

	USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
		"usbkbm_state= %p", usbkbmd);

	/*
	 * Set up private data.
	 */
	usbkbmd->usbkbm_readq = q;
	usbkbmd->usbkbm_writeq = WR(q);

	/*
	 * Set up queue pointers, so that the "put" procedure will accept
	 * the reply to the "ioctl" message we send down.
	 */
	q->q_ptr = (caddr_t)usbkbmd;
	WR(q)->q_ptr = (caddr_t)usbkbmd;

	error = kbtrans_streams_init(q, sflag, crp,
		(struct kbtrans_hardware *)usbkbmd, &kbd_usb_callbacks,
		&usbkbmd->usbkbm_kbtrans, usbkbm_led_state, 0);

	if (error != 0) {
		USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
			"kbdopen:  kbtrans_streams_init failed\n");
		kmem_free(usbkbmd, sizeof (*usbkbmd));
		return (error);
	}

	/*
	 * Set the polled information in the state structure.
	 * This information is set once, and doesn't change
	 */
	usbkbmd->usbkbm_polled_info.cons_polledio_version =
				    CONSPOLLEDIO_V0;

	usbkbmd->usbkbm_polled_info.cons_polledio_argument =
				(struct cons_polledio_arg *)usbkbmd;

	usbkbmd->usbkbm_polled_info.cons_polledio_putchar = NULL;

	usbkbmd->usbkbm_polled_info.cons_polledio_getchar =
				usbkbm_polled_getchar;

	usbkbmd->usbkbm_polled_info.cons_polledio_ischar =
				usbkbm_polled_ischar;

	usbkbmd->usbkbm_polled_info.cons_polledio_enter =
				    usbkbm_polled_enter;

	usbkbmd->usbkbm_polled_info.cons_polledio_exit =
				usbkbm_polled_exit;

	/*
	 * The head and the tail pointing at the same byte means empty or
	 * full. usbkbm_polled_buffer_num_characters is used to
	 * tell the difference.
	 */
	usbkbmd->usbkbm_polled_buffer_head =
			usbkbmd->usbkbm_polled_scancode_buffer;
	usbkbmd->usbkbm_polled_buffer_tail =
			usbkbmd->usbkbm_polled_scancode_buffer;
	usbkbmd->usbkbm_polled_buffer_num_characters = 0;

	qprocson(q);

	/* request hid report descriptor from HID */
	mctlmsg.ioc_cmd = HID_GET_PARSER_HANDLE;
	mctlmsg.ioc_count = 0;
	usbkbm_mctl_send(usbkbmd, mctlmsg, NULL, NULL);

	/*
	 * Now that we have sent the ioctl, wait for the response to
	 * come back.
	 */
	usbkbmd->usbkbm_flags |= USBKBM_QWAIT;

	while (usbkbmd->usbkbm_flags & USBKBM_QWAIT) {

		qwait(q);
	}

	if (usbkbmd->usbkbm_report_descr != NULL) {
		if (hidparser_get_country_code(usbkbmd->usbkbm_report_descr,
			(uint16_t *)&usbkbmd->usbkbm_layout) ==
			HIDPARSER_FAILURE) {
			USB_DPRINTF_L3(PRINT_MASK_OPEN,
			usbkbm_log_handle, "get_country_code failed"
			"setting default layout(0x21)");

			/* Setting to default layout = US */
			usbkbmd->usbkbm_layout = 0x21;
		}

		if (hidparser_get_packet_size(usbkbmd->usbkbm_report_descr,
			0, HIDPARSER_ITEM_INPUT, (uint32_t *)&packet_size) ==
			HIDPARSER_FAILURE) {

			USB_DPRINTF_L3(PRINT_MASK_OPEN,
				usbkbm_log_handle, "get_packet_size failed"
				"setting default packet size(8)");

			/* Setting to default packet size = 8 */
			usbkbmd->usbkbm_packet_size =
				USB_KBD_DEFAULT_PACKET_SIZE;
		} else {
			usbkbmd->usbkbm_packet_size = packet_size/8;
		}
	} else {
		USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
			"usbkbm: Invalid HID Descriptor Tree."
			"setting default layout(0x21) & packet_size(8)");
			usbkbmd->usbkbm_layout = 0x21;
			usbkbmd->usbkbm_packet_size =
				USB_KBD_DEFAULT_PACKET_SIZE;
	}

	kbtrans_streams_set_keyboard(usbkbmd->usbkbm_kbtrans, KB_USB,
					&keyindex_usb);

	usbkbmd->usbkbm_flags = USBKBM_OPEN;

	kbtrans_streams_enable(usbkbmd->usbkbm_kbtrans);

	USB_DPRINTF_L3(PRINT_MASK_OPEN, usbkbm_log_handle,
			"usbkbm_open exiting");
	return (0);
}


/*
 * usbkbm_close :
 *	Close a keyboard.
 */
/* ARGSUSED1 */
static int
usbkbm_close(register queue_t *q, int flag, cred_t *crp)
{
	usbkbm_state_t *usbkbmd = (usbkbm_state_t *)q->q_ptr;

	(void) kbtrans_streams_fini(usbkbmd->usbkbm_kbtrans);

	qprocsoff(q);
	/*
	 * Since we're about to destroy our private data, turn off
	 * our open flag first, so we don't accept any more input
	 * and try to use that data.
	 */
	usbkbmd->usbkbm_flags = 0;

	kmem_free(usbkbmd, sizeof (usbkbm_state_t));

	USB_DPRINTF_L3(PRINT_MASK_CLOSE, usbkbm_log_handle,
		"usbkbm_close exiting");

	return (0);
}


/*
 * usbkbm_wput :
 *	usb keyboard module output queue put procedure: handles M_IOCTL
 *	messages.
 */
static void
usbkbm_wput(register queue_t *q, register mblk_t *mp)
{
	usbkbm_state_t			*usbkbmd;
	enum kbtrans_message_response	ret;

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
				"usbkbm_wput entering");

	usbkbmd = (usbkbm_state_t *)q->q_ptr;

	/* First, see if kbtrans will handle the message */
	ret = kbtrans_streams_message(usbkbmd->usbkbm_kbtrans, mp);

	if (ret == KBTRANS_MESSAGE_HANDLED) {

		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_wput exiting:2");

		return;
	}

	/* kbtrans didn't handle the message.  Try to handle it here */

	switch (mp->b_datap->db_type) {

	case M_IOCTL:
		ret = usbkbm_ioctl(q, mp);

		if (ret == KBTRANS_MESSAGE_HANDLED) {

			USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
				"usbkbm_wput exiting:1");

			return;
		}
	default:
		break;
	}

	/*
	 * The message has not been handled
	 * by kbtrans or this module.  Pass it down the stream
	 */
	putnext(q, mp);

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
		"usbkbm_wput exiting:3");
}

/*
 * usbkbm_ioctl :
 *	Handles the ioctls sent from upper module. Returns
 *	ACK/NACK back.
 */
static enum kbtrans_message_response
usbkbm_ioctl(register queue_t *q, register mblk_t *mp)
{
	usbkbm_state_t			*usbkbmd;
	register struct iocblk		*iocp;
	size_t				ioctlrespsize;
	int				err;
	struct iocblk			mctlmsg;
	caddr_t				data = NULL;
	char				command;

	err = 0;

	usbkbmd = (usbkbm_state_t *)q->q_ptr;

	iocp = (struct iocblk *)mp->b_rptr;

	if (mp->b_cont != NULL) {
		data = (caddr_t)mp->b_cont->b_rptr;
	}

	switch (iocp->ioc_cmd) {
	case KIOCLAYOUT:
		{
		/* layout learned at attached time. */
		register mblk_t *datap;

		datap = allocb((int)sizeof (int), BPRI_HI);

		if (datap == NULL) {

			ioctlrespsize = sizeof (int);
			goto allocfailure;
		}

		*(int *)datap->b_wptr = usbkbmd->usbkbm_layout;

		datap->b_wptr += sizeof (int);

		if (mp->b_cont)
			freemsg(mp->b_cont);

		mp->b_cont = datap;

		iocp->ioc_count = sizeof (int);

		}
		break;

	case KIOCSLAYOUT:
		/*
		 * Supply a layout if not specified by the hardware, or
		 * override any that was specified.
		 */
		if (data != NULL)
			usbkbmd->usbkbm_layout = *(int *)data;
		else
			err = EINVAL;
		break;

	case KIOCCMD:
		/* Check if proper argument is passed */
		if (data == NULL) {
			err = EIO;
			break;
		}

		/* Sub command */
		command = (char)(*(int *)data);

		/*
		 * Check if this ioctl is followed by a previous
		 * KBD_CMD_SETLED command, in which case we take
		 * the command byte as the data for setting the LED
		 */
		if (usbkbmd->usbkbm_setled_second_byte) {
			usbkbm_streams_setled((struct kbtrans_hardware *)
						usbkbmd, command);
			usbkbmd->usbkbm_setled_second_byte = 0;
			break;
		}

		/*
		 * In  case of allocb failure, this will
		 * return the size of the allocation which
		 * failed so that it can be allocated later
		 * through bufcall.
		 */
		ioctlrespsize = 0;

		err = usbkbm_kioccmd(usbkbmd, mp, command, &ioctlrespsize);

		if (ioctlrespsize != 0) {
			goto allocfailure;
		}

		break;

	case CONSOPENPOLLEDIO:
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_ioctl CONSOPENPOLLEDIO");

		if (iocp->ioc_count != sizeof (struct cons_polledio *)) {

			USB_DPRINTF_L2(PRINT_MASK_ALL, usbkbm_log_handle,
				"usbkbm_ioctl:  wrong ioc_count");

			err = EIO;

			break;
		}

		usbkbmd->usbkbm_pending_link = mp;

		/*
		 * Get the polled input structure from hid
		 */
		mctlmsg.ioc_cmd = HID_OPEN_POLLED_INPUT;
		mctlmsg.ioc_count = 0;
		usbkbm_mctl_send(usbkbmd, mctlmsg, NULL, 0);

		/*
		 * Do not ack or nack the message, we will wait for the
		 * result of HID_OPEN_POLLED_INPUT
		 */
		return (KBTRANS_MESSAGE_HANDLED);

	case CONSCLOSEPOLLEDIO:
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_ioctl CONSCLOSEPOLLEDIO mp = 0x%x", mp);

		usbkbmd->usbkbm_pending_link = mp;

		/*
		 * Get the polled input structure from hid
		 */
		mctlmsg.ioc_cmd = HID_CLOSE_POLLED_INPUT;
		mctlmsg.ioc_count = 0;
		usbkbm_mctl_send(usbkbmd, mctlmsg, NULL, 0);

		/*
		 * Do not ack or nack the message, we will wait for the
		 * result of HID_CLOSE_POLLED_INPUT
		 */
		return (KBTRANS_MESSAGE_HANDLED);

	case CONSSETABORTENABLE:
		/*
		 * Nothing special to do for USB.
		 */
		break;


	default:
		return (KBTRANS_MESSAGE_NOT_HANDLED);
	}

	/*
	 * Send ACK/NACK to upper module for
	 * the messages that have been handled.
	 */
	if (err != 0) {
		iocp->ioc_rval = 0;
		iocp->ioc_error = err;
		mp->b_datap->db_type = M_IOCNAK;
	} else {
		iocp->ioc_rval = 0;
		iocp->ioc_error = 0;	/* brain rot */
		mp->b_datap->db_type = M_IOCACK;
	}

	/* Send the response back up the stream */
	putnext(usbkbmd->usbkbm_readq, mp);

	return (KBTRANS_MESSAGE_HANDLED);

allocfailure:
	/*
	 * We needed to allocate something to handle this "ioctl", but
	 * couldn't; save this "ioctl" and arrange to get called back when
	 * it's more likely that we can get what we need.
	 * If there's already one being saved, throw it out, since it
	 * must have timed out.
	 */
	if (usbkbmd->usbkbm_streams_iocpending != NULL)
		freemsg(usbkbmd->usbkbm_streams_iocpending);

	usbkbmd->usbkbm_streams_iocpending = mp;

	if (usbkbmd->usbkbm_streams_bufcallid) {

		qunbufcall(usbkbmd->usbkbm_readq,
			usbkbmd->usbkbm_streams_bufcallid);
	}
	usbkbmd->usbkbm_streams_bufcallid =
		qbufcall(usbkbmd->usbkbm_readq, ioctlrespsize, BPRI_HI,
			usbkbm_reioctl, usbkbmd);

	return (KBTRANS_MESSAGE_HANDLED);
}

/*
 * usbkbm_kioccmd :
 *	Handles KIOCCMD ioctl.
 */
static int
usbkbm_kioccmd(usbkbm_state_t *usbkbmd, register mblk_t *mp,
		char command, size_t *ioctlrepsize)
{

	register mblk_t			*datap;
	register struct iocblk		*iocp;
	int				err = 0;

	iocp = (struct iocblk *)mp->b_rptr;

	switch (command) {

		/* Keyboard layout command */
		case KBD_CMD_GETLAYOUT:
			/* layout learned at attached time. */
			datap = allocb((int)sizeof (int), BPRI_HI);

			/* Return error  on allocation failure */
			if (datap == NULL) {
				*ioctlrepsize = sizeof (int);
				return (EIO);
			}

			*(int *)datap->b_wptr = usbkbmd->usbkbm_layout;
			datap->b_wptr += sizeof (int);
			if (mp->b_cont)
				freemsg(mp->b_cont);
			mp->b_cont = datap;
			iocp->ioc_count = sizeof (int);
			break;

		case KBD_CMD_SETLED:
			/*
			 * Emulate type 4 keyboard :
			 * Ignore this ioctl; the following
			 * ioctl will specify the data byte for
			 * setting the LEDs; setting usbkbm_setled_second_byte
			 * will help recognizing that ioctl
			 */
			usbkbmd->usbkbm_setled_second_byte = 1;
			break;

		case KBD_CMD_RESET:
			break;

		case KBD_CMD_BELL:
			/*
			 * USB keyboards do not have a beeper
			 * in it, the generic beeper interface
			 * is used. Turn the beeper on.
			 */
			beeper_on(BEEP_TYPE4);
			break;

		case KBD_CMD_NOBELL:
			/*
			 * USB keyboards do not have a beeper
			 * in it, the generic beeper interface
			 * is used. Turn the beeper off.
			 */
			beeper_off();
			break;

		case KBD_CMD_CLICK:
			/* FALLTHRU */
		case KBD_CMD_NOCLICK:
			break;

		default:
			err = EIO;
			break;

	}

	return (err);
}


/*
 * usbkbm_rput :
 *	Put procedure for input from driver end of stream (read queue).
 */
static void
usbkbm_rput(register queue_t *q, register mblk_t *mp)
{
	usbkbm_state_t		*usbkbmd;

	usbkbmd = (usbkbm_state_t *)q->q_ptr;

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
		"usbkbm_rput");

	if (usbkbmd == 0) {
		freemsg(mp);	/* nobody's listening */

		return;
	}

	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(WR(q), FLUSHDATA);
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHDATA);

		freemsg(mp);
		return;

	case M_BREAK:
		/*
		 * Will get M_BREAK only if this is not the system
		 * keyboard, otherwise serial port will eat break
		 * and call kadb/OBP, without passing anything up.
		 */
		freemsg(mp);

		return;

	case M_DATA:
		if (!(usbkbmd->usbkbm_flags & USBKBM_OPEN)) {
			freemsg(mp);	/* not ready to listen */

			return;
		}

		break;

	case M_CTL:
		usbkbm_mctl_receive(q, mp);

		return;

	case M_IOCACK:
	case M_IOCNAK:
		(void) putnext(q, mp);

		return;

	default:

		(void) putnext(q, mp);

		return;
	}

	/*
	 * A data message, consisting of bytes from the keyboard.
	 * Ram them through the translator.
	 */
	usbkbm_unpack_usb_packet(usbkbmd, usbkbm_streams_callback,
		(uchar_t *)mp->b_rptr, usbkbmd->usbkbm_packet_size);

	freemsg(mp);
}

/*
 * usbkbm_mctl_receive :
 *	Handle M_CTL messages from hid. If we don't understand
 *	the command, send it up.
 */
static void
usbkbm_mctl_receive(register queue_t *q, register mblk_t *mp)
{
	register usbkbm_state_t *usbkbmd = (usbkbm_state_t *)q->q_ptr;
	register struct iocblk *iocp;
	caddr_t  data;
	mblk_t		*reply_mp;


	iocp = (struct iocblk *)mp->b_rptr;
	if (mp->b_cont != NULL)
		data = (caddr_t)mp->b_cont->b_rptr;

	switch (iocp->ioc_cmd) {

	case HID_SET_REPORT:
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive HID_SET mctl");
		freemsg(mp);
		/* Setting of the LED is not waiting for this message */
		break;

	case HID_GET_PARSER_HANDLE:
		usbkbmd->usbkbm_report_descr = *(hidparser_handle_t *)data;
		freemsg(mp);
		usbkbmd->usbkbm_flags &= ~USBKBM_QWAIT;
		break;

	case HID_OPEN_POLLED_INPUT:
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive HID_OPEN_POLLED_INPUT");

		/* Copy the information from hid into the state structure */
		bcopy(data, &usbkbmd->usbkbm_hid_callback,
				sizeof (hid_polled_input_callback_t));

		freemsg(mp);

		reply_mp = usbkbmd->usbkbm_pending_link;

		reply_mp->b_datap->db_type = M_IOCACK;

		usbkbmd->usbkbm_pending_link = NULL;

		/*
		 * We are given an appropriate-sized data block,
		 * and return a pointer to our structure in it.
		 * The structure is saved in the states structure
		 */
		*(cons_polledio_t **)reply_mp->b_cont->b_rptr =
			&usbkbmd->usbkbm_polled_info;

		(void) putnext(q, reply_mp);

		break;

	case HID_CLOSE_POLLED_INPUT:
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive HID_CLOSE_POLLED_INPUT");


		bzero(&usbkbmd->usbkbm_hid_callback,
				sizeof (hid_polled_input_callback_t));

		freemsg(mp);

		reply_mp = usbkbmd->usbkbm_pending_link;

		iocp = (struct iocblk *)reply_mp->b_rptr;

		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive reply reply_mp 0x%x cmd 0x%x",
			reply_mp, iocp->ioc_cmd);


		reply_mp->b_datap->db_type = M_IOCACK;

		usbkbmd->usbkbm_pending_link = NULL;

		(void) putnext(q, reply_mp);

		break;

	case HID_DISCONNECT_EVENT :
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive HID_DISCONNECT_EVENT");

		/*
		 * A hotplug is going to happen
		 * Send fake release events up
		 */
		kbtrans_streams_releaseall(usbkbmd->usbkbm_kbtrans);

		freemsg(mp);

		break;

	case HID_FULL_POWER :
		USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
			"usbkbm_mctl_receive HID_POWER_ON");

		/*
		 * A full power event : send setled command down to
		 * restore LED states
		 */

		usbkbm_streams_setled((struct kbtrans_hardware *)usbkbmd,
					usbkbm_led_state);

		freemsg(mp);

		break;

	default:
		(void) putnext(q, mp);
	}
}


/*
 * usbkbm_streams_setled :
 *	Update the keyboard LEDs to match the current keyboard state.
 *	Send LED state downstreams to hid driver.
 */
static void
usbkbm_streams_setled(struct kbtrans_hardware *kbtrans_hw, int state)
{
	struct iocblk	mctlmsg;
	mblk_t		*LED_message;
	hid_req_t	*LED_report;
	usbkbm_state_t	*usbkbmd;
	uchar_t		led_state;

	usbkbm_led_state = (uchar_t)state;

	usbkbmd = (usbkbm_state_t *)kbtrans_hw;

	LED_report = kmem_zalloc(sizeof (hid_req_t), KM_SLEEP);

	/*
	 * Send the request to the hid driver to set LED.
	 */

	/* Create an mblk_t */
	LED_message = allocb(sizeof (uchar_t), 0);
	ASSERT(LED_message != NULL);

	led_state = 0;

	/*
	 * Set the led state based on the state that is passed in.
	 */
	if (state & LED_NUM_LOCK) {
		led_state |= USB_LED_NUM_LOCK;
	}

	if (state & LED_COMPOSE) {
		led_state |= USB_LED_COMPOSE;
	}

	if (state & LED_SCROLL_LOCK) {
		led_state |= USB_LED_SCROLL_LOCK;
	}

	if (state & LED_CAPS_LOCK) {
		led_state |= USB_LED_CAPS_LOCK;
	}

	if (state & LED_KANA) {
		led_state |= USB_LED_KANA;
	}

	bcopy((void *)&led_state, (void *)LED_message->b_wptr, 1);

	LED_message->b_wptr = LED_message->b_wptr + 1;

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
		"usbkbm: Send Ctrl Request. Data is 0x%x",
		(uchar_t)*LED_message->b_rptr);

	LED_report->hid_req_version_no = HID_VERSION_V_0;
	LED_report->hid_req_wValue = REPORT_TYPE_OUTPUT;
	LED_report->hid_req_wLength = sizeof (uchar_t);
	LED_report->hid_req_data = LED_message;

	mctlmsg.ioc_cmd = HID_SET_REPORT;
	mctlmsg.ioc_count = sizeof (LED_report);
	usbkbm_mctl_send(usbkbmd, mctlmsg,
			    (char *)LED_report, (int)sizeof (hid_req_t));

	/*
	 * We are not waiting for response of HID_SET_REPORT
	 * mctl for setting the LED.
	 */
	kmem_free(LED_report, sizeof (hid_req_t));
}

/*
 * usbkbm_mctl_send :
 *	This function sends a M_CTL message to hid.
 *	We will use the same structure and message format that M_IOCTL uses,
 *	using struct iocblk.  buf is an optional buffer that can be copied to
 *	M_CTL message.
 */
static void
usbkbm_mctl_send(usbkbm_state_t *usbkbmd, struct iocblk mctlmsg,
		    char *buf, int len)
{
	mblk_t *bp1, *bp2;

	/*
	 * We must guarantee delievery of data.  Panic
	 * if we don't get the buffer.  This is clearly not acceptable
	 * beyond testing.  This will be changed later.
	 */
	if ((bp1 = allocb((int)sizeof (struct iocblk), NULL)) == NULL)
			cmn_err(CE_PANIC, "usbkbm_mctl_send: Can't allocate\
				block for M_CTL message");

	*((struct iocblk *)bp1->b_datap->db_base) = mctlmsg;

	bp1->b_datap->db_type = M_CTL;


	if (buf) {
		if ((bp2 = allocb(len, NULL)) == NULL)
			cmn_err(CE_PANIC, "usbkbm_mctl_send: Can't allocate\
				block for M_CTL message");
		bp1->b_cont = bp2;
		bcopy(buf, bp2->b_datap->db_base, len);
	}

	/*
	 *  we don't need to test for canputnext so long as we don't have
	 *  a local service procedure
	 */
	(void) putnext(usbkbmd->usbkbm_writeq, bp1);
}

/*
 * usbkbm_polled_is_scancode_available :
 *	This routine is called to determine if there is a scancode that
 *	is available for input.  This routine is called at poll time and
 *	returns a key/state pair to the caller.  If there are characters
 *	buffered up, the routine returns right away with the key/state pair.
 *	Otherwise, the routine calls down to check for characters and returns
 *	the first key/state pair if there are any characters pending.
 */
static boolean_t
usbkbm_polled_is_scancode_available(struct kbtrans_hardware *hw,
	int *key, enum keystate *state)
{
	usbkbm_state_t			*usbkbmd;
	uchar_t				*buffer;
	unsigned			num_keys;
	hid_polled_handle_t		hid_polled_handle;

	usbkbmd = (usbkbm_state_t *)hw;

	/*
	 * If there are already characters buffered up, then we are done.
	 */
	if (usbkbmd->usbkbm_polled_buffer_num_characters != 0) {

		usbkbm_get_scancode(usbkbmd, key, state);

		return (B_TRUE);
	}

	hid_polled_handle =
			usbkbmd->usbkbm_hid_callback.hid_polled_input_handle;

	num_keys = (usbkbmd->usbkbm_hid_callback.hid_polled_read)
				(hid_polled_handle, &buffer);

	/*
	 * If we don't get any characters back then indicate that, and we
	 * are done.
	 */
	if (num_keys == 0) {

		return (B_FALSE);
	}

	/*
	 * We have a usb packet, so pass this packet to
	 * usbkbm_unpack_usb_packet so that it can be broken up into
	 * individual key/state values.
	 */
	usbkbm_unpack_usb_packet(usbkbmd, usbkbm_poll_callback,
		buffer, num_keys);

	/*
	 * If a scancode was returned as a result of this packet,
	 * then translate the scancode.
	 */
	if (usbkbmd->usbkbm_polled_buffer_num_characters != 0) {

		usbkbm_get_scancode(usbkbmd, key, state);

		return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * usbkbm_streams_callback :
 *	This is the routine that is going to be called when unpacking
 *	usb packets for normal streams-based input.  We pass a pointer
 *	to this routine to usbkbm_unpack_usb_packet.  This routine will
 *	get called with an unpacked key (scancode) and state (press/release).
 *	We pass it to the generic keyboard module.
 */
static void
usbkbm_streams_callback(usbkbm_state_t *usbkbmd, int key, enum keystate state)
{
	kbtrans_streams_key(usbkbmd->usbkbm_kbtrans, key, state);
}

/*
 * usbkbm_poll_callback :
 *	This is the routine that is going to be called when unpacking
 *	usb packets for polled input.  We pass a pointer to this routine
 *	to usbkbm_unpack_usb_packet.  This routine will get called with
 *	an unpacked key (scancode) and state (press/release).  We will
 *	store the key/state pair into a circular buffer so that it can
 *	be translated into an ascii key later.
 */
static void
usbkbm_poll_callback(usbkbm_state_t *usbkbmd, int key, enum keystate state)
{
	/*
	 * Check to make sure that the buffer isn't already full
	 */
	if (usbkbmd->usbkbm_polled_buffer_num_characters ==
		USB_POLLED_BUFFER_SIZE) {

		/*
		 * The buffer is full, we will drop this character.
		 */
		return;
	}

	/*
	 * Save the scancode in the buffer
	 */
	usbkbmd->usbkbm_polled_buffer_head->poll_key = key;
	usbkbmd->usbkbm_polled_buffer_head->poll_state = state;

	/*
	 * We have one more character in the buffer
	 */
	usbkbmd->usbkbm_polled_buffer_num_characters++;

	/*
	 * Increment to the next available slot.
	 */
	usbkbmd->usbkbm_polled_buffer_head++;

	/*
	 * Check to see if the tail has wrapped.
	 */
	if (usbkbmd->usbkbm_polled_buffer_head -
		usbkbmd->usbkbm_polled_scancode_buffer ==
			USB_POLLED_BUFFER_SIZE) {

		usbkbmd->usbkbm_polled_buffer_head =
			usbkbmd->usbkbm_polled_scancode_buffer;
	}
}

/*
 * usbkbm_get_scancode :
 *	This routine retreives a key/state pair from the circular buffer.
 *	The pair was put in the buffer by usbkbm_poll_callback when a
 *	USB packet was translated into a key/state by usbkbm_unpack_usb_packet.
 */
static void
usbkbm_get_scancode(usbkbm_state_t *usbkbmd, int *key, enum keystate *state)
{
	/*
	 * Copy the character.
	 */
	*key = usbkbmd->usbkbm_polled_buffer_tail->poll_key;
	*state = usbkbmd->usbkbm_polled_buffer_tail->poll_state;

	/*
	 * Increment to the next character to be copied from
	 * and to.
	 */
	usbkbmd->usbkbm_polled_buffer_tail++;

	/*
	 * Check to see if the tail has wrapped.
	 */
	if (usbkbmd->usbkbm_polled_buffer_tail -
		usbkbmd->usbkbm_polled_scancode_buffer ==
			USB_POLLED_BUFFER_SIZE) {

		usbkbmd->usbkbm_polled_buffer_tail =
			usbkbmd->usbkbm_polled_scancode_buffer;
	}

	/*
	 * We have one less character in the buffer.
	 */
	usbkbmd->usbkbm_polled_buffer_num_characters--;
}

/*
 * usbkbm_polled_setled :
 *	This routine is a place holder.  Someday, we may want to allow led
 *	state to be updated from within polled mode.
 */
/* ARGSUSED */
static void
usbkbm_polled_setled(struct kbtrans_hardware *hw, int led_state)
{
	/* nothing to do for now */
}

/*
 * This is a pass-thru routine to get a character at poll time.
 */
static int
usbkbm_polled_getchar(struct cons_polledio_arg *arg)
{
	usbkbm_state_t			*usbkbmd;

	usbkbmd = (usbkbm_state_t *)arg;

	return (kbtrans_getchar(usbkbmd->usbkbm_kbtrans));
}

/*
 * This is a pass-thru routine to test if character is available for reading
 * at poll time.
 */
static boolean_t
usbkbm_polled_ischar(struct cons_polledio_arg *arg)
{
	usbkbm_state_t			*usbkbmd;

	usbkbmd = (usbkbm_state_t *)arg;

	return (kbtrans_ischar(usbkbmd->usbkbm_kbtrans));
}

/*
 * usbkbm_polled_input_enter :
 *	This is a pass-thru initialization routine for the lower layer drivers.
 *	This routine is called at poll time to set the state for polled input.
 */
static void
usbkbm_polled_enter(struct cons_polledio_arg *arg)
{
	usbkbm_state_t			*usbkbmd;
	hid_polled_handle_t		hid_polled_handle;

	usbkbmd = (usbkbm_state_t *)arg;

	hid_polled_handle =
		usbkbmd->usbkbm_hid_callback.hid_polled_input_handle;

	(void) (usbkbmd->usbkbm_hid_callback.hid_polled_input_enter)
					(hid_polled_handle);
}

/*
 * usbkbm_polled_input_exit :
 *	This is a pass-thru restoration routine for the lower layer drivers.
 *	This routine is called at poll time to reset the state back to streams
 *	input.
 */
static void
usbkbm_polled_exit(struct cons_polledio_arg *arg)
{
	usbkbm_state_t			*usbkbmd;
	hid_polled_handle_t		hid_polled_handle;

	usbkbmd = (usbkbm_state_t *)arg;

	hid_polled_handle =
			usbkbmd->usbkbm_hid_callback.hid_polled_input_handle;

	(void) (usbkbmd->usbkbm_hid_callback.hid_polled_input_exit)
			(hid_polled_handle);
}

/*
 * usbkbm_unpack_usb_packet :
 *	USB key packets contain 8 bytes while in boot protocol mode.
 *	The first byte contains bit packed modifier key information.
 *	Second byte is reserved. The last 6 bytes contain bytes of
 *	currently pressed keys. If a key was not recorded on the
 *	previous packet, but present in the current packet, then set
 *	state to KEY_PRESSED. If a key was recorded in the previous packet,
 *	but not present in the current packet, then state to KEY_RELEASED
 *	Follow a similar algorithm for bit packed modifier keys.
 */
static void
usbkbm_unpack_usb_packet(usbkbm_state_t *usbkbmd, process_key_callback_t func,
	uchar_t *usbpacket, int packet_size)
{

	uchar_t		mkb;
	uchar_t		lastmkb;
	static uchar_t	lastusbpacket[USBKBM_MAXPKTSIZE];
	int		uindex, lindex, rollover;

	mkb = usbpacket[0];

	lastmkb = lastusbpacket[0];

	for (uindex = 0; uindex < packet_size; uindex++) {

		USB_DPRINTF_L3(PRINT_MASK_PACKET, usbkbm_log_handle,
			" %x ", usbpacket[uindex]);
	}

	USB_DPRINTF_L3(PRINT_MASK_PACKET, usbkbm_log_handle,
			" is the usbkeypacket");

	/* check to see if modifier keys are different */
	if (mkb != lastmkb) {

		if ((mkb & USB_LSHIFTBIT) != (lastmkb & USB_LSHIFTBIT)) {
			(*func)(usbkbmd, USB_LSHIFTKEY, (mkb&USB_LSHIFTBIT) ?
				KEY_PRESSED : KEY_RELEASED);
			USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
				"unpack: sending USB_LSHIFTKEY");
		}

		if ((mkb & USB_LCTLBIT) != (lastmkb & USB_LCTLBIT)) {
			(*func)(usbkbmd, USB_LCTLCKEY, mkb&USB_LCTLBIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_LALTBIT) != (lastmkb & USB_LALTBIT)) {
			(*func)(usbkbmd, USB_LALTKEY, mkb&USB_LALTBIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_LMETABIT) != (lastmkb & USB_LMETABIT)) {
			(*func)(usbkbmd, USB_LMETAKEY, mkb&USB_LMETABIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_RMETABIT) != (lastmkb & USB_RMETABIT)) {
			(*func)(usbkbmd, USB_RMETAKEY, mkb&USB_RMETABIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_RALTBIT) != (lastmkb & USB_RALTBIT)) {
			(*func)(usbkbmd, USB_RALTKEY, mkb&USB_RALTBIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_RCTLBIT) != (lastmkb & USB_RCTLBIT)) {
			(*func)(usbkbmd, USB_RCTLCKEY, mkb&USB_RCTLBIT ?
				KEY_PRESSED : KEY_RELEASED);
		}

		if ((mkb & USB_RSHIFTBIT) != (lastmkb & USB_RSHIFTBIT)) {
			(*func)(usbkbmd, USB_RSHIFTKEY, mkb&USB_RSHIFTBIT ?
				KEY_PRESSED : KEY_RELEASED);
			USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
				"unpack: sending USB_RSHIFTKEY");
		}
	}

	/* save modifier bits */
	lastusbpacket[0] = usbpacket[0];

	/* Check Keyboard rollover error. */
	if (usbpacket[2] == USB_ERRORROLLOVER) {
		rollover = 1;
		for (uindex = 3; uindex < packet_size;
			uindex++) {
			if (usbpacket[uindex] != USB_ERRORROLLOVER) {
				rollover = 0;
				break;
			}
		}
		if (rollover) {
			USB_DPRINTF_L3(PRINT_MASK_ALL, usbkbm_log_handle,
				"unpack: errorrollover");
			return;
		}
	}

	/* check for new presses */
	for (uindex = 2; uindex < packet_size &&
		usbpacket[uindex]; uindex ++) {
		int newkey = 1;

		for (lindex = 2; lindex < packet_size &&
			lastusbpacket[lindex];
			lindex ++)
			if (usbpacket[uindex] == lastusbpacket[lindex]) {
				newkey = 0;
				break;
			}
		if (newkey) {
			(*func)(usbkbmd, usbpacket[uindex], KEY_PRESSED);
		}
	}

	/* check for released keys */
	for (lindex = 2; lindex < packet_size &&
		lastusbpacket[lindex]; lindex++) {
		int released = 1;

		for (uindex = 2; uindex < packet_size &&
			usbpacket[uindex];
			uindex++)
			if (usbpacket[uindex] == lastusbpacket[lindex]) {
				released = 0;
				break;
			}
		if (released)
			(*func)(usbkbmd, lastusbpacket[lindex], KEY_RELEASED);
	}

	/* save current packet */
	for (uindex = 2; uindex < packet_size; uindex ++)
		lastusbpacket[uindex] = usbpacket[uindex];
}

/*
 * usbkbm_reioctl :
 *	This function is set up as call-back function should an ioctl fail.
 *	It retries the ioctl
 */
static void
usbkbm_reioctl(void	*arg)
{
	usbkbm_state_t	*usbkbmd;
	mblk_t *mp;

	usbkbmd = (usbkbm_state_t *)arg;

	usbkbmd->usbkbm_streams_bufcallid = 0;

	if ((mp = usbkbmd->usbkbm_streams_iocpending) != NULL) {

		/* not pending any more */
		usbkbmd->usbkbm_streams_iocpending = NULL;

		(void) usbkbm_ioctl(usbkbmd->usbkbm_writeq, mp);
	}
}
