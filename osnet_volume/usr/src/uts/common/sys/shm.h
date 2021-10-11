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
 *	Copyright (c) 1986-1989,1997-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_SHM_H
#define	_SYS_SHM_H

#pragma ident	"@(#)shm.h	1.44	99/09/23 SMI"	/* SVr4.0 11.19 */

#include <sys/ipc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	IPC Shared Memory Facility.
 */

/*
 *	Implementation Constants.
 */
#if (defined(_KERNEL) || defined(_KMEMUSER))

#define	SHMLBA	PAGESIZE	/* segment low boundary address multiple */
				/* (SHMLBA must be a power of 2) */
#else
#include <sys/unistd.h>		/* needed for _SC_PAGESIZE */
extern long _sysconf(int);	/* System Private interface to sysconf() */
#define	SHMLBA	(_sysconf(_SC_PAGESIZE))
#endif	/* defined(_KERNEL) || defined(_KMEMUSER)) */

/*
 *	Permission Definitions.
 */
#define	SHM_R	0400	/* read permission */
#define	SHM_W	0200	/* write permission */

/*
 *	Message Operation Flags.
 */
#define	SHM_RDONLY	010000	/* attach read-only (else read-write) */
#define	SHM_RND		020000	/* round attach address to SHMLBA */
#define	SHM_SHARE_MMU	040000  /* share VM resources such as page table */

/*
 *	Segacct Flags.
 */

#define	SHMSA_ISM	1	/* uses shared page table */

typedef unsigned long shmatt_t;

/*
 *	Structure Definitions.
 */

/*
 *	There is a shared mem id data structure (shmid_ds) for each
 *	segment in the system.
 */
#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/t_lock.h>

struct shmid_ds {
	struct ipc_perm shm_perm;	/* operation permission struct */
	size_t		shm_segsz;	/* size of segment in bytes */
	struct anon_map	*shm_amp;	/* segment anon_map pointer */
	ushort_t	shm_lkcnt;	/* number of times it is being locked */
	pid_t		shm_lpid;	/* pid of last shmop */
	pid_t		shm_cpid;	/* pid of creator */
	shmatt_t	shm_nattch;	/* used only for shminfo */
	ulong_t		shm_cnattch;	/* used only for shminfo */
#if defined(_LP64)
	time_t		shm_atime;	/* last shmat time */
	time_t		shm_dtime;	/* last shmdt time */
	time_t		shm_ctime;	/* last change time */
#else	/* _LP64 */
	time_t		shm_atime;	/* last shmat time */
	long		shm_pad1;	/* reserved for time_t expansion */
	time_t		shm_dtime;	/* last shmdt time */
	int		shm_pad2;	/* reserved for time_t expansion */
	time_t		shm_ctime;	/* last change time */
	long		shm_pad3;	/* reserved for time_t expansion */
#endif	/* _LP64 */
	kcondvar_t	shm_cv;
	char		shm_pad4[2];	/* reserved for kcondvar_t expansion */
	struct sptinfo	*shm_sptinfo;	/* info about ISM segment */
	struct seg	*shm_sptseg;	/* pointer to ISM segment */
	long		shm_sptprot;	/* was reserved (still a "long") */
};

#else	/* user definition */

/* this maps to the kernel struct shmid_ds */

struct shmid_ds {
	struct ipc_perm	shm_perm;	/* operation permission struct */
	size_t		shm_segsz;	/* size of segment in bytes */
#if defined(_LP64) || defined(_XOPEN_SOURCE)
	void		*shm_amp;
#else
	struct anon_map	*shm_amp;	/* segment anon_map pointer */
#endif
	ushort_t	shm_lkcnt;	/* number of times it is being locked */
	pid_t		shm_lpid;	/* pid of last shmop */
	pid_t		shm_cpid;	/* pid of creator */
	shmatt_t	shm_nattch;	/* used only for shminfo */
	ulong_t		shm_cnattch;	/* used only for shminfo */
#if defined(_LP64)
	time_t		shm_atime;	/* last shmat time */
	time_t		shm_dtime;	/* last shmdt time */
	time_t		shm_ctime;	/* last change time */
	int64_t		shm_pad4[4];	/* reserve area */
#else	/* _LP64 */
	time_t		shm_atime;	/* last shmat time */
	int32_t		shm_pad1;	/* reserved for time_t expansion */
	time_t		shm_dtime;	/* last shmdt time */
	int32_t		shm_pad2;	/* reserved for time_t expansion */
	time_t		shm_ctime;	/* last change time */
	int32_t		shm_pad3;	/* reserved for time_t expansion */
	int32_t		shm_pad4[4];	/* reserve area  */
#endif	/* _LP64 */
};

