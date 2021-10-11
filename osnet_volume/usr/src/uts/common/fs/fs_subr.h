/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FS_SUBR_H
#define	_SYS_FS_SUBR_H

#pragma ident	"@(#)fs_subr.h	1.23	98/01/06 SMI"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <vm/page.h>
#include <sys/acl.h>
#include <sys/share.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Utilities shared among file system implementations.
 */

#ifdef	_KERNEL

extern int	fs_nosys();
extern int	fs_nosys_map(struct vnode *, offset_t, struct as *, caddr_t *,
			size_t, uchar_t, uchar_t, uint_t, struct cred *);
extern int	fs_nosys_addmap(struct vnode *, offset_t, struct as *, caddr_t,
			size_t, uchar_t, uchar_t, uint_t, struct cred *);
extern int	fs_nosys_poll(struct vnode *, short, int, short *,
			struct pollhead **);

extern int	fs_sync(struct vfs *, short, cred_t *);
extern void	fs_rwlock(vnode_t *, int);
extern void	fs_rwunlock(vnode_t *, int);
extern int	fs_cmp(vnode_t *, vnode_t *);
extern int	fs_frlock(vnode_t *, int, struct flock64 *, int, offset_t,
		    cred_t *);
extern int	fs_setfl(vnode_t *, int, int, cred_t *);
extern int	fs_poll(vnode_t *, short, int, short *, struct pollhead **);
extern int	fs_vcode(vnode_t *, uint_t *);
extern int	fs_pathconf(struct vnode *, int, ulong_t *, struct cred *);
extern void	clkset(time_t);
extern void	fs_dispose(struct vnode *, page_t *, int, int, struct cred *);
extern void	fs_nodispose(struct vnode *, page_t *, int, int, struct cred *);
extern int	fs_fab_acl(struct vnode *, vsecattr_t *, int flag, cred_t *);
extern int	fs_shrlock(struct vnode *, int, struct shrlock *, int);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_FS_SUBR_H */
