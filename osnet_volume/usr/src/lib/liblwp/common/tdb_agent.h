/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tdb_agent.h	1.1	99/10/14 SMI"

/*
 * Thread debug agent control structures.
 *
 * This is an implementation-specific header file that is shared
 * between libthread and libthread_db.  It is NOT a public header
 * file and must never be installed in /usr/include
 */

#include <thread_db.h>

/*
 * The structure containing per-thread event data.
 */
typedef struct {
	td_thr_events_t	eventmask;	/* Which events are enabled? */
	td_event_e	eventnum;	/* Most recent enabled event */
	void		*eventdata;	/* Param. for most recent event */
} td_evbuf_t;

#ifdef _SYSCALL32
typedef struct {
	td_thr_events_t	eventmask;	/* Which events are enabled? */
	td_event_e	eventnum;	/* Most recent enabled event */
	caddr32_t	eventdata;	/* Param. for most recent event */
} td_evbuf32_t;
#endif /* _SYSCALL32 */


/*
 * All of these structures are constrained to have a size of 48 bytes.
 * This is so that two 8-byte pointers can be inserted at the front to
 * make up a complete tdb_sync_stats_t structure of exactly 64 bytes.
 * The 'type' element of each structure identifies the type of the union,
 * with values from the following defines.  A non-zero 'offset' identifies
 * the condvar or mutex as being contained in an rwlock_t and is the
 * byte offset back to the origin of the rwlock.
 */

#define	TDB_NONE	0
#define	TDB_MUTEX	1
#define	TDB_COND	2
#define	TDB_RWLOCK	3
#define	TDB_SEMA	4

typedef struct {
	uint16_t	type;
	uint16_t	offset;
	uint_t		mutex_lock;
	hrtime_t	mutex_hold_time;
	hrtime_t	mutex_sleep_time;
	uint_t		mutex_sleep;
	uint_t		mutex_try;
	uint_t		mutex_try_fail;
	uint_t		mutex_pad[1];
	hrtime_t	mutex_begin_hold;
} tdb_mutex_stats_t;

typedef struct {
	uint16_t	type;
	uint16_t	offset;
	uint_t		cond_wait;
	uint_t		cond_timedwait;
	uint_t		cond_timedwait_timeout;
	hrtime_t	cond_wait_sleep_time;
	hrtime_t	cond_timedwait_sleep_time;
	uint_t		cond_signal;
	uint_t		cond_broadcast;
	uint_t		cond_pad[2];
} tdb_cond_stats_t;

typedef struct {
	uint16_t	type;
	uint16_t	offset;
	uint_t		rw_rdlock;
	/* rw_rdlock_sleep is the reader cv's cond_wait count */
	/* rw_rdlock_sleep_time is the reader cv's cond_wait_sleep_time */
	uint_t		rw_rdlock_try;
	uint_t		rw_rdlock_try_fail;
	uint_t		rw_pad[1];
	uint_t		rw_wrlock;
	/* rw_wrlock_sleep is the writer cv's cond_wait count */
	/* rw_wrlock_sleep_time is the writer cv's cond_wait_sleep_time */
	hrtime_t	rw_wrlock_hold_time;
	uint_t		rw_wrlock_try;
	uint_t		rw_wrlock_try_fail;
	hrtime_t	rw_wrlock_begin_hold;
} tdb_rwlock_stats_t;

typedef struct {
	uint16_t	type;
	uint16_t	offset;
	uint_t		sema_post;
	uint_t		sema_wait;
	uint_t		sema_wait_sleep;
	hrtime_t	sema_wait_sleep_time;
	uint_t		sema_trywait;
	uint_t		sema_trywait_fail;
	uint_t		sema_max_count;
	uint_t		sema_min_count;
	uint_t		sema_pad[2];
} tdb_sema_stats_t;

/*
 * An entry in the sync. object hash table.
 */
typedef struct {
	uint64_t	next;
	uint64_t	sync_addr;
	union {
		uint16_t		type;
		tdb_mutex_stats_t	mutex;
		tdb_cond_stats_t	cond;
		tdb_rwlock_stats_t	rwlock;
		tdb_sema_stats_t	sema;
	} un;
} tdb_sync_stats_t;

/* peg count values at UINT_MAX */
#define	tdb_incr(x)	(((x) != UINT_MAX)? (x)++ : 0)

/*
 * The tdb_register_sync variable is set to REGISTER_SYNC_ENABLE by a
 * debugger to enable synchronization object registration.
 * Thereafter, synchronization primitives call tdb_sync_obj_register()
 * to put their synchronization objects in the registration hash table.
 * In this state, the first call to tdb_sync_obj_register() empties the
 * hash table and sets tdb_register_sync to REGISTER_SYNC_ON.
 *
 * The tdb_register_sync variable is set to REGISTER_SYNC_DISABLE by a
 * debugger to disable synchronization object registration.
 * In this state, the first call to tdb_sync_obj_register() empties the
 * hash table and sets tdb_register_sync to REGISTER_SYNC_OFF.
 * Thereafter, synchronization primitives do not call tdb_sync_obj_register().
 *
 * Sync object *_destroy() functions always call tdb_sync_obj_deregister().
 */
typedef enum {
	REGISTER_SYNC_OFF = 0,	/* registration is off */
	REGISTER_SYNC_ON,	/* registration is on */
	REGISTER_SYNC_DISABLE,	/* debugger request to disable registration */
	REGISTER_SYNC_ENABLE	/* debugger request to enable registration */
} register_sync_t;

