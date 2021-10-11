/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)driver_lyr.c	1.8	99/09/13 SMI"

/*
 * Layered driver support.
 *
 * Warning:
 *
 *	These are Sun-Private interfaces, NOT part of the DDI.
 *	They are subject to more or less arbitrary change from
 *	release to release.  Contact the DDI i-team if you have
 *	comments, or for further information.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/fs/snode.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/bootconf.h>
#include <sys/pathname.h>

#include <sys/stat.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/autoconf.h>
#include <sys/sunddi_lyr.h>
#include <sys/errno.h>
#include <sys/debug.h>


/*
 * This handle is opaque to its callers, and is only
 * known to the contents of this file.
 *
 * As well as being a handy abstraction, it enables
 * us to reduce errors by enforcing the correct open/close
 * pairing in the device filesystem (specfs).
 *
 * XXX	Maybe make this a struct file?  (Though f_flag is only a
 *	short, and we rely on it for FKIOCTL security!!)
 */
struct ddi_lyr_handle {
	dev_t		lh_dev;		/* cache of v_rdev */
	dev_info_t	lh_devi;	/* cache of csp->s_dip */
	struct vnode	*lh_vp;		/* underlying vnode */
	int		lh_oflag;	/* flags at open time + OTYP_LYR */
};

#define	LH_TO_DEV(lh)	((struct ddi_lyr_handle *)lh)->lh_dev
#define	LH_TO_DEVI(lh)	((struct ddi_lyr_handle *)lh)->lh_devi

static int ddi_lyr_open_by_vp(struct vnode *vp, int flag, int *otyp,
    cred_t *cr, ddi_lyr_handle_t *lyr_handle_p);
static struct vnode *ddi_lyr_create_vp(dev_t dev);

/*
 * Open the underlying device by physical pathname
 */
int
ddi_lyr_open_by_name(char *pathname, int flag, int *otyp, cred_t *cr,
    ddi_lyr_handle_t *lyr_handle_p)
{
	dev_t dev;
	struct vnode *vp;

	if (modrootloaded) {
		if (lookupname(pathname, UIO_SYSSPACE, FOLLOW,
		    NULLVPP, &vp) == 0) {

			if (vp->v_type != VCHR && vp->v_type != VBLK) {
				VN_RELE(vp);
				return (ENODEV);
			}
			return (ddi_lyr_open_by_vp(vp, flag, otyp,
			    cr, lyr_handle_p));
		}
	}
	if ((dev = ddi_pathname_to_dev_t(pathname)) == (dev_t)-1)
		return (ENXIO);
	vp = ddi_lyr_create_vp(dev);
	return (ddi_lyr_open_by_vp(vp, flag, otyp, cr, lyr_handle_p));
}

/*
 * Open an underlying device by dev_t.
 */
int
ddi_lyr_open_by_dev_t(dev_t *devp, int flag, int *otyp, cred_t *cr,
    ddi_lyr_handle_t *lyr_handle_p)
{
	struct vnode *vp;
	int err;

	vp = ddi_lyr_create_vp(*devp);

	err = ddi_lyr_open_by_vp(vp, flag, otyp, cr, lyr_handle_p);

	*devp = vp->v_rdev;

	return (err);
}

static struct vnode *
ddi_lyr_create_vp(dev_t dev)
{
	struct dev_info *dip;
	struct ddi_minor_data *minordata;
	int type;

	for (dip = (struct dev_info *)devnamesp[getmajor(dev)].dn_head;
		dip != NULL; dip = dip->devi_next) {

		for (minordata = dip->devi_minor; minordata != NULL;
		    minordata = minordata->next) {
			if (minordata->type != DDM_ALIAS) {
				if (minordata->ddm_dev == dev) {
					type = minordata->ddm_spec_type;
					goto found;
				}
			} else {
				if (minordata->ddm_adev == dev) {
					type = minordata->ddm_aspec_type;
					goto found;
				}
			}
		}
	}

	return (NULL);

found:
	switch (type) {
	case S_IFCHR:
		return (makespecvp(dev, VCHR));
	case S_IFBLK:
		return (makespecvp(dev, VBLK));
	}
	return (NULL);
}

static int
ddi_lyr_open_by_vp(struct vnode *vp, int flag, int *otyp, cred_t *cr,
    ddi_lyr_handle_t *lyr_handle_p)
{
	struct ddi_lyr_handle *lhp;
	int error;

	flag |= FKLYR;
	if ((error = VOP_OPEN(&vp, flag, cr)) != 0) {
		VN_RELE(vp);
		return (error);
	}

	/*
	 * STREAMS are forbidden
	 */
	if (STREAMSTAB(getmajor(vp->v_rdev))) {
		(void) VOP_CLOSE(vp, flag, 1, (offset_t)0, cr);
		VN_RELE(vp);
		return (ENODEV);
	}

	lhp = kmem_alloc(sizeof (*lhp), KM_SLEEP);
	lhp->lh_vp = vp;
	lhp->lh_dev = vp->v_rdev;				/* a cache */
	ASSERT(vp->v_op == spec_getvnodeops());
	lhp->lh_devi = VTOS(VTOS(vp)->s_commonvp)->s_dip;	/* "  " */
	lhp->lh_oflag = flag;

	*lyr_handle_p = (ddi_lyr_handle_t)lhp;
	*otyp = vp->v_type == VCHR ? OTYP_CHR : OTYP_BLK;

	return (error);
}

