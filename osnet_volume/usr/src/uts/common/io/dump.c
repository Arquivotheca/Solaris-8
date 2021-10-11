/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dump.c	1.2	99/04/26 SMI"

/*
 * Dump driver.  Provides ioctls to get/set crash dump configuration.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/dumphdr.h>
#include <sys/dumpadm.h>
#include <sys/pathname.h>
#include <sys/file.h>
#include <vm/anon.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static dev_info_t *dump_devi;

static int
dump_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	if (ddi_create_minor_node(devi, "dump", S_IFCHR, 0, NULL, NULL) ==
	    DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	dump_devi = devi;
	return (DDI_SUCCESS);
}

static int
dump_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
dump_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = dump_devi;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
dump_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cred, int *rvalp)
{
	uint64_t size;
	int error = 0;
	char *pathbuf = kmem_zalloc(MAXPATHLEN, KM_SLEEP);
	vnode_t *vp;

	switch (cmd) {
	case DIOCGETDUMPSIZE:
		if (dump_conflags & DUMP_ALL)
			size = ptob((uint64_t)physmem) / DUMP_COMPRESS_RATIO;
		else
			size = ptob((uint64_t)(availrmem_initial - availrmem -
			    k_anoninfo.ani_mem_resv)) / DUMP_COMPRESS_RATIO;
		if (copyout(&size, (void *)arg, sizeof (size)) < 0)
			error = EFAULT;
		break;

	case DIOCGETCONF:
		mutex_enter(&dump_lock);
		*rvalp = dump_conflags;
		if (dumpvp && !(dumpvp->v_flag & VISSWAP))
			*rvalp |= DUMP_EXCL;
		mutex_exit(&dump_lock);
		break;

	case DIOCSETCONF:
		mutex_enter(&dump_lock);
		if (arg == DUMP_KERNEL || arg == DUMP_ALL)
			dump_conflags = arg;
		else
			error = EINVAL;
		mutex_exit(&dump_lock);
		break;

	case DIOCGETDEV:
		mutex_enter(&dump_lock);
		if (dumppath == NULL) {
			mutex_exit(&dump_lock);
			error = ENODEV;
			break;
		}
		(void) strcpy(pathbuf, dumppath);
		mutex_exit(&dump_lock);
		error = copyoutstr(pathbuf, (void *)arg, MAXPATHLEN, NULL);
		break;

	case DIOCSETDEV:
	case DIOCTRYDEV:
		if ((error = copyinstr((char *)arg, pathbuf, MAXPATHLEN,
		    NULL)) != 0 || (error = lookupname(pathbuf, UIO_SYSSPACE,
		    FOLLOW, NULLVPP, &vp)) != 0)
			break;
		mutex_enter(&dump_lock);
		error = dumpinit(vp, pathbuf, cmd == DIOCTRYDEV);
		mutex_exit(&dump_lock);
		break;

	case DIOCDUMP:
		mutex_enter(&dump_lock);
		if (dumpvp == NULL)
			error = ENODEV;
		else if (dumpvp->v_flag & VISSWAP)
			error = EBUSY;
		else
			dumpsys();
		mutex_exit(&dump_lock);
		break;

	default:
		error = ENXIO;
	}

	kmem_free(pathbuf, MAXPATHLEN);
	return (error);
}

struct cb_ops dump_cb_ops = {
	nulldev,		/* open */
	nulldev,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	dump_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev, 			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_NEW|D_MP		/* Driver compatibility flag */
};

struct dev_ops dump_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt */
	dump_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	dump_attach,		/* attach */
	dump_detach,		/* detach */
	nodev,			/* reset */
	&dump_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops, "crash dump driver", &dump_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
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
