/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)ufs_fs.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		ufs_fs.h
 *
 *   Description:	contains the definition for the UFS superblock,
 *			as well as filesystem-related macros, e.g.,
 *			SecPerTrack, Head, Sector, etc.
 *
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved.*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any 	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_UFS_FS_H
#define	_SYS_FS_UFS_FS_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Each disk drive contains some number of file systems.
 * A file system consists of a number of cylinder groups.
 * Each cylinder group has inodes and data.
 *
 * A file system is described by its super-block, which in turn
 * describes the cylinder groups.  The super-block is critical
 * data and is replicated in each cylinder group to protect against
 * catastrophic loss.  This is done at mkfs time and the critical
 * super-block data does not change, so the copies need not be
 * referenced further unless disaster strikes.
 *
 * For file system fs, the offsets of the various blocks of interest
 * are given in the super block as:
 *	[fs->fs_sblkno]		Super-block
 *	[fs->fs_cblkno]		Cylinder group block
 *	[fs->fs_iblkno]		Inode blocks
 *	[fs->fs_dblkno]		Data blocks
 * The beginning of cylinder group cg in fs, is given by
 * the ``cgbase(fs, cg)'' macro.
 *
 * The first boot and super blocks are given in absolute disk addresses.
 * The byte-offset forms are preferred, as they don't imply a sector size.
 */
#define	BBSIZE		8192
#define	SBSIZE		8192
#define	BBOFF		((off_t)(0))
#define	SBOFF		((off_t)(BBOFF + BBSIZE))
#define	BBLOCK		((daddr_t)(0))
#define	SBLOCK		((daddr_t)(BBLOCK + BBSIZE / DEV_BSIZE))

/*
 * Addresses stored in inodes are capable of addressing fragments
 * of `blocks'. File system blocks of at most size MAXBSIZE can
 * be optionally broken into 2, 4, or 8 pieces, each of which is
 * addressible; these pieces may be DEV_BSIZE, or some multiple of
 * a DEV_BSIZE unit.
 *
 * Large files consist of exclusively large data blocks.  To avoid
 * undue wasted disk space, the last data block of a small file may be
 * allocated as only as many fragments of a large block as are
 * necessary.  The file system format retains only a single pointer
 * to such a fragment, which is a piece of a single large block that
 * has been divided.  The size of such a fragment is determinable from
 * information in the inode, using the ``blksize(fs, ip, lbn)'' macro.
 *
 * The file system records space availability at the fragment level;
 * to determine block availability, aligned fragments are examined.
 *
 * The root inode is the root of the file system.
 * Inode 0 can't be used for normal purposes and
 * historically bad blocks were linked to inode 1,
 * thus the root inode is 2. (inode 1 is no longer used for
 * this purpose, however numerous dump tapes make this
 * assumption, so we are stuck with it)
 * The lost+found directory is given the next available
 * inode when it is created by ``mkfs''.
 */
#define	UFSROOTINO	((ino_t)2)	/* i number of all roots */
#define	LOSTFOUNDINO    (UFSROOTINO + 1)

#if 0					/* vla fornow..... */
/*
 * Mutex to control the number of active file system sync's
 */
extern kmutex_t ufs_syncbusy;
#endif					/* vla fornow..... */

/*
 * MINBSIZE is the smallest allowable block size.
 * In order to insure that it is possible to create files of size
 * 2^32 with only two levels of indirection, MINBSIZE is set to 4096.
 * MINBSIZE must be big enough to hold a cylinder group block,
 * thus changes to (struct cg) must keep its size within MINBSIZE.
 * Note that super blocks are always of size SBSIZE,
 * and that both SBSIZE and MAXBSIZE must be >= MINBSIZE.
 */
#define	MINBSIZE	4096

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in
 * the super block for this name.
 * The limit on the amount of summary information per file system
 * is defined by MAXCSBUFS. It is currently parameterized for a
 * maximum of two million cylinders.
 */
#define	MAXMNTLEN 512
#define	MAXCSBUFS 32

