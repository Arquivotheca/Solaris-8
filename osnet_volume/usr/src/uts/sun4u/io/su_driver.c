/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)su_driver.c	1.27	99/11/16 SMI"

/*
 *	Serial I/O driver for 82510/8250/16450/16550AF chips.
 *	Modified as sparc keyboard/mouse driver.
 */
#define	SU_DRIVER
#define	SU_REGISTER_FILE_NO 0
#define	SU_REGOFFSET 0
#define	SU_REGISTER_LEN 8

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
#include <sys/cred.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/consdev.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#ifdef DEBUG
#include <sys/promif.h>
#endif
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sudev.h>

/*
 * set the FIFO trigger_level to 8 bytes for now
 * we may want to make this configurable later.
 */
static	int asy_trig_level = 0 /* FIFO_TRIG_8 */;

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

static	int nasy = 0;
static	int max_asy_instance = -1;
static	int maxasy = MAXASY;	/* Maximum no. of asy ports supported XXX */
extern int hz;
static	struct asycom *asycom;
static	struct asyncline *asyncline;
#ifndef SU_DRIVER
static	struct asycom *asyconsole; /* asy as the console */
#endif

static	uint_t	asysoftintr(caddr_t intarg);
static	uint_t	asyintr(caddr_t argasy);

/* The async interrupt entry points */
static void	async_txint(struct asycom *asy);
static void	async_rxint(struct asycom *asy, uchar_t lsr);
static void	async_msint(struct asycom *asy);
static int	async_softint(struct asycom *asy);

static void	async_ioctl(struct asyncline *async, queue_t *q, mblk_t *mp);
static void	async_reioctl(void *);
static void	async_iocdata(queue_t *q, mblk_t *mp);
static void	async_restart(void *);
static void	async_start(struct asyncline *async);
static void	async_nstart(struct asyncline *async, int mode);
static void	async_resume(struct asyncline *async);
static void	asy_program(struct asycom *asy, int mode);

static int	asymctl(struct asycom *, int, int);
static int	asytodm(int, int);
static int	dmtoasy(int);
static void	asyinit(struct asycom *asy);
static void	asy_waiteot(struct asycom *asy);

#define	GET_PROP(devi, pname, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
		DDI_PROP_DONTPASS, (pname), (caddr_t)(pval), (plen)))

static ddi_iblock_cookie_t asy_iblock;
ddi_softintr_t asy_softintr_id;
static	int asy_addedsoft = 0;
int	asysoftpend;	/* soft interrupt pending */
kmutex_t asy_soft_lock;	/* lock protecting asysoftpend */
extern kcondvar_t lbolt_cv;
extern int ddi_create_internal_pathname(dev_info_t *dip, char *name,
		int spec_type, minor_t minor_num);


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
	0x001,	/* 115200 baud rate */
};

int asyopen(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr);
int asyclose(queue_t *q, int flag);
int asywput(queue_t *q, mblk_t *mp);
int asyrsrv(queue_t *q);

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

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"su driver 1.27",
	&asy_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
asyprobe(dev_info_t *devi)
{
	int		instance;
#ifndef SU_DRIVER
	ushort_t	ioaddr;
	int		value, len;
#endif

#ifdef SU_DRIVER
	ddi_acc_handle_t handle;
	uchar_t *addr;
	ddi_device_acc_attr_t attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	if (ddi_regs_map_setup(devi, SU_REGISTER_FILE_NO, (caddr_t *)&addr,
				SU_REGOFFSET, SU_REGISTER_LEN,
				&attr, &handle)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "asyprobe regs map setup failed");
		return (DDI_PROBE_FAILURE);
	}
#ifdef DEBUG
	if (asydebug)
	    printf("Probe address mapped %p\n", (void *)addr);
#endif


#else SU_DRIVER
	len = sizeof (value);
	if (GET_PROP(devi, "ioaddr", &value, &len) == DDI_PROP_SUCCESS)
		ioaddr = (ushort_t)value;
	else
		return (DDI_PROBE_FAILURE);
#endif SU_DRIVER
	/*
	 * Probe for the device:
	 * 	Ser. int. uses bits 0,1,2; FIFO uses 3,6,7; 4,5 wired low.
	 * 	If bit 4 or 5 appears on inb() ISR, board is not there.
	 */
#ifdef SU_DRIVER
	if (ddi_get8(handle, addr+ISR) & 0x30)
		return (DDI_PROBE_FAILURE);
#else
	if ((INB(ioaddr, ISR) & 0x30))
		return (DDI_PROBE_FAILURE);
#endif
	if (nasy < maxasy) {
		nasy++;
		instance = ddi_get_instance(devi);
		if ((instance < maxasy) && (max_asy_instance < instance))
			max_asy_instance = instance;
	}
#ifdef SU_DRIVER
	ddi_regs_map_free(&handle);
#endif SU_DRIVER
	return (DDI_PROBE_SUCCESS); /* hw is present */
}

/*ARGSUSED*/
static int
asydetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register int	instance;
	int		s;
	struct asycom	*asy;
	struct asyncline *async;
	char		name[16];
#ifndef SU_DRIVER
	extern int	console;
	extern int	(*getcharptr)();
	extern int	(*putcharptr)();
	extern int	(*ischarptr)();
	int		len;
	int		asygetchar();
	int		asyischar();
	int		asyputchar();
#endif

#ifndef SU_DRIVER
	if ((instance == 0) && (console == CONSOLE_IS_ASY))
		return (DDI_FAILURE);
#endif

	instance = ddi_get_instance(devi);	/* find out which unit */

	s = ddi_enter_critical();

	asy = &asycom[instance];

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		/* grab both mutex locks */
		mutex_enter(asy->asy_excl);
		mutex_enter(asy->asy_excl_hi);
		if (asy->suspended) {
			mutex_exit(asy->asy_excl_hi);
			mutex_exit(asy->asy_excl);
			ddi_exit_critical(s);
			return (DDI_SUCCESS);
		}
		asy->suspended = B_TRUE;
		/* Disable further interrupts */
		OUTB(asy->asy_ioaddr, ICR, 0);
		mutex_exit(asy->asy_excl_hi);
		mutex_exit(asy->asy_excl);
		ddi_exit_critical(s);
		return (DDI_SUCCESS);

	default:
		ddi_exit_critical(s);
		return (DDI_FAILURE);
	}

#ifdef SU_DRIVER
#else SU_DRIVER
	len = sizeof (int);
	(void) GET_PROP(devi, "ioaddr", &asy->asy_ioaddr, &len);
#endif
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_INIT)
		cmn_err(CE_NOTE, "asy%d: ASY%s shutdown.",
				instance,
				asy->asy_hwtype == ASY82510 ? "82510" :
				asy->asy_hwtype == ASY16550AF ? "16550AF" :
					"8250");
#endif
	ddi_exit_critical(s);

	/* remove minor device node(s) for this device */
	(void) sprintf(name, "%c", (instance+'a'));	/* serial-port */
	ddi_remove_minor_node(devi, name);
	(void) sprintf(name, "%c,cu", (instance+'a')); /* serial-port:dailout */
	ddi_remove_minor_node(devi, name);

	mutex_destroy(asy->asy_excl);
	mutex_destroy(asy->asy_excl_hi);
	kmem_free(asy->asy_excl, sizeof (kmutex_t));
	kmem_free(asy->asy_excl_hi, sizeof (kmutex_t));
	async = &asyncline[asy->asy_unit];
	cv_destroy(&async->async_flags_cv);
	ddi_remove_intr(devi, 0, asy->asy_iblock);
	nasy--;
	if (nasy == 0) {
		if (asy_addedsoft)
			ddi_remove_softintr(asy_softintr_id);
		asy_addedsoft = 0;
		mutex_destroy(&asy_soft_lock);
		kmem_free(asycom, maxasy*sizeof (struct asycom));
		asycom = (struct asycom *)0;
#ifdef DEBUG
		if (asydebug & ASY_DEBUG_INIT)
			cmn_err(CE_CONT, "The last of asy driver's removed\n");
#endif
	}
