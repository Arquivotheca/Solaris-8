/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1991, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)specsubr.c	1.53	99/10/22 SMI"
/*	From:	SVr4.0	"fs:fs/specfs/specsubr.c	1.42"	*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fs/snode.h>
#include <sys/fs/fifonode.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/open.h>
#include <sys/user.h>
#include <sys/termios.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/autoconf.h>
#include <sys/esunddi.h>
#include <sys/flock.h>
#include <sys/modctl.h>
#include <sys/dc_ki.h>

static dev_t specdev;
struct kmem_cache *snode_cache;

static struct snode *sfind(dev_t, vtype_t, struct vnode *);
static struct vnode *get_cvp(dev_t, vtype_t, struct snode *, int *);
static void sinsert(struct snode *);

/*
 * Return a shadow special vnode for the given dev.
 * If no snode exists for this dev create one and put it
 * in a table hashed by <dev, realvp>.  If the snode for
 * this dev is already in the table return it (ref count is
 * incremented by sfind).  The snode will be flushed from the
 * table when spec_inactive calls sdelete.
 *
 * The fsid is inherited from the real vnode so that clones
 * can be found.
 *
 */
struct vnode *
specvp(
	struct vnode	*vp,
	dev_t		dev,
	vtype_t		type,
	struct cred	*cr)
{
	register struct snode *sp;
	register struct snode *nsp;
	register struct snode *csp;
	register struct snode *tcsp;
	register struct vnode *svp;
	struct vattr va;
	int	rc;
	int	used_csp = 0;		/* Did we use pre-allocated csp */
	dev_t	ldev;			/* kernel internal dev number */
	minor_t lminor, gminor;
	dev_type_t class_type;

	if (vp == NULL)
		return (NULL);
	if (vp->v_type == VFIFO)
		return (fifovp(vp, cr));

	ASSERT(vp->v_type == type);
	ASSERT(vp->v_rdev == dev);

	/*
	 * Solaris Clustering: Note that specvp is passing in a global minor
	 * number encoded dev_t.  This assumption is based on the fact that
	 * the devices get dev_t out of the inode which would have the
	 * global minor number encoded.  We save the local encoded minor
	 * number in the dev saved in the snode.  This will result in
	 * getting the local dev_t in all snodes.
	 */

	gminor = getminor(dev);
	if (DC_RESOLVE_MINOR(&dcops, getmajor(dev), gminor, &lminor,
	    &class_type)) {
		ldev = NODEV;
	} else {
		ldev = makedevice(getmajor(dev), lminor);
	}

	/*
	 * Pre-allocate snodes before holding any locks in case we block
	 */
	nsp = kmem_cache_alloc(snode_cache, KM_SLEEP);
	csp = kmem_cache_alloc(snode_cache, KM_SLEEP);

	/*
	 * Get the time attributes outside of the stable lock since
	 * this operation may block. Unfortunately, it may not have
	 * been required if the snode is in the cache.
	 */
	va.va_mask = AT_TIMES;
	rc = VOP_GETATTR(vp, &va, 0, cr);	/* XXX may block! */

	mutex_enter(&stable_lock);
	if ((sp = sfind(ldev, type, vp)) == NULL) {
		struct vnode *cvp;

		sp = nsp;	/* Use pre-allocated snode */
		svp = STOV(sp);

		sp->s_realvp	= vp;
		sp->s_commonvp	= NULL;
		sp->s_dev	= ldev;
		sp->s_gdev	= dev;
		sp->s_dip	= NULL;
		sp->s_nextr	= NULL;
		sp->s_list	= NULL;
		sp->s_size	= 0;
		sp->s_flag	= 0;
		if (rc == 0) {
			/*
			 * Set times in snode to those in the vnode.
			 */
			sp->s_fsid = va.va_fsid;
			sp->s_atime = va.va_atime.tv_sec;
			sp->s_mtime = va.va_mtime.tv_sec;
			sp->s_ctime = va.va_ctime.tv_sec;
		} else {
			sp->s_fsid = specdev;
			sp->s_atime = 0;
			sp->s_mtime = 0;
			sp->s_ctime = 0;
		}
		sp->s_count	= 0;
		sp->s_mapcnt	= 0;

		VN_HOLD(vp);
		svp->v_flag	= 0;
		svp->v_count	= 1;
		svp->v_vfsmountedhere = NULL;
		svp->v_vfsp	= vp->v_vfsp;
		svp->v_stream	= NULL;
		svp->v_pages	= NULL;
		svp->v_type	= type;
		svp->v_rdev	= ldev;
		svp->v_filocks	= NULL;
		svp->v_shrlocks	= NULL;
		if (type == VBLK || type == VCHR) {
			cvp = get_cvp(ldev, type, csp, &used_csp);
			svp->v_stream = cvp->v_stream;

			sp->s_commonvp = cvp;
			tcsp = VTOS(cvp);

			/* Atomic update. No lock needed */
			sp->s_size = tcsp->s_size;
		}
		sinsert(sp);
		mutex_exit(&stable_lock);
		if (used_csp == 0) {
			/* Didn't use pre-allocated snode so free it */
			kmem_cache_free(snode_cache, csp);
		}
	} else {
		mutex_exit(&stable_lock);
		/* free unused snode memory */
		kmem_cache_free(snode_cache, nsp);
		kmem_cache_free(snode_cache, csp);
	}
	return (STOV(sp));
}

