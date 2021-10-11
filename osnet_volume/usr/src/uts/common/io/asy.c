/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)asy.c	1.87	99/09/27 SMI"

/*
 *	Serial I/O driver for 82510/8250/16450/16550AF chips.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/termio.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/strtty.h>
#include <sys/debug.h>
#include <sys/kbio.h>
#include <sys/cred.h>
#include <sys/stat.h>
#include <sys/consdev.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#ifdef DEBUG
#include <sys/promif.h>
#endif
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/asy.h>

#ifdef __ia64
/*
 * Dump simulator output to a log file
 */
#include <sys/types_ia64.h>
#include <sys/ssc.h>

static int serial_console_active = 0;

extern void *physical(void *);
extern int prom_stdin_is_keyboard(void);

void
SscDbgasy(uchar_t c)
{
	uchar_t dbuf[2];

	if (serial_console_active) {
		dbuf[0] = c;
		dbuf[1] = '\0';
		SscDbgPrintf(physical(dbuf));
	}
}
#endif

/*
 * set the FIFO trigger_level to 8 bytes for now
 * we may want to make this configurable later.
 */
static	int asy_trig_level = FIFO_TRIG_8;

#define	MAXASY	10

#define	async_stopc	async_ttycommon.t_stopc
#define	async_startc	async_ttycommon.t_startc

static	int asy_ocflags[MAXASY];	/* old cflags used in asy_program() */

#define	ASY_INIT	1
#define	ASY_NOINIT	0

#ifdef DEBUG
#define	ASY_DEBUG_INIT	0x001
#define	ASY_DEBUG_INPUT	0x002
#define	ASY_DEBUG_EOT	0x004
#define	ASY_DEBUG_CLOSE	0x008
#define	ASY_DEBUG_HFLOW	0x010
#define	ASY_DEBUG_PROCS	0x020
#define	ASY_DEBUG_STATE	0x040
#define	ASY_DEBUG_INTR	0x080
static	int asydebug = 0;
#endif

/*
 * This is a hack to keep 2.4 and 2.5 sources in sync. CRTSXOFF was
 * introduced in 2.5. In 2.4 we map them to the same flag.
 */
#ifndef	CRTSXOFF
#define	CRTSXOFF	CRTSCTS
#endif

#ifndef	CBAUDEXT
#define	CBAUDEXT	010000000
#define	CIBAUDEXT	020000000
#endif

#ifdef	TIOCGPPS
/*
 * PPS (Pulse Per Second) support.
 */
void ddi_hardpps();
/*
 * This is protected by the asy_excl_hi of the port on which PPS event
 * handling is enabled.  Note that only one port should have this enabled at
 * any one time.  Enabling PPS handling on multiple ports will result in
 * unpredictable (but benign) results.
 */
static struct ppsclockev asy_ppsev;

#ifdef PPSCLOCKLED
/* XXX Use these to observe PPS latencies and jitter on a scope */
#define	LED_ON
#define	LED_OFF
#else
#define	LED_ON
#define	LED_OFF
#endif
#endif

static	int nasy = 0;
static	int max_asy_instance = -1;
static	int maxasy = MAXASY;	/* Maximum no. of asy ports supported XXX */
static	struct asycom *asycom;
static	struct asyncline *asyncline;

static	uint_t	asysoftintr(caddr_t intarg);
static	uint_t	asyintr(caddr_t argasy);

static boolean_t abort_charseq_recognize(uchar_t ch);

/* The async interrupt entry points */
static void	async_txint(struct asycom *asy);
static void	async_rxint(struct asycom *asy, uchar_t lsr);
static void	async_msint(struct asycom *asy);
static void	async_softint(struct asycom *asy);

static void	async_ioctl(struct asyncline *async, queue_t *q, mblk_t *mp);
static void	async_reioctl(void *unit);
static void	async_iocdata(queue_t *q, mblk_t *mp);
static int	async_restart(struct asyncline *async);
static void	async_start(struct asyncline *async);
static void	async_nstart(struct asyncline *async, int mode);
static void	async_resume(struct asyncline *async);
static void	asy_program(struct asycom *asy, int mode);
static void	asyinit(struct asycom *asy);
static void	asy_waiteot(struct asycom *asy);
static void	asyputchar(struct cons_polledio_arg *, uchar_t c);
static int	asygetchar(struct cons_polledio_arg *);
static boolean_t	asyischar(struct cons_polledio_arg *);

static int	asymctl(struct asycom *, int, int);
static int	asytodm(int, int);
static int	dmtoasy(int);
static void	asyerror(int level, char *fmt, ...);
static void	asy_parse_mode(dev_info_t *devi, struct asycom *asy,
				int instance);
static char	asy_port_to_name(caddr_t port, int instance);

#define	GET_PROP(devi, pname, pflag, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
		(pflag), (pname), (caddr_t)(pval), (plen)))

static ddi_iblock_cookie_t asy_soft_iblock;
ddi_softintr_t asy_softintr_id;
static	int asy_addedsoft = 0;
int	asysoftpend;	/* soft interrupt pending */
kmutex_t asy_soft_lock;	/* lock protecting asysoftpend */
kmutex_t asy_glob_lock; /* lock protecting global data manipulation */
kcondvar_t asy_close_cv;

/*
 * Baud rate table. Indexed by #defines found in sys/termios.h
 */
ushort_t asyspdtab[] = {
	0,	/* 0 baud rate */
	0x900,	/* 50 baud rate */
	0x600,	/* 75 baud rate */
	0x417,	/* 110 baud rate (%0.026) */
	0x359,	/* 134 baud rate (%0.058) */
	0x300,	/* 150 baud rate */
	0x240,	/* 200 baud rate */
	0x180,	/* 300 baud rate */
	0x0c0,	/* 600 baud rate */
	0x060,	/* 1200 baud rate */
	0x040,	/* 1800 baud rate */
	0x030,	/* 2400 baud rate */
	0x018,	/* 4800 baud rate */
	0x00c,	/* 9600 baud rate */
	0x006,	/* 19200 baud rate */
	0x003,	/* 38400 baud rate */

	0x002,	/* 57600 baud rate */
	0x0,	/* 76800 baud rate not supported */
	0x001,	/* 115200 baud rate */
	0x0,	/* 153600 baud rate not supported */
	0x0,	/* 0x8002 (SMC chip) 230400 baud rate not supported */
	0x0,	/* 307200 baud rate not supported */
	0x0,	/* 0x8001 (SMC chip) 460800 baud rate not supported */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
};

static int asyrsrv(queue_t *q);
static int asyopen(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr);
static int asyclose(queue_t *q, int flag, cred_t *credp);
static int asywput(queue_t *q, mblk_t *mp);

struct module_info asy_info = {
	0,
	"asy",
	0,
	INFPSZ,
	4096,
	128
};

static struct qinit asy_rint = {
	putq,
	asyrsrv,
	asyopen,
	asyclose,
	NULL,
	&asy_info,
	NULL
};

static struct qinit asy_wint = {
	asywput,
	NULL,
	NULL,
	NULL,
	NULL,
	&asy_info,
	NULL
};

struct streamtab asy_str_info = {
	&asy_rint,
	&asy_wint,
	NULL,
	NULL
};

static int asyinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int asyprobe(dev_info_t *);
static int asyattach(dev_info_t *, ddi_attach_cmd_t);
static int asydetach(dev_info_t *, ddi_detach_cmd_t);

static 	struct cb_ops cb_asy_ops = {
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
	&asy_str_info,		/* cb_stream */
	D_NEW | D_MP		/* cb_flag */
};

struct dev_ops asy_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	asyinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	asyprobe,		/* devo_probe */
	asyattach,		/* devo_attach */
	asydetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_asy_ops,		/* devo_cb_ops */
};

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"ASY driver",
	&asy_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	int i;

	mutex_init(&asy_glob_lock, NULL, MUTEX_DRIVER, NULL);
	if ((i = mod_install(&modlinkage)) != 0)
		mutex_destroy(&asy_glob_lock);
	return (i);
}

int
_fini(void)
{
	int i;

	if ((i = mod_remove(&modlinkage)) == 0) {
		mutex_destroy(&asy_glob_lock);
	}
	return (i);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
asyprobe(dev_info_t *devi)
{
	int instance;
	int stat;
	int regnum = 0;
	uint8_t	*ioaddr;
	ddi_acc_handle_t iohandle;
	static ddi_device_acc_attr_t ioattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

#if !defined(__ia64)
/* BEGIN CSTYLED */
	{
/* END CSTYLED */
		int reglen, nregs;
		int i;
		struct {
			int bustype;
			int base;
			int size;
		} *reglist;

		/* new probe */
		if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, "asyprobe: reg property not found "
			    "in devices property list");
			return (DDI_PROBE_FAILURE);
		}
		nregs = reglen / sizeof (*reglist);
		for (i = 0; i < nregs; i++) {
			if (reglist[i].bustype == 1) {
				regnum = i;
				break;
			}
		}
		kmem_free(reglist, reglen);
	}
#endif /* __ia64 */

	if (ddi_regs_map_setup(devi, regnum, (caddr_t *)&ioaddr,
	    (offset_t)0, (offset_t)0, &ioattr, &iohandle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "asyprobe: could not map registers");
		return (DDI_PROBE_FAILURE);
	}

	/*
	 * Probe for the device:
	 * 	Ser. int. uses bits 0,1,2; FIFO uses 3,6,7; 4,5 wired low.
	 * 	If bit 4 or 5 appears on ISR, board is not there.
	 */
	if (ddi_io_get8(iohandle, ioaddr + ISR) & 0x30) {
		stat = DDI_PROBE_FAILURE;
	} else {
		mutex_enter(&asy_glob_lock);
		if (nasy < maxasy) {
			nasy++;
			instance = ddi_get_instance(devi);
			if ((instance < maxasy) &&
			    (max_asy_instance < instance))
				max_asy_instance = instance;
		}
		stat = DDI_PROBE_SUCCESS;
		mutex_exit(&asy_glob_lock);
	}
	ddi_regs_map_free(&iohandle);
	return (stat);
}

static int
asydetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct asycom *asy;
	char name[16];

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(devi);	/* find out which unit */

	asy = &asycom[instance];

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_INIT)
		cmn_err(CE_NOTE, "asy%d: ASY%s shutdown.",
			instance,
			asy->asy_hwtype == ASY82510 ? "82510" :
			asy->asy_hwtype == ASY16550AF ? "16550AF" : "8250");
#endif

	/* remove minor device node(s) for this device */
	(void) sprintf(name, "%c", (instance + 'a'));	/* serial-port */
	ddi_remove_minor_node(devi, name);
	/* serial-port:dailout */
	(void) sprintf(name, "%c,cu", (instance + 'a'));
	ddi_remove_minor_node(devi, name);

	mutex_destroy(&asy->asy_excl);
	mutex_destroy(&asy->asy_excl_hi);
	cv_destroy(&asyncline[asy->asy_unit].async_flags_cv);
	ddi_remove_intr(devi, 0, asy->asy_iblock);
	ddi_regs_map_free(&asy->asy_iohandle);
	mutex_enter(&asy_glob_lock);
	nasy--;
	if (nasy == 0) {
		if (asy_addedsoft)
			ddi_remove_softintr(asy_softintr_id);
		asy_addedsoft = 0;
		mutex_destroy(&asy_soft_lock);
		kmem_free(asycom, maxasy * sizeof (struct asycom));
		asycom = NULL;
		kmem_free(asyncline, maxasy * sizeof (struct asyncline));
		asyncline = NULL;
		cv_destroy(&asy_close_cv);
#ifdef DEBUG
		if (asydebug & ASY_DEBUG_INIT)
			cmn_err(CE_CONT, "The last of asy driver's removed\n");
#endif
	}
	mutex_exit(&asy_glob_lock);
	return (DDI_SUCCESS);
}

static char
asy_port_to_name(caddr_t port, int instance)
{
	char c;

	switch ((int)port) {
		case COM1_IOADDR: c = 'a'; break;
		case COM2_IOADDR: c = 'b'; break;
		case COM3_IOADDR: c = 'c'; break;
		case COM4_IOADDR: c = 'd'; break;
		default: c = 'e' + instance; break;
	}
	return (c);
}

static int
asyattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	int mcr;
	int len;
	int ret;
	int regnum = 0;
	struct asycom *asy;
	char name[40];
	char val[40];
	int status;
	static ddi_device_acc_attr_t ioattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(devi);	/* find out which unit */

	if (instance > max_asy_instance)
		return (DDI_FAILURE);

	mutex_enter(&asy_glob_lock);
	if (asycom == NULL) { /* allocate asycom structures for maxasy ports */
		/*
		 * Note: may be we should use ddi_soft_state mechanism.
		 * CHECK
		 */
		asycom = (struct asycom *)
			kmem_zalloc(maxasy * sizeof (struct asycom), KM_SLEEP);
	}
	mutex_exit(&asy_glob_lock);

	asy = &asycom[instance];

