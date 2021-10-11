/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

#pragma ident	"@(#)ufs_extvnops.c	1.13	99/07/27 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_lockfs.h>
#include <sys/fs/ufs_log.h>
#include <sys/fs/ufs_trans.h>
#include <sys/cmn_err.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <sys/fdbuffer.h>

#ifdef DEBUG
int evn_ufs_debug = 0;
#define	DEBUGF(args)	{ if (evn_ufs_debug) cmn_err args; }
#else
#define	DEBUGF(args)
#endif

/*ARGSUSED*/
int
ufs_rdwr_data(
	vnode_t *vp,
	u_offset_t offset,
	size_t len,
	fdbuffer_t *fdb,
	int flags,
	cred_t *cr)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	struct buf *bp;
	krw_t rwtype = RW_READER;
	u_offset_t o = offset;
	size_t iolen;
	int curlen, pplen;
	daddr_t bn;
	int contig = 0;
	int error;
	int n, on;
	int iswrite = flags & B_WRITE;
	int io_started = 0;
	klwp_t *lwp = ttolwp(curthread);

	fs = ufsvfsp->vfs_fs;
	iolen = len;

	DEBUGF((CE_CONT, "?ufs_rdwr: %s vp: %p pages:%p  off %llx len %lx"
	    " isize: %llx fdb: %p\n",
	    flags & B_READ ? "READ" : "WRITE", (void *)vp, (void *)vp->v_pages,
	    o, iolen, ip->i_size, (void *)fdb));

	rw_enter(&ip->i_ufsvfs->vfs_dqrwlock, RW_READER);
	rw_enter(&ip->i_contents, rwtype);

	ASSERT(o < ip->i_size);

	error = 0;
	curlen = 0;

	if ((o + iolen) > ip->i_size)
		iolen = ip->i_size - o;

	while (!error && curlen < iolen) {

		contig = 0;
		error = bmap_read(ip, o, &bn, &contig);

		ASSERT(!(bn == UFS_HOLE && iswrite));
		if (bn == UFS_HOLE) {
			if (iswrite && (rwtype == RW_READER)) {
				rwtype = RW_WRITER;
				if (!rw_tryupgrade(&ip->i_contents)) {
					rw_exit(&ip->i_contents);
					rw_enter(&ip->i_contents, rwtype);
					continue;
				}
			}
			on = blkoff(fs, o);
			pplen = roundup(len, PAGESIZE);
			n = MIN((pplen - curlen), (fs->fs_bsize - on));
			ASSERT(n > 0);
			/*
			 * We may be reading for writing.
			 */

			DEBUGF((CE_CONT, "?ufs_rdwr_data: hole %llx - %lx\n",
			    o, (iolen - curlen)));

			if (iswrite) {
				printf("**WARNING: ignoring hole in write\n");
				continue;
			} else {
				fdb_add_hole(fdb, o - offset, n);
			}
			o += n;
			curlen += n;
			continue;

		}
		ASSERT(contig > 0);
		pplen = roundup(len, PAGESIZE);

		contig = MIN(contig, len - curlen);
		contig = roundup(contig, DEV_BSIZE);

		bp = fdb_iosetup(fdb, o - offset, contig, vp, flags);

		bp->b_edev = ip->i_dev;
		bp->b_dev = cmpdev(ip->i_dev);
		bp->b_blkno = bn;

		(void) bdev_strategy(bp);
		io_started = 1;

		o += contig;
		curlen += contig;
		if (lwp != NULL) {
			if (iswrite)
				lwp->lwp_ru.oublock++;
			else
				lwp->lwp_ru.inblock++;
		}

		if ((flags & B_ASYNC) == 0) {
			error = biowait(bp);
			fdb_iodone(bp);
		}

		DEBUGF((CE_CONT, "?loop ufs_rdwr_data.. off %llx len %lx\n", o,
		    (iolen - curlen)));
	}

	DEBUGF((CE_CONT, "?ufs_rdwr_data: off %llx len %lx pages: %p ------\n",
	    o, (iolen - curlen), (void *)vp->v_pages));

	rw_exit(&ip->i_contents);
	rw_exit(&ip->i_ufsvfs->vfs_dqrwlock);

	if (flags & B_ASYNC)
		fdb_ioerrdone(fdb, error);

	if (io_started && flags & B_ASYNC)
		return (0);
	else
		return (error);
}

