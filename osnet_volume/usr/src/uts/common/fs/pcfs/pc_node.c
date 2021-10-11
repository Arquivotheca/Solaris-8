/*
 * Copyright (c) 1989, 1992, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pc_node.c	1.35	99/01/08 SMI"

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <vm/pvn.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>
#include <sys/dirent.h>
#include <sys/fdio.h>
#include <sys/file.h>
#include <sys/conf.h>

struct pchead pcfhead[NPCHASH];
struct pchead pcdhead[NPCHASH];

extern krwlock_t pcnodes_lock;

void		pc_init(void);
struct pcnode	*pc_getnode(struct pcfs *, daddr_t, int, struct pcdir *);
void		pc_rele(struct pcnode *);
void		pc_mark_mod(struct pcnode *);
void		pc_mark_acc(struct pcnode *);
int		pc_truncate(struct pcnode *, int);
int		pc_nodesync(struct pcnode *);
int		pc_nodeupdate(struct pcnode *);
int		pc_verify(struct pcfs *);
void		pc_mark_irrecov(struct pcfs *);
void		pc_diskchanged(struct pcfs *);

static int	pc_getentryblock(struct pcnode *, struct buf **);
static int	syncpcp(struct pcnode *, int);


/*
 * fake entry for root directory, since this does not have a parent
 * pointing to it.
 */
static struct pcdir rootentry = {
	"",
	"",
	PCA_DIR
};

void
pc_init(void)
{
	struct pchead *hdp, *hfp;
	int i;
	for (i = 0; i < NPCHASH; i++) {
		hdp = &pcdhead[i];
		hfp = &pcfhead[i];
		hdp->pch_forw =  (struct pcnode *)hdp;
		hdp->pch_back =  (struct pcnode *)hdp;
		hfp->pch_forw =  (struct pcnode *)hfp;
		hfp->pch_back =  (struct pcnode *)hfp;
	}
}

struct pcnode *
pc_getnode(
	struct pcfs *fsp,	/* filsystem for node */
	daddr_t blkno,		/* phys block no of dir entry */
	int offset,		/* offset of dir entry in block */
	struct pcdir *ep)	/* node dir entry */
{
	struct pcnode *pcp;
	struct pchead *hp;
	struct vnode *vp;
	pc_cluster32_t scluster;

	ASSERT(fsp->pcfs_flags & PCFS_LOCKED);
	if (ep == (struct pcdir *)0) {
		ep = &rootentry;
		scluster = 0;
	} else {
		scluster = pc_getstartcluster(fsp, ep);
	}
	/*
	 * First look for active nodes.
	 * File nodes are identified by the location (blkno, offset) of
	 * its directory entry.
	 * Directory nodes are identified by the starting cluster number
	 * for the entries.
	 */
	if (ep->pcd_attr & PCA_DIR) {
		hp = &pcdhead[PCDHASH(fsp, scluster)];
		rw_enter(&pcnodes_lock, RW_READER);
		for (pcp = hp->pch_forw;
		    pcp != (struct pcnode *)hp; pcp = pcp->pc_forw) {
			if ((fsp == VFSTOPCFS(PCTOV(pcp)->v_vfsp)) &&
			    (scluster == pcp->pc_scluster)) {
				VN_HOLD(PCTOV(pcp));
				rw_exit(&pcnodes_lock);
				return (pcp);
			}
		}
		rw_exit(&pcnodes_lock);
	} else {
		hp = &pcfhead[PCFHASH(fsp, blkno, offset)];
		rw_enter(&pcnodes_lock, RW_READER);
		for (pcp = hp->pch_forw;
		    pcp != (struct pcnode *)hp; pcp = pcp->pc_forw) {
			if ((fsp == VFSTOPCFS(PCTOV(pcp)->v_vfsp)) &&
			    ((pcp->pc_flags & PC_INVAL) == 0) &&
			    (blkno == pcp->pc_eblkno) &&
			    (offset == pcp->pc_eoffset)) {
				VN_HOLD(PCTOV(pcp));
				rw_exit(&pcnodes_lock);
				return (pcp);
			}
		}
		rw_exit(&pcnodes_lock);
	}
	/*
	 * Cannot find node in active list. Allocate memory for a new node
	 * initialize it, and put it on the active list.
	 */
	pcp = kmem_alloc(sizeof (struct pcnode), KM_SLEEP);
	bzero(pcp, sizeof (struct pcnode));
	vp = kmem_alloc(sizeof (struct vnode), KM_SLEEP);
	bzero(vp, sizeof (struct vnode));
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	pcp->pc_vn = vp;
	pcp->pc_entry = *ep;
	pcp->pc_eblkno = blkno;
	pcp->pc_eoffset = offset;
	pcp->pc_scluster = scluster;
	pcp->pc_flags = 0;
	vp->v_count = 1;
	if (ep->pcd_attr & PCA_DIR) {
		vp->v_op = &pcfs_dvnodeops;
		vp->v_type = VDIR;
		vp->v_flag = 0;
		if (scluster == 0) {
			vp->v_flag = VROOT;
			blkno = offset = 0;
			if (IS_FAT32(fsp)) {
				pcp->pc_size = pc_fileclsize(fsp,
				    fsp->pcfs_rdirstart) * fsp->pcfs_clsize;
			} else {
				pcp->pc_size =
				    fsp->pcfs_rdirsec * fsp->pcfs_secsize;
			}
		} else
			pcp->pc_size = pc_fileclsize(fsp, scluster) *
			    fsp->pcfs_clsize;
	} else {
		vp->v_op = &pcfs_fvnodeops;
		vp->v_type = VREG;
		vp->v_flag = VNOSWAP;
		fsp->pcfs_frefs++;
		pcp->pc_size = ltohi(ep->pcd_size);
	}
	fsp->pcfs_nrefs++;
	vp->v_data = (caddr_t)pcp;
	vp->v_vfsp = PCFSTOVFS(fsp);
	rw_enter(&pcnodes_lock, RW_WRITER);
	insque(pcp, hp);
	rw_exit(&pcnodes_lock);
	return (pcp);
}

