/*
 * Copyright (c) 1988-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cgthree.c	1.46	97/06/27 SMI"

/*
 * SBus 8 bit color memory frame buffer driver
 *
 */

#ifdef _DDICT
#define	NBBY  8
#define	UCRED 13

/*
 * from the new <sys/fbio.h> - not sure if the .h files will change
 * in time, so put it here...
 *
 * This is taken from <sys/memvar.h>
 * note that not all of the code from memvar.h associated with this
 * macro has been migrated from memvar.h to here....
 * not sure of what really needs to move...
 *
 * Also, pr_product() is taken from <sun/sys/pr_util.h>
 * These are copies, not moves, because of what else might break..
 */


/*
 * from <sun/sys/pr_util.h>
 */

#ifndef pr_product
#ifdef sun
#define	pr_product(a, b)	((short)(a) * (short)(b))
#else
#define	pr_product(a, b)	((a) * (b))
#endif
#endif

/*
 * from <sys/memvar.h>
 */

#ifndef MPR_LINEBITPAD
#define	MPR_LINEBITPAD	16
#endif

#ifndef mpr_linebytes
#define	mpr_linebytes(x, depth)				\
	(((pr_product(x, depth) + (MPR_LINEBITPAD-1)) >> 3) &~ 1)
#endif

#else /* _DDICT */

/*
 * from the new <sys/fbio.h> - not sure if the .h files will change
 * in time, so put it here...
 *
 * This is taken from <sys/memvar.h>
 * note that not all of the code from memvar.h associated with this
 * macro has been migrated from memvar.h to here....
 * not sure of what really needs to move...
 *
 * Also, pr_product() is taken from <sun/sys/pr_util.h>
 * These are copies, not moves, because of what else might break..
 */


/*
 * from <sun/sys/pr_util.h>
 */

#ifndef pr_product
#ifdef sun
#define	pr_product(a, b)	((short)(a) * (short)(b))
#else
#define	pr_product(a, b)	((a) * (b))
#endif
#endif

/*
 * from <sys/memvar.h>
 */

#ifndef MPR_LINEBITPAD
#define	MPR_LINEBITPAD	16
#endif

#ifndef mpr_linebytes
#define	mpr_linebytes(x, depth)				\
	(((pr_product(x, depth) + (MPR_LINEBITPAD-1)) >> 3) &~ 1)
#endif

#endif /* _DDICT */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/debug.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/visual_io.h>

#include "sys/fbio.h"
#include "sys/memfb.h"
#include "sys/pr_planegroups.h"	/* non-compliant */
#include "sys/cg3var.h"

#include <sys/modctl.h>

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>


/* configuration options */

#define	NO_HWINIT		/* support for broken PROMs */

#define	CG3DEBUG    (1)

#ifndef CG3DEBUG
#define	CG3DEBUG 0
#endif	/* CG3DEBUG */

#ifdef NO_HWINIT
static int cg3_hwinit = 1;	/* must be non-zero to enable hw init */
static int cg3_type = 0;	/* 0 = 66 Hz, 1 = 72 Hz, 2 = small */
#endif /* NO_HWINIT */

#if (CG3DEBUG > 0) || defined(lint) || defined(__lint)
/* static int cg3_debug = CG3DEBUG; */	/* set internally */
int cg3_debug = 0; 	/* if set externally */
#define	DEBUGF(level, args)	{ if (cg3_debug >= (level)) \
					cmn_err args; }
#else
#define	DEBUGF(level, args)	/* nothing */
#endif /* CG3DEBUG */

#define	CG3_CMAP_ENTRIES	MFB_CMAP_ENTRIES

#define	CG3_DRIVER	(0xc3c3)	/* magic number for the type */

/*
 * Per-instance data
 */
struct cg3_softc {
	int	driver;		/* CG3_DRIVER		*/

	kmutex_t	softc_lock;	/* protect softc	*/
	struct mfb_reg	*reg;		/* video chip registers */

	caddr_t	mfbsave;	/* backup copy of regs for power mgmt */
	short	_w, _h;		/* resolution */
	caddr_t	fb;		/* base of framebuffer */
	off_t	size;		/* total size of frame buffer */
	int	dummysize;	/* size fictious overlay plane */
	int	mapped_by_prom;	/* if true, fb mapped by prom */

	ddi_iblock_cookie_t	iblkc;	/* for blocking cg3 intrs */
	dev_info_t	*devi;		/* for dev-to-devi conversion */

	int	instance;	/* this instance */
	u_short	cmap_index;	/* colormap update index */
	u_short	cmap_count;	/* colormap update count */

	union {				/* shadow color map */
		u_long	cmap_long[CG3_CMAP_ENTRIES * 3 / sizeof (u_long)];
		u_char	cmap_char[3][CG3_CMAP_ENTRIES];
	} cmap_image;
#define	cmap_rgb	cmap_image.cmap_char[0]

	u_char		cmbuf[CG3_CMAP_ENTRIES]; /* see FBIOPUTCMAP */
	int		cg3_suspended;	/* for power management */
};

static int cg3_open(dev_t *, int, int, cred_t *);
static int cg3_close(dev_t, int, int, cred_t *);
static int cg3_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int cg3_mmap(dev_t, off_t, int);

static u_int cg3_intr(caddr_t);

static struct vis_identifier cg3_ident = { "SUNWcg3" };
static void cg3_cmap_bcopy(u_char *, u_char *, u_int);

static struct cb_ops cg3_cb_ops = {
	cg3_open,		/* open */
	cg3_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	cg3_ioctl,		/* ioctl */
	nodev,			/* devmap */
	cg3_mmap,		/* mmap */
	nodev, 			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW|D_MP		/* Driver compatibility flag */
};

static int cg3_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int cg3_identify(dev_info_t *);
static int cg3_attach(dev_info_t *, ddi_attach_cmd_t);
static int cg3_detach(dev_info_t *, ddi_detach_cmd_t);
static int cg3_power(dev_info_t *, int, int);

static struct dev_ops cgthree_ops = {
	DEVO_REV,
	0,			/* refcnt  */
	cg3_info,		/* info */
	cg3_identify,		/* identify */
	nulldev,		/* probe */
	cg3_attach,		/* attach */
	cg3_detach,		/* detach */
	nodev,			/* reset */
	&cg3_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* bus operations */
	cg3_power
};

