/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IPC_H
#define	_SYS_IPC_H

#pragma ident	"@(#)ipc.h	1.22	97/09/09 SMI"	/* SVr4.0 11.10	*/

#include <sys/isa_defs.h>
#include <sys/feature_tests.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Common IPC Access Structure */

/*
 * The kernel supports both the SVR3 ipc_perm, 32-bit ipc_perm
 * structures and 64-bit ipc_perm structures simultaneously.
 */

struct ipc_perm {
	uid_t	uid;	/* owner's user id */
	gid_t	gid;	/* owner's group id */
	uid_t	cuid;	/* creator's user id */
	gid_t	cgid;	/* creator's group id */
	mode_t	mode;	/* access modes */
	uint_t	seq;	/* slot usage sequence number */
	key_t	key;	/* key */
#if !defined(_LP64)
	int	pad[4]; /* reserve area */
#endif
};

#if	defined(_SYSCALL32)

/* Kernel's view of the ILP32 ipc_perm structure */

struct ipc_perm32 {
	uid32_t	uid;	/* owner's user id */
	gid32_t	gid;	/* owner's group id */
	uid32_t	cuid;	/* creator's user id */
	gid32_t	cgid;	/* creator's group id */
	mode32_t mode;	/* access modes */
	uint32_t seq;	/* slot usage sequence number */
	key32_t	key;	/* key */
	int32_t	pad[4];	/* reserve area */
};


#endif	/* _SYSCALL32 */

#if defined(_KERNEL)

/*
 * Size invariant version of SVR3 structure. Only kept around
 * to support old binaries. Perhaps this can go away someday.
 */
struct o_ipc_perm32 {
	o_uid_t	uid;	/* owner's user id */
	o_gid_t	gid;	/* owner's group id */
	o_uid_t	cuid;	/* creator's user id */
	o_gid_t	cgid;	/* creator's group id */
	o_mode_t 	mode;	/* access modes */
	uint16_t	seq;	/* slot usage sequence number */
	key32_t		key;	/* key */
};
#endif	/* _KERNEL */



/* Common IPC Definitions. */
/* Mode bits. */
#define	IPC_ALLOC	0100000		/* entry currently allocated */
#define	IPC_CREAT	0001000		/* create entry if key doesn't exist */
#define	IPC_EXCL	0002000		/* fail if key exists */
#define	IPC_NOWAIT	0004000		/* error if request must wait */

/* Keys. */
#define	IPC_PRIVATE	(key_t)0	/* private key */

/* Control Commands. */

#define	IPC_RMID	10	/* remove identifier */
#define	IPC_SET		11	/* set options */
#define	IPC_STAT	12	/* get options */

#if defined(_KERNEL)
	/* For compatibility */
#define	IPC_O_RMID	0	/* remove identifier */
#define	IPC_O_SET	1	/* set options */
#define	IPC_O_STAT	2	/* get options */

#endif	/* _KERNEL */

#if (!defined(_KERNEL) && !defined(_XOPEN_SOURCE)) || defined(_XPG4_2) || \
	defined(__EXTENSIONS__)
#if defined(__STDC__)
key_t ftok(const char *, int);
#else
key_t ftok();
#endif /* defined(__STDC__) */
#endif /* (!defined(_KERNEL) && !defined(_XOPEN_SOURCE))... */

#if defined(_KERNEL)
int	ipcaccess(struct ipc_perm *, int, struct cred *);
int	ipcget(key_t, int, struct ipc_perm *, ssize_t, size_t, int *,
		struct ipc_perm **);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IPC_H */
