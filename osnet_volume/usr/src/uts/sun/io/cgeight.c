/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cgeight.c	1.26	99/04/14 SMI"


/*
*******************************************************************
* *************************************************************** *
* *  "cgeight.c							* *
* *								* *
* *	Device driver for the True Color			* *
* *	(TC) Family of Frame Buffers				* *
* *	For The Sun Microsystems SBus.				* *
* *								* *
* *	Conventions:						* *
* *	(1) Routines are in alphabetical order.			* *
* *	(2) Prefix conventions for routine names:		* *
* *	(a) cgeightxxx - external entry point for driver.	* *
* *	(b) cg8_xxx - internal pixrect related code.		* *
* *	(c) pip_xxx - routine common to all pip implements.	* *
* *	(d) tc_xxx  - routine common to all hardware configs	* *
* *	(e) tcp_xxx - routine for TC and TCP products.		* *
* *	(f) tcs_xxx - routine for TCS product.			* *
* *								* *
* *		Copyright 1991, Sun Microsystems, Inc.		* *
* *************************************************************** *
*******************************************************************
 */

#ifdef _DDICT
#define	NBBY	8
#define	NBPG	0x1000	/* only for DDICT run */

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

#define	NBPG	PAGESIZE

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

#include <sys/types.h>		/* General type defs. */
#include <sys/param.h>	/* General system parameters and limits. */
#include <sys/file.h>
#include <sys/uio.h>

#include <sys/buf.h>		/* Input / Output buffer defs. */
#include <sys/errno.h>		/* Kernel error defs. */

#include <sys/cred.h>
#include <sys/open.h>

#include <sys/cmn_err.h>
#include <sys/debug.h>			/* for ASSERT and STATIC */

#include "sys/pr_planegroups.h"	/* Bit plane manipulation defs. */

#include <sys/visual_io.h>
#include "sys/fbio.h"		/* General frame buffer ioctl defs. */
#include "sys/memfb.h"		/* SBus memory mapped frame buf layout. */

#include "sys/cg8reg.h"	/* Device dependent memory layouts, etc. */
#include "sys/cg8var.h"	/* Pixrect related variables for our card */

#include <sys/conf.h>
#include <sys/stat.h>

#include <sys/modctl.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 *  Debugging equates and macros
 */
/* #define	TC_DEBUG 2 */	/* for now */


#ifndef TC_DEBUG
#define	TC_DEBUG 1
#endif TC_DEBUG

#define	TEST_NEW_HARDWARE 1

#if TC_DEBUG
/* static int tc1_debug = TC_DEBUG; */	/* internal Debugging level. */
int tc1_debug = 0;		/* external Debugging level. */
#define	DEBUGF(level, args)	\
	{ if (tc1_debug >= (level)) cmn_err args; }
#define	DEBUG_LEVEL(level) {extern int tc1_debug; tc1_debug = level; }
#else TC_DEBUG
#define	DEBUGF(level, args)	/* nothing */
#define	DEBUG_OFF
#define	DEBUG_LEVEL(level)
#endif TC_DEBUG

/*
 *  Information telling how and when to memory map a frame buffer:
 *  Frame buffer memory map region info:
 */
typedef struct Mmap_Info {
	/*
	 * int	group:	Type of frame buffer to memory map.
	 * bgn_offset:	Offset in vm of start of frame buffer.
	 * end_offset:	Offset in vm of 1st word not in fb.
	 * sbus_delta:	Offset in SBus slot (from dev regs) of fb.
	 */
	int	group;
	u_int	bgn_offset;
	u_int	end_offset;
	u_int	sbus_delta;
} Mmap_Info;


#define	CG8_DRIVER	(0xc8c8)	/* magic number */
/*
 *  Per-Unit Device Data Definitions:
 */
typedef struct Tc1_Softc
{
	int		driver;		/* CG8_DRIVER */
	kmutex_t	softc_mutex;	/* to protect updates to softc */

	/* Physical page # of mmap base addr. */
	int		basepage;

#ifdef TEST_NEW_HARDWARE
	/* Base page of slot 3 for testing h/w. */
	int	 test_basepage;
#endif TEST_NEW_HARDWARE

	/*
	 * cmap_begin:	Starting index for color map load.
	 * cmap_count:	number entries to load
	 * cmap_[]:	blue/green/red components of color map
	 */
	int	cmap_begin;
	int	cmap_count;
	u_char	cmap_blue[TC_CMAP_SIZE];
	u_char	cmap_green[TC_CMAP_SIZE];
	u_char	cmap_red[TC_CMAP_SIZE];

	/*
	 * device_reg:	Dev reg area virt. addr.
	 * device_tcs:	Dev reg are on TCS card.
	 */
	volatile Tc1_Device_Map	*device_reg;
	volatile Tcs_Device_Map	*device_tcs;

	/*
	 * dev_reg_mmap_offset:	Dev regs mem offset from selves.
	 * dev_reg_mmap_size:	Dev regr area size in bytes.
	 * dev_reg_sbus_base:	Dev reg area SBus phys. offset
	 * dev_reg_sbus_delta:	SBus offset from dev. regs. (=0).
	 */
	int	dev_reg_mmap_offset;
	int	dev_reg_mmap_size;
	int	dev_reg_sbus_base;
	int	dev_reg_sbus_delta;

	/*
	 * emulation indexed color maps
	 */
	u_char	em_blue[TC_CMAP_SIZE];
	u_char	em_green[TC_CMAP_SIZE];
	u_char	em_red[TC_CMAP_SIZE];

	/*
	 * fb_8bit:		Eight-bit buffer virt. address.
	 *
	 * fb_8bit_mmap_size:	Size region mmapd into virt mem
	 * fb_8bit_sbus_delta:	8-bit SBus offset from dev regs
	 *
	 * fb_model:		Frame buffer model
	 *
	 * fb_mono:		Overlay (mono) buf virt. addr.
	 * fb_mono_mmap_size:	Size region mmapd into virt mem
	 * fb_mono_sbus_delta:	Mono SBus offset from dev regs
	 *
	 * fb_sel:		Enable (select) mem virt. addr.
	 * fb_sel_mmap_size:	Size region mmapd into virt mem
	 * fb_sel_sbus_delta:	Select SBus offset from dev regs.
	 *
	 * fb_tc:		True color buffer virt. address.
	 * fb_tc_mmap_size:	Size region mmapd into virt mem
	 * fb_tc_sbus_delta:	TC SBus offset from dev regs
	 *
	 * fb_video:		Video enable buf virt. addr
	 * fb_video_mmap_size:	Size region mmap into vir. mem
	 * fb_video_sbus_delta:	SBus offset of video enable.
	 */
	u_char	*fb_8bit;

	int	fb_8bit_mmap_size;
	int	fb_8bit_sbus_delta;

	u_char	fb_model;

	u_char	*fb_mono;
	int	fb_mono_mmap_size;
	int	fb_mono_sbus_delta;

	u_char	*fb_sel;
	int	fb_sel_mmap_size;
	int	fb_sel_sbus_delta;

	long	*fb_tc;
	int	fb_tc_mmap_size;
	int	fb_tc_sbus_delta;

	u_char	*fb_video;
	int	fb_video_mmap_size;
	int	fb_video_sbus_delta;

	int	flags;		/* Miscellaneous flags. */
	int	height;		/* Height of display in scan lines. */
	int	width;		/* Width of display in pixels. */

	/*
	 * linebytes1:  # of bytes in a mono scan line.
	 * linebytes8:  # of bytes in an 8-bit scan line.
	 * linebytes32: # of bytes in a TC scan line.
	 */
	int	linebytes1;
	int	linebytes8;
	int	linebytes32;

	/*
	 * if true, fb mapped by prom
	 */
	int	mapped_by_prom;

	kmutex_t	busy_mutex;	/* mutex for .. */
	int		busy;		/* true if this unit is busy */
	ddi_iblock_cookie_t	cg8_iblock_cookie;

	dev_info_t	*devi;   	/* my devinfo ptr */
	int		instance;	/* my instance */

	/*
	 * memory_map_end:	Total size of mapping of regions
	 * mmap_count:		# of entries in use in mmap_info.
	 */
	int	memory_map_end;
	int	mmap_count;

	/*
	 * Information on how to mmap fbs.
	 */
	Mmap_Info	mmap_info[5];

	int	omap_begin;	/* Start index for overlay map load. */
	int	omap_count;	/* # of entries to load. */

	/*
	 * Mono overlay color map: blue, green, red
	 */
	u_char	omap_blue[TC_OMAP_SIZE];
	u_char	omap_green[TC_OMAP_SIZE];
	u_char	omap_red[TC_OMAP_SIZE];

	/*
	 * Plane groups which are possible; enabled
	 */
	u_char	pg_possible[FB_NPGS];
	u_char	pg_enabled[FB_NPGS];

	/*
	 *  -1 if on, else # of suspensions
	 */
	int	pip_on_off_semaphore;

	/*
	 * Berthold: cursor color is frozen.
	 */
	u_char	sw_cursor_color_frozen;

	/*
	 * Current timing regimen (TCS card.)
	 */
	int	timing_regimen;

} Tc1_Softc;

#define	SOFTC register struct Tc1_Softc

/* Compilation configuration switches and macros: */

#define	TC_CMAP_ENTRIES		MFB_CMAP_ENTRIES

/* Configuration information (device driver operations vector) */

static int cgeightopen(dev_t *, int, int, cred_t *);
static int cgeightclose(dev_t, int, int, cred_t *);
static int cgeightinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int cgeightioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int cgeightmmap(dev_t, off_t, int);
static u_int cgeightpoll(caddr_t);

static int x_g_attr(struct Tc1_Softc *, struct fbgattr *,
		int (*copyoutfun)(), int mode_flag);
static int x_getcmap(struct Tc1_Softc *, struct fbcmap *,
		int (*copyinfun)(), int (*copyoutfun)(), int mode_flag);
static int x_g_type(struct Tc1_Softc *, struct fbtype *,
		int (*copyoutfun)(), int mode_flag);
static int tc_putcmap(struct Tc1_Softc *, struct fbcmap *,
		int (*copyinfun)(), int mode_flag);
static int tcs_putcmap(SOFTC *, struct fbcmap *,
		int (*copyinfun)(), int mode_flag);
static int x_g_emulation_mode(SOFTC *, Pipio_Emulation *,
		int (*copyoutfun)(), int mode_flag);
static int x_g_fb_info(SOFTC *, Pipio_Fb_Info *,
		int (*copyoutfun)(), int mode_flag);
static int x_s_emulation_mode(SOFTC *, Pipio_Emulation *,
		int (*copyinfun)(), int mode_flag);
static int x_s_map_slot(SOFTC *, u_int *);
static int tcs_update_cmap(SOFTC *);

static struct vis_identifier cg8_ident = { "SUNWcg8" };

static struct cb_ops cg8_cb_ops = {
	cgeightopen,
	cgeightclose,
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	cgeightioctl,
	nodev,			/* devmap */
	cgeightmmap,
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,
	0,			/* streamtab  */
	D_NEW|D_MP		/* Driver compatibility flag */
};

static int cgeightidentify(dev_info_t *);
static int cgeightattach(dev_info_t *, ddi_attach_cmd_t);

static struct dev_ops cgeight_ops = {
	DEVO_REV,
	0,			/* refcnt  */
	cgeightinfo,
	cgeightidentify,
	nulldev,		/* probe */
	cgeightattach,
	nodev,			/* detach */
	nodev,			/* reset */
	&cg8_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* no bus operations */
};

/*
 *  Information kept on a per-unit basis for our device type.
 */
static void *tc1_state;	/* opaque handle where all the state hangs */

/*
 * Original SBus TC fb attributes.
 * see the definition of sun/sys/cg8reg.h//struct FB_Attribute_Flags
 * The combination of bits is very critical.
 * 9c000001 will bring up a monochrome version.
 * The "depth" property is assumed to be a coding of these bits.
 */
#define	TC_FB_ATTR 0x9C000001		/* monochrome */

/*
 * Handy macros:
 */
#define	SOFTC register struct Tc1_Softc

#define	getsoftc(unit)  \
	((struct Tc1_Softc *)ddi_get_soft_state(	\
		(void *)tc1_state, (int)(unit)))

#define	btob(n)		ptob(btopr(n))		   /* XXX */

#define	COPY(f, s, d, c, m)	\
		(f((caddr_t)(s), (caddr_t)(d), (u_int)(c), (m)))

#define	getprop(devi, name, def)			\
		ddi_getprop(DDI_DEV_T_ANY, (devi),	\
		DDI_PROP_DONTPASS, (name), (def))
#define	getproplen(devi, name, ptr)			\
		ddi_getproplen(DDI_DEV_T_ANY, (devi),	\
		DDI_PROP_DONTPASS, (name), (ptr))

/*
 * Forward  routine references:
 */
static int tcp_attach();
static int tcs_attach();
static int tcs_init_native(), tcs_init_ntsc(), tcs_init_pal();
static int pip_ioctl(), pip_off(), pip_on();
static int x_build_mmap_info();

/*  Default data structures for various ioctl requests. */

/*
 * Default for FBIOGATTR ioctl:
 */
static
struct fbgattr tc1_attrdefault = {
	FBTYPE_MEMCOLOR,	/* .  Actual type. */
	0,			/* .  Owner. */
	{		/* .  Frm buffer type(matchs tc1typedefault) */
		FBTYPE_MEMCOLOR,	/* .  .  Type. */
		0,			/* .  .  Height. */
		0,			/* .  .  Width. */
		32,			/* .  .  Depth. */
		256,			/* .  .  Color map size. */
		0			/* .  .  Size. */
	},			/* .  .  */
	{			/* .  Frame buffer attributes (flags): */
		FB_ATTR_AUTOINIT,	/* .  .  Flags. */
		FBTYPE_SUN2BW,		/* .  .  Emu type. */
		{ 0 }		/* .  .  Device specific info. */
	},			/* .  . */
	{			/* .  Emu types. */
		FBTYPE_MEMCOLOR,
		FBTYPE_SUN2BW,
		-1,
		-1
	}
};

/*
 * Default for FBIOGATTR ioctl in cg4 mode:
 */
static
struct fbgattr cg4_attrdefault = {
	FBTYPE_SUN4COLOR,	/* .  Actual type. */
	0,			/* .  Owner. */
	{		/* .  Frm buffer type(match tc1typedefault) */
		FBTYPE_SUN4COLOR,	/* .  .  Type. */
		0,			/* .  .  Height. */
		0,			/* .  .  Width. */
		8,			/* .  .  Depth. */
		256,			/* .  .  Color map size. */
		0			/* .  .  Size. */
	},			/* .  .  */
	{		/* .  Frame buffer attributes (flags): */
		FB_ATTR_AUTOINIT,	/* .  .  Flags. */
		FBTYPE_SUN2BW,		/* .  .  Emu type. */
		{ 0 }		/* .  .  Device specific information. */
	},			/* .  . */
	{			/* .  Emu types. */
		FBTYPE_SUN4COLOR,
		FBTYPE_SUN2BW,
		-1,
		-1
	}
};

/*
 * Default FBIOGTYPE ioctl for cg4, cg8:
 */
static
struct fbtype tc1typedefault = {
	FBTYPE_SUN2BW,		/* .  Type. */
	0,			/* .  Height. */
	0,			/* .  Width. */
	1,			/* .  Depth. */
	0,			/* .  Color map size. */
	0			/* .  Size. */
};



/*
 *  Table to translate Sun overlay index into a SBus TC overlay index.
 *
 *  Note that our overlay map works in a different manner from Sun's cg8, so a
 *  translation must be done (using translate_omap data structure):
 *   Function			   Sun LUT index   SBus TC LUT index
 *   --------			   -------------   ------------------
 *   True color				0		0
 *   Window system foreground		1		2
 *   Current window foreground		2		1
 *   Window system background		3		3
 *
 */
static
u_char translate_omap[] = /* xlat caller's omap index to match hardware */
	{ 0, 2, 1, 3};

/*
 * nice to know what is available in the driver ...
 * note these assume string concatination (ala ANSI C)
 */

#ifndef NO_PIP
#define	PIP_STR   " (with PIP)"
#else
#define	PIP_STR   /* nothing */
#endif

#ifdef TEST_NEW_HARDWARE
#define	TST_HW_STR  " (test new hardware)"
#else
#define	TST_HW_STR  /* nothing */
#endif


static struct modldrv modldrv = {
	&mod_driverops,		/* This is a device driver */
	"cgeight driver v1.14" PIP_STR TST_HW_STR,
	&cgeight_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};



int
_init(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "cg8: compiled %s, %s\n", __TIME__, __DATE__));

	e = ddi_soft_state_init(&tc1_state, sizeof (struct Tc1_Softc), 1);

	if (e == 0 && (e = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&tc1_state);
	}

	DEBUGF(1, (CE_CONT, "cg8: _init done, return(%d)\n", e));
	return (e);
}

