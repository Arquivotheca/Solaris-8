/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989, 1996-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)specvnops.c	1.127	99/12/15 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitmap.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/open.h>
#include <sys/swap.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/stream.h>
#include <sys/strsubr.h>

#include <sys/proc.h>
#include <sys/user.h>
#include <sys/session.h>
#include <sys/vmsystm.h>
#include <sys/vtrace.h>
#include <sys/dc_ki.h>

#include <sys/fs/snode.h>

#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_dev.h>
#include <vm/seg_vn.h>

#include <fs/fs_subr.h>

#include <sys/esunddi.h>
#include <sys/autoconf.h>

static int spec_open(struct vnode **, int, struct cred *);
static int spec_close(struct vnode *, int, int, offset_t, struct cred *);
static int spec_read(struct vnode *, struct uio *, int, struct cred *);
static int spec_write(struct vnode *, struct uio *, int, struct cred *);
static int spec_ioctl(struct vnode *, int, intptr_t, int, struct cred *, int *);
static int spec_getattr(struct vnode *, struct vattr *, int, struct cred *);
static int spec_setattr(struct vnode *, struct vattr *, int, struct cred *);
static int spec_access(struct vnode *, int, int, struct cred *);
static int spec_fsync(struct vnode *, int, struct cred *);
static void spec_inactive(struct vnode *, struct cred *);
static int spec_fid(struct vnode *, struct fid *);
static int spec_seek(struct vnode *, offset_t, offset_t *);
static int spec_frlock(struct vnode *, int, struct flock64 *, int, offset_t,
    struct cred *);
static int spec_realvp(struct vnode *, struct vnode **);

