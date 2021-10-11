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
 */

/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ufs_directio.c	1.21	99/03/07 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/dnlc.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/filio.h>

#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_lockfs.h>
#include <sys/fs/ufs_filio.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_quota.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_panic.h>
#include <sys/dirent.h>		/* must be AFTER <sys/fs/fsdir.h>! */
#include <sys/errno.h>

#include <sys/filio.h>		/* _FIOIO */

#include <vm/hat.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kmem.h>
#include <vm/rm.h>
#include <sys/swap.h>
#include <sys/epm.h>

#include <fs/fs_subr.h>

extern int	ufs_trans_directio;	/* see ufs_trans.c */

static void	*ufs_directio_zero_buf;
static int	ufs_directio_zero_len	= 8192;

int	ufs_directio_enabled = 1;	/* feature is enabled */

/*
 * for kstats reader
 */
struct ufs_directio_kstats {
	uint_t	logical_reads;
	uint_t	phys_reads;
	uint_t	hole_reads;
	uint_t	nread;
	uint_t	logical_writes;
	uint_t	phys_writes;
	uint_t	nwritten;
	uint_t	nflushes;
} ufs_directio_kstats;

kstat_t	*ufs_directio_kstatsp;

/*
 * use kmem_cache_create for direct-physio buffers. This has shown
 * a better cache distribution compared to buffers on the
 * stack. It also avoids semaphore construction/deconstruction
 * per request
 */
struct directio_buf {
	struct directio_buf	*next;
	char		*addr;
	size_t		nbytes;
	struct buf	buf;
};
static struct kmem_cache *directio_buf_cache;


/* ARGSUSED */
static int
directio_buf_constructor(void *dbp, void *cdrarg, int kmflags)
{
	bioinit((struct buf *)&((struct directio_buf *)dbp)->buf);
	return (0);
}

/* ARGSUSED */
static void
directio_buf_destructor(void *dbp, void *cdrarg)
{
	biofini((struct buf *)&((struct directio_buf *)dbp)->buf);
}

void
directio_bufs_init(void)
{
	directio_buf_cache = kmem_cache_create("directio_buf_cache",
		sizeof (struct directio_buf), 0,
		directio_buf_constructor, directio_buf_destructor,
		NULL, NULL, NULL, 0);
}

void
ufs_directio_init()
{
	/*
	 * kstats
	 */
	ufs_directio_kstatsp = kstat_create("ufs directio", 0,
			"UFS DirectIO Stats", "ufs directio",
			KSTAT_TYPE_RAW, sizeof (ufs_directio_kstats),
			KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE);
	if (ufs_directio_kstatsp) {
		ufs_directio_kstatsp->ks_data = (void *)&ufs_directio_kstats;
		kstat_install(ufs_directio_kstatsp);
	}
	/*
	 * kzero is broken so we have to use a private buf of zeroes
	 */
	ufs_directio_zero_buf = kmem_zalloc(ufs_directio_zero_len, KM_SLEEP);
	directio_bufs_init();
}

/*
 * Wait for the first direct IO operation to finish
 */
static int
directio_wait_one(
	struct as *as,
	struct directio_buf *dbp,
	long *bytes_iop,
	enum seg_rw rw)
{
	buf_t	*bp;
	int	error;
	/*
	 * Wait for IO to finish
	 */
	bp = &dbp->buf;
	error = biowait(bp);

	/*
	 * bytes_io will be used to figure out a resid
	 * for the caller. The resid is approximated by reporting
	 * the bytes following the first failed IO as the residual.
	 *
	 * I am cautious about using b_resid because I
	 * am not sure how well the disk drivers maintain it.
	 */
	if (error)
		if (bp->b_resid)
			*bytes_iop = bp->b_bcount - bp->b_resid;
		else
			*bytes_iop = 0;
	else
		*bytes_iop += bp->b_bcount;
	/*
	 * Release VM resources
	 */
	as_pageunlock(as, bp->b_shadow, dbp->addr, dbp->nbytes, rw);

	/*
	 * Release direct IO resources
	 */
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_SHADOW);
	kmem_cache_free(directio_buf_cache, dbp);
	return (error);
}

/*
 * Wait for all of the direct IO operations to finish
 */
static int
directio_wait(
	struct as *as,
	struct directio_buf *tail,
	long *bytes_iop,
	enum seg_rw rw)
{
	int	error = 0, newerror;
	struct directio_buf	*dbp;

