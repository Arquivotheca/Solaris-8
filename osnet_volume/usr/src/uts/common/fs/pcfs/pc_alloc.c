/*
 * Copyright (c) 1989, 1992-1994, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pc_alloc.c	1.22	99/11/01 SMI"

/*
 * Routines to allocate and deallocate data blocks on the disk
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>

static pc_cluster32_t pc_getcluster(struct pcfs *fsp, pc_cluster32_t cn);

/*
 * Convert file logical block (cluster) numbers to disk block numbers.
 * Also return number of physically contiguous blocks if asked for.
 * Used for reading only. Use pc_balloc for writing.
 */
int
pc_bmap(
	struct pcnode *pcp,		/* pcnode for file */
	daddr_t lcn,			/* logical cluster no */
	daddr_t *dbnp,			/* ptr to phys block no */
	uint_t *contigbp)		/* ptr to number of contiguous bytes */
					/* may be zero if not wanted */
{
	struct pcfs *fsp;	/* pcfs that file is in */
	struct vnode *vp;
	pc_cluster32_t cn, ncn;		/* current, next cluster number */

	PC_DPRINTF2(6, "pc_bmap: pcp=0x%p, lcn=%ld\n", (void *)pcp, lcn);

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	if (lcn < 0)
		return (ENOENT);
	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		daddr_t lbn, bn; /* logical (disk) block number */

		lbn = pc_cltodb(fsp, lcn);
		if (lbn >= fsp->pcfs_rdirsec) {
			PC_DPRINTF0(2, "pc_bmap: ENOENT1\n");
			return (ENOENT);
		}
		bn = fsp->pcfs_rdirstart + lbn;
		*dbnp = pc_dbdaddr(fsp, bn);
		if (contigbp) {
			ASSERT (*contigbp >= fsp->pcfs_secsize);
			*contigbp = MIN(*contigbp,
			    fsp->pcfs_secsize * (fsp->pcfs_rdirsec - lbn));
		}
	} else {

		if (lcn >= fsp->pcfs_ncluster) {
			PC_DPRINTF0(2, "pc_bmap: ENOENT2\n");
			return (ENOENT);
		}
		if (vp->v_type == VREG &&
		    (pcp->pc_size == 0 ||
		    lcn >= howmany(pcp->pc_size, fsp->pcfs_clsize))) {
			PC_DPRINTF0(2, "pc_bmap: ENOENT3\n");
			return (ENOENT);
		}
		ncn = pcp->pc_scluster;
		if (IS_FAT32(fsp) && ncn == 0)
			ncn = fsp->pcfs_rdirstart;
		do {
			cn = ncn;
			if (!pc_validcl(fsp, cn)) {
				if (IS_FAT32(fsp) && cn >= PCF_LASTCLUSTER32 &&
				    vp->v_type == VDIR) {
					PC_DPRINTF0(2, "pc_bmap: ENOENT4\n");
					return (ENOENT);
				} else if (!IS_FAT32(fsp) &&
				    cn >= PCF_LASTCLUSTER &&
				    vp->v_type == VDIR) {
					PC_DPRINTF0(2, "pc_bmap: ENOENT5\n");
					return (ENOENT);
				} else {
					PC_DPRINTF1(1,
					    "pc_bmap: badfs cn=%d\n", cn);
					(void) pc_badfs(fsp);
					return (EIO);
				}
			}
			ncn = pc_getcluster(fsp, cn);
		} while (lcn--);
		*dbnp = pc_cldaddr(fsp, cn);

		if (contigbp && *contigbp > fsp->pcfs_clsize) {
			uint_t count = fsp->pcfs_clsize;

			while ((cn + 1) == ncn && count < *contigbp &&
			    pc_validcl(fsp, ncn)) {
				count += fsp->pcfs_clsize;
				cn = ncn;
				ncn = pc_getcluster(fsp, ncn);
			}
			*contigbp = count;
		}
	}
	return (0);
}

