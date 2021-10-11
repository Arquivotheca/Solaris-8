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

#ifndef _SYS_CRED_H
#define	_SYS_CRED_H

#pragma ident	"@(#)cred.h	1.21	97/01/09 SMI"	/* SVr4.0 1.8	*/

#ifdef _KERNEL
#include <sys/t_lock.h>
#endif /* KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * User credentials.  The size of the cr_groups[] array is configurable
 * but is the same (ngroups_max) for all cred structures; cr_ngroups
 * records the number of elements currently in use, not the array size.
 */

typedef struct cred {
	uint_t	cr_ref;			/* reference count */
	uid_t	cr_uid;			/* effective user id */
	gid_t	cr_gid;			/* effective group id */
	uid_t	cr_ruid;		/* real user id */
	gid_t	cr_rgid;		/* real group id */
	uid_t	cr_suid;		/* "saved" user id (from exec) */
	gid_t	cr_sgid;		/* "saved" group id (from exec) */
	uint_t	cr_ngroups;		/* number of groups in cr_groups */
	gid_t	cr_groups[1];		/* supplementary group list */
} cred_t;

#ifdef _KERNEL

#define	CRED()		curthread->t_cred

struct proc;				/* cred.h is included in proc.h */

extern int ngroups_max;
/*
 * kcred is used when you need root permission.
 */
extern struct cred *kcred;

extern void cred_init(void);
extern void crhold(cred_t *);
extern void crfree(cred_t *);
extern cred_t *crget(void);
extern cred_t *crcopy(cred_t *);
extern void crcopy_to(cred_t *, cred_t *);
extern cred_t *crdup(cred_t *);
extern void crdup_to(cred_t *, cred_t *);
extern cred_t *crgetcred(void);
extern void crset(struct proc *, cred_t *);
extern int suser(cred_t *);
extern int groupmember(gid_t, cred_t *);
extern int hasprocperm(cred_t *, cred_t *);
extern int crcmp(cred_t *, cred_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CRED_H */
