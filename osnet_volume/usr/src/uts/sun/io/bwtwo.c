/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bwtwo.c	1.52	99/11/15 SMI"
/*
 * Generic monochrome frame buffer driver
 *
 * XXX - 12-12-94
 * one of the remaining areas of uncertianty is what to do about
 * the non-sid devices and support for them.
 * should all the code be ripped out, or left in?
 * are there any non-sid devices ever supported now?
 * For the time being, the code is left in.
 * the code which is compiled in when DDI_COMPLIANT is NOT defined
 * is support for the non-sid devices.
 */

#ifdef _DDICT
#define	NBBY 8
#define	UCRED 13	/* s/b ok - defined in <common/sys/ddi.h> */
/*
 * the ddict version of <sys/types.h>, line 10?? defines cred
 * to be a void struct.
 * need to wrap the actual code with #ifndef _DDICT
 * to make it compile properly
 *
 * put this in here as feeble attempt to get around problem
 *	struct cred { int type; } cred_t;	defined in <sys/cred.h>
 */
#define	DDI_COMPLIANT

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

/*
 * struct cred is not in the ddi
 * defined a stub, above, for ddict
 * this is the real code
 */
#ifndef _DDICT
#include <sys/cred.h>
#endif /* _DDICT */

#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/debug.h>

#include <sys/visual_io.h>

/*
 * These should have <> around them, but cannot be changed until
 * the .h files are properly integrated into the release tree
 * ow, the compiler will pick up the wrong versions
 */
#include "sys/fbio.h"
#include "sys/bw2reg.h"		/* bw2 frame buffer defines */
#include "sys/memfb.h"		/* the hardware registers */
#include <sys/modctl.h>

#include <sys/conf.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	NO_HWINIT		/* support for broken PROMs */

#ifndef BW2DEBUG
#define	BW2DEBUG	1
#endif /* BW2DEBUG */

#if (BW2DEBUG == 0) && (defined(lint) || defined(__lint))
#undef BW2DEBUG
#define	BW2DEBUG  1
#endif /* BW2DEBUF == 0 */

#ifdef NO_HWINIT
static int bw2_hwinit = 1;	/* must be non-zero to enable hw init */
#endif /* NO_HWINIT */

#if (BW2DEBUG > 0) || defined(lint) || defined(__lint)
/* static int bw2_debug = BW2DEBUG; */	/* internal debug level */
int bw2_debug = 0;	/* external debug level */
#define	DEBUGF(level, args)	{ if (bw2_debug >= (level)) \
					cmn_err args; }
#else /* BW2DEBUG > 0 */
#define	DEBUGF(level, args)	/* nothing */
#endif /* BW2DEBUG > 0 */

static int bw2_open(dev_t *, int, int, cred_t *);
static int bw2_close(dev_t, int, int, cred_t *);
static int bw2_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int bw2_mmap(dev_t, off_t, int);

static uint_t bw2_intr(caddr_t);

static struct vis_identifier bw2_ident = { "SUNWbw2" };

static struct cb_ops bw2_cb_ops = {
	bw2_open,
	bw2_close,
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	bw2_ioctl,
	nodev,			/* devmap */
	bw2_mmap,
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static int bw2_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result);
static int bw2_identify(dev_info_t *);
static int bw2_probe(dev_info_t *);
static int bw2_attach(dev_info_t *, ddi_attach_cmd_t);
static int bw2_detach(dev_info_t *, ddi_detach_cmd_t);
static int bw2_power(dev_info_t *, int, int);
static int bw2_reset(dev_info_t *, ddi_reset_cmd_t);

static struct dev_ops bwtwo_ops = {
	DEVO_REV,
	0,			/* refcnt  */
	bw2_getinfo,
	bw2_identify,
	bw2_probe,
	bw2_attach,
	bw2_detach,
	bw2_reset,
	&bw2_cb_ops,		/* driver operations */
	(struct bus_ops *)0,	/* no bus operations s/b NULL */
	bw2_power
};


#define	BW2_DRIVER	(0x9864)	/* magic number for type */

/*
 * Per-instance data
 */
struct bw2_softc {
	int		bw2_drv;	/* BW2_DRIVER .. driver tag */
	int		instance;	/* instance number */
	kmutex_t	reg_mutex;	/* mutex for .. */
	void 		*reg;		/* video device registers */

	off_t		reg_offset;	/* offset to get to registers */
	off_t		reg_size;	/* sizeof registers */

	short		_w, _h;		/* resolution */

	caddr_t		fb;		/* base vaddr of framebuff */
	off_t		fb_offset;	/* offset to get to framebuff */
	off_t		fb_size;	/* total size of frame buffer */
	int		linebytes;	/* bytes per line */
	int		bw2_type;	/* type of framebuffer */

	uchar_t	mapped_by_prom;	/* if true, fb mapd by prom */
	uchar_t	bw2_vstate;	/* original state of video */
	uchar_t	is_sid;		/* is a self-ident device */

	/*
	 * use MFB_SR_RES_MASK to determine the monitor type
	 * use MFB_SR_ID_MASK to determine the general graphics device
	 * (ie. the monitor type is a sub_type of the general type)
	 * This value is valid only if .is_sid == 1
	 */
	uchar_t		hw_type;	/* hardware type */

	ddi_iblock_cookie_t	iblkc;	/* for blocking bw2 intrs */
	dev_info_t	*devi;		/* for dev-to-devi conversion */
};

static void *bw2_state;	/* opaque handle where all the state hangs */

#define	getsoftc(minor)	\
	((struct bw2_softc *)ddi_get_soft_state(bw2_state, (minor)))

/*
 * Private support routines
 */

