/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LWP_DB_H
#define	_LWP_DB_H

#pragma ident	"@(#)lwp_db.h	1.1	99/10/14 SMI"

/*
 * Extensions to <thread_db.h>
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Statistics structures for the various synchronization objects, contained
 * within the td_syncstats structure returned by td_sync_get_stats().
 */
typedef struct {
	uint_t		mutex_lock;
	uint_t		mutex_sleep;
	hrtime_t	mutex_sleep_time;
	hrtime_t	mutex_hold_time;
	uint_t		mutex_try;
	uint_t		mutex_try_fail;
	uint_t		mutex_internal;		/* internal to libthread */
} td_mutex_stats_t;

typedef struct {
	uint_t		cond_wait;
	uint_t		cond_timedwait;
	hrtime_t	cond_wait_sleep_time;
	hrtime_t	cond_timedwait_sleep_time;
	uint_t		cond_timedwait_timeout;
	uint_t		cond_signal;
	uint_t		cond_broadcast;
	uint_t		cond_internal;		/* internal to libthread */
} td_cond_stats_t;

typedef struct {
	uint_t		rw_rdlock;
	uint_t		rw_rdlock_sleep;
	hrtime_t	rw_rdlock_sleep_time;
	uint_t		rw_rdlock_try;
	uint_t		rw_rdlock_try_fail;
	uint_t		rw_wrlock;
	uint_t		rw_wrlock_sleep;
	hrtime_t	rw_wrlock_sleep_time;
	hrtime_t	rw_wrlock_hold_time;
	uint_t		rw_wrlock_try;
	uint_t		rw_wrlock_try_fail;
} td_rwlock_stats_t;

typedef struct {
	uint_t		sema_wait;
	uint_t		sema_wait_sleep;
	hrtime_t	sema_wait_sleep_time;
	uint_t		sema_trywait;
	uint_t		sema_trywait_fail;
	uint_t		sema_post;
	uint_t		sema_max_count;
	uint_t		sema_min_count;
} td_sema_stats_t;

/*
 * Synchronization object statistics structure filled in by td_sync_get_stats()
 */
typedef struct td_syncstats {
	td_syncinfo_t	ss_info;	/* as returned by td_sync_getinfo */
	union {
		td_mutex_stats_t	mutex;
		td_cond_stats_t		cond;
		td_rwlock_stats_t	rwlock;
		td_sema_stats_t		sema;
		uint_t			pad[32];	/* for future growth */
	} ss_un;
} td_syncstats_t;

/*
 * Enable/disable a process's synchronization object tracking.
 */
td_err_e
td_ta_sync_tracking_enable(const td_thragent_t *, int);

/*
 * Get statistics for a synchronization object.
 */
td_err_e
td_sync_get_stats(const td_synchandle_t *, td_syncstats_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _LWP_DB_H */