	/*
	 * The linked list of directio buf structures is maintained
	 * in reverse order (tail->last request->penultimate request->...)
	 */
	while ((dbp = tail) != NULL) {
		tail = dbp->next;
		newerror = directio_wait_one(as, dbp, bytes_iop, rw);
		if (error == 0)
			error = newerror;
	}
	return (error);
}
/*
 * Initiate direct IO request
 */
static int
directio_start(
	struct ufsvfs *ufsvfsp,
	dev_t dev,
	uint_t total_bytes,
	uint_t maxcontig_bytes,
	offset_t offset,
	char *addr,
	enum seg_rw rw,
	struct proc *procp,
	struct as *as,
	struct directio_buf **tailp,
	long *bytes_iop)
{
	size_t nbytes;
	int error = 0;
	buf_t *bp;
	page_t **pplist;
	struct directio_buf *dbp;
	klwp_t *lwp = ttolwp(curthread);

	/*
	 * Kick off IO requests
	 */
	while (total_bytes && error == 0) {
		/*
		 * Limit to user-tunable fs_maxcontig
		 */
		nbytes = (size_t)MIN(total_bytes, maxcontig_bytes);

		/*
		 * Wire down pages
		 */
		error = as_pagelock(as, &pplist, addr, nbytes, rw);

		/*
		 * Can't safely wire down more pages; finish
		 * a previous request and try again.
		 */
		if (error == ENOMEM && *tailp != NULL) {
			dbp = *tailp;
			if (dbp) {
				*tailp = dbp->next;
				error = directio_wait_one(as,
						dbp, bytes_iop, rw);
			}
			continue;
		}
		if (error)
			break;

		/*
		 * Allocate a directio buf header
		 *   Note - list is maintained in reverse order.
		 *   directio_wait_one() depends on this fact when
		 *   adjusting the ``bytes_io'' param. bytes_io
		 *   is used to compute a residual in the case of error.
		 */
		dbp = kmem_cache_alloc(directio_buf_cache, KM_SLEEP);
		dbp->next = *tailp;
		*tailp = dbp;

		/*
		 * Initialize buf header
		 */
		dbp->addr = addr;
		dbp->nbytes = nbytes;
		bp = &dbp->buf;
		bp->b_edev = dev;
		bp->b_lblkno = btodt(offset);
		bp->b_bcount = nbytes;
		bp->b_un.b_addr = addr;
		bp->b_proc = procp;

		/*
		 * Note that S_WRITE implies B_READ and vice versa: a read(2)
		 * will B_READ data from the filesystem and S_WRITE it into
		 * the user's buffer; a write(2) will S_READ data from the
		 * user's buffer and B_WRITE it to the filesystem.
		 */
		if (rw == S_WRITE) {
			bp->b_flags = B_KERNBUF | B_BUSY | B_PHYS | B_READ;
			ufs_directio_kstats.phys_reads++;
			ufs_directio_kstats.nread += nbytes;
		} else {
			bp->b_flags = B_KERNBUF | B_BUSY | B_PHYS | B_WRITE;
			ufs_directio_kstats.phys_writes++;
			ufs_directio_kstats.nwritten += nbytes;
		}
		bp->b_shadow = pplist;
		if (pplist != NULL)
			bp->b_flags |= B_SHADOW;

		/*
		 * Issue I/O request.
		 */
		ufsvfsp->vfs_iotstamp = lbolt;
		(void) bdev_strategy(bp);

		/*
		 * Adjust pointers and counters
		 */
		addr += nbytes;
		offset += nbytes;
		total_bytes -= nbytes;
		if (lwp != NULL) {
			if (rw == S_WRITE)
				lwp->lwp_ru.oublock++;
			else
				lwp->lwp_ru.inblock++;
		}
	}
	return (error);
}

/*
 * Direct Write
 */
