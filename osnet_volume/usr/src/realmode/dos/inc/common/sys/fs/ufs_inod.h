/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)ufs_inod.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		ufs_inod(e).h
 *
 *   Description:	contains data structures pertaining to the UFS inode.
 *
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_UFS_INODE_H
#define	_SYS_FS_UFS_INODE_H

#include <sys/fs/ufs_fs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

/*
 * The I node is the focus of all local file activity in UNIX.
 * There is a unique inode allocated for each active file,
 * each current directory, each mounted-on file, each mapping,
 * and the root.  An inode is `named' by its dev/inumber pair.
 * Data in icommon is read in from permanent inode on volume.
 *
 * Each inode has 5 locks associated with it:
 *	i_rwlock:	Serializes ufs_write and ufs_setattr request
 *			and allows ufs_read requests to proceed in parallel.
 *			Serializes reads/updates to directories.
 *	i_contents:	Protects almost all of the fields in the inode
 *			except for those list below.
 *	i_tlock:	Protects just the i_atime, i_mtime, i_ctime,
 *			i_delayoff, i_delaylen, i_nextrio, i_writes
 *			and i_flag fields.
 *	icache_lock:	Protects inode cache and the i_forw/i_back/i_number
 *			and i_dev fields of each inode.
 *	ifree_lock:	Protects inode freelist and the i_freef/ifreeb
 *			fields of each inode.
 * Lock ordering:
 *	icache_lock > i_rwlock > i_contents > i_tlock > ifree_lock
 */


#define	UID_LONG  (o_uid_t)65535 /* flag value to use i_luid value as master */
#define	GID_LONG  (o_uid_t)65535 /* flag value to use i_lgid value as master */

#define	NDADDR	12		/* direct addresses in inode */
#define	NIADDR	3		/* indirect addresses in inode */
#define	FSL_SIZE (NDADDR+NIADDR-1) * sizeof (daddr_t)
				/* max fast symbolic name length is 56 */

#define	i_fs	i_ufsvfs->vfs_bufp->b_un.b_fs

struct 	icommon {
	o_mode_t ic_smode;	/*  0: mode and type of file */
	short	ic_nlink;	/*  2: number of links to file */
	o_uid_t	ic_suid;	/*  4: owner's user id */
	o_gid_t	ic_sgid;	/*  6: owner's group id */
	quad	ic_size;	/*  8: number of bytes in file */
#ifdef _KERNEL
	struct timeval ic_atime; /* 16: time last accessed */
	struct timeval ic_mtime; /* 24: time last modified */
	struct timeval ic_ctime; /* 32: last time inode changed */
#else
	time_t	ic_atime;	/* 16: time last accessed */
	long	ic_atspare;
	time_t	ic_mtime;	/* 24: time last modified */
	long	ic_mtspare;
	time_t	ic_ctime;	/* 32: last time inode changed */
	long	ic_ctspare;
#endif
	daddr_t	ic_db[NDADDR];	/* 40: disk block addresses */
	daddr_t	ic_ib[NIADDR];	/* 88: indirect blocks */
	long	ic_flags;	/* 100: status, currently unused */
	long	ic_blocks;	/* 104: blocks actually held */
	long	ic_gen;		/* 108: generation number */
	long	ic_mode_reserv; /* 112: placeholder for 32 bit mode */
	uid_t	ic_uid;		/* 116: long EFT version of uid */
	gid_t	ic_gid;		/* 120: long EFT version of gid */
	ulong	ic_oeftflag;	/* 124: zero in fsck for migration */
				/* used in idr1.0, 1.1 EFT version */
				/* 124, 4+4 bytes available */
};

