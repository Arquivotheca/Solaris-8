/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DEVOPS_H
#define	_SYS_DEVOPS_H

#pragma ident	"@(#)devops.h	1.42	99/04/21 SMI"	/* SVr4.0 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/poll.h>
#include <vm/as.h>

#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/ddimapreq.h>
#include <sys/ddipropdefs.h>
#include <sys/ddidevmap.h>
#include <sys/nexusdefs.h>
#include <sys/nexusintr.h>
#include <sys/aio_req.h>
#include <vm/page.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * cb_ops:	Leaf device drivers or bus nexus drivers supporting
 *		direct user process access (open/close/etc).
 *
 * This is an OR of cdevsw and bdevsw fields for drivers that
 * support both character and block entry points.
 *
 * For streams stuff, see also sys/stream.h.
 *
 * The following DDI/DKI or DKI only or DDI only functions are
 * provided in the character/block driver operations structure.
 *
 *	block/char	Function	description
 *	b/c		XXopen		DDI/DKI
 *	b/c		XXclose		DDI/DKI
 *	b		XXstrategy	DDI/DKI
 *	b  		XXprint		DDI/DKI
 *	b  		XXdump		DDI(Sun)
 *	  c		XXread		DDI/DKI
 *	  c		XXwrite		DDI/DKI
 *	  c		XXioctl		DDI/DKI
 *	  c		XXdevmap	DDI(Sun)
 *	  c		XXmmap		DKI
 *	  c		XXsegmap	DKI
 *	  c		XXchpoll	DDI/DKI
 *	  c		XXprop_op	DDI(Sun)
 */

struct cb_ops  {
#ifdef	__STDC__
	int	(*cb_open)(dev_t *devp, int flag, int otyp, cred_t *credp);
	int	(*cb_close)(dev_t dev, int flag, int otyp, cred_t *credp);
	int	(*cb_strategy)(struct buf *bp);
	int	(*cb_print)(dev_t dev, char *str);
	int	(*cb_dump)(dev_t dev, caddr_t addr, daddr_t blkno, int nblk);
	int	(*cb_read)(dev_t dev, struct uio *uiop, cred_t *credp);
	int	(*cb_write)(dev_t dev, struct uio *uiop, cred_t *credp);
	int	(*cb_ioctl)(dev_t dev, int cmd, intptr_t arg, int mode,
		    cred_t *credp, int *rvalp);
	int	(*cb_devmap)(dev_t dev, devmap_cookie_t dhp, offset_t off,
			size_t len, size_t *maplen, uint_t model);
	int	(*cb_mmap)(dev_t dev, off_t off, int prot);
	int	(*cb_segmap)(dev_t dev, off_t off, struct as *asp,
		    caddr_t *addrp, off_t len, unsigned int prot,
		    unsigned int maxprot, unsigned int flags, cred_t *credp);
	int	(*cb_chpoll)(dev_t dev, short events, int anyyet,
		    short *reventsp, struct pollhead **phpp);
	int	(*cb_prop_op)(dev_t dev, dev_info_t *dip,
		    ddi_prop_op_t prop_op, int mod_flags,
		    char *name, caddr_t valuep, int *length);

	struct streamtab *cb_str;	/* streams information */

	/*
	 * The cb_flag fields are here to tell the system a
	 * bit about the device. The bit definitions are
	 * in <sys/conf.h>.
	 */
	int	cb_flag;		/* driver compatability flag */
	int	cb_rev;			/* cb_ops version number */
	int	(*cb_aread)(dev_t dev, struct aio_req *aio, cred_t *credp);
	int	(*cb_awrite)(dev_t dev, struct aio_req *aio, cred_t *credp);
#else /* __STDC__ */
	int	(*cb_open)();
	int	(*cb_close)();
	int	(*cb_strategy)();
	int	(*cb_print)();
	int	(*cb_dump)();
	int	(*cb_read)();
	int	(*cb_write)();
	int	(*cb_ioctl)();
	int	(*cb_devmap)();
	int	(*cb_mmap)();
	int	(*cb_segmap)();
	int	(*cb_chpoll)();
	int	(*cb_prop_op)();
	struct streamtab *cb_str;
	int	cb_flag;
	int	cb_rev;
	int	(*cb_aread)();
	int	(*cb_awrite)();
#endif /* __STDC__ */
};

