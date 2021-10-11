/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vgatext.c	1.17	99/11/17 SMI"

#include "sys/errno.h"
#include "sys/types.h"
#include "sys/conf.h"
#include "sys/kmem.h"
#include <sys/visual_io.h>
#include "sys/font.h"
#include "sys/fbio.h"

#include "sys/ddi.h"
#include "sys/stat.h"
#include "sys/sunddi.h"
#include <sys/file.h>
#include <sys/open.h>
#include <sys/modctl.h>
#include <sys/vgareg.h>
#include <sys/vgasubr.h>
#include <sys/pci.h>
#include <sys/kd.h>
#include <sys/ddi_impldefs.h>

#define	MYNAME	"vgatext"

/* I don't know exactly where these should be defined, but this is a	*/
/* heck of a lot better than constants in the code.			*/
#define	TEXT_ROWS		25
#define	TEXT_COLS		80

#define	VGA_BRIGHT_WHITE	0x0f
#define	VGA_BLACK		0x00

#define	VGA_REG_ADDR		0x3c0
#define	VGA_REG_SIZE		0x20

#define	VGA_MEM_ADDR		0xa0000
#define	VGA_MEM_SIZE		0x20000

#define	VGA_MMAP_FB_BASE	VGA_MEM_ADDR

static int vgatext_open(dev_t *, int, int, cred_t *);
static int vgatext_close(dev_t, int, int, cred_t *);
static int vgatext_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int vgatext_mmap(dev_t, off_t, int);

static 	struct cb_ops cb_vgatext_ops = {
	vgatext_open,		/* cb_open */
	vgatext_close,		/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	vgatext_ioctl,		/* cb_ioctl */
	nodev,			/* cb_devmap */
	vgatext_mmap,		/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* cb_stream */
	D_NEW | D_MTSAFE	/* cb_flag */
};


static int vgatext_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int vgatext_attach(dev_info_t *, ddi_attach_cmd_t);
static int vgatext_detach(dev_info_t *, ddi_detach_cmd_t);

static struct vis_identifier text_ident = { "SUNWtext" };

struct dev_ops vgatext_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	vgatext_info,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	nulldev,		/* devo_probe */
	vgatext_attach,		/* devo_attach */
	vgatext_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_vgatext_ops,		/* devo_cb_ops */
	(struct bus_ops *)NULL,	/* devo_bus_ops */
	NULL			/* power */
};

struct vgatext_softc {
	struct vgaregmap 	regs;
	struct vgaregmap 	fb;
	off_t			fb_size;
	dev_info_t		*devi;
	int			mode;	/* KD_TEXT or KD_GRAPHICS */
	caddr_t			text_base;	/* hardware text base */
	char			shadow[TEXT_ROWS*TEXT_COLS*2];
	caddr_t			current_base;	/* hardware or shadow */
	struct {
		boolean_t visible;
		int row;
		int col;
	}			cursor;
	struct vis_polledio	polledio;
	struct {
		unsigned char red;
		unsigned char green;
		unsigned char blue;
	}			colormap[VGA8_CMAP_ENTRIES];
	unsigned char attrib_palette[VGA_ATR_NUM_PLT];
};

static int vgatext_devinit(struct vgatext_softc *, struct vis_devinit *data);
static void	vgatext_cons_copy(struct vgatext_softc *,
			struct vis_conscopy *);
static void	vgatext_cons_display(struct vgatext_softc *,
			struct vis_consdisplay *);
static void	vgatext_cons_cursor(struct vgatext_softc *,
			struct vis_conscursor *);
static void	vgatext_polled_copy(struct vis_polledio_arg *,
			struct vis_conscopy *);
static void	vgatext_polled_display(struct vis_polledio_arg *,
			struct vis_consdisplay *);
static void	vgatext_polled_cursor(struct vis_polledio_arg *,
			struct vis_conscursor *);
static void	vgatext_init(struct vgatext_softc *);
#if	defined(USE_BORDERS)
static void	vgatext_init_graphics(struct vgatext_softc *);
#endif
static int vgatext_kdsetmode(struct vgatext_softc *softc, int mode);
static void vgatext_setfont(struct vgatext_softc *softc);
static void vgatext_set_cursor(struct vgatext_softc *softc, int row, int col);
static void vgatext_hide_cursor(struct vgatext_softc *softc);
static void vgatext_save_colormap(struct vgatext_softc *softc);
static void vgatext_restore_colormap(struct vgatext_softc *softc);
static int vgatext_get_pci_reg_index(dev_info_t *const devi,
		unsigned long himask, unsigned long hival, unsigned long addr,
		off_t *offset);
