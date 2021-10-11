/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr.c	1.34	99/10/15 SMI"

/*
 * PIM-DR layer of DR driver.  Provides interface between user
 * level applications and the PSM-DR layer.
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/dditypes.h>
#include <sys/devops.h>
#include <sys/modctl.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>
#include <sys/mem_config.h>
#include <sys/mem_cage.h>

#include <sys/autoconf.h>
#include <sys/cmn_err.h>

#include <sys/ddi_impldefs.h>
#include <sys/promif.h>
#include <sys/machsystm.h>

#include <sys/dr.h>
#include <sys/drpriv.h>

extern int nulldev();
extern int nodev();

typedef struct {		/* arg to dr_init_handle */
	dev_t	dev;
	int	cmd;
	int	mode;
} dr_init_arg_t;

typedef struct {		/* arg to release callbacks */
	dr_handle_t	*hp;
	dr_nodetype_t	nodetype;
	dnode_t		nodeid;
} dr_callback_t;

typedef struct {
	dr_error_t	*errp;
	dr_flags_t	flags;
} dr_treeinfo_t;

/*
 * DR driver entry points.
 */
static int	dr_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
				void *arg, void **result);
static int	dr_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int	dr_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int	dr_probe(dev_info_t *dip);
static int	dr_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
				cred_t *cred_p, int *rval_p);
static int	dr_close(dev_t dev, int flag, int otyp, cred_t *cred_p);
static int	dr_open(dev_t *dev, int flag, int otyp, cred_t *cred_p);

/*
 * DR support operations.
 */
static int	dr_init_handle(dr_handle_t *hp, void *init_arg,
				board_t *bd, dr_error_t *err);
static void	dr_deinit_handle(dr_handle_t *hp);
static dr_handle_t *dr_get_handle(dev_t dev, dr_softstate_t *softsp,
				intptr_t arg, void *init_arg);
