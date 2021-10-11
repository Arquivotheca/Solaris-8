/*
 * Copyright (c) 1991-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stc.c	1.106	99/10/04 SMI"

/*
 * Driver for 8-port asynchronous serial card with parallel printer port
 * The card has the following names:
 *		SPC/S - marketing name
 *		SPIF  - engineering name
 *		STC   - original name, that's why most everything
 *			in the code is called stc_XXX something
 *
 * SPC/S has a Cirrus Logic cd-180 8-channel UART to handle the serial
 * data traffic, and a Western Digital/Paradise PPC-2 to handle the
 * parallel port.  The PPC-2 is a brain-dead piece of junk.
 *
 * For a really good description of the layout of the card, see the
 * various header files that come with the driver.
 *
 * Note to future debuggers and code perusers: please be sure that you
 *	are looking at the correct version of the device driver sources.
 *	The version that you are looking at now was released with V1.1
 *	of the software/hardware.
 *
 * comments/things to do can be found with "??" or "XX" searches
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/kmem.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/termiox.h>
#include <sys/stermio.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/tty.h>
#include <sys/ptyvar.h>
#include <sys/cred.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/mkdev.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>

#include <sys/strtty.h>
#include <sys/suntty.h>		/* XXX - temporary */
#include <sys/ksynch.h>

#include <sys/consdev.h>
#include <sys/ser_async.h>
#include <sys/debug/debug.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/debug.h>

/*
 * stc-related header files
 */
#include <sys/stcio.h>
#include <sys/stcvar.h>
#include <sys/stcreg.h>
#include <sys/stcconf.h>

/*
 * external data of interest to us
 */
extern kcondvar_t lbolt_cv;

/*
 * local driver data
 *    nstc - number of boards found by stc_attach()
 *    stc_dtrdrop - number of microseconds to wait after dropping
 *	DTR on close before allowing it to come back up
 */
void *stc_soft_state_p = NULL;
static int nstc = 0;
static int stc_dtrdrop = 500000;
static int stc_logcount = -1;
static struct {
	unsigned char line;
	unsigned char where;
	unsigned short func;
	unsigned int what;
} stc_logger[256];

#define	STC_LOGIT(w, x, y, z) \
if (stc_logcount >= 0) { \
	stc_logger[stc_logcount].line = (unsigned char) (w); \
	stc_logger[stc_logcount].where = (unsigned char) (x); \
	stc_logger[stc_logcount].func = (unsigned short) (y); \
	stc_logger[stc_logcount].what = (unsigned int) (z); \
	stc_logcount = (stc_logcount + 1) | 0xff; \
}

/*
 * When STC_DEBUG is not defined, the code compacts in the loadable
 *	case, however the strings still remain in the object file.
 */
#ifdef	DEBUG_STC
static int stc_debug = STC_DEBUG;
#define	SDM(level, args) \
	if (stc_debug >= (level)) cmn_err args;
#else
#define	SDM(level, args)	/* Nothing */
#endif	DEBUG_STC

#ifdef	DEBUG_STC
static void ioctl2text(char *tag, int ioc_cmd);
static void print_bits(int bits);
#endif
/*
 * cd180 functions
 */
static int cd180_init(struct stc_unit_t *stc, int chan);
static int stc_open(queue_t *q, dev_t *dev, int oflag, int sflag,
    cred_t *credp);
static int stc_close(queue_t *q, int flag, cred_t *credp);
static int stc_wput(queue_t *q, mblk_t *mp);
static void stc_ioctl(queue_t *q, mblk_t *mp);
static void stc_reioctl(void *);
static void stc_restart(void *);
static void stc_start(struct stc_line_t *line);
static void stc_param(struct stc_line_t *line);
static int stc_xmit(struct stc_unit_t *stc);
static int stc_rcv(struct stc_unit_t *stc);
static int stc_modem(struct stc_unit_t *stc);
static void stc_drainsilo(struct stc_line_t *line);
static void stc_draintimeout(void *);
static void silocopy(struct stc_line_t *line, uchar_t *buf, int count);
static void stc_xwait(struct stc_line_t *line);
static void stc_timeout(void *);
static u_int stc_poll(caddr_t arg);
static u_int stc_softint(caddr_t arg);
static void stc_softint_line(struct stc_line_t *line);
static int stc_init(struct stc_unit_t *stc);
static void stc_setdefaults(struct stc_line_t *line,
    struct stc_defaults_t *stc_defaults);
static void stc_getdefaults(dev_info_t *dip, u_int tag,
    struct stc_defaults_t *stc_defaults);
static void ppc_getdefaults(dev_info_t *dip, u_int tag,
    struct stc_defaults_t *stc_defaults);
static int stc_extractval(char *strbuf, int *offset, char **token, int *value);
static void stc_txflush(struct stc_line_t *line);
static int stc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int stc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int stc_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **result);
static int stc_dodiagregs(struct stc_line_t *line,
    struct stc_diagregs_t *stc_diagregs, u_int iocmd);
static void stc_mcopy(mblk_t *mp, mblk_t *dp, uint size, unsigned char type,
    void *cmd, void *useraddr);
static void stc_ack(mblk_t *mp, mblk_t *dp, uint size);
static int stc_rcvex(struct stc_unit_t *stc);

#ifdef	STC_ALIVE
static void stc_alive(void *);
timeout_id_t stc_alive_id;
#endif	STC_ALIVE

/*
 * ppc functions
 */
static u_int ppc_poll(caddr_t arg);
static int ppc_int(struct stc_unit_t *stc);
static void ppc_out(void *);
static void ppc_acktimeout(void *);
static void ppc_xwait(struct stc_line_t *line);
static int ppc_stat(struct ppcregs_t *regs, struct stc_line_t *line);
static void ppc_signal(struct stc_line_t *line, int sig_type);

/*
 * misc STREAMS data structs
 */
static struct module_info stcm_info = {
	0,		/* module id number */
	"stc",		/* module name */
	0,		/* min packet size */
	INFPSZ,		/* max packet size, was INFPSZ */
	2048,		/* hi-water mark, was 2048 */
	128,		/* lo-water mark, was 128 */
};

static struct qinit stc_rinit = {
	putq,		/* put proc */
	NULL,		/* service proc */
	stc_open,
	stc_close,
	NULL,		/* admin - "for 3bnet only" (see stream.h) */
	&stcm_info,
	NULL,		/* statistics */
};

static struct qinit stc_winit = {
	stc_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&stcm_info,
	NULL,
};

/*
 * streamtab
 */
struct streamtab stc_stab = {	/* used in cdevsw */
	&stc_rinit,
	&stc_winit,
	NULL,
	NULL,
/*	stc_modlist, */
};

/*
 * stc driver cb_ops and dev_ops
 */

static struct cb_ops stc_cb_ops = {
	nodev,		/* open */
	nodev,		/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	nodev,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* cb_prop op */
	&stc_stab,	/* stream tab */
	D_NEW|D_MP	/* Driver Compatability Flags */
};

static struct dev_ops stc_dev_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	stc_getinfo,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	stc_attach,		/* attach */
	stc_detach,		/* detach */
	nodev,			/* reset */
	&stc_cb_ops,		/* Driver Ops */
	(struct bus_ops *)0	/* Bus Operations */
};

/*
 * Module linkage information for the kernel
 */

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,			/* Type of Module = Driver */
	STC_DRIVERID,			/* Driver Identifier string. */
	&stc_dev_ops,			/* Driver Ops. */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/*
 * embedded transmit sequence used to send a BREAK
 */
static uchar_t stc_break[EMBED_XMIT_LEN] =
	{ 0x00, 0x81, 0x00, 0x82, BRK_DELAY, 0x00, 0x83 };
/*
 *				  ^^^^^^^^^
 * (see P15 of cd180 data sheet) BREAK duration in cd180 ticks
 * depends on PPR{H, L} registers
 *
 * the next two are used to support the TIOCSBRK and TIOCCBRK ioctl()'s
 */
#define	SBREAK_LEN	2
#define	EBREAK_LEN	2
static uchar_t stc_sbreak[SBREAK_LEN] = { 0x00, 0x81 };
static uchar_t stc_ebreak[EBREAK_LEN] = { 0x00, 0x83 };

/*
 * supported baud rates (from sys/ttydev.h); CD-180 translation table is
 * on pp. 10 of the CD-180 spec; you can generate this table using the
 * following shell script and c code:
 * ---------------------------------------------------------------------------
 *	#!/bin/sh
 *
 *	for CLOCK in 10 9
 *	 do
 *	  echo "unsigned static short stc_baud_$CLOCK[16] = {"
 *	  echo "	0x00,	/ * B0 * /"
 *	  for BAUD in
 *		50 75 110 134 150 200 300 600 1200 1800 2400 4800
 *		9600 19200 38400
 *	   do
 *	    if [ "$CLOCK" = 10 ];  then
 *		FCLOCK = 10.0;
 *	    else
 *		FCLOCK = 9.8304;
 *	    fi
 *	    DIVISOR = `stc_baud $FCLOCK $BAUD`
 *		if [ "$BAUD" = 38400 ];	 then
 *			echo "	$DIVISOR	/ * B$BAUD * /"
 *		else
 *			echo "	$DIVISOR,	/ * B$BAUD * /"
 *		fi
 *	   done
 *	   echo "}; "
 *	  done
 * -----------------------------------------------------------------------
 *	#include <stdio.h>
 *	#include <stdlib.h>
 *
 *	main(argc, argv)
 *	 int argc;
 *	 char *argv[];
 *	{
 *
 *		if (argc != 3)
 *			exit(fprintf(stderr,
 *				"usage: stc_baud clock(MhZ) baud_rate\n"));
 *
 *		printf("0x%x\n", (unsigned int)
 *		(((atof(argv[1])*1000000.0)/16.0)/(double)atoi(argv[2])+0.5));
 *
 *		exit(0);
 *	}
 * -------------------------------------------------------------------------
 *
 * the number of baud rates is fixed at 16; this ties in with the HI_BAUD()
 * and LO_BAUD() macros in stcvar.h
 *
 * table for a 10MhZ baud-rate oscillator
 */
static unsigned short stc_baud_10[16] = {
	0x00,	/* B0 */
	0x30d4, /* B50 */
	0x208d, /* B75 */
	0x1632, /* B110 */
	0x1238, /* B134 */
	0x1047, /* B150 */
	0xc35,	/* B200 */
	0x823,	/* B300 */
	0x412,	/* B600 */
	0x209,	/* B1200 */
	0x15b,	/* B1800 */
	0x104,	/* B2400 */
	0x82,	/* B4800 */
	0x41,	/* B9600 */
	0x21,	/* B19200 */
	0x10	/* B38400 */
};
/*
 * table for a 9.XXXMhZ baud-rate oscillator
 */
static unsigned short stc_baud_9[16] = {
	0x00,	/* B0 */
	0x3000, /* B50 */
	0x2000, /* B75 */
	0x15d1, /* B110 */
	0x11e9, /* B134 */
	0x1000, /* B150 */
	0xc00,	/* B200 */
	0x800,	/* B300 */
	0x400,	/* B600 */
	0x200,	/* B1200 */
	0x155,	/* B1800 */
	0x100,	/* B2400 */
	0x80,	/* B4800 */
	0x40,	/* B9600 */
	0x20,	/* B19200 */
	0x10	/* B38400 */
};

/* ********************************************************************** */

/*
 * Module Initialization functions.
 */

int
_init(void)
{
	int stat;

	SDM(STC_TRACK, (CE_CONT, "_init: entering ...\n"));

	/* Allocate soft state */
	if ((stat = ddi_soft_state_init(&stc_soft_state_p,
		sizeof (struct stc_unit_t), N_STC)) != DDI_SUCCESS)
	    return (stat);

	if ((stat = mod_install(&modlinkage)) != DDI_SUCCESS)
	    ddi_soft_state_fini(&stc_soft_state_p);

	SDM(STC_TRACK, (CE_CONT, "_init: exiting ... stat = %d\n", stat));
	return (stat);
}

int
_info(struct modinfo *infop)
{
	SDM(STC_TRACK, (CE_CONT, "_info: entering and exiting ...\n"));

	return (mod_info(&modlinkage, infop));
}

int
_fini()
{
	int stat = 0;

	SDM(STC_TRACK, (CE_CONT, "_fini: entering ...\n"));

	if ((stat = mod_remove(&modlinkage)) != DDI_SUCCESS)
	    return (stat);

	ddi_soft_state_fini(&stc_soft_state_p);

	SDM(STC_TRACK, (CE_CONT, "_fini: returning %d\n", stat));
	return (stat);
}

/*
 * stc_attach() - performs board initialization
 *
 * This routine initializes the stc driver and the board.
 *
 * Increments "nstc" with each board we find.
 *
 *	Returns:	DDI_SUCCESS, if able to attach.
 *			DDI_FAILURE, if unable to attach.
 */
static int
stc_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	struct stc_unit_t *stc;
	struct stc_line_t *line;
	int instance, stc_rev, osc_rev, error;
	int line_no;

	SDM(STC_DEBUG, (CE_CONT, "stc_attach: cmd = 0x%x\n", cmd));
	SDM(STC_DEBUG, (CE_CONT, "stc_attach: dip = 0x%x\n", dip));

	/*
	 * make sure we're only being asked to do an attach
	 */
	if (cmd != DDI_ATTACH) {
		cmn_err(CE_CONT, "stc_attach: cmd != DDI_ATTACH\n");
		return (DDI_FAILURE);
		/* NOTREACHED */
	}

	instance = ddi_get_instance(dip);
	nstc++;

	SDM(STC_DEBUG, (CE_CONT, "stc_attach: instance = %d\n", instance));

	/* Allocate soft state associated with this instance. */
	if (ddi_soft_state_zalloc(stc_soft_state_p, instance) != DDI_SUCCESS) {
	    cmn_err(CE_CONT, "stc_attach: Unable to alloc state\n");
	    return (DDI_FAILURE);
	    /* NOTREACHED */
	}

	stc = ddi_get_soft_state(stc_soft_state_p, instance);
	stc->dip = dip;
	ddi_set_driver_private(dip, (caddr_t)stc);

/* ************* LOOK do we really need all of these checks?? ************* */

	/*
	 * check the be sure that we have the proper number of register sets
	 *	and the proper number of interrupts
	 */
	if (ddi_dev_nregs(dip, &stc_rev) != DDI_SUCCESS) {
	    cmn_err(CE_CONT,
		"stc%d: can't determine number of register sets\n", instance);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	} else {
	    if (stc_rev != NUM_REGS) {
		cmn_err(CE_CONT,
			"stc%d: invalid number of register sets: 0x%x\n",
			instance, stc_rev);
		(void) stc_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
	    }
	}

	if (ddi_dev_nintrs(dip, &stc_rev) != DDI_SUCCESS) {
	    cmn_err(CE_CONT, "stc%d: can't determine number of interrupts\n",
		instance);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	} else {
	    if (stc_rev != NUM_INTS) {
		cmn_err(CE_CONT, "stc%d: invalid number of interrupts: 0x%x\n",
			instance, stc_rev);
		(void) stc_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
	    }
	}

	/*
	 * clear the per-unit flags field and assign a default baud rate
	 *	table
	 */
	stc->flags = 0;
	stc->stc_baud = stc_baud_9;

	/*
	 * get the revision level of the board; we only support boards
	 *	with V1.1 FCode PROMS
	 */
	stc_rev = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		STC_REV_PROP, STC_FCS2_REV);
	if (stc_rev < STC_FCS2_REV) {
	    cmn_err(CE_CONT,
		"stc%d: board revision: 0x%x not supported by driver\n",
		instance, stc_rev);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	} else {
		/*EMPTY*/ /* if DEBUG_STC not set */
	    SDM(STC_DEBUG, (CE_CONT,
		"stc%d: board revision: 0x%x\n", instance, stc_rev));
	}

	/*
	 * get the oscillator revision of the board and setup the pointer
	 *	to the correct baud rate table
	 */
	osc_rev = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
		OSC_REV_PROP, OSC_9);
	/*
	 * use the oscillator revision to point to the correct baud rate table
	 */
	switch (osc_rev) {
	    default:
		cmn_err(CE_CONT,
		"stc%d: wierd oscillator revision (0x%x), assuming 10Mhz\n",
			instance, osc_rev);
	    case OSC_10:		/* 10 Mhz oscillator */
		stc->stc_baud = stc_baud_10;
		break;
	    case OSC_9:			/* 9...... Mhz oscillator */
		stc->stc_baud = stc_baud_9;
		break;
	} /* switch */

	/*
	 * map in the device registers - there should only be one set
	 * use 1 mapping for the base of the board (called *prom) and
	 *	add offsets into the page for the various device's base
	 *	addresses
	 */
	if (ddi_map_regs(dip, 0, (caddr_t *)&stc->prom, (off_t)0,
		(off_t)0) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "stc_attach: Unable to map registers\n");
		(void) stc_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
		/* NOTREACHED */
	} else {
		for (line_no = 0; line_no < STC_LINES; line_no++) {
			stc->line[line_no].dtr =
			(uchar_t *)(stc->prom + DTR_OFFSET + line_no);
		}
		stc->regs =
			(struct stcregs_t *)(stc->prom + CD180_OFFSET);
		stc->iack =
			(struct stciack_t *)(stc->prom + IACK_OFFSET);
		stc->ppc =
			(struct ppcregs_t *)(stc->prom + PPC_OFFSET);
		/*
		 * set a flag to indicate to stc_detach() that we've got a valid
		 * mapping to the board's registers
		 */
		stc->flags = STC_REGOK;
	}

	/*
	 * initialize the CD-180 and PPC registers on each board; poke around
	 * the board a bit to see if we can talk to the card; if not, return
	 * an error and quit stc_init() does not cause the board to generate
	 * interrupts, so we don't have to be on the interrupt chain yet
	 */
	if ((stc_init(stc)) != 0) {
	    cmn_err(CE_CONT, "stc%d: error initializing board\n", instance);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	}

	/*
	 * install us on the interrupt chain - we have two interrupts:
	 *		- 1 for the cd180
	 *		- 1 for the ppc
	 * save a copy of the cookies for the cd180 and the ppc in the
	 *	unit struct
	 * we can only handle V1.1 FCode PROMS where both interrupts
	 *	are specified in the FCode PROM on the board
	 */
	error = ddi_add_intr(dip, (u_int)STC_INT_NUM, &stc->stc_blk_cookie,
		(ddi_idevice_cookie_t *)0, stc_poll, (caddr_t)stc);
	if (error != DDI_SUCCESS) {
	    cmn_err(CE_CONT,
		"stc_attach: unable to install cd-180 interrupt\n");
	    (void) stc_detach(dip, DDI_DETACH);
	    return (error);
	    /* NOTREACHED */
	} else {
	    stc->flags |= STC_CD180INTROK;
	}

	error = ddi_add_intr(dip, (u_int)PPC_INT_NUM, &stc->ppc_blk_cookie,
		&stc->ppc_dev_cookie, ppc_poll, (caddr_t)stc);
	if (error != DDI_SUCCESS) {
	    (void) stc_detach(dip, DDI_DETACH);
	    cmn_err(CE_CONT, "stc_attach: unable to install ppc interrupt\n");
	    return (error);
	    /* NOTREACHED */
	} else {
	    stc->flags |= STC_PPCINTROK;
	}

	error = ddi_add_softintr(dip, STC_SOFT_PREF, &stc->softint_id,
		&stc->soft_blk_cookie, (ddi_idevice_cookie_t *)0, stc_softint,
		(caddr_t)stc);
	if (error != DDI_SUCCESS) {
	    cmn_err(CE_CONT, "stc_attach: unable to install soft interrupt\n");
	    (void) stc_detach(dip, DDI_DETACH);
	    return (error);
	    /* NOTREACHED */
	} else {
	    stc->flags |= STC_SOFTINTROK;
	}

	/*
	 * set up the mutexii to protect the stc_unit_t struct
	 */
	stc->stc_mutex = kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	mutex_init(stc->stc_mutex, NULL, MUTEX_DRIVER,
		(void *)stc->stc_blk_cookie);
	stc->ppc_mutex = kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	mutex_init(stc->ppc_mutex, NULL, MUTEX_DRIVER,
		(void *)stc->ppc_blk_cookie);

	/*
	 * clear the open_inhibit and is_open flags for each of the lines of
	 *	this board;  don't forget the PPC and the board control line
	 * set up the per-line mutex to protect the stc_line_t struct for
	 *	each serial line
	 * for each per-line mutex, we use stc->stc_blk_cookie instead of
	 *	stc->soft_blk_cookie since we're using the line mutexii in
	 *	both the soft interrupt and the high-level interrupt code
	 */
	for (line_no = 0; line_no < STC_LINES; line_no++) {
		line = &stc->line[line_no];
		line->state = 0;
		line->line_mutex = kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
		mutex_init(line->line_mutex, NULL, MUTEX_DRIVER,
			(void *)stc->soft_blk_cookie);
		line->cvp = kmem_zalloc(sizeof (kcondvar_t), KM_SLEEP);
		cv_init(line->cvp, NULL, CV_DRIVER, NULL);
	}

	/*
	 * set up the mutex to protect the stc_line_t struct for the
	 *	parallel port
	 */
	line = &stc->ppc_line;
	line->line_mutex = kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	mutex_init(line->line_mutex, NULL, MUTEX_DRIVER,
		(void *)stc->ppc_blk_cookie);
	line->cvp = kmem_zalloc(sizeof (kcondvar_t), KM_SLEEP);
	cv_init(line->cvp, NULL, CV_DRIVER, NULL);

	/*
	 * set up the mutex to protect the stc_line_t struct for the
	 *	control line
	 */
	line = &stc->control_line;
	line->line_mutex = kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	mutex_init(line->line_mutex, NULL, MUTEX_DRIVER,
		(void *)stc->soft_blk_cookie);
	line->cvp = kmem_zalloc(sizeof (kcondvar_t), KM_SLEEP);
	cv_init(line->cvp, NULL, CV_DRIVER, NULL);

	stc->ppc_line.state = 0;
	stc->control_line.state = 0;

	/*
	 * the following code is used to initialize the driver statistics
	 *	structure for each board
	 */
	for (line_no = 0; line_no < STC_LINES; line_no++) {
	    bzero(&stc->line[line_no].stc_stats,
		sizeof (struct stc_stats_t));
	    stc->line[line_no].stc_stats.nqfretry = NQFRETRY;
	}

	/*
	 * create the minor devices for this instance
	 */
	for (line_no = 0; line_no < STC_LINES; line_no++) {
	    char stc_ttyname[16];
	    (void) sprintf(stc_ttyname, "%d", line_no);
	    if (ddi_create_minor_node(dip, stc_ttyname, S_IFCHR,
		STC_DINODE(instance, line_no), DDI_NT_SERIAL, 0) !=
		DDI_SUCCESS) {
		cmn_err(CE_CONT,
		"stc_attach: unable to create minor node for unit %d line %d\n",
			instance, line_no);
		(void) stc_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
	    }
	    (void) sprintf(stc_ttyname, "%d,cu", line_no);
	    if (ddi_create_minor_node(dip, stc_ttyname, S_IFCHR,
			STC_DONODE(instance, line_no), DDI_NT_SERIAL_DO, 0) !=
			DDI_SUCCESS) {
		cmn_err(CE_CONT,
		"stc_attach: unable to create minor node for unit %d line %d\n",
			instance, line_no);
		(void) stc_detach(dip, DDI_DETACH);
		return (DDI_FAILURE);
	    }
	}

	if (ddi_create_minor_node(dip, STC_PPCNAME, S_IFCHR,
		STC_PPCNODE(instance), STC_NT_PARALLEL, 0) != DDI_SUCCESS) {
	    cmn_err(CE_CONT,
		"stc_attach: unable to create minor node for unit %d (ppc)\n",
		instance);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(dip, STC_STCNAME, S_IFCHR,
		STC_STCNODE(instance), DDI_PSEUDO, 0) != DDI_SUCCESS) {
	    cmn_err(CE_CONT,
		"stc_attach: unable to create minor node for unit %d (stc)\n",
		instance);
	    (void) stc_detach(dip, DDI_DETACH);
	    return (DDI_FAILURE);
	}

	/*
	 * The routine report_dev() writes a line on the console and system
	 * log file to announce the attachment of the driver. This will
	 * happen either at boot or modload time, one line for each unit.
	 * It is nice to call report_dev before contacting the device
	 * if possible so that if something horrible happens there is
	 * a record of what was being attempted.
	 */
	ddi_report_dev(dip);
	SDM(STC_DEBUG, (CE_CONT, "stc%d: softint pri %d driver id %s\n",
				instance, STC_SOFT_PREF, STC_DRIVERID));
	SDM(STC_DEBUG, (CE_CONT, "stc%d: nstc = %d\n", instance, nstc));