/*
 * Allocate file logical blocks (clusters).
 * Return disk address of last allocated cluster.
 */
int
pc_balloc(
	struct pcnode *pcp,	/* pcnode for file */
	daddr_t lcn,		/* logical cluster no */
	int zwrite,			/* zerofill blocks? */
	daddr_t *dbnp)			/* ptr to phys block no */
{
	struct pcfs *fsp;	/* pcfs that file is in */
	struct vnode *vp;

	PC_DPRINTF2(5, "pc_balloc: pcp=0x%p, lcn=%ld\n", (void *)pcp, lcn);

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp -> v_vfsp);

	if (lcn < 0) {
		return (EFBIG);
	}

	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		daddr_t lbn;

		lbn = pc_cltodb(fsp, lcn);
		if (lbn >= fsp->pcfs_rdirsec)
			return (ENOSPC);
		*dbnp = pc_dbdaddr(fsp, fsp->pcfs_rdirstart + lbn);
	} else {
		pc_cluster32_t cn;	/* current cluster number */
		pc_cluster32_t ncn;	/* next cluster number */

		if (lcn >= fsp->pcfs_ncluster)
			return (ENOSPC);
		if ((vp->v_type == VREG && pcp->pc_size == 0) ||
		    (vp->v_type == VDIR && lcn == 0)) {
			switch (cn = pc_alloccluster(fsp, 1)) {
			case PCF_FREECLUSTER:
				return (ENOSPC);
			case PCF_ERRORCLUSTER:
				return (EIO);
			}
			pcp->pc_scluster = cn;
		} else {
			cn = pcp->pc_scluster;
			if (IS_FAT32(fsp) && cn == 0)
				cn = fsp->pcfs_rdirstart;
			if (!pc_validcl(fsp, cn)) {
				PC_DPRINTF1(1, "pc_balloc: badfs cn=%d\n", cn);
				(void) pc_badfs(fsp);
				return (EIO);
			}
		}
		while (lcn-- > 0) {
			ncn = pc_getcluster(fsp, cn);
			if ((IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER32) ||
			    (!IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER)) {
				/*
				 * Extend file (no holes).
				 */
				switch (ncn = pc_alloccluster(fsp, zwrite)) {
				case PCF_FREECLUSTER:
					return (ENOSPC);
				case PCF_ERRORCLUSTER:
					return (EIO);
				}
				pc_setcluster(fsp, cn, ncn);
			} else if (!pc_validcl(fsp, ncn)) {
				PC_DPRINTF1(1,
				    "pc_balloc: badfs ncn=%d\n", ncn);
				(void) pc_badfs(fsp);
				return (EIO);
			}
			cn = ncn;
		}
		*dbnp = pc_cldaddr(fsp, cn);
	}
	return (0);
}

/*
 * Free file cluster chain after the first skipcl clusters.
 */
