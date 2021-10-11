/*
 * Copyright (c) 1991,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FS_UFS_TRANS_H
#define	_SYS_FS_UFS_TRANS_H

#pragma ident	"@(#)ufs_trans.h	1.55	99/07/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/types.h>
#include	<sys/cred.h>
#include	<sys/fs/ufs_fs.h>

/*
 * Per-device transaction information, providing the link between UFS and the
 * metatrans device.  The list of these is searched at mount time.
 */
struct ufstrans {
	struct ufstrans		*ut_next;	/* next item in list */
	dev_t			ut_dev;		/* metatrans device no. */
	struct ufstransops	*ut_ops;	/* metatrans ops */
	struct vfs		*ut_vfsp;	/* XXX for inode pushes */
	void			*ut_data;	/* private data (?) */
	void			(*ut_onerror)();  /* callback to ufs on error */
	int			ut_onerror_state; /* ufs specific state */
	int			ut_validfs;	/* indicates mounted fs */
};
/*
 * values for ut_validfs
 */
#define	UT_UNMOUNTED	(0)
#define	UT_MOUNTED	(1)
#define	UT_HLOCKING	(2)

/*
 * Types of deltas
 */
typedef enum delta_type {
	DT_NONE,	/* no assigned type */
	DT_SB,		/* superblock */
	DT_CG,		/* cylinder group */
	DT_SI,		/* summary info */
	DT_AB,		/* allocation block */
	DT_ABZERO,	/* a zero'ed allocation block */
	DT_DIR,		/* directory */
	DT_INODE,	/* inode */
	DT_FBI,		/* fbiwrite */
	DT_QR,		/* quota record */
	DT_COMMIT,	/* commit record */
	DT_CANCEL,	/* cancel record */
	DT_BOT,		/* begin transaction */
	DT_EOT,		/* end   transaction */
	DT_UD,		/* userdata */
	DT_SUD,		/* userdata found during log scan */
	DT_SHAD,	/* data for a shadow inode */
	DT_MAX
} delta_t;

/*
 * transaction operation types
 */
typedef enum top_type {
	TOP_READ_SYNC,
	TOP_WRITE,
	TOP_WRITE_SYNC,
	TOP_SETATTR,
	TOP_CREATE,
	TOP_REMOVE,
	TOP_LINK,
	TOP_RENAME,
	TOP_MKDIR,
	TOP_RMDIR,
	TOP_SYMLINK,
	TOP_FSYNC,
	TOP_GETPAGE,
	TOP_PUTPAGE,
	TOP_SBUPDATE_FLUSH,
	TOP_SBUPDATE_UPDATE,
	TOP_SBUPDATE_UNMOUNT,
	TOP_SYNCIP_CLOSEDQ,
	TOP_SYNCIP_FLUSHI,
	TOP_SYNCIP_HLOCK,
	TOP_SYNCIP_SYNC,
	TOP_SYNCIP_FREE,
	TOP_SBWRITE_RECLAIM,
	TOP_SBWRITE_STABLE,
	TOP_IFREE,
	TOP_IUPDAT,
	TOP_MOUNT,
	TOP_COMMIT_ASYNC,
	TOP_COMMIT_FLUSH,
	TOP_COMMIT_UPDATE,
	TOP_COMMIT_UNMOUNT,
	TOP_SETSECATTR,
	TOP_QUOTA,
	TOP_ITRUNC,
	TOP_MAX
} top_t;

struct inode;
struct ufsvfs;

