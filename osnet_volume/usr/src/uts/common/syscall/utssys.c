/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)utssys.c	1.21	99/08/31 SMI"

#include <sys/param.h>
#include <sys/inttypes.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/session.h>
#include <sys/var.h>
#include <sys/utsname.h>
#include <sys/utssys.h>
#include <sys/ustat.h>
#include <sys/statvfs.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>

/*
 * utssys()
 */

static int uts_fusers(char *, int, char *, rval_t *);
static int dofusers(vnode_t *, int, char *, rval_t *);
static int _statvfs64_by_dev(dev_t, struct statvfs64 *);

#if defined(_ILP32) || defined(_SYSCALL32_IMPL)

static int utssys_uname32(caddr_t, rval_t *);
static int utssys_ustat32(dev_t, struct ustat32 *);

int64_t
utssys32(void *buf, int arg, int type, void *outbp)
{
	int error;
	rval_t rv;

	rv.r_vals = 0;

	switch (type) {
	case UTS_UNAME:
		/*
		 * This is an obsolete way to get the utsname structure
		 * (it only gives you the first 8 characters of each field!)
		 * uname(2) is the preferred and better interface.
		 */
		error = utssys_uname32(buf, &rv);
		break;
	case UTS_USTAT:
		error = utssys_ustat32(expldev((dev32_t)arg), buf);
		break;
	case UTS_FUSERS:
		error = uts_fusers(buf, arg, outbp, &rv);
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error == 0 ? rv.r_vals : (int64_t)set_errno(error));
}

static int
utssys_uname32(caddr_t buf, rval_t *rvp)
{
	if (copyout(utsname.sysname, buf, 8))
		return (EFAULT);
	buf += 8;
	if (subyte(buf, 0) < 0)
		return (EFAULT);
	buf++;
	if (copyout(utsname.nodename, buf, 8))
		return (EFAULT);
	buf += 8;
	if (subyte(buf, 0) < 0)
		return (EFAULT);
	buf++;
	if (copyout(utsname.release, buf, 8))
		return (EFAULT);
	buf += 8;
	if (subyte(buf, 0) < 0)
		return (EFAULT);
	buf++;
	if (copyout(utsname.version, buf, 8))
		return (EFAULT);
	buf += 8;
	if (subyte(buf, 0) < 0)
		return (EFAULT);
	buf++;
	if (copyout(utsname.machine, buf, 8))
		return (EFAULT);
	buf += 8;
	if (subyte(buf, 0) < 0)
		return (EFAULT);
	rvp->r_val1 = 1;
	return (0);
}

static int
utssys_ustat32(dev_t dev, struct ustat32 *cbuf)
{
	struct ustat32 ust32;
	struct statvfs64 stvfs;
	fsblkcnt64_t	fsbc64;
	char *cp, *cp2;
	int i, error;

	if ((error = _statvfs64_by_dev(dev, &stvfs)) != 0)
		return (error);

	fsbc64 = stvfs.f_bfree * (stvfs.f_frsize / 512);
	/*
	 * Check to see if the number of free blocks can be expressed
	 * in 31 bits or whether the number of free files is more than
	 * can be expressed in 32 bits and is not -1 (UINT64_MAX).  NFS
	 * Version 2 does not support the number of free files and
	 * hence will return -1.  -1, when translated from a 32 bit
	 * quantity to an unsigned 64 bit quantity, turns into UINT64_MAX.
	 */
	if (fsbc64 > INT32_MAX ||
	    (stvfs.f_ffree > UINT32_MAX && stvfs.f_ffree != UINT64_MAX))
		return (EOVERFLOW);

	ust32.f_tfree = (daddr32_t)fsbc64;
	ust32.f_tinode = (ino32_t)stvfs.f_ffree;

	cp = stvfs.f_fstr;
	cp2 = ust32.f_fname;
	i = 0;
	while (i++ < sizeof (ust32.f_fname))
		if (*cp != '\0')
			*cp2++ = *cp++;
		else
			*cp2++ = '\0';
	while (*cp != '\0' &&
	    (i++ < sizeof (stvfs.f_fstr) - sizeof (ust32.f_fpack)))
		cp++;
	(void) strncpy(ust32.f_fpack, cp + 1, sizeof (ust32.f_fpack));

	if (copyout(&ust32, cbuf, sizeof (ust32)))
		return (EFAULT);
	return (0);
}