#ifdef	STC_ALIVE
	stc_alive_id = timeout(stc_alive, stc, (STC_TIMEOUT*2));
#endif	STC_ALIVE

	/*
	 * set a flag that stc_open() can look at so that if this board
	 *	doesn't make it all the way through stc_attach(), we won't
	 *	try to open it
	 */
	stc->flags |= STC_ATTACHOK;

	return (DDI_SUCCESS);
}

#ifdef	STC_ALIVE
static void
stc_alive(void *arg)
{
	struct stc_unit_t *stc = arg;
	struct stc_line_t *line;
	int i;

	cmn_err(CE_CONT, "stc_alive: timer popped on 0x%x\n", stc);

	if (mutex_owned(stc->stc_mutex))
	    cmn_err(CE_CONT, "stc_alive: stc_mutex is owned by somebody\n");
	for (i = 0; i < STC_LINES; i++) {
	    line = &stc->line[i];
	    if (mutex_owned(line->line_mutex))
		cmn_err(CE_CONT,
			"stc_alive: line%d line_mutex is owned by somebody\n",
			line->line_no);
	}
	if (mutex_owned(stc->ppc_mutex))
	    cmn_err(CE_CONT, "ppc_alive: ppc_mutex is owned by somebody\n");

	line = &stc->control_line;
	if (mutex_owned(line->line_mutex))
	    cmn_err(CE_CONT,
		"stc_alive: control line line_mutex is owned by somebody\n");

	line = &stc->ppc_line;
	if (mutex_owned(line->line_mutex))
	    cmn_err(CE_CONT,
		"stc_alive: ppc line line_mutex is owned by somebody\n");

	stc_alive_id = timeout(stc_alive, stc, STC_TIMEOUT);
}
#endif	STC_ALIVE

/*
 * static int
 * stc_detach() - Deallocate kernel resources associate with this instance
 *	of the driver.
 *
 *	Returns:	DDI_SUCCESS, if able to detach.
 *			DDI_FAILURE, if unable to detach.
 */
static int
stc_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	struct stc_unit_t *stc;
	struct stc_line_t *line;
	unsigned int line_no;
	int instance;

	instance = ddi_get_instance(dip);

	SDM(STC_DEBUG, (CE_CONT, "stc_detach: cmd = 0x%x\n", cmd));
	SDM(STC_DEBUG, (CE_CONT, "stc_detach: instance = %d\n", instance));

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	stc = ddi_get_soft_state(stc_soft_state_p, instance);

	/*
	 * disable interrupts for each board, don't forget the PPC
	 * also deassert the DTR signal for each line
	 */
	if (stc->flags & STC_REGOK) {	/* make sure we've got mapped regs */
		for (line_no = 0; line_no < STC_LINES; line_no++) {
			stc->regs->car = (uchar_t)line_no;
			stc->regs->ier = 0;
			DTR_SET(stc->line, DTR_OFF);
		}
		/*
		 * disable ppc interrupts then clear any pending ppc interrupt
		 */
		stc->ppc->pcon = 0;
		stc->ppc->pwierd = stc->ppc->ipstat;
		/*
		 * now, unmap the registers since we don't need them anymore
		 */
		ddi_unmap_regs(dip, 0, (caddr_t *)&stc->prom, 0, 0);
	}

	/*
	 * free the various mutexii
	 */
	if (stc->stc_mutex) {
	    mutex_destroy(stc->stc_mutex);
	    kmem_free(stc->stc_mutex, sizeof (kmutex_t));
	}

	if (stc->ppc_mutex) {
	    mutex_destroy(stc->ppc_mutex);
	    kmem_free(stc->ppc_mutex, sizeof (kmutex_t));
	}

	line = &stc->ppc_line;
	if (line->line_mutex) {
	    mutex_destroy(line->line_mutex);
	    kmem_free(line->line_mutex, sizeof (kmutex_t));
	}

	if (line->cvp) {
	    cv_destroy(line->cvp);
	    kmem_free(line->cvp, sizeof (kcondvar_t));
	}

	line = &stc->control_line;
	if (line->line_mutex) {
	    mutex_destroy(line->line_mutex);
	    kmem_free(line->line_mutex, sizeof (kmutex_t));
	}

	if (line->cvp) {
	    cv_destroy(line->cvp);
	    kmem_free(line->cvp, sizeof (kcondvar_t));
	}

	for (line_no = 0; line_no < STC_LINES; line_no++) {
	    line = &stc->line[line_no];
	    if (line->line_mutex) {
		mutex_destroy(line->line_mutex);
		kmem_free(line->line_mutex, sizeof (kmutex_t));
	    }
	    if (line->cvp) {
		cv_destroy(line->cvp);
		kmem_free(line->cvp, sizeof (kcondvar_t));
	    }
	}

	/*
	 * unregister the interrupt routine for each board
	 * also, unregister the softinterrupt handler
	 */
	if (stc->flags & STC_CD180INTROK)
	    ddi_remove_intr(dip, STC_INT_NUM, stc->stc_blk_cookie);

	if (stc->flags & STC_PPCINTROK)
	    ddi_remove_intr(dip, PPC_INT_NUM, stc->ppc_blk_cookie);

	if (stc->flags & STC_SOFTINTROK)
	    ddi_remove_softintr(stc->softint_id);

	/*
	 * remove all of the device nodes for this instance
	 */
	ddi_remove_minor_node(dip, NULL);

	ddi_soft_state_free(stc_soft_state_p, instance);

#ifdef	STC_ALIVE
	UNTIMEOUT(stc_alive_id);
#endif	STC_ALIVE

	return (DDI_SUCCESS);
}

/*
 * stc_getinfo() - this routine translates the dip info dev_t and
 *	vice versa.
 *
 *	Returns:	DDI_SUCCESS, if successful.
 *			DDI_FAILURE, if unsuccessful.
 */
static int
stc_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	struct stc_unit_t *stc;
	int error = DDI_SUCCESS;
	int instance;

#ifdef lint
	dip = dip;
#endif
	SDM(STC_DEBUG, (CE_CONT, "stc_getinfo: cmd = 0x%x\n", cmd));
	SDM(STC_DEBUG, (CE_CONT, "stc_getinfo: arg = 0x%x\n", arg));

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
	    instance = STC_UNIT((dev_t)arg);
	    if (!(stc = ddi_get_soft_state(stc_soft_state_p, instance)))
		*result = NULL;
	    else
		*result = stc->dip;
	    break;
	case DDI_INFO_DEVT2INSTANCE:
	    instance = STC_UNIT((dev_t)arg);
	    *result = (void *)instance;
	    break;
	default:
	    SDM(STC_DEBUG, (CE_CONT, "stc_getinfo: FAILED\n"));
	    error = DDI_FAILURE;
	} /* switch */

	SDM(STC_TRACK, (CE_CONT,
		"stc_getinfo: exiting error = 0x%x *result = 0x%x\n",
		error, *result));
	return (error);
}

/*
 * stc_init(stc) - initialize the CD-180 and PPC and DTR latch on the board
 *
 *	we probe around on the board to see if we can at least read and write a
 *	few registers on the CD-180 and PPC.  if we fail any of these tests,
 *	return with an error, since the board is probably hosed
 *
 *    calling: unit - unit # of board to init
 *    returns:	0  - if no error
 *		-1 - if error
 */
static int
stc_init(struct stc_unit_t *stc)
{
	register struct stc_line_t *line;
	int i, unit;
	unsigned char r;
	struct stc_defaults_t stc_defbuf;

	/*
	 * get the (unit) instance number to use for messages
	 */
	unit = ddi_get_instance(stc->dip);

	/*
	 * first, reset the CD-180; if this fails (i.e. we get a bus error),
	 * then you might as well hang it up
	 *
	 * stuff a 0 into the GIVR, then tell the chip to reset all the channels
	 * this reinits the chip and firmware and sets the GIVR to 0x0ff
	 * when the firmware is ready
	 */
	if (ddi_poke8(stc->dip, (char *)&stc->regs->givr, 0) != DDI_SUCCESS) {
		SDM(STC_DEBUG, (CE_CONT,
			"stc_init%d: ERROR 1 on pokec of 0x%x\n",
			unit, (char *)&stc->regs->givr));
		return (-1);
	}
	if (ddi_poke8(stc->dip, (char *)&stc->regs->ccr, CD180_RESET_ALL) !=
		DDI_SUCCESS) {
		SDM(STC_DEBUG, (CE_CONT, "stc_init: ERROR 2 on pokec of 0x%x\n",
			(char *)&stc->regs->ccr));
		return (-1);
	}

	/*
	 * now check for the CD-180 GIVR register to be 0x0ff (as per
	 * the data sheet) this tells us that the CD-180 is reset and the
	 * firmware is ready for commands
	 *
	 * we need to wait here, since the CD-180 takes up to 500uS to
	 * initialize following a hardware or software reset
	 */
	drv_usecwait(4000);

	if (ddi_peek8(stc->dip, (char *)&stc->regs->givr, (char *)&r) !=
		DDI_SUCCESS) {
		SDM(STC_DEBUG, (CE_CONT, "stc_init: ERROR 3\n"));
		return (-1);
	}
	if ((r&0x0ff) != 0x0ff) {
		cmn_err(CE_CONT,
			"stc_init: unit %d GIVR was not 0x0ff, was: 0x%x\n",
			unit, (r&0x0ff));
		return (-1);
	}

	/*
	 * check the firmware revision register and display the
	 * firmware rev level
	 */

	if (ddi_peek8(stc->dip, (char *)&stc->regs->gfrcr, (char *)&r) !=
		DDI_SUCCESS) {
		SDM(STC_DEBUG, (CE_CONT, "stc_init: ERROR 4\n"));
		return (-1);
	} else {
		/*EMPTY*/ /* if DEBUG_STC not set */
		SDM(STC_DEBUG, (CE_CONT,
			"stc%d: CD-180 Firmware Revision Level: 0x%x\n",
			unit, r));
	}

	/*
	 * set the initial value of the ppc's control register and also
	 * update the soft copy of it in the line struct
	 * read the iack space to clear any pending interrupt, although at
	 * this point the monitor is probably screaming about "IO bus level 5
	 * interrupt not serviced"
	 */
	stc->ppc_line.pcon_s = PCON_INIT;
	stc->ppc->pcon = stc->ppc_line.pcon_s;
	stc->ppc->pwierd = stc->ppc->ipstat;

	/*
	 * turn off DTR for each port; it may get enabled later by
	 * the stc_defs defaults setup program
	 * note that the output state of the DTR lines are reversed, i.e.
	 * writing a 1 to the latch produces a LOW DTR output
	 */
	for (i = 0; i < STC_LINES; i++) {
	    if (ddi_poke8(stc->dip, (char *)stc->line[i].dtr, (char)DTR_OFF) !=
		DDI_SUCCESS) {
		SDM(STC_DEBUG, (CE_CONT, "stc_init: ERROR 5\n"));
		return (-1);
	    } else {
		line = &stc->line[i];
		DTR_SET(line, DTR_OFF);
	    }
	}

	/*
	 * initialize the serial line default parameter structure; use the
	 * values supplied from modload on the command line to override any
	 * driver defaults
	 *
	 * also initialize the buffer and silo params
	 * set up the line and unit numbers in each line struct so that if you
	 * use the STC_SDEFAULTS/STC_GDEFAULTS ioctl()'s before the line has
	 * been * first opened, they find the right line/unit number in the
	 * struct also, stuff a pointer to this unit's stc struct into each line
	 * struct for use by the PUTSILO/CHECK_RTS/FLUSHSILO macros
	 */
	for (i = 0; i < STC_LINES; i++) {
	    line = &stc->line[i];
	    line->state = 0;
	    line->unit_no = unit;
	    line->line_no = i;
	    line->stc = stc;		/* ugh... kludge */
	    line->stc_timeout_id = 0;
	    line->stc_draintimeout_id = 0;
	    line->ppc_acktimeout_id = 0;
	    line->ppc_out_id = 0;

		/*
		 * most of the defaults for the cd180 registers are set by
		 * stc_setdefaults()
		 * line->default_param.cor1 is initialized in stc_defaults()
		 */
	    line->default_param.cor3 = COR3;
	    bcopy(&stc_initmodes, &stc_defbuf,
		sizeof (struct stc_defaults_t));
	    stc_getdefaults(stc->dip, (unit*8)+i, &stc_defbuf);
	    stc_setdefaults(line, &stc_defbuf);
	    line->default_param.mcor1 = MCOR1;		/* pins to monitor */
	    line->default_param.mcor2 = MCOR2;		/* pins to monitor */
	    line->stc_txbufsize = STC_TXBUFSIZE;	/* size of tx buffer */
	    line->stc_silosize = STC_SILOSIZE;		/* size of rx silo */
	}

	/*
	 * do the same for the ppc default parameter structure
	 */
	line = &stc->ppc_line;
	line->state |= STC_LP;
	line->unit_no = unit;
	line->line_no = 0;
	line->stc = stc;		/* ugh... kludge */
	line->dtr = (uchar_t *)NULL;
	bcopy(&ppc_initmodes, &stc_defbuf, sizeof (struct stc_defaults_t));
	ppc_getdefaults(stc->dip, unit, &stc_defbuf);
	stc_setdefaults(line, &stc_defbuf);
	line->default_param.pcon = PCON_INIT;
	line->stc_txbufsize = PPC_TXBUFSIZE;	/* size of soft tx buffer */
	line->stc_silosize = STC_SILOSIZE;	/* size of soft rx silo */
	line->stc_timeout_id = 0;
	line->stc_draintimeout_id = 0;
	line->ppc_acktimeout_id = 0;
	line->ppc_out_id = 0;

	/*
	 * clear the address in the dtr field of the control line
	 */
	line = &stc->control_line;
	line->stc = stc;		/* ugh... kludge */
	line->dtr = (uchar_t *)NULL;
	line->stc_timeout_id = 0;
	line->stc_draintimeout_id = 0;
	line->ppc_acktimeout_id = 0;
	line->ppc_out_id = 0;

	SDM(STC_DEBUG, (CE_CONT,
		"stc_init: calling cd180_init(0x%x, %d)\n", stc, INIT_ALL));

	/*
	 * initialize the CD-180; tell cd180_init() to do all the channels
	 */
	if (cd180_init(stc, INIT_ALL)) {
		SDM(STC_DEBUG, (CE_CONT, "stc_init: ERROR 6\n"));
		return (-1);
	}

	SDM(STC_DEBUG, (CE_CONT, "stc_init: returning 0 (sucess)\n"));

	return (0);
}

/*
 * cd180_init(stc, chan) - initialize the CD-180 to default values
 *
 *    calling:	stc - pointer to board structure to initialize
 *		chan - channel number to initialize from defaults
 *			0..STC_LINES - init that channel
 *			INIT_ALL - reset CD180 and initialize all channels
 *	note that the passed channel number must be in range (0..STC_LINES)
 *
 * if the channel number passed is INIT_ALL, no mutex locking is provided
 *	since when we're called with INIT_ALL it's from stc_init() which
 *	is called from stc_attach() before any mutexes have been set up
 *
 *	this routine takes the values in the default_param fields of the
 *	per line struct and applys them to the cd180.  use stc_param()
 *	to set a channel's parameters from the soft_param fields instead
 *
 *    returns:	0 if all ok
 *		-1 if error
 */
static int
cd180_init(struct stc_unit_t *stc, int chan)
{
	int i, start_chan, end_chan, unit;
	uchar_t r;

	/*
	 * get the (unit) instance number to use for messages
	 */
	unit = ddi_get_instance(stc->dip);

	SDM(STC_DEBUG, (CE_CONT,
	"---------------(unit %d, line %d)------(cd180_init)---------\n",
		unit, chan));

	if (chan == INIT_ALL) {
		/*
		 * reset the CD-180 - this is a rehash of
		 * the stuff in stc_init()
		 */
	    stc->regs->givr = 0;
	    SET_CCR(stc->regs->ccr, CD180_RESET_ALL);
		/*
		 * now check for the CD-180 GIVR register to be 0x0ff
		 * (as per the data sheet) we need to wait here, since
		 * the CD-180 takes up to 500uS to initialize following
		 * a hardware or software reset
		 */
	    drv_usecwait(4000);
	    r = stc->regs->givr;
	    if (r != 0x0ff) {
		cmn_err(CE_CONT,
		"cd180_init(stc): unit %d GIVR was not 0x0ff, was: 0x%x\n",
			unit, (r&0x0ff));
		return (-1);
	    }

		/*
		 * check the firmware revision register
		 */
	    r = stc->regs->gfrcr;
	    SDM(STC_DEBUG, (CE_CONT,
		"stc%d: CD-180 Firmware Revision Level: 0x%x\n", unit, r));

		/*
		 * setup the default values for the CD-180 lines; first the
		 * global registers
		 */
	    stc->regs->pprh = PPRH;		/* prescaler period */
	    stc->regs->pprl = PPRL;
	    stc->regs->pilr1 = PILR1;	/* group 1 - modem signal change */
	    stc->regs->pilr2 = PILR2;	/* group 2 - transmit data interrupt */
	    stc->regs->pilr3 = PILR3;	/* group 3 - receive data/excptn int */
	    stc->regs->givr = 0;
	    stc->regs->gicr = 0;
		/*
		 * setup loop to initialize all channels
		 */
	    start_chan = 0;
	    end_chan = STC_LINES;
	} else {
	    mutex_enter(stc->stc_mutex);
	    stc->regs->car = (uchar_t)chan;
	    SET_CCR(stc->regs->ccr, RESET_CHAN);
	    mutex_exit(stc->stc_mutex);
	    drv_usecwait(2000);
	    start_chan = chan;
	    end_chan = chan + 1;
	} /* chan == INIT_ALL */

	if (chan != INIT_ALL)
	    mutex_enter(stc->stc_mutex);

	for (i = start_chan; i < end_chan; i++) {
	    stc->regs->car = (uchar_t)i; /* set channel number */
	    SET_COR(stc->regs->ccr, stc->regs->cor1, COR1_CHANGED,
		stc->line[i].default_param.cor1);
	    SET_COR(stc->regs->ccr, stc->regs->cor2, COR2_CHANGED,
		stc->line[i].default_param.cor2);
	    SET_COR(stc->regs->ccr, stc->regs->cor3, COR3_CHANGED,
		stc->line[i].default_param.cor3);
	    SET_CCR(stc->regs->ccr, (CHAN_CTL | XMTR_DIS | RCVR_DIS));
	    stc->regs->ier = 0;	    /* disable all interrupts */
	    stc->regs->schr1 = stc->line[i].default_param.schr1;
	    stc->regs->schr2 = stc->line[i].default_param.schr2;
	    stc->regs->schr3 = stc->line[i].default_param.schr3;
	    stc->regs->schr4 = stc->line[i].default_param.schr4;
	    stc->regs->mcor1 = stc->line[i].default_param.mcor1;
	    stc->regs->mcor2 = stc->line[i].default_param.mcor2;
	    stc->regs->mcr = 0;
	    stc->regs->msvr = 0;
	    stc->regs->rbprh = stc->line[i].default_param.rbprh;
	    stc->regs->rbprl = stc->line[i].default_param.rbprl;
	    stc->regs->tbprh = stc->line[i].default_param.tbprh;
	    stc->regs->tbprl = stc->line[i].default_param.tbprl;
	    stc->regs->rtpr =  stc->line[i].default_param.rtpr;
	}

	if (chan != INIT_ALL)
	    mutex_exit(stc->stc_mutex);

	return (0);
}

/*
 * stc_softint(stc) - handle the STREAMS stuff that the routines called
 *			by stc_poll() setup for us
 */
static u_int
stc_softint(caddr_t arg)
{
	register struct stc_line_t *line;
	int line_no;
	struct stc_unit_t *stc = (struct stc_unit_t *)arg;

	/*
	 * run through each of the serial lines on this board
	 * and check for work to do
	 */
	if (stc->flags & STC_ATTACHOK) {
		for (line_no = 0; line_no < STC_LINES; line_no++) {
			line = &stc->line[line_no];
			if (!(line->state & (STC_UNTIMEOUT | STC_RXWORK |
				STC_TXWORK | STC_CVBROADCAST))) {
				continue;
			}
			/*
			 * If another softint thread is working on this line
			 * skip this one and contine with next line.
			 */
			mutex_enter(line->line_mutex);
			if ((line->state & (STC_UNTIMEOUT | STC_RXWORK |
				STC_TXWORK | STC_CVBROADCAST)) &&
					!(line->state & STC_INSOFTINT)) {
				mutex_enter(stc->stc_mutex);
				stc_softint_line(line);
				mutex_exit(stc->stc_mutex);
			}
			mutex_exit(line->line_mutex);
		} /* for (line_no) */
	} /* STC_ATTACHOK */

	return (DDI_INTR_CLAIMED);
}

static void
stc_softint_line(struct stc_line_t *line)
{
	struct stc_unit_t *stc = line->stc;
	register queue_t *q;

	STC_LOGIT(line->line_no, 0, 0x736c, line);
	line->state |= STC_INSOFTINT;
again:
	/*
	 * see if we have to do a untimeout() on behalf of
	 *	the higher-level interrupt routines
	 */
	if (line->state & STC_UNTIMEOUT) {
		line->state &= ~STC_UNTIMEOUT;
		mutex_exit(stc->stc_mutex);
		UNTIMEOUT(line->stc_timeout_id);
		mutex_enter(stc->stc_mutex);
	}
	/*
	 * handle any Rx work to be done
	 */
	q = line->stc_ttycommon.t_readq;
	if (q && q->q_next && (line->state & STC_RXWORK)) {
		line->state &= ~STC_RXWORK;
		mutex_exit(stc->stc_mutex);
		enterq(q);
		/*
		 * check to see if there's any received data to
		 * send downstream
		 */
		if (line->stc_sscnt) {
			stc_drainsilo(line);
		}
		/*
		 * now, deal with the out-of-band conditions:
		 *	BREAK
		 *	HANGUP (loss of carrier)
		 *	UNHANGUP (resumption of carrier)
		 * these should probably be handled in band...
		 */
		if (line->state & STC_MBREAK) {
			mutex_enter(stc->stc_mutex);
			line->state &= ~STC_MBREAK;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
			(void) putnextctl(q, M_BREAK);
			mutex_enter(line->line_mutex);
		} /* STC_MBREAK */
		if (line->state & STC_MHANGUP) {
			mutex_enter(stc->stc_mutex);
			line->state &= ~STC_MHANGUP;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
			(void) putnextctl(q, M_HANGUP);
			mutex_enter(line->line_mutex);
		} /* STC_MHANGUP */
		if (line->state & STC_MUNHANGUP) {
			mutex_enter(stc->stc_mutex);
			line->state &= ~STC_MUNHANGUP;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
			(void) putnextctl(q, M_UNHANGUP);
			mutex_enter(line->line_mutex);
		} /* STC_MUNHANGUP */
		leaveq(q);
		mutex_enter(stc->stc_mutex);
	} /* if (q&&STC_RXWORK) */

	/*
	 * handle any Tx work to be done
	 */
	q = line->stc_ttycommon.t_writeq;
	if (q && (line->state & STC_TXWORK)) {
	/*
	 * let STREAMS know that we're going to
	 *	diddle with this queue
	 */
		STC_LOGIT(line->line_no, 1, 0x736c, line->state);
		line->state &= ~STC_TXWORK;
		mutex_exit(stc->stc_mutex);
		enterq(q);
		stc_start(line);
		leaveq(q);
		mutex_enter(stc->stc_mutex);
	} /* if (q&&STC_TXWORK) */

	/*
	 * see if we have to do a cv_broadcast() on behalf of
	 *	the higher-level interrupt routines
	 */
	if (line->state & STC_CVBROADCAST) {
		line->state &= ~STC_CVBROADCAST;
		mutex_exit(stc->stc_mutex);
		cv_broadcast(line->cvp);
		mutex_enter(stc->stc_mutex);
	}

	/*
	 * If STC_RXWORK or STC_TXWORK flag is set again by the
	 * hardware interrupt handler then we have more work to do.
	 */
	STC_LOGIT(line->line_no, 2, 0x736c, line->state);
	if (line->state & (STC_UNTIMEOUT | STC_RXWORK |
		STC_TXWORK | STC_CVBROADCAST)) {
		if (line->state & (STC_UNTIMEOUT | STC_CVBROADCAST))
			goto again;
		q = line->stc_ttycommon.t_readq;
		if (q && q->q_next && (line->state & STC_RXWORK))
			goto again;
		q = line->stc_ttycommon.t_writeq;
		if (q && (line->state & STC_TXWORK))
			goto again;
	}
	if (line->stc_sscnt) {
		line->state |= STC_RXWORK;
	}
	line->state &= ~STC_INSOFTINT;
}

