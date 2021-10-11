/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xmem_vfsops.c	1.4	99/06/15 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/time.h>
#include <sys/pathname.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/atomic.h>
#include <sys/bitmap.h>
#include <fs/fs_subr.h>
#include <vm/page.h>
#include <sys/model.h>
#include <sys/map.h>
#include <vm/seg_kmem.h>
#include <sys/cpuvar.h>

#include <sys/fs/swapnode.h>
#include <sys/fs/xmem.h>

#ifndef kmxtob
#define	RM_TO_VA(seg, index)	((seg)->s_base + mmu_ptob(index))
#define	VA_TO_RM(seg, vaddr)	mmu_btop((char *)(vaddr) - (seg)->s_base)
#define	kmxtob(x)		RM_TO_VA(&kvseg, x)
#define	btokmx(x)		VA_TO_RM(&kvseg, x)
#endif


/*
 * xmemfs vfs operations.
 */
static int xmemfsinit(struct vfssw *, int);
static int xmem_mount(struct vfs *, struct vnode *,
	struct mounta *, struct cred *);
static int xmem_unmount(struct vfs *, int, struct cred *);
static int xmem_root(struct vfs *, struct vnode **);
static int xmem_statvfs(struct vfs *, struct statvfs64 *);
static int xmem_vget(struct vfs *, struct vnode **, struct fid *);

static struct vfsops xmem_vfsops = {
	xmem_mount,
	xmem_unmount,
	xmem_root,
	xmem_statvfs,
	fs_sync,
	xmem_vget,
	fs_nosys,	/* mountroot */
	fs_nosys,	/* swapvp */
	fs_freevfs
};

/*
 * Loadable module wrapper
 */
#include <sys/modctl.h>

static struct vfssw vfw = {
	"xmemfs",
	xmemfsinit,
	&xmem_vfsops,
	0
};

/*
 * Module linkage information
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for xmemfs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlfs, NULL
};

pgcnt_t	xmemfs_minfree;

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int xmemfsfstype;
static dev_t xmemdev;
static uint32_t xmemfs_minor;


/*
 * initialize global xmemfs locks and such
 * called when loading xmemfs module
 */
static int
xmemfsinit(struct vfssw *vswp, int fstype)
{
	long		dev;
	extern void	xmemfs_hash_init();

	xmemfs_hash_init();
	xmemfsfstype = fstype;
	ASSERT(xmemfsfstype != 0);

	vswp->vsw_vfsops = &xmem_vfsops;
	if ((dev = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN, "xmemfsinit: Can't get unique device number.");
		dev = 0;
	}
	xmemdev = makedevice(dev, 0);

	return (0);
}


/*
 * xpg is an array of page_t * if xm_ppb > 1.
 * xpg is a page_t * if xm_ppb == 1
 */
void
xpgput(struct xmount *xm, void *xpg)
{
	ASSERT(xm->xm_xpgcnt < xm->xm_max);
	xm->xm_xpgarray[xm->xm_xpgcnt++] = xpg;
}

void *
xpgget(struct xmount *xm)
{
	if (!xm->xm_xpgcnt)
		return (NULL);

	return (xm->xm_xpgarray[--xm->xm_xpgcnt]);
}

void *
xpginit(struct xmount *xm)
{
	xm->xm_xpgcnt = 0;
	xm->xm_xpgarray = kmem_zalloc(sizeof (void *) * xm->xm_max,
					KM_NOSLEEP);
	return (xm->xm_xpgarray);
}

void
xpgtrunc(struct xmount *xm, size_t newsz)
{
	void 	*old = xm->xm_xpgarray;

	ASSERT(newsz == xm->xm_xpgcnt);
	if (newsz) {
		xm->xm_xpgarray =
			kmem_alloc(sizeof (void *) * newsz, KM_SLEEP);
		bcopy(old, xm->xm_xpgarray, sizeof (void *) * newsz);
	}
	kmem_free(old, sizeof (void *) * xm->xm_max);
}

