/*
 * Copyright (c) 1986-1991,1998,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mt_misc.c	1.29	99/07/20 SMI"

/*
 *	Define and initialize MT data for libnsl.
 *	The _libnsl_lock_init() function below is the library's .init handler.
 */

#include	"rpc_mt.h"
#include	<rpc/rpc.h>
#include	<sys/time.h>
#include	<stdlib.h>
#include	<syslog.h>


rwlock_t	svc_lock;	/* protects the services list (svc.c) */
rwlock_t	svc_fd_lock;	/* protects svc_fdset and the xports[] array */
rwlock_t	rpcbaddr_cache_lock; /* protects the RPCBIND address cache */
static rwlock_t	*rwlock_table[] = {
	&svc_lock,
	&svc_fd_lock,
	&rpcbaddr_cache_lock
};

mutex_t	authdes_lock;		/* protects authdes cache (svcauth_des.c) */
mutex_t	authnone_lock;		/* auth_none.c serialization */
mutex_t	authsvc_lock;		/* protects the Auths list (svc_auth.c) */
mutex_t	clntraw_lock;		/* clnt_raw.c serialization */
mutex_t	dname_lock;		/* domainname and domain_fd (getdname.c) */
				/*	and default_domain (rpcdname.c) */
mutex_t	dupreq_lock;		/* dupreq variables (svc_dg.c) */
mutex_t	keyserv_lock;		/* protects first_time and hostname */
				/*	(key_call.c) */
mutex_t	libnsl_trace_lock;	/* serializes rpc_trace() (rpc_trace.c) */
mutex_t	loopnconf_lock;		/* loopnconf (rpcb_clnt.c) */
mutex_t	ops_lock;		/* serializes ops initializations */
mutex_t	portnum_lock;		/* protects ``port'' static in bindresvport() */
mutex_t	proglst_lock;		/* protects proglst list (svc_simple.c) */
mutex_t	rpcsoc_lock;		/* serializes clnt_com_create() (rpc_soc.c) */
mutex_t	svcraw_lock;		/* svc_raw.c serialization */
mutex_t	tsd_lock;		/* protects TSD key creation */
mutex_t	xprtlist_lock;		/* xprtlist (svc_generic.c) */
mutex_t serialize_pkey;		/* serializes calls to public key routines */
mutex_t	svc_thr_mutex;		/* protects thread related variables */
mutex_t	svc_mutex;		/* protects service handle free lists */
mutex_t	svc_exit_mutex;		/* used for clean mt exit */

static mutex_t	*mutex_table[] = {
	&authdes_lock,
	&authnone_lock,
	&authsvc_lock,
	&clntraw_lock,
	&dname_lock,
	&dupreq_lock,
	&keyserv_lock,
	&libnsl_trace_lock,
	&loopnconf_lock,
	&ops_lock,
	&portnum_lock,
	&proglst_lock,
	&rpcsoc_lock,
	&svcraw_lock,
	&tsd_lock,
	&xprtlist_lock,
	&serialize_pkey,
	&svc_thr_mutex,
	&svc_mutex,
	&svc_exit_mutex
};

cond_t	svc_thr_fdwait;		/* threads wait on this for work */

int lock_value;

#pragma init(_libnsl_lock_init)

void
_libnsl_lock_init()
{
	int	i;

/* _thr_main() returns -1 if libthread no linked in */

	if (_thr_main() == -1)
		lock_value = 0;
	else
		lock_value = 1;

	for (i = 0; i <  (sizeof (mutex_table) / sizeof (mutex_table[0])); i++)
		mutex_init(mutex_table[i], 0, (void *) 0);

	for (i = 0; i < (sizeof (rwlock_table) / sizeof (rwlock_table[0])); i++)
		rwlock_init(rwlock_table[i], 0, (void *) 0);

	cond_init(&svc_thr_fdwait, USYNC_THREAD, 0);
}


#undef	rpc_createerr

struct rpc_createerr rpc_createerr;

struct rpc_createerr *
__rpc_createerr()
{
	static thread_key_t rce_key = 0;
	struct rpc_createerr *rce_addr = 0;

	if (_thr_main())
		return (&rpc_createerr);
	if (_thr_getspecific(rce_key, (void **) &rce_addr) != 0) {
		mutex_lock(&tsd_lock);
		if (_thr_keycreate(&rce_key, free) != 0) {
			mutex_unlock(&tsd_lock);
			return (&rpc_createerr);
		}
		mutex_unlock(&tsd_lock);
	}
	if (!rce_addr) {
		rce_addr = (struct rpc_createerr *)
			malloc(sizeof (struct rpc_createerr));
		if (rce_addr == NULL) {
			syslog(LOG_ERR, "__rpc_createerr : out of memory.");
			return (&rpc_createerr);
		}
		if (_thr_setspecific(rce_key, (void *) rce_addr) != 0) {
			if (rce_addr)
				free(rce_addr);
			return (&rpc_createerr);
		}
		memset(rce_addr, 0, sizeof (struct rpc_createerr));
		return (rce_addr);
	}
	return (rce_addr);
}

#undef rpc_callerr

struct rpc_err rpc_callerr;

struct rpc_err *
__rpc_callerr(void)
{
	static thread_key_t rpc_callerr_key = 0;
	struct rpc_err *tsd = 0;

	if (_thr_main())
		return (&rpc_callerr);
	if (_thr_getspecific(rpc_callerr_key, (void **)&tsd) != 0) {
		mutex_lock(&tsd_lock);
		if (_thr_keycreate(&rpc_callerr_key, free) != 0) {
			mutex_unlock(&tsd_lock);
			return (&rpc_callerr);
		}
		mutex_unlock(&tsd_lock);
	}
	if (!tsd) {
		tsd = (struct rpc_err *)
		    calloc(1, sizeof (struct rpc_err));
		if (tsd == NULL) {
			syslog(LOG_ERR, "__rpc_callerr : out of memory.");
			return (&rpc_callerr);
		}
		if (_thr_setspecific(rpc_callerr_key, (void *) tsd) != 0) {
			if (tsd)
				free(tsd);
			return (&rpc_callerr);
		}
		memset(tsd, 0, sizeof (struct rpc_err));
		return (tsd);
	}
	return (tsd);
}