#if !defined(__ia64)
/* BEGIN CSTYLED */
	{
/* END CSTYLED */
		int reglen, nregs;
		int i;
		struct {
			int bustype;
			int base;
			int size;
		} *reglist;

		/* new probe */
		if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
			mutex_enter(&asy_glob_lock);
			if (nasy == 0) {
				kmem_free(asycom,
					maxasy * sizeof (struct asycom));
				asycom = NULL;
			}
			mutex_exit(&asy_glob_lock);
			cmn_err(CE_WARN, "asyattach: reg property not found "
			    "in devices property list");
			return (DDI_PROBE_FAILURE);
		}
		nregs = reglen / sizeof (*reglist);
		for (i = 0; i < nregs; i++) {
			if (reglist[i].bustype == 1) {
				regnum = i;
				break;
			}
		}
		kmem_free(reglist, reglen);
	}
#endif /* __ia64 */

	if (ddi_regs_map_setup(devi, regnum, (caddr_t *)&asy->asy_ioaddr,
		(offset_t)0, (offset_t)0, &ioattr, &asy->asy_iohandle)
		!= DDI_SUCCESS) {
		mutex_enter(&asy_glob_lock);
		if (nasy == 0) {
			kmem_free(asycom, maxasy * sizeof (struct asycom));
				asycom = NULL;
		}
		mutex_exit(&asy_glob_lock);
		cmn_err(CE_WARN, "asyattach: could not map registers");

		return (DDI_FAILURE);
	}

	/* establish default usage */
	asy->asy_mcr = RTS|DTR;			/* do use RTS/DTR */
	asy->asy_lcr = STOP1|BITS8;		/* default to 1 stop 8 bits */
	asy->asy_bidx = B9600;			/* default to 9600  */
	mcr = 0;				/* don't enable until open */

	/* emulate tty eeprom properties */

	/* Property for ignoring DCD */
	(void) sprintf(name, "tty%c-ignore-cd",
		asy_port_to_name((caddr_t)asy->asy_ioaddr, instance));
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-ignore-cd", '1' + ('a' -
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance)));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "tty0%c-ignore-cd", '0' + ('a' -
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance)));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "port-%c-ignore-cd",
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret == DDI_PROP_SUCCESS && val[0] != 'f' && val[0] != 'F' &&
	    val[0] != '0') {
		asy->asy_flags |= ASY_IGNORE_CD;	/* ignore cd */
		asy->asy_mcr |= RTS|DTR;		/* rts/dtr hi */
		mcr = asy->asy_mcr;
	}

	/* Property for not raising DTR/RTS  */
	(void) sprintf(name, "tty%c-rts-dtr-off",
		asy_port_to_name((caddr_t)asy->asy_ioaddr, instance));
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-rts-dtr-off", '1' + ('a' -
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance)));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "tty0%c-rts-dtr-off", '0' + ('a' -
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance)));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "port-%c-rts-dtr-off",
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret == DDI_PROP_SUCCESS && val[0] != 'f' && val[0] != 'F' &&
	    val[0] != '0') {
		asy->asy_flags |= ASY_RTS_DTR_OFF;	/* OFF */
		asy->asy_mcr &= ~(RTS|DTR);		/* rts/dtr lo */
		mcr = asy->asy_mcr;
	}

	/* Parse property for tty modes */
	asy_parse_mode(devi, asy, instance);

	/*
	 * Initialize the port with default settings.
	 */

	asy->asy_fifo_buf = 1;
	asy->asy_use_fifo = FIFO_OFF;

	/*
	 * Get icookie for mutexes initialization
	 */
	if ((ddi_get_iblock_cookie(devi, 0, &asy->asy_iblock) !=
	    DDI_SUCCESS) ||
	    (ddi_get_soft_iblock_cookie(devi, DDI_SOFTINT_MED,
	    &asy_soft_iblock) != DDI_SUCCESS)) {
		ddi_regs_map_free(&asy->asy_iohandle);
		mutex_enter(&asy_glob_lock);
		nasy--;
		if (nasy == 0) {
			kmem_free(asycom, maxasy * sizeof (struct asycom));
			asycom = (struct asycom *)0;
		}
		mutex_exit(&asy_glob_lock);
		cmn_err(CE_CONT,
			"Can not set device interrupt for ASY driver\n\r");
		return (DDI_FAILURE);
	}

	/*
	 * Initialize mutexes before accessing the hardware
	 */
	mutex_init(&asy->asy_excl, NULL, MUTEX_DRIVER, asy_soft_iblock);
	mutex_init(&asy->asy_excl_hi, NULL, MUTEX_DRIVER,
		(void *)asy->asy_iblock);

	mutex_enter(&asy->asy_excl);
	mutex_enter(&asy->asy_excl_hi);

	/* check for ASY82510 chip */
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ISR, 0x20);
	/* 82510 chip is present */
	if (ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + ISR) & 0x20) {
		/*
		 * Since most of the general operation of the 82510 chip
		 * can be done from BANK 0 (8250A/16450 compatable mode)
		 * we will default to BANK 0.
		 */
		asy->asy_hwtype = ASY82510;
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + (DAT+7), 0x04); /* clear status */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + ISR, 0x40); /* set to bank 2 */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + MCR, 0x08); /* IMD */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + DAT, 0x21); /* FMD */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + ISR, 0x00); /* set to bank 0 */
	} else { /* Set the UART in FIFO mode if it has FIFO buffers */
		asy->asy_hwtype = ASY16550AF;
		/*
		 * Use 16550 fifo reset sequence specified in NS application
		 * note. Disable fifos until chip is initialized.
		 */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + FIFOR, 0x00); /* clear */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + FIFOR, FIFO_ON); /* enable */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + FIFOR, FIFO_ON | FIFORXFLSH);
								/* reset */

		/* set FIFO */
		ddi_io_put8(asy->asy_iohandle,
		    asy->asy_ioaddr + FIFOR,
		    FIFO_ON | FIFODMA | FIFOTXFLSH | FIFORXFLSH |
		    (asy_trig_level & 0xff));

		if ((ddi_io_get8(asy->asy_iohandle,
		    asy->asy_ioaddr + ISR) & 0xc0) == 0xc0) {
			asy->asy_fifo_buf = ASYFLEN; /* with FIFO buffers */
			asy->asy_use_fifo = FIFO_ON;
		} else {
			asy->asy_hwtype = ASY8250;
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + FIFOR, 0x00); /* NO FIFOs */
		}
	}

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_INIT)
		cmn_err(CE_NOTE, "asy%d: COM%c %s.", instance,
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance),
			asy->asy_hwtype == ASY16550AF ? "16550AF" :
			asy->asy_hwtype == ASY82510 ?	"82510" : "8250");
#endif

	/* disable all interrupts */
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ICR, 0);
	/* select baud rate generator */
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + LCR, DLAB);
	/* Set the baud rate to 9600 */
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + (DAT+DLL),
		asyspdtab[asy->asy_bidx] & 0xff);
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + (DAT+DLH),
		(asyspdtab[asy->asy_bidx] >> 8) & 0xff);
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + LCR,
		asy->asy_lcr);
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MCR, mcr);

	mutex_exit(&asy->asy_excl_hi);
	mutex_exit(&asy->asy_excl);

	/*
	 * Set up the other components of the asycom structure for this port.
	 */
	asy->asy_unit = instance;
	asy->asy_dip = devi;

	mutex_enter(&asy_glob_lock);
	if (asy_addedsoft == 0) { /* install the soft interrupt handler */
		if (ddi_add_softintr(devi, DDI_SOFTINT_MED,
		    &asy_softintr_id, NULL, 0, asysoftintr,
		    (caddr_t)0) != DDI_SUCCESS) {
			mutex_destroy(&asy->asy_excl);
			mutex_destroy(&asy->asy_excl_hi);
			ddi_regs_map_free(&asy->asy_iohandle);
			nasy--;
			if (nasy == 0) {
				kmem_free(asycom,
				    maxasy * sizeof (struct asycom));
				asycom = NULL;
			}
			mutex_exit(&asy_glob_lock);
			cmn_err(CE_CONT,
				"Can not set soft interrupt for ASY driver\n");
			return (DDI_FAILURE);
		}
		mutex_init(&asy_soft_lock, NULL, MUTEX_DRIVER,
			(void *)asy->asy_iblock);
		asy_addedsoft++;
		cv_init(&asy_close_cv, NULL, CV_DRIVER, NULL);
	}
	mutex_exit(&asy_glob_lock);

	mutex_enter(&asy->asy_excl);
	mutex_enter(&asy->asy_excl_hi);

	/*
	 * Install interrupt handler for this device.
	 */
	if (ddi_add_intr(devi, 0, NULL, 0, asyintr,
	    (caddr_t)asy) != DDI_SUCCESS) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		mutex_destroy(&asy->asy_excl);
		mutex_destroy(&asy->asy_excl_hi);
		ddi_regs_map_free(&asy->asy_iohandle);
		mutex_enter(&asy_glob_lock);
		nasy--;
		if (nasy == 0) {
			if (asy_addedsoft)
				ddi_remove_softintr(asy_softintr_id);
			asy_addedsoft = 0;
			mutex_destroy(&asy_soft_lock);
			kmem_free(asycom, maxasy * sizeof (struct asycom));
			asycom = NULL;
			cv_destroy(&asy_close_cv);
		}
		mutex_exit(&asy_glob_lock);
		cmn_err(CE_CONT,
			"Can not set device interrupt for ASY driver\n\r");
		return (DDI_FAILURE);
	}

	mutex_exit(&asy->asy_excl_hi);
	mutex_exit(&asy->asy_excl);

	asyinit(asy);	/* initialize the asyncline structure */

	/* create minor device node(s) for this device */
	(void) sprintf(name, "%c", asy_port_to_name((caddr_t)asy->asy_ioaddr,
		instance));
	if ((status = ddi_create_minor_node(devi, name, S_IFCHR, instance,
	    DDI_NT_SERIAL_MB, NULL)) != DDI_SUCCESS) {
		ddi_remove_minor_node(devi, NULL);
	} else {
		(void) sprintf(name, "%c,cu",
			asy_port_to_name((caddr_t)asy->asy_ioaddr,
			instance));
		if ((status = ddi_create_minor_node(devi, name, S_IFCHR,
		    instance|OUTLINE, DDI_NT_SERIAL_MB_DO, NULL)) !=
		    DDI_SUCCESS) {
			ddi_remove_minor_node(devi, NULL);
		}
	}

	if (status != DDI_SUCCESS) {
		ddi_remove_intr(devi, 0, asy->asy_iblock);
		mutex_destroy(&asy->asy_excl);
		mutex_destroy(&asy->asy_excl_hi);
		cv_destroy(&asyncline[asy->asy_unit].async_flags_cv);
		ddi_regs_map_free(&asy->asy_iohandle);
		mutex_enter(&asy_glob_lock);
		nasy--;
		if (nasy == 0) {
			if (asy_addedsoft)
				ddi_remove_softintr(asy_softintr_id);
			asy_addedsoft = 0;
			mutex_destroy(&asy_soft_lock);
			kmem_free(asycom, maxasy * sizeof (struct asycom));
			asycom = NULL;
			kmem_free(asyncline,
				maxasy * sizeof (struct asyncline));
			asyncline = NULL;
			cv_destroy(&asy_close_cv);
		}
		mutex_exit(&asy_glob_lock);
		return (DDI_FAILURE);
	}

	/*
	 * Fill in the polled I/O structure.
	 */
	asy->polledio.cons_polledio_version = CONSPOLLEDIO_V0;
	asy->polledio.cons_polledio_argument = (struct cons_polledio_arg *)asy;
	asy->polledio.cons_polledio_putchar = asyputchar;
	asy->polledio.cons_polledio_getchar = asygetchar;
	asy->polledio.cons_polledio_ischar = asyischar;
	asy->polledio.cons_polledio_enter = NULL;
	asy->polledio.cons_polledio_exit = NULL;

#ifdef __ia64
	/*
	 * If stdin is not the keyboard we assume
	 * we have a serial console.
	 */
	if (!prom_stdin_is_keyboard())
		serial_console_active++;
#endif

	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
asyinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	dev_t dev = (dev_t)arg;
	int instance, error;
	struct asycom *asy;

	if ((instance = UNIT(dev)) > max_asy_instance)
		return (DDI_FAILURE);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		asy = &asycom[instance];
		if (asy->asy_dip == NULL)
			error = DDI_FAILURE;
		else {
			*result = (void *) asy->asy_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * asyinit() initializes the TTY protocol-private data for this channel
 * before enabling the interrupts.
 */
static void
asyinit(struct asycom *asy)
{
	struct asyncline *async;

	mutex_enter(&asy_glob_lock);
	if (asyncline == NULL) {
		/*
		 * Allocate asyncline structures for maxasy ports
		 * Note: may be we should use ddi_soft_state mechanism. CHECK
		 */
		asyncline = kmem_zalloc(maxasy * sizeof (struct asyncline),
			KM_SLEEP);
	}
	mutex_exit(&asy_glob_lock);
	async = &asyncline[asy->asy_unit];
	mutex_enter(&asy->asy_excl);
	async->async_common = asy;
	cv_init(&async->async_flags_cv, NULL, CV_DRIVER, NULL);
	mutex_exit(&asy->asy_excl);
}

/*ARGSUSED3*/
static int
asyopen(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	struct asycom	*asy;
	struct asyncline *async;
	int		mcr;
	int		unit;
	int 		len;
	struct termios 	*termiosp;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_CLOSE)
		printf("open\n\r");
#endif
	unit = UNIT(*dev);
	if (unit > max_asy_instance)
		return (ENXIO);		/* unit not configured */
	async = &asyncline[unit];
	asy = async->async_common;
	if (asy == NULL)
		return (ENXIO);		/* device not found by autoconfig */
	mutex_enter(&asy->asy_excl);
	asy->asy_priv = (caddr_t)async;

again:
	mutex_enter(&asy->asy_excl_hi);

#ifdef XXX_MERGE386
	/*
	 *	Initialize the state keyboard state structure for this unit
	 */
	if (async->async_ppi_func && vm86_asy_is_assigned(async)) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		return (EACCES);
	}