int
_fini(void)
{
	register int e;

	DEBUGF(1, (CE_CONT, "cg8: _fini\n"));

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	ddi_soft_state_fini(&tc1_state);

	return (DDI_SUCCESS);
}

int
_info(struct modinfo *modinfop)
{
	DEBUGF(1, (CE_CONT, "cg8: _info\n"));

	return (mod_info(&modlinkage, modinfop));
}


/*
 * "cgeightattach"
 *
 *  Perform device specific initializations.
 *  (Called during autoconfiguration process).
 *  This routine allocates the per-unit data structure associated
 *  with the device and initializes it from the properties exported by the
 *  on-board PROM. In addition it maps the device's memory regions into
 *  virtual memory and performs hardware specific initializations.
 *
 *	  = DDI_SUCCESS if success
 *	  = DDI_FAILURE if failure
 *
 */
/* ARGSUSED */
static int
cgeightattach(devi, cmd)
	dev_info_t	*devi;	/* ->dev info struct w/node id... */
				/* (rest filled here) */
	ddi_attach_cmd_t cmd;
{
	FB_Attributes	fb_attr; /* Attr about fb from on-board PROM. */
	SOFTC	*softc;	/* Ptr to per-unit data for this device */
	int	unit = ddi_get_instance(devi);
	u_int	cgeightpoll(caddr_t intr_arg);
	char	name[16];

	DEBUGF(1, (CE_CONT, "cgeightattach%d, cmd 0x%x\n", unit, cmd));

	if (cmd != DDI_ATTACH)
	return (DDI_FAILURE);

	/*
	 * Allocate a tc1_softc unit structure for this unit
	 */
	if (ddi_soft_state_zalloc(tc1_state, unit) != 0) {
		return (DDI_FAILURE);
		/*NOTREACHED*/
	}

	softc = getsoftc(unit);
	softc->devi = devi;
	softc->instance = unit;
	softc->driver = CG8_DRIVER;
	ddi_set_driver_private(devi, (caddr_t)softc);

	/*
	 * If this instance is being used as the console,
	 * then the framebuffer may already be mapped in.
	 */
	if ((caddr_t)getprop(devi, "address", 0) != (caddr_t)0) {
		softc->mapped_by_prom = 1;
		DEBUGF(2, (CE_CONT,
		    "cgeightattach%d mapped by PROM\n", unit));
	}


	/*
	 *  SET UP PROPERTIES RELATED TO DEVICE REGISTER AREA AND MEMORIES
	 *
	 *  Get properties from the on-board PROM
	 *  (set up by Forth code at ipl time)
	 *  Fill in the switches related to them.
	 *  (Note we default enabled to possible here,
	 *  but enabled bits are related to the type of device
	 *  being emulated.)
	 */
	fb_attr.integer = (int)getprop(devi, "depth", TC_FB_ATTR);
	if (fb_attr.integer <= 1)
		fb_attr.integer = TC_FB_ATTR;
	DEBUGF(5, (CE_CONT,
		" PROM frame buffer attributes are %x\n",
			fb_attr.integer));
	softc->pg_possible[PIXPG_OVERLAY] = fb_attr.flags.monochrome;
	softc->pg_possible[PIXPG_OVERLAY_ENABLE] = fb_attr.flags.selection;
	softc->pg_possible[PIXPG_8BIT_COLOR] =
			fb_attr.flags.eight_bit_hardware;
	softc->pg_possible[PIXPG_24BIT_COLOR] = fb_attr.flags.true_color;
	softc->pg_possible[PIXPG_VIDEO_ENABLE] =
			fb_attr.flags.pip_possible;
	bcopy((caddr_t)softc->pg_possible,
	    (caddr_t)softc->pg_enabled, (u_int)FB_NPGS);

	softc->fb_model = fb_attr.flags.model;
	if (softc->fb_model == 0) {
		softc->fb_model = (softc->pg_possible[PIXPG_8BIT_COLOR]) ?
				RASTEROPS_TCP : RASTEROPS_TC;
	}
	softc->timing_regimen = NATIVE_TIMING;

	/*
	 *  Get display image dimensions,
	 *  and number of bytes between rows in a
	 *  1-bit pixel scan line. Use this information to generate the
	 *  inter-row spacing for 8-bit and 32-bit scan lines.
	 */
	softc->width =	getprop(devi, "width", 1152);
	softc->height = getprop(devi, "height", 900);
	softc->linebytes1 =
		getprop(devi, "linebytes", mpr_linebytes(1152, 8));
	softc->linebytes8 =  softc->linebytes1 * 8;
	softc->linebytes32 = softc->linebytes1 * 32;

	/*
	 *  PERFORM BOARD SPECIFIC PROCESSING
	 *  BASED ON THE TYPE OF BOARD SPECIFIED
	 *
	 *  The depth attribrute from the PROMs specifies the
	 *  SBus Card model being used.
	 *  Use this information to perform model specific
	 *  initializations. A return code of zero
	 *  indicates an error occurred,
	 *  in which case this routine should signal a failure.
	 */
	switch (softc->fb_model)
	{
	default:
	case RASTEROPS_TC:
	case RASTEROPS_TCP:
		if (tcp_attach(devi, softc) == DDI_FAILURE)
			return (DDI_FAILURE);
		break;
	case RASTEROPS_TCS:
		if (tcs_attach(devi, softc) == DDI_FAILURE)
			return (DDI_FAILURE);
		break;
	}

	/*
	 *  REGISTER OUR DEVICE
	 *
	 *  Save unit number in device information area we were passed.
	 *  Attach interrupt routine. Save back pointer to softc in device
	 *  information area. Increment the unit number for the
	 *  next attach, and report the device.
	 */
	(void) sprintf(name, "cgeight%d", unit);
	if (ddi_create_minor_node(devi, name, S_IFCHR,
	    unit, DDI_NT_DISPLAY, 0) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);	/* did return(-1) */
	}

	/*
	 * need to go thru the funcky sequence to get the cookies
	 */
	if (ddi_add_intr(devi, 0, &softc->cg8_iblock_cookie,
	    0, (u_int (*)(caddr_t))nulldev, NULL) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "cg8: cannot add interrupt handler");
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	mutex_init(&softc->softc_mutex, NULL, MUTEX_DRIVER,
	    softc->cg8_iblock_cookie);

	mutex_init(&softc->busy_mutex, NULL, MUTEX_DRIVER,
	    softc->cg8_iblock_cookie);

	ddi_remove_intr(devi, 0, softc->cg8_iblock_cookie);

	if (ddi_add_intr(devi, 0, &softc->cg8_iblock_cookie,
	    0, cgeightpoll, (caddr_t)softc) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "cg8: cannot add interrupt handler");
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	ddi_report_dev(devi);

	cmn_err(CE_CONT, "!%s%d: resolution %d x %d\n", ddi_get_name(devi),
		unit, softc->width, softc->height);

	return (DDI_SUCCESS);
	/*NOTREACHED*/
}


/*
 * "cgeightclose"
 *
 *  Close down operations on a SBus TC board
 *
 *	  = 0
 */
/*ARGSUSED*/
static int
cgeightclose(dev, flag, otyp, cred)
	dev_t dev;	/* = device spec for device to be closed. */
	int flag;	/* = read/write flags (unused in this routine. */
	int otyp;
	cred_t *cred;
{
	int	gpminor = getminor(dev);
	struct Tc1_Softc *softc = getsoftc(gpminor);

	DEBUGF(2, (CE_CONT, "cgeightclose(%d)\n", gpminor));

	if (otyp != OTYP_CHR)
		return (EINVAL);

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->instance != gpminor))
		return (ENXIO);

	/*
	 * mark this instance as unused (and therefore unloadable).
	 */
	mutex_enter(&softc->busy_mutex);
	softc->busy = 0;
	mutex_exit(&softc->busy_mutex);

	return (0);
}


/*
 * "cgeightidentify"
 *
 *  Check if a name matches that of our device, and if so update the
 *  count of devices we maintain.
 *
 *  = number of devices present
 *  = 0 if name passed does not match, or this device type is disabled
 */
static int
cgeightidentify(devi)
	dev_info_t *devi;
{
	char	*name = ddi_get_name(devi);
	int		e;

	DEBUGF(5, (CE_CONT, "cgeightidentify(%s)\n", name));

	e = DDI_NOT_IDENTIFIED;
	if ((strcmp(name, "cgeight") == 0) ||
	    (strcmp(name, "SUNW,cgeight") == 0))
		e = DDI_IDENTIFIED;

	return (e);
}

/*
 * "cgeightioctl"
 *
 *  Perform an input / output control operation on a SBus TC card
 *  or associated driver code.
 *
 * =  0 if successful operation
 * != 0 if error detected
 */
/*ARGSUSED*/
static int
cgeightioctl(
	dev_t		dev,	/* =  dev spec for dev to operate upon. */
	int		cmd,	/* =  opcode for operation to be performed. */
	intptr_t	arg,	/* -> data area for the oper. */
	int		flag,   /* =  modifier info. for op to be performed. */
	cred_t 		*cred,
	int		*rvalp)
{
	caddr_t data = (caddr_t)arg;
	register int	instance = getminor(dev);
	SOFTC   *softc = getsoftc(instance);  /* Ptr to per-unit info. */

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->instance != instance))
		return (ENXIO);

#ifdef TC_DEBUG
	DEBUGF(2,  (CE_CONT, "cgeightioctl: cmd = "));

	switch (cmd) {

	case VIS_GETIDENTIFIER:
		DEBUGF(2, (CE_CONT, "VIS_GET_DENTIFIER\n")); break;
	case FBIOSATTR:
		DEBUGF(2, (CE_CONT, "FBIOSATTR\n")); break;
	case FBIOGATTR:
		DEBUGF(2, (CE_CONT, "FBIOGATTR\n")); break;
	case FBIOGETCMAP:
		DEBUGF(2, (CE_CONT, "FBIOGETCMAP\n")); break;
	case FBIOPUTCMAP:
		DEBUGF(2, (CE_CONT, "FBIOPUTCMAP\n")); break;
	case FBIOGTYPE:
		DEBUGF(2, (CE_CONT, "FBIOGTYPE\n")); break;
	case FBIOSVIDEO:
		DEBUGF(2, (CE_CONT, "FBIOSVIDEO\n")); break;
	case FBIOGVIDEO:
		DEBUGF(2, (CE_CONT, "FBIOGVIDEO\n")); break;
	case PIPIO_G_CURSOR_COLOR_FREEZE:
		DEBUGF(2, (CE_CONT,
			"PIPIO_G_CURSOR_COLOR_FREEZE\n")); break;
	case PIPIO_G_EMULATION_MODE:
		DEBUGF(2, (CE_CONT, "PIPIO_G_EMULATION_MODE\n")); break;
	case PIPIO_G_FB_INFO:
		DEBUGF(2, (CE_CONT, "PIPIO_G_FB_INFO\n")); break;
	case PIPIO_S_CURSOR_COLOR_FREEZE:
		DEBUGF(2, (CE_CONT,
			"PIPIO_S_CURSOR_COLOR_FREEZE\n")); break;
	case PIPIO_S_EMULATION_MODE:
		DEBUGF(2, (CE_CONT, "PIPIO_S_EMULATION_MODE\n")); break;
	default:
		DEBUGF(2, (CE_CONT,
			"not supported (0x%x)\n", cmd)); break;
	}
#endif TC_DEBUG

	/*
	 *  Process request based on its type. Cases are alphabetical.
	 */
	switch (cmd) {

	case VIS_GETIDENTIFIER:
		DEBUGF(2, (CE_CONT, "VIS_GETIDENTIFIER\n"));

		if (COPY(ddi_copyout, &cg8_ident, data,
		    sizeof (struct vis_identifier), flag) != 0)
			return (EFAULT);
		break;

	case FBIOGATTR:
		DEBUGF(2, (CE_CONT, "FBIOGATTR\n"));
		return (x_g_attr(softc,
			    (struct  fbgattr *)data, ddi_copyout, flag));
	case FBIOGETCMAP:
		DEBUGF(2, (CE_CONT, "FBIOGETCMAP\n"));
		return (x_getcmap(softc, (struct fbcmap  *)data,
			ddi_copyin, ddi_copyout, flag));
	case FBIOGTYPE:
		DEBUGF(2, (CE_CONT, "FBIOGTYPE\n"));
		return (x_g_type(softc,
			    (struct fbtype  *)data, ddi_copyout, flag));
	case FBIOGVIDEO:
	{
		auto int video = -1;

		switch (softc->fb_model) {

		default:
			video = mfb_get_video(
			    & softc->device_reg->sun_mfb_reg) ?
				FBVIDEO_ON : FBVIDEO_OFF;
			break;
		case RASTEROPS_TCS:
			break;
		}
		if (video != -1)
			if (COPY(ddi_copyout, &video, data,
			    sizeof (int), flag) != 0)
				return (EFAULT);
	}
		break;
	case FBIOPUTCMAP:
	{
		auto int ret;

		switch (softc->fb_model) {

		default:
			ret = tc_putcmap(softc,
			    (struct fbcmap *)data, ddi_copyin, flag);
			break;
		case RASTEROPS_TCS:
			ret = tcs_putcmap(softc,
			    (struct fbcmap *)data, ddi_copyin, flag);
			break;
		}
		return (ret);
	}
	case FBIOSVIDEO:
	{
		auto int on;

		if (COPY(ddi_copyin, data, &on, sizeof (int), flag) != 0)
			return (EFAULT);

		mutex_enter(&softc->softc_mutex);
		switch (softc->fb_model) {

		default:
			mfb_set_video(&softc->device_reg->sun_mfb_reg,
			on & FBVIDEO_ON);
			break;
		case RASTEROPS_TCS:
			break;
		}
		mutex_exit(&softc->softc_mutex);
	}
		break;

	/*
	 *  Ioctls placed under the guise of the PIP which should
	 *  always be handled.
	 */
	case PIPIO_G_CURSOR_COLOR_FREEZE:
	{
		auto int freeze_flag;

		freeze_flag = softc->sw_cursor_color_frozen;
		if (COPY(ddi_copyout, &freeze_flag,
		    data, sizeof (int), flag) != 0)
			return (EFAULT);
	}
		break;
	case PIPIO_G_EMULATION_MODE:
		return (x_g_emulation_mode(softc,
			(Pipio_Emulation *)data, ddi_copyout, flag));
	case PIPIO_G_FB_INFO:
		return (x_g_fb_info(softc,
			(Pipio_Fb_Info *)data, ddi_copyout, flag));
	case PIPIO_S_CURSOR_COLOR_FREEZE:
	{
		auto int freeze_flag;

		if (COPY(ddi_copyin, data, &freeze_flag,
		    sizeof (int), flag) != 0)
			return (EFAULT);
		mutex_enter(&softc->softc_mutex);
		softc->sw_cursor_color_frozen = (u_char)flag;
		mutex_exit(&softc->softc_mutex);
	}
		break;
	case PIPIO_S_EMULATION_MODE:
		return (x_s_emulation_mode(softc, (Pipio_Emulation *)data,
			ddi_copyin, flag));
#ifdef TEST_NEW_HARDWARE
	case PIPIO_S_MAP_SLOT:
		return (x_s_map_slot(softc, (u_int *)data));
#endif TEST_NEW_HARDWARE

	/*
	 * - Unknown request, see if the pip knows about it...
	 */
	default:
#ifndef NO_PIP
		if ((pip_ioctl(softc, cmd, data, flag,
		    ddi_copyin, ddi_copyout) != 0)) {
			DEBUGF(5, (CE_CONT,
			    "not supported (0x%x)\n", cmd));
			return (EINVAL);
		}
#else NO_PIP
		return (EINVAL);
#endif !NO_PIP
	} /* switch(cmd) */
	return (0);
}

/*
 * "cgeightmmap"
 *
 *  Provide physical page address associated with a memory map offset. This
 *  routine determines which of the six discontiguous physical memory
 *  regions the offset passed to it is associated with. Based on that
 *  determination a physical page address is calculated and returned.
 *
 *	= PTE subset for the page in question
 *	= -1 if off is not within the memory space
 *		for one of the frame buffers
 *
 *  Notes:
 *  (1) The value given by "off" is relative to the device register area
 *	(since its address was given in the map_regs call to allocate
 *	virtual memory for the mapping.)
 *  (2) See the description "Physical to virtual memory mapping for devices
 *	on the tc1" near the end of this file for further details of
 *	the mapping process.
 *  (3) softc->basepage and ->test_basepage have already been
 *	converted via hat_getkpfnum().
 *
 * Not sure if the expr is correct:
 *	hat_getkpfnum(regs) + ddi_btop(..,offset)
 * The other way would be to not convert the basepage before getting here,
 * and do all of the math here.
 * but ... way I (bandit) found it; it;s DDI/DKI compliant (I think),
 * so ... leave it this way until it's broken
 */
