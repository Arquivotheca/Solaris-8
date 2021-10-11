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
 *	Copyright (c) 1986-1989,1996-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_VFS_H
#define	_SYS_VFS_H

#pragma ident	"@(#)vfs.h	1.68	99/10/18 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/statvfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Data associated with mounted file systems.
 */

/*
 * File system identifier. Should be unique (at least per machine).
 */
typedef struct {
	int val[2];			/* file system id type */
} fsid_t;

/*
 * File identifier.  Should be unique per filesystem on a single
 * machine.  This is typically called by a stateless file server
 * in order to generate "file handles".
 *
 * Do not change the definition of struct fid ... fid_t without
 * letting the CacheFS group know about it!  They will have to do at
 * least two things, in the same change that changes this structure:
 *   1. change CFSVERSION in usr/src/uts/common/sys/fs/cachefs_fs.h
 *   2. put the old version # in the canupgrade array
 *	in cachfs_upgrade() in usr/src/cmd/fs.d/cachefs/fsck/fsck.c
 * This is necessary because CacheFS stores FIDs on disk.
 *
 * Many underlying file systems cast a struct fid into other
 * file system dependant structures which may require 4 byte alignment.
 * Because a fid starts with a short it may not be 4 byte aligned, the
 * fid_pad will force the alignment.
 */
#define	MAXFIDSZ	64
#define	OLD_MAXFIDSZ	16

typedef struct fid {
	union {
		long fid_pad;
		struct {
			ushort_t len;	/* length of data in bytes */
			char	data[MAXFIDSZ]; /* data (variable len) */
		} _fid;
	} un;
} fid_t;

#ifdef _SYSCALL32
/*
 * Solaris 64 - use old-style cache format with 32-bit aligned fid for on-disk
 * struct compatibility.
 */
typedef struct fid32 {
	union {
		int32_t fid_pad;
		struct {
			uint16_t  len;   /* length of data in bytes */
			char    data[MAXFIDSZ]; /* data (variable len) */
		} _fid;
	} un;
} fid32_t;
#else /* not _SYSCALL32 */
#define	fid32	fid
typedef fid_t	fid32_t;
#endif /* _SYSCALL32 */

#define	fid_len		un._fid.len
#define	fid_data	un._fid.data

/*
 * Structure defining a mount option for a filesystem.
 * option names are found in mntent.h
 */
typedef struct mntopt {
	char	*mo_name;	/* option name */
	char	**mo_cancel;	/* list of options cancelled by this one */
	char	*mo_arg;	/* argument string for this option */
	int	mo_flags;	/* flags for this mount option */
	void	*mo_data;	/* filesystem specific data */
} mntopt_t;

/*
 * Flags that apply to mount options
 */

#define	MO_SET		0x01		/* option is set */
#define	MO_NODISPLAY	0x02		/* option not listed in mnttab */
#define	MO_HASVALUE	0x04		/* option takes a value */
#define	MO_IGNORE	0x08		/* option ignored by parser */
#define	MO_DEFAULT	MO_SET		/* option is on by default */
#define	MO_TAG		0x10		/* flags a tag set by user program */
#define	MO_EMPTY	0x20		/* empty space in option table */

#define	VFS_NOFORCEOPT	0x01		/* honor MO_IGNORE (don't set option) */
#define	VFS_DISPLAY	0x02		/* Turn off MO_NODISPLAY bit for opt */
#define	VFS_NODISPLAY	0x04		/* Turn on MO_NODISPLAY bit for opt */
#define	VFS_CREATEOPT	0x08		/* Create the opt if it's not there */

/*
 * Structure holding mount option strings for the mounted file system.
 */
typedef struct mntopts {
	int		mo_count;		/* number of entries in table */
	mntopt_t	*mo_list;		/* list of mount options */
} mntopts_t;