#endif
	/*
	 * Block waiting for carrier to come up, unless this is a no-delay open.
	 */
	if (!(async->async_flags & ASYNC_ISOPEN)) {
		/*
		 * Set the default termios settings (cflag).
		 * Others are set in ldterm.
		 */
		mutex_exit(&asy->asy_excl_hi);

		if (ddi_getlongprop(DDI_DEV_T_ANY, ddi_root_node(),
		    0, "ttymodes",
		    (caddr_t)&termiosp, &len) == DDI_PROP_SUCCESS &&
		    len == sizeof (struct termios)) {
			async->async_ttycommon.t_cflag = termiosp->c_cflag;
			kmem_free(termiosp, len);
		} else {
			cmn_err(CE_WARN,
				"asy: couldn't get ttymodes property!");
		}
		mutex_enter(&asy->asy_excl_hi);

		/* eeprom mode support - respect properties */
		if (asy->asy_cflag)
			async->async_ttycommon.t_cflag = asy->asy_cflag;

		async->async_ttycommon.t_iflag = 0;
		async->async_ttycommon.t_iocpending = NULL;
		async->async_ttycommon.t_size.ws_row = 0;
		async->async_ttycommon.t_size.ws_col = 0;
		async->async_ttycommon.t_size.ws_xpixel = 0;
		async->async_ttycommon.t_size.ws_ypixel = 0;
		async->async_dev = *dev;
		async->async_wbufcid = 0;

		async->async_startc = CSTART;
		async->async_stopc = CSTOP;
		asy_program(asy, ASY_INIT);
	} else if ((async->async_ttycommon.t_flags & TS_XCLUDE) &&
		!drv_priv(cr)) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		return (EBUSY);
	} else if ((*dev & OUTLINE) && !(async->async_flags & ASYNC_OUT)) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		return (EBUSY);
	}

	if (*dev & OUTLINE)
		async->async_flags |= ASYNC_OUT;

	/* Raise DTR on every open */
	mcr = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + MCR);
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MCR,
		mcr|(asy->asy_mcr&DTR));

	if (asy->asy_flags & ASY_IGNORE_CD)
		async->async_ttycommon.t_flags |= TS_SOFTCAR;

	/*
	 * Check carrier.
	 */
	if ((async->async_ttycommon.t_flags & TS_SOFTCAR) ||
	    ((asy->asy_msr = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + MSR)) & DCD))
		async->async_flags |= ASYNC_CARR_ON;
	else
		async->async_flags &= ~ASYNC_CARR_ON;
	mutex_exit(&asy->asy_excl_hi);

	/*
	 * If FNDELAY and FNONBLOCK are clear, block until carrier up.
	 * Quit on interrupt.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK)) &&
	    !(async->async_ttycommon.t_cflag & CLOCAL)) {
		if (!(async->async_flags & (ASYNC_CARR_ON|ASYNC_OUT)) ||
		    ((async->async_flags & ASYNC_OUT) &&
		    !(*dev & OUTLINE))) {
			async->async_flags |= ASYNC_WOPEN;
			if (cv_wait_sig(&async->async_flags_cv,
			    &asy->asy_excl) == B_FALSE) {
				async->async_flags &= ~ASYNC_WOPEN;
				mutex_exit(&asy->asy_excl);
				return (EINTR);
			}
			async->async_flags &= ~ASYNC_WOPEN;
			goto again;
		}
	} else if ((async->async_flags & ASYNC_OUT) && !(*dev & OUTLINE)) {
		mutex_exit(&asy->asy_excl);
		return (EBUSY);
	}

	async->async_ttycommon.t_readq = rq;
	async->async_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)async;
	mutex_exit(&asy->asy_excl);
	qprocson(rq);
	async->async_flags |= ASYNC_ISOPEN;
	async->async_polltid = 0;
	return (0);
}

/*
 * Close routine.
 */
/*ARGSUSED2*/
static int
asyclose(queue_t *q, int flag, cred_t *credp)
{
	struct asyncline *async;
	struct asycom	 *asy;
	int icr, lcr;
	mblk_t	*bp;
	clock_t	timeout_val;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_CLOSE)
		printf("close\n\r");
#endif
	if ((async = (struct asyncline *)q->q_ptr) == NULL)
		return (ENODEV);	/* already closed */
	asy = async->async_common;

	mutex_enter(&asy->asy_excl);

#ifdef	TIOCGPPS
	/*
	 * Turn off PPS handling early to avoid events occuring during
	 * close.  Also reset the DCD edge monitoring bit.
	 */
	mutex_enter(&asy->asy_excl_hi);
	asy->asy_flags &= ~(ASY_PPS | ASY_PPS_EDGE);
	mutex_exit(&asy->asy_excl_hi);
#endif

	/*
	 * If we still have carrier, wait here until all the data is gone;
	 * if interrupted in close, ditch the data and continue onward.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK))) {
		while ((async->async_flags & ASYNC_CARR_ON) &&
		    ((async->async_ocnt > 0) ||
		    (async->async_flags &
		    (ASYNC_BUSY|ASYNC_DELAY|ASYNC_BREAK)))) {
			timeout_val = ddi_get_lbolt() + drv_usectohz(10000);
			if (cv_timedwait_sig(&asy_close_cv, &asy->asy_excl,
			    timeout_val) == 0)
				break;
		}
	}

	/*
	 * If break is in progress, stop it.
	 */
	mutex_enter(&asy->asy_excl_hi);
	lcr = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + LCR);
	if (lcr & SETBREAK) {
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + LCR,
			(lcr & ~SETBREAK));
		async->async_flags &= ~ASYNC_BREAK;
	}

	async->async_ocnt = 0;

	/*
	 * If line has HUPCL set or is incompletely opened fix up the modem
	 * lines.
	 */
	if ((async->async_ttycommon.t_cflag & HUPCL) ||
	    (async->async_flags & ASYNC_WOPEN)) {
		/* turn off DTR, RTS but NOT interrupt to 386 */
		if (asy->asy_flags & (ASY_IGNORE_CD|ASY_RTS_DTR_OFF))
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR, asy->asy_mcr|OUT2);
		else
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR, OUT2);
		mutex_exit(&asy->asy_excl_hi);
		/*
		 * Don't let an interrupt in the middle of close
		 * bounce us back to the top; just continue closing
		 * as if nothing had happened.
		 */
		timeout_val = ddi_get_lbolt() + drv_usectohz(10000);
		if (cv_timedwait_sig(&asy_close_cv, &asy->asy_excl,
		    timeout_val) == 0)
			goto out;
		mutex_enter(&asy->asy_excl_hi);
	}
	/*
	 * If nobody's using it now, turn off receiver interrupts.
	 */
	if ((async->async_flags & (ASYNC_WOPEN|ASYNC_ISOPEN)) == 0) {
		icr = ddi_io_get8(asy->asy_iohandle,
			asy->asy_ioaddr + ICR);
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ICR,
			(icr & ~RIEN));
	}
	mutex_exit(&asy->asy_excl_hi);
out:
	/*
	 * Clear out device state.
	 */
	async->async_flags = 0;
	ttycommon_close(&async->async_ttycommon);
	cv_broadcast(&async->async_flags_cv);

	/*
	 * Cancel outstanding "bufcall" request.
	 */
	if (async->async_wbufcid) {
		unbufcall(async->async_wbufcid);
		async->async_wbufcid = 0;
	}

	/*
	 * Cancel outstanding timeout.
	 */
	if (async->async_polltid) {
		(void) untimeout(async->async_polltid);
		async->async_polltid = 0;
	}

	mutex_exit(&asy->asy_excl);
	qprocsoff(q);
	if (q)
		while (bp = getq(q))
			freemsg(bp);
	q->q_ptr = WR(q)->q_ptr = NULL;
	async->async_polltid = 0;
	async->async_ttycommon.t_readq = NULL;
	async->async_ttycommon.t_writeq = NULL;
	return (0);
}

asy_isbusy(struct asycom *asy)
{
	struct asyncline *async;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_EOT)
		printf("isbusy\n\r");
#endif
	async = (struct asyncline *)asy->asy_priv;
	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));
	return ((async->async_ocnt > 0) ||
		((ddi_io_get8(asy->asy_iohandle,
		    asy->asy_ioaddr + LSR) & (XSRE|XHRE)) == 0));
}

static void
asy_waiteot(struct asycom *asy)
{
	/*
	 * Wait for the current transmission block and the
	 * current fifo data to transmit. Once this is done
	 * we may go on.
	 */
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_EOT)
		printf("waiteot\n\r");
#endif
	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));
	while (asy_isbusy(asy)) {
		mutex_exit(&asy->asy_excl_hi);
		mutex_exit(&asy->asy_excl);
		drv_usecwait(10000);		/* wait .01 */
		mutex_enter(&asy->asy_excl);
		mutex_enter(&asy->asy_excl_hi);
	}
}

/*
 * Program the ASY port. Most of the async operation is based on the values
 * of 'c_iflag' and 'c_cflag'.
 */

#define	BAUDINDEX(cflg)	((cflg) & CBAUDEXT) ? \
			(((cflg) & CBAUD) + CBAUD + 1) : ((cflg) & CBAUD)

static void
asy_program(struct asycom *asy, int mode)
{
	struct asyncline *async;
	int baudrate, c_flag;
	int icr, lcr;
	int flush_reg;
	int ocflags;

	ASSERT(mutex_owned(&asy->asy_excl));
	ASSERT(mutex_owned(&asy->asy_excl_hi));

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("program\n\r");
#endif
	async = (struct asyncline *)asy->asy_priv;

	baudrate = BAUDINDEX(async->async_ttycommon.t_cflag);

	async->async_ttycommon.t_cflag &= ~(CIBAUD);

	if (baudrate > CBAUD) {
		async->async_ttycommon.t_cflag |= CIBAUDEXT;
		async->async_ttycommon.t_cflag |=
			(((baudrate - CBAUD - 1) << IBSHIFT) & CIBAUD);
	} else {
		async->async_ttycommon.t_cflag &= ~CIBAUDEXT;
		async->async_ttycommon.t_cflag |=
			((baudrate << IBSHIFT) & CIBAUD);
	}

	c_flag = async->async_ttycommon.t_cflag &
		(CLOCAL|CREAD|CSTOPB|CSIZE|PARENB|PARODD|CBAUD|CBAUDEXT);

	/* disable interrupts */
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ICR, 0);

	ocflags = asy_ocflags[asy->asy_unit];

	/* flush/reset the status registers */
	flush_reg = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + ISR);
	flush_reg = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + LSR);
	asy->asy_msr = flush_reg = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MSR);
	/*
	 * The device is programmed in the open sequence, if we
	 * have to hardware handshake, then this is a good time
	 * to check if the device can receive any data.
	 */

	if ((CRTSCTS & async->async_ttycommon.t_cflag) && !(flush_reg & CTS)) {
		async->async_flags |= ASYNC_HW_OUT_FLW;
	} else {
		async->async_flags &= ~ASYNC_HW_OUT_FLW;
	}

	/* manually flush receive buffer or fifo (workaround for buggy fifos) */
	if (mode == ASY_INIT)
		if (asy->asy_use_fifo == FIFO_ON) {
			for (flush_reg = asy->asy_fifo_buf; flush_reg-- > 0; )
				(void) ddi_io_get8(asy->asy_iohandle,
						asy->asy_ioaddr + DAT);
		} else {
			flush_reg = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + DAT);
		}

	if (ocflags != (c_flag & ~CLOCAL) || mode == ASY_INIT) {
		/* Set line control */
		lcr = ddi_io_get8(asy->asy_iohandle,
			asy->asy_ioaddr + LCR);
		lcr &= ~(WLS0|WLS1|STB|PEN|EPS);

		if (c_flag & CSTOPB)
			lcr |= STB;	/* 2 stop bits */

		if (c_flag & PARENB)
			lcr |= PEN;

		if ((c_flag & PARODD) == 0)
			lcr |= EPS;

		switch (c_flag & CSIZE) {
		case CS5:
			lcr |= BITS5;
			break;
		case CS6:
			lcr |= BITS6;
			break;
		case CS7:
			lcr |= BITS7;
			break;
		case CS8:
			lcr |= BITS8;
			break;
		}

		/* set the baud rate */
		ddi_io_put8(asy->asy_iohandle,
			asy->asy_ioaddr + LCR, DLAB);
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + DAT,
			asyspdtab[baudrate] & 0xff);
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ICR,
			(asyspdtab[baudrate] >> 8) & 0xff);
		/* set the line control modes */
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + LCR, lcr);

		/*
		 * If we have a FIFO buffer, enable/flush
		 * at intialize time, flush if transitioning from
		 * CREAD off to CREAD on.
		 */
		if ((ocflags & CREAD) == 0 && (c_flag & CREAD) ||
		    mode == ASY_INIT)
			if (asy->asy_use_fifo == FIFO_ON)
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + FIFOR,
					FIFO_ON | FIFODMA | FIFORXFLSH |
					(asy_trig_level & 0xff));

		/* remember the new cflags */
		asy_ocflags[asy->asy_unit] = c_flag & ~CLOCAL;
	}

	if (baudrate == 0)
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MCR,
			(asy->asy_mcr & RTS) | OUT2);
	else
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MCR,
			asy->asy_mcr | OUT2);

	/*
	 * Call the modem status interrupt handler to check for the carrier
	 * in case CLOCAL was turned off after the carrier came on.
	 * (Note: Modem status interrupt is not enabled if CLOCAL is ON.)
	 */
	async_msint(asy);

	/* Set interrupt control */
	if ((c_flag & CLOCAL) && !(async->async_ttycommon.t_cflag & CRTSCTS))
		/*
		 * direct-wired line ignores DCD, so we don't enable modem
		 * status interrupts.
		 */
		icr = (TIEN | SIEN);
	else
		icr = (TIEN | SIEN | MIEN);

	if (c_flag & CREAD)
		icr |= RIEN;

	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + ICR, icr);
}

