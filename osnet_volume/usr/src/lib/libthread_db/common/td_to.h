/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _TD_TO_H
#define	_TD_TO_H

#pragma ident	"@(#)td_to.h	1.50	99/08/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
* MODULE_td_to.h____________________________________________________
*  Description:
____________________________________________________________________ */

#define	TD_PARTIAL 0
#define	TD_FULL 1
#define	TD_EVENT_ON 1
#define	TD_EVENT_OFF 0

#define	TD_HASH_TAB_SIZE ALLTHR_TBLSIZ
#define	TD_HASH_TID(x) HASH_TID(x)

/*
*  Thread struct access macros.  Macros containing $$$$$$$$ access
* are place holders for information not yet available from libthread.
*/

#define	TD_CONVERT_TYPE(uthread_t) 			\
	((uthread_t).t_flag&T_INTERNAL ? TD_THR_SYSTEM : TD_THR_USER)

/*
*   Threads hash table
*/

#define	td_hash_first_(x) ((x).first)

#define	DB_NOT_SUSPENDED 0
#define	DB_SUSPENDED  1

/*
*   Thread information access macros.
* The td_toc_* and td_tov_* are relics of the earlier definition of
* the constant and variable parts of the thread information.
*/

/*
*   Macros for testing state of thread.
*/
#define	ISONPROC(x)	((*(x)).t_state == TS_ONPROC)
#define	ISZOMBIE(x)	((*(x)).t_state == TS_ZOMB)
#define	TO_ISONPROC(x) ((x).ti_state == TD_THR_ACTIVE)
#define	TO_ISBOUND(x)  ((x).ti_user_flags & THR_BOUND)
#define	TO_ISPARKED(x) ((x).ti_user_flags & T_PARKED)

/*
 * Is this thread currently associated with an lwp?
 * ISONPROC says that the thread is running on an lwp and we
 * can therefore set/get the registers from the lwp. ISBOUND
 * says that the thread is bound to an lwp.  Even if it is
 * not running, it still has an lwp so set/get of registers
 * to lwp are allowed. ISPARKED says the lwp is parked on a
 * semaphore waiting for a runnable thread.
 */
#define	ISVALIDLWP(t)	(ISONPROC(t) || ISPARKED(t) || \
	(!ISZOMBIE(t) && (ISBOUND(t) || ISTEMPBOUND(t))))

/*
 * FIXME - when thread struct has a bit to show when FP registers
 * in an LWP are valid, use it here.
 */
#define	HASVALIDFP(x)	(1)

#ifdef	__cplusplus
}
#endif

#endif /* _TD_TO_H */