/*ARGSUSED*/
static int
cgeightmmap(dev, off, prot)
	dev_t	dev;  /* = device which is to be memory mapped. */
	register off_t  off;	/* = offset into mem map for page */
				/* to be mapped. */
	int	 prot; /* (unused) */
{
	int		mmi;	/* Index: # of mmap_info entry exam. */
	Mmap_Info	*mmp;	/* Cursor: mmap_info entry examining. */
	off_t	phys_off;	/* Phys offset from base page of page. */
	int	instance = getminor(dev);
	SOFTC	*softc =	/* Pointer to per-unit info. */
			getsoftc(instance);

	DEBUGF(off ? 9 : 1,
	    (CE_CONT, "cgeightmmap(%d, 0x%x)\n", instance, (u_int) off));

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->instance != instance))
		return (-1);

#ifdef TEST_NEW_HARDWARE
	if (off >= 0x00900000) {
		if (softc->test_basepage == 0)
			return (-1);
		phys_off = (off-0x900000);
		DEBUGF(2, (CE_CONT,
		    "Returning offset %lx as phys_off %lx and address %lx\n",
		    off, phys_off,
		    softc->test_basepage+ddi_btop(softc->devi, phys_off)));
		return (softc->test_basepage
			    + ddi_btop(softc->devi, phys_off));
	}
#endif TEST_NEW_HARDWARE

	/*
	 * It's not us, it's an interesting approach, but its not us!
	 */
	if ((off < 0) || (off > softc->memory_map_end)) {
		return (-1);
	}

	/*
	 *  From "off" determine which physical memory is being referenced.
	 *  Recalculate "off" as a physical offset relative to base page
	 *  (that was mapped with map_regs).
	 */
	if (off >= softc->dev_reg_mmap_offset) {	/* DEVICE MAP */
		phys_off = (off-softc->dev_reg_mmap_offset) +
		    softc->dev_reg_sbus_delta;
		return (softc->basepage + ddi_btop(softc->devi, phys_off));
	}

	for (mmi = 0, mmp = softc->mmap_info;
	    mmi < softc->mmap_count; mmi++, mmp++) {
		if (off >= mmp->bgn_offset && off < mmp->end_offset) {
			phys_off = (off - mmp->bgn_offset) +
					    mmp->sbus_delta;
#if TC_DEBUG
			DEBUGF((mmp->group == PIXPG_OVERLAY &&
				off == mmp->bgn_offset) ? 1 : 9,
				(CE_CONT,
				"cgeightmmap returning 0x%lx (0x%lx)\n",
				ddi_ptob(softc->devi,
				    softc->basepage)+phys_off,
				softc->basepage
					+ddi_btop(softc->devi, phys_off)));
			if (off == mmp->bgn_offset) {
				switch (mmp->group) {

				case PIXPG_8BIT_COLOR:
	DEBUGF(2, (CE_CONT, "cgeightmmap returning eight bit color"));
					break;
				case PIXPG_OVERLAY:
	DEBUGF(2, (CE_CONT, "cgeightmmap returning monochrome"));
					break;
				case PIXPG_OVERLAY_ENABLE:
	DEBUGF(2, (CE_CONT, "cgeightmmap returning selection"));
					break;
				case PIXPG_24BIT_COLOR:
	DEBUGF(2, (CE_CONT, "cgeightmmap returning true color"));
					break;
				case PIXPG_VIDEO_ENABLE:
	DEBUGF(2, (CE_CONT, "cgeightmmap returning video enable"));
					break;
				}
				DEBUGF(2, (CE_CONT,
					" start at 0x%lx (0x%lx)\n",
					ddi_ptob(softc->devi,
					softc->basepage)+phys_off,
					softc->basepage
					+ddi_btop(softc->devi, phys_off)));
			}
#endif TC_DEBUG
			return (softc->basepage
			    + ddi_btop(softc->devi, phys_off));
		}
	}

	return (-1);
}

/*
 * "cgeightopen"
 *
 *  Open a SBus TC board "device" for use.
 *
 *  Parameters:
 *	  dev   = device to be opened
 *	  flag  = flags indicating access method (e.g., read, write)
 *
 *  Function value:
 *	  = return code from frame buffer driver open routine
 *
 *  XXX - other drivers check for expected values of flag, otyp
 */
/*ARGSUSED*/
static int
cgeightopen(devp, flag, otyp, cred)
	dev_t *devp;	/* =  device spec giving what device */
			/* instance to open */
	int   flag;	/* =  flags indicat access method */
			/* (e.g., read, write). */
	int   otyp;
	cred_t *cred;
{
	int			instance = getminor(*devp);
	struct Tc1_Softc	*softc = getsoftc(instance);
	int error = 0;

	DEBUGF(2, (CE_CONT,
	    "cgeightopen(%d,%d)\n", getmajor(*devp), instance));

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->instance != instance))
		error = DDI_FAILURE;

	if (!error) {
		/*
		 * Guard against our own close routine.
		 */
		mutex_enter(&softc->busy_mutex);
		softc->busy = 1;
		mutex_exit(&softc->busy_mutex);
	}

	return (error);
}

/*
 * "cgeightpoll"
 *
 *  Interrupt service routine for vertical retrace interrupts
 *  on this device type. Loop through each device instance
 *  looking for one with an interrupt pending.
 *
 *	  = number of interrupts serviced
 */
u_int
cgeightpoll(caddr_t intr_arg)
{
	/* Ptr to tcs dev reg area. */
	volatile Tcs_Device_Map  *device_tcs;
	u_int	serviced;	/* Number of interrupts serviced. */
	SOFTC	*softc;	/* Ptr to characteristics for our unit. */

	softc = (struct Tc1_Softc *)intr_arg;
	serviced = DDI_INTR_UNCLAIMED;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (DDI_INTR_UNCLAIMED);

	mutex_enter(&softc->softc_mutex);
	switch (softc->fb_model) {

	default:
		if (mfb_int_pending((struct mfb_reg *)softc->device_reg)) {
			mfb_int_disable(
				((struct mfb_reg *)softc->device_reg));
			serviced = DDI_INTR_CLAIMED;
		}
		break;
	case RASTEROPS_TCP:
	case RASTEROPS_TC:
		if (mfb_int_pending((struct mfb_reg *)softc->device_reg)) {
			mfb_int_disable(
				((struct mfb_reg *)softc->device_reg));
			serviced = DDI_INTR_CLAIMED;
		}
		break;

	case RASTEROPS_TCS:
		device_tcs = softc->device_tcs;
		if (device_tcs->tc_venus.status & VENUS_VERT_INT) {
			(void) tcs_update_cmap(softc);
			device_tcs->tc_venus.control4 = 0;
			device_tcs->tc_venus.status = 0;
			serviced = DDI_INTR_CLAIMED;
		}
		break;
	}
	mutex_exit(&softc->softc_mutex);

	return (serviced);
}