static int spec_getpage(struct vnode *, offset_t, size_t, uint_t *, page_t **,
    size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int spec_putapage(struct vnode *, page_t *, u_offset_t *, size_t *, int,
	struct cred *);
static struct buf *spec_startio(struct vnode *, page_t *, u_offset_t, size_t,
	int);
static int spec_getapage(struct vnode *, u_offset_t, size_t, uint_t *,
    page_t **, size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int spec_map(struct vnode *, offset_t, struct as *, caddr_t *, size_t,
    uchar_t, uchar_t, uint_t, struct cred *);
static int spec_addmap(struct vnode *, offset_t, struct as *, caddr_t, size_t,
    uchar_t, uchar_t, uint_t, struct cred *);
static int spec_delmap(struct vnode *, offset_t, struct as *, caddr_t, size_t,
    uint_t, uint_t, uint_t, struct cred *);

static int spec_poll(struct vnode *, short, int, short *, struct pollhead **);
static int spec_dump(struct vnode *, caddr_t, int, int);
static int spec_pageio(struct vnode *, page_t *, u_offset_t, size_t, int,
    cred_t *);

static int spec_getsecattr(struct vnode *, vsecattr_t *, int, struct cred *);
static int spec_setsecattr(struct vnode *, vsecattr_t *, int, struct cred *);

static struct vnodeops spec_vnodeops = {
	spec_open,
	spec_close,
	spec_read,
	spec_write,
	spec_ioctl,
	fs_setfl,
	spec_getattr,
	spec_setattr,
	spec_access,
	fs_nosys,	/* lookup */
	fs_nosys,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fs_nosys,	/* readdir */
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	spec_fsync,
	spec_inactive,
	spec_fid,
	fs_rwlock,
	fs_rwunlock,
	spec_seek,
	fs_cmp,
	spec_frlock,
	fs_nosys,	/* space */
	spec_realvp,
	spec_getpage,
	spec_putpage,
	spec_map,
	spec_addmap,
	spec_delmap,
	spec_poll,
	spec_dump,
	fs_pathconf,
	spec_pageio,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_dispose,
	spec_setsecattr,
	spec_getsecattr,
	fs_shrlock	/* shrlock */
};

/*
 * Return address of spec_vnodeops
 */
struct vnodeops *
spec_getvnodeops(void)
{
	return (&spec_vnodeops);
}

/*
 * Allow handler of special files to initialize and validate before
 * actual I/O.
 */

static major_t clonemaj = (major_t)-1;

extern vnode_t *rconsvp;

static int
spec_open(struct vnode **vpp, int flag, struct cred *cr)
{
	register major_t maj;
	register dev_t dev;
	dev_t newdev;
	struct vnode *nvp;
	struct vnode *vp = *vpp;
	struct vnode *cvp;
	struct snode *csp;	/* common snode ptr */
	struct snode *nsp;	/* new snode ptr */
	struct snode *sp;
	int error = 0;
	struct dev_ops *ops = NULL;
	major_t nmaj;
	struct dev_ops *nops;
	struct stdata *stp;

	flag &= ~FCREAT;		/* paranoia */

	sp = VTOS(vp);

	/*
	 * If the VFS_NOSUID bit was set for the mount, do not allow opens
	 * of special devices.
	 */
	if (sp->s_realvp &&
	    sp->s_realvp->v_vfsp->vfs_flag & VFS_NOSUID)
		return (ENXIO);

	cvp = sp->s_commonvp;
	csp = VTOS(cvp);
	mutex_enter(&csp->s_lock);
	csp->s_count++;			/* one more open reference */
	mutex_exit(&csp->s_lock);

	dev = vp->v_rdev;
	newdev = dev;

	/*
	 * Autoload, install and hold the driver.
	 */
	if (((maj = getmajor(dev)) >= devcnt) ||
	    ((ops = ddi_hold_installed_driver(maj)) == NULL) ||
	    (ops->devo_cb_ops == NULL)) {
		error = ENXIO;
		goto done;
		/*NOTREACHED*/
	}

	if (clonemaj == (major_t)-1)
		clonemaj = ddi_name_to_major("clone");

	switch (vp->v_type) {

	case VCHR:

		if (STREAMSTAB(maj)) {
			struct snode *ocsp = csp;
			int sflag = 0;

			/*
			 * XXX - Acquire the a serial lock on the common
			 * snode to prevent any new clone opens on this stream
			 * while one is in progress.  This is necessary since
			 * the stream currently associated with the clone
			 * device will not be part of it after the clone open
			 * completes. Unfortunately we don't know in advance
			 * if this is a clone device so we have to lock
			 * all opens.
			 *
			 * cv_wait_sig is used to allow signals to pull us
			 * out.
			 */
			mutex_enter(&ocsp->s_lock);
			while (ocsp->s_flag & SLOCKED) {
				ocsp->s_flag |= SWANT;
				if (!cv_wait_sig(&ocsp->s_cv, &ocsp->s_lock)) {
					mutex_exit(&ocsp->s_lock);
					error = EINTR;
					goto done;
				}
			}
			ocsp->s_flag |= SLOCKED;
			mutex_exit(&ocsp->s_lock);

			/*
			 * This flag tells stropen() that the vnode which points
			 * to the commonvp it receives is rconsvp, the console
			 * as opened from /dev/console.
			 */
			if (vp == rconsvp) {
				sflag |= CONSOPEN;
			}

			error = stropen(cvp, &newdev, flag, sflag, cr);

			stp = cvp->v_stream;
			if (error == 0) {
				if (dev != newdev) {
					minor_t lmin, gmin;
					dev_t gdev;
					int dcret;
					int cloned = (maj == clonemaj);
					/*
					 * Clone open. Autoload new driver
					 * if necessary. Possibly an extra
					 * hold may be done, but once we're
					 * committed to the newdev, we'll
					 * release the hold on the old dev.
					 */
					nmaj = getmajor(newdev);
					nops = NULL;
					if ((!cloned) && (nmaj < devcnt))
						nops =
						    ddi_hold_installed_driver(
						    nmaj);
					if ((cloned) || (nops != NULL)) {
						nvp = makespecvp(newdev, VCHR);
					}
					if (((!cloned) && (nops == NULL)) ||
					    (nvp == NULL)) {
						(void) strclose(cvp, flag, cr);
						error = ENXIO;
						if (nops != NULL)  {
							ddi_rele_driver(nmaj);
							error = ENOMEM;
						}
						mutex_enter(&ocsp->s_lock);
						if (ocsp->s_flag & SWANT)
						    cv_broadcast(&ocsp->s_cv);
						ocsp->s_flag &=
						    ~(SWANT|SLOCKED);
						mutex_exit(&ocsp->s_lock);
						break;
					}
					vp->v_stream = NULL;
					cvp->v_stream = NULL;

					/*
					 * XXX - Now, release the
					 * lock since the clone device
					 * doesn't have a stream anymore.
					 */
					mutex_enter(&ocsp->s_lock);
					if (ocsp->s_flag & SWANT)
						cv_broadcast(&ocsp->s_cv);
					ocsp->s_flag &= ~(SWANT|SLOCKED);
					ocsp->s_count--;
					mutex_exit(&ocsp->s_lock);
					/*
					 * The device driver is attempting
					 * to manufacture a new dev_t.
					 * This dev_t needs a corresponding
					 * global minor number.  This is
					 * the place to allocate such a
					 * number.
					 */
					lmin = getminor(newdev);
					dcret = DC_MAP_MINOR(&dcops, nmaj, lmin,
					    &gmin, DEV_CLONE);
					if (dcret != 0) {
						cmn_err(CE_WARN,
					    "dc_get_minor returned error.\n");
						(void) dev_close(newdev, flag,
						    OTYP_CHR, cr);
						error = ENOTUNIQ;
						break;
					}
					gdev = makedevice(nmaj, gmin);

					/*
					 * STREAMS clones inherit fsid and
					 * stream.
					 */
					nsp = VTOS(nvp);
					nsp->s_fsid = sp->s_fsid;
					if (!dcret) {
						nsp->s_flag |= SCLONE;
					}
					nsp->s_gdev = gdev;
					nvp->v_vfsp = vp->v_vfsp;
					nvp->v_stream = stp;
					cvp = nsp->s_commonvp;
					csp = VTOS(cvp);

					/*
					 * Don't let a close come in until
					 * after we have bumped the s_count
					 * on the common snode
					 */
					mutex_enter(&csp->s_lock);
					while (csp->s_flag & SLOCKED) {
						csp->s_flag |= SWANT;
						cv_wait(&csp->s_cv,
						    &csp->s_lock);
					}
					cvp->v_stream = stp;
					stp->sd_vnode = cvp;
					stp->sd_strtab =
					    STREAMSTAB(getmajor(newdev));
					csp->s_count++;
					mutex_exit(&csp->s_lock);

					VN_RELE(vp);
					vp = NULL;
					*vpp = nvp;
					/*
					 * We're committed to nmaj; release
					 * the original major device driver
					 * since we've held the possible new
					 * device driver.
					 */
					ddi_rele_driver(maj);
					nsp->s_size = 0;
					csp->s_size = 0;

				} else {
					/*
					 * Normal open.
					 */
					vp->v_stream = stp;
					mutex_enter(&ocsp->s_lock);
					if (ocsp->s_flag & SWANT)
						cv_broadcast(&ocsp->s_cv);
					ocsp->s_flag &= ~(SWANT|SLOCKED);
					mutex_exit(&ocsp->s_lock);

					/*
					 * We -don't- look up the size
					 * here, since we assert that
					 * STREAMS and STREAMS devices
					 * don't have a size.
					 */
					sp->s_size = 0;
					csp->s_size = 0;
				}
				if (stp->sd_flag & STRISTTY) {
					/*
					 * try to allocate it as a
					 * controlling terminal
					 */
					if (!(flag & FNOCTTY))
						strctty(stp);
				}
				break;

			} else {
				/*
				 * This flag in the stream head cannot
				 * change since the
				 * common snode is locked before the
				 * call to stropen().
				 */
				if ((stp != NULL) &&
				    (stp->sd_flag & STREOPENFAIL)) {
					/*
					 * Open failed part way through.
					 */
					mutex_enter(&stp->sd_lock);
					stp->sd_flag &= ~STREOPENFAIL;
					mutex_exit(&stp->sd_lock);

					mutex_enter(&ocsp->s_lock);
					if (ocsp->s_flag & SWANT)
						cv_broadcast(&ocsp->s_cv);
					ocsp->s_flag &= ~(SWANT|SLOCKED);
					mutex_exit(&ocsp->s_lock);
					(void) spec_close(vp, flag, 1,
					    (offset_t)0, cr);
					return (error);
					/*NOTREACHED*/
				} else {
					mutex_enter(&ocsp->s_lock);
					if (ocsp->s_flag & SWANT)
						cv_broadcast(&ocsp->s_cv);
					ocsp->s_flag &= ~(SWANT|SLOCKED);
					mutex_exit(&ocsp->s_lock);
					break;
				}
			}
		} else {	/* It's not a stream */

			/*
			 * Open the device, and try deferred attach if the
			 * device open returns ENXIO.
			 */
			error = dev_open(&newdev, flag, OTYP_CHR, cr);
			if ((error == ENXIO) &&
			    (e_ddi_deferred_attach(maj, newdev) == 0))
				error = dev_open(&newdev, flag, OTYP_CHR, cr);

			if ((error == 0) && (dev != newdev))  {
				minor_t lmin, gmin;
				dev_t gdev;
				int dcret;

				/*
				 * Clone open. Autoload new driver.
				 * See comments, above in streams case.
				 */
				nmaj = getmajor(newdev);
				nops = NULL;
				if ((nmaj < devcnt) &&
				    (nops = ddi_hold_installed_driver(nmaj)))
					nvp = makespecvp(newdev, VCHR);
				if ((nops == NULL) || (nvp == NULL))  {
					error = ENXIO;
					if (nops != NULL) {
						/*
						 * XXX: This is different
						 * than the streams case!
						 */
						(void) dev_close(newdev, flag,
						    OTYP_CHR, cr);
						ddi_rele_driver(nmaj);
						error = ENOMEM;
					}
					break;
					/* NOTREACHED */
				}
				/*
				 * The device driver is attempting
				 * to manufacture a new dev_t.
				 * This dev_t needs a corresponding
				 * global minor number.  This is
				 * the place to allocate such a
				 * number.
				 */
				lmin = getminor(newdev);
				dcret = DC_MAP_MINOR(&dcops, nmaj, lmin,
							&gmin, DEV_CLONE);
				if (dcret != 0) {
					cmn_err(CE_WARN,
					    "dc_get_minor returned error.\n");
					(void) dev_close(newdev, flag,
					    OTYP_CHR, cr);
					error = ENOTUNIQ;
					break;
				}
				gdev = makedevice(nmaj, gmin);
				/*
				 * Character clones inherit fsid.
				 */
				nsp = VTOS(nvp);
				nsp->s_fsid = sp->s_fsid;

				if (!dcret) {
					nsp->s_flag |= SCLONE;
				}
				nsp->s_gdev = gdev;
				mutex_enter(&csp->s_lock);
				csp->s_count--;
				mutex_exit(&csp->s_lock);
				nvp->v_vfsp = vp->v_vfsp;
				cvp = nsp->s_commonvp;
				csp = VTOS(cvp);
				mutex_enter(&csp->s_lock);
				csp->s_count++;
				mutex_exit(&csp->s_lock);
				VN_RELE(vp);
				vp = NULL;
				*vpp = nvp;
				/*
				 * We're committed to  newdev; release
				 * the hold on the original driver.
				 */
				ddi_rele_driver(maj);
				/*
				 * XXX	So, when do we call dev_open() on the
				 *	newdev?  And what if that open gives us
				 *	dev != newdev?  What if that open gives
				 *	us a stream?  Aargh!
				 */
				{
					u_offset_t size = cdev_Size(newdev);

					if (size == 0)
						size = cdev_size(newdev);
					nsp->s_size = csp->s_size = size;
				}
			}
			if ((error == 0) && (dev == newdev)) {
				/*
				 * The size field is initially UNKNOWN_SIZE.
				 * Now that the device is really open, get the
				 * real size if the driver exports it.
				 */
				{
					u_offset_t size = cdev_Size(dev);

					if (size == 0)
						size = cdev_size(dev);
					sp->s_size = csp->s_size = size;
				}
			}
		}
		break;

	case VBLK:

		/*
		 * Open the device, and try deferred attach if the
		 * device open returns ENXIO.
		 */
		if ((error = dev_open(&newdev, flag, OTYP_BLK, cr)) == ENXIO)
			if (e_ddi_deferred_attach(maj, newdev) == 0)
				error = dev_open(&newdev, flag, OTYP_BLK, cr);

		if ((error == 0) && (dev != newdev))  {
			minor_t lmin, gmin;
			dev_t gdev;
			int dcret;
			/*
			 * Clone open. Autoload new driver.
			 */
			nmaj = getmajor(newdev);
			nops = NULL;
			if (((nmaj = getmajor(newdev)) < devcnt) &&
			    ((nops = ddi_hold_installed_driver(nmaj)) != NULL))
				nvp = makespecvp(newdev, VBLK);
			if ((nops == NULL) || (nvp == NULL))  {
				error = ENXIO;
				if (nops)  {
					(void) dev_close(newdev, flag,
					    OTYP_BLK, cr);
					ddi_rele_driver(nmaj);
					error = ENOMEM;
				}
				break;
				/* NOTREACHED */
			}
			lmin = getminor(newdev);
			dcret = DC_MAP_MINOR(&dcops, nmaj, lmin,
						&gmin, DEV_CLONE);
			if (dcret != 0) {
				cmn_err(CE_WARN,
				    "dc_get_minor returned error.\n");
				(void) dev_close(newdev, flag, OTYP_CHR, cr);
				error = ENOTUNIQ;
				break;
			}
			gdev = makedevice(nmaj, gmin);
			/*
			 * Block clones inherit fsid.
			 */
			nsp = VTOS(nvp);
			nsp->s_fsid = sp->s_fsid;
			if (!dcret) {
				nsp->s_flag |= SCLONE;
			}
			nsp->s_gdev = gdev;

			mutex_enter(&csp->s_lock);
			csp->s_count--;
			mutex_exit(&csp->s_lock);
			nvp->v_vfsp = vp->v_vfsp;
			cvp = nsp->s_commonvp;
			csp = VTOS(cvp);
			mutex_enter(&csp->s_lock);
			csp->s_count++;
			mutex_exit(&csp->s_lock);
			VN_RELE(vp);
			vp = NULL;
			*vpp = nvp;
			/*
			 * We're committed to nmaj; release
			 * the old major device driver.
			 */
			ddi_rele_driver(maj);
		}

		/*
		 * Since we may have created the commonvp snode before the
		 * driver was loaded, we need to resize the block device.
		 * Now that it's really open, the devinfo tree now contains
		 * the driver and the driver can provide the size.
		 */
		if (error == 0) {
			u_offset_t sn_size = csp->s_size;
			if (sn_size == 0 || sn_size == UNKNOWN_SIZE) {
				sn_size = bdev_Size(dev);
				if (sn_size == (u_offset_t)-1)
					sn_size = bdev_size(dev);
				if (sn_size == (u_offset_t)-1 ||
				    sn_size >= lbtodb(MAXOFFSET_T))
					csp->s_size = UNKNOWN_SIZE;
				else
					csp->s_size = ldbtob(sn_size);
			}
		}
		break;

	default:
		cmn_err(CE_PANIC, "spec_open: type not VCHR or VBLK");
		break;
	}
done:
	if (error != 0) {
		mutex_enter(&csp->s_lock);
		csp->s_count--;		/* one less open reference */
		mutex_exit(&csp->s_lock);
		if (ops)
			ddi_rele_driver(maj);
	} else {
		/*
		 * Since we may have created the commonvp snode before the
		 * driver was loaded, we need to set up the s_dip pointer
		 * Given that the device is really open, we know that the
		 * devinfo tree now contains the driver.
		 *
		 * Once we've found the dip corresponding to a particular
		 * common snode, it will remain bound until the driver is
		 * detached from that dip, so we don't have to keep looking
		 * it up on subsequent opens.
		 *
		 * Note that dev_get_dev_info adds an extra hold to the
		 * devops that we need to rele.
		 */
		if (csp->s_dip == NULL) {
			register dev_info_t *dip;

			dip = e_ddi_get_dev_info(newdev, (*vpp)->v_type);
			if (dip != NULL) {
				register struct dev_ops *dv;

				dv = ddi_get_driver(dip);

				mutex_enter(&csp->s_lock);
				if (dv && dv->devo_cb_ops &&
				    dv->devo_cb_ops->cb_flag & D_64BIT)
					csp->s_flag |= SLOFFSET;
				csp->s_dip = dip; /* store order dependency! */
				mutex_exit(&csp->s_lock);

				ddi_rele_driver(getmajor(newdev));
			}
		}
	}
	TRACE_4(TR_FAC_SPECFS, TR_SPECFS_OPEN,
		"specfs open:maj %d vp %p cvp %p error %d", maj,
		*vpp, csp, error);
	return (error);
}

/*ARGSUSED2*/
static int
spec_close(
	struct vnode	*vp,
	int		flag,
	int		count,
	offset_t	offset,
	struct cred	*cr)
{
	register struct snode *sp;
	register struct snode *csp;
	enum vtype type;
	dev_t dev;
	register struct vnode *cvp;
	int error = 0;
	int clone = 0;

	cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	if (vp->v_stream)
		strclean(vp);
	if (count > 1)
		return (0);

	sp = VTOS(vp);
	cvp = sp->s_commonvp;

	dev = sp->s_dev;
	type = vp->v_type;

	ASSERT(type == VCHR || type == VBLK);

	/*
	 * Prevent close/close and close/open races by serializing closes
	 * on this common snode. Clone opens are held up until after
	 * we have closed this device so the streams linkage is maintained
	 */
	csp = VTOS(cvp);
	mutex_enter(&csp->s_lock);
	while (csp->s_flag & SLOCKED) {
		csp->s_flag |= SWANT;
		cv_wait(&csp->s_cv, &csp->s_lock);
	}
	csp->s_flag |= SLOCKED;
	csp->s_count--;			/* one fewer open reference */
	clone = sp->s_flag & SCLONE;
	mutex_exit(&csp->s_lock);

	/*
	 * Only call the close routine when the last open reference through
	 * any [s, v]node goes away.
	 */
	if (!stillreferenced(dev, type)) {
		error = device_close(vp, flag, cr);
		if (clone) {
			/*
			 * The driver had manufactured a new dev_t.
			 * The corresponding global dev_t allocated
			 * needs to be freed.
			 */
			(void) (DC_UNMAP_MINOR(&dcops, getmajor(sp->s_gdev),
			    getminor(sp->s_dev), getminor(sp->s_gdev),
			    DEV_CLONE));
			sp->s_flag &= ~SCLONE;
		}
	}

	mutex_enter(&csp->s_lock);
	if (csp->s_flag & SWANT)
		cv_broadcast(&csp->s_cv);
	csp->s_flag &= ~(SWANT|SLOCKED);
	mutex_exit(&csp->s_lock);

	/*
	 * Decrement the device drivers reference count for every close.
	 */
	ddi_rele_driver(getmajor(dev));
	return (error);
}

/*ARGSUSED2*/
static int
spec_read(
	register struct vnode	*vp,
	register struct uio	*uiop,
	int			ioflag,
	struct cred		*cr)
{
	int error;
	struct snode *sp = VTOS(vp);
	dev_t dev = sp->s_dev;
	size_t n;
	ulong_t on;
	u_offset_t bdevsize;
	offset_t maxoff;
	offset_t off;
	struct vnode *blkvp;

	ASSERT(vp->v_type == VCHR || vp->v_type == VBLK);

	if (STREAMSTAB(getmajor(dev))) {	/* stream */
		ASSERT(vp->v_type == VCHR);
		smark(sp, SACC);
		return (strread(vp, uiop, cr));
	}

	if (uiop->uio_resid == 0)
		return (0);

	maxoff = spec_maxoffset(vp);
	ASSERT(maxoff != -1LL);	/* only streams return -1 */
	if (uiop->uio_loffset < 0 ||
	    uiop->uio_loffset + uiop->uio_resid > maxoff)
		return (EINVAL);

	if (vp->v_type == VCHR) {
		smark(sp, SACC);
		ASSERT(STREAMSTAB(getmajor(dev)) == 0);
		return (cdev_read(dev, uiop, cr));
	}

	/*
	 * Block device.
	 */
	error = 0;
	blkvp = sp->s_commonvp;
	bdevsize = VTOS(blkvp)->s_size;

	do {
		caddr_t base;
		offset_t diff;

		off = uiop->uio_loffset & (offset_t)MAXBMASK;
		on = (size_t)(uiop->uio_loffset & MAXBOFFSET);
		n = (size_t)MIN(MAXBSIZE - on, uiop->uio_resid);
		diff = bdevsize - uiop->uio_loffset;

		if (diff <= 0)
			break;
		if (diff < n)
			n = (size_t)diff;

		base = segmap_getmapflt(segkmap, blkvp,
			(u_offset_t)(off + on), n, 1, S_READ);

		if ((error = uiomove(base + on, n, UIO_READ, uiop)) == 0) {
			int flags = 0;
			/*
			 * If we read a whole block, we won't need this
			 * buffer again soon.
			 */
			if (n + on == MAXBSIZE)
				flags = SM_DONTNEED | SM_FREE;
			error = segmap_release(segkmap, base, flags);
		} else {
			(void) segmap_release(segkmap, base, 0);
			if (bdevsize == UNKNOWN_SIZE) {
				error = 0;
				break;
			}
		}
	} while (error == 0 && uiop->uio_resid > 0 && n != 0);

	return (error);
}

static int
spec_write(
	struct vnode *vp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr)
{
	int error;
	struct snode *sp = VTOS(vp);
	dev_t dev = sp->s_dev;
	size_t n;
	ulong_t on;
	u_offset_t bdevsize;
	offset_t maxoff;
	offset_t off;
	struct vnode *blkvp;

	ASSERT(vp->v_type == VCHR || vp->v_type == VBLK);

	if (STREAMSTAB(getmajor(dev))) {
		ASSERT(vp->v_type == VCHR);
		smark(sp, SUPD);
		return (strwrite(vp, uiop, cr));
	}

	maxoff = spec_maxoffset(vp);
	ASSERT(maxoff != -1LL);	/* only streams return -1 */
	if (uiop->uio_loffset < 0 ||
	    uiop->uio_loffset + uiop->uio_resid > maxoff)
		return (EINVAL);

	if (vp->v_type == VCHR) {
		smark(sp, SUPD);
		ASSERT(STREAMSTAB(getmajor(dev)) == 0);
		return (cdev_write(dev, uiop, cr));
	}

	if (uiop->uio_resid == 0)
		return (0);

	error = 0;
	blkvp = sp->s_commonvp;
	bdevsize = VTOS(blkvp)->s_size;

	do {
		int pagecreate;
		int newpage;
		caddr_t base;
		offset_t diff;

		off = uiop->uio_loffset & (offset_t)MAXBMASK;
		on = (ulong_t)(uiop->uio_loffset & MAXBOFFSET);
		n = (size_t)MIN(MAXBSIZE - on, uiop->uio_resid);
		pagecreate = 0;

		diff = bdevsize - uiop->uio_loffset;
		if (diff <= 0) {
			error = ENXIO;
			break;
		}
		if (diff < n)
			n = (size_t)diff;

		/*
		 * Check to see if we can skip reading in the page
		 * and just allocate the memory.  We can do this
		 * if we are going to rewrite the entire mapping
		 * or if we are going to write to end of the device
		 * from the beginning of the mapping.
		 */
		if (n == MAXBSIZE || (on == 0 && (off + n) == bdevsize))
			pagecreate = 1;

		base = segmap_getmapflt(segkmap, blkvp,
		    (u_offset_t)(off + on), n, !pagecreate, S_WRITE);

		/*
		 * segmap_pagecreate() returns 1 if it calls
		 * page_create_va() to allocate any pages.
		 */
		newpage = 0;

		if (pagecreate)
			newpage = segmap_pagecreate(segkmap, base + on,
				n, 0);

		error = uiomove(base + on, n, UIO_WRITE, uiop);

		if (pagecreate &&
		    uiop->uio_loffset < roundup(off + on + n, PAGESIZE)) {
			/*
			 * We created pages w/o initializing them completely,
			 * thus we need to zero the part that wasn't set up.
			 * This can happen if we write to the end of the device
			 * or if we had some sort of error during the uiomove.
			 */
			long nzero;
			offset_t nmoved;

			nmoved = (uiop->uio_loffset - (off + on));
			if (nmoved < 0 || nmoved > n)
				cmn_err(CE_PANIC, "spec_write: nmoved bogus");
			nzero = (roundup(on + n, PAGESIZE) -
					(on + nmoved));
			if (nzero < 0 || (on + nmoved + nzero > MAXBSIZE))
				cmn_err(CE_PANIC, "spec_write: nzero bogus");
			(void) kzero(base + on + nmoved, (size_t)nzero);
		}

		/*
		 * Unlock the pages which have been allocated by
		 * page_create_va() in segmap_pagecreate().
		 */
		if (newpage)
			segmap_pageunlock(segkmap, base + on,
				(size_t)n, S_WRITE);

		if (error == 0) {
			int flags = 0;

			/*
			 * Force write back for synchronous write cases.
			 */
			if (ioflag & (FSYNC|FDSYNC))
				flags = SM_WRITE;
			else if (n + on == MAXBSIZE || IS_SWAPVP(vp)) {
				/*
				 * Have written a whole block.
				 * Start an asynchronous write and
				 * mark the buffer to indicate that
				 * it won't be needed again soon.
				 * Push swap files here, since it
				 * won't happen anywhere else.
				 */
				flags = SM_WRITE | SM_ASYNC | SM_DONTNEED;
			}
			smark(sp, SUPD|SCHG);
			error = segmap_release(segkmap, base, flags);
		} else
			(void) segmap_release(segkmap, base, SM_INVAL);

	} while (error == 0 && uiop->uio_resid > 0 && n != 0);

	return (error);
}

static int
spec_ioctl(struct vnode *vp, int cmd, intptr_t arg, int mode, struct cred *cr,
    int *rvalp)
{
	struct snode *sp;
	dev_t dev;
	int error;

	if (vp->v_type != VCHR)
		return (ENOTTY);
	sp = VTOS(vp);
	dev = sp->s_dev;
	if (STREAMSTAB(getmajor(dev))) {
		error = strioctl(vp, cmd, arg, mode, U_TO_K, cr, rvalp);
	} else {
		error = cdev_ioctl(dev, cmd, arg, mode, cr, rvalp);
	}
	return (error);
}

static int
spec_getattr(struct vnode *vp, struct vattr *vap, int flags, struct cred *cr)
{
	int error;
	dev_t	fsid;
	u_offset_t size;
	struct snode *sp;
	struct vnode *realvp;

	if (flags & ATTR_COMM && vp->v_type == VBLK) {
		sp = VTOS(vp);
		vp = sp->s_commonvp;
	}

	sp = VTOS(vp);
	realvp = sp->s_realvp;
	size = VTOS(sp->s_commonvp)->s_size;
	fsid = sp->s_fsid;

	if (realvp == NULL) {
		static int snode_shift	= 0;

		/*
		 * Calculate the amount of bitshift to a snode pointer which
		 * will still keep it unique.  See below.
		 */
		if (snode_shift == 0)
			snode_shift = highbit(sizeof (struct snode));
		ASSERT(snode_shift > 0);

		/*
		 * No real vnode behind this one.  Fill in the fields
		 * from the snode.
		 *
		 * This code should be refined to return only the
		 * attributes asked for instead of all of them.
		 */
		vap->va_type = vp->v_type;
		vap->va_mode = 0;
		vap->va_uid = vap->va_gid = 0;
		vap->va_fsid = fsid;
		/*
		 * If the va_nodeid is > MAX_USHORT, then i386 stats might
		 * fail. So we shift down the sonode pointer to try and get
		 * the most uniqueness into 16-bits.
		 */
		vap->va_nodeid = ((ino64_t)sp >> snode_shift) & 0xFFFF;
		vap->va_nlink = 0;
		vap->va_size = size;
		vap->va_rdev = sp->s_dev;
		vap->va_blksize = MAXBSIZE;
		vap->va_nblocks = (fsblkcnt64_t)btod(vap->va_size);
	} else {
		error = VOP_GETATTR(realvp, vap, flags, cr);
		if (error != 0)
			return (error);
		/*
		 * set the size to that of the device from the snode.
		 */
		vap->va_size = size;
	}

	mutex_enter(&sp->s_lock);
	vap->va_atime.tv_sec = sp->s_atime;
	vap->va_mtime.tv_sec = sp->s_mtime;
	vap->va_ctime.tv_sec = sp->s_ctime;
	mutex_exit(&sp->s_lock);

	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_nsec = 0;
	vap->va_vcode = 0;

	return (0);
}

static int
spec_setattr(
	struct vnode *vp,
	struct vattr *vap,
	int flags,
	struct cred *cr)
{
	struct snode *sp = VTOS(vp);
	struct vnode *realvp;
	int error;

	if (vp->v_type == VCHR && vp->v_stream && (vap->va_mask & AT_SIZE)) {
		/*
		 * 1135080:	O_TRUNC should have no effect on
		 *		named pipes and terminal devices.
		 */
		ASSERT(vap->va_mask == AT_SIZE);
		return (0);
	}

	if ((realvp = sp->s_realvp) == NULL)
		error = 0;	/* no real vnode to update */
	else
		error = VOP_SETATTR(realvp, vap, flags, cr);
	if (error == 0) {
		/*
		 * If times were changed, update snode.
		 */
		mutex_enter(&sp->s_lock);
		if (vap->va_mask & AT_ATIME)
			sp->s_atime = vap->va_atime.tv_sec;
		if (vap->va_mask & AT_MTIME) {
			sp->s_mtime = vap->va_mtime.tv_sec;
			sp->s_ctime = hrestime.tv_sec;
		}
		mutex_exit(&sp->s_lock);
	}
	return (error);
}

static int
spec_access(struct vnode *vp, int mode, int flags, struct cred *cr)
{
	register struct vnode *realvp;
	register struct snode *sp = VTOS(vp);

	if ((realvp = sp->s_realvp) != NULL)
		return (VOP_ACCESS(realvp, mode, flags, cr));
	else
		return (0);	/* Allow all access. */
}

/*
 * In order to sync out the snode times without multi-client problems,
 * make sure the times written out are never earlier than the times
 * already set in the vnode.
 */
static int
spec_fsync(struct vnode *vp, int syncflag, struct cred *cr)
{
	register struct snode *sp = VTOS(vp);
	register struct vnode *realvp;
	register struct vnode *cvp;
	struct vattr va, vatmp;

	/* If times didn't change, don't flush anything. */
	mutex_enter(&sp->s_lock);
	if ((sp->s_flag & (SACC|SUPD|SCHG)) == 0 && vp->v_type != VBLK) {
		mutex_exit(&sp->s_lock);
		return (0);
	}
	sp->s_flag &= ~(SACC|SUPD|SCHG);
	mutex_exit(&sp->s_lock);
	cvp = sp->s_commonvp;
	realvp = sp->s_realvp;

	if (vp->v_type == VBLK && cvp != vp && cvp->v_pages != NULL &&
	    (cvp->v_flag & VISSWAP) == 0)
		(void) VOP_PUTPAGE(cvp, (offset_t)0, 0, 0, cr);

	/*
	 * If no real vnode to update, don't flush anything.
	 */
	if (realvp == NULL)
		return (0);

	vatmp.va_mask = AT_ATIME|AT_MTIME;
	if (VOP_GETATTR(realvp, &vatmp, 0, cr) == 0) {

		mutex_enter(&sp->s_lock);
		if (vatmp.va_atime.tv_sec > sp->s_atime)
			va.va_atime = vatmp.va_atime;
		else {
			va.va_atime.tv_sec = sp->s_atime;
			va.va_atime.tv_nsec = 0;
		}
		if (vatmp.va_mtime.tv_sec > sp->s_mtime)
			va.va_mtime = vatmp.va_mtime;
		else {
			va.va_mtime.tv_sec = sp->s_mtime;
			va.va_mtime.tv_nsec = 0;
		}
		mutex_exit(&sp->s_lock);

		va.va_mask = AT_ATIME|AT_MTIME;
		(void) VOP_SETATTR(realvp, &va, 0, cr);
	}
	(void) VOP_FSYNC(realvp, syncflag, cr);
	return (0);
}

static void
spec_inactive(struct vnode *vp, struct cred *cr)
{
	register struct snode *sp = VTOS(vp);
	register struct vnode *cvp;
	register struct vnode *rvp;

	/*
	 * If no one has reclaimed the vnode, remove from the
	 * cache now.
	 */
	if (vp->v_count < 1)
		cmn_err(CE_PANIC, "spec_inactive: Bad v_count");
	mutex_enter(&stable_lock);

	mutex_enter(&vp->v_lock);
	/*
	 * Drop the temporary hold by vn_rele now
	 */
	if (--vp->v_count != 0) {
		mutex_exit(&vp->v_lock);
		mutex_exit(&stable_lock);
		return;
	}
	mutex_exit(&vp->v_lock);

	sdelete(sp);
	mutex_exit(&stable_lock);

	/* We are the sole owner of sp now */
	cvp = sp->s_commonvp;
	rvp = sp->s_realvp;

	if (rvp) {
		/*
		 * If the snode times changed, then update the times
		 * associated with the "realvp".
		 */
		if ((sp->s_flag & (SACC|SUPD|SCHG)) != 0) {

			struct vattr va, vatmp;

			mutex_enter(&sp->s_lock);
			sp->s_flag &= ~(SACC|SUPD|SCHG);
			mutex_exit(&sp->s_lock);
			vatmp.va_mask = AT_ATIME|AT_MTIME;
			if (VOP_GETATTR(rvp, &vatmp, 0, cr) == 0) {
				if (vatmp.va_atime.tv_sec > sp->s_atime)
					va.va_atime = vatmp.va_atime;
				else {
					va.va_atime.tv_sec = sp->s_atime;
					va.va_atime.tv_nsec = 0;
				}
				if (vatmp.va_mtime.tv_sec > sp->s_mtime)
					va.va_mtime = vatmp.va_mtime;
				else {
					va.va_mtime.tv_sec = sp->s_mtime;
					va.va_mtime.tv_nsec = 0;
				}

				va.va_mask = AT_ATIME|AT_MTIME;
				(void) VOP_SETATTR(rvp, &va, 0, cr);
			}
		}
	}
	ASSERT(vp->v_pages == NULL);

	if (rvp)
		VN_RELE(rvp);

	if (cvp && VN_CMP(cvp, vp) == 0)
		VN_RELE(cvp);

	kmem_cache_free(snode_cache, sp);
}

static int
spec_fid(struct vnode *vp, struct fid *fidp)
{
	register struct vnode *realvp;
	register struct snode *sp = VTOS(vp);

	if ((realvp = sp->s_realvp) != NULL)
		return (VOP_FID(realvp, fidp));
	else
		return (EINVAL);
}

/*ARGSUSED1*/
static int
spec_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	offset_t maxoff = spec_maxoffset(vp);

	if (maxoff == -1 || *noffp <= maxoff)
		return (0);
	else
		return (EINVAL);
}

static int
spec_frlock(
	register struct vnode *vp,
	int		cmd,
	struct flock64	*bfp,
	int		flag,
	offset_t	offset,
	struct cred	*cr)
{
	register struct snode *sp = VTOS(vp);
	register struct snode *csp;

	csp = VTOS(sp->s_commonvp);
	/*
	 * If file is being mapped, disallow frlock.
	 */
	if (csp->s_mapcnt > 0)
		return (EAGAIN);

	return (fs_frlock(vp, cmd, bfp, flag, offset, cr));
}

static int
spec_realvp(register struct vnode *vp, register struct vnode **vpp)
{
	struct vnode *rvp;

	if ((rvp = VTOS(vp)->s_realvp) != NULL) {
		vp = rvp;
		if (VOP_REALVP(vp, &rvp) == 0)
			vp = rvp;
	}

	*vpp = vp;
	return (0);
}

/*
 * Return all the pages from [off..off + len] in block
 * or character device.
 */
static int
spec_getpage(
	struct vnode	*vp,
	offset_t	off,
	size_t		len,
	uint_t		*protp,
	page_t		*pl[],
	size_t		plsz,
	struct seg	*seg,
	caddr_t		addr,
	enum seg_rw	rw,
	struct cred	*cr)
{
	register struct snode *sp = VTOS(vp);
	int err;

	ASSERT(sp->s_commonvp == vp);

	/*
	 * XXX	Given the above assertion, this might not do
	 *	what is wanted here.
	 */
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);
	TRACE_4(TR_FAC_SPECFS, TR_SPECFS_GETPAGE,
		"specfs getpage:vp %p off %llx len %ld snode %p",
		vp, off, len, sp);

	switch (vp->v_type) {
	case VBLK:
		if (protp != NULL)
			*protp = PROT_ALL;

		if ((u_offset_t)off + len > sp->s_size + PAGEOFFSET)
			return (EFAULT);	/* beyond EOF */

		if (len <= PAGESIZE)
			err = spec_getapage(vp, (u_offset_t)off, len, protp, pl,
			    plsz, seg, addr, rw, cr);
		else
			err = pvn_getpages(spec_getapage, vp, (u_offset_t)off,
			    len, protp, pl, plsz, seg, addr, rw, cr);
		break;

	case VCHR:
		cmn_err(CE_NOTE, "spec_getpage called for character device. "
		    "Check any non-ON consolidation drivers");
		err = 0;
		pl[0] = (page_t *)0;
		break;

	default:
		cmn_err(CE_PANIC, "spec_getpage: bad v_type 0x%x", vp->v_type);
		/*NOTREACHED*/
	}

	return (err);
}

