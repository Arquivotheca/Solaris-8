/*
 * Copyright (c) 1988-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)lofs_subr.c	1.22	98/05/14 SMI"	/* SunOS-4.1.1 */

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/t_lock.h>
#include <sys/debug.h>

#include <sys/fs/lofs_node.h>
#include <sys/fs/lofs_info.h>

static lnode_t *lfind(struct vnode *, struct loinfo *);
static void lsave(lnode_t *);
static struct vfs *makelfsnode(struct vfs *, struct loinfo *);
static struct lfsnode *lfsfind(struct vfs *, struct loinfo *);

int lo_lfscount = 0;
int lo_lncount = 0;
int lo_freecount = 0;

/*
 * Mutex to protect the following variables
 *	lofs_major
 *	lofs_minor
 */
kmutex_t lofs_minor_lock;
major_t lofs_major;
minor_t lofs_minor;

lnode_t *lofreelist;
kmutex_t lofs_ltable_lock;	/* protects lofs_ltable and lofreelist */

void
lofs_subrinit(void)
{
	mutex_init(&lofs_ltable_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&lofs_minor_lock, NULL, MUTEX_DEFAULT, NULL);
	/*
	 * Assign unique major number for all lofs mounts
	 */
	if ((lofs_major = getudev()) == -1) {
		cmn_err(CE_WARN, "lofs: init: can't get unique device number");
		lofs_major = 0;
	}
	lofs_minor = 0;
}

void
lofs_subrfini(void)
{
	mutex_destroy(&lofs_minor_lock);
	mutex_destroy(&lofs_ltable_lock);
}

/*
 * return a looped back vnode for the given vnode.
 * If no lnode exists for this vnode create one and put it
 * in a table hashed by vnode.  If the lnode for
 * this fhandle is already in the table return it (ref count is
 * incremented by lfind).  The lnode will be flushed from the
 * table when lo_inactive calls freelonode.
 * NOTE: vp is assumed to be a held vnode.
 */
struct vnode *
makelonode(vp, li)
	struct vnode *vp;
	struct loinfo *li;
{
	lnode_t *lp, *tlp;
	struct vfs *vfsp;

	mutex_enter(&lofs_ltable_lock);
	if ((lp = lfind(vp, li)) == NULL) {
		if (lofreelist) {
			lp = lofreelist;
			lofreelist = lp->lo_next;
			lo_freecount--;
			bzero(lp, sizeof (*lp));
		} else {
			mutex_exit(&lofs_ltable_lock);
			tlp = (lnode_t *)kmem_zalloc(sizeof (*tlp), KM_SLEEP);
			mutex_enter(&lofs_ltable_lock);
			lo_lncount++;
			if ((lp = lfind(vp, li)) != NULL) {
				tlp->lo_next = lofreelist;
				lofreelist = tlp;
				lo_freecount++;
				VN_RELE(vp);
				goto found_lnode;
			}
			lp = tlp;
		}
		li->li_refct++;
		vfsp = makelfsnode(vp->v_vfsp, li);
		VN_INIT(ltov(lp), vfsp, vp->v_type, li->li_rdev);
		ltov(lp)->v_flag |= (vp->v_flag & (VNOMOUNT|VNOMAP|VDIROPEN));
		ltov(lp)->v_op = &lo_vnodeops;
		ltov(lp)->v_data = (caddr_t)lp;
		lp->lo_vp = vp;
		lp->lo_looping = 0;
		lp->lo_crossedvp = NULL;
		lsave(lp);
	} else {
		VN_RELE(vp);
	}
found_lnode:
	mutex_exit(&lofs_ltable_lock);
	return (ltov(lp));
}

/*
 * Get/Make vfs structure for given real vfs
 */
static struct vfs *
makelfsnode(vfsp, li)
	struct vfs *vfsp;
	struct loinfo *li;
{
	struct lfsnode *lfs;
	struct lfsnode *tlfs;

	ASSERT(MUTEX_HELD(&lofs_ltable_lock));
	ASSERT(li->li_refct > 0);