struct ufstransops {
	void	(*trans_begin_sync)(struct ufstrans *, top_t, size_t);
	int	(*trans_begin_async)(struct ufstrans *, top_t, size_t, int);
	void	(*trans_end_sync)(struct ufstrans *, int *, top_t, size_t);
	void	(*trans_end_async)(struct ufstrans *, top_t, size_t);
	void	(*trans_delta)(struct ufstrans *, offset_t, off_t, delta_t,
			int (*)(), uintptr_t);
	int	(*trans_ud_delta)(struct ufstrans *, offset_t, off_t, delta_t,
			int (*)(), uintptr_t);
	void	(*trans_cancel)(struct ufstrans *, offset_t, off_t);
	void	(*trans_log)(struct ufstrans *, char *, offset_t, off_t);
	int	(*trans_iscancel)(struct ufstrans *, offset_t, off_t);
	void	(*trans_seterror)(struct ufstrans *);
	int	(*trans_iserror)(struct ufstrans *);
	/*
	 * debug ops
	 */
	void	(*trans_mataadd)(struct ufstrans *, offset_t, off_t);
	void	(*trans_matadel)(struct ufstrans *, offset_t, off_t);
	void	(*trans_mataclr)(struct ufstrans *);
};

/*
 * vfs_trans == NULL means no metatrans device
 */
#define	TRANS_ISTRANS(ufsvfsp)	(ufsvfsp->vfs_trans)

/*
 * begin a synchronous transaction
 */
#define	TRANS_BEGIN_SYNC(ufsvfsp, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		(*ufsvfsp->vfs_trans->ut_ops->trans_begin_sync)\
			(ufsvfsp->vfs_trans, vid, vsize); \
}

/*
 * begin a asynchronous transaction
 */
#define	TRANS_BEGIN_ASYNC(ufsvfsp, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		(void) (*ufsvfsp->vfs_trans->ut_ops->trans_begin_async)\
			(ufsvfsp->vfs_trans, vid, vsize, 0); \
}

/*
 * try to begin a asynchronous transaction
 */
#define	TRANS_TRY_BEGIN_ASYNC(ufsvfsp, vid, vsize, err)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		err = (*ufsvfsp->vfs_trans->ut_ops->trans_begin_async)\
			(ufsvfsp->vfs_trans, vid, vsize, 1); \
	else\
		err = 0; \
}

/*
 * begin a synchronous or asynchronous transaction
 */
#define	TRANS_BEGIN_CSYNC(ufsvfsp, issync, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp)) {\
		if (ufsvfsp->vfs_syncdir || curthread->t_flag & T_DONTPEND) {\
			(*ufsvfsp->vfs_trans->ut_ops->trans_begin_sync)\
				(ufsvfsp->vfs_trans, vid, vsize); \
			issync = 1; \
		} else {\
			(*ufsvfsp->vfs_trans->ut_ops->trans_begin_async)\
				(ufsvfsp->vfs_trans, vid, vsize, 0); \
			issync = 0; \
		}\
	}\
}

/*
 * try to begin a synchronous or asynchronous transaction
 */

#define	TRANS_TRY_BEGIN_CSYNC(ufsvfsp, issync, vid, vsize, error)\
{\
	if (TRANS_ISTRANS(ufsvfsp)) {\
		if (ufsvfsp->vfs_syncdir || curthread->t_flag & T_DONTPEND) {\
			(*ufsvfsp->vfs_trans->ut_ops->trans_begin_sync)\
				(ufsvfsp->vfs_trans, vid, vsize); \
			issync = 1; \
			error = 0; \
		} else {\
			error = (*ufsvfsp->vfs_trans->ut_ops->\
				trans_begin_async)\
				(ufsvfsp->vfs_trans, vid, vsize, 1); \
			issync = 0; \
		}\
	}\
}\


/*
 * end a asynchronous transaction
 */
#define	TRANS_END_ASYNC(ufsvfsp, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		(*ufsvfsp->vfs_trans->ut_ops->trans_end_async)\
			(ufsvfsp->vfs_trans, vid, vsize); \
}

/*
 * end a synchronous transaction
 */
#define	TRANS_END_SYNC(ufsvfsp, error, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		(*ufsvfsp->vfs_trans->ut_ops->trans_end_sync)\
			(ufsvfsp->vfs_trans, &error, vid, vsize); \
}

/*
 * end a synchronous or asynchronous transaction
 */