/* ARGSUSED */
static int
cgeightinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
		    void *arg, void **result)
{
	register dev_t dev = (dev_t)arg;
	register int instance, error;
	SOFTC *softc;

	instance = getminor(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((softc = getsoftc(instance)) == NULL) {
			error = DDI_FAILURE;
		} else {
			if ((softc == NULL)			||
			    (softc->driver != CG8_DRIVER)	||
			    (softc->instance != instance))
				return (DDI_FAILURE);
			*result = (void *) softc->devi;
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

#ifndef NO_PIP

/*
 *  Routines to support Picture In A Picture Option card.
 *
 *
 *  Notes:
 * (1) Control status register zero has one bit which is shared, that is,
 *    it has different meanings on input and output. If the bit is written
 *    it directs that a one-shot grab should be done. If the bit is read
 *    it indicates if the pip has been turned on. Because of this sharing
 *    it is not possible to use read modify write cycles in "pip_off".
 *
 * (2) The TCP_CSR0_TURN_PIP_ON bit is used to set the pip on or off.
 *    Its value when read
 *    (TCP_CSR0_PIP_IS_ACTIVE) indicates if the pip is
 *    actively writing data, NOT necessarily if the pip is on. The
 *    TCP_CSR0_PIP_IS_ON bit indicates the the pip has been turned on, or
 *    is running. The cycle looks like:
 *
 *           PIP_ON set  Field start             PIP_OFF set   Field end
 *                   |      |                            |        |
 *                   |      V                            V        V
 *                   V      ____________...________________________
 * PIP_IS_ACTIVE   ________/                                       \_____
 *                    __________________...________________________
 * PIP_IS_ON       __/                                             \_____
 *
 * (3) In addition, TCP_CSR0_PIP_IS_ACTIVE will drop if the input source is
 *    not present, even though it is "ON". So TCP_CSR0_PIP_IS_ON should be
 *    examined to determine the state of the pip, not
 *    TCP_CSR0_PIP_IS_ACTIVE.
 *
 * (4) When turning off the pip it will continue until the end of the field
 *    as shown above. So TCP_CSR0_PIP_IS_ACTIVE may be polled to see if
 *    the pip is off.
 *
 *   2
 *  I C Bus Programming Considerations And Details:
 *
 * (1) The i2c bus is implemented on the PIP as two contiguous bits in a
 *    single register. Thus read-modify writes are used to set and clear
 *    the bits. This can cause a problem during acknowledgement cycles
 *    since the receiver of data
 *    (one of the devices on the i2c bus) will
 *    hold the data line low during the acknowledgement cycle, which is
 *    ended with the clock being set high by us. However, since the clock
 *    is set high with a read-modify-write we pick up a low setting for
 *    the data bit and store it back. To get around this we explicitly set
 *    the data bit back to high after, or while, setting the clock. This
 *    actions are flagged with comments in the code below.
 *
 * (2) The standard cycle for sending the first data byte is as follows:
 *
 *           Start
 *             |
 *             |
 *             |
 *             V      1     2     3     4     5     6     7     8    ack
 * Clock:    ____    __    __    __    __    __    __    __    __    __
 *               \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__
 *
 * Sender:   __     _____ _____ _____ _____ _____ _____ _____ ____________
 * Data        \___/_____X_____X_____X_____X_____X_____X_____X_____/
 *
 * Reciever: ____________________________________________________       __
 * Data                                                          \_____/
 *
 * (3) The standard cycle for sending subsequent bytes is:
 *
 *
 *                      1     2     3     4     5     6     7     8    ack
 * Clock:          __    __    __    __    __    __    __    __    __
 *            ____/  \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__
 *
 * Sender:    _________ _____ _____ _____ _____ _____ _____ ___________
 * Data          \_____X_____X_____X_____X_____X_____X_____X_____/
 *
 * Reciever:  __________________________________________________      ___
 * Data                                                         \____/
 *
 * (4) The standard cycle for stop at the end of data transmission is:
 *
 *
 * Clock:          ______
 *            ____/
 *
 * Sender:    ___    ____
 * Data          \__/
 *
 * (5) The standard cycle for reading a byte is:
 *
 *
 *                 1     2     3     4     5     6     7     8    stop
 * Clock:          __    __    __    __    __    __    __    __    _____
 *            ____/  \__/  \__/  \__/  \__/  \__/  \__/  \__/  \__/
 *
 * Sender:    __________________________________________________    ____
 * Data                                                         \__/
 *
 * Receiver:  _________ _____ _____ _____ _____ _____ _____ ____
 * Data          \_____X_____X_____X_____X_____X_____X_____X____\____
 *
 */


/*
 * "pip_ioctl"
 *
 *  Perform a pip-related input / output control operation on a
 *  SBus TC Card or its associated driver code
 *
 *	=  0 if successful operation
 *	~= 0 if error detected
 */
/*ARGSUSED*/
static int
pip_ioctl(softc, cmd, data, flag, copyinfun, copyoutfun)
	SOFTC	*softc; /* -> instance dependent storage. */
	int	cmd;	/* =  opcode for operation to be performed. */
	caddr_t	data;   /* -> data area for the op (may be in or out). */
	int	flag;   /* =  modifier info. for op to be performed. */
	int	(*copyinfun)(), (*copyoutfun)();
{
	int		   on;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

#ifdef TC_DEBUG
	DEBUGF(2,  (CE_CONT, "pip_ioctl: cmd = "));
	switch (cmd) {

	case PIPIO_G_EMULATION_MODE:
		DEBUGF(2, (CE_CONT, "PIPIO_G_EMULATION_MODE\n")); break;
	case PIPIO_G_PIP_ON_OFF:
		DEBUGF(2, (CE_CONT, "PIPIO_G_PIP_ON_OFF\n")); break;
	case PIPIO_G_PIP_ON_OFF_SUSPEND:
		DEBUGF(2, (CE_CONT, "PIP_G_PIP_ON_OFF_SUSPEND\n")); break;
	case PIPIO_G_PIP_ON_OFF_RESUME:
		DEBUGF(2, (CE_CONT, "PIP_G_PIP_ON_OFF_RESUME\n")); break;

	case PIPIO_S_EMULATION_MODE:
		DEBUGF(2, (CE_CONT, "PIPIO_S_EMULATION_MODE\n")); break;
	case PIPIO_S_PIP_ON_OFF:
		DEBUGF(2, (CE_CONT, "PIP_S_PIP_ON_OFF\n")); break;
	default:
		DEBUGF(2, (CE_CONT, "not supported (0x%x)\n", cmd)); break;
	}
#endif TC_DEBUG

	/*
	 *  Make sure pip operations are possible, then process request
	 *  based on its type.  Cases are alphabetical.
	 */
	if (softc->pg_possible[PIXPG_VIDEO_ENABLE] == 0) {
		return (EINVAL);
	}

	switch (cmd) {

	case PIPIO_G_PIP_ON_OFF:
		on = (softc->device_reg->control_status &
			TCP_CSR0_PIP_IS_ON) ? 1 : 0;
		if (COPY(copyoutfun, &on, data, sizeof (int), flag) != 0)
			return (EFAULT);
		break;
	case PIPIO_G_PIP_ON_OFF_RESUME:
		mutex_enter(&softc->softc_mutex);
		softc->pip_on_off_semaphore--;
		if (softc->pip_on_off_semaphore < 0) {
			/* In case too many resumes */
			softc->pip_on_off_semaphore = -1;
			(void) pip_on(softc->device_reg);
			on = 1;
		} else {
			on = 0;
		}
		mutex_exit(&softc->softc_mutex);
		if (COPY(copyoutfun, &on, data, sizeof (int), flag) != 0)
			return (EFAULT);
		break;
	case PIPIO_G_PIP_ON_OFF_SUSPEND:
		mutex_enter(&softc->softc_mutex);
		on = pip_off(softc, 0);
		softc->pip_on_off_semaphore++;
		mutex_exit(&softc->softc_mutex);
		if (COPY(copyoutfun, &on, data, sizeof (int), flag) != 0)
			return (EFAULT);
		break;

	case PIPIO_S_PIP_ON_OFF:
		if (COPY(copyinfun, data, &on, sizeof (int), flag) != 0)
			return (EFAULT);
		mutex_enter(&softc->softc_mutex);
		if (on) {
			(void) pip_on(softc->device_reg);
			softc->pip_on_off_semaphore = -1;
		} else {
			(void) pip_off(softc, 0);
			softc->pip_on_off_semaphore = 0;
		}
		mutex_exit(&softc->softc_mutex);
		break;

	/* - Unknown request, return fact we could not process it. */

	default:
		return (ENOTTY);

	}					  /* switch(cmd) */
	return (0);
}

/*
 * "pip_off"
 *
 *  Turn off live video generation,
 *  returning the previous state of the bit.
 *
 *
 *  Notes:
 *  (1) See notes 2 and 3 at the beginning of this file for a detailed
 *	  explanation of all the shenanigans with bits in control status
 *	  register 0 that occur here.
 *
 *	  = 1 if pip was on
 *	  = 0 if pip was off
 */
static int
pip_off(softc, wait_for_inactive)
	SOFTC	*softc;	/* -> device instance dependent data. */
	int	wait_for_inactive;
		/* =1 if wait for pip to actually turn off */
{
	int	i;	/* Index: # of times gone around wait loop. */
	int	pip_was_on;	/* Indicates if pip was active. */
	/* -> device area on TCPIP card. */
	volatile Tc1_Device_Map  *dmap;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (0);

	dmap = softc->device_reg;
	pip_was_on = dmap->control_status & TCP_CSR0_PIP_IS_ON;
	dmap->control_status &=
	    (u_char)~(TCP_CSR0_TURN_PIP_ON|TCP_CSR0_PIP_ONE_SHOT);
	if (wait_for_inactive &&
	    (dmap->control_status & TCP_CSR0_PIP_IS_ACTIVE)) {
		cmn_err(CE_NOTE, "Waiting for inactive\n");
		for (i = 0; i < 10000000; i++) {
			if (!(dmap->control_status &
			    TCP_CSR0_PIP_IS_ACTIVE))
				break;
		}
		if (i >= 10000000) {
			cmn_err(CE_WARN,
			    " Pip did not turn off in %d loops!!!!\n", i);
		}
	}
	return ((pip_was_on) ? 1 : 0);
}

/*
 * "pip_on"
 *
 *  Turn on live video generation. (Don't do it unless there
 *  is an active data source connected.
 */
static int
pip_on(dmap)
	volatile Tc1_Device_Map  *dmap;  /* ->device area on TCPIP card. */
{
	if (dmap->control_status1 & TCP_CSR1_INPUT_CONNECTED) {
		dmap->control_status =
		    ((u_int)dmap->control_status&~TCP_CSR0_PIP_ONE_SHOT) |
			TCP_CSR0_TURN_PIP_ON;
	}
	return (0);
}

#endif !NO_PIP


/*
 * "tc_putcmap"
 *
 *  Local routine to set the color map values for either the 24-bit or the
 *  monochrome frame buffer on a TC or TCP card. In addition to real lookup
 *  table manipulation, emulation of 8-bit indexed color maps is done for
 *  24-bit mode. (Called by tcone_ioctl).
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
tc_putcmap(softc, cmap_p, copyinfun, mode)
	SOFTC	*softc; /* -> characteristics for our unit. */
	struct fbcmap *cmap_p;	/* -> where to get colormap */
				/* (data area from ioctl) */
	int (*copyinfun)();
	int mode;
{
	u_int	count;		/* # of color map entries to set. */
	volatile Tc1_Device_Map *device_reg;	/* ptr to Brooktrees. */
	int	force_update;	/* set, should access real luts */
	u_int	index;		/* Index of first entry to access. */
	int	n_entry;	/* Loop index: current color map entry. */
	int	plane_group;	/* Top 7 bits of index = plane group. */
	struct fbcmap  cmap;	/* copy of colormap data area from ioctl */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

	if (COPY(copyinfun, cmap_p, &cmap,
	    sizeof (struct fbcmap), mode) != 0)
		return (EFAULT);

	device_reg = softc->device_reg;

	/*
	 *  Verify we need to actually move some data.Initialize variables
	 *  from caller's parameters, etc. If the plane group is zero, use
	 *  the 24 bit group.
	 */
	if ((cmap.count == 0) || !cmap.red || !cmap.green || !cmap.blue) {
		return (EINVAL);
	}
	count = cmap.count;
	index = cmap.index & PIX_ALL_PLANES;
	if (index+count > TC_CMAP_SIZE) {
		return (EINVAL);
	}

	/*
	 * This is a hold-over from the sunview/pixrect stuff
	 * (look in the <sys/pixrect.h> for the comment
	 * about PR_FORCE_UPDATE)
	 * problem is, we are explicitly not including <sys/pixrect.h>
	 * so the symbol is never defined
	 *
	 * XXX going to always force
	 */
#ifdef notdef
	if (cmap.index & PR_FORCE_UPDATE) {
		return (EINVAL);
	}
#endif /* notdef */

	/* assume always forcing better than not forcing */
	force_update = 1;


	mutex_enter(&softc->softc_mutex);
	plane_group = PIX_ATTRGROUP(cmap.index);

	if (!plane_group) {
		plane_group = PIXPG_24BIT_COLOR;
	}

	/*	Move caller's data */
	DEBUGF(6, (CE_CONT, "putcmap, index=%x, count=%d, pg=%d\n",
	index, count, plane_group));
	if (COPY(copyinfun, cmap.red, softc->cmap_red, count, mode) ||
	    COPY(copyinfun, cmap.green, softc->cmap_green, count, mode) ||
	    COPY(copyinfun, cmap.blue, softc->cmap_blue, count, mode)) {
		mutex_exit(&softc->softc_mutex);
		return (EFAULT);
	}

	/*
	 *  If there is no eight bit buffer
	 *  and hardware update is not forced,
	 *  set 8-bit index emulation color table entries
	 *  for 24-bit buffer.
	 *  (The check for the 8-bit plane group is here in case the kernel
	 *   knows 8-bit h'ware exists,
	 *   but the driver didn't export it to Sunview.)
	 */
	if (!force_update && (softc->pg_enabled[PIXPG_8BIT_COLOR] == 0) &&
	    ((plane_group == PIXPG_24BIT_COLOR) ||
		(plane_group == PIXPG_8BIT_COLOR))) {
		for (n_entry = 0; n_entry < count; n_entry++, index++) {
			softc->em_red[index] = softc->cmap_red[n_entry];
			softc->em_green[index] =
			    softc->cmap_green[n_entry];
			softc->em_blue[index] = softc->cmap_blue[n_entry];
		}
	}

	/*
	 *  Set entries in actual Brooktree lookup tables
	 *  for 8/24-bit buffer.
	 */
	else if (plane_group == PIXPG_24BIT_COLOR ||
	    plane_group == PIXPG_8BIT_COLOR) {
		DEBUGF(6, (CE_CONT, "Real pcmap: group=%d ", plane_group));
		DEBUGF(6, (CE_CONT, " first = %d", index));
		DEBUGF(6, (CE_CONT, " count = %d\n", count));

		device_reg->dacs.bt457.all_address = index;
		for (n_entry = 0; n_entry < count; n_entry++) {
			device_reg->dacs.bt457.red_color =
				softc->cmap_red[n_entry];
			device_reg->dacs.bt457.green_color =
				softc->cmap_green[n_entry];
			device_reg->dacs.bt457.blue_color =
				softc->cmap_blue[n_entry];
		}
	}

	else if (plane_group != PIXPG_OVERLAY) {
		cmn_err(CE_WARN,
		    "Illegal plane group %d encountered\n", plane_group);
		mutex_exit(&softc->softc_mutex);
		return (EINVAL);
	}

	/*
	 *  Set entries in actual Brooktree lookup tables
	 *  for monochrome overlay.
	 *  At the same time write the shadow color map
	 *  which fbiogetcmap gets values from.
	 *  Note that our overlay map works in a different manner
	 *  from Sun's cg8, so a translation must be done
	 *  (using translate_omap data structure):
	 *   Function			Sun LUT index   SBus TC LUT index
	 *   --------			-------------   ------------------
	 *   True color				0		0
	 *   Window system foreground		1		2
	 *   Current window foreground		2		1
	 *   Window system background		3		3
	 *
	 *  Note we only write the overlay if 24-bit enabled to get around
	 *  a problem with cg4 emulation. Also we implement cursor color
	 *  freezing here for true color!
	 */
	else if (softc->pg_enabled[PIXPG_24BIT_COLOR]) {
		DEBUGF(6, (CE_CONT, "Overlay pcmap:"));
		DEBUGF(6, (CE_CONT, " first = %d", index));
		DEBUGF(6, (CE_CONT, " count = %d\n", count));

		for (n_entry = 0; n_entry < count; n_entry++, index++) {
			if (!softc->sw_cursor_color_frozen || index != 2) {
				device_reg->dacs.bt457.all_address =
					translate_omap[index];
				device_reg->dacs.bt457.red_overlay =
				softc->omap_red[index] =
					softc->cmap_red[n_entry];
				device_reg->dacs.bt457.green_overlay =
				softc->omap_green[index] =
					softc->cmap_green[n_entry];
				device_reg->dacs.bt457.blue_overlay =
				softc->omap_blue[index] =
					softc->cmap_blue[n_entry];
			}
		}
	}

	mutex_exit(&softc->softc_mutex);
	return (0);
}


/*
 * "tcp_attach"
 *
 *  Perform device specific initializations for TC or TCP frame buffer.
 *  (Called from cgeightattach().) This routine initializes parts of the
 *  per-unit data structure.
 *  In addition it maps the device's memory regions
 *  into virtual memory and performs hardware specific initializations.
 *  A number of per-unit data items have already been initialized
 *  from PROMs in cgeightattach().
 *
 *	  = DDI_SUCCESS if success
 *	  = DDI_FAILURE if failure
 */
static int
tcp_attach(devi, softc)
	dev_info_t *devi;	/* ->dev info struct w/node id. */
				/* (rest filled here) */
	SOFTC	   *softc; /* ->per-unit data struct to be filled in. */
{
	int	color;	/* Index: next color to set in lut ramp. */
	volatile Tc1_Device_Map  *dmap;  /* Fast ptr to dev reg area. */
		/* Virt. addr of device map(&optionally fbs) */
	caddr_t	reg;

	/*
	 *  PROCESS REMAINING PROM PARAMETERS:
	 *
	 *  Pick up the device specific params exported by the
	 *  on-board PROMs.
	 *  Use this information to set up memory mapping characteristics
	 *  of the board.
	 */
	DEBUGF(5, (CE_CONT, "tcp_attach softc at %p\n", (void *)softc));

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->devi != devi))
		return (DDI_FAILURE);

	/*
	 *  Device register area parameters:
	 */
	softc->dev_reg_sbus_base =
		getprop(devi, "device-map-offset", 0x400000);
	softc->dev_reg_mmap_offset = CG8_VADDR_DAC;
	softc->dev_reg_sbus_delta = 0;
	softc->dev_reg_mmap_size = ddi_ptob(devi, ddi_btopr(devi, NBPG));
	softc->memory_map_end =
	softc->dev_reg_mmap_offset + softc->dev_reg_mmap_size;

	DEBUGF(5, (CE_CONT,
		" dev_reg_sbus_base is %x\n", softc->dev_reg_sbus_base));

	/*
	 *  Monochrome frame buffer parameters:
	 */
	softc->fb_mono_sbus_delta =
		getprop(devi, "monochrome-offset", 0x00C00000);
	softc->fb_mono_sbus_delta -= softc->dev_reg_sbus_base;
	softc->fb_mono_mmap_size =
		getprop(devi, "monochrome-size", 1152*900/8);
	softc->fb_mono_mmap_size =
		ddi_ptob(devi, ddi_btopr(devi, softc->fb_mono_mmap_size));

	DEBUGF(5, (CE_CONT,
		"monochrome   sbus delta %x mmap offset	 %x size  %x\n",
		softc->fb_mono_sbus_delta, 0, softc->fb_mono_mmap_size));

	/*
	 *  Selection memory parameters:
	 */
	softc->fb_sel_sbus_delta =
		getprop(devi, "selection-offset", 0x00D00000);
	softc->fb_sel_sbus_delta -= softc->dev_reg_sbus_base;
	softc->fb_sel_mmap_size =
		getprop(devi, "selection-size", 1152*900/8);
	softc->fb_sel_mmap_size =
		ddi_ptob(devi, ddi_btopr(devi, softc->fb_sel_mmap_size));

	DEBUGF(5, (CE_CONT,
		"selection	sbus delta %x mmap offset  %x size  %x\n",
		softc->fb_sel_sbus_delta, softc->fb_mono_mmap_size,
		softc->fb_sel_mmap_size));

	/*
	 *  Eight bit frame buffer parameters
	 * (set them up even if don't use them):
	 */
	softc->fb_8bit_sbus_delta = 0x00700000 - softc->dev_reg_sbus_base;
	softc->fb_8bit_mmap_size =
		ddi_ptob(devi, ddi_btopr(devi, 1152*900));

	DEBUGF(5, (CE_CONT,
		"eight_bit	sbus delta %x mmap offset  %x size  %x\n",
		softc->fb_8bit_sbus_delta,
		softc->fb_mono_mmap_size+softc->fb_sel_mmap_size,
		softc->fb_8bit_mmap_size));

	/*
	 *  True color frame buffer parameters:
	 */
	softc->fb_tc_sbus_delta =
		getprop(devi, "true-color-offset", 0x00800000);
	softc->fb_tc_sbus_delta -= softc->dev_reg_sbus_base;
	softc->fb_tc_mmap_size =
		getprop(devi, "true-color-size", 1152*900*4);
	softc->fb_tc_mmap_size = ddi_ptob(devi, ddi_btopr(devi,
					softc->fb_tc_mmap_size));

	DEBUGF(5, (CE_CONT, "true_color   sbus delta %x size %x\n",
	softc->fb_tc_sbus_delta, softc->fb_tc_mmap_size));

	/*
	 *  Video enable memory parameters
	 * (set them up even if we don't use them):
	 */
	softc->fb_video_sbus_delta = 0x00E00000 - softc->dev_reg_sbus_base;
	softc->fb_video_mmap_size =
		ddi_ptob(devi, ddi_btopr(devi, 1152*900/8));

	DEBUGF(5, (CE_CONT, "video_enable sbus delta %x size  %x\n",
		softc->fb_video_sbus_delta, softc->fb_video_mmap_size));



	/*
	 *  ALLOCATE SPACE IN KERNEL VIRTUAL MEMORY
	 *  FOR THE DEVICE REGISTERS AND
	 *  MONOCHROME OPERATIONS, BUILD THE TABLE
	 *  USED FOR USER SPACE MAPPING
	 *
	 *  Allocate space in virtual memory for the device register area
	 *  (and possibly the frame buffers.)
	 *  Remember the virtual address of the
	 *  device register area, and its physical address, which will be
	 *  used by cgeightmmap as the base mapping for all physical pages.
	 *
	 */

	if (ddi_map_regs(devi, 0, &reg, softc->dev_reg_sbus_base,
	    softc->dev_reg_mmap_size) == -1)
		goto failed;

	softc->device_reg = (Tc1_Device_Map *) reg;
	dmap = softc->device_reg;
	softc->basepage = hat_getkpfnum(reg);

	DEBUGF(5, (CE_CONT, "tcp_attach reg=0x%x basepage=0x%lx (0x%x)\n",
		(u_int)reg, ddi_ptob(devi, softc->basepage),
		softc->basepage));

	/*
	 *  Default operations to emulate a Sun CG8 card
	 *  (this may be modified
	 *  later via the PIPIO_S_EMULATION_MODE ioctl.)
	 *  Determine if there is a pip and/or eight bit memory present.
	 *  If this is an old style frame buffer set the register to
	 *  enable use of selection memory.
	 */
	softc->pg_enabled[PIXPG_8BIT_COLOR] = 0;

#ifndef NO_PIP
	if (softc->pg_possible[PIXPG_VIDEO_ENABLE]) {
		if ((dmap->control_status1 & TCP_CSR1_NO_PIP) == 0) {
			softc->pip_on_off_semaphore = 0;
			(void) pip_off(softc, 0);
		} else {
			softc->pg_possible[PIXPG_VIDEO_ENABLE] = 0;
			softc->pg_enabled[PIXPG_VIDEO_ENABLE] = 0;
		}
	}
#endif !NO_PIP

	/*
	 *  SUNVIEW historical note:
	 *  For the kernel's use map in the monochrome frame buffer,
	 *  selection memory, and if present
	 *  the eight bit memory after the device registers.
	 *  For each buffer calculate its virtual address, and place this
	 *  information into the device-dependent structure.
	 *  Note in the kernel the device register appears before
	 *  the frame buffer so we are constantly
	 *  adding in NBPG to the virtual offsets.
	 *  In user mode the opposite is
	 *  true, because of the way cg8_make maps things.
	 */

	/*
	 *  Build the memory mapping information used to map in the various
	 *  frame buffers in user (process) space.
	 */
	(void) x_build_mmap_info(softc);



	/*
	 *  INITIALIZE RAMDACs AND SELECTION MEMORY
	 *  IN PREPARATION FOR WINDOW USAGE
	 *
	 *  Before initializing the Brooktree's make sure
	 *  the selection memory
	 *  indicates that the monochrome plane is visible.
	 *  This is necessary
	 *  because we are about to enable the selection memory
	 *  as an input into
	 *  the overlay port on the RAMDACs.
	 */

	/*
	 *  Initialize the 3 Brooktree 457s for:
	 *	  (1) 4:1 multiplexing (since we are not 1280x1024)
	 *	  (2) Transparent overlay for overlay color 0.
	 *	  (3) Display the overlay plane
	 *  Enable read and write to all planes, turn off blinking, and set
	 *  control register.
	 */
	DEBUGF(5, (CE_CONT,
		"cg8: tcp_attach(): Initializing Brooktrees\n"));

	dmap->dacs.bt457.all_address = BT457_COMMAND;
	dmap->dacs.bt457.all_control = 0x43;
	dmap->dacs.bt457.all_address = BT457_READ_MASK;
	dmap->dacs.bt457.all_control = 0xff;
	dmap->dacs.bt457.all_address = BT457_BLINK_MASK;
	dmap->dacs.bt457.all_control = 0;
	dmap->dacs.bt457.all_address = BT457_CONTROL;
	dmap->dacs.bt457.all_control = 0;

	/*
	 *  Initialize overlay plane color map and load Brooktree 457's
	 *  as a group with that map.
	 *  (See tc_putcmap routine below for description of this
	 *  funny mapping.)
	 */
	dmap->dacs.bt457.all_address = 0;

	/* Unused, this is transpary value! */
	dmap->dacs.bt457.all_overlay =
		softc->omap_red[0] =
		softc->omap_green[0] =
		softc->omap_blue[0] = 0x00;
	dmap->dacs.bt457.all_address = 2;

	/* Window system bg: white guardian */
	dmap->dacs.bt457.all_overlay =
		softc->omap_red[1] =
		softc->omap_green[1] =
		softc->omap_blue[1] = 0xff;
	dmap->dacs.bt457.all_address = 1;

	/* Current window foreground. */
	dmap->dacs.bt457.all_overlay =
		softc->omap_red[2] =
		softc->omap_green[2] =
		softc->omap_blue[2] = 0xff;
	dmap->dacs.bt457.all_address = 3;

	/* Window system fg: black guardian */
	dmap->dacs.bt457.all_overlay =
		softc->omap_red[3] =
		softc->omap_green[3] =
		softc->omap_blue[3] = 0x00;

	/* Allow cursor color to be set normally */
	softc->sw_cursor_color_frozen = 0;

	/*
	 *  Load Brooktree 457's as a group with a linear ramp for the
	 *  24-bit true color frame buffer.
	 */
	for (color = 0, dmap->dacs.bt457.all_address = 0;
	    color < TC_CMAP_SIZE; color++) {
		dmap->dacs.bt457.all_color = (u_char)color;
	}


	return (DDI_SUCCESS);
	/*NOTREACHED*/

failed:
	if (softc->fb_8bit != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->fb_8bit,
		    softc->fb_8bit_sbus_delta,
		    (off_t)softc->fb_8bit_mmap_size);
	if (softc->fb_sel != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->fb_sel,
		    softc->fb_sel_sbus_delta,
		    (off_t)softc->fb_sel_mmap_size);
	if (softc->fb_mono != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->fb_mono,
		    softc->fb_mono_sbus_delta,
		    (off_t)softc->fb_mono_mmap_size);
	if (softc->device_reg != (Tc1_Device_Map *)0)
		ddi_unmap_regs(devi, (u_int)0,
		    (caddr_t *)&softc->device_reg,
		    softc->dev_reg_sbus_base,
		    (off_t)softc->dev_reg_mmap_size);

	(void) ddi_soft_state_free(tc1_state,
		    ddi_get_instance(softc->devi));

	return (DDI_FAILURE);
}

/*
 * "tcs_attach"
 *
 *  Perform device specific initializations for TCS frame buffer.
 *  (Called from cgeightattach().) This routine initializes parts of the
 *  per-unit data structure.
 *  In addition it maps the device's memory regions
 *  into virtual memory and performs hardware specific initializations.
 *  A number of per-unit data items have already been initialized
 *  from PROMs in cgeightattach().
 *
 *	  = DDI_SUCCESS if success
 *	  = DDI_FAILURE if failure
 */
static int
tcs_attach(devi, softc)
	dev_info_t *devi;	/* -> dev info struct w/node id... */
				/* (rest filled here) */
	SOFTC	  *softc; /* -> per-unit data struct to be filled in. */
{
	int	color;	/* Index: next color to set in lut ramp. */
	volatile Tcs_Device_Map  *dmap;   /* Fast ptr to dev reg area. */
		/* Virt addr of device map(&optionally fbs) */
	caddr_t	 reg;
		/* Cursor: selection memory word to init. */
	register u_long *sel_addr;
		/* Index: # of sel. mem. words initialized */
	register int	sel_cnt;
		/* # of words in selection memory to init. */
	int	 sel_max;

	/*
	 *  PROCESS REMAINING PROM PARAMETERS:
	 *
	 *  Pick up the device specific params exported
	 *  by the on-board PROMs.
	 *  Use this information to set up memory mapping characteristics
	 *  of the board.
	 */
	DEBUG_LEVEL(5);
	DEBUGF(5, (CE_CONT, "tcs_attach softc at %p\n", (void *)softc));

	if ((softc == NULL)			||
	    (softc->driver != CG8_DRIVER)	||
	    (softc->devi != devi))
		return (DDI_FAILURE);

	/*
	 *  Device register area parameters:
	 */
	softc->dev_reg_sbus_base =
	    getprop(devi, "device-map-offset", 0x00040000);
	softc->dev_reg_mmap_offset = CG8_VADDR_DAC;
	softc->dev_reg_sbus_delta = 0;
	softc->dev_reg_mmap_size = ddi_ptob(devi, ddi_btopr(devi, NBPG));
	softc->memory_map_end =
	    softc->dev_reg_mmap_offset+softc->dev_reg_mmap_size;


	DEBUGF(5, (CE_CONT,
		" dev_reg_sbus_base is %x\n", softc->dev_reg_sbus_base));

	/*
	 *  Monochrome frame buffer parameters:
	 */
	softc->fb_mono_sbus_delta =
	    getprop(devi, "monochrome-offset", 0x00100000);
	softc->fb_mono_sbus_delta -= softc->dev_reg_sbus_base;
	softc->fb_mono_mmap_size =
	    getprop(devi, "monochrome-size", 0x40000);
	softc->fb_mono_mmap_size =
	    ddi_ptob(devi, ddi_btopr(devi, softc->fb_mono_mmap_size));

	DEBUGF(5, (CE_CONT,
		"monochrome   sbus delta %x mmap offset	 %x size  %x\n",
		softc->fb_mono_sbus_delta, 0, softc->fb_mono_mmap_size));

	/*
	 *  Selection memory parameters:
	 */
	softc->fb_sel_sbus_delta =
	    getprop(devi, "selection-offset", 0x00140000);
	softc->fb_sel_sbus_delta -= softc->dev_reg_sbus_base;
	softc->fb_sel_mmap_size = getprop(devi, "selection-size", 0x40000);
	softc->fb_sel_mmap_size =
	    ddi_ptob(devi, ddi_btopr(devi, softc->fb_sel_mmap_size));

	DEBUGF(5, (CE_CONT,
		"selection	sbus delta %x mmap offset  %x size  %x\n",
	softc->fb_sel_sbus_delta,
	softc->fb_mono_mmap_size,
	softc->fb_sel_mmap_size));

	/*
	 *  Eight bit frame buffer parameters
	 *  (set them up even if don't use them):
	 */
	softc->fb_8bit_sbus_delta = 0x00180000 - softc->dev_reg_sbus_base;
	softc->fb_8bit_mmap_size =
	    ddi_ptob(devi, ddi_btopr(devi, 0x80000));

	softc->pg_possible[PIXPG_8BIT_COLOR] = 0;

	DEBUGF(5, (CE_CONT,
		"eight_bit	sbus delta %x mmap offset  %x size  %x\n",
	softc->fb_8bit_sbus_delta,
	softc->fb_mono_mmap_size + softc->fb_sel_mmap_size,
	softc->fb_8bit_mmap_size));

	/*
	 *  True color frame buffer parameters:
	 */
	softc->fb_tc_sbus_delta =
	    getprop(devi, "true-color-offset", 0x00200000);
	softc->fb_tc_sbus_delta -= softc->dev_reg_sbus_base;

	/*
	 * Note that the variable is set in the first call,
	 * then used as a param to the call in the second stmt
	 */
	softc->fb_tc_mmap_size =
	    getprop(devi, "true-color-size", 0x200000);
	softc->fb_tc_mmap_size =
	    ddi_ptob(devi, ddi_btopr(devi, softc->fb_tc_mmap_size));

	DEBUGF(5, (CE_CONT, "true_color   sbus delta %x size %x\n",
	softc->fb_tc_sbus_delta,
	softc->fb_tc_mmap_size));


	/*
	 *  ALLOCATE SPACE IN KERNEL VIRTUAL MEMORY FOR THE
	 *  DEVICE REGISTERS AND MONOCHROME OPERATIONS
	 *
	 *  Allocate space in virtual memory for the device register area
	 *  (and possibly the frame buffers.)
	 *  Remember the virtual address of the
	 *  device register area, and its physical address, which will be
	 *  used by cgeightmmap as the base mapping for all physical pages.
	 *
	 */
	if (ddi_map_regs(devi, 0, &reg, softc->dev_reg_sbus_base,
	    softc->dev_reg_mmap_size) == -1)
		goto failed;

	softc->device_tcs = (Tcs_Device_Map *) reg;
	dmap = softc->device_tcs;
	softc->basepage = hat_getkpfnum(reg);

	DEBUGF(5, (CE_CONT, "tcs_attach reg=0x%x basepage=0x%lx (0x%x)\n",
		(u_int)reg, ddi_ptob(devi,
		softc->basepage), softc->basepage));

	/*
	 *  Default operations to emulate a Sun CG8 card
	 * (this may be modified later
	 *  via the PIPIO_S_EMULATION_MODE ioctl.)
	 */
	softc->pg_enabled[PIXPG_8BIT_COLOR] = 0;

	/*
	 *  For the kernel's use map in the monochrome frame buffer,
	 *  and selection memory after the device registers.
	 *  For each buffer calculate its virtual address,
	 *  and place this information into the device-dependent structure.
	 *  Note in the kernel the the device reg appears before the frame
	 *  buffer so we are constantly adding in
	 *  NBPG to the virtual offsets.
	 *  In user mode the opposite is true,
	 *  because of the way cg8_make maps things.
	 *
	 * solaris 2.4 port (DDI/DKI): comment is obsolete (was sunview)
	 * but left in for general info
	 */

	/*
	 *  Build the memory mapping information used to map
	 *  in the various frame
	 *  buffers in user (process) space.
	 */
	(void) x_build_mmap_info(softc);


	/*
	 *  INITIALIZE VENUS CHIPS, RAMDAC AND SELECTION MEMORY
	 *  IN PREPARATION FOR WINDOW USAGE
	 *
	 *  Initialize venus chips to turn on operation of Brooktree, memory
	 *  timing, etc. Use the switch settings if this is not a model a board.
	 *
	 *  Before initializing the Brooktree make sure the selection memory
	 *  indicates that the monochrome plane is visible. This is necessary
	 *  because we are about to enable the selection memory as an input into
	 *  the overlay port on the RAMDACs.
	 */
	if (softc->device_tcs->tc_venus.io_general & VENUS_TCS_MODEL_A) {
		(void) tcs_init_native(softc);
	} else {
		switch (softc->device_tcs->tc_venus.io_general &
					VENUS_TIMING_MASK)
		{
		default:
			cmn_err(CE_WARN,
	"Unknown switch setting: 0x%0x; defaulting to native timing.\n",
	softc->device_tcs->tc_venus.io_general & VENUS_TIMING_MASK);
			/*FALLTHROUGH*/
		case VENUS_TIMING_NATIVE:
			(void) tcs_init_native(softc);
			break;
		case VENUS_TIMING_NTSC:
			(void) tcs_init_ntsc(softc);
			break;
		case VENUS_TIMING_PAL:
			(void) tcs_init_pal(softc);
			break;
		}
		softc->linebytes1 = softc->width / 8;
		softc->linebytes32 = softc->width * 4;
	}

	sel_max = softc->width*softc->linebytes1 / sizeof (u_long);
	for (sel_addr = (u_long *)softc->fb_sel, sel_cnt = 0;
	    sel_cnt < sel_max; sel_cnt++) {
		*sel_addr++ = (u_long)0xffffffff;
	}

	/*
	 *  Initialize the Brooktree 473:
	 */
	DEBUGF(5, (CE_CONT, "tcs_attach: Initializing Brooktree 473\n"));

	dmap->dac.control = 0x30;	/* 8-bit pixels, */
					/* NTSC timing (IRE = 7.5) */
	dmap->dac.read_mask = 0xff;

	/*
	 *  Initialize overlay plane color map.
	 */
	dmap->dac.overlay_write_addr = 0; /* Overlay colors: */
	dmap->dac.overlay = 0x00;	 /* 00: transparent pass-thru */
	dmap->dac.overlay = 0x00;	 /* ... */
	dmap->dac.overlay = 0x00;	 /* ... */
	dmap->dac.overlay = 0xff;	 /* 01: TC cursor value: white. */
	dmap->dac.overlay = 0xff;	 /* ... */
	dmap->dac.overlay = 0xff;	 /* ... */
	dmap->dac.overlay = 0xff;	 /* 10: window bg color: white. */
	dmap->dac.overlay = 0xff;	 /* ... */
	dmap->dac.overlay = 0xff;	 /* ... */
	dmap->dac.overlay = 0x00;	 /* 11: window fg color: black. */
	dmap->dac.overlay = 0x00;	 /* ... */
	dmap->dac.overlay = 0x00;	 /* ... */

	softc->omap_red[0] = softc->omap_green[0] = softc->omap_blue[0] = 0x00;
	softc->omap_red[1] = softc->omap_green[1] = softc->omap_blue[1] = 0xff;
	softc->omap_red[2] = softc->omap_green[2] = softc->omap_blue[2] = 0xff;
	softc->omap_red[3] = softc->omap_green[3] = softc->omap_blue[3] = 0x00;

	/*
	 *  Load Brooktree 473 as a group with a linear ramp for the 24-bit true
	 *  color frame buffer. Use cmap_xxx as a shadow table since we load the
	 *  entire lut in tcs_putcmap(). (See note 1 in tcs_putcmap.)
	 */
	for (color = 0, dmap->dac.ram_write_addr = 0;
	    color < TC_CMAP_SIZE; color++) {
		softc->cmap_red[color] = dmap->dac.color = (u_char)color;
		softc->cmap_green[color] = dmap->dac.color = (u_char)color;
		softc->cmap_blue[color] = dmap->dac.color = (u_char)color;
	}

	DEBUG_LEVEL(0);

	return (DDI_SUCCESS);
	/*NOTREACHED*/

failed:
	if (softc->fb_sel != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->fb_sel,
		    softc->fb_sel_sbus_delta,
		    (off_t)softc->fb_sel_mmap_size);
	if (softc->fb_mono != NULL)
		ddi_unmap_regs(devi, (u_int)0, (caddr_t *)&softc->fb_mono,
		    softc->fb_mono_sbus_delta,
		    (off_t)softc->fb_mono_mmap_size);
	if (softc->device_tcs != (Tcs_Device_Map *)0)
		ddi_unmap_regs(devi, (u_int)0,
		    (caddr_t *)&softc->device_tcs,
		    softc->dev_reg_sbus_base,
		    (off_t)softc->dev_reg_mmap_size);

	(void) ddi_soft_state_free(tc1_state,
		    ddi_get_instance(softc->devi));

	return (DDI_FAILURE);
}

/*
 * "tcs_init_native"
 *
 *  Initialize venus chips on SBus TCS Card to operate at
 *  Apple 13" monitor scan rates (60Hz non-interlaced, 640 x 480.)
 */
static int
tcs_init_native(softc)
	SOFTC	*softc; /* characteristics for our unit. */
{
	/* -> device reg area on TCS card. */
	volatile Tcs_Device_Map  *dmap;
	u_int	io_conf; /* Value to load into io config reg. */
	u_int	io_gen;	 /* Value to load into io general reg. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (-1);

	dmap = softc->device_tcs;
	softc->timing_regimen = NATIVE_TIMING;
	softc->width = 640;
	softc->height = 480;
	softc->linebytes1 = softc->width / 8;
	softc->linebytes8 = softc->width;
	softc->linebytes32 = softc->width * 4;

	io_gen = (dmap->tc_venus.io_general & VENUS_TCS_MODEL_A)
	    ? 0x001c : 0x781c;
	io_conf = (dmap->tc_venus.io_general & VENUS_TCS_MODEL_A)
	    ? 0x00df : 0x1fdf;

	/*
	 *  Initialize venus chip 0 which affects
	 *  Brooktree operations and color
	 *  buffer operations. Configure the system
	 *  as a 24-bit true color card.
	 */
	dmap->tc_venus.control1			= 0x00;
	dmap->tc_venus.control2			= 0x00;
	dmap->tc_venus.control3			= 0x00;
	dmap->tc_venus.control4			= 0x00;
	dmap->tc_venus.refresh_interval	 = 0x70;
	dmap->tc_venus.control1			= 0x11;

	dmap->tc_venus.io_general		= io_gen;
	dmap->tc_venus.io_config		= io_conf;

	dmap->tc_venus.display_start		= 0x00000000;
	dmap->tc_venus.half_row_incr		= 0x00000100;
	dmap->tc_venus.display_pitch		= 0x00000140;
	dmap->tc_venus.cas_mask			= 0x7f;
	dmap->tc_venus.horiz_latency		= 0x00;

	dmap->tc_venus.horiz_end_sync		= 0x000c;
	dmap->tc_venus.horiz_end_blank		= 0x0024;
	dmap->tc_venus.horiz_start_blank	= 0x00c4;
	dmap->tc_venus.horiz_total		= 0x00d4;
	dmap->tc_venus.horiz_half_line		= 0x0000;
	dmap->tc_venus.horiz_count_load		= 0x0000;

	dmap->tc_venus.vert_end_sync		= 0x02;
	dmap->tc_venus.vert_end_blank		= 0x29;
	dmap->tc_venus.vert_start_blank		= 0x0209;
	dmap->tc_venus.vert_total		= 0x020c;
	dmap->tc_venus.vert_count_load		= 0x0000;
	dmap->tc_venus.vert_interrupt		= 0x020a;

	dmap->tc_venus.y_zoom			= 0x01;
	dmap->tc_venus.soft_register		= 0x00;

	dmap->tc_venus.io_general	= io_gen & ~VENUS_SOFT_RESET;
	dmap->tc_venus.io_general	= io_gen;

	/*
	 * Enable video.
	 */
	dmap->tc_venus.control3		= 0x73;
	dmap->tc_venus.control1		= 0xf1;

	/*
	 *  Initialize venus chip 1 which affects 1-bit
	 *  monochrome and selection
	 *  memory operations.
	 */
	dmap->mono_venus.control1		= 0x00;
	dmap->mono_venus.control2		= 0x00;
	dmap->mono_venus.control3		= 0x00;
	dmap->mono_venus.control4		= 0x00;
	dmap->mono_venus.refresh_interval	= 0x70;
	dmap->mono_venus.control1		= 0x13;

	dmap->mono_venus.io_config		= 0x00;
	dmap->mono_venus.display_start		= 0x00000000;
	dmap->mono_venus.half_row_incr		= 0x00000080;
	dmap->mono_venus.display_pitch		= 0x00000050;
	dmap->mono_venus.cas_mask		= 0x3f;
	dmap->mono_venus.horiz_latency		= 0x00;

	dmap->mono_venus.horiz_end_sync		= 0x000c;
	dmap->mono_venus.horiz_end_blank	= 0x0024;
	dmap->mono_venus.horiz_start_blank	= 0x00c4;
	dmap->mono_venus.horiz_total		= 0x00d4;
	dmap->mono_venus.horiz_half_line	= 0x0000;
	dmap->mono_venus.horiz_count_load	= 0x0000;

	dmap->mono_venus.vert_end_sync		= 0x02;
	dmap->mono_venus.vert_end_blank		= 0x29;
	dmap->mono_venus.vert_start_blank	= 0x0209;
	dmap->mono_venus.vert_total		= 0x020c;
	dmap->mono_venus.vert_count_load	= 0x0000;
	dmap->mono_venus.vert_interrupt		= 0x0000;

	dmap->mono_venus.y_zoom			= 0x01;
	dmap->mono_venus.soft_register		= 0xf0;

	dmap->mono_venus.control3		= 0xf3;
	dmap->mono_venus.control1		= 0xf3;
	return (0);
}

/*
 * "tcs_init_ntsc"
 *
 *  Initialize venus chips on SBus TCS Card to operate at NTSC scan rates.
 *  (30Hz interlaced 640 x 480.)
 *  If a genlock device is connected external (genlock) sync will be used.
 */
static int
tcs_init_ntsc(softc)
	SOFTC	*softc; /* characteristics for our unit. */
{
	volatile Tcs_Device_Map  *dmap;  /* -> device reg area on TCS card. */
	u_int   io_conf;	/* Value to load into io config register. */
	u_int   io_gen;	 /* Value to load into io general register. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (-1);

	dmap = softc->device_tcs;
	softc->timing_regimen = NTSC_TIMING;
	softc->width = 640;
	softc->height = 480;
	softc->linebytes1 = softc->width / 8;
	softc->linebytes8 = softc->width;
	softc->linebytes32 = softc->width * 4;

	if (dmap->tc_venus.io_general & VENUS_TCS_MODEL_A) {
		io_gen = (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) ?
			0x007c : 0x0074;
		io_conf = 0x00df;
	} else {
		io_gen = (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) ?
			0x78bc : 0x78f4;
		io_conf = 0x1fdf;
	}

	/*
	 *  Initialize venus chip 0 which affects Brooktree operations and color
	 *  buffer operations. Configure the system as a 24-bit true color card.
	 */
	dmap->tc_venus.control1			= 0x00;
	dmap->tc_venus.control2			= 0x00;
	dmap->tc_venus.control3			= 0x00;
	dmap->tc_venus.control4			= 0x00;
	dmap->tc_venus.refresh_interval		= 0x70;
	dmap->tc_venus.control1			= 0x11;
	dmap->tc_venus.control2			= 0x00;
	dmap->tc_venus.control3			= 0x7b;
	dmap->tc_venus.control4			= 0x00;

	dmap->tc_venus.io_general		= io_gen;
	dmap->tc_venus.io_config		= io_conf;

	dmap->tc_venus.display_start		= 0x00000000;
	dmap->tc_venus.half_row_incr		= 0x00000100;
	dmap->tc_venus.display_pitch		= 0x00000140;
	dmap->tc_venus.cas_mask			= 0x7f;
	dmap->tc_venus.horiz_latency		= 0x00;

	dmap->tc_venus.horiz_end_sync		= 0x000d;
	dmap->tc_venus.horiz_half_line		= 0x0060;
	dmap->tc_venus.horiz_count_load		= 0x0000;
	dmap->tc_venus.vert_end_sync		= 0x02;
	dmap->tc_venus.vert_end_blank		= 0x12;
	dmap->tc_venus.vert_start_blank		= 0x0102;
	dmap->tc_venus.vert_count_load		= 0x0000;
	dmap->tc_venus.vert_interrupt		= 0x0103;

	if (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) {
		dmap->tc_venus.horiz_end_blank		= 0x001c;
		dmap->tc_venus.horiz_start_blank	= 0x00bc;
		dmap->tc_venus.horiz_total		= 0x00c1;
		dmap->tc_venus.vert_total		= 0x0105;
	} else {
		dmap->tc_venus.horiz_end_blank		= 0x001b;
		dmap->tc_venus.horiz_start_blank	= 0x00bb;
		dmap->tc_venus.horiz_total		= 0x00ff;
		dmap->tc_venus.vert_total		= 0x01ff;
	}

	dmap->tc_venus.y_zoom			= 0x01;
	dmap->tc_venus.soft_register		= 0xf0;

	dmap->tc_venus.io_general		= io_gen & ~VENUS_SOFT_RESET;
	dmap->tc_venus.io_general		= io_gen;

	if (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) {
		dmap->tc_venus.control3		 = 0x7b;
		dmap->tc_venus.control1		 = 0xf9;
	} else {
		dmap->tc_venus.control3		 = 0xfb;
		dmap->tc_venus.control1		 = 0xf1;
	}

	/*
	 *  Initialize venus chip 1 which affects 1-bit monochrome and selection
	 *  memory operations.
	 */
	dmap->mono_venus.control1		= 0x00;
	dmap->mono_venus.control2		= 0x00;
	dmap->mono_venus.control3		= 0x00;
	dmap->mono_venus.control4		= 0x00;
	dmap->mono_venus.refresh_interval	= 0x70;
	dmap->mono_venus.control1		= 0x13;
	dmap->mono_venus.io_config		= 0x0000;

	dmap->mono_venus.display_start		= 0x00000000;
	dmap->mono_venus.half_row_incr		= 0x00000080;
	dmap->mono_venus.display_pitch		= 0x00000050;
	dmap->mono_venus.cas_mask		= 0x3f;
	dmap->mono_venus.horiz_latency		= 0x00;

	dmap->mono_venus.horiz_end_sync		= 0x000d;
	dmap->mono_venus.horiz_end_blank	= 0x001b;
	dmap->mono_venus.horiz_start_blank	= 0x00bb;
	dmap->mono_venus.horiz_total		= 0x00ff;
	dmap->mono_venus.horiz_half_line	= 0x0000;
	dmap->mono_venus.horiz_count_load	= 0x0000;

	dmap->mono_venus.vert_end_sync		= 0x02;
	dmap->mono_venus.vert_end_blank		= 0x12;
	dmap->mono_venus.vert_start_blank	= 0x0102;
	dmap->mono_venus.vert_total		= 0x01ff;
	dmap->mono_venus.vert_count_load	= 0x0000;
	dmap->mono_venus.vert_interrupt		= 0x0000;

	dmap->mono_venus.y_zoom			= 0x01;
	dmap->mono_venus.soft_register		= 0xf0;

	dmap->mono_venus.control3		= 0xfb;

	if (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) {
		dmap->mono_venus.control1		   = 0xfb;
	} else {
		dmap->mono_venus.control1		   = 0xf3;
	}
	return (0);
}

/*
 * "tcs_init_pal"
 *
 *  Initialize venus chips on SBus TCS Card to operate at PAL scan rates.
 *  (25Hz interlaced 768 x 592.)
 *  If a genlock device is connected external (genlock) sync will be used.
 */
static int
tcs_init_pal(softc)
	SOFTC	*softc; /* characteristics for our unit. */
{
	/* -> device reg area on TCS card. */
	volatile Tcs_Device_Map  *dmap;
	u_int	io_conf; /* Value to load into io config register. */
	u_int	io_gen; /* Value to load into io general reg. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (-1);

	dmap = softc->device_tcs;
	softc->timing_regimen = PAL_TIMING;
	softc->width = 768;
	softc->height = 592;
	softc->linebytes1 = softc->width / 8;
	softc->linebytes8 = softc->width;
	softc->linebytes32 = softc->width * 4;

	if (dmap->tc_venus.io_general & VENUS_TCS_MODEL_A) {
		io_gen = (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) ?
			0x00bc : 0x00b4;
		io_conf = 0x00df;
	} else {
		io_gen = (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) ?
			0x787c : 0x78f4;
		io_conf = 0x1fdf;
	}

	/*
	 *  Initialize venus chip 0 which affects Brooktree operations and color
	 *  buffer operations. Configure the system as a 24-bit true color card.
	 */
	dmap->tc_venus.control1			= 0x00;
	dmap->tc_venus.control2			= 0x00;
	dmap->tc_venus.control3			= 0x00;
	dmap->tc_venus.control4			= 0x00;
	dmap->tc_venus.refresh_interval		= 0x70;
	dmap->tc_venus.control1			= 0x11;
	dmap->tc_venus.control2			= 0x00;
	dmap->tc_venus.control3			= 0x00;
	dmap->tc_venus.control4			= 0x00;

	dmap->tc_venus.io_general		= io_gen;
	dmap->tc_venus.io_config		= io_conf;

	dmap->tc_venus.display_start		= 0x00000000;
	dmap->tc_venus.half_row_incr		= 0x00000100;
	dmap->tc_venus.display_pitch		= 0x00000180;
	dmap->tc_venus.cas_mask			= 0x7f;
	dmap->tc_venus.horiz_latency		= 0x00;

	dmap->tc_venus.horiz_end_sync		= 0x0010;
	dmap->tc_venus.horiz_end_blank		= 0x0024;
	dmap->tc_venus.horiz_start_blank	= 0x00e4;
	dmap->tc_venus.horiz_total		= 0x00ea;
	dmap->tc_venus.horiz_half_line		= 0x0075;
	dmap->tc_venus.horiz_count_load		= 0x0000;

	dmap->tc_venus.vert_end_sync		= 0x02;
	dmap->tc_venus.vert_end_blank		= 0x16;
	dmap->tc_venus.vert_start_blank		= 0x0136;
	dmap->tc_venus.vert_total		= 0x0138;
	dmap->tc_venus.vert_count_load		= 0x0000;
	dmap->tc_venus.vert_interrupt		= 0x0137;

	dmap->tc_venus.y_zoom			= 0x01;
	dmap->tc_venus.soft_register		= 0xf0;

	dmap->tc_venus.io_general		= io_gen & ~VENUS_SOFT_RESET;
	dmap->tc_venus.io_general		= io_gen;

	if (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) {
		dmap->tc_venus.control3		= 0x7b;
		dmap->tc_venus.control1		= 0xf9;
	} else {
		dmap->tc_venus.control3		= 0xfb;
		dmap->tc_venus.control1		= 0xf1;
	}

	/*
	 *  Initialize venus chip 1 which affects 1-bit monochrome and selection
	 *  memory operations.
	 */
	dmap->mono_venus.control1		= 0x00;
	dmap->mono_venus.control2		= 0x00;
	dmap->mono_venus.control3		= 0x00;
	dmap->mono_venus.control4		= 0x00;
	dmap->mono_venus.refresh_interval	= 0x70;
	dmap->mono_venus.control1		= 0x13;

	dmap->mono_venus.io_config		= 0x0000;
	dmap->mono_venus.display_start		= 0x00000000;
	dmap->mono_venus.half_row_incr		= 0x00000080;
	dmap->mono_venus.display_pitch		= 0x00000060;
	dmap->mono_venus.cas_mask		= 0x3f;
	dmap->mono_venus.horiz_latency		= 0x00;

	dmap->mono_venus.horiz_end_sync		= 0x0010;
	dmap->mono_venus.horiz_end_blank	= 0x0024;
	dmap->mono_venus.horiz_start_blank	= 0x00e4;
	dmap->mono_venus.horiz_total		= 0x00ff;
	dmap->mono_venus.horiz_half_line	= 0x0075;
	dmap->mono_venus.horiz_count_load	= 0x0000;

	dmap->mono_venus.vert_end_sync		= 0x02;
	dmap->mono_venus.vert_end_blank		= 0x16;
	dmap->mono_venus.vert_start_blank	= 0x0136;
	dmap->mono_venus.vert_total		= 0x01ff;
	dmap->mono_venus.vert_count_load	= 0x0000;
	dmap->mono_venus.vert_interrupt		= 0x0000;

	dmap->mono_venus.y_zoom			= 0x01;
	dmap->mono_venus.soft_register		= 0xf0;

	dmap->mono_venus.control3		= 0xfb;

	if (dmap->tc_venus.io_general & VENUS_NO_GENLOCK) {
		dmap->mono_venus.control1	= 0xfb;
	} else {
		dmap->mono_venus.control1	= 0xf3;
	}
	return (0);
}


/*
 * "tcs_putcmap"
 *
 *  Local routine to set the color map values for either the 24-bit or the
 *  monochrome frame buffer for RASTEROPS_TCS card. In addition to real
 *  lookup table manipulation, emulation of 8-bit indexed color maps is
 *  done for 24-bit mode.
 *
 *  Note that for this card the lut load must occur during vertical retrace,
 *  so the vertical retrace interrupt is used.
 *  Since the SPARCstation is really quite fast relative
 *  to the loading of the lut we always load the entire lut.
 *  This is possible because we maintain
 *  the whole lut in cmap_xxx within softc.
 *
 *	  = 0 if success
 *	  =   error indication otherwise
 */
static int
tcs_putcmap(softc, cmap_p, copyinfun, mode)
	SOFTC	  *softc; /* -> characteristics for our unit. */
	struct fbcmap  *cmap_p;	/* -> where to get color map */
				/* (ioctl data area). */
	int (*copyinfun)();
	int mode;
{
	u_int	count;		/* # of color map entries to set. */
	int	force_update;	/* Indicates if access real luts. */
	u_int	index;	/* Index: entry in cmap_xxx to access. */
	int	n_entry;	/* Index: ordinal # of curr map entry. */
	int	plane_group;	/* Top 7 bits of index = plane group. */
	struct fbcmap  cmap;	 /* copy of color map ioctl data area */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

	if (COPY(copyinfun, cmap_p, &cmap,
	    sizeof (struct fbcmap), mode) != 0)
		return (EFAULT);

	/*
	 *  Verify we need to actually move some data.
	 *  Initialize variables from caller's parameters, etc.
	 *  If the plane group is zero, default an
	 *  appropriate plane group.
	 */
	if ((cmap.count == 0) || !cmap.red || !cmap.green || !cmap.blue) {
		return (EINVAL);
	}
	count = cmap.count;
	index = cmap.index & PIX_ALL_PLANES;
	if (index+count > TC_CMAP_SIZE) {
		return (EINVAL);
	}

	/*
	 * This is a hold-over from the sunview/pixrect stuff
	 * (look in the <sys/pixrect.h> for the comment
	 * about PR_FORCE_UPDATE)
	 * problem is, we are explicitly not including <sys/pixrect.h>
	 * so the symbol is never defined
	 *
	 * XXX going to always force
	 */
#ifdef notdef
	if (cmap.index & PR_FORCE_UPDATE) {
		return (EINVAL);
	}
#endif /* notdef */

	/*
	 * assume always forcing better than not forcing
	 */
	force_update = 1;

	mutex_enter(&softc->softc_mutex);
	plane_group = PIX_ATTRGROUP(cmap.index);

	if (!plane_group) {
		plane_group = softc->pg_enabled[PIXPG_24BIT_COLOR] ?
			PIXPG_24BIT_COLOR : PIXPG_8BIT_COLOR;
	}

	/*
	 *  If interruts are enabled disable so we can
	 *  update things in peace.
	 */
#ifdef NOTDEF
	if (softc->device_tcs->tc_venus.control4 & VENUS_VERT_INT_ENA) {
		softc->device_tcs->tc_venus.control4 = 0;
		softc->device_tcs->tc_venus.status = 0;
	}
#endif NOTDEF

	/*
	 * Move caller's data, there are 2 cases:
	 * in overlay or colormap luts.
	 * Note we move the data into the proper
	 * index location RASTEROPS_TCS
	 * (see note 1 above.)
	 */
	if (plane_group == PIXPG_OVERLAY) {
		if (COPY(copyinfun, cmap.red,
			&softc->omap_red[index], count, mode) ||
		    COPY(copyinfun, cmap.green,
			&softc->omap_green[index], count, mode) ||
		    COPY(copyinfun,  cmap.blue,
			&softc->omap_blue[index], count, mode)) {

			mutex_exit(&softc->softc_mutex);
			return (EFAULT);
		}
	} else {
		if (COPY(copyinfun,  cmap.red,
			&softc->cmap_red[index], count, mode) ||
		    COPY(copyinfun, cmap.green,
			&softc->cmap_green[index], count, mode) ||
		    COPY(copyinfun,  cmap.blue,
			&softc->cmap_blue[index], count, mode)) {

			mutex_exit(&softc->softc_mutex);
			return (EFAULT);
		}
	}

	/*
	 *  If we are doing cg8 emulation,
	 *  and hardware update is not forced,
	 *  set 8-bit index emulation color table entries
	 *  for 24-bit buffer.
	 *  (The check for the 8-bit plane group is here
	 *  in case the kernel
	 *   knows 8-bit hardware exists,
	 *  but the driver did not export it to Sunview.)
	 */
	if (!force_update && (softc->pg_enabled[PIXPG_8BIT_COLOR] == 0) &&
	    ((plane_group == PIXPG_24BIT_COLOR) ||
	    (plane_group == PIXPG_8BIT_COLOR))) {
		for (n_entry = 0; n_entry < count; n_entry++, index++) {
			softc->em_red[index] = softc->cmap_red[index];
			softc->em_green[index] = softc->cmap_green[index];
			softc->em_blue[index] = softc->cmap_blue[index];
		}
	}

	/*
	 *  Set entries in actual Brooktree lookup tables
	 *  for 8/24-bit buffer.
	 */
	else if (plane_group == PIXPG_24BIT_COLOR ||
	    plane_group == PIXPG_8BIT_COLOR) {
		softc->flags |= CG8_24BIT_CMAP;		/* STUB */
		softc->device_tcs->tc_venus.control4 = 5;
	}

	/*
	 *  Set entries in actual Brooktree lookup tables
	 *  for monochrome overlay.
	 */
	else if (plane_group == PIXPG_OVERLAY) {
		if (softc->pg_enabled[PIXPG_24BIT_COLOR] != 0) {
			softc->flags |= CG8_OVERLAY_CMAP;	/* STUB */
			softc->device_tcs->tc_venus.control4 = 5;
		}
	}

	else {
		cmn_err(CE_WARN,
		    "Illegal plane group %d encountered\n", plane_group);

		mutex_exit(&softc->softc_mutex);
		return (EINVAL);
	}
	mutex_exit(&softc->softc_mutex);
	return (0);
}

/*
 * "tcs_update_cmap"
 *
 *  For a TCS card update the color maps (color and overlay) if
 *  required. This routine is called from cgeightpoll() when
 *  a vertical interrupt is detected on a tcs board. It does
 *  not clear the interrupt, as this is done in cgeightpoll().
 */
static int
tcs_update_cmap(softc)
	SOFTC	*softc;	  /* -> to characteristics for our unit. */
{
	/* ptr to tcs device reg area. */
	volatile Tcs_Device_Map  *device_tcs;
	int n_entry;	 /* Index: entry in cmap_xxx now accessing. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

	device_tcs = softc->device_tcs;

	if (softc->flags & CG8_24BIT_CMAP) {	/* STUB */
		device_tcs->dac.ram_write_addr = 0;
		for (n_entry = 0; n_entry < TC_CMAP_SIZE; n_entry++) {
			device_tcs->dac.color = softc->cmap_red[n_entry];
			device_tcs->dac.color = softc->cmap_green[n_entry];
			device_tcs->dac.color = softc->cmap_blue[n_entry];
		}
		softc->flags &= ~CG8_24BIT_CMAP;	/* STUB */
	}
	if (softc->flags & CG8_OVERLAY_CMAP) {	/* STUB */
		for (n_entry = 0; n_entry < TC_OMAP_SIZE; n_entry++) {
			if (!softc->sw_cursor_color_frozen || n_entry != 2) {
				device_tcs->dac.overlay_write_addr =
					translate_omap[n_entry];
				device_tcs->dac.overlay =
					softc->omap_red[n_entry];
				device_tcs->dac.overlay =
					softc->omap_green[n_entry];
				device_tcs->dac.overlay =
					softc->omap_blue[n_entry];
			}
		}
		softc->flags &= ~CG8_OVERLAY_CMAP;	/* STUB */
	}

	return (0);
}