void
xpgdeinit(struct xmount *xm)
{
	xm->xm_xpgcnt = 0;
	if (xm->xm_max)
		kmem_free(xm->xm_xpgarray, sizeof (void *) * xm->xm_max);
	xm->xm_xpgarray = NULL;
}


struct xmount	*xmountp;		/* ### DEBUG */

static int
xmem_mount(struct vfs *vfsp, struct vnode *mvp, struct mounta *uap,
	struct cred *cr)
{
	struct xmount	*xm = NULL;
	struct xmemnode	*xp;
	struct pathname	dpn;
	char		*data = uap->dataptr;
	int		datalen = uap->datalen;
	int		error;
	struct xmemfs_args xargs;
	struct vattr	rattr;
	int		got_attrs, num_pagesizes;
	uint_t		blocks_left;
	size_t		vmmapsize, mapsize, frag;
	caddr_t		vmaddr, addr;

	XMEMPRINTF(1, ("xmem_mount: vfs %x mvp %x uap %x cr %x\n", vfsp, mvp,
			uap, cr));
	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/*
	 * Get arguments
	 */
	if (datalen != 0) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (datalen != sizeof (xargs))
				return (EINVAL);
			else {
				if (copyin(data, &xargs, sizeof (xargs)))
					return (EFAULT);
			}
		} else {
			struct xmemfs_args32 xargs32;

			if (datalen != sizeof (xargs32))
				return (EINVAL);
			else {
				if (copyin(data, &xargs32, sizeof (xargs32)))
					return (EFAULT);
				xargs.xa_bsize = xargs32.xa_bsize;
				xargs.xa_fssize = xargs32.xa_fssize;
				xargs.xa_flags = xargs32.xa_flags;
			}
		}
		if (xargs.xa_bsize == 0)
			xargs.xa_bsize = PAGESIZE;
	} else {
		xargs.xa_bsize = PAGESIZE;
		xargs.xa_flags = 0;
		xargs.xa_fssize = 0;
	}

	XMEMPRINTF(1, ("xmem_mount: xa bsize %x fssize %x flags %x\n",
			xargs.xa_bsize, xargs.xa_fssize, xargs.xa_flags));

	num_pagesizes = page_num_pagesizes();

	if (xargs.xa_flags & XARGS_LARGEPAGES)
		xargs.xa_bsize = page_get_pagesize(num_pagesizes - 1);

	/* Make sure xa_bsize is a pure power of two */
	if (!IS_P2ALIGNED(xargs.xa_bsize, xargs.xa_bsize - 1)) {
		cmn_err(CE_WARN, "xmemfs: invalid blocksize %x",
			(int) xargs.xa_bsize);
		xargs.xa_bsize = PAGESIZE;
	}

	while (--num_pagesizes >= 0)
		if (xargs.xa_bsize == page_get_pagesize(num_pagesizes))
			break;

	if (num_pagesizes < 0) {
		cmn_err(CE_WARN,
			"xmemfs: blocksize %d not a natural pagesize",
				(int) xargs.xa_bsize);
		xargs.xa_bsize = PAGESIZE;
	}

	vmmapsize = mapsize = xargs.xa_bsize * SEGXMEM_NUM_SIMULMAPS;

	/* need to allocate more to ensure alignment if largepage */

	if (xargs.xa_bsize != PAGESIZE)
		vmmapsize += xargs.xa_bsize;

	vmaddr = addr = vmem_alloc(heap_arena, vmmapsize, VM_NOSLEEP);

	if (!addr) {
		return (ENOMEM);
	}

	frag = ((uintptr_t)addr & ((uintptr_t)xargs.xa_bsize - 1));

	if (frag) {
		addr += (xargs.xa_bsize - frag);
	}

	if (error = pn_get(uap->dir, UIO_USERSPACE, &dpn)) {
		vmem_free(heap_arena, vmaddr, vmmapsize);
		return (error);
	}

	if ((xm = xmem_memalloc(sizeof (struct xmount), 0)) == NULL) {
		vmem_free(heap_arena, vmaddr, vmmapsize);
		pn_free(&dpn);
		return (ENOMEM);
	}
	xmountp = xm;			/* ### DEBUG */

	XMEMPRINTF(4, ("xmem_mount: xmountp %x\n", xmountp));

	/* Set block size & max memory allowed for the file system */
	xm->xm_bsize = (size_t)xargs.xa_bsize;
	xm->xm_bshift = highbit(xargs.xa_bsize) - 1;

	/*
	 * 5 * lotsfree satisfies XMEMMINFREE for 4 GB of memory and above.
	 */
	xmemfs_minfree = min(5 * lotsfree, XMEMMINFREE/PAGESIZE);

	if (xargs.xa_fssize) {

		pgcnt_t		fspgcnt;

		xargs.xa_fssize = roundup(xargs.xa_fssize, xm->xm_bsize);

		fspgcnt = xargs.xa_fssize >> PAGESHIFT;

		/* sanity check this against freemem */
		if (fspgcnt + xmemfs_minfree > freemem) {
			vmem_free(heap_arena, vmaddr, vmmapsize);
			pn_free(&dpn);
			return (EFBIG);
		}
		xm->xm_max = xargs.xa_fssize >> xm->xm_bshift;
	} else {
		/*
		 * fssize is mandatory - should not be here but if
		 * fssize == 0 is allowed, grab all of free memory
		 * minus xmemfs_minfree.
		 */

		if (freemem < xmemfs_minfree)
			xm->xm_max = 0;
		else
			xm->xm_max = freemem - xmemfs_minfree;

		xm->xm_max >>= xm->xm_bshift - PAGESHIFT;
	}
	xm->xm_ppb = btop(xm->xm_bsize);		/* pages per block */

	XMEMPRINTF(1, ("xmem_mount: xm_max %x xm_bsize %x\n",
		xm->xm_max, xm->xm_bsize));

	if (!xpginit(xm)) {
		vmem_free(heap_arena, vmaddr, vmmapsize);
		pn_free(&dpn);
		xmem_memfree(xm, sizeof (struct xmount));
		return (ENOMEM);
	}

	if (xargs.xa_flags & XARGS_RESERVEMEM) {
		struct seg	tmpseg;

		/* grab all memory now */
		blocks_left = xm->xm_max;
		bzero(&tmpseg, sizeof (struct seg));
		tmpseg.s_as = &kas;

		while (blocks_left) {
			page_t		*pp, *pplist;
			page_t		**ppa;
			int		i;

			(void) page_create_wait(xm->xm_ppb, PG_WAIT);
			pplist = page_get_freelist(NULL, 0, &tmpseg, NULL,
							xm->xm_bsize, 0, NULL);
			if (pplist == NULL) {
				page_create_putback(xm->xm_ppb);
				xpgtrunc(xm, xm->xm_max - blocks_left);
				xm->xm_max -= blocks_left;
				if (xargs.xa_fssize)
					cmn_err(CE_WARN,
						"could only reserve %d blocks "
						"for xmemfs", (int)xm->xm_max);
				break;
			}

			/*
			 * xpages is 8 bytes and for ppb==1, xp_ppa is
			 * 4 bytes. For a large 4GB file system, this
			 * requires 1 million xpages plus 1 million
			 * number of xp_ppa which equals 12 MByte of
			 * memory and 2 million calls to kmem_alloc...
			 * optimise for ppb == 1 - let xp_ppa point directly
			 * to page.
			 */

			if (xm->xm_ppb > 1)
				ppa = kmem_alloc(sizeof (page_t *) * xm->xm_ppb,
					KM_SLEEP);

			for (i = 0; i < xm->xm_ppb; i++) {
				pp = pplist;
				page_sub(&pplist, pp);
				ASSERT(PAGE_EXCL(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(!hat_page_is_mapped(pp));
				PP_CLRFREE(pp);
				PP_CLRAGED(pp);
				if (xm->xm_ppb == 1)
					ppa = (page_t **)pp;
				else
					ppa[i] = pp;
			}

			xpgput(xm, ppa);
			blocks_left--;
		}
		if (!xm->xm_xpgcnt) {
			/* No pages at all */
			vmem_free(heap_arena, vmaddr, vmmapsize);
			pn_free(&dpn);
			xpgdeinit(xm);
			xmem_memfree(xm, sizeof (struct xmount));
			return (ENOMEM);
		}
		xm->xm_flags |= XARGS_RESERVEMEM;
	}
	xm->xm_bsize = (size_t)xargs.xa_bsize;
	/*
	 * Allocate a map to provide an address for each page in
	 * (xargs.xa_bsize * 4) and free all of them.
	 */
	xm->xm_vmmapsize = vmmapsize;
	xm->xm_vmmapaddr = vmaddr;
	xm->xm_map = rmallocmap_wait(mapsize/PAGESIZE);
	xm->xm_mapsize = mapsize;
	xm->xm_mapaddr = addr;
	rmfree(xm->xm_map, xm->xm_mapsize, (ulong_t)addr);

	/*
	 * find an available minor device number for this mount
	 */
	do {
		xm->xm_dev = makedevice(xmemdev,
		    atomic_add_32_nv(&xmemfs_minor, 1));
	} while (vfs_devismounted(xm->xm_dev));

	/*
	 * Set but don't bother entering the mutex
	 * (xmount not on mount list yet)
	 */
	mutex_init(&xm->xm_contents, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&xm->xm_renamelck, NULL, MUTEX_DEFAULT, NULL);

	xm->xm_vfsp = vfsp;

	vfsp->vfs_data = (caddr_t)xm;
	vfsp->vfs_fstype = xmemfsfstype;
	vfsp->vfs_dev = xm->xm_dev;
	vfsp->vfs_bsize = xm->xm_bsize;
	vfsp->vfs_flag |= VFS_NOTRUNC;
	vfs_make_fsid(&vfsp->vfs_fsid, xm->xm_dev, xmemfsfstype);
	xm->xm_mntpath = xmem_memalloc(dpn.pn_pathlen + 1, 1);
	(void) strcpy(xm->xm_mntpath, dpn.pn_path);

	/*
	 * allocate and initialize root xmemnode structure
	 */
	bzero(&rattr, sizeof (struct vattr));
	rattr.va_mode = (mode_t)(S_IFDIR | 0777);
	rattr.va_type = VDIR;
	rattr.va_rdev = 0;
	xp = xmem_memalloc(sizeof (struct xmemnode), 1);
	xmemnode_init(xm, xp, &rattr, cr);

	/*
	 * Get the mode, uid, and gid from the underlying mount point.
	 */
	rattr.va_mask = AT_MODE|AT_UID|AT_GID;	/* Hint to getattr */
	got_attrs = VOP_GETATTR(mvp, &rattr, 0, cr);

	rw_enter(&xp->xn_rwlock, RW_WRITER);
	XNTOV(xp)->v_flag |= VROOT;

	/*
	 * If the getattr succeeded, use its results.  Otherwise allow
	 * the previously set hardwired defaults to prevail.
	 */
	if (got_attrs == 0) {
		xp->xn_mode = rattr.va_mode;
		xp->xn_uid = rattr.va_uid;
		xp->xn_gid = rattr.va_gid;
	}

	/*
	 * initialize linked list of xmemnodes so that the back pointer of
	 * the root xmemnode always points to the last one on the list
	 * and the forward pointer of the last node is null.
	 */
	xp->xn_back = xp;
	xp->xn_forw = NULL;
	xp->xn_nlink = 0;
	xm->xm_rootnode = xp;

	xdirinit(xp, xp);

	rw_exit(&xp->xn_rwlock);

	pn_free(&dpn);
	return (0);
}

static int
xmem_unmount(struct vfs *vfsp, int flag, struct cred *cr)
{
	struct xmount *xm = (struct xmount *)VFSTOXM(vfsp);
	struct xmemnode *xp;

	if (!suser(cr))
		return (EPERM);
	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 */
	if (flag & MS_FORCE)
		return (ENOTSUP);

	mutex_enter(&xm->xm_contents);

	/*
	 * Don't close down the xmemfs if there are open files.
	 * There should be only one file referenced (the rootnode)
	 * and only one reference to the vnode for that file.
	 */
	xp = xm->xm_rootnode;
	if (XNTOV(xp)->v_count > 1) {
		mutex_exit(&xm->xm_contents);
		return (EBUSY);
	}

	for (xp = xp->xn_forw; xp; xp = xp->xn_forw) {
		if (XNTOV(xp)->v_count > 0) {
			mutex_exit(&xm->xm_contents);
			return (EBUSY);
		}
	}

	/*
	 * We can drop the mutex now because no one can find this mount
	 */
	mutex_exit(&xm->xm_contents);

	/*
	 * Free all kmemalloc'd and non-anonalloc'd memory associated with
	 * this filesystem.  To do this, we go through the file list twice,
	 * once to remove all the directory entries, and then to remove
	 * all the files.  We do this because there is useful code in
	 * xmemnode_free which assumes that the directory entry has been
	 * removed before the file.
	 */
	/*
	 * Remove all directory entries
	 */
	for (xp = xm->xm_rootnode; xp; xp = xp->xn_forw) {
		rw_enter(&xp->xn_rwlock, RW_WRITER);
		if (xp->xn_type == VDIR)
			xdirtrunc(xp);
		rw_exit(&xp->xn_rwlock);
	}

	ASSERT(xm->xm_rootnode);

	/*
	 * We re-acquire the lock to prevent others who have a HOLD on
	 * a xmemnode via its pages from blowing it away
	 * (in xmem_inactive) while we're trying to get to it here. Once
	 * we have a HOLD on it we know it'll stick around.
	 */
	mutex_enter(&xm->xm_contents);
	/*
	 * Remove all the files (except the rootnode) backwards.
	 */
	while ((xp = xm->xm_rootnode->xn_back) != xm->xm_rootnode) {
		/*
		 * Blow the xmemnode away by HOLDing it and RELE'ing it.
		 * The RELE calls inactive and blows it away because there
		 * we have the last HOLD.
		 */
		VN_HOLD(XNTOV(xp));
		mutex_exit(&xm->xm_contents);
		VN_RELE(XNTOV(xp));
		mutex_enter(&xm->xm_contents);
		/*
		 * It's still there after the RELE. Someone else like pageout
		 * has a hold on it so wait a bit and then try again - we know
		 * they'll give it up soon.
		 */
		if (xp == xm->xm_rootnode->xn_back) {
			mutex_exit(&xm->xm_contents);
			delay(hz / 4);
			mutex_enter(&xm->xm_contents);
		}
	}
	if (xm->xm_flags & XARGS_RESERVEMEM) {
		page_t	**ppa;
		uint_t	pindex;

		while ((ppa = xpgget(xm)) != NULL) {
			int	flag = B_FREE;
			if (xm->xm_ppb == 1) {
				VN_DISPOSE((page_t *)ppa, flag, 0, kcred);
				continue;
			}
			/* free each page */
			for (pindex = 0; pindex < xm->xm_ppb; pindex++) {
				VN_DISPOSE(ppa[pindex], flag, 0, kcred);
			}
			kmem_free(ppa, sizeof (*ppa) * xm->xm_ppb);
		}
		xpgdeinit(xm);
	}
	mutex_exit(&xm->xm_contents);

	VN_RELE(XNTOV(xm->xm_rootnode));

	ASSERT(xm->xm_mntpath);

	xmem_memfree(xm->xm_mntpath, strlen(xm->xm_mntpath) + 1);

	mutex_destroy(&xm->xm_contents);
	mutex_destroy(&xm->xm_renamelck);
	vmem_free(heap_arena, xm->xm_vmmapaddr, xm->xm_vmmapsize);
	rmfreemap(xm->xm_map);
	xmem_memfree(xm, sizeof (struct xmount));

	return (0);
}

/*
 * return root xmemnode for given vnode
 */
static int
xmem_root(struct vfs *vfsp, struct vnode **vpp)
{
	struct xmount *xm = (struct xmount *)VFSTOXM(vfsp);
	struct xmemnode *xp = xm->xm_rootnode;
	struct vnode *vp;

	ASSERT(xp);

	vp = XNTOV(xp);
	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

static int
xmem_statvfs(struct vfs *vfsp, struct statvfs64 *sbp)
{
	struct xmount	*xm = (struct xmount *)VFSTOXM(vfsp);
	long	blocks;
	dev32_t d32;

	sbp->f_bsize = xm->xm_bsize;
	sbp->f_frsize = xm->xm_bsize;	/* No fragmentation for now ? */

	/*
	 * Find the amount of available physical and memory swap
	 */
	if (xm->xm_flags & XARGS_RESERVEMEM)
		blocks = xm->xm_max - xm->xm_mem;
	else
		blocks = MAX((long)(freemem - lotsfree - xmemfs_minfree), 0);

	sbp->f_bavail = sbp->f_bfree = (fsblkcnt64_t)blocks;

	/*
	 * Total number of blocks is what's available plus what's been used
	 */
	sbp->f_blocks = (fsblkcnt64_t)(sbp->f_bfree + xm->xm_mem);

	/*
	 * return a somewhat arbitrary number of inodes available
	 */
	sbp->f_favail = sbp->f_ffree = (fsfilcnt64_t)((xm->xm_max/1024)+1);
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sbp->f_fsid = d32;
	(void) strcpy(sbp->f_basetype, vfssw[xmemfsfstype].vsw_name);
	(void) strcpy(sbp->f_fstr, xm->xm_mntpath);
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = MAXNAMELEN - 1;
	return (0);
}

static int
xmem_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp)
{
	register struct xfid *xfid;
	register struct xmount *xm = (struct xmount *)VFSTOXM(vfsp);
	register struct xmemnode *xp = NULL;

	xfid = (struct xfid *)fidp;
	*vpp = NULL;

	mutex_enter(&xm->xm_contents);
	for (xp = xm->xm_rootnode; xp; xp = xp->xn_forw) {
		mutex_enter(&xp->xn_tlock);
		if (xp->xn_nodeid == xfid->xfid_ino) {
			/*
			 * If the gen numbers don't match we know the
			 * file won't be found since only one xmemnode
			 * can have this number at a time.
			 */
			if (xp->xn_gen != xfid->xfid_gen || xp->xn_nlink == 0) {
				mutex_exit(&xp->xn_tlock);
				mutex_exit(&xm->xm_contents);
				return (0);
			}
			*vpp = (struct vnode *)XNTOV(xp);

			VN_HOLD(*vpp);

			if ((xp->xn_mode & S_ISVTX) &&
			    !(xp->xn_mode & (S_IXUSR | S_IFDIR))) {
				mutex_enter(&(*vpp)->v_lock);
				(*vpp)->v_flag |= VISSWAP;
				mutex_exit(&(*vpp)->v_lock);
			}
			mutex_exit(&xp->xn_tlock);
			mutex_exit(&xm->xm_contents);
			return (0);
		}
		mutex_exit(&xp->xn_tlock);
	}
	mutex_exit(&xm->xm_contents);
	return (0);
}