/*
 * klustsize should be a multiple of PAGESIZE and <= MAXPHYS.
 */

#define	KLUSTSIZE	(56 * 1024)

#ifdef	sun
#undef	KLUSTSIZE
extern int klustsize;	/* set in machdep.c */
#else	/* sun */
int klustsize = KLUSTSIZE;
#endif	/* sun */

int spec_ra = 1;
int spec_lostpage;	/* number of times we lost original page */

/*ARGSUSED2*/
static int
spec_getapage(
	register struct vnode *vp,
	u_offset_t	off,
	size_t		len,
	uint_t		*protp,
	page_t		*pl[],
	size_t		plsz,
	struct seg	*seg,
	caddr_t		addr,
	enum seg_rw	rw,
	struct cred	*cr)
{
	register struct snode *sp;
	struct buf *bp;
	page_t *pp, *pp2;
	u_offset_t io_off1, io_off2;
	size_t io_len1;
	size_t io_len2;
	size_t blksz;
	u_offset_t blkoff;
	int dora, err;
	page_t *pagefound;
	uint_t xlen;
	size_t adj_klustsize;
	u_offset_t size;
	u_offset_t tmpoff;

	sp = VTOS(vp);
	TRACE_3(TR_FAC_SPECFS, TR_SPECFS_GETAPAGE,
		"specfs getapage:vp %p off %llx snode %p", vp, off, sp);
reread:

	err = 0;
	bp = NULL;
	pp = NULL;
	pp2 = NULL;

	if (pl != NULL)
		pl[0] = NULL;

	size = VTOS(sp->s_commonvp)->s_size;

	if (spec_ra && sp->s_nextr == off)
		dora = 1;
	else
		dora = 0;

	if (size == UNKNOWN_SIZE) {
		dora = 0;
		adj_klustsize = PAGESIZE;
	} else {
		adj_klustsize = dora ? klustsize : PAGESIZE;
	}

again:
	if ((pagefound = page_exists(vp, off)) == NULL) {
		if (rw == S_CREATE) {
			/*
			 * We're allocating a swap slot and it's
			 * associated page was not found, so allocate
			 * and return it.
			 */
			if ((pp = page_create_va(vp, off,
			    PAGESIZE, PG_WAIT, seg, addr)) == NULL)
				cmn_err(CE_PANIC, "spec_getapage: page_create");
			io_len1 = PAGESIZE;
			sp->s_nextr = off + PAGESIZE;
		} else {
			/*
			 * Need to really do disk I/O to get the page(s).
			 */
			blkoff = (off / adj_klustsize) * adj_klustsize;
			if (size == UNKNOWN_SIZE) {
				blksz = PAGESIZE;
			} else {
				if (blkoff + adj_klustsize <= size)
					blksz = adj_klustsize;
				else
					blksz =
					    MIN(size - blkoff, adj_klustsize);
			}

			pp = pvn_read_kluster(vp, off, seg, addr, &tmpoff,
			    &io_len1, blkoff, blksz, 0);
			io_off1 = tmpoff;
			/*
			 * Make sure the page didn't sneek into the
			 * cache while we blocked in pvn_read_kluster.
			 */
			if (pp == NULL)
				goto again;

			/*
			 * Zero part of page which we are not
			 * going to be reading from disk now.
			 */
			xlen = (uint_t)(io_len1 & PAGEOFFSET);
			if (xlen != 0)
				pagezero(pp->p_prev, xlen, PAGESIZE - xlen);

			bp = spec_startio(vp, pp, io_off1, io_len1,
			    pl == NULL ? (B_ASYNC | B_READ) : B_READ);
			sp->s_nextr = io_off1 + io_len1;
		}
	}