#define	TRANS_END_CSYNC(ufsvfsp, error, issync, vid, vsize)\
{\
	if (TRANS_ISTRANS(ufsvfsp))\
		if (issync)\
			(*ufsvfsp->vfs_trans->ut_ops->trans_end_sync)\
				(ufsvfsp->vfs_trans, &error, vid, vsize); \
		else\
			(*ufsvfsp->vfs_trans->ut_ops->trans_end_async)\
				(ufsvfsp->vfs_trans, vid, vsize); \
}
/*
 * record a delta
 */
#define	TRANS_DELTA(ufsvfsp, mof, nb, dtyp, func, arg) \
	if (TRANS_ISTRANS(ufsvfsp)) \
		(*ufsvfsp->vfs_trans->ut_ops->trans_delta)\
			(ufsvfsp->vfs_trans, (offset_t)(mof), nb, dtyp, \
			func, arg)

/*
 * conditionally record a userdata delta
 */
#define	TRANS_UD_DELTA(ufsvfsp, mof, nb, dtyp, func, arg) \
	((TRANS_ISTRANS(ufsvfsp)) ? \
		(*ufsvfsp->vfs_trans->ut_ops->trans_ud_delta)\
			(ufsvfsp->vfs_trans, (offset_t)(mof), nb, dtyp, \
			func, arg) : 0)
/*
 * cancel a delta
 */
#define	TRANS_CANCEL(ufsvfsp, mof, nb) \
	if (TRANS_ISTRANS(ufsvfsp)) \
		(*ufsvfsp->vfs_trans->ut_ops->trans_cancel)\
			(ufsvfsp->vfs_trans, (offset_t)(mof), nb)
/*
 * log a delta
 */
#define	TRANS_LOG(ufsvfsp, va, mof, nb) \
	if (TRANS_ISTRANS(ufsvfsp)) \
		(*ufsvfsp->vfs_trans->ut_ops->trans_log)\
			(ufsvfsp->vfs_trans, va, (offset_t)(mof), nb)
/*
 * check if a range is being canceled (converting from metadata into userdata)
 */
#define	TRANS_ISCANCEL(ufsvfsp, mof, nb) \
	((TRANS_ISTRANS(ufsvfsp)) ? \
		(*ufsvfsp->vfs_trans->ut_ops->trans_iscancel)\
			(ufsvfsp->vfs_trans, (offset_t)(mof), nb) : 0)
/*
 * put the metatrans device into error state
 */
#define	TRANS_SETERROR(ufsvfsp) \
	if (TRANS_ISTRANS(ufsvfsp)) \
		(*ufsvfsp->vfs_trans->ut_ops->trans_seterror) \
			(ufsvfsp->vfs_trans)
/*
 * check if device has had an error
 */
#define	TRANS_ISERROR(ufsvfsp) \
	((TRANS_ISTRANS(ufsvfsp)) ? \
		(*ufsvfsp->vfs_trans->ut_ops->trans_iserror)\
			(ufsvfsp->vfs_trans) : 0)
/*
 * The following macros provide a more readable interface to TRANS_DELTA
 */
#define	TRANS_BUF(ufsvfsp, vof, nb, bp, type) \
	TRANS_DELTA(ufsvfsp, \
		ldbtob(bp->b_blkno) + (offset_t)(vof), nb, type, \
		ufs_trans_push_buf, bp->b_blkno)

#define	TRANS_BUF_ITEM(ufsvfsp, item, base, bp, type) \
	TRANS_BUF(ufsvfsp, (caddr_t)&(item) - (caddr_t)(base), \
		sizeof (item), bp, type)

#define	TRANS_BUF_ITEM_128(ufsvfsp, item, base, bp, type) \
	TRANS_BUF(ufsvfsp, \
	(((uintptr_t)&(item)) & ~(128 - 1)) - (uintptr_t)(base), 128, bp, type)