/*
 * stc_poll(stc) - cd-180 interrupt routine
 *
 * return:	1 if one or more interrupts has been serviced
 *		0 if not
 */
static u_int
stc_poll(caddr_t arg)
{
	int serviced = DDI_INTR_UNCLAIMED, line_no;
	uchar_t givr;
	struct stc_unit_t *stc = (struct stc_unit_t *)arg;

	STC_LOGIT(0xff, 0, 0x706f, stc);
	SDM(STC_TRACK, (CE_CONT, "stc_poll: we're here...\n"));

	if (!(stc->flags & STC_ATTACHOK))
	    return (DDI_INTR_UNCLAIMED);

	mutex_enter(stc->stc_mutex);

	/*
	 * march through each of the unit_nos, checking each line_no
	 * for interrupt requests; check all boards for receive
	 * data interrupts first, these are the first to get lost
	 * when the system gets heavily loaded; then check for
	 * modem signal change interrupts, and finally for transmitter
	 * buffer empty interrupts
	 *
	 * use if's instead of a switch to force the compiler to
	 * order the evaluations in the order that we want to
	 * see them
	 *
	 * handle group 3 (receive data/exception) interrupts
	 */
	    for (line_no = 0; line_no < STC_LINES; line_no++) {
		givr = stc->iack->pilr3;
		if (givr == RCV_DATA)		/* receive data available */
			serviced = stc_rcv(stc);
		else if (givr == RCV_EXP)	/* receive exception */
			serviced = stc_rcvex(stc);
	    } /* for (line_no) */

	/*
	 * handle group 2 (transmit data) interrupts
	 */
	    for (line_no = 0; line_no < STC_LINES; line_no++) {
		givr = stc->iack->pilr2;
		if (givr == XMIT_DATA) {
			STC_LOGIT(line_no, 1, 0x706f, stc);
			serviced = stc_xmit(stc);
		}
	    } /* for (line_no) */

	/*
	 * handle group 1 (modem signal change) interrupts
	 */
	    for (line_no = 0; line_no < STC_LINES; line_no++) {
		givr = stc->iack->pilr1;
		if (givr == MODEM_CHANGE)
			serviced = stc_modem(stc);
	    } /* for (line_no) */

	/*
	 * if we've got any work to do, schedule a softinterrupt
	 */
	if (serviced == DDI_INTR_CLAIMED)
	    ddi_trigger_softintr(stc->softint_id);

	SDM(STC_TRACK, (CE_CONT, "------(LEAVING stc_poll)-----\n"));

	mutex_exit(stc->stc_mutex);

	return (serviced);
}

/*
 * stc_xmit(stc) - handle xmit interrupts
 */
static int
stc_xmit(struct stc_unit_t *stc)
{
	register struct stcregs_t *regs = stc->regs;
	register struct stc_line_t *line;
	register int i;

	SDM(STC_TRACK, (CE_CONT, "----- stc_xmit(0x%x) ENTERING -----\n", stc));

	/*
	 * get the interrupting channel number and setup the line
	 *	pointer to it
	 */
	i = (int)((regs->gicr)>>2);
	line = &stc->line[i];

	STC_LOGIT(line->line_no, 0, 0x786d, line);
	SDM(STC_TRACK, (CE_CONT,
		"---------------(unit %d, line %d)------(stc_xmit)---------\n",
		line->unit_no, line->line_no));

	/*
	 * disable xmit interrupts for this channel and clear the busy
	 * flag - the reason we disable both xmit interrupts is because
	 * we might get here from stc_xwait() which enables the TX_MPTY
	 * interupt and it's easier to disable both of them rather than
	 * put another test in (save a few cycles and all that...)
	 */
	regs->ier &= ~(TX_RDY|TX_MPTY);
	ASSERT(mutex_owned(stc->stc_mutex));
	STC_LOGIT(line->line_no, 1, 0x786d, line->state);
	line->state &= ~STC_BUSY;
	regs->cor2 &= ~EMBED_XMIT;
	SET_CCR(stc->regs->ccr, COR2_CHANGED|COR_CHANGED);

	/*
	 * if we're coming here to signal when the transmitter is empty,
	 *	then just clear the wait flag and schedule a cv_broadcast()
	 *	otherwise load any chars ready into the xmit buffer and
	 *	schedule a call to stc_start() to keep the pump going
	 * if there's more output to send; we might also be coming here
	 *	due to a break condition - enable the cd180 to do the
	 *	break thing then go away until it's done
	 * we set stc_txcount equal to LINE_TXBUFSIZE as an indication to
	 *	us that we've been here before on a call from stc_xwait()
	 *	as well as to allow stc_xwait() to enble one more TxEMPTY
	 *	interrupt after the final Tx bytes have been written to
	 *	the cd180
	 */
	if (line->state & STC_XWAIT) {
		line->state &= ~STC_XWAIT;
		if (line->stc_txcount == LINE_TXBUFSIZE)
		    line->stc_txcount = 0;
		if (line->stc_txcount) {
		    line->stc_stats.xmit_cc += line->stc_txcount;
		    for (i = 0; i < line->stc_txcount; i++)
			regs->tdr = line->stc_txbuf[i];
		    line->stc_txcount = LINE_TXBUFSIZE;
		}
		line->state |= (STC_CVBROADCAST | STC_UNTIMEOUT);
	} else if (line->state & STC_BREAK) {
		int bl;
		uchar_t *bp;
		line->state &= ~STC_BREAK;
		regs->cor2 |= EMBED_XMIT;
		SET_CCR(stc->regs->ccr, COR2_CHANGED|COR_CHANGED);
		if (line->state & STC_SBREAK) {		/* start BREAK */
			line->state &= ~STC_SBREAK;
			bl = SBREAK_LEN;
			bp = stc_sbreak;
		} else if (line->state & STC_EBREAK) {	/* end BREAK */
			line->state &= ~STC_EBREAK;
			bl = EBREAK_LEN;
			bp = stc_ebreak;
		} else {				/* programmed BREAK */
			bl = EMBED_XMIT_LEN;
			bp = stc_break;
		}
		/* send the BREAK sequence */
		for (i = 0; i < bl; i++)
			regs->tdr = *bp++;
		line->state |= STC_TXWORK;
	} else { /* this is a real send-data interrupt */
		line->stc_stats.xmit_cc += line->stc_txcount;
		for (i = 0; i < line->stc_txcount; i++)
			regs->tdr = line->stc_txbuf[i];
		line->stc_txcount = 0;
		line->state |= STC_TXWORK;
	}

	regs->eoir = 0; /* ditto */

	line->stc_stats.xmit_int++;

	SDM(STC_TRACK, (CE_CONT,
		"-----(unit %d, line %d)------(LEAVING stc_xmit)-----\n",
		line->unit_no, line->line_no));

	return (DDI_INTR_CLAIMED);
}

/*
 * stc_rcv(stc) - handle receive interrupts
 */
static int
stc_rcv(struct stc_unit_t *stc)
{
	register struct stcregs_t *regs = stc->regs;
	register struct stc_line_t *line;
	register int i, rc;
	register uchar_t c;
	register tcflag_t iflag;

	SDM(STC_TRACK, (CE_CONT, "----- stc_rcv(0x%x) ENTERING -----\n", stc));

	/*
	 * get the interrupting channel number and setup the line
	 *	pointer to it
	 */
	i = ((regs->gicr)>>2);
	line = &stc->line[i];
	iflag = line->stc_ttycommon.t_iflag;

	SDM(STC_TRACK, (CE_CONT,
		"---------------(unit %d, line %d)------(stc_rcv)---------\n",
		line->unit_no, line->line_no));

	rc = (int)(regs->rdcr);
	line->stc_stats.rcv_cc += rc;

	SDM(STC_DEBUG, (CE_CONT, "stc_rcv: %d characters in fifo\n", rc));

	/*
	 * move the characters from the cd180's receive fifo into our
	 *	local soft silo
	 * the streams stuff takes care of ISTRIP
	 *	IGNPAR and PARMRK is taken care of in stc_rcvex() except
	 *	for checking for a valid `\377` received character
	 */
	for (i = 0; i < rc; i++) {
	    c = regs->rdr;
	    if ((iflag & IGNPAR) || !((iflag & PARMRK) && !(iflag & ISTRIP) &&
		(c == (uchar_t)'\377'))) {
		PUTSILO(line, c);
	    } else { /* !IGNPAR */
		PUTSILO(line, '\377');
		PUTSILO(line, c);
	    }
	} /* for (rc) */

	/*
	 * flag the line as having data available in the soft silo
	 */
	line->state |= STC_RXWORK;

	regs->eoir = 0;
	line->stc_stats.rcv_int++;

	SDM(STC_TRACK, (CE_CONT,
		"-----(unit %d, line %d)------(LEAVING stc_rcv)-----\n",
		line->unit_no, line->line_no));

	return (DDI_INTR_CLAIMED);
}

/*
 * stc_rcvex(stc) - handle receive exception interrupts
 */
static int
stc_rcvex(struct stc_unit_t *stc)
{
	register struct stcregs_t *regs = stc->regs;
	register struct stc_line_t *line;
	register int i, rc;
	register uchar_t c;
	register uchar_t rcsr;

	SDM(STC_TRACK, (CE_CONT,
		"----- stc_rcvex(0x%x) ENTERING -----\n", stc));

	/*
	 * get the interrupting channel number and setup the line
	 *	pointer to it
	 */
	i = ((regs->gicr)>>2);
	line = &stc->line[i];

	SDM(STC_TRACK, (CE_CONT,
		"---------------(unit %d, line %d)------(stc_rcvex)---------\n",
		line->unit_no, line->line_no));

	rc = (int)(regs->rdcr);

	for (i = 0; i < rc; i++) {
		/*
		 * read the status of the current fifo entry and also the
		 * character that coresponds to it
		 */
		rcsr = regs->rcsr;
		c = regs->rdr;

		/*
		 * if we got a break, send an M_BREAK message upstream, the
		 * tty modules will sort it out based on the termios t_iflag
		 * the tty_ldterm module handles the IGNBRK and BRKINT cases
		 * and to think I thought that streams was another useless
		 * layer of unnecessary processing...!
		 * if our q is null, then this is probably a stray interrupt
		 * process the exceptions anyway so that the cd180 thinks
		 * we dealt with them and doesn't generate another exception
		 * interrupt
		 */
		if (rcsr & RX_BREAK) {
			line->stc_stats.break_cnt++;
			line->state |= STC_MBREAK;
		}

	/*
	 * If IGNPAR is set, characters with framing or	parity	errors
	 * (other  than	break)	are  ignored.  Otherwise, if PARMRK is
	 * set, a character with a framing or parity error that is	not
	 * ignored	is  read  as  the three-character sequence: '\377',
	 * '\0', X, where X is the data of the  character  received	 in
	 * error.  To  avoid  ambiguity	in this case, if ISTRIP is not
	 * set, a valid character of '\377' is read as '\377',  '\377'.
	 * If  neither  IGNPAR  nor	 PARMRK is set, a framing or parity
	 * error (other than break) is read as a single ASCII NUL char-
	 * acter ('\0').
	 *
	 * Note that ldterm now does most of this for us, so we should
	 *	probably take this stuff out in the next release.
	 */
	    if (rcsr & (RX_PARITY | RX_FRAMING)) {
		if (line->stc_ttycommon.t_iflag & IGNPAR) {
			/*EMPTY*/ /* if DEBUG_STC not set */
		    SDM(STC_DEBUG, (CE_CONT,
		    "stc_rcvex: unit %d, line %d PARITY ERROR, char: 0x%x\n",
			line->unit_no, line->line_no, 0x0ff&c));
		} else { /* !IGNPAR */
		    if (line->stc_ttycommon.t_iflag & PARMRK) {
			PUTSILO(line, '\377');
			if ((c == (uchar_t)'\377') &&
				!(line->stc_ttycommon.t_iflag & ISTRIP)) {
				PUTSILO(line, '\377');
			} else { /* ISTRIP */
				PUTSILO(line, '\0');
				PUTSILO(line, c);
			}
		    } else { /* !PARMRK */
			PUTSILO(line, '\0');
		    }
		}
	    }

	    if (rcsr & RX_OVERRUN) {
		/*EMPTY*/ /* if DEBUG_STC not set */
		SDM(STC_TRACK, (CE_CONT,
		"stc_rcvex: unit %d line %d receiver overrun, char: 0x%x\n",
			line->unit_no, line->line_no, 0x0ff&c));
	    }
	} /* for (i < rc) */

	line->state |= STC_RXWORK;

	line->stc_stats.rcvex_int++;
	regs->eoir = 0;

	SDM(STC_DEBUG, (CE_CONT,
		"-----(unit %d, line %d)------(LEAVING stc_rcvex)-----\n",
		line->unit_no, line->line_no));

	return (DDI_INTR_CLAIMED);
}

/*
 * stc_modem(stc) - handles modem control line changes
 */
static int
stc_modem(struct stc_unit_t *stc)
{
	register struct stcregs_t *regs = stc->regs;
	register struct stc_line_t *line;
	register int i;
	uchar_t mcr;

	SDM(STC_TRACK, (CE_CONT,
		"----- stc_modem(0x%x) ENTERING -----\n", stc));

	/*
	 * get the interrupting channel number and setup the line
	 *	pointer to it
	 */
	i = ((regs->gicr)>>2);
	line = &stc->line[i];

	SDM(STC_TRACK, (CE_CONT,
		"---------------(unit %d, line %d)------(stc_rcv)---------\n",
		line->unit_no, line->line_no));

	/*
	 * read the modem signal change register - we should only
	 * get here due to a change on the CD line, since all the
	 * other modem control interrupts should never be enabled
	 *
	 * reading the msvr returns the states of the various input
	 * pins, but these are INVERTED, i.e. a HIGH on input to the
	 * cd180 reads as a LOW in this register, but it gets better
	 * because the line receivers invert the signal one more time
	 * so that a HIGH on the DB-25 connector reads as a HIGH in
	 * this register.  pretty neat, huh?
	 */
	if ((mcr = regs->mcr) & CD_CHANGE) {
		SDM(STC_DEBUG, (CE_CONT,
			"stc_modem: CD_CHANGE for unit %d line %d\n",
			line->unit_no, line->line_no));
		if (!(regs->msvr&CD_ON) &&
			!(line->stc_ttycommon.t_flags & TS_SOFTCAR)) {
			/*
			 * carrier dropped
			 */
		    if ((line->state&STC_CARR_ON) &&
			!(line->stc_ttycommon.t_cflag&CLOCAL)) {
			/*
			 * Carrier went away.
			 * Drop DTR, abort any output in progress,
			 * indicate that output is not stopped, and
			 * send a hangup notification upstream.
			 */
			DTR_SET(line, DTR_OFF); /* drop DTR */
			stc_txflush(line);
			line->state &= ~STC_STOPPED;
			line->state |= (STC_MHANGUP | STC_RXWORK);
			line->state |= STC_DRAIN;
		    }
		    line->state &= ~STC_CARR_ON;
		    ASSERT(mutex_owned(stc->stc_mutex));
		    STC_LOGIT(line->line_no, 0, 0x6d6f, line->state);
		    line->state &= ~STC_BUSY;
		} else {	/* carrier raised */
		    if (!(line->state&STC_CARR_ON)) {
			if (line->flags & (DTR_ASSERT | STC_DTRFORCE))
				DTR_SET(line, DTR_ON); /* raise DTR */
			line->state |= STC_CARR_ON;
			line->state |= (STC_MUNHANGUP | STC_RXWORK);
			line->state &= ~STC_DRAIN;
			line->state |= STC_CVBROADCAST;
		    }
		}
	} else {
		/*EMPTY*/ /* if DEBUG_STC not set */
#ifdef lint
		mcr = mcr;
#endif
		SDM(STC_TESTING, (CE_CONT,
"stc_modem: unit %d line %d interesting modem control: MCR: 0x%x, MSVR: 0x%x\n",
			line->unit_no, line->line_no,
			mcr&0x0ff, (regs->msvr)&0x0ff));
	}

	regs->mcr = 0;
	regs->eoir = 0;
	line->stc_stats.modem_int++;

	SDM(STC_TRACK, (CE_CONT,
		"-----(unit %d, line %d)------(LEAVING stc_modem)-----\n",
		line->unit_no, line->line_no));

	return (DDI_INTR_CLAIMED);
}

/*
 * stc_txflush(line) - resets cd-180 channel and flushes the rest of it's data
 *			even if being held off by flow control
 */
static void
stc_txflush(struct stc_line_t *line)
{
	register struct stcregs_t *regs = line->stc->regs;

	line->stc_txcount = 0;
	regs->car = (uchar_t)line->line_no;
	SET_CCR(regs->ccr, RESET_CHAN);
	SET_CCR(regs->ccr, (CHAN_CTL|XMTR_ENA|RCVR_ENA));
}

/*
 * stc_draintimeout(line)
 */
static void
stc_draintimeout(void *arg)
{
	struct stc_line_t *line = arg;

	SDM(STC_TRACK, (CE_CONT, "stc_draintimeout: unit %d line %d\n",
				line->unit_no, line->line_no));
	mutex_enter(line->line_mutex);
	if (line->stc_draintimeout_id == 0) {
		mutex_exit(line->line_mutex);
		return;
	}
	line->stc_draintimeout_id = 0;
	mutex_enter(line->stc->stc_mutex);
	line->state &= ~STC_CANWAIT;
	if (!(line->state & STC_INSOFTINT)) {
		stc_softint_line(line);
	}
	mutex_exit(line->stc->stc_mutex);
	mutex_exit(line->line_mutex);
}

static void
stc_drainbufcall(void *arg)
{
	struct stc_line_t *line = arg;

	SDM(STC_TRACK, (CE_CONT, "stc_drainbufcall: unit %d line %d\n",
				line->unit_no, line->line_no));
	mutex_enter(line->line_mutex);
	if (line->stc_wbufcid == 0) {
	    mutex_exit(line->line_mutex);
	    return;
	}
	mutex_enter(line->stc->stc_mutex);
	line->stc_wbufcid = 0;
	if (!(line->state & STC_INSOFTINT)) {
		stc_softint_line(line);
	}
	mutex_exit(line->stc->stc_mutex);
	mutex_exit(line->line_mutex);
}

/*
 * stc_drainsilo(line) - This snarfs chars out of the input silo and
 * does queue fu up the stream
 */
static void
stc_drainsilo(struct stc_line_t *line)
{
	queue_t *q = line->stc_ttycommon.t_readq;
	struct stc_unit_t *stc = line->stc;
	mblk_t *bp;
	int cc;
	timeout_id_t stc_draintimeout_id = 0;

	SDM(STC_TRACK, (CE_CONT,
	"---------------(unit %d, line %d)------(stc_drainsilo)---------\n",
		line->unit_no, line->line_no));

	/*
	 * if we got here because of a receive or receive exception interrupt,
	 * remove the timer from the timeout table or we'll get a panic
	 */
	if (line->state & STC_CANWAIT) {
		if (line->stc_draintimeout_id) {
			stc_draintimeout_id = line->stc_draintimeout_id;
			line->stc_draintimeout_id = 0;
		}
		mutex_enter(stc->stc_mutex);
		line->state &= ~STC_CANWAIT;
		mutex_exit(stc->stc_mutex);
		line->stc_qfcnt = 0;
		line->stc_stats.canwait++;
		if (stc_draintimeout_id) {
			mutex_exit(line->line_mutex);
			(void) untimeout(stc_draintimeout_id);
			mutex_enter(line->line_mutex);
		}
		SDM(STC_DEBUG, (CE_CONT,
			"stc_drainsilo: removed timer for unit %d line %d\n",
			line->unit_no, line->line_no));
	}

	/*
	 * we should never see this condition unless a timer pop goes off
	 * after the line is closed (but that should have been taken care
	 * of in stc_close())
	 * if we do see it, it could mean that the timer popped before the line
	 * was closed, usually due to a process sleeping in stc_open() waiting
	 * for carrier to come up - this is not an error, just silently go away
	 */
	if (q == NULL) {
		SDM(STC_DEBUG, (CE_CONT,
			"stc_drainsilo: q == NULL for unit %d, line %d\n",
			line->unit_no, line->line_no));
		goto drain_o;
	}

	/*
	 * allocate a message and send the data up the stream
	 * silocopy() handles all the pointers and counts
	 * if bufcall() fails, the softsilo will get cleared by
	 * PUTSILO someday.
	 */

	while (line->stc_sscnt) {
	    if (canputnext(q)) {
		line->stc_qfcnt = 0;
		cc = MIN(line->drain_size, line->stc_sscnt);
		if ((bp = allocb(cc, BPRI_MED)) != NULL) {
			SDM(STC_DEBUG, (CE_CONT, "drain silo of %d chars", cc));
			mutex_enter(stc->stc_mutex);
			silocopy(line, bp->b_wptr, cc);
			bp->b_wptr += cc;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
			putnext(q, bp);
			mutex_enter(line->line_mutex);
		} else {
			if (!(line->stc_wbufcid = bufcall(cc, BPRI_HI,
			    stc_drainbufcall, line))) {
				mutex_enter(stc->stc_mutex);
				FLUSHSILO(line);	/* flush the rcv silo */
				mutex_exit(stc->stc_mutex);
				cmn_err(CE_CONT,
"stc_drainsilo: unit %d line %d can't allocate %d byte streams buffer\n",
					line->unit_no, line->line_no, cc);
			} else {
				line->stc_stats.bufcall++;
				SDM(STC_DEBUG, (CE_CONT,
				"stc_drainsilo: bufcall succeeded, leaving\n"));
			}
			goto drain_o;
		} /* allocb() */
	    } else { /* canput() */
		/*
		 * no room in read queues, so try a few times then give up if we
		 * still can't do it
		 */
		SDM(STC_DEBUG, (CE_CONT,
			"stc_drainsilo: can't put unit %d line %d\n",
			line->unit_no, line->line_no));
		line->stc_stats.no_canput++;

		/*
		 * if we've exceeded the number of retrys that we want to
		 * make, punt by printing a message and flush the data
		 */
		if (++line->stc_qfcnt > line->stc_stats.nqfretry) {
			cmn_err(CE_CONT,
		"stc_drainsilo: unit %d line %d punting after %d put retries\n",
			line->unit_no, line->line_no, line->stc_stats.nqfretry);
			line->stc_stats.qpunt++;
			mutex_exit(line->line_mutex);
			ttycommon_qfull(&line->stc_ttycommon, q);
			mutex_enter(line->line_mutex);
			line->stc_qfcnt = 0;
			mutex_enter(stc->stc_mutex);
			FLUSHSILO(line);	/* flush the rcv silo */
			mutex_exit(stc->stc_mutex);
		} else {
			/*
			 * post a timer so that we can try again later
			 */
			mutex_enter(stc->stc_mutex);
			line->state |= STC_CANWAIT;
			mutex_exit(stc->stc_mutex);
			line->stc_draintimeout_id =
				timeout(stc_draintimeout, line, QFRETRYTIME);
			line->stc_stats.drain_timer++;
		}
		goto drain_o;
	    } /* if (canputnext(q)) */
	} /* while */

	SDM(STC_TRACK, (CE_CONT,
		"-----(unit %d, line %d)------(LEAVING stc_drainsilo)-----\n",
		line->unit_no, line->line_no));

drain_o:
	return;

}