/*
 * bus_ops:	bus nexus drivers only.
 *
 * These functions are used to implement the Sun DDI functions
 * described elsewhere.
 *
 * Only nexus drivers support these entry points.
 *
 * The following bus nexus functions are provided in the bus nexus
 * driver operations structure.  Note that all functions take both
 * their dip and the requesters dip except for the child functions since
 * they will be called from outside the ddi.
 *
 *	bus_map			-  Map/unmap/control IU -> device mappings.
 *	bus_get_intrspec	-  get interrupt specification by number
 *	bus_add_intrspec	-  add interrupt specification, return cookie
 *	bus_remove_intrspec	-  remove interrupt specification
 *	bus_map_fault		-  bus fault handler
 *	bus_dma_map		-  setup dma mapping
 *	bus_dma_mapctl		-  control (and free) dma mapping
 *	bus_ctl			-  generic control operations
 *	bus_prop_op		_  request for property
 */

#define	BUSO_REV	4
#define	BUSO_REV_3	3
#define	BUSO_REV_4	4

struct bus_ops  {
#ifdef __STDC__
	int		busops_rev;	/* rev of this structure */
	int		(*bus_map)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_map_req_t *mp, off_t offset, off_t len,
			    caddr_t *vaddrp);
	ddi_intrspec_t	(*bus_get_intrspec)(dev_info_t *dip, dev_info_t *rdip,
			    uint_t inumber);
	int		(*bus_add_intrspec)(dev_info_t *dip,
			    dev_info_t *rdip, ddi_intrspec_t intrspec,
			    ddi_iblock_cookie_t *ibcp,
			    ddi_idevice_cookie_t *idcp,
			    uint_t (*int_handler)(caddr_t intr_handler_arg),
			    caddr_t intr_handler_arg, int kind);
	void		(*bus_remove_intrspec)(dev_info_t *dip,
			    dev_info_t *rdip, ddi_intrspec_t intrspec,
			    ddi_iblock_cookie_t iblock_cookie);
	int		(*bus_map_fault)(dev_info_t *dip, dev_info_t *rdip,
			    struct hat *hat, struct seg *seg, caddr_t addr,
			    struct devpage *dp, uint_t pfn, uint_t prot,
			    uint_t lock);
	int		(*bus_dma_map)(dev_info_t *dip, dev_info_t *rdip,
			    struct ddi_dma_req *dmareq,
			    ddi_dma_handle_t *handlep);
	int		(*bus_dma_allochdl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_attr_t *attr, int (*waitfp)(caddr_t),
			    caddr_t arg, ddi_dma_handle_t *handlep);
	int		(*bus_dma_freehdl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle);
	int		(*bus_dma_bindhdl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
			    ddi_dma_cookie_t *, uint_t *);
	int		(*bus_dma_unbindhdl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle);
	int		(*bus_dma_flush)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle, off_t off,
			    size_t len, uint_t cache_flags);
	int		(*bus_dma_win)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle, uint_t win, off_t *offp,
			    size_t *lenp, ddi_dma_cookie_t *cookiep,
			    uint_t *ccountp);
	int		(*bus_dma_ctl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle,
			    enum ddi_dma_ctlops request, off_t *offp,
			    size_t *lenp, caddr_t *objp, uint_t flags);
	int		(*bus_ctl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_ctl_enum_t ctlop, void *arg, void *result);
	int		(*bus_prop_op)(dev_t dev, dev_info_t *dip,
			    dev_info_t *child_dip, ddi_prop_op_t prop_op,
			    int mod_flags, char *name, caddr_t valuep,
			    int *length);
	/*
	 * NOTE: the following 4 busops entrypoints are only available
	 * with version 3 or greater.
	 */

	int		(*bus_get_eventcookie)(dev_info_t *dip,
			    dev_info_t *rdip, char *eventname,
			    ddi_eventcookie_t *cookiep, ddi_plevel_t *plevelp,
			    ddi_iblock_cookie_t *iblock_cookiep);
	int		(*bus_add_eventcall)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_eventcookie_t eventid,
			    int (*event_hdlr)(dev_info_t *dip,
			    ddi_eventcookie_t event, void *arg,
			    void *bus_impldata), void *arg);
	int		(*bus_remove_eventcall)(dev_info_t *dip,
			    dev_info_t *rdip, ddi_eventcookie_t event);
	int		(*bus_post_event)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_eventcookie_t event, void *impl_data);

	/*
	 * NOTE: the following busop entrypoint is available with version
	 * 4 or greater.
	 */
	int		(*bus_intr_ctl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_intr_ctlop_t ctlop, void * arg, void * result);