#endif	/* _ILP32 || _SYSCALL32_IMPL */

#ifdef _LP64

static int uts_ustat64(dev_t, struct ustat *);

int64_t
utssys64(void *buf, long arg, int type, void *outbp)
{
	int error;
	rval_t rv;

	rv.r_vals = 0;

	switch (type) {
	case UTS_USTAT:
		error = uts_ustat64((dev_t)arg, buf);
		break;
	case UTS_FUSERS:
		error = uts_fusers(buf, (int)arg, outbp, &rv);
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error == 0 ? rv.r_vals : (int64_t)set_errno(error));
}

static int
uts_ustat64(dev_t dev, struct ustat *cbuf)
{
	struct ustat ust;
	struct statvfs64 stvfs;
	fsblkcnt64_t	fsbc64;
	char *cp, *cp2;
	int i, error;

	if ((error = _statvfs64_by_dev(dev, &stvfs)) != 0)
		return (error);

	fsbc64 = stvfs.f_bfree * (stvfs.f_frsize / 512);
	ust.f_tfree = (daddr_t)fsbc64;
	ust.f_tinode = (ino_t)stvfs.f_ffree;

	cp = stvfs.f_fstr;
	cp2 = ust.f_fname;
	i = 0;
	while (i++ < sizeof (ust.f_fname))
		if (*cp != '\0')
			*cp2++ = *cp++;
		else
			*cp2++ = '\0';
	while (*cp != '\0' &&
	    (i++ < sizeof (stvfs.f_fstr) - sizeof (ust.f_fpack)))
		cp++;
	(void) strncpy(ust.f_fpack, cp + 1, sizeof (ust.f_fpack));

	if (copyout(&ust, cbuf, sizeof (ust)))
		return (EFAULT);
	return (0);
}

#endif	/* _LP64 */

/*
 * Utility routine for the ustat implementations.
 * (If it wasn't for the 'find-by-dev_t' semantic of ustat(2), we could push
 * this all out into userland, sigh.)
 */
static int
_statvfs64_by_dev(dev_t dev, struct statvfs64 *svp)
{
	vfs_t *vfsp;

	/*
	 * Search vfs list for user-specified device.
	 */
	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev || cmpdev(vfsp->vfs_dev) == dev)
			break;
	vfs_list_unlock();
	if (vfsp == NULL)
		return (EINVAL);

	return (VFS_STATVFS(vfsp, svp));
}

/*
 * Determine the ways in which processes are using a named file or mounted
 * file system (path).  Normally return 0 with rvp->rval1 set to the number of
 * processes found to be using it.  For each of these, fill a f_user_t to
 * describe the process and its usage.  When successful, copy this list
 * of structures to the user supplied buffer (outbp).
 *
 * In error cases, clean up and return the appropriate errno.
 */

static int
uts_fusers(char *path, int flags, char *outbp, rval_t *rvp)
{
	vnode_t *fvp = NULL;
	int error;

	if ((error = lookupname(path, UIO_USERSPACE, FOLLOW, NULLVPP, &fvp))
	    != 0)
		return (error);
	ASSERT(fvp);
	error = dofusers(fvp, flags, outbp, rvp);
	VN_RELE(fvp);
	return (error);
}

