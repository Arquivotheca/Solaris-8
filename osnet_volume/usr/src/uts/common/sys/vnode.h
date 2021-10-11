/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

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
 *	Copyright (c) 1986-1989,1996-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_VNODE_H
#define	_SYS_VNODE_H

#pragma ident	"@(#)vnode.h	1.85	99/07/30 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/time_impl.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <vm/seg_enum.h>
#ifdef	_KERNEL
#include <sys/buf.h>
#endif	/* _KERNEL */

#ifdef	__cplusplus
extern "C" {
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
	VDOOR	= 7,
	VPROC	= 8,
	VSOCK	= 9,
	VBAD	= 10
} vtype_t;

/*
 * All of the fields in the vnode are read-only once they are initialized
 * (created) except for:
 *	v_flag:		protected by v_lock
 *	v_count:	protected by v_lock
 *	v_pages:	file system must keep page list in sync with file size
 *	v_filocks:	protected by flock_lock in flock.c
 *	v_shrlocks:	protected by v_lock
 */
/* XX64 Can fields be reordered? */
typedef struct vnode {
	kmutex_t	v_lock;			/* protects vnode fields */
	ushort_t	v_flag;			/* vnode flags (see below) */
	uint_t		v_count;		/* reference count */
	struct vfs	*v_vfsmountedhere;	/* ptr to vfs mounted here */
	struct vnodeops	*v_op;			/* vnode operations */
	struct vfs	*v_vfsp;		/* ptr to containing VFS */
	struct stdata	*v_stream;		/* associated stream */
	struct page	*v_pages;		/* vnode pages list */
	enum vtype	v_type;			/* vnode type */
	dev_t		v_rdev;			/* device (VCHR, VBLK) */
	caddr_t		v_data;			/* private data for fs */
	struct filock	*v_filocks;		/* ptr to filock list */
	struct shrlocklist *v_shrlocks;		/* ptr to shrlock list */
	kcondvar_t	v_cv;			/* synchronize locking */
	void		*v_locality;		/* hook for locality info */
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
#define	VISSWAP		0x40	/* vnode is being used for swap */
#define	VSWAPLIKE	0x80	/* vnode acts like swap (but may not be) */

#define	IS_SWAPVP(vp)	(((vp)->v_flag & (VISSWAP | VSWAPLIKE)) != 0)

/*
 * The following two flags are used to lock the v_vfsmountedhere field
 */
#define	VVFSLOCK	0x100
#define	VVFSWAIT	0x200

/*
 * Used to serialize VM operations on a vnode
 */
#define	VVMLOCK		0x400

/*
 * Tell vn_open() not to fail a directory open for writing but
 * to go ahead and call VOP_OPEN() to let the filesystem check.
 */
#define	VDIROPEN	0x800

/*
 * Flag to let the VM system know that this file is most likely a binary
 * or shared library since it has been mmap()ed EXEC at some time.
 */
#define	VVMEXEC		0x1000

#define	VPXFS		0x2000  /* clustering: global fs proxy vnode */
#define	IS_PXFSVP(vp)	((vp)->v_flag & VPXFS)

/*
 * Vnode attributes.  A bit-mask is supplied as part of the
 * structure to indicate the attributes the caller wants to
 * set (setattr) or extract (getattr).
 */

/*
 * Note that va_nodeid and va_nblocks are 64bit data type.
 * We support large files over NFSV3. With Solaris client and
 * Server that generates 64bit ino's and sizes these fields
 * will overflow if they are 32 bit sizes.
 */

typedef struct vattr {
	uint_t		va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* file access mode */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	dev_t		va_fsid;	/* file system id (dev for now) */
	u_longlong_t	va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	u_offset_t	va_size;	/* file size in bytes */
	timestruc_t	va_atime;	/* time of last access */
	timestruc_t	va_mtime;	/* time of last modification */
	timestruc_t	va_ctime;	/* time file ``created'' */
	dev_t		va_rdev;	/* device the file represents */
	uint_t		va_blksize;	/* fundamental block size */
	u_longlong_t	va_nblocks;	/* # of blocks allocated */
	uint_t		va_vcode;	/* version code */
} vattr_t;

#ifdef _SYSCALL32
/*
 * For bigtypes time_t changed to 64 bit on the 64-bit kernel.
 * Define an old version for user/kernel interface
 */
typedef struct vattr32 {
	uint32_t	va_mask;	/* bit-mask of attributes */
	vtype_t		va_type;	/* vnode type (for create) */
	mode32_t	va_mode;	/* file access mode */
	uid32_t		va_uid;		/* owner user id */
	gid32_t		va_gid;		/* owner group id */
	dev32_t		va_fsid;	/* file system id (dev for now) */
	u_longlong_t	va_nodeid;	/* node id */
	nlink_t		va_nlink;	/* number of references to file */
	u_offset_t	va_size;	/* file size in bytes */
	timestruc32_t	va_atime;	/* time of last access */
	timestruc32_t	va_mtime;	/* time of last modification */
	timestruc32_t	va_ctime;	/* time file ``created'' */
	dev32_t		va_rdev;	/* device the file represents */
	uint32_t	va_blksize;	/* fundamental block size */
	u_longlong_t	va_nblocks;	/* # of blocks allocated */
	uint32_t	va_vcode;	/* version code */
} vattr32_t;
#else  /* not _SYSCALL32 */
#define	vattr32		vattr
typedef vattr_t		vattr32_t;
#endif /* _SYSCALL32 */

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

#define	MANDMODE(mode)		(((mode) & (VSGID|(VEXEC>>3))) == VSGID)
#define	MANDLOCK(vp, mode)	((vp)->v_type == VREG && MANDMODE(mode))

/*
 * Flags for vnode operations.
 */
enum rm		{ RMFILE, RMDIRECTORY };	/* rm or rmdir (remove) */
enum symfollow	{ NO_FOLLOW, FOLLOW };		/* follow symlinks (or not) */
enum vcexcl	{ NONEXCL, EXCL };		/* (non)excl create */
enum create	{ CRCREAT, CRMKNOD, CRMKDIR };	/* reason for create */

typedef enum rm		rm_t;
typedef enum symfollow	symfollow_t;
typedef enum vcexcl	vcexcl_t;
typedef enum create	create_t;

/*
 * Stucture used on VOP_GETSECATTR and VOP_SETSECATTR operations
 */

typedef struct vsecattr {
	uint_t		vsa_mask;	/* See below */
	int		vsa_aclcnt;	/* ACL entry count */
	void		*vsa_aclentp;	/* pointer to ACL entries */
	int		vsa_dfaclcnt;	/* default ACL entry count */
	void		*vsa_dfaclentp;	/* pointer to default ACL entries */
} vsecattr_t;

/* vsa_mask values */
#define	VSA_ACL		0x0001
#define	VSA_ACLCNT	0x0002
#define	VSA_DFACL	0x0004
#define	VSA_DFACLCNT	0x0008

/*
 * Structure tags for function prototypes, defined elsewhere.
 */
struct pathname;
struct fid;
struct flock64;
struct shrlock;
struct page;
struct seg;
struct as;
struct pollhead;

/*
 * Operations on vnodes.
 */
typedef struct vnodeops {
	int	(*vop_open)(struct vnode **vpp, int flag, struct cred *cr);
	int	(*vop_close)(struct vnode *vp, int flag, int count,
				offset_t offset, struct cred *cr);
	int	(*vop_read)(struct vnode *vp, struct uio *uiop, int ioflag,
			    struct cred *cr);
	int	(*vop_write)(struct vnode *vp, struct uio *uiop, int ioflag,
				struct cred *cr);
	int	(*vop_ioctl)(struct vnode *vp, int cmd, intptr_t arg, int flag,
				struct cred *cr, int *rvalp);
	int	(*vop_setfl)(struct vnode *vp, int oflags, int nflags,
				struct cred *cr);
	int	(*vop_getattr)(struct vnode *vp, struct vattr *vap, int flags,
				struct cred *cr);
	int	(*vop_setattr)(struct vnode *vp, struct vattr *vap, int flags,
				struct cred *cr);
	int	(*vop_access)(struct vnode *vp, int mode, int flags,
				struct cred *cr);
	int	(*vop_lookup)(struct vnode *dvp, char *nm, struct vnode **vpp,
				struct pathname *pnp, int flags,
				struct vnode *rdir, struct cred *);
	int	(*vop_create)(struct vnode *dvp, char *name, struct vattr *vap,
				vcexcl_t excl, int mode, struct vnode **vpp,
				struct cred *cr, int flag);
	int	(*vop_remove)(struct vnode *vp, char *nm, struct cred *cr);
	int	(*vop_link)(struct vnode *tdvp, struct vnode *svp, char *tnm,
				struct cred *cr);
	int	(*vop_rename)(struct vnode *sdvp, char *snm,
				struct vnode *tdvp, char *tnm, struct cred *cr);
	int	(*vop_mkdir)(struct vnode *dvp, char *dirname,
				struct vattr *vap, struct vnode **vpp,
				struct cred *cr);
	int	(*vop_rmdir)(struct vnode *vp, char *nm, struct vnode *cdir,
				struct cred *cr);
	int	(*vop_readdir)(struct vnode *vp, struct uio *uiop,
				struct cred *cr, int *eofp);
	int	(*vop_symlink)(struct vnode *dvp, char *linkname,
				struct vattr *vap, char *target,
				struct cred *cr);
	int	(*vop_readlink)(struct vnode *vp, struct uio *uiop,
				struct cred *cr);
	int	(*vop_fsync)(struct vnode *vp, int syncflag, struct cred *cr);
	void	(*vop_inactive)(struct vnode *vp, struct cred *cr);
	int	(*vop_fid)(struct vnode *vp, struct fid *fidp);
	void	(*vop_rwlock)(struct vnode *vp, int write_lock);
	void	(*vop_rwunlock)(struct vnode *vp, int write_lock);
	int	(*vop_seek)(struct vnode *vp, offset_t ooff, offset_t *noffp);
	int	(*vop_cmp)(struct vnode *vp1, struct vnode *vp2);
	int	(*vop_frlock)(struct vnode *vp, int cmd, struct flock64 *bfp,
				int flag, offset_t offset, struct cred *cr);
	int	(*vop_space)(struct vnode *vp, int cmd, struct flock64 *bfp,
				int flag, offset_t offset, struct cred *cr);
	int	(*vop_realvp)(struct vnode *vp, struct vnode **vpp);
	int	(*vop_getpage)(struct vnode *vp, offset_t off, size_t len,
				uint_t *protp, struct page **plarr, size_t plsz,
				struct seg *seg, caddr_t addr, enum seg_rw rw,
				struct cred *cr);
	int	(*vop_putpage)(struct vnode *vp, offset_t off, size_t len,
				int flags, struct cred *cr);
	int	(*vop_map)(struct vnode *vp, offset_t off, struct as *as,
			    caddr_t *addrp, size_t len, uchar_t prot,
			    uchar_t maxprot, uint_t flags, struct cred *cr);
	int	(*vop_addmap)(struct vnode *vp, offset_t off, struct as *as,
				caddr_t addr, size_t len, uchar_t prot,
				uchar_t maxprot, uint_t flags, struct cred *cr);
	int	(*vop_delmap)(struct vnode *vp, offset_t off, struct as *as,
				caddr_t addr, size_t len, uint_t prot,
				uint_t maxprot, uint_t flags, struct cred *cr);
	int	(*vop_poll)(struct vnode *vp, short ev, int any, short *revp,
			struct pollhead **phpp);
	int	(*vop_dump)(struct vnode *vp, caddr_t addr, int lbdn,
				int dblks);
	int	(*vop_pathconf)(struct vnode *vp, int cmd, ulong_t *valp,
				struct cred *cr);
	int	(*vop_pageio)(struct vnode *vp, struct page *pp,
				u_offset_t io_off, size_t io_len, int flags,
				struct cred *cr);
	int	(*vop_dumpctl)(struct vnode *vp, int action, int *blkp);
	void	(*vop_dispose)(struct vnode *vp, struct page *pp, int flag,
				int dn, struct cred *cr);
	int	(*vop_setsecattr)(struct vnode *vp, vsecattr_t *vsap, int flag,
				    struct cred *cr);
	int	(*vop_getsecattr)(struct vnode *vp, vsecattr_t *vsap, int flag,
				    struct cred *cr);
	int	(*vop_shrlock)(struct vnode *vp, int cmd, struct shrlock *shr,
				int flag);
} vnodeops_t;

#define	VOP_OPEN(vpp, mode, cr) (*(*(vpp))->v_op->vop_open)(vpp, mode, cr)
#define	VOP_CLOSE(vp, f, c, o, cr) (*(vp)->v_op->vop_close)(vp, f, c, o, cr)
#define	VOP_READ(vp, uiop, iof, cr) (*(vp)->v_op->vop_read)(vp, uiop, iof, cr)
#define	VOP_WRITE(vp, uiop, iof, cr) (*(vp)->v_op->vop_write)(vp, uiop, iof, cr)
#define	VOP_IOCTL(vp, cmd, a, f, cr, rvp) \
	(*(vp)->v_op->vop_ioctl)(vp, cmd, a, f, cr, rvp)
#define	VOP_SETFL(vp, f, a, cr) (*(vp)->v_op->vop_setfl)(vp, f, a, cr)
#define	VOP_GETATTR(vp, vap, f, cr) (*(vp)->v_op->vop_getattr)(vp, vap, f, cr)
#define	VOP_SETATTR(vp, vap, f, cr) (*(vp)->v_op->vop_setattr)(vp, vap, f, cr)
#define	VOP_ACCESS(vp, mode, f, cr) (*(vp)->v_op->vop_access)(vp, mode, f, cr)
#define	VOP_LOOKUP(vp, cp, vpp, pnp, f, rdir, cr) \
	(*(vp)->v_op->vop_lookup)(vp, cp, vpp, pnp, f, rdir, cr)
#define	VOP_CREATE(dvp, p, vap, ex, mode, vpp, cr, flag) \
	(*(dvp)->v_op->vop_create)(dvp, p, vap, ex, mode, vpp, cr, flag)
#define	VOP_REMOVE(dvp, p, cr) (*(dvp)->v_op->vop_remove)(dvp, p, cr)
#define	VOP_LINK(tdvp, fvp, p, cr) (*(tdvp)->v_op->vop_link)(tdvp, fvp, p, cr)
#define	VOP_RENAME(fvp, fnm, tdvp, tnm, cr) \
	(*(fvp)->v_op->vop_rename)(fvp, fnm, tdvp, tnm, cr)
#define	VOP_MKDIR(dp, p, vap, vpp, cr) \
	(*(dp)->v_op->vop_mkdir)(dp, p, vap, vpp, cr)
#define	VOP_RMDIR(dp, p, cdir, cr) (*(dp)->v_op->vop_rmdir)(dp, p, cdir, cr)
#define	VOP_READDIR(vp, uiop, cr, eofp) \
	(*(vp)->v_op->vop_readdir)(vp, uiop, cr, eofp)
#define	VOP_SYMLINK(dvp, lnm, vap, tnm, cr) \
	(*(dvp)->v_op->vop_symlink) (dvp, lnm, vap, tnm, cr)
#define	VOP_READLINK(vp, uiop, cr) (*(vp)->v_op->vop_readlink)(vp, uiop, cr)
#define	VOP_FSYNC(vp, syncflag, cr) (*(vp)->v_op->vop_fsync)(vp, syncflag, cr)
#define	VOP_INACTIVE(vp, cr) (*(vp)->v_op->vop_inactive)(vp, cr)
#define	VOP_FID(vp, fidp) (*(vp)->v_op->vop_fid)(vp, fidp)
#define	VOP_RWLOCK(vp, w) (*(vp)->v_op->vop_rwlock)(vp, w)
#define	VOP_RWUNLOCK(vp, w) (*(vp)->v_op->vop_rwunlock)(vp, w)
#define	VOP_SEEK(vp, ooff, noffp) (*(vp)->v_op->vop_seek) \
	(vp, ooff, noffp)
#define	VOP_CMP(vp1, vp2) (*(vp1)->v_op->vop_cmp)(vp1, vp2)
#define	VOP_FRLOCK(vp, cmd, a, f, o, cr) \
	(*(vp)->v_op->vop_frlock)(vp, cmd, a, f, o, cr)
#define	VOP_SPACE(vp, cmd, a, f, o, cr) \
	(*(vp)->v_op->vop_space)(vp, cmd, a, f, o, cr)
#define	VOP_REALVP(vp1, vp2) (*(vp1)->v_op->vop_realvp)(vp1, vp2)
#define	VOP_GETPAGE(vp, of, sz, pr, pl, ps, sg, a, rw, cr) \
		((*(vp)->v_op->vop_getpage) \
		(vp, of, sz, pr, pl, ps, sg, a, rw, cr))
#define	VOP_PUTPAGE(vp, of, sz, fl, cr) \
		((*(vp)->v_op->vop_putpage)(vp, of, sz, fl, cr))
#define	VOP_MAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(*(vp)->v_op->vop_map) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_ADDMAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(*(vp)->v_op->vop_addmap) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_DELMAP(vp, of, as, a, sz, p, mp, fl, cr) \
	(*(vp)->v_op->vop_delmap) (vp, of, as, a, sz, p, mp, fl, cr)
#define	VOP_POLL(vp, events, anyyet, reventsp, phpp) \
	(*(vp)->v_op->vop_poll)(vp, events, anyyet, reventsp, phpp)
#define	VOP_DUMP(vp, addr, bn, count) \
	(*(vp)->v_op->vop_dump)(vp, addr, bn, count)
#define	VOP_PATHCONF(vp, cmd, valp, cr) \
	(*(vp)->v_op->vop_pathconf)(vp, cmd, valp, cr)
#define	VOP_PAGEIO(vp, pp, io_off, io_len, flags, cr) \
	(*(vp)->v_op->vop_pageio)(vp, pp, io_off, io_len, flags, cr)
#define	VOP_DUMPCTL(vp, action, blkp) \
	(*(vp)->v_op->vop_dumpctl)(vp, action, blkp)
#define	VOP_DISPOSE(vp, pp, flag, dn, cr) \
	(*(vp)->v_op->vop_dispose)(vp, pp, flag, dn, cr)
#define	VOP_GETSECATTR(vp, vsap, f, cr) \
	(*(vp)->v_op->vop_getsecattr) (vp, vsap, f, cr)
#define	VOP_SETSECATTR(vp, vsap, f, cr) \
	(*(vp)->v_op->vop_setsecattr) (vp, vsap, f, cr)
#define	VOP_SHRLOCK(vp, cmd, shr, f) \
	(*(vp)->v_op->vop_shrlock)(vp, cmd, shr, f)

/*
 * Flags for VOP_LOOKUP
 */
#define	LOOKUP_DIR	0x01	/* want parent dir vp */

/*
 * Flags for VOP_DUMPCTL
 */
#define	DUMP_ALLOC	0
#define	DUMP_FREE	1
#define	DUMP_SCAN	2

/*
 * Public vnode manipulation functions.
 */
#ifdef	_KERNEL

int	vn_open(char *pnamep, enum uio_seg seg, int filemode, int createmode,
		struct vnode **vpp, enum create crwhy, mode_t umask);
int	vn_create(char *pnamep, enum uio_seg seg, struct vattr *vap,
		    enum vcexcl excl, int mode, struct vnode **vpp,
		    enum create why, int flag, mode_t umask);
int	vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base, ssize_t len,
		offset_t offset, enum uio_seg seg, int ioflag, rlim64_t ulimit,
		cred_t *cr, ssize_t *residp);
