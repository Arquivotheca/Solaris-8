/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_NAMENODE_H
#define	_SYS_FS_NAMENODE_H

#pragma ident	"@(#)namenode.h	1.19	96/07/28 SMI"	/* SVr4.0 1.4	*/

#if defined(_KERNEL)
#include <sys/vnode.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This structure is used to pass a file descriptor from user
 * level to the kernel. It is first used by fattach() and then
 * be NAMEFS.
 */
struct namefd {
	int fd;
};

#if defined(_KERNEL)
/*
 * Each NAMEFS object is identified by a struct namenode/vnode pair.
 */
struct namenode {
	struct vnode    nm_vnode;	/* represents mounted file desc. */
	int		nm_flag;	/* flags defined below */
	struct vattr    nm_vattr;	/* attributes of mounted file desc. */
	struct vnode	*nm_filevp;	/* file desc. prior to mounting */
	struct file	*nm_filep;	/* file pointer of nm_filevp */
	struct vnode	*nm_mountpt;	/* mount point prior to mounting */
	struct namenode *nm_nextp;	/* next link in the linked list */
	kmutex_t	nm_lock;	/* protects nm_vattr */
};

/*
 * Valid flags for namenodes.
 */
#define	NMNMNT		0x01	/* namenode not mounted */

/*
 * Macros to convert a vnode to a namenode, and vice versa.
 */
#define	VTONM(vp) ((struct namenode *)((vp)->v_data))
#define	NMTOV(nm) (&(nm)->nm_vnode)

#define	NM_FILEVP_HASH_SIZE	64
#define	NM_FILEVP_HASH_MASK	(NM_FILEVP_HASH_SIZE - 1)
#define	NM_FILEVP_HASH_SHIFT	7
#define	NM_FILEVP_HASH(vp)	(&nm_filevp_hash[(((uintptr_t)vp) >> \
	NM_FILEVP_HASH_SHIFT) & NM_FILEVP_HASH_MASK])

extern struct namenode *nm_filevp_hash[NM_FILEVP_HASH_SIZE];
extern struct vfs namevfs;

extern int nameinit(struct vfssw *, int);
extern int nm_unmountall(struct vnode *, struct cred *);
extern void nameinsert(struct namenode *);
extern void nameremove(struct namenode *);
extern struct namenode *namefind(struct vnode *, struct vnode *);
extern struct vnodeops nm_vnodeops;
extern kmutex_t ntable_lock;

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_NAMENODE_H */
