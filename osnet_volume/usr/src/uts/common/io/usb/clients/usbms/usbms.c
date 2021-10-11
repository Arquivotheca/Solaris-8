/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident "@(#)usbms.c 1.4     99/10/22 SMI"

#include <sys/usb/usba.h>
#include <sys/usb/clients/hid/hid.h>
#include <sys/usb/clients/hidparser/hidparser.h>

#include <sys/stropts.h>

#include <sys/vuid_event.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/strtty.h>
#include <sys/msreg.h>
#include <sys/msio.h>

#include <sys/usb/clients/usbms/usbms.h>

/* debugging information */
static uint_t	usbms_errmask = (uint_t)PRINT_MASK_ALL;
static uint_t	usbms_errlevel = USB_LOG_L2;
static usb_log_handle_t usbms_log_handle;

static struct streamtab		usbms_streamtab;

static struct fmodsw fsw = {
			"usbms",
			&usbms_streamtab,
			D_NEW | D_MP | D_MTPERMOD
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops		mod_strmodops;

static struct modlstrmod modlstrmod = {
			&mod_strmodops,
			"USB mouse streams 1.4",
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

	if (rval == 0) {
		usbms_log_handle = usb_alloc_log_handle(NULL, "usbms",
			&usbms_errlevel, &usbms_errmask, NULL, NULL, 0);
	}