/*
 * Per cylinder group information; summarized in blocks allocated
 * from first cylinder group data blocks.  These blocks have to be
 * read in from fs_csaddr (size fs_cssize) in addition to the
 * super block.
 *
 * N.B. sizeof (struct csum) must be a power of two in order for
 * the ``fs_cs'' macro to work (see below).
 */
struct csum {
	long	cs_ndir;	/* number of directories */
	long	cs_nbfree;	/* number of free blocks */
	long	cs_nifree;	/* number of free inodes */
	long	cs_nffree;	/* number of free frags */
};

/*
 * In the 5.0 release, the file system state flag in the superblock (fs_clean)
 * is now used. The value of fs_clean can be:
 *	FSACTIVE	file system may have fsck inconsistencies
 *	FSCLEAN		file system has successfully unmounted (implies
 *			everything is ok)
 *	FSSTABLE	No fsck inconsistencies, no guarantee on user data
 *	FSBAD		file system is mounted from a partition that is
 *			neither FSCLEAN or FSSTABLE
 *	FSSUSPEND	Clean flag processing is temporarily disabled
 * Under this scheme, fsck can safely skip file systems that
 * are FSCLEAN or FSSTABLE.  To provide additional safeguard,
 * fs_clean information could be trusted only if
 * fs_state == FSOKAY - fs_time, where FSOKAY is a constant
 *
 * Note: mount(2) will now return ENOSPC if fs_clean is neither FSCLEAN nor
 * FSSTABLE, or fs_state is not valid.  The exceptions are the root or
 * the read-only partitions
 */

/*
 * Super block for a file system.
 *
 * Most of the data in the super block is read-only data and needs
 * no explicit locking to protect it. Exceptions are:
 *	fs_time
 *	fs_optim
 *	fs_cstotal
 *	fs_fmod
 *	fs_cgrotor
 * These fields require the use of fs->fs_lock.
 */
#define	FS_MAGIC	0x011954
#define	FSOKAY		0x7c269d38
/*
 * fs_clean values
 */
#define	FSACTIVE	((char)0)
#define	FSCLEAN		((char)0x1)
#define	FSSTABLE	((char)0x2)
#define	FSBAD		((char)0xff)	/* mounted !FSCLEAN and !FSSTABLE */
#define	FSSUSPEND	((char)0xfe)	/* temporarily suspended */

