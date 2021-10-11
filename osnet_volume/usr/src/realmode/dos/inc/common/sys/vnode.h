/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)vnode.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		vnode.h
 *
 *   Description:	definitions related to the vnode - the primary
 *			filesystem data structure.
 *
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved. 	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
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
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _SYS_VNODE_H
#define	_SYS_VNODE_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

/*
 * The vnode is the focus of all file activity in UNIX.
 * A vnode is allocated for each active file, each current
 * directory, each mounted-on file, and the root.
 */

/*
 * vnode types.  VNON means no type.  These values are unrelated to
 * values in on-disk inodes.
 */
typedef enum vtype {
	VNON	= 0,
	VREG	= 1,
	VDIR	= 2,
	VBLK	= 3,
	VCHR	= 4,
	VLNK	= 5,
	VFIFO	= 6,
	VXNAM	= 7,
	VBAD	= 8
} vtype_t;

/*
 * All of the fields in the vnode are read-only once they are initialized
 * (created) except for:
 *	v_flag:		protected by v_lock
 *	v_count:	protected by v_lock
 *	v_pages:	protected by v_lock
 *	v_filocks:	protected by flock_lock in flock.c
 */
typedef struct vnode {
	kmutex_t	v_lock;			/* protects vnode fields */
	u_short		v_flag;			/* vnode flags (see below) */
	u_short		v_count;		/* reference count */
	struct vfs 	_FAR_ *v_vfsmountedhere;	/* ptr to vfs mounted here */
	struct vnodeops	_FAR_ *v_op;			/* vnode operations */
	struct vfs	_FAR_ *v_vfsp;		/* ptr to containing VFS */
	struct stdata	_FAR_ *v_stream;		/* associated stream */
	struct page	_FAR_ *v_pages;		/* vnode pages list */
	enum vtype	v_type;			/* vnode type */
	dev_t		v_rdev;			/* device (VCHR, VBLK) */
	caddr_t		v_data;			/* private data for fs */
	struct filock	_FAR_ *v_filocks;		/* ptr to filock list */
	kcondvar_t	v_cv;			/* synchronize locking */
} vnode_t;

/*
 * vnode flags.
 */
#define	VROOT		0x01	/* root of its file system */
#define	VNOCACHE	0x02	/* don't keep cache pages on vnode */
#define	VNOMAP		0x04	/* file cannot be mapped/faulted */
#define	VDUP		0x08	/* file should be dup'ed rather then opened */
#define	VNOSWAP		0x10	/* file cannot be used as virtual swap device */
#define	VNOMOUNT	0x20	/* file cannot be covered by mount */
#define	VISSWAP		0x40	/* vnode is part of virtual swap device */
/*
 * The following two flags are used to lock the v_vfsmountedhere field
 */
#define	VVFSLOCK	0x80
#define	VVFSWAIT	0x100

/*
 * Used to serialize VM operations on a vnode
 */
#define	VVMLOCK		0x200

/* #ifdef MERGE */
#define	VXLOCKED	0x8000	/* Xenix frlock */
/* #endif MERGE */

/*
 * Operations on vnodes.
 */