#else /* __STDC__ */
	int		busops_rev;
	int		(*bus_map)();
	ddi_intrspec_t	(*bus_get_intrspec)();
	int		(*bus_add_intrspec)();
	void		(*bus_remove_intrspec)();
	int		(*bus_map_fault)();
	int		(*bus_dma_map)();
	int		(*bus_dma_allochdl)();
	int		(*bus_dma_freehdl)();
	int		(*bus_dma_bindhdl)();
	int		(*bus_dma_unbindhdl)();
	int		(*bus_dma_flush)();
	int		(*bus_dma_win)();
	int		(*bus_dma_ctl)();
	int		(*bus_ctl)();
	int		(*bus_prop_op)();
	/*
	 * NOTE: the following 4 busops entrypoints are only available
	 * with version 3 or greater.
	 */
	int		(*bus_get_eventcookie)();
	int		(*bus_add_eventcall)();
	int		(*bus_remove_eventcall)();
	int		(*bus_post_event)();
	int		(*bus_intr_ctl)();
#endif /* __STDC__ */
};

/*
 * REV 1 bus ops structure
 */

struct bus_ops_rev1 {
#ifdef __STDC__
	int		(*bus_map)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_map_req_t *mp, off_t offset, off_t len,
			    caddr_t *vaddrp);
	ddi_intrspec_t	(*bus_get_intrspec)(dev_info_t *dip, dev_info_t *rdip,
			    uint_t inumber);
	int		(*bus_add_intrspec)(dev_info_t *dip,
			    dev_info_t *rdip, ddi_intrspec_t intrspec,
			    ddi_iblock_cookie_t *ibcp,
			    ddi_idevice_cookie_t *idcp,
			    uint_t (*int_handler)(caddr_t intr_handler_arg),
			    caddr_t intr_handler_arg, int kind);
	void		(*bus_remove_intrspec)(dev_info_t *dip,
			    dev_info_t *rdip, ddi_intrspec_t intrspec,
			    ddi_iblock_cookie_t iblock_cookie);
	int		(*bus_map_fault)(dev_info_t *dip, dev_info_t *rdip,
			    struct hat *hat, struct seg *seg, caddr_t addr,
			    struct devpage *dp, uint_t pfn, uint_t prot,
			    uint_t lock);
	int		(*bus_dma_map)(dev_info_t *dip, dev_info_t *rdip,
			    struct ddi_dma_req *dmareq,
			    ddi_dma_handle_t *handlep);
	int		(*bus_dma_ctl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_dma_handle_t handle,
			    enum ddi_dma_ctlops request, off_t *offp,
			    uint_t *lenp, caddr_t *objp, uint_t flags);
	int		(*bus_ctl)(dev_info_t *dip, dev_info_t *rdip,
			    ddi_ctl_enum_t ctlop, void *arg, void *result);
	int		(*bus_prop_op)(dev_t dev, dev_info_t *dip,
			    dev_info_t *child_dip, ddi_prop_op_t prop_op,
			    int mod_flags, char *name, caddr_t valuep,
			    int *length);