static void	*cg3_state;	/* opaque handle for all the state */

#define	getsoftc(instance)	\
	((struct cg3_softc *)ddi_get_soft_state(cg3_state, (instance)))

/*
 * default structure for FBIOGATTR ioctl
 */
static struct fbgattr cg3_attr =  {
/*	real_type		owner */
	FBTYPE_SUN3COLOR,	0,
/* fbtype: type		    h  w  depth cms  size */
	{ FBTYPE_SUN3COLOR, 0, 0, 8,    256,  0 },
/* fbsattr: flags emu_type	dev_specific */
	{ 0, FBTYPE_SUN4COLOR, { 0 } },
/*	emu_types */
	{ FBTYPE_SUN3COLOR, FBTYPE_SUN4COLOR, -1, -1}
};

/*
 * Enable/disable interrupt
 */
#define	cg3_int_enable(softc)	mfb_int_enable((softc)->reg)
#define	cg3_int_disable(softc)	mfb_int_disable((softc)->reg)

/*
 * Modes to be passed to cg3_sync()
 */
#define	SYNC_ON		0
#define	SYNC_OFF	1
#define	SYNC_SAVE	2

/*
 * Check if color map update is pending
 */
#define	cg3_update_pending(softc)	((softc)->cmap_count)

/*
 * Private support routines
 */
static int cg3_sync(struct cg3_softc *, int);
static void cg3_reset_cmap(u_char *, u_int);
static void cg3_update_cmap(struct cg3_softc *, u_int, u_int);

#ifdef NO_HWINIT
static int cg3_pseudoPROM(dev_info_t *);
#define	HWINIT_STR		" (init HW)"
#else
#define	HWINIT_STR		/* nothing */
#endif /* NO_HWINIT */

static struct modldrv modldrv = {
	&mod_driverops,		/* This is a device driver */
	"cgthree framebuffer v1.37" HWINIT_STR,
	&cgthree_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "cg3: compiled %s, %s\n", __TIME__, __DATE__));

	if ((e = ddi_soft_state_init(&cg3_state,
	    sizeof (struct cg3_softc), 1)) != 0) {
		DEBUGF(1, (CE_CONT, "done, error %d soft_state\n", e));
		return (e);
	}

	if ((e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&cg3_state);
		DEBUGF(1, (CE_CONT, "done, error %d mod_install\n", e));
	}
	return (e);
}

int
_fini(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "cg3_fini\n"));

	if ((e = mod_remove(&modlinkage)) != 0) {
		return (e);
	}
	ddi_soft_state_fini(&cg3_state);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	DEBUGF(1, (CE_CONT, "cg3_info\n"));

	return (mod_info(&modlinkage, modinfop));
}