#endif	/* _KERNEL */

#if defined(_SYSCALL32)

/*
 * Kernel's view of the user ILP32 shmid_ds structure
 */

struct shmid_ds32 {
	struct ipc_perm32 shm_perm;	/* operation permission struct */
	size32_t	shm_segsz;	/* size of segment in bytes */
	caddr32_t	shm_amp;	/* segment anon_map pointer */
	uint16_t	shm_lkcnt;	/* number of times it is being locked */
	pid32_t		shm_lpid;	/* pid of last shmop */
	pid32_t		shm_cpid;	/* pid of creator */
	uint32_t	shm_nattch;	/* used only for shminfo */
	uint32_t	shm_cnattch;	/* used only for shminfo */
	time32_t	shm_atime;	/* last shmat time */
	int32_t		shm_pad1;	/* reserved for time_t expansion */
	time32_t	shm_dtime;	/* last shmdt time */
	int32_t		shm_pad2;	/* reserved for time_t expansion */
	time32_t	shm_ctime;	/* last change time */
	int32_t		shm_pad3;	/* reserved for time_t expansion */
	int32_t		shm_pad4[4];	/* reserve area  */
};

#endif	/* _SYSCALL32 */

#if defined(_KERNEL)

/*
 * Size invariant version of SVR3 structure. Only kept around
 * to support old binaries. Perhaps this can go away someday.
 */
struct o_shmid_ds32 {
	struct o_ipc_perm32 shm_perm;	/* operation permission struct */
	int32_t		shm_segsz;	/* size of segment in bytes */
	caddr32_t	shm_amp;	/* segment anon_map pointer */
	uint16_t	shm_lkcnt;	/* number of times it is being locked */
	int8_t 		shm_pad[2];
	o_pid_t		shm_lpid;	/* pid of last shmop */
	o_pid_t		shm_cpid;	/* pid of creator */
	uint16_t	shm_nattch;	/* used only for shminfo */
	uint16_t	shm_cnattch;	/* used only for shminfo */
	time32_t	shm_atime;	/* last shmat time */
	time32_t	shm_dtime;	/* last shmdt time */
	time32_t	shm_ctime;	/* last change time */
};

#endif	/* _KERNEL */

#if defined(__EXTENSIONS__) || \
	(!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
struct	shminfo {
	size_t	shmmax,		/* max shared memory segment size */
		shmmin;		/* min shared memory segment size */
	int	shmmni,		/* # of shared memory identifiers */
		shmseg;		/* max attached shared memory	  */
				/* segments per process		  */
};
#endif /* defined(__EXTENSIONS__) || (!defined(_POSIX_C_SOURCE) && ... */

/*
 * Shared memory control operations
 */
#define	SHM_LOCK	3	/* Lock segment in core */
#define	SHM_UNLOCK	4	/* Unlock segment */

#if defined(_KERNEL)
void	shminit(void);
void	shmfork(struct proc *, struct proc *);
void	shmexit(struct proc *);
int	shmgetid(struct proc *, caddr_t);
#else
#if defined(__STDC__)
int shmget(key_t, size_t, int);
int shmctl(int, int, struct shmid_ds *);
void *shmat(int, const void *, int);
#if defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4)
int shmdt(const void *);
#else
int shmdt(char *);
#endif /* defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 >= 4) */
#else /* __STDC __ */
int shmctl();
int shmget();
void *shmat();
int shmdt();
#endif /* __STDC __ */

#endif /* defined(_KERNEL) */

#ifdef _KERNEL

typedef struct sptinfo {
	struct as	*sptas;		/* dummy as ptr. for spt segment */
} sptinfo_t;

/*
 * Protected by p->p_lock
 */
typedef struct segacct {
	struct segacct	*sa_next;
	caddr_t		 sa_addr;
	size_t		 sa_len;
	struct anon_map *sa_amp;
	struct sptinfo	*sa_sptinfo;
	ulong_t		 sa_flags;
	int		 sa_id;
} segacct_t;

/*
 * Structures allocated in machdep.c
 */
extern struct shmid_ds	*shmem;		/* shared memory id pool */
extern struct shminfo	shminfo;	/* configuration parameters */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SHM_H */