static struct bw2_softc *bw2_setup_softc(dev_info_t *devi);
static struct bw2_softc *bw2_get_softc(dev_info_t *devi);
static struct bw2_softc *bw2_getminor_softc(dev_t dev);
static void bw2_wrapup_softc(register struct bw2_softc *softc);
static void bw2_wrapup_reg_map(register struct bw2_softc *softc);
static int bw2_setup_sid(register struct bw2_softc *softc);
static int bw2_setup_nsid(register struct bw2_softc *softc);
static int bw2_sid_probe(struct bw2_softc *softc);
static int bw2_nsid_probe(struct bw2_softc *softc);
static int bw2_sid_hw_init(register struct bw2_softc *softc);
static int bw2_nsid_hw_init(register struct bw2_softc *softc);
static void bw2_set_video(struct bw2_softc *softc, uint_t on, uint_t intr);
static uint_t bw2_get_video(struct bw2_softc *softc);
static uint_t bw2_intr(caddr_t i_handler_arg);

#ifndef DDI_COMPLIANT
/*
 * These routines only make sense in a sun4 world, and
 * so may not be available on all architectures.
 */
extern void setintrenable(int on), setvideoenable(int on);
extern int getvideoenable(void);

#pragma weak	setintrenable
#pragma weak	setvideoenable
#pragma weak	getvideoenable

#endif /* !DDI_COMPLIANT */

/*
 * The types of framebuffer we deal with
 * BW2_MFB	any self-ident bwtwo
 */
#define	BW2_MFB			0
#define	BW2_NONSID		4

/*
 * nonSID configuration information
 */

#define	nonSID_BW2_PAGESIZE	(8192)
#define	nonSID_BW2_PROBESIZE	((256 * 1024) + nonSID_BW2_PAGESIZE)

#undef nonSID_BW2_PROBESIZE
#undef nonSID_BW2_PAGESIZE

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module.  This one is a driver */
	"bwtwo framebuffer v1.52",
	&bwtwo_ops	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "bw2: compiled %s, %s\n", __TIME__, __DATE__));

	if ((e = ddi_soft_state_init(&bw2_state,
	    sizeof (struct bw2_softc), 1)) != 0) {
		return (e);
	}

	if ((e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&bw2_state);
	}
	return (e);
}


int
_fini(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "bw2: _fini() called\n"));

	if ((e = mod_remove(&modlinkage)) != 0) {
		return (e);
	}
	ddi_soft_state_fini(&bw2_state);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	DEBUGF(1, (CE_CONT, "bw2: _info() called\n"));

	return (mod_info(&modlinkage, modinfop));
}

static int
bw2_identify(dev_info_t *devi)
{
	DEBUGF(10, (CE_CONT, "bw2_identify: %s\n", ddi_get_name(devi)));

	if (strcmp(ddi_get_name(devi), "bwtwo") == 0 ||
	    strcmp(ddi_get_name(devi), "SUNW,bwtwo") == 0) {
		DEBUGF(1, (CE_CONT, "bw2_identify: identified\n"));
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

/*
 * In this generic driver, the probe routine is
 * used as the great leveller between SID and nonSID
 * devices.  Hence it's a bit complex.
 */
static int
bw2_probe(dev_info_t *devi)
{
	register int error;
	register struct bw2_softc *softc;

	if (! devi)
		return (DDI_PROBE_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_probe%d\n", ddi_get_instance(devi)));

	/* XXX - need to think about mutex() stuff */

	softc = bw2_setup_softc(devi);
	if (softc == NULL)
		return (DDI_PROBE_FAILURE);

	if (softc->bw2_drv != BW2_DRIVER)
		return (DDI_PROBE_FAILURE);

	/*
	 * Now prove the pudding (it's in the eating)
	 * do the setup/wrapup here for the functions
	 */
	error = DDI_PROBE_FAILURE;
	if (ddi_dev_is_sid(devi) == DDI_SUCCESS) {
		if (bw2_setup_sid(softc))
			error = bw2_sid_probe(softc);
		bw2_wrapup_reg_map(softc);
	} else {
		if (bw2_setup_nsid(softc))
			error = bw2_nsid_probe(softc);
		bw2_wrapup_reg_map(softc);
	}

	bw2_wrapup_softc(softc);

	return (error);
}


/*
 * setup enough of the state to start a probe.
 * Also used for the attatch()
 * return either a pointer to the softc struct,
 * or NULL on failure
 */

static struct bw2_softc *
bw2_setup_softc(dev_info_t *devi)
{
	register struct bw2_softc *softc;
	register int instance;

	if (! devi)
		return (NULL);

	instance = ddi_get_instance(devi);

	/*
	 * Allocate a soft state structure for this instance
	 * and set up some handy backward links
	 */
	if (ddi_soft_state_zalloc(bw2_state, instance) != DDI_SUCCESS) {
		return (NULL);
		/*NOTREACHED*/
	}
	if ((softc = getsoftc(instance)) == NULL)
		return (NULL);
	softc->bw2_drv = BW2_DRIVER;
	softc->instance = instance;
	softc->devi = devi;
	ddi_set_driver_private(devi, (caddr_t)softc);
	return (softc);    /* pointer all set */
}

/*
 * This function is used to restore the softc pointer once it is setup.
 * typical use is ioctl() or resume, or read,...
 *
 * Should just check for legal, and return NULL if not,
 * but this should be called only for legal reasons.
 */
static struct bw2_softc *
bw2_get_softc(dev_info_t *devi)
{
	register struct bw2_softc *softc;
	register int instance;

	if (! devi)
		return (NULL);

	instance = ddi_get_instance(devi);
	softc = getsoftc(instance);		/* will return a ptr */
	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->instance != instance)	||
	    (softc->devi != devi))
		return (NULL);
	return (softc);    /* pointer all set */
}


/*
 * This function is used to restore the softc pointer once it is setup.
 * This uses the "dev_t" value, not a (dev_info_t *)
 * typical use is ioctl() or resume, or read,...
 *
 * Should just check for legal, and return NULL if not,
 * but this should be called only for legal reasons.
 */
static struct bw2_softc *
bw2_getminor_softc(dev_t dev)
{
	register struct bw2_softc *softc;
	register int instance;

	instance = getminor(dev);
	softc = getsoftc(instance);		/* may return a ptr */
	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->instance != instance))
		return (NULL);
	return (softc);    /* pointer all set */
}


