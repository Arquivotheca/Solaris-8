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
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1997 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)s5_bmap.c	1.5	97/06/09 SMI"
/* from "ufs_bmap.c 2.55     90/01/09 SMI"    		*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/disp.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fs/s5_inode.h>
#include <sys/fs/s5_fs.h>
#include <vm/seg.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>


extern int geterror();

/*
 * bmap{rd,wr} define the structure of file system storage by mapping
 * a logical offset in a file to a physical block number on the device.
 * It should be called with a locked inode when allocation is to be
 * done (bmapwr).  Note this strangeness: bmapwr is always called from
 * getpage(), not putpage(), since getpage() is where all the allocation
 * is done.
 *
 * S_READ, S_OTHER -> bmaprd; S_WRITE -> bmapwr.
 *
 * The block number returned is the disk block number, not the
 * file system block number.  All the worries about block offsets and
 * page/block sizes are hidden inside of bmap.  Well, not quite,
 * unfortunately.  It's impossible to find one place to hide all this
 * mess.
 *
 * Returns 0 on success, or a non-zero errno if an error occurs.
 */
int
s5_bmap_read(ip, off, bnp, lenp)
	struct	inode *ip;
	u_int	off;
	daddr_t *bnp;
	int	*lenp;
{
	daddr_t lbn;
	struct	filsys *fs = (struct filsys *)ip->i_fs;
	struct	buf *bp;
	int	i, j;
	int	shft;			/* we maintain sh = 1 << shft */
	daddr_t	ob, nb, tbn, *bap;
	int	nindirshift, nindiroffset;
	struct	s5vfs *s5vfs = ip->i_s5vfs;
	u_int	bsize = s5vfs->vfs_bsize;

	ASSERT(RW_LOCK_HELD(&ip->i_contents));
	lbn = lblkno(s5vfs, off);
	if (lbn < 0)
		return (EFBIG);

	/*
	 * The first NDADDR blocks are direct blocks.
	 */
	if (lbn < NDADDR) {
		if ((nb  = ip->i_addr[lbn++]) != S5_HOLE) {
			*bnp = fsbtodb(fs, nb);
			for (i = bsize; lbn < NDADDR &&
			    ip->i_addr[lbn] == ip->i_addr[lbn-1] + 1; lbn++) {
					i += bsize;
			}
			*lenp = i;
		}
		return (0);
	}

	nindirshift = s5vfs->vfs_nshift;
	nindiroffset = s5vfs->vfs_nindir - 1;

	/*
	 * Determine how many levels of indirection.
	 */
	shft = 0;				/* sh = 1 */
	tbn = lbn - NDADDR;
	for (j = NIADDR; j > 0; j--) {
		int	sh;

		shft += nindirshift;		/* sh *= nindir */
		sh = 1 << shft;
		if (tbn < sh)
			break;
		tbn -= sh;
	}
	if (j == 0)
		return (EFBIG);

	/*
	 * Fetch the first indirect block.
	 */
	nb = ip->i_addr[NADDR - j];
	if (nb == S5_HOLE) {
		*bnp =  0;
		return (0);
	}

	/*
	 * Fetch through the indirect blocks.
	 */
	for (; j <= NIADDR; j++) {
		ob = nb;
		bp = bread(ip->i_dev, fsbtodb(fs, ob), s5vfs->vfs_bsize);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return (EIO);
		}
		bap = (daddr_t *)bp->b_un.b_daddr;
		shft -= nindirshift;		/* sh / nindir */
		i = (tbn >> shft) & nindiroffset; /* (tbn / sh) % nindir */
		nb = bap[i];
		if (nb == S5_HOLE) {
			brelse(bp);
			*bnp =  0;
			return (0);
		}
		if (j != NIADDR)
			brelse(bp);
	}

	if ((nb =  bap[i++]) != S5_HOLE) {
		*bnp = fsbtodb(fs, nb);
		for (j = bsize; lbn < NINDIR(s5vfs) &&
		    bap[i] == bap[i-1] + 1; i++) {
				j += bsize;
		}
		*lenp = j;
	}
	brelse(bp);
	return (0);
}