	if (vfsp == li->li_realvfs)
		return (li->li_mountvfs);
	if ((lfs = lfsfind(vfsp, li)) == NULL) {
		mutex_exit(&lofs_ltable_lock);
		lfs = kmem_zalloc(sizeof (*lfs), KM_SLEEP);
		mutex_enter(&lofs_ltable_lock);
		if ((tlfs = lfsfind(vfsp, li)) != NULL) {
			kmem_free(lfs, sizeof (struct lfsnode));
			lfs = tlfs;
			goto found_lfs;
		}
		lo_lfscount++;
		lfs->lfs_realvfs = vfsp;
		(void) VFS_ROOT(vfsp, &lfs->lfs_realrootvp);
		lfs->lfs_vfs.vfs_op = &lo_vfsops;
		lfs->lfs_vfs.vfs_data = (caddr_t)li;
		lfs->lfs_vfs.vfs_fstype = li->li_mountvfs->vfs_fstype;
		lfs->lfs_vfs.vfs_flag =
			(vfsp->vfs_flag | li->li_mflag) & INHERIT_VFS_FLAG;
		lfs->lfs_vfs.vfs_bsize = vfsp->vfs_bsize;
		lfs->lfs_vfs.vfs_dev = vfsp->vfs_dev;
		lfs->lfs_vfs.vfs_fsid = vfsp->vfs_fsid;
		lfs->lfs_next = li->li_lfs;
		li->li_lfs = lfs;
	}
found_lfs:
	lfs->lfs_refct++;
	return (&lfs->lfs_vfs);
}

/*
 * Free lfs node since no longer in use
 */
static void
freelfsnode(lfs, li)
	struct lfsnode *lfs;
	struct loinfo *li;
{
	struct lfsnode *prev = NULL;
	struct lfsnode *this;

	ASSERT(MUTEX_HELD(&lofs_ltable_lock));
	ASSERT(li->li_refct > 0);
	for (this = li->li_lfs; this; this = this->lfs_next) {
		if (this == lfs) {
			ASSERT(lfs->lfs_refct <= 0);
			if (prev == NULL)
				li->li_lfs = lfs->lfs_next;
			else
				prev->lfs_next = lfs->lfs_next;
			if (lfs->lfs_realrootvp != NULL) {
				VN_RELE(lfs->lfs_realrootvp);
			}
			kmem_free(lfs, sizeof (struct lfsnode));
			lo_lfscount--;
			return;
		}
		prev = this;
	}
	cmn_err(CE_PANIC, "freelfsnode");
}

/*
 * Find lfs given real vfs and mount instance(li)
 */
static struct lfsnode *
lfsfind(vfsp, li)
	struct vfs *vfsp;
	struct loinfo *li;
{
	struct lfsnode *lfs;

	ASSERT(MUTEX_HELD(&lofs_ltable_lock));

	/*
	 * We need to handle the case where a UFS filesystem was forced
	 * unmounted and then a subsequent mount got the same vfs structure.
	 * If the new mount lies in the lofs hierarchy, then this will confuse
	 * lofs, because the original vfsp (of the forced unmounted filesystem)
	 * is still around. We check for this condition here.
	 *
	 * If we find a cache vfsp hit, then we check to see if the cached
	 * filesystem was forced unmounted. Skip all such entries. This should
	 * be safe to do since no makelonode()->makelfsnode()->lfsfind() calls
	 * should be generated for such force-unmounted filesystems (because
	 * (ufs) lookup would've returned an error).
	 */
	for (lfs = li->li_lfs; lfs; lfs = lfs->lfs_next) {
		if (lfs->lfs_realvfs == vfsp) {
			struct vnode *realvp;

			realvp = lfs->lfs_realrootvp;
			if (realvp == NULL)
				continue;
			if (realvp->v_vfsp == NULL || realvp->v_type == VBAD)
				continue;
			else
				return (lfs);
		}
	}
	return (NULL);
}

/*
 * Find real vfs given loopback vfs
 */
struct vfs *
lo_realvfs(vfsp, realrootvpp)
	struct vfs *vfsp;
	struct vnode **realrootvpp;
{
	struct loinfo *li = vtoli(vfsp);
	struct lfsnode *lfs;

	ASSERT(li->li_refct > 0);
	if (vfsp == li->li_mountvfs) {
		if (realrootvpp != NULL)
			*realrootvpp = vtol(li->li_rootvp)->lo_vp;
		return (li->li_realvfs);
	}
	mutex_enter(&lofs_ltable_lock);
	for (lfs = li->li_lfs; lfs; lfs = lfs->lfs_next) {
		if (vfsp == &lfs->lfs_vfs) {
			if (realrootvpp != NULL)
				*realrootvpp = lfs->lfs_realrootvp;
			mutex_exit(&lofs_ltable_lock);
			return (lfs->lfs_realvfs);
		}
	}
	mutex_exit(&lofs_ltable_lock);
	panic("lo_realvfs");
	/* NOTREACHED */
}