typedef struct vnodeops {
	long	(_FAR_ *vop_open)();
	long	(_FAR_ *vop_close)();
	long	(_FAR_ *vop_read)();
	long	(_FAR_ *vop_write)();
	long	(_FAR_ *vop_ioctl)();
	long	(_FAR_ *vop_setfl)();
	long	(_FAR_ *vop_getattr)();
	long	(_FAR_ *vop_setattr)();
	long	(_FAR_ *vop_access)();
	long	(_FAR_ *vop_lookup)();
	long	(_FAR_ *vop_create)();
	long	(_FAR_ *vop_remove)();
	long	(_FAR_ *vop_link)();
	long	(_FAR_ *vop_rename)();
	long	(_FAR_ *vop_mkdir)();
	long	(_FAR_ *vop_rmdir)();
	long	(_FAR_ *vop_readdir)();
	long	(_FAR_ *vop_symlink)();
	long	(_FAR_ *vop_readlink)();
	long	(_FAR_ *vop_fsync)();
	void	(_FAR_ *vop_inactive)();
	long	(_FAR_ *vop_fid)();
	void	(_FAR_ *vop_rwlock)();
	void	(_FAR_ *vop_rwunlock)();
	long	(_FAR_ *vop_seek)();
	long	(_FAR_ *vop_cmp)();
	long	(_FAR_ *vop_frlock)();
	long	(_FAR_ *vop_space)();
	long	(_FAR_ *vop_realvp)();
	long	(_FAR_ *vop_getpage)();
	long	(_FAR_ *vop_putpage)();
	long	(_FAR_ *vop_map)();
	long	(_FAR_ *vop_addmap)();
	long	(_FAR_ *vop_delmap)();
	long	(_FAR_ *vop_poll)();
	long	(_FAR_ *vop_dump)();
	long	(_FAR_ *vop_pathconf)();
} vnodeops_t;

extern	long setswappingon();
extern	long setswappingoff();

#define	VOP_OPEN(vpp, mode, cr) (_FAR_ *(_FAR_ *(vpp))->v_op->vop_open)(vpp, mode, cr)
#define	VOP_CLOSE(vp, f, c, o, cr) (_FAR_ *(vp)->v_op->vop_close)(vp, f, c, o, cr)
#define	VOP_READ(vp, uiop, iof, cr) (_FAR_ *(vp)->v_op->vop_read)(vp, uiop, iof, cr)
#define	VOP_WRITE(vp, uiop, iof, cr) (_FAR_ *(vp)->v_op->vop_write)(vp, uiop, iof, cr)
#define	VOP_IOCTL(vp, cmd, a, f, cr, rvp) \
	(_FAR_ *(vp)->v_op->vop_ioctl)(vp, cmd, a, f, cr, rvp)
#define	VOP_SETFL(vp, f, a, cr) (_FAR_ *(vp)->v_op->vop_setfl)(vp, f, a, cr)
#define	VOP_GETATTR(vp, vap, f, cr) (_FAR_ *(vp)->v_op->vop_getattr)(vp, vap, f, cr)
#define	VOP_SETATTR(vp, vap, f, cr) (_FAR_ *(vp)->v_op->vop_setattr)(vp, vap, f, cr)
#define	VOP_ACCESS(vp, mode, f, cr) (_FAR_ *(vp)->v_op->vop_access)(vp, mode, f, cr)
#define	VOP_LOOKUP(vp, cp, vpp, pnp, f, rdir, cr) \
	(_FAR_ *(vp)->v_op->vop_lookup)(vp, cp, vpp, pnp, f, rdir, cr)
#define	VOP_CREATE(dvp, p, vap, ex, mode, vpp, cr) \
	(_FAR_ *(dvp)->v_op->vop_create)(dvp, p, vap, ex, mode, vpp, cr)
#define	VOP_REMOVE(dvp, p, cr) (_FAR_ *(dvp)->v_op->vop_remove)(dvp, p, cr)
#define	VOP_LINK(tdvp, fvp, p, cr) (_FAR_ *(tdvp)->v_op->vop_link)(tdvp, fvp, p, cr)
#define	VOP_RENAME(fvp, fnm, tdvp, tnm, cr) \
	(_FAR_ *(fvp)->v_op->vop_rename)(fvp, fnm, tdvp, tnm, cr)
#define	VOP_MKDIR(dp, p, vap, vpp, cr) \
	(_FAR_ *(dp)->v_op->vop_mkdir)(dp, p, vap, vpp, cr)
