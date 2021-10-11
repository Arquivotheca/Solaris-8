/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cgfourteen.c	1.124	99/04/14 SMI"

/*
 * Driver for Campus-II MBus framebuffer (CG14).
 * The CG14 is made up of the MDI, the VBC, the RAMDAC, the PCG, and various
 * amounts of VRAM.  Together, these make up the VSIMM.
 * The CG14 provides access to 24 bit frame buffers and compatibility modes
 * for 8-bit and 16-bit (8+8) frame buffer access. The main functions
 * of this driver are to provide:
 *
 *	a) mappings to the MDI control address space which contains the cursor
 *	   color look up tables, the DAC etc.
 *	b) mappings to the frame buffer a.k.a the VRAM in 8 different modes
 *	   of access.
 *	c) a cgthree emulation mode to provide backwards binary compatibility
 *	   for Pixrect/SunView applications.
 */


#include <sys/param.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/varargs.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/fbio.h>
#include <sys/debug.h>

#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/hat.h>
#include <sys/fs/snode.h>
#include <vm/seg_kmem.h>
#include <sys/x_call.h>
#include <sys/consdev.h>
#include <sys/proc.h>
#include <sys/visual_io.h>

/*
 * Header files for kernel pixrect support.
 */
#include <sys/pixrect.h>
#include <sys/pr_impl_util.h>
#include <sys/pr_planegroups.h>
#include <sys/memvar.h>
#include <sys/cg3var.h>	/* for CG3_MMAP_OFFSET */

#include <sys/cg14reg.h>
#include <sys/cg14io.h>

#include <sys/stat.h>
#include <sys/open.h>
#include <sys/t_lock.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/callb.h>

#ifdef CG14_DEBUG
static int debug = 1;
#else CG14_DEBUG
static int debug = 0;
#endif CG14_DEBUG

static int directmap_allowed = 1;

static enum BOOL {
	FALSE = 0,
	TRUE = 1,
	GET = 0,
	SET = 1
};

/* for debug only */
static int new_vcnt = 0;
static int exit_vcnt = 0;

/* Patchable vars for bugid 1150058 */
static int mdi_curs_retry = 5;
static int mdi_curs_scrub_enable = 1;

/*
 * The map table contains one entry for each type of mapping provided for
 * accessing MDI address spaces and the frame buffer.  Each entry of the
 * map table is of type struct mappings. The map table entries are initialized
 * at attach time.
 */
struct mappings {
	u_int	pagenum;	/* Base physical page frame number */
	u_int	cookie;		/* mapping cookie used in the mmap(2) call */
	u_int	length;		/* Maximum size of the mapping */
	u_int	prot;		/* Type of protection PROT_READ, PROT_WRITE */
	u_int	map_type;	/* Type of the mapping PRIVATE VS SHARED */
	u_int	cred;		/* Credentials. User or Supervisor */
};

struct maptable {
	struct mappings mappings[MDI_NMAPS];
};

/*
 * Device Identification
 */
struct vis_identifier cg14name = { "SUNWcg14" };

/*
 * Driver per-instance data structure
 * For the most part, a lock is required to gain access to this struct.
 */
struct mdi_softc {
			/*
			 *  This flag will mean that a thread wants
			 *  to update the h/w, has made the changes to
			 *  the softc, and is currently waiting for an
			 *  interrupt thread to do the update and clear
			 *  this flag.  This means that a mutex should
			 *  be entered before touching this flag.
			 */
	int		mdi_update;	/* lock for this data structure */
	volatile struct mdi_register_address *mdi_ctlp;	/* Control space */
	volatile struct mdi_register_address *mdi_save_ctlp;
	struct mdi_cursor_address *mdi_cursp;	/* Cursor control space */
	struct mdi_cursor_address *mdi_save_cursp;
	struct mdi_xlut_address *mdi_xlutp;	/* MDI X look up table page */
	struct mdi_clut_address	*mdi_clutp[3];	/* MDI color look up tables */
	struct mdi_dac_address *mdi_dacp;	/* DAC address space */
	struct mdi_shadow_luts	*mdi_shdw_lut; /* Driver copy of cluts */
	struct mdi_vesa_regs	*mdi_vesa_save; /* VESA chanels to monitor */
	u_int		*mdi_autoincr;	/* MDI auto increment counter */
	caddr_t	mdi_vrt_vaddr;	/* Shadow page containing vertical */
					/* retrace counter	*/
	int		mdi_gammaflag;	/* degammify CLUT inputs? */
	int		mdi_vrtflag; 	/* vertical retrace flag */
	int		mdi_vrtmappers;	/* # of processes mapping the */
					/* the vertical retrace */
					/* counter	*/
	int		mdi_impl;	/* Implentation modes	*/
	int		mdi_ncluts;	/* Number of cluts supported by hw */
	int		mdi_mon_width;	/* monitor resolution */
	int		mdi_mon_height;	/* in pixels */
	u_int		mdi_vram_paddr;	/* Frame buffer base phys addr */
	caddr_t		mdi_vram_vaddr;	/* vaddr of framebuffer */
	caddr_t		mdi_vram_save; /* save vram */
	off_t		mdi_vram_size;	/* size of vram (Bytes) */
	u_int		mdi_mon_size;	/* sizeof displayable part of fb */
	caddr_t		mdi_ndvram_save; /* save non-displayable vram */
	u_int		mdi_ndvram_size; /* size of non-display Video RAM */
	u_int		mdi_pixel_depth; /* total bits per pixel + X chan */

/* XXX These 4 should go away when all VSIMM 1 boards are gone */
	u_int		mdi_mbus_freq;	/* MBus frequency */
	u_int		mdi_sam_port_size;  /* for VBC registers */
	u_int		mdi_mihdel;	/* Used in R_SETUP calcs */
	u_int		mdi_v_rsetup;	/* shadow copy of VBC RCR */

	u_int		mdi_mon_vfreq;	/* Monitor frequency in Hz */
	u_int		mdi_mon_pixfreq; /* Pixel clock generator frequency */
	struct mdi_degammalut_data	*mdi_degamma; /* s/w-only table */
	struct fbcursor mdi_cur;		/* Soft cursor */
	u_int		mdi_shdw_curcmap[MDI_CURS_ENTRIES];
			/* Soft copy of 2 color regs */
	/*
	 * Pixrect support.
	 */
	kmutex_t	pixrect_mutex;	/* lock to protect the pixrect */
	Pixrect		mdi_pr;		/* kernel pixrect info	*/
	struct	mprp_data	mdi_prd; /* pixrect private data */

	u_char		mdi_pixmode;	/* shadow copy of MCR pixmode bits */
	struct mappings	mdi_maptable[MDI_NMAPS];
	kmutex_t	mdi_mutex;	/* lock for this structure */
	kcondvar_t	mdi_cv;		/* Condvar for sleeping		*/
	ddi_iblock_cookie_t	mdi_iblkc; /* iblock cookie for mutexen */
	dev_info_t	*dip;		/* back ptr to devinfo node */
	int		mdi_primordial_state; /* Video state before attach */
	int		mapped_by_prom;	/* flag to tell if console fb */
	int		mdi_emu_type;	/* active emulation type */
	kmutex_t	mdi_degammalock; /* Lock for degamma ops */
	boolean_t	intr_added;
	boolean_t	mutexen_initialized;
	boolean_t	mdi_control_regs_mapped;
	boolean_t	mdi_vram_space_mapped;
	boolean_t	attach_okay;
	u_int		mdi_maps_outstand; /* per-instance map count */
	int		mdi_suspended;
	int		mdi_sync_on;
	callb_id_t	mdi_callb_id;
#ifdef DEBUG
	int		mdi_cursor_softfails;	/* Stats for bug 1150058 */
	int		mdi_cursor_hardfails;
#endif
};



/*
 * Flags to indicate whether the alpha value needs to be updated or not.
 * Used in the color map update commands FBIOPUTMAP and MDI_SET_CLUT.
 */
static enum MDI_ACCESS_ALPHA {
	MDI_NOUPDATE_ALPHA = 0,
	MDI_UPDATE_ALPHA
};

static enum MDI_ACCESS_INDX {
	MDI_CG3_INDX = 0,	/* Cgthree emulation mode */
	MDI_ADDR_INDX,		/* Pages 0-15 of the MDI address space */
	MDI_CTLREG_INDX,	/* Page 0 of the MDI address space */
	MDI_CURSOR_INDX,	/* Hardware cursor; the MDI address space */
	MDI_SHDW_VRT_INDX,	/* Shadow vertical retrace counter */
	MDI_CHUNKY_XBGR_INDX,	/* Frame Buffer  32 bit chunky XBGR */
	MDI_CHUNKY_BGR_INDX,	/* Frame Buffer  32 bit chunky BGR */
	MDI_PLANAR_X16_INDX,	/* Frame Buffer  8+8 planar X channel */
	MDI_PLANAR_C16_INDX,	/* Frame Buffer  8+8 planar C channel */
	MDI_PLANAR_X32_INDX,	/* Frame Buffer 32 bit planar X */
	MDI_PLANAR_B32_INDX,	/* Frame Buffer 32 bit planar Blue */
	MDI_PLANAR_G32_INDX,	/* Frame Buffer 32 bit planar Green */
	MDI_PLANAR_R32_INDX	/* Frame Buffer 32 bit planar Red */
};

static enum MDI_REGSPACE {
	MDI_CONTROL_SPACE = 0,	/* The control space of the VSIMM */
	MDI_VRAM_SPACE		/* The VRAM space of the VSIMM */
};

static enum MDI_VRT {
	MDI_VRT_WAKEUP = 1,	/* Sleep until the next vertical retrace */
	MDI_VRT_COUNTER		/* The page containing the vertical retrace */
};				/* counter has been mapped by user processes */

static enum MDI_CT_UPDATE {
	MDI_NOACCESS_CLEARTXT = 0,
	MDI_ACCESS_CLEARTXT
};

/*
 * Bits 22, 23, 24 and 25 of the VRAM physical address encode the VRAM i.e
 * the frame buffer access mode. The following address masks determine the
 * various access modes.
 */
static enum MDI_ACCESS_MODE {
	CHUNKY_XBGR_MODE = 0x0,		/* 32 bit chunky XBGR */
	CHUNKY_BGR_MODE = 0x01000000,	/* 32 bit chunky BGR */
	PLANAR_X16_MODE = 0x02000000,	/* 8+8 planar X channel */
	PLANAR_C16_MODE = 0x02800000,	/* 8+8 planar C channel */
	PLANAR_X32_MODE = 0x03000000,	/* 32 bit planar X channel */
	PLANAR_B32_MODE = 0x03400000,	/* 32 bit planar Blue channel */
	PLANAR_G32_MODE = 0x03800000,	/* 32 bit planar Green channel */
	PLANAR_R32_MODE = 0x03c00000	/* 32 bit planar Red channel */
};


mon_spec_t mon_spec_table[] = {
	1024,   768,    60,
	64000000, 0, 16, 128, 160, 1024, 0, 60, 2, 6, 29, 768, 0,

	1024,   768,    66,
	74000000, 0, 4, 124, 160, 1024, 0, 66, 1, 5, 39, 768, 0,

	1024,   768,    70,
	74000000, 0, 16, 136, 136, 1024, 0, 70, 2, 6, 32, 768, 0,

	1152,   900,    66,
	94000000, 0, 40, 64, 272, 1152, 0, 66, 2, 8, 27, 900, 0,

	1152,   900,    76,
	108000000, 0, 28, 64, 260, 1152, 0, 76, 2, 8, 33, 900, 0,

	1280,   1024,   66,
	117000000, 0, 24, 64, 280, 1280, 0, 66, 2, 8, 41, 1024, 0,

	1280,   1024,   76,
	135000000, 0, 32, 64, 288, 1280, 0, 76, 2, 8, 32, 1024, 0,

	1600,   1280,   66,
	189000000, 0, 0, 256, 384, 1600, 0, 66, 0, 10, 44, 1280, 0,

	1920,   1080,   72,
	216000000, 0, 48, 216, 376, 1920, 0, 72, 3, 3, 86, 1080, 0,

	0,	0,	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const struct monitor_clock {
	u_int	mc_pixelfreq;
	u_char	mc_ics1562[MDI_ICS_NREGS];
} mdi_mc[] = {

	47000000,
	5, 0, 0, 2, 8, 1, 0, 0, 4, 10, 0, 1, 0, 0, 0, 0,

	54000000,
	4, 0, 2, 2, 8, 1, 0, 0, 4, 10, 0, 1, 0, 0, 0, 0,

	64000000,
	3, 0, 1, 2, 8, 1, 0, 0, 4, 10, 0, 1, 0, 0, 0, 0,

	74000000,
	5, 0, 3, 4, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	81000000,
	6, 0, 0, 5, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	84000000,
	3, 0, 1, 3, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	94000000,
	2, 0, 0, 2, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	108000000,
	3, 0, 2, 4, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	117000000,
	2, 0, 2, 3, 8, 1, 0, 0, 5, 10, 0, 1, 0, 0, 0, 0,

	135000000,
	3, 0, 4, 5, 8, 1, 0, 0, 6, 10, 0, 1, 0, 0, 0, 0,

	189000000,
	2, 0, 0, 2, 8, 1, 0, 0, 5, 10, 0, 0, 0, 0, 0, 0,

	216000000,
	3, 0, 2, 4, 8, 1, 0, 0, 5, 10, 0, 0, 0, 0, 0, 0,

	0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

};

/*
 * Per segment private data for all segments managed by the MDI driver
 */
static struct mdi_data {	/* Per segment private data	*/
	dev_t   dev;	/* The dev_t of this fb			*/
	u_int   maptype; /* Type of mapping MAP_SHARED/MAP_PRIVATE */
	u_int   objtype; /* Object type for which a mapping is provided */
	u_int   prot;		/* Protections for addresses mapped by seg */
	u_int   maxprot; /* Max protections for addresses mapped by seg */
	struct	vnode *vp;	/* common vnode for this device */
	offset_t	offset;	/* Device offset for start of mapping */
	kmutex_t	segmdi_mutex;	/* Mutex for segment driver */
};

struct mdi_lut_data {
	int	mdi_index;	/* Index into the color look up table */
	int	mdi_count;	/* and a count of the entries to be updated */
	u_char	mdi_alpha[MDI_CMAP_ENTRIES];	/* alpha component */
	u_char	mdi_blue[MDI_CMAP_ENTRIES];	/* blue component */
	u_char	mdi_green[MDI_CMAP_ENTRIES];	/* green component */
	u_char	mdi_red[MDI_CMAP_ENTRIES];	/* red component */
};

/*
 *  This data is local to the driver.  It never gets stuck into any
 *  hardware.  The app may request to update it (with MDI_SET_DEGAMMALUT),
 *  and it is used to fool apps that don't want any gamma correction.
 */
struct mdi_degammalut_data {
	int	index;				/* The usual meaning */
	int	count;				/* Ditto		*/
	u_char	degamma[MDI_CMAP_ENTRIES];	/* Only 1 channel needed */
};

/* Translations */
#define	MDI_DEGAMMA	MDI_GAMMA_CORRECT
#define	MDI_DEGAMMA_ON	MDI_GAMMA_CORRECTION_OFF
#define	MDI_DEGAMMA_OFF	MDI_GAMMA_CORRECTION_ON

static struct mdi_xlut_data {
	int	mdi_index;	/* Index into the color look up table */
	int	mdi_count;	/* and a count of the entries to be updated */
	u_char  mdi_xlut[MDI_CMAP_ENTRIES];
};

/*
 *  This lut really lives in the RAMDAC, but we treat it like any
 *  other; that is, we still keep a shadow copy of it and only update
 *  it at vertical retrace time.
 */
struct mdi_gammalut_data {
	int	index;				/* The usual meaning    */
	int	count;				/* Ditto		*/
	unsigned short red[MDI_CMAP_ENTRIES];   /* Low 10 bits go to DAC */
	unsigned short green[MDI_CMAP_ENTRIES]; /* Low 10 bits go to DAC */
	unsigned short blue[MDI_CMAP_ENTRIES];  /* Low 10 bits go to DAC */
};

/*
 *  This is the collection of all the software shadow-maps of the hardware.
 */
static struct mdi_shadow_luts {
	struct mdi_lut_data s_clut[3];	/* Shadow table describing cluts */
	struct mdi_xlut_data s_xlut;	/* Shadow table describing XLUT */
	struct mdi_lut_data s_cleartxt[3];	/* shdw table for cleartxt */
	struct mdi_gammalut_data s_glut; /* Shadow table describing RAMDAC */
	int	s_update_flag;	/* Indicates look up table to update */
};

static	struct mdi_vesa_regs {
	u_short m_hss; /* Horizontal Sync Set */
	u_short m_hsc; /* Horizontal Sync Set */
	u_short m_vss; /* Verical Sync Set */
	u_short m_vsc; /* Verical Sync Clear */
	int	video_state;
};

/*
 *  The hardware can become inconsistent when we update it
 *  outside of the vertical retrace interrupt handler.  So we
 *  keep shadow copies of the hardware around and schedule updates
 *  to it only at interrupt time.
 */
#define	MDI_CURS_UPDATE		0x0000001 /* update h/w cursor */
#define	MDI_PIXMODE_UPDATE	0x0000002 /* change the pixelmode in the MCR */
#define	MDI_VIDEO_UPDATE	0x0000004 /* update the state of video enable */
#define	MDI_DEFAULT_LINEBYTES   5120

/* array of clut id's */
static char mdi_acl[MDI_MAX_CLUTS] = {MDI_CLUT1, MDI_CLUT2, MDI_CLUT3};

/*
 *  The obligatory "Handy macros"
 *  They all assume they are given (mdi_register_address *)p
 */
		/* both VSIMMs have same intr_ena bit in MCR */
#define	mdi_set_vint(p, on)	((p)->m_mcr.mcr2.intr_ena = (on & 1))

#define	mdi_get_vint(p)		((p)->m_mcr.mcr2.intr_ena)

#define	mdi_get_vcntr(p)	((p)->m_vct & MDI_VCT)

/* For VSIMM 1 only */
#define	mdi_stop_vcntr(p)	if ((p)->m_rsr.revision == 0) \
					(p)->m_mcr.mcr1.reset = 0

#define	mdi_start_vcntr(p)	if ((p)->m_rsr.revision == 0) \
					(p)->m_mcr.mcr1.reset = 1

/*
 *  Sometimes we gotta empty the write buffers in the system.  We do
 *  this by forcing a write of something (like the MSR).  Force the hand
 *  of optimising compilers by using ``volatile''.
 */
#if	!defined(lint)
#define	FLUSH_WRITE_BUFFERS(p)	{volatile char t; t = (p)->m_msr; }
#else
#define	FLUSH_WRITE_BUFFERS(p)
#endif

/*
 *  Driver entry point prototypes
 */
static int mdi_identify(dev_info_t *);
static int mdi_attach(dev_info_t *, ddi_attach_cmd_t);
static int mdi_detach(dev_info_t *, ddi_detach_cmd_t);
static int mdi_open(dev_t *, int, int, cred_t *);
static int mdi_close(dev_t, int, int, cred_t *);
static u_int mdi_poll(caddr_t);
static int mdi_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int mdi_segmap(dev_t, off_t, struct as *, caddr_t *, off_t,
			u_int, u_int, u_int, cred_t *);
static int mdi_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int mdi_mmap(dev_t, off_t, int);
static int mdi_reset(dev_info_t *, ddi_reset_cmd_t);
static int mdi_power(dev_info_t *, int, int);

/*
 *  Private support prototypes
 */
static void mdi_intr(struct mdi_softc *);
static void mdi_update_cmap_index(int *, int *, int, int);
static void mdi_update_gamma_table(u_short *, u_short *, u_short *,
					int, struct mdi_softc *);
static void mdi_linear_cluts(struct mdi_softc *);
static int mdi_gamma_ops(struct mdi_softc *, struct mdi_set_gammalut *, int);
static int mdi_degamma_ops(struct mdi_softc *, struct mdi_set_degammalut *,
	int);
static int mdi_fbiocmds(dev_t, int, int, int);
static int mdi_set_pixelfreq(struct mdi_softc *);
static int mdi_get_pixelfreq(struct mdi_softc *);
static boolean_t mdi_setupinfo(dev_info_t *, int);
static boolean_t mdi_init(dev_info_t *, int);
static void mdi_setup_maptbl(int);
static void mdi_update_hw_xmap(u_char *, int, struct mdi_softc *);
static void mdi_update_hw_cmap(u_char *, u_char *, u_char *, u_char *,
	int, struct mdi_softc *);
static int mdi_get_shdw_clut(struct mdi_softc *, void *, int, int, int, int);
static int mdi_set_shdw_clut(struct mdi_softc *softc, void *, int, int,
	int, int);
static int mdi_set_shdw_xlut(struct mdi_softc *, struct mdi_set_xlut *);
static int mdi_get_shdw_xlut(struct mdi_softc *, caddr_t);
static void mdi_setcurpos(struct mdi_softc *, struct fbcurpos *);
static int mdi_set_curshape(struct mdi_softc *, struct fbcursor *, int);
static int mdi_set_shdw_curcmap(struct mdi_softc *, struct fbcmap *, int);
static int mdi_get_shdw_curcmap(struct mdi_softc *, struct fbcursor *, int);
static int mdi_ppr_setup(struct mdi_softc *, int);
static int mdi_set_pixelmode(struct mdi_softc *, int);
static int mdi_setcounters(struct mdi_softc *, struct mdi_set_counters *);
static int mdi_getmapinfo(dev_t, u_int, u_int *, u_int *, u_int *,
	u_int *, u_int *);
static int mdi_set_cursor(struct mdi_softc *, struct mdi_cursor_info *);
static void mdi_printf(int, char *, ...);
static void mdi_set_video(volatile struct mdi_register_address *, int);
static int mdi_get_video(volatile struct mdi_register_address *);
static void mdi_set_blanking(volatile struct mdi_register_address *, int);
static int mdi_get_blanking(volatile struct mdi_register_address *);
static void mdi_free_resources(struct mdi_softc *);
static void mdi_verify_cursor(struct mdi_softc *, u_long *, u_long *, u_long);
static int mdi_set_res(struct mdi_softc *, struct mdi_set_resolution *);
static struct mdi_set_resolution *mdi_get_timing_params(whf_t *);
static void mdi_set_timing_params(struct mdi_set_counters *, int, int, int,
	int, int, int, int, int);
static u_char *lookup_pcg_regs(u_int);
static void write_pcg_regs(struct mdi_softc *, u_char *);
static int mdi_do_suspend(struct mdi_softc *);
static int mdi_do_resume(struct mdi_softc *);
static int mdi_monitor_down(struct mdi_softc *);
static int mdi_monitor_up(struct mdi_softc *);

/* Call back routine for checkpoint/resume. */
static	boolean_t	mdi_callb(void *, int);

#define	COMPONENT_ONE	1
#define	ON	1
#define	OFF	0

/*
 *  External routines
 */
extern int nulldev(), nodev();

/*
 *  External data
 */
extern struct as kas;

/*
 *  Configuration data structures
 */
static struct cb_ops mdi_cb_ops = {
	mdi_open,
	mdi_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	mdi_ioctl,
	nodev,		/* devmap */
	mdi_mmap,
	mdi_segmap,
	nochpoll,	/* poll */
	ddi_prop_op,
	0,		/* streamtab */
	D_NEW | D_MP	/* driver compatibility flags */
};

struct dev_ops mdi_ops = {
	DEVO_REV,	/* revision */
	0,		/* refcnt */
	mdi_getinfo,
	mdi_identify,
	nulldev,	/* probe */
	mdi_attach,
	mdi_detach,
	mdi_reset,
	&mdi_cb_ops,		/* local driver ops */
	(struct bus_ops *)0,	/* no bus ops */
	mdi_power
};

static void *mdi_softc_state;	/* opaque ptr holds softc state instances */
/*
 *  This is the address at which the *console* frame buffer will be mapped.
 *  It will live in kernel-land and this driver may choose to alter the
 *  underlying physical mappings.
 */
caddr_t mdi_console_vaddr = (caddr_t)NULL;

/*
 *  Dynamic loading data structures
 */
extern struct mod_ops mod_driverops;
static struct modldrv modldrv = {
	&mod_driverops,		/* This is a driver module */
	"VSIMM/MDI driver V1.124",
	&mdi_ops		/* The dev_ops for this driver */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/* Handy state macro */
#define	getsoftc(i)	((struct mdi_softc *)ddi_get_soft_state(\
		mdi_softc_state, (i)))
/*
 * Data for the fbio get attribute fbgattr command. FBIOGTYPE command will
 * always return the frame buffer type emulated by MDI which is a cgthree.
 * FBIOGATTR command will return the real frame buffer type (MDI) and the
 * emulated frame buffer type.
 */

static struct fbgattr mdi_cg3_attr =  {

	FBTYPE_MDICOLOR,	/* real frame buffer type */
	0,			/* Owner */

