/*
 * Copyright (c) 1989-1991, 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_TMP_H
#define	_SYS_FS_TMP_H

#pragma ident	"@(#)tmp.h	1.29	99/11/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * tmpfs per-mount data structure.
 *
 * All fields are protected by tm_contents.
 * File renames on a particular file system are protected tm_renamelck.
 */
struct tmount {
	struct vfs	*tm_vfsp;	/* filesystem's vfs struct */
	struct tmpnode	*tm_rootnode;	/* root tmpnode */
	char 		*tm_mntpath;	/* name of tmpfs mount point */
	ulong_t		tm_anonmax;	/* file system max anon reservation */
	pgcnt_t		tm_anonmem;	/* pages of reserved anon memory */
	dev_t		tm_dev;		/* unique dev # of mounted `device' */
	uint_t		tm_gen;		/* pseudo generation number for files */
	kmutex_t	tm_contents;	/* lock for tmount structure */
	kmutex_t	tm_renamelck;	/* rename lock for this mount */
};

/*
 * File system independent to tmpfs conversion macros
 */
#define	VFSTOTM(vfsp)		((struct tmount *)(vfsp)->vfs_data)
#define	VTOTM(vp)		((struct tmount *)(vp)->v_vfsp->vfs_data)
#define	VTOTN(vp)		((struct tmpnode *)(vp)->v_data)
#define	TNTOV(tp)		((struct vnode *)&(tp)->tn_vnode)
#define	tmpnode_hold(tp)	VN_HOLD(TNTOV(tp))
#define	tmpnode_rele(tp)	VN_RELE(TNTOV(tp))

/*
 * enums
 */
enum de_op	{ DE_CREATE, DE_MKDIR, DE_LINK, DE_RENAME }; /* direnter ops */
enum dr_op	{ DR_REMOVE, DR_RMDIR, DR_RENAME };	/* dirremove ops */

/*
 * tmpfs_minfree is the amount (in pages) of anonymous memory that tmpfs
 * leaves free for the rest of the system.  E.g. in a system with 32MB of
 * configured swap space, if 16MB were reserved (leaving 16MB free),
 * tmpfs could allocate up to 16MB - tmpfs_minfree.  The default value
 * for tmpfs_minfree is btopr(TMPMINFREE) but it can cautiously patched
 * to a different number of pages.
 * NB: If tmpfs allocates too much swap space, other processes will be
 * unable to execute.
 */
#define	TMPMINFREE	2 * 1024 * 1024	/* 2 Megabytes */

extern long	tmpfs_minfree;		/* Anonymous memory in pages */

/*
 * tmpfs can allocate only a certain percentage of kernel memory,
 * which is used for tmpnodes, directories, file names, etc.
 * This is statically set as TMPMAXFRACKMEM of physical memory.
 * The actual number of allocatable bytes can be patched in tmpfs_maxkmem.
 */
#define	TMPMAXFRACKMEM	25	/* 1/25 of physical memory */

extern size_t 	tmp_kmemspace;
extern size_t	tmpfs_maxkmem;	/* Allocatable kernel memory in bytes */

extern	void	tmpnode_init(struct tmount *, struct tmpnode *,
	struct vattr *, struct cred *);
extern	int	tmpnode_trunc(struct tmount *, struct tmpnode *, ulong_t);
extern	void	tmpnode_growmap(struct tmpnode *, ulong_t);
extern	int	tdirlookup(struct tmpnode *, char *, struct tmpnode **,
    struct cred *);
extern	int	tdirdelete(struct tmpnode *, struct tmpnode *, char *,
	enum dr_op, struct cred *);
extern	void	tdirinit(struct tmpnode *, struct tmpnode *);
extern	void	tdirtrunc(struct tmpnode *);
extern	void	*tmp_memalloc(size_t, int);
extern	void	tmp_memfree(void *, size_t);
extern	int	tmp_resv(struct tmount *, struct tmpnode *, size_t, int);
extern	int	tmp_taccess(struct tmpnode *, int, struct cred *);
extern	int	tmp_sticky_remove_access(struct tmpnode *, struct tmpnode *,
	struct cred *);
extern	int	tmp_convnum(char *);
extern	int	tdirenter(struct tmount *, struct tmpnode *, char *,
	enum de_op, struct tmpnode *, struct tmpnode *, struct vattr *,
	struct tmpnode **, struct cred *);

#define	TMP_MUSTHAVE	0x01

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_TMP_H */