/*
 * Return a special vnode for the given dev; no vnode is supplied
 * for it to shadow.  Always create a new snode and put it in the
 * table hashed by <dev, NULL>.  The snode will be flushed from the
 * table when spec_inactive() calls sdelete().
 *
 * N.B. Assumes caller takes on responsibility of making sure no one
 * else is creating a snode for (dev, type) at this time.
 */
struct vnode *
makespecvp(register dev_t dev, register vtype_t type)
{
	struct snode *sp;
	struct vnode *svp, *cvp;
	time_t now;

	sp = kmem_cache_alloc(snode_cache, KM_SLEEP);
	svp = STOV(sp);
	cvp = commonvp(dev, type);
	now = hrestime.tv_sec;

	sp->s_realvp	= NULL;
	sp->s_commonvp	= cvp;
	sp->s_dev	= dev;
	sp->s_dip	= NULL;
	sp->s_nextr	= NULL;
	sp->s_list	= NULL;
	sp->s_size	= VTOS(cvp)->s_size;
	sp->s_flag	= 0;
	sp->s_fsid	= specdev;
	sp->s_atime	= now;
	sp->s_mtime	= now;
	sp->s_ctime	= now;
	sp->s_count	= 0;
	sp->s_mapcnt	= 0;

	svp->v_flag	= 0;
	svp->v_count	= 1;
	svp->v_vfsmountedhere = NULL;
	svp->v_vfsp	= NULL;
	svp->v_stream	= cvp->v_stream;
	svp->v_pages	= NULL;
	svp->v_type	= type;
	svp->v_rdev	= dev;
	svp->v_filocks	= NULL;
	svp->v_shrlocks	= NULL;

	mutex_enter(&stable_lock);
	sinsert(sp);
	mutex_exit(&stable_lock);

	return (svp);
}

/*
 * Find a special vnode that refers to the given device
 * of the given type.  Never return a "common" vnode.
 * Return NULL if a special vnode does not exist.
 * HOLD the vnode before returning it.
 */
struct vnode *
specfind(dev_t dev, vtype_t type)
{
	register struct snode *st;
	register struct vnode *nvp;

	mutex_enter(&stable_lock);
	st = stable[STABLEHASH(dev)];
	while (st != NULL) {
		if (st->s_dev == dev) {
			nvp = STOV(st);
			if (nvp->v_type == type && st->s_commonvp != nvp) {
				VN_HOLD(nvp);
				mutex_exit(&stable_lock);
				return (nvp);
			}
		}
		st = st->s_next;
	}
	mutex_exit(&stable_lock);
	return (NULL);
}

/*
 * Loop through the snode cache looking for snodes referencing dip
 * which currently have open references or are mapped.  We only
 * look at commonvp nodes since these are the only nodes which
 * have their s_mapped or s_count fields bumped.  If the s_dip
 * field is null, then spec_open() was unable to determine a
 * devinfo node for a dev_t at open time - in this case, we
 * can no longer determine whether there are any open references to
 * this dip so we fail.
 *
 * Returns:
 *	DEVI_REFERENCED		- if dip is referenced
 *	DEVI_NOT_REFERENCED	- if dip is not referenced
 *	DEVI_REF_UNKNOWN	- if unable to determine if dip is referenced.
 */