static void	dr_release_handle(dr_handle_t *hp);
static int	dr_pre_op(dr_handle_t *hp);
static void	dr_post_op(dr_handle_t *hp);
static void	dr_exec_op(dr_handle_t *hp);
static void	dr_dev_connect(dr_handle_t *hp);
static void	dr_dev_disconnect(dr_handle_t *hp);
static void	dr_dev_configure(dr_handle_t *hp);
static void	dr_dev_release(dr_handle_t *hp);
static void	dr_dev_unconfigure(dr_handle_t *hp);
static void	dr_attach_cpu(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static void	dr_attach_io(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static int	dr_attach_branch(dev_info_t *pdip, dnode_t nodeid, void *arg);
static void	dr_attach_mem(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static void	dr_release_cpu(dr_handle_t *hp, dnode_t nodeid);
static void	dr_release_mem(dr_handle_t *hp, dnode_t nodeid);
static void	dr_release_io(dr_handle_t *hp, dnode_t nodeid);
static void	dr_detach_cpu(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static void	dr_detach_io(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static int	dr_detach_branch(dev_info_t *dip, void *arg);
static void	dr_detach_mem(dr_handle_t *hp, dr_error_t *ep,
				dnode_t nodeid);
static void	dr_dev_cancel(dr_handle_t *hp);
static void	dr_dev_status(dr_handle_t *hp);
static void	dr_mach_ioctl(dr_handle_t *hp);

static void	dr_release_mem_done(void *arg, int error);

static void	dr_release_dev_callback(void *arg);

static void 	dr_err_decode(int ndi_error, dr_error_t *ep, dev_info_t *dip,
				int attach);

static void	ddi_walk_devs_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);
static int 	i_ddi_walk_devs_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);
static int 	i_ddi_while_sibling_reverse(dev_info_t *,
				int (*f)(dev_info_t *, void *), void *);
/*
 * Autoconfiguration data structures
 */

struct cb_ops dr_cb_ops = {
	dr_open,	/* open */
	dr_close,	/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	dr_ioctl,	/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* chpoll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* struct streamtab */
	D_NEW | D_MP,	/* compatibility flags */
	CB_REV,		/* Rev */
	nodev,		/* cb_aread */
	nodev		/* cb_awrite */
};

struct dev_ops dr_dev_ops = {
	DEVO_REV,	/* build version */
	0,		/* dev ref count */
	dr_getinfo,	/* getinfo */
	nulldev,	/* identify */
	dr_probe,	/* probe */
	dr_attach,	/* attach */
	dr_detach,	/* detach */
	nodev,		/* reset */
	&dr_cb_ops,	/* cb_ops */
	(struct bus_ops *)NULL, /* bus ops */
	NULL		/* power */
};

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"Dyn Recfg PIM (1.34)",
	&dr_dev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/*
 * PIM-DR layer depends on PSM-DR module.
 * Non-existence of PSM-DR layer prevents DR operations.
 */
#ifndef	lint
static char _depends_on[] = "misc/drmach";
#endif	/* lint */

/*
 * dr Global data elements
 */
struct dr_global {
	dr_softstate_t	*softsp;	/* pointer to initialize soft state */
	kmutex_t	cblock;		/* callback serialization */
	dr_ops_t	*ops;
} dr_g;

int	dr_modunload_okay = 0;

/*
 * Driver entry points.
 */
int
_init(void)
{
	int	err;

	/*
	 * If you need to support multiple nodes (instances), then
	 * whatever the maximum number of supported nodes is would
	 * need to passed as the third parameter to ddi_soft_state_init().
	 * Alternative would be to dynamically fini and re-init the
	 * soft state structure each time a node is attached.
	 */
	err = ddi_soft_state_init((void **)&dr_g.softsp,
					sizeof (dr_softstate_t), 1);
	if (err)
		return (err);

	mutex_init(&dr_g.cblock, NULL, MUTEX_DRIVER, NULL);

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	int	err;

	if ((err = mod_remove(&modlinkage)) != 0)
		return (err);

	mutex_destroy(&dr_g.cblock);

	ddi_soft_state_fini((void **)&dr_g.softsp);

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
dr_open(dev_t *dev, int flag, int otyp, cred_t *cred_p)
{
	int		instance;
	static fn_t	f = "dr_open";

	/*
	 * Don't open unless we've attached.
	 */
	instance = DR_GET_MINOR2INST(getminor(*dev));

	if (GET_SOFTC(instance) == NULL) {
		cmn_err(CE_WARN,
			"dr:%s:%d: module not yet attached",
			f, instance);
		return (ENXIO);
	}

	return (0);
}

/*ARGSUSED*/
static int
dr_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

/*ARGSUSED*/
static int
dr_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
	cred_t *cred_p, int *rval_p)
{
	int		rv, instance;
	dr_handle_t	*hp;
	dr_softstate_t	*softsp;
	dr_init_arg_t	init_arg;
	static fn_t	f = "dr_ioctl";

	instance = DR_GET_MINOR2INST(getminor(dev));
	if ((softsp = (dr_softstate_t *)GET_SOFTC(instance)) == NULL) {
		cmn_err(CE_WARN,
			"dr:%s:%d: module not yet attached",
			f, instance);
		return (ENXIO);
	}

	init_arg.dev = getminor(dev);
	init_arg.cmd = cmd;
	init_arg.mode = mode;

	hp = dr_get_handle(dev, softsp, arg, &init_arg);
	if (hp == NULL) {
		cmn_err(CE_WARN,
			"dr:%s:%d: handle not found", f, getminor(dev));
		return (ENOENT);
	}

	if (VALID_HANDLE(hp) == B_FALSE) {
		cmn_err(CE_WARN,
			"dr:%s:%d: handle invalid", f, getminor(dev));
		dr_release_handle(hp);
		return (EINVAL);
	}

	if (dr_pre_op(hp)) {
		rv = GET_HERRNO(hp);
		dr_release_handle(hp);
		return (rv);
	}

	dr_exec_op(hp);

	dr_post_op(hp);

	rv = GET_HERRNO(hp);

	dr_release_handle(hp);

	return (rv);
}

/* ARGSUSED */
static int
dr_probe(dev_info_t *dip)
{
	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
dr_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int 		instance;
	dr_softstate_t	*softsp;
	static fn_t	f = "dr_attach";

	instance = ddi_get_instance(dip);

	switch (cmd) {

	case DDI_ATTACH:

		if (ALLOC_SOFTC(instance) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"dr:%s:%d: failed to alloc soft-state",
				f, instance);
			return (DDI_FAILURE);
		}

		softsp = (dr_softstate_t *)GET_SOFTC(instance);
		softsp->dip = dip;
		/*
		 * Allows for per-instance ops, i.e. possibly different
		 * dr_ops_t for different (hardware) platforms being
		 * linked together?
		 */
		if (dr_platform_init((void **)&softsp->machsoftsp, dip,
					&dr_g.ops)) {
			cmn_err(CE_WARN,
				"dr:%s:%d: failed to init psm-dr",
				f, instance);
			FREE_SOFTC(instance);
			return (DDI_FAILURE);
		}
		if (dr_g.ops == NULL) {
			cmn_err(CE_WARN,
				"dr:%s:%d: failed to init psm-dr ops",
				f, instance);
			(void) dr_platform_fini((void **)&softsp->machsoftsp);
			FREE_SOFTC(instance);
			return (DDI_FAILURE);
		}

		if (DR_PLAT_MAKE_NODES(dip)) {
			cmn_err(CE_WARN,
				"dr:%s:%d: failed to make nodes",
				f, instance);
			ddi_remove_minor_node(dip, NULL);
			(void) dr_platform_fini((void **)&softsp->machsoftsp);
			FREE_SOFTC(instance);
			return (DDI_FAILURE);
		}

		/*
		 * Announce the node's presence.
		 */
		ddi_report_dev(dip);

		return (DDI_SUCCESS);

		/* break; */

	default:
		return (DDI_FAILURE);
	}

}

/*ARGSUSED*/
static int
dr_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int 		instance;
	dr_softstate_t	*softsp;

	instance = ddi_get_instance(dip);

	switch (cmd) {

	case DDI_DETACH:

		if (!dr_modunload_okay)
			return (DDI_FAILURE);

		softsp = (dr_softstate_t *)GET_SOFTC(instance);

		(void) dr_platform_fini((void **)&softsp->machsoftsp);

		FREE_SOFTC(instance);

		/*
		 * Remove the minor nodes.
		 */
		ddi_remove_minor_node(dip, NULL);

		return (DDI_SUCCESS);

	default:

		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
dr_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	dev_t		dev = (dev_t)arg;
	int		instance, error;
	dr_softstate_t	*softsp;

	*result = NULL;
	error = DDI_SUCCESS;
	instance = DR_GET_MINOR2INST(getminor(dev));

	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((softsp = GET_SOFTC(instance)) == NULL)
			return (DDI_FAILURE);
		*result = (void *)softsp->dip;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)instance;
		break;

	default:
		error = DDI_FAILURE;
		break;
	}

	return (error);
}