#define	TRANS_UD_BUF(ufsvfsp, islogged, vof, nb, bp, type) \
{ \
	islogged = TRANS_UD_DELTA(ufsvfsp, \
		ldbtob(bp->b_blkno) + (offset_t)(vof), nb, type, \
		ufs_trans_push_buf, bp->b_blkno); \
}

#define	TRANS_INODE(ufsvfsp, ip) \
	TRANS_DELTA(ufsvfsp, ip->i_doff, sizeof (struct dinode), \
			DT_INODE, ufs_trans_push_inode, ip->i_number)

#define	TRANS_INODE_DELTA(ufsvfsp, vof, nb, ip) \
	TRANS_DELTA(ufsvfsp, (ip->i_doff + (offset_t)(vof)), \
		nb, DT_INODE, ufs_trans_push_inode, ip->i_number)

#define	TRANS_INODE_FIELD(ufsvfsp, field, ip) \
	TRANS_INODE_DELTA(ufsvfsp, (caddr_t)&(field) - (caddr_t)&ip->i_ic, \
		sizeof (field), ip)

#define	TRANS_SI(ufsvfsp, fs, cg) \
	TRANS_DELTA(ufsvfsp, \
		ldbtob(fsbtodb(fs, fs->fs_csaddr)) + \
		((caddr_t)&fs->fs_cs(fs, cg) - (caddr_t)fs->fs_u.fs_csp), \
		sizeof (struct csum), DT_SI, ufs_trans_push_si, cg)

#define	TRANS_DIR(ip, offset) \
	(TRANS_ISTRANS(ip->i_ufsvfs) ? ufs_trans_dir(ip, offset) : 0)

#define	TRANS_QUOTA(dqp)	\
	if (TRANS_ISTRANS(dqp->dq_ufsvfsp))	\
		ufs_trans_quota(dqp);

#define	TRANS_DQRELE(ufsvfsp, dqp) \
	if (TRANS_ISTRANS(ufsvfsp) && \
	    ((curthread->t_flag & T_DONTBLOCK) == 0)) { \
		ufs_trans_dqrele(dqp); \
	} else { \
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER); \
		dqrele(dqp); \
		rw_exit(&ufsvfsp->vfs_dqrwlock); \
	}

#define	TRANS_ITRUNC(ip, length, flags, cr)	\
	ufs_trans_itrunc(ip, length, flags, cr);

#define	TRANS_WRITE_RESV(ip, uiop, ulp, resvp, residp)	\
	if ((TRANS_ISTRANS(ip->i_ufsvfs) != NULL) && (ulp != NULL)) \
		ufs_trans_write_resv(ip, uiop, resvp, residp);

#define	TRANS_WRITE(ip, uiop, ioflag, err, ulp, cr, resv, resid)	\
	if ((TRANS_ISTRANS(ip->i_ufsvfs) != NULL) && (ulp != NULL)) \
		err = ufs_trans_write(ip, uiop, ioflag, cr, resv, resid); \
	else \
		err = wrip(ip, uiop, ioflag, cr);

/*
 * These functions "wrap" functions that are not VOP or VFS
 * entry points but must still use the TRANS_BEGIN/TRANS_END
 * protocol
 */
#define	TRANS_SBUPDATE(ufsvfsp, vfsp, topid) \
	ufs_trans_sbupdate(ufsvfsp, vfsp, topid)
#define	TRANS_SYNCIP(ip, bflags, iflag, topid) \
	ufs_trans_syncip(ip, bflags, iflag, topid)
#define	TRANS_SBWRITE(ufsvfsp, topid)	ufs_trans_sbwrite(ufsvfsp, topid)
#define	TRANS_IUPDAT(ip, waitfor)	ufs_trans_iupdat(ip, waitfor)

/*
 * Test/Debug ops
 *	The following ops maintain the metadata map.
 *	The metadata map is a debug/test feature.
 *	These ops are *not* used in the production product.
 */

/*
 * is metatrans device doing metadata checking?
 */
