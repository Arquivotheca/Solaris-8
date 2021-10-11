/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hsfs_vnops.c	1.72	99/10/22 SMI"

/*
 * Vnode operations for the High Sierra filesystem
 */

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
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>

#include <sys/dirent.h>
#include <sys/errno.h>

#include <vm/hat.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_kmem.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <vm/page.h>
#include <sys/swap.h>

#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/hsfs_spec.h>
#include <sys/fs/hsfs_node.h>
#include <sys/fs/hsfs_impl.h>
#include <sys/fs/hsfs_susp.h>
#include <sys/fs/hsfs_rrip.h>

#include <fs/fs_subr.h>

#define	ISVDEV(t) ((t) == VCHR || (t) == VBLK || (t) == VFIFO)

/* ARGSUSED */
static int
hsfs_fsync(vnode_t *cp, int syncflag, cred_t *cred)
{
	return (0);
}


/*ARGSUSED*/
static int
hsfs_read(struct vnode *vp, struct uio *uiop, int ioflag, struct cred *cred)
{
	struct hsnode *hp;
	register ulong_t off;
	register long mapon, on;
	register caddr_t base;
	uint_t	filesize;
	long nbytes, n;
	uint_t flags;
	int error;

	hp = VTOH(vp);
	/*
	 * if vp is of type VDIR, make sure dirent
	 * is filled up with all info (because of ptbl)
	 */
	if (vp->v_type == VDIR) {
		if (hp->hs_dirent.ext_size == 0)
			hs_filldirent(vp, &hp->hs_dirent);
	}
	filesize = hp->hs_dirent.ext_size;

	if (uiop->uio_loffset >= MAXOFF_T) {
		error = 0;
		goto out;
	}

	if (uiop->uio_offset >= filesize) {
		error = 0;
		goto out;
	}

	do {
		/* map file to correct page boundary */
		off = uiop->uio_offset & MAXBMASK;
		mapon = uiop->uio_offset & MAXBOFFSET;

		/* set read in data size */
		on = (uiop->uio_offset) & PAGEOFFSET;
		nbytes = MIN(PAGESIZE - on, uiop->uio_resid);
		/* adjust down if > EOF */
		n = MIN((filesize - uiop->uio_offset), nbytes);
		if (n == 0) {
			error = 0;
			goto out;
		}

		/* map the file into memory */
		base = segmap_getmap(segkmap, vp, (u_offset_t)off);
		error = uiomove(base+mapon, (size_t)n, UIO_READ, uiop);
		if (error == 0) {
			/*
			 * if read a whole block, or read to eof,
			 *  won't need this buffer again soon.
			 */
			if (n + on == PAGESIZE ||
			    uiop->uio_offset == filesize)
				flags = SM_DONTNEED;
			else
				flags = 0;
			error = segmap_release(segkmap, base, flags);
		} else
			(void) segmap_release(segkmap, base, 0);

	} while (error == 0 && uiop->uio_resid > 0);

out:
	return (error);
}

/*ARGSUSED2*/
static int
hsfs_getattr(
	struct vnode *vp,
	struct vattr *vap,
	int flags,
	struct cred *cred)
{
	register struct hsnode *hp;
	register struct vfs *vfsp;
	struct hsfs *fsp;

	hp = VTOH(vp);
	fsp = VFS_TO_HSFS(vp->v_vfsp);
	vfsp = vp->v_vfsp;

	if ((hp->hs_dirent.ext_size == 0) && (vp->v_type == VDIR)) {
		hs_filldirent(vp, &hp->hs_dirent);
	}
	vap->va_type = IFTOVT(hp->hs_dirent.mode);
	vap->va_mode = hp->hs_dirent.mode;
	vap->va_uid = hp->hs_dirent.uid;
	vap->va_gid = hp->hs_dirent.gid;

	vap->va_fsid = expldev((dev32_t)(vfsp->vfs_fsid.val[0]));
	vap->va_nodeid = (ino64_t)hp->hs_nodeid;
	vap->va_nlink = hp->hs_dirent.nlink;
	vap->va_size =	(offset_t)hp->hs_dirent.ext_size;