/*
 *	do a copy from ring queue to buffer using bcopy(),
 *	and update counts and pointers when done
 */
static void
silocopy(struct stc_line_t *line, uchar_t *buf, int count)
{
	register uchar_t *sink = line->stc_sink;
	register int cc = count;

	/*
	 * if the sink pointer is less than the source pointer,
	 * we haven't wrapped yet, so just bcopy and update
	 */
	if (sink < line->stc_source) {
		bcopy(sink, buf, cc);
	} else {
		cc = MIN(count, &line->stc_ssilo[line->stc_silosize] - sink);
		bcopy(sink, buf, cc);
		if (cc != count) {
			buf += cc;
			cc = count - cc;
			sink = line->stc_ssilo;
			bcopy(sink, buf, cc);
		}
	}
	line->stc_sink = (sink + cc);
	line->stc_sscnt -= count;
	/*
	 * check if we should diddle with RTS now that we've taken some
	 * data out of the silo
	 */
	CHECK_RTS(line);
}

/*
 * stc_open(q, dev, flag, sflag) - the open system call
 *
 */
static int
stc_open(queue_t *q, dev_t *dev, int oflag, int sflag, cred_t *credp)
{
	int unit_no, line_no;
	struct stc_line_t *line;
	struct stc_unit_t *stc;

#ifdef lint
	sflag = sflag;
#endif
	unit_no = STC_UNIT(*dev);
	line_no = STC_LINE(*dev);

	STC_LOGIT(line_no, 0, 0x6f70, q);
	SDM(STC_TRACK, (CE_CONT, "stc_open: unit %d line %d\n",
		unit_no, line_no));
	SDM(STC_TRACK, (CE_CONT,
		"stc_open: getminor(0x%x) 0x%x getmajor(0x%x) 0x%x\n",
		*dev, getminor(*dev), *dev, getmajor(*dev)));

	/*
	 * Check for how many are allocated. At this point
	 * nstc must be set.
	 */
	if ((unit_no >= nstc) || (line_no >= STC_LINES))
		return (ENXIO);

	if ((stc = ddi_get_soft_state(stc_soft_state_p, unit_no)) == NULL)
		return (EFAULT);

	/*
	 * determine what type of line we're trying to open; we can open
	 * either one of the serial lines (for both incoming and outgoing
	 * operation), a parallel line or the board's control line
	 * the control line is used mainly for ioctl()'s to the other lines
	 * and to do things like flush the queues for a given line
	 * if the user tries to open the control line and they're not
	 * superuser, then return an open error
	 * set STC_DRAIN for the control line so that all writes go into the
	 * ozone if the control line is opened when the board didn't pass
	 * stc_attach() OK,
	 * return ECHILD; this is mostly so that stc_config can make the
	 * correct number of devices since it tries to cat /dev/null to each
	 * control line and checks for an error to determine whether or not the
	 * board is physically present.	 by generating this error on the control
	 * line here even if the board didn't make it through stc_attach() we
	 * take care of the case where we have a failed board before a good
	 * board (this relly stinks...)
	 */
	if (STC_CONTROL_LINE(*dev)) {
	    if (!drv_priv(credp)) {
		if (stc->flags & STC_ATTACHOK) {
		    line = &stc->control_line;
		    line->state |= (STC_CONTROL | STC_DRAIN);
		} else {
		    return (ECHILD);
		}
	    } else {
		return (EPERM);
	    }
	} else if (STC_LP_LINE(*dev)) {
	    if ((line_no >= PPC_LINES) || !(stc->flags & STC_ATTACHOK)) {
		return (ENXIO);
	    }
	    line = &stc->ppc_line;
	    line->state |= STC_LP;
	} else {
	    if (!(stc->flags & STC_ATTACHOK)) {
		return (ENXIO);
	    } else {
		line = &stc->line[line_no];
	    }
	}

	/*
	 * protect the line struct
	 */
	mutex_enter(line->line_mutex);

	/*
	 * if we're inhibited because the user wants to unload the driver,
	 * we won't * allow any more opens except for the control port
	 * also come here if we fall out of the sleep() due to a wakeup()
	 * in close() or stc_modem()
	 */
again:
	if ((line->state & STC_OPEN_INH) && !(line->state & STC_CONTROL)) {
		mutex_exit(line->line_mutex);
		return (EBUSY);
	}

	/*
	 * check to see if we're already open.	If so, check for exclusivity,
	 * if not, set up our data and enable the line.
	 * most of this stuff makes no sense if we're the control line, but
	 * it's easier to go through the generic case than to have a bunch
	 * of conditionals all around here
	 */
	if (!(line->state & STC_ISOPEN)) {
		/*
		 * this is for stc_ioctl() **** LOOK yeech! ****
		 */
		if (!drv_priv(credp))
		    line->state |= STC_ISROOT;

		line->stc_ttycommon.t_iflag =
			line->default_param.sd.termios.c_iflag;
		line->stc_ttycommon.t_cflag =
			line->default_param.sd.termios.c_cflag;
		line->stc_ttycommon.t_stopc =
			line->default_param.sd.termios.c_cc[VSTOP];
		line->stc_ttycommon.t_startc =
			line->default_param.sd.termios.c_cc[VSTART];
		line->stc_ttycommon.t_iocpending = NULL;
		line->stc_ttycommon.t_size.ws_row = 0;
		line->stc_ttycommon.t_size.ws_col = 0;
		line->stc_ttycommon.t_size.ws_xpixel = 0;
		line->stc_ttycommon.t_size.ws_ypixel = 0;
		line->stc_flowc = 0;	/* this will get set in stc_wput() */
		/* these next two are duplicates of what stc_init() does */
		line->unit_no = unit_no;
		line->line_no = line_no;
		line->stc_source = line->stc_sink = line->stc_ssilo;
		line->stc_sscnt = line->stc_qfcnt = 0;
		line->stc_wbufcid = 0;
		line->flags = line->default_param.sd.flags;

		/*
		 * the parallel port
		 */
		if (line->state & STC_LP) {
			/*
			 * disable draining on a fresh open
			 */
			line->state &= ~STC_DRAIN;
			/*
			 * set up various counters and state
			 */
			line->busy_cnt = 0;
			line->pstate = 0;
			line->ack_loops = 0;
			line->error_cnt = 0;
			/*
			 * set up the interface timing parameters
			 */
			line->strobe_w = line->default_param.sd.strobe_w;
			line->data_setup = line->default_param.sd.data_setup;
			/*
			 * set up the various timeouts
			 */
			line->ack_maxloops = line->default_param.ack_maxloops;
			line->ack_timeout = line->default_param.sd.ack_timeout;
			line->error_timeout =
				line->default_param.sd.error_timeout;
			line->busy_timeout =
				line->default_param.sd.busy_timeout;
			/*
			 * initialize the ppc and enable ppc interrupts
			 */
			mutex_enter(stc->ppc_mutex);
			line->pcon_s =	line->default_param.pcon;
			stc->ppc->pcon = line->pcon_s;
			mutex_exit(stc->ppc_mutex);
			/*
			 * turn this on just in case...
			 */
			line->stc_ttycommon.t_flags |= TS_SOFTCAR;
			/*
			 * check the printer status
			 * the user will get a PP_SIGTYPE signal if there's an
			 * error condition and they've set the PP_SIGNAL bit
			 * in the line->flags field of the line struct
			 * ppc_stat() is protected by our line_mutex
			 */
			(void) ppc_stat(stc->ppc, line);
		/*
		 * the control line
		 */
		} else if (line->state & STC_CONTROL) {
			line->flags |= SOFT_CARR;
			line->stc_ttycommon.t_flags |= TS_SOFTCAR;
		/*
		 * a serial line
		 */
		} else {
			/*
			 * disable draining on a fresh open
			 */
			line->state &= ~STC_DRAIN;
			/*
			 * set up some parameters for the Rx interrupt
			 * handler code
			 */
			line->drain_size = line->default_param.sd.drain_size;
			line->stc_hiwater = line->default_param.sd.stc_hiwater;
			line->stc_lowwater =
				line->default_param.sd.stc_lowwater;
			/*
			 * make sure this line's default flags track TS_SOFTCAR
			 * in the tty struct also
			 */
			if (line->stc_ttycommon.t_flags & TS_SOFTCAR) {
				line->default_param.sd.flags |= SOFT_CARR;
				line->flags |= SOFT_CARR;
			}
			/*
			 * now set up the cd180 line to default parameters
			 * (perhaps passed in by modload) - enable the
			 * transmitter and receiver and modem control
			 * interrupts because cd180_init() disables them all
			 */
			(void) cd180_init(stc, line_no);
			mutex_enter(stc->stc_mutex);
			stc_param(line);
			stc->regs->car = (uchar_t)line_no;
			SET_CCR(stc->regs->ccr,
				(CHAN_CTL | XMTR_ENA | RCVR_ENA));
			stc->regs->ier = (RX_DATA | CD_INT);
			mutex_exit(stc->stc_mutex);
		}
	/* only one open at a time, unless we're root */
	} else if ((line->state & STC_LP) && (drv_priv(credp))) {
		mutex_exit(line->line_mutex);
		return (EBUSY);
	} else if ((line->stc_ttycommon.t_flags & TS_XCLUDE) &&
		(drv_priv(credp))) {
		mutex_exit(line->line_mutex);
		return (EBUSY);		/* already opened exclusively */
	} else if ((STC_OUTLINE(*dev)) && !(line->state & STC_OUT)) {
		mutex_exit(line->line_mutex);
		return (EBUSY);		/* already open but not for dial-out */
	}

	/*
	 * if we're the parallel port or the control port, there's no carrier
	 * so wait for, so just blast on through
	 */
	if (!(line->state & (STC_LP | STC_CONTROL))) {
		/*
		 * play carrier games
		 * note that since the only way we can get here is if this
		 *	is a serial port open, we're still covered by the
		 *	SPL_STC() issued up above so I commented out the
		 *	folowing line that was in the V1.0 release
		 *		s = SPL_STC(unit_no);
		 */
		mutex_enter(stc->stc_mutex);
		stc->regs->car = (uchar_t)line->line_no;
		if (line->flags & (DTR_ASSERT|STC_DTRFORCE)) {
			/*
			 * assert DTR on open
			 */
			DTR_SET(line, DTR_ON);
		}
		/*
		 * assert RTS - most of the time, this will be asserted
		 * by stc_param(), above, unless the user has set the default
		 * for this line to be CRTSCTS, in which case RTS could never
		 * be set, since stc_param() depends on PUTSILO() to diddle
		 * with RTS, and PUTSILO() only gets called at receive interrupt
		 * time, and if a device is waiting for RTS to be asserted
		 * before they start sending us data, we'll get into a
		 * deadlock situation, so raise RTS here (this has no nasty
		 * side effects), assuming an empty receive silo (a valid
		 * assumption), and let PUTSILO() deal with it if the user
		 * wants CTS/RTS flow control
		 */
		stc->regs->msvr |= RTS_ON;
		if (STC_OUTLINE(*dev)) {
			line->state |= (STC_CARR_ON | STC_OUT);
		} else if ((line->stc_ttycommon.t_flags & TS_SOFTCAR) ||
			(stc->regs->msvr & CD_ON)) {
			line->state |= STC_CARR_ON;
		}
		mutex_exit(stc->stc_mutex);

		/*
		 * Sleep here if:
		 *  - opened with blocking and we're paying attention to modem
		 *	lines (CLOCAL not set) and there's no carrier
		 *  - we're dial IN and the device has been opened to dial OUT,
		 *	stay asleep even though carrier is on.
		 */
		if (!(oflag & (FNDELAY | FNONBLOCK)) &&
			!(line->stc_ttycommon.t_cflag & CLOCAL)) {
		    if (!(line->state & STC_CARR_ON) || (line->state&STC_OUT &&
			!(STC_OUTLINE(*dev)))) {
			mutex_enter(stc->stc_mutex);
			line->state |= STC_WOPEN;
			mutex_exit(stc->stc_mutex);
			/*
			 * there are 3 reasons that we can return from the sleep
			 *  - if we get an interrupted sys call, and the line
			 * has not been opened by someone else, turn off all
			 * interrupts and return OPENFAIL; also, drop DTR
			 * and RTS; we sleep on lbolt before retunring so
			 * that an external device has time to recognize
			 * to modem signal transitions
			 *  - we were woken up in close() by the outgoing line's
			 * close, so go back up and restore all the line
			 * and h/w parameters that the close zapped to defaults
			 *  - CD occured, so just proceed normally
			 */
			if (!cv_wait_sig(line->cvp, line->line_mutex)) {
				mutex_enter(stc->stc_mutex);
				line->state &= ~STC_WOPEN;
				mutex_exit(stc->stc_mutex);
				if (!(line->state & STC_ISOPEN)) {
					mutex_enter(stc->stc_mutex);
					stc->regs->car = (uchar_t)line->line_no;
					stc->regs->ier = 0;
					DTR_SET(line, DTR_OFF);
					stc->regs->msvr &= ~RTS_ON;
					mutex_exit(stc->stc_mutex);
					/* Timeplex hack below */
					cv_wait(&lbolt_cv, line->line_mutex);
				}
				mutex_exit(line->line_mutex);
				return (EINTR);
			/*
			 * we got here because stc_close() did a wakeup() on
			 * us or the CD line changed state; either way, this
			 * handles the case where we were sleeping waiting for
			 * CD on an incoming line, and either the incoming
			 * line asserted CD after the (modem) answered the
			 * phone and established a connection or we were using
			 * this line as an outgoing line (i.e. via tip) in
			 * either case, clear the line state (but leave the open
			 * inhibit flag alone in case the user wanted to
			 * unload the driver), and reinitialize the line,
			 * because we don't know what nasty things the
			 * outgoing line process could have done to our
			 * parameters
			 */
			} else {	/* woken up by stc_close() or CD line */
				goto again;
			}
		    }
		} else {
			if (!(STC_OUTLINE(*dev)) && (line->state & STC_OUT)) {
			    mutex_exit(line->line_mutex);
				/*
				 * already opened but not for dial-in
				 */
			    return (EBUSY);
			}
		}
	} /* !STC_LP */

	/*
	 * now set up the streams queues
	 */
	line->stc_ttycommon.t_readq = q;
	line->stc_ttycommon.t_writeq = WR(q);
	q->q_ptr = (caddr_t)line;
	WR(q)->q_ptr = (caddr_t)line;

	/*
	 * start queue processing and mark the line as finished with the open
	 */
	mutex_enter(stc->stc_mutex);
	line->state &= ~STC_WOPEN;
	line->state |= STC_ISOPEN;
	line->state |= STC_INSOFTINT;	/* Added to keep the softint off */
	mutex_exit(stc->stc_mutex);
	mutex_exit(line->line_mutex);
	qprocson(q);

	mutex_enter(line->line_mutex);
	mutex_enter(stc->stc_mutex);
	line->state &= ~STC_INSOFTINT;	/* Allow the softint now */
	mutex_exit(stc->stc_mutex);
	mutex_exit(line->line_mutex);

	return (0);
}

/*
 * your basic close() routine
 */
/*ARGSUSED1*/
static int
stc_close(queue_t *q, int flag, cred_t *credp)
{
	struct stc_line_t *line;
	struct stc_unit_t *stc;
	int setdtr = 0;
	timeout_id_t stc_draintimeout_id = 0;
	bufcall_id_t stc_wbufcid = 0;

	/*
	 * get the pointer this this queue's line struct; if it's NULL, then
	 * we're already closed (but we should never see this)
	 */
	if ((line = (struct stc_line_t *)q->q_ptr) == NULL) {
		cmn_err(CE_CONT, "stc_close: unit %d line %d already closed\n",
					line->unit_no, line->line_no);
		return (0);
	}

	STC_LOGIT(line->line_no, 0, 0x636c, q);
	SDM(STC_TRACK, (CE_CONT,
		"stc_close: unit %d line %d\n", line->unit_no, line->line_no));

	/*
	 * get a pointer to the unit structure
	 */
	stc = line->stc;

	/*
	 **** LOOK yeech!! ****
	 */
	mutex_enter(stc->stc_mutex);
	line->state &= ~STC_ISROOT;
	mutex_exit(stc->stc_mutex);

	/*
	 * if we're the control line, there's no real device attached to us,
	 * so there's nothing really to close
	 *
	 * ignore the open inhibit flag, since stc_open() ignores it for a
	 * control line open
	 */
	if (line->state & STC_CONTROL) {
		/*
		 * disable the line and free the queues
		 */
		mutex_enter(line->line_mutex);
		ttycommon_close(&line->stc_ttycommon);
		/*
		 * cancel any outstanding bufcall() request
		 */
		if (line->stc_wbufcid) {
			stc_wbufcid = line->stc_wbufcid;
			line->stc_wbufcid = 0;
		}
		line->state = 0;
		qprocsoff(q);
		q->q_ptr = WR(q)->q_ptr = NULL;
		mutex_exit(line->line_mutex);
		if (stc_wbufcid)
			unbufcall(stc_wbufcid);
		return (0);
	}

	/*
	 * serial port close code here
	 */
	if (!(line->state & STC_LP)) {
		register struct stcregs_t *regs = stc->regs;
		int softint_waitcount = 0;

		mutex_enter(line->line_mutex);

		/*
		 * wait for the transmitter to drain
		 */
		stc_xwait(line);

		while ((softint_waitcount++ < 5) &&
			(line->state & STC_INSOFTINT)) {
			mutex_exit(line->line_mutex);
			delay(drv_usectohz(100000));
			mutex_enter(line->line_mutex);
		}
		if (line->state & STC_INSOFTINT) {
			cmn_err(CE_CONT,
			"stc_close: ignoring softint on line %d %p\n",
			line->line_no, (void *)line);
		}

		qprocsoff(q);
		/*
		 * if we've got a pending timeout from stc_drainsilo(),
		 * remove it
		 */

		if (line->stc_draintimeout_id) {
			stc_draintimeout_id = line->stc_draintimeout_id;
			line->stc_draintimeout_id = 0;
		}

		/*
		 * clear any break condition, and drop DTR if HUPCL or
		 * nobody listening
		 *
		 * if we're holding DTR high via soft carrier, raise it
		 * an interesting point here about DTR and the STC_DTRCLOSE
		 * flag - even though the semantics for DTR specify that
		 * DTR be held high after the close if TS_SOFTCAR is set,
		 * the zs/alm/mcp drivers drop it no matter what the
		 * state of TS_SOFTCAR (see sundev/zs_async.c in zsclose()
		 * and zsmctl()), so we provide a flag that when CLEAR (the
		 * default case), we implement the same semantics for DTR
		 * on close() as the zs/alm/mcp drivers do - if the STC_DTRCLOSE
		 * is SET, then TS_SOFTCAR will govern the state of DTR after
		 * the close() here is done
		 */
		mutex_enter(stc->stc_mutex);
		if ((line->stc_ttycommon.t_cflag & HUPCL) ||
			(line->state & STC_WOPEN) ||
			!(line->state & STC_ISOPEN)) {
			if ((line->stc_ttycommon.t_flags & TS_SOFTCAR) &&
				(line->flags & STC_DTRCLOSE)) {
				DTR_SET(line, DTR_ON);
			} else {
				DTR_SET(line, DTR_OFF);
			}
			setdtr = 1;
		}

		/*
		 * disable cd180 interrupts, receiver and transmitter
		 * drop the RTS line - if we weren't using CTS/RTS flow control,
		 * this won't have any effect, and if we were, it's the right
		 * thing to do here
		 */
		regs->car = (uchar_t)line->line_no;
		regs->ier = 0;
		regs->msvr &= ~RTS_ON;
		stc_txflush(line);
		SET_CCR(regs->ccr, (CHAN_CTL | RCVR_DIS | XMTR_DIS));
		mutex_exit(stc->stc_mutex);

		/*
		 * disable the line and free the queues
		 */
		ttycommon_close(&line->stc_ttycommon);

		/*
		 * cancel any outstanding bufcall() request
		 */
		if (line->stc_wbufcid) {
			stc_wbufcid = line->stc_wbufcid;
			line->stc_wbufcid = 0;
		}
		/*
		 * give the modem control lines some time to settle down
		 * so that the external device will have enough time to
		 * notice the DTR (and maybe RTS) transition
		 * only do this if we're waiting in stc_open()
		 */
		if (setdtr) {
			mutex_exit(line->line_mutex);
			delay(drv_usectohz(stc_dtrdrop));
			mutex_enter(line->line_mutex);
		}
		/*
		 * set STC_WCLOSE if we're sleeping in open
		 * save the state of the open inhibit flag
		 */
		mutex_enter(stc->stc_mutex);
		if (line->state & STC_WOPEN)
			line->state =
				((line->state & STC_OPEN_INH) | STC_WCLOSE);
		else
			line->state = (line->state & STC_OPEN_INH);
		mutex_exit(stc->stc_mutex);

		/*
		 * clean up the queue pointers and signal any potential
		 *	sleepers in stc_open()
		 */
		cv_broadcast(line->cvp);
		q->q_ptr = WR(q)->q_ptr = NULL;
		line->stc_ttycommon.t_readq = NULL;
		line->stc_ttycommon.t_writeq = NULL;
		mutex_exit(line->line_mutex);
	} else {
		/*
		 * the parallel port close code here
		 */
		register struct ppcregs_t *regs = stc->ppc;

		/*
		 * remove any pending timers from the timeout queue so that
		 * they don't pop up after we're closed
		 */
		UNTIMEOUT(line->ppc_acktimeout_id); /* ACK handshake timer */
		UNTIMEOUT(line->ppc_out_id);	/* BUSY wait timer */

		/*
		 * wait for the driver's local ppc buffer to drain
		 */
		ppc_xwait(line);
		/*
		 * disable the ppc interrupt and free the queues
		 * read the ppc's status register in IACK space to
		 * clear any pending interrupt
		 */
		mutex_enter(line->line_mutex);
		mutex_enter(stc->ppc_mutex);
		line->pcon_s &= ~PPC_IRQE;
		regs->pcon = line->pcon_s;
		regs->pwierd = regs->ipstat;
		mutex_exit(stc->ppc_mutex);

		ttycommon_close(&line->stc_ttycommon);
		/*
		 * cancel any outstanding bufcall() request
		 */
		if (line->stc_wbufcid) {
			stc_wbufcid = line->stc_wbufcid;
			line->stc_wbufcid = 0;
		}
		/*
		 * save the state of the open inhibit flag
		 */
		line->state = (line->state & STC_OPEN_INH);
		qprocsoff(q);
		q->q_ptr = WR(q)->q_ptr = NULL;
		mutex_exit(line->line_mutex);
	}

	if (stc_wbufcid)
		unbufcall(stc_wbufcid);

	if (stc_draintimeout_id)
		(void) untimeout(stc_draintimeout_id);

	SDM(STC_TRACK, (CE_CONT, "Leaving stc_close, unit %d, line %d\n",
				line->unit_no, line->line_no));
	return (0);
}

/*
 * Send a negative acknowledgement for the ioctl denoted by mp through the
 * queue q, specifying the error code err.
 *
 * This routine could be a macro or in-lined, except that space is more
 * critical than time in error cases.
 */
static void
stc_sendnak(q, mp, err)
	queue_t *q;
	mblk_t	*mp;
	int	err;
{
	register struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCNAK;
	iocp->ioc_count = 0;
	iocp->ioc_error = err;
	qreply(q, mp);
}

/*
 * Convert the M_IOCTL or M_IOCDATA mesage denoted by mp into an M_IOCACK.
 * Free any data associated with the message and replace it with dp if dp is
 * non-NULL, adjusting dp's write pointer to match size.
 */
static void
stc_ack(mblk_t *mp, mblk_t *dp, uint size)
{
	register struct iocblk	*iocp = (struct iocblk *)mp->b_rptr;

	SDM(STC_TRACK, (CE_CONT, "stc_ack: mp = 0x%x dp = 0x%x size = 0x%x\n",
					mp, dp, size));

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;
	if (mp->b_cont != NULL) {
		/* freeb(mp->b_cont); */
		freemsg(mp->b_cont);
	}
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
}