static int
dofusers(vnode_t *fvp, int flags, char *outbp, rval_t *rvp)
{
	proc_t *prp;
	int pcnt = 0;			/* number of f_user_t's copied out */
	int error = 0;
	int contained = (flags == F_CONTAINED);
	vfs_t *cvfsp;
	int use_flag = 0;
	file_t *fp;
	f_user_t *fuentry, *fubuf;	/* accumulate results here */
	int i;
	struct seg *seg;
	struct as *as;
	vnode_t *mvp;
	int v_proc = v.v_proc;
	uf_entry_t *ufp;
	pid_t npids, pidx, *pidlist;

	fuentry = fubuf = kmem_alloc(v_proc * sizeof (f_user_t), KM_SLEEP);
	pidlist = kmem_alloc(v_proc * sizeof (pid_t), KM_SLEEP);

	if (contained && !(fvp->v_flag & VROOT)) {
		error = EINVAL;
		goto out;
	}
	if (fvp->v_count == 1)		/* no other active references */
		goto out;
	cvfsp = fvp->v_vfsp;
	ASSERT(cvfsp);

	mutex_enter(&pidlock);
	for (npids = 0, prp = practive; prp != NULL; prp = prp->p_next)
		pidlist[npids++] = prp->p_pid;
	mutex_exit(&pidlock);

	for (pidx = 0; pidx < npids; pidx++) {
		user_t *up;
		uf_info_t *fip;

		prp = sprlock(pidlist[pidx]);
		if (prp == NULL)
			continue;

		up = PTOU(prp);
		fip = P_FINFO(prp);

		if (up->u_cdir && (VN_CMP(fvp, up->u_cdir) || contained &&
		    up->u_cdir->v_vfsp == cvfsp)) {
			use_flag |= F_CDIR;
		}
		if (up->u_rdir && (VN_CMP(fvp, up->u_rdir) || contained &&
		    up->u_rdir->v_vfsp == cvfsp)) {
			use_flag |= F_RDIR;
		}
		if (prp->p_exec && (VN_CMP(fvp, prp->p_exec) ||
		    contained && prp->p_exec->v_vfsp == cvfsp)) {
			use_flag |= F_TEXT;
		}
		if (fvp->v_type == VCHR) {
			/*
			 * Do this like /proc does it (see prgetpsinfo())
			 */
			extern dev_t rwsconsdev, rconsdev, uconsdev;
			dev_t d = cttydev(prp);

			if (d == rwsconsdev || d == rconsdev)
				d = uconsdev;
			if (d == fvp->v_rdev)
				use_flag |= F_TTY;
		}

		mutex_enter(&fip->fi_lock);
		for (i = 0; i < fip->fi_nfiles; i++) {
			UF_ENTER(ufp, fip, i);
			if ((fp = ufp->uf_file) != NULL) {
				/*
				 * There is no race between the fp
				 * assignment and the line below
				 * because uf_lock is held.  f_vnode
				 * will not change value because we
				 * make sure that the uf_file entry
				 * is not updated until the final
				 * value for f_vnode is acquired.
				 */
				if (fp->f_vnode &&
				    (VN_CMP(fvp, fp->f_vnode) ||
				    contained &&
				    fp->f_vnode->v_vfsp == cvfsp)) {
					use_flag |= F_OPEN;
					UF_EXIT(ufp);
					break; /* we don't count fds */
				}
			}
			UF_EXIT(ufp);
		}
		mutex_exit(&fip->fi_lock);

		/*
		 * mmap usage
		 */
		mutex_exit(&prp->p_lock);
		if (prp->p_as != &kas) {
			as = prp->p_as;
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			for (seg = AS_SEGP(as, as->a_segs); seg;
			    seg = AS_SEGP(as, seg->s_next)) {
				if ((seg->s_ops != &segvn_ops) ||
				    (seg->s_data == NULL))
					continue;
				if (SEGOP_GETVP(seg, seg->s_base, &mvp) ||
				    (mvp == NULL))
					continue;
				if (VN_CMP(fvp, mvp) ||
				    (contained && (cvfsp == mvp->v_vfsp))) {
					use_flag |= F_MAP;
					break;
				}
			}
			AS_LOCK_EXIT(as, &as->a_lock);
		}
		mutex_enter(&prp->p_lock);

		if (use_flag) {
			fuentry->fu_pid = prp->p_pid;
			fuentry->fu_flags = use_flag;
			mutex_enter(&prp->p_crlock);
			fuentry->fu_uid = prp->p_cred->cr_ruid;
			mutex_exit(&prp->p_crlock);
			fuentry++;
			pcnt++;
			use_flag = 0;
		}
		sprunlock(prp);
	}
	if (copyout(fubuf, outbp, pcnt * sizeof (f_user_t)))
		error = EFAULT;
out:
	kmem_free(fubuf, v_proc * sizeof (f_user_t));
	kmem_free(pidlist, v_proc * sizeof (pid_t));
	rvp->r_val1 = pcnt;
	return (error);
}