#ifdef SU_DRIVER
/*	ddi_regs_map_free(&asy->asy_handle); */
#endif
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
asyattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register int	instance;
	int		s;
	struct asycom	*asy;
#ifndef SU_DRIVER
	extern int	console;
	extern int	(*getcharptr)();
	extern int	(*putcharptr)();
	extern int	(*ischarptr)();
	char		name[16];
	int		len;
	int		asygetchar();
	int		asyischar();
	int		asyputchar();
#else SU_DRIVER
	ddi_device_acc_attr_t attr;
#endif
	instance = ddi_get_instance(devi);	/* find out which unit */

	if (instance > max_asy_instance)
		return (DDI_FAILURE);


	if (asycom == NULL) { /* allocate asycom structures for maxasy ports */
		/* Note: may be we should use ddi_soft_state mechanism. CHECK */
		asycom = (struct asycom *)
			kmem_zalloc(maxasy*sizeof (struct asycom), KM_SLEEP);
	}

	s = ddi_enter_critical();

	asy = &asycom[instance];

	switch (cmd) {
	case DDI_ATTACH:
		break;
	    case DDI_RESUME: {
		struct asyncline *async;

		/* grab both mutex locks */
		mutex_enter(asy->asy_excl);
		mutex_enter(asy->asy_excl_hi);
		if (!asy->suspended) {
			mutex_exit(asy->asy_excl_hi);
			mutex_exit(asy->asy_excl);
			ddi_exit_critical(s);
			return (DDI_SUCCESS);
		}
		/* re-setup all the registers and enable interrupts if needed */
		async = (struct asyncline *)asy->asy_priv;
		if ((async) && (async->async_flags & ASYNC_ISOPEN))
			asy_program(asy, ASY_INIT);
		asy->suspended = B_FALSE;
		mutex_exit(asy->asy_excl_hi);
		mutex_exit(asy->asy_excl);
		ddi_exit_critical(s);
		return (DDI_SUCCESS);
	    }
	    default:
		ddi_exit_critical(s);
		return (DDI_FAILURE);
	}

#ifdef SU_DRIVER
	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = DDI_STRUCTURE_LE_ACC;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	if (ddi_regs_map_setup(devi, SU_REGISTER_FILE_NO,
				(caddr_t *)&asy->asy_ioaddr,
				SU_REGOFFSET, SU_REGISTER_LEN,
				&attr, &asy->asy_handle)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "asyprobe regs map setup failed");
		return (DDI_PROBE_FAILURE);
	}
#ifdef DEBUG
	if (asydebug)
		printf("su attach mapped %p\n", (void *)asy->asy_ioaddr);
#endif
#else SU_DRIVER
	len = sizeof (int);
	(void) GET_PROP(devi, "ioaddr", &asy->asy_ioaddr, &len);
#endif SU_DRIVER

	/*
	 * Initialize the port with default settings.
	 */

	asy->asy_fifo_buf = 1;
	asy->asy_use_fifo = FIFO_OFF;

	/* check for ASY82510 chip */
	OUTB(asy->asy_ioaddr, ISR, 0x20);
	if (INB(asy->asy_ioaddr, ISR) & 0x20) { /* 82510 chip is present */
		/*
		 * Since most of the general operation of the 82510 chip
		 * can be done from BANK 0 (8250A/16450 compatable mode)
		 * we will default to BANK 0.
		 */
		asy->asy_hwtype = ASY82510;
		OUTB(asy->asy_ioaddr, DAT+7, 0x04); /* clear status */
		OUTB(asy->asy_ioaddr, ISR, 0x40); /* set to bank 2 */
		OUTB(asy->asy_ioaddr, MCR, 0x08); /* IMD */
		OUTB(asy->asy_ioaddr, DAT, 0x21); /* FMD */
		OUTB(asy->asy_ioaddr, ISR, 0x00); /* set to bank 0 */
	} else { /* Set the UART in FIFO mode if it has FIFO buffers */
		asy->asy_hwtype = ASY16550AF;
		OUTB(asy->asy_ioaddr, FIFOR, 0x00); /* clear fifo register */

		/* set FIFO */
		OUTB(asy->asy_ioaddr, FIFOR,
			FIFO_ON | FIFODMA | FIFOTXFLSH | FIFORXFLSH |
			(asy_trig_level & 0xff));

		if ((INB(asy->asy_ioaddr, ISR) & 0xc0) == 0xc0) {
			asy->asy_fifo_buf = 16; /* with FIFO buffers */
			asy->asy_use_fifo = FIFO_ON;
		} else {
			asy->asy_hwtype = ASY8250;
			OUTB(asy->asy_ioaddr, FIFOR, 0x00); /* NO FIFOs */
		}
	}

#ifdef DEBUG
#ifndef SU_DRIVER
	len = ~asy->asy_ioaddr & 0x110;
	if (asydebug & ASY_DEBUG_INIT)
		cmn_err(CE_NOTE, "asy%d: COM%d %s.", instance,
				1 + (len >> 8)|((len >> 3) & 2),
				asy->asy_hwtype == ASY16550AF ? "16550AF" :
				asy->asy_hwtype == ASY82510 ?	"82510" :
								"8250");