/*
 * DR operations.
 */
static int
dr_init_handle(dr_handle_t *hp, void *init_arg, board_t *bp, dr_error_t *ep)
{
	dr_init_arg_t	*iap = (dr_init_arg_t *)init_arg;

	if ((hp == NULL) || (iap == NULL) || (bp == NULL) || (ep == NULL))
		return (-1);

	bzero((caddr_t)hp, sizeof (dr_handle_t));

	hp->h_bd = bp;
	hp->h_err = ep;
	hp->h_dev = iap->dev;
	hp->h_cmd = iap->cmd;
	hp->h_mode = iap->mode;
	DR_ALLOC_ERR(ep);

	return (0);
}

static void
dr_deinit_handle(dr_handle_t *hp)
{
	if (hp->h_err && hp->h_err->e_str) {
		DR_FREE_ERR(hp->h_err);
	}
}

/*ARGSUSED*/
static dr_handle_t *
dr_get_handle(dev_t dev, dr_softstate_t *softsp, intptr_t arg, void *iarg)
{
	dr_handle_t	*hp;

	hp = DR_PLAT_GET_HANDLE(dev, softsp->machsoftsp, arg,
				dr_init_handle, iarg);

	return (hp);
}

/*ARGSUSED*/
static void
dr_release_handle(dr_handle_t *hp)
{
	DR_PLAT_RELEASE_HANDLE(hp, dr_deinit_handle);
}

/*ARGSUSED*/
static int
dr_pre_op(dr_handle_t *hp)
{
	return (DR_PLAT_PRE_OP(hp));
}

/*ARGSUSED*/
static void
dr_post_op(dr_handle_t *hp)
{
	DR_PLAT_POST_OP(hp);
}

/*ARGSUSED*/
static void
dr_exec_op(dr_handle_t *hp)
{
	static fn_t	f = "dr_exec_op";

	switch (hp->h_cmd) {

	case DR_CMD_CONNECT:
		dr_dev_connect(hp);
		break;

	case DR_CMD_CONFIGURE:
		dr_dev_configure(hp);
		break;

	case DR_CMD_RELEASE:
		dr_dev_release(hp);
		break;

	case DR_CMD_UNCONFIGURE:
		dr_dev_unconfigure(hp);
		break;

	case DR_CMD_DISCONNECT:
		dr_dev_disconnect(hp);
		break;

	case DR_CMD_CANCEL:
		dr_dev_cancel(hp);
		break;

	case DR_CMD_STATUS:
		dr_dev_status(hp);
		break;

	case DR_CMD_IOCTL:
		dr_mach_ioctl(hp);
		break;

	default:
		cmn_err(CE_WARN,
			"dr:%s: unknown command (%d)",
			f, hp->h_cmd);
		break;
	}
}

/*ARGSUSED*/
static void
dr_dev_connect(dr_handle_t *hp)
{
	if (DR_PLAT_PROBE_BOARD(hp))
		return;

	(void) DR_PLAT_CONNECT(hp);
}

/*ARGSUSED*/
static void
dr_dev_disconnect(dr_handle_t *hp)
{
	if (DR_PLAT_DISCONNECT(hp))
		return;

	(void) DR_PLAT_DEPROBE_BOARD(hp);
}