struct inode {
	struct	inode _FAR_ *i_chain[2];	/* must be first */
	struct	vnode i_vnode;	/* vnode associated with this inode */
	struct	vnode _FAR_ *i_devvp;	/* vnode for block I/O */
	u_short	i_flag;
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* i number, 1-to-1 with device address */
	off_t	i_diroff;	/* offset in dir, where we found last entry */
	struct	ufsvfs _FAR_ *i_ufsvfs; /* incore fs associated with inode */
	struct	dquot _FAR_ *i_dquot;	/* quota structure controlling this file */
	krwlock_t i_rwlock;	/* serializes write/setattr requests */
	krwlock_t i_contents;	/* protects (most of) inode contents */
	kmutex_t i_tlock;	/* protects time fields, i_flag */
	daddr_t	i_nextr;	/*
				 * next byte read offset (read-ahead)
				 *   No lock required
				 */
	struct inode  _FAR_ *i_freef;	/* free list forward */
	struct inode _FAR_ **i_freeb;	/* free list back */
	ulong	i_vcode;	/* version code attribute */
	long	i_mapcnt;	/* mappings to file pages */
   long	_FAR_ *i_map;		/* block list for the corresponding file */
	long	i_rdev;		/* INCORE rdev from i_oldrdev by ufs_iget */
				/*
				 * LMXXX - if the async stuff sticks around,
				 * replace i_delay{len,off} w/ a async_req*.
				 */
	long	i_delaylen;	/* delayed writes, units=bytes */
	long	i_delayoff;	/* where we started delaying */
	long	i_nextrio;	/* where to start the next clust */
	long	i_writes;	/* number of outstanding bytes in write q */
	kcondvar_t i_wrcv;	/* sleep/wakeup for write throttle */
	struct 	icommon	i_ic;
};

struct dinode {
	union {
		struct	icommon di_icom;
		char	di_size[128];
	} di_un;
};

#define	i_mode		i_ic.ic_smode 	/* retain short mode for now */
#define	i_nlink		i_ic.ic_nlink
#define	i_uid		i_ic.ic_uid
#define	i_gid		i_ic.ic_gid
#define	i_smode		i_ic.ic_smode
#define	i_suid		i_ic.ic_suid
#define	i_sgid		i_ic.ic_sgid


#if 1					/* vla fornow..... */
#define	i_size		i_ic.ic_size.val[0]
#endif					/* vla fornow..... */
/* ugh! -- must be fixed */
#if defined(vax) || defined(i386)
#define	i_size		i_ic.ic_size.val[0]
#endif
#if defined(mc68000) || defined(sparc) || defined(u3b2) || defined(u3b15)
#define	i_size		i_ic.ic_size.val[1]
#endif
#define	i_db		i_ic.ic_db
#define	i_ib		i_ic.ic_ib

#define	i_atime		i_ic.ic_atime
#define	i_mtime		i_ic.ic_mtime
#define	i_ctime		i_ic.ic_ctime

#define	i_blocks	i_ic.ic_blocks
#define	i_ordev		i_ic.ic_db[0]	/* was i_oldrdev */
/* *DEL* #define	i_rdev		i_ic.ic_db[1]  */
#define	i_gen		i_ic.ic_gen
#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]

/* transition aids XXX */
#define	oEFT_MAGIC	0x90909090
#define	di_mode_reserved di_ic.ic_mode_reserved
#define	di_oeftflag	di_ic.ic_oeftflag

#define	di_ic		di_un.di_icom
#define	di_mode		di_ic.ic_smode
#define	di_nlink	di_ic.ic_nlink
#define	di_uid		di_ic.ic_uid
#define	di_gid		di_ic.ic_gid
#define	di_smode	di_ic.ic_smode
#define	di_suid		di_ic.ic_suid
#define	di_sgid		di_ic.ic_sgid

#if defined(vax) || defined(i386)
#define	di_size		di_ic.ic_size.val[0]
#endif
#if defined(mc68000) || defined(sparc) || defined(u3b2) || defined(u3b15)
#define	di_size		di_ic.ic_size.val[1]
#endif
#define	di_db		di_ic.ic_db
#define	di_ib		di_ic.ic_ib

#define	di_atime	di_ic.ic_atime
#define	di_mtime	di_ic.ic_mtime
#define	di_ctime	di_ic.ic_ctime

#define	di_ordev	di_ic.ic_db[0]
#define	di_blocks	di_ic.ic_blocks
#define	di_gen		di_ic.ic_gen

/* flags */
#define	IUPD		0x001		/* file has been modified */
#define	IACC		0x002		/* inode access time to be updated */
#define	IMOD		0x004		/* inode has been modified */
#define	ICHG		0x008		/* inode has been changed */
#define	INOACC		0x010		/* no access time update in getpage */
#define	IMODTIME	0x020		/* mod time already set */
#define	IREF		0x040		/* inode is being referenced */
#define	ISYNC		0x080		/* do all allocation synchronously */
#define	IFASTSYMLNK	0x100		/* fast symbolic link */
#define	IMODACC		0x200		/* only access time changed; */
					/*   filesystem won't become active */