	vap->va_atime.tv_sec = hp->hs_dirent.adate.tv_sec;
	vap->va_atime.tv_nsec = hp->hs_dirent.adate.tv_usec*1000;
	vap->va_mtime.tv_sec = hp->hs_dirent.mdate.tv_sec;
	vap->va_mtime.tv_nsec = hp->hs_dirent.mdate.tv_usec*1000;
	vap->va_ctime.tv_sec = hp->hs_dirent.cdate.tv_sec;
	vap->va_ctime.tv_nsec = hp->hs_dirent.cdate.tv_usec*1000;
	if (vp->v_type == VCHR || vp->v_type == VBLK)
		vap->va_rdev = hp->hs_dirent.r_dev;
	else
		vap->va_rdev = 0;
	vap->va_blksize = vfsp->vfs_bsize;
	/* no. of blocks = no. of data blocks + no. of xar blocks */
	vap->va_nblocks = (fsblkcnt64_t)howmany(vap->va_size + (u_longlong_t)
	    (hp->hs_dirent.xar_len << fsp->hsfs_vol.lbn_shift), DEV_BSIZE);
	vap->va_vcode = hp->hs_vcode;
	return (0);
}

/*ARGSUSED*/
static int
hsfs_readlink(struct vnode *vp, struct uio *uiop, struct cred *cred)
{
	struct hsnode *hp;

	if (vp->v_type != VLNK)
		return (EINVAL);

	hp = VTOH(vp);

	if (hp->hs_dirent.sym_link == (char *)NULL)
		return (ENOENT);

	return (uiomove(hp->hs_dirent.sym_link,
	    (size_t)MIN(hp->hs_dirent.ext_size,
	    uiop->uio_resid), UIO_READ, uiop));
}

/*ARGSUSED*/
static void
hsfs_inactive(struct vnode *vp, struct cred *cred)
{
	register struct hsnode *hp;
	register struct hstable *htp;

	int nopage;

	hp = VTOH(vp);
	htp = ((struct hsfs *)VFS_TO_HSFS(vp->v_vfsp))->hsfs_hstbl;
	/*
	 * Note: acquiring and holding v_lock for quite a while
	 * here serializes on the vnode; this is unfortunate, but
	 * likely not to overly impact performance, as the underlying
	 * device (CDROM drive) is quite slow.
	 */
	rw_enter(&htp->hshash_lock, RW_WRITER);
	mutex_enter(&hp->hs_contents_lock);
	mutex_enter(&vp->v_lock);

	if (vp->v_count < 1)
		cmn_err(CE_PANIC, "hsfs_inactive: v_count < 1");

	if (vp->v_count > 1 || (hp->hs_flags & HREF) == 0) {
		vp->v_count--;	/* release hold from vn_rele */
		mutex_exit(&vp->v_lock);
		mutex_exit(&hp->hs_contents_lock);
		rw_exit(&htp->hshash_lock);
		return;
	}
	vp->v_count--;	/* release hold from vn_rele */
	if (vp->v_count == 0) {
		/* proceed to free the hsnode */

		/*
		 * if there is no pages associated with the
		 * hsnode, put at front of the free queue,
		 * else put at the end
		 */
		nopage = vp->v_pages == NULL? 1 : 0;
		hs_freenode(VTOH(vp), vp->v_vfsp, nopage);
		hp->hs_flags = 0;
	}
	mutex_exit(&vp->v_lock);
	mutex_exit(&hp->hs_contents_lock);
	rw_exit(&htp->hshash_lock);
}


/*ARGSUSED*/
static int
hsfs_lookup(
	struct vnode *dvp,
	char *nm,
	struct vnode **vpp,
	struct pathname *pnp,
	int flags,
	struct vnode *rdir,
	struct cred *cred)
{
	int namelen = (int)strlen(nm);

	/*
	 * If we're looking for ourself, life is simple.
	 */
	if (namelen == 1 && *nm == '.') {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	}

	return (hs_dirlook(dvp, nm, namelen, vpp, cred));
}