#define	VOP_RMDIR(dp, p, cdir, cr) (_FAR_ *(dp)->v_op->vop_rmdir)(dp, p, cdir, cr)
#define	VOP_READDIR(vp, uiop, cr, eofp) \
	(_FAR_ *(vp)->v_op->vop_readdir)(vp, uiop, cr, eofp)
#define	VOP_SYMLINK(dvp, lnm, vap, tnm, cr) \
	(_FAR_ *(dvp)->v_op->vop_symlink) (dvp, lnm, vap, tnm, cr)
#define	VOP_READLINK(vp, uiop, cr) (_FAR_ *(vp)->v_op->vop_readlink)(vp, uiop, cr)
#define	VOP_FSYNC(vp, cr) (_FAR_ *(vp)->v_op->vop_fsync)(vp, cr)
#define	VOP_INACTIVE(vp, cr) (_FAR_ *(vp)->v_op->vop_inactive)(vp, cr)
#define	VOP_FID(vp, fidpp) (_FAR_ *(vp)->v_op->vop_fid)(vp, fidpp)
#define	VOP_RWLOCK(vp, w) (_FAR_ *(vp)->v_op->vop_rwlock)(vp, w)
#define	VOP_RWUNLOCK(vp, w) (_FAR_ *(vp)->v_op->vop_rwunlock)(vp, w)
#define	VOP_SEEK(vp, ooff, noffp) (_FAR_ *(vp)->v_op->vop_seek)(vp, ooff, noffp)
#define	VOP_CMP(vp1, vp2) (_FAR_ *(vp1)->v_op->vop_cmp)(vp1, vp2)
#define	VOP_FRLOCK(vp, cmd, a, f, o, cr) \
	(_FAR_ *(vp)->v_op->vop_frlock)(vp, cmd, a, f, o, cr)
#define	VOP_SPACE(vp, cmd, a, f, o, cr) \
	(_FAR_ *(vp)->v_op->vop_space)(vp, cmd, a, f, o, cr)
#define	VOP_REALVP(vp1, vp2) (_FAR_ *(vp1)->v_op->vop_realvp)(vp1, vp2)

/*
 * VOP_GETPAGE() and VOP_PUTPAGE() are trick macros that use the comma
 * operator to sandwich the call to vop_getpage and vop_putpage between
 * code that turns swapping off and on for the current thread. This is
 * done because a thread in this code path mustn't be swapped out.
 */
#define	VOP_GETPAGE(vp, of, sz, pr, pl, ps, sg, a, rw, cr) \
		(curthread->t_dontswap++, \
		setswappingon(((_FAR_ *(vp)->v_op->vop_getpage) \
		(vp, of, sz, pr, pl, ps, sg, a, rw, cr))))
#define	VOP_PUTPAGE(vp, of, sz, fl, cr) \
		(curthread->t_dontswap++, \
		setswappingon(((_FAR_ *(vp)->v_op->vop_putpage)(vp, of, sz, fl, cr))))

#define	VOP_MAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(_FAR_ *(vp)->v_op->vop_map) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_ADDMAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(_FAR_ *(vp)->v_op->vop_addmap) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_DELMAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(_FAR_ *(vp)->v_op->vop_delmap) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_POLL(vp, events, anyyet, reventsp, phpp) \
	(_FAR_ *(vp)->v_op->vop_poll)(vp, events, anyyet, reventsp, phpp)
#define	VOP_DUMP(vp, addr, bn, count) \
	(_FAR_ *(vp)->v_op->vop_dump)(vp, addr, bn, count)
#define	VOP_PATHCONF(vp, cmd, valp, cr) \
	(_FAR_ *(vp)->v_op->vop_pathconf)(vp, cmd, valp, cr)

/*
 * I/O flags for VOP_READ and VOP_WRITE.
 */
#define	IO_APPEND	0x01	/* append write (VOP_WRITE) */
#define	IO_SYNC		0x02	/* sync I/O (VOP_WRITE) */

/*
 * Flags for VOP_LOOKUP.
 */