int
devi_stillreferenced(dev_info_t *dip)
{
	struct snode *sp;
	struct vnode *vp;
	int i;
	major_t major;
	int dip_state = DEVI_NOT_REFERENCED;
#ifdef DEBUG
	extern struct devnames *devnamesp;
	struct devnames *dnp;
#endif	/* DEBUG */

	major = ddi_name_to_major(ddi_binding_name(dip));
	if ((major == (major_t)-1)) {
		return (DEVI_REF_UNKNOWN);
	}

#ifdef DEBUG
	ASSERT(dip != NULL);

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	ASSERT(DN_BUSY_CHANGING(dnp->dn_flags));
	ASSERT(curthread == dnp->dn_busy_thread);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
#endif	/* DEBUG */

	mutex_enter(&stable_lock);
	for (i = 0; i < STABLESIZE; i++) {
		for (sp = stable[i]; sp != NULL; sp = sp->s_next) {
			vp = STOV(sp);
			/*
			 * We are only interested in commonvp's - they
			 * are the nodes whose s_count fields get bumped.
			 */
			if (sp->s_commonvp != vp) {
				continue;
			}
			if ((getmajor(sp->s_dev) == major) &&
			    (sp->s_dip == NULL)) {
				/*
				 * There is no way for us to know if this
				 * node is a minor node of the dip we are
				 * interested in.
				 * We record this and continue in case
				 * we do find a minor node for this dip
				 * that is open.
				 */
				dip_state = DEVI_REF_UNKNOWN;
			}
			if (sp->s_dip == dip) {
				ASSERT(getmajor(sp->s_dev) == major);
				mutex_enter(&sp->s_lock);
				if ((sp->s_count > 0) || (sp->s_mapcnt > 0) ||
				    (sp->s_flag & SLOCKED)) {
					/*
					 * Found an open minor node for this
					 * dip.
					 */
					mutex_exit(&sp->s_lock);
					mutex_exit(&stable_lock);
					return (DEVI_REFERENCED);
				}
				mutex_exit(&sp->s_lock);
			}
		}
	}
	mutex_exit(&stable_lock);
	return (dip_state);
}

/*
 * Check to see if a device is still in use (indicated by the
 * values of the s_count and s_mapcnt of the common snode).  The
 * device may still be in use if someone has it open either through
 * the same snode or a different snode, or if someone has it mapped.
 * If it is still open, return 1, otherwise return 0.
 *
 */
int
stillreferenced(dev_t dev, vtype_t type)
{
	register struct snode *sp;
	register struct vnode *vp;

	mutex_enter(&stable_lock);
	sp = stable[STABLEHASH(dev)];
	while (sp != NULL) {
		if (sp->s_dev == dev) {
			vp = STOV(sp);
			if (vp->v_type == type && sp->s_commonvp == vp) {
				mutex_exit(&stable_lock);
				/*
				 * the reference count of the found common
				 * vnode will not drop to zero since the
				 * caller of this routine still holds the vnode
				 */
				return ((sp->s_count > 0 || sp->s_mapcnt > 0)?
				    1: 0);
			}
		}
		sp = sp->s_next;
	}
	mutex_exit(&stable_lock);

	return (0);
}

/*
 * Given a device vnode, return the common
 * vnode associated with it.
 */
struct vnode *
common_specvp(register struct vnode *vp)
{
	register struct snode *sp;

	if ((vp->v_type != VBLK) && (vp->v_type != VCHR) ||
	    vp->v_op != spec_getvnodeops())
		return (vp);
	sp = VTOS(vp);
	return (sp->s_commonvp);
}

/*
 * Returns a special vnode for the given dev.  The vnode is the
 * one which is "common" to all the snodes which represent the
 * same device.
 * Similar to commonvp() but doesn't acquire the stable_lock, and
 * may use a pre-allocated snode provided by caller.
 */