#define	TRANS_DOMATAMAP(ufsvfsp) \
	ufsvfsp->vfs_domatamap = \
		(TRANS_ISTRANS(ufsvfsp) && \
		ufsvfsp->vfs_trans->ut_ops->trans_mataadd)

#define	TRANS_MATA_IGET(ufsvfsp, ip) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_iget(ip)

#define	TRANS_MATA_FREE(ufsvfsp, mof, nb) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_free(ufsvfsp, (offset_t)(mof), nb)

#define	TRANS_MATA_ALLOC(ufsvfsp, ip, bno, size, zero) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_alloc(ufsvfsp, ip, bno, size, zero)

#define	TRANS_MATA_MOUNT(ufsvfsp) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_mount(ufsvfsp)

#define	TRANS_MATA_UMOUNT(ufsvfsp) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_umount(ufsvfsp)

#define	TRANS_MATA_SI(ufsvfsp, fs) \
	if (ufsvfsp->vfs_domatamap) \
		ufs_trans_mata_si(ufsvfsp, fs)

#define	TRANS_MATAADD(ufsvfsp, mof, nb) \
	(*ufsvfsp->vfs_trans->ut_ops->trans_mataadd)\
		(ufsvfsp->vfs_trans, (offset_t)(mof), nb)

#define	TRANS_MATADEL(ufsvfsp, mof, nb) \
	(*ufsvfsp->vfs_trans->ut_ops->trans_matadel)\
		(ufsvfsp->vfs_trans, (offset_t)(mof), nb)

#define	TRANS_MATACLR(ufsvfsp) \
	(*ufsvfsp->vfs_trans->ut_ops->trans_mataclr)(ufsvfsp->vfs_trans)

#include	<sys/fs/ufs_quota.h>
#include	<sys/fs/ufs_lockfs.h>
/*
 * identifies the type of opetation passed into TRANS_BEGIN/END
 */
#define	TOP_SYNC		(0x00000001)
#define	TOP_ASYNC		(0x00000002)
/*
 *  estimated values
 */
#define	HEADERSIZE		(128)
#define	ALLOCSIZE		(160)
#define	INODESIZE		(sizeof (struct dinode) + HEADERSIZE)
#define	SIZESB			((sizeof (struct fs)) + HEADERSIZE)
#define	SIZEDIR			(DIRBLKSIZ + HEADERSIZE)
/*
 * calculated values
 */
#define	SIZECG(IP)		((IP)->i_fs->fs_cgsize + HEADERSIZE)
#define	FRAGSIZE(IP)		((IP)->i_fs->fs_fsize + HEADERSIZE)
#define	ACLSIZE(IP)		(((IP)->i_ufsvfs->vfs_maxacl + HEADERSIZE) + \
					INODESIZE)
#define	MAXACLSIZE		((MAX_ACL_ENTRIES << 1) * sizeof (aclent_t))
#define	DIRSIZE(IP)		(INODESIZE + (4 * ALLOCSIZE) + \
				    (IP)->i_fs->fs_fsize + HEADERSIZE)
#define	QUOTASIZE		sizeof (struct dquot) + HEADERSIZE
/*
 * size calculations
 */
#define	TOP_CREATE_SIZE(IP)	\
	(ACLSIZE(IP) + SIZECG(IP) + DIRSIZE(IP) + INODESIZE)
#define	TOP_REMOVE_SIZE(IP)	\
	DIRSIZE(IP)  + SIZECG(IP) + INODESIZE + SIZESB
#define	TOP_LINK_SIZE(IP)	\
	DIRSIZE(IP) + INODESIZE
#define	TOP_RENAME_SIZE(IP)	\
	DIRSIZE(IP) + DIRSIZE(IP) + SIZECG(IP)
#define	TOP_MKDIR_SIZE(IP)	\
	DIRSIZE(IP) + INODESIZE + DIRSIZE(IP) + INODESIZE + FRAGSIZE(IP) + \
	    SIZECG(IP) + ACLSIZE(IP)