#define	LOOKUP_DIR	0x01	/* want parent dir vp */

/*
 * Vnode attributes.  A bit-mask is supplied as part of the
 * structure to indicate the attributes the caller wants to
 * set (setattr) or extract (getattr).
 */
typedef struct vattr {
	long		va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* file access mode */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	dev_t		va_fsid;	/* file system id (dev for now) */
	ino_t		va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	u_long		va_size0;	/* file size pad (for future use) */
	u_long		va_size;	/* file size in bytes */
	timestruc_t	va_atime;	/* time of last access */
	timestruc_t	va_mtime;	/* time of last modification */
	timestruc_t	va_ctime;	/* time file ``created'' */
	dev_t		va_rdev;	/* device the file represents */
	u_long		va_blksize;	/* fundamental block size */
	u_long		va_nblocks;	/* # of blocks allocated */
	u_long		va_vcode;	/* version code */
} vattr_t;

/*
 * Attributes of interest to the caller of setattr or getattr.
 */
#define	AT_TYPE		0x0001
#define	AT_MODE		0x0002
#define	AT_UID		0x0004
#define	AT_GID		0x0008
#define	AT_FSID		0x0010
#define	AT_NODEID	0x0020
#define	AT_NLINK	0x0040
#define	AT_SIZE		0x0080
#define	AT_ATIME	0x0100
#define	AT_MTIME	0x0200
#define	AT_CTIME	0x0400
#define	AT_RDEV		0x0800
#define	AT_BLKSIZE	0x1000
#define	AT_NBLOCKS	0x2000
#define	AT_VCODE	0x4000

#define	AT_ALL		(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
			AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|\
			AT_RDEV|AT_BLKSIZE|AT_NBLOCKS|AT_VCODE)

#define	AT_STAT		(AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|AT_NLINK|\
			AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV)

#define	AT_TIMES	(AT_ATIME|AT_MTIME|AT_CTIME)

#define	AT_NOSET	(AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
			AT_BLKSIZE|AT_NBLOCKS|AT_VCODE)

/*
 *  Modes.  Some values same as S_xxx entries from stat.h for convenience.
 */
#define	VSUID		04000		/* set user id on execution */
#define	VSGID		02000		/* set group id on execution */
#define	VSVTX		01000		/* save swapped text even after use */

/*
 * Permissions.
 */
#define	VREAD		00400
#define	VWRITE		00200
#define	VEXEC		00100

#define	MODEMASK	07777		/* mode bits plus permission bits */
#define	PERMMASK	00777		/* permission bits */

/*
 * Check whether mandatory file locking is enabled.
 */

/* #ifdef MERGE */
#define	MANDLOCK(vp, mode)	\
	((vp)->v_type == VREG && ((((mode) & (VSGID|(VEXEC>>3))) == VSGID) || \
	((vp)->v_flag & VXLOCKED) == VXLOCKED))
#if 0 /* MERGE else case */
#define	MANDLOCK(type, mode)	\
	((type) == VREG && ((mode) & (VSGID|(VEXEC>>3))) == VSGID)
#endif
/* #endif MERGE */

/*
 * Flags for vnode operations.
 */
enum rm		{ RMFILE, RMDIRECTORY };	/* rm or rmdir (remove) */
enum symfollow	{ NO_FOLLOW, FOLLOW };		/* follow symlinks (or not) */
enum vcexcl	{ NONEXCL, EXCL };		/* (non)excl create */
enum create	{ CRCREAT, CRMKNOD, CRMKDIR, CRCORE }; /* reason for create */

typedef enum rm		rm_t;
typedef enum symfollow	symfollow_t;
typedef enum vcexcl	vcexcl_t;
typedef enum create	create_t;

/*
 * Public vnode manipulation functions.
 */
#ifdef __STDC__

extern long vn_open(char _FAR_ *, enum uio_seg, long, long, struct vnode _FAR_ **,
    enum create);
