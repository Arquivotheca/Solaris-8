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
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _SYS_DNLC_H
#define	_SYS_DNLC_H

#pragma ident	"@(#)dnlc.h	1.17	98/03/18 SMI"	/* SVr4.0 1.4.1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This structure describes the elements in the cache of recent
 * names looked up.
 */

struct ncache {
	struct ncache *hash_next; 	/* hash chain, MUST BE FIRST */
	struct ncache *hash_prev;
	struct ncache *next_free; 	/* freelist chain */
	struct vnode *vp;		/* vnode the name refers to */
	struct vnode *dp;		/* vnode of parent of name */
	struct cred *cred;		/* credentials */
	char *name;			/* segment name */
	int namlen;			/* length of name */
	int hash;			/* hash signature */
};

/*
 * Stats on usefulness of name cache.
 */
struct ncstats {
	int	hits;		/* hits that we can really use */
	int	misses;		/* cache misses */
	int	enters;		/* number of enters done */
	int	dbl_enters;	/* number of enters tried when already cached */
	int	long_enter;	/* deprecated, no longer accounted */
	int	long_look;	/* deprecated, no longer accounted */
	int	move_to_front;	/* entry moved to front of hash chain */
	int	purges;		/* number of purges of cache */
};

#define	ANYCRED	((cred_t *)-1)
#define	NOCRED	((cred_t *)0)

#if defined(_KERNEL)

#include <sys/vfs.h>
#include <sys/vnode.h>

extern int ncsize;

/*
 * External routines.
 */
void	dnlc_init(void);
void	dnlc_enter(vnode_t *, char *, vnode_t *, cred_t *);
void	dnlc_update(vnode_t *, char *, vnode_t *, cred_t *);
vnode_t	*dnlc_lookup(vnode_t *, char *, cred_t *);
void	dnlc_purge(void);
void	dnlc_purge_vp(vnode_t *);
int	dnlc_purge_vfsp(vfs_t *, int);
void	dnlc_remove(vnode_t *, char *);
int	dnlc_fs_purge1(struct vnodeops *);

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DNLC_H */