int
syncpcp(struct pcnode *pcp, int flags)
{
	int err;
	if (PCTOV(pcp)->v_pages == NULL)
		err = 0;
	else
		err = VOP_PUTPAGE(PCTOV(pcp), (offset_t)0, (uint_t)0,
		    flags, (struct cred *)0);

	return (err);
}

void
pc_rele(struct pcnode *pcp)
{
	struct pcfs *fsp;
	struct vnode *vp;
	int err;

	vp = PCTOV(pcp);
	PC_DPRINTF1(8, "pc_rele vp=0x%p\n", (void *)vp);

	fsp = VFSTOPCFS(vp->v_vfsp);
	ASSERT(fsp->pcfs_flags & PCFS_LOCKED);

	rw_enter(&pcnodes_lock, RW_WRITER);
	pcp->pc_flags |= PC_RELEHOLD;

retry:
	if (vp->v_type != VDIR && (pcp->pc_flags & PC_INVAL) == 0) {
		/*
		 * If the file was removed while active it may be safely
		 * truncated now.
		 */

		if (pcp->pc_entry.pcd_filename[0] == PCD_ERASED) {
			(void) pc_truncate(pcp, 0);
		} else if (pcp->pc_flags & PC_CHG) {
			(void) pc_nodeupdate(pcp);
		}
		err = syncpcp(pcp, B_INVAL);
		if (err) {
			(void) syncpcp(pcp, B_INVAL|B_FORCE);
		}
	}
	if (vp->v_pages != NULL) {
		/*
		 * pvn_vplist_dirty will abort all old pages
		 */
		(void) pvn_vplist_dirty(vp, (u_offset_t)0,
		    pcfs_putapage, B_INVAL, (struct cred *)NULL);
	}

	(void) pc_syncfat(fsp);
	mutex_enter(&vp->v_lock);
	if (vp->v_pages != NULL) {
		mutex_exit(&vp->v_lock);
		goto retry;
	}
	ASSERT(vp->v_pages == 0);

	vp->v_count--;  /* release our hold from vn_rele */
	if (vp->v_count > 0) { /* Is this check still needed? */
		PC_DPRINTF1(3, "pc_rele: pcp=0x%p HELD AGAIN!\n", (void *)pcp);
		mutex_exit(&vp->v_lock);
		pcp->pc_flags &= ~PC_RELEHOLD;
		rw_exit(&pcnodes_lock);
		return;
	}

	remque(pcp);
	rw_exit(&pcnodes_lock);
	if ((vp->v_type == VREG) && !(pcp->pc_flags & PC_INVAL)) {
		fsp->pcfs_frefs--;
	}
	fsp->pcfs_nrefs--;

	if (fsp->pcfs_nrefs < 0) {
		panic("pc_rele: nrefs count");
	}
	if (fsp->pcfs_frefs < 0) {
		panic("pc_rele: frefs count");
	}

	mutex_exit(&vp->v_lock);
	mutex_destroy(&vp->v_lock);
	kmem_free(vp, sizeof (struct vnode));
	kmem_free(pcp, sizeof (struct pcnode));
}

/*
 * Mark a pcnode as modified with the current time.
 */