	return (rval);
}

int
_fini()
{
	int rval = mod_remove(&modlinkage);

	if (rval == 0) {
		usb_free_log_handle(usbms_log_handle);
	}

	return (rval);
}


int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/* Function prototypes */
static void		usbms_reioctl(void *);
static void		usbms_ioctl(queue_t *, mblk_t *);
static int		usbms_open();
static int		usbms_close();
static int		usbms_wput();
static void		usbms_rput();
static void		usbms_mctl_receive(
				register queue_t	*q,
				register mblk_t		*mp);

static void		usbms_mctl_send(
				usbms_state_t		*usbmsd,
				struct iocblk		mctlmsg,
				char			*buf,
				int			len);

static void		usbms_rserv(queue_t		*q);

static void		usbms_resched(void *);

static int		usbms_getparms(
				register Ms_parms	*data,
				usbms_state_t		*usbmsp);

static int		usbms_setparms(
				register Ms_parms	*data,
				usbms_state_t		*usbmsp);

static void		usbms_flush(usbms_state_t	*usbmsp);

static void		usbms_incr(void *);
static void		usbms_input(
				usbms_state_t		*usbmsp,
				char			c);
static void		usbms_rserv_vuid_button(
				queue_t			*q,
				struct mouseinfo	*mi,
				mblk_t			**bpaddr);

static void		usbms_rserv_vuid_event_y(
				queue_t			*q,
				struct mouseinfo	*mi,
				mblk_t			**bpaddr);
static void		usbms_rserv_vuid_event_x(
				queue_t			*q,
				struct mouseinfo	*mi,
				mblk_t			**bpaddr);

extern void		uniqtime32();
extern int		getiocseqno();


/*
 * Device driver qinit functions
 */
static struct module_info usbms_mod_info = {
	0x0ffff,		/* module id number */
	"usbms",		/* module name */
	0,			/* min packet size accepted */
	INFPSZ,			/* max packet size accepted */
	512,			/* hi-water mark */
	128			/* lo-water mark */
};

/* read side queue information structure */
static struct qinit rinit = {
	(int (*)())usbms_rput,	/* put procedure not needed */
	(int (*)())usbms_rserv, /* service procedure */
	usbms_open,		/* called on startup */
	usbms_close,		/* called on finish */
	NULL,			/* for future use */
	&usbms_mod_info,	/* module information structure */
	NULL			/* module statistics structure */
};

/* write side queue information structure */
static struct qinit winit = {
	usbms_wput,		/* put procedure */
	NULL,			/* no service proecedure needed */
	NULL,			/* open not used on write side */
	NULL,			/* close not used on write side */
	NULL,			/* for future use */
	&usbms_mod_info,	/* module information structure */
	NULL			/* module statistics structure */
};

static struct streamtab usbms_streamtab = {
	&rinit,
	&winit,
	NULL,			/* not a MUX */
	NULL			/* not a MUX */
};

/*
 * Message when overrun circular buffer
 */
static int			overrun_msg;

/* Increment when overrun circular buffer */
static int			overrun_cnt;

extern int			hz;

/*
 * Mouse buffer size in bytes.	Place here as variable so that one could
 * massage it using adb if it turns out to be too small.
 */
static uint16_t			usbms_buf_bytes = USBMS_BUF_BYTES;


/*
 * Regular STREAMS Entry points
 */

/*
 * usbms_open() :
 *	open() entry point for the USB mouse module.
 */
/*ARGSUSED*/
static int
usbms_open(queue_t			*q,
	dev_t				*devp,
	int				flag,
	int				sflag,
	cred_t				*credp)

{
	register struct mousebuf	*mousebufp;
	register struct ms_softc	*msd_soft;
	usbms_state_t			*usbmsp;
	struct iocblk			mctlmsg;


	/* Clone opens are not allowed */
	if (sflag != MODOPEN)
		return (EINVAL);

	/* If the module is already open, just return */
	if (q->q_ptr) {
		return (0);
	}

	/* allocate usbms state structure */
	usbmsp = kmem_zalloc(sizeof (usbms_state_t), KM_SLEEP);

	q->q_ptr = usbmsp;
	WR(q)->q_ptr = usbmsp;

	usbmsp->usbms_rq_ptr = q;
	usbmsp->usbms_wq_ptr = WR(q);

	qprocson(q);

	/*
	 * Set up private data.
	 */
	usbmsp->usbms_state = USBMS_WAIT_BUTN;
	usbmsp->usbms_iocpending = NULL;
	usbmsp->usbms_jitter_thresh = USBMS_JITTER_THRESH;
	usbmsp->usbms_speedlimit = USBMS_SPEEDLIMIT;
	usbmsp->usbms_speedlaw = USBMS_SPEEDLAW;
	usbmsp->usbms_speed_count = USBMS_SPEED_COUNT;

	msd_soft = &usbmsp->usbms_softc;

	/*
	 * Initially set the format to MS_VUID_FORMAT
	 */
	msd_soft->ms_readformat = MS_VUID_FORMAT;

	/*
	 * Allocate buffer and initialize data.
	 */
	if (msd_soft->ms_buf == 0) {
		msd_soft->ms_bufbytes = usbms_buf_bytes;
		mousebufp = kmem_zalloc((uint_t)msd_soft->ms_bufbytes,
					KM_SLEEP);
		mousebufp->mb_size = (uint16_t)(1 + (msd_soft->ms_bufbytes -
			sizeof (struct mousebuf)) / sizeof (struct mouseinfo));
		msd_soft->ms_buf = mousebufp;
		msd_soft->ms_vuidaddr = VKEY_FIRST;
		usbmsp->usbms_jittertimeout = JITTER_TIMEOUT;
		usbms_flush(usbmsp);
	}

	/* request hid report descriptor from HID */
	mctlmsg.ioc_cmd = HID_GET_PARSER_HANDLE;
	mctlmsg.ioc_count = 0;
	usbms_mctl_send(usbmsp, mctlmsg, NULL, NULL);

	usbmsp->usbms_flags |= USBMS_QWAIT;
	while (usbmsp->usbms_flags & USBMS_QWAIT)
		qwait(q);

	if (usbmsp->usbms_report_descr_handle != NULL) {
		if (hidparser_get_usage_attribute(
				usbmsp->usbms_report_descr_handle,
				0,
				HIDPARSER_ITEM_INPUT,
				USBMS_USAGE_PAGE_BUTTON,
				0,
				HIDPARSER_ITEM_REPORT_COUNT,
				(int32_t *)&usbmsp->usbms_num_buttons) ==
					HID_SUCCESS) {
			USB_DPRINTF_L3(PRINT_MASK_OPEN,
				usbms_log_handle, "Num of buttons is : %d",
				usbmsp->usbms_num_buttons);
		} else {
			USB_DPRINTF_L3(PRINT_MASK_OPEN,
				usbms_log_handle,
				"hidparser_get_usage_attribute failed : "
				"Set to default number of buttons(3).");

			usbmsp->usbms_num_buttons = USB_MS_DEFAULT_BUTTON_NO;
		}
		if ((usbmsp->usbms_num_buttons < 2) ||
			(usbmsp->usbms_num_buttons > 3)) {
			USB_DPRINTF_L1(PRINT_MASK_ALL,
				usbms_log_handle, "Invalid "
				"number of buttons: %d. Set to default value "
				"(3 buttons)", usbmsp->usbms_num_buttons);

			usbmsp->usbms_num_buttons = USB_MS_DEFAULT_BUTTON_NO;
		}
	} else {
		USB_DPRINTF_L1(PRINT_MASK_ALL,
			usbms_log_handle, "usbms: Invalid HID "
			"Descriptor Tree. Set to default value(3 buttons).");
		usbmsp->usbms_num_buttons = USB_MS_DEFAULT_BUTTON_NO;
	}

	usbmsp->usbms_flags |= USBMS_OPEN;

	USB_DPRINTF_L3(PRINT_MASK_OPEN, usbms_log_handle,
		"usbms_open exiting");

	return (0);
}


/*
 * usbms_close() :
 *	close() entry point for the USB mouse module.
 */
/*ARGSUSED*/
static int
usbms_close(queue_t			*q,
	int 				flag,
	cred_t 				*credp)
{
	usbms_state_t			*usbmsp = q->q_ptr;
	register struct ms_softc 	*ms;

	USB_DPRINTF_L3(PRINT_MASK_CLOSE, usbms_log_handle,
		"usbms_close entering");

	qprocsoff(q);

	if (usbmsp->usbms_jitter) {
		(void) quntimeout(q,
			(timeout_id_t)(long)usbmsp->usbms_timeout_id);
		usbmsp->usbms_jitter = 0;
	}
	if (usbmsp->usbms_reioctl_id) {
		qunbufcall(q, (bufcall_id_t)(long)usbmsp->usbms_reioctl_id);
		usbmsp->usbms_reioctl_id = 0;
	}
	if (usbmsp->usbms_resched_id) {
		qunbufcall(q, (bufcall_id_t)usbmsp->usbms_resched_id);
		usbmsp->usbms_resched_id = 0;
	}
	if (usbmsp->usbms_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(usbmsp->usbms_iocpending);
		usbmsp->usbms_iocpending = NULL;
	}

	ms = &usbmsp->usbms_softc;

	/* Free mouse buffer */
	if (ms->ms_buf != NULL) {
		kmem_free(ms->ms_buf, ms->ms_bufbytes);
	}

	kmem_free(usbmsp, sizeof (usbms_state_t));

	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;


	USB_DPRINTF_L3(PRINT_MASK_CLOSE, usbms_log_handle,
		"usbms_close exiting");

	return (0);
}


/*
 * usbms_rserv() :
 *	Read queue service routine.
 *	Turn buffered mouse events into stream messages.
 */
static void
usbms_rserv(queue_t		*q)
{
	usbms_state_t		*usbmsp = q->q_ptr;
	struct ms_softc	*ms;
	struct mousebuf	*b;
	struct mouseinfo	*mi;
	mblk_t				*bp;

	ms = &usbmsp->usbms_softc;
	b = ms->ms_buf;

	USB_DPRINTF_L3(PRINT_MASK_SERV, usbms_log_handle,
		"usbms_rserv entering");

	while (canputnext(q) && ms->ms_oldoff != b->mb_off) {
		mi = &b->mb_info[ms->ms_oldoff];
		switch (ms->ms_readformat) {

		case MS_3BYTE_FORMAT: {
			register char	*cp;

			if ((bp = allocb(3, BPRI_HI)) != NULL) {
				cp = (char *)bp->b_wptr;

				*cp++ = 0x80 | mi->mi_buttons;
				/* Update read buttons */
				ms->ms_prevbuttons = mi->mi_buttons;

				*cp++ = mi->mi_x;
				*cp++ = -mi->mi_y;
				/* lower pri to avoid mouse droppings */
				bp->b_wptr = (uchar_t *)cp;
				(void) putnext(q, bp);
			} else {
				if (usbmsp->usbms_resched_id) {
					qunbufcall(q,
					(bufcall_id_t)usbmsp->usbms_resched_id);
				}
				usbmsp->usbms_resched_id = qbufcall(q,
							(size_t)3,
							(uint_t)BPRI_HI,
						(void (*)())usbms_resched,
							(void *) usbmsp);
				if (usbmsp->usbms_resched_id == 0)

					return;	/* try again later */
				/* bufcall failed; just pitch this event */
				/* or maybe flush queue? */
			}
			ms->ms_oldoff++;	/* next event */

			/* circular buffer wraparound */
			if (ms->ms_oldoff >= b->mb_size) {
				ms->ms_oldoff = 0;
			}
			break;
		}

		case MS_VUID_FORMAT: {

			bp = NULL;

			switch (ms->ms_eventstate) {

			case EVENT_BUT3:  /* Send right button information */
			case EVENT_BUT2:  /* Send middle button information */
			case EVENT_BUT1:  /* Send left button information */
				usbms_rserv_vuid_button(q, mi, &bp);
				break;

			case EVENT_Y:
				usbms_rserv_vuid_event_y(q, mi, &bp);
				break;

			case EVENT_X:
				usbms_rserv_vuid_event_x(q, mi, &bp);
				break;

			}
			if (bp != NULL) {
				/* lower pri to avoid mouse droppings */
				bp->b_wptr += sizeof (Firm_event);
				(void) putnext(q, bp);
			}
			if (ms->ms_eventstate == EVENT_X) {
				ms->ms_eventstate = EVENT_BUT3;
				ms->ms_oldoff++;	/* next event */

				/* circular buffer wraparound */
				if (ms->ms_oldoff >= b->mb_size) {
					ms->ms_oldoff = 0;
				}
			} else
				ms->ms_eventstate--;
		}
		}
	}
	USB_DPRINTF_L3(PRINT_MASK_SERV, usbms_log_handle,
		"usbms_rserv exiting");
}

/*
 * usbms_rserv_vuid_button() :
 *	Process a VUID button event
 */
static void
usbms_rserv_vuid_button(queue_t			*q,
			struct mouseinfo	*mi,
			mblk_t			**bpaddr)
{
	usbms_state_t		*usbmsp = q->q_ptr;
	struct ms_softc	*ms;
	int			button_number;
	int			hwbit;
	Firm_event		*fep;
	mblk_t			*bp;

	ms = &usbmsp->usbms_softc;

	/* Test button. Send an event if it changed. */
	button_number = ms->ms_eventstate - EVENT_BUT1;
	hwbit = USBMS_BUT3 >> button_number;

	if ((ms->ms_prevbuttons & hwbit) !=
			(mi->mi_buttons & hwbit)) {
		if ((bp = allocb(sizeof (Firm_event),
			BPRI_HI)) != NULL) {
			*bpaddr = bp;
			fep = (Firm_event *)bp->b_wptr;
			fep->id = vuid_id_addr(
				ms->ms_vuidaddr) |
				vuid_id_offset(BUT(1)
				+ button_number);
			fep->pair_type = FE_PAIR_NONE;
			fep->pair = 0;

			/*
			 * Update read buttons and set
			 * value
			 */
			if (mi->mi_buttons & hwbit) {
				fep->value = 0;
				ms->ms_prevbuttons |=
					hwbit;
			} else {
				fep->value = 1;
				ms->ms_prevbuttons &=
						~hwbit;
			}
			fep->time = mi->mi_time;

		} else {
			if (usbmsp->usbms_resched_id) {
				qunbufcall(q,
				(bufcall_id_t)usbmsp->usbms_resched_id);
			}
			usbmsp->usbms_resched_id =
				qbufcall(q,
				sizeof (Firm_event),
				BPRI_HI,
			(void (*)())usbms_resched,
				(void *) usbmsp);
			if (usbmsp->usbms_resched_id
				== 0)
				/* try again later */
				return;
			/*
			 * bufcall failed; just pitch
			 * this event
			 */
			/* or maybe flush queue? */
			ms->ms_eventstate = EVENT_X;
		}
	}
}

/*
 * usbms_rserv_vuid_event_y() :
 *	Process a VUID y-event
 */
static void
usbms_rserv_vuid_event_y(register queue_t		*q,
			register struct mouseinfo	*mi,
			mblk_t				**bpaddr)
{
	usbms_state_t			*usbmsp = q->q_ptr;
	register struct ms_softc	*ms;
	register Firm_event		*fep;
	mblk_t				*bp;

	ms = &usbmsp->usbms_softc;

	/* Send y if changed. */
	if (mi->mi_y != 0) {
		if ((bp = allocb(sizeof (Firm_event),
			BPRI_HI)) != NULL) {
			*bpaddr = bp;
			fep = (Firm_event *)bp->b_wptr;
			fep->id = vuid_id_addr(
				ms->ms_vuidaddr) |
				vuid_id_offset(
					LOC_Y_DELTA);
			fep->pair_type =
				FE_PAIR_ABSOLUTE;
			fep->pair =
				(uchar_t)LOC_Y_ABSOLUTE;
			fep->value = -mi->mi_y;
			fep->time = mi->mi_time;
		} else {
			if (usbmsp->usbms_resched_id) {
				qunbufcall(q,
		(bufcall_id_t)usbmsp->usbms_resched_id);
			}
			usbmsp->usbms_resched_id =
				qbufcall(q,
				sizeof (Firm_event),
				BPRI_HI,
			(void (*)())usbms_resched,
				(void *)usbmsp);
			if (usbmsp->usbms_resched_id
				== 0) {
				/* try again later */
				return;
			}

			/*
			 * bufcall failed; just pitch
			 * this event
			 */
			/* or maybe flush queue? */
			ms->ms_eventstate = EVENT_X;
		}
	}
}

/*
 * usbms_rserv_vuid_event_x() :
 *	Process a VUID x-event
 */
static void
usbms_rserv_vuid_event_x(register queue_t		*q,
			register struct mouseinfo	*mi,
			mblk_t				**bpaddr)
{
	usbms_state_t			*usbmsp = q->q_ptr;
	register struct ms_softc	*ms;
	register Firm_event		*fep;
	mblk_t				*bp;

	ms = &usbmsp->usbms_softc;

	/* Send x if changed. */
	if (mi->mi_x != 0) {
		if ((bp = allocb(sizeof (Firm_event),
			BPRI_HI)) != NULL) {
			*bpaddr = bp;
			fep = (Firm_event *)bp->b_wptr;
			fep->id = vuid_id_addr(
				ms->ms_vuidaddr) |
			vuid_id_offset(LOC_X_DELTA);
			fep->pair_type =
				FE_PAIR_ABSOLUTE;
			fep->pair =
				(uchar_t)LOC_X_ABSOLUTE;
			fep->value = mi->mi_x;
			fep->time = mi->mi_time;
		} else {
			if (usbmsp->usbms_resched_id)
				qunbufcall(q,
	(bufcall_id_t)usbmsp->usbms_resched_id);
			usbmsp->usbms_resched_id =
				qbufcall(q,
				sizeof (Firm_event),
				BPRI_HI,
		(void (*)())usbms_resched,
				(void *) usbmsp);
			if (usbmsp->usbms_resched_id
				== 0)
				/* try again later */
				return;

			/*
			 * bufcall failed; just
			 * pitch this event
			 */
			/* or maybe flush queue? */
			ms->ms_eventstate = EVENT_X;
		}
	}
}

/*
 * usbms_resched() :
 *	Callback routine for the qbufcall() in case
 *	of allocb() failure. When buffer becomes
 *	available, this function is called and
 *	enables the queue.
 */
static void
usbms_resched(void 	* usbmsp)
{
	register queue_t	*q;
	register usbms_state_t	*tmp_usbmsp = (usbms_state_t *)usbmsp;

	tmp_usbmsp->usbms_resched_id = 0;
	if ((q = tmp_usbmsp->usbms_rq_ptr) != 0)
		qenable(q);	/* run the service procedure */
}

/*
 * usbms_wput() :
 *	wput() routine for the mouse module.
 *	Module below : hid, module above : consms
 */
static int
usbms_wput(queue_t		*q,
	mblk_t			*mp)
{
	USB_DPRINTF_L3(PRINT_MASK_ALL, usbms_log_handle,
			"usbms_wput entering");
	switch (mp->b_datap->db_type) {

	case M_FLUSH:  /* Canonical flush handling */
		if (*mp->b_rptr & FLUSHW) {
			flushq(q, FLUSHDATA);
		} else {
			if (*mp->b_rptr & FLUSHR)	{
				flushq(RD(q), FLUSHDATA);
			} else {
				freemsg(mp);
			}
		}
		break;

	case M_IOCTL:
		usbms_ioctl(q, mp);
		break;

	default:
		(void) putnext(q, mp); /* pass it down the line. */
	}

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbms_log_handle,
		"usbms_wput exiting");

