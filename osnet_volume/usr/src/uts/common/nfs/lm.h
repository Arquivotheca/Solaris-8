/*
 * Copyright 1991 NCR Corporation - Dayton, Ohio, USA
 *
 * Copyright (c) 1994-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_NFS_LM_H
#define	_NFS_LM_H

#pragma ident	"@(#)lm.h	1.65	99/09/07 SMI"	/* NCR OS2.00.00 1.2 */

/*
 * This file contains implementation and interface details for the network
 * lock manager.
 */

#include <nfs/lm_nlm.h>
#include <sys/mutex.h>
#include <sys/thread.h>
#include <sys/note.h>		/* needed for warlock */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These two are declared before warlock sees any references to them.
 */
#ifdef _KERNEL
extern kmutex_t		   lm_lck;
extern krwlock_t	   lm_sysids_lock;
#endif

/*
 * LM statistics.  This struct defines counters and miscellaneous stats
 * that are protected by lm_lck.  A couple "length" counters are
 * implemented as integers that are protected by the locks that protect the
 * associated data structures.
 *
 * The RPC counters (tot_in, ... proc_out) are not maintained in a
 * straightforward manner.  For example, the "bad_in" count can be
 * incremented by more than 1 if there are multiple errors associated with
 * the incoming RPC.  Check the actual implementation for details.
 */
struct lm_stat {
	time_t	start_time;	/* Time of last start of LM daemons. */
	int	servers;	/* Number of servers running. */
	int	cookie;		/* Cookie for Granted calls. */
	int	async_len;	/* Length of async cache. */
	int	block_len;	/* Length of block list. */
	int	client_len;	/* Length of client cache. */
	int	sleep_len;	/* Length of sleep list. */
	int	tot_in;		/* Total number of calls. */
	int	tot_out;
	int	bad_in;		/* Total number of calls that fails. */
	int	bad_out;
	int	proc_in[NLM_NUMRPCS];	/* Count of messages. */
	int	proc_out[NLM_NUMRPCS];
#ifdef LM_DEBUG_COTS
	struct knetconfig cots_config;	/* Force COTS client calls. */
#endif
};

#ifndef lint
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_stat::start_time))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_stat::client_len))
#endif

/*
 * Sysid cache.  The strings in the knetconfig are dynamically allocated.
 * - The "config" and "addr" fields contain the information needed to
 *   contact the host.  "addr" is read-only.  "config" is protected by
 *   "lock".
 * - The "name" is the hostname.  It is used for finding lm_sysid records
 *   after a client or server reboots.  It is read-only.
 * - The "sysid" field is a value used to identify the host to the
 *   local locking code.  Each lm_sysid has its own sysid value, but a
 *   given host can have multiple lm_sysid's (even with the same name).
 *   Because the sysid value is recorded by the local locking code, it is
 *   important not to free an lm_sysid if any locks exist for that sysid.
 *   Once the lm_sysid is freed, its sysid value can be reused.  This field
 *   is read-only.
 * - The "next" field is protected by the lock for the lm_sysid list.
 * - The "lock" mutex protects the reference count ("refcnt") and "config".
 *   Each lm_sysid has its own lock to avoid possible deadlock or recursive
 *   mutex panics that might happen if lm_lck were used.
 * - The "sm_client" and "sm_server" fields indicate whether we have
 *   registered interest in "name" with statd.  sm_client is used when we
 *   are a client of "name", and sm_server is used when we are a server for
 *   "name".  These fields are not MT-protected.  The only correctness
 *   requirement is that the field be FALSE when the lock manager needs to
 *   register the hostname with statd.
 * - The "in_recovery" field is used to prevent client processes from
 *   making requests while the lock manager is going through recovery with
 *   a server.  There is a potential race if the client request happens at
 *   about the same time that recovery happens, but this should be
 *   harmless.  This race cannot be prevented by protecting in_recovery
 *   with a mutex, so the field is unprotected.
 *
 * Note that an lm_sysid is not freed when its reference count goes to
 * zero.  It is kept around in case it is needed later.  If memory gets
 * tight, it can be garbage-collected.
 *
 * lm_sysids_lock is a readers/writer lock that protects the integrity
 * of the lm_sysids list itself.  To add an entry to the list, you must
 * acquire the lock as a writer.  To prevent modifications to the list
 * while traversing it, you must acquire it as a reader.
 *
 * To obtain any combination of lm_sysids_lock, lm_lck and lm_sysid->lock
 * (the individual lock), they must be acquired in the order:
 *
 *              lm_sysids_lock > lm_lck > lm_sysid->lock
 *
 * and released in the opposite order to prevent deadlocking.
 */