	if (dora && rw != S_CREATE) {
		u_offset_t off2;
		caddr_t addr2;

		off2 = ((off / adj_klustsize) + 1) * adj_klustsize;
		addr2 = addr + (off2 - off);

		pp2 = NULL;
		/*
		 * If we are past EOF then don't bother trying
		 * with read-ahead.
		 */
		if (off2 >= size)
			pp2 = NULL;
		else {
			if (off2 + adj_klustsize <= size)
				blksz = adj_klustsize;
			else
				blksz = MIN(size - off2, adj_klustsize);

			pp2 = pvn_read_kluster(vp, off2, seg, addr2, &tmpoff,
			    &io_len2, off2, blksz, 1);
			io_off2 = tmpoff;
		}

		if (pp2 != NULL) {
			/*
			 * Zero part of page which we are not
			 * going to be reading from disk now.
			 */
			xlen = (uint_t)(io_len2 & PAGEOFFSET);
			if (xlen != 0)
				pagezero(pp2->p_prev, xlen, PAGESIZE - xlen);

			(void) spec_startio(vp, pp2, io_off2, io_len2,
			    B_READ | B_ASYNC);
		}
	}

	if (pl == NULL)
		return (err);

	if (bp != NULL) {
		err = biowait(bp);
		pageio_done(bp);

		if (err) {
			if (pp != NULL)
				pvn_read_done(pp, B_ERROR);
			return (err);
		}
	}

