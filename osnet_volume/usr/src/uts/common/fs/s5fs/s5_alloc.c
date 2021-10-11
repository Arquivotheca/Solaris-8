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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1997 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/* #ident  "@(#)ufs_alloc.c 2.26     90/01/08 SMI"	*/
#pragma ident	"@(#)s5_alloc.c	1.11	97/11/03 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/fs/s5_fs.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fblk.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <fs/fs_subr.h>
#include <sys/cmn_err.h>

extern int	inside[], around[];
void delay();

/*
 * Allocate a block in the file system.
 */
/*ARGSUSED2*/
s5_alloc(ip, bnp, cr)
	register struct inode *ip;
	daddr_t *bnp;
	struct cred *cr;
{
	register struct filsys *fs;
	register struct s5vfs *s5vfsp;
	register struct vfs *vp = ip->i_vnode.v_vfsp;
	struct buf	*bp;
	daddr_t bno;

	s5vfsp = ip->i_s5vfs;
	fs = (struct filsys *)s5vfsp->vfs_fs;
	mutex_enter(&s5vfsp->vfs_lock);
	do {
		if (fs->s_nfree <= 0)
		{
			mutex_exit(&s5vfsp->vfs_lock);
			goto nospace;
		}
		if ((bno = fs->s_free[--fs->s_nfree]) == 0)
		{
			mutex_exit(&s5vfsp->vfs_lock);
			goto nospace;
		}
	} while (s5_badblock(fs, bno, vp->vfs_dev));

	if (fs->s_nfree <= 0) {
		mutex_exit(&s5vfsp->vfs_lock);
		bp = bread(vp->vfs_dev, fsbtodb(fs, bno), s5vfsp->vfs_bsize);
		mutex_enter(&s5vfsp->vfs_lock);
		if ((bp->b_flags & B_ERROR) == 0) {
			fs->s_nfree = (FBLKP(bp->b_un.b_addr))->df_nfree;
			bcopy((caddr_t)(FBLKP(bp->b_un.b_addr))->df_free,
				    (caddr_t)fs->s_free, sizeof (fs->s_free));
		}
		/* make sure write retries are also cleared */
		bp->b_flags &= ~(B_DELWRI | B_RETRYWRI);
		bp->b_flags |= B_STALE|B_AGE;
		mutex_exit(&s5vfsp->vfs_lock);
		brelse(bp);
		mutex_enter(&s5vfsp->vfs_lock);
	}
	if (fs->s_nfree <= 0 || fs->s_nfree > NICFREE) {
		mutex_exit(&s5vfsp->vfs_lock);
		printf("Bad free count on dev = %lu\n", vp->vfs_dev);
		goto nospace;
	}
	if (fs->s_tfree)
		fs->s_tfree--;
	fs->s_fmod = 1;
	mutex_exit(&s5vfsp->vfs_lock);
	if (bno > 0) {
		*bnp = bno;
		return (0);
	}
nospace:
	delay(5*hz);
	cmn_err(CE_NOTE, "s5alloc: %lu: file system full\n", vp->vfs_dev);
	return (ENOSPC);
}


/*
 * Allocate an inode in the file system.
 *
 */
s5_ialloc(pip, ipp, cr)
	register struct inode *pip;
	struct inode **ipp;
	struct cred *cr;
{
	struct inode *ip;
	register struct filsys *fs;
	struct s5vfs *s5vfsp;
	register struct vfs *vp = pip->i_vnode.v_vfsp;
	ino_t ino;
	int err;
	int i;
	daddr_t	adr;
	struct buf	*bp;
	struct dinode *dp;