static
asy_baudok(struct asycom *asy)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	int baudrate;


	baudrate = BAUDINDEX(async->async_ttycommon.t_cflag);

	if (baudrate >= sizeof (asyspdtab)/sizeof (*asyspdtab))
		return (0);

	return (baudrate == 0 || asyspdtab[baudrate]);
}

/*
 * asyintr() is the High Level Interrupt Handler.
 *
 * There are four different interrupt types indexed by ISR register values:
 *		0: modem
 *		1: Tx holding register is empty, ready for next char
 *		2: Rx register now holds a char to be picked up
 *		3: error or break on line
 * This routine checks the Bit 0 (interrupt-not-pending) to determine if
 * the interrupt is from this port.
 */
uint_t
asyintr(caddr_t argasy)
{
	struct asycom		*asy = (struct asycom *)argasy;
	struct asyncline	*async;
	int			ret_status = DDI_INTR_UNCLAIMED;
	uchar_t			interrupt_id, lsr;

	interrupt_id = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + ISR) & 0x0F;
	async = (struct asyncline *)asy->asy_priv;
	if ((async == NULL) || asy_addedsoft == 0 ||
		!(async->async_flags & (ASYNC_ISOPEN|ASYNC_WOPEN))) {
		if (interrupt_id & NOINTERRUPT)
			return (DDI_INTR_UNCLAIMED);
		else {
			/*
			 * reset the device by:
			 *	reading line status
			 *	reading any data from data status register
			 *	reading modem status
			 */
			(void) ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + LSR);
			(void) ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + DAT);
			asy->asy_msr = ddi_io_get8(asy->asy_iohandle,
						asy->asy_ioaddr + MSR);
			return (DDI_INTR_CLAIMED);
		}
	}
#ifdef XXX_MERGE386
	/* needed for com port attachment */
	if (merge386enable && async->async_ppi_func &&
	    (*async->async_ppi_func)(async, -1))
		return (DDI_INTR_CLAIMED);
#endif
	mutex_enter(&asy->asy_excl_hi);
	/*
	 * We will loop until the interrupt line is pulled low. asy
	 * interrupt is edge triggered.
	 */
	/* CSTYLED */
	for (;;	interrupt_id = (ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + ISR) & 0x0F)) {
		if (interrupt_id & NOINTERRUPT)
			break;
		ret_status = DDI_INTR_CLAIMED;
		if (asy->asy_hwtype == ASY82510)
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + ISR, 0x00); /* set bank 0 */

#ifdef DEBUG
		if (asydebug & ASY_DEBUG_INTR)
			prom_printf("l");
#endif
		lsr = ddi_io_get8(asy->asy_iohandle,
			asy->asy_ioaddr + LSR);
		switch (interrupt_id) {
		case RxRDY:
		case RSTATUS:
		case FFTMOUT:
			/* receiver interrupt or receiver errors */
			async_rxint(asy, lsr);
			break;
		case TxRDY:
			/* transmit interrupt */
			async_txint(asy);
			continue;
		case MSTATUS:
			/* modem status interrupt */
			async_msint(asy);
			break;
		}
		if ((lsr & XHRE) && (async->async_flags & ASYNC_BUSY) &&
		    (async->async_ocnt > 0))
			async_txint(asy);
	}
	mutex_exit(&asy->asy_excl_hi);
	return (ret_status);
}

/*
 * Transmitter interrupt service routine.
 * If there is more data to transmit in the current pseudo-DMA block,
 * send the next character if output is not stopped or draining.
 * Otherwise, queue up a soft interrupt.
 *
 * XXX -  Needs review for HW FIFOs.
 */
static void
async_txint(struct asycom *asy)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	uchar_t		ss;
	int		fifo_len;

	fifo_len = asy->asy_fifo_buf; /* with FIFO buffers */

	if ((ss = async->async_flowc) != '\0') {
		if (ss == async->async_startc) {
			if (!(async->async_ttycommon.t_cflag & CRTSXOFF)) {
#ifdef __ia64
				SscDbgasy(ss);
#endif
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + DAT, ss);
			}
		} else {
			if (!(async->async_ttycommon.t_cflag & CRTSXOFF)) {
#ifdef __ia64
				SscDbgasy(ss);
#endif
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + DAT, ss);
			}
		}
		async->async_flowc = '\0';
		if (!(async->async_ttycommon.t_cflag & CRTSXOFF))
			async->async_flags |= ASYNC_BUSY;
		return;
	}

	if (async->async_ocnt > 0 &&
		! (async->async_flags & (ASYNC_HW_OUT_FLW|ASYNC_STOPPED))) {
		while (fifo_len-- > 0 && async->async_ocnt-- > 0) {
#ifdef __ia64
			SscDbgasy(*async->async_optr);
#endif
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + DAT, *async->async_optr++);
		}
	}

	if (fifo_len == 0)
		return;

	ASYSETSOFT(asy);
}

#ifdef	TIOCGPPS
/*
 * Interrupt on port: handle PPS event.  This function is only called
 * for a port on which PPS event handling has been enabled.
 */
static void
asy_ppsevent(struct asycom *asy, int msr)
{
	if (asy->asy_flags & ASY_PPS_EDGE) {
		/* Have seen leading edge, now look for and record drop */
		if ((msr & DCD) == 0)
			asy->asy_flags &= ~ASY_PPS_EDGE;
		/*
		 * Waiting for leading edge, look for rise; stamp event and
		 * calibrate kernel clock.
		 */
	} else if (msr & DCD) {
			/*
			 * This code captures a timestamp at the designated
			 * transition of the PPS signal (DCD asserted).  The
			 * code provides a pointer to the timestamp, as well
			 * as the hardware counter value at the capture.
			 *
			 * Note: the kernel has nano based time values while
			 * NTP requires micro based, an in-line fast algorithm
			 * to convert nsec to usec is used here -- see hrt2ts()
			 * in common/os/timers.c for a full description.
			 */
			struct timeval *tvp = &asy_ppsev.tv;
			timestruc_t ts;
			long nsec, usec;

			asy->asy_flags |= ASY_PPS_EDGE;
			LED_OFF;
			gethrestime(&ts);
			LED_ON;
			nsec = ts.tv_nsec;
			usec = nsec + (nsec >> 2);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 4);
			usec = nsec - (usec >> 3);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 3);
			usec = nsec + (usec >> 4);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 6);
			tvp->tv_usec = usec >> 10;
			tvp->tv_sec = ts.tv_sec;

			++asy_ppsev.serial;

			/*
			 * Because the kernel keeps a high-resolution time,
			 * pass the current highres timestamp in tvp and zero
			 * in usec.
			 */
			ddi_hardpps(tvp, 0);
	}
}
#endif

/*
 * Receiver interrupt: RxRDY interrupt, FIFO timeout interrupt or receive
 * error interrupt.
 * Try to put the character into the circular buffer for this line; if it
 * overflows, indicate a circular buffer overrun. If this port is always
 * to be serviced immediately, or the character is a STOP character, or
 * more than 15 characters have arrived, queue up a soft interrupt to
 * drain the circular buffer.
 * XXX - needs review for hw FIFOs support.
 */

static void
async_rxint(struct asycom *asy, uchar_t lsr)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	uchar_t c = 0, mcr;
	uint_t s = 0, needsoft = 0;
	tty_common_t *tp;
	int looplim = ASYFLEN * 2;

	tp = &async->async_ttycommon;
	if (!(tp->t_cflag & CREAD)) {
		while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
			(void) (ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + DAT) & 0xff);
			lsr = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + LSR);
			if (looplim-- < 0)		/* limit loop */
				break;
		}
		return; /* line is not open for read? */
	}

	while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
		c = 0;
		s = 0;				/* reset error status */
		if (lsr & RCA) {
			c = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + DAT) & 0xff;
			if ((tp->t_iflag & IXON) &&
			    ((c & 0177) == async->async_stopc))
				needsoft = 1;
		}

		/*
		 * Check for character break sequence
		 */
		if ((abort_enable == KIOCABORTALTERNATE) &&
		    (asy->asy_flags & ASY_CONSOLE)) {
			if (abort_charseq_recognize(c))
				abort_sequence_enter((char *)NULL);
		}

#ifdef XXX_MERGE386
		/* Needed for direct attachment of the serial port */
		if (merge386enable)
			if (async->async_ppi_func &&
			    (*async->async_ppi_func)(async, c))
				return;
#endif
		/* Handle framing errors */
		if (lsr & (PARERR|FRMERR|BRKDET|OVRRUN)) {
			if (lsr & PARERR) {
				if (tp->t_iflag & INPCK) /* parity enabled */
					s |= PERROR;
			}

			if (lsr & (FRMERR|BRKDET))
				s |= FRERROR;
			if (lsr & OVRRUN) {
				async->async_hw_overrun = 1;
				s |= OVERRUN;
			}
		}

		if (s == 0)
			if ((tp->t_iflag & PARMRK) &&
			    !(tp->t_iflag & (IGNPAR|ISTRIP)) &&
			    (c == 0377))
				if (RING_POK(async, 2)) {
					RING_PUT(async, 0377);
					RING_PUT(async, c);
				} else
					async->async_sw_overrun = 1;
			else
				if (RING_POK(async, 1))
					RING_PUT(async, c);
				else
					async->async_sw_overrun = 1;
		else
			if (s & FRERROR) /* Handle framing errors */
				if (c == 0)
					if ((asy->asy_flags & ASY_CONSOLE) &&
					    (abort_enable !=
					    KIOCABORTALTERNATE))
						abort_sequence_enter((char *)0);
					else
						async->async_break++;
				else
					if (RING_POK(async, 1))
						RING_MARK(async, c, s);
					else
						async->async_sw_overrun = 1;
			else /* Parity errors are handled by ldterm */
				if (RING_POK(async, 1))
					RING_MARK(async, c, s);
				else
					async->async_sw_overrun = 1;
		lsr = ddi_io_get8(asy->asy_iohandle,
			asy->asy_ioaddr + LSR);
		if (looplim-- < 0)		/* limit loop */
			break;
	}
	if (async->async_ttycommon.t_cflag & CRTSXOFF)
		if (RING_CNT(async) > (RINGSIZE * 3)/4) {
#ifdef DEBUG
			if (asydebug & ASY_DEBUG_HFLOW)
				printf("asy%d: hardware flow stop input.\n\r",
					UNIT(async->async_dev));
#endif
			async->async_flags |= ASYNC_HW_IN_FLOW;
			async->async_flowc = async->async_stopc;
			mcr = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR);
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR,
				(mcr & ~(asy->asy_mcr & RTS)));
		}

	if ((async->async_flags & ASYNC_SERVICEIMM) || needsoft ||
	    (RING_FRAC(async)) || (async->async_polltid == 0))
		ASYSETSOFT(asy);	/* need a soft interrupt */
}

/*
 * Modem status interrupt.
 *
 * (Note: It is assumed that the MSR hasn't been read by asyintr().)
 */

static void
async_msint(struct asycom *asy)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	int msr, t_cflag = async->async_ttycommon.t_cflag;

async_msint_retry:
	/* this resets the interrupt */
	msr = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + MSR);
	/*
	 * Reading MSR resets the interrupt,  we save the
	 * value of msr so that other functions could examine MSR by
	 * looking at asy_msr.
	 */
	asy->asy_msr = (uchar_t)msr;
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_STATE) {
		printf("   transition: %3s %3s %3s %3s\n\r"
			"current state: %3s %3s %3s %3s\n\r",
				(msr & DCTS) ? "CTS" : "   ",
				(msr & DDSR) ? "DSR" : "   ",
				(msr & DRI) ?  "RI " : "   ",
				(msr & DDCD) ? "DCD" : "   ",
				(msr & CTS) ?  "CTS" : "   ",
				(msr & DSR) ?  "DSR" : "   ",
				(msr & RI) ?   "RI " : "   ",
				(msr & DCD) ?  "DCD" : "   ");
	}