static int
cg3_identify(register dev_info_t *devi)
{
	DEBUGF(10, (CE_CONT, "cg3_identify: %s\n", ddi_get_name(devi)));

	if (strcmp(ddi_get_name(devi), "cgthree") == 0 ||
	    strcmp(ddi_get_name(devi), "SUNW,cgthree") == 0) {
		DEBUGF(1, (CE_CONT, "cg3_identify: identified\n"));
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}


static int
cg3_attach(register dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register int	instance = ddi_get_instance(devi);
	register struct cg3_softc *softc;
	register int	w, h, linebytes;
	auto int	proplen;
	char		name[16];
	int		err;

	DEBUGF(1, (CE_CONT, "cg3_attach%d, cmd 0x%x\n", instance, cmd));

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		DEBUGF(1, (CE_CONT, "cg3_attach, DDI_RESUME\n"));
#ifdef NO_HWINIT
		/*
		 * Some PROMs don't initialize the hardware on non-console
		 * framebuffers.
		 * Ok, so we peek to see if the device is
		 * out there, try and deduce the monitor type
		 * and the width & height then initialize
		 * it all appropriately (I mean, like why have
		 * a PROM anyway :-)  If we succeed, the "right"
		 * properties will be built into the tree.
		 */
		if (!cg3_pseudoPROM(devi)) {
		  return (DDI_FAILURE);
			/*NOTREACHED*/
		}
#endif /* NO_HWINIT */
		if (!(softc = (struct cg3_softc *)ddi_get_driver_private(devi)))
			return (DDI_FAILURE);

		if ((softc == NULL)			||
		    (softc->driver != CG3_DRIVER)	||
		    (softc->instance != instance))
			return (DDI_FAILURE);

		/* If not suspended, then resume is successful */
		if (!softc->cg3_suspended)
			return (DDI_SUCCESS);

		mutex_enter(&softc->softc_lock);

		/* Restore MFB registers */
		err = cg3_sync(softc, SYNC_ON);

		/* Alert cg3_intr that we've changed the cmap */
		softc->cmap_count = CG3_CMAP_ENTRIES;
		softc->cmap_index = 0;

		/* And turn interrupts on */
		cg3_int_enable(softc);

		if (err != DDI_FAILURE)
			softc->cg3_suspended = 0;

		mutex_exit(&softc->softc_lock);

		return (err);

	default:
		return (DDI_FAILURE);
	}

	/* ****************  normal attach  ********************* */

#ifdef NO_HWINIT
	/*
	 * Some PROMs don't initialize the hardware on non-console
	 * framebuffers - this can be deduced by a missing width
	 * property.. ick.
	 */
	if (ddi_getproplen(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS, "width",
	    &proplen) != DDI_PROP_SUCCESS || proplen != sizeof (int)) {

		/*
		 * Ok, so we peek to see if the device is
		 * out there, try and deduce the monitor type
		 * and the width & height then initialize
		 * it all appropriately (I mean, like why have
		 * a PROM anyway :-)  If we succeed, the "right"
		 * properties will be built into the tree.
		 */
		if (!cg3_pseudoPROM(devi)) {
			return (DDI_FAILURE);
			/*NOTREACHED*/
		}
	}
#endif /* NO_HWINIT */

	/*
	 * Allocate a cg3_softc instance structure for this instance
	 */
	if (ddi_soft_state_zalloc(cg3_state, instance) != 0) {
		return (DDI_FAILURE);
		/*NOTREACHED*/
	}

	softc = getsoftc(instance);
	softc->driver = CG3_DRIVER;
	softc->instance = instance;
	softc->devi = devi;
	ddi_set_driver_private(devi, (caddr_t)softc);

	/*
	 * Map in the device registers
	 */
	if (ddi_map_regs(devi, 0, (caddr_t *)&softc->reg, MFB_OFF_REG,
	    (off_t)sizeof (struct mfb_reg)) != DDI_SUCCESS) {
		ddi_soft_state_free(cg3_state, instance);
		return (DDI_FAILURE);
		/*NOTREACHED*/
	}
	DEBUGF(1, (CE_CONT, "cg3_attach%d reg=0x%x\n",
	    instance, (u_int)softc->reg));

	/*
	 * Make sure the save pointers are NULL
	 * We use these in cg3_{power|sync}
	 */
	softc->mfbsave = NULL;

	/*
	 * Get the basic framebuffer properties
	 */
	softc->_w = w = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "width", 1152);
	softc->_h = h = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "height", 900);
	/*
	 * Compute the size of the frame buffer
	 */
	linebytes = ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "linebytes", mpr_linebytes(w, 8));
	softc->size = ddi_ptob(devi, ddi_btopr(devi, linebytes * h));

	/*
	 * Compute the size of the dummy overlay/enable planes
	 * (we don't actually have one.. but there you are..
	 *  the graphics world seems full of schizoid devices)
	 */
	softc->dummysize = ddi_ptob(devi,
	    ddi_btopr(devi, mpr_linebytes(w, 1) * h)) * 2;

	/*
	 * If this instance is being used as the console,
	 * then the framebuffer may already be mapped in.
	 * Otherwise, we explicitly map it in here.
	 */
	if ((softc->fb = (caddr_t)ddi_getprop(DDI_DEV_T_ANY, devi,
	    DDI_PROP_DONTPASS, "address", 0)) != (caddr_t)0) {
		softc->mapped_by_prom = 1;
		DEBUGF(2, (CE_CONT, "cgthree%d mapped by PROM\n", instance));
	} else {
		/*
		 * XXX	This is a real waste of resources- we need only
		 *	map it in to have it work with SunView - the exception
		 *	rather than the rule.  We should only map it in when
		 *	we get the FBIOGPIXRECT ioctl.
		 */
		if (ddi_map_regs(devi, 0, (caddr_t *)&softc->fb,
		    MFB_OFF_FB, (off_t)softc->size) != DDI_SUCCESS) {
			goto failed;
			/*NOTREACHED*/
		}
	}

	DEBUGF(1, (CE_CONT, "cg3_attach%d fb 0x%p kpfnum 0x%lx\n",
		instance, (void *)softc->fb,
		hat_getkpfnum((caddr_t)softc->fb)));

	/*
	 * Register the interrupt handler
	 * Create and initialize the softc lock
	 * this sequence is from the WDD, aug 94, pg 114
	 * XXX - this should be fixed when the sequence fix is done
	 */
	DEBUGF(1, (CE_CONT,
	    "cg3: interr handler making mutex for softc 0x%x\n",
	    (int)softc));

	(void) ddi_add_intr(devi, 0, &softc->iblkc,
	    NULL, (u_int (*)(caddr_t))nulldev, NULL);
	mutex_init(&softc->softc_lock, NULL, MUTEX_DRIVER, softc->iblkc);
	ddi_remove_intr(devi, 0, softc->iblkc);
	if (ddi_add_intr(devi, 0, &softc->iblkc,
	    NULL, cg3_intr, (caddr_t)softc) != DDI_SUCCESS)
		goto failed;

	/*
	 * Initialize hardware colormap and software colormap images
	 * and if we're not mapped by the PROM, turn the video on.
	 */
	cg3_reset_cmap(softc->cmap_rgb, CG3_CMAP_ENTRIES);
	cg3_update_cmap(softc, 0, CG3_CMAP_ENTRIES);
	if (!softc->mapped_by_prom)
		mfb_set_video(softc->reg, FBVIDEO_ON);

	(void) sprintf(name, "cgthree%d", instance);
	if (ddi_create_minor_node(devi, name, S_IFCHR,
	    instance, DDI_NT_DISPLAY, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		goto failed;
		/*NOTREACHED*/
	}

	ddi_report_dev(devi);
	cmn_err(CE_CONT, "!%s%d: resolution %d x %d\n", ddi_get_name(devi),
	    instance, w, h);

	/*
	 * Initialize power management bookkeeping; components are created idle
	 */
	if (pm_create_components(devi, 2) == DDI_SUCCESS) {
		(void) pm_busy_component(devi, 0);
		pm_set_normal_power(devi, 0, 1);
		pm_set_normal_power(devi, 1, 1);
	} else {
		goto failed;
	}

	/*
	 * dbug - dump out the info needed for the mmap() function
	 * so we can predict the behaviour
	 */
	DEBUGF(2, (CE_CONT,
	    "cg3_attach: MFB_REG_MMAP_OFFSET=0x%x; CG3_MMAP_OFFSET=0x%x\n",
	    MFB_REG_MMAP_OFFSET, CG3_MMAP_OFFSET));

	DEBUGF(2, (CE_CONT,
	    "cg3_attach: softc->size=0x%lx; softc->fb=0x%p; softc->reg=0x%p\n",
	    softc->size, (void *)softc->fb, (void *)softc->reg));

	DEBUGF(2, (CE_CONT, "cg3_attach() softc->dummysize=0x%x\n",
	    softc->dummysize));

	return (DDI_SUCCESS);
	/*NOTREACHED*/

failed:
	(void) cg3_detach(devi, DDI_DETACH);
	return (DDI_FAILURE);
}