	{	/* frame buffer type information (struct fbtype ) */
		FBTYPE_SUN3COLOR,	/* type returned by FBGTYPE cmd */
		0,		/* height */
		0, 		/* width */
		8,		/* depth */
		MDI_CMAP_ENTRIES, /* entries in the color look up table */
		0 		/* frame buffer size */
	},

	{ 	/* fbsattr info (struct fbsattr) */
		0,			/* flags */
		FBTYPE_SUN3COLOR, 	/* emulation type */
		{ 0 }			/* device specific info */
	},

	/*  list of devices emulated */
	{ FBTYPE_SUN3COLOR, -1, -1, -1}
};

/*
 * Routines for Sunwindows & pixrects
 */

/*
 * SunWindows specific stuff
 */
static int mdi_pixrect_rop(), mdi_pixrect_putcmap();

/*
 * kernel pixrect ops vector
 */
static struct pixrectops mdi_pixrect_ops = {
	mdi_pixrect_rop,
	mdi_pixrect_putcmap,
	mem_putattributes
};

static int
mdi_pixrect_rop(dpr, dx, dy, dw, dh, op, spr, sx, sy)
	Pixrect *dpr;
	int dx, dy, dw, dh, op;
	Pixrect *spr;
	int sx, sy;
{
	Pixrect mpr;

	if (spr && spr->pr_ops == &mdi_pixrect_ops) {
		mpr = *spr;
		mpr.pr_ops = &mem_ops;
		spr = &mpr;
	}

	return (mem_rop(dpr, dx, dy, dw, dh, op, spr, sx, sy));
}

static int
mdi_pixrect_putcmap(
	Pixrect *pr,
	int index,
	int  count,
	u_char *red,
	u_char *green,
	u_char *blue)
{
	volatile struct mdi_register_address *mdi;
	register struct mdi_softc *softc = getsoftc(mpr_d(pr)->md_primary);
	struct fbcmap cmap_info;
	int vrt_state, error = 0;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	cmap_info.index = index;
	cmap_info.count = count;
	cmap_info.red = red;
	cmap_info.green = green;
	cmap_info.blue = blue;

	mutex_enter(&softc->mdi_mutex);

	vrt_state = (u_int)mdi_get_vint(mdi);
	mdi_set_vint(mdi, FALSE);

	mutex_exit(&softc->mdi_mutex);

	error = mdi_set_shdw_clut(softc, (void *)&cmap_info,
			MDI_NOUPDATE_ALPHA, 0 /* MDI_CLUT1 */, FKIOCTL,
			MDI_ACCESS_CLEARTXT);

	mutex_enter(&softc->mdi_mutex);
	if (error) {
		mdi_set_vint(mdi, vrt_state);
		return (PIX_ERR);
	} else
		mdi_set_vint(mdi, TRUE);

	mutex_exit(&softc->mdi_mutex);
	return (error);
}

int
_init(void)
{
	register int error;
	char version[] = "1.124";

#if	defined(DEBUG)
	cmn_err(CE_CONT, "?CG14 driver V%s loaded.\n", version);
#endif	/* DEBUG */

	mdi_printf(CE_CONT, "CG14 driver compiled at %s, on %s\n",
					__TIME__, __DATE__);

	if ((error = ddi_soft_state_init(&mdi_softc_state,
	    sizeof (struct mdi_softc), MDI_MAX)) != 0)
		return (error);

	if ((error = mod_install(&modlinkage)) != 0) {
		ddi_soft_state_fini(&mdi_softc_state);
	}

	return (error);
}

int
_fini(void)
{
	register int error;

	if ((error = mod_remove(&modlinkage)) != 0)
		return (error);

	ddi_soft_state_fini(&mdi_softc_state);

	/* Ain't got no stinking callbacks */

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


/*
 * Identify MDI (a.k.a VRAM or framebuffer) device
 */
static int
mdi_identify(dev_info_t *dip)
{
	char *name = ddi_get_name(dip);
	char *s = "SUNW,cgfourteen";

	if ((strcmp(MDI_NAME, name) == 0) || (strcmp(s, name) == 0)) {
		mdi_printf(CE_CONT, "%s identified.\n", name);
		return (DDI_IDENTIFIED);
	} else
		return (DDI_NOT_IDENTIFIED);
}

/*
 *
 * Upto 4 Video SIMMs are supported in the C2+/sun4m architecture. Only two
 * Video SIMMS are supported in the current implementation.
 * The attach routine is invoked for each of the identified cg14 devices.
 */
static int
mdi_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register int instance = ddi_get_instance(dip);
	register struct mdi_softc *softc;
	char name[20];
	int error;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_RESUME:
		softc = (struct mdi_softc *)getsoftc(instance);
		if (!softc)
			return (DDI_FAILURE);

		if (!softc->mdi_suspended)
			return (DDI_SUCCESS);

		/* Restore the hardware states */
		error = mdi_do_resume(softc);

		return (error);

	default:
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(mdi_softc_state, instance) !=
	    DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	softc = getsoftc(instance);
	softc->dip = dip;
	softc->attach_okay = B_FALSE;
	softc->intr_added = B_FALSE;
	softc->mutexen_initialized = B_FALSE;
	softc->mdi_shdw_lut = NULL;
	softc->mdi_vrt_vaddr = NULL;
	softc->mdi_degamma = NULL;
	softc->mdi_control_regs_mapped = B_FALSE;
	softc->mdi_vram_space_mapped = B_FALSE;
	softc->mdi_cur.image = NULL;
	softc->mdi_cur.mask = NULL;
	softc->mdi_suspended = B_FALSE;
	softc->mdi_sync_on = 1; /* sync regs initialized by prom. */

	/*
	 * install the handler for interrupts from the MDI.
	 */
	if (ddi_add_intr(dip, (u_int)MDI_CONTROL_SPACE,
	    &softc->mdi_iblkc, (ddi_idevice_cookie_t *)NULL,
	    mdi_poll, (caddr_t)softc) != DDI_SUCCESS) {
		mdi_printf(CE_WARN, "Could not add MDI interrupt\n");
		goto failed;
	}
	softc->intr_added = B_TRUE;

	mutex_init(&softc->mdi_mutex, NULL, MUTEX_DRIVER, softc->mdi_iblkc);
	mutex_init(&softc->pixrect_mutex, NULL, MUTEX_DRIVER, softc->mdi_iblkc);
	mutex_init(&softc->mdi_degammalock, NULL, MUTEX_DRIVER, NULL);
	softc->mutexen_initialized = B_TRUE;

	/*
	 * Initialize software colormap. Initialize CLUT1 and
	 * CLUT2 mdi_init() will turn on the vertical retrace
	 * interrupts and the interrupt handler code will update
	 * the hardware color look up table.
	 */

	/* Get properties, map regs etc */
	if (!mdi_setupinfo(dip, instance))
		goto failed;

	if (!mdi_init(dip, instance))
		goto failed;

	(void) mdi_setup_maptbl(instance);

	(void) sprintf(name, "cgfourteen%d", instance);
	if (ddi_create_minor_node(dip, name, S_IFCHR, instance,
	    DDI_NT_DISPLAY, NULL) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "cgfourteen: "
		    "Could not create mdi minor node.\n");
		goto failed;
	}

	/*
	 * Initialize power management bookkeeping;
	 * components are created idle.
	 */
	if (pm_create_components(dip, 2) == DDI_SUCCESS) {
		(void) pm_busy_component(dip, 0);
		pm_set_normal_power(dip, 0, 1);
		pm_set_normal_power(dip, 1, 1);
	} else {
		goto failed;
	}

	ddi_report_dev(dip);

	/*
	 * Checkpoint/Resume needs to know ahead how much space to
	 * reserve for the state file. Since VRAM can be as big as
	 * as 8MB, we should add a call back routine to let cpr know
	 * in advance of the VRAM size.
	 */
	softc->mdi_callb_id = callb_add(mdi_callb, (void *)softc,
		CB_CL_CPR_FB, "cgfourteen");

	softc->attach_okay = B_TRUE;

	return (DDI_SUCCESS);

failed:
	mdi_free_resources(softc);
	return (DDI_FAILURE);
}


static int
mdi_power(dev_info_t *dip, int cmpt, int level)
{
	int instance = ddi_get_instance(dip);
	struct mdi_softc *softc = getsoftc(instance);
	int error = 0;

	if (cmpt != 1 || level < 0)
		return (DDI_FAILURE);

	if (level == 0)
		error = mdi_monitor_down(softc);
	else
		error = mdi_monitor_up(softc);

	if (error) {
		return (DDI_FAILURE);
	} else
		return (DDI_SUCCESS);
}