struct  fs {
 	struct fs _FAR_ *fs_link;		/* linked list of file systems */
	struct fs _FAR_ *fs_rlink;		/* used for incore super blocks */
	daddr_t  	fs_sblkno;		/* addr of super-block in filesys */
	daddr_t	fs_cblkno;		/* offset of cyl-block in filesys */
	daddr_t	fs_iblkno;		/* offset of inode-blocks in filesys */
	daddr_t	fs_dblkno;		/* offset of first data after cg */
	long	fs_cgoffset;		/* cylinder group offset in cylinder */
	long	fs_cgmask;		/* used to calc mod fs_ntrak */
	time_t	fs_time;		/* last time written */
	long	fs_size;		/* number of blocks in fs */
	long	fs_dsize;		/* number of data blocks in fs */
	long	fs_ncg;			/* number of cylinder groups */
	long	fs_bsize;		/* size of basic blocks in fs */
	long	fs_fsize;		/* size of frag blocks in fs */
	long	fs_frag;		/* number of frags in a block in fs */
/* these are configuration parameters */
	long	fs_minfree;		/* minimum percentage of free blocks */
	long	fs_rotdelay;		/* num of ms for optimal next block */
	long	fs_rps;			/* disk revolutions per second */
/* these fields can be computed from the others */
	long	fs_bmask;		/* ``blkoff'' calc of blk offsets */
	long	fs_fmask;		/* ``fragoff'' calc of frag offsets */
	long	fs_bshift;		/* ``lblkno'' calc of logical blkno */
	long	fs_fshift;		/* ``numfrags'' calc number of frags */
/* these are configuration parameters */
	long	fs_maxcontig;		/* max number of contiguous blks */
	long	fs_maxbpg;		/* max number of blks per cyl group */
/* these fields can be computed from the others */
	long	fs_fragshift;		/* block to frag shift */
	long	fs_fsbtodb;		/* fsbtodb and dbtofsb shift constant */
	long	fs_sbsize;		/* actual size of super block */
	long	fs_csmask;		/* csum block offset */
	long	fs_csshift;		/* csum block number */
	long	fs_nindir;		/* value of NINDIR */
	long	fs_inopb;		/* value of INOPB */
	long	fs_nspf;		/* value of NSPF */
/* yet another configuration parameter */
	long	fs_optim;		/* optimization preference, see below */
/* these fields are derived from the hardware */
	long	fs_npsect;		/* # sectors/track including spares */
	long	fs_interleave;		/* hardware sector interleave */
	long	fs_trackskew;		/* sector 0 skew, per track */
/* a unique id for this filesystem (currently unused and unmaintained) */
/* In 4.3 Tahoe this space is used by fs_headswitch and fs_trkseek */
/* Neither of those fields is used in the Tahoe code right now but */
/* there could be problems if they are.				*/
	long	fs_id[2];		/* file system id */
/* sizes determined by number of cylinder groups and their sizes */
	daddr_t	fs_csaddr;		/* blk addr of cyl grp summary area */
	long	fs_cssize;		/* size of cyl grp summary area */
	long	fs_cgsize;		/* cylinder group size */
/* these fields are derived from the hardware */
	long	fs_ntrak;		/* tracks per cylinder */
	long	fs_nsect;		/* sectors per track */
	long	fs_spc;			/* sectors per cylinder */
/* this comes from the disk driver partitioning */
	long	fs_ncyl;		/* cylinders in file system */
/* these fields can be computed from the others */
	long	fs_cpg;			/* cylinders per group */
	long	fs_ipg;			/* inodes per group */
	long	fs_fpg;			/* blocks per group * fs_frag */
/* this data must be re-computed after crashes */
	struct	csum fs_cstotal;	/* cylinder summary information */
/* these fields are cleared at mount time */
	char	fs_fmod;		/* super block modified flag */
	char	fs_clean;		/* file system state flag */
	char	fs_ronly;		/* mounted read-only flag */
	char	fs_flags;		/* currently unused flag */
	char	fs_fsmnt[MAXMNTLEN];	/* name mounted on */
/* these fields retain the current block allocation info */
	long	fs_cgrotor;		/* last cg searched */
	struct csum _FAR_ *fs_csp[MAXCSBUFS];	/* list of fs_cs info buffers */
	long	fs_cpc;			/* cyl per cycle in postbl */
	short	fs_opostbl[16][8];	/* old rotation block list head */
	long	fs_sparecon[55];	/* reserved for future constants */
#define	fs_ntime fs_sparecon[54]	/* INCORE only; time in nanoseconds */
	long	fs_state;		/* file system state time stamp */
	quad	fs_qbmask;		/* ~fs_bmask - for use with quad size */
	quad	fs_qfmask;		/* ~fs_fmask - for use with quad size */
	long	fs_postblformat;	/* format of positional layout tables */
	long	fs_nrpos;		/* number of rotaional positions */
	long	fs_postbloff;		/* (short) rotation block list head */
	long	fs_rotbloff;		/* (u_char) blocks for each rotation */
	long	fs_magic;		/* magic number */
	u_char	fs_space[1];		/* list of blocks for each rotation */
/* actually longer */
};

/*
 * Preference for optimization.
 */
#define	FS_OPTTIME	0	/* minimize allocation time */
#define	FS_OPTSPACE	1	/* minimize disk fragmentation */

/*
 * Rotational layout table format types
 */
#define	FS_42POSTBLFMT		-1	/* 4.2BSD rotational table format */
#define	FS_DYNAMICPOSTBLFMT	1	/* dynamic rotational table format */

/*
 * Macros for access to superblock array structures
 */
#ifdef _KERNEL
#define	fs_postbl(ufsvfsp, cylno) \
	(((ufsvfsp)->vfs_fs->fs_postblformat != FS_DYNAMICPOSTBLFMT) \
	? ((ufsvfsp)->vfs_fs->fs_opostbl[cylno]) \
	: ((short _FAR_ *)((char _FAR_ *)(ufsvfsp)->vfs_fs + \
	(ufsvfsp)->vfs_fs->fs_postbloff) \
	+ (cylno) * (ufsvfsp)->vfs_nrpos))