void
pc_mark_mod(struct pcnode *pcp)
{
	if (PCTOV(pcp)->v_type == VREG) {
		pc_tvtopct(&hrestime, &pcp->pc_entry.pcd_mtime);
		pcp->pc_flags |= PC_CHG;
	}
}

/*
 * Mark a pcnode as accessed with the current time.
 */
void
pc_mark_acc(struct pcnode *pcp)
{
	struct pctime pt;

	if (PCTOV(pcp)->v_type == VREG) {
		pc_tvtopct(&hrestime, &pt);
		pcp->pc_entry.pcd_ladate = pt.pct_date;
		pcp->pc_flags |= PC_CHG;
	}
}

/*
 * Truncate a file to a length.
 * Node must be locked.
 */
int
pc_truncate(struct pcnode *pcp, int length)
{
	struct pcfs *fsp;
	struct vnode *vp;
	uint_t off;
	int error = 0;

	PC_DPRINTF3(4, "pc_truncate pcp=0x%p, len=%d, size=%d\n",
	    (void *)pcp, length, pcp->pc_size);
	vp = PCTOV(pcp);
	if (pcp->pc_flags & PC_INVAL)
		return (EIO);
	fsp = VFSTOPCFS(vp->v_vfsp);
	/*
	 * directories are always truncated to zero and are not marked
	 */
	if (vp->v_type == VDIR) {
		error = pc_bfree(pcp, 0);
		return (error);
	}
	/*
	 * If length is the same as the current size
	 * just mark the pcnode and return.
	 */
	if (length > pcp->pc_size) {
		daddr_t bno;
		uint_t llcn;

		/*
		 * We are extending a file.
		 * Extend it with _one_ call to pc_balloc (no holes)
		 * since we don't need the use the block number(s).
		 */
		if (howmany(pcp->pc_size, fsp->pcfs_clsize) <
		    (llcn = howmany(length, fsp->pcfs_clsize))) {
			error = pc_balloc(pcp, (daddr_t)(llcn - 1), 1, &bno);
		}
		if (error) {
			PC_DPRINTF1(2, "pc_truncate: error=%d\n", error);
			/*
			 * probably ran out disk space;
			 * determine current file size
			 */
			pcp->pc_size = fsp->pcfs_clsize *
			    pc_fileclsize(fsp, pcp->pc_scluster);
		} else
			pcp->pc_size = length;

	} else if (length < pcp->pc_size) {
		/*
		 * We are shrinking a file.
		 * Free blocks after the block that length points to.
		 */
		off = pc_blkoff(fsp, length);
		if (off == 0) {
			(void) pvn_vplist_dirty(PCTOV(pcp), (u_offset_t)length,
				pcfs_putapage, B_INVAL | B_TRUNC, CRED());
		} else {
			pvn_vpzero(PCTOV(pcp), (u_offset_t)length,
			    (uint_t)(fsp->pcfs_clsize - off));
			(void) pvn_vplist_dirty(PCTOV(pcp), (u_offset_t)length,
				pcfs_putapage, B_INVAL | B_TRUNC, CRED());
		}
		error = pc_bfree(pcp,
		    (pc_cluster32_t)howmany(length, fsp->pcfs_clsize));
		pcp->pc_size = length;
	}
	pc_mark_mod(pcp);
	return (error);
}

/*
 * Get block for entry.
 */
static int
pc_getentryblock(struct pcnode *pcp, struct buf **bpp)
{
	struct pcfs *fsp;

	PC_DPRINTF0(7, "pc_getentryblock ");
	fsp = VFSTOPCFS(PCTOV(pcp)->v_vfsp);
	if (pcp->pc_eblkno >= fsp->pcfs_datastart ||
	    (pcp->pc_eblkno - fsp->pcfs_rdirstart) <
	    (fsp->pcfs_rdirsec & ~(fsp->pcfs_spcl - 1))) {
		*bpp = bread(fsp->pcfs_xdev,
		    pc_dbdaddr(fsp, pcp->pc_eblkno), fsp->pcfs_clsize);
	} else {
		*bpp = bread(fsp->pcfs_xdev,
		    pc_dbdaddr(fsp, pcp->pc_eblkno),
		    (int)(fsp->pcfs_datastart-pcp->pc_eblkno) *
		    fsp->pcfs_secsize);
	}
	if ((*bpp)->b_flags & B_ERROR) {
		PC_DPRINTF0(1, "pc_getentryblock: error ");
		brelse(*bpp);
		pc_mark_irrecov(fsp);
		return (EIO);
	}
	return (0);
}

/*
 * Sync all data associated with a file.
 * Flush all the blocks in the buffer cache out to disk, sync the FAT and
 * update the directory entry.
 */
