/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SHARE_H
#define	_SYS_SHARE_H

#pragma ident	"@(#)share.h	1.6	98/10/09 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Maximum size of a shrlock owner.
 * Must be large enough to handle a netobj.
 */
#define	MAX_SHR_OWNER_LEN	1024

/*
 * Contents of shrlock owner field for local share requests
 */
struct shr_locowner {
	pid_t	sl_pid;
	int	sl_id;
};

struct shrlock {
	short	s_access;
	short	s_deny;
	int32_t	s_sysid;	/* 0 if local otherwise passed by lm */
	pid_t	s_pid;		/* 0 if remote otherwise local pid */
	int	s_own_len;	/* if 0 and F_UNSHARE matching sysid */
	caddr_t	s_owner;	/* variable length opaque owner */
};

struct shrlocklist {
	struct shrlock *shr;
	struct shrlocklist *next;
};

#if defined(_KERNEL)
extern int add_share(struct vnode *, struct shrlock *);
extern int del_share(struct vnode *, struct shrlock *);
extern void cleanshares(struct vnode *, pid_t);
extern int shr_has_remote_shares(vnode_t *, int32_t);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SHARE_H */
