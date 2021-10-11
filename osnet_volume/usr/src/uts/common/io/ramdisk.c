/*
 * Copyright (c) 1991, by Sun Microsystems Inc.
 */

#pragma ident	"@(#)ramdisk.c	1.13	95/12/01 SMI"

/*
 * Ramdisk pseudo-device to support I/O to kernel heap.
 *
 * This is a simple SunDDI example driver.  It is NOT intended
 * to be shipped as part of the final product - there are many
 * better ways of using memory!
 *
 * To use the ramdisk with the default ramsize below do:
 *
 * # mkfs -F ufs -o nsect=8,ntrack=8,free=5 /devices/pseudo/ramdisk@0:c,raw 1024
 * # mount -F ufs /devices/pseudo/ramdisk@0:c /mount-point
 *
 * By appending entries to the /etc/devlink.tab file, you can
 * make several ramdisks appear, all with the same geometry.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	DEFAULT_MAXPHYS	(63 * 1024)			/* '126b' */
#define	DEFAULT_SIZE	((u_long)(1024*DEV_BSIZE))	/* 1/2 a meagrebyte */

/*
 * Patch these in the /etc/system file
 */
static int ramdisk_maxphys = DEFAULT_MAXPHYS;
static int ramdisk_size = DEFAULT_SIZE;

/*
 * The entire state of each ramdisk device.
 */
typedef struct {
	char		*ram;		/* the memory we use */
	int		ramsize;	/* how much is there */
	int		maxphys;	/* how big per transfer */
	dev_info_t	*dip;		/* my devinfo handle */
} rd_devstate_t;

/*
 * An opaque handle where our set of ramdisk devices lives
 */
static void *rd_state;

static int rd_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int rd_strategy(struct buf *bp);
static int rd_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int rd_write(dev_t dev, struct uio *uiop, cred_t *credp);
static int rd_print(dev_t dev, char *str);

static struct cb_ops rd_cb_ops = {
	rd_open,
	nulldev,	/* close */
	rd_strategy,
	rd_print,
	nodev,		/* dump */
	rd_read,
	rd_write,
	nodev,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

static int rd_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int rd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int rd_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static struct dev_ops rd_ops = {
	DEVO_REV,
	0,
	rd_getinfo,
	nulldev,	/* identify */
	nulldev,	/* probe */
	rd_attach,
	rd_detach,
	nodev,		/* reset */
	&rd_cb_ops,
	(struct bus_ops *)0
};


extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"ramdisk driver v1.13",
	&rd_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&rd_state,
	    sizeof (rd_devstate_t), 1)) != 0) {
		return (e);
	}

	if ((e = mod_install(&modlinkage)) != 0)  {
		ddi_soft_state_fini(&rd_state);
	}

	return (e);
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)  {
		return (e);
	}
	ddi_soft_state_fini(&rd_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
rd_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int nblocks;
	int instance;
	rd_devstate_t *rsp;

	switch (cmd) {

	case DDI_ATTACH:

		instance = ddi_get_instance(dip);

		if (ddi_soft_state_zalloc(rd_state, instance) != DDI_SUCCESS) {
			cmn_err(CE_CONT, "%s%d: can't allocate state\n",
			    ddi_get_name(dip), instance);
			return (DDI_FAILURE);
		} else
			rsp = ddi_get_soft_state(rd_state, instance);

		rsp->maxphys = ramdisk_maxphys;
		rsp->ramsize = ramdisk_size;

		rsp->ram = kmem_alloc(rsp->ramsize, KM_NOSLEEP);
		if (rsp->ram == (void *)0) {
			cmn_err(CE_CONT,
			    "%s%d: can't allocate %d bytes for disk\n",
			    ddi_get_name(dip), instance, rsp->ramsize);
			goto attach_failed;
		}

		/*
		 * The 'nblocks' property is mandatory for block devices
		 */
		nblocks = rsp->ramsize / DEV_BSIZE;
		if (ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		    "nblocks", (caddr_t)&nblocks, sizeof (int))
		    != DDI_PROP_SUCCESS) {
			cmn_err(CE_CONT, "%s%d: can't create nblocks prop\n",
			    ddi_get_name(dip), instance);
			goto attach_failed;
		}

		if ((ddi_create_minor_node(dip,
		    "c,raw", S_IFCHR, instance, NULL, 0) == DDI_FAILURE) ||
		    (ddi_create_minor_node(dip,
		    "c", S_IFBLK, instance, NULL, 0) == DDI_FAILURE)) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}

		rsp->dip = dip;
		ddi_report_dev(dip);
		cmn_err(CE_CONT, "%s%d: misusing %d bytes of memory\n",
			ddi_get_name(dip), ddi_get_instance(dip),
			rsp->ramsize);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