/*
 * Convert mp to an M_COPYIN or M_COPYOUT message (as specified by type)
 * requesting size bytes.  Assumes mp denotes a TRANSPARENT M_IOCTL or
 * M_IOCDATA message.  If dp is non-NULL, it is assumed to point to data to be
 * copied out and is linked onto mp.  cmd is passed so that when we get an
 * M_IOCDATA message, we know what it's for.
 */
static void
stc_mcopy(mblk_t *mp, mblk_t *dp, uint size, unsigned char type,
    void *cmd, void *useraddr)
{
	register struct copyreq *cp = (struct copyreq *)mp->b_rptr;

	SDM(STC_TRACK, (CE_CONT,
"stc_mcopy: mp:0x%x dp:0x%x size:0x%x type:0x%x cmd:0x%x useraddr:0x%x\n",
		mp, dp, size, type, cmd, useraddr));

	cp->cq_private = (mblk_t *)cmd;
	cp->cq_flag = 0;
	cp->cq_size = size;
	if (!useraddr && (type == M_COPYIN))
	    cp->cq_addr = (caddr_t)(*(long *)(mp->b_cont->b_rptr));
	else
	    cp->cq_addr = (caddr_t)useraddr;

	if (mp->b_cont != NULL)
		freemsg(mp->b_cont);
	if (dp != NULL) {
		mp->b_cont = dp;
		dp->b_wptr += size;
	} else
		mp->b_cont = NULL;
	mp->b_datap->db_type = type;
	mp->b_wptr = mp->b_rptr + sizeof (struct copyreq);
}

/*
 * stc_wput(q, mp) - Write side put procedure
 * All outgoing traffic goes through this routine - ioctls and data.
 * we silently discard any messages for the control line that aren't
 * M_IOCTL messages
 */
static int
stc_wput(queue_t *q, mblk_t *mp)
{
	register struct stc_line_t *line = (struct stc_line_t *)q->q_ptr;
	register struct stc_unit_t *stc = line->stc;
	struct stc_state_t *stp;
	struct copyresp *csp;
	int size;

	SDM(STC_TRACK, (CE_CONT,
	"---------(stc%d, line%d)---(stc_wput)--(msg type 0x%x)---------- \n",
		line->unit_no, line->line_no, mp->b_datap->db_type));

	switch (mp->b_datap->db_type) {
	    case M_DATA:
	    case M_DELAY:
		if (line->state & STC_CONTROL) {
			freemsg(mp);
			break;
		}
		/*
		 * put the message on the queue and start it off. stc_start()
		 * does the idle check
		 */
		(void) putq(q, mp);
		mutex_enter(line->line_mutex);
		stc_start(line);
		mutex_exit(line->line_mutex);
		break;
	    case M_BREAK:
		freemsg(mp);
		break;
		/*
		 * note that we really should have a general way of handling
		 * transparent and non-transparent ioctls here and shouldn't
		 * have to special-case all of them here
		 */
	    case M_IOCTL:

#ifdef	DEBUG_STCIOCTL
		ioctl2text("stc_wput",
			(((struct iocblk *)mp->b_rptr)->ioc_cmd));
		if (mp->b_cont == NULL)
		    cmn_err(CE_CONT, "stc_wput: no b_cont on M_IOCTL\n");
#endif	DEBUG_STCIOCTL

		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {
			/*
			 *** LOOK - see if there are any more we need to
			 * deal with here ***
			 */
			case TCSETS:
				stc_ioctl(q, mp);
				break;
			/*
			 * These changes happen in band - when all output that
			 * preceded them has drained - we call stc_start()
			 * just in case we're idle
			 */
			case TCSBRK:
			case TIOCSSOFTCAR:
			case TCSETSW:
			case TCSETSF:
			case TCSETAW:
			case TCSETAF:
			case TIOCSBRK:
			case TIOCCBRK:
				(void) putq(q, mp);
				mutex_enter(line->line_mutex);
				stc_start(line);
				mutex_exit(line->line_mutex);
				break;
			/*
			 * handle the driver-specific ioctl()'s here
			 */
			case STC_SREGS:
			case STC_GREGS:
			case STC_SPPC:
			case STC_GPPC:
			case STC_GSTATS:
			/*
			 * handle some of the termiox stuff here
			 */
			case TCSETX:
			case TCSETXW:
			case TCSETXF:
			case TIOCMSET:
			case TIOCMBIS:
			case TIOCMBIC:
			case TIOCMGET:
			    if (((struct iocblk *)mp->b_rptr)->ioc_count
				!= TRANSPARENT) {
				(void) putq(q, mp);
				mutex_enter(line->line_mutex);
				stc_start(line);
				mutex_exit(line->line_mutex);
			    } else {
				switch (((struct iocblk *)
				    mp->b_rptr)->ioc_cmd) {
				    case TCSETX:
				    case TCSETXW:
				    case TCSETXF:
					size = sizeof (struct termiox);
					break;
				    case STC_SREGS:
				    case STC_GREGS:
					size = sizeof (struct stc_diagregs_t);
					break;
				    case STC_GPPC:
				    case STC_SPPC:
					size = sizeof (struct ppc_params_t);
					break;
				    case STC_GSTATS:
					size = sizeof (struct stc_stats_t);
					break;
				    case TIOCMSET:
				    case TIOCMBIS:
				    case TIOCMBIC:
				    case TIOCMGET:
					size = sizeof (int);
					break;
				}
				stp =
				    (struct stc_state_t *)kmem_zalloc(
				    sizeof (struct stc_state_t), KM_NOSLEEP);
				if (stp == NULL)
				    stc_sendnak(q, mp, ENOMEM);
				else {
				    stp->state = STC_COPYIN;
				    stp->addr = (caddr_t)(*(long *)
					(mp->b_cont->b_rptr));
				    stc_mcopy(mp, NULL, size, M_COPYIN, stp,
					NULL);
				    qreply(q, mp);
				}
			    }
			    break;
			/*
			 * We don't understand this ioctl(), for now, just pass
			 * it along to stc_ioctl() where it will either live
			 * long and prosper, or die a firey death
			 */
			default:
			    SDM(STC_TRACK, (CE_CONT,
				    "stc_wput: M_IOCTL (0x%x) default case\n",
				    (((struct iocblk *)mp->b_rptr)->ioc_cmd)));
#ifdef	DEBUG_STCIOCTL
			    if (((struct iocblk *)mp->b_rptr)->ioc_count !=
				TRANSPARENT) {
				ioctl2text("stc_wput (!TRANSPARENT)",
					(((struct iocblk *)mp->b_rptr)->
						ioc_cmd));
			    } else {
				ioctl2text("stc_wput (TRANSPARENT)",
					(((struct iocblk *)mp->b_rptr)->
					ioc_cmd));
			    }
#endif	DEBUG_STCIOCTL
			    stc_ioctl(q, mp);
			    break;
		} /* switch (struct iocblk *) */
mioctl_out:
		break;
		/*
		 * do all M_IOCDATA processing in stc_ioctl()
		 */
	    case M_IOCDATA:

#ifdef	DEBUG_STCIOCTL
		if (mp->b_cont == NULL)
		    cmn_err(CE_CONT, "stc_wput: no b_cont on M_IODATA\n");
#endif	DEBUG_STCIOCTL

		SDM(STC_TRACK, (CE_CONT, "stc_wput: M_IOCDATA\n"));
		csp = (struct copyresp *)mp->b_rptr;
		stp = (struct stc_state_t *)csp->cp_private;
		switch (stp->state) {
		    case STC_COPYOUT:
			SDM(STC_TRACK, (CE_CONT, "stc_wput: STC_COPYOUT\n"));
			/*
			 * free the state struct - we don't need it anymore
			 */
			if (stp)
			    kmem_free(stp, sizeof (struct stc_state_t));
			if (csp->cp_rval) {
			    SDM(STC_TRACK, (CE_CONT,
				"stc_wput: STC_COPYOUT fails\n"));
			    freemsg(mp);
			    return (0);
			}
			stc_ack(mp, NULL, 0);
			qreply(q, mp);
			break;
		    case STC_COPYIN:
			SDM(STC_TRACK, (CE_CONT, "stc_wput: STC_COPYIN\n"));
		    default:
			SDM(STC_TRACK, (CE_CONT,
				"stc_wput: calling stc_ioctl(M_IOCDATA)\n"));
			if (csp->cp_rval) {
				SDM(STC_TRACK, (CE_CONT,
					"stc_wput: STC_COPYIN fails\n"));
				if (stp)
				    kmem_free(stp, sizeof (struct stc_state_t));
				freemsg(mp);
				return (0);
			}
			(void) putq(q, mp);
			mutex_enter(line->line_mutex);
			stc_start(line);
			mutex_exit(line->line_mutex);
		}
		break;
	    case M_FLUSH:
		/*
		 * we can't stop any transmission in progress, but we can
		 * inhibit further xmits while flushing.
		 * FLUSHALL was FLUSHDATA
		 */
		if (*mp->b_rptr & FLUSHW) {
			STC_LOGIT(line->line_no, 0, 0x7770, line->state);
			mutex_enter(line->line_mutex);
			mutex_enter(stc->stc_mutex);
			line->state &= ~STC_BUSY;
			line->state |= STC_FLUSH;
			mutex_exit(stc->stc_mutex);
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			*mp->b_rptr &= ~FLUSHW;
			mutex_enter(stc->stc_mutex);
			line->state &= ~STC_FLUSH;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
		}
		if (*mp->b_rptr & FLUSHR) {
			flushq(RD(q), FLUSHDATA);
			qreply(q, mp);	/* give the read queues a shot */
		} else {
			freemsg(mp);
		}
		/*
		 * We must make sure we process messages that survive the
		 * write-side flush.  Without this call, the close protocol
		 * with ldterm can hang forever.  (ldterm will have sent us a
		 * TCSBRK ioctl that it expects a response to.)
		 */
		mutex_enter(line->line_mutex);
		stc_start(line);
		mutex_exit(line->line_mutex);
		break;
		/*
		 * If we are supposed to stop, the best we can do is not xmit
		 * anymore (the current xmit will go).  When we are told to
		 * start, stc_start() sees if it has work to do (or is in
		 * progress).
		 * If this is a serial line and the cd180 flow control modes are
		 * enabled, the cd180 will take care of starting and stopping
		 * transmission.
		 * since the ppc has only a 1-byte fifo, we can stop it at the
		 * next character
		 */
	    case M_STOP:
		mutex_enter(line->line_mutex);
		mutex_enter(stc->stc_mutex);
		line->state |= STC_STOPPED;
		mutex_exit(stc->stc_mutex);
		mutex_exit(line->line_mutex);
		freemsg(mp);
		break;
	    case M_START:
		if (line->state & STC_STOPPED) {
		    mutex_enter(line->line_mutex);
		    mutex_enter(stc->stc_mutex);
		    line->state &= ~STC_STOPPED;
		    mutex_exit(stc->stc_mutex);
		    stc_start(line);
		    mutex_exit(line->line_mutex);
		}
		freemsg(mp);
		break;
		/*
		 * stop and start input - send flow control
		 * this only works for the serial lines
		 */
	    case M_STOPI:
		if (!(line->state & (STC_LP | STC_CONTROL))) {
		    mutex_enter(line->line_mutex);
		    mutex_enter(stc->stc_mutex);
		    stc->regs->car = (uchar_t)line->line_no;
		    stc->regs->schr2 = line->stc_ttycommon.t_stopc;
		    SET_CCR(stc->regs->ccr, SEND_SPEC2);
		    mutex_exit(stc->stc_mutex);
		    mutex_exit(line->line_mutex);
		} else {
			/*EMPTY*/ /* if DEBUG_STC not set */
		    SDM(STC_DEBUG, (CE_CONT,
	    "stc_wput: unit %d trying to M_STOPI on ppc or control device\n",
			line->unit_no));
		}
		freemsg(mp);
		break;
	    case M_STARTI:
		if (!(line->state & (STC_LP | STC_CONTROL))) {
		    mutex_enter(line->line_mutex);
		    mutex_enter(stc->stc_mutex);
		    stc->regs->car = (uchar_t)line->line_no;
		    stc->regs->schr1 = line->stc_ttycommon.t_startc;
		    SET_CCR(stc->regs->ccr, SEND_SPEC1);
		    mutex_exit(stc->stc_mutex);
		    mutex_exit(line->line_mutex);
		} else {
			/*EMPTY*/ /* if DEBUG_STC not set */
		    SDM(STC_DEBUG, (CE_CONT,
	"stc_wput: unit %d trying to M_STARTI on ppc or control device\n",
			line->unit_no));
		}
		freemsg(mp);
		break;
	    case M_CTL:
		switch (*mp->b_rptr) {
		    case MC_SERVICEIMM:
			SDM(STC_DEBUG, (CE_CONT,
		"stc_wput: M_CTL(MC_SERVICEIMM) message not supported\n"));
			break;
		    case MC_SERVICEDEF:
			SDM(STC_DEBUG, (CE_CONT,
		"stc_wput: M_CTL(MC_SERVICEDEF) message not supported\n"));
			break;
		    default:
			break;
		}
		freemsg(mp);
		break;
	    default:
		SDM(STC_DEBUG, (CE_CONT,
		"stc_wput: unit %d line %d unknown message: 0x%x\n",
		line->unit_no, line->line_no, mp->b_datap->db_type));
		freemsg(mp);
		break;
	}

	return (0);
}

/*
 * stc_reioctl(line) - retry an ioctl.
 * Called when ttycommon_ioctl fails due to an allocb() failure
 */
static void
stc_reioctl(void *arg)
{
	struct stc_line_t *line = arg;
	queue_t *q;
	mblk_t *mp;

	SDM(STC_TRACK, (CE_CONT,
		"stc_reioctl: unit %d line %d\n",
		line->unit_no, line->line_no));

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(line->line_mutex);
	if (line->stc_wbufcid == 0) {
	    mutex_exit(line->line_mutex);
	    return;
	}
	line->stc_wbufcid = 0;
	if ((q = line->stc_ttycommon.t_writeq) == NULL) {
	    mutex_exit(line->line_mutex);
	    return;
	}
	if ((mp = line->stc_ttycommon.t_iocpending) != NULL) {
	    /* not pending any more */
	    line->stc_ttycommon.t_iocpending = NULL;
	    mutex_exit(line->line_mutex);
	    stc_ioctl(q, mp);
	} else {
	    mutex_exit(line->line_mutex);
	}
}

/*
 * stc_ioctl(q, mp) - Process an "ioctl" message sent down to us.
 */