	return (0);
}


/*
 * usbms_ioctl() :
 *	Process ioctls we recognize and own.  Otherwise, NAK.
 */
static void
usbms_ioctl(register queue_t		*q,
		register mblk_t		*mp)
{
	usbms_state_t *usbmsp = (usbms_state_t *)q->q_ptr;
	register struct ms_softc 	*ms;
	register struct iocblk 		*iocp;
	caddr_t  			data;
	Vuid_addr_probe			*addr_probe;
	uint_t				ioctlrespsize;
	int				err = 0;

	USB_DPRINTF_L3(PRINT_MASK_IOCTL, usbms_log_handle,
		"usbms_ioctl entering");

	if (usbmsp == 0) {
		err = EINVAL;
		goto done;
	}
	ms = &usbmsp->usbms_softc;

	iocp = (struct iocblk *)mp->b_rptr;
	if (mp->b_cont != NULL) {
		data = (caddr_t)mp->b_cont->b_rptr;
	}

	switch (iocp->ioc_cmd) {

	case VUIDSFORMAT:


		if (*(int *)data == ms->ms_readformat) {
			break;
		}
		ms->ms_readformat = *(int *)data;
		/*
		 * Flush mouse buffer because the messages upstream of us
		 * are in the old format.
		 */

		usbms_flush(usbmsp);
		break;

	case VUIDGFORMAT: {
		register mblk_t		*datap;

		if ((datap = allocb(sizeof (int), BPRI_HI)) == NULL) {
			ioctlrespsize = sizeof (int);
			goto allocfailure;
		}
		*(int *)datap->b_wptr = ms->ms_readformat;
		datap->b_wptr += sizeof (int);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
		}
		mp->b_cont = datap;
		iocp->ioc_count = sizeof (int);
		break;
	}

	case VUIDSADDR:
		addr_probe = (Vuid_addr_probe *) data;
		if (addr_probe->base != VKEY_FIRST) {
			err = ENODEV;
			break;
		}
		ms->ms_vuidaddr = addr_probe->data.next;
		break;

	case VUIDGADDR:
		addr_probe = (Vuid_addr_probe *) data;
		if (addr_probe->base != VKEY_FIRST) {
			err = ENODEV;
			break;
		}
		addr_probe->data.current = ms->ms_vuidaddr;
		break;

	case MSIOGETPARMS: {
		register mblk_t 	*datap;

		if ((datap = allocb(sizeof (Ms_parms), BPRI_HI)) == NULL) {
			ioctlrespsize = sizeof (Ms_parms);
			goto allocfailure;
		}
		err = usbms_getparms((Ms_parms *)datap->b_wptr, usbmsp);
		datap->b_wptr += sizeof (Ms_parms);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
		}
		mp->b_cont = datap;
		iocp->ioc_count = sizeof (Ms_parms);
		break;
	}

	case MSIOSETPARMS:
		err = usbms_setparms((Ms_parms *)data, usbmsp);
		break;


	case MSIOBUTTONS: {
		register mblk_t		*datap;

		if ((datap = allocb(sizeof (int), BPRI_HI)) == NULL) {
			ioctlrespsize = sizeof (int);
			goto allocfailure;
		}
		*(int *)datap->b_wptr = (int)usbmsp->usbms_num_buttons;
		datap->b_wptr += sizeof (int);
		if (mp->b_cont) {
			freemsg(mp->b_cont);
		}
		mp->b_cont = datap;
		iocp->ioc_count = sizeof (int);
		break;
	}

	default:
	    (void) putnext(q, mp); /* pass it down the line */

	    return;
	} /* switch */