#else
#define	fs_postbl(fs, cylno) \
	(((fs)->fs_postblformat != FS_DYNAMICPOSTBLFMT) \
	? ((fs)->fs_opostbl[cylno]) \
	: ((short _FAR_ *)((char _FAR_ *)(fs) + \
	(fs)->fs_postbloff) \
	+ (cylno) * (fs)->fs_nrpos))
#endif

#define	fs_rotbl(fs) \
	(((fs)->fs_postblformat != FS_DYNAMICPOSTBLFMT) \
	? ((fs)->fs_space) \
	: ((u_char _FAR_ *)((char _FAR_ *)(fs) + (fs)->fs_rotbloff)))

/*
 * Convert cylinder group to base address of its global summary info.
 *
 * N.B. This macro assumes that sizeof (struct csum) is a power of two.
 */

#define	fs_cs(fs, indx) \
	fs_csp[(indx) >> (fs)->fs_csshift][(indx) & ~(fs)->fs_csmask]

/*
 * Cylinder group block for a file system.
 *
 * Writable fields in the cylinder group are protected by the associated
 * super block lock fs->fs_lock.
 */
#define	CG_MAGIC	0x090255
struct	cg {
	struct	cg _FAR_ *cg_link;		/* linked list of cyl groups */
	long	cg_magic;		/* magic number */
	time_t	cg_time;		/* time last written */
	long	cg_cgx;			/* we are the cgx'th cylinder group */
	short	cg_ncyl;		/* number of cyl's this cg */
	short	cg_niblk;		/* number of inode blocks this cg */
	long	cg_ndblk;		/* number of data blocks this cg */
	struct	csum cg_cs;		/* cylinder summary information */
	long	cg_rotor;		/* position of last used block */
	long	cg_frotor;		/* position of last used frag */
	long	cg_irotor;		/* position of last used inode */
	long	cg_frsum[MAXFRAG];	/* counts of available frags */
	long	cg_btotoff;		/* (long) block totals per cylinder */
	long	cg_boff;		/* (short) free block positions */
	long	cg_iusedoff;		/* (char) used inode map */
	long	cg_freeoff;		/* (u_char) free block map */
	long	cg_nextfreeoff;		/* (u_char) next available space */
	long	cg_sparecon[16];	/* reserved for future use */
	u_char	cg_space[1];		/* space for cylinder group maps */
/* actually longer */
};
/*
 * Macros for access to cylinder group array structures
 */

#define	cg_blktot(cgp) \
	(((cgp)->cg_magic != CG_MAGIC) \
	? (((struct ocg _FAR_ *)(cgp))->cg_btot) \
	: ((long _FAR_ *)((char _FAR_ *)(cgp) + (cgp)->cg_btotoff)))

#ifdef _KERNEL
#define	cg_blks(ufsvfsp, cgp, cylno) \
	(((cgp)->cg_magic != CG_MAGIC) \
	? (((struct ocg _FAR_ *)(cgp))->cg_b[cylno]) \
	: ((short _FAR_ *)((char _FAR_ *)(cgp) + (cgp)->cg_boff) + \
	(cylno) * (ufsvfsp)->vfs_nrpos))
#else
#define	cg_blks(fs, cgp, cylno) \
	(((cgp)->cg_magic != CG_MAGIC) \
	? (((struct ocg _FAR_ *)(cgp))->cg_b[cylno]) \
	: ((short _FAR_ *)((char _FAR_ *)(cgp) + (cgp)->cg_boff) + \
	(cylno) * (fs)->fs_nrpos))
#endif

#define	cg_inosused(cgp) \
	(((cgp)->cg_magic != CG_MAGIC) \
	? (((struct ocg _FAR_ *)(cgp))->cg_iused) \
	: ((char _FAR_ *)((char _FAR_ *)(cgp) + (cgp)->cg_iusedoff)))

#define	cg_blksfree(cgp) \
	(((cgp)->cg_magic != CG_MAGIC) \
	? (((struct ocg _FAR_ *)(cgp))->cg_free) \
	: ((u_char _FAR_ *)((char _FAR_ *)(cgp) + (cgp)->cg_freeoff)))