void	vn_rele(struct vnode *vp);
void	vn_rele_stream(struct vnode *vp);
int	vn_link(char *from, char *to, enum uio_seg seg);
int	vn_rename(char *from, char *to, enum uio_seg seg);
int	vn_remove(char *fnamep, enum uio_seg seg, enum rm dirflag);
int	vn_vfslock(struct vnode *vp);
int	vn_vfswlock(struct vnode *vp);
int	vn_vfswlock_wait(struct vnode *vp);
void	vn_vfsunlock(struct vnode *vp);
int	vn_vfswlock_held(struct vnode *vp);
vnode_t *specvp(struct vnode *vp, dev_t dev, vtype_t type, struct cred *cr);
vnode_t *makespecvp(dev_t dev, vtype_t type);

#endif	/* _KERNEL */

#define	VN_HOLD(vp)	{ \
	mutex_enter(&(vp)->v_lock); \
	(vp)->v_count++; \
	mutex_exit(&(vp)->v_lock); \
}

#define	VN_RELE(vp)	{ \
	vn_rele(vp); \
}

#define	VN_INIT(vp, vfsp, type, dev)	{ \
	mutex_init(&(vp)->v_lock, NULL, MUTEX_DEFAULT, NULL); \
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
#define	ATTR_HINT	0x08	/* information returned will be `hint' */
#define	ATTR_REAL	0x10	/* yield attributes of the real vp */