#endif SU_DRIVER
#endif

	OUTB(asy->asy_ioaddr, ICR, 0); /* disable all interrupts */
	OUTB(asy->asy_ioaddr, LCR, DLAB); /* select baud rate generator */
	/* Set the baud rate to 9600 */
	OUTB(asy->asy_ioaddr, DAT+DLL, ASY9600 & 0xff);
	OUTB(asy->asy_ioaddr, DAT+DLH, (ASY9600 >> 8) & 0xff);
	OUTB(asy->asy_ioaddr, LCR, STOP1|BITS8);
	OUTB(asy->asy_ioaddr, MCR, (DTR | RTS));

	/*
	 * Set up the other components of the asycom structure for this port.
	 */
	asy->asy_excl = (kmutex_t *)
		kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	asy->asy_excl_hi = (kmutex_t *)
		kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
	asy->asy_unit = instance;
	asy->asy_dip = devi;

	/*
	 * Install interrupt handler for this device.
	 */
	if (ddi_add_intr(devi, 0, &asy->asy_iblock, 0, asyintr,
						(caddr_t)asy) != DDI_SUCCESS) {
		cmn_err(CE_CONT,
			"Can not set device interrupt for ASY driver\n\r");
		nasy--;
		return (DDI_FAILURE);
	}

	if (asy_addedsoft == 0) { /* install the soft interrupt handler */
		if (ddi_add_softintr(devi, DDI_SOFTINT_HIGH,
				&asy_softintr_id, &asy_iblock, 0, asysoftintr,
				(caddr_t)0) != DDI_SUCCESS) {
			cmn_err(CE_CONT,
				"Can not set soft interrupt for ASY driver\n");
			nasy--;
			return (DDI_FAILURE);
		}
		mutex_init(&asy_soft_lock, NULL, MUTEX_DRIVER,
		    (void *)asy->asy_iblock);
		asy_addedsoft++;
	}
	mutex_init(asy->asy_excl, NULL, MUTEX_DRIVER, NULL);
	mutex_init(asy->asy_excl_hi, NULL, MUTEX_DRIVER,
	    (void *)asy->asy_iblock);

	asyinit(asy);	/* initialize the asyncline structure */

	ddi_exit_critical(s);

	if (ddi_getprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		"keyboard", 0)) {

		/*
		 * If the device is a keyboard, then create an internal
		 * pathname so that the dacf code will link the node into
		 * the keyboard console stream.  See dacf.conf.
		 */
		if (ddi_create_internal_pathname(devi, "keyboard",
			S_IFCHR, instance) == DDI_FAILURE) {

				ddi_remove_minor_node(devi, NULL);
				return (-1);
		}
	} else if (ddi_getprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		"mouse", 0)) {

		/*
		 * If the device is a mouse, then create an internal
		 * pathname so that the dacf code will link the node into
		 * the mouse stream.  See dacf.conf.
		 */
		if (ddi_create_internal_pathname(devi, "mouse", S_IFCHR,
				instance) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			return (-1);
		}
	} else {

		/*
		 * If not used for keyboard/mouse, create minor devices nodes
		 * for this device
		 */
		char    name[8];

		/* serial-port */
		(void) sprintf(name, "%c", (instance+'a'));
		if (ddi_create_minor_node(devi, name, S_IFCHR, instance,
			DDI_NT_SERIAL_MB, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			return (-1);
		}
		/* serial-port:dailout */
		(void) sprintf(name, "%c,cu", (instance+'a'));
		if (ddi_create_minor_node(devi, name, S_IFCHR, instance|OUTLINE,
			DDI_NT_SERIAL_MB_DO, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			return (-1);
		}
	}

	/*
	 * Use the first instance of the device for CONSOLE redirection
	 * Note: May be we should get the instance number for redirection
	 *	as a device property (e.g asyconsole-instance).
	 */

#ifndef SU_DRIVER
	if ((instance == 0) && (console == CONSOLE_IS_ASY)) {
		getcharptr = asygetchar;
		putcharptr = asyputchar;
		ischarptr  = asyischar;
		asyconsole = asy;
	}
#endif
	ddi_report_dev(devi);

	/*
	 * Initialize power management bookkeeping; components are
	 * created idle.
	 */
	if (pm_create_components(devi, 1) == DDI_SUCCESS) {
		pm_set_normal_power(devi, 0, 1);
	} else {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
asyinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register dev_t dev = (dev_t)arg;
	register int instance, error;
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

	if (asyncline == NULL) {
		/* Allocate asyncline structures for maxasy ports */
		/* Note: may be we should use ddi_soft_state mechanism. CHECK */
		asyncline = (struct asyncline *)
			kmem_zalloc(maxasy*sizeof (struct asyncline), KM_SLEEP);
	}

	async = &asyncline[asy->asy_unit];
	mutex_enter(asy->asy_excl);
	async->async_common = asy;
	cv_init(&async->async_flags_cv, NULL, CV_DEFAULT, NULL);
	mutex_exit(asy->asy_excl);
}

/*ARGSUSED3*/
int
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
	mutex_enter(asy->asy_excl);
	asy->asy_priv = (caddr_t)async;

again:
	mutex_enter(asy->asy_excl_hi);
	/*
	 * Block waiting for carrier to come up, unless this is a no-delay open.
	 */
	if (!(async->async_flags & ASYNC_ISOPEN)) {
		/*
		 * Set the default termios settings (cflag).
		 * Others are set in ldterm.
		 */
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
		mutex_exit(asy->asy_excl_hi);
		mutex_exit(asy->asy_excl);
		return (EBUSY);
	} else if ((*dev & OUTLINE) && !(async->async_flags & ASYNC_OUT)) {
		mutex_exit(asy->asy_excl_hi);
		mutex_exit(asy->asy_excl);
		return (EBUSY);
	}

	if (*dev & OUTLINE)
		async->async_flags |= ASYNC_OUT;

	/* Raise DTR on every open */
	mcr = INB(asy->asy_ioaddr, MCR);
	OUTB(asy->asy_ioaddr, MCR, mcr|DTR);

	/*
	 * Check carrier.
	 */

#ifdef SU_DRIVER
	async->async_ttycommon.t_flags |= TS_SOFTCAR; /* &&& kbd/mouse */
#endif
	if ((async->async_ttycommon.t_flags & TS_SOFTCAR) ||
					(INB(asy->asy_ioaddr, MSR) & DCD))
		async->async_flags |= ASYNC_CARR_ON;
	else
		async->async_flags &= ~ASYNC_CARR_ON;
	mutex_exit(asy->asy_excl_hi);

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
			    asy->asy_excl) == 0) {
				async->async_flags &= ~ASYNC_WOPEN;
				mutex_exit(asy->asy_excl);
				return (EINTR);
			}
			async->async_flags &= ~ASYNC_WOPEN;
			goto again;
		}
	} else if ((async->async_flags & ASYNC_OUT) && !(*dev & OUTLINE)) {
			mutex_exit(asy->asy_excl);
			return (EBUSY);
	}

	/* Mark device as busy (for power management) */
	(void) pm_busy_component(asy->asy_dip, 0);

	if (asy->suspended) {
		mutex_exit(asy->asy_excl);
		(void) ddi_dev_is_needed(asy->asy_dip, 0, 1);
		mutex_enter(asy->asy_excl);
	}

	async->async_ttycommon.t_readq = rq;
	async->async_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)async;
	mutex_exit(asy->asy_excl);
	qprocson(rq);
	async->async_flags |= ASYNC_ISOPEN;
	async->async_polltid = 0;
	return (0);
}

/*
 * Close routine.
 */
