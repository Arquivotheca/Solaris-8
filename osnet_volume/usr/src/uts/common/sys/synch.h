/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYNCH_H
#define	_SYS_SYNCH_H

#pragma ident	"@(#)synch.h	1.37	99/11/16 SMI"

#ifndef _ASM
#include <sys/types.h>
#include <sys/int_types.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/*
 * Thread and LWP mutexes have the same type
 * definitions.
 *
 * NOTE:
 *
 * POSIX requires that <pthread.h> define the structures pthread_mutex_t
 * and pthread_cond_t.  Although these structures are identical to mutex_t
 * (lwp_mutex_t) and cond_t (lwp_cond_t), defined here, a typedef of these
 * types would require including <synch.h> in <pthread.h>, pulling in
 * non-posix symbols/constants, violating POSIX namespace restrictions.  Hence,
 * pthread_mutex_t/pthread_cond_t have been redefined (in <sys/types.h>).
 * Any modifications done to mutex_t/lwp_mutex_t or cond_t/lwp_cond_t must
 * also be done to pthread_mutex_t/pthread_cond_t.
 */
typedef struct _lwp_mutex {
	struct _mutex_flags {
		uint16_t	flag1;
		uint8_t		flag2;
		uint8_t		ceiling;
		union _mbcp_type_un {
			uint16_t bcptype;
			struct _mtype_rcount {
				uint8_t		count_type1;
				uint8_t		count_type2;
			} mtype_rcount;
		} mbcp_type_un;
		uint16_t	magic;
	} flags;
	union _mutex_lock_un {
		struct _mutex_lock {
			uint8_t	pad[8];
		} lock64;
		upad64_t owner64;
	} lock;
	upad64_t data;
} lwp_mutex_t;

/*
 * Thread and LWP condition variables have the same
 * type definition.
 * NOTE:
 * The layout of the following structure should be kept in sync with the
 * layout of pthread_cond_t in sys/types.h. See NOTE above for lwp_mutex_t.
 */
typedef struct _lwp_cond {
	struct _lwp_cond_flags {
		uint8_t		flag[4];
		uint16_t 	type;
		uint16_t 	magic;
	} flags;
	upad64_t data;
} lwp_cond_t;


/*
 * LWP semaphores
 */

typedef struct _lwp_sema {
	uint32_t	count;		/* semaphore count */
	uint16_t 	type;
	uint16_t 	magic;
	uint8_t		flags[8];	/* last byte reserved for waiters */
	upad64_t	data;		/* optional data */
} lwp_sema_t;

#endif /* _ASM */
/*
 * Definitions of synchronization types.
 */
#define	USYNC_THREAD	0x00		/* private to a process */
#define	USYNC_PROCESS	0x01		/* shared by processes */

/* Keep the following 3 fields in sync with pthread.h */
#define	LOCK_NORMAL	0x00		/* same as USYNC_THREAD */
#define	LOCK_ERRORCHECK	0x02		/* error check lock */
#define	LOCK_RECURSIVE	0x04		/* recursive lock */

#define	USYNC_PROCESS_ROBUST	0x08	/* shared by processes robustly */

/* Keep the following 5 fields in sync with pthread.h */

#define	LOCK_PRIO_NONE		0x00
#define	LOCK_PRIO_INHERIT	0x10
#define	LOCK_PRIO_PROTECT	0x20
#define	LOCK_STALL_NP		0x00
#define	LOCK_ROBUST_NP		0x40

/*
 * lwp_mutex_t flags
 */
#define	LOCK_OWNERDEAD		0x1
#define	LOCK_NOTRECOVERABLE	0x2
#define	LOCK_INITED		0x4
#define	LOCK_UNMAPPED		0x8

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SYNCH_H */