/*
 * wrapup the softc struct
 * for now, mainly just free the struct
 * Set the .bw2_drv type to 0 to force bad struct
 * (in case the free() doesn't trash the struct)
 */

static void
bw2_wrapup_softc(register struct bw2_softc *softc)
{
	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return;

	/*
	 * trash the struct contents.
	 * would really like to zero the whole thing out
	 * (and we could, because we ownm it)
	 * but for now just set critical fields to 0
	 */
	softc->bw2_drv = 0;	/* trash the struct */

	ddi_soft_state_free(bw2_state, softc->instance);
}


/*
 * This is the wrapup of the mapping via the bw2_setup_sid()
 * and bw2_setup_nsid() for the probe.
 * The actions are exactly the same for either kind of probe,
 * so just use one function
 */

static void
bw2_wrapup_reg_map(register struct bw2_softc *softc)
{
	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL))
		return;

	DEBUGF(1, (CE_CONT, "bw2_wrapup_probe%d: mfb_reg 0x%x\n",
	    ddi_get_instance(softc->devi), (uint_t)softc->reg));

	/*
	 * if BW2_NONSID, the registers were mapped out during the
	 * attach() process; but not during the probe()
	 * Thus, if NONSID, && ->reg is NULL, mapped out
	 * else still needs to be mapped out.
	 * In fact, the only time ->reg == NULL is for NONSID
	 * thus can use ->reg as the "flag"
	 */

	if (softc->reg) {
		ddi_unmap_regs(softc->devi, (uint_t)0,
		    (char **)&(softc->reg),
		    softc->reg_offset, softc->reg_size);
	} else {
		if ((softc->bw2_type != BW2_NONSID)	||
		    (softc->reg_offset != 0)		||
		    (softc->reg_size != 0))
			return;		/* error */
	}

	/*
	 * this is set by attach() and others
	 */
	if (!softc->mapped_by_prom && softc->fb != NULL)
		ddi_unmap_regs(softc->devi, (uint_t)0,
		    &softc->fb, softc->fb_offset, softc->fb_size);
}


#ifdef NO_HWINIT
/*
 * Tables to initialize the various mfb bwtwo's.
 * Why doesn't the idPROM have this crap in it?
 *
 * The table entries consist of two byte sets.
 * The first byte is the index into the mfb_reg struct, assuming it's a
 * byte array. The second byte is the value for the register.
 * Note that [0x14] is mfb_reg.h_blank_set, and [0x10] is mfb_reg.control
 * The table delimiter is an offset == 0.
 * This stuff really should be re-written to make this more obvious.
 */

static uchar_t bw2_mv_1600_1280_ecl[] = {	/* high-res ECL */
	0x14, 0x8b,	0x15, 0x28,	0x16, 0x03,	0x17, 0x13,
	0x18, 0x7b,	0x19, 0x05,	0x1a, 0x34,	0x1b, 0x2e,
	0x1c, 0x00,	0x1d, 0x0a,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x21,
	0
};

static uchar_t bw2_mv_1152_900_ecl[] = {		/* low-res ECL */
	0x14, 0x65,	0x15, 0x1e,	0x16, 0x04,	0x17, 0x0c,
	0x18, 0x5e,	0x19, 0x03,	0x1a, 0xa7,	0x1b, 0x23,
	0x1c, 0x00,	0x1d, 0x08,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,
	0
	/* Note - at 0x1a: increase 0xa7 for > 900 lines */
};

static uchar_t bw2_mv_1152_900_analog[] = {	/* low-res analog */
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x03,	0x17, 0x13,
	0x18, 0xb0,	0x19, 0x03,	0x1a, 0xa6,	0x1b, 0x22,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,
	0
};

/*
 * New sense codes that almost certainly belong someplace else..
 * MFB_SR_1152_900_76HZ_A	76Hz monitor sense codes
 * MFB_SR_1152_900_76HZ_B
 * MFB_SR_ID_MSYNC		SR id of 501-1561 bwtwo
 */
#define	MFB_SR_1152_900_76HZ_A	0x40
#define	MFB_SR_1152_900_76HZ_B	0x60
#define	MFB_SR_ID_MSYNC		0x04

/*
 * 501-1561 (prom 1.5) 66Hz
 */
static uchar_t bw2_mv_501_1561_66Hz[] = {
	0x14, 0xbb,	0x15, 0x2b,	0x16, 0x04,	0x17, 0x14,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xa8,	0x1b, 0x24,
	0x1c, 0x01,	0x1d, 0x05,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x20,
	0
};

/*
 * 501-1561 (prom 1.5) 76Hz
 */
static uchar_t bw2_mv_501_1561_76Hz[] = {
	0x14, 0xb7,	0x15, 0x27,	0x16, 0x03,	0x17, 0x0f,
	0x18, 0xae,	0x19, 0x03,	0x1a, 0xae,	0x1b, 0x2a,
	0x1c, 0x01,	0x1d, 0x09,	0x1e, 0xff,	0x1f, 0x01,
	0x10, 0x24,
	0
};
#endif /* NO_HWINIT */

/*
 * setup the register mapping and specific elements of the softc struct
 * This setsup for a known self-ident device
 * return 1 on success; 0 for failure
 */

static int
bw2_setup_sid(register struct bw2_softc *softc)
{
	auto struct mfb_reg *mfb_reg;

	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (0);

	/*
	 * Map in the device registers
	 */
	softc->reg_size = (off_t)sizeof (struct mfb_reg);
	softc->reg_offset = (off_t)MFB_OFF_REG;

	if (ddi_map_regs(softc->devi, 0, (caddr_t *)&mfb_reg,
	    softc->reg_offset, softc->reg_size) != DDI_SUCCESS) {
		return (0);
		/*NOTREACHED*/
	}

	/*
	 * setup the various softc fields we can at this point
	 */

	softc->reg = (void *)mfb_reg;
	softc->fb_offset = MFB_OFF_FB;
	softc->bw2_type = BW2_MFB;
	softc->is_sid = 1;

	/*
	 * do a peek at the device just to get it's basic type
	 * and make sure it's really there
	 */
	if (ddi_peek8(softc->devi,
		(char *)&(((struct mfb_reg *)softc->reg)->status),
			    (char *)&(softc->hw_type)) == DDI_FAILURE) {
		return (0);
		/*NOTREACHED*/
	}

	DEBUGF(1, (CE_CONT, "bw2_setup_sid%d: mfb_reg 0x%x\n",
	    ddi_get_instance(softc->devi), (uint_t)softc->reg));

	return (1);
}