int
pc_nodesync(struct pcnode *pcp)
{
	struct pcfs *fsp;
	int err;
	struct vnode *vp;

	PC_DPRINTF1(7, "pc_nodesync pcp=0x%p\n", (void *)pcp);
	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	err = 0;
	if (pcp->pc_flags & PC_MOD) {
		/*
		 * Flush all data blocks from buffer cache and
		 * update the FAT which points to the data.
		 */
		if (err = syncpcp(pcp, 0)) { /* %% ?? how to handle error? */
			if (err == ENOMEM)
				return (err);
			else {
				pc_mark_irrecov(fsp);
				return (EIO);
			}
		}
		pcp->pc_flags &= ~PC_MOD;
	}
	/*
	 * update the directory entry
	 */
	if (pcp->pc_flags & PC_CHG)
		(void) pc_nodeupdate(pcp);
	return (err);
}

/*
 * Update the node's directory entry.
 */
int
pc_nodeupdate(struct pcnode *pcp)
{
	struct buf *bp;
	int error;
	struct vnode *vp;
	struct pcfs *fsp;

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	if (IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		/* no node to update */
		pcp->pc_flags &= ~(PC_CHG | PC_MOD | PC_ACC);
		return (0);
	}
	if (vp->v_flag & VROOT) {
		panic("pc_nodeupdate");
	}
	if (pcp->pc_flags & PC_INVAL)
		return (0);
	PC_DPRINTF3(7, "pc_nodeupdate pcp=0x%p, bn=%ld, off=%d\n", (void *)pcp,
	    pcp->pc_eblkno, pcp->pc_eoffset);

	if (error = pc_getentryblock(pcp, &bp)) {
		return (error);
	}
	if (vp->v_type == VREG) {
		if (pcp->pc_flags & PC_CHG)
			pcp->pc_entry.pcd_attr |= PCA_ARCH;
		pcp->pc_entry.pcd_size = htoli(pcp->pc_size);
	}
	pc_setstartcluster(fsp, &pcp->pc_entry, pcp->pc_scluster);
	*((struct pcdir *)(bp->b_un.b_addr + pcp->pc_eoffset)) = pcp->pc_entry;
	bwrite2(bp);
	error = geterror(bp);
	if (error)
		error = EIO;
	brelse(bp);
	if (error) {
		PC_DPRINTF0(1, "pc_nodeupdate ERROR\n");
		pc_mark_irrecov(VFSTOPCFS(vp->v_vfsp));
	}
	pcp->pc_flags &= ~(PC_CHG | PC_MOD | PC_ACC);
	return (error);
}

/*
 * Verify that the disk in the drive is the same one that we
 * got the pcnode from.
 * MUST be called with node unlocked.
 */
/* ARGSUSED */
int
pc_verify(struct pcfs *fsp)
{
	int fdstatus = 0;
	int error = 0;

	if (!fsp || fsp->pcfs_flags & PCFS_IRRECOV)
		return (EIO);

	if (!(fsp->pcfs_flags & PCFS_NOCHK) && fsp->pcfs_fatp) {
		PC_DPRINTF1(4, "pc_verify fsp=0x%p\n", (void *)fsp);
		error = cdev_ioctl(fsp->pcfs_vfs->vfs_dev,
		    FDGETCHANGE, (intptr_t)&fdstatus, FKIOCTL, NULL, NULL);

		if (error) {
			PC_DPRINTF0(1, "pc_verify: FDGETCHANGE ioctl failed\n");
			pc_mark_irrecov(fsp);
		} else if (fsp->pcfs_fatjustread) {
			/*
			 * Ignore the results of the ioctl if we just
			 * read the FAT.  There is a good chance that
			 * the disk changed bit will be on, because
			 * we've just mounted and we don't want to
			 * give a false positive that the sky is falling.
			 */
			fsp->pcfs_fatjustread = 0;
		} else {
			/*
			 * Oddly enough we can't check just one flag here. The
			 * x86 floppy driver sets a different flag
			 * (FDGC_DETECTED) than the sparc driver does.
			 * I think this MAY be a bug, and I filed 4165938
			 * to get someone to look at the behavior
			 * a bit more closely.  In the meantime, my testing and
			 * code examination seem to indicate it is safe to
			 * check for either bit being set.
			 */
			if (fdstatus & (FDGC_HISTORY | FDGC_DETECTED)) {
				PC_DPRINTF0(1, "pc_verify: change detected\n");
				pc_mark_irrecov(fsp);
			}
		}
	}
	if (!(error || fsp->pcfs_fatp)) {
		error = pc_getfat(fsp);
	}

	return (error);
}