static struct vnode *
get_cvp(
	dev_t		dev,
	vtype_t		type,
	struct snode	*nsp,		/* pre-allocated snode */
	int		*used_nsp)	/* flag indicating if we use nsp */
{
	register struct snode *sp;
	register struct vnode *svp;

	ASSERT(MUTEX_HELD(&stable_lock));
	if ((sp = sfind(dev, type, NULL)) == NULL) {
		sp = nsp;		/* Use pre-allocated snode */
		*used_nsp = 1;		/* return value */
		svp = STOV(sp);

		sp->s_realvp	= NULL;
		sp->s_commonvp	= svp;		/* points to itself */
		sp->s_dev	= dev;
		sp->s_dip	= NULL;
		sp->s_nextr	= NULL;
		sp->s_list	= NULL;
		sp->s_size	= UNKNOWN_SIZE;
		sp->s_flag	= 0;
		sp->s_fsid	= specdev;
		sp->s_atime	= 0;
		sp->s_mtime	= 0;
		sp->s_ctime	= 0;
		sp->s_count	= 0;
		sp->s_mapcnt	= 0;

		svp->v_flag	= 0;
		svp->v_count	= 1;
		svp->v_vfsmountedhere = NULL;
		svp->v_vfsp	= NULL;
		svp->v_stream	= NULL;
		svp->v_pages	= NULL;
		svp->v_type	= type;
		svp->v_rdev	= dev;
		svp->v_filocks	= NULL;
		svp->v_shrlocks	= NULL;

		sinsert(sp);
	} else
		*used_nsp = 0;
	return (STOV(sp));
}

/*
 * Returns a special vnode for the given dev.  The vnode is the
 * one which is "common" to all the snodes which represent the
 * same device.  For use ONLY by SPECFS.
 */
struct vnode *
commonvp(dev_t dev, vtype_t type)
{
	struct snode *sp, *nsp;
	struct vnode *svp;

	/* Pre-allocate snode in case we might block */
	nsp = kmem_cache_alloc(snode_cache, KM_SLEEP);

	mutex_enter(&stable_lock);
	if ((sp = sfind(dev, type, NULL)) == NULL) {
		sp = nsp;		/* Use pre-alloced snode */
		svp = STOV(sp);

		sp->s_realvp	= NULL;
		sp->s_commonvp	= svp;		/* points to itself */
		sp->s_dev	= dev;
		sp->s_dip	= NULL;
		sp->s_nextr	= NULL;
		sp->s_list	= NULL;
		sp->s_size	= UNKNOWN_SIZE;
		sp->s_flag	= 0;
		sp->s_fsid	= specdev;
		sp->s_atime	= 0;
		sp->s_mtime	= 0;
		sp->s_ctime	= 0;
		sp->s_count	= 0;
		sp->s_mapcnt	= 0;

		svp->v_flag	= 0;
		svp->v_count	= 1;
		svp->v_vfsmountedhere = NULL;
		svp->v_vfsp	= NULL;
		svp->v_stream	= NULL;
		svp->v_pages	= NULL;
		svp->v_type	= type;
		svp->v_rdev	= dev;
		svp->v_filocks	= NULL;
		svp->v_shrlocks	= NULL;

		sinsert(sp);
		mutex_exit(&stable_lock);
	} else {
		mutex_exit(&stable_lock);
		/* Didn't need the pre-allocated snode */
		kmem_cache_free(snode_cache, nsp);
	}
	return (STOV(sp));
}

/*
 * Snode lookup stuff.
 * These routines maintain a table of snodes hashed by dev so
 * that the snode for an dev can be found if it already exists.
 */
struct snode *stable[STABLESIZE];
kmutex_t	stable_lock;

/*
 * Put a snode in the table.
 */
static void
sinsert(struct snode *sp)
{
	ASSERT(MUTEX_HELD(&stable_lock));
	sp->s_next = stable[STABLEHASH(sp->s_dev)];
	stable[STABLEHASH(sp->s_dev)] = sp;
}

/*
 * Remove an snode from the hash table.
 * The realvp is not released here because spec_inactive() still
 * needs it to do a spec_fsync().
 */
void
sdelete(struct snode *sp)
{
	struct snode *st;
	struct snode *stprev = NULL;

	ASSERT(MUTEX_HELD(&stable_lock));
	st = stable[STABLEHASH(sp->s_dev)];
	while (st != NULL) {
		if (st == sp) {
			if (stprev == NULL)
				stable[STABLEHASH(sp->s_dev)] = st->s_next;
			else
				stprev->s_next = st->s_next;
			break;
		}
		stprev = st;
		st = st->s_next;
	}
}

/*
 * Lookup an snode by <dev, type, vp>.
 * ONLY looks for snodes with non-NULL s_realvp members and
 * common snodes (with s_commonvp poining to its vnode).
 *
 * If vp is NULL, only return commonvp. Otherwise return
 * shadow vp with both shadow and common vp's VN_HELD.
 */
static struct snode *
sfind(
	dev_t	dev,
	vtype_t	type,
	struct vnode *vp)
{
	register struct snode *st;
	register struct vnode *svp;