static int
cg3_detach(register dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register int instance;
	register struct cg3_softc *softc;
	int err;

	instance = ddi_get_instance(devi);
	softc = getsoftc(instance);

	DEBUGF(1, (CE_CONT, "cg3_detach%d; cmd=0x%x\n", instance, cmd));

	if ((softc == NULL)			||
	    (softc->driver != CG3_DRIVER)	||
	    (softc->devi != devi)		||
	    (softc->instance != instance))
		return (DDI_FAILURE);

	switch (cmd) {

	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		DEBUGF(1, (CE_CONT, "cg3_detach, DDI_SUSPEND\n"));

		/* Do some sanity checks */
		if (softc == NULL)
			return (DDI_FAILURE);

		if (softc->cg3_suspended)
			return (DDI_FAILURE);

		mutex_enter(&softc->softc_lock);

		/* Disable interrupts */
		cg3_int_disable(softc);

		/* Save a copy of the mfb registers */
		err = cg3_sync(softc, SYNC_SAVE);

		if (err != DDI_FAILURE)
			softc->cg3_suspended = 1;

		mutex_exit(&softc->softc_lock);

		return (err);

	default:
		return (DDI_FAILURE);
	}

	/* *******************  normal detach  *********************** */

	if (!softc->mapped_by_prom)
		mfb_set_video(softc->reg, 0);

	cg3_int_disable(softc);

	if (softc->iblkc)
		ddi_remove_intr(devi, (u_int)0, &softc->iblkc);

	if (softc->reg != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->reg,
		    MFB_OFF_REG, (off_t)sizeof (struct mfb_reg));

	/*
	 * Destroy the local mutexes
	 */
	mutex_destroy(&softc->softc_lock);

	(void) ddi_soft_state_free(cg3_state, instance);

	pm_destroy_components(devi);
	return (DDI_SUCCESS);
}

static int
cg3_power(dev_info_t *dip, int cmpt, int level)
{
	register struct cg3_softc *softc;
	int err;

	DEBUGF(1, (CE_CONT, "cg3_power: cmpt=%d, level=%d\n",
	    cmpt, level));

	if (cmpt != 1 || 0 > level || level > 1)
			return (DDI_FAILURE);

	if (!(softc = (struct cg3_softc *)ddi_get_driver_private(dip)))
			return (DDI_FAILURE);

	if ((softc == NULL) || (softc->driver != CG3_DRIVER))
		return (DDI_FAILURE);

	if (level) {
		/* Turn on the sync */
		err = cg3_sync(softc, SYNC_ON);
		if (err == DDI_FAILURE) {
			cmn_err(CE_WARN,
				"cg3_power: Unable to turn on the sync\n");
			return (DDI_FAILURE);
		}

		/* Turn on Video */
		mfb_set_video(softc->reg, FBVIDEO_ON);

	} else {
		/* Turn off Video */
		mfb_set_video(softc->reg, FBVIDEO_OFF);

		/* Turn off the sync */
		err = cg3_sync(softc, SYNC_OFF);
		if (err == DDI_FAILURE) {
			cmn_err(CE_WARN,
			    "cg3_power: Unable to turn off the sync\n");
			return (DDI_FAILURE);
		}
	}

	return (DDI_SUCCESS);
}

/*
 * Enable/Disable the sync.  This doesn't really -disable- the sync,
 * it just drops it down to 4 Hz, which is enough for VESA monitors
 * devices to switch off.
 *
 * To figure out the correct values for the [hv]_sync_{set, clear},
 * refer to the forth code: [from s4video.fth]
 *
 *  Configuration parameters - default values for 1152x900 Sun Monitor
 *  d#	92, 940, 500 = PCLK	  \ Pixel clock (Hz) (Crystal freq.)
 *
 *  d#	61, 800= HSW		  \ Horizontal sweep rate (Hz)
 *  d#	 0.310 = HFP		  \ Horizontal front porch (usec)
 *  d#	 2.1   = HBP		  \ Horizontal back porch (usec)
 *  d#	 1.38  = HS		  \ Horizontal sync width (usec)
 *
 *  d#	65.960 = VSW		  \ Vertical sweep rate (Hz)
 *  d#	    32 = VFP		  \ Vertical front porch (usec)
 *  d#	   502 = VBP		  \ Vertical back porch (usec)
 *  d#	    64 = VS		  \ Vertical sync width (usec)
 *
 *  Additional parameters, specific to S4 video controller
 *  d#	   255 = xcs		  \ Transfer hold off start
 *  d#	    1  = xcc		  \ Transfer hold off end
 *  d#	    8  = VCLKD		  \ Vclock divide ratio
 *				  \    (4/8/16 only)(jumper)
 *  value clksel  \ Clock select (lower Master Control Reg nibble)
 *		  \ Bits 1:0 select divider (1:x, x=bits+1, i.e. 1,2,3,4)
 *		  \ Bits 3:2 select oscillator (0 = osc#1)
 *
 *  Calculate "magic" numbers for registers in S4 video controller
 *  Equations: (derived by Mark Insley and Ed Gonzalez)
 *							  Default:
 *  vclk = PCLK / VCLKD		  \ pixel multiplier
 *  hbs = (vclk / HSW) - 1	  \ Offset 14		  bb
 *  hbc = (vclk * HBP) + hsc	  \ Offset 15		  2b
 *  hss = (vclk * HFP) - 1	  \ Offset 16		   3 *note = 4
 *  hsc = (vclk * HS) + hss	  \ Offset 17		  13 *note = 14
 *  csc = hbs - hsc + (2 * hss)	  \ Offset 18		  ae
 *  *note: hss = 4 instead of 3 seems to leave the screen centered better
 *	   hsc becomes 14 to preserve sync width, but hbc stays the same
 *	   Equivalent to: HFP larger, HBP smaller
 *
 *  vbs = (HSW / VSW) - 1	  \ Offset 19(hi), 1a(lo)  3, a8
 *  vbc = (VBP * HSW) + vsc	  \ Offset 1b		  24
 *  vss = (VFVFP * HSW) - 1	  \ Offset 1c		   1
 *  vsc = (VS  * HSW) + vss	  \ Offset 1d		   5
 *  (xcs)			  \ Offset 1e		  ff
 *  (xcc)			  \ Offset 1f		   1
 */

