/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mntvnops.c	1.16	99/11/16 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/pathname.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/mode.h>
#include <sys/poll.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/vmsystm.h>
#include <sys/atomic.h>
#include <sys/mkdev.h>
#include <sys/mntio.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <fs/fs_subr.h>
#include <vm/rm.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/fs/mntdata.h>

#define	MNTROOTINO	2

uint_t mnt_nopen;	/* count of vnodes open on mnttab info */
mntnode_t *mntgetnode(vnode_t *);
void mntfs_freesnap(mntnode_t *);

/*
 * Defined and initialized after all functions have been defined.
 */
extern vnodeops_t mntvnodeops;

/*
 * NOTE: The following variable enables the generation of the "dev=xxx"
 * in the option string for a mounted file system.  Really this should
 * be gotten rid of altogether, but for the sake of backwards compatibility
 * we had to leave it in.  It is defined as a 32-bit device number.  This
 * means that when 64-bit device numbers are in use, if either the major or
 * minor part of the device number will not fit in a 16 bit quantity, the
 * "dev=" will be set to NODEV (0x7fffffff).  See PSARC 1999/566 and
 * 1999/131 for details.  The cmpldev() function used to generate the 32-bit
 * device number handles this check and assigns the proper value.
 */
int mntfs_enabledev = 1;	/* enable old "dev=xxx" option */

static int
mntfs_devsize(struct vfs *vfsp)
{
	dev32_t odev;

	(void) cmpldev(&odev, vfsp->vfs_dev);
	return (snprintf(NULL, 0, "dev=%x", odev));
}

static int
mntfs_devprint(struct vfs *vfsp, char *buf)
{
	dev32_t odev;

	(void) cmpldev(&odev, vfsp->vfs_dev);
	return (snprintf(buf, MAX_MNTOPT_STR, "dev=%x", odev));
}

static int
mntfs_optsize(struct vfs *vfsp)
{
	int i, size = 0;
	mntopt_t *mop;

	for (i = 0; i < vfsp->vfs_mntopts.mo_count; i++) {
		mop = &vfsp->vfs_mntopts.mo_list[i];
		if (mop->mo_flags & MO_NODISPLAY)
			continue;
		if (mop->mo_flags & MO_SET) {
			if (size)
				size++; /* space for comma */
			size += strlen(mop->mo_name);
			/*
			 * count option value if there is one
			 */
			if (mop->mo_arg != NULL) {
				size += strlen(mop->mo_arg) + 1;
			}
		}
	}
	if (mntfs_enabledev) {
		if (size != 0)
			size++; /* space for comma */
		size += mntfs_devsize(vfsp);
	}
	if (size == 0)
		size = strlen("-");
	return (size);
}

static int
mntfs_optprint(struct vfs *vfsp, char *buf)
{
	int i, optinbuf = 0;
	mntopt_t *mop;
	char *origbuf = buf;

	for (i = 0; i < vfsp->vfs_mntopts.mo_count; i++) {
		mop = &vfsp->vfs_mntopts.mo_list[i];
		if (mop->mo_flags & MO_NODISPLAY)
			continue;
		if (mop->mo_flags & MO_SET) {
			if (optinbuf)
				*buf++ = ',';
			else
				optinbuf = 1;
			buf += snprintf(buf, MAX_MNTOPT_STR,
				"%s", mop->mo_name);
			/*
			 * print option value if there is one
			 */
			if (mop->mo_arg != NULL) {
				buf += snprintf(buf, MAX_MNTOPT_STR, "=%s",
					mop->mo_arg);
			}
		}
	}
	if (mntfs_enabledev) {
		if (optinbuf++)
			*buf++ = ',';
		buf += mntfs_devprint(vfsp, buf);
	}
	if (!optinbuf) {
		buf += snprintf(buf, MAX_MNTOPT_STR, "-");
	}
	return (buf - origbuf);
}

static int
mntfs_len(uint_t *nent_ptr)
{
	struct vfs *vfsp;
	int size = 0;
	uint_t cnt = 0;

	for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
		/*
		 * Skip mounts that should not show up in mnttab
		 */
		if (vfsp->vfs_flag & VFS_NOMNTTAB)
			continue;
		cnt++;
		if (vfsp->vfs_resource != NULL && *vfsp->vfs_resource != '\0')
			size += strlen(vfsp->vfs_resource) + 1;
		else
			size += strlen("-") + 1;
		if (vfsp->vfs_mntpt != NULL && *vfsp->vfs_mntpt != '\0')
			size += strlen(vfsp->vfs_mntpt) + 1;
		else
			size += strlen("-") + 1;
		size += strlen(vfssw[vfsp->vfs_fstype].vsw_name) + 1;
		size += mntfs_optsize(vfsp);
		size += snprintf(NULL, 0, "\t%ld\n", vfsp->vfs_mtime);
	}
	*nent_ptr = cnt;
	return (size);
}