static int vgatext_get_isa_reg_index(dev_info_t *const devi,
		unsigned long hival, unsigned long addr, off_t *offset);

static void	*vgatext_softc_head;

/* Loadable Driver stuff */

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"VGA text driver v1.17",	/* Name of the module. */
	&vgatext_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *) &modldrv, NULL
};

static const unsigned char solaris_color_to_pc_color[16] = {
	15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

int
_init(void)
{
	int e;

	cmn_err(CE_CONT, "?" MYNAME ": compiled %s, %s\n", __TIME__, __DATE__);

	if ((e = ddi_soft_state_init(&vgatext_softc_head,
		    sizeof (struct vgatext_softc), 1)) != 0) {
	    return (e);
	}

	e = mod_install(&modlinkage);

	if (e) {
	    ddi_soft_state_fini(&vgatext_softc_head);
	}
	return (e);
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)
	    return (e);

	ddi_soft_state_fini(&vgatext_softc_head);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* default structure for FBIOGATTR ioctl */
static struct fbgattr vgatext_attr =  {
/*	real_type	owner */
	FBTYPE_SUNFAST_COLOR, 0,
/* fbtype: type		h  w  depth cms  size */
	{ FBTYPE_SUNFAST_COLOR, TEXT_ROWS, TEXT_COLS, 1,    256,  0 },
/* fbsattr: flags emu_type	dev_specific */
	{ 0, FBTYPE_SUN4COLOR, { 0 } },
/*	emu_types */
	{ -1 }
};

/*
 * handy macros
 */

#define	getsoftc(instance) ((struct vgatext_softc *)	\
			ddi_get_soft_state(vgatext_softc_head, (instance)))

static int
vgatext_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct vgatext_softc *softc;
	int		unit = ddi_get_instance(devi);
	int	error;
	char	*parent_type;
	int	reg_rnumber;
	off_t	reg_offset;
	int	mem_rnumber;
	off_t	mem_offset;
	char	buf[80];
	static ddi_device_acc_attr_t reg_access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};
	static ddi_device_acc_attr_t fb_access_mode = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

	switch (cmd) {
	case DDI_ATTACH:
	    break;

	case DDI_RESUME:
	    return (DDI_SUCCESS);
	default:
	    return (DDI_FAILURE);
	}

	/* DDI_ATTACH */

	/* Allocate softc struct */
	if (ddi_soft_state_zalloc(vgatext_softc_head, unit) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}
	softc = getsoftc(unit);

	/* link it in */
	softc->devi = devi;
	ddi_set_driver_private(devi, (caddr_t)softc);

	softc->polledio.arg = (struct vis_polledio_arg *)softc;
	softc->polledio.display = vgatext_polled_display;
	softc->polledio.copy = vgatext_polled_copy;
	softc->polledio.cursor = vgatext_polled_cursor;

	error = ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_get_parent(devi),
		DDI_PROP_DONTPASS, "device_type", &parent_type);
	if (error != DDI_SUCCESS) {
		cmn_err(CE_WARN, MYNAME ": can't determine parent type.");
#ifdef __ia64
		/* XXX Merced fix me when I have a real device tree */
		cmn_err(CE_WARN, MYNAME ": assuming isa");
		parent_type = "isa";
#else
		goto fail;
#endif
	}

#define	STREQ(a, b)	(strcmp((a), (b)) == 0)
	if (STREQ(parent_type, "isa") || STREQ(parent_type, "eisa")) {
		reg_rnumber = vgatext_get_isa_reg_index(devi, 1, VGA_REG_ADDR,
			&reg_offset);
		if (reg_rnumber < 0) {
			cmn_err(CE_WARN,
				MYNAME ": can't find reg entry for registers");
			goto fail;
		}
		mem_rnumber = vgatext_get_isa_reg_index(devi, 0, VGA_MEM_ADDR,
			&mem_offset);
		if (mem_rnumber < 0) {
			cmn_err(CE_WARN,
				MYNAME ": can't find reg entry for memory");
			goto fail;
		}
	} else if (STREQ(parent_type, "pci")) {
		reg_rnumber = vgatext_get_pci_reg_index(devi,
			PCI_REG_ADDR_M|PCI_REG_REL_M,
			PCI_ADDR_IO|PCI_RELOCAT_B, VGA_REG_ADDR,
			&reg_offset);
		if (reg_rnumber < 0) {
			cmn_err(CE_WARN,
				MYNAME ": can't find reg entry for registers");
			goto fail;
		}
		mem_rnumber = vgatext_get_pci_reg_index(devi,
			PCI_REG_ADDR_M|PCI_REG_REL_M,
			PCI_ADDR_MEM32|PCI_RELOCAT_B, VGA_MEM_ADDR,
			&mem_offset);
		if (mem_rnumber < 0) {
			cmn_err(CE_WARN,
				MYNAME ": can't find reg entry for memory");
			goto fail;
		}
	} else {
		cmn_err(CE_WARN, MYNAME ": unknown parent type \"%s\".",
			parent_type);
		goto fail;
	}