/*ARGSUSED*/
static void
dr_dev_configure(dr_handle_t *hp)
{
	int		n;
	int32_t		pass, devnum;
	dnode_t		nodeid;
	dr_devlist_t	*devlist, *(*get_devlist)();
	dr_nodetype_t	nodetype;

	pass = 1;

	get_devlist = dr_g.ops->dr_platform_get_attach_devlist;
	if (get_devlist == NULL)
		return;

	while ((devlist = (*get_devlist)(hp, &devnum, pass)) != NULL) {
		int	err;

		err = DR_PLAT_PRE_ATTACH_DEVLIST(hp, devlist, devnum);
		if (err < 0) {
			break;
		} else if (err > 0) {
			pass++;
			continue;
		}

		for (n = 0; n < devnum; n++) {
			dr_error_t	*ep;

			ep = &devlist[n].dv_error;
			DR_SET_ERRNO(ep, 0);
			nodeid = devlist[n].dv_nodeid;
			nodetype = DR_PLAT_GET_DEVTYPE(hp, nodeid);

			switch (nodetype) {
			case DR_NT_CPU:
				dr_attach_cpu(hp, ep, nodeid);
				break;

			case DR_NT_MEM:
				dr_attach_mem(hp, ep, nodeid);
				break;

			case DR_NT_IO:
				dr_attach_io(hp, ep, nodeid);
				break;

			default:
				DR_SET_ERRNO(ep, EINVAL);
				break;
			}
		}

		err = DR_PLAT_POST_ATTACH_DEVLIST(hp, devlist, devnum);
		if (err < 0)
			break;

		pass++;
	}
}

/*ARGSUSED*/
static void
dr_dev_release(dr_handle_t *hp)
{
	int		n;
	int32_t		pass, devnum;
	dnode_t		nodeid;
	dr_devlist_t	*devlist, *(*get_devlist)();
	dr_nodetype_t	nodetype;

	pass = 1;

	get_devlist = dr_g.ops->dr_platform_get_release_devlist;
	if (get_devlist == NULL)
		return;

	while ((devlist = (*get_devlist)(hp, &devnum, pass)) != NULL) {
		int	err;

		err = DR_PLAT_PRE_RELEASE_DEVLIST(hp, devlist, devnum);
		if (err < 0) {
			break;
		} else if (err > 0) {
			pass++;
			continue;
		}

		for (n = 0; n < devnum; n++) {
			nodeid = devlist[n].dv_nodeid;
			nodetype = DR_PLAT_GET_DEVTYPE(hp, nodeid);
			/*
			 * Release results come in asynchronously.
			 */
			switch (nodetype) {
			case DR_NT_CPU:
				dr_release_cpu(hp, nodeid);
				break;

			case DR_NT_MEM:
				dr_release_mem(hp, nodeid);
				break;

			case DR_NT_IO:
				dr_release_io(hp, nodeid);
				break;

			default:
				break;
			}
		}

		err = DR_PLAT_POST_RELEASE_DEVLIST(hp, devlist, devnum);
		if (err < 0)
			break;

		pass++;
	}
}

/*ARGSUSED*/
static void
dr_dev_unconfigure(dr_handle_t *hp)
{
	int		n;
	int32_t		pass, devnum;
	dnode_t		nodeid;
	dr_devlist_t	*devlist, *(*get_devlist)();
	dr_nodetype_t	nodetype;

	pass = 1;

	get_devlist = dr_g.ops->dr_platform_get_detach_devlist;
	if (get_devlist == NULL)
		return;

	while ((devlist = (*get_devlist)(hp, &devnum, pass)) != NULL) {
		int	err;

		err = DR_PLAT_PRE_DETACH_DEVLIST(hp, devlist, devnum);
		if (err < 0) {
			break;
		} else if (err > 0) {
			pass++;
			continue;
		}

		for (n = 0; n < devnum; n++) {
			dr_error_t	*ep;

			ep = &devlist[n].dv_error;
			DR_SET_ERRNO(ep, 0);
			nodeid = devlist[n].dv_nodeid;
			nodetype = DR_PLAT_GET_DEVTYPE(hp, nodeid);

			switch (nodetype) {
			case DR_NT_CPU:
				dr_detach_cpu(hp, ep, nodeid);
				break;

			case DR_NT_MEM:
				dr_detach_mem(hp, ep, nodeid);
				break;

			case DR_NT_IO:
				dr_detach_io(hp, ep, nodeid);
				break;

			default:
				DR_SET_ERRNO(ep, EINVAL);
				break;
			}
		}

		err = DR_PLAT_POST_DETACH_DEVLIST(hp, devlist, devnum);
		if (err < 0)
			break;

		pass++;
	}
}

/*ARGSUSED*/
static void
dr_attach_cpu(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	processorid_t	cpuid;
	int		rv = 0;
	dev_info_t	*dip;
	dr_treeinfo_t	ti;
	static fn_t	f = "dr_attach_cpu";

	ASSERT(MUTEX_HELD(&cpu_lock));

	ti.flags = hp->h_flags;
	ti.errp = ep;

	if (dr_attach_branch(ddi_root_node(), nodeid, (void *)&ti))
		return;

	cpuid = DR_PLAT_GET_CPUID(hp, nodeid);
	if (cpuid < 0) {
		/*
		 * Fill in error in handle.
		 */
		rv = ENODEV;
		dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid);
		if (dip != NULL)
			ddi_walk_devs_reverse(dip, dr_detach_branch,
						(void *)&ti);
	} else if ((rv = cpu_configure(cpuid)) != 0) {
		/*
		 * Fill in error in handle.
		 */
		cmn_err(CE_WARN,
			"dr:%s: cpu_configure for cpu %d failed",
			f, cpuid);
		dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid);
		if (dip != NULL)
			ddi_walk_devs_reverse(dip, dr_detach_branch,
						(void *)&ti);
	}

	if (rv) {
		DR_SET_ERRNO(ep, rv);
	}
}