static int
mdi_reset(dev_info_t *dip, ddi_reset_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	struct mdi_softc *softc = getsoftc(instance);
#if	!defined(lint)
	volatile char t;
#endif

	/* No mutex_enter()'s are allowed in this entry point */

	switch (cmd) {
	case DDI_RESET_FORCE:
		/* Put the cluts in a state that the prom can understand */
		mdi_linear_cluts(softc);

		/* Set PPR to correct state */
		softc->mdi_ctlp->m_ppr = MDI_BLEND_LEFT_CLUT1;
		FLUSH_WRITE_BUFFERS(softc->mdi_ctlp);

		/* let the poor interrupt system rest now */
		mdi_set_vint(softc->mdi_ctlp, FALSE);

		softc->mdi_ctlp->m_msr = 0; /* Clear VINT in MSR */
#if	!defined(lint)
		t = softc->mdi_ctlp->m_fsr; /* clear faults from FSR */
#endif

		if (!softc->mapped_by_prom) {
			/* this device is not the console fb */
			mdi_set_vint(softc->mdi_ctlp, FALSE);
			mdi_stop_vcntr(softc->mdi_ctlp);
		}

		FLUSH_WRITE_BUFFERS(softc->mdi_ctlp);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
mdi_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	struct mdi_softc *softc = getsoftc(instance);
	int video_state, vrt_state;

	switch (cmd) {
	case DDI_DETACH:
		break;

	case DDI_SUSPEND:
		if (softc->mdi_suspended)
			return (DDI_FAILURE);

		/* Save the hardware states */
		(void) mdi_do_suspend(softc);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

	if (!softc->attach_okay) {
		mdi_free_resources(softc);
		ddi_soft_state_free(mdi_softc_state, instance);
		return (DDI_SUCCESS);
	}

	/* Delete the callb routine which was added at attach time. */
	(void) callb_delete(softc->mdi_callb_id);

	mutex_enter(&softc->mdi_mutex);
	video_state = mdi_get_video(softc->mdi_ctlp);

	if (softc->mdi_ctlp->m_rsr.revision >= 1) {
		mdi_set_video(softc->mdi_ctlp,
		    softc->mdi_primordial_state);
	} else {
		switch (softc->mdi_primordial_state) {
		case FBVIDEO_ON:
			if (!video_state) {
				mdi_stop_vcntr(softc->mdi_ctlp);
				mdi_start_vcntr(softc->mdi_ctlp);
				mdi_set_video(softc->mdi_ctlp, TRUE);
			}
			break;

		case FBVIDEO_OFF:
			if (video_state) {
				softc->mdi_update |= MDI_VIDEO_UPDATE;
				vrt_state =
				    mdi_get_vint(softc->mdi_ctlp);
				mdi_set_vint(softc->mdi_ctlp, TRUE);
				while (softc->mdi_update &
				    MDI_VIDEO_UPDATE) {
					cv_wait(&softc->mdi_cv,
					    &softc->mdi_mutex);
				}
				mdi_set_vint(softc->mdi_ctlp,
				    vrt_state);
			}
			break;
		}
	}
	mutex_exit(&softc->mdi_mutex);

	/*
	 * XXX do I need to undo any stuff from
	 * mdi_setup_maptbl() or mdi_setupinfo() ???
	 */

	/* Disable vertical retrace intrs */
	mdi_set_vint(softc->mdi_ctlp, FALSE);

	mdi_free_resources(softc);
	ddi_soft_state_free(mdi_softc_state, instance);
	pm_destroy_components(dip);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mdi_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error = DDI_SUCCESS;
	struct mdi_softc *softc;

	switch (infocmd) {

	case DDI_INFO_DEVT2DEVINFO:
		if ((softc = getsoftc(getminor((dev_t)arg))) != NULL) {
			*result = softc->dip;
		} else {
			*result = NULL;
			error = DDI_FAILURE;
		}
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		break;

	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

/*ARGSUSED*/
static int
mdi_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	if (otyp != OTYP_CHR)
		return (EINVAL);

	if (getsoftc(getminor(*devp)) == NULL)
		return (ENXIO);

	return (0);
}

/*ARGSUSED*/
static int
mdi_close(dev_t dev, int flag, int otyp, struct cred *credp)
{
	int instance = getminor(dev);
	struct mdi_softc *softc;
	int error = 0;

	if (otyp != OTYP_CHR)
		error = EINVAL;

	else if ((softc = getsoftc(instance)) == NULL)
		error = ENXIO;

	/* disable cursor for BrokenWindows */
	softc->mdi_cur.enable = 0;
	softc->mdi_cursp->curs_ccr &= ~MDI_CURS_ENABLE;

	return (error);
}


/*ARGSUSED*/
static int
mdi_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	volatile struct mdi_register_address *mdi;
	struct mdi_softc *softc = getsoftc(getminor(dev));
	int error = 0;
	int vrt_state;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	switch (cmd) {

	case MDI_GET_CFGINFO: {
		struct mdi_cfginfo cfginfo;

		mutex_enter(&softc->mdi_mutex);

		cfginfo.mdi_type = FBTYPE_MDICOLOR;
		cfginfo.mdi_ncluts = softc->mdi_ncluts;
		cfginfo.mdi_height = softc->mdi_mon_height;
		cfginfo.mdi_width = softc->mdi_mon_width;
		cfginfo.mdi_size = (u_int)softc->mdi_vram_size;
		cfginfo.mdi_mode = softc->mdi_pixel_depth;
		cfginfo.mdi_pixfreq = softc->mdi_mon_pixfreq;

		mutex_exit(&softc->mdi_mutex);

		if (copyout((caddr_t)&cfginfo, (caddr_t)arg,
				sizeof (struct mdi_cfginfo)) != 0)
			error = EFAULT;

	}
	break;

	case MDI_GET_DIAGINFO: {
		struct mdi_diaginfo diaginfo;

		if (suser(credp) == 0) {
			error = EPERM;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		diaginfo.mdi_cfg.mdi_type = FBTYPE_MDICOLOR;
		diaginfo.mdi_cfg.mdi_ncluts = softc->mdi_ncluts;
		diaginfo.mdi_cfg.mdi_height = softc->mdi_mon_height;
		diaginfo.mdi_cfg.mdi_width = softc->mdi_mon_width;
		diaginfo.mdi_cfg.mdi_size = (u_int)softc->mdi_vram_size;
		diaginfo.mdi_cfg.mdi_mode = softc->mdi_pixel_depth;
		diaginfo.mdi_cfg.mdi_pixfreq = softc->mdi_mon_pixfreq;

		diaginfo.mdi_mihdel = softc->mdi_mihdel;
		diaginfo.mdi_gstate = softc->mdi_gammaflag;

		mutex_exit(&softc->mdi_mutex);

		if (copyout((caddr_t)&diaginfo, (caddr_t)arg,
				sizeof (struct mdi_diaginfo)) != 0)
			error = EFAULT;

	}
	break;

	case MDI_SET_RESOLUTION: {
		struct mdi_set_resolution res;

		if ((ddi_copyin((caddr_t)arg, (caddr_t)&res,
			sizeof (struct mdi_set_resolution), 0)) != 0) {
			error = EFAULT;
			break;
		}

		/*
		 * Need to check for any outstanding mappings
		 * and exit if found
		 */
		mutex_enter(&softc->mdi_mutex);

		ASSERT(softc->mdi_maps_outstand >= 0);
		if (softc->mdi_maps_outstand != 0) {
			mutex_exit(&softc->mdi_mutex);
			error = EBUSY;
			break;
		}

		error = mdi_set_res(softc, &res);
		mutex_exit(&softc->mdi_mutex);

	}
	break;

	case MDI_SET_PIXELMODE:

		/*
		 * Select the mode in which the MDI interprets input data.
		 */
		mutex_enter(&softc->mdi_mutex);
		error = mdi_set_pixelmode(softc, (int)arg);
		mutex_exit(&softc->mdi_mutex);
		break;

	case MDI_DEGAMMA:
		softc->mdi_gammaflag = (int)arg;
		break;

	case MDI_GET_GAMMALUT:
	case MDI_SET_GAMMALUT: {
		struct mdi_set_gammalut glut;

		if (copyin((caddr_t)arg, (caddr_t)&glut,
			sizeof (struct mdi_set_gammalut))
			!= 0) {
			error = EFAULT;
			break;
		}
		error = mdi_gamma_ops(softc, &glut, cmd);
	}
	break;

	case MDI_GET_DEGAMMALUT:
	case MDI_SET_DEGAMMALUT: {
		struct mdi_set_degammalut dglut;

		if (copyin((caddr_t)arg, (caddr_t)&dglut,
			sizeof (struct mdi_set_degammalut)) != 0) {
			error = EFAULT;
			break;
		}
		error = mdi_degamma_ops(softc, &dglut, cmd);
	}
	break;

	case MDI_SET_PPR:

		mutex_enter(&softc->mdi_mutex);
		error = mdi_ppr_setup(softc, (int)arg);
		mutex_exit(&softc->mdi_mutex);

		break;

	case MDI_SET_COUNTERS: {
		struct mdi_set_counters counters;

		if (copyin((caddr_t)arg, (caddr_t)&counters,
			sizeof (struct mdi_set_counters)) != 0) {
			error = EFAULT;
			break;
		}

		/*
		 * Program MDI horizontal, vertical, composite etc.
		 * blank start, blank clear etc. counters.
		 */

		mutex_enter(&softc->mdi_mutex);
		error = mdi_setcounters(softc, &counters);
		mutex_exit(&softc->mdi_mutex);
	}
	break;

	case MDI_VRT_CNTL: 	/* Enable/Disable vertical retrace interrupt */

		mutex_enter(&softc->mdi_mutex);

		switch (arg) {
		case MDI_ENABLE_VIRQ:
			mdi_set_vint(mdi, TRUE);
			break;
		case MDI_DISABLE_VIRQ:
			mdi_set_vint(mdi, FALSE);
			break;
		default:
			error = EINVAL;
			break;
		}

		mutex_exit(&softc->mdi_mutex);
		break;

	case MDI_SET_CLUT: {
		struct mdi_set_clut clut;
		int lut = 0;

		if (copyin((caddr_t)arg, (caddr_t)&clut,
					sizeof (struct mdi_set_clut)) != 0) {
			error = EFAULT;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		vrt_state = (u_int)mdi_get_vint(mdi);
		mdi_set_vint(mdi, FALSE);

		mutex_exit(&softc->mdi_mutex);

		switch (clut.lut) {
		case MDI_CLUT1:
			lut = 0;
			break;
		case MDI_CLUT2:
			lut = 1;
			break;
		case MDI_CLUT3:
			lut = 2;
			break;
		default:
			error = EINVAL;
			break;
		}

		if (error)
			break;

		error = mdi_set_shdw_clut(softc, (void *)&clut,
			MDI_UPDATE_ALPHA, lut, 0, MDI_ACCESS_CLEARTXT);

		mutex_enter(&softc->mdi_mutex);
		if (error)
			mdi_set_vint(mdi, vrt_state);
		else
			/*
			 *  Enable vertical retrace interrupts
			 *  so that we "push out" the new data
			 *  in the interrupt routine.
			 */
			mdi_set_vint(mdi, TRUE);

		mutex_exit(&softc->mdi_mutex);

	}
	break;

	case MDI_GET_CLUT: {
		struct mdi_set_clut clut;
		int lut = 0;

		/* copying just to get the index and count */
		if (copyin((caddr_t)arg, (caddr_t)&clut,
			sizeof (struct mdi_set_clut)) != 0) {
			error = EFAULT;
			break;
		}

		switch (clut.lut) {
		case MDI_CLUT1:
			lut = 0;
			break;
		case MDI_CLUT2:
			lut = 1;
			break;
		case MDI_CLUT3:
			lut = 2;
			break;
		default:
			error = EINVAL;
			break;
		}

		if (error)
			break;

		error = mdi_get_shdw_clut(softc, (void *)&clut,
			MDI_UPDATE_ALPHA, lut, 0, MDI_ACCESS_CLEARTXT);

	}
	break;
	/*
	 * Get/Set the X look up tables.
	 */
	case MDI_SET_XLUT: {
		struct mdi_set_xlut xlut;

		if (copyin((caddr_t)arg, (caddr_t)&xlut,
			sizeof (struct mdi_set_xlut)) != 0) {
			error = EFAULT;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		vrt_state = (u_int)mdi_get_vint(mdi);
		mdi_set_vint(mdi, FALSE);

		mutex_exit(&softc->mdi_mutex);

		error = mdi_set_shdw_xlut(softc, &xlut);

		mutex_enter(&softc->mdi_mutex);
		if (error)
			mdi_set_vint(mdi, vrt_state);
		else
			/*
			 *  Enable vertical retrace interrupts
			 *  so that we "push out" the new data
			 *  in the interrupt routine.
			 */
			mdi_set_vint(mdi, TRUE);

		mutex_exit(&softc->mdi_mutex);

	}
	break;

	case MDI_GET_XLUT: {
		struct mdi_set_xlut xlut;

		if (copyin((caddr_t)arg, (caddr_t)&xlut,
			sizeof (struct mdi_set_xlut)) != 0) {
			error = EFAULT;
			break;
		}

		error = mdi_get_shdw_xlut(softc, (caddr_t)&xlut);

	}
	break;

	case MDI_SET_CURSOR: {
		struct mdi_cursor_info curs_info;

		if (copyin((caddr_t)arg, (caddr_t)&curs_info,
		    sizeof (struct mdi_cursor_info)) != 0)
			error = EFAULT;
		else
			error = mdi_set_cursor(softc, &curs_info);

	}
	break;

	case VIS_GETIDENTIFIER: {

		if (ddi_copyout((caddr_t)&cg14name, (caddr_t)arg,
			sizeof (cg14name), mode) != 0)
			error = EFAULT;

	}
	break;


	case SET_MONITOR_POWER: {
		if (arg < 0 || arg > 1)
			error = EINVAL;
		else {
			if (arg == 0)
				error = mdi_monitor_down(softc);
			else
				error = mdi_monitor_up(softc);
		}
	}
	break;

	default:
		error = mdi_fbiocmds(dev, cmd, mode, (int)arg);
		break;
	}
	if (error)
		mdi_printf(CE_NOTE, "ioctl 0x%x returning errcode %d\n",
			cmd, error);
	return (error);
}

/*
 *  The PROM init's the cluts to linear, so when the last proc (the Openwin
 *  server closes the MDI device, we should remap the cluts and the RAMDAC
 *  to linear.
 */
static void
mdi_linear_cluts(softc)
struct mdi_softc *softc;
{
	int i, j;

	struct mdi_shadow_luts *shdw_clutp = softc->mdi_shdw_lut;

	for (j = 0; j < softc->mdi_ncluts; j++) {
		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			shdw_clutp->s_clut[j].mdi_alpha[i] = (u_char)0;
			shdw_clutp->s_clut[j].mdi_red[i] = (u_char)i;
			shdw_clutp->s_clut[j].mdi_green[i] = (u_char)i;
			shdw_clutp->s_clut[j].mdi_blue[i] = (u_char)i;
		}
		shdw_clutp->s_update_flag |= mdi_acl[j];
	}

	/* now the gamma table */
	for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
		shdw_clutp->s_glut.red[i] = (u_short)(i << 2);
		shdw_clutp->s_glut.green[i] = (u_short)(i << 2);
		shdw_clutp->s_glut.blue[i] = (u_short)(i << 2);
	}
	shdw_clutp->s_update_flag |= MDI_GAMMALUT;
}

static int
mdi_gamma_ops(struct mdi_softc *softc, struct mdi_set_gammalut *glut, int cmd)
{
	struct mdi_shadow_luts *shdw_lut = softc->mdi_shdw_lut;
	int usr_index, usr_count;
	u_short *usr_red, *usr_green, *usr_blue;
	struct mdi_gammalut_data *stack_glut;
	int error = 0;

	usr_index = glut->index;
	usr_count = glut->count;

	if (usr_count == 0)
		return (0);

	usr_red = glut->red;
	usr_green = glut->green;
	usr_blue = glut->blue;

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
	    (usr_index + usr_count > MDI_CMAP_ENTRIES))
		return (EINVAL);

	stack_glut = kmem_alloc(sizeof (struct mdi_gammalut_data), KM_SLEEP);

	switch (cmd) {
	case MDI_SET_GAMMALUT:
		if (ddi_copyin((caddr_t)usr_red + usr_index,
		    (caddr_t)stack_glut->red + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		if (ddi_copyin((caddr_t)usr_green + usr_index,
		    (caddr_t)stack_glut->green + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		if (ddi_copyin((caddr_t)usr_blue + usr_index,
		    (caddr_t)stack_glut->blue + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		/* Save index and count into shdw lut */
		mutex_enter(&softc->mdi_mutex);
		shdw_lut->s_glut.index = usr_index;
		shdw_lut->s_glut.count = usr_count;

		/* copy from stack to global area */
		bcopy((caddr_t)stack_glut->red + usr_index,
		    (caddr_t)shdw_lut->s_glut.red + usr_index,
		    usr_count * sizeof (u_short));
		bcopy((caddr_t)stack_glut->green + usr_index,
		    (caddr_t)shdw_lut->s_glut.green + usr_index,
		    usr_count * sizeof (u_short));
		bcopy((caddr_t)stack_glut->blue + usr_index,
		    (caddr_t)shdw_lut->s_glut.blue + usr_index,
		    usr_count * sizeof (u_short));

		if (error == 0)
			shdw_lut->s_update_flag |= MDI_GAMMALUT;

		mutex_exit(&softc->mdi_mutex);

		break;

	case MDI_GET_GAMMALUT:
		mutex_enter(&softc->mdi_mutex);

		/* copy from global to stack area */
		bcopy((caddr_t)shdw_lut->s_glut.red + usr_index,
		    (caddr_t)stack_glut->red + usr_index,
		    usr_count * sizeof (u_short));
		bcopy((caddr_t)shdw_lut->s_glut.green + usr_index,
		    (caddr_t)stack_glut->green + usr_index,
		    usr_count * sizeof (u_short));
		bcopy((caddr_t)shdw_lut->s_glut.blue + usr_index,
		    (caddr_t)stack_glut->blue + usr_index,
		    usr_count * sizeof (u_short));
		mutex_exit(&softc->mdi_mutex);

		if (ddi_copyout((caddr_t)stack_glut->red + usr_index,
		    (caddr_t)usr_red + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		if (ddi_copyout((caddr_t)stack_glut->green + usr_index,
		    (caddr_t)usr_green + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		if (ddi_copyout((caddr_t)stack_glut->blue + usr_index,
		    (caddr_t)usr_blue + usr_index,
		    usr_count * sizeof (u_short), 0) != 0) {
			error = EFAULT;
			break;
		}

		break;
	}

	kmem_free(stack_glut, sizeof (struct mdi_gammalut_data));
	return (error);
}

static int
mdi_degamma_ops(struct mdi_softc *softc, struct mdi_set_degammalut *dglut,
		int cmd)
{
	int usr_index, usr_count;
	u_char *usr_degamma;
	u_char *stack_degamma;
	int i;
	struct mdi_set_clut *clut;
	u_char *space;
	u_char *red, *green, *blue, *alpha;
	int error = 0;

	usr_index = dglut->index;
	usr_count = dglut->count;

	if (usr_count == 0)
		return (0);

	usr_degamma = dglut->degamma;

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
	    (usr_index + usr_count > MDI_CMAP_ENTRIES))
		return (EINVAL);

	stack_degamma = kmem_alloc(MDI_CMAP_ENTRIES * sizeof (u_char),
		KM_SLEEP);

	switch (cmd) {
	case MDI_SET_DEGAMMALUT:
		if (ddi_copyin((caddr_t)usr_degamma + usr_index,
		    (caddr_t)stack_degamma + usr_index,
		    usr_count * sizeof (stack_degamma[0]), 0) != 0) {
			error = EFAULT;
			goto failed;
		}

		mutex_enter(&softc->mdi_degammalock);
		bcopy((caddr_t)stack_degamma + usr_index,
			(caddr_t)softc->mdi_degamma->degamma + usr_index,
			usr_count * sizeof (stack_degamma[0]));
		mutex_exit(&softc->mdi_degammalock);

		/*
		 *  Now we need to
		 *  1  read the cleartext table
		 *  2  index thru the degamma table
		 *  3  re-write the h/w shadow table (not the cleartxt)
		 *  4  update the h/w
		 *  5  Do this for both cluts
		 *  We assume that the cleartxt table has
		 *  already been set up so that we can
		 *  get back to the original clut data.
		 */

		clut = (struct mdi_set_clut *)kmem_zalloc(
			sizeof (struct mdi_set_clut), KM_SLEEP);

		space = kmem_alloc(MDI_CMAP_ENTRIES * 4 *
			sizeof (u_char), KM_NOSLEEP);
		red = space;
		green = &space[MDI_CMAP_ENTRIES];
		blue =  &space[MDI_CMAP_ENTRIES * 2];
		alpha = &space[MDI_CMAP_ENTRIES * 3];

		if (clut == NULL || red == NULL || green == NULL ||
					blue == NULL)
			return (ENOMEM);

		for (i = 0; i < softc->mdi_ncluts; i++) {

			clut->lut = mdi_acl[i];
			clut->index = 0;
			clut->count = MDI_CMAP_ENTRIES;
			clut->alpha = alpha;
			clut->red = red;
			clut->green = green;
			clut->blue = blue;

			if ((error = mdi_get_shdw_clut(softc,
				(void *)clut, MDI_UPDATE_ALPHA,
				i, FKIOCTL, MDI_ACCESS_CLEARTXT)) != 0)

				break;

			clut->lut = mdi_acl[i];
			clut->index = 0;
			clut->count = MDI_CMAP_ENTRIES;
			if ((error = mdi_set_shdw_clut(softc,
				(void *)clut, MDI_UPDATE_ALPHA,
				i, FKIOCTL, MDI_NOACCESS_CLEARTXT)) != 0)

				break;
		}

		kmem_free(clut, sizeof (struct mdi_set_clut));
		kmem_free(space, MDI_CMAP_ENTRIES * 4 * sizeof (u_char));

		break;

	case MDI_GET_DEGAMMALUT:
		mutex_enter(&softc->mdi_mutex);
		bcopy((caddr_t)softc->mdi_degamma->degamma + usr_index,
		    (caddr_t)stack_degamma + usr_index,
		    usr_count * sizeof (stack_degamma[0]));
		mutex_exit(&softc->mdi_mutex);

		if (ddi_copyout((caddr_t)stack_degamma + usr_index,
		    (caddr_t)usr_degamma + usr_index,
		    usr_count * sizeof (stack_degamma[0]), 0) != 0) {
			error = EINVAL;
			goto failed;
		}
		break;
	}

	error = 0;
failed:
	kmem_free(stack_degamma, MDI_CMAP_ENTRIES * sizeof (u_char));
	return (error);
}

/*
 */
static int
mdi_set_cursor(struct mdi_softc *softc, struct mdi_cursor_info *curs_info)
{
	int i;
	struct mdi_cursor_address *cursor;
	u_long tmp[MDI_CURS_SIZE];

	cursor = (struct mdi_cursor_address *)softc->mdi_cursp;

	mutex_enter(&softc->mdi_mutex);

	/* use non-autoinc for now */
	for (i = 0; i < MDI_CURS_SIZE; i++) {
		cursor->curs_cpl0[i] = curs_info->curs_enable0[i];
		cursor->curs_cpl1[i] = 0;
	}

	bzero((caddr_t)tmp, sizeof (tmp));
	mdi_verify_cursor(softc, (u_long *)curs_info->curs_enable0, tmp,
		(u_long) ~0);

	cursor->curs_ccr = curs_info->curs_ctl;
	cursor->curs_xcu = curs_info->curs_xpos;
	cursor->curs_ycu = curs_info->curs_ypos;

	mutex_exit(&softc->mdi_mutex);

	return (0);

}

/*
 * Routine to implement the fbio commands to support the cgthree emulation
 * mode.
 */
static int
mdi_fbiocmds(dev_t dev, int cmd, int mode, int arg)
{
	volatile struct mdi_register_address *mdi;
	/* XXX Don't really need to do this...just pass the softc */
	struct mdi_softc *softc = getsoftc(minor(dev));
	int error = 0;
	int vrt_state, video_state;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	switch (cmd) {

	/*
	 * For the cgthree emulation mode commands always return the type
	 * of the frame buffer to be that of a cgthree. The real device
	 * type returned will always be MDI.
	 */
	case FBIOGATTR: {
		struct fbgattr gattr;

		mdi_printf(CE_CONT, "FBIOGATTR\n");

		mutex_enter(&softc->mdi_mutex);

		bcopy((caddr_t)&mdi_cg3_attr, (caddr_t)&gattr, sizeof (gattr));

		gattr.fbtype.fb_type = softc->mdi_emu_type;
		gattr.fbtype.fb_height = softc->mdi_mon_height;
		gattr.fbtype.fb_width = softc->mdi_mon_width;
		gattr.fbtype.fb_size = softc->mdi_mon_size;

		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR)
			gattr.fbtype.fb_depth = 8;
		else
			gattr.fbtype.fb_depth = softc->mdi_pixel_depth;

		gattr.sattr.emu_type = softc->mdi_emu_type;

		mutex_exit(&softc->mdi_mutex);

		if (ddi_copyout((caddr_t)&gattr, (caddr_t)arg,
				sizeof (gattr), mode) != 0) {
			error = EFAULT;
			break;
		}
	}
	break;

	case FBIOGTYPE: {
		struct fbtype fbt;

		mdi_printf(CE_CONT, "FBIOGTYPE\n");

		mutex_enter(&softc->mdi_mutex);

		bcopy((caddr_t)&mdi_cg3_attr.fbtype, (caddr_t)&fbt,
			sizeof (fbt));

		fbt.fb_type = FBTYPE_SUN3COLOR;
		fbt.fb_height = softc->mdi_mon_height;
		fbt.fb_width = softc->mdi_mon_width;
		fbt.fb_size = softc->mdi_mon_size;
		fbt.fb_depth = MDI_DEFAULT_DEPTH;
		fbt.fb_cmsize = MDI_CMAP_ENTRIES;

		mutex_exit(&softc->mdi_mutex);

		if (ddi_copyout((caddr_t)&fbt, (caddr_t)arg,
				sizeof (fbt), mode) != 0)
			error = EFAULT;

	}
	break;

	case FBIOSATTR: {
		/*
		 *  OWV3 and other non-MDI cognizant apps will NOT call
		 *  this ioctl.  But for those that do, this is their
		 *  chance to override the emulation type which is set
		 *  to MDI_SUN3COLOR by default.
		 */
		struct fbsattr sattr;

		mdi_printf(CE_CONT, "FBIOSATTR\n");
		if (ddi_copyin((caddr_t)arg, (caddr_t)&sattr,
			sizeof (struct fbsattr), mode) != 0) {
			error = EFAULT;
			break;
		}

		switch (sattr.emu_type) {
		case FBTYPE_SUN3COLOR:
		case FBTYPE_MDICOLOR:

			mutex_enter(&softc->mdi_mutex);
			softc->mdi_emu_type = sattr.emu_type;
			mutex_exit(&softc->mdi_mutex);
			break;
		case -1:
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	break;

	case FBIOPUTCMAP: {
		struct fbcmap cmap;

		/*
		 * Invoke the common (used by FBIOPUTCMAP and MDI_SET_CLUT)
		 * commands) shadow color map update routine. The flag
		 * MDI_NOUPDATE_ALPHA indicates that the alpha value of the
		 * color look up table should be updated with the default
		 * value of 0xff. This command updates only color look up
		 * table 1
		 */
		mdi_printf(CE_CONT, "FBIOPUTCMAP\n");

		if (ddi_copyin((caddr_t)arg, (caddr_t)&cmap,
			sizeof (cmap), mode) != 0) {
			error = EFAULT;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		vrt_state = (u_int)mdi_get_vint(mdi);
		mdi_set_vint(mdi, FALSE);

		mutex_exit(&softc->mdi_mutex);

		/* Only use MDI_CLUT1 for this ioctl */
		error = mdi_set_shdw_clut(softc, (void *)&cmap,
			MDI_NOUPDATE_ALPHA, 0 /* MDI_CLUT1 */, mode,
			MDI_ACCESS_CLEARTXT);

		mutex_enter(&softc->mdi_mutex);
		if (error)
			mdi_set_vint(mdi, vrt_state);
		else
			/* Enable vertical retrace interrupts */
			mdi_set_vint(mdi, TRUE);

		mutex_exit(&softc->mdi_mutex);
	}
	break;

	case FBIOGETCMAP: {
		struct fbcmap cmap;
		mdi_printf(CE_CONT, "FBIOGETCMAP\n");

		/* copying just to get index and count */
		if (ddi_copyin((caddr_t)arg, (caddr_t)&cmap,
			sizeof (cmap), mode) != 0) {
			error = EFAULT;
			break;
		}

		error = mdi_get_shdw_clut(softc, (void *)&cmap,
			MDI_NOUPDATE_ALPHA, 0 /* MDI_CLUT1 */, mode,
			MDI_ACCESS_CLEARTXT);

	}
	break;

	/*
	 * HW cursor control. Cursor is manipulated from two interfaces
	 * a) via kernel Sunwindows and b) directly by mapping the MDI
	 * cursor control page. These commands cannot be used to set the
	 * alpha blend proportions in the cursor color registers.
	 * These commands affect only the hardware cursor, not the full
	 * screen cursor.
	 */

	case FBIOSCURSOR: {
		struct fbcursor cp;

		mdi_printf(CE_CONT, "FBIOSCURSOR\n");

		/* If we are emulating CG3, then NO hw cursor is needed */
		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR) {
			error = ENOTTY;
			break;
		}

		if (ddi_copyin((caddr_t)arg, (caddr_t)&cp,
				sizeof (struct fbcursor), mode) != 0) {
			error = EFAULT;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		if (cp.set & FB_CUR_SETCUR)	/* Enable/Disable HW cursor */
			softc->mdi_cur.enable = cp.enable;

		if (cp.set & FB_CUR_SETHOT) {	/* Set cursor hot spot */
			softc->mdi_cur.hot.x = cp.hot.x & MDI_CURS_XMASK;
			softc->mdi_cur.hot.y = cp.hot.y & MDI_CURS_YMASK;
		}

		/*
		 * update hardware to reflect the cursor enable/disable and
		 * position.
		 */
		if (cp.set & FB_CUR_SETPOS)	/* Set cursor position */
			softc->mdi_cur.pos = cp.pos;

		mdi_setcurpos(softc, &cp.pos);

		mutex_exit(&softc->mdi_mutex);

		/*
		 * Set cursor shape. Make sure we are within the bounds of
		 * a 32x32 bit cursor.
		 */
		if (cp.set & FB_CUR_SETSHAPE)
			if ((error = mdi_set_curshape(softc, &cp, mode)) != 0)
				break;


		/*
		 * load cursor colormap
		 */
		if (cp.set & FB_CUR_SETCMAP) {

			mutex_enter(&softc->mdi_mutex);
			vrt_state = (u_int)mdi_get_vint(mdi);
			mdi_set_vint(mdi, FALSE);
			mutex_exit(&softc->mdi_mutex);

			error = mdi_set_shdw_curcmap(softc,
				(struct fbcmap *)&cp.cmap, mode);

			mutex_enter(&softc->mdi_mutex);
			if (error)
				mdi_set_vint(mdi, vrt_state);
			else
				/* Enable vertical retrace interrupts */
				mdi_set_vint(mdi, TRUE);
			mutex_exit(&softc->mdi_mutex);
		}

	}
	break;

	case FBIOGCURSOR: {
		struct fbcursor fbc;

		mdi_printf(CE_CONT, "FBIOGCURSOR\n");

		if (ddi_copyin((caddr_t)arg, (caddr_t)&fbc,
				sizeof (fbc), mode) != 0) {
			error = EFAULT;
			break;
		}

		/* If we are emulating CG3, then NO hw cursor is needed */
		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR) {
			error = ENOTTY;
			break;
		}

		if (error = mdi_get_shdw_curcmap(softc, &fbc, mode))
			break;

		if (ddi_copyout((caddr_t)&fbc, (caddr_t)arg,
			sizeof (struct fbcursor), mode) != 0)
				error = EFAULT;

	}
	break;

	case FBIOSCURPOS: {
		struct fbcurpos pos;

		mdi_printf(CE_CONT, "FBIOSCURSPOS\n");

		if (ddi_copyin((caddr_t)arg, (caddr_t)&pos, sizeof (pos),
						mode) != 0) {
			error = EFAULT;
			break;
		}

		/* If we are emulating CG3, then NO hw cursor is needed */
		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR) {
			error = ENOTTY;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		/*  If curs not enabled, we're wasting everyone's time */
		if (softc->mdi_cur.enable)
			mdi_setcurpos(softc, &pos);
		else
			error = ENXIO;

		mutex_exit(&softc->mdi_mutex);

	}
	break;

	case FBIOGCURPOS:
		mdi_printf(CE_CONT, "FBIOGCURSPOS\n");

		/* If we are emulating CG3, then NO hw cursor is needed */
		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR) {
			error = ENOTTY;
			break;
		}

		if (!softc->mdi_cur.enable) {
			error = ENXIO;
			break;
		}

		if (ddi_copyout((caddr_t)&softc->mdi_cur.pos, (caddr_t)arg,
					sizeof (struct fbcurpos), mode) != 0)
			error = EFAULT;

		break;

	case FBIOGCURMAX: {
		static struct fbcurpos curmax = {MDI_CURS_SIZE, MDI_CURS_SIZE};

		mdi_printf(CE_CONT, "FBIOGCURMAX\n");

		/* If we are emulating CG3, then NO hw cursor is needed */
		if (softc->mdi_emu_type == FBTYPE_SUN3COLOR) {
			error = ENOTTY;
			break;
		}

		if (ddi_copyout((caddr_t)&curmax, (caddr_t)arg,
				sizeof (struct fbcurpos), mode) != 0)
			error = EFAULT;

	}
	break;

	case FBIOVERTICAL:

		/*
		 * Wait until the next vertical retrace interrupt.
		 */
		mdi_printf(CE_CONT, "FBIOVERTICAL\n");

		mutex_enter(&softc->mdi_mutex);
		/*
		 *  With video off, this ioctl makes no sense.
		 */
		video_state = mdi_get_video(mdi);

		if (!video_state) {
			error = ENXIO;
			mutex_exit(&softc->mdi_mutex);
			break;
		}

		softc->mdi_vrtflag |= MDI_VRT_WAKEUP;

		vrt_state = (u_int)mdi_get_vint(mdi);

		/* Enable vertical interrupts */
		mdi_set_vint(mdi, TRUE);

		while (softc->mdi_vrtflag & MDI_VRT_WAKEUP)
			cv_wait(&softc->mdi_cv, &softc->mdi_mutex);

		mdi_set_vint(mdi, vrt_state);
		mutex_exit(&softc->mdi_mutex);

		break;

	case FBIOSVIDEO: {
		int venable;

		mdi_printf(CE_CONT, "FBIOSVIDEO\n");
		if (ddi_copyin((caddr_t)arg, (caddr_t)&venable,
					sizeof (int), mode) != 0) {
			error = EFAULT;
			break;
		}

		mutex_enter(&softc->mdi_mutex);

		if (mdi->m_rsr.revision != 0) {
			mdi_set_blanking(mdi, venable);

			/* We should always have video on for VSIMM 2 */
			if (mdi_get_video(mdi) == 0)
				mdi_set_video(mdi, TRUE);

		} else {

			video_state = mdi_get_video(mdi);

			switch (venable) {
			case FBVIDEO_ON:
				if (!video_state) {
					mdi_stop_vcntr(mdi);
					mdi_start_vcntr(mdi);
					mdi_set_video(mdi, TRUE);
				}
			/* else do nothing cuz currentstate = desired */

				break;

			case FBVIDEO_OFF:
				if (video_state) {
					softc->mdi_update |= MDI_VIDEO_UPDATE;
					vrt_state = mdi_get_vint(mdi);
					mdi_set_vint(mdi, TRUE);
					while (softc->mdi_update &
							MDI_VIDEO_UPDATE)
						cv_wait(&softc->mdi_cv,
							&softc->mdi_mutex);
					mdi_set_vint(mdi, vrt_state);
				}
			/* else do nothing cuz currentstate = desired */

				break;

			default:
				error = EINVAL;
				break;
			}
		}
		mutex_exit(&softc->mdi_mutex);

	}
	break;

	case FBIOGVIDEO: {
		int status;
		int venable;

		mdi_printf(CE_CONT, "FBIOGVIDEO\n");

		mutex_enter(&softc->mdi_mutex);
		if (mdi->m_rsr.revision != 0)
			status = mdi_get_blanking(mdi);
		else
			status = mdi_get_video(mdi);

		if (status)
			venable = FBVIDEO_ON;
		else
			venable = FBVIDEO_OFF;

		mutex_exit(&softc->mdi_mutex);

		if (ddi_copyout((caddr_t)&venable, (caddr_t)arg,
					sizeof (int), mode) != 0)
			error = EFAULT;
	}
	break;


	case FBIOGPIXRECT: {
		struct fbpixrect fb_pr;

		mdi_printf(CE_CONT, "FBIOGPIXRECT\n");

		mutex_enter(&softc->pixrect_mutex);

		fb_pr.fbpr_pixrect = &softc->mdi_pr;

		/*
		 * initialize pixrect and private data
		 */
		softc->mdi_pr.pr_ops = &mdi_pixrect_ops;

		/*
		 * pr_size set in attach, set the depth value here
		 */
		softc->mdi_pr.pr_depth = 8;
		softc->mdi_pr.pr_data = (caddr_t)&softc->mdi_prd;

		/* md_linebytes, md_image set in attach */
		/* md_offset already zero */
		softc->mdi_prd.mpr.md_primary = getminor(dev);
		softc->mdi_prd.mpr.md_flags = MP_DISPLAY | MP_PLANEMASK;
		softc->mdi_prd.planes = 255;

		/*
		 * enable video
		 */
		mdi_stop_vcntr(mdi);
		mdi_start_vcntr(mdi);
		mdi_set_video(mdi, TRUE);

		mutex_exit(&softc->pixrect_mutex);

		if (ddi_copyout((caddr_t)&fb_pr, (caddr_t)arg,
			sizeof (struct fbpixrect), mode) != 0) {
			error = EFAULT;
			break;
		}

	}
	break;

	case FBIOPUTCMAPI: {
		struct fbcmap_i fbci;
		struct fbcmap fbc;

		if (ddi_copyin((caddr_t)arg, (caddr_t)&fbci,
			sizeof (struct fbcmap_i), mode) != 0) {
			error = EFAULT;
			break;
		}

		fbc.index = fbci.index;
		fbc.count = fbci.count;
		fbc.red = fbci.red;
		fbc.green = fbci.green;
		fbc.blue = fbci.blue;

		error = mdi_set_shdw_clut(softc, (void *)&fbc,
			MDI_NOUPDATE_ALPHA, fbci.id, mode,
			MDI_ACCESS_CLEARTXT);

		if (fbci.flags & FB_CMAP_BLOCK) {

			mutex_enter(&softc->mdi_mutex);
			while (softc->mdi_shdw_lut->s_update_flag &
			    (MDI_CLUT1 | MDI_CLUT2 | MDI_CLUT3))
				cv_wait(&softc->mdi_cv, &softc->mdi_mutex);
			mutex_exit(&softc->mdi_mutex);
		}

	}
	break;

	case FBIOGETCMAPI: {
		struct fbcmap_i fbci;
		struct fbcmap fbc;

		/* copying just to get index and count */
		if (ddi_copyin((caddr_t)arg, (caddr_t)&fbci,
			sizeof (struct fbcmap_i), mode) != 0) {
			error = EFAULT;
			break;
		}

		fbc.index = fbci.index;
		fbc.count = fbci.count;
		fbc.red = fbci.red;
		fbc.green = fbci.green;
		fbc.blue = fbci.blue;

		error = mdi_get_shdw_clut(softc, (void *)&fbc,
			MDI_NOUPDATE_ALPHA, fbci.id, mode,
			MDI_ACCESS_CLEARTXT);

	}
	break;

	case FBIOVRTOFFSET: {
		int cookie = MDI_SHDW_VRT_MAP;

		if (ddi_copyout((caddr_t)&cookie, (caddr_t)arg,
						sizeof (cookie), mode) != 0)
			error = EFAULT;
	}
	break;

	default:
		error =  ENOTTY;
		break;
	}

	return (error);
}

/*
 * MDI segment driver routines
 */

/* XXX use a new mutex for each of the routines below */

static	int segmdi_create(struct seg *seg, void *argsp);
static	int segmdi_dup(struct seg *seg, struct seg *newsep);
static	int segmdi_unmap(struct seg *seg, caddr_t addr, size_t len);
static	void segmdi_free(struct seg *seg);
static	faultcode_t segmdi_fault(struct hat *hat, struct seg *seg,
		caddr_t addr, size_t len, enum fault_type type,
		enum seg_rw rw);
static	faultcode_t segmdi_nofault(struct seg *seg, caddr_t addr);
static	int segmdi_setprot(struct seg *seg, caddr_t addr, size_t len,
		u_int prot);
static	int segmdi_checkprot(struct seg *seg, caddr_t addr, size_t len,
		u_int prot);
static	int segmdi_kluster(struct seg *seg, caddr_t addr, ssize_t delta);
static	size_t segmdi_incore(struct seg *seg, caddr_t addr, size_t len,
		char *vec);
static	int segmdi_getprot(struct seg *seg, caddr_t addr, size_t len,
		u_int *protv);
static u_offset_t segmdi_getoffset(struct seg *seg, caddr_t addr);
static	int segmdi_nop();
static size_t segmdi_swapout(struct seg *seg);
static void segmdi_dump(struct seg *seg);
static int segmdi_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static int segmdi_gettype(struct seg *seg, caddr_t addr);
static struct mdi_data *sdpmdi_create(struct mdi_data *old,
		struct mdi_softc *softc);
static int segmdi_pagelock(struct seg *seg, caddr_t addr, size_t len,
		struct page ***ppp, enum lock_type type, enum seg_rw rw);
static int segmdi_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp);

struct	seg_ops segmdi_ops =  {

	segmdi_dup,	/* dup */
	segmdi_unmap,	/* unmap */
	segmdi_free,	/* free */
	segmdi_fault,	/* fault */
	segmdi_nofault,	/* asynchronous faults are not supported */
	segmdi_setprot,	/* setprot */
	segmdi_checkprot,	/* checkprot */
	segmdi_kluster,	/* kluster */
	segmdi_swapout,	/* swapout */
	segmdi_nop,	/* sync */
	segmdi_incore,	/* incore */
	segmdi_nop,	/* lock_op */
	segmdi_getprot,	/* getprot? */
	segmdi_getoffset,	/* getoffset? */
	segmdi_gettype,	/* gettype */
	segmdi_getvp,	/* getvp */
	segmdi_nop,	/* advise */
	segmdi_dump,	/* dump */
	segmdi_pagelock,	/* pagelock */
	segmdi_getmemid,	/* getmemid */
};

/*ARGSUSED*/
static void
segmdi_dump(struct seg *seg)
{
	/* Empy function. */
}

/*ARGSUSED*/
static int
segmdi_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

static int
segmdi_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;
	/*
	 * It looks as if it always mapped shared
	 */
	memidp->val[0] = (u_longlong_t)sdp->vp;
	memidp->val[1] = (u_longlong_t)(addr - seg->s_base);
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segmdi_getoffset(struct seg *seg, caddr_t addr)
{
	return ((u_offset_t)0);
}

/*ARGSUSED*/
static int
segmdi_gettype(seg, addr)
	struct seg *seg;
	caddr_t addr;
{
	return (MAP_SHARED);
}

/*ARGSUSED*/
static size_t
segmdi_swapout(struct seg *seg)
{
	return ((size_t)0);
}

/*ARGSUSED*/
static int
segmdi_getvp(register struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	*vpp = sdp->vp;

	return (0);

}

/*ARGSUSED*/
static int
mdi_mmap(dev_t dev, off_t offset, int prot)
{
	u_int objtype;
	u_int map_allowed,
	    prot_allowed,
	    len_allowed;
	u_int pfnum;


	/*
	 * Find out the type of object being mapped, the allowable protection,
	 * and the size (in bytes) of the object.
	 */
	if (mdi_getmapinfo(dev, offset, &objtype, &prot_allowed,
	    &map_allowed, &len_allowed, &pfnum) == -1)
		return (-1);

	if ((offset) < objtype + len_allowed)
		return (pfnum + btop(offset - objtype));
	else
		return (-1);
}

/*
 * Routine to provide mappings to MDI control address space, VRT counter, and
 * mappings to the VRAM (frame buffer) in different modes of access. This
 * routine checks for the validity of the requested mapping, creates a segment
 * for the requested mapping, initializes the segment private data field with
 * data specific to the object being mapped and returns a virtual address
 * corresponding to the mapping. The actual translation itself is set up
 * during fault handling.
 */

/*ARGSUSED*/
static int
mdi_segmap(
	dev_t dev,
	off_t offset,
	struct as *as,
	caddr_t *addr,
	off_t len,
	u_int prot,
	u_int maxprot,
	u_int flags,
	cred_t *cred)
{
	struct mdi_data seg_priv_data;
	u_int prot_allowed, maptype_allowed;
	u_int len_allowed, pfnum;
	u_int objtype;		/* Offset determines object to be mapped */
	int error = 0;
	extern void map_addr();

	/* Validate the range requested */
	if (mdi_getmapinfo(dev, offset, &objtype, &prot_allowed,
	    &maptype_allowed, &len_allowed, &pfnum) == -1)
		return (EACCES);

	/*
	 * Ensure that requested protections are OK
	 */
	if ((prot_allowed & prot) != prot)
		return (EACCES);

	/*
	 * Ensure that requested mapping type (shared vs private) is OK
	 */
	if ((maptype_allowed & (flags & MAP_TYPE)) != (flags & MAP_TYPE))
		return (EACCES);

	/*
	 * Ensure that requested length is OK.
	 */
	if (offset != 0)
		offset -= objtype; /* Get the actual offset into the device */

	/*
	 * Initialize the per-segment private data.
	 */
	seg_priv_data.dev = dev;
	seg_priv_data.maptype = flags & ~MAP_FIXED;
	seg_priv_data.objtype = objtype;
	seg_priv_data.prot = prot;
	seg_priv_data.maxprot = maxprot;
	seg_priv_data.offset = offset;

	len = roundup(len, PAGESIZE);

	if ((len + offset) > len_allowed)
		return (ENXIO);

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * Select a virtual address to enclose the user's mapping.
		 */
		(void) map_addr(addr, (size_t)len, (offset_t)0, 0, flags);
		if (*addr == NULL)
			return (ENOMEM);
		/* XXX - no as_rangeunlock! Okay? */
	} else {

		/*
		 * User specified address -
		 * Blow away any previous mappings.
		 */
		(void) as_unmap((struct as *)as, *addr, len);
	}

	error = as_map((struct as *)as, *addr, len, segmdi_create,
		&seg_priv_data);

	as_rangeunlock(as);

	AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	hat_devload(as->a_hat, *addr, len, pfnum, prot, HAT_LOAD);
	AS_LOCK_EXIT(as, &as->a_lock);

	return (error);
}

/*
 * Initialize segment segops and private data fields. The segment provides a
 * mapping to the MDI control address space, frame buffer.
 */

static int
segmdi_create(struct seg *seg, void *argsp)
{
	register struct mdi_softc *softc;
	int error;
	struct mdi_data *sdp;
	extern struct vnode *specfind(dev_t, vtype_t);

	ASSERT(seg != NULL);

	softc = getsoftc(getminor(((struct mdi_data *)argsp)->dev));

	sdp = sdpmdi_create((struct mdi_data *)argsp, softc);

	/*
	 *  Find common vnode -- we only want
	 *  character (VCHR) devices. Both the common and the shadow should
	 *  be held.  The shadow is held by specfind().
	 */
	sdp->vp = specfind(sdp->dev, VCHR);
	ASSERT(sdp->vp != NULL);

	seg->s_ops = &segmdi_ops;
	seg->s_data = (void *)sdp;

	error = VOP_ADDMAP(common_specvp(sdp->vp), (offset_t)sdp->offset,
		seg->s_as, seg->s_base, seg->s_size,
		sdp->prot, sdp->maxprot, MAP_SHARED, CRED());

	if (error != 0) {
		return (error);
	}

	mutex_enter(&softc->mdi_mutex);

	softc->mdi_maps_outstand += seg->s_size;

	if ((sdp->objtype >=  MDI_SHDW_VRT_MAP) &&
		(sdp->objtype <  MDI_CHUNKY_XBGR_MAP)) {

		/*
		 * Increment the number of mappings to the vertical retrace
		 * counter.
		 */
		softc->mdi_vrtmappers++;
		softc->mdi_vrtflag |= MDI_VRT_COUNTER;
	}
	mutex_exit(&softc->mdi_mutex);
	return (0);
}

/*
 * Duplicate seg and return new segment in newsegp.
 */
static int
segmdi_dup(struct seg *seg, struct seg *newseg)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;

	return (segmdi_create(newseg, (caddr_t)sdp));
}

/*
 * asynchronous fault is a no op. Fail silently.
 */
/*ARGSUSED*/
static faultcode_t
segmdi_nofault(
	struct seg *seg,
	caddr_t addr)
{
	return ((faultcode_t)0);
}

/*
 * Handle a fault for mappings to VRAM (framebuffer), MDI control address
 * space, shadow color look up tables, vertical retrace counter and the VRAM
 * (frame buffers).
 */
/*ARGSUSED*/
static faultcode_t
segmdi_fault(
	struct hat *hat,
	struct seg *seg,
	caddr_t addr,
	size_t len,
	enum fault_type type,
	enum seg_rw rw)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;
	u_int  prot;
	int pf;

	if (type != F_INVAL && type != F_SOFTLOCK && type != F_SOFTUNLOCK)
		return (FC_MAKE_ERR(EFAULT));

	/*
	 * The SOFTLOCK and SOFTUNLOCK operations are applied to the
	 * entire address range mapped by this segment because we assume
	 * that translations for cmem are set up using large page sizes
	 * of the SRMMU and that the SRMMU level 2 tables are always
	 * locked.  This assumption is OK until full support for multiple
	 * page sizes is available in the VM/MMU layers of SunOS.
	 */

	if (type == F_SOFTUNLOCK) {
		hat_unlock(hat, seg->s_base, seg->s_size);
		return (NULL);
	}

	switch (rw) {
	case S_READ:
		prot = PROT_READ;
		break;

	case S_WRITE:
		prot = PROT_WRITE;
		break;

	case S_EXEC:
		prot = PROT_EXEC;
		break;

	case S_OTHER:
	default:
		prot = PROT_READ | PROT_WRITE | PROT_EXEC;
		break;
	}

	mutex_enter(&sdp->segmdi_mutex);

	if ((sdp->prot & prot) == 0) {
		mutex_exit(&sdp->segmdi_mutex);
		return (FC_MAKE_ERR(EACCES));
	}

	len = roundup(len, PAGESIZE);

	/*
	 *  Validate the entire range of (addr, len)
	 */
	if ((pf = mdi_mmap(sdp->dev, sdp->objtype + sdp->offset, prot)) == -1) {
		mutex_exit(&sdp->segmdi_mutex);
		return (FC_MAKE_ERR(EFAULT));
	}

	/*
	 *  We want to load up the whole seg.
	 */
	hat_devload(hat, seg->s_base, seg->s_size, (u_int)pf, sdp->prot,
	    HAT_LOAD | ((type == F_SOFTLOCK) ? HAT_LOAD_LOCK : HAT_LOAD));

	mutex_exit(&sdp->segmdi_mutex);

	return (0);
}

static int
segmdi_unmap(struct seg *seg, caddr_t addr, size_t len)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;
	struct mdi_softc *softc = getsoftc(getminor(sdp->dev));

	mutex_enter(&sdp->segmdi_mutex);

	/*
	 * Check for bad sizes.
	 */
	if ((addr < seg->s_base) || (addr + len > seg->s_base + seg->s_size) ||
	    (len & PAGEOFFSET) || ((u_int)addr & PAGEOFFSET))
		cmn_err(CE_PANIC, "segmdi_unmap: bad size\n");

	/*
	 * Inform the vnode of the unmapping.
	 */
	ASSERT(sdp->vp != NULL);
	VOP_DELMAP(common_specvp(sdp->vp),
			(offset_t)sdp->offset + (addr - seg->s_base),
			seg->s_as, addr, len, sdp->prot, sdp->maxprot,
			MAP_SHARED, CRED());

	/* decrement our per-instance mapcnt */
	softc->mdi_maps_outstand -= len;

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */
	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	/*
	 *  For this segment driver, we enforce the policy of
	 *  NO PARTIAL UNMAPS.
	 */
	ASSERT((addr == seg->s_base) && (len == seg->s_size));
	mutex_exit(&sdp->segmdi_mutex);

	/* Won't need this mutex anymore */
	mutex_destroy(&sdp->segmdi_mutex);
	seg_free(seg);
	return (0);

#if PARTIAL_UNMAPS
	/*
	 * Check if we have to free the entire segment
	 */
	if ((addr == seg->s_base) && (len == seg->s_size)) {
		mutex_exit(&sdp->segmdi_mutex);

		/* Won't need this mutex anymore */
		mutex_destroy(&sdp->segmdi_mutex);
		seg_free(seg);
		return (0);
	}

	/*
	 * Check whether the address is at the begining of the segment
	 */
	if (addr == seg->s_base) {
		sdp->offset += len;
		seg->s_base += len;
		seg->s_size -= len;
		mutex_exit(&sdp->segmdi_mutex);
		return (0);
	}

	/*
	 * Check whether the range of addresses to be unmapped can
	 * fall out of the end of the segment.
	 */
	if ((addr + len) == (seg->s_base + seg->s_size)) {
		seg->s_size -= len;
		mutex_exit(&sdp->segmdi_mutex);
		return (0);
	}

	/*
	 * The section to remove is in the middle of the segment,
	 * have to make it into two segments.  nseg is made for
	 * the high end while seg is cut down at the low end.
	 */
	nbase = addr + len;	/* new seg base */
	nsize = (seg->s_base + seg->s_size) - nbase;	/* new seg size */
	seg->s_size = addr - seg->s_base;		/* shrink old seg */
	nseg = seg_alloc(seg->s_as, nbase, nsize);
	ASSERT(nseg != NULL);

	new_sdp = sdpmdi_create(sdp, getsoftc(getminor(sdp->dev)));

	nseg->s_ops = seg->s_ops;
	nseg->s_data = (void *) new_sdp;

	VN_HOLD(new_sdp->vp);	/* Hold vnode associated with the new seg */

	mutex_exit(&sdp->segmdi_mutex);

	return (0);
#endif /* PARTIAL_UNMAPS */
}

static void
segmdi_free(struct seg *seg)
{
	register struct mdi_softc *softc;
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;

	softc = getsoftc(getminor(sdp->dev));

	/* release shadow (and common, if no more refs on shadow) */
	VN_RELE(sdp->vp);

	if ((sdp->objtype >=  MDI_SHDW_VRT_MAP) &&
		(sdp->objtype <  MDI_CHUNKY_XBGR_MAP)) {

		/*
		 * Decrement the count of number of mappings to the vertical
		 * retrace counter.
		 */

		softc->mdi_vrtmappers--;
	}

	kmem_free(seg->s_data, sizeof (struct mdi_data));
}

/*ARGSUSED*/
static int
segmdi_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{

	/*
	 * Since we load mappings to the Video RAM using SRMMU level2 pages,
	 * we do not  allow changing the protection until we implement handling
	 * page demotions in the SRMMU driver.
	 */

	return (EACCES);

}

/*ARGSUSED*/
static int
segmdi_checkprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	struct mdi_data *sdp = (struct mdi_data *)seg->s_data;

	/*
	 * Since we only use segment level protection, simply check against
	 * them.
	 */
	mutex_enter(&sdp->segmdi_mutex);
	if ((sdp->prot & prot) != prot) {
		mutex_exit(&sdp->segmdi_mutex);
		return (-1);
	} else {
		mutex_exit(&sdp->segmdi_mutex);
		return (0);
	}

/*NOTREACHED*/
}

/*ARGSUSED*/
static int
segmdi_kluster(seg, addr, delta)
	struct seg *seg;
	caddr_t addr;
	ssize_t delta;
{
	return (-1);	/* Don't allow klustering */
}

/*
 * MDI pages are always "in core".
 */
/*ARGSUSED*/
static size_t
segmdi_incore(
	struct seg *seg,
	caddr_t addr,
	size_t len,
	char *vec)
{
	u_int v = 0;

	for (len = (len + PAGEOFFSET) & PAGEMASK; len; len -= MMU_PAGESIZE,
	    v += MMU_PAGESIZE)
		*vec++ = 1;
	return (v);
}

static int
segmdi_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	register struct mdi_data *sdp = (struct mdi_data *)seg->s_data;
	u_int pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;

	mutex_enter(&sdp->segmdi_mutex);
	if (pgno != 0) {
		do
			protv[--pgno] = sdp->prot;
		while (pgno != 0);
	}
	mutex_exit(&sdp->segmdi_mutex);
	return (0);
}


static int
segmdi_nop()
{
	return (0);	/* Fail silently for unsupported segment operations */
}

static struct mdi_data *
sdpmdi_create(struct mdi_data *old, struct mdi_softc *softc)
{
	struct mdi_data *new;

	new = kmem_zalloc(sizeof (struct mdi_data), KM_SLEEP);

	*new = *old;   /* struct copy */

	mutex_enter(&softc->mdi_mutex);

	mutex_init(&new->segmdi_mutex, NULL, MUTEX_DRIVER, softc->mdi_iblkc);

	mutex_exit(&softc->mdi_mutex);

	return (new);

}

/* End of segment driver section */


/*
 * Invoked by the attach routine to map in the MDI control address space
 * (control register page, cursor page, look up tables etc), get properties
 * of the VRAM (frame buffer) such as size of the frame buffer and initialize
 * the color maps
 */
static boolean_t
mdi_setupinfo(dev_info_t *dip, int instance)
{
	register struct mdi_softc *softc = getsoftc(instance);

	/* Mappings table entry */
	register struct mappings *mp;
	caddr_t reg, vram;
	u_int linebytes;
	u_int *clutp;
	u_char *xlutp;
	int h, i, j;
	u_int paddr;
	volatile struct mdi_register_address *mdi;
	int length;
	struct address_prop {
		caddr_t	controlspace;
		caddr_t	vram;
	};
	struct address_prop	addr_prop;

	mp = softc->mdi_maptable;

	/*
	 * Allocate memory for shadow color look up table
	 */
	softc->mdi_shdw_lut = (struct mdi_shadow_luts *)
	    kmem_zalloc(sizeof (struct mdi_shadow_luts), KM_NOSLEEP);
	if (softc->mdi_shdw_lut == NULL) {
		mdi_printf(CE_WARN, "Could not get memory for shadow lut");
		return (B_FALSE);
	}

	/*
	 * Allocate a page for shadow vertical retrace counter.
	 */
	softc->mdi_vrt_vaddr = kmem_alloc(PAGESIZE, KM_NOSLEEP);
	if (softc->mdi_vrt_vaddr == NULL) {
		return (B_FALSE);
	}
	bzero((caddr_t)softc->mdi_vrt_vaddr, MMU_PAGESIZE);
	mp[MDI_SHDW_VRT_INDX].pagenum = kvtoppid(softc->mdi_vrt_vaddr);

	/* Allocate some mem for the degamma table */
	softc->mdi_degamma = (struct mdi_degammalut_data *)
	    kmem_zalloc(sizeof (struct mdi_degammalut_data),
	    KM_NOSLEEP);
	if (softc->mdi_degamma == NULL) {
		mdi_printf(CE_WARN, "Could not get memory for degamma table");
		return (B_FALSE);
	}

	/*
	 * Find the size of VRAM, which is configurable.  Note that the
	 * size of the control space is MDI_MAPSIZE and is not
	 * configurable.
	 */
	if (ddi_dev_regsize(dip, (u_int)MDI_VRAM_SPACE,
	    (off_t *)&softc->mdi_vram_size) != DDI_SUCCESS) {
		mdi_printf(CE_WARN, "Could not determine size of VRAM.\n");
	}

	/*
	 * To share, or not to share?
	 *
	 * Use the "address" property to identify the console frame buffer.
	 * The "address" property is the virtual address at which the
	 * PROM maps in the console frame buffer, if it is being used.
	 * If the system is NOT using the fb for boot messages, the
	 * "address" prop will not be present because the prom will not
	 * have mapped the fb.
	 * This means we can use the mere existence of "address" as proof that
	 * we are to share the prom's mappings for the fb.
	 */
	length = sizeof (addr_prop);
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "address", (caddr_t)&addr_prop,
	    &length) == DDI_PROP_SUCCESS) {
		softc->mapped_by_prom = B_TRUE;

		/*
		 *  The struct now has 2 entries in it; the first one is
		 *  for control space, the second is for the VRAM.
		 *  Since we share both of these, we just save their
		 *  virtual addresses here.
		 */
		reg = addr_prop.controlspace;
		vram = addr_prop.vram;
		mdi_printf(CE_CONT, "Console fb mapped at 0x%x\n", vram);
		mdi_console_vaddr = (caddr_t)vram;

	} else {
		softc->mapped_by_prom = B_FALSE;

		/* Map in the control regs */
		if (ddi_map_regs(dip, (u_int)MDI_CONTROL_SPACE,
		    (caddr_t *)&reg, (off_t)0,
		    (off_t)MDI_MAPSIZE) == DDI_FAILURE) {
			cmn_err(CE_WARN, "cgfourteen: "
			    "Could not map registers\n");
			return (B_FALSE);
		}
		softc->mdi_control_regs_mapped = B_TRUE;

		/* ...and the VRAM */
		/* XXX can I get by with just mapping in 1 page? */
		if (ddi_map_regs(dip, (u_int)MDI_VRAM_SPACE,
			(caddr_t *)&vram, (off_t)0,
			softc->mdi_vram_size) == DDI_FAILURE) {
			cmn_err(CE_WARN, "cgfourteen: "
			    "Could not map in framebuffer\n");
			return (B_FALSE);
		}
		softc->mdi_vram_space_mapped = B_TRUE;

		/*
		 * Note:  the mdi_console_vaddr may or may not be
		 * initialized.  It is a bug to do so here!
		 */
	}

	softc->mdi_prd.mpr.md_image = (MPR_T *)vram;
	softc->mdi_vram_vaddr = vram;
	softc->mdi_vram_paddr = ptob(kvtoppid(vram));

	/*
	 * Map in the MDI control address space. This address space consists
	 * of 16 pages (each page is 4K bytes).
	 *
	 *	Page 0: Control registers
	 *	Page 1: Cursor registers
	 *	Page 2: DAC address space
	 *	Page 3: XLUT (X Look Up Table)
	 *	Page 4: CLUT1 (Color Look Up Table 1)
	 *	Page 5: CLUT2 (Color Look Up Table 2)
	 *	Page 6: CLUT3 (Color Look Up Table 3)
	 *	Page 7-15: reserved
	 */

	mdi_printf(CE_CONT, "MDI control addr space mapped at 0x%x\n", reg);
	/*
	 * Save the MDI control address space kernel virtual address
	 * for future reference
	 */
	softc->mdi_ctlp = mdi = (struct mdi_register_address *)reg;

	switch (mdi->m_rsr.impl) {
	case 0:
	case 2:
		softc->mdi_ncluts = 2;
		break;
	case 1:
	case 3:
		softc->mdi_ncluts = 3;
		break;
	default:
		cmn_err(CE_WARN, "cgfourteen: invalid impl field\n");
		return (B_FALSE);
	}

	softc->mdi_cursp = (struct mdi_cursor_address *)(reg + MDI_CURSOFFSET);
	softc->mdi_xlutp = (struct mdi_xlut_address *)(reg + MDI_XLUTOFFSET);

	softc->mdi_clutp[0] = (struct mdi_clut_address *)
	    (reg + MDI_CLUT1OFFSET);
	softc->mdi_clutp[1] = (struct mdi_clut_address *)
	    (reg + MDI_CLUT2OFFSET);

	if (softc->mdi_ncluts == 3) {
		softc->mdi_clutp[2] = (struct mdi_clut_address *)
		    (reg + MDI_CLUT3OFFSET);
	}

	softc->mdi_dacp = (struct mdi_dac_address *)(reg + MDI_DACOFFSET);
	softc->mdi_autoincr = (u_int *)(reg + MDI_AUTOOFFSET);

	/*
	 * Initialize the physical address fields of the various mappings
	 * to the MDI address space from the userland. The following mappings
	 * to the MDI address space are supported:
	 *
	 *	1) Read/Write mapping to all 16 pages of the MDI address space.
	 *	2) Read only mapping to all 16 pages of the MDI address space.
	 *	3) Read/Write mapping to the Cursor registers page.
	 */

	mp[MDI_ADDR_INDX].pagenum = mp[MDI_CTLREG_INDX].pagenum =
	    kvtoppid(reg);
	mp[MDI_CURSOR_INDX].pagenum = kvtoppid(reg + MDI_CURSOFFSET);

	softc->mdi_cur.set = 0;
	softc->mdi_cur.enable = 0;
	softc->mdi_cur.pos.x = softc->mdi_cur.pos.y = 0;
	softc->mdi_cur.hot.x = softc->mdi_cur.hot.y = 0;
	softc->mdi_cur.size.x = softc->mdi_cur.size.y = MDI_CURS_SIZE;

	/* get memory for s/w copy of cursor image and mask */
	softc->mdi_cur.image = (char *)kmem_zalloc(MDI_CURS_SIZE * sizeof (int),
	    KM_NOSLEEP);
	if (softc->mdi_cur.image == NULL) {
		cmn_err(CE_WARN, "cgfourteen: "
		    "Could not alloc mem for cursor image.\n");
		return (B_FALSE);
	}

	softc->mdi_cur.mask = (char *)kmem_zalloc(MDI_CURS_SIZE * sizeof (int),
	    KM_NOSLEEP);
	if (softc->mdi_cur.mask == NULL) {
		cmn_err(CE_WARN, "cgfourteen: "
		    "Could not alloc mem for cursor mask.\n");
		return (B_FALSE);
	}

	/* Save the state of video */
	/* XXX what about pixmode?  shall I save that too? */
	softc->mdi_primordial_state = mdi_get_video(mdi);

	/*
	 *  Grab frame buffer properties from PROM.
	 *  Note that many of these properties may be changed by
	 *  the driver.
	 */
	softc->mdi_mon_width = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "width", MDI_DEFAULT_WIDTH);
	mdi_printf(CE_CONT, "width = %d ", softc->mdi_mon_width);

	softc->mdi_mon_height = h =
	    ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS, "height",
	    MDI_DEFAULT_HEIGHT);
	mdi_printf(CE_CONT, "height = %d ", h);

	linebytes = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "linebytes", MDI_DEFAULT_LINEBYTES);
	mdi_printf(CE_CONT, "linebytes = %d\n", linebytes);

	/* Read the pixmode from the hw */
	softc->mdi_pixmode = mdi->m_mcr.mcr2.pixmode;

	switch (softc->mdi_pixmode) {
	case MDI_MCR_8PIX:
		softc->mdi_pixel_depth = MDI_8_PIX;
		break;
	case MDI_MCR_16PIX:
		softc->mdi_pixel_depth = MDI_16_PIX;
		break;
	case MDI_MCR_32PIX:
		softc->mdi_pixel_depth = MDI_32_PIX;
		break;
	default:
		softc->mdi_pixel_depth = ddi_getprop(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "depth", MDI_8_PIX);
		break;
	}

	softc->mdi_emu_type = FBTYPE_SUN3COLOR;

	softc->mdi_prd.mpr.md_linebytes = linebytes;

	/*
	 * Compute size of frame buffer, that is, the amount
	 * of memory which can be displayed on this monitor at one time.
	 */