#define	cg_chkmagic(cgp) \
	((cgp)->cg_magic == CG_MAGIC || \
	((struct ocg _FAR_ *)(cgp))->cg_magic == CG_MAGIC)

/*
 * The following structure is defined
 * for compatibility with old file systems.
 */
struct	ocg {
	struct	ocg _FAR_ *cg_link;		/* linked list of cyl groups */
	struct	ocg _FAR_ *cg_rlink;		/* used for incore cyl groups */
	time_t	cg_time;		/* time last written */
	long	cg_cgx;			/* we are the cgx'th cylinder group */
	short	cg_ncyl;		/* number of cyl's this cg */
	short	cg_niblk;		/* number of inode blocks this cg */
	long	cg_ndblk;		/* number of data blocks this cg */
	struct	csum cg_cs;		/* cylinder summary information */
	long	cg_rotor;		/* position of last used block */
	long	cg_frotor;		/* position of last used frag */
	long	cg_irotor;		/* position of last used inode */
	long	cg_frsum[8];		/* counts of available frags */
	long	cg_btot[32];		/* block totals per cylinder */
	short	cg_b[32][8];		/* positions of free blocks */
	char	cg_iused[256];		/* used inode map */
	long	cg_magic;		/* magic number */
	u_char	cg_free[1];		/* free block map */
/* actually longer */
};

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define	fsbtodb(fs, b)	((b) << (fs)->fs_fsbtodb)

#define	dbtofsb(fs, b)	((b) >> (fs)->fs_fsbtodb)

/*
 * Cylinder group macros to locate things in cylinder groups.
 * They calc file system addresses of cylinder group data structures.
 */
#define	cgbase(fs, c)	((daddr_t)((fs)->fs_fpg * (c)))

#define	cgstart(fs, c) \
	(cgbase(fs, c) + (fs)->fs_cgoffset * ((c) & ~((fs)->fs_cgmask)))

#define	cgsblock(fs, c)	(cgstart(fs, c) + (fs)->fs_sblkno)	/* super blk */

#define	cgtod(fs, c)	(cgstart(fs, c) + (fs)->fs_cblkno)	/* cg block */

#define	cgimin(fs, c)	(cgstart(fs, c) + (fs)->fs_iblkno)	/* inode blk */

#define	cgdmin(fs, c)	(cgstart(fs, c) + (fs)->fs_dblkno)	/* 1st data */

/*
 * Macros for handling inode numbers:
 *	inode number to file system block offset.
 *	inode number to cylinder group number.
 *	inode number to file system block address.
 */
#define	itoo(fs, x)	((x) % (u_long)INOPB(fs))

#define	itog(fs, x)	((x) / (u_long)(fs)->fs_ipg)

#define	itod(fs, x) \
	((daddr_t)(cgimin(fs, itog(fs, x)) + \
	(blkstofrags((fs), (((x)%(u_long)(fs)->fs_ipg)/(u_long)INOPB(fs))))))

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	dtog(fs, d)	((d) / (fs)->fs_fpg)
#define	dtogd(fs, d)	((d) % (fs)->fs_fpg)

/*
 * Extract the bits for a block from a map.
 * Compute the cylinder and rotational position of a cyl block addr.
 */
#define	blkmap(fs, map, loc) \
	(((map)[(loc) / NBBY] >> ((loc) % NBBY)) & \
	(0xff >> (NBBY - (fs)->fs_frag)))

#define	cbtocylno(fs, bno) \
	((bno) * NSPF(fs) / (fs)->fs_spc)

#ifdef _KERNEL
#define	cbtorpos(ufsvfsp, bno) \
	(((((bno) * NSPF((ufsvfsp)->vfs_fs) % (ufsvfsp)->vfs_fs->fs_spc) % \
	(ufsvfsp)->vfs_fs->fs_nsect) % (ufsvfsp)->vfs_fs->fs_nsect) * \
	(ufsvfsp)->vfs_nrpos) / (ufsvfsp)->vfs_fs->fs_nsect
#else
#define	cbtorpos(fs, bno) \
	(((((bno) * NSPF(fs) % (fs)->fs_spc) % \
	(fs)->fs_nsect) % (fs)->fs_nsect) * \
	(fs)->fs_nrpos) / (fs)->fs_nsect