static void
stc_ioctl(queue_t *q, mblk_t *mp)
{
	register struct stc_line_t *line = (struct stc_line_t *)q->q_ptr;
	register struct stc_unit_t *stc = line->stc;
	register struct stcregs_t *regs = stc->regs;
	register struct iocblk *iocp;
	register unsigned datasize;
	register int ioc_done = 0;
	struct stc_state_t *stp;
	struct copyresp *csp;
	int error = 0;
	unsigned int dblkhold = mp->b_datap->db_type;
	bufcall_id_t stc_wbufcid = 0;
	mblk_t *cp_private;

	STC_LOGIT(line->line_no, 0, 0x696f, q);
	SDM(STC_DEBUG, (CE_CONT,
		"---------(stc%d, line%d)---(stc_ioctl)---------- \n",
		line->unit_no, line->line_no));

#ifdef	DEBUG_STCIOCTL
	if (mp->b_cont == NULL)
	    cmn_err(CE_CONT, "stc_ioctl: no data block on message\n");
#endif	DEBUG_STCIOCTL

	mutex_enter(line->line_mutex);

	if (line->stc_ttycommon.t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(line->stc_ttycommon.t_iocpending);
		line->stc_ttycommon.t_iocpending = NULL;
	}

	/*
	 * XXXX for the LP64 kernel the call to ttycommon_ioctl() below
	 * with message type M_IOCDATA will clobber the field cp_private
	 * so the field cp_private needs to be saved and restored later.
	 * For ILP32 kernel the field cp_rval will be clobbered by
	 * ttycommon_iocl() but then this field is already checked for
	 * stc_wput for M_IOCDATA message type before it gets clobbered.
	 * The driver should be fixed in later release to not call
	 * ttycommon_ioctl for the ioctl cmd that ttycommon_ioctl does
	 * not recognise.
	 */
	if (dblkhold == M_IOCDATA) {
		csp = (struct copyresp *)mp->b_rptr;
		cp_private = csp->cp_private;
	}

	iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * ttycommon_ioctl sets up the data in stc_ttycommon for us.
	 * The only way in which "ttycommon_ioctl" can fail is if the "ioctl"
	 * requires a response containing data to be returned to the user,
	 * and no mblk could be allocated for the data.
	 * No such "ioctl" alters our state.  Thus, we always go ahead and
	 * do any state-changes the "ioctl" calls for.	If we couldn't allocate
	 * the data, "ttycommon_ioctl" has stashed the "ioctl" away safely, so
	 * we just call "bufcall" to request that we be called back when we
	 * stand a better chance of allocating the data.
	 */
	mutex_exit(line->line_mutex);
	datasize = ttycommon_ioctl(&line->stc_ttycommon, q, mp, &error);
	mutex_enter(line->line_mutex);
	if (datasize) {
	    SDM(STC_DEBUG, (CE_CONT,
		"stc_ioctl: returning datasize 0x%x unit %d line %d\n",
		datasize, line->unit_no, line->line_no));
	    if (line->stc_wbufcid) {
		stc_wbufcid = line->stc_wbufcid;
		line->stc_wbufcid = 0;
	    }
	    if (!(line->stc_wbufcid = bufcall(datasize, BPRI_HI,
		stc_reioctl, line))) {
		cmn_err(CE_CONT,
	"stc_ioctl: unit %d line %d can't allocate streams buffer for ioctl\n",
			line->unit_no, line->line_no);
		mutex_exit(line->line_mutex);
		stc_sendnak(q, mp, error);
	    } else
		mutex_exit(line->line_mutex);
	    if (stc_wbufcid)
		unbufcall(stc_wbufcid);
	    return;
	}
	if (error == 0) {
	/*
	 * "ttycommon_ioctl" did most of the work; we just use the data it
	 * set up.
	 */
	    switch (iocp->ioc_cmd) {
		case TCSETS:		/* set termios immediate */
		case TCSETSW:		/* set termios 'in band' */
		case TCSETSF:		/* set termios 'in band', flush input */
		case TCSETA:		/* set termio immediate */
		case TCSETAW:		/* set termio 'in band' */
		case TCSETAF:		/* set termio 'in band', flush input */
		    if (!(line->state & (STC_LP | STC_CONTROL))) {
			mutex_enter(stc->stc_mutex);
			stc_param(line);
			mutex_exit(stc->stc_mutex);
		    }
		    break;
		case TIOCSSOFTCAR:
		    if ((*(int *)mp->b_cont->b_rptr)&1)
			line->stc_ttycommon.t_flags |= TS_SOFTCAR;
		    else
			line->stc_ttycommon.t_flags &= ~TS_SOFTCAR;
		    break;
		}
	} else if (error < 0) {
	/*
	 * "ttycommon_ioctl" didn't do anything; we process it here.
	 * this switch is for ioctl()'s that affect both the serial and
	 * parallel lines
	 */
	    error = 0;
	    if (dblkhold == M_IOCDATA) {
		csp = (struct copyresp *)mp->b_rptr;
		csp->cp_private = cp_private;
	    }

	    switch (iocp->ioc_cmd) {
		/*
		 * we only understand the concept of RTS/CTS flow control;
		 * all the other termiox stuff doesn't apply to us so we
		 * ignore it
		 */
		case TCSETX:
		case TCSETXW:
		case TCSETXF: {
			struct termiox *tiox =
				(struct termiox *)mp->b_cont->b_rptr;
			    if (tiox->x_hflag & RTSXOFF)
				line->stc_ttycommon.t_cflag |= CRTSXOFF;
			    else
				line->stc_ttycommon.t_cflag &= ~CRTSXOFF;
			    if (tiox->x_hflag & CTSXON)
				line->stc_ttycommon.t_cflag |= CRTSCTS;
			    else
				line->stc_ttycommon.t_cflag &= ~CRTSCTS;
			    csp = (struct copyresp *)mp->b_rptr;
			    stp = (struct stc_state_t *)csp->cp_private;
			    if (stp)
				kmem_free(stp, sizeof (struct stc_state_t));
			    stc_ack(mp, NULL, 0);
				/*
				 * now set the line parameters
				 */
			    if (!(line->state & (STC_LP | STC_CONTROL))) {
				mutex_enter(stc->stc_mutex);
				stc_param(line);
				mutex_exit(stc->stc_mutex);
			    }
			}
			ioc_done++;
			break;
		/*
		 * there are 5 ioctl()'s that are specific to this driver:
		 *	STC_SREGS(struct stc_diagregs_t *) (must be
		 *		root to do this)
		 *		sets default parameters for line that take
		 *		effect on next open
		 *	STC_GREGS(struct stc_diagregs_t *)
		 *		get default parameters (parameters won't be
		 *		active until next open)
		 *	STC_SPPC(struct ppc_params_t *)
		 *		set parallel port parameters (until close())
		 *	STC_GPPC(struct ppc_params_t *)
		 *		get parallel port parameters (valid until
		 *		changed or close())
		 *	STC_GSTATS(struct stc_stats *)
		 *		get/set driver stats (must be root to do this)
		 *
		 * STC_SPPC and STC_GPPC can only be used on the parallel port
		 */
		/* *** LOOK - what about mutex() locks here?? *** */
		case STC_GREGS:	/* device control */
		case STC_SREGS:
			if ((line->state & STC_ISROOT) &&
				(line->state & STC_CONTROL)) {
				/*
				 * only let root do this
				 */
				register struct stc_diagregs_t *stc_diagregs =
					(struct stc_diagregs_t *)mp->
						b_cont->b_rptr;
				error = stc_dodiagregs(line, stc_diagregs,
					iocp->ioc_cmd);
				/*
				 * if we're going to return the stc_defaults_t
				 * struct, allocate some space for it
				 */
				if (!error) {
				    register struct stc_diagregs_t *sd;
				    register mblk_t *datap;
				    if ((datap = allocb(
					sizeof (struct stc_diagregs_t),
					BPRI_HI)) == NULL) {
					error = ENOSR;
					cmn_err(CE_CONT,
	"stc_ioctl: unit %d line %d can't allocate STC_SREGS block\n",
						line->unit_no, line->line_no);
				    } else {
					SDM(STC_TRACK, (CE_CONT,
					"stc_ioctl: returning sd struct\n"));
					sd = (struct stc_diagregs_t *)
						datap->b_wptr;
					bcopy(stc_diagregs, sd,
						sizeof (struct stc_diagregs_t));
					csp = (struct copyresp *)mp->b_rptr;
					stp = (struct stc_state_t *)
						csp->cp_private;
					stp->state = STC_COPYOUT;
					stc_mcopy(mp, datap,
						sizeof (struct stc_diagregs_t),
						M_COPYOUT, stp, stp->addr);
				    }
				}
			} else { /* not root */
				error = EPERM;
			}
			ioc_done++;
			break;
		case STC_SPPC:		/* set ppc parameters */
			if (line->state & STC_LP) {
			    register struct ppc_params_t *pp =
				(struct ppc_params_t *)mp->b_cont->b_rptr;
			    if (FUNKY(pp->strobe_w, MIN_STBWIDTH,
				MAX_STBWIDTH) ||
				FUNKY(pp->data_setup, MIN_DATASETUP,
					MAX_DATASETUP) ||
				FUNKY(pp->ack_timeout, MIN_ACKTIMEOUT,
					MAX_ACKTIMEOUT) ||
				FUNKY(pp->busy_timeout, MIN_BUSYTIMEOUT,
					MAX_BUSYTIMEOUT) ||
				FUNKY(pp->error_timeout, MIN_ERRTIMEOUT,
					MAX_ERRTIMEOUT)) {
				error = EINVAL;
			    } else {
				line->strobe_w = pp->strobe_w;
				line->data_setup = pp->data_setup;
				/*
				 * the user passed us seconds, we convert
				 * to ticks
				 */
				line->ack_loops = 0;
				line->ack_maxloops =
					(pp->ack_timeout / PPC_ACK_POLL);
				line->ack_timeout = (PPC_ACK_POLL * hz);
				line->busy_timeout =
					((pp->busy_timeout * hz) /
					BUSY_COUNT_C);
				line->error_timeout = (pp->error_timeout * hz);
				line->flags = pp->flags;
			    }
			} else { /* not the parallel port */
				error = EINVAL;
			}
			/*
			 * now free the state struct and ACK this ioctl()
			 */
			csp = (struct copyresp *)mp->b_rptr;
			stp = (struct stc_state_t *)csp->cp_private;
			if (stp)
			    kmem_free(stp, sizeof (struct stc_state_t));
			if (!error)
			    stc_ack(mp, NULL, 0);
			ioc_done++;
			break;
		case STC_GPPC:		/* get ppc parameters */
			if (line->state & STC_LP) {
			    register struct ppcregs_t *pregs = stc->ppc;
			    register struct ppc_params_t *pp;
			    register mblk_t *datap;
			    if ((datap = allocb(sizeof (struct ppc_params_t),
				BPRI_HI)) == NULL) {
				error = ENOSR;
				cmn_err(CE_CONT,
		"stc_ioctl: unit %d line %d can't allocate STC_GPPC block\n",
					line->unit_no, line->line_no);
			    } else {
				pp = (struct ppc_params_t *)datap->b_wptr;
				/*
				 * the widths and the flags
				 */
				pp->strobe_w = line->strobe_w;
				pp->data_setup = line->data_setup;
				pp->flags = line->flags;
				/*
				 * convert the timeouts from ticks to return
				 * seconds to the user
				 */
				pp->ack_timeout =
					((line->ack_timeout *
					line->ack_maxloops) / hz);
				pp->error_timeout = (line->error_timeout / hz);
				pp->busy_timeout =
					((line->busy_timeout * BUSY_COUNT_C) /
					hz);
				/*
				 * return the state of the interface to the user
				 * they get both what the software thinks it is
				 * and what's currently on the port lines
				 */
				(void) ppc_stat(pregs, line);
				pp->state = (line->pstate & ((1<<PP_SHIFT)-1));
				if (PPC_PAPER_OUT(pregs->pstat, PP_PAPER_OUT))
					pp->state |= (PP_PAPER_OUT<<PP_SHIFT);
				if (PPC_ERROR(pregs->pstat, PP_ERROR))
					pp->state |= (PP_ERROR<<PP_SHIFT);
				if (PPC_BUSY(pregs->pstat, PP_BUSY))
					pp->state |= (PP_BUSY<<PP_SHIFT);
				/* LINTED */
				if (PPC_SELECT(pregs->pstat, PP_SELECT))
					pp->state |= (PP_SELECT<<PP_SHIFT);
				csp = (struct copyresp *)mp->b_rptr;
				stp = (struct stc_state_t *)csp->cp_private;
				stp->state = STC_COPYOUT;
				stc_mcopy(mp, datap,
					sizeof (struct ppc_params_t),
					M_COPYOUT, stp, stp->addr);
			    }
			} else { /* not the parallel port */
				error = EINVAL;
			}
			ioc_done++;
			break;
		case STC_GSTATS: {	/* get driver stats */
			register struct stc_stats_t *stc_stats =
				(struct stc_stats_t *)mp->b_cont->b_rptr;
			register mblk_t *datap;
			int ln = ((stc_stats->line_no)&7);
			int lcmd = stc_stats->cmd;
			int nqfretry = stc_stats->nqfretry;
			if ((line->state & STC_ISROOT) &&
				(line->state & STC_CONTROL)) {
				/*
				 * only let root do this
				 */
			    if ((datap = allocb((sizeof (struct stc_stats_t)),
				BPRI_HI)) == NULL) {
				error = ENOSR;
				cmn_err(CE_CONT,
		"stc_ioctl: unit %d line %d can't allocate STC_GSTATS block\n",
					line->unit_no, line->line_no);
			    } else {
				stc_stats = (struct stc_stats_t *)datap->b_wptr;
				if (!(lcmd & STAT_SET)) {
				    nqfretry = stc->line[ln].stc_stats.nqfretry;
				}
				if (lcmd & STAT_CLEAR)
				    bzero(&stc->line[ln].stc_stats,
					sizeof (struct stc_stats_t));
				bcopy(&stc->line[ln].stc_stats, stc_stats,
					sizeof (struct stc_stats_t));
				stc_stats->line_no = ln;
				stc_stats->nqfretry = nqfretry;
				stc->line[ln].stc_stats.nqfretry = nqfretry;
				csp = (struct copyresp *)mp->b_rptr;
				stp = (struct stc_state_t *)csp->cp_private;
				stp->state = STC_COPYOUT;
				stc_mcopy(mp, datap,
					sizeof (struct stc_stats_t), M_COPYOUT,
					stp, stp->addr);
			    }
			} else { /* not root */
				error = EPERM;
			}
		    }
		    ioc_done++;
		    break;
		case TCSBRK:
			if (*(int *)mp->b_cont->b_rptr) { /* if !0, flush */
				/*
				 * if we're a serial line and we need to flush,
				 * the only thing I can think of doing here is
				 * to clear any pending BREAK in progress and
				 * enable the transmitter (which should also
				 * cause a received XOFF to be tossed and
				 * re-start output if we're using XON/XOFF
				 * flow control)
				 */
				if (!(line->state & (STC_LP | STC_CONTROL))) {
					mutex_enter(stc->stc_mutex);
					line->state |= (STC_BREAK | STC_EBREAK);
					regs->car = (uchar_t)line->line_no;
					SET_CCR(regs->ccr, (CHAN_CTL|XMTR_ENA));
					regs->ier |= TX_RDY;
					STC_LOGIT(line->line_no, 1, 0x696f,
						line->state);
					while (line->state & STC_BUSY) {
					    line->state |= STC_CVBROADCAST;
					    mutex_exit(stc->stc_mutex);
					    cv_wait(line->cvp,
						line->line_mutex);
					    mutex_enter(stc->stc_mutex);
					}
					mutex_exit(stc->stc_mutex);
				}
				ioc_done++;
				/*
				 * now ACK this ioctl(), even if we're the
				 * parallel or control line if we're supposed
				 * to do a real BREAK, we'll ACK
				 * this ioctl() farther down
				 */
				stc_ack(mp, NULL, 0);
			} else {
				/*
				 * trying to do a BREAK on the ppc or
				 * control line
				 */
				if (line->state & (STC_LP|STC_CONTROL))
					error = EINVAL;
			}
			break;
		default:
			/*
			 * not one of these, but could still be good for
			 * the serial lines
			 */
			if (line->state & (STC_LP|STC_CONTROL))
				/*
				 * the rest are just for the serial ports
				 */
				error = EINVAL;
			break;
	    } /* switch */

		/*
		 * if it's an ioctl specific to a serial line, handle it here,
		 * otherwise return
		 */
		/*
		 * left out level of indent here ... just too damn much
		 * work to cstyle.
		 */
		if (!error && !(line->state & (STC_LP|STC_CONTROL)) &&
			!ioc_done) {
		switch (iocp->ioc_cmd) {
		/*
		 * set a break condition on the line for 0.25sec. We do it
		 * immediately here, so the message had better have been
		 * queued in the right place!
		 */
		case TCSBRK:	/* timed BREAK */
			if (*(int *)mp->b_cont->b_rptr == 0) {
				mutex_enter(stc->stc_mutex);
				/*
				 * Set the break flag; we'll come back
				 * when the cd-180 has finished processing
				 * the BREAK (normally 1/4 second); it
				 * will turn the BREAK bit off, and call
				 * stc_start() to grab the next message.
				 */
				line->state |= STC_BREAK;
				regs->car = (uchar_t)line->line_no;
				regs->ier |= TX_RDY;
				line->stc_stats.stc_break++;
				mutex_exit(stc->stc_mutex);
			} /* else (flush) case taken care of up above */
			/*
			 * now ACK this ioctl()
			 */
			stc_ack(mp, NULL, 0);
			break;
		case TIOCSBRK:	/* start BREAK */
			mutex_enter(stc->stc_mutex);
			line->state |= (STC_BREAK | STC_SBREAK);
			regs->car = (uchar_t)line->line_no;
			regs->ier |= TX_RDY;
			line->stc_stats.stc_sbreak++;
			mutex_exit(stc->stc_mutex);
			/*
			 * now ACK this ioctl()
			 */
			stc_ack(mp, NULL, 0);
			break;
		case TIOCCBRK:	/* end BREAK */
			mutex_enter(stc->stc_mutex);
			line->state |= (STC_BREAK | STC_EBREAK);
			regs->car = (uchar_t)line->line_no;
			regs->ier |= TX_RDY;
			line->stc_stats.stc_ebreak++;
			mutex_exit(stc->stc_mutex);
			/*
			 * now ACK this ioctl()
			 */
			stc_ack(mp, NULL, 0);
			break;
		/* Set modem control lines */
		case TIOCMSET:
				mutex_enter(stc->stc_mutex);
				regs->car = (uchar_t)line->line_no;
				if (*(int *)mp->b_cont->b_rptr & TIOCM_LE) {
					SET_CCR(regs->ccr,
						(CHAN_CTL|XMTR_ENA|RCVR_ENA));
				} else {
					SET_CCR(regs->ccr,
						(CHAN_CTL|XMTR_DIS|RCVR_DIS));
				}

				if (*(int *)mp->b_cont->b_rptr & TIOCM_DTR) {
					DTR_SET(line, DTR_ON);
				} else {
					DTR_SET(line, DTR_OFF);
				}

				if (*(int *)mp->b_cont->b_rptr & TIOCM_RTS)
					regs->msvr |= RTS_ON;
				else
					regs->msvr &= ~RTS_ON;
				mutex_exit(stc->stc_mutex);
				if (iocp->ioc_count == TRANSPARENT) {
				    csp = (struct copyresp *)mp->b_rptr;
				    stp = (struct stc_state_t *)csp->cp_private;
				    if (stp)
					kmem_free(stp,
					    sizeof (struct stc_state_t));
				}
				stc_ack(mp, NULL, 0);
			line->stc_stats.set_modem++;
			break;
		/* Turn on modem control lines */
		case TIOCMBIS:
				mutex_enter(stc->stc_mutex);
				regs->car = (uchar_t)line->line_no;
				if (*(int *)mp->b_cont->b_rptr & TIOCM_DTR) {
					DTR_SET(line, DTR_ON);
				}
				if (*(int *)mp->b_cont->b_rptr & TIOCM_LE)
					SET_CCR(regs->ccr,
						(CHAN_CTL|XMTR_ENA|RCVR_ENA));
				if (*(int *)mp->b_cont->b_rptr & TIOCM_RTS)
					regs->msvr |= RTS_ON;
				mutex_exit(stc->stc_mutex);
				if (iocp->ioc_count == TRANSPARENT) {
				    csp = (struct copyresp *)mp->b_rptr;
				    stp = (struct stc_state_t *)csp->cp_private;
				    if (stp)
					kmem_free(stp,
					    sizeof (struct stc_state_t));
				}
				stc_ack(mp, NULL, 0);
			line->stc_stats.set_modem++;
			break;
		/* Turn off modem control lines */
		case TIOCMBIC:
			    mutex_enter(stc->stc_mutex);
			    regs->car = (uchar_t)line->line_no;
			    if (!mp->b_cont) {
				    cmn_err(CE_CONT,
					"No arg to TIOCMBIC call\n");
				    error = EINVAL;
				    mutex_exit(stc->stc_mutex);
			    } else {
				if (*(int *)mp->b_cont->b_rptr & TIOCM_DTR) {
					DTR_SET(line, DTR_OFF);
				}
				if (*(int *)mp->b_cont->b_rptr & TIOCM_LE)
					SET_CCR(regs->ccr,
						(CHAN_CTL|XMTR_DIS|RCVR_DIS));
				if (*(int *)mp->b_cont->b_rptr & TIOCM_RTS)
					regs->msvr &= ~RTS_ON;
				mutex_exit(stc->stc_mutex);
				if (iocp->ioc_count == TRANSPARENT) {
				    csp = (struct copyresp *)mp->b_rptr;
				    stp = (struct stc_state_t *)csp->cp_private;
				    if (stp)
					kmem_free(stp,
					    sizeof (struct stc_state_t));
				}
			    }
			    stc_ack(mp, NULL, 0);
			line->stc_stats.set_modem++;
			break;
		/* Return values of modem control lines */
		case TIOCMGET: {
			register int *bits;
			register mblk_t *datap;

			if ((datap = allocb(sizeof (int), BPRI_HI)) == NULL) {
				error = ENOSR;
				cmn_err(CE_CONT,
		"stc_ioctl: unit %d line %d can't allocate TIOCMGET block\n",
					line->unit_no, line->line_no);
			} else {
				bits = (int *)datap->b_wptr;
				*bits = 0;
				mutex_enter(stc->stc_mutex);
				regs->car = (uchar_t)line->line_no;
				if (regs->ccsr & (RX_ENA|TX_ENA))
					*bits |= TIOCM_LE;
				if (regs->msvr&CD_ON)
					*bits |= TIOCM_CAR;
				if (regs->msvr&DSR_ON)
					*bits |= TIOCM_DSR;
				if (regs->msvr&CTS_ON)
					*bits |= TIOCM_CTS;
				if (DTR_GET(line))
					*bits |= TIOCM_DTR;
				mutex_exit(stc->stc_mutex);
				if (dblkhold == M_IOCTL) {
				    stc_ack(mp, datap, sizeof (int));
				} else {
				    csp = (struct copyresp *)mp->b_rptr;
				    stp = (struct stc_state_t *)csp->cp_private;
				    stp->state = STC_COPYOUT;
				    stc_mcopy(mp, datap, sizeof (int),
					M_COPYOUT, stp, stp->addr);
				}
			}
			line->stc_stats.get_modem++;
			}break;

		/* We don't understand it either. */
		default:
		SDM(STC_DEBUG, (CE_CONT,
			"stc_ioctl: invalid ioctl (0x%x) unit %d line %d\n",
			iocp->ioc_cmd, line->unit_no, line->line_no));
		error = EINVAL;
		break;

		} /* switch */
		} /* !error && !STC_LP */
	} /* (error < 0) */

	/*
	 * if there was an error, send a NAK, otherwise ACK
	 * this ioctl
	 */

	mutex_exit(line->line_mutex);

	if (error) {
	    stc_sendnak(q, mp, error);
	} else {
	    qreply(q, mp);
	}

	SDM(STC_TRACK, (CE_CONT,
		"--------(stc_ioctl)-----(LEAVING error = 0x%x)-------\n",
		error));
}

/*
 * stc_dodiagregs(line, sd) - provides diagnostic access to the board registers
 *
 * note that we don't provide access to the cd180 *IACK space, the PPC *IACK
 * space or the DTR latch
 * note also that if we read from the PPC control register (pcon), we don't
 * update the soft copy of this register (line->pcon_s)
 * really, you shouldn't play with all of this unless you know what
 * you are doing
 */
static int
stc_dodiagregs(struct stc_line_t *line, struct stc_diagregs_t *stc_diagregs,
    u_int iocmd)
{
	register struct stc_unit_t *stc = line->stc;
	register uchar_t *cregs;

	switch (stc_diagregs->reg_flag) {
		case STC_IOREG:	    /* dink the cd180 register */
		    if (iocmd == STC_GREGS) {
			mutex_enter(stc->stc_mutex);
			cregs = (uchar_t *)stc->regs + stc_diagregs->reg_offset;
			stc_diagregs->reg_data = *cregs;
			mutex_exit(stc->stc_mutex);
		    } else {
			mutex_enter(stc->stc_mutex);
			cregs = (uchar_t *)stc->regs + stc_diagregs->reg_offset;
			*cregs = stc_diagregs->reg_data;
			mutex_exit(stc->stc_mutex);
		    }
		    break;
		case STC_PPCREG:	/* dink the PPC register */
		    if (iocmd == STC_GREGS) {
			mutex_enter(stc->ppc_mutex);
			cregs = (uchar_t *)stc->ppc + stc_diagregs->reg_offset;
			stc_diagregs->reg_data = *cregs;
			mutex_exit(stc->ppc_mutex);
		    } else {
			mutex_enter(stc->ppc_mutex);
			cregs = (uchar_t *)stc->ppc + stc_diagregs->reg_offset;
			*cregs = stc_diagregs->reg_data;
			mutex_exit(stc->ppc_mutex);
		    }
		    break;
	}
	return (0);
}

/*
 * stc_rstart(line) - restart output on a line after a delay
 */
static void
stc_restart(void *arg)
{
	struct stc_line_t *line = arg;
	queue_t *q;

	SDM(STC_TRACK, (CE_CONT, "stc_restart: unit %d line %d\n",
		line->unit_no, line->line_no));

	mutex_enter(line->stc->stc_mutex);
	line->state &= ~STC_DELAY;
	mutex_exit(line->stc->stc_mutex);

	/*
	 * Get a pointer to our write side queue
	 */
	if ((q = line->stc_ttycommon.t_writeq) == NULL) {
	    cmn_err(CE_CONT, "stc_restart: unit %d line %d q is NULL\n",
				line->unit_no, line->line_no);
	} else {
	    mutex_enter(line->line_mutex);
	    enterq(q);
	    stc_start(line);
	    leaveq(q);
	    mutex_exit(line->line_mutex);
	}
}

/*
 * stc_param(line) - set up paramters for a line
 *
 *	line - pointer to stc_line_t structure
 */
static void
stc_param(struct stc_line_t *line)
{
	register int ispeed, ospeed;
	unsigned long cflag = line->stc_ttycommon.t_cflag;
	unsigned long iflag = line->stc_ttycommon.t_iflag;
	register struct stc_unit_t *stc = line->stc;

	SDM(STC_TRACK, (CE_CONT,
		"----------(stc_param)----(unit %d line %d)----------\n",
		line->unit_no, line->line_no));

	line->stc_stats.set_params++;

	/*
	 * hang up if zero speed. If CIBAUD bits are non-zero they specify
	 * the input baud rate, otherwise CBAUD specifies both speeds. I'm
	 * assuming that both speeds are set to zero to hangup.
	 */
	if ((ispeed = ((cflag & CIBAUD) >> IBSHIFT)) == 0) {
		ispeed = cflag & CBAUD;
	}

	ospeed = cflag & CBAUD;
	if (ospeed == 0) {
		DTR_SET(line, DTR_OFF);
		return;
	}

	/*
	 * select the channel number we're interested in
	 */
	stc->regs->car = (uchar_t)line->line_no;
	stc->regs->cor1 = 0;		/* OR-in as we need things */

	/*
	 * Rx/Tx speeds - we don't do split yet, cuz stty doesn't seem
	 *	to send input speed and some brain-dead programs like
	 *	cu don't set ispeed to zero
	 */
#define	NO_SPEED_SPLIT

	stc->regs->tbprh = HI_BAUD(stc, ospeed);
	stc->regs->tbprl = LO_BAUD(stc, ospeed);
#ifdef	NO_SPEED_SPLIT
	stc->regs->rbprh = HI_BAUD(stc, ospeed);
	stc->regs->rbprl = LO_BAUD(stc, ospeed);
#ifdef lint
	ispeed = ispeed;
#endif
#else
	stc->regs->rbprh = HI_BAUD(stc, ispeed);
	stc->regs->rbprl = LO_BAUD(stc, ispeed);
#endif

	/*
	 * character length, parity and stop bits
	 * CS5 is 00, so we don't need a case for it
	 */
	switch (cflag & CSIZE) {
	    case CS8:
		stc->regs->cor1 |= CHAR_8;
		break;
	    case CS7:
		stc->regs->cor1 |= CHAR_7;
		break;
	    case CS6:
		stc->regs->cor1 |= CHAR_6;
		break;
	}

	if (cflag & CSTOPB)
		stc->regs->cor1 |= STOP_2;

	/*
	 * do the parity stuff here - if we get a parity error,
	 * it will show up in the receive exception interrupt
	 * handler (stc_rcvex())
	 */
	if (cflag & PARENB) {
	    if (!(iflag & INPCK))		/* checking input parity? */
		stc->regs->cor1 |= IGNORE_P;	/* nope, ignore input parity */
	    stc->regs->cor1 |= NORMAL_P;	/* do normal parity proc */
	    if (cflag & PARODD)
		stc->regs->cor1 |= ODD_P;
	} /* cor1 = 0 case is no parity at all */

	/*
	 * check for CTS/RTS flow control
	 *
	 * if we're using CTS/RTS flow control, enable the CTS side of
	 * it on the cd-180 (cd-180 will stop sending within 2 char
	 * times after CTS goes low); the PUTSILO() macro will monitor
	 * the silo level and if it passes the low or high water marks
	 * and the CRTSCTS bit in stc_ttycommon.t_cflag is set, will
	 * do the right thing with the RTS line
	 *
	 * if we're not using CTS/RTS, then just raise RTS (beacuse zs
	 * and alm do it - this is really stupid, because in my opinion,
	 * if you're NOT using CTS/RTS flow control, your device should
	 * ignore the state of our RTS line, but alas, seems that "prior art"
	 * has, once again, besmerched the serial drivers...)
	 */
	if (cflag & (CRTSCTS|CRTSXOFF)) {
		stc->regs->cor2 |= FLOW_RTSCTS;
	} else {
		stc->regs->cor2 &= ~FLOW_RTSCTS;
		stc->regs->msvr |= RTS_ON;
	}

	/*
	 * check for XON/XOFF flow control
	 * this is only done for the transmitter, i.e. when we
	 * receive an XOFF, the cd180 will stop transmitting
	 * until we get an XON (or any char if IXANY is set)
	 * we let streams handle IXOFF (the receiver side of
	 * things) until the code gets into the receiver
	 * interrupt handler
	 * ??should this be added to PUTSILO(), just as we
	 * handle the RTS line there, or should we let the
	 * STREAMS code deal with it??
	 */
	if (iflag & IXON) {
		stc->regs->schr2 = line->stc_ttycommon.t_stopc;
		stc->regs->schr1 = line->stc_ttycommon.t_startc;
		stc->regs->schr3 = 0;	/* we don't use schr3 and schr4 */
		stc->regs->schr4 = 0;
		stc->regs->cor3 &= ~(XON_13|XOFF_24);
			/* use schr1 as XON/schr2 as XOFF */
		stc->regs->cor3 |= (NO_FLOW_EXP|SCHR_DET);
		stc->regs->cor2 |= FLOW_IXON;
		if (iflag & IXANY)
			stc->regs->cor2 |= FLOW_IXANY;
		else
			stc->regs->cor2 &= ~FLOW_IXANY;
	} else {
		stc->regs->cor2 &= ~(FLOW_IXON|FLOW_IXANY);
		stc->regs->cor3 &= ~(NO_FLOW_EXP|SCHR_DET);
	}

	/*
	 * set the receive flow control flag for the receive
	 *	interrupt handler so that it can pop an XOFF to
	 *	the remote when the receive silo becomes close to full
	 * ??see comment above about PUTSILO() control of this??
	 */
	if (iflag & IXOFF)
		line->state |= STC_IXOFF;
	else
		line->state &= ~STC_IXOFF;

	/*
	 * stick 'em into the cd180
	 */
	SET_CCR(stc->regs->ccr, COR1_CHANGED|COR_CHANGED);
	SET_CCR(stc->regs->ccr, COR2_CHANGED|COR_CHANGED);
	SET_CCR(stc->regs->ccr, COR3_CHANGED|COR_CHANGED);

	/*
	 * if the user has disabled XON/XOFF flow control
	 *	and they have STC_INSTANTFLOW set in the line's
	 *	flag field (via the defaults ioctl), then
	 *	immediately enable the transmitter to send data
	 * also, be sure that the transmitter is enabled
	 */
	if (!(iflag & IXON) && (line->flags & STC_INSTANTFLOW))
	    if (stc->regs->ccsr & TX_ENA)
		SET_CCR(stc->regs->ccr, (CHAN_CTL|XMTR_ENA));

	SDM(STC_TRACK, (CE_CONT,
		"-----------(stc_param)------(farvernugen)---------------\n"));
}

/*
 * stc_xwait(line) - waits for transmitter to drain
 *
 *    this routine will wait for the bytes in the cd-180's transmitter to
 *	drain by requesting a transmitter empty interrupt; it is assumed
 *	that stc_txcount is 0, meaning that all of the data from the
 *	driver's Tx buffer has been transferred to the cd-180's Tx fifo
 *
 */