/*
 * The disk has changed, pulling the rug out from beneath us.
 * Mark the FS as being in an irrecoverable state.
 * In a short while we'll clean up.
 */
void
pc_mark_irrecov(struct pcfs *fsp)
{
	if (!(fsp->pcfs_flags & PCFS_NOCHK)) {
		if (pc_lockfs(fsp, 1, 0)) {
			/*
			 * Locking failed, which currently would
			 * only happen if the FS were already
			 * marked as hosed.  If another reason for
			 * failure were to arise in the future, this
			 * routine would have to change.
			 */
			return;
		}

		fsp->pcfs_flags |= PCFS_IRRECOV;
		cmn_err(CE_WARN,
			"Disk was changed during an update or\n"
			"an irrecoverable error was encountered.\n"
			"File damage is possible.  To prevent further\n"
			"damage, this pcfs instance will now be frozen.\n"
			"Use umount(1M) to release the instance.\n");
		(void) pc_unlockfs(fsp);
	}
}

/*
 * The disk has been changed!
 */
void
pc_diskchanged(struct pcfs *fsp)
{
	struct pcnode *pcp, *npcp = NULL;
	struct pchead *hp;
	struct vnode  *vp;
	extern vfs_t    EIO_vfs;

	/*
	 * Eliminate all pcnodes (dir & file) associated to this fs.
	 * If the node is internal, ie, no references outside of
	 * pcfs itself, then release the associated vnode structure.
	 * Invalidate the in core FAT.
	 * Invalidate cached data blocks and blocks waiting for I/O.
	 */
	PC_DPRINTF1(1, "pc_diskchanged fsp=0x%p\n", (void *)fsp);

	for (hp = pcdhead; hp < &pcdhead[NPCHASH]; hp++) {
		for (pcp = hp->pch_forw;
		    pcp != (struct pcnode *)hp; pcp = npcp) {
			npcp = pcp -> pc_forw;
			vp = PCTOV(pcp);
			if (VFSTOPCFS(vp->v_vfsp) == fsp &&
			    !(pcp->pc_flags & PC_RELEHOLD)) {
				mutex_enter(&(vp)->v_lock);
				if (vp->v_count > 0) {
					mutex_exit(&(vp)->v_lock);
					continue;
				}
				mutex_exit(&(vp)->v_lock);
				VN_HOLD(vp);
				remque(pcp);
				vp->v_data = NULL;
				vp->v_vfsp = &EIO_vfs;
				vp->v_type = VBAD;
				VN_RELE(vp);
				if (!(pcp->pc_flags & PC_EXTERNAL)) {
					mutex_destroy(&(vp->v_lock));
					kmem_free(vp, sizeof (struct vnode));
				}
				kmem_free(pcp, sizeof (struct pcnode));
				fsp->pcfs_nrefs --;
			}
		}
	}
	for (hp = pcfhead; fsp->pcfs_frefs && hp < &pcfhead[NPCHASH]; hp++) {
		for (pcp = hp->pch_forw; fsp->pcfs_frefs &&
		    pcp != (struct pcnode *)hp; pcp = npcp) {
			npcp = pcp -> pc_forw;
			vp = PCTOV(pcp);
			if (VFSTOPCFS(vp->v_vfsp) == fsp &&
			    !(pcp->pc_flags & PC_RELEHOLD)) {
				mutex_enter(&(vp)->v_lock);
				if (vp->v_count > 0) {
					mutex_exit(&(vp)->v_lock);
					continue;
				}
				mutex_exit(&(vp)->v_lock);
				VN_HOLD(vp);
				remque(pcp);
				vp->v_data = NULL;
				vp->v_vfsp = &EIO_vfs;
				vp->v_type = VBAD;
				VN_RELE(vp);
				if (!(pcp->pc_flags & PC_EXTERNAL)) {
					mutex_destroy(&(vp->v_lock));
					kmem_free(vp, sizeof (struct vnode));
				}
				kmem_free(pcp, sizeof (struct pcnode));
				fsp->pcfs_frefs --;
				fsp->pcfs_nrefs --;
			}
		}
	}
#ifdef undef
	if (fsp->pcfs_frefs) {
		rw_exit(&pcnodes_lock);
		panic("pc_diskchanged: frefs");
	}
	if (fsp->pcfs_nrefs) {
		rw_exit(&pcnodes_lock);
		panic("pc_diskchanged: nrefs");
	}
#endif
	if (fsp->pcfs_fatp != (uchar_t *)0) {
		pc_invalfat(fsp);
	} else {
		binval(fsp->pcfs_xdev);
	}
}
