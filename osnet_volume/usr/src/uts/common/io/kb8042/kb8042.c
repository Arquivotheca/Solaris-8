/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kb8042.c	1.50	99/05/20 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/inline.h>
#include <sys/termio.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strtty.h>
#include <sys/strsubr.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/note.h>
#include "sys/consdev.h"
#include <sys/kbd.h>
#include <sys/kbtrans.h>
#include "kb8042.h"

#include <sys/i8042.h>

#include "sys/kbio.h"		/* for KIOCSLAYOUT */
#include "sys/stat.h"
#include "sys/reboot.h"
#include <sys/promif.h>
#include <sys/beep.h>

/*
 * "polled support" is support for kernel and kadb getchar(), used on
 * x86 and PowerPC because there's no PROM to help.  (On PowerPC, we've
 * discarded the PROM by the end of startup.)
 */
#define	INCLUDE_POLLED_SUPPORT

/*
 * DEBUG (or KD_DEBUG for just this module) turns on a flag called
 * kb8042_enable_debug_hotkey.  If kb8042_enable_debug_hotkey is true,
 * then the following hotkeys are enabled:
 *    F10 - turn on debugging "normal" translations
 *    F9  - turn on debugging "getchar" translations
 *    F8  - turn on "low level" debugging
 *    F7  - turn on terse press/release debugging
 *    F1  - turn off all debugging
 * The default value for kb8042_enable_debug_hotkey is false, disabling
 * these hotkeys.
 */

#if	defined(DEBUG) || defined(lint)
#define	KD_DEBUG
#endif

#ifdef	KD_DEBUG
boolean_t	kb8042_enable_debug_hotkey = B_FALSE;
boolean_t	kb8042_debug = B_FALSE;
boolean_t	kb8042_getchar_debug = B_FALSE;
boolean_t	kb8042_low_level_debug = B_FALSE;
boolean_t	kb8042_pressrelease_debug = B_FALSE;
static void kb8042_debug_hotkey(int atscan);
#endif

enum state_return { STATE_NORMAL, STATE_INTERNAL };

static void kb8042_init(struct kb8042 *kb8042);
static uint_t kb8042_intr(caddr_t arg);
static void kb8042_wait_poweron(struct kb8042 *kb8042);
static void kb8042_iocnack(queue_t *qp, mblk_t *mp, struct iocblk *iocp,
			int error);
static void kb8042_iocack(queue_t *qp, mblk_t *mp, struct iocblk *iocp);
static void kb8042_start_state_machine(struct kb8042 *, boolean_t);
static enum state_return kb8042_state_machine(struct kb8042 *, int, boolean_t);
static void kb8042_send_to_keyboard(struct kb8042 *, int, boolean_t);
static int kb8042_xlate_leds(int);
static void kb8042_streams_setled(struct kbtrans_hardware *hw, int led_state);
static void kb8042_polled_setled(struct kbtrans_hardware *hw, int led_state);
static boolean_t kb8042_polled_is_scancode_available(
			struct kbtrans_hardware *hw, int *key,
			enum keystate *state);
static void kb8042_get_initial_leds(struct kb8042 *, int *, int *);
static boolean_t kb8042_autorepeat_detect(struct kb8042 *kb8042, int key_pos,
			enum keystate state);
static void kb8042_type4_cmd(struct kb8042 *kb8042, int cmd);
static void kb8042_ioctlmsg(struct kb8042 *kb8042, queue_t *, mblk_t *);
static void kb8042_iocdatamsg(queue_t *, mblk_t *);

static struct kbtrans_hw_callbacks kb8042_callbacks = {
	kb8042_streams_setled,
	kb8042_polled_setled,
	kb8042_polled_is_scancode_available,
};

extern struct keyboard keyindex_pc;

#define	DRIVER_NAME(dip) \
	ddi_major_to_name(ddi_name_to_major(ddi_get_name(dip)))

static	char	module_name[] = "kb8042";

static int kb8042_open(queue_t *qp, dev_t *devp, int flag, int sflag,
			cred_t *credp);