static void
stc_xwait(struct stc_line_t *line)
{
	register struct stcregs_t *regs = line->stc->regs;
	register struct stc_unit_t *stc = line->stc;
	register uchar_t ier, ccr;

	SDM(STC_TRACK, (CE_CONT,
		"----- stc_xwait(0x%x) ENTERING -----\n", line));

	/*
	 * save the old interrupt mask in case we're being called from
	 * ioctl, enable only the xmit interrupt and let the transmitter
	 * service routine wake us up when the xmit buffer is empty
	 * if we get interrupted from sleep, then disable all interrupts
	 * because there's probably a good reason for the interrupt
	 * if we're draining on the line because of an STC_DCONTROL ioctl(),
	 * then don't bother to wait for the rest of the data to be shifted
	 * out of the UART, because the line is probably hosed anyway
	 */
	if (!(line->state & STC_DRAIN)) {
	    do {
		mutex_enter(stc->stc_mutex);
		line->state |= STC_XWAIT;
		regs->car = (uchar_t)line->line_no;
		ier = regs->ier;
		regs->ier = TX_MPTY;
		ccr = (((regs->ccsr&TX_ENA)?XMTR_ENA:XMTR_DIS) |
			((regs->ccsr&RX_ENA)?RCVR_ENA:RCVR_DIS));
		/*
		 * make sure that the transmitter is enabled so that this whole
		 * scheme can work properly - we do this since the user can
		 * issue an ioctl() that disables the transmitter
		 */
		if (!(regs->ccsr & TX_ENA)) {
			cmn_err(CE_CONT,
			"stc_xwait: unit %d line %d enabling transmitter\n",
				line->unit_no, line->line_no);
			SET_CCR(regs->ccr, (CHAN_CTL|XMTR_ENA));
		} else {
			ccr = 0;
		}

		/*
		 * check if we've been stopped by flow control from
		 * the remote; if we have, display a message (if enabled)
		 * so that if we're being stuck here due to the remote
		 * asserting flow control, the user will know it
		 */
		if (regs->ccsr & TX_FLOFF) {
		    if (line->flags & STC_CFLOWMSG) {
			cmn_err(CE_CONT,
		"stc_xwait: unit %d line %d stopped by remote flow control\n",
				line->unit_no, line->line_no);
		    }
		}

		/*
		 * if we just want to blast through this close without
		 *	waiting for software flow control semantics,
		 *	disable s/w flow control (special character
		 *	detection) and	hit the Tx enable bit in the
		 *	CCR; this will start data moving again if it's
		 *	been stopped due to a received XOFF character
		 */
		if (line->flags & STC_CFLOWFLUSH) {
			stc_txflush(line);
		}
		mutex_exit(stc->stc_mutex);

		/*
		 * post a timer so that we don't hang here forever
		 * the timer will be removed from the queue by stc_xmit()
		 * when the transmitter becomes empty, or if we get
		 * interrupted while in the sleep()
		 */
		line->stc_timeout_id =
			timeout(stc_timeout, line, STC_TIMEOUT);
		if (!cv_wait_sig(line->cvp, line->line_mutex)) {
			UNTIMEOUT(line->stc_timeout_id);
			SDM(STC_DEBUG, (CE_CONT,
				"stc_xwait: interrupt sleep unit %d line %d\n",
				line->unit_no, line->line_no));
			ier = 0;	/* interrupted here, disable ints */
			mutex_enter(stc->stc_mutex);
			line->stc_txcount = 0;
			mutex_exit(stc->stc_mutex);
		} else {
			/* *** LOOK vvvv this shouldn't be here *** */
			UNTIMEOUT(line->stc_timeout_id);
			/* *** LOOK ^^^^ this shouldn't be here *** */
		}

		/*
		 * restore the old transmitter and receiver enable states
		 * and the interrupt enable register
		 */
		mutex_enter(stc->stc_mutex);
		regs->car = (uchar_t)line->line_no;
		if (ccr)
			SET_CCR(regs->ccr, (CHAN_CTL|ccr));
		regs->ier = ier;	/* restore old interrupt mask */
		mutex_exit(stc->stc_mutex);
	    } while (line->stc_txcount && (!(line->state & STC_DRAIN)));
	} /* if (!STC_DRAIN) */

	SDM(STC_TRACK,
		(CE_CONT, "----- stc_xwait(0x%x) LEAVING -----\n", line));
}

static void
stc_timeout(void *arg)
{
	struct stc_line_t *line = arg;

	SDM(STC_DEBUG, (CE_CONT, "stc_timeout: timer popped on 0x%x\n", line));
	cv_broadcast(line->cvp);
}

/*
 * stc_start(line) - start output on a line
 *
 * this handles both the serial lines and the ppc
 */
static void
stc_start(struct stc_line_t *line)
{
	register queue_t *q;
	register mblk_t *bp;
	register int cc, bytesleft;
	register struct stc_unit_t *stc = line->stc;
	register struct stcregs_t *regs = stc->regs;
	register uchar_t *current;
	register kmutex_t *hw_mutex;

	SDM(STC_TRACK, (CE_CONT,
		"---------(stc%d, line%d)---(stc_start)------------ \n",
		line->unit_no, line->line_no));

	STC_LOGIT(line->line_no, 0, 0x7374, line);
	mutex_enter(stc->stc_mutex);
	if (line->state & STC_STARTED) {
		SDM(STC_TRACK, (CE_CONT,
		    "stc_start: doing nothing, already started line: 0x%x\n",
			line));
		mutex_exit(stc->stc_mutex);
		return;
	}

	line->state |= STC_STARTED;


	/*
	 * Get a pointer to our write side queue
	 */
	if ((q = line->stc_ttycommon.t_writeq) == NULL) {
		cmn_err(CE_CONT, "stc_start: unit%d line%d q is NULL\n",
				line->unit_no, line->line_no);
		mutex_exit(stc->stc_mutex);
		goto out;		/* not attached to a stream */
	}

	if ((line->state &
		(STC_BREAK | STC_BUSY | STC_STOPPED | STC_FLUSH | STC_DELAY)) &&
		!(line->state & STC_DRAIN)) {
		SDM(STC_TRACK, (CE_CONT,
			"stc_start: doing nothing, state: 0x%x, line: 0x%x\n",
			line->state, line));
		mutex_exit(stc->stc_mutex);
		goto out;	/* in the middle of something already */
	}

	mutex_exit(stc->stc_mutex);

	STC_LOGIT(line->line_no, 1, 0x7374, stc);
	STC_LOGIT(line->line_no, 2, 0x7374, line->state);
	/*
	 * get the correct hardware mutex to use
	 * Note: coming in here as the control line will lock out
	 *	ppc interrupts for a while
	 */
	if (!(line->state & (STC_LP | STC_CONTROL)))
	    hw_mutex = stc->stc_mutex;
	else
	    hw_mutex = stc->ppc_mutex;

	/*
	 * setup the local transmit buffer stuff
	 */
	mutex_enter(hw_mutex);
	bytesleft = line->stc_txbufsize;
	current = line->stc_txbuf;
	line->stc_txcount = 0;
	line->stc_txbufp = line->stc_txbuf;
	mutex_exit(hw_mutex);

	/*
	 * handle next message block (if any)
	 */
	while ((bp = getq(q)) != NULL) {
	    switch (bp->b_datap->db_type) {
		/*
		 * For either delay or an ioctl, we need to process them when
		 * any characters seen so far have drained - if there aren't any
		 * we are in the right place at the right time.
		 */
		case M_IOCTL:
		case M_IOCDATA:

#ifdef	DEBUG_STCIOCTL
			if (bp->b_cont == NULL)
			    cmn_err(CE_CONT, "stc_start: no b_cont on bp\n");
#endif	DEBUG_STCIOCTL

			if (bytesleft != line->stc_txbufsize) {
				(void) putbq(q, bp);
				goto transmit;
			}
			mutex_exit(line->line_mutex);
			stc_ioctl(q, bp);
			mutex_enter(line->line_mutex);
			break;
		case M_DELAY:
			if (bytesleft != line->stc_txbufsize) {
				(void) putbq(q, bp);
				goto transmit;
			}
			mutex_enter(stc->stc_mutex);
			line->state |= STC_DELAY;
			mutex_exit(stc->stc_mutex);
			(void) timeout(stc_restart, line,
				(int)(*(unsigned char *) bp->b_rptr));
			freemsg(bp);
			/*
			 * exit this right now; we'll get called back by
			 *	stc_restart() to continue processing any
			 *	other messages that are still on our queue
			 */
			goto out;
		/*
		 * suck up all the data we can from these mesages until we
		 * run out of data messages (above) or we fill the txbuf
		 * if we're draining the queue (STC_DRAIN set), just suck
		 * every last byte of data out and pretend that we have
		 * transmitted it to the device
		 */
		case M_DATA: {
			register mblk_t *nbp;

			do {
				if (!(line->state & STC_DRAIN)) {
					while ((cc =
					    (bp->b_wptr - bp->b_rptr)) != 0) {
						if (!bytesleft) {
							(void) putbq(q, bp);
							goto transmit;
						}
						mutex_enter(hw_mutex);
						cc = MIN(cc, bytesleft);
						bcopy(bp->b_rptr, current, cc);
						line->stc_txcount += cc;
						current += cc;
						bytesleft -= cc;
						bp->b_rptr += cc;
						mutex_exit(hw_mutex);
					}
				} else { /* STC_DRAIN */
					/*EMPTY*/ /* if DEBUG_STC not set */
					SDM(STC_DEBUG, (CE_CONT,
			"stc_start: (STC_DRAIN) sucking data unit %d line %d\n",
						line->unit_no, line->line_no));
				}
				nbp = bp;
				bp = bp->b_cont;
				freeb(nbp);
			} while (bp != NULL);
		}
		break;
		default:
			SDM(STC_DEBUG, (CE_CONT,
			"stc_start: unit %d line %d unknown message: 0x%x\n",
				line->unit_no, line->line_no,
				bp->b_datap->db_type));
			freemsg(bp);
			goto out;
	    }
	}

transmit:
	SDM(STC_DEBUG, (CE_CONT,
		"stc_start: txcount=%d, txbufsize=%d, bytesleft=%d\n",
		line->stc_txcount, line->stc_txbufsize, bytesleft));
	if (!(line->state & (STC_DRAIN | STC_CONTROL))) {
	    if ((cc = (line->stc_txbufsize - bytesleft)) != 0) {
		SDM(STC_DEBUG, (CE_CONT,
			"stc_start: xmit %d\n", line->stc_txcount));
		if (line->state & STC_LP) {
			/*
			 * send the character and enable ppc interrupts
			 */
			mutex_enter(stc->stc_mutex);
			line->state |= STC_BUSY;
			line->state &= ~STC_STARTED;
			mutex_exit(stc->stc_mutex);
			mutex_exit(line->line_mutex);
			ppc_out(line);
			mutex_enter(line->line_mutex);
		} else {
			/*
			 * enable the transmitter empty interrupts and off we go
			 */
			STC_LOGIT(line->line_no, 3, 0x7374, line->state);
			mutex_enter(stc->stc_mutex);
			regs->car = (uchar_t)line->line_no;
			regs->ier |= TX_RDY;
			line->state |= STC_BUSY;
			line->state &= ~STC_STARTED;
			mutex_exit(stc->stc_mutex);
			return;
		}
	    }
	} /* !STC_DRAIN */

out:
	mutex_enter(stc->stc_mutex);
	line->state &= ~STC_STARTED;
	mutex_exit(stc->stc_mutex);
}

/*
 * stc_getdefaults(dip, tag, stc_defaults) - get default parameters for a line
 */
static void
stc_getdefaults(dev_info_t *dip, u_int tag,
    struct stc_defaults_t *stc_defaults)
{
	int deflen;
	int alloc_len;
	char defname[STC_MAXPROPLEN];
	char *defstrptr;
	int index;
	char *stctoken = NULL;
	int stcval;
	int type;


	SDM(STC_DEBUG, (CE_CONT, "stc_getdefaults got tag %d\n", tag));
	(void) sprintf(defname, "%s%d", STC_SERIAL_PROP_ID, tag);
	SDM(STC_DEBUG, (CE_CONT, "stc_getdefaults got name %s\n",
		defname));
	if (ddi_getproplen(DDI_DEV_T_ANY, dip,
		0, defname, &alloc_len) !=
		DDI_PROP_SUCCESS)
			return;

	deflen = alloc_len;
	alloc_len++;
	defstrptr = kmem_alloc((size_t)alloc_len, KM_SLEEP);

	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, dip,
		0, defname, (caddr_t)defstrptr, &deflen) !=
		DDI_PROP_SUCCESS) {
		    cmn_err(CE_CONT, "stc: Dropped default properties for %s\n",
			defname);
		    kmem_free((void *)defstrptr, (size_t)alloc_len);
		    return;
	}


	defstrptr[deflen] = 0;
	SDM(STC_DEBUG, (CE_CONT, "Default: tag= %d, prop= %s\n", tag,
		defstrptr));

	index = 0;
	stc_defaults->flags = 0;
	while (index < deflen) {
		type = stc_extractval(defstrptr, &index, &stctoken, &stcval);
		if (type == STC_EXTRACT_FAILURE) {
		    cmn_err(CE_CONT, "stc: Failed extracting default.\n");
		    kmem_free((void *)defstrptr, (size_t)alloc_len);
		    return;
		}

		if (strcmp(stctoken, "soft_carrier") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= SOFT_CARR;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "dtr_assert") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= DTR_ASSERT;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "dtr_force") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= STC_DTRFORCE;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "dtr_close") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= STC_DTRCLOSE;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "cflow_flush") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= STC_CFLOWFLUSH;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "cflow_msg") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= STC_CFLOWMSG;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "instantflow") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= STC_INSTANTFLOW;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "display") == 0) {
		    if (type == STC_BOOLEAN) {
			cmn_err(CE_CONT, "stc: nothing to display\n");
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "drain_size") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_DRAIN_SIZE, MAX_DRAIN_SIZE)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->drain_size = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "hiwater") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_HIWATER, MAX_HIWATER)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->stc_hiwater = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "lowwater") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_LOWWATER, MAX_LOWWATER)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->stc_lowwater = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "rtpr") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_RTPR, MAX_RTPR)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->rtpr = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "rxfifo") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_RX_FIFO, MAX_RX_FIFO)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->rx_fifo_thld = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else {
		    cmn_err(CE_CONT, "stc: %s is not a valid line parameter\n",
			stctoken);
		}
	}

	kmem_free((void *)defstrptr, (size_t)alloc_len);
}

/*
 * ppc_getdefaults(dip, tag, stc_defaults) - get default parameters for a
 * parallel line
 */
static void
ppc_getdefaults(dev_info_t *dip, u_int tag,
    struct stc_defaults_t *stc_defaults)
{
	int deflen;
	int alloc_len;
	char defname[STC_MAXPROPLEN];
	char *defstrptr;
	int index;
	char *stctoken = NULL;
	int stcval;
	int type;


	(void) sprintf(defname, "%s%d", STC_PARALLEL_PROP_ID, tag);
	if (ddi_getproplen(DDI_DEV_T_ANY, dip, 0,
		defname, &alloc_len) !=
		DDI_PROP_SUCCESS)
			return;

	deflen = alloc_len;
	alloc_len++;
	defstrptr = kmem_alloc((size_t)alloc_len, KM_SLEEP);

	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, dip,
		0, defname, (caddr_t)defstrptr, &deflen) !=
		DDI_PROP_SUCCESS) {
		    cmn_err(CE_CONT, "stc: Dropped default properties for %s\n",
			defname);
		    kmem_free((void *)defstrptr, (size_t)alloc_len);
		    return;
	}


	defstrptr[deflen] = 0;
	SDM(STC_DEBUG, (CE_CONT, "Default: tag= %d, prop= %s\n", tag,
		defstrptr));

	index = 0;
	stc_defaults->flags = 0;
	while (index < deflen) {
		type = stc_extractval(defstrptr, &index, &stctoken, &stcval);
		if (type == STC_EXTRACT_FAILURE) {
		    cmn_err(CE_CONT, "stc: Failured extracting default.\n");
		    kmem_free((void *)defstrptr, (size_t)alloc_len);
		    return;
		}

		if (strcmp(stctoken, "paper_out") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_PAPER_OUT;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "error") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_ERROR;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "busy") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_BUSY;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "select") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_SELECT;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "pp_message") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_MSG;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "pp_signal") == 0) {
		    if (type == STC_BOOLEAN) {
			stc_defaults->flags |= PP_SIGNAL;
		    } else {
			cmn_err(CE_CONT, "stc: %s is a boolean parameter\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "ack_timeout") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_ACKTIMEOUT, MAX_ACKTIMEOUT)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->ack_timeout = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "error_timeout") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_ERRTIMEOUT, MAX_ERRTIMEOUT)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->error_timeout = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "busy_timeout") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_BUSYTIMEOUT, MAX_BUSYTIMEOUT)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->busy_timeout = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "data_setup") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_DATASETUP, MAX_DATASETUP)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->data_setup = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else if (strcmp(stctoken, "strobe_width") == 0) {
		    if (type == STC_VALUED) {
			if (FUNKY(stcval, MIN_STBWIDTH, MAX_STBWIDTH)) {
			    cmn_err(CE_CONT, "stc: %s value %d out of range\n",
				stctoken, stcval);
			} else {
			    stc_defaults->strobe_w = stcval;
			}
		    } else {
			cmn_err(CE_CONT, "stc: %s requires a value\n",
			    stctoken);
		    }
		} else {
		    cmn_err(CE_CONT, "stc: %s is not a valid line parameter\n",
			stctoken);
		}
	}

	kmem_free((void *)defstrptr, (size_t)alloc_len);
}

static int
stc_extractval(char *strbuf, int *offset, char **token, int *value)
{
	int c;
	char *tc;
	int retval = STC_EXTRACT_FAILURE;

	*token = &strbuf[*offset];
	for (; (c = strbuf[*offset]) != 0 && c != ':' && c != '='; (*offset)++)
		;

	switch (c) {
	    case ':':
		strbuf[*offset] = 0;
		/* FALLTHROUGH */
	    case 0:
		(*offset)++;
		return (STC_BOOLEAN);
		/* NOTREACHED */
	    case '=':
		strbuf[*offset] = 0;
		(*offset)++;
		tc = &strbuf[*offset];
		for (; (c = strbuf[*offset]) != 0 && c != ':'; (*offset)++)
			;
		if (c == ':')
		    strbuf[*offset] = 0;
		(*offset)++;
		*value = stoi(&tc);
		return (STC_VALUED);
		/* NOTREACHED */
	}
	return (retval);
}

/*
 * stc_setdefaults(line, stc_defaults) - set up default paramters for a line
 */
static void
stc_setdefaults(struct stc_line_t *line, struct stc_defaults_t *stc_defaults)
{
	register ispeed, ospeed;
	unsigned long cflag = stc_defaults->termios.c_cflag;
	unsigned long iflag = stc_defaults->termios.c_iflag;

	/*
	 * do a simple sanity check on the ppc parameters - isn't there a
	 * better way to do this? (yeah - trust the user to always give you
	 * the right values... ha!)
	 */
	if (line->state & STC_LP) {

		line->default_param.sd.strobe_w = stc_defaults->strobe_w;
		line->default_param.sd.data_setup = stc_defaults->data_setup;
		/*
		 * the user passed us seconds, we convert to ticks
		 */
		line->default_param.ack_maxloops =
			(stc_defaults->ack_timeout / PPC_ACK_POLL);
		line->default_param.sd.ack_timeout = (PPC_ACK_POLL * hz);
		line->default_param.sd.busy_timeout =
			((stc_defaults->busy_timeout * hz) / BUSY_COUNT_C);
		line->default_param.sd.error_timeout =
			(stc_defaults->error_timeout * hz);
	} else {

		line->default_param.sd.drain_size = stc_defaults->drain_size;
		line->default_param.sd.stc_hiwater = stc_defaults->stc_hiwater;
		line->default_param.sd.stc_lowwater =
			stc_defaults->stc_lowwater;
		line->default_param.rtpr = stc_defaults->rtpr;
		line->default_param.cor3 &= ~RX_FIFO_MASK;
		line->default_param.cor3 |=
			(stc_defaults->rx_fifo_thld & RX_FIFO_MASK);
		line->default_param.sd.rx_fifo_thld =
			stc_defaults->rx_fifo_thld;
	}

	/*
	 * set the line flags like SOFT_CARR, DTR_ASSERT, etc...
	 */
	line->default_param.sd.flags = stc_defaults->flags;
	if (stc_defaults->flags & SOFT_CARR)
		line->stc_ttycommon.t_flags |= TS_SOFTCAR;
	else
		line->stc_ttycommon.t_flags &= ~TS_SOFTCAR;

	/*
	 * copy the user's default termios structure into the line default
	 * termios structure
	 */
	bcopy(&stc_defaults->termios,
		&line->default_param.sd.termios, sizeof (struct termios));

	/*
	 * from here on, if we're dealing with the parallel port, we don't
	 * really need to set any of this stuff.  but why not?
	 *
	 * If CIBAUD bits are non-zero they specify the input baud rate,
	 * otherwise CBAUD specifies both speeds.
	 */
	if ((ispeed = ((cflag & CIBAUD) >> IBSHIFT)) == 0) {
		ispeed = cflag & CBAUD;
	}
	ospeed = cflag & CBAUD;

	line->default_param.cor1 = 0;		/* OR-in as we need things */

	/*
	 * Rx/Tx speeds
	 */
	line->default_param.tbprh = HI_BAUD(line->stc, ospeed);
	line->default_param.tbprl = LO_BAUD(line->stc, ospeed);
	line->default_param.rbprh = HI_BAUD(line->stc, ispeed);
	line->default_param.rbprl = LO_BAUD(line->stc, ispeed);

	/*
	 * character length, parity and stop bits
	 * CS5 is 00, so we don't need a case for it
	 */
	switch (cflag & CSIZE) {
	    case CS8:
		line->default_param.cor1 |= CHAR_8;
		break;
	    case CS7:
		line->default_param.cor1 |= CHAR_7;
		break;
	    case CS6:
		line->default_param.cor1 |= CHAR_6;
		break;
	}

	if (cflag & CSTOPB)
		line->default_param.cor1 |= STOP_2;

	/*
	 * do the parity stuff here
	 */
	if (cflag & PARENB) {
	    if (!(iflag & INPCK))	/* checking input parity? */
		line->default_param.cor1 |= IGNORE_P;
			/* nope, ignore input parity */
	    line->default_param.cor1 |= NORMAL_P;
		    /* do normal parity processing */
	    if (cflag & PARODD)
		line->default_param.cor1 |= ODD_P;
	} /* cor1 = 0 case is no parity at all */

	/*
	 * check for CTS/RTS flow control
	 */
	if (cflag & (CRTSCTS|CRTSXOFF))
		line->default_param.cor2 |= FLOW_RTSCTS;
	else
		line->default_param.cor2 &= ~FLOW_RTSCTS;

	/*
	 * check for XON/XOFF flow control
	 */
	if (iflag & IXON) {
		line->default_param.schr2 = stc_defaults->termios.c_cc[VSTOP];
			/* Xoff */
		line->default_param.schr1 = stc_defaults->termios.c_cc[VSTART];
			/* Xon */
		line->default_param.cor3 &= ~(XON_13|XOFF_24);
			/* use schr1 as XON/schr2 as XOFF */
		line->default_param.cor3 |= (NO_FLOW_EXP|SCHR_DET);
		line->default_param.cor2 |= FLOW_IXON;
		if (iflag & IXANY)
			line->default_param.cor2 |= FLOW_IXANY;
		else
			line->default_param.cor2 &= ~FLOW_IXANY;
	} else {
		line->default_param.cor2 &= ~(FLOW_IXON|FLOW_IXANY);
		line->default_param.cor3 &= ~(NO_FLOW_EXP|SCHR_DET);
	}

	/*
	 * if we're holding DTR high by using soft_carrier and stc_dtrclose
	 * then we need to raise it here if the line's not open
	 * if we're coming here to set the line's defaults and the line itself
	 * is open, then don't actually change the state of DTR; when the line
	 * gets closed, DTR will be set to the correct state
	 */
	if (!(line->state & (STC_LP | STC_CONTROL))) {
		if (!(line->state & (STC_ISOPEN | STC_WOPEN | STC_WCLOSE))) {
			if ((line->stc_ttycommon.t_flags & TS_SOFTCAR) &&
				(line->default_param.sd.flags & STC_DTRCLOSE)) {
				DTR_SET(line, DTR_ON);
			} else {
				DTR_SET(line, DTR_OFF);
			}
		} else { /* line is open, so just set the flag */
			if (line->default_param.sd.flags & STC_DTRCLOSE)
				line->flags |= STC_DTRCLOSE;
			else
				line->flags &= ~STC_DTRCLOSE;
		} /* if (!STC_ISOPEN...) */
	} /* if (!STC_LP|STC_CONTROL) */

	/*
	 * if the user wants DTR always on no matter what, then assert it right
	 * here; don't worry if the line is already open or not
	 */
	if (!(line->state & (STC_LP | STC_CONTROL))) {
	    if (line->default_param.sd.flags & STC_DTRFORCE) {
		line->flags |= STC_DTRFORCE;
		DTR_SET(line, DTR_ON);
	    } else {
		line->flags &= ~STC_DTRFORCE;
	    }
	}

}