/*
 * "x_build_mmap_info"
 *
 *  Local routine to build table used to memory map frame buffers into
 *  user space (as opposed to kernel space which is done using fb_mapin).
 *  Note the order of mapping will properly map a traditional cg8 card
 *  (if other extraneous plane groups are not present.)
 */
static int
x_build_mmap_info(softc)
	SOFTC	*softc; /* -> characteristics for this instance. */
{
	int	mmi;	/* Index: num entries in mmap_info set up. */
	Mmap_Info   *mmp;   /* Cursor: entry in mmap_info to initialize. */
	u_int	   offset; /* Offset into virtual memory of next fb. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

	mmi = 0;
	mmp = softc->mmap_info;
	offset = 0;

	if (softc->pg_enabled[PIXPG_OVERLAY]) {
		mmp->group = PIXPG_OVERLAY;
		mmp->bgn_offset = offset;
		mmp->end_offset = offset + softc->fb_mono_mmap_size;
		mmp->sbus_delta = softc->fb_mono_sbus_delta;
		offset = mmp->end_offset;
		mmp++;
		mmi++;
	}
	if (softc->pg_enabled[PIXPG_OVERLAY_ENABLE]) {
		mmp->group = PIXPG_OVERLAY_ENABLE;
		mmp->bgn_offset = offset;
		mmp->end_offset = offset + softc->fb_sel_mmap_size;
		mmp->sbus_delta = softc->fb_sel_sbus_delta;
		offset = mmp->end_offset;
		mmp++;
		mmi++;
	}
	if (softc->pg_enabled[PIXPG_24BIT_COLOR]) {
		mmp->group = PIXPG_24BIT_COLOR;
		mmp->bgn_offset = offset;
		mmp->end_offset = offset + softc->fb_tc_mmap_size;
		mmp->sbus_delta = softc->fb_tc_sbus_delta;
		offset = mmp->end_offset;
		mmp++;
		mmi++;
	}
	if (softc->pg_enabled[PIXPG_8BIT_COLOR]) {
		mmp->group = PIXPG_8BIT_COLOR;
		mmp->bgn_offset = offset;
		mmp->end_offset = offset + softc->fb_8bit_mmap_size;
		mmp->sbus_delta = softc->fb_8bit_sbus_delta;
		offset = mmp->end_offset;
		mmp++;
		mmi++;
	}
	if (softc->pg_enabled[PIXPG_VIDEO_ENABLE]) {
		mmp->group = PIXPG_VIDEO_ENABLE;
		mmp->bgn_offset = offset;
		mmp->end_offset = offset + softc->fb_video_mmap_size;
		mmp->sbus_delta = softc->fb_video_sbus_delta;
		offset = mmp->end_offset;
		mmp++;
		mmi++;
	}

	softc->mmap_count = mmi;
	return (0);
}

/*
 * "x_g_attr"
 *
 *  Local routine to return the characteristics (attributes) associated
 *  with the current frame buffer. (Called by tcone_ioctl).
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_g_attr(softc, gattr_p, copyoutfun, mode_flag)
	SOFTC	*softc; /* -> characteristics for our unit. */
	struct  fbgattr *gattr_p;	/* -> where to put attr */
					/* (data area from ioctl) */
	int (*copyoutfun)();
	int mode_flag;
{
	struct  fbgattr gattr;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if (copyoutfun == 0)
		return (EINVAL);

	/*
	 *  Return the characteristics of the frame buffer being emulated.
	 *  Fill in the characteristics which were received from the PROMs.
	 */
	if (softc->pg_enabled[PIXPG_24BIT_COLOR]) {
		gattr = tc1_attrdefault;
		gattr.fbtype.fb_height = softc->height;
		gattr.fbtype.fb_width = softc->width;
		gattr.fbtype.fb_size =
			softc->fb_mono_mmap_size +
			softc->fb_sel_mmap_size +
			softc->fb_tc_mmap_size;
	} else {
		gattr = cg4_attrdefault;
		gattr.fbtype.fb_height = softc->height;
		gattr.fbtype.fb_width = softc->width;
		gattr.fbtype.fb_size =
			softc->fb_mono_mmap_size +
			softc->fb_sel_mmap_size +
			softc->fb_8bit_mmap_size;
	}

	if (COPY(copyoutfun, &gattr, gattr_p,
	    sizeof (struct fbgattr), mode_flag) != 0)
		return (EFAULT);

	return (0);
}

/*
 * "x_g_emulation_mode"
 *
 *  Local routine to get emulation mode of driver operations.
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_g_emulation_mode(softc, mode_p, copyoutfun, flag)
	SOFTC	*softc; /* -> characteristics for our unit. */
	Pipio_Emulation *mode_p;  /* -> where to put mode of emulation. */
	int (*copyoutfun)();
	int flag;
{
	int pgi;	/* Index: next entry in pg_enabled to access. */
	Pipio_Emulation mode;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if (copyoutfun == 0)
		return (EINVAL);

	/*
	 *  Set up plane group information based on softc entries.
	 *  Get timing regimen.
	 */
	for (pgi = 0; pgi < FB_NPGS; pgi++) {
		if (softc->pg_enabled[pgi]) {
			mode.plane_groups[pgi] = 1;
		} else if (softc->pg_possible[pgi]) {
			mode.plane_groups[pgi] = 2;
		} else {
			mode.plane_groups[pgi] = 0;
		}
	}

	mode.timing = softc->timing_regimen;

	if (COPY(copyoutfun, &mode, mode_p,
	    sizeof (Pipio_Emulation), flag) != 0)
		return (EFAULT);

	return (0);
}

/*
 * "x_getcmap"
 *
 *  Local routine to return color map values for either the 24-bit or the
 *  monochrome frame buffer. In addition to real lookup table manipulation,
 *  emulation of 8-bit indexed color maps is done for 24-bit mode.
 *  (Called by tcone_ioctl).
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_getcmap(softc, cmap_p, copyinfun, copyoutfun, mode_flag)
	SOFTC *softc; /* -> characteristics for our unit. */
	struct fbcmap  *cmap_p;  /* -> where to return the color map values. */
	int (*copyinfun)(), (*copyoutfun)();
	int mode_flag;
{
	u_int count;  /* # of color map entries to return. */
	volatile Tc1_Device_Map *device_reg; /* Pointer to the Brooktrees. */
	volatile Tcs_Device_Map *device_tcs; /* Ptr to the TCS card dev regs. */
	int force_update;	/* Indicates if should access real luts. */
	u_int  index;		/* Index of first entry to access. */
	int n_entry;		/* Loop index: current color map entry. */
	int	plane_group;	 /* Top 7 bits of index = plane group. */
	struct fbcmap  cmap;	 /* copy of user-provided structure */
	u_char tmp_blue[TC_CMAP_SIZE];   /* Temporary for blue values. */
	u_char tmp_green[TC_CMAP_SIZE];  /* Temporary for green values. */
	u_char tmp_red[TC_CMAP_SIZE];	/* Temporary for red values. */

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if ((copyoutfun == 0) || (copyinfun == 0))
		return (EINVAL);

	if (COPY(copyinfun, cmap_p, &cmap, sizeof (struct fbcmap),
	    mode_flag) != 0)
		return (EFAULT);

	/*
	 *  Initialize variables from caller's parameters, etc.
	 *  Verify we need to actually move some data.
	 *  A zero plane group defaults based on emulation type.
	 */
	count = cmap.count;
	if ((count == 0) || !cmap.red || !cmap.green || !cmap.blue) {
		return (0);
	}

	index = cmap.index & PIX_ALL_PLANES;
	if (index+count > TC_CMAP_SIZE) {
		return (EINVAL);
	}

	/*
	 * This is a hold-over from the sunview/pixrect stuff
	 * (look in the <sys/pixrect.h> for the comment about PR_FORCE_UPDATE)
	 * problem is, we are explicitly not including <sys/pixrect.h>
	 * so the symbol is never defined
	 *
	 * XXX going to always force
	 */
#ifdef notdef
	if (cmap.index & PR_FORCE_UPDATE) {
		return (EINVAL);
	}
#endif /* notdef */

	/*
	 * assume always forcing better than not forcing
	 */
	force_update = 1;

	plane_group = PIX_ATTRGROUP(cmap.index);
	if (!plane_group) {
		plane_group = softc->pg_enabled[PIXPG_24BIT_COLOR] ?
			PIXPG_24BIT_COLOR : PIXPG_8BIT_COLOR;
	}

	/*
	 *  If there is no 8-bit frame buffer, and an update is not forced,
	 *  get 8-bit index emulation color table entries for 24-bit buffer.
	 */
	if (!force_update && (softc->pg_enabled[PIXPG_8BIT_COLOR] == 0) &&
	    (plane_group == PIXPG_24BIT_COLOR)) {
		if (COPY(copyoutfun, &softc->em_red[index],
					cmap.red, count, mode_flag) ||
		    COPY(copyoutfun, &softc->em_green[index],
					cmap.green, count, mode_flag) ||
		    COPY(copyoutfun, &softc->em_blue[index],
					cmap.blue, count, mode_flag)) {
			return (EFAULT);
		}
	}

	/*
	 *  Get entries for the actual Brooktree lookup tables
	 *  for 8/24-bit buffer.
	 */
	else if (plane_group == PIXPG_24BIT_COLOR ||
	    plane_group == PIXPG_8BIT_COLOR) {
		switch (softc->fb_model) {

		default:
		case RASTEROPS_TC:
		case RASTEROPS_TCP:
			device_reg = softc->device_reg;
			device_reg->dacs.bt457.all_address = index;
			for (n_entry = 0; n_entry < count; n_entry++) {
				tmp_red[n_entry] =
					device_reg->dacs.bt457.red_color;
				tmp_green[n_entry] =
					device_reg->dacs.bt457.green_color;
				tmp_blue[n_entry] =
					device_reg->dacs.bt457.blue_color;
			}
			break;
		case RASTEROPS_TCS:
			device_tcs = softc->device_tcs;
			device_tcs->dac.ram_read_addr = index;
			for (n_entry = 0; n_entry < count; n_entry++) {
				tmp_red[n_entry] = device_tcs->dac.color;
				tmp_green[n_entry] = device_tcs->dac.color;
				tmp_blue[n_entry] = device_tcs->dac.color;
			}
			break;
		case RASTEROPS_TCL:
			break;
		}
		if (COPY(copyoutfun, tmp_red, cmap.red,
				    count, mode_flag) ||
		    COPY(copyoutfun, tmp_green, cmap.green,
				    count, mode_flag) ||
		    COPY(copyoutfun, tmp_blue, cmap.blue,
				    count, mode_flag)) {
			return (EFAULT);
		}
	}

	/*
	 *  Get entries for the actual Brooktree lookup tables for monochrome
	 *  overlay.  (A shadow color map is accessed rather than hardware.)
	 */
	else if (plane_group == PIXPG_OVERLAY) {
		if (COPY(copyoutfun, &softc->omap_red[index],
			cmap.red, count, mode_flag) ||
		    COPY(copyoutfun, &softc->omap_green[index],
			cmap.green, count, mode_flag) ||
		    COPY(copyoutfun, &softc->omap_blue[index],
			cmap.blue, count, mode_flag)) {
			return (EFAULT);
		}
	} else {
		cmn_err(CE_WARN,
		    "Illegal plane group %d encountered\n", plane_group);
		return (EINVAL);
	}

	return (0);
}

/*
 * "x_g_fb_info"
 *
 *  Local routine to return a description of the frame buffers supported
 *  by this device instance. The plane groups are processed in order of
 *  their virtual address so that total_mmap_bytes can be properly calculated.
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_g_fb_info(softc, info_p, copyoutfun, flag)
	SOFTC	*softc;	 /* -> characteristics for our unit. */
	Pipio_Fb_Info   *info_p;  /* -> where to place frame buffer info. */
	int (*copyoutfun)();
	int flag;
{
	int fbi;	/* Index: next frame buffer description to move. */
	int mmi;	/* Index: # of mmap_info entry looking at. */
	Mmap_Info   *mmp;   /* Cursor: mmap_info entry looking at. */
	Pipio_Fb_Info   info;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if (copyoutfun == 0)
		return (EINVAL);

	fbi = 0;

	for (mmi = 0, mmp = softc->mmap_info;
	    mmi < softc->mmap_count; mmi++, mmp++) {
		info.fb_descriptions[fbi].group = mmp->group;
		info.fb_descriptions[fbi].width = softc->width;
		info.fb_descriptions[fbi].height = softc->height;
		info.fb_descriptions[fbi].mmap_size =
			mmp->end_offset-mmp->bgn_offset;
		info.fb_descriptions[fbi].mmap_offset = mmp->bgn_offset;

		switch (mmp->group) {

		case PIXPG_24BIT_COLOR:
			info.fb_descriptions[fbi].depth = 32;
			info.fb_descriptions[fbi].linebytes =
				softc->linebytes32;
			break;
		case PIXPG_8BIT_COLOR:
			info.fb_descriptions[fbi].depth = 8;
			info.fb_descriptions[fbi].linebytes =
				softc->linebytes8;
			break;

		default:
			info.fb_descriptions[fbi].depth = 1;
			info.fb_descriptions[fbi].linebytes =
				softc->linebytes1;
			break;
		}
		fbi++;
	}

	info.frame_buffer_count = softc->mmap_count;
	info.total_mmap_size =
		softc->mmap_info[softc->mmap_count-1].end_offset;

	if (COPY(copyoutfun, &info, info_p,
	    sizeof (Pipio_Fb_Info), flag) != 0)
		return (EFAULT);

	return (0);
}


/*
 * "x_g_type"
 *
 *  Local routine to return the type of the active frame buffer and its
 *  resolution, etc.
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_g_type(softc, fb_p, copyoutfun, mode)
	SOFTC	 *softc; /* -> characteristics for our unit. */
	struct fbtype  *fb_p;	/* -> where to put type info */
				/* (ioctl data area). */
	int (*copyoutfun)();
	int mode;
{
	struct fbtype  fb;

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if (copyoutfun == 0)
		return (EINVAL);
	if (fb_p == NULL)
		return (EINVAL);

	/*
	 *  Return information based on the type
	 *  of frame buffer being emulated.
	 */
	if (softc->pg_enabled[PIXPG_24BIT_COLOR]) {
		/*
		 *  If we are emulating a Sun monochrome BW2
		 *  return that data,
		 *  otherwise return the actual information
		 *  about the current frame buffer.
		 */
		switch (tc1_attrdefault.sattr.emu_type) {

		case FBTYPE_SUN2BW:
			fb = tc1typedefault;
			fb.fb_height = softc->height;
			fb.fb_width = softc->width;
			fb.fb_size = softc->fb_mono_mmap_size;
			break;

		default:
			fb = tc1_attrdefault.fbtype;
			fb.fb_height = softc->height;
			fb.fb_width = softc->width;
			fb.fb_size =
			    softc->fb_mono_mmap_size +
			    softc->fb_sel_mmap_size +
			    softc->fb_8bit_mmap_size +
			    softc->fb_tc_mmap_size;
			break;
		}
	} else {
		fb = cg4_attrdefault.fbtype;
		fb.fb_height = softc->height;
		fb.fb_width = softc->width;
		fb.fb_size = softc->fb_mono_mmap_size +
				    softc->fb_sel_mmap_size +
				    softc->fb_8bit_mmap_size;
	}

	if (COPY(copyoutfun, &fb, fb_p, sizeof (struct fbtype), mode) != 0)
		return (EFAULT);

	return (0);
}

/*
 * "x_s_emulation_mode"
 *
 *  Local routine to set emulation mode for driver operations.
 *  Use the mode specification to determine how to configure the
 *  system.
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
static int
x_s_emulation_mode(softc, mode_p, copyinfun, flag)
	SOFTC	*softc; /* characteristics for our unit. */
	Pipio_Emulation *mode_p;  /* -> mode of emulation to set. */
	int (*copyinfun)();
	int flag;
{
	int color;	   /* Index: color to put into look-up table. */
	volatile Tc1_Device_Map  *device_reg; /* ptr to device regs. */
	volatile Tcs_Device_Map  *device_tcs; /* ptr to tcs dev regs. */
	int pgi;	/* Index: entry in pg_enabled to access. */
	Pipio_Emulation mode;

	DEBUGF(2, (CE_CONT, "x_s_emulation_mode(ENTER)\n"));

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);
	if (copyinfun == 0)
		return (EINVAL);
	if (mode_p == NULL)
		return (EINVAL);

	if (COPY(copyinfun, mode_p, &mode,
	    sizeof (Pipio_Emulation), flag) != 0) {
		DEBUGF(2, (CE_CONT,
		    "x_s_emulation_mode(), copyin failed\n"));
		return (EFAULT);
	}

	mutex_enter(&softc->softc_mutex);

	/*
	 *  Process the emulation timing and size specified by the caller.
	 *  If our device is not capable of providing the specified timing
	 *  or size return an error.
	 */
	DEBUGF(2, (CE_CONT,
		"x_s_emulation_mode() mode.timing=%d; softc->fb_model=%d\n",
		mode.timing, softc->fb_model));

	switch (mode.timing) {

	case NATIVE_TIMING:
		if (softc->fb_model == RASTEROPS_TCS) {
			DEBUGF(2, (CE_CONT,
				"x_s.. NATIVE_TIMING; RASTEROPS_TCS\n"));
			(void) tcs_init_native(softc);
		}
		break;
	case NTSC_TIMING:
		if (softc->fb_model != RASTEROPS_TCS) {
			DEBUGF(2, (CE_CONT,
				"x_s.. NTSC_TIMING; !!!!  RASTEROPS_TCS\n"));
			goto errexit;
		}

		DEBUGF(2, (CE_CONT, "x_s.. NTSC_TIMING; RASTEROPS_TCS\n"));
		(void) tcs_init_ntsc(softc);
		break;
	case PAL_TIMING:
		if (softc->fb_model != RASTEROPS_TCS) {
			DEBUGF(2, (CE_CONT,
				"x_s.. PAL_TIMING; !!!!  RASTEROPS_TCS\n"));
			goto errexit;
		}
		DEBUGF(2, (CE_CONT, "x_s.. PAL_TIMING; RASTEROPS_TCS\n"));
		(void) tcs_init_pal(softc);
		break;
	default:
		DEBUGF(2, (CE_CONT, "x_s.. default case - ERROR\n"));
		goto errexit;
	}


	/*
	 *  Set only the plane groups specified by the caller.
	 */
	for (pgi = 0; pgi < FB_NPGS; pgi++) {
		if (mode.plane_groups[pgi] == 1) {
			if (softc->pg_possible[pgi] == 0)
				goto errexit;
			softc->pg_enabled[pgi] = 1;
		} else {
			softc->pg_enabled[pgi] = 0;
		}
	}

	/*
	 *  If 24-bit memory is enabled, reset true color ramp in luts.
	 */
	if (softc->pg_enabled[PIXPG_24BIT_COLOR]) {
		switch (softc->fb_model) {

		default:
			device_reg = softc->device_reg;
			device_reg->dacs.bt457.all_address = 0;
			for (color = 0; color < TC_CMAP_SIZE; color++) {
				device_reg->dacs.bt457.red_color =
					(u_char)color;
				device_reg->dacs.bt457.green_color =
					(u_char)color;
				device_reg->dacs.bt457.blue_color =
					(u_char)color;
			}
			break;
		case RASTEROPS_TCS:
			device_tcs = softc->device_tcs;
			device_tcs->dac.ram_write_addr = 0;
			for (color = 0; color < TC_CMAP_SIZE; color++) {
				device_tcs->dac.color = (u_char)color;
				device_tcs->dac.color = (u_char)color;
				device_tcs->dac.color = (u_char)color;
			}
			break;
		case RASTEROPS_TCL:
			break;
		}
	}

	/*
	 *  Recompute the memory mapping information, so new configuration
	 *  will be mapped properly.
	 */
	(void) x_build_mmap_info(softc);

	mutex_exit(&softc->softc_mutex);

	return (0);

errexit:
	DEBUGF(2, (CE_CONT, "x_s.. ERROR EXIT CASE\n"));
	mutex_exit(&softc->softc_mutex);
	return (EINVAL);
}