/*
	softc->mdi_mon_size = linebytes * h * (softc->mdi_pixel_depth / 8);
*/
	softc->mdi_mon_size = softc->mdi_mon_height *
		softc->mdi_mon_width * (softc->mdi_pixel_depth / 8);
	softc->mdi_ndvram_size = softc->mdi_vram_size - softc->mdi_mon_size;

	/*
	 * For Sunview the frame buffers must be mapped into the
	 * kernel address space.
	 */

	softc->mdi_pr.pr_size.x = softc->mdi_mon_width;
	softc->mdi_pr.pr_size.y = softc->mdi_mon_height;

	/* Nothing busy */
	softc->mdi_update = 0;

	/*
	 * Since we are still in attach(9e), we cannot rely on
	 * device interrupts being enabled, so we cannot set the
	 * LUTs during vertical blanking.  If we do it outside
	 * vertical blanking, the user will see a glitch at system
	 * boot (and possibly other attach times), but we cannot
	 * risk the hardware getting out of sync with the software
	 * shadow tables. (see 1134625).
	 */
	for (j = 0; j < softc->mdi_ncluts; j++) {
		for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
			clutp = (u_int *)&softc->mdi_clutp[j]->c_clut[i];

			softc->mdi_shdw_lut->s_clut[j].mdi_alpha[i] =
			softc->mdi_shdw_lut->s_cleartxt[j].mdi_alpha[i] =
			    (u_char)(*clutp >> 24);
			softc->mdi_shdw_lut->s_clut[j].mdi_blue[i] =
			softc->mdi_shdw_lut->s_cleartxt[j].mdi_blue[i] =
			    (u_char)(*clutp >> 16);
			softc->mdi_shdw_lut->s_clut[j].mdi_green[i] =
			softc->mdi_shdw_lut->s_cleartxt[j].mdi_green[i] =
			    (u_char)(*clutp >> 8);
			softc->mdi_shdw_lut->s_clut[j].mdi_red[i] =
			softc->mdi_shdw_lut->s_cleartxt[j].mdi_red[i] =
			    (u_char)(*clutp);
		}
	}

	for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
		/* Xlut */
		xlutp = (u_char *)&softc->mdi_xlutp->x_xlut[i];
		softc->mdi_shdw_lut->s_xlut.mdi_xlut[i] = (u_char)(*xlutp);
	}

	/*
	 * Init our shadow gamma lut with whatever is currently
	 * in the DAC.
	 */
	softc->mdi_dacp->dac_addr_reg = 0;
	for (i = 0; i < MDI_CMAP_ENTRIES; i++) {
		softc->mdi_shdw_lut->s_glut.red[i] =
		    (u_short)(softc->mdi_dacp->dac_glut << 2);
		softc->mdi_shdw_lut->s_glut.red[i] |=
		    (u_short)(softc->mdi_dacp->dac_glut & 0x3);
		softc->mdi_shdw_lut->s_glut.green[i] =
		    (u_short)(softc->mdi_dacp->dac_glut << 2);
		softc->mdi_shdw_lut->s_glut.green[i] |=
		    (u_short)(softc->mdi_dacp->dac_glut & 0x3);
		softc->mdi_shdw_lut->s_glut.blue[i] =
		    (u_short)(softc->mdi_dacp->dac_glut << 2);
		softc->mdi_shdw_lut->s_glut.blue[i] |=
		    (u_short)(softc->mdi_dacp->dac_glut & 0x3);
	}

	/*
	 * Init our degamma table to just be linear
	 */
	for (i = 0; i < MDI_CMAP_ENTRIES; i++)
		softc->mdi_degamma->degamma[i] = (u_char)i;

	/*
	 *  Apps can do what they want, but we start
	 *  with degamma correction OFF
	 */
	softc->mdi_gammaflag = MDI_DEGAMMA_OFF;

	softc->mdi_maps_outstand = 0;
	softc->mdi_suspended = 0;

	/* Get the monitor vertical frequency */
	softc->mdi_mon_vfreq = ddi_getprop(DDI_DEV_T_ANY, dip,
	    DDI_PROP_DONTPASS, "vfreq", 0x4c);

	/* Get pixel clock generator frequency from mon_spec_table */
	softc->mdi_mon_pixfreq = mdi_get_pixelfreq(softc);

	mdi_printf(CE_CONT, "Monitor vertical frequency : %d Hz\n",
	    softc->mdi_mon_vfreq);
	mdi_printf(CE_CONT, "Monitor Pixel frequency : %d Hz\n",
	    softc->mdi_mon_pixfreq);

	/*
	 * Set up physical addresses for various frame buffer mappings. The
	 * frame buffer can be accessed in 8 different ways and the driver
	 * provides upto 8 different mapping cookies one for each type of
	 * access. The various types of mappings available are:
	 *
	 *   32-bit chunky XBGR mode (X and Blue, Green, Red)
	 *   32-bit chunky BGR mode (Blue, Green, Red)
	 *   8+8 Planar X
	 *   8+8 Planar C
	 *   32-bit planar X
	 *   32-bit planar B
	 *   32-bit planar G
	 *   32-bit planar R
	 *
	 * The length of the frame buffer varies with the type of access. If
	 * the frame buffer size is 4MB then all 4MB are available in the chunky
	 * modes, 2MB in 8+8 (16-bit) planar mode and 1 MB in the 32-bit
	 * planar mode.
	 * For more details on these access methods refer to the MDI
	 * and {E,S}MC hardware specification documents.
	 */

	/*
	 * Bits 22, 23, 24 and 25 of the VRAM physical address encode the VRAM
	 * i.e the frame buffer access mode. The mapping table stores the
	 * physical page frame number corresponding to the physical address
	 * of the access mode.
	 */

	/*
	 *  Note that this code assumes that the mdi_vram_paddr was
	 *  obtained while in 32b XBGR chunky mode.
	 */

	paddr = (softc->mdi_vram_paddr & ~CHUNKY_XBGR_MODE) | CHUNKY_XBGR_MODE;
	mp[MDI_CG3_INDX].pagenum = btop(paddr);
	mp[MDI_CG3_INDX].length = softc->mdi_vram_size;

	paddr = (softc->mdi_vram_paddr & ~CHUNKY_XBGR_MODE) | CHUNKY_XBGR_MODE;
	mp[MDI_CHUNKY_XBGR_INDX].pagenum = btop(paddr);
	mp[MDI_CHUNKY_XBGR_INDX].length = softc->mdi_vram_size;

	paddr = (softc->mdi_vram_paddr & ~CHUNKY_BGR_MODE) | CHUNKY_BGR_MODE;
	mp[MDI_CHUNKY_BGR_INDX].pagenum = btop(paddr);
	mp[MDI_CHUNKY_BGR_INDX].length = softc->mdi_vram_size;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_X16_MODE) | PLANAR_X16_MODE;
	mp[MDI_PLANAR_X16_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_X16_INDX].length = softc->mdi_vram_size/2;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_C16_MODE) | PLANAR_C16_MODE;
	mp[MDI_PLANAR_C16_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_C16_INDX].length = softc->mdi_vram_size/2;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_X32_MODE) | PLANAR_X32_MODE;
	mp[MDI_PLANAR_X32_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_X32_INDX].length = softc->mdi_vram_size/4;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_B32_MODE) | PLANAR_B32_MODE;
	mp[MDI_PLANAR_B32_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_B32_INDX].length = softc->mdi_vram_size/4;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_G32_MODE) | PLANAR_G32_MODE;
	mp[MDI_PLANAR_G32_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_G32_INDX].length = softc->mdi_vram_size/4;

	paddr = (softc->mdi_vram_paddr & ~PLANAR_R32_MODE) | PLANAR_R32_MODE;
	mp[MDI_PLANAR_R32_INDX].pagenum = btop(paddr);
	mp[MDI_PLANAR_R32_INDX].length = softc->mdi_vram_size/4;

	return (B_TRUE);
}