int
asyclose(queue_t *q, int flag)
{
	struct asyncline *async;
	struct asycom	 *asy;
	int icr, lcr;
	mblk_t	*bp;


#ifdef DEBUG
	if (asydebug & ASY_DEBUG_CLOSE)
		printf("close\n\r");
#endif
	if ((async = (struct asyncline *)q->q_ptr) == NULL)
		return (ENODEV);	/* already closed */
	asy = async->async_common;

	mutex_enter(asy->asy_excl);
	/*
	 * If we still have carrier, wait here until all the data is gone;
	 * if interrupted in close, ditch the data and continue onward.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK))) {
		while ((async->async_flags & ASYNC_CARR_ON) &&
				((async->async_ocnt > 0) ||
				(async->async_flags &
				(ASYNC_BUSY|ASYNC_DELAY|ASYNC_BREAK))))
			if (cv_wait_sig(&lbolt_cv, asy->asy_excl) == 0)
				break;
	}

	/*
	 * If break is in progress, stop it.
	 */
	mutex_enter(asy->asy_excl_hi);
	lcr = INB(asy->asy_ioaddr, LCR);
	if (lcr & SETBREAK) {
		OUTB(asy->asy_ioaddr, LCR, (lcr & ~SETBREAK));
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
		OUTB(asy->asy_ioaddr, MCR, OUT2);
		mutex_exit(asy->asy_excl_hi);
		/*
		 * Don't let an interrupt in the middle of close
		 * bounce us back to the top; just continue closing
		 * as if nothing had happened.
		 */
		if (cv_wait_sig(&lbolt_cv, asy->asy_excl) == 0)
			goto out;
		mutex_enter(asy->asy_excl_hi);
	}
	/*
	 * If nobody's using it now, turn off receiver interrupts.
	 */
	if ((async->async_flags & (ASYNC_WOPEN|ASYNC_ISOPEN)) == 0) {
		icr = INB(asy->asy_ioaddr, ICR);
		OUTB(asy->asy_ioaddr, ICR, (icr & ~RIEN));
	}
	mutex_exit(asy->asy_excl_hi);
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

	mutex_exit(asy->asy_excl);
	qprocsoff(q);
	if (q)
		while (bp = getq(q))
			freemsg(bp);
	q->q_ptr = WR(q)->q_ptr = NULL;
	async->async_polltid = 0;
	async->async_ttycommon.t_readq = NULL;
	async->async_ttycommon.t_writeq = NULL;

	/* Mark device as available for power management */
	(void) pm_idle_component(asy->asy_dip, 0);

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
	ASSERT(mutex_owned(asy->asy_excl));
	ASSERT(mutex_owned(asy->asy_excl_hi));
	return ((async->async_ocnt > 0) ||
			((INB(asy->asy_ioaddr, LSR) & (XSRE|XHRE)) == 0));
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
	ASSERT(mutex_owned(asy->asy_excl));
	ASSERT(mutex_owned(asy->asy_excl_hi));
	/*
	while (lsr = INB(asy->asy_ioaddr, LSR),
					((lsr & (XSRE|XHRE)) == 0)) {
	*/
	while (asy_isbusy(asy)) {
		mutex_exit(asy->asy_excl_hi);
		mutex_exit(asy->asy_excl);
		drv_usecwait(10000);		/* wait .01 */
		mutex_enter(asy->asy_excl);
		mutex_enter(asy->asy_excl_hi);
	}
}

/*
 * Program the ASY port. Most of the async operation is based on the values
 * of 'c_iflag' and 'c_cflag'.
 */
static void
asy_program(struct asycom *asy, int mode)
{
	struct asyncline *async;
	int baudrate, c_flag;
	int icr, lcr;
	int ocflags;

	ASSERT(mutex_owned(asy->asy_excl));
	ASSERT(mutex_owned(asy->asy_excl_hi));

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("program\n\r");
#endif
	async = (struct asyncline *)asy->asy_priv;

	baudrate = async->async_ttycommon.t_cflag & CBAUD;

	async->async_ttycommon.t_cflag &= ~(CIBAUD);
	async->async_ttycommon.t_cflag |= ((baudrate << IBSHIFT) & CIBAUD);

	c_flag = async->async_ttycommon.t_cflag &
		(CLOCAL | CREAD | CSTOPB | CSIZE | PARENB | PARODD | CBAUD);
	OUTB(asy->asy_ioaddr, ICR, 0);	/* disable interrupts */

	ocflags = asy_ocflags[asy->asy_unit];

	/* flush/reset the status registers */
	(void) INB(asy->asy_ioaddr, ISR);
	(void) INB(asy->asy_ioaddr, LSR);
	(void) INB(asy->asy_ioaddr, MSR);
	if (mode == ASY_INIT)
		(void) INB(asy->asy_ioaddr, DAT);

	if (ocflags != (c_flag & ~CLOCAL) || mode == ASY_INIT) {
		/* Set line control */
		lcr = INB(asy->asy_ioaddr, LCR);
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
		OUTB(asy->asy_ioaddr, LCR, DLAB);
		OUTB(asy->asy_ioaddr, DAT, asyspdtab[baudrate] & 0xff);
		OUTB(asy->asy_ioaddr, ICR, (asyspdtab[baudrate] >> 8) & 0xff);
		/* set the line control modes */
		OUTB(asy->asy_ioaddr, LCR, lcr);

		/*
		 * if transitioning from CREAD off to CREAD on,
		 * flush the FIFO buffer if we have one.
		 */
		if ((ocflags & CREAD) == 0 && (c_flag & CREAD))
			if (asy->asy_use_fifo == FIFO_ON)
				OUTB(asy->asy_ioaddr, FIFOR,
					FIFO_ON | FIFODMA | FIFORXFLSH |
					(asy_trig_level & 0xff));

		/* remember the new cflags */
		asy_ocflags[asy->asy_unit] = c_flag & ~CLOCAL;
	}

	if (baudrate == 0)
		OUTB(asy->asy_ioaddr, MCR, RTS|OUT2);
	else
		OUTB(asy->asy_ioaddr, MCR, DTR|RTS|OUT2);

	/*
	 * Call the modem status interrupt handler to check for the carrier
	 * in case CLOCAL was turned off after the carrier came on.
	 * (Note: Modem status interrupt is not enabled if CLOCAL is ON.)
	 */
	async_msint(asy);

	/* Set interrupt control */
	if (c_flag & CLOCAL) {
		/*
		 * direct-wired line ignores DCD, so we don't enable modem
		 * status interrupts.
		 */
		icr = (TIEN | SIEN);
	}
	else
		icr = (TIEN | SIEN | MIEN);

	if (c_flag & CREAD)
		icr |= RIEN;

	OUTB(asy->asy_ioaddr, ICR, icr);
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

	interrupt_id = INB(asy->asy_ioaddr, ISR) & 0x0F;
	async = (struct asyncline *)asy->asy_priv;
	if ((async == NULL) ||
		!(async->async_flags & (ASYNC_ISOPEN|ASYNC_WOPEN))) {
		if (interrupt_id & NOINTERRUPT)
			return (DDI_INTR_UNCLAIMED);
		else {
			/* reset line status */
			(void) INB(asy->asy_ioaddr, LSR);
			/* discard any data */
			(void) INB(asy->asy_ioaddr, DAT);
			/* reset modem sttus */
			(void) INB(asy->asy_ioaddr, MSR);
			return (DDI_INTR_CLAIMED);
		}
	}
	mutex_enter(asy->asy_excl_hi);
	for (; ; interrupt_id = INB(asy->asy_ioaddr, ISR) & 0x0F) {
		if (interrupt_id & NOINTERRUPT)
			break;
		ret_status = DDI_INTR_CLAIMED;
		if (asy->asy_hwtype == ASY82510)
			OUTB(asy->asy_ioaddr, ISR, 0x00); /* set bank 0 */

#ifdef DEBUG
		if (asydebug & ASY_DEBUG_INTR)
			prom_printf("l");
#endif
		lsr = INB(asy->asy_ioaddr, LSR);
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
	mutex_exit(asy->asy_excl_hi);
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
	int		mcr;
	uchar_t		ss;
	int		fifo_len;

	fifo_len = asy->asy_fifo_buf; /* with FIFO buffers */

	if ((ss = async->async_flowc) != '\0') {
		if (ss == async->async_startc) {
			mcr = INB(asy->asy_ioaddr, MCR);
			OUTB(asy->asy_ioaddr, MCR, (mcr | RTS));
			if (!(async->async_ttycommon.t_cflag & CRTSCTS))
				OUTB(asy->asy_ioaddr, DAT, ss);
		} else {
			if (!(async->async_ttycommon.t_cflag & CRTSCTS))
				OUTB(asy->asy_ioaddr, DAT, ss);
			mcr = INB(asy->asy_ioaddr, MCR);
			OUTB(asy->asy_ioaddr, MCR, (mcr & ~RTS));
		}
		async->async_flowc = '\0';
		if (!(async->async_ttycommon.t_cflag & CRTSCTS))
			async->async_flags |= ASYNC_BUSY;
		return;
	}

	if (async->async_ocnt > 0 &&
		! (async->async_flags & (ASYNC_HW_OUT_FLW|ASYNC_STOPPED))) {
		while (fifo_len-- > 0 && async->async_ocnt-- > 0)
			OUTB(asy->asy_ioaddr, DAT, *async->async_optr++);
	}

	if (fifo_len == 0)
		return;

	ASYSETSOFT(asy);
}

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
	uchar_t c = 0;
	uint_t s = 0, needsoft = 0;
	register tty_common_t *tp;

	tp = &async->async_ttycommon;
	if (!(tp->t_cflag & CREAD)) {
		while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
			(void) (INB(asy->asy_ioaddr, DAT) & 0xff);
			lsr = INB(asy->asy_ioaddr, LSR);
		}
		return; /* line is not open for read? */
	}

	while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
		c = 0;
		if (lsr & RCA) {
			c = INB(asy->asy_ioaddr, DAT) & 0xff;
			if ((tp->t_iflag & IXON) &&
					((c & 0177) == async->async_stopc))
				needsoft = 1;
		}

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
#ifdef SU_DRIVER
		/* Look for break on kbd, stdin, or rconsdev */
				    if ((async->async_dev == kbddev) ||
					(async->async_dev == rconsdev) ||
					(async->async_dev == stdindev))