/*
 * "x_s_map_slot"
 *
 *  Local routine to return information using test ioctl entry
 *  point.
 *
 *	  = 0 if success
 *	  = error indication otherwise
 */
/*ARGSUSED*/
static int
x_s_map_slot(softc, phys_addr)
	SOFTC *softc;	/* -> characteristics for our unit. */
	u_int *phys_addr;	/* -> where to put attributes */
				/* (data area from ioctl) */
{
#ifdef DWP_LATER

	if ((softc == NULL) || (softc->driver != CG8_DRIVER))
		return (EINVAL);

	if (softc->test_basepage)
		return (0);
	if (!(reg = (caddr_t)map_regs((caddr_t)*phys_addr,
	    (u_int)0x1000, 0x01))) {
		return (-1);
	}
	mutex_enter(&softc->softc_mutex);
	softc->test_basepage = hat_getkpfnum(reg); /* was fbgetpage(reg) */
	mutex_exit(&softc->softc_mutex);
#endif
	return (0);
}

/*
 *  OVERVIEW OF TC BOARD HARDWARE
 *
 *	(1) The SBus TC Card product
 *	    contains two frame buffers. One frame buffer is 24 bits deep,
 *	    and its pixel values are treated as 3 8-bit indices
 *	    into 3 separate 8-in-8-out lookup tables to yield
 *          8 bits for each of the red, green, and blue guns
 *	    of the monitor. Each of the 8-in-8-out lookup tables
 *	    is loaded with a linear ramp
 *	    (or possibly a gamma corrected ramp)
 *	    yielding a true color display of pixel values.
 *
 *		    +------------+
 *		    |		 |	  +---+
 *		    |		 |---/--->|vlt|---/---> red
 *		    |     24     |   8    +---+   8
 *		    |		 |
 *		    |    bit     |	  +---+
 *	    ---/--->|		 |---/--->|vlt|---/---> green
 *	      24    |    frame   |   8    +---+   8
 *		    |		 |
 *		    |   buffer   |	  +---+
 *		    |		 |---/--->|vlt|---/---> blue
 *		    |		 |   8    +---+   8
 *		    +------------+
 *
 *	(2) The other frame buffer is monochrome and is 1 bit deep.
 *	    Its pixel values are treated as inputs into the overlay
 *	    of the 8-in-8-out lookup tables associated with the
 *	    24-bit frame buffer. Thus, the 1-bit frame buffer appears
 *	    as an overlay over the 24-bit frame buffer on the screen.
 *	    For each pixel, if the overlay is visible the 24-bit value
 *	    is not and vice-versa. Whether the overlay is visible or not
 *	    is controlled by a separate 1-bit deep selection memory. The
 *	    output of this memory is the input for the overlay transparency
 *	    bit on each of the 8-in-8-out lookup tables. Thus, the value in
 *	    the selection memory associated with a pixel specifies which
 *	    value is displayed for a given pixel, the one from the 24-bit
 *	    memory or the monochrome overlay.
 *
 *		    +-----------+			  +--------+
 *		    |		|      +-----------+      |	   |
 *		    |     1	|--/-->|  red vlt  |<--/--|   1    |
 *		    |		|  1   +-----------+   1  |	   |
 *		    |     bit	|      +-----------+      |  bit   |
 *	    ---/--->|		|--/-->| green vlt |<--/--|	   |
 *	       1    |    frame	|  1   +-----------+   1  |select. |
 *		    |		|      +-----------+      |	   |
 *		    |  buffer	|--/-->| blue vlt  |<--/--| memory |
 *		    |		|  1   +-----------+   1  |	   |
 *		    +-===-------+			  +--------+
 *
 *
 */

