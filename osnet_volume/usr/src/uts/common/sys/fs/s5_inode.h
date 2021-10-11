/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_FS_S5_INODE_H
#define	_SYS_FS_S5_INODE_H

#pragma ident	"@(#)s5_inode.h	1.10	98/01/23 SMI"

#include <sys/fbuf.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/t_lock.h>
#include <sys/fs/s5_fs.h>
#include <sys/fs/s5_lockfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The I node is the focus of all local file activity in UNIX.
 * There is a unique inode allocated for each active file,
 * each current directory, each mounted-on file, each mapping,
 *  and the root.  An inode is `named' by its dev/inumber pair.
 *
 * Each inode has 5 locks associated with it:
 *      i_rwlock:       Serializes s5_write and s5_setattr request
 *                      and allows s5_read requests to proceed in parallel.
 *                      Serializes reads/updates to directories.
 *      i_contents:     Protects almost all of the fields in the inode
 *                      except for those list below.
 *      i_tlock:        Protects just the i_atime, i_mtime, i_ctime,
 *                      i_nextrio and i_flag fields.
 *      icache_lock:    Protects inode cache and the i_forw/i_back/i_number
 *                      and i_dev fields of each inode.
 *      ifree_lock:     Protects inode freelist.
 * Lock ordering:
 *      icache_lock > i_rwlock > i_contents > i_tlock > ifree_lock
 */

#define	NADDR	13		/* address in inode */
#define	NDADDR	10		/* direct addresses in inode */
#define	NIADDR	(NADDR-NDADDR)	/* indirect addresses in inode */
#define	NSADDR  (NADDR * sizeof (daddr_t)/sizeof (short))
#define	NINDIR(s5vfs) ((s5vfs)->vfs_bsize / sizeof (daddr_t))

#define	i_fs	i_s5vfs->vfs_bufp->b_un.b_fs

struct inode {
	struct inode	*i_forw;	/* inode hash chain */
	struct inode	*i_back;	/* " */
	struct inode	*i_freef;	/* freelist chain */
	struct inode	**i_freeb;	/* " */
	struct vnode	i_vnode;	/* vnode associated with this inode */
	struct vnode	*i_devvp;	/* vnode for block I/O */
	ushort_t	i_flag;		/* flags */
	o_ino_t		i_number;	/* inode number */
	dev_t		i_dev;		/* device where inode resides */
	struct s5vfs	*i_s5vfs;	/* incore fs associated with inode */
	mode_t		i_mode;		/* file mode and type */
	uid_t		i_uid;		/* owner */
	gid_t		i_gid;		/* group */
	nlink_t		i_nlink;	/* number of links */
	off_t		i_size;		/* size in bytes */
	time_t		i_atime;	/* last access time */
	time_t		i_mtime;	/* last modification time */
	time_t		i_ctime;	/* last "inode change" time */
	daddr_t		i_addr[NADDR];	/* block address list */
	daddr_t		i_nextr;	/* next byte read offset (read-ahead) */
	uchar_t		i_gen;		/* generation number */
	long		i_blocks;	/* not a count, as in ufs; just delta */
	long		i_mapcnt;	/* number of mappings of pages */
	uint_t		i_vcode;	/* version code attribute */
	int		*i_map;		/* block list for the corresponding */
					/* file */
	dev_t		i_rdev;		/* rdev field for block/char specials */
	long		i_delaylen;	/* delayed writes, units=bytes */
	long		i_delayoff;	/* where we started delaying */
	long		i_nextrio;	/* where to start the next clust */
	long		i_writes;	/* number of outstanding bytes in */
					/* write q */
	kcondvar_t	i_wrcv;		/* sleep/wakeup for write throttle */
	krwlock_t	i_rwlock;	/* serializes write/setattr requests */
	krwlock_t	i_contents;	/* protects (most of) inode contents */
	kmutex_t	i_tlock;	/* protects time fields, i_flag */
};

#define	i_oldrdev	i_addr[0]
#define	i_bcflag	i_addr[1]	/* block/char special flag occupies */
					/* bytes 3-5 in di_addr */
#define	NDEVFORMAT	0x1		/* device number stored in new area */
#define	i_major		i_addr[2] 	/* major component occupies bytes 6-8 */
					/* in di_addr */
#define	i_minor		i_addr[3]	/* minor component occupies bytes */
					/* 9-11 in di_addr */


/* Flags */

#define	INOACC		0x0001	/* no access time update in getpage */
#define	IUPD		0x0002	/* file has been modified */
#define	IACC		0x0004	/* inode access time to be updated */
#define	IREF		0x0010	/* inode is being referenced */
#define	ICHG		0x0040	/* inode has been changed */
#define	ISYN		0x0080	/* do synchronous write for iupdat */
#define	IMOD		0x0100	/* inode times have been modified */
#define	IMODACC		0x0200	/* only access time changed */
#define	ISYNC		0x0400	/* do all block allocation synchronously */
#define	IMODTIME	0x0800	/* mod time already set */
#define	IINACTIVE	0x2000	/* iinactive in progress */

/*
 * File types.
 */

#define	IFMT	0xF000	/* type of file */
#define	IFIFO	0x1000	/* fifo special */
#define	IFCHR	0x2000	/* character special */
#define	IFDIR	0x4000	/* directory */
#define	IFNAM	0x5000	/* XENIX special named file */
#define	IFBLK	0x6000	/* block special */
#define	IFREG	0x8000	/* regular */
#define	IFLNK	0xA000	/* symbolic link */

/*
 * File modes.
 */
#define	ISUID	VSUID	/* set user id on execution */
#define	ISGID	VSGID	/* set group id on execution */
#define	ISVTX	VSVTX	/* save swapped text even after use */

/*
 * Permissions.
 */
#define	IREAD	VREAD	/* read permission */
#define	IWRITE	VWRITE	/* write permission */
#define	IEXEC	VEXEC	/* execute permission */

/* specify how the inode info is written in s5_syncip() */
#define	I_SYNC	1	/* wait for the inode written to disk */
#define	I_ASYNC	0	/* don't wait for the inode written */

/* Statistics on inodes */
struct instats {
	int in_hits;			/* Cache hits */
	int in_misses;			/* Cache misses */
	int in_malloc;			/* kmem_alloce'd */
	int in_mfree;			/* kmem_free'd */
	int in_maxsize;			/* Largest size reached by cache */
	int in_frfront;			/* # put at front of freelist */
	int in_frback;			/* # put at back of freelist */
	int in_dnlclook;		/* # examined in dnlc */
	int in_dnlcpurge;		/* # purged from dnlc */
};

/*
 * Inode structure as it appears on a disk block.  Of the 40 address
 * bytes, 39 are used as disk addresses (13 addresses of 3 bytes each)
 * and the 40th is used as a file generation number.
 */
struct	dinode {
	o_mode_t	di_mode;	/* mode and type of file */
	o_nlink_t	di_nlink;	/* number of links to file */
	o_uid_t		di_uid;		/* owner's user id */
	o_gid_t		di_gid;		/* owner's group id */
	off_t		di_size;	/* number of bytes in file */
	char  		di_addr[39];	/* disk block addresses */
	unsigned char	di_gen;		/* file generation number */
	time_t		di_atime;   	/* time last accessed */
	time_t		di_mtime;   	/* time last modified */
	time_t		di_ctime;   	/* time created */
};


#ifdef _KERNEL
struct inode *s5_inode;		/* the inode table itself */
extern int s5_allocinode;		/* # inodes actually allocated */
extern int s5_iincr;			/* number of inodes to alloc at once */

extern struct vnodeops s5_vnodeops;	/* vnode operations for s5 */

/*
 * Convert between inode pointers and vnode pointers
 */
#define	VTOI(VP)	((struct inode *)(VP)->v_data)
#define	ITOV(IP)	((struct vnode *)&(IP)->i_vnode)

/*
 * convert to fs
 */
#define	ITOF(IP)	((struct filsys *)(IP)->i_fs)

#define	vfs_fs	vfs_bufp->b_un.b_fs


/*
 * This overlays the fid structure (see vfs.h)
 */
struct ufid {
	ushort_t ufid_len;
	ino_t	ufid_ino;
	long	ufid_gen;
};

#define	INOHSZ	512

/*
 * S5 VFS private data.
 *
 * S5 file system instances may be linked on several lists.
 *
 * -	The vfs_next field chains together every extant s5fs instance; this
 *	list is rooted at s5fs_instances and should be used in preference to
 *	the overall vfs list (which is properly the province of the generic
 *	file system code, not of file system implementations).  This same list
 *	link is used during forcible unmounts to chain together instances that
 *	can't yet be completely dismantled. XXX is this true?
 *
 * -	The vfs_wnext field is used within s5fs_update to form a work list of
 *	s5fs instances to be synced out.
 */
struct s5vfs {
	struct vfs	*vfs_vfs;	/* back link			*/
	struct s5vfs	*vfs_next;	/* instance list link		*/
	struct s5vfs	*vfs_wnext;	/* work list link		*/
	struct vnode	*vfs_root;	/* root vnode			*/
	struct buf	*vfs_bufp;	/* buffer containing superblock */
	struct vnode	*vfs_devvp;	/* block device vnode		*/
	/*
	 * These are copied from the super block at mount time.
	 */
	long		vfs_nindir;	/* bsize/sizeof (daddr_t) */
	long		vfs_inopb;	/* bsize/sizeof (dinode) */
	long		vfs_bsize;	/* bsize */
	long		vfs_bmask;	/* bsize-1 */
	long		vfs_nmask;	/* nindir-1 */
	long		vfs_ltop;	/* ltop or ptol shift constant */
	long		vfs_bshift;	/* log2(bsize) */
	long		vfs_nshift;	/* log2(nindir) */
	long		vfs_inoshift;	/* log2(inopb) */
	/*
	 * This lock protects cg's and super block pointed at by
	 * vfs_bufp->b_fs.
	 */
	kmutex_t vfs_lock;		/* Locks contents of fs and cg's */
					/* and contents of vfs_dio */
	struct	ulockfs	vfs_ulockfs;	/* s5 lockfs support */
	ulong_t	vfs_nointr;		/* disallow lockfs interrupts */
	int	vfs_rdclustsz;		/* bytes in read cluster */
	int	vfs_wrclustsz;		/* bytes in write cluster */
};


#if ((INOHSZ & (INOHSZ - 1)) == 0)
#define	INOHASH(dev, ino)	(hash2ints((int)dev, (int)ino) & INOHSZ - 1)
#else
#define	INOHASH(dev, ino)	(hash2ints((int)dev, (int)ino) % INOHSZ)
#endif

union ihead {
	union	ihead	*ih_head[2];
	struct	inode	*ih_chain[2];
};

extern	union	ihead	ihead[];

/*
 * Mark an inode with the current (unique) timestamp.
 */
struct timeval32 iuniqtime;

#define	IMARK(ip) s5_imark(ip)
#define	ITIMES_NOLOCK(ip) s5_itimes_nolock(ip)

#define	ITIMES(ip) { \
	mutex_enter(&(ip)->i_tlock); \
	ITIMES_NOLOCK(ip); \
	mutex_exit(&(ip)->i_tlock); \
}

#define	FsINOS(s5vfs, ino) ((((ino) >> (s5vfs)->vfs_inoshift) << \
	(s5vfs)->vfs_inoshift) + 1)

#define	FsITOD(s5vfs, ino) (daddr_t)(((unsigned)(ino) + \
	(2*(s5vfs)->vfs_inopb-1)) >> (s5vfs)->vfs_inoshift)

/*
 * Macros for handling inode numbers:
 *	inode number to file system block offset.
 *	inode number to file system block address.
 */
#define	itod(s5vfs, x) (daddr_t)((long)(x+(2*s5vfs->vfs_inopb-1)) >> \
	(long)(s5vfs->vfs_inoshift))
#define	itoo(s5vfs, x) (daddr_t)((x-1)& (s5vfs->vfs_inopb-1))

/*
 * Allocate the specified block in the inode
 * and make sure any in-core pages are initialized.
 */
#define	BMAPALLOC(ip, off, size, cr) \
	s5_bmap_write((ip), (off), (size), 0, cr)

#define	ESAME	(-1)		/* trying to rename linked files (special) */

/*
 * Check that file is owned by current user or user is su.
 */
#define	OWNER(CR, IP) (((CR)->cr_uid == (IP)->i_uid)? 0: (suser(CR)? 0: EPERM))

#define	S5_HOLE	(daddr_t)0	/* value used when no block allocated */

/*
 * enums
 */
enum de_op   { DE_CREATE, DE_MKDIR, DE_LINK, DE_RENAME };  /* direnter ops */
enum dr_op   { DR_REMOVE, DR_RMDIR, DR_RENAME };	   /* dirremove ops */

#endif /* _KERNEL */

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */

#define	blkoff(s5vfs, loc)	/* calculates (loc % s5vfs->vfs_bsize) */ \
	((loc) & (s5vfs)->vfs_bmask)

#define	lblkno(s5vfs, loc)	/* calculates (loc / s5vfs->vfs_bsize) */ \
	((loc) >> (s5vfs)->vfs_bshift)

/* calculates roundup(size, s5vfs->vfs_bsize) */
#define	blkroundup(s5vfs, size) \
	(((size) + (s5vfs)->vfs_bmask) & ~(s5vfs)->vfs_bmask)

/*
 * s5 function prototypes
 */
#if defined(_KERNEL) && defined(__STDC__)

extern	int	s5_iget(struct vfs *, struct filsys *, ino_t,
    struct inode **, struct cred *);
extern	void	s5_iinactive(struct inode *, struct cred *);
extern	void	s5_iupdat(struct inode *, int);
extern	void	s5_iput(struct inode *);
extern	void	irele(struct inode *);
extern	void	idrop(struct inode *);
extern	void	s5_iaddfree(struct inode *);
extern	void	s5_irmfree(struct inode *);
extern	void	s5_delcache(struct inode *);
extern	int	s5_itrunc(struct inode *, ulong_t, struct cred *);
extern	int	s5_iaccess(struct inode *, int, struct cred *);
extern	int	s5_iflush(struct vfs *);

extern void	s5_imark(struct inode *);
extern void	s5_itimes_nolock(struct inode *);

extern	int	s5_dirlook(struct inode *, char *, struct inode **,
    struct cred *);
extern	int	s5_direnter(struct inode *, char *, enum de_op, struct inode *,
    struct inode *, struct vattr *, struct inode **, struct cred *);
extern	int	s5_dirremove(struct inode *, char *, struct inode *,
    struct vnode *, enum dr_op, struct cred *);

extern	void	sbupdate(struct vfs *);

extern	int	s5_ialloc(struct inode *, struct inode **, struct cred *);
extern	void	s5_ifree(struct inode *, ino_t, mode_t);
extern	void	s5_free(struct inode *, daddr_t, off_t);
extern	int	s5_alloc(struct inode *, daddr_t *, struct cred *);
extern	int	s5_freesp(struct vnode *, struct flock64 *, int,
	struct cred *);
extern	int	s5_rdwri(enum uio_rw, struct inode *, caddr_t, int, offset_t,
    enum uio_seg, int *, struct cred *);
extern	int	s5_bmap_read(struct inode *, uint_t, daddr_t *, int *);
extern	int	s5_bmap_write(struct inode *, uint_t, int,
    int, struct cred *);
extern void	s5_sbupdate(struct vfs *vfsp);

extern	void	s5_vfs_add(struct s5vfs *);
extern	void	s5_vfs_remove(struct s5vfs *);

extern	void	s5_sbwrite(struct s5vfs *s5vfsp);
extern	void	s5_update(int);
extern	void	s5_flushi(int);
extern	int	s5_syncip(struct inode *, int, int);
extern int	s5_sync_indir(struct inode *ip);
extern	int	s5_badblock(struct filsys *, daddr_t, dev_t);
extern	void	s5_notclean(struct s5vfs *);
extern	void	s5_checkclean(struct vfs *, struct s5vfs *, dev_t, time_t);
extern	int	skpc(char, uint_t, char *);
extern	int	s5_fbwrite(struct fbuf *, struct inode *);
extern	int	s5_fbiwrite(struct fbuf *, struct inode *, daddr_t, long);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FS_S5_INODE_H */