/*
 *	translate NDI error types
 *	attach flag = 0 if called during attach
 *		    = 1 if on detach
 */
void
dr_err_decode(int ndi_error, dr_error_t *ep, dev_info_t *dip, int attach)
{
	char		*p;

	ASSERT(ndi_error != 0);

	DR_SET_ERRNO(ep, 0);

	switch (ndi_error) {
	case NDI_NOMEM:
		DR_SET_ERRNO(ep, ENOMEM);
		break;

	case NDI_BUSY:
		DR_SET_ERRNO(ep, EBUSY);
		break;

	case NDI_FAILURE:
		DR_SET_ERRNO(ep, EIO);
		break;

	case NDI_BADHANDLE:
	case NDI_FAULT:
	default:
		DR_SET_ERRNO(ep, EFAULT);
		break;
	}

	if (attach)
		(void) ddi_pathname(ddi_get_parent(dip),
						DR_GET_ERRSTR(ep));
	else
		(void) ddi_pathname(dip, DR_GET_ERRSTR(ep));
	if (attach) {
		p = "/";
		(void) strcat(DR_GET_ERRSTR(ep), p);
		(void) strcat(DR_GET_ERRSTR(ep), ddi_node_name(dip));
	}
}

/*
 * Given a dnode_t of a root node child, walk down the
 * branch to allocate devinfos and attach drivers
 */
/*ARGSUSED*/
static void
dr_attach_io(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	dr_treeinfo_t	ti;

	ti.flags = hp->h_flags;
	ti.errp = ep;

	(void) dr_attach_branch(ddi_root_node(), nodeid, (void *)&ti);
}

/*
 * Walk a branch from top to bottom and attach driver for
 * each dev_info.
 */
/*ARGSUSED*/
static int
dr_attach_branch(dev_info_t *pdip, dnode_t nodeid, void *arg)
{
	dnode_t			child_id;
	dr_treeinfo_t		*tip;
	dev_info_t		*dip = NULL;
	dr_error_t		*ep;
	static int		err = 0;
	static int		len = 0;
	static int		reg[64];
	static char		buf[OBP_MAXDRVNAME];

	tip = (dr_treeinfo_t *)arg;
	ep = tip->errp;

	if ((nodeid == OBP_NONODE) || (!DDI_CF2(pdip)))
		return (DR_GET_ERRNO(ep));

	(void) prom_getprop(nodeid, OBP_NAME, (caddr_t)buf);

	/*
	 * allocate/initialize the device node
	 */
	err = ndi_devi_alloc(pdip, buf, nodeid, &dip);
	if (err != NDI_SUCCESS) {
		dr_err_decode(err, ep, NULL, 1);
		return (DR_GET_ERRNO(ep));
	}
	len = prom_getprop(nodeid, OBP_REG, (caddr_t)reg);
	if (len <= 0) {
		(void) ndi_devi_free(dip);
		return (DR_GET_ERRNO(ep));
	}

	/*
	 * bind/attach driver to the device node
	 */
	err = ndi_devi_online(dip, NDI_ONLINE_ATTACH);
	if (err != NDI_SUCCESS) {
		/*
		 * ndi_devi_online() may fail for valid
		 * reasons which are not fatal to the
		 * DR operation.  We want to stop, however
		 * there's no need to report the error.
		 */
		(void) ndi_devi_free(dip);
		return (DR_GET_ERRNO(ep));
	}

	child_id = prom_childnode(nodeid);
	if (child_id != OBP_NONODE) {
		for (; child_id != OBP_NONODE;
				child_id = prom_nextnode(child_id)) {
			(void) dr_attach_branch(dip, child_id, (void *)tip);
		}
	}

	return (DR_GET_ERRNO(ep));
}