done:
	if (err != 0) {
		iocp->ioc_rval = 0;
		iocp->ioc_error = err;
		mp->b_datap->db_type = M_IOCNAK;
	} else {
		iocp->ioc_rval = 0;
		iocp->ioc_error = 0;
		mp->b_datap->db_type = M_IOCACK;
	}
	qreply(q, mp);

	return;

allocfailure:
	/*
	 * We needed to allocate something to handle this "ioctl", but
	 * couldn't; save this "ioctl" and arrange to get called back when
	 * it's more likely that we can get what we need.
	 * If there's already one being saved, throw it out, since it
	 * must have timed out.
	 */
	if (usbmsp->usbms_iocpending != NULL) {
		freemsg(usbmsp->usbms_iocpending);
	}
	usbmsp->usbms_iocpending = mp;
	if (usbmsp->usbms_reioctl_id) {
		qunbufcall(q, (bufcall_id_t)usbmsp->usbms_reioctl_id);
	}
	usbmsp->usbms_reioctl_id = qbufcall(q, ioctlrespsize, BPRI_HI,
					(void (*)())usbms_reioctl,
					(void *)usbmsp);
}


/*
 * usbms_reioctl() :
 *	This function is set up as call-back function should an ioctl fail.
 *	It retries the ioctl.
 */
