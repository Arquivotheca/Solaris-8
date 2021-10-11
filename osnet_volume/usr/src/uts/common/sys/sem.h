/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SEM_H
#define	_SYS_SEM_H

#pragma ident	"@(#)sem.h	1.27	99/04/14 SMI"	/* SVr4.0 11.14 */

#include <sys/ipc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * IPC Semaphore Facility.
 */

/*
 * Implementation Constants.
 */

/*
 * Permission Definitions.
 */

#define	SEM_A	0200	/* alter permission */
#define	SEM_R	0400	/* read permission */

/*
 * Semaphore Operation Flags.
 */

#define	SEM_UNDO	010000	/* set up adjust on exit entry */

/*
 * Semctl Command Definitions.
 */

#define	GETNCNT	3	/* get semncnt */
#define	GETPID	4	/* get sempid */
#define	GETVAL	5	/* get semval */
#define	GETALL	6	/* get all semval's */
#define	GETZCNT	7	/* get semzcnt */
#define	SETVAL	8	/* set semval */
#define	SETALL	9	/* set all semval's */

/*
 * Structure Definitions.
 */

/*
 * There is one semaphore id data structure (semid_ds) for each set of
 * semaphores in the system.
 */
struct semid_ds {
	struct ipc_perm sem_perm;	/* operation permission struct */
	struct sem	*sem_base;	/* ptr to first semaphore in set */
	ushort_t	sem_nsems;	/* # of semaphores in set */
#if defined(_LP64)
	time_t		sem_otime;	/* last semop time */
	time_t		sem_ctime;	/* last change time */
#else	/* _LP64 */
	time_t		sem_otime;	/* last semop time */
	int32_t		sem_pad1;	/* reserved for time_t expansion */
	time_t		sem_ctime;	/* last change time */
	int32_t		sem_pad2;	/* time_t expansion */
#endif	/* _LP64 */
	int		sem_binary;	/* flag indicating semaphore type */
	long		sem_pad3[3];	/* reserve area */
};

#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/t_lock.h>
#include <sys/vmem.h>

/*
 * There is one semaphore structure (sem) for each semaphore in the system.
 */

struct sem {
	ushort_t semval;		/* semaphore value */
	pid_t	sempid;		/* pid of last operation */
	ushort_t semncnt;	/* # awaiting semval > cval */
	ushort_t semzcnt;	/* # awaiting semval = 0 */
	kcondvar_t semncnt_cv;
	kcondvar_t semzcnt_cv;
};

#else		/* user level definition */

struct sem {
	ushort_t	semval;		/* semaphore value */
	pid_t		sempid;		/* pid of last operation */
	ushort_t	semncnt;	/* # awaiting semval > cval */
	ushort_t	semzcnt;	/* # awaiting semval = 0 */
	ushort_t	semncnt_cv;
	ushort_t	semzcnt_cv;
};

#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#if defined(_SYSCALL32)

/* Kernel's view of the user ILP32 semid_ds structure */

struct semid_ds32 {
	struct ipc_perm32 sem_perm;	/* operation permission struct */
	caddr32_t	sem_base;	/* ptr to first semaphore in set */
	uint16_t	sem_nsems;	/* # of semaphores in set */
	time32_t	sem_otime;	/* last semop time */
	int32_t		sem_pad1;	/* reserved for time_t expansion */
	time32_t	sem_ctime;	/* last semop time */
	int32_t		sem_pad2;	/* reserved for time_t expansion */
	int32_t		sem_binary;	/* flag indicating semaphore type */
	int32_t		sem_pad3[3];	/* reserve area */
};

#endif	/* _SYSCALL32 */

#if defined(_KERNEL)

/*
 * Size invariant version of SVR3 structure. Only kept around
 * to support old binaries. Perhaps this can go away someday.
 */
struct o_semid_ds32 {
	struct o_ipc_perm32 sem_perm;	/* operation permission struct */
	caddr32_t	sem_base;	/* ptr to first semaphore in set */
	uint16_t	sem_nsems;	/* # of semaphores in set */
	time32_t	sem_otime;	/* last semop time */
	time32_t	sem_ctime;	/* last change time */
};

#endif /* _KERNEL */

/*
 * There is one undo structure per process in the system.
 */

#if defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE)
struct sem_undo {
	struct sem_undo	*un_np;	/* ptr to next active undo structure */
	short		un_cnt;	/* # of active entries */
	struct undo {
		short	un_aoe;		/* adjust on exit values */
		ushort_t un_num;		/* semaphore # */
		int	un_id;		/* semid */
	} un_ent[1];			/* undo entries (one minimum) */
};
#endif /* defined(__EXTENSIONS__) || !defined(_XOPEN_SOURCE) */

/*
 * Semaphore information structure
 */
struct	seminfo	{
	int	semmni;		/* # of semaphore identifiers */
	int	semmns;		/* # of semaphores in system */
	int	semmnu;		/* # of undo structures in system */
	int	semmsl;		/* max # of semaphores per id */
	int	semopm;		/* max # of operations per semop call */
	int	semume;		/* max # of undo entries per process */
	int	semusz;		/* size in bytes of undo structure */
	int	semvmx;		/* semaphore maximum value */
	int	semaem;		/* adjust on exit max value */
};

/*
 * User semaphore template for semop system calls.
 */
struct sembuf {
	ushort_t	sem_num;	/* semaphore # */
	short		sem_op;		/* semaphore operation */
	short		sem_flg;	/* operation flags */
};

#if !defined(_KERNEL)
#if defined(__STDC__)
int semctl(int, int, int, ...);
int semget(key_t, int, int);
int semop(int, struct sembuf *, size_t);
#else /* __STDC __ */
int semctl();
int semget();
int semop();
#endif /* __STDC __ */
#endif /* ! _KERNEL */

#ifdef _KERNEL

/*
 * Defined in space.c, allocated/initialized in sem.c
 */
extern struct semid_ds	*sema;		/* semaphore id pool */
extern struct sem	*sem;		/* semaphore pool */
extern struct sem_undo	**sem_undo;	/* per process undo table */
extern struct sem_undo	*semunp;	/* ptr to head of undo chain */
extern struct sem_undo	*semfup;	/* ptr to head of free undo chain */
extern int		*semu;		/* undo structure pool */
extern struct seminfo	seminfo;	/* semaphore parameters */

extern void semexit(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SEM_H */