/*
 * This does the setup for the non-sid devices
 * All we really have is a ulong_t register
 */

static int
bw2_setup_nsid(register struct bw2_softc *softc)
{
	auto caddr_t reg;

	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (0);

	/*
	 * Map in the place where the registers might be..
	 */
	softc->reg_size = (off_t)sizeof (ulong_t);
	softc->reg_offset = (off_t)0;

	if (ddi_map_regs(softc->devi, (uint_t)0, (caddr_t *)&reg,
	    softc->reg_offset, softc->reg_size) == DDI_FAILURE) {
		return (0);
		/*NOTREACHED*/
	}

	/*
	 * set the rest of the values we can at this time.
	 * softc->bw2_type and ->fb_offset are set in bw2_probe_nsid()
	 */
	softc->reg = (void *)reg;
	softc->is_sid = 0;

	/*
	 * do the peek() here for the general case
	 */
	if (ddi_peek16(softc->devi, (short *)softc->reg, (short *)0)
					    != DDI_SUCCESS) {
		return (0);
		/*NOTREACHED*/
	}

	DEBUGF(1, (CE_CONT, "bw2_setup_nsid%d: reg 0x%p\n",
	    ddi_get_instance(softc->devi), softc->reg));

	return (1);
}


/*
 * determine the type of monitor
 * and if there is even a need for us to init the hardware.
 * Return DDI_PROBE_DONTCARE for success (yes, seems weird)
 * or DDI_PROBE_FAILURE for failed
 *
 * DO not set the height/width values here,
 * otherwise need to re-probe during attatch().
 */

static int
bw2_sid_probe(register struct bw2_softc *softc)
{
	auto int proplen;

	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (DDI_PROBE_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_sid_probe%d: mfb_reg 0x%x\n",
	    ddi_get_instance(softc->devi), (uint_t)softc->reg));

	/*
	 * Some PROMs don't initialize the hardware on non-console
	 * framebuffers - this can be deduced by a missing width property..
	 * Ick.
	 *
	 * if NO_HWINIT is defined, then determine the monitor.
	 * if not defined, then fail
	 *
	 * Note that no matter what, a peek() has already set
	 * the softc->hw_type
	 */
	if (ddi_getproplen(DDI_DEV_T_ANY, softc->devi,
	    DDI_PROP_DONTPASS, "width", &proplen) != DDI_PROP_SUCCESS ||
	    proplen != sizeof (int)) {
#ifdef NO_HWINIT

		/*
		 * already did the peek()
		 * make sure it's what we think it should be
		 */

		switch ((int)softc->hw_type & MFB_SR_ID_MASK) {
		case MFB_SR_ID_MONO_ECL:
		case MFB_SR_ID_MONO:
		case MFB_SR_ID_MSYNC:
			break;

		default:
			cmn_err(CE_WARN, "%s%d: can't identify type 0x%x",
			    ddi_get_name(softc->devi), softc->instance,
			    (int)softc->hw_type);
			return (DDI_PROBE_FAILURE);
		}

		/* FALLTHRU */
#else /* NO_HWINIT */
		return (DDI_PROBE_FAILED);
#endif /* NO_HWINIT */
	}

	return (DDI_PROBE_DONTCARE);
}

/*
 * This will probe for the non-sid devices
 */

static int
bw2_nsid_probe(register struct bw2_softc *softc)
{
	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (DDI_PROBE_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_nsid_probe%d: reg 0x%x\n",
	    ddi_get_instance(softc->devi), (int)softc->reg));

	/*
	 * The non-P4 framebuffer is just
	 * memory plus a yucky enable register
	 * (hostage to history, sigh) - we map it in later - but other
	 * devices have a separate "register" area which we'll be using.
	 *
	 * for 2.4 (DDI compliant)
	 * the registers are unmapped anyway,
	 * so pay attention in attatch()/detatch()
	 */

	return (DDI_PROBE_SUCCESS);
	/*NOTREACHED*/
}

/*
 * This is the init sequence for sid devices
 * Need to determine the hardware setup is needed, and do so.
 * Only init the hardware, nothing else
 */