static int
cg3_sync(register struct cg3_softc *softc, int mode)
{
	int switchoff = 1;

	if ((softc == NULL) || (softc->driver != CG3_DRIVER))
		return (DDI_FAILURE);

	DEBUGF(1, (CE_CONT, "cg3_sync: %d, %d\n",
	    softc->instance, mode));


	switch (mode) {

	case SYNC_ON:
		/* If we are already on, we are done */
		if (!softc->mfbsave)
			return (DDI_SUCCESS);

		/* Restore sync vals */
		bcopy(softc->mfbsave, (caddr_t)softc->reg,
		    sizeof (struct mfb_reg));

		kmem_free(softc->mfbsave, sizeof (struct mfb_reg));

		/* Null it out again so SYNC_OFF realloc's it */
		softc->mfbsave = NULL;
		break;

	case SYNC_SAVE:
		/* Save sync values, but don't turn sync off */
		switchoff = 0;
		/*FALLTHRU*/

	case SYNC_OFF:
		/*
		 * Allocate space (if necessary) and save the MFB regs
		 *
		 * XXX - actually, the pointer should ALWAYS be NULL.
		 * it is NULL'd out via SYNC_ON.
		 * if the pointer is non-NULL, then will just save
		 * the sync values right on top of the valid values.
		 * The exception is the fall-thru case of SYNC_SAVE
		 */

		/* If we are already off, we are done */
		if (softc->mfbsave)
			return (DDI_SUCCESS);

		/* Allocate space (if necessary) and save the MFB regs */
		softc->mfbsave = kmem_alloc(sizeof (struct mfb_reg),
		    KM_NOSLEEP);
		if (!softc->mfbsave)
			return (DDI_FAILURE);

		bcopy((caddr_t)softc->reg, softc->mfbsave,
		    sizeof (struct mfb_reg));

		/* If we're just saving, exit here */
		if (!switchoff)
			break;

		/*
		 * Now set the mfb to values that will drop the
		 * sync to 4 Hz.
		 * hbs = 184.88		0xB8
		 * hbc = 43.03		0x2B
		 * hss = 2.60		0x02
		 * hsc = 18.63		0x13
		 * csc = 171.45		0xAB
		 * vbs = 15624		0x3D, 0x08
		 * vss = 1.0		0x01
		 * vsc = 5.00		0x05
		 */

		softc->reg->h_blank_set		= 0xB8;
		softc->reg->h_blank_clear	= 0x2B;
		softc->reg->h_sync_set		= 0x02;
		softc->reg->h_sync_clear	= 0x13;
		softc->reg->comp_sync_clear	= 0xAB;
		softc->reg->v_blank_set_high	= 0x3D;
		softc->reg->v_blank_set_low	= 0x08;
		softc->reg->v_sync_set		= 0x01;
		softc->reg->v_sync_clear	= 0x05;

		break;
	}

	return (DDI_SUCCESS);
}

static int
cg3_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int instance;
	register int error = DDI_FAILURE;
	register struct cg3_softc *softc;

#if defined(lint) || defined(__lint)
	dip = dip;
#endif

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		instance = getminor((dev_t)arg);

		DEBUGF(2, (CE_CONT,
		    "cg3_info%d(DDI_INFO_DEVT2DEVINFO)\n",
		    instance));

		if ((softc = getsoftc(instance)) != NULL) {
			if ((softc == NULL)			||
			    (softc->driver != CG3_DRIVER)	||
			    (softc->instance != instance))
				return (DDI_FAILURE);
			*result = (void *) softc->devi;
			error = DDI_SUCCESS;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		DEBUGF(2, (CE_CONT,
		    "cg3_info(DDI_INFO_DEVT2INSTANCE) arg=0x%p\n",
		    arg));

		*result = (void *)getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;

	default:
		break;
	}

	return (error);
}


/*ARGSUSED1*/
static int
cg3_open(dev_t *dev_p, int flag, int otyp, cred_t *cred_p)
{
	register int error;
	register int instance = getminor(*dev_p);
	register struct cg3_softc *softc = getsoftc(instance);

	DEBUGF(2, (CE_CONT, "cg3_open%d\n", instance));

	if (otyp != OTYP_CHR) {
		error = EINVAL;
	} else if (softc == NULL) {
		error = ENXIO;
	} else {
		if ((softc == NULL)			||
		    (softc->driver != CG3_DRIVER)	||
		    (softc->instance != instance))
			return (DDI_FAILURE);

		error = 0;
	}

	return (error);
}


/*ARGSUSED1*/
static int
cg3_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	register int instance = getminor(dev);
	register struct cg3_softc *softc;
	register int error;

	DEBUGF(2, (CE_CONT, "cg3_close%d\n", instance));

	if (otyp != OTYP_CHR) {
		error = EINVAL;
	} else if ((softc = getsoftc(instance)) == NULL) {
		error = ENXIO;
	} else {
		if ((softc == NULL)			||
		    (softc->driver != CG3_DRIVER)	||
		    (softc->instance != instance))
			return (DDI_FAILURE);
		/*
		 * Disable cursor compare
		 * Note that modifying the register needs to be atomic
		 */
		mutex_enter(&softc->softc_lock);
		softc->reg->control &= ~MFB_CR_CURSOR;
		mutex_exit(&softc->softc_lock);
		error = 0;
	}

	return (error);
}


/*
 * The cgthree framebuffer layout is described in
 * memfb.h - there are effectively four non-contiguous
 * regions; in order of increasing physical address:
 *
 * - the ID prom		 MFB_OFF_ID	< 1 page
 * - a page of control registers MFB_OFF_REG	< 1 page
 * - the framebuffer itself	 MFB_OFF_FB	for 1M or so
 * - a dummy overlay area	 MFB_OFF_DUMMY
 *
 * In the attach routine we separately mapped the
 * registers and framebuffer to separate kernel virtual
 * address ranges - softc->reg and softc->fb respectively.
 * So in this routine we convert the external offsets known
 * to userland into mappings of these regions. Note there is
 * no actual hardware corresponding to the dummy overlay area,
 * so any attempt to actually map it will be faulted.
 */