#else
				    if (asy == asyconsole)
#endif
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
		lsr = INB(asy->asy_ioaddr, LSR);
	}
	if (async->async_ttycommon.t_cflag & CRTSCTS)
		if ((int)(RING_CNT(async)) > (RINGSIZE * 3)/4) {
#ifdef DEBUG
			if (asydebug & ASY_DEBUG_HFLOW)
				printf("asy%d: hardware flow stop input.\n\r",
				UNIT(async->async_dev));
#endif
			async->async_flags |= ASYNC_HW_IN_FLOW;
			async->async_flowc = async->async_stopc;
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
	int msr;

	msr = INB(asy->asy_ioaddr, MSR);	/* this resets the interrupt */
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
	if (async->async_ttycommon.t_cflag & CRTSCTS && !(msr & CTS)) {
#ifdef DEBUG
		if (asydebug & ASY_DEBUG_HFLOW)
			printf("asy%d: hflow start\n\r",
				UNIT(async->async_dev));
#endif
		async->async_flags |= ASYNC_HW_OUT_FLW;
	}
	if (asy->asy_hwtype == ASY82510)
		OUTB(asy->asy_ioaddr, MSR, (msr & 0xF0));
	async->async_ext++;
	ASYSETSOFT(asy);
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
			mutex_enter(asy->asy_excl_hi);
			if (asy->asy_flags & ASY_NEEDSOFT) {
				asy->asy_flags &= ~ASY_NEEDSOFT;
				mutex_exit(asy->asy_excl_hi);
				(void) async_softint(asy);
			} else
				mutex_exit(asy->asy_excl_hi);
		}
	}
	return (rv);
}

/*
 * Handle a software interrupt.
 */
static int
async_softint(struct asycom *asy)
{
	struct asyncline *async = (struct asyncline *)asy->asy_priv;
	short	cc;
	mblk_t	*bp;
	queue_t	*q;
	uchar_t	val;
	uchar_t	c;
	tty_common_t	*tp;

#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("process\n\r");
#endif
	mutex_enter(asy->asy_excl);
	if (asy->asy_flags & ASY_DOINGSOFT) {
		mutex_exit(asy->asy_excl);
		return (0);
	}
	tp = &async->async_ttycommon;
	q = tp->t_readq;
	if (q != NULL)
		enterq(q);
	mutex_enter(asy->asy_excl_hi);
	asy->asy_flags |= ASY_DOINGSOFT;
	val = INB(asy->asy_ioaddr, MSR) & 0xFF;
	if (async->async_ttycommon.t_cflag & CRTSCTS) {
		if ((val & CTS) && (async->async_flags & ASYNC_HW_OUT_FLW)) {
#ifdef DEBUG
			if (asydebug & ASY_DEBUG_HFLOW)
				printf("asy%d: hflow start\n\r",
					UNIT(async->async_dev));
#endif
			async->async_flags &= ~ASYNC_HW_OUT_FLW;
			mutex_exit(asy->asy_excl_hi);
			if (async->async_ocnt > 0) {
				mutex_enter(asy->asy_excl_hi);
				async_resume(async);
				mutex_exit(asy->asy_excl_hi);
			} else {
				async_start(async);
			}
			mutex_enter(asy->asy_excl_hi);
		}
	}
	if (async->async_ext) {
		async->async_ext = 0;
		/* check for carrier up */
		if ((val & DCD) || (tp->t_flags & TS_SOFTCAR)) {
			/* carrier present */
			if ((async->async_flags & ASYNC_CARR_ON) == 0) {
				async->async_flags |= ASYNC_CARR_ON;
				if (async->async_flags & ASYNC_ISOPEN) {
					mutex_exit(asy->asy_excl_hi);
					mutex_exit(asy->asy_excl);
					(void) putctl(q, M_UNHANGUP);
					mutex_enter(asy->asy_excl);
					mutex_enter(asy->asy_excl_hi);
				}
				cv_broadcast(&async->async_flags_cv);
			}
		} else {
			if ((async->async_flags & ASYNC_CARR_ON) &&
			    !(tp->t_cflag & CLOCAL)) {
				/*
				 * Carrier went away.
				 * Drop DTR, abort any output in
				 * progress, indicate that output is
				 * not stopped, and send a hangup
				 * notification upstream.
				 */
				val = INB(asy->asy_ioaddr, MCR);
				OUTB(asy->asy_ioaddr, MCR, (val & ~DTR));
				if (async->async_flags & ASYNC_BUSY)
					async->async_ocnt = 0;
				async->async_flags &= ~ASYNC_STOPPED;
				if (async->async_flags & ASYNC_ISOPEN) {
					mutex_exit(asy->asy_excl_hi);
					mutex_exit(asy->asy_excl);
					(void) putctl(q, M_HANGUP);
					mutex_enter(asy->asy_excl);
					mutex_enter(asy->asy_excl_hi);
				}
			}
				async->async_flags &= ~ASYNC_CARR_ON;
		}
	}
	mutex_exit(asy->asy_excl_hi);

	/*
	 * If data has been added to the circular buffer, remove
	 * it from the buffer, and send it up the stream if there's
	 * somebody listening. Try to do it 16 bytes at a time. If we
	 * have more than 16 bytes to move, move 16 byte chunks and
	 * leave the rest for next time around (maybe it will grow).
	 */
	mutex_enter(asy->asy_excl_hi);
	if (!(async->async_flags & ASYNC_ISOPEN)) {
		RING_INIT(async);
		goto rv;
	}
	if ((cc = RING_CNT(async)) <= 0)
		goto rv;
	mutex_exit(asy->asy_excl_hi);
	if (async->async_ttycommon.t_cflag & CRTSCTS) {
		if (!canput(q)) {
			if ((async->async_flags & ASYNC_HW_IN_FLOW) == 0) {
#ifdef DEBUG
				if (!(asydebug & ASY_DEBUG_HFLOW)) {
					printf("asy%d: hflow stop input.\n\r",
					UNIT(async->async_dev));
					if (canputnext(q))
						printf("asy%d: next queue is "
							"ready\r\n",
						UNIT(async->async_dev));
				}
#endif
				mutex_enter(asy->asy_excl_hi);
				async->async_flags |= ASYNC_HW_IN_FLOW;
				async->async_flowc = async->async_stopc;
			} else
				mutex_enter(asy->asy_excl_hi);
			goto rv;
		} else if ((async->async_flags & ASYNC_HW_IN_FLOW) &&
				((int)(RING_CNT(async)) < (RINGSIZE/4))) {
#ifdef DEBUG
			if (asydebug & ASY_DEBUG_HFLOW)
				printf("asy%d: hflow start input.\n\r",
				UNIT(async->async_dev));
#endif
			mutex_enter(asy->asy_excl_hi);
			async->async_flags &= ~ASYNC_HW_IN_FLOW;
			async->async_flowc = async->async_startc;
			mutex_exit(asy->asy_excl_hi);
		}
	}
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_INPUT)
		printf("asy%d: %d char(s) in queue.\n\r",
			UNIT(async->async_dev), cc);