static void
mntfs_generate(mntnode_t *mnp, char *base)
{
	struct vfs *vfsp;
	char *cp = base;
	uint_t *dp = mnp->mnt_devlist;

	for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
		/*
		 * Skip mounts that should not show up in mnttab
		 */
		if (vfsp->vfs_flag & VFS_NOMNTTAB)
			continue;
		*dp++ = (uint_t)getmajor(vfsp->vfs_dev);
		*dp++ = (uint_t)getminor(vfsp->vfs_dev);
		if (vfsp->vfs_resource != NULL && *vfsp->vfs_resource != '\0') {
			cp += snprintf(cp, MAXPATHLEN, "%s\t",
				vfsp->vfs_resource);
		} else {
			cp += snprintf(cp, MAXPATHLEN, "-\t");
		}
		if (vfsp->vfs_mntpt != NULL && *vfsp->vfs_mntpt != '\0') {
			cp += snprintf(cp, MAXPATHLEN, "%s\t",
				vfsp->vfs_mntpt);
		} else {
			cp += snprintf(cp, MAXPATHLEN, "-\t");
		}
		cp += snprintf(cp, MAX_MNTOPT_STR, "%s\t",
			vfssw[vfsp->vfs_fstype].vsw_name);
		cp += mntfs_optprint(vfsp, cp);
		cp += snprintf(cp, MAX_MNTOPT_STR, "\t%ld", vfsp->vfs_mtime);
		*cp++ = '\n';
	}
}

static char *
mntfs_mapin(char *base, size_t size)
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

void
mntfs_freesnap(mntnode_t *mnp)
{
	(void) as_unmap(curproc->p_as, mnp->mnt_base,
		roundup(mnp->mnt_size, PAGESIZE));
	kmem_free(mnp->mnt_devlist, mnp->mnt_nres * sizeof (uint_t) * 2);
	mnp->mnt_devlist = NULL;
	mnp->mnt_size = 0;
	mnp->mnt_nres = 0;
}

size_t mnt_size;	/* Latest snapshot size */

/*
 * Snapshot the latest version of the kernel mounted resource information
 */
static int
mntfs_snapshot(mntnode_t *mnp)
{
	size_t size;
	char *addr, *base;
	timespec_t lastmodt;

	vfs_list_lock();
	/*
	 * Check if the mnttab info has changed since the last snapshot
	 */
	vfs_mnttab_modtime(&lastmodt);
	if (mnp->mnt_nres && lastmodt.tv_sec == mnp->mnt_time.tv_sec &&
		lastmodt.tv_nsec == mnp->mnt_time.tv_nsec) {
		vfs_list_unlock();
		return (0);
	}
	/*
	 * Free the previous snapshot if it exists
	 */
	if (mnp->mnt_nres != 0)
		mntfs_freesnap(mnp);
	size = mntfs_len(&mnp->mnt_nres);
	base = kmem_alloc(size, KM_SLEEP);
	mnp->mnt_devlist = kmem_alloc(mnp->mnt_nres * sizeof (uint_t) * 2,
		KM_SLEEP);
	mntfs_generate(mnp, base);
	vfs_mnttab_modtime(&mnp->mnt_time);
	vfs_list_unlock();
	addr = mntfs_mapin(base, size);
	kmem_free(base, size);
	if (addr == NULL)
		return (EOVERFLOW);
	mnp->mnt_base = addr;
	mnp->mnt_size = size;
	mnt_size = size;
	return (0);
}

/* ARGSUSED */
static int
mntopen(vnode_t **vpp, int flag, cred_t *cr)
{
	vnode_t *vp = *vpp;
	mntnode_t *nmnp;

	/*
	 * Not allowed to open for writing, return error.
	 */
	if (flag & FWRITE)
		return (EPERM);
	/*
	 * Create a new mnt/vnode for each open, this will give us a handle to
	 * hang the snapshot on.
	 */
	nmnp = mntgetnode(vp);
	*vpp = MTOV(nmnp);
	atomic_add_32(&mnt_nopen, 1);
	VN_RELE(vp);
	return (0);
}

/* ARGSUSED */
static int
mntclose(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cr)
{
	mntnode_t *mnp = VTOM(vp);

	if (count > 1)
		return (0);
	if (vp->v_count == 1) {
		mntfs_freesnap(mnp);
		atomic_add_32(&mnt_nopen, -1);
	}
	return (0);
}