	ASSERT(RW_WRITE_HELD(&pip->i_rwlock));
	fs = (struct filsys *)pip->i_fs;
	s5vfsp = pip->i_s5vfs;
loop:
	mutex_enter(&s5vfsp->vfs_lock);
	if (fs->s_tinode == 0)
		goto noinodes;
loop1:
	if (fs->s_ninode > 0 && (ino = fs->s_inode[--fs->s_ninode])) {
		mutex_exit(&s5vfsp->vfs_lock);
		if (err = s5_iget(pip->i_vnode.v_vfsp, fs, ino, &ip, cr)) {
			return (err);
		}
/*		vp = ITOV(ip); */
		rw_enter(&ip->i_contents, RW_WRITER);
		if (ip->i_mode) {
			/*
			 * Inode was allocated after all.  Look some more.
			 */
			cmn_err(CE_NOTE,
			    "ialloc: inode was already allocated\n");
			s5_iupdat(ip, 1);
			rw_exit(&ip->i_contents);
			s5_iput(ip);
			goto loop;
		}
		ip->i_size = 0;
		for (i = 0; i < NADDR; i++)
			ip->i_addr[i] = 0;
		s5_iupdat(ip, 1);
		rw_exit(&ip->i_contents);
		mutex_enter(&s5vfsp->vfs_lock);
		if (fs->s_tinode)
			fs->s_tinode--;
		fs->s_fmod = 1;
		mutex_exit(&s5vfsp->vfs_lock);
		*ipp = ip;
		return (0);
	}
	/*
	 * Only try to rebuild freelist if there are free inodes.
	 */
	if (fs->s_tinode > 0) {
		fs->s_ninode = NICINOD;
		ino = FsINOS(s5vfsp, fs->s_inode[0]);
		for (adr = FsITOD(s5vfsp, ino); adr < (daddr_t)fs->s_isize;
		    adr++) {
			mutex_exit(&s5vfsp->vfs_lock);
			bp = bread(vp->vfs_dev, adr, s5vfsp->vfs_bsize);
			mutex_enter(&s5vfsp->vfs_lock);
			if (bp->b_flags & B_ERROR) {
				brelse(bp);
				ino += s5vfsp->vfs_inopb;
				continue;
			}
			dp = (struct dinode *)bp->b_un.b_addr;
			for (i = 0; i < s5vfsp->vfs_inopb; i++, ino++, dp++) {
				if (fs->s_ninode <= 0)
					break;
				if (dp->di_mode == 0)
					fs->s_inode[--fs->s_ninode] = ino;
			}
			bdwrite(bp);
			if (fs->s_ninode <= 0)
				break;
		}
		if (fs->s_ninode > 0) {
			fs->s_inode[fs->s_ninode-1] = 0;
			fs->s_inode[0] = 0;
		}
		if (fs->s_ninode != NICINOD) {
			fs->s_ninode = NICINOD;
/*			mutex_exit(&s5vfsp->vfs_lock); */
			goto loop1;
		}
	}
noinodes:
	fs->s_ninode = 0;
	fs->s_tinode = 0;
	mutex_exit(&s5vfsp->vfs_lock);
	printf("Out of inodes on dev = %lu\n", vp->vfs_dev);
	return (ENOSPC);
}


/*
 * Free a block.
 *
 * The specified block is placed back in the free map.
 */
/*ARGSUSED2*/
void
s5_free(ip, bno, size)
	register struct inode *ip;
	daddr_t bno;
	off_t size;
{
	register struct filsys *fp;
	register struct s5vfs *s5vfsp;
	register struct buf *bp;
	register struct vfs *vp = ip->i_vnode.v_vfsp;

	s5vfsp = ip->i_s5vfs;
	fp = (struct filsys *)ip->i_fs;
	if (s5_badblock(fp, bno, vp->vfs_dev))
		return;
	mutex_enter(&s5vfsp->vfs_lock);
	if (fp->s_nfree <= 0) {
		fp->s_nfree = 1;
		fp->s_free[0] = 0;
	}
	if (fp->s_nfree >= NICFREE) {
		fp->s_flock++;
		mutex_exit(&s5vfsp->vfs_lock);
		bp = bread(vp->vfs_dev, fsbtodb(fp, bno), s5vfsp->vfs_bsize);
		mutex_enter(&s5vfsp->vfs_lock);
		(FBLKP(bp->b_un.b_addr))->df_nfree = fp->s_nfree;
		bcopy((caddr_t)fp->s_free,
		    (caddr_t)(FBLKP(bp->b_un.b_addr))->df_free,
		    sizeof (fp->s_free));
		fp->s_nfree = 0;
		mutex_exit(&s5vfsp->vfs_lock);
		bdwrite(bp);
		mutex_enter(&s5vfsp->vfs_lock);
	}

	fp->s_free[fp->s_nfree++] = bno;
	fp->s_tfree++;
	fp->s_fmod = 1;
	mutex_exit(&s5vfsp->vfs_lock);
}

/*
 * Free an inode.
 *
 * The specified inode is placed back in the free map.
 */
/*ARGSUSED2*/
void
s5_ifree(ip, ino, mode)
	struct inode *ip;
	ino_t ino;
	mode_t mode;
{
	register struct filsys *fp;

	fp = (struct filsys *)ip->i_fs;
	if (ip->i_number == ino && ip->i_mode != 0)
		cmn_err(CE_PANIC,
		    "s5_ifree: illegal mode: %u, ino %d\n",
		    ip->i_mode, ip->i_number);

	mutex_enter(&ip->i_s5vfs->vfs_lock);
	/*
	 * Update disk inode from incore slot before putting it on
	 * the freelist; this eliminates a race in the simplex code
	 * which did an ifree() and then an iupdat() in s5_iput().
	 */
	s5_iupdat(ip, 1);
	fp->s_tinode++;
	fp->s_fmod = 1;
	if (fp->s_ninode >= NICINOD || fp->s_ninode == 0) {
		if (ino < fp->s_inode[0])
			fp->s_inode[0] = ino;
	} else
		fp->s_inode[fp->s_ninode++] = ino;
	mutex_exit(&ip->i_s5vfs->vfs_lock);
}