static int
bw2_sid_hw_init(register struct bw2_softc *softc)
{
	auto int proplen;
	register dev_info_t *devi;

	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->is_sid == 0)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (DDI_FAILURE);

	devi = softc->devi;

	DEBUGF(1, (CE_CONT, "bw2_sid_hw_init%d: mfb_reg 0x%x\n",
	    ddi_get_instance(devi), (uint_t)softc->reg));

	/*
	 * Some PROMs don't initialize the hardware on non-console
	 * framebuffers - this can be deduced by a missing width property..
	 * Ick.
	 *
	 * at this point, we know if we need a hardware init by
	 * the sid_probe() trick of looking for "width" property.
	 * The issue is: Are we the first device, or the nth?
	 * If the first, no setup yet; if the nth, already setup.
	 * we don't want to tromp on the already setup hardware.
	 */
	if (ddi_getproplen(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS, "width",
	    &proplen) != DDI_PROP_SUCCESS || proplen != sizeof (int)) {
#ifdef NO_HWINIT
		register int mon_type;
		auto int w, h;
		register uchar_t *init;

		/*
		 * set the height && width for the default case,
		 * and change it if needed
		 */
		h = 900;
		w = 1152;	/* from original code */

		/*
		 * make sure it's what we think it should be
		 */

		mon_type = (int)softc->hw_type & MFB_SR_RES_MASK;

		switch ((int)softc->hw_type & MFB_SR_ID_MASK) {
		case MFB_SR_ID_MONO_ECL:
			if (mon_type == MFB_SR_1600_1280) {
				w = 1600;
				h = 1280;
				init = bw2_mv_1600_1280_ecl;
			} else
				init = bw2_mv_1152_900_ecl;
			break;

		case MFB_SR_ID_MONO:
			init = bw2_mv_1152_900_analog;
			break;

		case MFB_SR_ID_MSYNC:
			if (mon_type == MFB_SR_1152_900_76HZ_A ||
			    mon_type == MFB_SR_1152_900_76HZ_B)
				init = bw2_mv_501_1561_76Hz;
			else
				init = bw2_mv_501_1561_66Hz;
			break;

		default:
			cmn_err(CE_PANIC, "%s%d: can't identify type 0x%x",
			    ddi_get_name(softc->devi), softc->instance,
			    (int)softc->hw_type);
			/*NOTREACHED*/
		}

		if (bw2_hwinit) {
			register struct mfb_reg *mfb_reg;
			/*
			 * Initialize video chip
			 * this really should be written to start
			 * at the proper register.
			 */
			mfb_reg = (struct mfb_reg *)softc->reg;
			for (/* space */; *init; init += 2)
				((uchar_t *)mfb_reg)[init[0]] = init[1];
		}

		/*
		 * Create the basic properties the PROM would've created
		 */
		(void) ddi_prop_create(DDI_DEV_T_NONE, devi,
		    DDI_PROP_CANSLEEP, "width",
		    (caddr_t)&(w), sizeof (int));
		(void) ddi_prop_create(DDI_DEV_T_NONE, devi,
		    DDI_PROP_CANSLEEP, "height",
		    (caddr_t)&(h), sizeof (int));

		/* FALLTHRU */

#endif /* NO_HWINIT */
	}
	return (DDI_SUCCESS);
}

/*
 * Init the non-sid device, probably a P4 device
 * set the hight/width properties
 * Only init the hardware
 * Need to init softc->bw2_type and ->fb_offset, because this is the
 * only place we can in a reasonable fashion
 *
 * XXX - shouldn't we first check, becayse we could be the nth device,
 * not the first?
 *
 * 11-02-94 bandit
 * eliminate the p4 code
 */

static int
bw2_nsid_hw_init(register struct bw2_softc *softc)
{
	auto int proplen;
	auto int w, h;

	if ((softc == NULL)			||
	    (softc->bw2_drv != BW2_DRIVER)	||
	    (softc->devi == NULL)		||
	    (softc->instance != ddi_get_instance(softc->devi)))
		return (DDI_PROBE_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_nsid_hw_init%d: reg 0x%x\n",
	    ddi_get_instance(softc->devi), (uint_t)softc->reg));

	/*
	 * first check the property does not exist.
	 * need to do this because this could be the Nth init
	 */
	if (ddi_getproplen(DDI_DEV_T_ANY, softc->devi,
	    DDI_PROP_DONTPASS, "width", &proplen) != DDI_PROP_SUCCESS ||
					    proplen != sizeof (int)) {
		(void) ddi_prop_create(DDI_DEV_T_NONE, softc->devi,
		    DDI_PROP_CANSLEEP, "width",
		    (caddr_t)&w, sizeof (int));
		(void) ddi_prop_create(DDI_DEV_T_NONE, softc->devi,
		    DDI_PROP_CANSLEEP, "height",
		    (caddr_t)&h, sizeof (int));
	}

	/*
	 * The non-P4 framebuffer is just memory
	 * plus a yucky enable register
	 * (hostage to history, sigh) - we map it in later - but other
	 * devices have a separate "register" area which we'll be using.
	 *
	 * We need to do the unmapping here because
	 * this is the real thing.
	 */
	if (softc->bw2_type == BW2_NONSID) {
		ddi_unmap_regs(softc->devi, (uint_t)0,
		    (char **)&softc->reg,
		    softc->reg_offset, softc->reg_size);
		softc->reg = (void *)NULL;
		softc->reg_offset = 0;
		softc->reg_size = 0;	/* reset the values */
	}
	return (DDI_PROBE_SUCCESS);
	/*NOTREACHED*/
}


/*
 * This is an entry point to the driver.
 * at this point, need to allocate resources for the instance.
 */