	if (pagefound) {
		se_t se = (rw == S_CREATE ? SE_EXCL : SE_SHARED);
		/*
		 * Page exists in the cache, acquire the appropriate
		 * lock.  If this fails, start all over again.
		 */

		if ((pp = page_lookup(vp, off, se)) == NULL) {
			spec_lostpage++;
			goto reread;
		}
		pl[0] = pp;
		pl[1] = NULL;

		sp->s_nextr = off + PAGESIZE;
		return (0);
	}

	if (pp != NULL)
		pvn_plist_init(pp, pl, plsz, off, io_len1, rw);
	return (0);
}

/*
 * Flags are composed of {B_INVAL, B_DIRTY B_FREE, B_DONTNEED, B_FORCE}.
 * If len == 0, do from off to EOF.
 *
 * The normal cases should be len == 0 & off == 0 (entire vp list),
 * len == MAXBSIZE (from segmap_release actions), and len == PAGESIZE
 * (from pageout).
 */
int
spec_putpage(
	register struct vnode *vp,
	offset_t	off,
	size_t		len,
	int		flags,
	struct cred	*cr)
{
	register struct snode *sp = VTOS(vp);
	register struct vnode *cvp;
	register page_t *pp;
	u_offset_t io_off;
	size_t io_len = 0;	/* for lint */
	int err = 0;
	u_offset_t size;
	u_offset_t tmpoff;

	ASSERT(vp->v_count != 0);

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	cvp = sp->s_commonvp;
	size = VTOS(cvp)->s_size;

	if (vp->v_pages == NULL || off >= size)
		return (0);

	ASSERT(vp->v_type == VBLK && cvp == vp);
	TRACE_4(TR_FAC_SPECFS, TR_SPECFS_PUTPAGE,
		"specfs putpage:vp %p off %llx len %ld snode %p",
		vp, off, len, sp);

