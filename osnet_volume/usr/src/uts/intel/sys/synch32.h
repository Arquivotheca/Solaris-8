/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SYNC32_H
#define	_SYS_SYNC32_H

#pragma ident	"@(#)synch32.h	1.17	99/10/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* special defines for LWP mutexes */
#define	mutex_flag	flags.flag1
#define	mutex_ceiling	flags.ceiling
#define	mutex_type	flags.mbcp_type_un.mtype_rcount.count_type1
#define	mutex_rcount	flags.mbcp_type_un.mtype_rcount.count_type2
#define	mutex_magic	flags.magic
#define	mutex_owner	data
/* used to atomically operate on whole word via cas or swap instruction */
#define	mutex_lockword	lock.lock64.pad[4]  /* address of word containing lk */
#define	mutex_lockw	lock.lock64.pad[7]
#define	mutex_waiters	lock.lock64.pad[6]

/* robust lock owner pid */
#define	mutex_ownerpid	lock.lock64.pad[0]

/* Max. recusrion count for recursive mutexes */
#define	RECURSION_MAX	255

/* special defines for LWP condition variables */
#define	cond_type	flags.type
#define	cond_magic	flags.magic
#define	cond_waiters	flags.flag[3]

/* special defines for LWP semaphores */
#define	sema_count	count
#define	sema_type	type
#define	sema_waiters	flags[7]

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SYNC32_H */