/*ARGSUSED*/
static int
hsfs_readdir(
	struct vnode	*vp,
	struct uio	*uiop,
	struct cred	*cred,
	int		*eofp)
{
	struct hsnode	*dhp;
	struct hsfs	*fsp;
	struct hs_direntry hd;
	register struct dirent64	*nd;
	int	error;
	register uint_t	offset;		/* real offset in directory */
	uint_t		dirsiz;		/* real size of directory */
	uchar_t		*blkp;
	register int	hdlen;		/* length of hs directory entry */
	register long	ndlen;		/* length of dirent entry */
	int		bytes_wanted;
	size_t		bufsize;	/* size of dirent buffer */
	register char	*outbuf;	/* ptr to dirent buffer */
	char		*dname;
	int		dnamelen;
	size_t		dname_size;
	struct fbuf	*fbp;
	uint_t		last_offset;	/* last index into current dir block */
	ulong_t		dir_lbn;	/* lbn of directory */
	ino_t		dirino;	/* temporary storage before storing in dirent */
	off_t		diroff;

	dhp = VTOH(vp);
	fsp = VFS_TO_HSFS(vp->v_vfsp);
	if (dhp->hs_dirent.ext_size == 0)
		hs_filldirent(vp, &dhp->hs_dirent);
	dirsiz = dhp->hs_dirent.ext_size;
	dir_lbn = dhp->hs_dirent.ext_lbn;
	if (uiop->uio_loffset >= dirsiz) {	/* at or beyond EOF */
		if (eofp)
			*eofp = 1;
		return (0);
	}
	ASSERT(uiop->uio_loffset <= MAXOFF_T);
	offset = (uint_t)uiop->uio_offset;

	dname_size = fsp->hsfs_namemax + 1;	/* 1 for the ending NUL */
	dname = kmem_alloc(dname_size, KM_SLEEP);
	bufsize = uiop->uio_resid + sizeof (struct dirent64);

	outbuf = kmem_alloc(bufsize, KM_SLEEP);
	nd = (struct dirent64 *)outbuf;

	while (offset < dirsiz) {
		if ((offset & MAXBMASK) + MAXBSIZE > dirsiz)
			bytes_wanted = dirsiz - (offset & MAXBMASK);
		else
			bytes_wanted = MAXBSIZE;

		error = fbread(vp, (offset_t)(offset & MAXBMASK),
			(unsigned int)bytes_wanted, S_READ, &fbp);
		if (error)
			goto done;

		blkp = (uchar_t *)fbp->fb_addr;
		last_offset = (offset & MAXBMASK) + fbp->fb_count - 1;

#define	rel_offset(offset) ((offset) & MAXBOFFSET)	/* index into blkp */

		while (offset < last_offset) {
			/*
			 * Directory Entries cannot span sectors.
			 * Unused bytes at the end of each sector are zeroed.
			 * Therefore, detect this condition when the size
			 * field of the directory entry is zero.
			 */
			hdlen = (int)((uchar_t)
				HDE_DIR_LEN(&blkp[rel_offset(offset)]));
			if (hdlen == 0) {
				/* advance to next sector boundary */
				offset = (offset & MAXHSMASK) + HS_SECTOR_SIZE;

				/*
				 * Have we reached the end of current block?
				 */
				if (offset > last_offset)
					break;
				else
					continue;
			}

			/* make sure this is nullified before  reading it */
			bzero(&hd, sizeof (hd));

			/*
			 * Just ignore invalid directory entries.
			 * XXX - maybe hs_parsedir() will detect EXISTENCE bit
			 */
			if (!hs_parsedir(fsp, &blkp[rel_offset(offset)],
				&hd, dname, &dnamelen)) {
				/*
				 * Determine if there is enough room
				 */
				ndlen = (long)DIRENT64_RECLEN((dnamelen));

				if ((ndlen + ((char *)nd - outbuf)) >
				    uiop->uio_resid) {
					fbrelse(fbp, S_READ);
					goto done; /* output buffer full */
				}

				diroff = offset + hdlen;
				/*
				 * Generate nodeid.
				 * If a directory, nodeid points to the
				 * canonical dirent describing the directory:
				 * the dirent of the "." entry for the
				 * directory, which is pointed to by all
				 * dirents for that directory.
				 * Otherwise, nodeid points to dirent of file.
				 */
				if (hd.type == VDIR) {
					dirino = MAKE_NODEID(hd.ext_lbn, 0,
					    vp->v_vfsp);
				} else {
					struct hs_volume *hvp;
					ulong_t lbn, off;

					/*
					 * Normalize lbn and off
					 */
					hvp = &fsp->hsfs_vol;
					lbn = dir_lbn +
					    (offset >> hvp->lbn_shift);
					off = offset & hvp->lbn_maxoffset;
					dirino = MAKE_NODEID(lbn,
					    off, vp->v_vfsp);
				}

				(void) strcpy(nd->d_name, dname);
				nd->d_reclen = (ushort_t)ndlen;
				nd->d_off = (offset_t)diroff;
				nd->d_ino = (ino64_t)dirino;
				nd = (struct dirent64 *)((char *)nd + ndlen);

				/*
				 * free up space allocated for symlink
				 */
				if (hd.sym_link != (char *)NULL) {
					kmem_free(hd.sym_link,
					    (size_t)(hd.ext_size+1));
					hd.sym_link = (char *)NULL;
				}
			}

			offset += hdlen;
		}
		fbrelse(fbp, S_READ);
	}

	/*
	 * Got here for one of the following reasons:
	 *	1) outbuf is full (error == 0)
	 *	2) end of directory reached (error == 0)
	 *	3) error reading directory sector (error != 0)
	 *	4) directory entry crosses sector boundary (error == 0)
	 *
	 * If any directory entries have been copied, don't report
	 * case 4.  Instead, return the valid directory entries.
	 *
	 * If no entries have been copied, report the error.
	 * If case 4, this will be indistiguishable from EOF.
	 */
done:
	ndlen = ((char *)nd - outbuf);
	if (ndlen != 0) {
		error = uiomove(outbuf, (size_t)ndlen, UIO_READ, uiop);
		uiop->uio_offset = offset;
	}
	kmem_free(dname, dname_size);
	kmem_free(outbuf, bufsize);
	if (eofp && error == 0)
		*eofp = (uiop->uio_offset >= dirsiz);
	return (error);
}