extern long vn_create(char _FAR_ *, enum uio_seg, struct vattr _FAR_ *, enum vcexcl, long,
    struct vnode _FAR_ **, enum create);
extern long vn_rdwr(enum uio_rw, struct vnode _FAR_ *, caddr_t, long, off_t,
    enum uio_seg, long, long, cred_t _FAR_ *, long _FAR_ *);
extern long vn_close(struct vnode _FAR_ *, long, long, long, struct cred _FAR_ *);
extern void vn_rele(struct vnode _FAR_ *);
extern long vn_link(char _FAR_ *, char _FAR_ *, enum uio_seg);
extern long vn_rename(char _FAR_ *, char _FAR_ *, long);
extern long vn_remove(char _FAR_ *, enum uio_seg, enum rm);
extern vnode_t _FAR_ *specvp(struct vnode _FAR_ *, dev_t, vtype_t, struct cred _FAR_ *);
extern vnode_t _FAR_ *makespecvp(dev_t, vtype_t);

#else

extern long vn_open();
extern long vn_create();
extern long vn_rdwr();
extern long vn_close();
extern void vn_rele();
extern long vn_link();
extern long vn_rename();
extern long vn_remove();
extern vnode_t _FAR_ *specvp();
extern vnode_t _FAR_ *makespecvp();
#endif	/* __STDC__ */

#ifdef XENIX_MERGE
extern long fifo_rdchk();
extern long spec_rdchk();
#endif /* XENIX_MERGE */

#define	VN_HOLD(vp)	{ \
	mutex_enter(&(vp)->v_lock); \
	(vp)->v_count++; \
	mutex_exit(&(vp)->v_lock); \
}

#define	VN_RELE(vp)	{ \
	vn_rele(vp); \
}

#define	VN_INIT(vp, vfsp, type, dev)	{ \
	mutex_init(&(vp)->v_lock, "vnode lock", MUTEX_DEFAULT, DEFAULT_WT); \
	(vp)->v_flag = 0; \
	(vp)->v_count = 1; \
	(vp)->v_vfsp = (vfsp); \
	(vp)->v_type = (type); \
	(vp)->v_rdev = (dev); \
	(vp)->v_pages = NULL; \
	(vp)->v_stream = NULL; \
}

/*
 * Compare two vnodes for equality.  In general this macro should be used
 * in preference to calling VOP_CMP directly.
 */
#define	VN_CMP(VP1, VP2)	((VP1) == (VP2) ? 1 : 	\
	((VP1) && (VP2) && ((VP1)->v_op == (VP2)->v_op) ? \
	VOP_CMP(VP1, VP2) : 0))

/*
 * Flags to VOP_SETATTR/VOP_GETATTR.
 */
#define	ATTR_UTIME	0x01	/* non-default utime(2) request */
#define	ATTR_EXEC	0x02	/* invocation from exec(2) */
#define	ATTR_COMM	0x04	/* yield common vp attributes */

/*
 * Generally useful macros.
 */
#define	VBSIZE(vp)	((vp)->v_vfsp->vfs_bsize)
#define	NULLVP		((struct vnode _FAR_ *)0)
#define	NULLVPP		((struct vnode _FAR_ **)0)

#ifdef	_KERNEL
extern long lookupname();

/*
 * Structure used while handling asynchronous VOP_PUTPAGE operations.
 */
struct async_reqs {
	struct async_reqs _FAR_ *a_next;	/* pointer to next arg struct */
	struct vnode _FAR_ *a_vp;		/* vnode pointer */
	offset_t a_off;			/* offset in file */
	u_long a_len;			/* size of i/o request */
	long a_flags;			/* flags to indicate operation type */
	struct cred _FAR_ *a_cred;		/* cred pointer	*/
	u_short a_prealloced;		/* set if struct is pre-allocated */
};

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VNODE_H */