/*ARGSUSED2*/
static int
cg3_mmap(dev_t dev, register off_t off, int prot)
{
	int		instance = getminor(dev);
	struct cg3_softc *softc = getsoftc(getminor(dev));
	caddr_t	kvaddr = (caddr_t)0;

	DEBUGF(off ? 9 : 2,
	    (CE_CONT, "cg3_mmap%d: off=0x%x\n", instance, (u_int)off));

	if ((softc == NULL)			||
	    (softc->driver != CG3_DRIVER)	||
	    (softc->instance != instance))
		return (-1);

	if (off == MFB_REG_MMAP_OFFSET) {
		/*
		 * Need ddi_get_cred() here because segdev doesn't
		 * pass the credentials of the faultee through to here.
		 */
		if (drv_priv(ddi_get_cred()) != EPERM)
			kvaddr = (caddr_t)softc->reg;
	} else if (off >= CG3_MMAP_OFFSET) {
		if ((off - CG3_MMAP_OFFSET) < softc->size)
			kvaddr = (caddr_t)softc->fb + off - CG3_MMAP_OFFSET;
	} else if ((u_int)off < softc->dummysize) {
		/*
		 * in 4.x, this case would return the kpfnum of
		 * the first page of a bogus dummy area regardless
		 * of the offset value, e.g.
		 *
		 * kvaddr = (caddr_t)softc->reg + (MFB_OFF_DUMMY-MFB_OFF_REG);
		 *
		 * We need to sort out what's really intended here..
		 */
		cmn_err(CE_WARN, "cg3_mmap: can't map dummy space!");
		DEBUGF(2, (CE_CONT, "cg3_mmap() softc->dummysize=0x%x\n",
		    softc->dummysize));
	} else if ((u_int)(off - softc->dummysize) < softc->size) {
		kvaddr = (caddr_t)softc->fb + off - softc->dummysize;
	} else {
		/*
		 * no mapping - dump the info if debugging
		 */
		DEBUGF(2, (CE_CONT, "cg3_mmap() no mapping off=0x%lx\n", off));

		DEBUGF(2, (CE_CONT,
		    "cg3_mmap: MFB_REG_MMAP_OFFSET %x CG3_MMAP_OFFSET %x\n",
		    MFB_REG_MMAP_OFFSET, CG3_MMAP_OFFSET));

		DEBUGF(2, (CE_CONT,
		    "cg3_mmap: softc->size %lx softc->fb %p softc->reg %p\n",
		    softc->size, (void *)softc->fb, (void *)softc->reg));

		DEBUGF(2, (CE_CONT, "cg3_mmap() softc->dummysize=0x%x\n",
		    softc->dummysize));
	}

	if (kvaddr != 0) {
		DEBUGF(kvaddr != (caddr_t)softc->fb ? 9 : 2, (CE_CONT,
		    "cg3_mmap: kpfnum 0x%lx (0x%p)\n",
		    hat_getkpfnum(kvaddr), (void *)kvaddr));
		return (hat_getkpfnum(kvaddr));
	} else
		return (-1);
}


/*
 * Compute color map update parameters: starting
 * index and count.  If count is already nonzero,
 * adjust values as necessary.
 */
static void
cg3_update_cmap(struct cg3_softc *softc, u_int index, u_int count)
{
	if ((softc == NULL) || (softc->driver != CG3_DRIVER))
		return;

	if (cg3_update_pending(softc)) {
		if (index + count >
		    (int)(softc->cmap_count += softc->cmap_index)) {
			softc->cmap_count = index + count;
		}
		if (index < (int)softc->cmap_index) {
			softc->cmap_index = index;
		}
		softc->cmap_count -= softc->cmap_index;
	} else {
		softc->cmap_index = index;
		softc->cmap_count = count;
	}
}


/*
 * This assumes ddi_copyin() and ddi_copyout() are being used
 * The "mode" parameter in the ioctl() call is the 4th arg
 * to these two functions.
 * Putting it explicitly into this macro then assumes it will
 * only be used in the context of ioctl()
 */
#define	COPY(f, s, d, c)	(f((caddr_t)(s),	\
				    (caddr_t)(d), (u_int)(c), mode))