static int
hsfs_fid(struct vnode *vp, struct fid *fidp)
{
	register struct hsnode *hp;
	register struct hsfid *fid;

	if (fidp->fid_len < (sizeof (*fid) - sizeof (fid->hf_len))) {
		fidp->fid_len = sizeof (*fid) - sizeof (fid->hf_len);
		return (ENOSPC);
	}

	fid = (struct hsfid *)fidp;
	fid->hf_len = sizeof (*fid) - sizeof (fid->hf_len);
	hp = VTOH(vp);
	mutex_enter(&hp->hs_contents_lock);
	fid->hf_dir_lbn = hp->hs_dir_lbn;
	fid->hf_dir_off = (ushort_t)hp->hs_dir_off;
	mutex_exit(&hp->hs_contents_lock);
	return (0);
}

/*ARGSUSED*/
static int
hsfs_open(struct vnode **vpp, int flag, struct cred *cred)
{
	return (0);
}

/*ARGSUSED*/
static int
hsfs_close(
	struct vnode *vp,
	int flag,
	int count,
	offset_t offset,
	struct cred *cred)
{
	(void) cleanlocks(vp, ttoproc(curthread)->p_pid, 0);
	cleanshares(vp, ttoproc(curthread)->p_pid);
	return (0);
}

/*ARGSUSED2*/
static int
hsfs_access(struct vnode *vp, int mode, int flags, cred_t *cred)
{
	return (hs_access(vp, (mode_t)mode, cred));
}

/*
 * the seek time of a CD-ROM is very slow, and data transfer
 * rate is even worse (max. 150K per sec).  The design
 * decision is to reduce access to cd-rom as much as possible,
 * and to transfer a sizable block (read-ahead) of data at a time.
 * UFS style of read ahead one block at a time is not appropriate,
 * and is not supported
 */

/*
 * KLUSTSIZE should be a multiple of PAGESIZE and <= MAXPHYS.
 */
#define	KLUSTSIZE	(56 * 1024)
/* we don't support read ahead */
int hsfs_lostpage;	/* no. of times we lost original page */

/*
 * Used to prevent biodone() from releasing buf resources that
 * we didn't allocate in quite the usual way.
 */
/*ARGSUSED*/
int
hsfs_iodone(struct buf *bp)
{
	sema_v(&bp->b_io);
	return (0);
}

/*
 * Each file may have a different interleaving on disk.  This makes
 * things somewhat interesting.  The gist is that there are some
 * number of contiguous data sectors, followed by some other number
 * of contiguous skip sectors.  The sum of those two sets of sectors
 * defines the interleave size.  Unfortunately, it means that we generally
 * can't simply read N sectors starting at a given offset to satisfy
 * any given request.
 *
 * What we do is get the relevant memory pages via pvn_read_kluster(),
 * then stride through the interleaves, setting up a buf for each
 * sector that needs to be brought in.  Instead of kmem_alloc'ing
 * space for the sectors, though, we just point at the appropriate
 * spot in the relevant page for each of them.  This saves us a bunch
 * of copying.
 */