/*
 * Generally useful macros.
 */
#define	VBSIZE(vp)	((vp)->v_vfsp->vfs_bsize)
#define	NULLVP		((struct vnode *)0)
#define	NULLVPP		((struct vnode **)0)

#ifdef	_KERNEL

/*
 * Structure used while handling asynchronous VOP_PUTPAGE operations.
 */
struct async_reqs {
	struct async_reqs *a_next;	/* pointer to next arg struct */
	struct vnode *a_vp;		/* vnode pointer */
	u_offset_t a_off;			/* offset in file */
	uint_t a_len;			/* size of i/o request */
	int a_flags;			/* flags to indicate operation type */
	struct cred *a_cred;		/* cred pointer	*/
	ushort_t a_prealloced;		/* set if struct is pre-allocated */
};

/*
 * VN_DISPOSE() -- given a page pointer, safely invoke VOP_DISPOSE().
 */
#define	VN_DISPOSE(pp, flag, dn, cr)	{ \
	extern struct vnode kvp; \
	if ((pp)->p_vnode != NULL && (pp)->p_vnode != &kvp) \
		VOP_DISPOSE((pp)->p_vnode, (pp), (flag), (dn), (cr)); \
	else if ((flag) == B_FREE) \
		page_free((pp), (dn)); \
	else \
		page_destroy((pp), (dn)); \
	}


#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VNODE_H */