/*
 * Close a layered device.
 */
int
ddi_lyr_close(ddi_lyr_handle_t lh, cred_t *cr)
{
	struct vnode *vp;
	struct ddi_lyr_handle *lhp;
	int error;

	if ((lhp = (struct ddi_lyr_handle *)lh) == NULL) {
		cmn_err(CE_WARN, "layered close on null handle");
		return (ENXIO);
	}

	if ((vp = lhp->lh_vp) == NULL)
		return (ENXIO);

	ASSERT(vp->v_type == VCHR || vp->v_type == VBLK);
	error = VOP_CLOSE(vp, lhp->lh_oflag, 1, (offset_t)0, cr);
	lhp->lh_vp = NULL;
	VN_RELE(vp);
	kmem_free(lhp, sizeof (*lhp));
	return (error);
}

/*
 * Most of these routines are simple duplicates of the stuff
 * in driver.c - though there are some subtle gotchas ..
 *
 * On sparc with SC2.0, most of these routines expand to
 *
 *	save
 *	call the routine
 *	restore ! in the delay slot
 *
 * which doesn't really matter too much as they're unlikely to be
 * leaf routines underneath. Thus most of the time we're just
 * prefaulting a register window that we'll do a save on in the
 * next instruction anyway.
 */
int
ddi_lyr_strategy(ddi_lyr_handle_t lh, struct buf *bp)
{
	bp->b_edev = LH_TO_DEV(lh);
	bp->b_dev = cmpdev(bp->b_edev);
	return (bdev_strategy(bp));
}

int
ddi_lyr_print(ddi_lyr_handle_t lh, char *str)
{
	return (bdev_print(LH_TO_DEV(lh), str));
}

int
ddi_lyr_dump(ddi_lyr_handle_t lh, caddr_t addr, daddr_t blkno, int nblk)
{
	return (bdev_dump(LH_TO_DEV(lh), addr, blkno, nblk));
}

int
ddi_lyr_read(ddi_lyr_handle_t lh, struct uio *uiop, cred_t *credp)
{
	return (cdev_read(LH_TO_DEV(lh), uiop, credp));
}

int
ddi_lyr_write(ddi_lyr_handle_t lh, struct uio *uiop, cred_t *credp)
{
	return (cdev_write(LH_TO_DEV(lh), uiop, credp));
}

int
ddi_lyr_ioctl(ddi_lyr_handle_t lh, int cmd, intptr_t arg, int mode,
	cred_t *cr, int *rvalp)
{
	register struct ddi_lyr_handle *lh_p = (struct ddi_lyr_handle *)lh;

	return (cdev_ioctl(lh_p->lh_dev, cmd, arg, mode | lh_p->lh_oflag,
		cr, rvalp));
}

int
ddi_lyr_mmap(ddi_lyr_handle_t lh, off_t off, int prot)
{
	register struct cb_ops	*cb;
	register dev_t dev = LH_TO_DEV(lh);

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return (cdev_mmap(cb->cb_mmap, dev, off, prot));
}

int
ddi_lyr_chpoll(ddi_lyr_handle_t lh, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	return (cdev_poll(LH_TO_DEV(lh), events, anyyet, reventsp, phpp));
}

/*
 * Rather than expect the layering driver to figure out the
 * right dev_info node to pass in here, we use the fact that we
 * already looked it up at VOP_OPEN time.
 *
 * This seems to have the right property of not requiring
 * the layered driver to get at the devinfo node of the underlying
 * driver (which would violate the layering).
 *
 * XXX	The ugly alternative is to expose e_ddi_get_dev_info() and
 *	the corresponding hold/rele protocol.  There's no need for
 *	the hold/rele protocol here anyway- we *know* we're open
 *	by virtue of having the handle
 */
int
ddi_lyr_prop_op(ddi_lyr_handle_t lh, ddi_prop_op_t prop_op,
	int mod_flags, char *name, caddr_t valuep, int *length)
{
	dev_info_t *devi;

	if ((devi = LH_TO_DEVI(lh)) == NULL)
		return (DDI_PROP_NOT_FOUND);
	return (cdev_prop_op(LH_TO_DEV(lh), devi, prop_op, mod_flags,
		name, valuep, length));
}