/*
 * Structure per mounted file system.  Each mounted file system has
 * an array of operations and an instance record.
 *
 * The file systems are kept on a singly linked list headed by "rootvfs" and
 * terminated by NULL.  File system implementations should not access this
 * list; it's intended for use only in the kernel's vfs layer.
 */
typedef struct vfs {
	struct vfs	*vfs_next;		/* next VFS in VFS list */
	struct vfsops	*vfs_op;		/* operations on VFS */
	struct vnode	*vfs_vnodecovered;	/* vnode mounted on */
	uint_t		vfs_flag;		/* flags */
	uint_t		vfs_bsize;		/* native block size */
	int		vfs_fstype;		/* file system type index */
	fsid_t		vfs_fsid;		/* file system id */
	caddr_t		vfs_data;		/* private data */
	dev_t		vfs_dev;		/* device of mounted VFS */
	ulong_t		vfs_bcount;		/* I/O count (accounting) */
	ushort_t	vfs_nsubmounts;		/* immediate sub-mount count */
	struct vfs	*vfs_list;		/* sync list pointer */
	struct vfs	*vfs_hash;		/* hash list pointer */
	ksema_t		vfs_reflock;		/* mount/unmount/sync lock */
	uint_t		vfs_count;		/* vfs reference count */
	mntopts_t	vfs_mntopts;		/* options mounted with */
	char		*vfs_resource;		/* mounted resource string */
	char 		*vfs_mntpt;		/* mount point string */
	time_t		vfs_mtime;		/* time we were mounted */
} vfs_t;

/*
 * VFS flags.
 */
#define	VFS_RDONLY	0x01		/* read-only vfs */
#define	VFS_NOMNTTAB	0x02		/* vfs not seen in mnttab */
#define	VFS_NOSUID	0x08		/* setuid disallowed */
#define	VFS_REMOUNT	0x10		/* modify mount options only */
#define	VFS_NOTRUNC	0x20		/* does not truncate long file names */
#define	VFS_UNLINKABLE	0x40		/* unlink(2) can be applied to root */
#define	VFS_PXFS	0x80		/* clustering: global fs proxy vfs */
#define	VFS_UNMOUNTED	0x100		/* file system has been unmounted */

#define	VFS_NORESOURCE	"unspecified_resource"
#define	VFS_NOMNTPT	"unspecified_mountpoint"

/*
 * Argument structure for mount(2).
 *
 * Flags are defined in <sys/mount.h>.
 *
 * Note that if the MS_SYSSPACE bit is set in flags, the pointer fields in
 * this structure are to be interpreted as kernel addresses.  File systems
 * should be prepared for this possibility.
 */
struct mounta {
	char	*spec;
	char	*dir;
	int	flags;
	char	*fstype;
	char	*dataptr;
	int	datalen;
	char	*optptr;
	int	optlen;
};

/*
 * Reasons for calling the vfs_mountroot() operation.
 */
enum whymountroot { ROOT_INIT, ROOT_REMOUNT, ROOT_UNMOUNT, ROOT_FRONTMOUNT,
	ROOT_BACKMOUNT};
typedef enum whymountroot whymountroot_t;


/*
 * Operations supported on virtual file system.
 */
/*
 * XXX:  Due to a bug in the current compilation system, which has
 * trouble mixing new (ansi) and old (K&R) style prototypes when a
 * short or char is a parameter, we do not prototype this vfsop:
 *	vfs_sync
 */
typedef struct vfsops {
	int	(*vfs_mount)(struct vfs *, struct vnode *, struct mounta *,
			struct cred *);
	int	(*vfs_unmount)(struct vfs *, int, struct cred *);
	int	(*vfs_root)(struct vfs *, struct vnode **);
	int	(*vfs_statvfs)(struct vfs *, struct statvfs64 *);
	int	(*vfs_sync)(struct vfs *, short, struct cred *);
	int	(*vfs_vget)(struct vfs *, struct vnode **, struct fid *);
	int	(*vfs_mountroot)(struct vfs *, enum whymountroot);
	int	(*vfs_swapvp)(struct vfs *, struct vnode **, char *);
	void	(*vfs_freevfs)(struct vfs *);
} vfsops_t;

