/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)objmgr.c	1.22	99/05/20 SMI"

#include <sys/dktp/cm.h>
#include <sys/dktp/objmgr.h>

/*
 * Local static data
 */
#ifdef	OBJMGR_DEBUG
#define	DENT	0x0001
#define	DERR	0x0002
static	int	objmgr_debug = DENT|DERR;

#endif	/* OBJMGR_DEBUG */

static struct obj_entry obj_head = {&obj_head, &obj_head, 0, 0};
static struct obj_entry *obj_lstentp = NULL;
static dev_info_t *objmgr_dip;
static kmutex_t objmgr_mutex;

/*
 *	external interface
 */

/*
 *	local functions protocol
 */
static struct obj_entry *objmgr_scan_entry(char *obj_keyp);

/*
 * Config information
 */

static ddi_intrspec_t
objmgr_get_intrspec(dev_info_t *, dev_info_t *, u_int);

static int
objmgr_add_intrspec(dev_info_t *, dev_info_t *, ddi_intrspec_t,
	ddi_iblock_cookie_t *, ddi_idevice_cookie_t *, u_int (*)(caddr_t),
	caddr_t, int);

static void
objmgr_remove_intrspec(dev_info_t *, dev_info_t *,
	ddi_intrspec_t, ddi_iblock_cookie_t);

static int
objmgr_ctl(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

struct bus_ops objmgr_bus_ops = {
	BUSO_REV,
	nullbusmap,
	objmgr_get_intrspec,		/* NO OP */
	objmgr_add_intrspec,		/* NO OP */
	objmgr_remove_intrspec,	/* NO OP */
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	objmgr_ctl,
	ddi_bus_prop_op
};

static int objmgr_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int objmgr_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

struct dev_ops objmgr_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	objmgr_attach,		/* attach */
	objmgr_detach,		/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&objmgr_bus_ops	/* bus operations */

};

/*
 * This is the loadable module wrapper.
 */

#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Object Manager",
	&objmgr_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
#ifdef OBJMGR_DEBUG
	if (objmgr_debug & DENT)
		PRF("objmgr_init: call\n");
#endif
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
objmgr_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	objmgr_dip = devi;
	mutex_init(&objmgr_mutex, NULL, MUTEX_DRIVER, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
objmgr_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

/*	check for any registered objects				*/
	mutex_enter(&objmgr_mutex);
	if (((struct obj_entry *)&obj_head)->o_forw != &obj_head) {
		mutex_exit(&objmgr_mutex);
		return (DDI_FAILURE);
	}
	mutex_exit(&objmgr_mutex);

	objmgr_dip = NULL;
	mutex_destroy(&objmgr_mutex);
	return (DDI_SUCCESS);
}

/*
 * objmgr_get_intrspec: objmgr convert an interrupt number to an
 *			   interrupt. NO OP for objmgr drivers.
 */

/*ARGSUSED*/
static ddi_intrspec_t
objmgr_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	return ((ddi_intrspec_t)0);
}

/*
 * objmgr_add_intrspec:
 *
 *	Add an interrupt specification.
 *	NO OP for objmgr drivers.
 */
/*ARGSUSED*/
static int
objmgr_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	return (DDI_FAILURE);
}

/*
 * objmgr_remove_intrspec:	remove an interrupt specification.
 *				NO OP for the objmgr drivers.
 */
/*ARGSUSED*/
static void
objmgr_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
}


/*ARGSUSED*/
static int
objmgr_ctl(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?objmgr-device: %s%d\n",
		    ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);
	case DDI_CTLOPS_INITCHILD:
	{
		dev_info_t *child = (dev_info_t *)arg;
		ddi_set_name_addr(child, "");
		return (DDI_SUCCESS);
	}
	default:
		return (DDI_FAILURE);
	}
}


static struct obj_entry *
objmgr_scan_entry(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;

	if (obj_lstentp) {
		if (strcmp(obj_lstentp->o_keyp, obj_keyp) == 0)
			return (obj_lstentp);
	}
	for (obj_ep = &obj_head; obj_ep->o_forw != &obj_head; ) {
		obj_ep = obj_ep->o_forw;
		if (strcmp(obj_ep->o_keyp, obj_keyp) == 0) {
			obj_lstentp = obj_ep;
			return (obj_ep);
		}
	}
	obj_lstentp = NULL;
	return (NULL);
}