#endif
	/*
	 * MSR is read in ioctls so let us not rely on DCTS.
	 */
#ifdef	USE_DCTS
	if (t_cflag & CLOCAL)
		if (((msr & DCTS) == 0) || ((t_cflag & CRTSCTS) == 0))
			return;
#endif

	if (t_cflag & CRTSCTS) {
		if (msr & CTS)
			async->async_flags &= ~ASYNC_HW_OUT_FLW;
		    else
			async->async_flags |= ASYNC_HW_OUT_FLW;

#ifdef DEBUG
		if (asydebug & ASY_DEBUG_HFLOW)
			printf("asy%d: hflow start/stop\n\r",
				UNIT(async->async_dev));
#endif
	}
	if (asy->asy_hwtype == ASY82510)
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MSR,
			(msr & 0xF0));
#ifdef	TIOCGPPS
	/* Handle PPS event */
	if (asy->asy_flags & ASY_PPS)
		asy_ppsevent(asy, msr);
#endif

	async->async_ext++;
	ASYSETSOFT(asy);
	/*
	 * We will make sure that the modem status presented to us
	 * during the previous read has not changed. If the chip samples
	 * the modem status on the falling edge of the interrupt line,
	 * and uses this state as the base for detecting change of modem
	 * status, we would miss a change of modem status event that occured
	 * after we initiated a read MSR operation.
	 */
	msr = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + MSR);
	if (STATES(msr) != STATES(asy->asy_msr))
		goto	async_msint_retry;
}

/*
 * Handle a second-stage interrupt.
 */
/*ARGSUSED*/
uint_t
asysoftintr(caddr_t intarg)
{
	struct asycom *asy;
	int rv;

	/*
	 * Test and clear soft interrupt.
	 */
	mutex_enter(&asy_soft_lock);
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("softintr\n\r");
#endif
	rv = asysoftpend;
	if (rv != 0)
		asysoftpend = 0;
	mutex_exit(&asy_soft_lock);

	if (rv) {
		/*
		 * Note - we can optimize the loop by remembering the last
		 * device that requested soft interrupt
		 */
		for (asy = &asycom[0]; asy <= &asycom[max_asy_instance];
								asy++) {
			if (asy->asy_priv == NULL)
				continue;
			mutex_enter(&asy_soft_lock);
			if (asy->asy_flags & ASY_NEEDSOFT) {
				asy->asy_flags &= ~ASY_NEEDSOFT;
				mutex_exit(&asy_soft_lock);
				async_softint(asy);
			} else
				mutex_exit(&asy_soft_lock);
		}
	}
	return (rv ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}

/*
 * Handle a software interrupt.
 */
static void
async_softint(struct asycom *asy)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	short	cc;
	mblk_t	*bp;
	queue_t	*q;
	uchar_t	val, mcr;
	uchar_t	c;
	tty_common_t	*tp;
	int nb;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("process\n\r");
#endif
	mutex_enter(&asy_soft_lock);
	if (asy->asy_flags & ASY_DOINGSOFT) {
		asy->asy_flags |= ASY_DOINGSOFT_RETRY;
		mutex_exit(&asy_soft_lock);
		return;
	}
	asy->asy_flags |= ASY_DOINGSOFT;
begin:
	asy->asy_flags &= ~ASY_DOINGSOFT_RETRY;
	mutex_exit(&asy_soft_lock);
	mutex_enter(&asy->asy_excl);
	tp = &async->async_ttycommon;
	q = tp->t_readq;
	mutex_enter(&asy->asy_excl_hi);
	if ((async->async_ttycommon.t_cflag & CRTSCTS) &&
	    (!(async->async_flags & ASYNC_HW_OUT_FLW))) {
#ifdef DEBUG
		if (asydebug & ASY_DEBUG_HFLOW)
			printf("asy%d: hflow start\n\r",
				UNIT(async->async_dev));
#endif
		mutex_exit(&asy->asy_excl_hi);
		if (async->async_ocnt > 0) {
			mutex_enter(&asy->asy_excl_hi);
			async_resume(async);
			mutex_exit(&asy->asy_excl_hi);
		} else {
			if (async->async_xmitblk)
				freeb(async->async_xmitblk);
			async->async_xmitblk = NULL;
			async_start(async);
		}
		mutex_enter(&asy->asy_excl_hi);
	}
	if (async->async_ext) {
		async->async_ext = 0;
		/* check for carrier up */
		if ((asy->asy_msr & DCD) || (tp->t_flags & TS_SOFTCAR)) {
			/* carrier present */
			if ((async->async_flags & ASYNC_CARR_ON) == 0) {
				async->async_flags |= ASYNC_CARR_ON;
				if (async->async_flags & ASYNC_ISOPEN) {
					mutex_exit(&asy->asy_excl_hi);
					mutex_exit(&asy->asy_excl);
					(void) putctl(q, M_UNHANGUP);
					mutex_enter(&asy->asy_excl);
					mutex_enter(&asy->asy_excl_hi);
				}
				cv_broadcast(&async->async_flags_cv);
			}
		} else {
			if ((async->async_flags & ASYNC_CARR_ON) && (
				!(tp->t_cflag & CLOCAL))) {
				/*
				 * Carrier went away.
				 * Drop DTR, abort any output in
				 * progress, indicate that output is
				 * not stopped, and send a hangup
				 * notification upstream.
				 */
				val = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR);
				ddi_io_put8(asy->asy_iohandle,
				    asy->asy_ioaddr + MCR, (val & ~DTR));
				if (async->async_flags & ASYNC_BUSY)
					async->async_ocnt = 0;
				async->async_flags &= ~ASYNC_STOPPED;
				if (async->async_flags & ASYNC_ISOPEN) {
					mutex_exit(&asy->asy_excl_hi);
					mutex_exit(&asy->asy_excl);
					(void) putctl(q, M_HANGUP);
					mutex_enter(&asy->asy_excl);
					mutex_enter(&asy->asy_excl_hi);
				}
			}
				async->async_flags &= ~ASYNC_CARR_ON;
		}
	}
	mutex_exit(&asy->asy_excl_hi);

	/*
	 * If data has been added to the circular buffer, remove
	 * it from the buffer, and send it up the stream if there's
	 * somebody listening. Try to do it 16 bytes at a time. If we
	 * have more than 16 bytes to move, move 16 byte chunks and
	 * leave the rest for next time around (maybe it will grow).
	 */
	mutex_enter(&asy->asy_excl_hi);
	if (!(async->async_flags & ASYNC_ISOPEN)) {
		RING_INIT(async);
		goto rv;
	}
	if ((cc = RING_CNT(async)) <= 0)
		goto rv;
	mutex_exit(&asy->asy_excl_hi);

	if ((async->async_ttycommon.t_cflag & CRTSXOFF) &&
	    (!canput(q))) {
		if ((async->async_flags & ASYNC_HW_IN_FLOW) == 0) {
#ifdef DEBUG
			if (!(asydebug & ASY_DEBUG_HFLOW)) {
				printf("asy%d: hflow stop input.\n\r",
				    UNIT(async->async_dev));
				if (canputnext(q)) {
				    printf("asy%d: next queue is ready\r\n",
					UNIT(async->async_dev));
				}
			}
#endif
			mutex_enter(&asy->asy_excl_hi);
			async->async_flags |= ASYNC_HW_IN_FLOW;
			async->async_flowc = async->async_stopc;
			mcr = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR);
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR,
				(val & ~(asy->asy_mcr & RTS)));
		} else
			mutex_enter(&asy->asy_excl_hi);
		goto rv;
	}
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_INPUT)
		printf("asy%d: %d char(s) in queue.\n\r",
			UNIT(async->async_dev), cc);
#endif
	if (!(bp = allocb(cc, BPRI_MED))) {
		mutex_exit(&asy->asy_excl);
		ttycommon_qfull(&async->async_ttycommon, q);
		mutex_enter(&asy->asy_excl);
		mutex_enter(&asy->asy_excl_hi);
		goto rv;
	}
	mutex_enter(&asy->asy_excl_hi);
	do {
		if (RING_ERR(async, S_ERRORS)) {
			RING_UNMARK(async);
			c = RING_GET(async);
			break;
		} else
			*bp->b_wptr++ = RING_GET(async);
	} while (--cc);
	mutex_exit(&asy->asy_excl_hi);
	mutex_exit(&asy->asy_excl);
	if (bp->b_wptr > bp->b_rptr) {
			if (!canput(q)) {
				asyerror(CE_NOTE, "asy%d: local queue full\n\r",
					UNIT(async->async_dev));
				freemsg(bp);
			} else
				(void) putq(q, bp);
	} else
		freemsg(bp);
	/*
	 * If we have a parity error, then send
	 * up an M_BREAK with the "bad"
	 * character as an argument. Let ldterm
	 * figure out what to do with the error.
	 */
	if (cc) {
		(void) putctl1(q, M_BREAK, c);
		ASYSETSOFT(async->async_common);	/* finish cc chars */
	}
	mutex_enter(&asy->asy_excl);
	mutex_enter(&asy->asy_excl_hi);
rv:
	if ((async->async_ttycommon.t_cflag & CRTSXOFF) &&
	    (async->async_flags & ASYNC_HW_IN_FLOW) &&
	    (RING_CNT(async) < (RINGSIZE/4))) {
#ifdef DEBUG
			if (asydebug & ASY_DEBUG_HFLOW)
				printf("asy%d: hflow start input.\n\r",
					UNIT(async->async_dev));
#endif
			async->async_flags &= ~ASYNC_HW_IN_FLOW;
			async->async_flowc = async->async_startc;
			mcr = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR);
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + MCR,
				(mcr | (asy->asy_mcr & RTS)));
	}

	/*
	 * If a transmission has finished, indicate that it's finished,
	 * and start that line up again.
	 */
	if (async->async_break > 0) {
		nb = async->async_break;
		async->async_break = 0;
		if (async->async_flags & ASYNC_ISOPEN) {
			mutex_exit(&asy->asy_excl_hi);
			mutex_exit(&asy->asy_excl);
			for (; nb > 0; nb--)
				(void) putctl(q, M_BREAK);
			mutex_enter(&asy->asy_excl);
			mutex_enter(&asy->asy_excl_hi);
		}
	}
	if (async->async_ocnt <= 0 && (async->async_flags & ASYNC_BUSY)) {
		async->async_flags &= ~ASYNC_BUSY;
		mutex_exit(&asy->asy_excl_hi);
		if (async->async_xmitblk)
			freeb(async->async_xmitblk);
		async->async_xmitblk = NULL;
		async_start(async);
		mutex_enter(&asy->asy_excl_hi);
	}
	/*
	 * A note about these overrun bits: all they do is *tell* someone
	 * about an error- They do not track multiple errors. In fact,
	 * you could consider them latched register bits if you like.
	 * We are only interested in printing the error message once for
	 * any cluster of overrun errrors.
	 */
	if (async->async_hw_overrun) {
		if (async->async_flags & ASYNC_ISOPEN) {
			mutex_exit(&asy->asy_excl_hi);
			mutex_exit(&asy->asy_excl);
			asyerror(CE_NOTE, "asy%d: silo overflow\n\r",
				UNIT(async->async_dev));
			mutex_enter(&asy->asy_excl);
			mutex_enter(&asy->asy_excl_hi);
		}
		async->async_hw_overrun = 0;
	}
	if (async->async_sw_overrun) {
		if (async->async_flags & ASYNC_ISOPEN) {
			mutex_exit(&asy->asy_excl_hi);
			mutex_exit(&asy->asy_excl);
			asyerror(CE_NOTE, "asy%d: ring buffer overflow\n\r",
				UNIT(async->async_dev));
			mutex_enter(&asy->asy_excl);
			mutex_enter(&asy->asy_excl_hi);
		}
		async->async_sw_overrun = 0;
	}
	mutex_exit(&asy->asy_excl_hi);
	mutex_exit(&asy->asy_excl);
	mutex_enter(&asy_soft_lock);
	if (asy->asy_flags & ASY_DOINGSOFT_RETRY) {
		goto begin;
	}
	asy->asy_flags &= ~ASY_DOINGSOFT;
	mutex_exit(&asy_soft_lock);
}

/*
 * Restart output on a line after a delay or break timer expired.
 */
static int
async_restart(struct asyncline *async)
{
	struct asycom *asy = async->async_common;
	uchar_t lcr;

	/*
	 * If break timer expired, turn off the break bit.
	 */
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("restart\n\r");
#endif
	mutex_enter(&asy->asy_excl);
	if (async->async_flags & ASYNC_BREAK) {
		mutex_enter(&asy->asy_excl_hi);
		lcr = ddi_io_get8(asy->asy_iohandle,
			asy->asy_ioaddr + LCR);
		ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + LCR,
			(lcr & ~SETBREAK));
		mutex_exit(&asy->asy_excl_hi);
	}
	async->async_flags &= ~(ASYNC_DELAY|ASYNC_BREAK);
	async_start(async);

	mutex_exit(&asy->asy_excl);

	return (0);
}