/*
 *  Put all hw setup stuff in here
 */
static boolean_t
mdi_init(dev_info_t *dip, int instance)
{
	struct mdi_softc *softc = getsoftc(instance);
	volatile struct mdi_register_address *mdi;
	struct mdi_cursor_address *cursor;
	int count;
	volatile struct mdi_vesa_regs	*vesa_save;

#if	!defined(lint)
	volatile u_short dummy;
#endif

	mdi = softc->mdi_ctlp;
	softc->mdi_impl = mdi->m_rsr.impl;

	mdi_printf(CE_CONT,
		"VRAM SIMM2 #%d rev = %d impl = %d VBC version = %d\n",
			instance, mdi->m_rsr.revision, mdi->m_rsr.impl,
			mdi->v_vca.version);

	if (mdi->v_vca.version < 1) {
		/*
		 *  clock freq lives in the rootnode,
		 *  so crawl up the tree to get it
		 */
		softc->mdi_mbus_freq = ddi_getprop(DDI_DEV_T_ANY, dip,
			0, "clock-frequency", 0x2625a00);
		mdi_printf(CE_CONT, "MBus Clock Frequency : %d MHz\n",
				softc->mdi_mbus_freq / 1000000);

		softc->mdi_mihdel = ddi_getprop(DDI_DEV_T_ANY, dip,
			DDI_PROP_DONTPASS, "mih-delay", 0);
		mdi_printf(CE_CONT, "EMC mih-delay : 0x%x\n",
				softc->mdi_mihdel);

		softc->mdi_sam_port_size = ddi_getprop(DDI_DEV_T_ANY,
			dip, DDI_PROP_DONTPASS, "sam-port-size", 512);

		mdi_printf(CE_CONT, "Monitor SAM port size : 0x%x\n",
				softc->mdi_sam_port_size);
	}

	/*
	 * Turn off vertical retrace interrupts.
	 */
	mdi_set_vint(mdi, FALSE);

	/*
	 *  Clear any pending interrupts/fault conditions
	 */
	mdi->m_msr = 0;		/* clears vertical intrs */
#if	!defined(lint)
	dummy = mdi->m_fsr;	/* clears the Fault Detected bit in msr */
	dummy = mdi->m_fsa;	/* clears faults in fsr */
#endif

	cursor = (struct mdi_cursor_address *)softc->mdi_cursp;

	/* Disable cursor display */
	cursor->curs_ccr &= ~MDI_CURS_ENABLE;

	/* Disable cursor plane with autoincrement  */
	count = MDI_CURS_SIZE;
	*cursor->curs_cpl0i = 0;
	while (count-- > 0)
		*softc->mdi_autoincr = 0;

	/* Use Cursor Color Reg #1 for all */
	count = MDI_CURS_SIZE;
	*cursor->curs_cpl1i = 0;
	while (count-- > 0)
		*softc->mdi_autoincr = 0;

	/* Set (x,y) positions to (0,0) */
	cursor->curs_xcu &= ~MDI_CURS_XMASK;
	cursor->curs_ycu &= ~MDI_CURS_YMASK;

	/* Set blend to 100% left */
	cursor->curs_cc1 &= ~MDI_CURS_ALPHAMASK;
	cursor->curs_cc2 &= ~MDI_CURS_ALPHAMASK;

	/* Alloc memory space and save VESA registers contents. */
	softc->mdi_vesa_save = kmem_zalloc(sizeof (struct mdi_vesa_regs),
		KM_SLEEP);
	vesa_save = softc->mdi_vesa_save;
	vesa_save->m_hss = mdi->m_hss;
	vesa_save->m_hsc = mdi->m_hsc;
	vesa_save->m_vss = mdi->m_vss;
	vesa_save->m_vsc = mdi->m_vsc;
	vesa_save->video_state = mdi_get_video(mdi);

	return (B_TRUE);
}

/*
 * Invoked by the attach routine to initialize the table containing the mapping
 * cookies, lengths (where applicable), type of mapping and access
 * permissions.
 */