int
ufs_directio_write(struct inode *ip, uio_t *uio, cred_t *cr, int *statusp)
{
	long		resid, bytes_written;
	u_offset_t	size, uoff;
	rlim64_t	limit = uio->uio_llimit;
	int		on, n, error, newerror, len, has_holes;
	daddr_t		bn;
	size_t		nbytes;
	struct fs	*fs;
	vnode_t		*vp;
	iovec_t		*iov;
	struct ufsvfs	*ufsvfsp = ip->i_ufsvfs;
	struct proc	*procp;
	struct as	*as;
	struct directio_buf	*tail;

	/*
	 * assume that directio isn't possible (normal case)
	 */
	*statusp = DIRECTIO_FAILURE;

	/*
	 * Don't go direct
	 */
	if (ufs_directio_enabled == 0)
		return (0);

	/*
	 * mapped file; nevermind
	 */
	if (ip->i_mapcnt)
		return (0);

	/*
	 * not on old Logging UFS
	 */
	if (ufsvfsp->vfs_log == NULL &&
	    ufs_trans_directio == 0 && TRANS_ISTRANS(ufsvfsp))
		return (0);

	/*
	 * CAN WE DO DIRECT IO?
	 */
	uoff = uio->uio_loffset;
	resid = uio->uio_resid;

	/*
	 * beyond limit
	 */
	if (uoff + resid > limit)
		return (0);

	/*
	 * must be sector aligned
	 */
	if ((uoff & (u_offset_t)(DEV_BSIZE - 1)) || (resid & (DEV_BSIZE - 1)))
		return (0);

	/*
	 * must be short aligned and sector aligned
	 */
	iov = uio->uio_iov;
	nbytes = uio->uio_iovcnt;
	while (nbytes--) {
		if (((uint_t)iov->iov_len & (DEV_BSIZE - 1)) != 0)
			return (0);
		if ((intptr_t)(iov++->iov_base) & 1)
			return (0);
	}

	/*
	 * SHOULD WE DO DIRECT IO?
	 */
	size = ip->i_size;
	has_holes = -1;

	/*
	 * only on regular files; no metadata
	 */
	if (((ip->i_mode & IFMT) != IFREG) || ip->i_ufsvfs->vfs_qinod == ip)
		return (0);

	/*
	 * Synchronous, allocating writes run very slow in Direct-Mode
	 * 	XXX - can be fixed with bmap_write changes for large writes!!!
	 *	XXX - can be fixed for updates to "almost-full" files
	 *	XXX - WARNING - system hangs if bmap_write() has to
	 * 			allocate lots of pages since pageout
	 * 			suspends on locked inode
	 */
	if (ip->i_flag & ISYNC) {
		if ((uoff + resid) > size)
			return (0);
		has_holes = bmap_has_holes(ip);
		if (has_holes)
			return (0);
	}

	/*
	 * DIRECTIO
	 */

	fs = ip->i_fs;
	/*
	 * allocate space
	 */
	do {
		on = (int)blkoff(fs, uoff);
		n = (int)MIN(fs->fs_bsize - on, resid);
		if ((uoff + n) > ip->i_size) {
			error = bmap_write(ip, uoff, (int)(on + n),
				    (int)(uoff & (offset_t)MAXBOFFSET) == 0,
			    cr);
			if (error)
				break;
			ip->i_size = uoff + n;
			ip->i_flag |= IATTCHG;
		} else if (n == MAXBSIZE) {
			error = bmap_write(ip, uoff, (int)(on + n), 1, cr);
		} else {
			if (has_holes < 0)
				has_holes = bmap_has_holes(ip);
			if (has_holes) {
				uint_t	blk_size;
				u_offset_t offset;

				offset = uoff & (offset_t)fs->fs_bmask;
				blk_size = (int)blksize(fs, ip,
				    (daddr_t)lblkno(fs, offset));
				error = bmap_write(ip, uoff, blk_size, 0, cr);
			} else
				error = 0;
		}
		if (error)
			break;
		uoff += n;
		resid -= n;
		/*
		 * if file has grown larger than 2GB, set flag
		 * in superblock if not already set
		 */
		if ((ip->i_size > MAXOFF32_T) &&
		    !(fs->fs_flags & FSLARGEFILES)) {
			ASSERT(ufsvfsp->vfs_lfflags & UFS_LARGEFILES);
			mutex_enter(&ufsvfsp->vfs_lock);
			fs->fs_flags |= FSLARGEFILES;
			ufs_sbwrite(ufsvfsp);
			mutex_exit(&ufsvfsp->vfs_lock);
		}
	} while (resid);

	if (error) {
		/*
		 * restore original state
		 */
		if (resid) {
			if (size == ip->i_size)
				return (0);
			(void) ufs_itrunc(ip, size, 0, cr);
		}
		/*
		 * try non-directio path
		 */
		return (0);
	}

	/*
	 * get rid of cached pages
	 */
	vp = ITOV(ip);
	if (vp->v_pages != NULL) {
		(void) VOP_PUTPAGE(vp, (offset_t)0, (size_t)0, B_INVAL, cr);
		ufs_directio_kstats.nflushes++;
	}
	if (vp->v_pages)
		return (0);

	/*
	 * Direct Writes
	 */

	/*
	 * proc and as are for VM operations in directio_start()
	 */
	if (uio->uio_segflg == UIO_USERSPACE) {
		procp = ttoproc(curthread);
		as = procp->p_as;
	} else {
		procp = NULL;
		as = &kas;
	}
	*statusp = DIRECTIO_SUCCESS;
	error = 0;
	resid = uio->uio_resid;
	tail = NULL;
	bytes_written = 0;
	ufs_directio_kstats.logical_writes++;
	while (error == 0 && resid && uio->uio_iovcnt) {
		/*
		 * Adjust number of bytes
		 */
		uoff = uio->uio_loffset;
		iov = uio->uio_iov;
		nbytes = (size_t)MIN(iov->iov_len, resid);
		if (nbytes == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}

		/*
		 * Re-adjust number of bytes to contiguous range
		 */
		len = (ssize_t)blkroundup(fs, nbytes);
		error = bmap_read(ip, uoff, &bn, &len);
		if (error)
			break;
		if (bn == UFS_HOLE || len == 0)
			break;
		nbytes = (size_t)MIN(nbytes, len);

		/*
		 * Kick off one or more direct write requests
		 */
		error = directio_start(ufsvfsp, ip->i_dev, nbytes,
			    ufsvfsp->vfs_wrclustsz, ldbtob(bn),
			    iov->iov_base, S_READ,
			    procp, as, &tail, &bytes_written);
		if (error)
			break;

		/*
		 * Adjust pointers and counters
		 */
		iov->iov_len -= nbytes;
		iov->iov_base += nbytes;
		uio->uio_loffset += nbytes;
		resid -= nbytes;
	}
	/*
	 * Wait for outstanding requests
	 */
	newerror = directio_wait(as, tail, &bytes_written, S_READ);

	/*
	 * If error, adjust resid to begin at the first
	 * un-writable byte.
	 */
	if (error == 0)
		error = newerror;
	if (error)
		resid = uio->uio_resid - bytes_written;
	uio->uio_resid = resid;

	/*
	 * If there is a residual; adjust the EOF if necessary
	 */
	ip->i_flag |= IUPD | ICHG;
	TRANS_INODE(ip->i_ufsvfs, ip);
	if (resid) {
		if (size == ip->i_size)
			return (error);
		if (uio->uio_loffset > size)
			size = uio->uio_loffset;
		(void) ufs_itrunc(ip, size, 0, cr);
	}
	return (error);
}
/*
 * Direct read of a hole
 */