static int
bw2_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	register struct bw2_softc *softc;
	register int w, h;
	char name[16];

	if (devi == NULL)
		return (DDI_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_attach%d cmd 0x%x\n",
	    ddi_get_instance(devi), cmd));

	switch (cmd) {

	case DDI_ATTACH:

		/*
		 * first, the basic init of the structs
		 * and of the hardware
		 */

		softc = bw2_setup_softc(devi);
		if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
			return (DDI_FAILURE);

		if (ddi_dev_is_sid(devi) == DDI_SUCCESS) {
			if (bw2_setup_sid(softc) == 0)
			    goto failed;
			if (bw2_sid_hw_init(softc) != DDI_SUCCESS)
			    goto failed;
		} else {
			if (bw2_setup_nsid(softc) == 0)
			    goto failed;
			if (bw2_nsid_hw_init(softc) != DDI_SUCCESS)
			    goto failed;
		}


		/*
		 * Get the framebuffer dimensions, and compute mapped size
		 */
		softc->_w = w = ddi_getprop(DDI_DEV_T_ANY, devi,
		    DDI_PROP_DONTPASS, "width", 1152);
		softc->_h = h = ddi_getprop(DDI_DEV_T_ANY, devi,
		    DDI_PROP_DONTPASS, "height", 900);
		softc->linebytes = ddi_getprop(DDI_DEV_T_ANY, devi,
		    DDI_PROP_DONTPASS, "linebytes", mpr_linebytes(w, 1));
		softc->fb_size = (off_t)ddi_ptob(devi,
		    ddi_btopr(devi, softc->linebytes * h));

		/*
		 * If this instance of the device is being used as the
		 * console, then the framebuffer may already be mapped in.
		 */
		if ((softc->fb = (caddr_t)ddi_getprop(DDI_DEV_T_ANY,
		    softc->devi,
		    DDI_PROP_DONTPASS, "address", 0)) != (caddr_t)0) {
			softc->mapped_by_prom = 1;
			DEBUGF(2, (CE_CONT,
			    "bw2_attach%d mapped by PROM\n",
			    softc->instance));
		}

		/*
		 * XXX ... Is this still true ??? needed ???
		 *
		 * This bit of code goes away eventually pending new
		 * mapping routines such that we can work out the
		 * right thing to return to the calling segment driver
		 * without explicitly mapping the framebuffer into the
		 * kernel address space .. until then, we explicitly map
		 * in the entire framebuffer here.
		 */
		if (!softc->mapped_by_prom) {
			if (ddi_map_regs(softc->devi, (uint_t)0,
			    (caddr_t *)&softc->fb, softc->fb_offset,
			    softc->fb_size) != DDI_SUCCESS) {
				goto failed;
				/*NOTREACHED*/
			}
		}

		DEBUGF(1, (CE_CONT, "bw2_attach%d fb 0x%p kpfnum 0x%lx\n",
		    softc->instance, (void *)softc->fb,
		    hat_getkpfnum((caddr_t)softc->fb)));

		/*
		 * Register the interrupt handler, if any.
		 * if a SID, there is no interr handler needed,
		 * but only need a mutex, no cookie needed.
		 * if a non-sid, then need a cookie. mutex, interr
		 */
		if (softc->bw2_type == BW2_MFB) {
			mutex_init(&softc->reg_mutex, NULL, MUTEX_DRIVER, NULL);
		} else {
			/*
			 * one of the "in the know" engineers
			 * claims that one should call a function to
			 * get a cookie, then set the mutex,
			 * then add the interr handler.
			 * however, there is no fcn (that I could find)
			 * that will return the cookie; and the man
			 * pages state ddi_add_intr() returns the cookie.
			 *
			 * this sequence matches the WDD aug 94, pg 114
			 * sequence. It should be replaced with the new
			 * function call to make it invisible.
			 */
			if (ddi_add_intr(softc->devi,
			    (uint_t)0, &softc->iblkc,
			    (ddi_idevice_cookie_t *)0,
			    (uint_t (*)(caddr_t))nulldev,
			    NULL) != DDI_SUCCESS) {
				goto failed;
				/*NOTREACHED*/
			}
			mutex_init(&softc->reg_mutex, NULL, MUTEX_DRIVER,
			    softc->iblkc);
			ddi_remove_intr(softc->devi, (uint_t)0,
			    softc->iblkc);
			if (ddi_add_intr(softc->devi,
			    (uint_t)0, &softc->iblkc,
			    (ddi_idevice_cookie_t *)0, bw2_intr,
			    (caddr_t)softc) != DDI_SUCCESS) {
				goto failed;
				/*NOTREACHED*/
			}
		}

		softc->bw2_vstate = bw2_get_video(softc);
		bw2_set_video(softc, FBVIDEO_ON, 1);

		(void) sprintf(name, "bwtwo%d", softc->instance);
		if (ddi_create_minor_node(softc->devi, name, S_IFCHR,
		    softc->instance, DDI_NT_DISPLAY, 0)
						    == DDI_FAILURE) {
			ddi_remove_minor_node(softc->devi, NULL);
			goto failed;
		}

		ddi_report_dev(softc->devi);
		cmn_err(CE_CONT, "!%s%d: resolution %d x %d\n",
		    ddi_get_name(devi), softc->instance, w, h);

		/*
		 * Initialize power management bookkeeping; components are
		 * created idle.
		 */
		if (pm_create_components(softc->devi, 2) == DDI_SUCCESS) {
			(void) pm_busy_component(softc->devi, 0);
			pm_set_normal_power(softc->devi, 0, 1);
			pm_set_normal_power(softc->devi, 1, 1);
		} else {
			goto failed;
		}

		return (DDI_SUCCESS);
		/*NOTREACHED*/

failed:

		bw2_wrapup_reg_map(softc);
		bw2_wrapup_softc(softc);

		return (DDI_FAILURE);

	case DDI_RESUME:
		/*
		 * setup the softc struct from the prev call
		 */
		softc = bw2_get_softc(devi);
		if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
			return (DDI_FAILURE);
		return (DDI_SUCCESS);

	default:
		/*
		 * Other values of 'cmd' are reserved and may be
		 * extended in future releases.  So we check to see
		 * that we're only responding for the case we've
		 * implemented here.
		 */
		return (DDI_FAILURE);
	}
}

/*
 * Here's what detach and reset are supposed to do.  If you leave them
 * as 'nodev' in your devops structure, all that will happen is that
 * your driver will refuse to unload.
 *
 * DON'T PANIC
 *
 * Now, assuming you want it to unload..
 *
 * The detach(9E) routine of a driver can be seen as the logical inverse
 * of the attach(9E) routine.  In other words it releases the resources
 * managed by a particular devinfo node to the system and quiesces
 * the hardware associated with that node.
 *
 * Once the detach(9E) routine has run successfully, the device should
 * be entirely quiescent, and, if there are no devinfo nodes left for
 * the driver to manage, the driver can be unloaded.
 *
 * detach(9E) is invoked by the system on each devinfo node owned by the
 * driver as part of the mod_remove(9F) routine called by _fini(9E).  The
 * _fini(9E) routine holds locks such that the open(9E) routine can never
 * run simultaneously with the _fini(9E) routine.  The system framework
 * also guarantees that the driver will not be active (open, mmap-ed or
 * otherwise made active) when the driver's detach entry point is called.
 * Of course, the driver must manage it's own interrupt handlers and
 * callbacks, which still may be called, thus if there are non-cancellable
 * callbacks, the driver must refuse to detach.
 *
 * The values taken by the 'cmd' parameter may be extended, so the driver
 * must allow for future extensions by returning DDI_FAILURE, as below.
 */