static int kb8042_close(queue_t *qp, int flag, cred_t *credp);
static int kb8042_wsrv();

struct module_info kb8042_sinfo = {
	42,		/* Module ID */
	module_name,
	0, 32,		/* Minimum and maximum packet sizes */
	256, 128	/* High and low water marks */
};

static struct qinit kb8042_rinit = {
	NULL, NULL, kb8042_open, kb8042_close, NULL, &kb8042_sinfo, NULL
};

static struct qinit kb8042_winit = {
	putq, kb8042_wsrv, kb8042_open, kb8042_close, NULL, &kb8042_sinfo, NULL
};

struct streamtab
	kb8042_str_info = { &kb8042_rinit, &kb8042_winit, NULL, NULL };

struct kb8042	Kdws = {0};
static dev_info_t *kb8042_dip;

static int kb8042_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int kb8042_attach(dev_info_t *, ddi_attach_cmd_t);

static 	struct cb_ops cb_kb8042_ops = {
	nulldev,		/* cb_open */
	nulldev,		/* cb_close */
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
	&kb8042_str_info,	/* cb_stream */
	D_MP | D_NEW
};

struct dev_ops kb8042_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	kb8042_getinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	kb8042_attach,		/* devo_attach */
	nodev,			/* devo_detach */
	nodev,			/* devo_reset */
	&cb_kb8042_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL	/* devo_bus_ops */
};


/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"PS/2 Keyboard 1.50, 99/05/20",
	&kb8042_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *) &modldrv,
	NULL
};

_init()
{
	int	rv;

	rv = mod_install(&modlinkage);
	return (rv);
}


