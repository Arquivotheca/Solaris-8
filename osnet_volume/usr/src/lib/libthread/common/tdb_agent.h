/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_TDB_AGENT_H
#define	_TDB_AGENT_H

#pragma ident	"@(#)tdb_agent.h	1.13	99/08/10 SMI"

/*
 * Thread debug agent control structures.
 */

#include <thread_db.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status of libthread_db attachment to this process.
 */
typedef enum {
	TDB_NOT_ATTACHED,	/* must be default: no libthread_db */
	TDB_START_AGENT,	/* tdb attaching:  please start agent */
	TDB_ATTACHED		/* tdb attached; agent started */
} tdb_agt_stat_t;

/*
 * This enumerates the request codes for requests that libthread_db
 * makes to the thread_db agent in the target process.
 */
typedef enum {
	NONE_PENDING,		/* Placeholder */
	THREAD_SUSPEND,		/* td_thr_dbsuspend */
	THREAD_RESUME		/* td_thr_dbcontinue */
} tdb_opcode_t;

/*
 * This structure contains target process data pertaining to the
 * target process's tdb agent thread.  The agent thread is ready to
 * accept requests when the agent_ready field is non-zero.  After
 * that, libthread_db need not read this structure again.
 */
typedef struct {
	int agent_ready;
	lwpid_t agent_lwpid;
	caddr_t agent_go_addr;
	caddr_t agent_stop_addr;
} tdb_agent_data_t;

#ifdef _SYSCALL32_IMPL
typedef struct {
	int agent_ready;
	lwpid_t agent_lwpid;
	caddr32_t agent_go_addr;
	caddr32_t agent_stop_addr;
} tdb_agent_data32_t;
#endif /* _SYSCALL32_IMPL */

typedef struct {
	tdb_opcode_t opcode;
	int result;
	union {
		uthread_t *thr_p;
		int conc_lvl;
	} u;
} tdb_ctl_t;

#ifdef _SYSCALL32_IMPL
typedef struct {
	tdb_opcode_t opcode;
	int result;
	union {
		caddr32_t thr_p;
		int conc_lvl;
	} u;
} tdb_ctl32_t;
#endif /* _SYSCALL32_IMPL */

#define	TDB_SYNC_DESC_HASHSIZE 256

/*
 * An entry in the sync. object registry.
 */
typedef struct _sync_desc {
	struct _sync_desc *next;
	struct _sync_desc *prev;
	int sync_magic;
	caddr_t sync_addr;
} tdb_sync_desc_t;

#ifdef _SYSCALL32_IMPL
typedef struct _sync_desc32 {
	caddr32_t next;
	caddr32_t prev;
	int sync_magic;
	caddr32_t sync_addr;
} tdb_sync_desc32_t;
#endif /* _SYSCALL32_IMPL */

/*
 * This structure contains target process data that is valid as soon
 * as the startup rtld work is done (e.g., function addresses); the
 * controlling process reads it from the target process once, at attach
 * time.
 */
typedef struct {
	caddr_t tdb_stats_addr;
	caddr_t tdb_stats_enable_addr;
	caddr_t tdb_eventmask_addr;
	caddr_t sync_desc_hash_addr;
	caddr_t nthreads_addr;
	caddr_t nlwps_addr;
	caddr_t tdb_nlwps_req_addr;
	caddr_t tdb_agent_stat_addr;
	caddr_t allthreads_addr;
	caddr_t aslwp_id_addr;
	caddr_t tsd_common_addr;
	caddr_t tdb_agent_data_addr;
	caddr_t tdb_agent_ctl_addr;
	caddr_t tdb_t0_addr;
	void (*tdb_events[TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1])();
} tdb_invar_data_t;

#ifdef _SYSCALL32_IMPL
typedef struct {
	caddr32_t tdb_stats_addr;
	caddr32_t tdb_stats_enable_addr;
	caddr32_t tdb_eventmask_addr;
	caddr32_t sync_desc_hash_addr;
	caddr32_t nthreads_addr;
	caddr32_t nlwps_addr;
	caddr32_t tdb_nlwps_req_addr;
	caddr32_t tdb_agent_stat_addr;
	caddr32_t allthreads_addr;
	caddr32_t aslwp_id_addr;
	caddr32_t tsd_common_addr;
	caddr32_t tdb_agent_data_addr;
	caddr32_t tdb_agent_ctl_addr;
	caddr32_t tdb_t0_addr;
	caddr32_t tdb_events[TD_MAX_EVENT_NUM - TD_MIN_EVENT_NUM + 1];
} tdb_invar_data32_t;
#endif /* _SYSCALL32_IMPL */

extern td_thr_events_t __tdb_event_global_mask;
extern int __tdb_stats_enabled;
extern int __tdb_nlwps_req;
extern tdb_agt_stat_t __tdb_attach_stat;	/* is libthread_db attached? */

#define	__td_event_report(t, event)					\
	((t)->t_td_events_enable &&					\
	(td_eventismember(&(t)->t_td_evbuf.eventmask, (event)) ||	\
	td_eventismember(&__tdb_event_global_mask, (event))))

extern void _tdb_sync_obj_register(caddr_t obj, int type);
extern void _tdb_sync_obj_deregister(caddr_t obj);

#ifdef __cplusplus
}
#endif

#endif	/* _TDB_AGENT_H */