#else /* __STDC__ */
	int		(*bus_map)();
	ddi_intrspec_t	(*bus_get_intrspec)();
	int		(*bus_add_intrspec)();
	void		(*bus_remove_intrspec)();
	int		(*bus_map_fault)();
	int		(*bus_dma_map)();
	int		(*bus_dma_ctl)();
	int		(*bus_ctl)();
	int		(*bus_prop_op)();
#endif /* __STDC__ */
};

/*
 * dev_ops:	Contains driver common fields and pointers
 *		to the bus_ops and/or cb_ops parts.
 *
 * Drivers should set devo_rev to DEVO_REV at compile time.
 * All drivers should support these entry points.
 *
 * the following device functions are provided in the device operations
 * structure.
 *
 *	devo_getinfo		-  Device handle conversion
 *	devo_identify		-  Device Driver confirmer
 *	devo_probe		-  Probe for device's existence
 *	devo_attach		-  Attach driver to dev_info
 *	devo_detach		-  Detach/prepare driver to unload
 *	devo_reset		-  Reset device
 */

#define		DEVO_REV		3
#define		CB_REV			1

/*
 * Return from driver's devo_identify function:
 */

#define	DDI_IDENTIFIED		(0)		/* devinfo claimed */
#define	DDI_NOT_IDENTIFIED	(-1)		/* not claimed */

/*
 * Return from driver's devo_probe function:
 */

#define	DDI_PROBE_FAILURE	ENXIO	/* matches nodev return */
#define	DDI_PROBE_DONTCARE	0	/* matches nulldev return */
#define	DDI_PROBE_PARTIAL	1
#define	DDI_PROBE_SUCCESS	2

/*
 * Typedefs for the info, attach, detach and reset routines.
 * These are mostly placeholders for now.
 */
typedef enum {
	DDI_INFO_DEVT2DEVINFO = 0,	/* Convert a dev_t to a dev_info_t */
	DDI_INFO_DEVT2INSTANCE		/* Convert a dev_t to an instance # */
} ddi_info_cmd_t;

typedef enum {
	DDI_ATTACH = 0,
	DDI_RESUME = 1,
	DDI_PM_RESUME = 2
} ddi_attach_cmd_t;

typedef enum {
	DDI_DETACH = 0,
	DDI_SUSPEND = 1,
	DDI_PM_SUSPEND = 2,
	DDI_HOTPLUG_DETACH = 3		/* detach, don't try to auto-unconfig */
} ddi_detach_cmd_t;

typedef enum {
	DDI_RESET_FORCE = 0
} ddi_reset_cmd_t;


struct dev_ops  {
#ifdef	__STDC__
	int		devo_rev;	/* Driver build version		*/
	int		devo_refcnt;	/* device reference count	*/

	int		(*devo_getinfo)(dev_info_t *dip,
			    ddi_info_cmd_t infocmd, void *arg, void **result);
	int		(*devo_identify)(dev_info_t *dip);
	int		(*devo_probe)(dev_info_t *dip);
	int		(*devo_attach)(dev_info_t *dip, ddi_attach_cmd_t cmd);
	int		(*devo_detach)(dev_info_t *dip, ddi_detach_cmd_t cmd);
	int		(*devo_reset)(dev_info_t *dip, ddi_reset_cmd_t cmd);

	struct cb_ops	*devo_cb_ops;	/* cb_ops pointer for leaf drivers   */
	struct bus_ops	*devo_bus_ops;	/* bus_ops pointer for nexus drivers */
	int		(*devo_power)(dev_info_t *dip, int component,
			    int level);
#else /* __STDC__ */
	int	devo_rev;
	int	devo_refcnt;
	int	(*devo_getinfo)();
	int	(*devo_identify)();
	int	(*devo_probe)();
	int	(*devo_attach)();
	int	(*devo_detach)();
	int	(*devo_reset)();
	struct cb_ops	*devo_cb_ops;
	struct bus_ops	*devo_bus_ops;
	int	(*devo_power)();
#endif /* __STDC__ */
};