#endif

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */

#define	blkoff(fs, loc)		/* calculates (loc % fs->fs_bsize) */ \
	((loc) & ~(fs)->fs_bmask)

#define	fragoff(fs, loc)	/* calculates (loc % fs->fs_fsize) */ \
	((loc) & ~(fs)->fs_fmask)

#define	lblkno(fs, loc)		/* calculates (loc / fs->fs_bsize) */ \
	((loc) >> (fs)->fs_bshift)

#define	numfrags(fs, loc)	/* calculates (loc / fs->fs_fsize) */ \
	((loc) >> (fs)->fs_fshift)

#define	blkroundup(fs, size)	/* calculates roundup(size, fs->fs_bsize) */ \
	(((size) + (fs)->fs_bsize - 1) & (fs)->fs_bmask)

#define	fragroundup(fs, size)	/* calculates roundup(size, fs->fs_fsize) */ \
	(((size) + (fs)->fs_fsize - 1) & (fs)->fs_fmask)

#define	fragstoblks(fs, frags)	/* calculates (frags / fs->fs_frag) */ \
	((frags) >> (fs)->fs_fragshift)

#define	blkstofrags(fs, blks)	/* calculates (blks * fs->fs_frag) */ \
	((blks) << (fs)->fs_fragshift)

#define	fragnum(fs, fsb)	/* calculates (fsb % fs->fs_frag) */ \
	((fsb) & ((fs)->fs_frag - 1))

#define	blknum(fs, fsb)		/* calculates rounddown(fsb, fs->fs_frag) */ \
	((fsb) &~ ((fs)->fs_frag - 1))

/*
 * Determine the number of available frags given a
 * percentage to hold in reserve
 */
#define	freespace(fs, percentreserved) \
	(blkstofrags((fs), (fs)->fs_cstotal.cs_nbfree) + \
	(fs)->fs_cstotal.cs_nffree - ((fs)->fs_dsize / 100 * (percentreserved)))

/*
 * Determining the size of a file block in the file system.
 */

#define	blksize(fs, ip, lbn) \
	(((lbn) >= NDADDR || (ip)->i_size >= ((lbn) + 1) << (fs)->fs_bshift) \
	    ? (fs)->fs_bsize \
	    : (fragroundup(fs, blkoff(fs, (ip)->i_size))))

#define	dblksize(fs, dip, lbn) \
	(((lbn) >= NDADDR || (dip)->di_size >= ((lbn) + 1) << (fs)->fs_bshift) \
	    ? (fs)->fs_bsize \
	    : (fragroundup(fs, blkoff(fs, (dip)->di_size))))

/*
 * Number of disk sectors per block; assumes DEV_BSIZE byte sector size.
 */
#define	NSPB(fs)	((fs)->fs_nspf << (fs)->fs_fragshift)
#define	NSPF(fs)	((fs)->fs_nspf)

/*
 * INOPB is the number of inodes in a secondary storage block.
 */
#define	INOPB(fs)	((fs)->fs_inopb)
#define	INOPF(fs)	((fs)->fs_inopb >> (fs)->fs_fragshift)

/*
 * NINDIR is the number of indirects in a file system block.
 */
#define	NINDIR(fs)	((fs)->fs_nindir)

/*
 * bit map related macros
 */
#define	setbit(a, i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a, i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a, i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a, i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#define	getfs(vfsp) \
	((struct fs _FAR_ *)((struct ufsvfs _FAR_ *)vfsp->vfs_data)->vfs_bufp->b_un.b_addr)

/*
 * at386 - specific macros for use with ROM BIOS
 */

#define SecPerCyl ( realp->bootfrom.ufs.secPerTrk * realp->bootfrom.ufs.trkPerCyl )
#define SecPerTrk realp->bootfrom.ufs.secPerTrk
#define Head( block )	( ( block % SecPerCyl ) / SecPerTrk )
#define Track( block )	( block / SecPerCyl )
#define Sector( block )	( ( ( block % SecPerCyl ) % SecPerTrk ) + 1 )

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_FS_H */