/* modes */
#define	IFMT		0170000		/* type of file */
#define	IFIFO		0010000		/* named pipe (fifo) */
#define	IFCHR		0020000		/* character special */
#define	IFDIR		0040000		/* directory */
#define	IFBLK		0060000		/* block special */
#define	IFREG		0100000		/* regular */
#define	IFLNK		0120000		/* symbolic link */
#define	IFSOCK		0140000		/* socket */

#define	ISUID		04000		/* set user id on execution */
#define	ISGID		02000		/* set group id on execution */
#define	ISVTX		01000		/* save swapped text even after use */
#define	IREAD		0400		/* read, write, execute permissions */
#define	IWRITE		0200
#define	IEXEC		0100

/* specify how the inode info is written in ufs_syncip() */
#define	I_SYNC		1		/* wait for the inode written to disk */
#define	I_ASYNC		0		/* don't wait for the inode written */

/* Statistics on inodes */
struct instats {
	long in_hits;			/* Cache hits */
	long in_misses;			/* Cache misses */
	long in_malloc;			/* kmem_alloce'd */
	long in_mfree;			/* kmem_free'd */
	long in_maxsize;			/* Largest size reached by cache */
	long in_frfront;			/* # put at front of freelist */
	long in_frback;			/* # put at back of freelist */
	long in_dnlclook;		/* # examined in dnlc */
	long in_dnlcpurge;		/* # purged from dnlc */
};

#ifdef _KERNEL
extern long ufs_ninode;			/* high-water mark for inode cache */
struct inode _FAR_ *fs_inode;		/* the inode table itself */
extern long ufs_allocinode;		/* # inodes actually allocated */
extern long ufs_iincr;			/* number of inodes to alloc at once */

extern struct vnodeops ufs_vnodeops;	/* vnode operations for ufs */

/*
 * Convert between inode pointers and vnode pointers
 */
#define	VTOI(VP)	((struct inode _FAR_ *)(VP)->v_data)
#define	ITOV(IP)	((struct vnode _FAR_ *)&(IP)->i_vnode)

/*
 * convert to fs
 */
#define	ITOF(IP)	((struct fs _FAR_ *)(IP)->i_fs)

/*
 * Convert between vnode types and inode formats
 */
extern enum vtype	iftovt_tab[];

#ifdef notneeded

/* Look at sys/mode.h and os/vnode.c */

extern long		vttoif_tab[];
/* #define	IFTOVT(M) (iftovt_tab[(((o_mode_t)(M)) & IFMT) >> 13 now 12]) */
/* #define	VTTOIF(T) (vttoif_tab[(long)(T)])		*/

#endif

/*
 * Mark an inode with the current (unique) timestamp.
 */
struct timeval iuniqtime;

#define	IMARK(ip) { \
	ASSERT(MUTEX_HELD(&(ip)->i_tlock)); \
	if (hrestime.tv_sec > iuniqtime.tv_sec || \
		hrestime.tv_nsec/1000 > iuniqtime.tv_usec) { \
		iuniqtime.tv_sec = hrestime.tv_sec; \
		iuniqtime.tv_usec = hrestime.tv_nsec/1000; \
	} else { \
		iuniqtime.tv_usec++; \
	} \
	if ((ip)->i_flag & IACC) \
		(ip)->i_atime = iuniqtime; \
	if ((ip)->i_flag & IUPD) { \
		(ip)->i_mtime = iuniqtime; \
		(ip)->i_flag |= IMODTIME; \
	} \
	if ((ip)->i_flag & ICHG) { \
		ip->i_diroff = 0; \
		(ip)->i_ctime = iuniqtime; \
	} \
}
#define	ITIMES_NOLOCK(ip) { \
	if ((ip)->i_flag & (IUPD|IACC|ICHG)) { \
		if (((ip)->i_flag & (IUPD|IACC|ICHG)) == IACC) \
			(ip)->i_flag |= IMODACC; \
		else \
			(ip)->i_flag |= IMOD; \
		IMARK(ip); \
		(ip)->i_flag &= ~(IACC|IUPD|ICHG); \
	} \
}
#define	ITIMES(ip) { \
	mutex_enter(&(ip)->i_tlock); \
	ITIMES_NOLOCK(ip); \
	mutex_exit(&(ip)->i_tlock); \
}