/*ARGSUSED*/
static int
cg3_ioctl(dev_t dev,
    int cmd, intptr_t arg, int mode, cred_t *cred_p, int *rval_p)
{
	int instance = getminor(dev);
	struct cg3_softc *softc = getsoftc(instance);
	int error = 0;

	DEBUGF(2, (CE_CONT, "cg3_ioctl%d: 0x%x\n", instance, cmd));

	if ((softc == NULL)			||
	    (softc->driver != CG3_DRIVER)	||
	    (softc->instance != instance))
		return (EINVAL);


#define	copyinfun	ddi_copyin
#define	copyoutfun	ddi_copyout

	switch (cmd) {

	case VIS_GETIDENTIFIER:
		DEBUGF(2, (CE_CONT, "cg3_ioctl() VIS_GETIDENTIFIER\n"));
		if (COPY(copyoutfun, &cg3_ident, arg,
		    sizeof (struct vis_identifier)) != 0)
			error = EFAULT;
		break;

	case FBIOPUTCMAP:
	case FBIOGETCMAP: {
		auto struct fbcmap	fbcmap;
		register struct fbcmap	*cmap = &fbcmap;
		register u_int		index, count, entries;
		register u_char		*map;
		register u_char		*cmbuf = softc->cmbuf;

		if (COPY(copyinfun, arg, cmap, sizeof (struct fbcmap)) != 0) {
			error = EFAULT;
			break;
			/*NOTREACHED*/
		}

		index = (u_int)cmap->index;
		count = (u_int)cmap->count;

		switch (PIX_ATTRGROUP(index)) {
		case 0:
		case PIXPG_8BIT_COLOR:
			DEBUGF(2, (CE_CONT,
"cg3_ioctl() FBIOget/putCMAP; PIX_ATTRGROUP(index=%d)=0x%x\n",
				index, PIX_ATTRGROUP(index)));

			map = softc->cmap_rgb;
			entries = CG3_CMAP_ENTRIES;
			break;

		default:
			return (EINVAL);
			/*NOTREACHED*/
		}

		if ((index &= PIX_ALL_PLANES) >= entries ||
		    index + count > entries) {
			error = EINVAL;
			break;
			/*NOTREACHED*/
		}


		if (count == 0) {
			break;
			/*NOTREACHED*/
		}

		/*
		 * We force exclusive access for both read and write to the
		 * colormap image in softc.  This is a little stricter
		 * than needed, since there could be any number of readers
		 * so long as there were no writers.  Since color map reads
		 * are so rare the speed gain is not worth the complication.
		 */
		mutex_enter(&softc->softc_lock);

		if (cmd == FBIOPUTCMAP) {
			DEBUGF(2, (CE_CONT, "FBIOPUTCMAP\n"));

			map += index * 3;

			if (COPY(copyinfun, cmap->red, cmbuf, count) != 0)
				goto copyin_error;
			cg3_cmap_bcopy(cmbuf, map++, count);

			if (COPY(copyinfun, cmap->green, cmbuf, count) != 0)
				goto copyin_error;
			cg3_cmap_bcopy(cmbuf, map++, count);

			if (COPY(copyinfun, cmap->blue, cmbuf, count) != 0)
				goto copyin_error;
			cg3_cmap_bcopy(cmbuf, map, count);

			cg3_update_cmap(softc, index, count);
			cg3_int_enable(softc);
			mutex_exit(&softc->softc_lock);
			break;
			/*NOTREACHED*/
	copyin_error:
			/*
			 * and fix up the interrupts again
			 */
			if (cg3_update_pending(softc))
				cg3_int_enable(softc);
			error = EFAULT;
		} else {
			DEBUGF(2, (CE_CONT, "FBIOGETCMAP\n"));

			map += index * 3;

			cg3_cmap_bcopy(cmbuf, map++, -count);
			if (COPY(copyoutfun, cmbuf, cmap->red, count) != 0) {
				error = EFAULT;
				mutex_exit(&softc->softc_lock);
				break;
				/*NOTREACHED*/
			}

			cg3_cmap_bcopy(cmbuf, map++, -count);
			if (COPY(copyoutfun, cmbuf, cmap->green, count) != 0) {
				error = EFAULT;
				mutex_exit(&softc->softc_lock);
				break;
				/*NOTREACHED*/
			}

			cg3_cmap_bcopy(cmbuf, map, -count);
			if (COPY(copyoutfun, cmbuf, cmap->blue, count) != 0) {
				mutex_exit(&softc->softc_lock);
				error = EFAULT;
				break;
				/*NOTREACHED*/
			}
		}
		mutex_exit(&softc->softc_lock);
	}
	break;

	case FBIOGATTR: {
		auto struct fbgattr attr = cg3_attr;

		DEBUGF(2, (CE_CONT, "FBIOGATTR\n"));

		attr.fbtype.fb_width  = softc->_w;
		attr.fbtype.fb_height = softc->_h;
		attr.fbtype.fb_size   = softc->size;

		DEBUGF(2, (CE_CONT, "FBIOGATTR; wide=%d; high=%d; size=0x%lx\n",
			softc->_w, softc->_h, softc->size));

		if (COPY(copyoutfun, &attr, arg, sizeof (struct fbgattr)))
			error = EFAULT;
	}	break;

	case FBIOGTYPE: {
		auto struct fbtype fb = cg3_attr.fbtype;

		DEBUGF(2, (CE_CONT, "FBIOGTYPE\n"));

		fb.fb_type = FBTYPE_SUN4COLOR;
		fb.fb_width  = softc->_w;
		fb.fb_height = softc->_h;
		fb.fb_size   = softc->size;

		DEBUGF(2, (CE_CONT, "FBIOGTYPE; wide=%d; high=%d; size=0x%lx\n",
			softc->_w, softc->_h, softc->size));

		if (COPY(copyoutfun, &fb, arg, sizeof (struct fbtype)) != 0)
			error = EFAULT;
	}	break;

	case FBIOSVIDEO: {
		auto int video;

		DEBUGF(2, (CE_CONT, "FBIOSVIDEO\n"));
		if (COPY(copyinfun, arg, &video, sizeof (int)) != 0)
			error = EFAULT;
		else {
			mutex_enter(&softc->softc_lock);
			mfb_set_video(softc->reg, video & FBVIDEO_ON);
			mutex_exit(&softc->softc_lock);
		}
	}	break;

	case FBIOGVIDEO: {
		auto int video;

		DEBUGF(2, (CE_CONT, "FBIOGVIDEO\n"));
		mutex_enter(&softc->softc_lock);
		video = mfb_get_video(softc->reg) ? FBVIDEO_ON : FBVIDEO_OFF;
		mutex_exit(&softc->softc_lock);
		if (COPY(copyoutfun, &video, arg, sizeof (int)) != 0)
			error = EFAULT;
	}	break;

	default:
		error = ENOTTY;
		break;
	} /* switch (cmd) */

	return (error);
}

/*
 * This assures that the COPY macro only used within ioctl()
 */
#undef	COPY

static u_int
cg3_intr(register caddr_t int_handler_arg)
{
	register struct cg3_softc *softc = (struct cg3_softc *)int_handler_arg;
	register int	instance = ddi_get_instance(softc->devi);

	(void) mfb_int_pending(softc->reg);

	if ((softc == NULL)			||
	    (softc->driver != CG3_DRIVER)	||
	    (softc->instance != instance))
		return (DDI_INTR_UNCLAIMED);

	if (!cg3_update_pending(softc)) {
		DEBUGF(6, (CE_NOTE, "cg3_intr%d: but none pending\n",
		    instance));
		cg3_int_disable(softc);
		return (DDI_INTR_UNCLAIMED);
		/*NOTREACHED*/
	}

	mutex_enter(&softc->softc_lock);

	DEBUGF(7, (CE_CONT, "cg3_intr%d\n", ddi_get_instance(softc->devi)));

	if (cg3_update_pending(softc)) {
		register struct mfb_cmap *cmap = &softc->reg->cmap;
		register int		index = softc->cmap_index;
		register int		count = softc->cmap_count;
		register u_long		*in, *out;

		in = &softc->cmap_image.cmap_long[0];
		out = (u_long *)&cmap->cmap;

		/*
		 * count multiples of 4 RGB entries
		 */
		count = (count + (index & 3) + 3) >> 2;

		/*
		 * round index to 4 entry boundary
		 */
		index &= ~3;

		cmap->addr = (u_char)index;
		in = (u_long *) ((caddr_t)in + (index * 3));

		/*
		 * copy 12 bytes (4 RGB entries)
		 * per loop iteration
		 */
		while (--count >= 0) {
			*out = in[0];
			*out = in[1];
			*out = in[2];
			in += 3;
		}

		softc->cmap_count = 0;
	}

	cg3_int_disable(softc);

	mutex_exit(&softc->softc_lock);
	return (DDI_INTR_CLAIMED);
}


/*
 * Initialize a colormap: background = white, all others = black
 */
static void
cg3_reset_cmap(register u_char *cmap, u_int entries)
{
	DEBUGF(4, (CE_CONT, "cg3_reset_cmap(); cmap=0x%p; entries=%d\n",
	    (void *)cmap, entries));

	(void) bzero((char *)cmap, entries * 3);
	cmap[0] = cmap[1] = cmap[2] = 255;
}