struct lm_sysid {
	struct lm_sysid *next;
	int		refcnt;
	struct knetconfig  config;
	struct netbuf   addr;
	char		*name;
	sysid_t		sysid;
	bool_t		sm_client;
	bool_t		sm_server;
	bool_t	in_recovery; /* True while reclaiming locks from `name' */
	int		sm_state; /* State of server statd */
	kmutex_t	lock;
};

/*
 * For the benefit of warlock.
 * The `LM ignore' protection scheme declared here simply means warlock
 * does not check these fields for any type of locking protection.
 */
#ifndef lint
_NOTE(RWLOCK_PROTECTS_DATA(lm_sysids_lock, lm_sysid::next))
_NOTE(MUTEX_PROTECTS_DATA(lm_sysid::lock, lm_sysid::refcnt))
_NOTE(MUTEX_PROTECTS_DATA(lm_sysid::lock, lm_sysid::config))
_NOTE(SCHEME_PROTECTS_DATA("LM ignore", lm_sysid::sm_client))
_NOTE(SCHEME_PROTECTS_DATA("LM ignore", lm_sysid::sm_server))
_NOTE(SCHEME_PROTECTS_DATA("LM ignore", lm_sysid::in_recovery))
_NOTE(LOCK_ORDER(lm_sysids_lock lm_lck lm_sysid::lock))
#endif

/*
 * The numeric sysid is used to identify a host and transport.  For systems
 * that support RFS, the lock manager sysids should be allocated from a
 * different range than the RFS sysids.
 *
 * The local locking code uses (pid, sysid) to uniquely identify a process.
 * This means that the client-side code must doctor up the sysid before
 * registering a lock, so that the local locking code doesn't confuse a
 * remote process with a local process just because they have the same pid.
 * We currently do this by ORing LM_SYSID_CLIENT into the sysid before
 * registering a lock.
 *
 * If you change LM_SYSID and LM_SYSID_MAX, be sure to pick values so that
 * LM_SYSID_MAX > LM_SYSID using signed arithmetic, and don't use zero.
 * You may also need a different way to tag lock manager locks that are
 * registered locally.
 */
#define	LM_SYSID	((sysid_t)0x0001)
#define	LM_SYSID_MAX	((sysid_t)0x3FFF)
#define	LM_SYSID_CLIENT	((sysid_t)0x4000)

/*
 * Global declarations.
 */
#define	LM_STATD_DELAY	10	/* Timeout when first contacting local statd. */
#define	LM_NO_TIMOUT	 0	/* Timeout == 0. */
#define	LM_GR_TIMOUT	20	/* Timeout on NLM_GRANTED. Must be big! */
#define	LM_SM_TIMOUT	 5	/* Timeout when talking with SM. */
#define	LM_CR_TIMOUT    20	/* Timeout for SM_SIMU_CRASH. Must be big. */
#define	LM_RLOCK_SLP	20	/* Seconds to sleep before relock_server(). */
#define	LM_ERROR_SLP	30	/* Seconds to sleep after an RPC error. */
#define	LM_GRACE_SLP	10	/* Seconds to sleep if server in grace period */
#define	LM_BLOCK_SLP	60	/* Seconds to sleep if client gets blocked. */
#define	LM_RETRY	 5	/* Retry count in lm_callrpc(). */
/*
 * Retry count for lm_nlm_reclaim()/lm_nlm4_reclaim. This
 * is needed because LM_RETRY was not sufficient enough
 * to have the server setup it's lockd after a crash such that client could
 * obtain the file handle.  The value was made big enough in order
 * to accommodate varying times that it may take different servers to set up
 * its' lockd.
 */
#define	LM_RECLAIM_RETRY	100

/*
 * LM_ASYN_RETRY is not used in the current scheme to deal with
 * retransmission of callbacks to the client after granting a blocked
 * lock.  If we come up with a better scheme we may want to go back
 * to using LM_ASYN_RETRY.
 */