static void
usbms_reioctl(void	* usbms_addr)
{
	usbms_state_t *usbmsp = (usbms_state_t *)usbms_addr;
	register queue_t 	*q;
	register mblk_t 	*mp;

	q = usbmsp->usbms_wq_ptr;
	if ((mp = usbmsp->usbms_iocpending) != NULL) {
		usbmsp->usbms_iocpending = NULL; /* not pending any more */
		usbms_ioctl(q, mp);
	}
}

/*
 * usbms_getparms() :
 *	Called from MSIOGETPARMS ioctl to get the
 *	current jitter_thesh, speed_law and speed_limit
 *	values.
 */
static int
usbms_getparms(register Ms_parms	*data,
		usbms_state_t		*usbmsp)
{
	data->jitter_thresh = usbmsp->usbms_jitter_thresh;
	data->speed_law = usbmsp->usbms_speedlaw;
	data->speed_limit = usbmsp->usbms_speedlimit;

	return (0);
}


/*
 * usbms_setparms() :
 *	Called from MSIOSETPARMS ioctl to set the
 *	current jitter_thesh, speed_law and speed_limit
 *	values.
 */
static int
usbms_setparms(register Ms_parms	*data,
		usbms_state_t		*usbmsp)
{
	usbmsp->usbms_jitter_thresh = data->jitter_thresh;
	usbmsp->usbms_speedlaw = data->speed_law;
	usbmsp->usbms_speedlimit = data->speed_limit;

	return (0);
}