/*
 *  INDEXED COLOR MAP EMULATION
 *
 *  The SBus TCP support provides emulation of indexed color maps for both
 *  the 24-bit true color frame buffer and the monochrome overlay frame buffer.
 *  This support is done jointly by this kernel pixrect driver (tcone.c) and
 *  the normal pixrect driver colormap support (tc1_colormap.c).
 *
 *  For indexed color map emulation in the 24-bit buffer the kernel level
 *  driver for the tc1 maintains a main memory color map with 256 entries. When
 *  pixels are written or read in emulation mode the entries in this color map
 *  are accessed to perform a mapping between the 8-bit value used by the user,
 *  and the 24-bit true color value actually written to frame buffer memory.
 *  This mapping is performed by routines in this module, which are called from
 *  other modules (e.g., cg8_getput.c). In emulation mode the actual lookup
 *  table entries in the 3 Brooktree 457 RAMDACs are not touched, only the
 *  separate main memory color map. However, it is also possible to actually
 *  access the look up table entries in the RAMDACs. This is done by specifying
 *  the PR_FORCE_UPDATE flag in the plane parameter passed to cg8_getcolormap
 *  or cg8_putcolormap.
 *
 *  solaris 2.4: explicitly not including <sys/pixrect.h>
 *  so the symbol PR_FORCE_UPDATE is never defined.
 *  XXX always forcing the update - no other way to handle it for now.
 *
 *  The monochrome overlay emulation is handled in tc1_colormap.c. It is also
 *  possible to by-pass the emulation and actually write the four overlay color
 *  values.
 */