#define	LM_ASYN_RETRY	 3	/* Retry count in lm_asynrpc(). */
#define	LM_PMAP_TIMEOUT	15	/* timeout for talking to portmapper */
#define	LM_GR_RETRY	 1	/* Retry count for GRANTED callback */

/*
 * Owner handle information.  We use the process ID followed by
 * LM_OH_SYS_LEN bytes of system identifier.  See lm_svc() and
 * lm_set_oh().
 */
#define	LM_OH_SYS_LEN	(sizeof (lm_owner_handle_sys))
#define	LM_OH_LEN	(sizeof (pid_t) + LM_OH_SYS_LEN)

/*
 * Map for fp -> knetconfig.  The strings in the knetconfig are statically
 * allocated.
 */
struct lm_config {
	struct lm_config *next;		/* link in null-terminated list */
	struct file *fp;		/* fp originally paired w/this config */
	struct knetconfig config;	/* knetconfig info from userland */
};

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_config))
#endif

/*
 * Map for thread -> xprt.
 */
struct lm_xprt {
	struct lm_xprt *next;
	SVCXPRT *xprt;			/* xprt for this service thread */
	kthread_t *thread;
	int valid;			/* 0 ==> invalid entry, 1 ==> valid */
};

/*
 * For the benefit of warlock.  The "scheme" that protects the other
 * lm_xprt fields is that they are only sampled when the lm_server_status
 * state machine is LM_UP, and only written when being created (holding
 * lm_lck) or when being destroyed (state LM_SHUTTING_DOWN or LM_DOWN,
 * at which time LM is essentially running single-threaded).
 */
#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_xprt::next))
_NOTE(SCHEME_PROTECTS_DATA("LM xprt", lm_xprt::{xprt thread valid}))
#endif

/*
 * lm_sleep
 *
 * List of client processes sleeping on a blocked NLM_LOCK call.  The lock
 * information is used to make sure we can match a GRANTED call with the
 * right process.  The sysid information is to assist with debugging.  An
 * entry in the list is free iff in_use == FALSE.  The condition variable
 * cv is used by the incoming GRANTED thread to wake us.
 */
struct lm_sleep {
	struct lm_sleep *next;
	struct lm_sysid *sysid;
	pid_t pid;
	struct netobj fh;
	struct netobj oh;
	u_offset_t offset;
	len_t length;
	bool_t in_use;
	bool_t waiting;
	kcondvar_t cv;
	vnode_t *vp;
};

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_sleep))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_sleep::vp))
#endif

#ifdef _KERNEL
/*
 * Globals and functions used by the lock manager.  Except for
 * functions exported in modstubs.s, these should all be treated as
 * private to the lock manager.
 */

extern time_t		   time;
extern struct lm_svc_args  lm_sa;
extern struct lm_stat	   lm_stat;
extern struct lm_sysid	  *lm_sysids;
extern kcondvar_t	   lm_status_cv; /* should go in lm_server.h? */
extern bool_t		   lm_caches_created;

/* number of outstanding NLM requests. Protected by lm_lck. */
extern unsigned int lm_num_outstanding;

/* Number of entries in the lm_config table.  Protected by lm_lck. */
extern unsigned int lm_numconfigs;

/* Number of entries in the lm_sysids list.  Protected by lm_sysids_lock. */
extern unsigned int lm_sysid_len;

/* the system portion of the owner handle */
extern int lm_owner_handle_sys;

/* tunable delay for reclaim */
extern int lm_reclaim_factor;

#ifdef DEBUG
extern int		   lm_gc_sysids;
#endif

#define	lm_zalloc(s)	kmem_zalloc(sizeof (s), KM_SLEEP)

struct pmap;
extern bool_t		   xdr_pmap(XDR *, struct pmap *);
extern void		   lm_numcat(char *, int);
extern void		   lm_caches_init(void);
extern void		   lm_server_caches_init(void);
extern char		  *lm_dup(char *, size_t);
extern int		   lm_delay(clock_t);
extern void		   lm_exit(void);
extern struct lm_sysid    *lm_get_me(void);
extern struct lm_sysid	  *lm_get_sysid(struct knetconfig *, struct netbuf *,
					char *name, bool_t alloc,
					bool_t *dropped_read_lock,
					bool_t *netconfig_change);