/*
 * usbms_flush() :
 *	Resets the ms_softc structure to default values
 *	and sends M_FLUSH above.
 */
static void
usbms_flush(usbms_state_t		*usbmsp)
{
	register struct ms_softc *ms = &usbmsp->usbms_softc;
	register queue_t		*q;

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbms_log_handle,
		"usbms_flush entering");

	ms->ms_oldoff = 0;
	ms->ms_eventstate = EVENT_BUT3;
	ms->ms_buf->mb_off = 0;
	ms->ms_prevbuttons = USBMS_BUT1 | USBMS_BUT2 | USBMS_BUT3;
	usbmsp->usbms_oldbutt = ms->ms_prevbuttons;
	if ((q = usbmsp->usbms_rq_ptr) != NULL && q->q_next != NULL) {
		(void) putnextctl1(q, M_FLUSH, FLUSHR);
	}

	USB_DPRINTF_L3(PRINT_MASK_ALL, usbms_log_handle,
		"usbms_flush exiting");
}


/*
 * usbms_rput() :
 *	Put procedure for input from driver end of stream (read queue).
 */
static void
usbms_rput(queue_t		*q,
		mblk_t		*mp)
{
	usbms_state_t *usbmsp = q->q_ptr;
	register char		*readp;
	int	readcount;
	mblk_t	*tmp_mp;

	/* Maintain the original mp */
	tmp_mp = mp;

	if (usbmsp == 0) {
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
		 * We don't have to handle this
		 * because nothing is sent from the downstream
		 */

		freemsg(mp);

		return;

	case M_DATA:
		if (!(usbmsp->usbms_flags & USBMS_OPEN)) {
			freemsg(mp);	/* not ready to listen */

			return;
		}
		break;

	case M_CTL:
		usbms_mctl_receive(q, mp);

		return;

	default:
		(void) putnext(q, mp);

		return;
	}

	/*
	 * A data message, consisting of bytes from the mouse.
	 * Hand each byte to our input routine.
	 * Our driver can handle only 3-byte packet.
	 * So, ignore the 4th byte and rest if any device
	 * is sending a bigger packet.
	 */
	do {
		readp = (char *)tmp_mp->b_rptr;
		readcount = 1;
		while (readp < (char *)tmp_mp->b_wptr) {
			if (readcount++ > 3) {
				readp++;
				USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR,
				usbms_log_handle,
				"Trashing 4th byte and on");
				break;
			} else {
				usbms_input(usbmsp, *readp++);
			}
		}
		tmp_mp->b_rptr = (unsigned char *)readp;
	} while ((tmp_mp = tmp_mp->b_cont) != NULL);   /* next block, if any */

	freemsg(mp);
}