#ifndef __ia64
	/* XXX Merced fix me when I have a real device tree */
	ddi_prop_free(parent_type);
#endif

	error = ddi_regs_map_setup(devi, reg_rnumber,
		(caddr_t *)&softc->regs.addr, reg_offset, VGA_REG_SIZE,
		&reg_access_mode, &softc->regs.handle);
	if (error != DDI_SUCCESS)
		goto fail;
	softc->regs.mapped = B_TRUE;

	softc->fb_size = VGA_MEM_SIZE;

	error = ddi_regs_map_setup(devi, mem_rnumber,
		(caddr_t *)&softc->fb.addr,
		mem_offset, softc->fb_size,
		&fb_access_mode, &softc->fb.handle);
	if (error != DDI_SUCCESS)
		goto fail;
	softc->fb.mapped = B_TRUE;

	if (ddi_io_get8(softc->regs.handle,
	    softc->regs.addr + VGA_MISC_R) & VGA_MISC_IOA_SEL)
		softc->text_base = (caddr_t)softc->fb.addr + VGA_COLOR_BASE;
	else
		softc->text_base = (caddr_t)softc->fb.addr + VGA_MONO_BASE;
	softc->current_base = softc->text_base;

	(void) sprintf(buf, "text-%d", unit);
	error = ddi_create_minor_node(devi, buf, S_IFCHR,
	    unit, DDI_NT_DISPLAY, NULL);
	if (error != DDI_SUCCESS)
		goto fail;

	error = ddi_prop_create(makedevice(DDI_MAJOR_T_UNKNOWN, unit),
	    devi, DDI_PROP_CANSLEEP, DDI_KERNEL_IOCTL, NULL, 0);
	if (error != DDI_SUCCESS)
		goto fail;

	vgatext_init(softc);
	vgatext_save_colormap(softc);

	return (DDI_SUCCESS);

fail:
	(void) vgatext_detach(devi, DDI_DETACH);
	return (error);
}

static int
vgatext_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(devi);
	struct vgatext_softc *softc = getsoftc(instance);

	switch (cmd) {
	case DDI_DETACH:
		if (softc->fb.mapped)
			ddi_regs_map_free(&softc->fb.handle);
		if (softc->regs.mapped)
			ddi_regs_map_free(&softc->regs.handle);
		ddi_remove_minor_node(devi, NULL);
		(void) ddi_soft_state_free(vgatext_softc_head, instance);
		return (DDI_SUCCESS);

	default:
		cmn_err(CE_WARN, "vgatext_detach: unknown cmd 0x%x\n", cmd);
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
vgatext_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	dev_t dev;
	int error;
	int instance;
	struct vgatext_softc *softc;

	error = DDI_SUCCESS;

	dev = (dev_t)arg;
	instance = getminor(dev);
	softc = getsoftc(instance);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (softc == NULL || softc->devi == NULL) {
			error = DDI_FAILURE;
		} else {
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
		break;
	}
	return (error);
}


/*ARGSUSED*/
static int
vgatext_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	struct vgatext_softc *softc = getsoftc(getminor(*devp));

	if (softc == NULL || otyp == OTYP_BLK)
		return (ENXIO);

	return (0);
}

/*ARGSUSED*/
static int
vgatext_close(dev_t devp, int flag, int otyp, cred_t *cred)
{
	return (0);
}