#define	VFS_MOUNT(vfsp, mvp, uap, cr) \
	(*(vfsp)->vfs_op->vfs_mount)(vfsp, mvp, uap, cr)
#define	VFS_UNMOUNT(vfsp, flag, cr) \
	(*(vfsp)->vfs_op->vfs_unmount)(vfsp, flag, cr)
#define	VFS_ROOT(vfsp, vpp)	(*(vfsp)->vfs_op->vfs_root)(vfsp, vpp)
#define	VFS_STATVFS(vfsp, sp)	(*(vfsp)->vfs_op->vfs_statvfs)(vfsp, sp)
#define	VFS_SYNC(vfsp, flag, cr) \
	(*(vfsp)->vfs_op->vfs_sync)(vfsp, flag, cr)
#define	VFS_VGET(vfsp, vpp, fidp) \
	(*(vfsp)->vfs_op->vfs_vget)(vfsp, vpp, fidp)
#define	VFS_MOUNTROOT(vfsp, init) \
	(*(vfsp)->vfs_op->vfs_mountroot)(vfsp, init)
#define	VFS_SWAPVP(vfsp, vpp, nm) \
	(*(vfsp)->vfs_op->vfs_swapvp)(vfsp, vpp, nm)
#define	VFS_FREEVFS(vfsp)	\
	(*(vfsp)->vfs_op->vfs_freevfs)(vfsp)


/*
 * Filesystem type switch table.
 */
typedef struct vfssw {
	char		*vsw_name;	/* type name string */
	int		(*vsw_init)(struct vfssw *, int);
					/* init routine */
	struct vfsops	*vsw_vfsops;	/* filesystem operations vector */
	int		vsw_flag;	/* flags */
	mntopts_t	*vsw_optproto;	/* mount options table prototype */
} vfssw_t;

/*
 * flags for vfssw
 */
#define	VSW_HASPROTO	0x01	/* struct has a mount options prototype */
#define	VSW_CANRWRO	0x02	/* file system can transition from rw to ro */

#if defined(_KERNEL)
/*
 * Public operations.
 */
struct umounta;
struct statvfsa;
struct fstatvfsa;

int	rootconf(void);
int	domount(char *, struct mounta *, vnode_t *, struct cred *,
	    struct vfs **);
int	dounmount(struct vfs *, int, cred_t *);
int	vfs_lock(struct vfs *);
void	vfs_lock_wait(struct vfs *);
void	vfs_unlock(struct vfs *);
int	vfs_lock_held(struct vfs *);
void	sync(void);
void	vfs_sync(int);
void	vfs_mountroot(void);
void	vfs_add(vnode_t *, struct vfs *, int);
void	vfs_remove(struct vfs *);
void	vfs_createopttbl(mntopts_t *, const char *);
void	vfs_copyopttbl(mntopts_t *, mntopts_t *);
void	vfs_clearmntopt(mntopts_t *, const char *);
void	vfs_setmntopt(mntopts_t *, const char *, const char *, int);
void	vfs_parsemntopts(mntopts_t *, char *, int);
void	vfs_setresource(struct vfs *, const char *);
void	vfs_setmntpoint(struct vfs *, const char *);
int	vfs_buildoptionstr(mntopts_t *, char *, int);
int	vfs_optionisset(mntopts_t *, const char *, char **);
int	vfs_setoptprivate(mntopts_t *, const char *, void *);
int	vfs_getoptprivate(mntopts_t *, const char *, void **);
struct mntopt *vfs_hasopt(mntopts_t *, const char *);
int	vfs_settag(uint_t, uint_t, const char *, const char *);
int	vfs_clrtag(uint_t, uint_t, const char *, const char *);
void	vfs_syncall(void);
void	vfsinit(void);
void	vfs_unmountall(void);
void	vfs_make_fsid(fsid_t *, dev_t, int);
int	vfs_devismounted(dev_t);
int	vfs_opsinuse(struct vfsops *);
struct vfs *getvfs(fsid_t *);
struct vfs *vfs_dev2vfsp(dev_t);
struct vfssw *allocate_vfssw(char *);
struct vfssw *vfs_getvfssw(char *);
struct vfssw *vfs_getvfsswbyname(char *);
uint_t	vf_to_stf(uint_t);
void	vfs_mnttab_modtime(timespec_t *);
void	vfs_mnttab_poll(timespec_t *, struct pollhead **);