	if (len == 0) {
		/*
		 * Search the entire vp list for pages >= off.
		 */
		err = pvn_vplist_dirty(vp, off, spec_putapage,
		    flags, cr);
	} else {
		u_offset_t eoff;

		/*
		 * Loop over all offsets in the range [off...off + len]
		 * looking for pages to deal with.  We set limits so
		 * that we kluster to klustsize boundaries.
		 */
		eoff = off + len;
		for (io_off = off; io_off < eoff && io_off < size;
		    io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || pvn_getdirty(pp, flags) == 0)
				io_len = PAGESIZE;
			else {
				err = spec_putapage(vp, pp, &tmpoff, &io_len,
				    flags, cr);
				io_off = tmpoff;
				if (err != 0)
					break;
				/*
				 * "io_off" and "io_len" are returned as
				 * the range of pages we actually wrote.
				 * This allows us to skip ahead more quickly
				 * since several pages may've been dealt
				 * with by this iteration of the loop.
				 */
			}
		}
	}
	return (err);
}


/*
 * Write out a single page, possibly klustering adjacent
 * dirty pages.
 */
/*ARGSUSED5*/
static int
spec_putapage(
	struct vnode	*vp,
	page_t		*pp,
	u_offset_t	*offp,		/* return value */
	size_t		*lenp,		/* return value */
	int		flags,
	struct cred	*cr)
{
	struct snode *sp = VTOS(vp);
	u_offset_t io_off;
	size_t io_len;
	size_t blksz;
	u_offset_t blkoff;
	int err = 0;
	struct buf *bp;
	u_offset_t size;
	size_t adj_klustsize;
	u_offset_t tmpoff;

	/*
	 * Destroy read ahead value since we are really going to write.
	 */
	sp->s_nextr = 0;
	size = VTOS(sp->s_commonvp)->s_size;

	adj_klustsize = klustsize;

	blkoff = (pp->p_offset / adj_klustsize) * adj_klustsize;

	if (blkoff + adj_klustsize <= size)
		blksz = adj_klustsize;
	else
		blksz = size - blkoff;

	/*
	 * Find a kluster that fits in one contiguous chunk.
	 */
	pp = pvn_write_kluster(vp, pp, &tmpoff, &io_len, blkoff,
		blksz, flags);
	io_off = tmpoff;

	/*
	 * Check for page length rounding problems
	 * XXX - Is this necessary?
	 */
	if (io_off + io_len > size) {
		ASSERT((io_off + io_len) - size < PAGESIZE);
		io_len = size - io_off;
	}

	bp = spec_startio(vp, pp, io_off, io_len, B_WRITE | flags);

	/*
	 * Wait for i/o to complete if the request is not B_ASYNC.
	 */
	if ((flags & B_ASYNC) == 0) {
		err = biowait(bp);
		pageio_done(bp);
		pvn_write_done(pp, ((err) ? B_ERROR : 0) | B_WRITE | flags);
	}

	if (offp)
		*offp = io_off;
	if (lenp)
		*lenp = io_len;
	TRACE_4(TR_FAC_SPECFS, TR_SPECFS_PUTAPAGE,
		"specfs putapage:vp %p off %llx snode %p err %d",
		vp, *offp, sp, err);
	return (err);
}

/*
 * Flags are composed of {B_ASYNC, B_INVAL, B_FREE, B_DONTNEED}
 */
static struct buf *
spec_startio(
	register struct vnode *vp,
	page_t		*pp,
	u_offset_t	io_off,
	size_t		io_len,
	int		flags)
{
	register struct buf *bp;
	klwp_t *lwp = ttolwp(curthread);

	bp = pageio_setup(pp, io_len, vp, flags);

	bp->b_edev = vp->v_rdev;
	bp->b_dev = cmpdev(vp->v_rdev);
	bp->b_blkno = btodt(io_off);
	bp->b_un.b_addr = (caddr_t)0;

	(void) bdev_strategy(bp);

	if (lwp != NULL) {
		if (flags & B_READ)
			lwp->lwp_ru.inblock++;
		else
			lwp->lwp_ru.oublock++;
	}

	return (bp);
}

static int
spec_poll(
	struct vnode	*vp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead **phpp)
{
	register dev_t dev;
	register int error;

	if (vp->v_type == VBLK)
		error = fs_poll(vp, events, anyyet, reventsp, phpp);
	else {
		ASSERT(vp->v_type == VCHR);
		dev = vp->v_rdev;
		if (STREAMSTAB(getmajor(dev))) {
			ASSERT(vp->v_stream != NULL);
			error = strpoll(vp->v_stream, events, anyyet,
			    reventsp, phpp);
		} else if (devopsp[getmajor(dev)]->devo_cb_ops->cb_chpoll) {
			error = cdev_poll(dev, events, anyyet, reventsp, phpp);
		} else {
			error = fs_poll(vp, events, anyyet, reventsp, phpp);
		}
	}
	return (error);
}

/*
 * This routine is called through the cdevsw[] table to handle
 * traditional mmap'able devices that support a d_mmap function.
 */
/*ARGSUSED8*/
int
spec_segmap(
	dev_t dev,
	off_t off,
	struct as *as,
	caddr_t *addrp,
	off_t len,
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	struct cred *cred)
{
	struct segdev_crargs dev_a;
	int (*mapfunc)(dev_t dev, off_t off, int prot);
	size_t i;
	int	error;

	if ((mapfunc = devopsp[getmajor(dev)]->devo_cb_ops->cb_mmap) == nodev)
		return (ENODEV);
	TRACE_4(TR_FAC_SPECFS, TR_SPECFS_SEGMAP,
		"specfs segmap:dev %x as %p len %lx prot %x",
		dev, as, len, prot);

	/*
	 * Character devices that support the d_mmap
	 * interface can only be mmap'ed shared.
	 */
	if ((flags & MAP_TYPE) != MAP_SHARED)
		return (EINVAL);

	/*
	 * Check to ensure that the entire range is
	 * legal and we are not trying to map in
	 * more than the device will let us.
	 */
	for (i = 0; i < len; i += PAGESIZE) {
		if (cdev_mmap(mapfunc, dev, off + i, maxprot) == -1)
			return (ENXIO);
	}

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		/*
		 * Pick an address w/o worrying about
		 * any vac alignment contraints.
		 */
		map_addr(addrp, len, (offset_t)off, 0, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User-specified address; blow away any previous mappings.
		 */
		(void) as_unmap(as, *addrp, len);
	}

	dev_a.mapfunc = mapfunc;
	dev_a.dev = dev;
	dev_a.offset = off;
	dev_a.prot = (uchar_t)prot;
	dev_a.maxprot = (uchar_t)maxprot;
	dev_a.hat_flags = 0;
	dev_a.hat_attr = 0;
	dev_a.devmap_data = NULL;

	error = as_map(as, *addrp, len, segdev_create, &dev_a);
	as_rangeunlock(as);
	return (error);
}

int
spec_char_map(
	dev_t dev,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cred)
{
	int error = 0;
	major_t maj = getmajor(dev);
	int map_flag;
	int (*segmap)(dev_t, off_t, struct as *,
	    caddr_t *, off_t, uint_t, uint_t, uint_t, cred_t *);
	int (*devmap)(dev_t, devmap_cookie_t, offset_t,
		size_t, size_t *, uint_t);
	int (*mmap)(dev_t dev, off_t off, int prot);

	/*
	 * Character device: let the device driver
	 * pick the appropriate segment driver.
	 *
	 * 4.x compat.: allow 'NULL' cb_segmap => spec_segmap
	 * Kindness: allow 'nulldev' cb_segmap => spec_segmap
	 */
	segmap = devopsp[maj]->devo_cb_ops->cb_segmap;
	if (segmap == NULL || segmap == nulldev || segmap == nodev) {
		mmap = devopsp[maj]->devo_cb_ops->cb_mmap;
		map_flag = devopsp[maj]->devo_cb_ops->cb_flag;

		/*
		 * Use old mmap framework if the driver has both mmap
		 * and devmap entry points.  This is to prevent the
		 * system from calling invalid devmap entry point
		 * for some drivers that might have put garbage in the
		 * devmap entry point.
		 */
		if ((map_flag & D_DEVMAP) || mmap == NULL ||
		    mmap == nulldev || mmap == nodev) {
			devmap = devopsp[maj]->devo_cb_ops->cb_devmap;

			/*
			 * If driver provides devmap entry point in
			 * cb_ops but not xx_segmap(9E), call
			 * devmap_setup with default settings
			 * (NULL) for callback_ops and driver
			 * callback private data
			 */
			if (devmap == nodev || devmap == NULL ||
			    devmap == nulldev)
				return (ENODEV);

			error = devmap_setup(dev, off, as, addrp,
			    len, prot, maxprot, flags, cred);

			return (error);
		} else
			segmap = spec_segmap;
	} else
		segmap = cdev_segmap;

	return ((*segmap)(dev, (off_t)off, as, addrp, len, prot,
	    maxprot, flags, cred));
}