static void
async_start(struct asyncline *async)
{
	async_nstart(async, 0);
}

/*
 * Start output on a line, unless it's busy, frozen, or otherwise.
 */
/*ARGSUSED*/
static void
async_nstart(struct asyncline *async, int mode)
{
	struct asycom *asy = async->async_common;
	int cc;
	queue_t *q;
	mblk_t *bp;
	uchar_t *xmit_addr;
	uchar_t	ss;
	uchar_t	val;
	int	fifo_len = 1;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("start\n\r");
#endif
	if (asy->asy_use_fifo == FIFO_ON)
		fifo_len = asy->asy_fifo_buf; /* with FIFO buffers */

	ASSERT(mutex_owned(&asy->asy_excl));

	/*
	 * If the chip is busy (i.e., we're waiting for a break timeout
	 * to expire, or for the current transmission to finish, or for
	 * output to finish draining from chip), don't grab anything new.
	 */
	if (async->async_flags & (ASYNC_BREAK|ASYNC_BUSY)) {
#ifdef DEBUG
		if (mode && asydebug & ASY_DEBUG_CLOSE)
			printf("asy%d: start %s.\n\r",
				UNIT(async->async_dev),
				async->async_flags & ASYNC_BREAK
				? "break" : "busy");
#endif
		return;
	}

	/*
	 * If we have a flow-control character to transmit, do it now.
	 */
	if ((ss = async->async_flowc) != '\0') {
		mutex_enter(&asy->asy_excl_hi);
		if (ddi_io_get8(asy->asy_iohandle,
		    asy->asy_ioaddr + LSR) & XHRE)
			if (ss == async->async_startc) {
				val = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR);
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR,
					(val | (asy->asy_mcr & RTS)));
				if (!(async->async_ttycommon.t_cflag &
				    CRTSXOFF)) {
#ifdef __ia64
					SscDbgasy(ss);
#endif
					ddi_io_put8(asy->asy_iohandle,
					    asy->asy_ioaddr + DAT, ss);
				}
			} else {
				if (!(async->async_ttycommon.t_cflag &
				    CRTSXOFF)) {
#ifdef __ia64
					SscDbgasy(ss);
#endif
					ddi_io_put8(asy->asy_iohandle,
					    asy->asy_ioaddr + DAT, ss);
				}
				val = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR);
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR,
					(val & ~(asy->asy_mcr & RTS)));
			}
		async->async_flowc = '\0';
		if (!(async->async_ttycommon.t_cflag & CRTSXOFF))
			async->async_flags |= ASYNC_BUSY;
		mutex_exit(&asy->asy_excl_hi);
#ifdef DEBUG
		if (mode && asydebug & ASY_DEBUG_CLOSE)
			printf("asy%d: start flow control.\n\r",
				UNIT(async->async_dev));
#endif
		return;
	}

	/*
	 * If we're waiting for a delay timeout to expire, don't grab
	 * anything new.
	 */
	if (async->async_flags & ASYNC_DELAY) {
#ifdef DEBUG
		if (mode && asydebug & ASY_DEBUG_CLOSE)
			printf("asy%d: start ASYNC_DELAY.\n\r",
				UNIT(async->async_dev));
#endif
		return;
	}

	if ((q = async->async_ttycommon.t_writeq) == NULL) {
#ifdef DEBUG
		if (mode && asydebug & ASY_DEBUG_CLOSE)
			printf("asy%d: start writeq is null.\n\r",
				UNIT(async->async_dev));
#endif
		return;	/* not attached to a stream */
	}

	for (;;) {
		if ((bp = getq(q)) == NULL)
			return;	/* no data to transmit */

		/*
		 * We have a message block to work on.
		 * Check whether it's a break, a delay, or an ioctl (the latter
		 * occurs if the ioctl in question was waiting for the output
		 * to drain).  If it's one of those, process it immediately.
		 */
		switch (bp->b_datap->db_type) {

		case M_BREAK:
			/*
			 * Set the break bit, and arrange for "async_restart"
			 * to be called in 1/4 second; it will turn the
			 * break bit off, and call "async_start" to grab
			 * the next message.
			 */
			mutex_enter(&asy->asy_excl_hi);
			val = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + LCR);
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + LCR, (val | SETBREAK));
			mutex_exit(&asy->asy_excl_hi);
			async->async_flags |= ASYNC_BREAK;
			(void) timeout((void (*)())async_restart,
				(caddr_t)async, drv_usectohz(1000000)/4);
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_DELAY:
			/*
			 * Arrange for "async_restart" to be called when the
			 * delay expires; it will turn ASYNC_DELAY off,
			 * and call "async_start" to grab the next message.
			 */
			(void) timeout((void (*)())async_restart,
				(caddr_t)async,
				(int)(*(unsigned char *)bp->b_rptr + 6));
			async->async_flags |= ASYNC_DELAY;
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_IOCTL:
			/*
			 * This ioctl was waiting for the output ahead of
			 * it to drain; obviously, it has.  Do it, and
			 * then grab the next message after it.
			 */
			mutex_exit(&asy->asy_excl);
			async_ioctl(async, q, bp);
			mutex_enter(&asy->asy_excl);
			continue;
		}

		if ((cc = bp->b_wptr - bp->b_rptr) > 0)
			break;

		freemsg(bp);
	}

	/*
	 * We have data to transmit.  If output is stopped, put
	 * it back and try again later.
	 */
	if (async->async_flags & (ASYNC_HW_OUT_FLW|ASYNC_STOPPED)) {
#ifdef DEBUG
		if (asydebug & ASY_DEBUG_HFLOW &&
		    async->async_flags & ASYNC_HW_OUT_FLW)
			printf("asy%d: output hflow in effect.\n\r",
				UNIT(async->async_dev));
#endif
		(void) putbq(q, bp);
		return;
	}

	async->async_xmitblk = bp;
	xmit_addr = bp->b_rptr;
	bp = bp->b_cont;
	if (bp != NULL)
		(void) putbq(q, bp);	/* not done with this message yet */

	/*
	 * In 5-bit mode, the high order bits are used
	 * to indicate character sizes less than five,
	 * so we need to explicitly mask before transmitting
	 */
	if ((async->async_ttycommon.t_cflag & CSIZE) == CS5) {
		unsigned char *p = xmit_addr;
		int cnt = cc;

		while (cnt--)
			*p++ &= (unsigned char) 0x1f;
	}

	/*
	 * Set up this block for pseudo-DMA.
	 */
	mutex_enter(&asy->asy_excl_hi);
	async->async_optr = xmit_addr;
	async->async_ocnt = (short)cc;
	/*
	 * If the transmitter is ready, shove the first
	 * character out.
	 */
	while (fifo_len-- && async->async_ocnt)
		if (ddi_io_get8(asy->asy_iohandle,
		    asy->asy_ioaddr + LSR) & XHRE) {
#ifdef __ia64
			SscDbgasy(*async->async_optr);
#endif
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + DAT, *async->async_optr++);
			async->async_ocnt--;
		}
	async->async_flags |= ASYNC_BUSY;
	mutex_exit(&asy->asy_excl_hi);
}

/*
 * Resume output by poking the transmitter.
 */
static void
async_resume(struct asyncline *async)
{
	struct asycom *asy = async->async_common;
	uchar_t ss;
	uchar_t mcr;

	ASSERT(mutex_owned(&asy->asy_excl_hi));
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("resume\n\r");
#endif

	if (ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + LSR) & XHRE) {
		if ((ss = async->async_flowc) != '\0') {
			if (ss == async->async_startc) {
				mcr = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR);
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR,
					(mcr | (asy->asy_mcr & RTS)));
				if (!(async->async_ttycommon.t_cflag &
				    CRTSXOFF)) {
#ifdef __ia64
					SscDbgasy(ss);
#endif
					ddi_io_put8(asy->asy_iohandle,
					    asy->asy_ioaddr + DAT, ss);
				}
			} else {
				if (!(async->async_ttycommon.t_cflag &
				    CRTSXOFF)) {
#ifdef __ia64
					SscDbgasy(ss);
#endif
					ddi_io_put8(asy->asy_iohandle,
					    asy->asy_ioaddr + DAT, ss);
				}
				mcr = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR);
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + MCR,
					(mcr & ~(asy->asy_mcr & RTS)));
			}
			async->async_flowc = '\0';
			if (!(async->async_ttycommon.t_cflag & CRTSXOFF))
				async->async_flags |= ASYNC_BUSY;
		} else if (async->async_ocnt > 0) {
#ifdef __ia64
			SscDbgasy(*async->async_optr);
#endif
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + DAT, *async->async_optr++);
			async->async_ocnt--;
		}
	}
}

/*
 * Process an "ioctl" message sent down to us.
 * Note that we don't need to get any locks until we are ready to access
 * the hardware.  Nothing we access until then is going to be altered
 * outside of the STREAMS framework, so we should be safe.
 */
int asydelay = 10000;
static void
async_ioctl(struct asyncline *async, queue_t *wq, mblk_t *mp)
{
	struct asycom *asy = async->async_common;
	tty_common_t  *tp = &async->async_ttycommon;
	struct iocblk *iocp;
	unsigned datasize;
	struct copyreq *crp;
	int error = 0;
	uchar_t val;
	int transparent = 0;
	void *parg;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("ioctl\n\r");
#endif

	if (tp->t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(async->async_ttycommon.t_iocpending);
		async->async_ttycommon.t_iocpending = NULL;
	}

	iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * For TIOCMGET and the PPS ioctls, do NOT call ttycommon_ioctl()
	 * because this function frees up the message block (mp->b_cont) that
	 * contains the user location where we pass back the results.
	 *
	 * Similarly, CONSOPENPOLLEDIO needs ioc_count, which ttycommon_ioctl
	 * zaps.  We know that ttycommon_ioctl doesn't know any CONS*
	 * ioctls, so keep the others safe too.
	 */
	switch (iocp->ioc_cmd) {
	case TIOCMGET:
#ifdef	TIOCGPPS
	case TIOCGPPS:
	case TIOCSPPS:
	case TIOCGPPSEV:
#endif
	case CONSOPENPOLLEDIO:
	case CONSCLOSEPOLLEDIO:
	case CONSSETABORTENABLE:
	case CONSGETABORTENABLE:
		error = -1; /* Do Nothing */
		break;
	default:

		/*
		 * The only way in which "ttycommon_ioctl" can fail is if the
		 * "ioctl" requires a response containing data to be returned
		 * to the user, and no mblk could be allocated for the data.
		 * No such "ioctl" alters our state.  Thus, we always go ahead
		 * and do any state-changes the "ioctl" calls for.  If we
		 * couldn't allocate the data, "ttycommon_ioctl" has stashed
		 * the "ioctl" away safely, so we just call "bufcall" to
		 * request that we be called back when we stand a better
		 * chance of allocating the data.
		 */
		if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0) {
			if (async->async_wbufcid)
				unbufcall(async->async_wbufcid);
			async->async_wbufcid = bufcall(datasize, BPRI_HI,
			    async_reioctl,
			    (void *)async->async_common->asy_unit);
			return;
		}
	}

	mutex_enter(&asy->asy_excl);

	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETS:
			mutex_enter(&asy->asy_excl_hi);
			if (asy_baudok(asy))
				asy_program(asy, ASY_NOINIT);
			else
				error = EINVAL;
			mutex_exit(&asy->asy_excl_hi);
			break;
		case TCSETSF:
		case TCSETSW:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			mutex_enter(&asy->asy_excl_hi);
			if (!asy_baudok(asy))
				error = EINVAL;
			else {
				if (asy_isbusy(asy))
					asy_waiteot(asy);
				asy_program(asy, ASY_NOINIT);
			}
			mutex_exit(&asy->asy_excl_hi);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		error = 0;
		switch (iocp->ioc_cmd) {

#ifdef	TIOCGPPS
		case TIOCGPPS:
		{
			mblk_t *bp;
			/*
			 * Get PPS on/off.
			 */
			if (mp->b_cont != NULL) {
				error = EINVAL;
				break;
			}
			bp = allocb(sizeof (int), BPRI_HI);
			if (bp == NULL) {
				error = ENOMEM;
				break;
			}
			mp->b_cont = bp;
			if (asy->asy_flags & ASY_PPS)
				*(int *)bp->b_wptr = 1;
			else
				*(int *)bp->b_wptr = 0;
			bp->b_wptr += sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = sizeof (int);
			break;
		}
		case TIOCSPPS:
			/*
			 * Set PPS on/off.
			 */
			if (mp->b_cont == NULL)
				error = EINVAL;
			else {
				mutex_enter(&asy->asy_excl_hi);
				if (*(int *)mp->b_cont->b_rptr)
					asy->asy_flags |= ASY_PPS;
				else
					asy->asy_flags &= ~ASY_PPS;
				/* Reset edge sense */
				asy->asy_flags &= ~ASY_PPS_EDGE;
				mutex_exit(&asy->asy_excl_hi);
				mp->b_datap->db_type = M_IOCACK;
			}
			break;

		case TIOCGPPSEV:
		{
			/*
			 * Get PPS event data.
			 */
			mblk_t *bp;
			void *buf;
#ifdef _SYSCALL32_IMPL
			struct ppsclockev32 p32;
#endif
			struct ppsclockev ppsclockev;

			if (mp->b_cont != NULL) {
				error = EINVAL;
				break;
			}
			if ((asy->asy_flags & ASY_PPS) == 0) {
				error = ENXIO;
				break;
			}

			/* Protect from incomplete asy_ppsev */
			mutex_enter(&asy->asy_excl_hi);
			ppsclockev = asy_ppsev;
			mutex_exit(&asy->asy_excl_hi);

#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				TIMEVAL_TO_TIMEVAL32(&p32.tv, &ppsclockev.tv);
				p32.serial = ppsclockev.serial;
				buf = &p32;
				iocp->ioc_count = sizeof (struct ppsclockev32);
			} else
#endif
			{
				buf = &ppsclockev;
				iocp->ioc_count = sizeof (struct ppsclockev);
			}

			if ((bp = allocb(iocp->ioc_count, BPRI_HI)) == NULL) {
				error = ENOMEM;
				break;
			}
			mp->b_cont = bp;

			bcopy(buf, bp->b_wptr, iocp->ioc_count);
			bp->b_wptr += iocp->ioc_count;
			mp->b_datap->db_type = M_IOCACK;
			break;
		}