/*
 * See s5_bmap_read for general notes.
 *
 * If alloc_only is set, bmap does not clear the newly allocated disk blocks.
 * Otherwise, the in-core pages are created and initialized as needed.
 *
 * Returns 0 on success, or a non-zero errno if an error occurs.
 */
s5_bmap_write(ip, off, size, alloc_only, cr)
	struct	inode *ip;
	u_int	off;
	int	size;
	int	alloc_only;
	struct	cred *cr;
{
	struct	filsys *fs;
	struct	buf *bp;
	int	i;
	struct	buf *nbp;
	int	j;
	int	shft;			/* we maintain sh = 1 << shft */
	daddr_t	ob, nb, lbn, tbn, *bap;
	struct	vnode *vp = ITOV(ip);
	long	bsize;
	int	issync, isdir;
	int	err, write_out_later = 0;
	dev_t	dev;
	struct	fbuf *fbp;
	int	nindirshift;
	int	nindiroffset;
	struct	s5vfs *s5vfs = ip->i_s5vfs;

	ASSERT(RW_WRITE_HELD(&ip->i_contents));

	bsize = s5vfs->vfs_bsize;
	fs = (struct filsys *)ip->i_fs;
	lbn = lblkno(s5vfs, off);

	if (lbn < 0)
		return (EFBIG);
	isdir = ((ip->i_mode & IFMT) == IFDIR);
	issync = ((ip->i_flag & ISYNC) != 0);

	if (isdir || issync)
		alloc_only = 0;		/* make sure */

	/*
	 * The first NDADDR blocks are direct blocks.
	 */
	while (lbn < NDADDR && size > 0) {
		if ((nb = ip->i_addr[lbn]) == 0) {
			err = s5_alloc(ip, &nb, cr);
			if (err)
				return (err);

			ip->i_addr[lbn] = nb;

			/*
			 * fbzero does not cause a pagefault
			 */
			fbp = NULL;
			if (!alloc_only)
				fbzero(vp, (offset_t)(lbn << s5vfs->vfs_bshift),
					(u_int)bsize, &fbp);
			if (isdir) {
				(void) s5_fbiwrite(fbp, ip, nb, bsize);
			} else if (fbp) {
				fbrelse(fbp, S_WRITE);
			}
			ip->i_blocks++;			/* note change */
			mutex_enter(&ip->i_tlock);
			ip->i_flag |= IUPD | ICHG;
			mutex_exit(&ip->i_tlock);
		}
		size -= bsize;
		lbn++;
		off += bsize;
	}

	if (size <= 0)
		return (0);

	nindirshift = s5vfs->vfs_nshift;
	nindiroffset = s5vfs->vfs_nindir - 1;

	/*
	 * Determine how many levels of indirection.
	 */
	shft = 0;				/* sh = 1 */
	tbn = lbn - NDADDR;
	for (j = NIADDR; j > 0; j--) {
		int	sh;

		shft += nindirshift;		/* sh *= nindir */
		sh = 1 << shft;
		if (tbn < sh)
			break;
		tbn -= sh;
	}

	if (j == 0)
		return (EFBIG);

	/*
	 * Fetch the first indirect block.
	 *  This is the only place that indirect blk lists are allocated
	 */
	dev = ip->i_dev;
	nb = ip->i_addr[NADDR - j];
	if (nb == 0) {
		/*
		 * Need to allocate an indirect block
		 * (there was not a previously allocated
		 * indirect block).
		 */
		err = s5_alloc(ip, &nb, cr);
		if (err)
			return (err);
		/*
		 * Write zero block synchronously so that
		 * indirect blocks never point at garbage.
		 */
		bp = getblk(dev, fsbtodb(fs, nb), bsize);
		clrbuf(bp);
		bwrite(bp);

		/*
		 * update the inode to point to the new indirect blk
		 * we do not change offset or nblocks since this is
		 * a hiden overhead block not a requested block.
		 */
		ip->i_addr[NADDR - j] = nb;
		ip->i_blocks++;			/* note change */
		mutex_enter(&ip->i_tlock);
		ip->i_flag |= IUPD | ICHG;
		mutex_exit(&ip->i_tlock);

		/*
		 * In the ISYNC case, rwip will notice that the block
		 * count on the inode has changed and will be sure to
		 * s5_iupdat the inode at the end of rwip.
		 */
	}