/*
 * Lnode lookup stuff.
 * These routines maintain a table of lnodes hashed by vp so
 * that the lnode for a vp can be found if it already exists.
 * NOTE: LTABLESIZE must be a power of 2 for ltablehash to work!
 */

#define	LTABLESIZE	64
#define	ltablehash(vp)	((((intptr_t)(vp))>>10) & (LTABLESIZE-1))

lnode_t *ltable[LTABLESIZE];

/*
 * Put a lnode in the table
 */
static void
lsave(lp)
	lnode_t *lp;
{
	ASSERT(MUTEX_HELD(&lofs_ltable_lock));
	ASSERT(lp->lo_vp);

#ifdef LODEBUG
	lo_dprint(4, "lsave lp %x hash %d\n",
			lp, ltablehash(lp->lo_vp));
#endif
	lp->lo_next = ltable[ltablehash(lp->lo_vp)];
	ltable[ltablehash(lp->lo_vp)] = lp;
}

/*
 * Remove a lnode from the table
 */
void
freelonode(lp)
	lnode_t *lp;
{
	lnode_t *lt;
	lnode_t *ltprev = NULL;
	struct loinfo *li;
	struct lfsnode *lfs;
	struct vfs *vfsp;
	struct vnode *vp = ltov(lp);
	struct vnode *realvp = realvp(vp);

#ifdef LODEBUG
	lo_dprint(4, "freelonode lp %x hash %d\n",
			lp, ltablehash(lp->lo_vp));
#endif
	mutex_enter(&lofs_ltable_lock);

	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1) {
		vp->v_count--;	/* release our hold from vn_rele */
		mutex_exit(&vp->v_lock);
		mutex_exit(&lofs_ltable_lock);
		return;
	}
	mutex_exit(&vp->v_lock);
	mutex_destroy(&vp->v_lock);

	for (lt = ltable[ltablehash(lp->lo_vp)]; lt;
	    ltprev = lt, lt = lt->lo_next) {
		if (lt == lp) {
#ifdef LODEBUG
			lo_dprint(4, "freeing %x, vfsp %x\n",
					vp, vp->v_vfsp);
#endif
			vfsp = vp->v_vfsp;
			li = vtoli(vfsp);
			li->li_refct--;
			if (vfsp != li->li_mountvfs) {
				/* check for ununsed lfs */
				for (lfs = li->li_lfs; lfs;
					lfs = lfs->lfs_next) {
					if (vfsp == &lfs->lfs_vfs)
						break;
				}
				lfs->lfs_refct--;
				if (lfs->lfs_refct <= 0)
					freelfsnode(lfs, li);
			}
			if (ltprev == NULL) {
				ltable[ltablehash(lp->lo_vp)] = lt->lo_next;
			} else {
				ltprev->lo_next = lt->lo_next;
			}
			lt->lo_next = lofreelist;
			lofreelist = lt;
			lo_freecount++;
			mutex_exit(&lofs_ltable_lock);
			VN_RELE(realvp);
			return;
		}
	}
	cmn_err(CE_PANIC, "freelonode");
}

/*
 * Lookup a lnode by vp
 */
static lnode_t *
lfind(vp, li)
	struct vnode *vp;
	struct loinfo *li;
{
	register lnode_t *lt;

	ASSERT(MUTEX_HELD(&lofs_ltable_lock));
	lt = ltable[ltablehash(vp)];
	while (lt != NULL) {
		if (lt->lo_vp == vp && vtoli(ltov(lt)->v_vfsp) == li) {
			VN_HOLD(ltov(lt));
			return (lt);
		}
		lt = lt->lo_next;
	}
	return (NULL);
}

#ifdef	LODEBUG
static int lofsdebug;
#endif	LODEBUG

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */

#ifdef LODEBUG
/*VARARGS2*/
lo_dprint(level, str, a1, a2, a3, a4, a5, a6, a7, a8, a9)
	int level;
	char *str;
	int a1, a2, a3, a4, a5, a6, a7, a8, a9;
{

	if (lofsdebug == level || (lofsdebug > 10 && (lofsdebug - 10) >= level))
		printf(str, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}
#endif