/*
 * usbms_mctl_receive() :
 *	Handle M_CTL messages from hid.  If
 *	we don't understand the command, free message.
 */
static void
usbms_mctl_receive(register queue_t		*q,
			register mblk_t		*mp)
{
	usbms_state_t *usbmsd = (usbms_state_t *)q->q_ptr;
	struct iocblk				*iocp;
	caddr_t					data;


	iocp = (struct iocblk *)mp->b_rptr;
	if (mp->b_cont != NULL)
		data = (caddr_t)mp->b_cont->b_rptr;

	switch (iocp->ioc_cmd) {

	case HID_GET_PARSER_HANDLE:
		usbmsd->usbms_report_descr_handle =
			*(hidparser_handle_t *)data;
		freemsg(mp);
		usbmsd->usbms_flags &= ~USBMS_QWAIT;
		break;

	default:
	    freemsg(mp);
	    break;
	}
}


/*
 * usbms_mctl_send() :
 *	This function sends down a M_CTL message to the hid driver.
 */
static void
usbms_mctl_send(usbms_state_t		*usbmsd,
		struct iocblk		mctlmsg,
		char			*buf,
		int			len)
{
	mblk_t 				*bp1, *bp2;

	/*
	 * We must guarantee delievery of data.  Panic
	 * if we don't get the buffer.  This is clearly not acceptable
	 * beyond testing.  This will be changed by Venetia.
	 */

	bp1 = allocb((int)sizeof (struct iocblk), NULL);
	ASSERT(bp1 != NULL);

	*((struct iocblk *)bp1->b_datap->db_base) = mctlmsg;
	bp1->b_datap->db_type = M_CTL;


	if (buf) {
		bp2 = allocb(len, NULL);
		ASSERT(bp2 != NULL);

		bp1->b_cont = bp2;
		bcopy(buf, bp2->b_datap->db_base, len);
	}

	/*
	 *  we don't need to test for canputnext so long as we don't have
	 *  a local service procedure
	 */
	(void) putnext(usbmsd->usbms_wq_ptr, bp1);
}


/*
 * usbms_input() :
 *
 *	Mouse input routine; process a byte received from a mouse and
 *	assemble into a mouseinfo message for the window system.
 *
 *	The USB mouse send a three-byte packet organized as
 *		button, dx, dy
 *	where dx and dy can be any signed byte value. The mouseinfo message
 *	is organized as
 *		dx, dy, button, timestamp
 *	Our strategy is to collect but, dx & dy three-byte packet, then
 *	send the mouseinfo message up.
 *
 *	Basic algorithm: throw away bytes until we get a [potential]
 *	button byte. Collect button; Collect dx; Collect dy; Send button,
 *	dx, dy, timestamp.
 *
 *	Watch out for overflow!
 */