extern	register_sync_t		tdb_register_sync;
extern	tdb_sync_stats_t	*tdb_sync_obj_register(void *);
extern	void			tdb_sync_obj_deregister(void *);

/*
 * Definitions for acquiring pointers to synch object statistics blocks
 * contained in the synchronization object registration hash table.
 */
extern	tdb_mutex_stats_t	*tdb_mutex_stats(mutex_t *);
extern	tdb_cond_stats_t	*tdb_cond_stats(cond_t *);
extern	tdb_rwlock_stats_t	*tdb_rwlock_stats(rwlock_t *);
extern	tdb_sema_stats_t	*tdb_sema_stats(sema_t *);

#define	MUTEX_STATS(mp)		(tdb_register_sync? tdb_mutex_stats(mp) : \
				((mp)->mutex_magic = MUTEX_MAGIC, \
				(tdb_mutex_stats_t *)NULL))
#define	COND_STATS(cvp)		(tdb_register_sync? tdb_cond_stats(cvp) : \
				((cvp)->cond_magic = COND_MAGIC, \
				(tdb_cond_stats_t *)NULL))
#define	RWLOCK_STATS(rwlp)	(tdb_register_sync? tdb_rwlock_stats(rwlp) : \
				((rwlp)->magic = RWL_MAGIC, \
				(tdb_rwlock_stats_t *)NULL))
#define	SEMA_STATS(sp)		(tdb_register_sync? tdb_sema_stats(sp) : \
				((sp)->magic = SEMA_MAGIC, \
				(tdb_sema_stats_t *)NULL))

/*
 * Parameters of the synchronization object registration hash table.
 */
#define	TDB_HASH_SHIFT	15	/* 32K hash table entries */
#define	TDB_HASH_SIZE	(1 << TDB_HASH_SHIFT)
#define	TDB_HASH_MASK	(TDB_HASH_SIZE - 1)

/*
 * lwp_invar_data contains target process data that is valid as soon
 * as the startup rtld work is done (e.g., function addresses); the
 * controlling process, via libthread_db, reads it from the target
 * process once, at attach time.
 *
 * libthread defines lwp_invar_data to be of type lwp_initial_data_t
 * but libthread_db expects to get a structure of type lwp_invar_data_t
 * These two are identical binary structures for sparc and x86.
 *
 * For ia64, they are different in the declaration of the tdb_events[]
 * array.  ia64 requires the address of a function to be, in reality,
 * a pointer to a function descriptor.  libthread_db has to deal with
 * this stupidity.  After it reads the structure, it reads each function
 * descriptor and extracts the actual function address and stores it in
 * place of the function descriptor address.
 */

typedef	void (*pFrv)(void);
typedef struct {
	psaddr_t	tdb_stats_addr;
	psaddr_t	tdb_stats_enable_addr;
	psaddr_t	tdb_eventmask_addr;
	psaddr_t	sync_addr_hash_addr;
	psaddr_t	tdb_register_sync_addr;
	psaddr_t	nthreads_addr;
	psaddr_t	all_lwps_addr;
	psaddr_t	ulwp_one_addr;
	psaddr_t	tsd_common_addr;
	psaddr_t	hash_table_addr;
	pFrv		tdb_events[TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1];
} lwp_initial_data_t;

typedef struct {
	psaddr_t	tdb_stats_addr;
	psaddr_t	tdb_stats_enable_addr;
	psaddr_t	tdb_eventmask_addr;
	psaddr_t	sync_addr_hash_addr;
	psaddr_t	tdb_register_sync_addr;
	psaddr_t	nthreads_addr;
	psaddr_t	all_lwps_addr;
	psaddr_t	ulwp_one_addr;
	psaddr_t	tsd_common_addr;
	psaddr_t	hash_table_addr;
	psaddr_t	tdb_events[TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1];
} lwp_invar_data_t;

#ifdef _SYSCALL32
typedef struct {
	caddr32_t	tdb_stats_addr;
	caddr32_t	tdb_stats_enable_addr;
	caddr32_t	tdb_eventmask_addr;
	caddr32_t	sync_addr_hash_addr;
	caddr32_t	tdb_register_sync_addr;
	caddr32_t	nthreads_addr;
	caddr32_t	all_lwps_addr;
	caddr32_t	ulwp_one_addr;
	caddr32_t	tsd_common_addr;
	caddr32_t	hash_table_addr;
	caddr32_t	tdb_events[TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1];
} lwp_invar_data32_t;
#endif	/* _SYSCALL32 */

extern td_thr_events_t tdb_ev_global_mask;
extern int tdb_stats_enabled;

#define	__td_event_report(ulwp, event)					\
	((ulwp)->ul_td_events_enable &&					\
	(td_eventismember(&(ulwp)->ul_td_evbuf.eventmask, (event)) ||	\
	td_eventismember(&tdb_ev_global_mask, (event))))

/*
 * Event "reporting" functions.  A thread reports an event by calling
 * one of these empty functions; a debugger can set a breakpoint
 * at the address of any of these functions to determine that an
 * event is being reported.
 */
extern	void	tdb_event_ready(void);
extern	void	tdb_event_sleep(void);
extern	void	tdb_event_switchto(void);
extern	void	tdb_event_switchfrom(void);
extern	void	tdb_event_lock_try(void);
extern	void	tdb_event_catchsig(void);
extern	void	tdb_event_idle(void);
extern	void	tdb_event_create(void);
extern	void	tdb_event_death(void);
extern	void	tdb_event_preempt(void);
extern	void	tdb_event_pri_inherit(void);
extern	void	tdb_event_reap(void);
extern	void	tdb_event_concurrency(void);
extern	void	tdb_event_timeout(void);