extern void		   lm_flush_clients(struct lm_sysid *);
extern int		   lm_netobj_eq(struct netobj *, struct netobj *);
extern int		   lm_callrpc(struct lm_sysid *,
				rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t,
				caddr_t, xdrproc_t, caddr_t, int, int, bool_t);
extern int		   lm_asynrpc(struct lm_sysid *,
				rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t,
				caddr_t, int, enum nlm_stats *, int, int);
extern void		   lm_asynrply(int, enum nlm_stats);
extern struct lm_sleep	  *lm_get_sleep(struct lm_sysid *ls, struct netobj *fh,
				struct netobj *oh, u_offset_t offset,
				len_t length, struct vnode *vp);
extern void		   lm_rel_sleep(struct lm_sleep *);
extern int		   lm_has_sleep(const struct vnode *);
extern int		   lm_waitfor_granted(struct lm_sleep *);
extern int		   lm_signal_granted(pid_t pid, struct netobj *fh,
				struct netobj *oh, u_offset_t offset,
				len_t length);
extern void		   lm_sm_client(struct lm_sysid *ls,
				struct lm_sysid *me);
extern struct lm_config	  *lm_saveconfig(struct file *, struct knetconfig *);
extern struct lm_config	  *lm_getconfig(struct file *);
extern void		   lm_rmconfig(struct file *);
extern int		   lm_frlock(struct vnode *vp, int cmd,
				struct flock64 *flk, int flag,
				u_offset_t offset, struct cred *cr,
				netobj *fh);
extern int		   lm4_frlock(struct vnode *vp, int cmd,
				struct flock64 *flk, int flag,
				u_offset_t offset, struct cred *cr,
				netobj *fh);
extern int		   lm_shrlock(struct vnode *vp, int cmd,
				struct shrlock *shr, int flag, netobj *fh);
extern int		   lm4_shrlock(struct vnode *vp, int cmd,
				struct shrlock *shr, int flag, netobj *fh);
extern struct lm_xprt	  *lm_savexprt(SVCXPRT *xprt);
extern struct lm_xprt	  *lm_getxprt(void);
extern void		   lm_relxprt(SVCXPRT *xprt);
extern void		   lm_flush_clients_mem(void *);
extern void		   lm_free_sysid_table(void *);
extern void		   lm_ref_sysid(struct lm_sysid *);
extern void		   lm_rel_sysid(struct lm_sysid *);
extern int		   lm_sigispending(void);
extern void		   lm_register_lock_locally(vnode_t *,
				struct lm_sysid *, struct flock64 *, int,
				u_offset_t);
extern void		   lm_send_siglost(struct flock64 *, struct lm_sysid *);
extern void		   lm_cprsuspend(void);
extern void		   lm_cprresume(void);
extern void		   lm_set_oh(char *, size_t, pid_t);
extern int		   lm_safelock(vnode_t *, const struct flock64 *,
				cred_t *);
extern int		   lm_safemap(const vnode_t *);

#else /* _KERNEL */

#ifdef __STDC__
extern int lm_shutdown(void);
#else
extern int lm_shutdown();
#endif /* __STDC__ */

#endif /* _KERNEL */

/*
 * Definitions for handling LM debug.
 */
#define	LM_DEBUG_SUPPORT		/* enable runtime tracing */

#ifdef  LM_DEBUG_SUPPORT

/*PRINTFLIKE3*/
extern void lm_debug(int level, char *function, const char *fmt, ...);

#define	LM_DEBUG(a)			lm_debug a

extern void lm_alock(int, char *, nlm_lock *);
extern void lm_d_nsa(int, char *, nlm_shareargs *);
extern void lm_n_buf(int, char *, char *, struct netbuf *);
extern void lm_alock4(int, char *, nlm4_lock *);
extern void lm_d_nsa4(int, char *, nlm4_shareargs *);

#else /* LM_DEBUG_SUPPORT */

#define	LM_DEBUG(a)

#define	lm_alock(a, b, c)
#define	lm_d_nsa(a, b, c)
#define	lm_n_buf(a, b, c, d)

#endif /* LM_DEBUG_SUPPORT */

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_LM_H */