/*ARGSUSED*/
hsfs_getapage(
	struct vnode *vp,
	u_offset_t off,
	size_t len,
	uint_t *protp,
	struct page *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cred)
{
	struct hsnode *hp;
	struct hsfs *fsp;
	int	err;
	struct buf *bufs;
	caddr_t *vas;
	caddr_t va;
	struct page *pp, *searchp, *lastp;
	page_t	*pagefound;
	uint_t	bof;
	struct vnode *devvp;
	ulong_t	byte_offset;
	size_t	io_len_tmp;
	uint_t	io_off, io_len;
	uint_t	xlen;
	uint_t	filsiz;
	uint_t	secsize;
	uint_t	bufcnt;
	uint_t	bufsused;
	uint_t	count;
	uint_t	io_end;
	uint_t	which_chunk_lbn;
	uint_t	offset_lbn;
	uint_t	offset_extra;
	uint_t	offset_bytes;
	uint_t	remaining_bytes;
	uint_t	extension;
	int	remainder;	/* must be signed */
	int	chunk_lbn_count;
	int	chunk_data_bytes;
	int	xarsiz;
	daddr_t driver_block;
	u_offset_t io_off_tmp;
	klwp_t	*lwp = ttolwp(curthread);

	/*
	 * We don't support asynchronous operation at the moment, so
	 * just pretend we did it.  If the pages are ever actually
	 * needed, they'll get brought in then.
	 */
	if (pl == NULL)
		return (0);

	hp = VTOH(vp);
	fsp = VFS_TO_HSFS(vp->v_vfsp);
	devvp = fsp->hsfs_devvp;
	secsize = fsp->hsfs_vol.lbn_size;  /* bytes per logical block */

	/* file data size */
	filsiz = hp->hs_dirent.ext_size;

	/* disk addr for start of file */
	bof = LBN_TO_BYTE(hp->hs_dirent.ext_lbn, vp->v_vfsp);

	/* xarsiz byte must be skipped for data */
	xarsiz = hp->hs_dirent.xar_len << fsp->hsfs_vol.lbn_shift;

	/* how many logical blocks in an interleave (data+skip) */
	chunk_lbn_count = hp->hs_dirent.intlf_sz + hp->hs_dirent.intlf_sk;

	if (chunk_lbn_count == 0) {
		chunk_lbn_count = 1;
	}

	/*
	 * Convert interleaving size into bytes.  The zero case
	 * (no interleaving) optimization is handled as a side-
	 * effect of the read-ahead logic.
	 */
	if (hp->hs_dirent.intlf_sz == 0) {
		chunk_data_bytes = LBN_TO_BYTE(1, vp->v_vfsp);
	} else {
		chunk_data_bytes = LBN_TO_BYTE(hp->hs_dirent.intlf_sz,
			vp->v_vfsp);
	}

reread:
	err = 0;
	pagefound = 0;

	/*
	 * Do some read-ahead.  This mostly saves us a bit of
	 * system cpu time more than anything else when doing
	 * sequential reads.  At some point, could do the
	 * read-ahead asynchronously which might gain us something
	 * on wall time, but it seems unlikely....
	 *
	 * We do the easy case here, which is to read through
	 * the end of the chunk, minus whatever's at the end that
	 * won't exactly fill a page.
	 */
	which_chunk_lbn = (off + len) / chunk_data_bytes;
	extension = ((which_chunk_lbn + 1) * chunk_data_bytes) - off;
	extension -= (extension % PAGESIZE);
	if (extension != 0 && extension < filsiz - off) {
		len = extension;
	}
	/*
	 * Some cd writers don't write sectors that aren't used.  Also,
	 * there's no point in reading sectors we'll never look at.  So,
	 * if we're asked to go beyond the end of a file, truncate to the
	 * length of that file.
	 *
	 * Additionally, this behaviour is required by section 6.4.5 of
	 * ISO 9660:1988(E).
	 */
	if (len > (filsiz - off)) {
		len = filsiz - off;
	}

	/*
	 * After all that, make sure we're asking for things in units
	 * that bdev_strategy() will understand (see bug 4202551).
	 */
	len = roundup(len, DEV_BSIZE);