static int
directio_hole(struct uio *uio, size_t nbytes)
{
	int		error = 0, nzero;
	uio_t		phys_uio;
	iovec_t		phys_iov;

	ufs_directio_kstats.hole_reads++;
	ufs_directio_kstats.nread += nbytes;

	phys_iov.iov_base = uio->uio_iov->iov_base;
	phys_iov.iov_len = nbytes;

	phys_uio.uio_iov = &phys_iov;
	phys_uio.uio_iovcnt = 1;
	phys_uio.uio_resid = phys_iov.iov_len;
	phys_uio.uio_segflg = uio->uio_segflg;
	while (error == 0 && phys_uio.uio_resid) {
		nzero = (int)MIN(phys_iov.iov_len, ufs_directio_zero_len);
		error = uiomove(ufs_directio_zero_buf, nzero, UIO_READ,
				&phys_uio);
	}
	return (error);
}

/*
 * Direct Read
 */
int
ufs_directio_read(struct inode *ip, uio_t *uio, cred_t *cr, int *statusp)
{
	ssize_t		resid, bytes_read;
	u_offset_t	size, uoff;
	int		error, newerror, len;
	size_t		nbytes;
	struct fs	*fs;
	vnode_t		*vp;
	daddr_t		bn;
	iovec_t		*iov;
	struct ufsvfs	*ufsvfsp = ip->i_ufsvfs;
	struct proc	*procp;
	struct as	*as;
	struct directio_buf	*tail;

	/*
	 * assume that directio isn't possible (normal case)
	 */
	*statusp = DIRECTIO_FAILURE;

	/*
	 * Don't go direct
	 */
	if (ufs_directio_enabled == 0)
		return (0);

	/*
	 * mapped file; nevermind
	 */
	if (ip->i_mapcnt)
		return (0);

	/*
	 * CAN WE DO DIRECT IO?
	 */
	/*
	 * must be sector aligned
	 */
	uoff = uio->uio_loffset;
	resid = uio->uio_resid;
	if ((uoff & (u_offset_t)(DEV_BSIZE - 1)) || (resid & (DEV_BSIZE - 1)))
		return (0);
	/*
	 * must be short aligned and sector aligned
	 */
	iov = uio->uio_iov;
	nbytes = uio->uio_iovcnt;
	while (nbytes--) {
		if (((size_t)iov->iov_len & (DEV_BSIZE - 1)) != 0)
			return (0);
		if ((intptr_t)(iov++->iov_base) & 1)
			return (0);
	}

	/*
	 * DIRECTIO
	 */
	fs = ip->i_fs;

	/*
	 * don't read past EOF
	 */
	size = ip->i_size;

	/*
	 * The file offset is past EOF so bail out here; we don't want
	 * to update uio_resid and make it look like we read something.
	 * We say that direct I/O was a success to avoid having rdip()
	 * go through the same "read past EOF logic".
	 */
	if (uoff >= size) {
		*statusp = DIRECTIO_SUCCESS;
		return (0);
	}

	/*
	 * The read would extend past EOF so make it smaller.
	 */
	if ((uoff + resid) > size) {
		resid = size - uoff;
		/*
		 * recheck sector alignment
		 */
		if (resid & (DEV_BSIZE - 1))
			return (0);
	}

	/*
	 * At this point, we know there is some real work to do.
	 */
	ASSERT(resid);

	/*
	 * get rid of cached pages
	 */
	vp = ITOV(ip);
	if (vp->v_pages != NULL) {
		rw_exit(&ip->i_contents);
		rw_enter(&ip->i_contents, RW_WRITER);
		(void) VOP_PUTPAGE(vp, (offset_t)0, (size_t)0, B_INVAL, cr);
		ufs_directio_kstats.nflushes++;
	}
	if (vp->v_pages)
		return (0);
	/*
	 * Direct Reads
	 */

	/*
	 * proc and as are for VM operations in directio_start()
	 */
	if (uio->uio_segflg == UIO_USERSPACE) {
		procp = ttoproc(curthread);
		as = procp->p_as;
	} else {
		procp = NULL;
		as = &kas;
	}

	*statusp = DIRECTIO_SUCCESS;
	error = 0;
	bytes_read = 0;
	tail = NULL;
	ufs_directio_kstats.logical_reads++;
	while (error == 0 && resid && uio->uio_iovcnt) {
		/*
		 * Adjust number of bytes
		 */
		uoff = uio->uio_loffset;
		iov = uio->uio_iov;
		nbytes = (size_t)MIN(iov->iov_len, resid);
		if (nbytes == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}

		/*
		 * Re-adjust number of bytes to contiguous range
		 */
		len = (ssize_t)blkroundup(fs, nbytes);
		error = bmap_read(ip, uoff, &bn, &len);
		if (error)
			break;

		if (bn == UFS_HOLE) {
			nbytes = (size_t)MIN(
				fs->fs_bsize - (long)blkoff(fs, uoff), nbytes);
			error = directio_hole(uio, nbytes);
			/*
			 * Hole reads are not added to the list processed
			 * by directio_wait() below so account for bytes
			 * read here.
			 */
			if (!error)
				bytes_read += nbytes;
		} else {
			nbytes = (size_t)MIN(nbytes, len);
			/*
			 * Kick off one or more direct read requests
			 */
			error = directio_start(ufsvfsp, ip->i_dev, nbytes,
				    ufsvfsp->vfs_rdclustsz, ldbtob(bn),
				    iov->iov_base, S_WRITE,
				    procp, as, &tail, &bytes_read);
		}
		/*
		 * Adjust pointers and counters
		 */
		iov->iov_len -= nbytes;
		iov->iov_base += nbytes;
		uio->uio_loffset += nbytes;
		resid -= nbytes;
	}
	/*
	 * Wait for outstanding requests
	 */
	newerror = directio_wait(as, tail, &bytes_read, S_WRITE);

	/*
	 * If error, adjust resid to begin at the first
	 * un-read byte.
	 */
	if (error == 0)
		error = newerror;
	uio->uio_resid -= bytes_read;
	return (error);
}