static void
mdi_setup_maptbl(int instance)
{
	register struct mappings *mp;

	mp = (getsoftc(instance))->mdi_maptable;

	/*
	 * Vertical retrace counter
	 */
	mp[MDI_SHDW_VRT_INDX].map_type = MAP_SHARED;
	mp[MDI_SHDW_VRT_INDX].length = MDI_PAGESIZE;
	mp[MDI_SHDW_VRT_INDX].prot = PROT_READ | PROT_USER;

	/*
	 * Mapping information for all 16 pages of the MDI control
	 * address space
	 */
	mp[MDI_ADDR_INDX].map_type = MAP_SHARED;
	mp[MDI_ADDR_INDX].length = MDI_MAPSIZE;
	mp[MDI_ADDR_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	/*
	 * Mapping information for page 0 of the MDI control
	 * address space
	 */
	mp[MDI_CTLREG_INDX].map_type = MAP_SHARED;
	mp[MDI_CTLREG_INDX].length = MDI_PAGESIZE;
	mp[MDI_CTLREG_INDX].prot = PROT_READ | PROT_USER;

	/* Mapping information for the MDI cursor address space  */

	mp[MDI_CURSOR_INDX].map_type = MAP_SHARED;
	mp[MDI_CURSOR_INDX].length = MDI_PAGESIZE;
	mp[MDI_CURSOR_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	/*
	 * The following mappings are for the various access modes
	 * supported by the VRAM. The length and physical page frame
	 * numbers are computed and initialized in the routine
	 * mdi_init()
	 */
	mp[MDI_CG3_INDX].map_type = MAP_SHARED;
	mp[MDI_CG3_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_CHUNKY_XBGR_INDX].map_type = MAP_SHARED;
	mp[MDI_CHUNKY_XBGR_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_CHUNKY_BGR_INDX].map_type = MAP_SHARED;
	mp[MDI_CHUNKY_BGR_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_X16_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_X16_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_C16_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_C16_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_X32_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_X32_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_B32_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_B32_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_G32_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_G32_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;

	mp[MDI_PLANAR_R32_INDX].map_type = MAP_SHARED;
	mp[MDI_PLANAR_R32_INDX].prot = PROT_READ|PROT_WRITE|PROT_USER;
}


/*
 *  Invoked from the interrupt handler to update the RAMDAC lookup tables.
 */
static void
mdi_update_gamma_table(
	register u_short *red,
	register u_short *green,
	register u_short *blue,
	register int count,
	register struct mdi_softc *softc)
{
	while (count-- > 0) {
		softc->mdi_dacp->dac_glut = (u_char)(*red >> 2);
		softc->mdi_dacp->dac_glut = (u_char)(*red++ & 0x3);
		softc->mdi_dacp->dac_glut = (u_char)(*green >> 2);
		softc->mdi_dacp->dac_glut = (u_char)(*green++ & 0x3);
		softc->mdi_dacp->dac_glut = (u_char)(*blue >> 2);
		softc->mdi_dacp->dac_glut = (u_char)(*blue++ & 0x3);
	}
}

/*
 *  Invoked from MDI interrupt handler to update MDI hardware X look up table
 *  during a vertical retrace.
 *  This routine assumes the autoincrement address space has already been
 *  set up for it.
 */
static void
mdi_update_hw_xmap(
	register u_char *shdw_xmapaddr,
	register int count,
	register struct mdi_softc *softc)
{
	/* always use autoinc for speed */
	while (count-- > 0)
		*(u_char *)softc->mdi_autoincr = *shdw_xmapaddr++;
}

/*
 *  Invoked from MDI interrupt handler to update MDI hardware X and
 *  Color Look Up Tables. Hardware look up tables should be updated only
 *  during a vertical retrace.
 *  This routine assumes the autoincrement address space has already been
 *  set up for it.
 */
static void
mdi_update_hw_cmap(
	register u_char *alpha,
	register u_char *red,
	register u_char *green,
	register u_char *blue,
	register int count,
	register struct mdi_softc *softc)
{

	/* always use autoinc for speed */
	while (count-- > 0) {
		*softc->mdi_autoincr = (u_int)((*alpha++ << 24) |
			(*blue++ << 16) | (*green++ << 8) | (*red++));
	}
}

/*
 * Routines to get/set the shadow color map tables. ioctl(2) commands
 * for color map tables affect only the shadow color look up tables. The
 * hardware color look up tables are updated only during a vertical retrace.
 */

/*
 *  Copy the shadow color look up table into the user address space.
 *  Invoked for FBIOGETCMAP and MDI_GET_CLUT commands.
 */
static int
mdi_get_shdw_clut(register struct mdi_softc *softc,
	void *user_parms, /* This can be either (fbcmap *)/(mdi_set_clut *) */
	int alpha_flag, int clut_id, int mode, int get_clrtxt)
{
	register u_char  *usr_red, *usr_green, *usr_blue, *usr_alpha;
	register u_char *k_red, *k_green, *k_blue;
	register struct mdi_shadow_luts *shdw_clutp;
	int lut, usr_index, usr_count;
	struct mdi_lut_data *map;
	u_char *space;
	u_char *stack_red, *stack_green, *stack_blue, *stack_alpha;
	struct mdi_lut_data *cleartxt;
	int error;

	space = kmem_alloc(MDI_CMAP_ENTRIES * 4 * sizeof (u_char), KM_NOSLEEP);
	if (space == NULL)
		return (ENOMEM);

	stack_blue = space;
	stack_green = &space[MDI_CMAP_ENTRIES];
	stack_red = &space[MDI_CMAP_ENTRIES * 2];
	stack_alpha = &space[MDI_CMAP_ENTRIES * 3];

	mutex_enter(&softc->mdi_mutex);

	shdw_clutp = softc->mdi_shdw_lut;

	lut = clut_id;

	if (lut >= MDI_MAX_CLUTS) {
		mutex_exit(&softc->mdi_mutex);
		error = EINVAL;
		goto failed;
	}

	if (alpha_flag == MDI_NOUPDATE_ALPHA) {
		/*
		 *  from FBIOGETCMAP command.
		 *  Just need R G and B components
		 */
		struct fbcmap *fbio_cmap;
		fbio_cmap = (struct fbcmap *)user_parms;

		map = &shdw_clutp->s_clut[lut];
		cleartxt = &shdw_clutp->s_cleartxt[lut];

		usr_index = fbio_cmap->index;
		usr_count = fbio_cmap->count;
		usr_red = fbio_cmap->red;
		usr_green = fbio_cmap->green;
		usr_blue = fbio_cmap->blue;
	}

	/*
	 * from MDI_GET_CLUT command.
	 * Need to determine look up table number and
	 * update the alpha value also, in addition to RGB components.
	 */
	else {
		struct mdi_set_clut *mdi_cmap;

		mdi_cmap = (struct mdi_set_clut *)user_parms;

		usr_index = mdi_cmap->index;
		usr_count = mdi_cmap->count;
		usr_red = mdi_cmap->red;
		usr_green = mdi_cmap->green;
		usr_blue = mdi_cmap->blue;
		usr_alpha = mdi_cmap->alpha;

		/*
		 * Determine which color look up table to return.
		 */
		switch (mdi_acl[lut]) {

		case MDI_CLUT1:
			map = &shdw_clutp->s_clut[0];
			cleartxt = &shdw_clutp->s_cleartxt[0];
			break;

		case MDI_CLUT2:
			map = &shdw_clutp->s_clut[1];
			cleartxt = &shdw_clutp->s_cleartxt[1];
			break;

		case MDI_CLUT3:

			if (softc->mdi_ncluts == 3) {
				map = &shdw_clutp->s_clut[2];
				cleartxt = &shdw_clutp->s_cleartxt[2];
			} else {
				mutex_exit(&softc->mdi_mutex);
				error = EINVAL;
				goto failed;
			}
			break;

		default:
			mutex_exit(&softc->mdi_mutex);
			error = EINVAL;
			goto failed;
			/*NOTREACHED*/
		}
	}

	if (usr_count == 0) {
		mutex_exit(&softc->mdi_mutex);
		error = 0;
		goto failed;
	}

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
	    (usr_index + usr_count > MDI_CMAP_ENTRIES)) {
		mutex_exit(&softc->mdi_mutex);
		error = EINVAL;
		goto failed;
	}

	/* Adjust clut source, if needed */
	if (get_clrtxt) {
		/* Set copyout ptr to the cleartxt tables */
		k_red = cleartxt->mdi_red;
		k_green = cleartxt->mdi_green;
		k_blue = cleartxt->mdi_blue;
	} else {
		/* Set the copyout ptr to the shadow tables themselves */
		k_red = map->mdi_red;
		k_green = map->mdi_green;
		k_blue = map->mdi_blue;
	}

	/* copy from global area to stack */
	bcopy((caddr_t)k_red + usr_index, (caddr_t)stack_red + usr_index,
	    usr_count * sizeof (stack_red[0]));
	bcopy((caddr_t)k_green + usr_index, (caddr_t)stack_green + usr_index,
	    usr_count * sizeof (stack_green[0]));
	bcopy((caddr_t)k_blue + usr_index, (caddr_t)stack_blue + usr_index,
	    usr_count * sizeof (stack_blue[0]));

	if (alpha_flag == MDI_UPDATE_ALPHA) {
		bcopy((caddr_t)map->mdi_alpha + usr_index,
		    (caddr_t)stack_alpha + usr_index,
		    usr_count * sizeof (stack_alpha[0]));
	}

	mutex_exit(&softc->mdi_mutex);

	/*
	 * Copy out the blue, green and alpha components.
	 */
	if (ddi_copyout((caddr_t)stack_red + usr_index, (caddr_t)usr_red,
	    usr_count * sizeof (usr_red[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	if (ddi_copyout((caddr_t)stack_green + usr_index, (caddr_t)usr_green,
	    usr_count * sizeof (usr_green[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	if (ddi_copyout((caddr_t)stack_blue + usr_index, (caddr_t)usr_blue,
	    usr_count * sizeof (usr_blue[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	/*
	 * Copy out the alpha value. This needs to be done only for the
	 * MDI_GET_CLUT command, in which case alpha_flag is set to TRUE.
	 * There is no alpha table to be gamma corrected, of course.
	 */
	if (alpha_flag == MDI_UPDATE_ALPHA)
		if (ddi_copyout((caddr_t)stack_alpha + usr_index,
		    (caddr_t)usr_alpha, usr_count * sizeof (stack_alpha[0]),
		    mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	error = 0;

failed:
	kmem_free(space, MDI_CMAP_ENTRIES * 4 * sizeof (u_char));
	return (error);
}

/*
 * Update a shadow color map. Invoked as a result of FBIOPUTCMAP and
 * MDI_SET_CLUT commands. In the case of FBIOPUTCMAP command,  color map
 * table 1 is updated. The FBIOPUTCMAP command does not supply an alpha value,
 * so a the alpha value is not updated.
 * This code has the limitation that it will only update 1 clut at a time,
 * since the data structures cannot be easily linked together.
 */
static int
mdi_set_shdw_clut(struct mdi_softc *softc, void *data, int alpha_flag,
			int clut_id, int mode, int set_clrtxt)
{
	register struct mdi_shadow_luts *shdw_clutp;
	u_char  *usr_red, *usr_green, *usr_blue, *usr_alpha;
	struct fbcmap *fbio_cmap;
	struct mdi_set_clut *mdi_cmap;
	int  i, *cmap_index, *cmap_count;
	u_int usr_index, usr_count;
	u_char *space;
	u_char *stack_blue, *stack_green, *stack_red, *stack_alpha;
	struct mdi_lut_data *map;
	struct mdi_lut_data *cleartxt;
	int lut;
	int error = 0;

	space = kmem_alloc(MDI_CMAP_ENTRIES * 4 * sizeof (u_char), KM_SLEEP);

	stack_blue = space;
	stack_green = &space[MDI_CMAP_ENTRIES];
	stack_red = &space[MDI_CMAP_ENTRIES * 2];
	stack_alpha = &space[MDI_CMAP_ENTRIES * 3];

	mutex_enter(&softc->mdi_mutex);

	shdw_clutp = softc->mdi_shdw_lut;

	lut = clut_id;

	if (lut >= MDI_MAX_CLUTS) {
		mutex_exit(&softc->mdi_mutex);
		error = EINVAL;
		goto failed;
	}

	if (alpha_flag == MDI_NOUPDATE_ALPHA) {

		/*
		 * Color map update because of the FBIOPUTCMAP command.
		 */
		fbio_cmap = (struct fbcmap *)data;
		map = &shdw_clutp->s_clut[lut];
		cleartxt = &shdw_clutp->s_cleartxt[lut];
		cmap_index = &shdw_clutp->s_clut[lut].mdi_index;
		cmap_count = &shdw_clutp->s_clut[lut].mdi_count;

		usr_index = fbio_cmap->index;
		usr_count = fbio_cmap->count;
		usr_red = fbio_cmap->red;
		usr_green = fbio_cmap->green;
		usr_blue = fbio_cmap->blue;
	}

	/*
	 * MDI_SET_CLUT command. Need to determine look up table number and
	 * update the alpha value also, in addition to RGB components.
	 */

	else {
		mdi_cmap = (struct mdi_set_clut *)data;

		usr_index = mdi_cmap->index;
		usr_count = mdi_cmap->count;
		usr_red = mdi_cmap->red;
		usr_green = mdi_cmap->green;
		usr_blue = mdi_cmap->blue;
		usr_alpha = mdi_cmap->alpha;

		switch (mdi_acl[lut]) {

		case MDI_CLUT1:
			map = &shdw_clutp->s_clut[0];
			cmap_index = &(shdw_clutp->s_clut[0].mdi_index);
			cmap_count = &(shdw_clutp->s_clut[0].mdi_count);
			cleartxt = &shdw_clutp->s_cleartxt[0];
			break;

		case MDI_CLUT2:
			map = &shdw_clutp->s_clut[1];
			cmap_index = &(shdw_clutp->s_clut[1].mdi_index);
			cmap_count = &(shdw_clutp->s_clut[1].mdi_count);
			cleartxt = &shdw_clutp->s_cleartxt[1];
			break;

		case MDI_CLUT3:

			if (softc->mdi_ncluts == 3) {
				map = &shdw_clutp->s_clut[2];
				cmap_index = &shdw_clutp->s_clut[2].mdi_index;
				cmap_count = &shdw_clutp->s_clut[2].mdi_count;
				cleartxt = &shdw_clutp->s_cleartxt[2];
			} else {
				mutex_exit(&softc->mdi_mutex);
				error = EINVAL;
				goto failed;
			}
			break;
		default:
			mutex_exit(&softc->mdi_mutex);
			error = EINVAL;
			goto failed;
			/*NOTREACHED*/
		}

	}

	mutex_exit(&softc->mdi_mutex);

	if (usr_count == 0) {
		error = 0;
		goto failed;
	}

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
	    (usr_index + usr_count > MDI_CMAP_ENTRIES)) {
		error = EINVAL;
		goto failed;
	}

	/*
	 * Update only the blue, green and red components of the
	 * shadow color look up table from the user supplied
	 * component arrays.
	 */
	if (ddi_copyin((caddr_t)usr_blue, (caddr_t)stack_blue + usr_index,
	    usr_count * sizeof (stack_blue[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	if (ddi_copyin((caddr_t)usr_green, (caddr_t)stack_green + usr_index,
	    usr_count * sizeof (stack_green[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	if (ddi_copyin((caddr_t)usr_red, (caddr_t)stack_red + usr_index,
	    usr_count * sizeof (stack_red[0]), mode) != 0) {
		error = EFAULT;
		goto failed;
	}

	if (alpha_flag == MDI_UPDATE_ALPHA) {
		/*
		 * Update the alpha component.
		 */
		if (ddi_copyin((caddr_t)usr_alpha, (caddr_t)stack_alpha +
		    usr_index, usr_count * sizeof (stack_alpha[0]),
		    mode) != 0) {
			error = EFAULT;
			goto failed;
		}
	}

	mutex_enter(&softc->mdi_mutex);

	/*
	 *  We must save the original (stack_* data) into the "cleartxt"
	 *  part of the shadow tables so that it can be returned by
	 *  MDI_GET_CLUT later.
	 */
	if (set_clrtxt) {

		bcopy((caddr_t)stack_red + usr_index,
			(caddr_t)cleartxt->mdi_red + usr_index,
			usr_count * sizeof (stack_red[0]));
		bcopy((caddr_t)stack_green + usr_index,
			(caddr_t)cleartxt->mdi_green + usr_index,
			usr_count * sizeof (stack_green[0]));
		bcopy((caddr_t)stack_blue + usr_index,
			(caddr_t)cleartxt->mdi_blue + usr_index,
			usr_count * sizeof (stack_blue[0]));
	}

	/*
	 * Next, we must do an index lookup thru the degamma table
	 * to degammify the data, and then put this degammified data
	 * into the h/w shadow table.
	 * We never degammify the alpha channel.
	 * For apps that want no degamma pre-correction, they should set the
	 * degamma table to an identity table.
	 */
	mutex_enter(&softc->mdi_degammalock);
	for (i = usr_index; i < usr_index + usr_count; i++) {
		stack_red[i] =
			softc->mdi_degamma->degamma[stack_red[i]];
		stack_green[i] =
			softc->mdi_degamma->degamma[stack_green[i]];
		stack_blue[i] =
			softc->mdi_degamma->degamma[stack_blue[i]];
	}
	mutex_exit(&softc->mdi_degammalock);

	/* copy from stack to global */
	bcopy((caddr_t)stack_red + usr_index, (caddr_t)map->mdi_red +
				usr_index, usr_count * sizeof (stack_red[0]));
	bcopy((caddr_t)stack_green + usr_index, (caddr_t)map->mdi_green +
				usr_index, usr_count * sizeof (stack_green[0]));
	bcopy((caddr_t)stack_blue + usr_index, (caddr_t)map->mdi_blue +
				usr_index, usr_count * sizeof (stack_blue[0]));

	if (alpha_flag == MDI_UPDATE_ALPHA)
		bcopy((caddr_t)stack_alpha + usr_index,
				(caddr_t)map->mdi_alpha + usr_index,
				usr_count * sizeof (stack_alpha[0]));

	/* schedule the update, but do NOT put process to sleep */
	shdw_clutp->s_update_flag |= mdi_acl[lut];

	/*
	 * Update color map index and count fields.
	 */
	mdi_update_cmap_index(cmap_index, cmap_count, usr_index, usr_count);

	mutex_exit(&softc->mdi_mutex);

	error = 0;

failed:
	kmem_free(space, MDI_CMAP_ENTRIES * 4 * sizeof (u_char));
	return (error);
}

/*
 * Update shadow X look up table.
 */
static int
mdi_set_shdw_xlut(struct mdi_softc *softc, struct mdi_set_xlut *usr_xlutp)
{
	register u_char *shdw_xmap;
	int *xmap_index, *xmap_count;
	struct mdi_shadow_luts *shdw_clutp;
	register int i;
	u_int usr_count, usr_index;
	u_char *space, *xbuf, *maskbuf;
	int error = 0;

	space = kmem_alloc(MDI_CMAP_ENTRIES * 2 * sizeof (u_char), KM_SLEEP);

	xbuf = space;
	maskbuf = &space[MDI_CMAP_ENTRIES];

	mutex_enter(&softc->mdi_mutex);

	shdw_clutp = softc->mdi_shdw_lut;
	shdw_xmap = softc->mdi_shdw_lut->s_xlut.mdi_xlut;

	xmap_index = &softc->mdi_shdw_lut->s_xlut.mdi_index;
	xmap_count = &softc->mdi_shdw_lut->s_xlut.mdi_count;

	usr_index = usr_xlutp->index;
	usr_count = usr_xlutp->count;

	if (usr_count == 0) {
		mutex_exit(&softc->mdi_mutex);
		error = 0;
		goto failed;
	}

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
	    (usr_index + usr_count > MDI_CMAP_ENTRIES)) {
		mutex_exit(&softc->mdi_mutex);
		error = EINVAL;
		goto failed;
	}

	shdw_xmap += usr_index;

	mutex_exit(&softc->mdi_mutex);

	if (copyin((caddr_t)usr_xlutp->xbuf, (caddr_t)xbuf, usr_count) != 0) {
		error = EFAULT;
		goto failed;
	}


	if (usr_xlutp->maskbuf == (u_char *)NULL) {
		/*
		 * Use the same mask of the bits to be preserved for each
		 * entry of the X table. This mask is given in the x->mask
		 * field.
		 */
		mutex_enter(&softc->mdi_mutex);
		for (i = 0; i < usr_count; i++) {
			*shdw_xmap = ((*shdw_xmap) & ~usr_xlutp->mask) |
			    (xbuf[i] & usr_xlutp->mask);
			shdw_xmap++;
		}

	} else { /* Different mask for each entry */

		if (copyin((caddr_t)usr_xlutp->maskbuf, (caddr_t)maskbuf,
		    usr_count) != 0) {
			error = EFAULT;
			goto failed;
		}

		mutex_enter(&softc->mdi_mutex);

		for (i = 0; i < usr_count; i++, shdw_xmap++) {
			*shdw_xmap = ((*shdw_xmap) & ~maskbuf[i]) |
			    (xbuf[i] & maskbuf[i]);
		}
	}

	shdw_clutp->s_update_flag |= MDI_XLUT;

	/*
	 * Update color map index and count fields.
	 */
	mdi_update_cmap_index(xmap_index, xmap_count, usr_index, usr_count);

	mutex_exit(&softc->mdi_mutex);

	error = 0;
failed:
	kmem_free(space, MDI_CMAP_ENTRIES * 2 * sizeof (u_char));
	return (error);
}

static int
mdi_get_shdw_xlut(register struct mdi_softc *softc, caddr_t data)
{
	register struct mdi_set_xlut *usr_xlutp = (struct mdi_set_xlut *)data;
	register u_char *shdw_xmap;
	u_int usr_count, usr_index;
	u_char stack_xmap[MDI_CMAP_ENTRIES];

	usr_index = usr_xlutp->index & MDI_CMAP_MASK;
	usr_count = usr_xlutp->count;

	if (usr_count == 0)
		return (0);

	if ((usr_index >= MDI_CMAP_ENTRIES) ||
		(usr_index + usr_count > MDI_CMAP_ENTRIES))
		return (EINVAL);

	shdw_xmap = softc->mdi_shdw_lut->s_xlut.mdi_xlut;

	mutex_enter(&softc->mdi_mutex);

	bcopy((caddr_t)shdw_xmap + usr_index, (caddr_t)stack_xmap + usr_index,
		usr_count * sizeof (stack_xmap[0]));

	mutex_exit(&softc->mdi_mutex);

	if (copyout((caddr_t)stack_xmap + usr_index,
			(caddr_t)usr_xlutp->xbuf,
			usr_count * sizeof (stack_xmap[0])) != 0)
		return (EFAULT);

	return (0);
}


/*
 * Update the shadow color and X tables index and count fields.
 * for optimized copy to hardware look up tables.
 */
static void
mdi_update_cmap_index(
	int *curindex,
	int *curcount,
	int newindex,
	int newcount)
{
	int low, high;

	if (*curcount != 0) {

		low = *curindex;
		high = low + *curcount;

		if (newindex < low)
			*curindex = low = newindex;

		if (newindex + newcount > high)
			high = newindex + newcount;
		*curcount = high - low;
	} else {
		*curindex = newindex;
		*curcount = newcount;
	}
}

/*
 * enable/disable/update HW cursor
 */

static void
mdi_setcurpos(struct mdi_softc *softc, struct fbcurpos *pos)
{
	struct mdi_cursor_address *cursor = (struct mdi_cursor_address *)
						softc->mdi_cursp;

	if (softc->mdi_cur.enable) {
		cursor->curs_ccr |= MDI_CURS_ENABLE;

		cursor->curs_xcu =
			(pos->x - softc->mdi_cur.hot.x) & MDI_CURS_XMASK;
		cursor->curs_ycu =
			(pos->y - softc->mdi_cur.hot.y) & MDI_CURS_YMASK;
		softc->mdi_cur.pos.x = cursor->curs_xcu & MDI_CURS_XMASK;
		softc->mdi_cur.pos.y = cursor->curs_ycu & MDI_CURS_YMASK;
	} else
		cursor->curs_ccr &= ~MDI_CURS_ENABLE;
}

/*
 * Set the MDI hardware cursor shape.
 */
static int
mdi_set_curshape(struct mdi_softc *softc, struct fbcursor *cp, int mode)
{
	int cbytes;
	u_long tmp, edge = 0;
	u_long *image, *mask;
	int i;
	u_long curbuf[MDI_CURS_SIZE];
	volatile struct mdi_cursor_address *cu =
		(struct mdi_cursor_address *)softc->mdi_cursp;

	if ((u_int)cp->size.x > MDI_CURS_SIZE ||
					(u_int)cp->size.y > MDI_CURS_SIZE)
		return (EINVAL);

	softc->mdi_cur.size = cp->size;

	/*
	 * compute cursor bitmap bytes
	 */
	cbytes = softc->mdi_cur.size.y * sizeof (u_int);

	/*
	 * copy cursor image and mask bits into softc
	 * (Shadow copy).
	 * The image bits go to MDI cursor plane 1. and
	 * the mask bits go into MDI cursor plane 0.
	 */
	if (cp->image != NULL) {
		if (ddi_copyin((caddr_t)cp->image, (caddr_t)curbuf,
				cbytes, mode) != 0)
			return (EFAULT);

		bzero((caddr_t)softc->mdi_cur.image,
			sizeof (softc->mdi_cur.image) * MDI_CURS_SIZE);
		bcopy((caddr_t)curbuf, (caddr_t)softc->mdi_cur.image, cbytes);
	}

	if (cp->mask != NULL) {
		if (ddi_copyin((caddr_t)cp->mask, (caddr_t)curbuf,
				cbytes, mode) != 0)
			return (EFAULT);

		bzero((caddr_t)softc->mdi_cur.mask,
			sizeof (softc->mdi_cur.mask) * MDI_CURS_SIZE);
		bcopy((caddr_t)curbuf, (caddr_t)softc->mdi_cur.mask, cbytes);
	}

	/*
	 * load HW cursor bitmaps
	 */

	/*
	 * compute right edge mask of the cursor
	 */
	if (softc->mdi_cur.size.x)
		edge = (u_long) ~0 << (MDI_CURS_SIZE - softc->mdi_cur.size.x);

	mask = (u_long *)softc->mdi_cur.mask;
	image = (u_long *)softc->mdi_cur.image;
	mutex_enter(&softc->mdi_mutex);

	for (i = 0; i < MDI_CURS_SIZE; i++) {
		/* Update cursor plane 0 */
		cu->curs_cpl0[i] = tmp = mask[i] & edge;
		/* and cursor plane 1 */
		cu->curs_cpl1[i] = tmp & image[i];
	}

	mdi_verify_cursor(softc, mask, image, edge);
	mutex_exit(&softc->mdi_mutex);

	return (0);
}

/*
 *  This is to fix bug 1150058 .  The caller should hold the mdi_mutex.
 *  With DEBUG off, this routine becomes a leaf routine.
 */
void
mdi_verify_cursor(struct mdi_softc *softc, u_long *mask, u_long *image,
	u_long edge)
{
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	volatile struct mdi_cursor_address *cu = softc->mdi_cursp;
	u_long tmp;
	int i, j, fail_flg;

	ASSERT(MUTEX_HELD(&softc->mdi_mutex));

	if ((mdi->m_rsr.revision != 3 && mdi->m_rsr.revision != 1) ||
			mdi_curs_scrub_enable == 0)
		return;

	for (i = 0; i < mdi_curs_retry; i++) {
		fail_flg = 0;
		for (j = 0; j < MDI_CURS_SIZE; j++) {
		    u_char *cp0 = (u_char *)&cu->curs_cpl0[j];
		    u_char *cp1 = (u_char *)&cu->curs_cpl1[j];

			tmp = mask[j] & edge;

		/* Must use byte writes for repairs */
			if (cp0[0] != (u_char)(tmp >> 24)) {
				cp0[0] = (u_char)(tmp >> 24);
				fail_flg++;
			}
			if (cp0[1] != (u_char)(tmp >> 16)) {
				cp0[1] = (u_char)(tmp >> 16);
				fail_flg++;
			}
			if (cp0[2] != (u_char)(tmp >> 8)) {
				cp0[2] = (u_char)(tmp >> 8);
				fail_flg++;
			}
			if (cp0[3] != (u_char)(tmp)) {
				cp0[3] = (u_char)tmp;
				fail_flg++;
			}

			if (cp1[0] != (u_char)((tmp & image[j]) >> 24)) {
				cp1[0] = (u_char)((tmp & image[j])>> 24);
				fail_flg++;
			}
			if (cp1[1] != (u_char)((tmp & image[j]) >> 16)) {
				cp1[1] = (u_char)((tmp & image[j])>> 16);
				fail_flg++;
			}
			if (cp1[2] != (u_char)((tmp & image[j]) >> 8)) {
				cp1[2] = (u_char)((tmp & image[j])>> 8);
				fail_flg++;
			}
			if (cp1[3] != (u_char)(tmp & image[j])) {
				cp1[3] = (u_char)(tmp & image[j]);
				fail_flg++;
			}
		}
#ifdef DEBUG
		/* Log some stats */
		if (fail_flg != 0)
			softc->mdi_cursor_softfails++;
#endif /* DEBUG */
		if (fail_flg == 0)
			break;
	}
#ifdef DEBUG
	if (i == mdi_curs_retry)
		softc->mdi_cursor_hardfails++;
#endif /* DEBUG */

}

/*
 * Update shadow cursor color registers
 */
static int
mdi_set_shdw_curcmap(register struct mdi_softc *softc, struct fbcmap *cmap,
	int mode)
{
	register int i;
	u_int *mdi_cmap = softc->mdi_shdw_curcmap;
	u_char red[MDI_CURS_ENTRIES], green[MDI_CURS_ENTRIES],
	    blue[MDI_CURS_ENTRIES];

	if ((cmap->index >= MDI_CURS_ENTRIES) ||
		(cmap->index + cmap->count > MDI_CURS_ENTRIES))
		return (EINVAL);

	/*
	 * Copy the red green and blue components.
	 */
	if (ddi_copyin((caddr_t)cmap->red + cmap->index, (caddr_t)red,
	    cmap->count, mode) != 0)
		return (EFAULT);

	if (ddi_copyin((caddr_t)cmap->green + cmap->index, (caddr_t)green,
	    cmap->count, mode) != 0)
		return (EFAULT);

	if (ddi_copyin((caddr_t)cmap->blue + cmap->index, (caddr_t)blue,
	    cmap->count, mode) != 0)
		return (EFAULT);

	/* setup the shadow map(s) */
	mdi_cmap += cmap->index;
	for (i = 0; i < cmap->count; i++) {
		*mdi_cmap = (blue[i] << 16) | (green[i] << 8) | (red[i]);
		mdi_cmap++;
	}

	/* schedule the update */
	softc->mdi_update |= MDI_CURS_UPDATE;

	return (0);

}

static int
mdi_get_shdw_curcmap(register struct mdi_softc *softc, struct fbcursor *cp,
	int mode)
{
	register u_int *mdi_curcmap = softc->mdi_shdw_curcmap;
	int cbytes;
	int i, index, count;
	u_char red[MDI_CURS_ENTRIES], green[MDI_CURS_ENTRIES],
	    blue[MDI_CURS_ENTRIES];

	/* First get what the user has sent */
	index = cp->cmap.index;
	count = cp->cmap.count;

	/* Next, insert the "easy stuff" */
	mutex_enter(&softc->mdi_mutex);
	cp->set = 0;
	cp->enable = softc->mdi_cur.enable;
	cp->pos = softc->mdi_cur.pos;
	cp->hot = softc->mdi_cur.hot;
	cp->size = softc->mdi_cur.size;
	mutex_exit(&softc->mdi_mutex);

	if ((index >= MDI_CURS_ENTRIES) ||
			(index + count > MDI_CURS_ENTRIES))
		return (EINVAL);

	/*
	 * compute cursor bitmap bytes
	 */
	cbytes = (softc->mdi_cur.size.y) * sizeof (softc->mdi_cur.image[0]);

	/*
	 * if image pointer is non-null, copy both bitmaps
	 */
	if (cp->image != NULL) {
		if (ddi_copyout((caddr_t)softc->mdi_cur.image,
			(caddr_t)cp->image, cbytes, mode) != 0)
			return (EFAULT);
		if (ddi_copyout((caddr_t)softc->mdi_cur.mask,
			(caddr_t)cp->mask, cbytes, mode) != 0)
			return (EFAULT);
	}

	/*
	 * if red pointer is non-null copy colormap
	 */
	if (cp->cmap.red != NULL) {

		mdi_curcmap += index;
		for (i = 0; i < count; i++) {

			red[i] = (u_char)(*mdi_curcmap & 0xff);
			green[i] = (u_char)(*mdi_curcmap & 0xff00);
			blue[i] = (u_char)(*mdi_curcmap & 0xff0000);
			mdi_curcmap++;
		}
		if (ddi_copyout((caddr_t)red,
			(caddr_t)(cp->cmap.red + index), count, mode) != 0)
				return (EFAULT);

		if (ddi_copyout((caddr_t)green, (caddr_t)(cp->cmap.green +
						    index), count, mode) != 0)
			return (EFAULT);

		if (ddi_copyout((caddr_t)blue,
			(caddr_t)(cp->cmap.blue + index), count, mode) != 0)
				return (EFAULT);

	} else {
	/*
	 * just trying to find out colormap size
	 */
		cp->cmap.index = 0;
		cp->cmap.count = MDI_CURS_ENTRIES;
	}

	return (0);
}


/*
 * When the MDI is programmed in the packed pixel mode, blend selection
 * (selection of two paths from Greyscale or direct, Color Look Up Table1,
 * Color Look Up Table2, Color Look Up Table 3 and Cursor) criteria can be
 * be established by programming the packed pixel register. This routine is
 * invoked from the ioctl() routine.
 */
static int
mdi_ppr_setup(struct mdi_softc *softc, int arg)
{
	volatile struct mdi_register_address *mdi;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	ASSERT(MUTEX_HELD(&softc->mdi_mutex));

	/* XXX pray the user knows what she's doing */
	mdi->m_ppr = (u_int)arg & 0xf0;

	FLUSH_WRITE_BUFFERS(mdi);

	return (0);
}

/*
 * Select the mode in which the MDI interprets input data.
 * The user must be prepared to send valid data into the color LUTs
 * and into the framebuffer itself.
 */
static int
mdi_set_pixelmode(struct mdi_softc *softc, int mode)
{
	volatile struct mdi_register_address *mdi;
	int vsfreq, r_setup;
	int size, i;
	struct mappings *mp;
	caddr_t a;
	u_int phys_pagenum, prot;
	u_int video_state, vrt_state;

	mp = softc->mdi_maptable;
	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	/* Dispense with redundant calls */
	if (mode == softc->mdi_pixel_depth)
		return (0);

	switch (mode) {

	case MDI_8_PIX:
	/* MDI interprets incoming data as 16 8-bit pixels */

		softc->mdi_pixmode = MDI_MCR_8PIX;
		softc->mdi_pixel_depth = MDI_8_PIX;

		if (softc->mapped_by_prom) {
			phys_pagenum = mp[MDI_CHUNKY_XBGR_INDX].pagenum;
			prot = mp[MDI_CHUNKY_XBGR_INDX].prot & ~PROT_USER;
			size = mp[MDI_CHUNKY_XBGR_INDX].length;
		}
		break;

	case MDI_16_PIX:
	/* MDI interprets incoming data as 8 16-bit pixels */

		softc->mdi_pixmode = MDI_MCR_16PIX;
		softc->mdi_pixel_depth = MDI_16_PIX;

		if (softc->mapped_by_prom) {
			phys_pagenum = mp[MDI_PLANAR_X16_INDX].pagenum;
			prot = mp[MDI_PLANAR_X16_INDX].prot & ~PROT_USER;
			size = mp[MDI_PLANAR_X16_INDX].length;
		}
		break;

	case MDI_32_PIX:
	/* MDI interprets incoming data as 4 32-bit pixels */

		softc->mdi_pixmode = MDI_MCR_32PIX;
		softc->mdi_pixel_depth = MDI_32_PIX;

		if (softc->mapped_by_prom) {
			phys_pagenum = mp[MDI_PLANAR_X32_INDX].pagenum;
			prot = mp[MDI_PLANAR_X32_INDX].prot & ~PROT_USER;
			size = mp[MDI_PLANAR_X32_INDX].length;
		}
		break;

	default:
		return (EINVAL);
	}

	if (softc->mapped_by_prom) {
		struct snode *csp = (struct snode *)NULL;

		ASSERT(mdi_console_vaddr != NULL);

		/* Find the enclosing snode of fbvp */
		if (fbvp != NULL)
			csp = VTOS(VTOS(fbvp)->s_commonvp);

		if (csp != NULL)
			mutex_enter(&csp->s_lock);

		/*
		 *  Unlock the range of mappings.
		 *  Now we have everthing we need to smash the
		 *  current mappings...
		 */
		hat_unload(kas.a_hat, mdi_console_vaddr, size,
						HAT_UNLOAD_UNLOCK);

		/*
		 *  and setup a new range for the console device.
		 */
		for (i = 0, a = mdi_console_vaddr;
			a < (caddr_t)(mdi_console_vaddr+size);
			a += MDI_PAGESIZE, i++) {

			hat_devload(kas.a_hat, a, PAGESIZE,
				(phys_pagenum + i), prot, HAT_LOAD_LOCK);
		}

		if (csp != NULL)
			mutex_exit(&csp->s_lock);
	}

	if (mdi->v_vca.version >= 1) {
		mdi->m_mcr.mcr2.pixmode = softc->mdi_pixmode;
		softc->mdi_mon_size = softc->mdi_mon_height *
			softc->mdi_mon_width *
			(softc->mdi_pixel_depth / 8);
		softc->mdi_ndvram_size = softc->mdi_vram_size -
			softc->mdi_mon_size;
		return (0);
	}

	/*
	 *  If we are not on the console fb, then "just do it".
	 *  There is no need to dork with the mappings or with the XLUT.
	 *  Since the prom does not use the non-console fb, we can be
	 *  pretty fast and loose here.
	 */

	/*
	 * Compute the video shift clock which is a function of the mode
	 * in which MDI interprets incoming pixel data (8, 16 or 32 bits/pixel)
	 * the pixel clock frquency and the number of input pixels from the
	 * VBC to the MDI (8, 16 or 32). The VBC always provides 128 bits of
	 * data as input to the MDI every Video Shift Clock.
	 */

/* Method 1 */
	vsfreq = (softc->mdi_mon_pixfreq / 64) * softc->mdi_pixel_depth;
	r_setup = softc->mdi_sam_port_size -
		(((32 + softc->mdi_mihdel) * vsfreq) / softc->mdi_mbus_freq);
#if 0
/* Method 2 */
	i1 = (32 + softc->mdi_mihdel) * softc->mdi_pixel_depth;
	i2 = softc->mdi_mon_pixfreq / (64 * softc->mdi_mbus_freq);
	r_setup = softc->mdi_sam_port_size - (i1 * i2);

/* Method 3 */
	r_setup = softc->mdi_sam_port_size -
	    ((((32 + softc->mdi_mihdel) * softc->mdi_mon_pixfreq)
		* softc->mdi_pixel_depth) / (64 * softc->mdi_mbus_freq));

/* Method 4 */
	vsfreq = ((softc->mdi_mon_pixfreq / 128) * 2) * softc->mdi_pixel_depth;
	r_setup = (softc->mdi_sam_port_size - 1) -
		(((32 + softc->mdi_mihdel) * vsfreq) / softc->mdi_mbus_freq);
#endif

	softc->mdi_v_rsetup = r_setup;
	/*
	*  Now we have all the data we need.
	*  If the user does NOT have video enabled, then we
	*  can do this in user context.  If she does have video
	*  enabled, we must do this at interrupt context since the VBC
	*  could be in the middle of a reload.
	*/
	video_state = mdi_get_video(mdi);
	if (!video_state) {
		/*
		*  User has video off.  We will NEVER see an interrupt.
		*  Just Do It.
		*/
		mdi->m_mcr.mcr1.pixmode = softc->mdi_pixmode;
		mdi->v_mcr.v_mcr1.r_setup = (u_int)softc->mdi_v_rsetup;

		FLUSH_WRITE_BUFFERS(mdi);

	} else {

		softc->mdi_update |= MDI_PIXMODE_UPDATE;

		/* Enable vertical retrace intrs */
		vrt_state = (u_int)mdi_get_vint(mdi);
		mdi_set_vint(mdi, TRUE);

		/* and wait for the interrupt handler to update MDI pixmode */
		while (softc->mdi_update & MDI_PIXMODE_UPDATE)
			cv_wait(&softc->mdi_cv, &softc->mdi_mutex);

		/* restore saved state of VINT */
		mdi_set_vint(mdi, vrt_state);
	}


	softc->mdi_mon_size = softc->mdi_mon_height *
		softc->mdi_mon_width * (softc->mdi_pixel_depth / 8);
	softc->mdi_ndvram_size = softc->mdi_vram_size -
		softc->mdi_mon_size;
	return (0);
}

/*
 * Program the Memory Display Interface's (MDI) vertical/horizontal counters
 */

static int
mdi_setcounters(struct mdi_softc *softc, struct mdi_set_counters *mcp)
{
	volatile struct mdi_register_address *mdi;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	/*
	 * Do a minimum sanity check on the counter values.
	 */

	if ((mcp->m_hss < mcp->m_hsc) && (mcp->m_hsc < mcp->m_hbc) &&
	    (mcp->m_hbc < mcp->m_hbs) && (mcp->m_vss < mcp->m_vsc) &&
	    (mcp->m_vsc < mcp->m_vbc) && (mcp->m_vbc < mcp->m_vbs)) {

		mdi->m_hbs = mcp->m_hbs;	/* horizontal blank start */
		mdi->m_hbc = mcp->m_hbc;	/* horizontal blank clear */
		mdi->m_hss = mcp->m_hss;	/* horizontal sync set */
		mdi->m_hsc = mcp->m_hsc;	/* horizontal sync clear */
		mdi->m_csc = mcp->m_csc;	/* composite sync clear */
		mdi->m_vbs = mcp->m_vbs;	/* vertical blank start	*/
		mdi->m_vbc = mcp->m_vbc;	/* vertical blank clear	*/
		mdi->m_vss = mcp->m_vss;	/* vertical sync start	*/
		mdi->m_vsc = mcp->m_vsc;	/* vertical sync clear	*/
		if (mdi->m_rsr.revision == 0) {
			mdi->m_xcs = mcp->m_xcs;  /* transfer cycle start */
			mdi->m_xcc = mcp->m_xcc;  /* transfer cycle clear */
		}

		FLUSH_WRITE_BUFFERS(mdi);

		return (0);
	}
	else
		return (EINVAL);
}


/*
 * Determine which MDI is interrupting and call mdi_intr() to process the
 * interrupt.
 */

static u_int
mdi_poll(register caddr_t i_handler_arg)
{
	struct mdi_softc *softc = (struct mdi_softc *)i_handler_arg;
	volatile struct mdi_register_address *mdi;
	int retval = DDI_INTR_UNCLAIMED;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	mutex_enter(&softc->mdi_mutex);

	/* Make sure this VSIMM really needs service */
	if (mdi->m_msr & MDI_MSR_INTPEND) {

		/*
		 * Interrupt pending from MDI. Invoke mdi_intr()
		 * to check source of interrupt and initiate the
		 * appropriate action.
		 */
		mdi_intr(softc);

		retval = DDI_INTR_CLAIMED;
	}

	mutex_exit(&softc->mdi_mutex);
	return (retval);
}

static void
mdi_intr(struct mdi_softc *softc)
{
	volatile struct mdi_register_address *mdi;
	struct mdi_cursor_address *mdi_curp;
	struct mdi_shadow_luts *shdw_clutp; /* color table page */
	u_char *src;
	int cmap_index, cmap_count;
	u_char *alpha, *red, *green, *blue;

	mdi = (struct mdi_register_address *)softc->mdi_ctlp;

	if (mdi->m_msr & MDI_MSR_VINT) { /* Vertical retrace interrupt */

		shdw_clutp = softc->mdi_shdw_lut;

		if (shdw_clutp->s_update_flag & MDI_XLUT) {
			struct mdi_xlut_address *table;

			cmap_index = shdw_clutp->s_xlut.mdi_index;
			cmap_count = shdw_clutp->s_xlut.mdi_count;
			src = shdw_clutp->s_xlut.mdi_xlut + cmap_index;

			/* do a write to activate autoinc mode */
			table = (struct mdi_xlut_address *)softc->mdi_xlutp;
			table->x_xlut_inc[cmap_index] = 0;
			mdi_update_hw_xmap(src, cmap_count, softc);

			/* reset the info struct */
			shdw_clutp->s_xlut.mdi_index = 0;
			shdw_clutp->s_xlut.mdi_count = 0;
			shdw_clutp->s_update_flag &= ~MDI_XLUT;
		}
		if (shdw_clutp->s_update_flag & MDI_CLUT1) {
			struct mdi_clut_address *table;

			cmap_index = shdw_clutp->s_clut[0].mdi_index;
			cmap_count = shdw_clutp->s_clut[0].mdi_count;
			alpha = (u_char *)shdw_clutp->s_clut[0].mdi_alpha +
						cmap_index;
			red = (u_char *)shdw_clutp->s_clut[0].mdi_red +
						cmap_index;
			green = (u_char *)shdw_clutp->s_clut[0].mdi_green +
						cmap_index;
			blue = (u_char *)shdw_clutp->s_clut[0].mdi_blue +
						cmap_index;

			/* do a write to activate autoinc mode */
			table = (struct mdi_clut_address *)softc->mdi_clutp[0];
			table->c_clut_inc[cmap_index] = 0;
			mdi_update_hw_cmap(alpha, red, green, blue,
					cmap_count, softc);

			/* reset the info struct */
			shdw_clutp->s_clut[0].mdi_index = 0;
			shdw_clutp->s_clut[0].mdi_count = 0;
			shdw_clutp->s_update_flag &= ~MDI_CLUT1;
		}
		if (shdw_clutp->s_update_flag & MDI_CLUT2) {
			struct mdi_clut_address *table;

			cmap_index = shdw_clutp->s_clut[1].mdi_index;
			cmap_count = shdw_clutp->s_clut[1].mdi_count;
			alpha = (u_char *)shdw_clutp->s_clut[1].mdi_alpha +
						cmap_index;
			red = (u_char *)shdw_clutp->s_clut[1].mdi_red +
						cmap_index;
			green = (u_char *)shdw_clutp->s_clut[1].mdi_green +
						cmap_index;
			blue = (u_char *)shdw_clutp->s_clut[1].mdi_blue +
						cmap_index;

			/* do a write to activate autoinc mode */
			table = (struct mdi_clut_address *)softc->mdi_clutp[1];
			table->c_clut_inc[cmap_index] = 0;
			mdi_update_hw_cmap(alpha, red, green, blue,
					cmap_count, softc);

			/* reset the info struct */
			cmap_index = shdw_clutp->s_clut[1].mdi_index = 0;
			cmap_count = shdw_clutp->s_clut[1].mdi_count = 0;
			shdw_clutp->s_update_flag &= ~MDI_CLUT2;
		}
		if (shdw_clutp->s_update_flag & MDI_CLUT3) {
			struct mdi_clut_address *table;

			cmap_index = shdw_clutp->s_clut[2].mdi_index;
			cmap_count = shdw_clutp->s_clut[2].mdi_count;
			alpha = (u_char *)shdw_clutp->s_clut[2].mdi_alpha +
						cmap_index;
			red = (u_char *)shdw_clutp->s_clut[2].mdi_red +
						cmap_index;
			green = (u_char *)shdw_clutp->s_clut[2].mdi_green +
						cmap_index;
			blue = (u_char *)shdw_clutp->s_clut[2].mdi_blue +
						cmap_index;

			/* do a write to activate autoinc mode */
			table = (struct mdi_clut_address *)softc->mdi_clutp[2];
			table->c_clut_inc[cmap_index] = 0;
			mdi_update_hw_cmap(alpha, red, green, blue,
					cmap_count, softc);

			/* reset the info struct */
			cmap_index = shdw_clutp->s_clut[2].mdi_index = 0;
			cmap_count = shdw_clutp->s_clut[2].mdi_count = 0;
			shdw_clutp->s_update_flag &= ~MDI_CLUT3;
		}
		if (shdw_clutp->s_update_flag & MDI_GAMMALUT) {
		/* We need shorts here, and the names are unavoidable */
			u_short *red, *green, *blue;

			cmap_index = shdw_clutp->s_glut.index;
			cmap_count = shdw_clutp->s_glut.count;
			red = (u_short *)shdw_clutp->s_glut.red +
						cmap_index;
			green = (u_short *)shdw_clutp->s_glut.green +
						cmap_index;
			blue = (u_short *)shdw_clutp->s_glut.blue +
						cmap_index;

			/* Setup the autoincrement address */
			softc->mdi_dacp->dac_addr_reg = (u_char)cmap_index;

			mdi_update_gamma_table(red, green, blue,
					cmap_count, softc);

			/* reset the info struct */
			cmap_index = shdw_clutp->s_glut.index = 0;
			cmap_count = shdw_clutp->s_glut.count = 0;
			shdw_clutp->s_update_flag &= ~MDI_GAMMALUT;
		}

		if (softc->mdi_update & MDI_CURS_UPDATE) {
			/*
			 * Update cursor color registers. Update only the
			 * low order 24 bits of the cursor color register.
			 * The high order 8-bits containing the alpha
			 * information should not be touched.
			 */
			mdi_curp =
				(struct mdi_cursor_address *)softc->mdi_cursp;
			mdi_curp->curs_cc1 = (u_int)
				(softc->mdi_shdw_curcmap[0] & MDI_CURS_PIXVAL);
			mdi_curp->curs_cc2 = (u_int)
				(softc->mdi_shdw_curcmap[1] & MDI_CURS_PIXVAL);

			softc->mdi_update &= ~MDI_CURS_UPDATE;
		}

		/*
		 * Increment shadow vertical retrace counter, wakeup processes
		 * waiting for a vertical retrace interrupt, clear and disable
		 * vertical retrace interrupts if no one has mapped the
		 * vertical retrace counter.
		 */

		/*
		 * If there are processes waiting for the vertical retrace
		 * interrupt, signal them.
		 */
		if (softc->mdi_vrtflag & MDI_VRT_WAKEUP) {
			mutex_enter(&softc->pixrect_mutex);
			softc->mdi_vrtflag &= ~MDI_VRT_WAKEUP;
			cv_broadcast(&softc->mdi_cv);
			mutex_exit(&softc->pixrect_mutex);
		}

		/*
		 * If there are processes which mapped the vertical retrace
		 * counter increment the vertical retrace counter. Otherwise
		 * disable vertical retrace interrupts.
		 */
		if (softc->mdi_vrtflag & MDI_VRT_COUNTER) {

			if (softc->mdi_vrtmappers == 0) {
				softc->mdi_vrtflag &= ~MDI_VRT_COUNTER;
			} else
				*softc->mdi_vrt_vaddr += 1;
		}
		/*
		 * Clear vertical retrace interrupt. A write to the MDI
		 * status register clears the vertical retrace interrupt.
		 * (NOTE: A write to the status register does not affect
		 * the contents of the register, since it is a read only
		 * register).
		 */
		mdi->m_msr = 0;

		/*
		*  for debugging only
		*  This code lets us see what the current reading
		*  of the vertical counter is.
		*/
		new_vcnt = mdi_get_vcntr(mdi);
		if (new_vcnt > exit_vcnt)
			exit_vcnt = new_vcnt;

	}

	/*
	 * Check for interrupts posted due to a fault. MDI posts a fault
	 * interrupt when a read is attempted at a write only address or a
	 * write is attempted at a read only address in the control address
	 * space. The fault status register contains the reason for the
	 * fault. The interrupt is cleared by reading the low order byte
	 * of the fault address register.
	 */
	if (mdi->m_msr & MDI_MSR_FAULT) {
		/*
		 *  This first read of the FSR will clear the MDI_MSR_FAULT
		 */
		if (mdi->m_fsr & MDI_FSR_UNIMP) {
			u_short fault_addr = mdi->m_fsa;

			/* allow access to XCS and XCC for now */
			if (! (mdi->m_rsr.revision != 0 &&
				(fault_addr == 0x2a || fault_addr == 0x2c))) {

				cmn_err(CE_WARN,
					"MDI Unimplemented addr fault ");
				cmn_err(CE_CONT,
					"on VSIMM #%d, address = %x\n",
					ddi_get_instance(softc->dip),
					fault_addr);
			}
		} else if (mdi->m_fsr & MDI_FSR_WERR) {
			u_short fault_addr = mdi->m_fsa;

			/*
			 * Ignore the noise that causes a write to the
			 * read-only Test Mode Status reguster 0 while resume.
			 * Writing to TMS has no effect.
			 */
			if (fault_addr != 0x2) {
				cmn_err(CE_WARN, "MDI Write fault ");
				cmn_err(CE_CONT, "on VSIMM #%d, address = %x\n",
					ddi_get_instance(softc->dip),
					fault_addr);
			}
		}
	}

	FLUSH_WRITE_BUFFERS(mdi);
}

/*
 * Return information pertinent to the type of object that is mapped.
 */
static int
mdi_getmapinfo(
	dev_t dev,
	u_int offset,
	u_int *objtype,
	u_int *prot_allowed,
	u_int *map_allowed,
	u_int *len_allowed,
	u_int *pf)
{
	int	mapindex;
	u_int	paddr;

	register struct mappings *mp;
	register struct mdi_softc *softc = getsoftc(getminor(dev));
	volatile struct mdi_register_address *mdi;

	/* for CG3_MMAP_OFFSET bug, test for 0 */
	if ((offset == (u_int)NULL) ||
	    ((offset >= (u_int)CG3_MMAP_OFFSET) &&
		(offset < (u_int)MDI_DIRECT_MAP))) {
		mapindex = MDI_CG3_INDX;
		*objtype = (u_int)CG3_MMAP_OFFSET;
	}

	/* XXX should check cred for root here! */
	else if ((offset >= (u_int)MDI_DIRECT_MAP) &&
	    (offset < (u_int)MDI_CTLREG_MAP) && directmap_allowed) {
		mapindex = MDI_ADDR_INDX;
		*objtype = (u_int)MDI_DIRECT_MAP;
	}

	else if ((offset >= (u_int)MDI_CTLREG_MAP) &&
	    (offset < (u_int)MDI_CURSOR_MAP)) {
		mapindex = MDI_CTLREG_INDX;
		*objtype = (u_int)MDI_CTLREG_MAP;
	}

	else if ((offset >= (u_int)MDI_CURSOR_MAP) &&
	    (offset < (u_int)MDI_SHDW_VRT_MAP)) {
		mapindex = MDI_CURSOR_INDX;
		*objtype = (u_int)MDI_CURSOR_MAP;
	}

	else if ((offset >= (u_int)MDI_SHDW_VRT_MAP) &&
	    (offset < (u_int)MDI_CHUNKY_XBGR_MAP)) {
		mapindex = MDI_SHDW_VRT_INDX;
		*objtype = (u_int)MDI_SHDW_VRT_MAP;
	}

	else if ((offset >= (u_int)MDI_CHUNKY_XBGR_MAP) &&
	    (offset < (u_int)MDI_CHUNKY_BGR_MAP)) {
		mapindex = MDI_CHUNKY_XBGR_INDX;
		*objtype = (u_int)MDI_CHUNKY_XBGR_MAP;
	}

	else if ((offset >= (u_int)MDI_CHUNKY_BGR_MAP) &&
	    (offset < (u_int)MDI_PLANAR_X16_MAP)) {
		mapindex = MDI_CHUNKY_BGR_INDX;
		*objtype = (u_int)MDI_CHUNKY_BGR_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_X16_MAP) &&
	    (offset < (u_int)MDI_PLANAR_C16_MAP)) {
		mapindex = MDI_PLANAR_X16_INDX;
		*objtype = (u_int)MDI_PLANAR_X16_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_C16_MAP) &&
	    (offset < (u_int)MDI_PLANAR_X32_MAP)) {
		mapindex = MDI_PLANAR_C16_INDX;
		*objtype = (u_int)MDI_PLANAR_C16_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_X32_MAP) &&
	    (offset < (u_int)MDI_PLANAR_B32_MAP)) {
		mapindex = MDI_PLANAR_X32_INDX;
		*objtype = (u_int)MDI_PLANAR_X32_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_B32_MAP) &&
	    (offset < (u_int)MDI_PLANAR_G32_MAP)) {
		mapindex = MDI_PLANAR_B32_INDX;
		*objtype = (u_int)MDI_PLANAR_B32_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_G32_MAP) &&
	    (offset < (u_int)MDI_PLANAR_R32_MAP)) {
		mapindex = MDI_PLANAR_G32_INDX;
		*objtype = (u_int)MDI_PLANAR_G32_MAP;
	}

	else if ((offset >= (u_int)MDI_PLANAR_R32_MAP)) {
		mapindex = MDI_PLANAR_R32_INDX;
		*objtype = (u_int)MDI_PLANAR_R32_MAP;
	}

	else
		return (-1); /* Object not mapped by the VSIMM segment driver */

	mp = softc->mdi_maptable;

	*prot_allowed = mp[mapindex].prot;
	*map_allowed = mp[mapindex].map_type;

	if (*objtype == (u_int)CG3_MMAP_OFFSET) {

		/*
		 * For cgthree emulation physical page frame number depends
		 * upon the MDI pixel mode.
		 */
		mdi = (struct mdi_register_address *)softc->mdi_ctlp;
		switch (mdi->m_mcr.mcr2.pixmode) {

			case MDI_MCR_8PIX:
				paddr = (softc->mdi_vram_paddr &
					~CHUNKY_XBGR_MODE) | CHUNKY_XBGR_MODE;
				break;

			case MDI_MCR_16PIX:
				paddr = (softc->mdi_vram_paddr &
					~PLANAR_X16_MODE) | PLANAR_X16_MODE;
				break;

			case MDI_MCR_32PIX:
				paddr = (softc->mdi_vram_paddr &
					~PLANAR_X32_MODE) | PLANAR_X32_MODE;
				break;
			default:
				return (-1);
		}
		*pf = btop(paddr);
		*len_allowed = mp[MDI_CG3_INDX].length;

	} else {
		*pf = mp[mapindex].pagenum;
		*len_allowed = mp[mapindex].length;
	}

	return (0);

}

/*ARGSUSED*/
void
mdi_printf(int level, char *fmt, ...)
{
	va_list adx;

	if (!debug)
		return;

	va_start(adx, fmt);
	vcmn_err(level, fmt, adx);
	va_end(adx);
}

static void
mdi_set_video(volatile struct mdi_register_address *mdi, int on)
{
	if (mdi->m_rsr.revision == 0)
		mdi->m_mcr.mcr1.vid_ena = on & 1;
	else
		mdi->m_mcr.mcr2.vid_ena = on & 1;
}

static int
mdi_get_video(volatile struct mdi_register_address *mdi)
{
	if (mdi->m_rsr.revision == 0)
		return (mdi->m_mcr.mcr1.vid_ena);
	else
		return (mdi->m_mcr.mcr2.vid_ena);
}

static int
mdi_get_blanking(volatile struct mdi_register_address *mdi)
{
	/* VSIMM 1 has no blanking bit */
	if (mdi->m_rsr.revision != 0)
		return (mdi->m_mcr.mcr2.blank);

	return (0);
}

static void
mdi_set_blanking(volatile struct mdi_register_address *mdi, int on)
{
	/* VSIMM 1 has no blanking bit */
	if (mdi->m_rsr.revision != 0)
		mdi->m_mcr.mcr2.blank = on & 1;
}

static void
mdi_free_resources(struct mdi_softc *softc)
{
	/*
	 * Free all allocated resources
	 */
	if (softc->mdi_shdw_lut != NULL) {
		kmem_free(softc->mdi_shdw_lut, sizeof (struct mdi_shadow_luts));
		softc->mdi_shdw_lut = NULL;
	}
	if (softc->mdi_degamma != NULL) {
		kmem_free(softc->mdi_degamma,
		    sizeof (struct mdi_degammalut_data));
		softc->mdi_degamma = NULL;
	}
	if (softc->mdi_cur.image != NULL) {
		kmem_free(softc->mdi_cur.image,
		    MDI_CURS_SIZE * sizeof (int));
		softc->mdi_cur.image = NULL;
	}
	if (softc->mdi_cur.mask != NULL) {
		kmem_free(softc->mdi_cur.mask,
		    MDI_CURS_SIZE * sizeof (int));
		softc->mdi_cur.mask = NULL;
	}
	if (softc->mdi_vrt_vaddr != NULL) {
		kmem_free(softc->mdi_vrt_vaddr, PAGESIZE);
		softc->mdi_vrt_vaddr = NULL;
	}
	if (softc->intr_added) {
		ddi_remove_intr(softc->dip, MDI_CONTROL_SPACE,
		    softc->mdi_iblkc);
		softc->intr_added = B_FALSE;
	}
	if (softc->mdi_control_regs_mapped) {
		ddi_unmap_regs(softc->dip, (u_int)MDI_CONTROL_SPACE,
		    (caddr_t *)&softc->mdi_ctlp, (off_t)0,
		    (off_t)MDI_MAPSIZE);
		softc->mdi_control_regs_mapped = B_FALSE;
	}
	if (softc->mdi_vram_space_mapped) {
		/*
		 * XXX - Have to revisit this if we ever just map 1
		 * page of vram
		 */
		ddi_unmap_regs(softc->dip, (u_int)MDI_VRAM_SPACE,
		    (caddr_t *)&softc->mdi_vram_vaddr, (off_t)0,
		    (off_t)softc->mdi_vram_size);
		softc->mdi_vram_space_mapped = B_FALSE;
	}

	/* Free up the monitor power management backup resource */
	if (softc->mdi_vesa_save) {
		kmem_free(softc->mdi_vesa_save,
		    sizeof (struct mdi_vesa_regs));
	}

	ddi_remove_minor_node(softc->dip, NULL);
	if (softc->mutexen_initialized) {
		mutex_destroy(&softc->mdi_mutex);
		mutex_destroy(&softc->pixrect_mutex);
		mutex_destroy(&softc->mdi_degammalock);
	}
}

/*
 * given the whf_t, find the corresponding struct mdi_set_resolution.
 */
static struct mdi_set_resolution *
mdi_get_timing_params(whf_t *res_whf)
{
	mon_spec_t *p;
	int i;

	/* Table walk */
	for (i = 0; mon_spec_table[i].ms_whf.width; i++) {
		p = (struct mon_spec *)&mon_spec_table[i];
		if (p->ms_whf.width == res_whf->width &&
			p->ms_whf.height == res_whf->height &&
			p->ms_whf.vfreq == res_whf->vfreq)
				return (&(p->ms_msr));
	}

	return ((struct mdi_set_resolution *)NULL);
}


static int
mdi_get_pixelfreq(struct mdi_softc *softc)
{
	whf_t res_whf;
	struct mdi_set_resolution *msr;

	res_whf.width = softc->mdi_mon_width;
	res_whf.height = softc->mdi_mon_height;
	res_whf.vfreq = softc->mdi_mon_vfreq;
	msr = mdi_get_timing_params(&res_whf);
	ASSERT(msr);
	return (msr->pixelfreq);
}


static int
mdi_set_res(struct mdi_softc *softc, struct mdi_set_resolution *res)
{
	int error = 0;
	struct mdi_set_counters cntrs;

	ASSERT(MUTEX_HELD(&softc->mdi_mutex));

	/* Update MDI counters */
	(void) mdi_set_timing_params(&cntrs, res->hfporch,
		res->hsync, res->hbporch, res->hvistime, res->vfporch,
		res->vsync, res->vbporch, res->vvistime);

	error = mdi_setcounters(softc, &cntrs);

	if (error) {
		return (error);
	}

	/* Now program new pixelclock into PCG */
	softc->mdi_mon_pixfreq = res->pixelfreq;
	error = mdi_set_pixelfreq(softc);

	if (error) {
		return (error);
	}

	/* Ok.  We succeeded in changing the res.  Now export it */
	softc->mdi_mon_pixfreq = res->pixelfreq;
	softc->mdi_mon_height = res->vvistime;
	softc->mdi_mon_width = res->hvistime;
	softc->mdi_mon_vfreq = res->vfreq;
	softc->mdi_mon_size = softc->mdi_mon_height *
		softc->mdi_mon_width * (softc->mdi_pixel_depth / 8);
	softc->mdi_ndvram_size = softc->mdi_vram_size - softc->mdi_mon_size;

	return (0);
}

static void
mdi_set_timing_params(struct mdi_set_counters *cntrs,
	int hfporch,
	int hsync,
	int hbporch,
	int hvistime,
	int vfporch,
	int vsync,
	int vbporch,
	int vvistime)
{
	cntrs->m_hss = MAX(0, (hfporch / 4) - 1);
	cntrs->m_hsc = MAX(0, ((hfporch + hsync) / 4) - 1);
	cntrs->m_hbc = MAX(0, ((hfporch + hsync + hbporch) / 4) - 1);
	cntrs->m_hbs = MAX(0, ((hfporch + hsync + hbporch + hvistime) / 4) - 1);

	cntrs->m_vss = MAX(0, vfporch - 1);
	cntrs->m_vsc = MAX(0, vfporch + vsync - 1);
	cntrs->m_vbc = MAX(0, vfporch + vsync + vbporch - 1);
	cntrs->m_vbs = MAX(0, vfporch + vsync + vbporch + vvistime - 1);

	cntrs->m_csc = ((hfporch + hbporch + hvistime) / 4) - 1;

}

static int
mdi_set_pixelfreq(struct mdi_softc *softc)
{
	u_char *pcg_regs;

	pcg_regs = lookup_pcg_regs(softc->mdi_mon_pixfreq);
	if (pcg_regs == (u_char *)NULL)
		return (EINVAL);

	(void) write_pcg_regs(softc, pcg_regs);

	return (0);
}

static void
write_pcg_regs(struct mdi_softc *softc, u_char pcg[])
{
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	int i;
	volatile u_char tmp;

	mdi->m_ccr |= MDI_CCR_PCGSDAT_DIRSEL;
	mdi->m_ccr |= MDI_CCR_PCGSDAT;

	for (i = 0; i < MDI_ICS_NREGS; i++) {
		/* write address */
		tmp = (mdi->m_ccr & ~(MDI_CCR_DATABITS | MDI_CCR_PCGSDAT)) |
			(i << MDI_CCR_DATASHIFT) | MDI_CCR_PCGSDAT_DIRSEL;
		mdi->m_ccr = tmp;

		/* write data */
		tmp = (mdi->m_ccr & ~MDI_CCR_DATABITS) |
			(pcg[i] << MDI_CCR_DATASHIFT) |
			MDI_CCR_PCGSDAT_DIRSEL | MDI_CCR_PCGSDAT;
		mdi->m_ccr = tmp;
	}

	/*
	 * The ICS1562 part requires 32 writes before any of them
	 * become effective.  We do this by writing (32 - MDI_ICS_NREGS)
	 * times of register 0 data into register 0.
	 */
	for (i = 0; i < (32 - MDI_ICS_NREGS); i++) {
		/* write address */
		tmp = (mdi->m_ccr & ~(MDI_CCR_DATABITS | MDI_CCR_PCGSDAT)) |
			(0 << MDI_CCR_DATASHIFT) |
			MDI_CCR_PCGSDAT_DIRSEL;
		mdi->m_ccr = tmp;

		/* write data */
		tmp = (mdi->m_ccr & ~MDI_CCR_DATABITS) |
			(pcg[0] << MDI_CCR_DATASHIFT) |
			MDI_CCR_PCGSDAT_DIRSEL | MDI_CCR_PCGSDAT;
		mdi->m_ccr = tmp;
	}
}

static u_char *
lookup_pcg_regs(u_int pixelfreq)
{
	int i;

	/* Table walk */
	for (i = 0; mdi_mc[i].mc_pixelfreq; i++) {
		if (mdi_mc[i].mc_pixelfreq == pixelfreq)
			return ((u_char *)&(mdi_mc[i].mc_ics1562[0]));
	}

	return ((u_char *)NULL);
}


static int
mdi_do_suspend(struct mdi_softc *softc)
{
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	volatile struct mdi_register_address *mdi_save;
	struct mdi_cursor_address *cursor = softc->mdi_cursp;
	struct mdi_cursor_address *cursor_save;
	caddr_t vaddr, ndvram, ndvram_save;
	int	size, i;

	mutex_enter(&softc->mdi_mutex);

	/*
	 * Alloc memory space to save hardware state
	 */
	size = sizeof (struct mdi_register_address) +
		sizeof (struct mdi_cursor_address) +
		softc->mdi_ndvram_size;

	vaddr = (caddr_t)kmem_alloc(size, KM_NOSLEEP);

	if (vaddr == NULL)
		return (DDI_FAILURE);

	/* Backup space for the control registers */
	softc->mdi_save_ctlp = (volatile struct mdi_register_address *)vaddr;
	mdi_save = softc->mdi_save_ctlp;

	/* Backup space for the cursor states */
	softc->mdi_save_cursp = (struct mdi_cursor_address *)
		(vaddr + sizeof (struct mdi_register_address));
	cursor_save = softc->mdi_save_cursp;

	/* Backup space for the non-displayable VRAM */
	softc->mdi_ndvram_save = (caddr_t)(vaddr +
		sizeof (struct mdi_register_address) +
		sizeof (struct mdi_cursor_address));
	ndvram_save = softc->mdi_ndvram_save;


	/*
	 *****************************************
	 * PAGE 0: Control Registers, PCG and VBC
	 *****************************************
	 */

	/*
	 * Save video_enable, blanking and vertical retrace states first,
	 * because we are going to critical section to save the states.
	 */
	mdi_save->m_mcr.mcr2.intr_ena = mdi->m_mcr.mcr2.intr_ena;
	mdi_save->m_mcr.mcr2.blank = mdi->m_mcr.mcr2.blank;
	mdi_save->m_mcr.mcr2.vid_ena = mdi->m_mcr.mcr2.vid_ena;

	/*
	 * Turn off video and disable vertical retrace interrupt to
	 * make sure there is nothing sneaking on.
	 */
	mdi_set_blanking(mdi, 0);
	mdi_set_video(softc->mdi_ctlp, FALSE);
	mdi_set_vint(mdi, FALSE);

	/* Save mcr, ppr, ccr, mod, acr registers */
	mdi_save->m_mcr.mcr2.pixmode = mdi->m_mcr.mcr2.pixmode;
	mdi_save->m_mcr.mcr2.tm_ena = mdi->m_mcr.mcr2.tm_ena;
	mdi_save->m_ppr = mdi->m_ppr;
	mdi_save->m_ccr = mdi->m_ccr;
	mdi_save->m_mod = mdi->m_mod;
	mdi_save->m_acr = mdi->m_acr;

	/* Save horizontal and vertical sync and blank registers */
	mdi_save->m_hbs = mdi->m_hbs;
	mdi_save->m_hbc = mdi->m_hbc;
	mdi_save->m_hss = mdi->m_hss;
	mdi_save->m_hsc = mdi->m_hsc;
	mdi_save->m_csc = mdi->m_csc;
	mdi_save->m_vbs = mdi->m_vbs;
	mdi_save->m_vbc = mdi->m_vbc;
	mdi_save->m_vss = mdi->m_vss;
	mdi_save->m_vsc = mdi->m_vsc;

	/* PCG is write-only, we have table to look up, so no need to save */

	/* VBC Address Space */
	mdi_save->v_vbr.framebase = mdi->v_vbr.framebase;
	mdi_save->v_mcr.v_mcr2.fbconfig = mdi->v_mcr.v_mcr2.fbconfig;
	mdi_save->v_mcr.v_mcr2.trc = mdi->v_mcr.v_mcr2.trc;
	mdi_save->v_mcr.v_mcr2.refresh = mdi->v_mcr.v_mcr2.refresh;
	mdi_save->v_vcr.v_vcr2.ref_req = mdi->v_vcr.v_vcr2.ref_req;
	mdi_save->v_vca.hires = mdi->v_vca.hires;
	mdi_save->v_vca.ramspeed = mdi->v_vca.ramspeed;
	mdi_save->v_vca.version = mdi->v_vca.version;
	mdi_save->v_vca.cad = mdi->v_vca.cad;


	/*
	 ****************************************
	 * PAGE 1: Cursor CSRs and Address Space
	 ****************************************
	 */

	for (i = 0; i < MDI_CURS_SIZE; i++) {
		cursor_save->curs_cpl0[i] = cursor->curs_cpl0[i];
		cursor_save->curs_cpl1[i] = cursor->curs_cpl1[i];
	}
	cursor_save->curs_ccr = cursor->curs_ccr;
	cursor_save->curs_xcu = cursor->curs_xcu;
	cursor_save->curs_ycu = cursor->curs_ycu;


	/*
	 ***********************************************************
	 * PAGE 2: DAC Address Space
	 * PAGE 3, 4, 5, 6: XLUT, CLUT1, CLUT2. CLUT3 is not implemented.
	 *
	 * These tables are already saved in softc->mdi_shdw_lut, so
	 * no need to save them here.
	 ***********************************************************
	 */


	/*
	 ***********************************************************
	 * PAGE 7 - 14: reserved.
	 * PAGE 15: Autoincrement Address Space (don't care)
	 ***********************************************************
	 */


	/*
	 ***********************************************************
	 * Non-displayable VRAM
	 ***********************************************************
	 */

	ndvram = softc->mdi_vram_vaddr + softc->mdi_mon_size;
	bcopy(ndvram, ndvram_save, softc->mdi_ndvram_size);


	/*
	 * Restore video_enable, blanking and vertical retrace states
	 * because we are done with the critical section and users
	 * should continue to see the spinning bar for the cpr progress.
	 */
	mdi->m_mcr.mcr2.intr_ena = mdi_save->m_mcr.mcr2.intr_ena;
	mdi->m_mcr.mcr2.blank = mdi_save->m_mcr.mcr2.blank;
	mdi->m_mcr.mcr2.vid_ena = mdi_save->m_mcr.mcr2.vid_ena;

	softc->mdi_suspended = B_TRUE;
	mutex_exit(&softc->mdi_mutex);

	return (DDI_SUCCESS);
}


static int
mdi_do_resume(struct mdi_softc *softc)
{
	int i, error, size = 0;
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	volatile struct mdi_register_address *mdi_save = softc->mdi_save_ctlp;
	struct mdi_cursor_address *cursor_save = softc->mdi_save_cursp;
	struct mdi_shadow_luts *shdw_clutp = softc->mdi_shdw_lut;
	struct mdi_cursor_info *curs_info;
	caddr_t ndvram, ndvram_save;

	mutex_enter(&softc->mdi_mutex);


	/*
	 ***********************************************************
	 * PAGE 0: Control Registers, PCG and VBC
	 ***********************************************************
	 */

	/*
	 * Turn off video and disable vertical retrace interrupt to
	 * make sure there is nothing sneaking on.
	 */
	mdi_set_blanking(mdi, 0);
	mdi_set_video(softc->mdi_ctlp, FALSE);
	mdi_set_vint(mdi, FALSE);

	/* Clean up the screen first. */
	bzero(softc->mdi_vram_vaddr, softc->mdi_vram_size);

	/* PCG info can be got from the static table. */
	error = mdi_set_pixelfreq(softc);
	if (error) {
		mutex_exit(&softc->mdi_mutex);
		return (DDI_FAILURE);
	}

	/* VBC Address Space */
	mdi->v_vbr.framebase = mdi_save->v_vbr.framebase;
	mdi->v_mcr.v_mcr2.fbconfig = mdi_save->v_mcr.v_mcr2.fbconfig;
	mdi->v_mcr.v_mcr2.trc = mdi_save->v_mcr.v_mcr2.trc;
	mdi->v_mcr.v_mcr2.refresh = mdi_save->v_mcr.v_mcr2.refresh;
	mdi->v_vcr.v_vcr2.ref_req = mdi_save->v_vcr.v_vcr2.ref_req;
	mdi->v_vca.hires = mdi_save->v_vca.hires;
	mdi->v_vca.ramspeed = mdi_save->v_vca.ramspeed;
	mdi->v_vca.version = mdi_save->v_vca.version;
	mdi->v_vca.cad = mdi_save->v_vca.cad;

	/* Restore ppr, ccr, mod and acr registers */
	mdi->m_ppr = mdi_save->m_ppr;
	mdi->m_ccr = mdi_save->m_ccr;
	mdi->m_mod = mdi_save->m_mod & 0xFB; /* bit 2 is read-only */
	mdi->m_acr = mdi_save->m_acr;

	/* Restore the horizontal and vertical blank and sync registers */
	mdi->m_hbs = mdi_save->m_hbs;
	mdi->m_hbc = mdi_save->m_hbc;
	mdi->m_hss = mdi_save->m_hss;
	mdi->m_hsc = mdi_save->m_hsc;
	mdi->m_csc = mdi_save->m_csc;
	mdi->m_vbs = mdi_save->m_vbs;
	mdi->m_vbc = mdi_save->m_vbc;
	mdi->m_vss = mdi_save->m_vss;
	mdi->m_vsc = mdi_save->m_vsc;

	/* Restore the mcr the last. */
	mdi->m_mcr.mcr2.pixmode = mdi_save->m_mcr.mcr2.pixmode;
	mdi->m_mcr.mcr2.tm_ena = mdi_save->m_mcr.mcr2.tm_ena;

	FLUSH_WRITE_BUFFERS(mdi);



	/*
	 ***********************************************************
	 * PAGE 1: Cursor CSRs and Address Space
	 *
	 * NOTE: Due to a hardware bug, to restore cursor planes, we
	 * need to call mdi_set_cursor() to include the cursor planes
	 * write retry workaround. It also updates curs_xcu and curs_ycu.
	 ***********************************************************
	 */

	/* Prepare a parameter suitable for mdi_set_cursor() call */
	curs_info = (struct mdi_cursor_info *)kmem_alloc(
		sizeof (struct mdi_cursor_info), KM_NOSLEEP);
	if (curs_info == NULL) {
		mutex_exit(&softc->mdi_mutex);
		return (DDI_FAILURE);
	}

	for (i = 0; i < MDI_CURS_SIZE; i++)
		curs_info->curs_enable0[i] = cursor_save->curs_cpl0[i];
	curs_info->curs_ctl = cursor_save->curs_ccr;
	curs_info->curs_xpos = cursor_save->curs_xcu;
	curs_info->curs_ypos = cursor_save->curs_ycu;

	/* Drop the mutex because mdi_set_cursor() will grap it again. */
	mutex_exit(&softc->mdi_mutex);
	(void) mdi_set_cursor(softc, curs_info);

	mutex_enter(&softc->mdi_mutex);

	/*
	 * Now set the update flag, enable vertical retrace interrupt and
	 * wait for the handler to update hw cursor.
	 */
	softc->mdi_update |=  MDI_CURS_UPDATE;
	softc->mdi_vrtflag |= MDI_VRT_WAKEUP;
	mdi_set_blanking(mdi, 1);
	mdi_set_video(softc->mdi_ctlp, TRUE);
	mdi_set_vint(mdi, TRUE);
	while (softc->mdi_vrtflag & MDI_VRT_WAKEUP)
		cv_wait(&softc->mdi_cv, &softc->mdi_mutex);

	kmem_free(curs_info, sizeof (struct mdi_cursor_info));


	/*
	 ***********************************************************
	 * PAGE 2: DAC Address Space
	 * PAGE 3, 4, 5, 6: XLUT, CLUT1, CLUT2. CLUT3 is not implemented.
	 *
	 * Make sure they are updated.
	 ***********************************************************
	 */
	/*
	 * Turn off video and disable vertical retrace interrupt to
	 * make sure there is nothing sneaking on.
	 */
	mdi_set_blanking(mdi, 0);
	mdi_set_video(softc->mdi_ctlp, FALSE);
	mdi_set_vint(mdi, FALSE);

	shdw_clutp->s_glut.index = 0;
	shdw_clutp->s_glut.count = MDI_CMAP_ENTRIES;
	shdw_clutp->s_update_flag |= MDI_GAMMALUT;

	shdw_clutp->s_xlut.mdi_index = 0;
	shdw_clutp->s_xlut.mdi_count = MDI_CMAP_ENTRIES;
	shdw_clutp->s_update_flag |= MDI_XLUT;

	shdw_clutp->s_clut[0].mdi_index = 0;
	shdw_clutp->s_clut[0].mdi_count = MDI_CMAP_ENTRIES;
	shdw_clutp->s_update_flag |= MDI_CLUT1;

	shdw_clutp->s_clut[1].mdi_index = 0;
	shdw_clutp->s_clut[1].mdi_count = MDI_CMAP_ENTRIES;
	shdw_clutp->s_update_flag |= MDI_CLUT2;

	/*
	 * Now Turn on video and enable vertical retrace interrupt and
	 * wait for the handler to update luts.
	 */
	softc->mdi_vrtflag |= MDI_VRT_WAKEUP;
	mdi_set_blanking(mdi, 1);
	mdi_set_video(softc->mdi_ctlp, TRUE);
	mdi_set_vint(mdi, TRUE);
	while (softc->mdi_vrtflag & MDI_VRT_WAKEUP)
		cv_wait(&softc->mdi_cv, &softc->mdi_mutex);


	/*
	 ***********************************************************
	 * PAGE 7 - 14: reserved.
	 * PAGE 15: Autoincrement Address Space (don't care)
	 ***********************************************************
	 */


	/*
	 ***********************************************************
	 * Non-displayable VRAM
	 ***********************************************************
	 */
	/* Clear up the screen first. */
	bzero(softc->mdi_vram_vaddr, softc->mdi_mon_size);
	ndvram = softc->mdi_vram_vaddr + softc->mdi_mon_size;
	ndvram_save = softc->mdi_ndvram_save;
	bcopy(ndvram_save, ndvram, softc->mdi_ndvram_size);


	/* Restore the vertical retrace interrupt the last. */
	mdi->m_mcr.mcr2.intr_ena = mdi_save->m_mcr.mcr2.intr_ena;


	/*
	 * Restore the video_enable, blanking and vertical retrace interrupt
	 * the last.
	 */

	mdi->m_mcr.mcr2.intr_ena = mdi_save->m_mcr.mcr2.intr_ena;
	mdi->m_mcr.mcr2.blank = mdi_save->m_mcr.mcr2.blank;
	mdi->m_mcr.mcr2.vid_ena = mdi_save->m_mcr.mcr2.vid_ena;

	FLUSH_WRITE_BUFFERS(mdi);
	softc->mdi_suspended = B_FALSE;

	/*
	 * Free up the hardware state backup resource
	 */
	size = sizeof (struct mdi_register_address) +
		sizeof (struct mdi_cursor_address) +
		softc->mdi_ndvram_size;
	kmem_free((caddr_t)softc->mdi_save_ctlp, size);

	mutex_exit(&softc->mdi_mutex);

	return (DDI_SUCCESS);
}


static int
mdi_monitor_down(struct mdi_softc *softc)
{
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	volatile struct mdi_vesa_regs	*vesa_save;

	mutex_enter(&softc->mdi_mutex);
	ASSERT(MUTEX_HELD(&softc->mdi_mutex));

	vesa_save = softc->mdi_vesa_save;

	/* If the sync registers are on, save them before we turn them off. */
	if (softc->mdi_sync_on) {
		vesa_save->m_hss = mdi->m_hss;
		vesa_save->m_hsc = mdi->m_hsc;
		vesa_save->m_vss = mdi->m_vss;
		vesa_save->m_vsc = mdi->m_vsc;
		vesa_save->video_state = mdi_get_video(mdi);
	}

	/*
	 * Blank and disable video before we halt the sync registers
	 * to prevent a jumping screen.
	 */
	mdi_set_blanking(mdi, 0); /* blank bit active low */
	mdi_set_video(mdi, FALSE);

	/* Halt the sync registers */
	mdi->m_hss = 0;
	mdi->m_hsc = 0;
	mdi->m_vss = 0;
	mdi->m_vsc = 0;
	softc->mdi_sync_on = 0;

	mutex_exit(&softc->mdi_mutex);

	return (DDI_SUCCESS);
}


static int
mdi_monitor_up(struct mdi_softc *softc)
{
	volatile struct mdi_register_address *mdi = softc->mdi_ctlp;
	volatile struct mdi_vesa_regs	*vesa_save = softc->mdi_vesa_save;

	mutex_enter(&softc->mdi_mutex);

	/*
	 * Restore the states of the sync registers if called from off
	 * to on.
	 */
	if (!softc->mdi_sync_on) {
		mdi->m_hss = vesa_save->m_hss;
		mdi->m_hsc = vesa_save->m_hsc;
		mdi->m_vss = vesa_save->m_vss;
		mdi->m_vsc = vesa_save->m_vsc;
		softc->mdi_sync_on = 1;
	}

	/*
	 * Blank and restore the video state.
	 * XXX Need to stagger in about 4 seconds of delay for pre-p4 monitors
	 * to sync up with the video sync signal. That will prevent the ugly
	 * screen jumping. However if we do so, the coming up time for p4
	 * monitors will be too long because p4 monitors already has the
	 * delay built in.
	 */
	mdi_set_video(mdi, TRUE);
	mdi_set_blanking(mdi, 1); /* blank bit active low */

	mutex_exit(&softc->mdi_mutex);

	return (DDI_SUCCESS);
}

/*
 * Call back handler to report the non-displayable VRAM size.
 * Caller needs to pass in a pointer to the return value.
 */
/*ARGSUSED*/
boolean_t
mdi_callb(void *arg, int code)
{
	int *rval = (int *)code;
	struct mdi_softc *softc = arg;

	*rval = (int)softc->mdi_ndvram_size;
	return (B_TRUE);
}