int
_fini()
{
	return (mod_remove(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
kb8042_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int	rc;

	struct kb8042	*kb8042;
	static ddi_device_acc_attr_t attr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	kb8042 = &Kdws;

	atKeyboardConvertScan_init(kb8042);

	kb8042->debugger.mod1 = 58;	/* Left Ctrl */
	kb8042->debugger.mod2 = 60;	/* Left Alt */
	kb8042->debugger.trigger = 33;	/* D */
	kb8042->debugger.mod1_down = B_FALSE;
	kb8042->debugger.mod2_down = B_FALSE;
	kb8042->debugger.enabled = B_FALSE;

	kb8042->polled_synthetic_release_pending = B_FALSE;

	if (ddi_create_minor_node(devi, module_name, S_IFCHR, 0,
		    DDI_NT_KEYBOARD, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	kb8042_dip = devi;

	rc = ddi_regs_map_setup(devi, 0, (caddr_t *)&kb8042->addr,
		(offset_t)0, (offset_t)0, &attr, &kb8042->handle);
	if (rc != DDI_SUCCESS) {
#if	defined(KD_DEBUG)
		cmn_err(CE_WARN, "kb8042_attach:  can't map registers");
#endif
		return (rc);
	}

	if (ddi_get_iblock_cookie(devi, 0, &kb8042->w_iblock) !=
		DDI_SUCCESS) {
		cmn_err(CE_WARN, "kb8042_attach:  Can't get iblock cookie");
		return (DDI_FAILURE);
	}

	mutex_init(&kb8042->w_hw_mutex, NULL, MUTEX_DRIVER, kb8042->w_iblock);

	kb8042_init(kb8042);

	/*
	 * Turn on interrupts...
	 */
	if (ddi_add_intr(devi, 0,
		&kb8042->w_iblock, (ddi_idevice_cookie_t *)NULL,
		kb8042_intr, (caddr_t)kb8042) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "kb8042_attach: cannot add interrupt");
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
#ifdef	KD_DEBUG
	cmn_err(CE_CONT, "?%s #%d: version %s, compiled on %s, %s\n",
			DRIVER_NAME(devi), ddi_get_instance(devi),
			"1.50 (99/05/20)", __DATE__, __TIME__);
#endif
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
kb8042_getinfo(
    dev_info_t *dip,
    ddi_info_cmd_t infocmd,
    void *arg,
    void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (kb8042_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) kb8042_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
		break;
	}
	return (error);
}

static void
kb8042_init(struct kb8042 *kb8042)
{
	if (kb8042->w_init)
		return;

	kb8042->w_kblayout = 0;	/* Default to US */

	kb8042->w_qp = (queue_t *)NULL;

	kb8042_wait_poweron(kb8042);

	kb8042->kb_old_scan = 0;

	/* Set up the command state machine and start it running. */
	kb8042->leds.commanded = -1;	/* Unknown initial state */
	kb8042->leds.desired = -1;	/* Unknown initial state */
	kb8042->command_state = KB_COMMAND_STATE_WAIT;
	kb8042_send_to_keyboard(kb8042, KB_ENABLE, B_FALSE);

	kb8042->w_init++;

	(void) drv_setparm(SYSRINT, 1);	/* reset keyboard interrupts */
}

/*
 *
 */

/*ARGSUSED2*/
static int
kb8042_open(queue_t *qp, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	struct kb8042	*kb8042;
	int err;
	int initial_leds;
	int initial_led_mask;

	kb8042 = &Kdws;

	kb8042->w_dev = *devp;

	qp->q_ptr = (caddr_t)kb8042;
	WR(qp)->q_ptr = qp->q_ptr;
	if (!kb8042->w_qp)
		kb8042->w_qp = qp;

	kb8042_get_initial_leds(kb8042, &initial_leds, &initial_led_mask);
	err = kbtrans_streams_init(qp, sflag, credp,
		(struct kbtrans_hardware *)kb8042, &kb8042_callbacks,
		&kb8042->hw_kbtrans,
		initial_leds, initial_led_mask);
	if (err != 0)
		return (err);

	kbtrans_streams_set_keyboard(kb8042->hw_kbtrans, KB_PC, &keyindex_pc);

	kb8042->polledio.cons_polledio_version = CONSPOLLEDIO_V0;
	kb8042->polledio.cons_polledio_argument =
		(struct cons_polledio_arg *)kb8042->hw_kbtrans;
	kb8042->polledio.cons_polledio_putchar = NULL;
	kb8042->polledio.cons_polledio_getchar =
		(int (*)(struct cons_polledio_arg *))kbtrans_getchar;
	kb8042->polledio.cons_polledio_ischar =
		(boolean_t (*)(struct cons_polledio_arg *))kbtrans_ischar;
	kb8042->polledio.cons_polledio_enter = NULL;
	kb8042->polledio.cons_polledio_exit = NULL;

	qprocson(qp);
	return (0);
}

/*
 * Close
 */

/*ARGSUSED1*/
static int
kb8042_close(queue_t *qp, int flag, cred_t *credp)
{
	struct kb8042	*kb8042;

	kb8042 = (struct kb8042 *)qp->q_ptr;

	kb8042->w_qp = (queue_t *)NULL;
	qprocsoff(qp);

	return (0);
}

/*
 *
 */

static int
kb8042_wsrv(queue_t *qp)
{
	struct kb8042 *kb8042;

	mblk_t	*mp;

	kb8042 = (struct kb8042 *)qp->q_ptr;

	while ((mp = getq(qp))) {
		switch (kbtrans_streams_message(kb8042->hw_kbtrans, mp)) {
		case KBTRANS_MESSAGE_HANDLED:
			continue;
		case KBTRANS_MESSAGE_NOT_HANDLED:
			break;
		}
		switch (mp->b_datap->db_type) {
		case M_IOCTL:
			kb8042_ioctlmsg(kb8042, qp, mp);
			continue;
		case M_IOCDATA:
			kb8042_iocdatamsg(qp, mp);
			continue;
		case M_DELAY:
		case M_STARTI:
		case M_STOPI:
		case M_READ:	/* ignore, no buffered data */
			freemsg(mp);
			continue;
		case M_FLUSH:
			*mp->b_rptr &= ~FLUSHW;
			if (*mp->b_rptr & FLUSHR)
				qreply(qp, mp);
			else
				freemsg(mp);
			continue;
		default:
			cmn_err(CE_NOTE, "kb8042_wsrv: bad msg %x",
						mp->b_datap->db_type);
			freemsg(mp);
			continue;
		}
	}
	return (0);
}

/*
 *
 */
static void
kb8042_streams_setled(struct kbtrans_hardware *hw, int led_state)
{
	struct kb8042 *kb8042 = (struct kb8042 *)hw;

	kb8042->leds.desired = led_state;
	mutex_enter(&kb8042->w_hw_mutex);

	kb8042_start_state_machine(kb8042, B_FALSE);

	mutex_exit(&kb8042->w_hw_mutex);
}

static void
kb8042_ioctlmsg(struct kb8042 *kb8042, queue_t *qp, mblk_t *mp)
{
	caddr_t  data = NULL;
	struct iocblk	*iocp;
	mblk_t *datap;

	iocp = (struct iocblk *)mp->b_rptr;
	if (mp->b_cont != NULL)
		data = (caddr_t)mp->b_cont->b_rptr;

	switch (iocp->ioc_cmd) {

	case CONSOPENPOLLEDIO:
		if (iocp->ioc_count != sizeof (struct cons_polledio *)) {
			kb8042_iocnack(qp, mp, iocp, EINVAL);
			break;
		}
		/*
		 * We are given an appropriate-sized data block,
		 * and return a pointer to our structure in it.
		 */
		*(struct cons_polledio **)mp->b_cont->b_rptr =
			&kb8042->polledio;
		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = 0;
		qreply(qp, mp);
		break;

	case CONSCLOSEPOLLEDIO:
		kb8042_iocack(qp, mp, iocp);
		break;

	case CONSSETABORTENABLE:
		if (data != NULL) {
		    kb8042->debugger.enabled = *(boolean_t *)data;
		    kb8042_iocack(qp, mp, iocp);
		} else {
		    kb8042_iocnack(qp, mp, iocp, EINVAL);
		}
		break;

	case KIOCLAYOUT:
		if (kb8042->w_kblayout == -1) {
			kb8042_iocnack(qp, mp, iocp, EINVAL);
			break;
		}

		if ((datap = allocb(sizeof (int), BPRI_HI)) == NULL) {
			kb8042_iocnack(qp, mp, iocp, ENOMEM);
			break;
		}

		*(int *)datap->b_wptr = kb8042->w_kblayout;
		datap->b_wptr += sizeof (int);
		if (mp->b_cont)
			freemsg(mp->b_cont);
		mp->b_cont = datap;
		iocp->ioc_count = sizeof (int);
		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_error = 0;
		qreply(qp, mp);
		break;

	case KIOCSLAYOUT:
		if (data != NULL) {
			kb8042->w_kblayout = *(int *)data;
			kb8042_iocack(qp, mp, iocp);
		} else
			kb8042_iocnack(qp, mp, iocp, EINVAL);
		break;

	case KIOCCMD:
		if (data != NULL) {
			kb8042_type4_cmd(kb8042, *(int *)data);
			kb8042_iocack(qp, mp, iocp);
		} else
			kb8042_iocnack(qp, mp, iocp, EINVAL);
		break;

	default:
#ifdef DEBUG1
		cmn_err(CE_NOTE, "!kb8042_ioctlmsg %x", iocp->ioc_cmd);
#endif
		kb8042_iocnack(qp, mp, iocp, EINVAL);
		break;
	}
}

/*
 * Process a byte received from the keyboard
 */
static void
kb8042_received_byte(
	struct kb8042	*kb8042,
	int		atscan)	/* raw scan code */
{
	boolean_t	legit;		/* is this a legit key pos'n? */
	int		key_pos = -1;
	enum keystate	state;
	boolean_t	synthetic_release_needed;

#ifdef	KD_DEBUG
	kb8042_debug_hotkey(atscan);
#endif

	if (!kb8042->w_init)	/* can't do anything anyway */
		return;

	legit = atKeyboardConvertScan(kb8042, atscan, &key_pos, &state,
		&synthetic_release_needed);

	if (legit == 0) {
		/* Eaten by translation */
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf("kb8042_intr:  0x%x -> ignored\n", atscan);
#endif
		return;
	}

#ifdef	KD_DEBUG
	if (kb8042_debug) {
		prom_printf("kb8042_intr:  0x%x -> %s %d",
			atscan,
			state == KEY_RELEASED ? "released" : "pressed",
			key_pos);
	}
#endif

	/*
	 * Don't know if we want this permanently, but it seems interesting
	 * for the moment.
	 */
	if (key_pos == kb8042->debugger.mod1) {
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf(" -> debug mod1");
#endif
		kb8042->debugger.mod1_down = (state == KEY_PRESSED);
	}
	if (key_pos == kb8042->debugger.mod2) {
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf(" -> debug mod2");
#endif
		kb8042->debugger.mod2_down = (state == KEY_PRESSED);
	}
	if (kb8042->debugger.enabled &&
	    key_pos == kb8042->debugger.trigger &&
	    kb8042->debugger.mod1_down &&
	    kb8042->debugger.mod2_down) {
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf(" -> debugger\n");
#endif
		/*
		 * Require new presses of the modifiers.
		 */
		kb8042->debugger.mod1_down = B_FALSE;
		kb8042->debugger.mod2_down = B_FALSE;
		abort_sequence_enter(NULL);
		return;
	}

	/*
	 * If there's no queue above us - as can happen if we've been
	 * attached but not opened - drop the keystroke.
	 * Note that we do this here instead of above so that
	 * Ctrl-Alt-D still works.
	 */
	if (kb8042->w_qp == NULL) {
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf(" -> nobody home\n");
#endif
		return;
	}

	/*
	 * This is to filter out auto repeat since it can't be
	 * turned off at the hardware.  (Yeah, yeah, PS/2 keyboards
	 * can.  Don't know whether they've taken over the world.
	 * Don't think so.)
	 */
	if (kb8042_autorepeat_detect(kb8042, key_pos, state)) {
#ifdef	KD_DEBUG
		if (kb8042_debug)
			prom_printf(" -> autorepeat ignored\n");
#endif
		return;
	}

#ifdef	KD_DEBUG
	if (kb8042_debug)
		prom_printf(" -> OK\n");
#endif

#if	defined(KD_DEBUG)
	if (kb8042_pressrelease_debug) {
		prom_printf(" %s%d ",
			state == KEY_PRESSED ? "+" : "-",
			key_pos);
	}
#endif

	kbtrans_streams_key(kb8042->hw_kbtrans, key_pos, state);

	/*
	 * This is a total hack.  For some stupid reason, the two additional
	 * keys on Korean keyboards (Hangul and Hangul/Hanja) report press
	 * only.  We synthesize a release immediately.
	 */
	if (synthetic_release_needed) {
#if	defined(KD_DEBUG)
		if (kb8042_debug)
			prom_printf("synthetic release %d\n", key_pos);
		if (kb8042_pressrelease_debug)
			prom_printf(" -%d(s) ", key_pos);
#endif
		(void) kb8042_autorepeat_detect(kb8042, key_pos, KEY_RELEASED);
		kbtrans_streams_key(kb8042->hw_kbtrans, key_pos, KEY_RELEASED);
	}
}

/*
 * Called from interrupt handler when keyboard interrupt occurs.
 */

static uint_t
kb8042_intr(caddr_t arg)
{
	uchar_t atscan;	/* raw scan code */
	int rc;
	struct kb8042 *kb8042 = (struct kb8042 *)arg;

	rc = DDI_INTR_UNCLAIMED;

	/* don't care if drv_setparm succeeds */
	(void) drv_setparm(SYSRINT, 1);

	while (ddi_get8(kb8042->handle, kb8042->addr + I8042_INT_INPUT_AVAIL)
	    != 0) {
		rc = DDI_INTR_CLAIMED;

		atscan = ddi_get8(kb8042->handle,
			kb8042->addr + I8042_INT_INPUT_DATA);

#if	defined(KD_DEBUG)
		if (kb8042_low_level_debug)
			prom_printf(" <K:%x ", atscan);
#endif

		mutex_enter(&kb8042->w_hw_mutex);

		if (kb8042_state_machine(kb8042, atscan, B_FALSE) !=
		    STATE_NORMAL) {
			mutex_exit(&kb8042->w_hw_mutex);
			continue;
		}


		mutex_exit(&kb8042->w_hw_mutex);

		kb8042_received_byte(kb8042, atscan);
	}

	return (rc);
}

/*
 *
 */

static void
kb8042_iocdatamsg(queue_t *qp, mblk_t *mp)
{
	struct copyresp	*csp;

	csp = (struct copyresp *)mp->b_rptr;
	if (csp->cp_rval) {
		freemsg(mp);
		return;
	}

	switch (csp->cp_cmd) {
	default:
		kb8042_iocack(qp, mp, (struct iocblk *)mp->b_rptr);
		break;
	}
}

#if	defined(INCLUDE_POLLED_SUPPORT)
static boolean_t
kb8042_polled_is_scancode_available(
    struct kbtrans_hardware *hw,
    int *key,
    enum keystate *state)
{
	struct kb8042 *kb8042 = (struct kb8042 *)hw;
	int	atscan;
	boolean_t	legit;
	boolean_t	synthetic_release_needed;

	if (kb8042->polled_synthetic_release_pending) {
		*key = kb8042->polled_synthetic_release_key;
		*state = KEY_RELEASED;
		kb8042->polled_synthetic_release_pending = B_FALSE;
#if	defined(KD_DEBUG)
		if (kb8042_getchar_debug)
			prom_printf("synthetic release 0x%x\n", *key);
#endif
		(void) kb8042_autorepeat_detect(kb8042, *key, *state);
		return (B_TRUE);
	}

	for (;;) {
		if (ddi_get8(kb8042->handle,
		    kb8042->addr + I8042_POLL_INPUT_AVAIL) == 0) {
			return (B_FALSE);
		}

		atscan = ddi_get8(kb8042->handle,
				kb8042->addr + I8042_POLL_INPUT_DATA);

#if	defined(KD_DEBUG)
		if (kb8042_low_level_debug)
			prom_printf(" g<%x ", atscan);
#endif

		if (kb8042_state_machine(kb8042, atscan, B_FALSE) !=
		    STATE_NORMAL) {
			continue;
		}

#ifdef	KD_DEBUG
		kb8042_debug_hotkey(atscan);
		if (kb8042_getchar_debug)
			prom_printf("polled 0x%x", atscan);
#endif

		legit = atKeyboardConvertScan(kb8042, atscan, key, state,
			&synthetic_release_needed);
		if (!legit) {
#ifdef	KD_DEBUG
			if (kb8042_getchar_debug)
				prom_printf(" -> ignored\n");
#endif
			continue;
		}
#ifdef	KD_DEBUG
		if (kb8042_getchar_debug) {
			prom_printf(" -> %s %d\n",
				*state == KEY_PRESSED ? "pressed" : "released",
				*key);
		}
#endif
		/*
		 * For the moment at least, we rely on hardware autorepeat
		 * for polled I/O autorepeat.  However, for coordination
		 * with the interrupt-driven code, maintain the last key
		 * pressed.
		 */
		(void) kb8042_autorepeat_detect(kb8042, *key, *state);

		/*
		 * This is a total hack to support two additional keys
		 * on Korean keyboards.  They report only on press, and
		 * so we synthesize a release.  Most likely this will
		 * never be important to polled I/O, but if I do it
		 * "right" the first time it _won't_ be an issue.
		 */
		if (synthetic_release_needed) {
			kb8042->polled_synthetic_release_pending = B_TRUE;
			kb8042->polled_synthetic_release_key = *key;
		}

		return (B_TRUE);
	}
}

static void
kb8042_polled_setled(struct kbtrans_hardware *hw, int led_state)
{
	struct kb8042 *kb8042 = (struct kb8042 *)hw;

	kb8042->leds.desired = led_state;

	kb8042_start_state_machine(kb8042, B_TRUE);
}
#endif

static void
kb8042_iocack(queue_t *qp, mblk_t *mp, struct iocblk *iocp)
{
	mblk_t	*tmp;

	mp->b_datap->db_type = M_IOCACK;
	if ((tmp = unlinkb(mp)) != (mblk_t *)NULL)
		freeb(tmp);
	iocp->ioc_count = iocp->ioc_error = 0;
	qreply(qp, mp);
}

static void
kb8042_iocnack(queue_t *qp, mblk_t *mp, struct iocblk *iocp, int error)
{
	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_rval = -1;
	iocp->ioc_error = error;
	qreply(qp, mp);
}

static void
kb8042_send_to_keyboard(struct kb8042 *kb8042, int byte, boolean_t polled)
{
	if (polled) {
		ddi_put8(kb8042->handle,
		    kb8042->addr + I8042_POLL_OUTPUT_DATA, byte);
	} else {
		ddi_put8(kb8042->handle,
		    kb8042->addr + I8042_INT_OUTPUT_DATA, byte);
	}

#if	defined(KD_DEBUG)
	if (kb8042_low_level_debug)
		prom_printf(" >K:%x ", byte);
#endif
}

/*
 * Wait until the keyboard is fully up, maybe.
 * We may be the first person to talk to the keyboard, in which case
 * it's patiently waiting to say "AA" to us to tell us it's up.
 * In theory it sends the AA in 300ms < n < 9s, but it's a pretty
 * good bet that we've already spent that long getting to that point,
 * so we'll only wait long enough for the communications electronics to
 * run.
 */
static void
kb8042_wait_poweron(struct kb8042 *kb8042)
{
	int cnt;
	int ready;
	unsigned char byt;

	/* wait for up to about a quarter-second for response */
	for (cnt = 0; cnt < 250; cnt++) {
		ready = ddi_get8(kb8042->handle,
			kb8042->addr + I8042_INT_INPUT_AVAIL);
		if (ready != 0)
			break;
		drv_usecwait(1000);
	}

	/*
	 * If there's something pending, read and discard it.  If not,
	 * assume things are OK anyway - maybe somebody else ate it
	 * already.  (On a PC, the BIOS almost certainly did.)
	 */
	if (ready != 0) {
		byt = ddi_get8(kb8042->handle,
			kb8042->addr + I8042_INT_INPUT_DATA);
#if	defined(KD_DEBUG)
		if (kb8042_low_level_debug)
			prom_printf(" <K:%x ", byt);
#endif
	}
}

static void
kb8042_start_state_machine(struct kb8042 *kb8042, boolean_t polled)
{
	if (kb8042->command_state == KB_COMMAND_STATE_IDLE) {
		if (kb8042->leds.desired != kb8042->leds.commanded) {
			kb8042_send_to_keyboard(kb8042, KB_SET_LED, polled);
			kb8042->command_state = KB_COMMAND_STATE_LED;
		}
	}
}

enum state_return
kb8042_state_machine(struct kb8042 *kb8042, int atscan, boolean_t polled)
{
	switch (kb8042->command_state) {
	case KB_COMMAND_STATE_IDLE:
		break;

	case KB_COMMAND_STATE_LED:
		if (atscan == KB_ACK) {
			kb8042_send_to_keyboard(kb8042,
				kb8042_xlate_leds(kb8042->leds.desired),
				polled);
			kb8042->leds.commanded = kb8042->leds.desired;
			kb8042->command_state = KB_COMMAND_STATE_WAIT;
			return (STATE_INTERNAL);
		}
		/* Drop normal scan codes through. */
		break;

	case KB_COMMAND_STATE_WAIT:
		if (atscan == KB_ACK) {
			kb8042->command_state = KB_COMMAND_STATE_IDLE;
			kb8042_start_state_machine(kb8042, polled);
			return (STATE_INTERNAL);
		}
		/* Drop normal scan codes through. */
		break;
	}
	return (STATE_NORMAL);
}

static int
kb8042_xlate_leds(int led)
{
	int res;

	res = 0;

	if (led & LED_NUM_LOCK)
		res |= LED_NUM;
	if (led & LED_SCROLL_LOCK)
		res |= LED_SCR;
	if (led & LED_CAPS_LOCK)
		res |= LED_CAP;

	return (res);
}

/*ARGSUSED*/
static void
kb8042_get_initial_leds(
    struct kb8042 *kb8042,
    int *initial_leds,
    int *initial_led_mask)
{
#if	defined(i86pc)
	extern caddr_t	p0_va;
	uint8_t		bios_kb_flag;

	bios_kb_flag = p0_va[BIOS_KB_FLAG];

	*initial_led_mask = LED_CAPS_LOCK | LED_NUM_LOCK | LED_SCROLL_LOCK;
	*initial_leds = 0;
	if (bios_kb_flag & BIOS_CAPS_STATE)
		*initial_leds |= LED_CAPS_LOCK;
	if (bios_kb_flag & BIOS_NUM_STATE)
		*initial_leds |= LED_NUM_LOCK;
	if (bios_kb_flag & BIOS_SCROLL_STATE)
		*initial_leds |= LED_SCROLL_LOCK;
#else
	*initial_leds = 0;
	*initial_led_mask = 0;
#endif
}

#if	defined(KD_DEBUG)
static void
kb8042_debug_hotkey(int atscan)
{
	if (!kb8042_enable_debug_hotkey)
		return;

	switch (atscan) {
	case 0x09:	/* F10 */
		if (!kb8042_debug) {
			prom_printf("\nKeyboard:  normal debugging on\n");
			kb8042_debug = B_TRUE;
		}
		break;
	case 0x01:	/* F9 */
		if (!kb8042_getchar_debug) {
			prom_printf("\nKeyboard:  getchar debugging on\n");
			kb8042_getchar_debug = B_TRUE;
		}
		break;
	case 0x0a:	/* F8 */
		if (!kb8042_low_level_debug) {
			prom_printf("\nKeyboard:  low-level debugging on\n");
			kb8042_low_level_debug = B_TRUE;
		}
		break;
	case 0x83:	/* F7 */
		if (!kb8042_pressrelease_debug) {
			prom_printf(
			    "\nKeyboard:  press/release debugging on\n");
			kb8042_pressrelease_debug = B_TRUE;
		}
		break;
	case 0x05:	/* F1 */
		if (kb8042_debug ||
		    kb8042_getchar_debug ||
		    kb8042_low_level_debug ||
		    kb8042_pressrelease_debug) {
			prom_printf("\nKeyboard:  all debugging off\n");
			kb8042_debug = B_FALSE;
			kb8042_getchar_debug = B_FALSE;
			kb8042_low_level_debug = B_FALSE;
			kb8042_pressrelease_debug = B_FALSE;
		}
		break;
	}
}
#endif

static boolean_t
kb8042_autorepeat_detect(
    struct kb8042 *kb8042,
    int key_pos,
    enum keystate state)
{
	if (state == KEY_RELEASED) {
		if (kb8042->kb_old_scan == key_pos)
			kb8042->kb_old_scan = 0;
	} else {
		if (kb8042->kb_old_scan == key_pos) {
			return (B_TRUE);
		}
		kb8042->kb_old_scan = key_pos;
	}
	return (B_FALSE);
}

/* ARGSUSED */
static void
kb8042_type4_cmd(struct kb8042 *kb8042, int cmd)
{
	switch (cmd) {
	case KBD_CMD_BELL:
		beeper_on(BEEP_TYPE4);
		break;
	case KBD_CMD_NOBELL:
		beeper_off();
		break;
	}
}