	pp = NULL;
again:
	/* search for page in buffer */
	if ((pagefound = page_exists(vp, off)) == 0) {
		/*
		 * Need to really do disk IO to get the page.
		 */
		pp = pvn_read_kluster(vp, off, seg, addr, &io_off_tmp,
		    &io_len_tmp, off, len, 0);

		io_off = (uint_t)io_off_tmp;
		io_len = (uint_t)io_len_tmp;

		/* check for truncation */
		/*
		 * xxx Clean up and return EIO instead?
		 * xxx Ought to go to u_offset_t for everything, but we
		 * xxx call lots of things that want uint_t arguments.
		 */
		ASSERT(io_off == io_off_tmp);

		if (pp == NULL)
			goto again;

		/*
		 * get enough buffers for worst-case scenario
		 * (i.e., no coalescing possible).
		 */
		bufcnt = (len + secsize - 1) / secsize;
		bufs = kmem_zalloc(bufcnt * sizeof (struct buf), KM_SLEEP);
		vas = kmem_alloc(bufcnt * sizeof (caddr_t), KM_SLEEP);
		for (count = 0; count < bufcnt; count++) {
			bufs[count].b_edev = devvp->v_rdev;
			bufs[count].b_dev = cmpdev(devvp->v_rdev);
			bufs[count].b_flags = B_KERNBUF|B_NOCACHE|B_BUSY|B_READ;
			bufs[count].b_iodone = hsfs_iodone;
			bufs[count].b_vp = vp;
			sema_init(&bufs[count].b_io, 0, NULL,
			    SEMA_DEFAULT, NULL);
			sema_init(&bufs[count].b_sem, 0, NULL,
			    SEMA_DEFAULT, NULL);
		}

		/* zero not-to-be-read page parts */
		xlen = io_len & PAGEOFFSET;
		if (xlen != 0)
			pagezero(pp->p_prev, xlen, PAGESIZE - xlen);

		va = NULL;
		lastp = NULL;
		searchp = pp;
		io_end = io_off + io_len;
		for (count = 0, byte_offset = io_off;
			byte_offset < io_end;
			count++) {
			ASSERT(count < bufcnt);

			/* Compute disk address for interleaving. */

			/* considered without skips */
			which_chunk_lbn = byte_offset / chunk_data_bytes;

			/* factor in skips */
			offset_lbn = which_chunk_lbn * chunk_lbn_count;

			/* convert to physical byte offset for lbn */
			offset_bytes = LBN_TO_BYTE(offset_lbn, vp->v_vfsp);

			/* don't forget offset into lbn */
			offset_extra = byte_offset % chunk_data_bytes;

			/* get virtual block number for driver */
			driver_block = btodb(bof + xarsiz
				+ offset_bytes + offset_extra);

			if (lastp != searchp) {
				/* this branch taken first time through loop */
				va = vas[count]
					= ppmapin(searchp, PROT_WRITE,
						(caddr_t)-1);
				/* ppmapin() guarantees not to return NULL */
			} else {
				vas[count] = NULL;
			}

			bufs[count].b_un.b_addr = va + byte_offset % PAGESIZE;
			bufs[count].b_blkno = driver_block;

			remaining_bytes = ((which_chunk_lbn + 1)
				* chunk_data_bytes)
				- byte_offset;

			/*
			 * remaining_bytes can't be zero, as we derived
			 * which_chunk_lbn directly from byte_offset.
			 */
			if ((remaining_bytes+byte_offset) < (off+len)) {
				/* coalesce-read the rest of the chunk */
				bufs[count].b_bcount = remaining_bytes;
			} else {
				/* get the final bits */
				bufs[count].b_bcount = off + len - byte_offset;
			}

			/*
			 * It would be nice to do multiple pages'
			 * worth at once here when the opportunity
			 * arises, as that has been shown to improve
			 * our wall time.  However, to do that
			 * requires that we use the pageio subsystem,
			 * which doesn't mix well with what we're
			 * already using here.  We can't use pageio
			 * all the time, because that subsystem
			 * assumes that a page is stored in N
			 * contiguous blocks on the device.
			 * Interleaving violates that assumption.
			 */

			remainder = PAGESIZE - (byte_offset % PAGESIZE);
			if (bufs[count].b_bcount > remainder) {
				bufs[count].b_bcount = remainder;
			}

			bufs[count].b_bufsize = bufs[count].b_bcount;
			byte_offset += bufs[count].b_bcount;

			(void) bdev_strategy(&bufs[count]);

			if (lwp != NULL)
				lwp->lwp_ru.inblock++;
			lastp = searchp;
			if ((remainder - bufs[count].b_bcount) < 1) {
				searchp = searchp->p_next;
			}
		}

		bufsused = count;
		/* Now wait for everything to come in */
		for (count = 0; count < bufsused; count++) {
			if (err == 0) {
				err = biowait(&bufs[count]);
			} else
				(void) biowait(&bufs[count]);
		}

		/* Don't leak resources */
		for (count = 0; count < bufcnt; count++) {
			sema_destroy(&bufs[count].b_io);
			sema_destroy(&bufs[count].b_sem);
			if (count < bufsused && vas[count] != NULL) {
				ppmapout(vas[count]);
			}
		}

		kmem_free(vas, bufcnt * sizeof (caddr_t));
		kmem_free(bufs, bufcnt * sizeof (struct buf));
	}