	/*
	 * Fetch through the indirect blocks.
	 *
	 *  If we cross a indirect blk then recurse to do it.
	 *  In the following code ob is the block
	 *  that contains the indirect block list.
	 *  Nb is the new block that will be added
	 *  to that list.
	 */
	ob = nb;

	/* get the indirect list */
	bp = bread(ip->i_dev, fsbtodb(fs, ob), bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return (EIO);
	}

	/* bap points to the indirect block list */
	bap = (daddr_t *)bp->b_un.b_daddr;
	shft -= nindirshift;		/* sh /= nindir */
	i = (tbn >> shft) & nindiroffset; /* (tbn / sh) % nindir */

	do {
		nb = bap[i];

		/* if the block is not already allocated do it now */
		if (nb == 0) {
			err = s5_alloc(ip, &nb, cr);
			if (err) {
				brelse(bp);
				return (err);
			}
			bap[i] = nb;

			if (!alloc_only) {
				/*
				 * To avoid deadlocking if the pageout
				 * daemon decides to push a page for this
				 * inode while we are sleeping holding the
				 * bp but waiting more pages for fbzero,
				 * we give up the bp now.
				 *
				 * XXX - need to avoid having the pageout
				 * daemon get in this situation to begin with!
				 */
				brelse(bp);
				fbzero(vp, (offset_t)(lbn << s5vfs->vfs_bshift),
					(u_int)bsize, &fbp);

				/*
				 * Cases which we need to do a synchronous
				 * write of the zeroed blocks:
				 *
				 * 1) If we are writing a directory then we
				 * want to write synchronously so blocks in
				 * directories never contain garbage.
				 *
				 * 2) If we are filling in a hole and the
				 * indirect block is going to be synchronously
				 * written back below we need to make sure
				 * that the zeroes are written here before
				 * the indirect block is updated so that if
				 * we crash before the real data is pushed
				 * we will not end up with random data is
				 * the middle of the file.
				 *
				 * 3) If the size of the request rounded up
				 * to the system page size is smaller than
				 * the file system block size, we want to
				 * write out all the pages now so that
				 * they are not aborted before they actually
				 * make it to s5_putpage since the length
				 * of the inode will not include the pages.
				 */
				if (isdir || issync)
					(void) s5_fbiwrite(fbp, ip, nb,
						bsize);
				else
					fbrelse(fbp, S_WRITE);

				/*
				 * Now get the bp back
				 */
				bp = bread(ip->i_dev, fsbtodb(fs, ob),
					bsize);

				err = geterror(bp);
				if (err) {
					(void) s5_free(ip, nb, (off_t)bsize);
					brelse(bp);
					return (err);
				}
				bap = (daddr_t *)bp->b_un.b_daddr;
			} else {
				/*
				 * Write synchronously so indirect
				 * blocks never point at garbage.
				 */
				nbp = getblk(dev, fsbtodb(fs, nb), bsize);

				clrbuf(nbp);
				bwrite(nbp);
			}

			ip->i_blocks++;
			mutex_enter(&ip->i_tlock);
			ip->i_flag |= IUPD | ICHG;
			mutex_exit(&ip->i_tlock);

			write_out_later = 1;

		}
		off += bsize;
		size -= bsize;
	} while (++i < NINDIR(s5vfs) && size > 0);

	if (write_out_later) {
		if (issync)
			bwrite(bp);
		else
			bdwrite(bp);
	} else
		brelse(bp);

	return (size > 0 ?
	    s5_bmap_write(ip, off, size, alloc_only, cr) : 0);
}