/*
 * Allocate the specified block in the inode
 * and make sure any in-core pages are initialized.
 */
#define	BMAPALLOC(ip, off, size, cr) \
	bmap_write((ip), (off), (daddr_t _FAR_ *NULL), (long _FAR_ *0), (size), 0, cr)

#define	ESAME	(-1)		/* trying to rename linked files (special) */

/*
 * Check that file is owned by current user or user is su.
 */
#define	OWNER(CR, IP) (((CR)->cr_uid == (IP)->i_uid)? 0: (suser(CR)? 0: EPERM))

#define	UFS_HOLE	(daddr_t)-1	/* value used when no block allocated */

/*
 * enums
 */
enum de_op   { DE_CREATE, DE_MKDIR, DE_LINK, DE_RENAME };  /* direnter ops */
enum dr_op   { DR_REMOVE, DR_RMDIR, DR_RENAME };	   /* dirremove ops */

/*
 * This overlays the fid structure (see vfs.h)
 */
struct ufid {
	u_short	ufid_len;
	ino_t	ufid_ino;
	long	ufid_gen;
};

#define	INOHSZ	512

/*
 * UFS VFS private data.
 */
struct ufsvfs {
	struct vnode _FAR_	*vfs_root;	/* root vnode			*/
	struct buf _FAR_	*vfs_bufp;	/* buffer containing superblock */
	struct vnode  _FAR_	*vfs_devvp;	/* block device vnode		*/
	struct inode  _FAR_	*vfs_qinod;	/* QUOTA: pointer to quota file */
	u_short		vfs_qflags;	/* QUOTA: filesystem flags	*/
	u_long		vfs_btimelimit;	/* QUOTA: block time limit	*/
	u_long		vfs_ftimelimit;	/* QUOTA: file time limit	*/
	struct async_reqs _FAR_ *vfs_async_reqs; /* async request list	*/
	struct async_reqs _FAR_ *vfs_async_tail; /* async request list tail	*/
	kcondvar_t	vfs_async_reqs_cv;
	u_short		vfs_rthreads;	/* number of active threads	*/
	u_short		vfs_flag;	/* async flags			*/
	kcondvar_t	vfs_flag_cv;
	kmutex_t	vfs_async_lock;	/* lock to protect async list	*/
	/*
	 * These are copied from the super block at mount time.
	 */
	long	vfs_nrpos;		/* # rotational positions */
	long	vfs_npsect;		/* # sectors/track including spares */
	long	vfs_interleave;		/* hardware sector interleave */
	long	vfs_tracksew;		/* sector 0 skew, per track */
	/*
	 * This lock protects cg's and super block pointed at by
	 * vfs_bufp->b_fs.
	 */
	kmutex_t vfs_lock;		/*
					 * Locks contents of fs and cg's
					 * and contents of vfs_dio
					 */
	struct	ulockfs	vfs_ulockfs;	/* ufs lockfs support */
	u_long	vfs_dio;		/* delayed io (_FIODIO) */
	u_long	vfs_nointr;		/* disallow lockfs interrupts */
};

#define	vfs_fs	vfs_bufp->b_un.b_fs

/*
 * Async flags.
 */
#define	UNMOUNT_INPROGRESS	0x01

#if	((INOHSZ&(INOHSZ-1)) == 0)
#define	INOHASH(dev, ino)	(((dev)+(ino))&(INOHSZ-1))
#else
#define	INOHASH(dev, ino)	(((unsigned long)((dev)+(ino)))%INOHSZ)
#endif

union ihead {
	union	ihead	_FAR_ *ih_head[2];
	struct	inode	_FAR_ *ih_chain[2];
};

extern	union	ihead	ihead[];

#endif /* _KERNEL */

/*
 * ufs function prototypes
 */
#if defined(_KERNEL) && defined(__STDC__)

extern	long	ufs_iget(struct vfs _FAR_ *, struct fs _FAR_ *, ino_t,
    struct inode _FAR_ **, struct cred _FAR_ *);