#endif
	if (!(bp = allocb(cc, BPRI_MED))) {
		ttycommon_qfull(&async->async_ttycommon, q);
		mutex_enter(asy->asy_excl_hi);
		goto rv;
	}
	mutex_enter(asy->asy_excl_hi);
	do {
		if (RING_ERR(async, S_ERRORS)) {
			RING_UNMARK(async);
			c = RING_GET(async);
			break;
		} else
			*bp->b_wptr++ = RING_GET(async);
	} while (--cc);
	mutex_exit(asy->asy_excl_hi);
	mutex_exit(asy->asy_excl);
	if (bp->b_wptr > bp->b_rptr) {
		if (!canputnext(q)) {
			if (!canput(q)) {
				cmn_err(CE_NOTE,
					"asy%d: local queue full\n\r",
					UNIT(async->async_dev));
				freemsg(bp);
			} else {
				(void) putq(q, bp);
			}
		} else {
			putnext(q, bp);
		}
	} else {
		freemsg(bp);
	}
	/*
	 * If we have a parity error, then send
	 * up an M_BREAK with the "bad"
	 * character as an argument. Let ldterm
	 * figure out what to do with the error.
	 */
	if (cc)
		(void) putctl1(q, M_BREAK, c);
	mutex_enter(asy->asy_excl);
	mutex_enter(asy->asy_excl_hi);
rv:
	/*
	 * If a transmission has finished, indicate that it's finished,
	 * and start that line up again.
	 */
	if (async->async_break) {
		async->async_break = 0;
		if (async->async_flags & ASYNC_ISOPEN) {
			mutex_exit(asy->asy_excl_hi);
			mutex_exit(asy->asy_excl);
			(void) putctl(q, M_BREAK);
			mutex_enter(asy->asy_excl);
			mutex_enter(asy->asy_excl_hi);
		}
	}
	if (async->async_ocnt <= 0 && (async->async_flags & ASYNC_BUSY)) {
		async->async_flags &= ~ASYNC_BUSY;
		mutex_exit(asy->asy_excl_hi);
		if (async->async_xmitblk)
			freeb(async->async_xmitblk);
		async->async_xmitblk = NULL;
		if (async->async_flags & ASYNC_ISOPEN)
			enterq(async->async_ttycommon.t_writeq);
		async_start(async);
		if (async->async_flags & ASYNC_ISOPEN)
			leaveq(async->async_ttycommon.t_writeq);
		mutex_enter(asy->asy_excl_hi);
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
			mutex_exit(asy->asy_excl_hi);
			mutex_exit(asy->asy_excl);
			cmn_err(CE_NOTE, "asy%d: silo overflow\n\r",
				UNIT(async->async_dev));
			mutex_enter(asy->asy_excl);
			mutex_enter(asy->asy_excl_hi);
		}
		async->async_hw_overrun = 0;
	}
	if (async->async_sw_overrun) {
		if (async->async_flags & ASYNC_ISOPEN) {
			mutex_exit(asy->asy_excl_hi);
			mutex_exit(asy->asy_excl);
			cmn_err(CE_NOTE, "asy%d: ring buffer overflow\n\r",
				UNIT(async->async_dev));
			mutex_enter(asy->asy_excl);
			mutex_enter(asy->asy_excl_hi);
		}
		async->async_sw_overrun = 0;
	}
	asy->asy_flags &= ~ASY_DOINGSOFT;
	mutex_exit(asy->asy_excl_hi);
	if (q != NULL)
		leaveq(q);
	mutex_exit(asy->asy_excl);
	return (0);
}

/*
 * Restart output on a line after a delay or break timer expired.
 */
static void
async_restart(void *arg)
{
	struct asyncline *async = arg;
	struct asycom *asy = async->async_common;
	queue_t *q;
	uchar_t lcr;

	/*
	 * If break timer expired, turn off the break bit.
	 */
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("restart\n\r");
#endif
	mutex_enter(asy->asy_excl);
	if (async->async_flags & ASYNC_BREAK) {
		mutex_enter(asy->asy_excl_hi);
		lcr = INB(asy->asy_ioaddr, LCR);
		OUTB(asy->asy_ioaddr, LCR, (lcr & ~SETBREAK));
		mutex_exit(asy->asy_excl_hi);
	}
	async->async_flags &= ~(ASYNC_DELAY|ASYNC_BREAK);
	if ((q = async->async_ttycommon.t_writeq) != NULL)
		enterq(q);
	async_start(async);
	if (q != NULL)
		leaveq(q);

	mutex_exit(asy->asy_excl);
}

static void
async_start(struct asyncline *async)
{
	async_nstart(async, 0);
}

/*
 * Start output on a line, unless it's busy, frozen, or otherwise.
 */
static void
async_nstart(struct asyncline *async, int mode)
{
	register struct asycom *asy = async->async_common;
	register int cc;
	register queue_t *q;
	register mblk_t *bp;
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

	ASSERT(mutex_owned(asy->asy_excl));

#ifdef lint
	mode = mode;
#endif

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
		mutex_enter(asy->asy_excl_hi);
		if (INB(asy->asy_ioaddr, LSR) & XHRE)
			if (ss == async->async_startc) {
				val = INB(asy->asy_ioaddr, MCR);
				OUTB(asy->asy_ioaddr, MCR, (val | RTS));
				if (!(async->async_ttycommon.t_cflag & CRTSCTS))
					OUTB(asy->asy_ioaddr, DAT, ss);
			} else {
				if (!(async->async_ttycommon.t_cflag & CRTSCTS))
					OUTB(asy->asy_ioaddr, DAT, ss);
				val = INB(asy->asy_ioaddr, MCR);
				OUTB(asy->asy_ioaddr, MCR, (val & ~RTS));
			}
		async->async_flowc = '\0';
		if (!(async->async_ttycommon.t_cflag & CRTSCTS))
			async->async_flags |= ASYNC_BUSY;
		mutex_exit(asy->asy_excl_hi);
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
			mutex_enter(asy->asy_excl_hi);
			val = INB(asy->asy_ioaddr, LCR);
			OUTB(asy->asy_ioaddr, LCR, (val | SETBREAK));
			mutex_exit(asy->asy_excl_hi);
			async->async_flags |= ASYNC_BREAK;
			(void) timeout(async_restart, async, hz / 4);
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_DELAY:
			/*
			 * Arrange for "async_restart" to be called when the
			 * delay expires; it will turn ASYNC_DELAY off,
			 * and call "async_start" to grab the next message.
			 */
			(void) timeout(async_restart, async,
				(clock_t)(*(unsigned char *)bp->b_rptr + 6));
			async->async_flags |= ASYNC_DELAY;
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_IOCTL:
			/*
			 * This ioctl was waiting for the output ahead of
			 * it to drain; obviously, it has.  Do it, and
			 * then grab the next message after it.
			 */
			mutex_exit(asy->asy_excl);
			async_ioctl(async, q, bp);
			mutex_enter(asy->asy_excl);
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
		register unsigned char *p = xmit_addr;
		register int cnt = cc;

		while (cnt--)
			*p++ &= (unsigned char) 0x1f;
	}

	/*
	 * Set up this block for pseudo-DMA.
	 */
	mutex_enter(asy->asy_excl_hi);
	async->async_optr = xmit_addr;
	async->async_ocnt = (short)cc;
	/*
	 * If the transmitter is ready, shove the first
	 * character out.
	 */
	while (fifo_len-- && async->async_ocnt)
		if (INB(asy->asy_ioaddr, LSR) & XHRE) {
			OUTB(asy->asy_ioaddr, DAT, *async->async_optr++);
			async->async_ocnt--;
		}
	async->async_flags |= ASYNC_BUSY;
	mutex_exit(asy->asy_excl_hi);
}