#endif

		case TCSBRK:
			if (*(int *)mp->b_cont->b_rptr == 0) {
				/*
				 * Set the break bit, and arrange for
				 * "async_restart" to be called in 1/4 second;
				 * it will turn the break bit off, and call
				 * "async_start" to grab the next message.
				 */

				/*
				 * XXX Arrangements to ensure that a break
				 * isn't in progress should be sufficient.
				 * This ugly delay() is the only thing
				 * that seems to work on the NCR Worldmark.
				 * It should be replaced. Note that an
				 * asy_waiteot() also does not work.
				 */
				if (asydelay)
					delay(drv_usectohz(asydelay));

				mutex_enter(&asy->asy_excl_hi);
				while (async->async_flags & ASYNC_BREAK) {
					mutex_exit(&asy->asy_excl_hi);
					delay(drv_usectohz(10000));
					mutex_enter(&asy->asy_excl_hi);
				}
				val = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + LCR);
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + LCR,
					(val | SETBREAK));
				mutex_exit(&asy->asy_excl_hi);
				async->async_flags |= ASYNC_BREAK;
				(void) timeout((void (*)())async_restart,
					(caddr_t)async,
					drv_usectohz(1000000)/4);
			} else {
#ifdef DEBUG
				if (asydebug & ASY_DEBUG_CLOSE)
					printf("asy%d: wait for flush.\n\r",
						UNIT(async->async_dev));
#endif
				mutex_enter(&asy->asy_excl_hi);
				asy_waiteot(asy);
				mutex_exit(&asy->asy_excl_hi);
#ifdef DEBUG
				if (asydebug & ASY_DEBUG_CLOSE)
					printf("asy%d: ldterm satisfied.\n\r",
						UNIT(async->async_dev));
#endif
			}
			break;

		case TIOCSBRK:
			mutex_enter(&asy->asy_excl_hi);
			val = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + LCR);
			ddi_io_put8(asy->asy_iohandle,
			    asy->asy_ioaddr + LCR, (val | SETBREAK));
			mutex_exit(&asy->asy_excl_hi);
			break;

		case TIOCCBRK:
			mutex_enter(&asy->asy_excl_hi);
			val = ddi_io_get8(asy->asy_iohandle,
				asy->asy_ioaddr + LCR);
			ddi_io_put8(asy->asy_iohandle,
				asy->asy_ioaddr + LCR, (val & ~SETBREAK));
			mutex_exit(&asy->asy_excl_hi);
			break;

		case TIOCMSET:
		case TIOCMBIS:
		case TIOCMBIC:
			mutex_enter(&asy->asy_excl_hi);
			(void) asymctl(asy, dmtoasy(*(int *)mp->b_cont->b_rptr),
				iocp->ioc_cmd);
			mutex_exit(&asy->asy_excl_hi);
			iocp->ioc_error = 0;
			mp->b_datap->db_type = M_IOCACK;
			break;

		case TIOCMGET:
			if (iocp->ioc_count == TRANSPARENT) {
				transparent = 1;
				crp =  (struct copyreq *)mp->b_rptr;
				crp->cq_addr =
					(caddr_t)*(int *)mp->b_cont->b_rptr;
				crp->cq_size = sizeof (int);
				crp->cq_flag = 0;
			}

			if (mp->b_cont)
				freemsg(mp->b_cont);

			mp->b_cont = allocb(sizeof (int), BPRI_MED);

			if (mp->b_cont == (mblk_t *)NULL) {
				error = EAGAIN;
				break;
			}

			if (transparent) {
				mutex_enter(&asy->asy_excl_hi);
				*(int *)mp->b_cont->b_rptr =
					asymctl(asy, 0, TIOCMGET);
				mutex_exit(&asy->asy_excl_hi);
				mp->b_datap->db_type = M_COPYOUT;
				mp->b_cont->b_wptr = mp->b_cont->b_rptr +
					sizeof (int);
				mp->b_wptr = mp->b_rptr+sizeof (struct copyreq);
			} else {
				mutex_enter(&asy->asy_excl_hi);
				*(int *)*(int *)mp->b_cont->b_rptr =
					asymctl(asy, 0, TIOCMGET);
				mutex_exit(&asy->asy_excl_hi);
				mp->b_datap->db_type = M_IOCACK;
				iocp->ioc_count = sizeof (int);
			}

			break;

		case CONSOPENPOLLEDIO:
			if (iocp->ioc_count !=
				sizeof (struct cons_polledio *)) {
				error = EINVAL;
				break;
			}
			/*
			 * We are given an appropriate-sized data block,
			 * and return a pointer to our structure in it.
			 */
			*(struct cons_polledio **)mp->b_cont->b_rptr =
				&asy->polledio;

			mp->b_datap->db_type = M_IOCACK;
			break;

		case CONSCLOSEPOLLEDIO:
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSSETABORTENABLE:
			error = drv_priv(iocp->ioc_cr);
			if (error != 0)
				break;

			if (*(boolean_t *)mp->b_cont->b_rptr)
			    asy->asy_flags |= ASY_CONSOLE;
			else
			    asy->asy_flags &= ~ASY_CONSOLE;

			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSGETABORTENABLE:
			/*CONSTANTCONDITION*/
			ASSERT(sizeof (boolean_t) <= sizeof (boolean_t *));
			/*
			 * Get a pointer to our data block.  This is
			 * where our argument is, and we also turn it
			 * around to return the data for the M_COPYOUT.
			 */
			parg = mp->b_cont->b_rptr;

			/*
			 * OK, let's turn this puppy around into an
			 * M_COPYOUT.
			 *
			 * Replace the iocblk with a copyreq.
			 * Note that the first three fields of the
			 * copyreq (cmd, cr, id) are inherited from
			 * the iocblk and the block is large enough
			 * for a copyreq; this is authorized by WDD.
			 */
			crp = (struct copyreq *)mp->b_rptr;
			crp->cq_addr = (caddr_t)*(boolean_t **)parg;
			crp->cq_size = sizeof (boolean_t);
			crp->cq_flag = 0;
			crp->cq_private = NULL;
			mp->b_wptr = (unsigned char *)(crp+1);

			*(boolean_t *)parg =
				(asy->asy_flags & ASY_CONSOLE) != 0;
			mp->b_cont->b_wptr = mp->b_cont->b_rptr +
				sizeof (boolean_t);

			/*
			 * And mark the result as an M_COPYOUT
			 */
			mp->b_datap->db_type = M_COPYOUT;
			break;

		default: /* unexpected ioctl type */
#ifdef XXX_MERGE386
			if (merge386enable) {
				if (vm86_com_ppiioct(wq, mp, async,
					iocp->ioc_cmd)) {
					mutex_exit(&asy->asy_excl);
					return;
				}
			}
#endif
			/*
			 * If we don't understand it, it's an error.  NAK it.
			 */
			error = EINVAL;
			break;
		}
	}
	if (error != 0) {
		iocp->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK;
	}
	mutex_exit(&asy->asy_excl);
	qreply(wq, mp);
}

static int
asyrsrv(queue_t *q)
{
	mblk_t *bp;
	struct asyncline *async;

	async = (struct asyncline *)q->q_ptr;

	while (canputnext(q) && (bp = getq(q)))
		putnext(q, bp);
	ASYSETSOFT(async->async_common);
	async->async_polltid = 0;
	return (0);
}

/*
 * Put procedure for write queue.
 * Respond to M_STOP, M_START, M_IOCTL, and M_FLUSH messages here;
 * set the flow control character for M_STOPI and M_STARTI messages;
 * queue up M_BREAK, M_DELAY, and M_DATA messages for processing
 * by the start routine, and then call the start routine; discard
 * everything else.  Note that this driver does not incorporate any
 * mechanism to negotiate to handle the canonicalization process.
 * It expects that these functions are handled in upper module(s),
 * as we do in ldterm.
 */
static int
asywput(queue_t *q, mblk_t *mp)
{
	struct asyncline *async;
	struct asycom *asy;

	async = (struct asyncline *)q->q_ptr;
	asy = async->async_common;

	switch (mp->b_datap->db_type) {

	case M_STOP:
		/*
		 * Since we don't do real DMA, we can just let the
		 * chip coast to a stop after applying the brakes.
		 */
		mutex_enter(&asy->asy_excl);
		async->async_flags |= ASYNC_STOPPED;
		mutex_exit(&asy->asy_excl);
		freemsg(mp);
		break;

	case M_START:
		mutex_enter(&asy->asy_excl);
		if (async->async_flags & ASYNC_STOPPED) {
			async->async_flags &= ~ASYNC_STOPPED;
			/*
			 * If an output operation is in progress,
			 * resume it.  Otherwise, prod the start
			 * routine.
			 */
			if (async->async_ocnt > 0) {
				mutex_enter(&asy->asy_excl_hi);
				async_resume(async);
				mutex_exit(&asy->asy_excl_hi);
			} else {
				async_start(async);
			}
		}
		mutex_exit(&asy->asy_excl);
		freemsg(mp);
		break;

	case M_IOCTL:
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {

		case TCSBRK:
			if (*(int *)mp->b_cont->b_rptr != 0) {
#ifdef DEBUG
				if (asydebug & ASY_DEBUG_CLOSE)
					printf("asy%d: flush request.\n\r",
						UNIT(async->async_dev));
#endif
				(void) putq(q, mp);
				mutex_enter(&asy->asy_excl);
				async_nstart(async, 1);
				mutex_exit(&asy->asy_excl);
				break;
			}
			/*FALLTHROUGH*/
		case TCSETSW:
		case TCSETSF:
		case TCSETAW:
		case TCSETAF:
			/*
			 * The changes do not take effect until all
			 * output queued before them is drained.
			 * Put this message on the queue, so that
			 * "async_start" will see it when it's done
			 * with the output before it.  Poke the
			 * start routine, just in case.
			 */
			(void) putq(q, mp);
			mutex_enter(&asy->asy_excl);
			async_start(async);
			mutex_exit(&asy->asy_excl);
			break;

		default:
			/*
			 * Do it now.
			 */
			async_ioctl(async, q, mp);
			break;
		}
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) {
			mutex_enter(&asy->asy_excl);

			/*
			 * Abort any output in progress.
			 */
			mutex_enter(&asy->asy_excl_hi);
			if (async->async_flags & ASYNC_BUSY)
				async->async_ocnt = 0;
			mutex_exit(&asy->asy_excl_hi);

			/* Flush FIFO buffers */
			if (asy->asy_use_fifo == FIFO_ON) {
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + FIFOR,
					FIFO_ON | FIFODMA | FIFOTXFLSH |
					(asy_trig_level & 0xff));
			}

			/*
			 * Flush our write queue.
			 */
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			mutex_exit(&asy->asy_excl);
			*mp->b_rptr &= ~FLUSHW;	/* it has been flushed */
		}
		if (*mp->b_rptr & FLUSHR) {
			/* Flush FIFO buffers */
			if (asy->asy_use_fifo == FIFO_ON) {
				ddi_io_put8(asy->asy_iohandle,
					asy->asy_ioaddr + FIFOR,
					FIFO_ON | FIFODMA | FIFORXFLSH |
					(asy_trig_level & 0xff));
			}
			flushq(RD(q), FLUSHDATA);
			qreply(q, mp);	/* give the read queues a crack at it */
		} else {
			freemsg(mp);
		}

		/*
		 * We must make sure we process messages that survive the
		 * write-side flush.  Without this call, the close protocol
		 * with ldterm can hang forever.  (ldterm will have sent us a
		 * TCSBRK ioctl that it expects a response to.)
		 */
		mutex_enter(&asy->asy_excl);
		async_start(async);
		mutex_exit(&asy->asy_excl);
		break;

	case M_BREAK:
	case M_DELAY:
	case M_DATA:
		/*
		 * Queue the message up to be transmitted,
		 * and poke the start routine.
		 */
		(void) putq(q, mp);
		mutex_enter(&asy->asy_excl);
		async_start(async);
		mutex_exit(&asy->asy_excl);
		break;

	case M_STOPI:
		mutex_enter(&asy->asy_excl);
		async->async_flowc = async->async_stopc;
		async_start(async);		/* poke the start routine */
		mutex_exit(&asy->asy_excl);
		freemsg(mp);
		break;

	case M_STARTI:
		mutex_enter(&asy->asy_excl);
		async->async_flowc = async->async_startc;
		async_start(async);		/* poke the start routine */
		mutex_exit(&asy->asy_excl);
		freemsg(mp);
		break;

	case M_CTL:
		/*
		 * These MC_SERVICE type messages are used by upper
		 * modules to tell this driver to send input up
		 * immediately, or that it can wait for normal
		 * processing that may or may not be done.  Sun
		 * requires these for the mouse module. (XXX - for x86?)
		 */
		mutex_enter(&asy->asy_excl);
		switch (*mp->b_rptr) {

		case MC_SERVICEIMM:
			async->async_flags |= ASYNC_SERVICEIMM;
			break;

		case MC_SERVICEDEF:
			async->async_flags &= ~ASYNC_SERVICEIMM;
			break;
		}
		mutex_exit(&asy->asy_excl);
		freemsg(mp);
		break;

	case M_IOCDATA:
		async_iocdata(q, mp);
		break;

	default:
		freemsg(mp);
		break;
	}
	return (0);
}