void	vfs_list_lock();
void	vfs_list_unlock();
void	vfs_list_add(struct vfs *);
void	vfs_list_remove(struct vfs *);
void	vfs_hold(vfs_t *vfsp);
void	vfs_rele(vfs_t *vfsp);
void	fs_freevfs(vfs_t *);

#define	vfs_opttblptr(vfsp) (&vfsp->vfs_mntopts)
#define	VFSHASH(dev, fstyp) (((int)dev + (int)fstyp) & (vfshsz - 1))

/*
 * Globals.
 */

extern struct vfssw vfssw[];		/* table of filesystem types */
extern krwlock_t vfssw_lock;
extern char rootfstype[];		/* name of root fstype */
extern int nfstype;			/* # of elements in vfssw array */

/*
 * The following variables are private to the the kernel's vfs layer.  File
 * system implementations should not access them.
 */
extern struct vfs *rootvfs;		/* ptr to root vfs structure */
extern struct vfs **rvfs_head;		/* root hash vfs structures */
extern kmutex_t *rvfs_lock;		/* protects vfs hash list */
extern int vfshsz;			/* # of elements in rvfs_head array */

#endif /* defined(_KERNEL) */

#define	VFS_HOLD(vfsp) { \
	vfs_hold(vfsp); \
}

#define	VFS_RELE(vfsp)	{ \
	vfs_rele(vfsp); \
}

#define	VFS_INIT(vfsp, op, data)	{ \
	(vfsp)->vfs_count = 0; \
	(vfsp)->vfs_next = (struct vfs *)0; \
	(vfsp)->vfs_op = (op); \
	(vfsp)->vfs_flag = 0; \
	(vfsp)->vfs_data = (data); \
	(vfsp)->vfs_nsubmounts = 0; \
	(vfsp)->vfs_resource = NULL; \
	(vfsp)->vfs_mntpt = NULL; \
	(vfsp)->vfs_mntopts.mo_count = 0; \
	(vfsp)->vfs_mntopts.mo_list = NULL; \
	sema_init(&(vfsp)->vfs_reflock, 1, NULL, SEMA_DEFAULT, NULL); \
}

#define	VFS_INSTALLED(vfsswp)		((vfsswp)->vsw_vfsops)
#define	ALLOCATED_VFSSW(vswp)		((vswp)->vsw_name[0] != '\0')
#define	RLOCK_VFSSW()			(rw_enter(&vfssw_lock, RW_READER))
#define	RUNLOCK_VFSSW()			(rw_exit(&vfssw_lock))
#define	WLOCK_VFSSW()			(rw_enter(&vfssw_lock, RW_WRITER))
#define	WUNLOCK_VFSSW()			(rw_exit(&vfssw_lock))
#define	VFSSW_LOCKED()			(RW_LOCK_HELD(&vfssw_lock))
#define	VFSSW_WRITE_LOCKED()		(RW_WRITE_HELD(&vfssw_lock))
/*
 * VFS_SYNC flags.
 */
#define	SYNC_ATTR	0x01		/* sync attributes only */
#define	SYNC_CLOSE	0x02		/* close open file */
#define	SYNC_ALL	0x04		/* force to sync all fs */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VFS_H */