extern	void	ufs_iinactive(struct inode _FAR_ *, struct cred _FAR_ *);
extern	void	ufs_iupdat(struct inode _FAR_ *, long);
extern	void	ufs_iput(struct inode _FAR_ *);
extern	void	irele(struct inode _FAR_ *);
extern	void	idrop(struct inode _FAR_ *);
extern	void	ufs_iaddfree(struct inode _FAR_ *);
extern	void	ufs_irmfree(struct inode _FAR_ *);
extern	void	ufs_delcache(struct inode _FAR_ *);
extern	long	ufs_itrunc(struct inode _FAR_ *, u_long, struct cred _FAR_ *);
extern	long	ufs_iaccess(struct inode _FAR_ *, long, struct cred _FAR_ *);
#ifdef QUOTA
extern	long	ufs_iflush(struct vfs _FAR_ *, struct inode _FAR_ *);
#else
extern	long	ufs_iflush(struct vfs _FAR_ *);
#endif

extern	long	ufs_dirlook(struct inode _FAR_ *, char _FAR_ *, struct inode _FAR_ **,
    struct cred _FAR_ *);
extern	long	ufs_direnter(struct inode _FAR_ *, char _FAR_ *, enum de_op, struct inode _FAR_ *,
    struct inode _FAR_ *, struct vattr _FAR_ *, struct inode _FAR_ **, struct cred _FAR_ *);
extern	long	ufs_dirremove(struct inode _FAR_ *, char _FAR_ *, struct inode _FAR_ *,
    struct vnode _FAR_ *, enum dr_op, struct cred _FAR_ *);

extern	void	sbupdate(struct vfs _FAR_ *);

extern	long	ufs_ialloc(struct inode _FAR_ *, ino_t, mode_t, struct inode _FAR_ **,
    struct cred _FAR_ *);
extern	void	ufs_ifree(struct inode _FAR_ *, ino_t, mode_t);
extern	void	free(struct inode _FAR_ *, daddr_t, off_t);
extern	long	alloc(struct inode _FAR_ *, daddr_t, long, daddr_t _FAR_ *, struct cred _FAR_ *);
extern	long	realloccg(struct inode _FAR_ *, daddr_t, daddr_t, long, long,
    daddr_t _FAR_ *, struct cred _FAR_ *);
extern	long	ufs_freesp(struct vnode _FAR_ *, struct flock _FAR_ *, long, struct cred _FAR_ *);
extern	ino_t	dirpref(struct ufsvfs _FAR_ *);
extern	daddr_t	blkpref(struct inode _FAR_ *, daddr_t, long, daddr_t _FAR_ *);

extern	long	ufs_rdwri(enum uio_rw, struct inode _FAR_ *, caddr_t, long, off_t,
    enum uio_seg, long _FAR_ *, struct cred _FAR_ *);
extern	void	ufs_async_start(struct ufsvfs _FAR_ *);
extern	void	ufs_async_stop(struct vfs _FAR_ *);

extern	long	bmap_read(struct inode _FAR_ *, u_long, daddr_t _FAR_ *, long _FAR_ *);
extern	long	bmap_write(struct inode _FAR_ *, u_long, daddr_t _FAR_ *, long _FAR_ *, long,
    long, struct cred _FAR_ *);

extern	void	ufs_sbwrite();
extern	void	ufs_update();
extern	void	ufs_flushi(long);
extern	long	ufs_getsummaryinfo(dev_t, struct fs _FAR_ *);
extern	long	ufs_syncip(struct inode _FAR_ *, long, long);
extern	long	ufs_badblock(struct fs _FAR_ *, daddr_t);
extern	void	ufs_notclean(struct ufsvfs _FAR_ *);
extern	void	ufs_checkclean(struct vfs _FAR_ *, struct ufsvfs _FAR_ *, dev_t,
    timestruc_t _ far *);
extern	long	isblock(struct fs _FAR_ *, unsigned char _FAR_ *, daddr_t);
extern	void	setblock(struct fs _FAR_ *, unsigned char _FAR_ *, daddr_t);
extern	void	clrblock(struct fs _FAR_ *, u_char _FAR_ *, daddr_t);
extern	void	fragacct(struct fs _FAR_ *, long, long _FAR_ *, long);
extern	long	skpc(long, u_long, char _FAR_ *);
extern	long	scanc(u_long, u_char _FAR_ *, u_char _FAR_ *, u_char);
extern	long	ufs_fbwrite(struct fbuf _FAR_ *, struct inode _FAR_ *);
extern	long	ufs_fbiwrite(struct fbuf _FAR_ *, struct inode _FAR_ *, daddr_t, long);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_INODE_H */