/*
 * ------------------------------ ppc routines --------------------------------
 *
 * ppc_poll(stc) - gets called whenever the ppc interrupts
 *
 * return:	1 if one or more interrupts has been serviced
 *		0 if not
 */
static u_int
ppc_poll(caddr_t arg)
{
	int serviced = DDI_INTR_UNCLAIMED;
	uchar_t ipstat;
	struct stc_unit_t *stc = (struct stc_unit_t *)arg;

	SDM(STC_TRACK, (CE_CONT, "ppc_poll: we're here...\n"));

	/*
	 * we only get interrupted by the *ACK line
	 */
	if (stc->flags & STC_ATTACHOK) {
	    ipstat = stc->ppc->ipstat;
	    if (ipstat == PPC_INTMASK) {
		serviced = ppc_int(stc);
	    }
	} /* STC_ATTACHOK */

	SDM(STC_TRACK, (CE_CONT,
		"------(LEAVING ppc_poll, serviced=%d)-----\n", serviced));
	return (serviced);
}

/*
 * ppc_int(stc) - handles ppc *ACK interrupts
 *
 * note that we can only deal with one char at a time, so stc_start() had
 * better not put more than that into the tx_buf
 * the exception conditions like PAPER OUT and BUSY are handled in stc_start()
 */
static int
ppc_int(struct stc_unit_t *stc)
{
	struct stc_line_t *line = &stc->ppc_line;
	struct ppcregs_t *regs = stc->ppc;
	timeout_id_t ppc_acktimeout_id = 0;
	timeout_id_t ppc_out_id = 0;

	SDM(STC_DEBUG, (CE_CONT,
		"---------------(unit %d)------(ppc_int)---------\n",
		line->unit_no));

	mutex_enter(line->line_mutex);

	/*
	 * By default, disable interrupts...ppc_out will reenable
	 * only if actually waiting for ACK interrupt to come
	 * in when printer is done printing character. This
	 * prevents continuous interrupt stream from hanging
	 * system when printer is powered off (but connected)
	 */
	mutex_enter(stc->ppc_mutex);
	line->pcon_s &= ~PPC_IRQE;
	regs->pcon = line->pcon_s;
	mutex_exit(stc->ppc_mutex);

	/*
	 * take the ACK timeout timer off of the timer queue
	 */
	line->ack_loops = 0;
	line->state &= ~STC_ACKWAIT;
	if (line->ppc_acktimeout_id) {
		ppc_acktimeout_id = line->ppc_acktimeout_id;
		line->ppc_acktimeout_id = 0;
	}

	/*
	 * also remove the busy timer and clear the busy timer counter
	 * this timer could also have been set by the code that waits for
	 * an error condition to clear itself; if so, and the error condition
	 * still persists, we'll catch it in the next call to ppc_out()
	 */
	line->busy_cnt = 0;
	if (line->ppc_out_id) {
		ppc_out_id = line->ppc_out_id;
		line->ppc_out_id = 0;
	}

	if (ppc_acktimeout_id || ppc_out_id) {
		mutex_exit(line->line_mutex);
		if (ppc_acktimeout_id)
			(void) untimeout(ppc_acktimeout_id);
		if (ppc_out_id)
			(void) untimeout(ppc_out_id);
		mutex_enter(line->line_mutex);
	}

	/*
	 * if we get a stray interrupt, just ack it to ppc_poll() and then drop
	 * it on the floor and let the user deal with the error message on the
	 * console
	 */
	if (!(line->state & STC_ISOPEN)) {
		cmn_err(CE_CONT,
			"ppc_int: unit %d stray interrupt\n", line->unit_no);
		mutex_exit(line->line_mutex);
		return (DDI_INTR_CLAIMED);
	}

	/*
	 * if we were waiting for all output to drain, then wakeup the sleep
	 * in ppc_xwait()
	 * don't forget to remove the timer that ppc_xwait() posted for us
	 */
	if (line->state & STC_XWAIT) {
		line->state &= ~STC_XWAIT;
		UNTIMEOUT(line->stc_timeout_id);
		SDM(STC_DEBUG, (CE_CONT, "ppc_int: STC_XWAIT timeout\n"));
		cv_broadcast(line->cvp);
	}

	/*
	 * if we've still got data to send in our local buffer,
	 * send it otherwise call stc_start() to fill stc_txbuf
	 * keep STC_BUSY so that stc_start doesn't dump any more
	 * characters into our buffer until we're ready for them
	 */
	if (!(line->stc_txcount)) {
		STC_LOGIT(line->line_no, 0, 0x5049, line->state);
		line->state &= ~STC_BUSY;
		stc_start(line);
		mutex_exit(line->line_mutex);
	} else {
		mutex_exit(line->line_mutex);
		ppc_out(line);
	}

	SDM(STC_TRACK, (CE_CONT, "-----(unit %d)------(LEAVING ppc_int)-----\n",
				line->unit_no));

	return (DDI_INTR_CLAIMED);
}

/*
 * ppc_out(line) - send a char to the ppc and enable interrupts
 */
static void
ppc_out(void *arg)
{
	struct stc_line_t *line = arg;
	struct stc_unit_t *stc = line->stc;
	struct ppcregs_t *regs = stc->ppc;
	int i, busy_timeout;

	SDM(STC_TRACK, (CE_CONT,
		"----------(ppc_out)----(starting, unit: %d)----------\n",
		line->unit_no));

	/*
	 * if we're discarding output, then don't bother to check the printer
	 * status or send any data to it, just return - this prevents an error
	 * condition on the printer from keeping us stuck in this loop
	 */
	if (line->state & STC_DRAIN)
		return;

	/*
	 * if we see a PAPER OUT, off-line or ERROR condition, keep trying until
	 * it clears itself
	 *
	 * this call to ppc_stat() handles the case where ACK comes from the
	 * printer BEFORE the error condition is cleared, but we shouldn't send
	 * another char until the user fixes whatever's wrong with the printer
	 *
	 * note that these conditions take precedence over BUSY, so that we
	 * (shouldn't) generate a BUSY timeout if we're just out of paper or
	 * off line the only time we can get a BUSY timeout is if the printer
	 * says everything is OK (i.e. loaded with paper, on-line, no error),
	 * but BUSY is still asserted for a long period of time
	 *
	 * the user will get a PP_SIGTYPE signal if there's an error condition
	 * and they've * set the PP_SIGNAL bit in the line->flags field of the
	 * line struct
	 */
	mutex_enter(line->line_mutex);
	if (ppc_stat(regs, line)) {
		line->ppc_out_id = timeout(ppc_out, line, line->error_timeout);
		mutex_exit(line->line_mutex);
		return;
	}

	/*
	 * if we're busy, loop a few times and see if we become not busy;
	 * if we're still busy after all that, post a timer to call us back
	 * later and return if we run out of times to wait for busy to go away,
	 * call ppc_acktimeout() as one last shot
	 */
	i = 0;
	while (PPC_BUSY(regs->pstat, line->flags)) {
		if (i++ > BUSY_LOOP_C)
			break;
	}

	if (PPC_BUSY(regs->pstat, line->flags)) {
		/*
		 * if we're not using a BUSY timeout, then force the BUSY loop
		 * counter to zero so that we stay here as long as BUSY is
		 * asserted; set the poll time to periodically poll to see if
		 * BUSY has been deasserted
		 */
		if (!(line->busy_timeout)) {
			line->busy_cnt = 0;
			busy_timeout = (NOBUSY_POLLTIME * hz);
		} else {
			busy_timeout = line->busy_timeout;
		}
		if (line->busy_cnt++ >= BUSY_COUNT_C) {
			mutex_exit(line->line_mutex);
			ppc_acktimeout(line);
			return;
		} else {
			line->ppc_out_id = timeout(ppc_out, line, busy_timeout);
			mutex_exit(line->line_mutex);
			return;
		}
	}

	/*
	 * send the data from the local buffer, we should never get here
	 * with no data to send
	 * stc_txcount and stc_txbufp are set to the right values when
	 * stc_start() fills the tx buffer up with data
	 */
	mutex_enter(stc->ppc_mutex);
	regs->pdata = *(line->stc_txbufp++);
	line->stc_txcount--;

	/*
	 * provide a data setup time in case it's specified, then strobe the
	 * data to the printer.
	 * start a timer in case we don't hear from the printer in a while
	 */
	if (line->data_setup)
		drv_usecwait(line->data_setup);
	line->pcon_s |= PPC_IRQE;
	STROBE(regs->pcon, line->strobe_w, line->pcon_s);
	regs->pcon = line->pcon_s;
	mutex_exit(stc->ppc_mutex);
	line->ppc_acktimeout_id = timeout(ppc_acktimeout, line,
	    line->ack_timeout);
	mutex_exit(line->line_mutex);

	SDM(STC_TRACK, (CE_CONT,
		"----------(ppc_out)----(LEAVING)----------\n"));
}

/*
 * ppc_acktimeout(line) - handle ACK timeouts
 *
 * we come here every PPC_ACK_POLL seconds
 */
static void
ppc_acktimeout(void *arg)
{
	struct stc_line_t *line = arg;
	struct stc_unit_t *stc = line->stc;
	struct ppcregs_t *regs = stc->ppc;
	queue_t *q;
	int busy_timeout;

	/*
	 * if STC_DRAIN is set, we were called from somewhere other than the ACK
	 * timer or the BUSY timer, so ignore any further handshaking semantics
	 * and just flush the rest of the data and send an error to the user
	 * because either the ACK or BUSY handshake timed out or the user
	 * requested that we deep-6 this line via the control device
	 */

	mutex_enter(line->line_mutex);
	if (line->ppc_acktimeout_id == 0) {
		mutex_exit(line->line_mutex);
		return;
	}
	if (!(line->state & STC_DRAIN)) {
		/*
		 * if we see a PAPER OUT, off-line or ERROR condition,
		 * keep trying until it clears itself
		 *
		 * this code handles the case where ACK comes from the printer
		 * AFTER the error condition is cleared
		 *
		 * set STC_ACKWAIT so that if we get here again and there's no
		 * error condition we can tell if we came here because of a
		 * bona-fide ACK timeout or because of an error condition that
		 * cleared itself the user will get a PP_SIGTYPE signal if
		 * there's an error condition and they've set the PP_SIGNAL
		 * bit in the line->flags field of the line struct
		 */
		if (ppc_stat(regs, line)) {
			line->state |= STC_ACKWAIT;
			line->ppc_acktimeout_id = timeout(ppc_acktimeout,
			    line, line->error_timeout);
			mutex_exit(line->line_mutex);
			return;
		}

		/*
		 * if we got here because an error condition cleared itself,
		 * wait one more time for ACK to be asserted; if it is,
		 * ppc_int() will remove us from the the timer queue and
		 * we'll never get back here use the "error_timeout" time
		 * to give the user better response after they have cleared
		 * the error condition
		 */
		if (line->state & STC_ACKWAIT) {
			line->state &= ~STC_ACKWAIT;
			line->ppc_acktimeout_id = timeout(ppc_acktimeout,
			    line, line->error_timeout);
			mutex_exit(line->line_mutex);
			return;
		}

		/*
		 * if we get here, that means that none of the printer status
		 * signals such as ERROR, OFFLINE or PAPEROUT are asserted,
		 * and we can check for BUSY and ACK timeouts.	Note that when
		 * we fall through this code, it means that we either
		 * detected a BUSY or an ACK timeout; while we're waiting for
		 * either timeout to occur, we post a timeout(ppc_acktimeout)
		 * and do a return () so that if the printer responds by
		 * deasserting its BUSY signal or by generating an ACK before
		 * the timeout period, ppc_int() will take us off of the
		 * timeout queue
		 */
		if (PPC_BUSY(regs->pstat, line->flags)) {
		/*
		 * if BUSY is asserted, check to see if we want an
		 * infinite BUSY timeout, in which case just keep calling
		 * this code periodically, checking for BUSY to go away
		 */
		    if (!(line->busy_timeout)) {
			line->busy_cnt = 0;
			busy_timeout = (NOBUSY_POLLTIME * hz);
		    } else {
			busy_timeout = line->busy_timeout;
		    }
		    if (line->busy_cnt++ >= BUSY_COUNT_C) {
			line->busy_cnt = 0;
			cmn_err(CE_CONT,
				"ppc_acktimeout: unit %d BUSY timeout\n",
				line->unit_no);
		    } else {
			line->ppc_acktimeout_id = timeout(ppc_acktimeout,
			    line, busy_timeout);
			mutex_exit(line->line_mutex);
			return;
		    }
		} else {
			/*
			 * if we're here on what might be a bona-fide ACK
			 * timeout, check the timeout loop counter and if
			 * we've expired it, * then this must really be a
			 * timeout
			 */
		    if (line->ack_loops++ < line->ack_maxloops) {
			line->ppc_acktimeout_id = timeout(ppc_acktimeout,
			    line, line->ack_timeout);
			mutex_exit(line->line_mutex);
			return;
		    } else {
			line->ack_loops = 0;
			cmn_err(CE_CONT,
				"ppc_acktimeout: unit %d ACK timeout\n",
				line->unit_no);
		    }
		}
	} /* !STC_DRAIN */

	/*
	 * tell stc_start that we're draining all M_DATA messages
	 * if we got here because of a timer pop posted by ppc_xwait(), then
	 * setting STC_DRAIN will cause ppc_xwait() to fall out of the
	 * while (...) loop and complete the close
	 */
	line->state |= STC_DRAIN;
	mutex_exit(line->line_mutex);

	if ((q = line->stc_ttycommon.t_readq) == NULL) {
		cmn_err(CE_CONT,
			"ppc_acktimeout: unit %d can't get pointer to read q\n",
			line->unit_no);
	} else {
		if (!(putnextctl1(q, M_ERROR, EIO))) {
			cmn_err(CE_CONT,
			"ppc_acktimeout: unit %d can't send M_ERROR message\n",
				line->unit_no);
		}
	}

	/*
	 * if we were waiting for all output to drain, then wakeup the sleep
	 * in ppc_xwait()
	 * we got here because the timer posted by ppc_xwait() expired and
	 * apparently the printer is hosed
	 */
	if (line->state & STC_XWAIT) {
		SDM(STC_DEBUG, (CE_CONT,
			"ppc_acktimeout: STC_XWAIT timeout\n"));
		cv_broadcast(line->cvp);
	} else {
	/*
	 * this call to stc_start() will flush the rest of the messages so
	 * that the lower portions of the STREAMS code don't wait forever
	 */
		mutex_enter(line->line_mutex);
		stc_start(line);
		mutex_exit(line->line_mutex);
	}
}

/*
 * ppc_xwait(line) - waits for ppc data to drain
 */
static void
ppc_xwait(struct stc_line_t *line)
{
	SDM(STC_TRACK, (CE_CONT,
		"----------(ppc_xwait)----(unit %d, line %d)----------\n",
		line->unit_no, line->line_no));

	/*
	 * wait until we have sent all the bytes from the driver's local buffer
	 * we will be woken up by ppc_int() or ppc_acktimeout() after each
	 * transfer if we're draining because of an error or timeout,
	 * don't bother to wait for the rest of the data to get to the
	 * printer because it will probably timeout in the transfer anyway,
	 * so just return and let stc_close() shut down the
	 * ppc and zap the line
	 */
	mutex_enter(line->line_mutex);
	while (!(line->state & STC_DRAIN) && line->stc_txcount) {
	    line->state |= STC_XWAIT;
	    line->ack_loops = 0;
	    line->stc_timeout_id = timeout(ppc_acktimeout,
		line, line->ack_timeout);
	    if (!cv_wait_sig(line->cvp, line->line_mutex)) {
		SDM(STC_DEBUG, (CE_CONT,
			"ppc_xwait: unit %d interrupted sleep\n",
			line->unit_no));
		break;
	    }
	}

	/*
	 * set the drain flag so that ppc_out() doesn't loop around
	 * waiting for the printer to become ready any more, and rip
	 * out any timers that ppc_acktimeout() might have queued up for us
	 */
	line->state |= STC_DRAIN;
	mutex_exit(line->line_mutex);

	UNTIMEOUT(line->stc_timeout_id);
	UNTIMEOUT(line->ppc_acktimeout_id);
	UNTIMEOUT(line->ppc_out_id);

	SDM(STC_TRACK, (CE_CONT,
		"----------(ppc_xwait)----(DONE)----------\n"));
}

/*
 * ppc_stat(regs, line) - checks status of printer interface
 *
 *    calling:
 *		regs - pointer to ppc registers
 *		line - pointer to line struct
 *
 *    returns:
 *		1 if PAPER OUT, OFFLINE or ERROR
 *		0 if none of the above are true
 */
static int
ppc_stat(struct ppcregs_t *regs, struct stc_line_t *line)
{
	register int ssig = 0;

	/*
	 * check the various conditions that the printer can report back to us
	 * the funny games with the flags are played so that we only get one
	 * message for each type of occurance rather than a message every time
	 * we go around this loop
	 * note that each of these macros check the line->flags field, and if
	 * the user specified to ignore a particular signal, the macros will
	 * return the correct value
	 *
	 * check PAPER OUT line
	 */
	if (PPC_PAPER_OUT(regs->pstat, line->flags)) {
	    if (DO_PPC_ERRMSG(line))
		line->pstate &= ~PP_PAPER_OUT;
		if (!(line->pstate & PP_PAPER_OUT)) {
			line->pstate |= PP_PAPER_OUT;
			ssig = 1;
			cmn_err(CE_CONT,
				"ppc_stat: unit %d PAPER OUT\n", line->unit_no);
		}
	} else {
		if (line->pstate & PP_PAPER_OUT) {
			line->pstate &= ~PP_PAPER_OUT;
			cmn_err(CE_CONT,
			"ppc_stat: unit %d PAPER OUT condition cleared\n",
				line->unit_no);
		}
	}

	/*
	 * check SELECT line - this should probably be called OFFLINE, but...
	 * SELECT being FALSE is interpreted as an error condition
	 */
	if (!PPC_SELECT(regs->pstat, line->flags)) {
	    if (DO_PPC_ERRMSG(line))
		line->pstate &= ~PP_SELECT;
		if (!(line->pstate & PP_SELECT)) {
			line->pstate |= PP_SELECT;
			ssig = 1;
			cmn_err(CE_CONT,
				"ppc_stat: unit %d OFFLINE\n", line->unit_no);
		}
	} else {
		if (line->pstate & PP_SELECT) {
			line->pstate &= ~PP_SELECT;
			cmn_err(CE_CONT,
				"ppc_stat: unit %d OFFLINE condition cleared\n",
				line->unit_no);
		}
	}

	/*
	 * check ERROR line
	 */
	if (PPC_ERROR(regs->pstat, line->flags)) {
	    if (DO_PPC_ERRMSG(line))
		line->pstate &= ~PP_ERROR;
		if (!(line->pstate & PP_ERROR)) {
			line->pstate |= PP_ERROR;
			ssig = 1;
			cmn_err(CE_CONT,
				"ppc_stat: unit %d ERROR\n", line->unit_no);
		}
	} else {
		if (line->pstate & PP_ERROR) {
			line->pstate &= ~PP_ERROR;
			cmn_err(CE_CONT,
				"ppc_stat: unit %d ERROR condition cleared\n",
				line->unit_no);
		}
	}

	/*
	 * if we detected an error condition on this go-round that we didn't see
	 * before, send the user a signal if they asked for one
	 */
	if (ssig)
		ppc_signal(line, PP_SIGTYPE);

	/*
	 * if any of the error conditions were true, then return 1
	 * update the error message counter so that the next time through we'll
	 * get another error message if it's time to print one
	 */
	if (line->pstate & (PP_PAPER_OUT | PP_ERROR | PP_SELECT)) {
		if ((line->flags & PP_MSG) &&
			(line->error_cnt++
				> (PPC_ERRMSG_TIME/(line->error_timeout))))
			line->error_cnt = 0;
		return (1);
	}

	line->error_cnt = 0;
	return (0);
}

/*
 * ppc_signal(line, sig_type) - send a sig_type signal to the user process
 * if PP_SIGNAL is set in the line->flags field of the line struct
 */
static void
ppc_signal(struct stc_line_t *line, int sig_type)
{
	register queue_t *q;

	SDM(STC_TRACK, (CE_CONT,
		"--------------(ppc_signal)--------(unit %d)--------\n",
		line->unit_no));

	if (line->flags & PP_SIGNAL) {
		if ((q = line->stc_ttycommon.t_readq) == NULL) {
			cmn_err(CE_CONT,
			"ppc_signal: unit %d can't get pointer to read q\n",
				line->unit_no);
		} else {
			if (!(putnextctl1(q, M_PCSIG, sig_type))) {
				cmn_err(CE_CONT,
	"ppc_signal: unit %d can't send M_PCSIG(PP_SIGTYPE 0x%x) message\n",
					line->unit_no, sig_type);
			} else {
				/*EMPTY*/ /* if DEBUG_STC not set */
				SDM(STC_DEBUG, (CE_CONT,
			"ppc_signal: sent M_PCSIG(0x%x) message unit %d\n",
					sig_type, line->unit_no));
			}
		}
	}

	SDM(STC_TRACK, (CE_CONT,
		"--------------(ppc_signal)--------(LEAVING)--------\n",
		line->unit_no));
}

#ifdef	DEBUG_STC
static void
print_bits(int bits)
{

	cmn_err(CE_CONT, "print_bits: 0x%x\n", bits);
	if (bits & TIOCM_LE)
		cmn_err(CE_CONT, "TIOCM_LE ");
	if (bits & TIOCM_DTR)
		cmn_err(CE_CONT, "TIOCM_DTR ");
	if (bits & TIOCM_RTS)
		cmn_err(CE_CONT, "TIOCM_RTS ");
	if (bits & TIOCM_ST)
		cmn_err(CE_CONT, "TIOCM_ST ");
	if (bits & TIOCM_SR)
		cmn_err(CE_CONT, "TIOCM_SR ");
	if (bits & TIOCM_CTS)
		cmn_err(CE_CONT, "TIOCM_CTS ");
	if (bits & TIOCM_CAR)
		cmn_err(CE_CONT, "TIOCM_CAR ");
	if (bits & TIOCM_RI)
		cmn_err(CE_CONT, "TIOCM_RI ");
	if (bits & TIOCM_DSR)
		cmn_err(CE_CONT, "TIOCM_DSR ");
	cmn_err(CE_CONT, "\n");
}

static void
ioctl2text(char *tag, int ioc_cmd)
{
	int i = 0, found = 0;

	while (ioc_txt[i].name) {
		if (ioc_txt[i].ioc_cmd == ioc_cmd) {
			if (!found) {
				cmn_err(CE_CONT, "%s: M_IOCTL(0x%x) [%s]\n",
					tag, ioc_cmd, ioc_txt[i].name);
			} else {
				cmn_err(CE_CONT, "... and M_IOCTL(0x%x) [%s]\n",
					ioc_cmd, ioc_txt[i].name);
			}
			found = 1;
		}
		i++;
	}

	if (!found)
	cmn_err(CE_CONT, "%s: M_IOCTL(0x%x) [(unknown)]\n", tag, ioc_cmd);

}
#endif	DEBUG_STC
