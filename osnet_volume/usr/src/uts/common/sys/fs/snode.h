/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_SNODE_H
#define	_SYS_FS_SNODE_H

#pragma ident	"@(#)snode.h	1.35	98/11/21 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/cred.h>
#include <sys/vnode.h>

/*
 * The snode represents a special file in any filesystem.  There is
 * one snode for each active special file.  Filesystems that support
 * special files use specvp(vp, dev, type, cr) to convert a normal
 * vnode to a special vnode in the ops lookup() and create().
 *
 * To handle having multiple snodes that represent the same
 * underlying device vnode without cache aliasing problems,
 * the s_commonvp is used to point to the "common" vnode used for
 * caching data.  If an snode is created internally by the kernel,
 * then the s_realvp field is NULL and s_commonvp points to s_vnode.
 * The other snodes which are created as a result of a lookup of a
 * device in a file system have s_realvp pointing to the vp which
 * represents the device in the file system while the s_commonvp points
 * into the "common" vnode for the device in another snode.
 */

/*
 * Include SUNDDI type definitions so that the s_dip tag doesn't urk.
 */
#include <sys/dditypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct snode {
	/* These fields are protected by stable_lock */
	struct	snode *s_next;		/* must be first */
	struct	vnode s_vnode;		/* vnode associated with this snode */
	/*
	 * These fields are initialized once.
	 */
	struct	vnode *s_realvp;	/* vnode for the fs entry (if any) */
	struct	vnode *s_commonvp;	/* common device vnode */
	dev_t	s_dev;			/* device the snode represents */
	dev_info_t *s_dip;		/* dev_info (common snode only) */
	/*
	 * Doesn't always need to be updated atomically because it is a hint.
	 * No lock required.
	 */
	u_offset_t s_nextr;		/* next byte read offset (read-ahead) */

	/* These fields are protected by spec_syncbusy */
	struct	snode *s_list;		/* used for syncing */
	/* These fields are protected by s_lock */
	u_offset_t s_size;		/* block device size in bytes */
	ushort_t s_flag;			/* flags, see below */
	dev_t	s_fsid;			/* file system identifier */
	time_t  s_atime;		/* time of last access */
	time_t  s_mtime;		/* time of last modification */
	time_t  s_ctime;		/* time of last attributes change */
	int	s_count;		/* count of opened references */
	long	s_mapcnt;		/* count of mappings of pages */
	/* The locks themselves */
	kmutex_t	s_lock;		/* protects snode fields */
	kcondvar_t	s_cv;		/* synchronize open/closes */
	dev_t	s_gdev;			/* Solaris Clustering global devt */
};

/* flags */
#define	SUPD		0x01		/* update device access time */
#define	SACC		0x02		/* update device modification time */
#define	SCHG		0x04		/* update device change time */
#define	SPRIV		0x08		/* file open for private access */
#define	SLOFFSET	0x10		/* device takes 64-bit uio offsets */
#define	SLOCKED		0x20		/* use to serialize open/closes */
#define	SWANT		0x40		/* some process waiting on lock */
#define	SCLONE		0x100		/* represents a cloned device */

#ifdef _KERNEL
/*
 * Convert between vnode and snode
 */
#define	VTOS(vp)	((struct snode *)((vp)->v_data))
#define	STOV(sp)	(&(sp)->s_vnode)


/*
 * Forward declarations
 */
struct vfssw;
struct cred;

extern struct vfsops spec_vfsops;
extern struct kmem_cache *snode_cache;

/*
 * specfs functions
 */
offset_t	spec_maxoffset(struct vnode *);
struct vnodeops	*spec_getvnodeops(void);
struct vnode *specvp(struct vnode *, dev_t, vtype_t, struct cred *);
struct vnode *makespecvp(dev_t, vtype_t);
struct vnode *other_specvp(struct vnode *);
struct vnode *common_specvp(struct vnode *);
struct vnode *specfind(dev_t, vtype_t);
struct vnode *commonvp(dev_t, vtype_t);
struct vnode *makectty(vnode_t *);
void	strpunlink(struct cred *);
void	sdelete(struct snode *);
void 	smark(struct snode *, int);
int	specinit(struct vfssw *, int);
int	device_close(struct vnode *, int, struct cred *);
int	stillreferenced(dev_t, vtype_t);
int	spec_putpage(struct vnode *, offset_t, size_t, int, struct cred *);
int	spec_segmap(dev_t, off_t, struct as *, caddr_t *, off_t,
		    uint_t, uint_t, uint_t, cred_t *);

/*
 * If driver does not have a size routine (e.g. old drivers), the size of the
 * device is assumed to be "infinite".  To not break NFS and the older
 * interfaces, we will not expand this beyond the 2^31-1 limit.
 *
 * XX64 If no size property is defined, should we go up to 2^63-1?
 */
#define	UNKNOWN_SIZE 	0x7fffffff

#if defined(_NO_LONGLONG)
#define	SPEC_MAXOFFSET_T	MAXOFF_T
#elif defined(_LP64)
#define	SPEC_MAXOFFSET_T	MAXOFFSET_T
#else /* !defined(_NO_LONGLONG) && !defined(_LP64) */
#define	SPEC_MAXOFFSET_T	((1LL << NBBY * sizeof (daddr32_t) + \
						DEV_BSHIFT - 1) - 1)
#endif 	/* defined(_NO_LONGLONG) */

/*
 * Snode lookup stuff.
 * These routines maintain a table of snodes hashed by dev so
 * that the snode for an dev can be found if it already exists.
 * NOTE: STABLESIZE must be a power of 2 for STABLEHASH to work!
 */

#define	STABLESIZE	256
#define	STABLEHASH(dev)	((getmajor(dev) + getminor(dev)) & (STABLESIZE - 1))
extern struct snode *stable[];
extern kmutex_t	stable_lock;
extern kmutex_t	spec_syncbusy;

/*
 * Variables used by during asynchronous VOP_PUTPAGE operations.
 */
extern struct async_reqs *spec_async_reqs;	/* async request list */
extern kmutex_t spec_async_lock;		/* lock to protect async list */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_SNODE_H */