static int
bw2_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register struct bw2_softc *softc;

	switch (cmd) {

	case DDI_DETACH:
		/*
		 * setup the softc struct from the prev call
		 */
		softc = bw2_get_softc(devi);
		if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
			return (DDI_FAILURE);

		DEBUGF(1, (CE_CONT, "bw2_detach%d cmd 0x%x\n",
		    softc->instance, cmd));

		/*
		 * Restore the state of the device to what it was before
		 * we attached it.
		 */
		bw2_set_video(softc, softc->bw2_vstate, 1);

		if (softc->bw2_type != BW2_MFB) {
			ddi_remove_intr(softc->devi, 0, &softc->iblkc);
		}
		ddi_remove_minor_node(softc->devi, NULL);
		ddi_prop_remove_all(softc->devi);

		bw2_wrapup_reg_map(softc);
		pm_destroy_components(devi);

		mutex_destroy(&softc->reg_mutex);

		bw2_wrapup_softc(softc);

		return (DDI_SUCCESS);

	case DDI_SUSPEND:

		DEBUGF(1, (CE_CONT, "bw2_detach%d (DDI_SUSPEND) 0x%x\n",
		    ddi_get_instance(devi), cmd));

		return (DDI_SUCCESS);

	default:

		DEBUGF(1, (CE_CONT, "bw2_detach%d unsupported cmd 0x%x\n",
		    ddi_get_instance(devi), cmd));

		return (DDI_FAILURE);
	}
}

static int
bw2_power(dev_info_t *devi, int cmpt, int level)
{
	register struct bw2_softc *softc;

	/*
	 * setup the softc struct from the prev call
	 */
	softc = bw2_get_softc(devi);
	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
		return (DDI_FAILURE);

	DEBUGF(1, (CE_CONT, "bw2_power%d cmpt=0x%x; level=0x%x\n",
		    softc->instance, cmpt, level));


	if (cmpt != 1 || 0 > level || level > 1 || !softc)
		return (DDI_FAILURE);
	if (level) {
		bw2_set_video(softc, FBVIDEO_ON, 0);
	} else {
		bw2_set_video(softc, FBVIDEO_OFF, 0);
	}
	return (DDI_SUCCESS);
}

/*
 * An implementation may choose to issue the reset(9E) routine with the
 * parameter DDI_RESET_FORCE at halt time to ensure all the system hardware
 * is correctly quiesced before returning control to the monitor.
 *
 * As above, the semantics of the reset(9E) routine may be extended
 * via additional 'cmd' values, so the routine should return DDI_FAILURE
 * for unrecognised commands.
 */
static int
bw2_reset(dev_info_t *devi, ddi_reset_cmd_t cmd)
{
	register struct bw2_softc *softc;

	switch (cmd) {

	case DDI_RESET_FORCE:
		/*
		 * setup the softc struct from the prev call
		 * See the comment below about the softc being
		 * yanked from us by a dead thread.
		 */

		softc = bw2_get_softc(devi);
		if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
			return (DDI_FAILURE);

		DEBUGF(1, (CE_CONT, "bw2_reset%d cmd 0x%x\n",
		    softc->instance, cmd));

		/*
		 * Unconditionally restore the state of framebuffer video
		 * enable and disable interrupts. Called at high interrupt
		 * level, so we must not block.
		 *
		 * XXX	This needs a little more thought, since we don't
		 *	want to contend for the mutex that bw2_set_video()
		 *	holds.  We also may be halting at the same time
		 *	that another (dead) thread is in the process of
		 *	detaching us, so our softc may already be gone.
		 */
		if (softc) {
			if ((softc->devi != devi) ||
			    (softc->bw2_drv != BW2_DRIVER))
				return (DDI_FAILURE);
			bw2_set_video(softc, softc->bw2_vstate, 1);
		}
		return (DDI_SUCCESS);

	default:

		DEBUGF(1, (CE_CONT, "bw2_reset%d unsupported cmd 0x%x\n",
		    ddi_get_instance(devi), cmd));

		return (DDI_FAILURE);
	}
}

static int
bw2_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error = DDI_FAILURE;
	register struct bw2_softc *softc;

#if defined(lint) || defined(__lint)
	dip = dip;
#endif


	DEBUGF(1,
	    (CE_CONT, "bw2_getinfo cmd 0x%x; arg=0x%p; result=0x%p\n",
	    infocmd, arg, (void *)result));

	/*
	 * test for the arg pointer here because needed in the switch()
	 * but not sure what the default case will need.
	 * Test for the result pointer in the switch() cases
	 * for the same reason
	 */
	if (arg == NULL)
		return (error);		/* need a pointer */

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		if ((arg == NULL) || (result == NULL))
			return (error);
		softc = bw2_getminor_softc((dev_t)arg);
		if (softc != NULL) {
			if (softc->bw2_drv != BW2_DRIVER)
				return (DDI_FAILURE);
			*result = softc->devi;
			error = DDI_SUCCESS;
		} else
			*result = NULL;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		if ((arg == NULL) || (result == NULL))
			return (error);
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
bw2_open(dev_t *dev_p, int flag, int otyp, cred_t *cr)
{
	register int error = 0;
	register struct bw2_softc *softc;

	DEBUGF(2, (CE_CONT, "bw2_open%d\n", getminor(*dev_p)));

	if (dev_p == NULL)
		return (ENXIO);

	if (otyp != OTYP_CHR)
		error = EINVAL;
	else {
		softc = bw2_getminor_softc(*dev_p);
		if (softc == NULL)
			error = ENXIO;
		else if (softc->bw2_drv != BW2_DRIVER)
			error = ENXIO;
	}
	return (error);
}


/*ARGSUSED1*/
static int
bw2_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	register struct bw2_softc *softc;
	register int error = 0;

	DEBUGF(2, (CE_CONT, "bw2_close%d\n", getminor(dev)));

	if (otyp != OTYP_CHR)
		error = EINVAL;
	else {
		softc = bw2_getminor_softc(dev);
		if (softc == NULL)
			error = ENXIO;
		else if (softc->bw2_drv != BW2_DRIVER) /* eg. invalid */
			error = ENXIO;
	}
	if (!error) {
		if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
			return (DDI_FAILURE);

		/*
		 * Disable cursor compare.
		 */
		if (softc->bw2_type == BW2_MFB) {
			mutex_enter(&softc->reg_mutex);
			((struct mfb_reg *)softc->reg)->control &=
			    ~MFB_CR_CURSOR;
			mutex_exit(&softc->reg_mutex);
		}
	}

	return (error);
}