/*ARGSUSED*/
static int
vgatext_ioctl(
    dev_t dev,
    int cmd,
    intptr_t data,
    int mode,
    cred_t *cred,
    int *rval)
{
	struct vgatext_softc *softc = getsoftc(getminor(dev));
	static char kernel_only[] = "vgatext_ioctl: %s is a kernel only ioctl";
	int err;
	int kd_mode;

	switch (cmd) {
	case KDSETMODE:
	    return (vgatext_kdsetmode(softc, (int)data));

	case KDGETMODE:
	    kd_mode = softc->mode;
	    if (ddi_copyout((caddr_t)&kd_mode, (caddr_t)data,
		    sizeof (int), mode))
		return (EFAULT);
	    break;

	case VIS_GETIDENTIFIER:

	    if (ddi_copyout((caddr_t)&text_ident,
			    (caddr_t)data,
			    sizeof (struct vis_identifier),
			    mode))
		return (EFAULT);
	    break;

	case VIS_DEVINIT:

	    if (!(mode & FKIOCTL)) {
		    cmn_err(CE_CONT, kernel_only, "VIS_DEVINIT");
		    return (ENXIO);
	    }

	    err = vgatext_devinit(softc, (struct vis_devinit *)data);
	    if (err != 0) {
		    cmn_err(CE_WARN,
			"vgatext_ioctl:  could not initialize console");
		    return (err);
	    }
	    break;

	case VIS_CONSCOPY:	/* move */
	{
	    struct vis_conscopy pma;

	    if (ddi_copyin((caddr_t)data, (caddr_t)&pma,
			sizeof (struct vis_conscopy), mode))
		    return (EFAULT);

	    vgatext_cons_copy(softc, &pma);
	    break;
	}

	case VIS_CONSDISPLAY:	/* display */
	{
	    struct vis_consdisplay display_request;

	    if (ddi_copyin((caddr_t)data, (caddr_t)&display_request,
			sizeof (display_request), mode))
		    return (EFAULT);

	    vgatext_cons_display(softc, &display_request);
	    break;
	}

	case VIS_CONSCURSOR:
	{
	    struct vis_conscursor cursor_request;

	    if (ddi_copyin((caddr_t)data, (caddr_t)&cursor_request,
			sizeof (cursor_request), mode))
		    return (EFAULT);

	    vgatext_cons_cursor(softc, &cursor_request);
	    break;
	}

	case VIS_GETCMAP:
	case VIS_PUTCMAP:
	case FBIOPUTCMAP:
	case FBIOGETCMAP:
		/*
		 * At the moment, text mode is not considered to have
		 * a color map.
		 */
		return (EINVAL);

	case FBIOGATTR:
		if (copyout((caddr_t)&vgatext_attr, (caddr_t)data,
		    sizeof (struct fbgattr)))
			return (EFAULT);
		break;

	case FBIOGTYPE:
		if (copyout((caddr_t)&vgatext_attr.fbtype, (caddr_t)data,
		    sizeof (struct fbtype)))
			return (EFAULT);
		break;

#if 0
	case GLY_LD_GLYPH:
	{
		temstat_t	*ap;
		da_t	da;
		struct glyph	g;
		int	size;
		uchar_t	*c;

		if (ddi_copyin((caddr_t)arg, (caddr_t)&g, sizeof (g), flag))
			return (EFAULT);

		size = g.width * g.height;
		c = (uchar_t *)kmem_alloc(size, KM_SLEEP);
		if (c == NULL)
			return (EFAULT);
		if (ddi_copyin((caddr_t)g.raster, (caddr_t)c, size, flag)) {
			kmem_free(c, size);
			return (EFAULT);
		}
		ap = (temstat_t *)something;
		da.data = c;
		da.width = g.width;
		da.height = g.height;
		da.col = g.x_dest;
		da.row = g.y_dest;
		ap->a_fp.f_ad_display(ap->a_private, &da);
		kmem_free(c, size);
		return (0);
	}
#endif

	default:
		return (ENXIO);
	}
	return (0);
}

static int
vgatext_kdsetmode(struct vgatext_softc *softc, int mode)
{
	int i;

	if (mode == softc->mode)
		return (0);

	switch (mode) {
	case KD_TEXT:
		vgatext_init(softc);
		for (i = 0; i < sizeof (softc->shadow); i++) {
			softc->text_base[i] = softc->shadow[i];
		}
		softc->current_base = softc->text_base;
		if (softc->cursor.visible) {
			vgatext_set_cursor(softc,
				softc->cursor.row, softc->cursor.col);
		}
		vgatext_restore_colormap(softc);
		break;

	case KD_GRAPHICS:
		for (i = 0; i < sizeof (softc->shadow); i++) {
			softc->shadow[i] = softc->text_base[i];
		}
		softc->current_base = softc->shadow;
#if	defined(USE_BORDERS)
		vgatext_init_graphics(softc);
#endif
		break;

	default:
		return (EINVAL);
	}
	softc->mode = mode;
	return (0);
}