/*ARGSUSED*/
static void
dr_attach_mem(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	struct memlist	*ml, *mc, *nmc;
	dev_info_t	*dip;
	dr_treeinfo_t	ti;

	ti.flags = hp->h_flags;
	ti.errp = ep;

	if (dr_attach_branch(ddi_root_node(), nodeid, (void *)&ti))
		return;

	ml = DR_PLAT_GET_MEMLIST(nodeid);

	for (mc = ml; mc; mc = nmc) {
		pfn_t	base;
		pgcnt_t	npgs;
		int	err;

		nmc = mc->next;

		base = (pfn_t)(mc->address >> PAGESHIFT);
		npgs = (pgcnt_t)(mc->size >> PAGESHIFT);

		kcage_range_lock();
		err = kcage_range_add(base, npgs, 1);
		kcage_range_unlock();
		if (err != 0) {
			DR_SET_ERRNO(ep, err);
			DR_SET_ERRSTR(ep, "kcage_range_add");
		} else {
			err = kphysm_add_memory_dynamic(base, npgs);
			if (err != KPHYSM_OK) {
				/*
				 * Need to fill in handle error based on err.
				 */
				switch (err) {
				case KPHYSM_ERESOURCE:
					err = ENOMEM;
					break;

				case KPHYSM_EFAULT:
					err = EFAULT;
					break;

				default:
					err = EINVAL;
					break;
				}

				DR_SET_ERRNO(ep, err);
				DR_SET_ERRSTR(ep, "kphysm_add_memory_dynamic");
			}
		}

		if (err != 0) {
			struct memlist *mt;

			/*
			 * Take back added ranges.
			 */
			kcage_range_lock();
			for (mt = ml; mt != mc; mt = mt->next) {
				err = kcage_range_delete(
					mt->address >> PAGESHIFT,
					mt->size >> PAGESHIFT);
				/* TODO: deal with error */
				err = err;
			}
			kcage_range_unlock();

			/*
			 * Need to free up remaining memlist
			 * entries.
			 */
			do {
				nmc = mc->next;
				FREESTRUCT(mc, struct memlist, 1);
			} while ((mc = nmc) != NULL);
			dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid);
			if (dip != NULL)
				ddi_walk_devs_reverse(dip, dr_detach_branch,
							(void *)&ti);
			return;
		}

		FREESTRUCT(mc, struct memlist, 1);
	}
}

/*ARGSUSED*/
static void
dr_release_cpu(dr_handle_t *hp, dnode_t nodeid)
{
	dr_callback_t	*cbp;

	/*
	 * Cannot perform done operation within context
	 * of release as it could result in deadlock.
	 */
	cbp = GETSTRUCT(dr_callback_t, 1);
	cbp->hp = hp;
	cbp->nodetype = DR_NT_CPU;
	cbp->nodeid = nodeid;

	(void) timeout(dr_release_dev_callback, (caddr_t)cbp, 1);
}

/*
 * When we reach here the memory being drain should have
 * already been reserved in dr_plat_pre_release_mem().
 * Our only task here is to kick off the "drain".
 */
/*ARGSUSED*/
static void
dr_release_mem(dr_handle_t *hp, dnode_t nodeid)
{
	int		err;
	memhandle_t	mh;
	dr_callback_t	*cbp;
	static fn_t	f = "dr_release_mem";

	cbp = GETSTRUCT(dr_callback_t, 1);
	cbp->hp = hp;
	cbp->nodetype = DR_NT_MEM;
	cbp->nodeid = nodeid;

	/*
	 * Now that all the spans to be deleted have been
	 * "added" to our memhandle, time to start the
	 * release process.
	 */
	if (DR_PLAT_GET_MEMHANDLE(hp, nodeid, &mh) < 0) {
		cmn_err(CE_WARN,
			"dr:%s: failed to get memhandle for nodeid 0x%x",
			f, (uint_t)nodeid);
		/*
		 * Have to at least issue callback since
		 * PSM layer is expecting it.
		 */
		(void) timeout(dr_release_dev_callback, (caddr_t)cbp, 1);
	} else {
		err = kphysm_del_start(mh, dr_release_mem_done, (void *)cbp);
		if (err != KPHYSM_OK) {
			kphysm_del_release(mh);
			DR_SET_ERRNO(DR_HD2ERR(hp), EPROTO);
			DR_SET_ERRSTR(DR_HD2ERR(hp), "kphysm_del_start");
			/*
			 * Cannot execute release-done callback within
			 * the context of the release operation as it
			 * could result in deadlock.
			 */
			(void) timeout(dr_release_dev_callback,
					(caddr_t)cbp, 1);
		}
	}
}

/*
 * Memory has been logically removed by the time this routine is called.
 */
/*ARGSUSED*/
static void
dr_release_mem_done(void *arg, int error)
{
	ASSERT(arg);

	if (error != KPHYSM_OK) {
		dr_handle_t	*hp = ((dr_callback_t *)arg)->hp;
		/*
		 * Need to protect against asynchronous callbacks
		 * that may also be manipulating error portion
		 * of handler.
		 */
		mutex_enter(&dr_g.cblock);
		DR_SET_ERRNO(DR_HD2ERR(hp), EPROTO);
		DR_SET_ERRSTR(DR_HD2ERR(hp), "delete_memory_thread");
		mutex_exit(&dr_g.cblock);
	}

	dr_release_dev_callback(arg);
}

/*ARGSUSED*/
static void
dr_release_io(dr_handle_t *hp, dnode_t nodeid)
{
	dr_callback_t	*cbp;

	/*
	 * Cannot perform done operation within context
	 * of release as it could result in deadlock.
	 */
	cbp = GETSTRUCT(dr_callback_t, 1);
	cbp->hp = hp;
	cbp->nodetype = DR_NT_IO;
	cbp->nodeid = nodeid;

	(void) timeout(dr_release_dev_callback, (caddr_t)cbp, 1);
}

