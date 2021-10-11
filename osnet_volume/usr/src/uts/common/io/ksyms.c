/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ksyms.c	1.23	99/07/07 SMI"

/*
 * ksyms driver - exports a single symbol/string table for the kernel
 * by concatenating all the module symbol/string tables.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/kobj.h>
#include <sys/ksyms.h>
#include <sys/vmsystm.h>
#include <vm/seg_vn.h>
#include <sys/atomic.h>
#include <sys/compress.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

typedef struct ksyms_image {
	caddr_t	ksyms_base;	/* base address of image */
	size_t	ksyms_size;	/* size of image */
} ksyms_image_t;

int nksyms_clones;		/* tunable: max clones of this device */

static ksyms_image_t *ksyms_clones;	/* clone device array */
static dev_info_t *ksyms_devi;

static char *
ksyms_mapin(char *base, size_t size)
{
	size_t rlen = roundup(size, PAGESIZE);
	struct as *as = curproc->p_as;
	char *addr;

	as_rangelock(as);
	map_addr(&addr, rlen, 0, 1, 0);
	if (addr == NULL || as_map(as, addr, rlen, segvn_create, zfod_argsp)) {
		as_rangeunlock(as);
		return (NULL);
	}
	as_rangeunlock(as);
	if (copyout(base, addr, size)) {
		(void) as_unmap(as, addr, rlen);
		return (NULL);
	}
	return (addr);
}

/*
 * Copy a snapshot of the kernel symbol table into the user's address space.
 */
/* ARGSUSED */
static int
ksyms_open(dev_t *devp, int flag, int otyp, struct cred *cred)
{
	minor_t clone;
	size_t size = 0;
	size_t realsize;
	char *addr;
	char *base = NULL;

	ASSERT(getminor(*devp) == 0);

	for (;;) {
		realsize = ksyms_snapshot(bcopy, base, size);
		if (realsize <= size)
			break;
		kmem_free(base, size);
		size = realsize;
		base = kmem_alloc(size, KM_SLEEP);
	}

	addr = ksyms_mapin(base, realsize);
	kmem_free(base, size);
	if (addr == NULL)
		return (EOVERFLOW);

	/*
	 * Reserve a clone entry.  Note that we don't use clone 0
	 * since that's the "real" minor number.
	 */
	for (clone = 1; clone < nksyms_clones; clone++) {
		if (casptr(&ksyms_clones[clone].ksyms_base, 0, addr) == 0) {
			ksyms_clones[clone].ksyms_size = size;
			*devp = makedevice(getemajor(*devp), clone);
			(void) ddi_prop_update_int(*devp, ksyms_devi,
			    "size", size);
			modunload_disable();
			return (0);
		}
	}
	cmn_err(CE_NOTE, "ksyms: too many open references");
	(void) as_unmap(curproc->p_as, base, roundup(size, PAGESIZE));
	return (ENXIO);
}

/* ARGSUSED */
static int
ksyms_close(dev_t dev, int flag, int otyp, struct cred *cred)
{
	minor_t clone = getminor(dev);

	(void) as_unmap(curproc->p_as, ksyms_clones[clone].ksyms_base,
	    roundup(ksyms_clones[clone].ksyms_size, PAGESIZE));
	ksyms_clones[clone].ksyms_base = 0;
	modunload_enable();
	(void) ddi_prop_remove(dev, ksyms_devi, "size");
	return (0);
}

/* ARGSUSED */
static int
ksyms_read(dev_t dev, struct uio *uio, struct cred *cred)
{
	ksyms_image_t *kip = &ksyms_clones[getminor(dev)];
	int error;
	char *buf;
	off_t off = uio->uio_offset;
	size_t len = uio->uio_resid;

	if ((size_t)(off + len) > kip->ksyms_size)
		len = kip->ksyms_size - off;

	if (off < 0 || len > kip->ksyms_size)
		return (EFAULT);

	if (len == 0)
		return (0);

	/*
	 * The ksyms image is stored in the user's address space,
	 * so we have to copy it into the kernel from userland,
	 * then copy it back out to the specified address.
	 */
	buf = kmem_alloc(len, KM_SLEEP);
	if (copyin(kip->ksyms_base + off, buf, len))
		error = EFAULT;
	else
		error = uiomove(buf, len, UIO_READ, uio);
	kmem_free(buf, len);

	return (error);
}

/* ARGSUSED */
static int
ksyms_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
    uint_t prot, uint_t maxprot, uint_t flags, struct cred *cred)
{
	ksyms_image_t *kip = &ksyms_clones[getminor(dev)];
	int error = 0;
	char *buf;

	if (flags & MAP_FIXED)
		return (ENOTSUP);

	if (off < 0 || len <= 0 || (size_t)(off + len) > kip->ksyms_size)
		return (EINVAL);

	buf = kmem_alloc(len, KM_SLEEP);
	if (copyin(kip->ksyms_base + off, buf, len))
		error = EFAULT;
	else if ((*addrp = ksyms_mapin(buf, len)) == NULL)
		error = EOVERFLOW;
	kmem_free(buf, len);
	return (error);
}

/* ARGSUSED */
static int
ksyms_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = ksyms_devi;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

static int
ksyms_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	if (ddi_create_minor_node(devi, "ksyms", S_IFCHR, 0, NULL, NULL) ==
	    DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	ksyms_devi = devi;
	return (DDI_SUCCESS);
}

static int
ksyms_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

static struct cb_ops ksyms_cb_ops = {
	ksyms_open,		/* open */
	ksyms_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	ksyms_read,		/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	ksyms_segmap,		/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops ksyms_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ksyms_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	ksyms_attach,		/* attach */
	ksyms_detach,		/* detach */
	nodev,			/* reset */
	&ksyms_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* no bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops, "kernel symbols driver", &ksyms_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	int error;

	if (nksyms_clones == 0)
		nksyms_clones = maxusers + 50;

	ksyms_clones = kmem_zalloc(nksyms_clones *
	    sizeof (ksyms_image_t), KM_SLEEP);

	if ((error = mod_install(&modlinkage)) != 0)
		kmem_free(ksyms_clones, nksyms_clones * sizeof (ksyms_image_t));

	return (error);
}

int
_fini(void)
{
	int error;

	if ((error = mod_remove(&modlinkage)) == 0)
		kmem_free(ksyms_clones, nksyms_clones * sizeof (ksyms_image_t));
	return (error);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