/*ARGSUSED*/
static int
vgatext_mmap(dev_t dev, off_t off, int prot)
{
	struct vgatext_softc *softc = getsoftc(getminor(dev));

	if (off >= VGA_MMAP_FB_BASE && off < VGA_MMAP_FB_BASE + softc->fb_size)
		return (hat_getkpfnum((caddr_t)softc->fb.addr +
			off - VGA_MMAP_FB_BASE));

	cmn_err(CE_WARN, "vgatext: Unknown mmap offset 0x%lx", off);

	return (-1);
}

static int
vgatext_devinit(struct vgatext_softc *softc, struct vis_devinit *data)
{
	/* initialize console instance */
	data->version = VIS_CONS_REV;
	data->width = TEXT_COLS;
	data->height = TEXT_ROWS;
	data->linebytes = TEXT_COLS;
	data->depth = 4;	/* ??? */
	data->mode = VIS_TEXT;
	data->polledio = &softc->polledio;

	return (0);
}

/*
 * display a string on the screen at (row, col)
 *	 assume it has been cropped to fit.
 */

static void
vgatext_cons_display(struct vgatext_softc *softc, struct vis_consdisplay *da)
{
	unsigned char	*string;
	int	i;
	unsigned char	attr;
	struct cgatext {
		unsigned char ch;
		unsigned char attr;
	};
	struct cgatext *addr;

	/*
	 * To be fully general, we should copyin the data.  This is not
	 * really relevant for this text-only driver, but a graphical driver
	 * should support these ioctls from userland to enable simple
	 * system startup graphics.
	 */
	attr = (solaris_color_to_pc_color[da->bg_color & 0xf] << 4)
		| solaris_color_to_pc_color[da->fg_color & 0xf];
#ifdef __ia64
	/*
	 * SoftSDV VGA turns the characters yellow when the bright/blink
	 * intensity bit is set.
	 */
	attr &= ~0x88;
#endif
	string = da->data;
	addr = (struct cgatext *)softc->current_base
		+  (da->row * TEXT_COLS + da->col);
	for (i = 0; i < da->width; i++) {
		addr->ch = string[i];
		addr->attr = attr;
		addr++;
	}
}

static void
vgatext_polled_display(
	struct vis_polledio_arg *arg,
	struct vis_consdisplay *da)
{
	vgatext_cons_display((struct vgatext_softc *)arg, da);
}

/*
 * screen-to-screen copy
 */

static void
vgatext_cons_copy(struct vgatext_softc *softc, struct vis_conscopy *ma)
{
	unsigned short	*from;
	unsigned short	*to;
	int		cnt;
	screen_size_t chars_per_row;
	unsigned short	*to_row_start;
	unsigned short	*from_row_start;
	screen_size_t	rows_to_move;
	unsigned short	*base;

	/*
	 * Remember we're going to copy shorts because each
	 * character/attribute pair is 16 bits.
	 */
	chars_per_row = ma->e_col - ma->s_col + 1;
	rows_to_move = ma->e_row - ma->s_row + 1;

	base = (unsigned short *)softc->current_base;

	to_row_start = base + ((ma->t_row * TEXT_COLS) + ma->t_col);
	from_row_start = base + ((ma->s_row * TEXT_COLS) + ma->s_col);

	if (to_row_start < from_row_start) {
		while (rows_to_move--) {
			to = to_row_start;
			from = from_row_start;
			to_row_start += TEXT_COLS;
			from_row_start += TEXT_COLS;
			for (cnt = chars_per_row; cnt-- > 0; )
				*to++ = *from++;
		}
	} else {
		/*
		 * Offset to the end of the region and copy backwards.
		 */
		cnt = rows_to_move * TEXT_COLS + chars_per_row;
		to_row_start += cnt;
		from_row_start += cnt;

		while (rows_to_move--) {
			to_row_start -= TEXT_COLS;
			from_row_start -= TEXT_COLS;
			to = to_row_start;
			from = from_row_start;
			for (cnt = chars_per_row; cnt-- > 0; )
				*--to = *--from;
		}
	}
}