/* ARGSUSED */
static int
mntread(vnode_t *vp, uio_t *uio, int ioflag, cred_t *cred)
{
	int error = 0;
	char *buf;
	off_t off = uio->uio_offset;
	size_t len = uio->uio_resid;
	mntnode_t *mnp = VTOM(vp);

	if (off == (off_t)0 || mnp->mnt_nres == 0)
		if ((error = mntfs_snapshot(mnp)) != 0)
			return (error);
	if ((size_t)(off + len) > mnp->mnt_size)
		len = mnp->mnt_size - off;

	if (off < 0 || len > mnp->mnt_size)
		return (EFAULT);

	if (len == 0)
		return (0);

	/*
	 * The mnttab image is stored in the user's address space,
	 * so we have to copy it into the kernel from userland,
	 * then copy it back out to the specified address.
	 */
	buf = kmem_alloc(len, KM_SLEEP);
	if (copyin(mnp->mnt_base + off, buf, len))
		error = EFAULT;
	else
		error = uiomove(buf, len, UIO_READ, uio);
	kmem_free(buf, len);
	return (error);
}


static int
mntgetattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	mntnode_t *mnp = VTOM(vp);
	int error;
	vnode_t *rvp;
	extern timespec_t vfs_mnttab_ctime;

	/*
	 * Return all the attributes.  Should be refined
	 * so that it returns only those asked for.
	 * Most of this is complete fakery anyway.
	 */
	rvp = mnp->mnt_mountvp;
	/*
	 * Attributes are same as underlying file with modifications
	 */
	if (error = VOP_GETATTR(rvp, vap, flags, cr))
		return (error);

	/*
	 * We always look like a regular file
	 */
	vap->va_type = VREG;
	/*
	 * mode should basically be read only
	 */
	vap->va_mode &= 07444;
	vap->va_fsid = vp->v_vfsp->vfs_dev;
	vap->va_blksize = DEV_BSIZE;
	vap->va_rdev = 0;
	vap->va_vcode = 0;
	/*
	 * Set nlink to the number of open vnodes for mnttab info
	 * plus one for existing.
	 */
	vap->va_nlink = mnt_nopen + 1;
	/*
	 * Set size to size of latest snapshot
	 */
	vap->va_size = mnt_size;
	/*
	 * Fetch mtime from the vfs mnttab timestamp
	 */
	vap->va_ctime = vfs_mnttab_ctime;
	vfs_list_lock();
	vfs_mnttab_modtime(&vap->va_mtime);
	vap->va_atime = vap->va_mtime;
	vfs_list_unlock();
	/*
	 * Nodeid is always ROOTINO;
	 */
	vap->va_nodeid = (ino64_t)MNTROOTINO;
	vap->va_nblocks = btod(vap->va_size);
	return (0);
}


static int
mntaccess(vnode_t *vp, int mode, int flags, cred_t *cr)
{
	mntnode_t *mnp = VTOM(vp);

	if (mode & (VWRITE|VEXEC))
		return (EROFS);

	/*
	 * Do access check on the underlying directory vnode.
	 */
	return (VOP_ACCESS(mnp->mnt_mountvp, mode, flags, cr));
}


/*
 * New /mntfs vnode required; allocate it and fill in most of the fields.
 */
mntnode_t *
mntgetnode(vnode_t *dp)
{
	mntnode_t *mnp;
	vnode_t *vp;

	mnp = kmem_zalloc(sizeof (mntnode_t), KM_SLEEP);
	mnp->mnt_mountvp = VTOM(dp)->mnt_mountvp;
	vp = MTOV(mnp);
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	vp->v_flag = VNOCACHE|VNOMAP|VNOSWAP|VNOMOUNT;
	vp->v_count = 1;
	vp->v_op = &mntvnodeops;
	vp->v_vfsp = dp->v_vfsp;
	vp->v_type = VREG;
	vp->v_data = (caddr_t)mnp;
	cv_init(&vp->v_cv, NULL, CV_DEFAULT, NULL);

	return (mnp);
}

/*
 * Free the storage obtained from mntgetnode().
 */
void
mntfreenode(mntnode_t *mnp)
{
	vnode_t *vp = MTOV(mnp);

	mutex_destroy(&vp->v_lock);
	cv_destroy(&vp->v_cv);

	kmem_free(mnp, sizeof (*mnp));
}


/* ARGSUSED */
static int
mntfsync(vnode_t *vp, int syncflag, cred_t *cr)
{
	return (0);
}

/* ARGSUSED */
static void
mntinactive(vnode_t *vp, cred_t *cr)
{
	mntnode_t *mnp = VTOM(vp);

	mntfreenode(mnp);
}

/* ARGSUSED */
static int
mntseek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
	return (0);
}

/*
 * Return the answer requested to poll().
 * POLLRDBAND will return when the mtime of the mnttab
 * information is newer than the latest one read for this open.
 */