/*ARGSUSED4*/
static int
bw2_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *cred_p, int *rval_p)
{
	int instance = getminor(dev);
	struct bw2_softc *softc = getsoftc(instance);
	int error = 0;

	DEBUGF(2, (CE_CONT, "bw2_ioctl%d: cmd=0x%x\n", instance, cmd));

	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
		return (DDI_FAILURE);

	switch (cmd) {

	case VIS_GETIDENTIFIER:
		if (ddi_copyout(&bw2_ident, (void *)arg,
		    sizeof (struct vis_identifier), mode) != 0)
			error = EFAULT;
		break;

	case FBIOGTYPE: {
		static const struct fbtype bw2_fb_readonly = {
		/*	type		h  w  depth cms size */
			FBTYPE_SUN2BW,  0, 0, 1,    2,  0
		};
		auto struct fbtype bw2_fb = bw2_fb_readonly;

		DEBUGF(2, (CE_CONT, "FBIOGTYPE\n"));

		bw2_fb.fb_height = softc->_h;
		bw2_fb.fb_width = softc->_w;
		bw2_fb.fb_size = softc->fb_size;
		if (ddi_copyout(&bw2_fb, (void *)arg,
		    sizeof (struct fbtype), mode) != 0)
			error = EFAULT;
	}	break;

	case FBIOSVIDEO: {
		auto int on;

		DEBUGF(2, (CE_CONT, "FBIOSVIDEO\n"));
		if (ddi_copyin((void *)arg, &on, sizeof (int), mode) != 0)
			error = EFAULT;
		else
			bw2_set_video(softc, on, 0);
	}	break;

	case FBIOGVIDEO: {
		auto int video;

		DEBUGF(2, (CE_CONT, "FBIOGVIDEO\n"));

		video = bw2_get_video(softc);
		if (ddi_copyout(&video, (void *)arg, sizeof (int), mode) != 0)
			error = EFAULT;
	}	break;

	default:
		error = ENOTTY;
	}

	return (error);

}


/*
 * Note that we treat all the fields of the softc that we use
 * as read-only in this routine -> don't need to grab any mutexen.
 */
/*ARGSUSED2*/
static int
bw2_mmap(dev_t dev, off_t off, int prot)
{
	struct bw2_softc *softc;
	caddr_t kvaddr = (caddr_t)0;

	DEBUGF(off ? 9 : 1,
	    (CE_CONT, "bw2_mmap%d: 0x%x\n",
	    (int)getminor(dev), (uint_t)off));

	softc = bw2_getminor_softc(dev);
	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
		return (DDI_FAILURE);

	if (softc->bw2_type == BW2_MFB && off == MFB_REG_MMAP_OFFSET) {
		/*
		 * Need ddi_get_cred() here because segdev doesn't
		 * pass the credentials of the faultee through to here.
		 */
		if (drv_priv(ddi_get_cred()) != EPERM)
			kvaddr = (caddr_t)softc->reg;
	} else if ((uint_t)off < softc->fb_size)
		kvaddr = (caddr_t)softc->fb + off;

	if (kvaddr != 0) {
		DEBUGF(kvaddr != (caddr_t)softc->fb ? 9 : 2, (CE_CONT,
		    "bw2_mmap: pfnum 0x%lx (kva 0x%p)\n", hat_getkpfnum(kvaddr),
		    (void *)kvaddr));
		return ((int)hat_getkpfnum(kvaddr));
	} else
		return (-1);
}

static uint_t
bw2_intr(register caddr_t i_handler_arg)
{
	register struct bw2_softc *softc;
	register int serviced = DDI_INTR_UNCLAIMED;

	softc = (struct bw2_softc *)i_handler_arg;
	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER))
		return (serviced);

	DEBUGF(7, (CE_CONT, "bw2_intr%d\n", softc->instance));

	mutex_enter(&softc->reg_mutex);

	switch (softc->bw2_type) {

	case BW2_NONSID:
#ifndef DDI_COMPLIANT
		(void) setintrenable(0);
#endif /* !DDI_COMPLIANT */
		serviced = DDI_INTR_CLAIMED;
		break;

	default:
		/*
		 * The mfb version of bwtwo should never interrupt
		 * (We didn't register it's interrupt routine either)
		 */
		break;
	}

	mutex_exit(&softc->reg_mutex);

	return (serviced);
}

static uint_t
bw2_get_video(register struct bw2_softc *softc)
{
	register uint_t r = 0;

	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER) ||
	    (softc->reg == NULL))
		return (FBVIDEO_OFF);	/* a guess on error return */

	mutex_enter(&softc->reg_mutex);

	switch (softc->bw2_type) {
	case BW2_MFB:
		r = mfb_get_video((struct mfb_reg *)softc->reg);
		break;

	case BW2_NONSID: {
#ifndef DDI_COMPLIANT
		r = getvideoenable();
#endif /* !DDI_COMPLIANT */
		break;
	}

	default:
		break;
	}

	mutex_exit(&softc->reg_mutex);

	return (r ? FBVIDEO_ON : FBVIDEO_OFF);
}

static void
bw2_set_video(register struct bw2_softc *softc, uint_t videoon, uint_t intrclr)
{
	if ((softc == NULL) || (softc->bw2_drv != BW2_DRIVER) ||
	    (softc->reg == NULL))
		return;

	videoon = ((videoon & FBVIDEO_ON) == FBVIDEO_ON) ? 1 : 0;

	mutex_enter(&softc->reg_mutex);

	switch (softc->bw2_type) {
	case BW2_MFB:
		mfb_set_video((struct mfb_reg *)softc->reg, videoon);
		break;

	case BW2_NONSID:
#ifndef DDI_COMPLIANT
		(void) setvideoenable(videoon);
		if (intrclr)
			(void) setintrenable(0);
#endif /* !DDI_COMPLIANT */
		break;

	default:
		break;
	}

	mutex_exit(&softc->reg_mutex);
}
