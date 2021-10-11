/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_AUTOFS_H
#define	_SYS_FS_AUTOFS_H

#pragma ident	"@(#)autofs.h	1.24	99/07/16 SMI"

#include <rpcsvc/autofs_prot.h>
#include <rpc/rpc.h>
#include <sys/note.h>
#include <sys/time_impl.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <sys/varargs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

struct action_list;

/*
 * Tracing macro; expands to nothing for non-debug kernels.
 */
#ifndef DEBUG
#define	AUTOFS_DPRINT(x)
#else
#define	AUTOFS_DPRINT(x)	auto_dprint x
#endif

/*
 * Per AUTOFS mountpoint information.
 */
typedef struct fninfo {
	struct vfs	*fi_mountvfs;		/* mounted-here VFS */
	struct vnode	*fi_rootvp;		/* root vnode */
	struct knetconfig fi_knconf;		/* netconfig */
	struct netbuf	fi_addr;		/* daemon address */
	char		*fi_path;		/* autofs mountpoint */
	char 		*fi_map;		/* context/map-name */
	char		*fi_subdir;		/* subdir within map */
	char		*fi_key;		/* key to use on direct maps */
	char		*fi_opts;		/* default mount options */
	int		fi_pathlen;		/* autofs mountpoint len */
	int		fi_maplen;		/* size of context */
	int		fi_subdirlen;
	int		fi_keylen;
	int		fi_optslen;		/* default mount options len */
	int		fi_refcnt;		/* reference count */
	int		fi_flags;
	int		fi_mount_to;
	int		fi_rpc_to;
} fninfo_t;

/*
 * The AUTOFS locking scheme:
 *
 * The locks:
 * 	fn_lock: protects the fn_node. It must be grabbed to change any
 *		 field on the fn_node, except for those protected by
 *		 fn_rwlock.
 *
 * 	fn_rwlock: readers/writers lock to protect the subdirectory and
 *		   top level list traversal.
 *		   Protects: fn_dirents
 *			     fn_next
 *		             fn_size
 *		             fn_linkcnt
 *                 - Grab readers when checking if certain fn_node exists
 *                   under fn_dirents.
 *		   - Grab readers when attempting to reference a node
 *                   pointed to by fn_dirents, fn_next, and fn_parent.
 *                 - Grab writers to add a new fnnode under fn_dirents and
 *		     to remove a node pointed to by fn_dirents or fn_next.
 *
 *
 * The flags:
 *	MF_INPROG:
 *		- Indicates a mount request has been sent to the daemon.
 *		- If this flag is set, the thread sets MF_WAITING on the
 *                fnnode and sleeps.
 *
 *	MF_WAITING:
 *		- Set by a thread when it puts itself to sleep waiting for
 *		  the ongoing operation on this fnnode to be done.
 *
 * 	MF_LOOKUP:
 * 		- Indicates a lookup request has been sent to the daemon.
 *		- If this flag is set, the thread sets MF_WAITING on the
 *                fnnode and sleeps.
 *
 *	MF_IK_MOUNT:
 *		- This flag is set to indicate the mount was done in the
 *		  kernel, and so should the unmount.
 *
 *	MF_DIRECT:
 *		- Direct mountpoint if set, indirect otherwise.
 *
 *	MF_TRIGGER:
 *		- This is a trigger node.
 *
 *	MF_THISUID_MATCH_RQD:
 *		- User-relative context binding kind of node.
 *		- Node with this flag set requires a name match as well
 *		  as a cred match in order to be returned from the directory
 *		  hierarchy.
 */

/*
 * The inode of AUTOFS
 */