/*
 * Copy colormap entries between red, green, or blue array and
 * interspersed rgb array.
 *
 * count > 0 : copy count bytes from buf to rgb
 * count < 0 : copy -count bytes from rgb to buf
 */

static void
cg3_cmap_bcopy(register u_char *bufp, register u_char *rgb, u_int count)
{
	register int rcount = count;

	DEBUGF(4, (CE_CONT,
	    "cg3_cmap_bcopy(); bufp=0x%p; rgb=0x%p; count=%d\n",
	    (void *)bufp, (void *)rgb, count));


	if (--rcount >= 0) {
		do {
			*rgb = *bufp++;
			rgb += 3;
		} while (--rcount >= 0);
	} else {
		rcount = -rcount - 2;
		do {
			*bufp++ = *rgb;
			rgb += 3;
		} while (--rcount >= 0);
	}
}

#ifdef	NO_HWINIT
/*
 * 76Hz sense codes for CG3 FB 501-1718 (prom 1.4)
 * - these really belong in memfb.h, but someone
 *   needs to sort out the namespace there..
 */
#define	MFB_SR_1152_900_76Hz_A	0x40
#define	MFB_SR_1152_900_76Hz_B	0x60

/*
 * tables to initialize the various bits of hardware with
 */

static u_char cg3_mfbvals_type0[] = {	/* 1152 x 900, 66 Hz */
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x04,	0x17, 0x14,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xa8,	0x1b, 0x24,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,
	0
};

static u_char cg3_mfbvals_type1[] = {	/* 1152 x 900, 76 Hz */
	0x14, 0xb7,	0x15, 0x27,	0x16, 0x03,	0x17, 0x0f,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xae,	0x1b, 0x2a,
	0x1c, 0x01,	0x1d, 0x09,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x24,
	0
};

static u_char cg3_mfbvals_type2[] = {	/* 640 x 480, ??Hz */
	0x14, 0x70,	0x15, 0x20,	0x16, 0x08,	0x17, 0x10,
	0x18, 0x06,	0x19, 0x02,	0x1a, 0x31,	0x1b, 0x51,
	0x1c, 0x06,	0x1d, 0x0c,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x22,
	0
};

static u_char *cg3_mfbvals[] = {
	cg3_mfbvals_type0, cg3_mfbvals_type1, cg3_mfbvals_type2
};

static u_char cg3_dacvals[] = {
	4, 0xff,	5, 0x00,	6, 0x70,	7, 0x00,
	0
};

/*
 * Do what the PROM does for the times when the PROM doesn't
 *
 * NOTE: This will map in the registers, init the hardware, unmap the regs.
 * thus, the only effective thing done is init the HW.
 */
static int
cg3_pseudoPROM(register dev_info_t *devi)
{
	auto struct mfb_reg	*mfb;
	static int		w, h;
	register int		mon_type, rval = 0;
	auto char		status;

	DEBUGF(1, (CE_CONT, "cg3_pseudoPROM%d\n", ddi_get_instance(devi)));

	/*
	 * Map in the device registers
	 */
	if (ddi_map_regs(devi, (u_int)0, (caddr_t *)&mfb, MFB_OFF_REG,
	    (off_t)sizeof (struct mfb_reg)) != DDI_SUCCESS) {
		return (0);
		/*NOTREACHED*/
	}

	/*
	 * Is there anything there?
	 */
	if (ddi_peek8(devi, (char *)&mfb->status, &status) == DDI_FAILURE) {
		goto failed;
		/*NOTREACHED*/
	}

	/*
	 * Now try and intuit (guess) what "it" is
	 */
	switch (((int)status) & MFB_SR_ID_MASK) {
	case MFB_SR_ID_COLOR:
		mon_type = ((int)status) & MFB_SR_RES_MASK;
		if (mon_type == MFB_SR_1152_900_76Hz_A ||
		    mon_type == MFB_SR_1152_900_76Hz_B)
			cg3_type = 1;
		break;

	default:
		cmn_err(CE_WARN,
		    "cg3_pseudoPROM%d: can't initialize type 0x%x",
		    ddi_get_instance(devi), status);
		goto failed;
		/*NOTREACHED*/
	}

	/*
	 * cg3_type is a global, and so might be externally
	 * patched to possibly stupid values, so we check it
	 * here .. sigh .. this is all pretty gross really ..
	 *
	 * XXX	Note that this isn't going to work in the event
	 *	you have two broken-but-different devices.
	 *	A (probably) better solution is to put the correct
	 *	property values into an hwconf file.
	 */
	switch (cg3_type) {
	case 2:
		w = 640;
		h = 480;
		break;

	case 0:
	case 1:
		w = 1152;
		h = 900;
		break;

	default:
		cmn_err(CE_WARN, "cg3_pseudoPROM%d: can't init unknown type %d",
		    ddi_get_instance(devi), cg3_type);
		goto failed;
		/*NOTREACHED*/
	}

	if (cg3_hwinit) {
		register u_char	*p;

		/*
		 * Initialize the video chip
		 */
		for (p = cg3_mfbvals[cg3_type]; *p; p += 2)
			((u_char *)mfb)[p[0]] = p[1];

		/*
		 * Initialize the DAC
		 */
		for (p = cg3_dacvals; *p; p += 2) {
			mfb->cmap.addr = p[0];
			mfb->cmap.ctrl = p[1];
		}
	}

	/*
	 * Create the basic properties that the PROM would've created.
	 *
	 * XXX	Do this with an hwconf file?
	 */
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "width", (caddr_t)&w, sizeof (int));
	(void) ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
	    "height", (caddr_t)&h, sizeof (int));

	DEBUGF(1, (CE_CONT, "cg3_pseudoPROM%d: type %d - %d x %d\n",
	    ddi_get_instance(devi), cg3_type, w, h));

succeeded:
	rval = 1;
	/*FALLTHRU*/
failed:
	ddi_unmap_regs(devi, 0, (caddr_t *)&mfb, MFB_OFF_REG,
	    (off_t)sizeof (struct mfb_reg));
	return (rval);
}
#endif /* NO_HWINIT */