static int
spec_map(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cred)
{
	int error = 0;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off > SPEC_MAXOFFSET_T)
		return (EINVAL);

	/*
	 * If file is locked, fail mapping attempt.
	 */
	if (vp->v_filocks != NULL)
		return (EAGAIN);

	if (vp->v_type == VCHR) {
		return (spec_char_map(vp->v_rdev, off, as, addrp, len, prot,
		    maxprot, flags, cred));
	} else if (vp->v_type == VBLK) {
		struct segvn_crargs vn_a;
		struct vnode *cvp;
		struct snode *sp;

		/*
		 * Block device, use the underlying commonvp name for pages.
		 */
		sp = VTOS(vp);
		cvp = sp->s_commonvp;
		ASSERT(cvp != NULL);

		if (off < 0 || (off + len) < 0)
			return (EINVAL);

		as_rangelock(as);
		if ((flags & MAP_FIXED) == 0) {
			map_addr(addrp, len, off, 1, flags);
			if (*addrp == NULL) {
				as_rangeunlock(as);
				return (ENOMEM);
			}
		} else {
			/*
			 * User-specified address; blow away any
			 * previous mappings.
			 */
			(void) as_unmap(as, *addrp, len);
		}

		vn_a.vp = cvp;
		vn_a.offset = off;
		vn_a.type = flags & MAP_TYPE;
		vn_a.prot = (uchar_t)prot;
		vn_a.maxprot = (uchar_t)maxprot;
		vn_a.flags = flags & ~MAP_TYPE;
		vn_a.cred = cred;
		vn_a.amp = NULL;

		error = as_map(as, *addrp, len, segvn_create, &vn_a);
		as_rangeunlock(as);
	} else
		return (ENODEV);

	return (error);
}

/*ARGSUSED1*/
static int
spec_addmap(
	struct vnode *vp,	/* the common vnode */
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,		/* how many bytes to add */
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cred)
{
	int error = 0;
	struct snode *csp = VTOS(vp);
	ulong_t npages;

	ASSERT(vp != NULL && VTOS(vp)->s_commonvp == vp);

	/*
	 * XXX	Given the above assertion, this might not
	 *	be a particularly sensible thing to test.
	 */
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	npages = btopr(len);
	mutex_enter(&csp->s_lock);
	while (csp->s_flag & SLOCKED) {
		csp->s_flag |= SWANT;
		cv_wait(&csp->s_cv, &csp->s_lock);
	}
	csp->s_flag |= SLOCKED;
	mutex_exit(&csp->s_lock);

	/*
	 * Increment the reference count the first time the device
	 * gets a mapped page... (The device must already
	 * be opened and referenced at this point).
	 */
	if (csp->s_mapcnt == 0) {
		(void) ddi_hold_installed_driver(getmajor(csp->s_dev));
	}

	csp->s_mapcnt += npages;

	mutex_enter(&csp->s_lock);
	if (csp->s_flag & SWANT) {
		cv_broadcast(&csp->s_cv);
	}
	csp->s_flag &= ~(SLOCKED|SWANT);
	mutex_exit(&csp->s_lock);
	return (error);
}

/*ARGSUSED1*/
static int
spec_delmap(
	struct vnode *vp,	/* the common vnode */
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,		/* how many bytes to take away */
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	struct cred *cred)
{
	register struct snode *csp = VTOS(vp);
	ulong_t npages;
	register long mcnt;

	/* segdev passes us the common vp */

	ASSERT(vp != NULL && VTOS(vp)->s_commonvp == vp);

	/*
	 * XXX	Given the above assertion, this might not
	 *	be a particularly sensible thing to test..
	 */
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	npages = btopr(len);
	mutex_enter(&csp->s_lock);
	while (csp->s_flag & SLOCKED) {
		csp->s_flag |= SWANT;
		cv_wait(&csp->s_cv, &csp->s_lock);
	}
	csp->s_flag |= SLOCKED;
	mcnt = (csp->s_mapcnt -= npages);
	mutex_exit(&csp->s_lock);

	if (mcnt == 0) {
		dev_t dev = csp->s_dev;

		/*
		 * Call the close routine when the last reference of any
		 * kind through any [s, v]node goes away.
		 */
		if (!stillreferenced(dev, vp->v_type)) {
			/*
			 * XXX - want real file flags here
			 */
			(void) device_close(vp, 0, cred);
		}

		/*
		 * Decrement the driver reference count when the number
		 * of mapped pages goes back to zero. The driver may
		 * (or may not) already be closed.
		 */
		ddi_rele_driver(getmajor(dev));

	} else if (mcnt < 0)
		cmn_err(CE_PANIC, "spec_delmap: fewer than 0 mappings");

	mutex_enter(&csp->s_lock);
	if (csp->s_flag & SWANT) {
		cv_broadcast(&csp->s_cv);
	}
	csp->s_flag &= ~(SLOCKED|SWANT);
	mutex_exit(&csp->s_lock);

	return (0);
}

static int
spec_dump(struct vnode *vp, caddr_t addr, int bn, int count)
{
	ASSERT(vp->v_type == VBLK);
	return (bdev_dump(vp->v_rdev, addr, bn, count));
}


/*
 * Do i/o on the given page list from/to vp, io_off for io_len.
 * Flags are composed of:
 * 	{B_ASYNC, B_INVAL, B_FREE, B_DONTNEED, B_READ, B_WRITE}
 * If B_ASYNC is not set i/o is waited for.
 */
/*ARGSUSED5*/
static int
spec_pageio(
	register struct vnode *vp,
	page_t	*pp,
	u_offset_t io_off,
	size_t	io_len,
	int	flags,
	cred_t	*cr)
{
	struct buf *bp = NULL;
	int err = 0;

	if (pp == NULL)
		return (EINVAL);

	bp = spec_startio(vp, pp, io_off, io_len, flags);

	/*
	 * Wait for i/o to complete if the request is not B_ASYNC.
	 */
	if ((flags & B_ASYNC) == 0) {
		err = biowait(bp);
		pageio_done(bp);
	}
	return (err);
}

/*
 * Set ACL on underlying vnode if one exists, or return ENOSYS otherwise.
 */
int
spec_setsecattr(struct vnode *vp, vsecattr_t *vsap, int flag, struct cred *cr)
{
	struct vnode *realvp;
	struct snode *sp = VTOS(vp);
	int error;

	/*
	 * The acl(2) system calls VOP_RWLOCK on the file before setting an
	 * ACL, but since specfs does not serialize reads and writes, this
	 * VOP does not do anything.  However, some backing file systems may
	 * expect the lock to be held before setting an ACL, so it is taken
	 * here privately to avoid serializing specfs reads and writes.
	 */
	if ((realvp = sp->s_realvp) != NULL) {
		VOP_RWLOCK(realvp, 1);
		error = VOP_SETSECATTR(realvp, vsap, flag, cr);
		VOP_RWUNLOCK(realvp, 1);
		return (error);
	} else
		return (fs_nosys());
}

/*
 * Get ACL from underlying vnode if one exists, or fabricate it from
 * the permissions returned by spec_getattr() otherwise.
 */
int
spec_getsecattr(struct vnode *vp, vsecattr_t *vsap, int flag, struct cred *cr)
{
	struct vnode *realvp;
	struct snode *sp = VTOS(vp);

	if ((realvp = sp->s_realvp) != NULL)
		return (VOP_GETSECATTR(realvp, vsap, flag, cr));
	else
		return (fs_fab_acl(vp, vsap, flag, cr));
}