/*
 * Create a dev_ops suitable for a streams driver:
 *
 * XXX: Note:  Since this is a macro, it is NOT supported as
 * XXX: part of the Sun DDI.  It is not a documented Sun DDI interface.
 *
 * STR_OPS(name, identify, probe, attach, detach, reset,
 *	info, flag, stream_tab);
 *
 *	XXname is the name of the dev_ops structure.
 *	XXidentify is the name of the identify routine
 *	XXprobe is the name of the probe routine, or nulldev
 *	XXattach is the name of the attach routine
 *	XXdetach is the name of the detach routine, or nodev
 *	XXreset is the name of the reset routine, or nodev
 *	XXinfo is the name of the info routine
 *	XXflag is driver flag (cb_flag) in cb_ops,
 *	XXstream_tab is the obvious.
 *	cb_##XXname is the name of the internally defined cb_ops struct.
 *
 * uses cb_XXname as name of static cb_ops structure.
 */

/*
 * This file is included by genassym.c now and I couldn't get it to take the
 * next line if it was broken into two lines joined by a '\'.  So, don't try
 * to reformat it to satisfy Cstyle because genassym.c won't compile.
 */
/* CSTYLED */
#define	DDI_DEFINE_STREAM_OPS(XXname, XXidentify, XXprobe, XXattach, XXdetach, XXreset, XXgetinfo, XXflag, XXstream_tab) \
static struct cb_ops cb_##XXname = {					\
	nulldev,		/* cb_open */				\
	nulldev,		/* cb_close */				\
	nodev,			/* cb_strategy */			\
	nodev,			/* cb_print */				\
	nodev,			/* cb_dump */				\
	nodev,			/* cb_read */				\
	nodev,			/* cb_write */				\
	nodev,			/* cb_ioctl */				\
	nodev,			/* cb_devmap */				\
	nodev,			/* cb_mmap */				\
	nodev,			/* cb_segmap */				\
	nochpoll,		/* cb_chpoll */				\
	ddi_prop_op,		/* cb_prop_op */			\
	(XXstream_tab),		/* cb_stream */				\
	(int)(XXflag),		/* cb_flag */				\
	CB_REV,			/* cb_rev */				\
	nodev,			/* cb_aread */				\
	nodev,			/* cb_awrite */				\
};									\
									\
static struct dev_ops XXname = {					\
	DEVO_REV,		/* devo_rev */				\
	0,			/* devo_refcnt */			\
	(XXgetinfo),		/* devo_getinfo */			\
	(XXidentify),		/* devo_identify */			\
	(XXprobe),		/* devo_probe */			\
	(XXattach),		/* devo_attach */			\
	(XXdetach),		/* devo_detach */			\
	(XXreset),		/* devo_reset */			\
	&(cb_##XXname),		/* devo_cb_ops */			\
	(struct bus_ops *)NULL,	/* devo_bus_ops */			\
	NULL			/* devo_power */			\
}

#define	MAPDEV_REV	1

struct ddi_mapdev_ctl {
#ifdef	__STDC__
	int	mapdev_rev;
	int	(*mapdev_access)(ddi_mapdev_handle_t handle, void *devprivate,
	    off_t offset);
	void	(*mapdev_free)(ddi_mapdev_handle_t handle, void *devprivate);
	int	(*mapdev_dup)(ddi_mapdev_handle_t handle, void *devprivate,
	    ddi_mapdev_handle_t new_handle, void **new_devprivate);
#else /* __STDC__ */
	int	mapdev_rev;
	int	(*mapdev_access)();
	void	(*mapdev_free)();
	int	(*mapdev_dup)();
#endif /* __STDC__ */
};

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEVOPS_H */