int
pc_bfree(struct pcnode *pcp, pc_cluster32_t skipcl)
{
	struct pcfs *fsp;
	pc_cluster32_t cn;
	pc_cluster32_t ncn;
	int n;
	struct vnode *vp;

	vp = PCTOV(pcp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	if (!IS_FAT32(fsp) && (vp->v_flag & VROOT)) {
		panic("pc_bfree");
	}

	PC_DPRINTF2(5, "pc_bfree: pcp=0x%p, after first %d clusters\n",
	    (void *)pcp, skipcl);

	if (pcp->pc_size == 0 && vp->v_type == VREG) {
		return (0);
	}
	if (vp->v_type == VREG) {
		n = howmany(pcp->pc_size, fsp->pcfs_clsize);
		if (n > fsp->pcfs_ncluster) {
			PC_DPRINTF1(1, "pc_bfree: badfs n=%d\n", n);
			(void) pc_badfs(fsp);
			return (EIO);
		}
	} else {
		n = fsp->pcfs_ncluster;
	}
	cn = pcp->pc_scluster;
	if (IS_FAT32(fsp) && cn == 0)
		cn = fsp->pcfs_rdirstart;
	if (skipcl == 0) {
		if (IS_FAT32(fsp))
			pcp->pc_scluster = PCF_LASTCLUSTERMARK32;
		else
			pcp->pc_scluster = PCF_LASTCLUSTERMARK;
	}

	while (n--) {
		if (!pc_validcl(fsp, cn)) {
			PC_DPRINTF1(1, "pc_bfree: badfs cn=%d\n", cn);
			(void) pc_badfs(fsp);
			return (EIO);
		}
		ncn = pc_getcluster(fsp, cn);
		if (skipcl == 0) {
			pc_setcluster(fsp, cn, PCF_FREECLUSTER);
		} else {
			skipcl--;
			if (skipcl == 0) {
				if (IS_FAT32(fsp)) {
					pc_setcluster(fsp, cn,
					    PCF_LASTCLUSTERMARK32);
				} else
					pc_setcluster(fsp, cn,
					    PCF_LASTCLUSTERMARK);
			}
		}
		if (IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER32 &&
		    vp->v_type == VDIR)
			break;
		if (!IS_FAT32(fsp) && ncn >= PCF_LASTCLUSTER &&
		    vp->v_type == VDIR)
			break;
		cn = ncn;
	}
	return (0);
}

/*
 * Return the number of free blocks in the filesystem.
 */
int
pc_freeclusters(struct pcfs *fsp)
{
	pc_cluster32_t cn;
	int free;

	/*
	 * make sure the FAT is in core
	 */
	free = 0;
	for (cn = PCF_FIRSTCLUSTER;
	    (int)cn < fsp->pcfs_ncluster + PCF_FIRSTCLUSTER; cn++) {
		if (pc_getcluster(fsp, cn) == PCF_FREECLUSTER) {
			free++;
		}
	}
	return (free);
}

/*
 * Cluster manipulation routines.
 * FAT must be resident.
 */

/*
 * Get the next cluster in the file cluster chain.
 *	cn = current cluster number in chain
 */
static pc_cluster32_t
pc_getcluster(struct pcfs *fsp, pc_cluster32_t cn)
{
	unsigned char *fp;

	PC_DPRINTF1(7, "pc_getcluster: cn=%x ", cn);
	if (fsp->pcfs_fatp == (uchar_t *)0 || !pc_validcl(fsp, cn))
		panic("pc_getcluster");

	if (IS_FAT32(fsp)) {	/* 32 bit FAT */
		fp = fsp->pcfs_fatp + (cn << 2);
		cn = ltohi(*(pc_cluster32_t *)fp);
	} else if (fsp->pcfs_flags & PCFS_FAT16) {	/* 16 bit FAT */
		fp = fsp->pcfs_fatp + (cn << 1);
		cn = ltohs(*(pc_cluster16_t *)fp);
	} else {	/* 12 bit FAT */
		fp = fsp->pcfs_fatp + (cn + (cn >> 1));
		if (cn & 01) {
			cn = (((unsigned int)*fp++ & 0xf0) >> 4);
			cn += (*fp << 4);
		} else {
			cn = *fp++;
			cn += ((*fp & 0x0f) << 8);
		}
		if (cn >= PCF_12BCLUSTER)
			cn |= PCF_RESCLUSTER;
	}
	PC_DPRINTF1(7, " %x\n", cn);
	return (cn);
}

/*
 * Set a cluster in the FAT to a value.
 *	cn = cluster number to be set in FAT
 *	ncn = new value
 */
void
pc_setcluster(struct pcfs *fsp, pc_cluster32_t cn, pc_cluster32_t ncn)
{
	unsigned char *fp;

	PC_DPRINTF2(7, "pc_setcluster: cn=%d ncn=%d\n", cn, ncn);
	if (fsp->pcfs_fatp == (uchar_t *)0 || !pc_validcl(fsp, cn))
		panic("pc_setcluster");
	fsp->pcfs_flags |= PCFS_FATMOD;
	pc_mark_fat_updated(fsp, cn);
	if (IS_FAT32(fsp)) {	/* 32 bit FAT */
		fp = fsp->pcfs_fatp + (cn << 2);
		*(pc_cluster32_t *)fp = htoli(ncn);
	} else if (fsp->pcfs_flags & PCFS_FAT16) {	/* 16 bit FAT */
		pc_cluster16_t	ncn16;

		fp = fsp->pcfs_fatp + (cn << 1);
		ncn16 = (pc_cluster16_t)ncn;
		*(pc_cluster16_t *)fp = htols(ncn16);
	} else {	/* 12 bit FAT */
		fp = fsp->pcfs_fatp + (cn + (cn >> 1));
		if (cn & 01) {
			*fp = (*fp & 0x0f) | ((ncn << 4) & 0xf0);
			fp++;
			*fp = (ncn >> 4) & 0xff;
		} else {
			*fp++ = ncn & 0xff;
			*fp = (*fp & 0xf0) | ((ncn >> 8) & 0x0f);
		}
	}
	if (ncn == PCF_FREECLUSTER) {
		fsp->pcfs_nxfrecls = PCF_FIRSTCLUSTER;
		if (IS_FAT32(fsp)) {
			if (fsp->fsinfo_native.fs_free_clusters !=
			    FSINFO_UNKNOWN)
				fsp->fsinfo_native.fs_free_clusters++;
		}
	}
}

/*
 * Allocate a new cluster.
 */
pc_cluster32_t
pc_alloccluster(
	struct pcfs *fsp,	/* file sys to allocate in */
	int zwrite)			/* boolean for writing zeroes */
{
	pc_cluster32_t cn;
	int	error;

	if (fsp->pcfs_fatp == (uchar_t *)0)
		panic("pc_addcluster: no FAT");

	for (cn = fsp->pcfs_nxfrecls;
	    (int)cn < fsp->pcfs_ncluster + PCF_FIRSTCLUSTER; cn++) {
		if (pc_getcluster(fsp, cn) == PCF_FREECLUSTER) {
			struct buf *bp;

			if (IS_FAT32(fsp)) {
				pc_setcluster(fsp, cn, PCF_LASTCLUSTERMARK32);
				if (fsp->fsinfo_native.fs_free_clusters !=
				    FSINFO_UNKNOWN)
					fsp->fsinfo_native.fs_free_clusters--;
			} else
				pc_setcluster(fsp, cn, PCF_LASTCLUSTERMARK);
			if (zwrite) {
				/*
				 * zero the new cluster
				 */
				bp = ngeteblk(fsp->pcfs_clsize);
				bp->b_edev = fsp->pcfs_xdev;
				bp->b_dev = cmpdev(bp->b_edev);
				bp->b_blkno = pc_cldaddr(fsp, cn);
				clrbuf(bp);
				bwrite2(bp);
				error = geterror(bp);
				brelse(bp);
				if (error) {
					PC_DPRINTF0(1,
					    "pc_alloccluster: error\n");
					pc_mark_irrecov(fsp);
					return (PCF_ERRORCLUSTER);
				}
			}
			fsp->pcfs_nxfrecls = cn + 1;
			PC_DPRINTF1(5, "pc_alloccluster: new cluster = %d\n",
			    cn);
			return (cn);
		}
	}
	return (PCF_FREECLUSTER);
}

/*
 * Get the number of clusters used by a file or subdirectory
 */
int
pc_fileclsize(
	struct pcfs *fsp,
	pc_cluster32_t strtcluster)
{
	int count = 0;

	while (pc_validcl(fsp, strtcluster)) {
		count++;
		strtcluster = pc_getcluster(fsp, strtcluster);
	}
	return (count);
}