/*ARGSUSED*/
static void
dr_release_dev_callback(void *arg)
{
	dr_callback_t	*cbp = (dr_callback_t *)arg;
	dr_handle_t	*hp = cbp->hp;
	dr_nodetype_t	nodetype = cbp->nodetype;
	dnode_t		nodeid = cbp->nodeid;

	FREESTRUCT(cbp, dr_callback_t, 1);

	mutex_enter(&dr_g.cblock);
	if (DR_PLAT_RELEASE_DONE(hp, nodetype, nodeid) == B_TRUE)
		dr_release_handle(hp);
	mutex_exit(&dr_g.cblock);
}

/*ARGSUSED*/
static void
dr_detach_cpu(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	dev_info_t 	*dip;
	processorid_t	cpuid;
	int		rv = 0;
	dr_treeinfo_t	ti;
	static fn_t	f = "dr_detach_cpu";

	ASSERT(MUTEX_HELD(&cpu_lock));

	ti.flags = hp->h_flags;
	ti.errp = ep;

	cpuid = DR_PLAT_GET_CPUID(hp, nodeid);
	if (cpuid < 0) {
		/*
		 * Fill in error in handle.
		 */
		rv = ENODEV;

	} else if ((rv = cpu_unconfigure(cpuid)) != 0) {
		/*
		 * Fill in error in handle.
		 */
		cmn_err(CE_WARN,
			"dr:%s: cpu_unconfigure for cpu %d failed",
			f, cpuid);
	}

	if (rv) {
		DR_SET_ERRNO(ep, rv);
		return;
	}

	/*
	 * Now detach cpu devinfo node from device tree.
	 */
	if ((dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid)) != NULL) {
		ddi_walk_devs_reverse(dip, dr_detach_branch, (void *)&ti);
	}
}


/*
 * remove device nodes for the branch indicated by nodeid
 */
/*ARGSUSED*/
static void
dr_detach_io(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	dev_info_t 	*dip;
	dr_treeinfo_t	ti;

	ti.flags = hp->h_flags;
	ti.errp = ep;

	if ((dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid)) != NULL) {
		ddi_walk_devs_reverse(dip, dr_detach_branch, (void *)&ti);
	}
}

/*ARGSUSED*/
static void
dr_detach_mem(dr_handle_t *hp, dr_error_t *ep, dnode_t nodeid)
{
	dr_treeinfo_t	ti;
	dev_info_t 	*dip;

	ti.flags = hp->h_flags;
	ti.errp = ep;

	if (DR_PLAT_DETACH_MEM(hp, nodeid)) {
		dr_error_t	*hep = DR_HD2ERR(hp);

		DR_SET_ERRNO(ep, DR_GET_ERRNO(hep));
		DR_SET_ERRSTR(ep, DR_GET_ERRSTR(hep));
		return;
	}

	/*
	 * Now detach mem-unit devinfo node from device tree.
	 */
	if ((dip = e_ddi_nodeid_to_dip(ddi_root_node(), nodeid)) != NULL)
		ddi_walk_devs_reverse(dip, dr_detach_branch, (void *)&ti);
}

/*
 * Walk a branch from bottom to top and detach driver or clean up
 * each dev_info.
 */
static int
dr_detach_branch(dev_info_t *dip, void *arg)
{
	int		err;
	dr_treeinfo_t	*tip = (dr_treeinfo_t *)arg;
	uint_t		flag = 0;
	dev_info_t	*pdip = ddi_get_parent(dip);

	ASSERT(tip->errp);
	/*
	 * NDI_DEVI_REMOVE flag specifies removal of the device node from
	 * the driver list and the device tree
	 */
	if (tip->flags & DR_FLAG_DEVI_REMOVE)
		flag |= NDI_DEVI_REMOVE;

	/*
	 * NDI_DEVI_FORCE flag specifies offline of nodes whose open state
	 * is not known by specfs().
	 * NOTE:
	 *	Sunfire only permits this for STREAMS devices
	 *	(ddi_streams_driver).  We need to do it so that
	 *	the mem-unit node is properly removed on detach,
	 *	otherwise on a subsequent attach we'll get an error
	 *	(WARNING: Cannot merge hwconf devinfo node mem-unit@NNN)
	 *	because the previous mem-unit node is still there.
	 */
	if (tip->flags & DR_FLAG_DEVI_FORCE &&
	    ((strncmp("mem-unit", ddi_binding_name(dip),
		strlen("mem-unit")) == 0) ||
	    ddi_streams_driver(dip) == DDI_SUCCESS))
		flag |= NDI_DEVI_FORCE;

	if (DDI_CF2(pdip)) {
		(void) ndi_post_event(dip, dip,
		    ndi_event_getcookie(DDI_DEVI_REMOVE_EVENT), NULL);
	}
	/*
	 * offline the child device node via ndi_devi_offline, using
	 * NDI flags provided in the handle.
	 */
	if ((err = ndi_devi_offline(dip, flag)) != NDI_SUCCESS) {
		dr_err_decode(err, tip->errp, dip, 0);
		return (DDI_WALK_TERMINATE);
	}

	return (DDI_WALK_CONTINUE);
}