/* ARGSUSED */
static int
mntpoll(vnode_t *vp, short ev, int any, short *revp, pollhead_t **phpp)
{
	mntnode_t *mnp = VTOM(vp);

	*revp = 0;
	*phpp = (pollhead_t *)NULL;
	if (ev & POLLIN)
		*revp |= POLLIN;

	if (ev & POLLRDNORM)
		*revp |= POLLRDNORM;

	if (ev & POLLRDBAND) {
		vfs_mnttab_poll(&mnp->mnt_time, phpp);
		if (*phpp == (pollhead_t *)NULL)
			*revp |= POLLRDBAND;
	}
	if (*revp || *phpp != NULL || any) {
		return (0);
	}
	/*
	 * If someone is polling an unsupported poll events (e.g.
	 * POLLOUT, POLLPRI, etc.), just return POLLERR revents.
	 * That way we will ensure that we don't return a 0
	 * revents with a NULL pollhead pointer.
	 */
	*revp = POLLERR;
	return (0);
}

/* ARGSUSED */
int
mntioctl(struct vnode *vp, int cmd, intptr_t arg, int flag,
	cred_t *cr, int *rvalp)
{
	uint_t *up = (uint_t *)arg;
	struct mnttagdesc *dp = (struct mnttagdesc *)arg;
	STRUCT_DECL(mnttagdesc, tagdesc);
	mntnode_t *mnp = VTOM(vp);
	char tagbuf[MAX_MNTOPT_TAG];
	char *pbuf;
	size_t len;
	int error;

	error = 0;
	switch (cmd) {

	case MNTIOC_NMNTS: {		/* get no. of mounted resources */
		if (mnp->mnt_nres == 0)
			if ((error = mntfs_snapshot(mnp)) != 0)
				break;
		if (copyout(&mnp->mnt_nres, up, sizeof (uint_t)))
			error = EFAULT;
		break;
	}

	case MNTIOC_GETDEVLIST: {	/* get mounted device major/minor nos */
		if (mnp->mnt_nres == 0)
			if ((error = mntfs_snapshot(mnp)) != 0)
				break;
		if (copyout(mnp->mnt_devlist, up, sizeof (uint_t) * 2 *
			mnp->mnt_nres))
			error = EFAULT;
		break;
	}

	case MNTIOC_SETTAG:		/* set tag on mounted file system */
	case MNTIOC_CLRTAG:		/* clear tag on mounted file system */
	{
		char *cptr;
		uint32_t major, minor;

		STRUCT_INIT(tagdesc, get_udatamodel());
		if (copyin(dp, STRUCT_BUF(tagdesc), STRUCT_SIZE(tagdesc))) {
			error = EFAULT;
			break;
		}
		pbuf = kmem_alloc(MAXPATHLEN, KM_SLEEP);
		cptr = STRUCT_FGETP(tagdesc, mtd_mntpt);
		if ((error = copyinstr(cptr, pbuf, MAXPATHLEN, &len))) {
			kmem_free(pbuf, MAXPATHLEN);
			break;
		}
		cptr = STRUCT_FGETP(tagdesc, mtd_tag);
		if ((error = copyinstr(cptr, tagbuf, MAX_MNTOPT_TAG, &len))) {
			kmem_free(pbuf, MAXPATHLEN);
			break;
		}
		major = STRUCT_FGET(tagdesc, mtd_major);
		minor = STRUCT_FGET(tagdesc, mtd_minor);
		if (cmd == MNTIOC_SETTAG)
			error = vfs_settag(major, minor, pbuf, tagbuf);
		else
			error = vfs_clrtag(major, minor, pbuf, tagbuf);
		kmem_free(pbuf, MAXPATHLEN);
		break;
	}

	default:
		error = EINVAL;
		break;
	}

	return (error);
}


/*
 * /mntfs vnode operations vector
 */
vnodeops_t mntvnodeops = {
	mntopen,
	mntclose,
	mntread,
	fs_nosys,
	mntioctl,
	fs_nosys,
	mntgetattr,
	fs_nosys,	/* setattr */
	mntaccess,
	fs_nosys,
	fs_nosys,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fs_nosys,
	fs_nosys,	/* symlink */
	fs_nosys,
	mntfsync,
	mntinactive,
	fs_nosys,	/* fid */
	fs_rwlock,
	fs_rwunlock,
	mntseek,
	fs_nosys,
	fs_frlock,	/* frlock */
	fs_nosys,	/* space */
	fs_nosys,
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* map */
	fs_nosys_addmap, /* addmap */
	fs_nosys,	/* delmap */
	mntpoll,	/* poll */
	fs_nosys,	/* dump */
	fs_nosys,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,	/* dispose */
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_nosys	/* shrlock */
};