/*
 *  PHYSICAL TO VIRTUAL MEMORY MAPPING FOR DEVICES ON THE SBus TC Card
 *
 *  This driver performs mapping for the devices on the SBus TC Card. The
 *  mapping makes the physical devices appear to be contiguous with each other
 *  within the virtual space
 *
 *		Virtual Space			Physical Space
 *
 *				    Physical		    Offset from
 *				    address:		device registers
 *				    xx000000+------------------+
 *					    | On-board PROMs   |
 *					    +------------------+
 *
 *				    xx400000+------------------+00000000
 *					    | Device registers |
 *  Offset from device			    +------------------+
 *  registers
 *  00000000+------------------+    xx800000+------------------+00400000
 *	    | Monochrome buffer|	    | True color buffer|
 *  00020000+------------------+	    +------------------+
 *	    | Selection memory |
 *  00040000+------------------+    xxC00000+------------------+00800000
 *	    | True color buffer|	    | Monochrome buffer|
 *  00435000+------------------+	    +------------------+
 *	    | Device registers |
 *	    +------------------+    xxD00000+------------------+00900000
 *					    | Selection memory |
 *					    +------------------+
 *
 *  The offset from the device registers in virtual space is defined near the
 *  beginning of this file in constants of the form TCP_xxx_VADDR_OFFSET, where
 *  xxx is MONOCHROME, TRUE_COLOR, etc.
 *
 *  The offset from the device registers in physical space is defined near the
 *  beginning of this file in constants of the form TCP_xxx_PADDR_OFFSET.
 *
 *  The offset from the beginning of the SBus physical device space
 *  (and thus the on-board PROMs) is defined in constants of the form
 *  TCP_xxx_sbus_delta_offset.
 *
 *  The mapping from virtual to physical space is accomplished in cgeightmmap,
 *  and is based on an offset relative to the beginning of the device registers
 *  in virtual address space. The reason the offset is based on the device
 *  registers is that the device registers where passed into map_regs to set up
 *  the virtual space.
 *
 *  ADDITIONAL DEVICES ON THE SBus TCP Card [w/pip option]
 *
 *  The SBus TCP Card provides two additional buffers which must be mapped
 *  these are the 8-bit view of the 24-bit true color memory and the video
 *  enable memory. This results in a virtual space which looks like:
 *
 *	Virtual Space				Physical Space
 *
 *				    Physical		    Offset from
 *				    address:		device registers
 *				    xx000000+------------------+
 *					    | On-board PROMs   |
 *					    +------------------+
 *
 *				    xx400000+------------------+00000000
 *					    | Device registers |
 *					    +------------------+
 *
 *				    xx700000+------------------+00000000
 *					    | 8-bit buffer     |
 *  Offset presented to cgeightmmap for	    +------------------+
 *  mapping
 *  00000000+------------------+    xx800000+------------------+00400000
 *	    | Monochrome buffer|	    | True color buffer|
 *  00020000+------------------+	    +------------------+
 *	    | Selection memory |
 *  00040000+------------------+    xxC00000+------------------+00800000
 *	    | True color memory|	    | Monochrome buffer|
 *  00435000+------------------+	    +------------------+
 *	    | 8-bit buffer     |
 *  00533000+------------------+    xxD00000+------------------+00900000
 *	    | Video enable mem.|	    | Selection memory |
 *  00553000+------------------+	    +------------------+
 *	    | Device Registers |
 *	    +------------------+    xxE00000+------------------+00A00000
 *					    | Video enable mem.|
 *					    +------------------+
 */