/*
 * Resume output by poking the transmitter.
 */
static void
async_resume(struct asyncline *async)
{
	register struct asycom *asy = async->async_common;
	uchar_t ss;
	uchar_t mcr;

	ASSERT(mutex_owned(asy->asy_excl_hi));
#ifdef DEBUG
	if (asydebug & ASY_DEBUG_PROCS)
		printf("resume\n\r");
#endif

	if (INB(asy->asy_ioaddr, LSR) & XHRE) {
		if ((ss = async->async_flowc) != '\0') {
			if (ss == async->async_startc) {
				mcr = INB(asy->asy_ioaddr, MCR);
				OUTB(asy->asy_ioaddr, MCR, (mcr | RTS));
				if (!(async->async_ttycommon.t_cflag & CRTSCTS))
					OUTB(asy->asy_ioaddr, DAT, ss);
			} else {
				if (!(async->async_ttycommon.t_cflag & CRTSCTS))
					OUTB(asy->asy_ioaddr, DAT, ss);
				mcr = INB(asy->asy_ioaddr, MCR);
				OUTB(asy->asy_ioaddr, MCR, (mcr & ~RTS));
			}
			async->async_flowc = '\0';
			if (!(async->async_ttycommon.t_cflag & CRTSCTS))
				async->async_flags |= ASYNC_BUSY;
		} else if (async->async_ocnt > 0) {
			OUTB(asy->asy_ioaddr, DAT, *async->async_optr++);
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
static void
async_ioctl(struct asyncline *async, queue_t *wq, mblk_t *mp)
{
	register struct asycom *asy = async->async_common;
	register tty_common_t  *tp = &async->async_ttycommon;
	register struct iocblk *iocp;
	register unsigned datasize;
	struct copyreq *crp;
	int error = 0;
	uchar_t val;
	int transparent = 0;
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
	 * For TIOCMGET, do NOT call ttycommon_ioctl() because this function
	 * frees up the message block (mp->b_cont) that contains the address
	 * of the user variable where we need to pass back the bit array.
	 */
	if (iocp->ioc_cmd == TIOCMGET)
		error = -1; /* Do Nothing */
	else

	/*
	 * The only way in which "ttycommon_ioctl" can fail is if the "ioctl"
	 * requires a response containing data to be returned to the user,
	 * and no mblk could be allocated for the data.
	 * No such "ioctl" alters our state.  Thus, we always go ahead and
	 * do any state-changes the "ioctl" calls for.  If we couldn't allocate
	 * the data, "ttycommon_ioctl" has stashed the "ioctl" away safely, so
	 * we just call "bufcall" to request that we be called back when we
	 * stand a better chance of allocating the data.
	 */
	if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0) {
		if (async->async_wbufcid)
			unbufcall(async->async_wbufcid);
		async->async_wbufcid = bufcall(datasize, BPRI_HI, async_reioctl,
		    async);
		return;
	}

	mutex_enter(asy->asy_excl);

	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETS:
			mutex_enter(asy->asy_excl_hi);
			asy_program(asy, ASY_NOINIT);
			mutex_exit(asy->asy_excl_hi);
			break;
		case TCSETSF:
		case TCSETSW:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			mutex_enter(asy->asy_excl_hi);
			if (asy_isbusy(asy))
				asy_waiteot(asy);
			asy_program(asy, ASY_NOINIT);
			mutex_exit(asy->asy_excl_hi);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		error = 0;
		switch (iocp->ioc_cmd) {

		case TCSBRK:
			if (*(int *)mp->b_cont->b_rptr == 0) {
				/*
				 * Set the break bit, and arrange for
				 * "async_restart" to be called in 1/4 second;
				 * it will turn the break bit off, and call
				 * "async_start" to grab the next message.
				 */
				mutex_enter(asy->asy_excl_hi);
				val = INB(asy->asy_ioaddr, LCR);
				OUTB(asy->asy_ioaddr, LCR, (val | SETBREAK));
				mutex_exit(asy->asy_excl_hi);
				async->async_flags |= ASYNC_BREAK;
				(void) timeout(async_restart, async, hz / 4);
			} else {
#ifdef DEBUG
				if (asydebug & ASY_DEBUG_CLOSE)
					printf("asy%d: wait for flush.\n\r",
					UNIT(async->async_dev));
#endif
				mutex_enter(asy->asy_excl_hi);
				asy_waiteot(asy);
				mutex_exit(asy->asy_excl_hi);
#ifdef DEBUG
				if (asydebug & ASY_DEBUG_CLOSE)
					printf("asy%d: ldterm satisfied.\n\r",
					UNIT(async->async_dev));
#endif
			}
			break;

		case TIOCSBRK:
			mutex_enter(asy->asy_excl_hi);
			val = INB(asy->asy_ioaddr, LCR);
			OUTB(asy->asy_ioaddr, LCR, (val | SETBREAK));
			mutex_exit(asy->asy_excl_hi);
			break;

		case TIOCCBRK:
			mutex_enter(asy->asy_excl_hi);
			val = INB(asy->asy_ioaddr, LCR);
			OUTB(asy->asy_ioaddr, LCR, (val & ~SETBREAK));
			mutex_exit(asy->asy_excl_hi);
			break;

		case TIOCMSET:
		case TIOCMBIS:
		case TIOCMBIC:
			mutex_enter(asy->asy_excl_hi);
			(void) asymctl(asy, dmtoasy(*(int *)mp->b_cont->b_rptr),
				iocp->ioc_cmd);
			mutex_exit(asy->asy_excl_hi);
			iocp->ioc_error = 0;
			mp->b_datap->db_type = M_IOCACK;
			break;

		case TIOCMGET:
			mutex_enter(asy->asy_excl_hi);

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
				mutex_exit(asy->asy_excl_hi);
				break;
			}

			if (transparent) {
				*(int *)mp->b_cont->b_rptr =
					asymctl(asy, 0, TIOCMGET);
				mp->b_datap->db_type = M_COPYOUT;
				mp->b_cont->b_wptr = mp->b_cont->b_rptr +
					sizeof (int);
				mp->b_wptr = mp->b_rptr+sizeof (struct copyreq);
			} else {
				*(int *)*(int *)mp->b_cont->b_rptr =
					asymctl(asy, 0, TIOCMGET);
				mp->b_datap->db_type = M_IOCACK;
				iocp->ioc_count = sizeof (int);
			}

			mutex_exit(asy->asy_excl_hi);
			break;
		default: /* unexpected ioctl type */
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
	qreply(wq, mp);
	mutex_exit(asy->asy_excl);
}

int
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
int
asywput(queue_t *q, mblk_t *mp)
{
	register struct asyncline *async;
	register struct asycom *asy;

	async = (struct asyncline *)q->q_ptr;
	asy = async->async_common;

	switch (mp->b_datap->db_type) {

	case M_STOP:
		/*
		 * Since we don't do real DMA, we can just let the
		 * chip coast to a stop after applying the brakes.
		 */
		mutex_enter(asy->asy_excl);
		async->async_flags |= ASYNC_STOPPED;
		mutex_exit(asy->asy_excl);
		freemsg(mp);
		break;

	case M_START:
		mutex_enter(asy->asy_excl);
		if (async->async_flags & ASYNC_STOPPED) {
			async->async_flags &= ~ASYNC_STOPPED;
			/*
			 * If an output operation is in progress,
			 * resume it.  Otherwise, prod the start
			 * routine.
			 */
			if (async->async_ocnt > 0) {
				mutex_enter(asy->asy_excl_hi);
				async_resume(async);
				mutex_exit(asy->asy_excl_hi);
			} else {
				async_start(async);
			}
		}
		mutex_exit(asy->asy_excl);
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
				mutex_enter(asy->asy_excl);
				async_nstart(async, 1);
				mutex_exit(asy->asy_excl);
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
			mutex_enter(asy->asy_excl);
			async_start(async);
			mutex_exit(asy->asy_excl);
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
			mutex_enter(asy->asy_excl);

			/*
			 * Abort any output in progress.
			 */
			mutex_enter(asy->asy_excl_hi);
			if (async->async_flags & ASYNC_BUSY)
				async->async_ocnt = 0;
			mutex_exit(asy->asy_excl_hi);

			/* Flush FIFO buffers */
			if (asy->asy_use_fifo == FIFO_ON) {
				OUTB(asy->asy_ioaddr, FIFOR,
					FIFO_ON | FIFODMA | FIFOTXFLSH |
					(asy_trig_level & 0xff));
			}

			/*
			 * Flush our write queue.
			 */
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			mutex_exit(asy->asy_excl);
			*mp->b_rptr &= ~FLUSHW;	/* it has been flushed */
		}
		if (*mp->b_rptr & FLUSHR) {
			/* Flush FIFO buffers */
			if (asy->asy_use_fifo == FIFO_ON) {
				OUTB(asy->asy_ioaddr, FIFOR,
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
		mutex_enter(asy->asy_excl);
		async_start(async);
		mutex_exit(asy->asy_excl);
		break;

	case M_BREAK:
	case M_DELAY:
	case M_DATA:
		/*
		 * Queue the message up to be transmitted,
		 * and poke the start routine.
		 */
		(void) putq(q, mp);
		mutex_enter(asy->asy_excl);
		async_start(async);
		mutex_exit(asy->asy_excl);
		break;

	case M_STOPI:
		mutex_enter(asy->asy_excl);
		async->async_flowc = async->async_stopc;
		async_start(async);		/* poke the start routine */
		mutex_exit(asy->asy_excl);
		freemsg(mp);
		break;

	case M_STARTI:
		mutex_enter(asy->asy_excl);
		async->async_flowc = async->async_startc;
		async_start(async);		/* poke the start routine */
		mutex_exit(asy->asy_excl);
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
		mutex_enter(asy->asy_excl);
		switch (*mp->b_rptr) {

		case MC_SERVICEIMM:
			async->async_flags |= ASYNC_SERVICEIMM;
			break;

		case MC_SERVICEDEF:
			async->async_flags &= ~ASYNC_SERVICEIMM;
			break;
		}
		mutex_exit(asy->asy_excl);
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
async_reioctl(void *arg)
{
	struct asyncline *async = arg;
	struct asycom *asy = async->async_common;
	queue_t	*q;
	mblk_t		*mp;

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(asy->asy_excl);
	async->async_wbufcid = 0;
	if ((q = async->async_ttycommon.t_writeq) == NULL) {
		mutex_exit(asy->asy_excl);
		return;
	}
	if ((mp = async->async_ttycommon.t_iocpending) != NULL) {
		/* not pending any more */
		async->async_ttycommon.t_iocpending = NULL;
		mutex_exit(asy->asy_excl);
		async_ioctl(async, q, mp);
	} else
		mutex_exit(asy->asy_excl);
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

	mutex_enter(asy->asy_excl);
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
	mutex_exit(asy->asy_excl);
}


#ifndef SU_DRIVER
/*
 * debugger/console support routines.
 */

/*
 * put a character out the first serial port.
 * Do not use interrupts.  If char is LF, put out LF, CR.
 */
asyputchar(c)
uchar_t	c;
{
	if (!asyconsole || (INB(asyconsole->asy_ioaddr, ISR) & 0x38))
		return;
	while ((INB(asyconsole->asy_ioaddr, LSR) & XHRE) == 0) {
		/* wait for xmit to finish */
		if ((INB(asyconsole->asy_ioaddr, MSR) & DCD) == 0)
			return;
		drv_usecwait(10);
	}
	OUTB(asyconsole->asy_ioaddr, DAT, c); /* put the character out */
	if (c == '\n')
		asyputchar(0x0d);
}

/*
 * See if there's a character from the first serial port.
 *
 * If no character is available, return 0.
 * Run in polled mode, no interrupts.
 */

int
asyischar()
{
	if (!asyconsole || (INB(asyconsole->asy_ioaddr, ISR) & 0x38) ||
			((INB(asyconsole->asy_ioaddr, LSR) & RCA) == 0))
		return (0);
	else
		return (1);
}

/*
 * get a character from the first serial port.
 *
 * If no character is available, return -1.
 * Run in polled mode, no interrupts.
 */

int
asygetchar()
{
	if (!asyconsole)
		return (0);
	while ((INB(asyconsole->asy_ioaddr, ISR) & 0x38) ||
			((INB(asyconsole->asy_ioaddr, LSR) & RCA) == 0))
		drv_usecwait(10);
	return (INB(asyconsole->asy_ioaddr, DAT));
}
#endif SU_DRIVER
/*
 * Set or get the modem control status.
 */
static int
asymctl(struct asycom *asy, int bits, int how)
{
	register int mcr_r, msr_r;

	ASSERT(mutex_owned(asy->asy_excl_hi));
	ASSERT(mutex_owned(asy->asy_excl));

	/* Read Modem Control Registers */
	mcr_r = INB(asy->asy_ioaddr, MCR);

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
		msr_r = INB(asy->asy_ioaddr, MSR);

		return (asytodm(mcr_r, msr_r));
	}

	OUTB(asy->asy_ioaddr, MCR, mcr_r);

	return (mcr_r);
}

static int
asytodm(int mcr_r, int msr_r)
{
	register int b = 0;


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

	return (b);
}

static int
dmtoasy(int bits)
{
	register int b = 0;

#ifdef	CAN_NOT_SET	/* only DTR and RTS can be set */
	if (bits & TIOCM_CAR)
		b |= DCD;
	if (bits & TIOCM_CTS)
		b |= CTS;
#endif

	if (bits & TIOCM_RTS)
		b |= RTS;
	if (bits & TIOCM_DTR)
		b |= DTR;

	return (b);
}