typedef struct fnnode {
	char		*fn_name;
	char		*fn_symlink;		/* if VLNK, this is what it */
						/* points to */
	int		fn_namelen;
	int		fn_symlinklen;
	uint_t		fn_linkcnt;		/* link count */
	mode_t		fn_mode;		/* file mode bits */
	uid_t		fn_uid;			/* owner's uid */
	gid_t		fn_gid;			/* group's uid */
	int		fn_error;		/* mount/lookup error */
	ino_t		fn_nodeid;
	off_t		fn_offset;		/* offset into directory */
	int		fn_flags;
	uint_t		fn_size;		/* size of directory */
	struct vnode	fn_vnode;
	struct fnnode	*fn_parent;
	struct fnnode	*fn_next;		/* sibling */
	struct fnnode	*fn_dirents;		/* children */
	struct fnnode	*fn_trigger; 		/* pointer to next level */
						/* AUTOFS trigger nodes */
	struct action_list *fn_alp;		/* Pointer to mount info */
						/* used for remounting */
						/* trigger nodes */
	cred_t		*fn_cred;		/* pointer to cred, used for */
						/* "thisuser" processing */
	krwlock_t	fn_rwlock;		/* protects list traversal */
	kmutex_t	fn_lock;		/* protects the fnnode */
	timestruc_t	fn_atime;
	timestruc_t	fn_mtime;
	timestruc_t	fn_ctime;
	time_t		fn_ref_time;		/* time last referenced */
	time_t		fn_unmount_ref_time;	/* last time unmount was done */
	kcondvar_t	fn_cv_mount;		/* mount blocking variable */
	struct vnode	*fn_seen;		/* vnode already traversed */
	kthread_t	*fn_thread;		/* thread that has currently */
						/* modified fn_seen */
} fnnode_t;

#define	vntofn(vp)	((struct fnnode *)((vp)->v_data))
#define	fntovn(fnp)	(&((fnp)->fn_vnode))
#define	vfstofni(vfsp)	((struct fninfo *)((vfsp)->vfs_data))

#define	MF_DIRECT	0x001
#define	MF_INPROG	0x002		/* Mount in progress */
#define	MF_WAITING	0x004
#define	MF_LOOKUP	0x008		/* Lookup in progress */
#define	MF_ATTR_WAIT	0x010
#define	MF_IK_MOUNT	0x040
#define	MF_TRIGGER	0x080
#define	MF_THISUID_MATCH_RQD	0x100	/* UID match required for this node */
					/* required for thisuser kind of */
					/* nodes */
#define	AUTOFS_MODE		0555
#define	AUTOFS_BLOCKSIZE	1024

struct autofs_callargs {
	fnnode_t	*fnc_fnp;	/* fnnode */
	char		*fnc_name;	/* path to lookup/mount */
	kthread_t	*fnc_origin;	/* thread that fired up this thread */
					/* used for debugging purposes */
	cred_t		*fnc_cred;
};

/*
 * Sets the MF_INPROG flag on this fnnode.
 * fnp->fn_lock should be held before this macro is called,
 * operation is either MF_INPROG or MF_LOOKUP.
 */
#define	AUTOFS_BLOCK_OTHERS(fnp, operation)	{ \
	ASSERT(MUTEX_HELD(&(fnp)->fn_lock)); \
	ASSERT(!((fnp)->fn_flags & operation)); \
	(fnp)->fn_flags |= (operation); \
}

#define	AUTOFS_UNBLOCK_OTHERS(fnp, operation)	{ \
	auto_unblock_others((fnp), (operation)); \
}

extern fnnode_t *rootfnnodep;
extern kmutex_t fnnode_count_lock;

extern struct vnodeops auto_vnodeops;

#define	BROWSE		"browse"
#define	NOBROWSE	"nobrowse"

/*
 * Utility routines
 */
extern int auto_search(fnnode_t *, char *, fnnode_t **, cred_t *);
extern int auto_enter(fnnode_t *, char *, fnnode_t **, cred_t *);
extern void auto_unblock_others(fnnode_t *, uint_t);
extern int auto_wait4mount(fnnode_t *);
extern fnnode_t *auto_makefnnode(vtype_t, vfs_t *, char *, cred_t *);
extern void auto_freefnnode(fnnode_t *);
extern void auto_disconnect(fnnode_t *, fnnode_t *);
extern void auto_do_unmount(void);
/*PRINTFLIKE2*/
extern void auto_log(int level, const char *fmt, ...);
/*PRINTFLIKE2*/
extern void auto_dprint(int level, const char *fmt, ...);
extern int auto_calldaemon(fninfo_t *, rpcproc_t, xdrproc_t, void *,
	xdrproc_t, void *, cred_t *, bool_t);
extern int auto_lookup_aux(fnnode_t *, char *, cred_t *);
extern void auto_new_mount_thread(fnnode_t *, char *, cred_t *);
extern int auto_nobrowse_option(char *);

/*
 * external routines not defined in any header file
 */
extern bool_t xdr_uid_t(XDR *, uid_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_AUTOFS_H */