	if (err) {
		pvn_read_done(pp, B_ERROR);
		return (err);
	}

	/*
	 * Lock the requested page, and the one after it if possible.
	 * Don't bother if our caller hasn't given us a place to stash
	 * the page pointers, since otherwise we'd lock pages that would
	 * never get unlocked.
	 */
	if (pagefound) {
		int index;
		ulong_t soff;

		/*
		 * Make sure it's in memory before we say it's here.
		 */
		if ((pp = page_lookup(vp, off, SE_SHARED)) == NULL) {
			hsfs_lostpage++;
			goto reread;
		}

		pl[0] = pp;
		index = 1;

		/*
		 * Try to lock the next page, if it exists, without
		 * blocking.
		 */
		plsz -= PAGESIZE;
		/* LINTED (plsz is unsigned) */
		for (soff = off + PAGESIZE; plsz > 0;
		    soff += PAGESIZE, plsz -= PAGESIZE) {
			pp = page_lookup_nowait(vp, (u_offset_t)soff,
					SE_SHARED);
			if (pp == NULL)
				break;
			pl[index++] = pp;
		}
		pl[index] = NULL;
		return (0);
	}

	if (pp != NULL) {
		pvn_plist_init(pp, pl, plsz, off, io_len, rw);
	}

	return (err);
}

static int
hsfs_getpage(
	struct vnode *vp,
	offset_t off,
	size_t len,
	uint_t *protp,
	struct page *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cred)
{
	int err;
	uint_t filsiz;
	struct hsnode *hp = VTOH(vp);

	/* does not support write */
	if (rw == S_WRITE) {
		cmn_err(CE_PANIC, "write attempt on READ ONLY HSFS");
	}

	if (vp->v_flag & VNOMAP) {
		return (ENOSYS);
	}

	ASSERT(off <= MAXOFF_T);

	/*
	 * Determine file data size for EOF check.
	 */
	filsiz = hp->hs_dirent.ext_size;
	if ((off + len) > (offset_t)(filsiz + PAGEOFFSET) && seg != segkmap)
		return (EFAULT);	/* beyond EOF */

	if (protp != NULL)
		*protp = PROT_ALL;

	if (len <= PAGESIZE)
		err = hsfs_getapage(vp, (u_offset_t)off, len, protp, pl, plsz,
		    seg, addr, rw, cred);
	else
		err = pvn_getpages(hsfs_getapage, vp, off, len, protp,
		    pl, plsz, seg, addr, rw, cred);

	return (err);
}



/*
 * This function should never be called. We need to have it to pass
 * it as an argument to other functions.
 */
/*ARGSUSED*/
int
hsfs_putapage(
	vnode_t		*vp,
	page_t		*pp,
	u_offset_t	*offp,
	size_t		*lenp,
	int		flags,
	cred_t		*cr)
{
	/* should never happen - just destroy it */
	cmn_err(CE_NOTE, "hsfs_putapage: dirty HSFS page");
	pvn_write_done(pp, B_ERROR | B_WRITE | B_INVAL | B_FORCE | flags);
	return (0);
}


/*
 * The only flags we support are B_INVAL, B_FREE and B_DONTNEED.
 * B_INVAL is set by:
 *
 *	1) the MC_SYNC command of memcntl(2) to support the MS_INVALIDATE flag.
 *	2) the MC_ADVISE command of memcntl(2) with the MADV_DONTNEED advice
 *	   which translates to an MC_SYNC with the MS_INVALIDATE flag.
 *
 * The B_FREE (as well as the B_DONTNEED) flag is set when the
 * MADV_SEQUENTIAL advice has been used. VOP_PUTPAGE is invoked
 * from SEGVN to release pages behind a pagefault.
 */
/*ARGSUSED*/
static int
hsfs_putpage(
	struct vnode	*vp,
	offset_t	off,
	size_t		len,
	int		flags,
	struct cred	*cr)
{
	int error = 0;

	if (vp->v_count == 0)
		cmn_err(CE_PANIC, "hsfs_putpage: bad v_count");

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	ASSERT(off <= MAXOFF_T);