static void
dr_dev_cancel(dr_handle_t *hp)
{
	(void) DR_PLAT_CANCEL(hp);
}

static void
dr_dev_status(dr_handle_t *hp)
{
	(void) DR_PLAT_STATUS(hp);
}

/*ARGSUSED*/
static void
dr_mach_ioctl(dr_handle_t *hp)
{
	(void) DR_PLAT_IOCTL(hp);
}

int
i_ddi_while_sibling_reverse(dev_info_t *dev,
				int (*f)(dev_info_t *, void *),
				void *arg)
{
	register dev_info_t *dip;
	register dev_info_t *sib;

	sib = dip = dev;

	while (dip != (dev_info_t *)NULL) {
		sib = (dev_info_t *)DEVI(dip)->devi_sibling;

		/* Recurse on siblings. */
		switch (i_ddi_walk_devs_reverse(dip, f, arg)) {
		case DDI_WALK_TERMINATE:
			return (DDI_WALK_TERMINATE);

		case DDI_WALK_PRUNESIB:
			return (DDI_WALK_CONTINUE);

		case DDI_WALK_CONTINUE:
		default:
			/* Continue down the sibling chain */
			break;
		}
		/*
		 * continue with other siblings, if any
		 */
		dip = sib;
	}
	return (DDI_WALK_CONTINUE);
}

/*
 * The implementation of ddi_walk_devs_reverse().
 *
 * The routine can deal with functions removing the child node, but only by the
 * fact that ddi_remove_child() will link parent->devi_child to the siblings, if
 * one exists.
 */
int
i_ddi_walk_devs_reverse(dev_info_t *dev,
			int (*f)(dev_info_t *, void *),
			void *arg)
{
	dev_info_t *lw = dev;
	dev_info_t *parent = dev;
	dev_info_t *start = dev;
	dev_info_t *sib = NULL;

	if (lw == (dev_info_t *)NULL)
		return (DDI_WALK_CONTINUE);

	/* Find the last child */
	while (ddi_get_child(lw) != NULL) {
		lw = ddi_get_child(lw);
	}

	/* lw should point to last child at this point */
	while (lw != (dev_info_t *)NULL) {
		/*
		 * If there are at that start of this tree, execute and return.
		 * Otherwise, save the parent and sibling pointer, in case lw
		 * got removed by (*f)()
		 */
		if (lw == start) {
			return ((*f)(lw, arg));
		} else {
			parent = ddi_get_parent(lw);
			sib = ddi_get_next_sibling(lw);
		}


		switch ((*f)(lw, arg)) {
		case DDI_WALK_TERMINATE:
			/*
			 * Caller is done!  Just return.
			 */
			return (DDI_WALK_TERMINATE);
			/*NOTREACHED*/

		case DDI_WALK_PRUNESIB:
			/*
			 * Caller has told us not to continue with our siblings.
			 * Set lw to point the parent and start over.
			 * Be careful lw may be removed at this point (by *f())
			 * Thus we use the saved parent pointer.
			 */
			lw = parent;
			break;

		case DDI_WALK_CONTINUE:
		default:
			/*
			 * If lw is removed by (*f)(), then parent->devi_child
			 * is pointing to the sibling.  If lw has no sibling,
			 * then we move up to the parent level and continue;
			 *
			 * If parent is at the start level, we stop.
			 */
			if ((lw == NULL) && (sib == NULL)) {
				lw = parent;
				break;
			}

			/*
			 * If we have siblings, we need to use recursion to
			 * continue with the siblings before we go back up to
			 * our parent. When all sibling nodes and their children
			 * are done, then we can go back to our parent node.
			 */
			if (sib != NULL) {
				if (i_ddi_while_sibling_reverse(
					(dev_info_t *)sib, f, arg)
					== DDI_WALK_TERMINATE) {
					return (DDI_WALK_TERMINATE);
				}
			}

			/*
			 * Set lw to our parent node and start over.
			 */
			lw = parent;
			break;
		}
	}

	return (DDI_WALK_CONTINUE);
}

/*
 * This general-purpose routine traverses the tree of dev_info nodes bottom-up,
 * starting from the end node, and calls the given function for each
 * node that it finds with the current node and the pointer arg (which
 * can point to a structure of information that the function
 * needs) as arguments.
 *
 */

void
ddi_walk_devs_reverse(dev_info_t *dev,
			int (*f)(dev_info_t *, void *),
			void *arg)
{
	(void) i_ddi_walk_devs_reverse(dev, f, arg);
}