int
objmgr_ins_entry(char *obj_keyp, opaque_t obj_token, char *obj_modgrp)
{
	register struct	obj_entry *obj_ep;
	register struct	obj_entry *new_ep;

/*	search to the end of the list					*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);
	if (obj_ep) {
		mutex_exit(&objmgr_mutex);
		return (DDI_SUCCESS);
	}

	new_ep = (struct obj_entry *)kmem_zalloc(sizeof (*obj_ep), KM_SLEEP);
	obj_head.o_back->o_forw = new_ep;
	new_ep->o_forw = &obj_head;
	new_ep->o_back = obj_head.o_back;
	obj_head.o_back  = new_ep;

	new_ep->o_keyp   = obj_keyp;
	new_ep->o_cfunc  = (opaque_t (*)())obj_token;
	new_ep->o_modgrp = obj_modgrp;
	obj_lstentp = new_ep;
	mutex_exit(&objmgr_mutex);
	return (DDI_SUCCESS);
}


int
objmgr_del_entry(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;
	register struct	obj_entry *modgrp_ep;

	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);
	if (!obj_ep) {
		mutex_exit(&objmgr_mutex);
		return (DDI_SUCCESS);
	}
	if (obj_ep->o_refcnt) {
		mutex_exit(&objmgr_mutex);
		return (DDI_FAILURE);
	}

/*	check for reference counts of other modules in the same group	*/
	if (obj_ep->o_modgrp) {
		for (modgrp_ep = obj_head.o_forw; modgrp_ep != &obj_head; ) {
			if (modgrp_ep->o_modgrp &&
			    (strcmp(modgrp_ep->o_modgrp, obj_ep->o_modgrp)) ==
				0) {
				if (modgrp_ep->o_refcnt) {
					mutex_exit(&objmgr_mutex);
					return (DDI_FAILURE);
				}
			}
			modgrp_ep = modgrp_ep->o_forw;
		}
	}

/*	unlink entry							*/
	obj_ep->o_back->o_forw = obj_ep->o_forw;
	obj_ep->o_forw->o_back = obj_ep->o_back;
	obj_lstentp = NULL;
	mutex_exit(&objmgr_mutex);
	kmem_free((caddr_t)obj_ep, sizeof (struct obj_entry));
	return (DDI_SUCCESS);

}

int
objmgr_load_obj(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;
	register struct	obj_entry *modgrp_ep;
	register int	 modid;
	char	 obj_keypath[OBJNAMELEN];
	int	 obj_keylen;

/*	search to the end of the list					*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);
	mutex_exit(&objmgr_mutex);
	if (obj_ep)
		return (DDI_SUCCESS);

	obj_keylen = sizeof (obj_keypath);
	if (ddi_prop_op(DDI_DEV_T_NONE, objmgr_dip, PROP_LEN_AND_VAL_BUF,
		DDI_PROP_CANSLEEP|DDI_PROP_DONTPASS, obj_keyp,
		(caddr_t)obj_keypath, &obj_keylen) != DDI_PROP_SUCCESS) {
#ifdef OBJMGR_DEBUG
		if (objmgr_debug & DERR)
			PRF("objmgr_load_entry: prop undefined= %s\n",
				obj_keyp);
#endif

		return (DDI_FAILURE);
	}
	obj_keypath[obj_keylen] = (char)0;
	if ((modid = modload((char *)NULL, obj_keypath)) == -1)
		return (DDI_FAILURE);

/*	scan the new list						*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);

/*	check for insertion of entry failure				*/
	if (!obj_ep) {
		mutex_exit(&objmgr_mutex);
		return (DDI_FAILURE);
	}

	obj_ep->o_modid = modid;
/*	assign module id for other modules in the same group		*/
	if (obj_ep->o_modgrp) {
		for (modgrp_ep = obj_head.o_forw; modgrp_ep != &obj_head; ) {
			if (modgrp_ep->o_modgrp &&
			    (strcmp(modgrp_ep->o_modgrp, obj_ep->o_modgrp) ==
				0))
				modgrp_ep->o_modid = modid;
			modgrp_ep = modgrp_ep->o_forw;
		}
	}
	mutex_exit(&objmgr_mutex);
	return (DDI_SUCCESS);
}

void
objmgr_unload_obj(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;
	register int	modid;

/*	search to the end of the list					*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);

	if (obj_ep) {
		if (!obj_ep->o_refcnt) {
			modid = obj_ep->o_modid;
			mutex_exit(&objmgr_mutex);

			(void) modunload(modid);
			return;
		}
	}
	mutex_exit(&objmgr_mutex);
	return;

}

opaque_t
objmgr_create_obj(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;
	opaque_t objp;

/*	search to the end of the list					*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);
	if (obj_ep) {
		obj_ep->o_refcnt++;
		mutex_exit(&objmgr_mutex);
		objp = obj_ep->o_cfunc();
		if (objp)
			return (objp);
		mutex_enter(&objmgr_mutex);
		obj_ep->o_refcnt--;
	}
	mutex_exit(&objmgr_mutex);
	return (NULL);
}

int
objmgr_destroy_obj(char *obj_keyp)
{
	register struct	obj_entry *obj_ep;

/*	search to the end of the list					*/
	mutex_enter(&objmgr_mutex);
	obj_ep = objmgr_scan_entry(obj_keyp);
	if (obj_ep)
		obj_ep->o_refcnt--;
	mutex_exit(&objmgr_mutex);
	return (DDI_SUCCESS);

}