	if (vp->v_pages == NULL)	/* no pages mapped */
		return (0);

	if (len == 0)		/* from 'off' to EOF */
		error = pvn_vplist_dirty(vp, off,
					hsfs_putapage, flags, cr);
	else {
		offset_t end_off = off + len;
		offset_t file_size = VTOH(vp)->hs_dirent.ext_size;
		offset_t io_off;

		file_size = (file_size + PAGESIZE - 1) & PAGEMASK;
		if (end_off > file_size)
			end_off = file_size;

		for (io_off = off; io_off < end_off; io_off += PAGESIZE) {
			page_t *pp;

			/*
			 * We insist on getting the page only if we are
			 * about to invalidate, free or write it and
			 * the B_ASYNC flag is not set.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL)
				continue;
			/*
			 * Normally pvn_getdirty() should return 0, which
			 * impies that it has done the job for us.
			 * The shouldn't-happen scenario is when it returns 1.
			 * This means that the page has been modified and
			 * needs to be put back.
			 * Since we can't write on a CD, we fake a failed
			 * I/O and force pvn_write_done() to destroy the page.
			 */
			if (pvn_getdirty(pp, flags) == 1) {
				cmn_err(CE_NOTE,
					"hsfs_putpage: dirty HSFS page");
				pvn_write_done(pp, flags |
				    B_ERROR | B_WRITE | B_INVAL | B_FORCE);
			}
		}
	}
	return (error);
}


/*ARGSUSED*/
static int
hsfs_map(
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
	struct segvn_crargs vn_a;
	int error;

	/* VFS_RECORD(vp->v_vfsp, VS_MAP, VS_CALL); */

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off > MAXOFF_T)
		return (EFBIG);

	if (off < 0 || (offset_t)(off + len) < 0)
		return (EINVAL);

	if (vp->v_type != VREG) {
		return (ENODEV);
	}

	/*
	 * If file is being locked, disallow mapping.
	 */
	if (vp->v_filocks != NULL && MANDLOCK(vp, VTOI(vp)->i_mode))
		return (EAGAIN);

	as_rangelock(as);

	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User specified address - blow away any previous mappings
		 */
		(void) as_unmap(as, *addrp, len);
	}

	vn_a.vp = vp;
	vn_a.offset = off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = prot;
	vn_a.maxprot = maxprot;
	vn_a.flags = flags & ~MAP_TYPE;
	vn_a.cred = cred;
	vn_a.amp = NULL;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);
	return (error);
}

/* ARGSUSED */
static int
hsfs_addmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	register struct hsnode *hp;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	hp = VTOH(vp);
	mutex_enter(&hp->hs_contents_lock);
	hp->hs_mapcnt += btopr(len);
	mutex_exit(&hp->hs_contents_lock);
	return (0);
}

/*ARGSUSED*/
static int
hsfs_delmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	register struct hsnode *hp;

	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	hp = VTOH(vp);
	mutex_enter(&hp->hs_contents_lock);
	hp->hs_mapcnt -= btopr(len);	/* Count released mappings */
	ASSERT(hp->hs_mapcnt >= 0);
	mutex_exit(&hp->hs_contents_lock);
	return (0);
}

/* ARGSUSED */
static int
hsfs_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	return ((*noffp < 0 || *noffp > MAXOFFSET_T) ? EINVAL : 0);
}

struct vnodeops hsfs_vnodeops = {
	hsfs_open,
	hsfs_close,
	hsfs_read,
	fs_nosys,	/* write */
	fs_nosys,	/* ioctl */
	fs_setfl,	/* setfl */
	hsfs_getattr,
	fs_nosys,	/* setattr */
	hsfs_access,
	hsfs_lookup,
	fs_nosys,	/* create */
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	hsfs_readdir,
	fs_nosys,	/* symlink */
	hsfs_readlink,
	hsfs_fsync,	/* fsync */
	hsfs_inactive,
	hsfs_fid,
	fs_rwlock,	/* rwlock */
	fs_rwunlock,	/* rwunlock */
	hsfs_seek,	/* seek */
	fs_cmp,
	fs_frlock,	/* frlock */
	fs_nosys,	/* space */
	fs_nosys,	/* realvp */
	hsfs_getpage,
	hsfs_putpage,
	hsfs_map,
	hsfs_addmap,	/* addmap */
	hsfs_delmap,	/* delmap */
	fs_poll,	/* poll */
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_dispose,
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_shrlock	/* shrlock */
};