/*
 * Retry an "ioctl", now that "bufcall" claims we may be able to allocate
 * the buffer we need.
 */
static void
async_reioctl(void *unit)
{
	struct asyncline *async = &asyncline[(int)unit];
	struct asycom *asy = async->async_common;
	queue_t	*q;
	mblk_t	*mp;

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(&asy->asy_excl);
	async->async_wbufcid = 0;
	if ((q = async->async_ttycommon.t_writeq) == NULL) {
		mutex_exit(&asy->asy_excl);
		return;
	}
	if ((mp = async->async_ttycommon.t_iocpending) != NULL) {
		/* not pending any more */
		async->async_ttycommon.t_iocpending = NULL;
		mutex_exit(&asy->asy_excl);
		async_ioctl(async, q, mp);
	} else
		mutex_exit(&asy->asy_excl);
}

static void
async_iocdata(queue_t *q, mblk_t *mp)
{
	struct asyncline	*async = (struct asyncline *)q->q_ptr;
	struct asycom		*asy;
	struct iocblk *ip;
	struct copyresp *csp;

	asy = async->async_common;
	ip = (struct iocblk *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	if (csp->cp_rval != 0) {
		if (csp->cp_private)
			freemsg(csp->cp_private);
		freemsg(mp);
		return;
	}

	mutex_enter(&asy->asy_excl);
	switch (csp->cp_cmd) {

	case TIOCMGET:
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		mp->b_datap->db_type = M_IOCACK;
		ip->ioc_error = 0;
		ip->ioc_count = 0;
		ip->ioc_rval = 0;
		mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
		break;

	default:
		mp->b_datap->db_type = M_IOCNAK;
		ip->ioc_error = EINVAL;
		break;
	}
	qreply(q, mp);
	mutex_exit(&asy->asy_excl);
}

/*
 * debugger/console support routines.
 */

/*
 * put a character out
 * Do not use interrupts.  If char is LF, put out CR, LF.
 */
static void
asyputchar(struct cons_polledio_arg *arg, uchar_t c)
{
	struct asycom *asy = (struct asycom *)arg;
	struct asyncline *async;

	async = &asyncline[asy->asy_unit];

	if (((ddi_io_get8(asy->asy_iohandle,
	    asy->asy_ioaddr + MSR) & DCD) == 0) &&
	    ((async->async_ttycommon.t_flags & TS_SOFTCAR) == 0))
		return;

	if (c == '\n')
		asyputchar(arg, '\r');

	while ((ddi_io_get8(asy->asy_iohandle,
	    asy->asy_ioaddr + LSR) & XHRE) == 0) {
		/* wait for xmit to finish */
		drv_usecwait(10);
	}

	/* put the character out */
#ifdef __ia64
	SscDbgasy(c);
#endif
	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + DAT, c);
}

/*
 * See if there's a character available. If no character is
 * available, return 0. Run in polled mode, no interrupts.
 */
static boolean_t
asyischar(struct cons_polledio_arg *arg)
{
	struct asycom *asy = (struct asycom *)arg;

	return ((ddi_io_get8(asy->asy_iohandle,
		asy->asy_ioaddr + LSR) & RCA) != 0);
}

/*
 * Get a character. Run in polled mode, no interrupts.
 */
static int
asygetchar(struct cons_polledio_arg *arg)
{
	struct asycom *asy = (struct asycom *)arg;

	while (!asyischar(arg))
		drv_usecwait(10);
	return (ddi_io_get8(asy->asy_iohandle,
		asy->asy_ioaddr + DAT));
}

/*
 * Set or get the modem control status.
 */
static int
asymctl(struct asycom *asy, int bits, int how)
{
	int mcr_r, msr_r;

	ASSERT(mutex_owned(&asy->asy_excl_hi));
	ASSERT(mutex_owned(&asy->asy_excl));

	/* Read Modem Control Registers */
	mcr_r = ddi_io_get8(asy->asy_iohandle, asy->asy_ioaddr + MCR);

	switch (how) {

	case TIOCMSET:
		mcr_r |= (DTR | RTS);		/* Can only set DTR and RTS */
		break;

	case TIOCMBIS:
		mcr_r &= ~(DTR | RTS);		/* Clear DTR and RTS	*/
		mcr_r |= bits;			/* Set bits from input	*/
		break;

	case TIOCMBIC:
		mcr_r &= ~bits;			/* Set ~bits from input	*/
		break;

	case TIOCMGET:
		/* Read Modem Status Registers */
		/*
		 * If modem interrupts are enabled, we return the
		 * saved value of msr. We read MSR only in async_msint()
		 */
		if (ddi_io_get8(asy->asy_iohandle,
		    asy->asy_ioaddr + ICR) & MIEN)
			msr_r = asy->asy_msr;
		else
			msr_r = ddi_io_get8(asy->asy_iohandle,
					asy->asy_ioaddr + MSR);

		return (asytodm(mcr_r, msr_r));
	}

	ddi_io_put8(asy->asy_iohandle, asy->asy_ioaddr + MCR, mcr_r);

	return (mcr_r);
}

static int
asytodm(int mcr_r, int msr_r)
{
	int b = 0;


	/* MCR registers */
	if (mcr_r & RTS)
		b |= TIOCM_RTS;

	if (mcr_r & DTR)
		b |= TIOCM_DTR;

	/* MSR registers */
	if (msr_r & DCD)
		b |= TIOCM_CAR;

	if (msr_r & CTS)
		b |= TIOCM_CTS;

	if (msr_r & DSR)
		b |= TIOCM_DSR;

	if (msr_r & RI)
		b |= TIOCM_RNG;

	return (b);
}

static int
dmtoasy(int bits)
{
	int b = 0;

#ifdef	CAN_NOT_SET	/* only DTR and RTS can be set */
	if (bits & TIOCM_CAR)
		b |= DCD;
	if (bits & TIOCM_CTS)
		b |= CTS;
#endif
	if (bits & TIOCM_DSR)
		b |= DSR;
	if (bits & TIOCM_RNG)
		b |= RI;

	if (bits & TIOCM_RTS)
		b |= RTS;
	if (bits & TIOCM_DTR)
		b |= DTR;

	return (b);
}


/*PRINTFLIKE2*/
static void
asyerror(int level, char *fmt, ...)
{
	va_list adx;
	static	long	last;
	static	char	*lastfmt;


	/*
	 * Don't print the same error message too often.
	 * Print the message only if we have not printed the
	 * message within the last second.
	 * Note: that fmt can not be a pointer to a string
	 * stored on the stack. The fmt pointer
	 * must be in the data segment otherwise lastfmt would point
	 * to non-sense.
	 */
	if ((last == hrestime.tv_sec) && (lastfmt == fmt))
		return;

	last = hrestime.tv_sec;
	lastfmt = fmt;

	va_start(adx, fmt);
	vcmn_err(level, fmt, adx);
	va_end(adx);
}

/*
 * asy_parse_mode(struct asycom *asy)
 * The value of this property is in the form of "9600,8,n,1,-"
 * 1) speed: 9600, 4800, ...
 * 2) data bits
 * 3) parity: n(none), e(even), o(odd)
 * 4) stop bits
 * 5) handshake: -(none), h(hardware: rts/cts), s(software: xon/off)
 *
 * This parsing came from a SPARCstation eeprom.
 */
static void
asy_parse_mode(dev_info_t *devi, struct asycom *asy, int instance)
{
	char		name[40];
	char		val[40];
	int		len;
	int		ret;
	char		*p;
	char		*p1;

	/*
	 * Parse the ttyx-mode property
	 */
	(void) sprintf(name, "tty%c-mode",
		asy_port_to_name((caddr_t)asy->asy_ioaddr, instance));
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-mode", '1' + ('a' -
			asy_port_to_name((caddr_t)asy->asy_ioaddr, instance)));
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}

	/* no property to parse */
	asy->asy_cflag = 0;
	if (ret != DDI_PROP_SUCCESS)
		return;

	p = val;
	/* ---- baud rate ---- */
	asy->asy_cflag = CREAD|B9600;		/* initial default */
	if (p && (p1 = strchr(p, ',')) != 0) {
		*p1++ = '\0';
	} else {
		asy->asy_cflag |= BITS8;	/* add default bits */
		return;
	}

	if (strcmp(p, "110") == 0)
		asy->asy_cflag = CREAD|B110;
	else if (strcmp(p, "150") == 0)
		asy->asy_cflag = CREAD|B150;
	else if (strcmp(p, "300") == 0)
		asy->asy_cflag = CREAD|B300;
	else if (strcmp(p, "600") == 0)
		asy->asy_cflag = CREAD|B600;
	else if (strcmp(p, "1200") == 0)
		asy->asy_cflag = CREAD|B1200;
	else if (strcmp(p, "2400") == 0)
		asy->asy_cflag = CREAD|B2400;
	else if (strcmp(p, "4800") == 0)
		asy->asy_cflag = CREAD|B4800;
	else if (strcmp(p, "9600") == 0)
		asy->asy_cflag = CREAD|B9600;
	else if (strcmp(p, "19200") == 0)
		asy->asy_cflag = CREAD|B19200;
	else if (strcmp(p, "38400") == 0)
		asy->asy_cflag = CREAD|B38400;
	else if (strcmp(p, "57600") == 0)
		asy->asy_cflag = CREAD|B57600;
	else if (strcmp(p, "76800") == 0)
		asy->asy_cflag = CREAD|B76800;
	else if (strcmp(p, "115200") == 0)
		asy->asy_cflag = CREAD|B115200|CBAUDEXT;
	else if (strcmp(p, "153600") == 0)
		asy->asy_cflag = CREAD|B153600|CBAUDEXT;
	else if (strcmp(p, "230400") == 0)
		asy->asy_cflag = CREAD|B230400|CBAUDEXT;
	else if (strcmp(p, "307200") == 0)
		asy->asy_cflag = CREAD|B307200|CBAUDEXT;
	else if (strcmp(p, "460800") == 0)
		asy->asy_cflag = CREAD|B460800|CBAUDEXT;

	asy->asy_bidx = BAUDINDEX(asy->asy_cflag);

	/* ---- Next item is data bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		asy->asy_cflag |= BITS8;	/* add default bits */
		return;
	}
	switch (*p) {
		default:
		case '8':
			asy->asy_cflag |= CS8;
			asy->asy_lcr = BITS8;
			break;
		case '7':
			asy->asy_cflag |= CS7;
			asy->asy_lcr = BITS7;
			break;
		case '6':
			asy->asy_cflag |= CS6;
			asy->asy_lcr = BITS6;
			break;
		case '5':
			/*LINTED*/
			asy->asy_cflag |= CS5;
			asy->asy_lcr = BITS5;
			break;
	}

	/* ---- Parity info ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	switch (*p)  {
		default:
		case 'n':
			break;
		case 'e':
			asy->asy_cflag |= PARENB;
			asy->asy_lcr |= PEN; break;
		case 'o':
			asy->asy_cflag |= PARENB|PARODD;
			asy->asy_lcr |= PEN|EPS;
			break;
	}

	/* ---- Find stop bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	if (*p == '2') {
		asy->asy_cflag |= CSTOPB;
		asy->asy_lcr |= STB;
	}

	/* ---- handshake is next ---- */
	p = p1;
	if (p) {
		if ((p1 = strchr(p, ',')) != 0)
			*p1++ = '\0';

		if (*p == 'h')
			asy->asy_cflag |= CRTSCTS;
		else if (*p == 's')
			asy->asy_cflag |= CRTSXOFF;
	}
}

/*
 * Check for abort character sequence
 */
static boolean_t
abort_charseq_recognize(uchar_t ch)
{
	static int state = 0;
#define	CNTRL(c) ((c)&037)
	static char sequence[] = { '\r', '~', CNTRL('b') };

	if (ch == sequence[state]) {
		if (++state >= sizeof (sequence)) {
			state = 0;
			return (B_TRUE);
		}
	} else {
		state = (ch == sequence[0]) ? 1 : 0;
	}
	return (B_FALSE);
}