/*
 * Check that a block number is in the range between the I list
 * and the size of the device.  This is used mainly to check that
 * a garbage file system has not been mounted.
 *
 * bad block on dev x/y -- not in range
 */
int
s5_badblock(fp, bn, dev)
	register struct filsys *fp;
	daddr_t bn;
	dev_t dev;
{
	if (bn < (daddr_t)fp->s_isize || bn >= (daddr_t)fp->s_fsize) {
		cmn_err(CE_WARN, "bad block on dev = %lu", dev);
		return (1);
	}
	return (0);
}


/*
 * Free storage space associated with the specified inode.  The portion
 * to be freed is specified by lp->l_start and lp->l_len (already
 * normalized to a "whence" of 0).
 *
 * This is an experimental facility whose continued existence is not
 * guaranteed.  Currently, we only support the special case
 * of l_len == 0, meaning free to end of file.
 *
 * Blocks are freed in reverse order.  This FILO algorithm will tend to
 * maintain a contiguous free list much longer than FIFO.
 * See also s5_itrunc() in s5_inode.c.
 *
 * Bug: unused bytes in the last retained block are not cleared.
 * This may result in a "hole" in the file that does not read as zeroes.
 */
/* ARGSUSED */
int
s5_freesp(vp, lp, flag, cr)
	register struct vnode *vp;
	register struct flock64 *lp;
	int flag;
	struct cred *cr;
{
	register int i;
	register struct inode *ip = VTOI(vp);
	int error;

	ASSERT(vp->v_type == VREG);
	ASSERT(lp->l_start >= (offset_t)0);	/* checked by convoff */

	if (lp->l_len != (offset_t)0)
		return (EINVAL);

	if (lp->l_start > MAXOFF_T)
		return (EFBIG);

	if (lp->l_end > MAXOFF_T)
		return (EFBIG);

	/*
	 * dont need to check lp->l_len because it was checked to be 0
	 * up above.
	 */

	rw_enter(&ip->i_contents, RW_READER);
	if (ip->i_size == (off_t)lp->l_start) {
		rw_exit(&ip->i_contents);
		return (0);
	}

	/*
	 * Check if there is any active mandatory lock on the
	 * range that will be truncated/expanded.
	 */
	if (MANDLOCK(vp, ip->i_mode)) {
		int save_start;

		save_start = (int)lp->l_start;

		if (ip->i_size < (off_t)lp->l_start) {
			/*
			 * "Truncate up" case: need to make sure there
			 * is no lock beyond current end-of-file. To
			 * do so, we need to set l_start to the size
			 * of the file temporarily.
			 */
			lp->l_start = (offset_t)ip->i_size;
		}
		lp->l_type = F_WRLCK;
		lp->l_sysid = 0;
		lp->l_pid = ttoproc(curthread)->p_pid;
		i = (flag & (FNDELAY|FNONBLOCK)) ? 0 : SLPFLCK;
		rw_exit(&ip->i_contents);
		if ((i = reclock(vp, lp, i, 0, lp->l_start)) != 0 ||
		    lp->l_type != F_UNLCK) {
			lp->l_start = (offset_t)save_start;
			return (i ? i : EAGAIN);
		}
		rw_enter(&ip->i_contents, RW_READER);

		lp->l_start = (offset_t)save_start;
	}


	if (vp->v_type == VREG && (error = fs_vcode(vp, &ip->i_vcode))) {
		rw_exit(&ip->i_contents);
		return (error);
	}

	/*
	 * Make sure a write isn't in progress (allocating blocks)
	 * by acquiring i_rwlock (we promised s5_bmap we wouldn't
	 * truncate while it was allocating blocks).
	 * Grab the locks in the right order.
	 */
	rw_exit(&ip->i_contents);
	rw_enter(&ip->i_rwlock, RW_WRITER);
	rw_enter(&ip->i_contents, RW_WRITER);
	error = s5_itrunc(ip, (u_long)lp->l_start, cr);
	rw_exit(&ip->i_contents);
	rw_exit(&ip->i_rwlock);
	return (error);
}