attach_failed:
	/*
	 * Use our own detach routine to toss
	 * away any stuff we allocated above.
	 */
	(void) rd_detach(dip, DDI_DETACH);
	return (DDI_FAILURE);
}

static int
rd_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance;
	register rd_devstate_t *rsp;

	switch (cmd) {

	case DDI_DETACH:
		/*
		 * Undo what we did in rd_attach, freeing resources
		 * and removing things we installed.  The system
		 * framework guarantees we are not active with this devinfo
		 * node in any other entry points at this time.
		 */
		ddi_prop_remove_all(dip);
		instance = ddi_get_instance(dip);
		rsp = ddi_get_soft_state(rd_state, instance);
		if (rsp->ram && rsp->ramsize)
			kmem_free(rsp->ram, rsp->ramsize);
		ddi_remove_minor_node(dip, NULL);
		ddi_soft_state_free(rd_state, instance);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
rd_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	rd_devstate_t *rsp;
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((rsp = ddi_get_soft_state(rd_state,
		    getminor((dev_t)arg))) != NULL) {
			*result = rsp->dip;
			error = DDI_SUCCESS;
		} else
			*result = NULL;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;

	default:
		break;
	}

	return (error);
}


/*ARGSUSED*/
static int
rd_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	if (otyp != OTYP_BLK && otyp != OTYP_CHR)
		return (EINVAL);

	if (ddi_get_soft_state(rd_state, getminor(*devp)) == NULL)
		return (ENXIO);

	return (0);
}

static void
rd_minphys(struct buf *bp)
{
	rd_devstate_t *rsp;

	rsp = ddi_get_soft_state(rd_state, getminor(bp->b_edev));
	if (bp->b_bcount > rsp->maxphys)
		bp->b_bcount = rsp->maxphys;
}

/*ARGSUSED*/
static int
rd_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	int instance = getminor(dev);
	rd_devstate_t *rsp = ddi_get_soft_state(rd_state, instance);

	if (uiop->uio_offset >= rsp->ramsize)
		return (EINVAL);

	return (physio(rd_strategy, (struct buf *)0, dev, B_READ,
	    rd_minphys, uiop));
}

/*ARGSUSED*/
static int
rd_write(dev_t dev, register struct uio *uiop, cred_t *credp)
{
	int instance = getminor(dev);
	rd_devstate_t *rsp = ddi_get_soft_state(rd_state, instance);

	if (uiop->uio_offset >= rsp->ramsize)
		return (EINVAL);

	return (physio(rd_strategy, (struct buf *)0, dev, B_WRITE,
	    rd_minphys, uiop));
}

static int
rd_strategy(struct buf *bp)
{
	rd_devstate_t *rsp;
	u_long offset = bp->b_blkno * DEV_BSIZE;

	rsp = ddi_get_soft_state(rd_state, getminor(bp->b_edev));
	if (rsp == NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
	} else if (offset >= rsp->ramsize) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
	} else {
		register caddr_t	buf_addr, raddr;
		register unsigned	nbytes;

		raddr = rsp->ram + offset;
		nbytes = min(bp->b_bcount, rsp->ramsize - offset);

		bp_mapin(bp);

		buf_addr = bp->b_un.b_addr;

		if (bp->b_flags & B_READ)
			(void) bcopy(raddr, buf_addr, nbytes);
		else
			(void) bcopy(buf_addr, raddr, nbytes);

		bp->b_resid = bp->b_bcount - nbytes;
	}

	biodone(bp);
	return (0);
}

static int
rd_print(dev_t dev, char *str)
{
	int instance = getminor(dev);
	rd_devstate_t *rsp = ddi_get_soft_state(rd_state, instance);

	cmn_err(CE_WARN, "%s%d: %s\n", ddi_get_name(rsp->dip), instance, str);
	return (0);
}