int
ufs_alloc_data(
	vnode_t *vp,
	u_offset_t offset,
	size_t *len,
	fdbuffer_t *fdb,
	int flags,
	cred_t *cr)
{
	struct inode *ip = VTOI(vp);
	size_t done_len, io_len;
	int contig;
	u_offset_t uoff, io_off;
	int error = 0;
	int on, n;
	daddr_t bn;
	struct fs *fs;
	struct ufsvfs *ufsvfsp = ip->i_ufsvfs;
	int i_size_changed = 0;
	u_offset_t old_i_size;
	struct ulockfs *ulp;
	int trans_size;
	int issync;
	int32_t	oblks;
	long 	secs_allocated, num_extra_secs, available_secs,
		min_sec_needed;
	int	io_started = 0;

	klwp_t *lwp = ttolwp(curthread);

	ASSERT((flags & B_WRITE) == 0);

	/*
	 * Obey the lockfs protocol
	 */
	error = ufs_lockfs_begin_getpage(ufsvfsp, &ulp, segkmap, 0, 0);
	if (error)
		goto out;

	if (ulp) {
		TRANS_TRY_BEGIN_CSYNC(ufsvfsp, issync, TOP_GETPAGE,
		    trans_size = TOP_GETPAGE_SIZE(ip), error);
		if (error == EWOULDBLOCK) {
			error = EDEADLK;
			ufs_lockfs_end(ulp);
			goto out;
		}
	}

	uoff = offset;
	io_off = offset;
	io_len = *len;
	done_len = 0;

	DEBUGF((CE_CONT, "?ufs_alloc: off %llx len %lx size %llx fdb: %p\n",
	    uoff, (io_len - done_len), ip->i_size, (void *)fdb));

	rw_enter(&ip->i_ufsvfs->vfs_dqrwlock, RW_READER);

	rw_enter(&ip->i_contents, RW_WRITER);

	ASSERT((ip->i_mode & IFMT) == IFREG);

	fs = ip->i_fs;
	oblks = ip->i_blocks;

	while (error == 0 && done_len < io_len) {
		uoff = (u_offset_t)(io_off + done_len);
		on = (int)blkoff(fs, uoff);
		n = (int)MIN(fs->fs_bsize - on, io_len - done_len);

		DEBUGF((CE_CONT, "?ufs_alloc_data: offset: %llx len %x\n",
		    uoff, n));

		if (uoff + n > ip->i_size) {
			/*
			 * We are extending the length of the file.
			 * bmap is used so that we are sure that
			 * if we need to allocate new blocks, that it
			 * is done here before we up the file size.
			 */

			DEBUGF((CE_CONT, "?ufs_alloc_data: grow %llx -> %llx\n",
			    ip->i_size, uoff + n));

			error = bmap_write(ip, uoff, (on + n), 1, cr);

			if (error) {
				DEBUGF((CE_CONT, "?ufs_alloc_data: grow "
				    "failed err: %d\n", error));
				break;
			}

			if (fdb) {
				if (uoff >= ip->i_size) {
					fdb_add_hole(fdb, uoff - offset, n);
				} else {
					int contig;
					buf_t *bp;

					error = bmap_read(ip, uoff, &bn,
					    &contig);

					if (error) {
						break;
					}
					contig = ip->i_size - uoff;
					contig = roundup(contig, DEV_BSIZE);

					bp = fdb_iosetup(fdb, uoff - offset,
					    contig, vp, flags);

					bp->b_edev = ip->i_dev;
					bp->b_dev = cmpdev(ip->i_dev);
					bp->b_blkno = bn;

					(void) bdev_strategy(bp);
					io_started = 1;

					if (lwp != NULL)
						lwp->lwp_ru.oublock++;

					if ((flags & B_ASYNC) == 0) {
						error = biowait(bp);
						fdb_iodone(bp);
						if (error)
							break;
					}
					if (contig > (ip->i_size - uoff)) {
						contig -= ip->i_size - uoff;
						fdb_add_hole(fdb,
						    ip->i_size - offset,
						    contig);
					}
				}
			}

			i_size_changed = 1;
			old_i_size = ip->i_size;
			UFS_SET_ISIZE(uoff + n, ip);
			TRANS_INODE(ip->i_ufsvfs, ip);
			/*
			 * file has grown larger than 2GB. Set flag
			 * in superblock to indicate this, if it
			 * is not already set.
			 */
			if ((ip->i_size > MAXOFF32_T) &&
			    !(fs->fs_flags & FSLARGEFILES)) {
				ASSERT(ufsvfsp->vfs_lfflags & UFS_LARGEFILES);
				mutex_enter(&ufsvfsp->vfs_lock);
				fs->fs_flags |= FSLARGEFILES;
				ufs_sbwrite(ufsvfsp);
				mutex_exit(&ufsvfsp->vfs_lock);
			}
		} else {
			/*
			 * We have to allocate blocks for the hole (if
			 * there is any in fact) we go ahead and check
			 * to see if that is in fact the case.
			 */

			error = bmap_read(ip, uoff, &bn, &contig);

			if (error) {
				DEBUGF((CE_CONT, "?ufs_alloc_data: "
				    "bmap_read err: %d\n", error));
				break;
			}

			if (bn != UFS_HOLE) {
				int contig = roundup(n, DEV_BSIZE);
				buf_t *bp;

				if (fdb) {
					bp = fdb_iosetup(fdb, uoff - offset,
					    contig, vp, flags);

					bp->b_edev = ip->i_dev;
					bp->b_dev = cmpdev(ip->i_dev);
					bp->b_blkno = bn;

					(void) bdev_strategy(bp);
					io_started = 1;

					if (lwp != NULL)
						lwp->lwp_ru.oublock++;

					if ((flags & B_ASYNC) == 0) {
						error = biowait(bp);
						fdb_iodone(bp);
						if (error)
							break;
					}
				}
			} else {

				error = bmap_write(ip, uoff, (on + n), 1, cr);

				if (error) {
					DEBUGF((CE_CONT, "?ufs_alloc_data: fill"
					    " hole failed error: %d\n", error));
					break;
				}
				if (fdb) {
					fdb_add_hole(fdb, uoff - offset, n);
				}
			}
		}
		done_len += n;
	}

	if (error) {
		if (i_size_changed) {
			/*
			 * Allocation of the blocks for the file failed.
			 * so trucate the file size back.
			 */
			(void) ufs_itrunc(ip, old_i_size, 0, cr);
		}
	}

	DEBUGF((CE_CONT, "?ufs_alloc: uoff %llx len %lx\n",
	    uoff, (io_len - done_len)));

	secs_allocated = ip->i_blocks - oblks;
	min_sec_needed = (ip->i_size - roundup(old_i_size, 512))/512;

	num_extra_secs = MAX(secs_allocated, min_sec_needed);
	available_secs = MIN(num_extra_secs, fs->fs_bsize/512);

	if (available_secs)
		*len = roundup(ip->i_size, available_secs * 512);
	else
		*len = roundup(ip->i_size, 512);

	/*
	 * Flush cached pages.
	 */
	(void) VOP_PUTPAGE(vp, 0, 0, B_INVAL, cr);

	rw_exit(&ip->i_contents);
	rw_exit(&ip->i_ufsvfs->vfs_dqrwlock);
	if (fdb && flags & B_ASYNC)
		fdb_ioerrdone(fdb, error);

	if (ulp) {
		TRANS_END_CSYNC(ufsvfsp, error, issync, TOP_GETPAGE,
		    trans_size);
		ufs_lockfs_end(ulp);
	}
out:
	if (io_started && flags & B_ASYNC)
		return (0);
	else
		return (error);
}