static void
vgatext_polled_copy(
	struct vis_polledio_arg *arg,
	struct vis_conscopy *ca)
{
	vgatext_cons_copy((struct vgatext_softc *)arg, ca);
}


static void
vgatext_cons_cursor(struct vgatext_softc *softc, struct vis_conscursor *ca)
{
	switch (ca->action) {
	case VIS_HIDE_CURSOR:
		softc->cursor.visible = B_FALSE;
		if (softc->current_base == softc->text_base)
			vgatext_hide_cursor(softc);
		break;
	case VIS_DISPLAY_CURSOR:
		softc->cursor.visible = B_TRUE;
		softc->cursor.col = ca->col;
		softc->cursor.row = ca->row;
		if (softc->current_base == softc->text_base)
			vgatext_set_cursor(softc, ca->row, ca->col);
		break;
	}
}

static void
vgatext_polled_cursor(
	struct vis_polledio_arg *arg,
	struct vis_conscursor *ca)
{
	vgatext_cons_cursor((struct vgatext_softc *)arg, ca);
}



/*ARGSUSED*/
static void
vgatext_hide_cursor(struct vgatext_softc *softc)
{
	/* Nothing at present */
}

static void
vgatext_set_cursor(struct vgatext_softc *softc, int row, int col)
{
	short	addr;

	addr = row * TEXT_COLS + col;

	vga_set_crtc(&softc->regs, VGA_CRTC_CLAH, addr >> 8);
	vga_set_crtc(&softc->regs, VGA_CRTC_CLAL, addr & 0xff);
}

static void
vgatext_init(struct vgatext_softc *softc)
{
	unsigned char atr_mode;

	atr_mode = vga_get_atr(&softc->regs, VGA_ATR_MODE);
	atr_mode &= ~VGA_ATR_MODE_BLINK;
	atr_mode &= ~VGA_ATR_MODE_9WIDE;
	vga_set_atr(&softc->regs, VGA_ATR_MODE, atr_mode);
#if	defined(USE_BORDERS)
	vga_set_atr(&softc->regs, VGA_ATR_BDR_CLR,
		vga_get_atr(&softc->regs, VGA_BRIGHT_WHITE));
#else
	vga_set_atr(&softc->regs, VGA_ATR_BDR_CLR,
		vga_get_atr(&softc->regs, VGA_BLACK));
#endif
	vgatext_setfont(softc);	/* need selectable font? */
}

#if	defined(USE_BORDERS)
static void
vgatext_init_graphics(struct vgatext_softc *softc)
{
	vga_set_atr(&softc->regs, VGA_ATR_BDR_CLR,
		vga_get_atr(&softc->regs, VGA_BLACK));
}
#endif

static void
vgatext_setfont(struct vgatext_softc *softc)
{
	extern unsigned char *ENCODINGS[];
	unsigned char *from;
	unsigned char *to;
	int	i;
	int	j;
	int	bpc;

	/*
	 * I'm embarassed to say that I don't know what these magic
	 * sequences do, other than at the high level of "set the
	 * memory window to allow font setup".  I stole them straight
	 * from "kd"...
	 */
	vga_set_seq(&softc->regs, 0x02, 0x04);
	vga_set_seq(&softc->regs, 0x04, 0x06);
	vga_set_grc(&softc->regs, 0x05, 0x00);
	vga_set_grc(&softc->regs, 0x06, 0x04);

	/*
	 * This assumes 8x16 characters, which yield the traditional 80x25
	 * screen.  It really should support other character heights.
	 */
	bpc = 16;
	for (i = 0; i < 256; i++) {
		from = ENCODINGS[i];
		to = (unsigned char *)softc->fb.addr + i * 0x20;
		for (j = 0; j < bpc; j++)
			*to++ = *from++;
	}

	vga_set_seq(&softc->regs, 0x02, 0x03);
	vga_set_seq(&softc->regs, 0x04, 0x02);
	vga_set_grc(&softc->regs, 0x04, 0x00);
	vga_set_grc(&softc->regs, 0x05, 0x10);
	vga_set_grc(&softc->regs, 0x06, 0x0e);
}