	ASSERT(MUTEX_HELD(&stable_lock));
	st = stable[STABLEHASH(dev)];
	while (st != NULL) {
		svp = STOV(st);
		if (st->s_dev == dev && svp->v_type == type &&
		    VN_CMP(st->s_realvp, vp) &&
		    (vp != NULL || st->s_commonvp == svp)) {
			VN_HOLD(svp);
			return (st);
		}
		st = st->s_next;
	}
	return (NULL);
}

/*
 * Mark the accessed, updated, or changed times in an snode
 * with the current time.
 */
void
smark(register struct snode *sp, register int flag)
{
	time_t now = hrestime.tv_sec;

	mutex_enter(&sp->s_lock);
	sp->s_flag |= flag;
	if (flag & SACC)
		sp->s_atime = now;
	if (flag & SUPD)
		sp->s_mtime = now;
	if (flag & SCHG)
		sp->s_ctime = now;
	mutex_exit(&sp->s_lock);
}

/*
 * Return the maximum file offset permitted for this device.
 * -1 means unrestricted.
 */
offset_t
spec_maxoffset(struct vnode *vp)
{
	struct snode *sp = VTOS(vp);
	struct snode *csp = VTOS(sp->s_commonvp);

	if (vp->v_type == VBLK)
		return (MAXOFFSET_T);
	else if (STREAMSTAB(getmajor(sp->s_dev)))
		return ((offset_t)(-1));
	else if (csp->s_flag & SLOFFSET)
		return (SPEC_MAXOFFSET_T);
	else
		return (MAXOFF_T);
}

/*ARGSUSED*/
static int
snode_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct snode *sp = buf;
	struct vnode *vp = STOV(sp);

	vp->v_op = spec_getvnodeops();
	vp->v_data = (caddr_t)sp;

	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&sp->s_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sp->s_cv, NULL, CV_DEFAULT, NULL);
	return (0);
}

/*ARGSUSED1*/
static void
snode_destructor(void *buf, void *cdrarg)
{
	struct snode *sp = buf;
	struct vnode *vp = STOV(sp);

	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);
	mutex_destroy(&sp->s_lock);
	cv_destroy(&sp->s_cv);
}

/*ARGSUSED1*/
int
specinit(struct vfssw *vswp, int fstype)
{
	dev_t dev;

	mutex_init(&stable_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&spec_syncbusy, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Create snode cache
	 */
	snode_cache = kmem_cache_create("snode_cache", sizeof (struct snode),
		0, snode_constructor, snode_destructor, NULL, NULL, NULL, 0);

	/*
	 * Associate vfs and vnode operations.
	 */
	vswp->vsw_vfsops = &spec_vfsops;
	if ((dev = getudev()) == -1)
		dev = 0;
	specdev = makedevice(dev, 0);
	return (0);
}

int
device_close(struct vnode *vp, int flag, struct cred *cr)
{
	struct snode *sp = VTOS(vp);
	enum vtype type = vp->v_type;
	struct vnode *cvp;
	dev_t dev;
	register int error;

	dev = sp->s_dev;
	cvp = sp->s_commonvp;

	switch (type) {

	case VCHR:
		if (STREAMSTAB(getmajor(dev))) {
			if (cvp->v_stream != NULL)
				error = strclose(cvp, flag, cr);
			vp->v_stream = NULL;
		} else
			error = dev_close(dev, flag, OTYP_CHR, cr);
		break;

	case VBLK:
		/*
		 * On last close a block device we must
		 * invalidate any in-core blocks so that we
		 * can, for example, change floppy disks.
		 */
		(void) spec_putpage(cvp, (offset_t)0,
		    (size_t)0, B_INVAL|B_FORCE, cr);
		bflush(dev);
		binval(dev);
		error = dev_close(dev, flag, OTYP_BLK, cr);
		break;
	}

	return (error);
}

struct vnode *
makectty(register vnode_t *ovp)
{
	register vnode_t *vp;

	if (vp = makespecvp(ovp->v_rdev, VCHR)) {
		register struct snode *sp;
		register struct snode *csp;
		register struct vnode *cvp;

		sp = VTOS(vp);
		cvp = sp->s_commonvp;
		csp = VTOS(cvp);
		mutex_enter(&csp->s_lock);
		csp->s_count++;
		mutex_exit(&csp->s_lock);

		/*
		 * Keep the reference count correct for the controlling tty.
		 */
		(void) ddi_hold_installed_driver(getmajor(ovp->v_rdev));
	}

	return (vp);
}