#define	TOP_SYMLINK_SIZE(IP)	\
	DIRSIZE((IP)) + INODESIZE + INODESIZE + SIZECG(IP)
#define	TOP_GETPAGE_SIZE(IP)	\
	ALLOCSIZE + ALLOCSIZE + ALLOCSIZE + INODESIZE + SIZECG(IP)
#define	TOP_SYNCIP_SIZE		INODESIZE
#define	TOP_FSYNC_SIZE		INODESIZE
#define	TOP_READ_SIZE		INODESIZE
#define	TOP_RMDIR_SIZE		(SIZESB + (INODESIZE * 2) + SIZEDIR)
#define	TOP_SETQUOTA_SIZE(FS)	((FS)->fs_bsize << 2)
#define	TOP_QUOTA_SIZE		(QUOTASIZE)
#define	TOP_SETSECATTR_SIZE(IP)	(MAXACLSIZE)
#define	TOP_IUPDAT_SIZE(IP)	INODESIZE + SIZECG(IP)
#define	TOP_SBUPDATE_SIZE	(SIZESB)
#define	TOP_SBWRITE_SIZE	(SIZESB)
#define	TOP_PUTPAGE_SIZE(IP)	(INODESIZE + SIZECG(IP))
#define	TOP_SETATTR_SIZE(IP)	(SIZECG(IP) + INODESIZE + QUOTASIZE + \
		ACLSIZE(IP))
#define	TOP_IFREE_SIZE(IP)	(SIZECG(IP) + INODESIZE + QUOTASIZE)
#define	TOP_MOUNT_SIZE		(SIZESB)
#define	TOP_COMMIT_SIZE		(0)

/*
 * The minimum log size is 1M.  So we will allow 1 fs operation to
 * reserve at most 512K of log space.
 */
#define	TOP_MAX_RESV	(512 * 1024)


/*
 * ufs trans function prototypes
 */
#if defined(_KERNEL) && defined(__STDC__)

extern struct ufstrans	*ufs_trans_get(dev_t, struct vfs *);
extern int		ufs_trans_put(dev_t);
extern int		ufs_trans_hlock();
extern void		ufs_trans_onerror();
extern void		ufs_trans_reset(dev_t);
extern struct ufstrans	*ufs_trans_set(dev_t, struct ufstransops *, void *);
extern int		ufs_trans_push_inode(struct ufstrans *, delta_t, ino_t);
extern int		ufs_trans_push_buf(struct ufstrans *, delta_t, daddr_t);
extern int		ufs_trans_push_si(struct ufstrans *, delta_t, int);
extern void		ufs_trans_sbupdate(struct ufsvfs *, struct vfs *,
				top_t);
extern int		ufs_trans_syncip(struct inode *, int, int, top_t);
extern void		ufs_trans_sbwrite(struct ufsvfs *, top_t);
extern void		ufs_trans_iupdat(struct inode *, int);
extern void		ufs_trans_mata_mount(struct ufsvfs *);
extern void		ufs_trans_mata_umount(struct ufsvfs *);
extern void		ufs_trans_mata_si(struct ufsvfs *, struct fs *);
extern void		ufs_trans_mata_iget(struct inode *);
extern void		ufs_trans_mata_free(struct ufsvfs *, offset_t, off_t);
extern void		ufs_trans_mata_alloc(struct ufsvfs *, struct inode *,
				daddr_t, ulong_t, int);
extern int		ufs_trans_dir(struct inode *, off_t);
extern void		ufs_trans_quota(struct dquot *);
extern void		ufs_trans_dqrele(struct dquot *);
extern int		ufs_trans_itrunc(struct inode *, u_offset_t, int,
			    cred_t *);
extern int		ufs_trans_write(struct inode *, struct uio *, int,
			    cred_t *, int, long);
extern void		ufs_trans_write_resv(struct inode *, struct uio *,
				int *, int *);
extern int		ufs_trans_check(dev_t);

#endif	/* defined(_KERNEL) && defined(__STDC__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_UFS_TRANS_H */