static void
vgatext_save_colormap(struct vgatext_softc *softc)
{
	int i;

	for (i = 0; i < VGA_ATR_NUM_PLT; i++) {
		softc->attrib_palette[i] = vga_get_atr(&softc->regs, i);
	}
	for (i = 0; i < VGA8_CMAP_ENTRIES; i++) {
		vga_get_cmap(&softc->regs, i,
			&softc->colormap[i].red,
			&softc->colormap[i].green,
			&softc->colormap[i].blue);
	}
}

static void
vgatext_restore_colormap(struct vgatext_softc *softc)
{
	int i;

	for (i = 0; i < VGA_ATR_NUM_PLT; i++) {
		vga_set_atr(&softc->regs, i, softc->attrib_palette[i]);
	}
	for (i = 0; i < VGA8_CMAP_ENTRIES; i++) {
		vga_put_cmap(&softc->regs, i,
			softc->colormap[i].red,
			softc->colormap[i].green,
			softc->colormap[i].blue);
	}
}

/*
 * search the entries of the "reg" property for one which has the desired
 * combination of phys_hi bits and contains the desired address.
 *
 * This version searches a PCI-style "reg" property.  It was prompted by
 * issues surrounding the presence or absence of an entry for the ROM:
 * (a) a transition problem with PowerPC Virtual Open Firmware
 * (b) uncertainty as to whether an entry will be included on a device
 *     with ROM support (and so an "active" ROM base address register),
 *     but no ROM actually installed.
 *
 * See the note below on vgatext_get_isa_reg_index for the reasons for
 * returning the offset.
 *
 * Note that this routine may not be fully general; it is intended for the
 * specific purpose of finding a couple of particular VGA reg entries and
 * may not be suitable for all reg-searching purposes.
 */
static int
vgatext_get_pci_reg_index(
	dev_info_t *const devi,
	unsigned long himask,
	unsigned long hival,
	unsigned long addr,
	off_t *offset)
{

	int			length, index;
	pci_regspec_t	*reg;

	if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		"reg", (caddr_t)&reg, &length) != DDI_PROP_SUCCESS) {
		return (-1);
	}

	for (index = 0; index < length / sizeof (pci_regspec_t); index++) {
		if ((reg[index].pci_phys_hi & himask) != hival)
			continue;
		if (reg[index].pci_size_hi != 0)
			continue;
		if (reg[index].pci_phys_mid != 0)
			continue;
		if (reg[index].pci_phys_low > addr)
			continue;
		if (reg[index].pci_phys_low + reg[index].pci_size_low <= addr)
			continue;

		*offset = addr - reg[index].pci_phys_low;
		kmem_free(reg, (size_t)length);
		return (index);
	}
	kmem_free(reg, (size_t)length);

	return (-1);
}

/*
 * search the entries of the "reg" property for one which has the desired
 * combination of phys_hi bits and contains the desired address.
 *
 * This version searches a ISA-style "reg" property.  It was prompted by
 * issues surrounding 8514/A support.  By IEEE 1275 compatibility conventions,
 * 8514/A registers should have been added after all standard VGA registers.
 * Unfortunately, the Solaris/Intel device configuration framework
 * (a) lists the 8514/A registers before the video memory, and then
 * (b) also sorts the entries so that I/O entries come before memory
 *     entries.
 *
 * It returns the "reg" index and offset into that register set.
 * The offset is needed because there exist (broken?) BIOSes that
 * report larger ranges enclosing the standard ranges.  One reports
 * 0x3bf for 0x21 instead of 0x3c0 for 0x20, for instance.  Using the
 * offset adjusts for this difference in the base of the register set.
 *
 * Note that this routine may not be fully general; it is intended for the
 * specific purpose of finding a couple of particular VGA reg entries and
 * may not be suitable for all reg-searching purposes.
 */
static int
vgatext_get_isa_reg_index(
	dev_info_t *const devi,
	unsigned long hival,
	unsigned long addr,
	off_t *offset)
{

	int		length, index;
	struct regspec	*reg;

	if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		"reg", (caddr_t)&reg, &length) != DDI_PROP_SUCCESS) {
		return (-1);
	}

	for (index = 0; index < length / sizeof (struct regspec); index++) {
		if (reg[index].regspec_bustype != hival)
			continue;
		if (reg[index].regspec_addr > addr)
			continue;
		if (reg[index].regspec_addr + reg[index].regspec_size <= addr)
			continue;

		*offset = addr - reg[index].regspec_addr;
		kmem_free(reg, (size_t)length);
		return (index);
	}
	kmem_free(reg, (size_t)length);

	return (-1);
}