static void
usbms_input(usbms_state_t		*usbmsp,
		char			c)
{
	register struct ms_softc	*ms;
	register struct mousebuf	*b;
	register struct mouseinfo	*mi;
	register int			jitter_radius;
	register int			temp;


	ms = &usbmsp->usbms_softc;
	b = ms->ms_buf;

	USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
		"usbms_input entering");

	if (b == NULL) {

		return;
	}

	mi = &b->mb_info[b->mb_off];

	switch (usbmsp->usbms_state) {
	case USBMS_WAIT_BUTN:
		/*
		 * Probably a button byte.
		 * Lower 3 bits are middle, right, left.
		 */
		mi->mi_buttons = USB_NO_BUT_PRESSED;
		if (c & USBMS_BUT1) {	 /* left button is pressed */
			mi->mi_buttons = mi->mi_buttons & USB_LEFT_BUT_PRESSED;
			USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR,
				usbms_log_handle,
				"left button pressed");
		}
		if (c & USBMS_BUT2) {	/* right button is pressed */
			mi->mi_buttons = mi->mi_buttons & USB_RIGHT_BUT_PRESSED;
			USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR,
				usbms_log_handle,
				"right button pressed");
		}
		if (c & USBMS_BUT3) {   /* middle button is pressed */
			mi->mi_buttons = mi->mi_buttons &
						USB_MIDDLE_BUT_PRESSED;
			USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR,
				usbms_log_handle,
				"middle button pressed");
		}
		break;

	case USBMS_WAIT_X:
		/*
		 * Delta X byte.  Add the delta X from this sample to
		 * the delta X we're accumulating in the current event.
		 */
		temp = (int)(mi->mi_x + c);
		mi->mi_x = USB_BYTECLIP(temp);
		USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR,
			usbms_log_handle, "x = %d", (int)mi->mi_x);

		uniqtime32(&mi->mi_time); /* record time when sample arrived */
		break;

	case USBMS_WAIT_Y:
		/*
		 * Delta Y byte.  Add the delta Y from this sample to
		 * the delta Y we're accumulating in the current event.
		 * (Add, because the mouse reports increasing Y down
		 * the screen.)
		 */
		temp = (int)(mi->mi_y + c);
		mi->mi_y = USB_BYTECLIP(temp);
		USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
			"y = %d", (int)mi->mi_y);
		break;
	}

	/*
	 * Done yet?
	 */
	if (usbmsp->usbms_state == USBMS_WAIT_Y) {
		usbmsp->usbms_state = USBMS_WAIT_BUTN;	/* Start again. */
	} else {
		usbmsp->usbms_state += 1;

		return;
	}

	if (usbmsp->usbms_jitter) {
		(void) quntimeout(usbmsp->usbms_rq_ptr,
				(timeout_id_t)usbmsp->usbms_timeout_id);
		usbmsp->usbms_jitter = 0;
	}

	if (mi->mi_buttons == usbmsp->usbms_oldbutt) {
		/*
		 * Buttons did not change; did position?
		 */
		if (mi->mi_x == 0 && mi->mi_y == 0) {
			/* no, position did not change */
			return;
		}

		/*
		 * Did the mouse move more than the jitter threshhold?
		 */
		jitter_radius = usbmsp->usbms_jitter_thresh;
		if (USB_ABS((int)mi->mi_x) <= jitter_radius &&
			USB_ABS((int)mi->mi_y) <= jitter_radius) {
			/*
			 * Mouse moved less than the jitter threshhold.
			 * Don't indicate an event; keep accumulating motions.
			 * After "jittertimeout" ticks expire, treat
			 * the accumulated delta as the real delta.
			 */
			usbmsp->usbms_jitter = 1;
			usbmsp->usbms_timeout_id =
			qtimeout(usbmsp->usbms_rq_ptr, (void (*)())usbms_incr,
			(void *)usbmsp, (clock_t)usbmsp->usbms_jittertimeout);

			return;
		}
	}
	usbmsp->usbms_oldbutt = mi->mi_buttons;
	usbms_incr(usbmsp);

	USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
			"usbms_input exiting");
}


/*
 * usbms_incr() :
 *	Increment the mouse sample pointer.
 *	Called either immediately after a sample or after a jitter timeout.
 */
static void
usbms_incr(void				*arg)
{
	usbms_state_t			*usbmsp = arg;
	register struct ms_softc	*ms = &usbmsp->usbms_softc;
	register struct mousebuf	*b;
	register struct mouseinfo	*mi;
	char				oldbutt;
	register short			xc, yc;
	register int			wake;
	register int			speedl = usbmsp->usbms_speedlimit;
	register int			xabs, yabs;

	/*
	 * No longer waiting for jitter timeout
	 */
	usbmsp->usbms_jitter = 0;

	b = ms->ms_buf;

	USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
		"usbms_incr entering");

	if (b == NULL) {

		return;
	}
	mi = &b->mb_info[b->mb_off];

	if (usbmsp->usbms_speedlaw) {
		xabs = USB_ABS((int)mi->mi_x);
		yabs = USB_ABS((int)mi->mi_y);
		if (xabs > speedl || yabs > speedl) {
			usbmsp->usbms_speed_count++;
		}
		if (xabs > speedl) {
			mi->mi_x = 0;
		}
		if (yabs > speedl) {
			mi->mi_y = 0;
		}
	}

	oldbutt = mi->mi_buttons;

	xc = yc = 0;

	/* See if we need to wake up anyone waiting for input */
	wake = b->mb_off == ms->ms_oldoff;

	/* Adjust circular buffer pointer */
	if (++b->mb_off >= b->mb_size) {
		b->mb_off = 0;
		mi = b->mb_info;
	} else {
		mi++;
	}

	/*
	 * If over-took read index then flush buffer so that mouse state
	 * is consistent.
	 */
	if (b->mb_off == ms->ms_oldoff) {
		if (overrun_msg) {
			USB_DPRINTF_L1(PRINT_MASK_ALL, usbms_log_handle,
				"Mouse buffer flushed when overrun.");
		}
		usbms_flush(usbmsp);
		overrun_cnt++;
		mi = b->mb_info;
	}

	/* Remember current buttons and fractional part of x & y */
	mi->mi_buttons = oldbutt;
	mi->mi_x = (char)xc;
	mi->mi_y = (char)yc;

	if (wake) {
		USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
			"usbms_incr run service");
		qenable(usbmsp->usbms_rq_ptr);	/* run the service proc */
	}
	USB_DPRINTF_L3(PRINT_MASK_INPUT_INCR, usbms_log_handle,
		"usbms_incr exiting");
}
