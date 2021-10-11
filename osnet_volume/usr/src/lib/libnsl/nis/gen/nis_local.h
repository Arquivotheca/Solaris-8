/*
 *	nis_local.h
 *
 * Manifest constants for the NIS+ client library.
 *
 *	Copyright (c) 1988-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */


#ifndef _NIS_LOCAL_H
#define	_NIS_LOCAL_H

#pragma ident	"@(#)nis_local.h	1.23	99/04/27 SMI"

#include "../../rpc/rpc_mt.h"
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef DEBUG
#define	ASSERT(cond)  \
	{ \
		if (!(cond)) { \
			(void) printf("ASSERT ERROR:(%s),file %s,line %d\n", \
			    /* */#cond, __FILE__, __LINE__); \
			abort(); \
		} \
	}
#else
#define	ASSERT(cond)  /* no op */
#endif /* DEBUG */

#define	MAX_LINKS	16
#define	NIS_MAXSRCHLEN		2048
#define	NIS_MAXPATHDEPTH	128
#define	NIS_MAXPATHLEN		8192
#ifndef NIS_MAXREPLICAS
#define	NIS_MAXREPLICAS		128
#endif
typedef u_char h_mask[NIS_MAXREPLICAS+1];

/* clock definitions */
#define	MAXCLOCKS 16
#define	CLOCK_SERVER 		0
#define	CLOCK_DB 		1
#define	CLOCK_CLIENT 		2
#define	CLOCK_CACHE 		3
#define	CLOCK_CACHE_SEARCH 	4
#define	CLOCK_CACHE_FINDDIR 	5
#define	CLOCK_SCRATCH		6

#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

struct nis_tick_data {
	uint32_t aticks,
		dticks,
		zticks,
		cticks;
};

#define	UPD_TICKS(t, r) {t.aticks += r->aticks; \
			t.dticks += r->dticks; \
			t.zticks += r->zticks; \
			t.cticks += r->cticks; }
#define	CLR_TICKS(t) {t.aticks = 0; \
			t.dticks = 0; \
			t.zticks = 0; \
			t.cticks = 0; }
#define	RET_TICKS(t, r) {r->aticks = t.aticks; \
			r->dticks = t.dticks; \
			r->zticks = t.zticks; \
			r->cticks = t.cticks; }

/*
 *  Either srv or name must be set.  If srv is set, then we bind
 *  to that server, otherwise we bind to name and parent_first
 *  determines whether we should bind to the name itself or to
 *  the parent.
 */
typedef struct {
	nis_server *srv;
	int nsrv;
	char *name;
	char *server_name;
	int parent_first;
	u_int flags;
	struct timeval timeout;
	nis_error niserror;
	uint32_t aticks;

	/* private:  used internally */
	int state;
	nis_bound_directory *binding;
	int base;
	int end;
	int count;	/* end - base + 1 */
	int start;	/* base <= start < end */
	int cur;
	int bound_to;
	int refresh_count;
} nis_call_state;

/*
 * Manifest timeouts
 */
#define	NIS_PING_TIMEOUT	5   /* timeout of ping operations */
#define	NIS_DUMP_TIMEOUT	120 /* timeout for dump/dumplog operations */
#define	NIS_FINDDIR_TIMEOUT	15  /* timeout for finddirectory operations */
#define	NIS_TAG_TIMEOUT		30  /* timeout for statistics operations */
#define	NIS_GEN_TIMEOUT		15  /* timeout for general NIS+ operations */
#define	NIS_READ_TIMEOUT	5   /* timeout for read NIS+ operations */
#define	NIS_HARDSLEEP		5   /* interval to sleep during HARD_LOOKUP */
#define	NIS_CBACK_TIMEOUT	180 /* timeout for callback */

/*
 * use for the cached client handles
 */
#define	SRV_IS_FREE		0
#define	SRV_TO_BE_FREED		1
#define	SRV_IN_USE		2
#define	SRV_INVALID		3
#define	SRV_AUTH_INVALID	4

#define	BAD_SERVER 1
#define	GOOD_SERVER 0

#define	NIS_SEND_SIZE 2048
#define	NIS_RECV_SIZE 2048
#define	NIS_TCP_TIMEOUT 3600
#define	NIS_UDP_TIMEOUT 120

/*
 * Internal functions
 */
extern nis_result *__nis_core_lookup(ib_request *, u_int, int, void *,
				int (*)(nis_name, nis_object *, void *));
extern CLIENT	*__nis_get_server(nis_call_state *);
extern void	__nis_release_server(nis_call_state *, CLIENT *,
				enum clnt_stat);
extern void	__nis_bad_auth_server(CLIENT *);

extern void 	* thr_get_storage(thread_key_t *, int, void(*)(void *));
extern void 	thr_sigblock(sigset_t *);
extern void 	abort(void);
extern int	_thr_main(void);
void nis_sort_directory_servers(directory_obj *);

nis_error nis_bind_dir(char *, int, nis_bound_directory **, u_int);
CLIENT *nis_client_handle(nis_bound_directory *, int, u_int);
nis_server * __nis_server_dup(nis_server *, nis_server *);
void *__inet_get_local_interfaces(void);
void __inet_free_local_interfaces(void *);
int __inet_address_is_local(void *, struct in_addr);
int __inet_address_count(void *);
int __inet_uaddr_is_local(void *, struct netconfig *, char *);
char *__inet_get_uaddr(void *, struct netconfig *, int);
char *__inet_get_networka(void *, int);
int __inet_address_is_local_af(void *, sa_family_t, void *);
int __nis_server_is_local(endpoint *, void *);
endpoint *__get_bound_endpoint(nis_bound_directory *binding, int n);
endpoint *__endpoint_dup(endpoint *src, endpoint *dst);
void __endpoint_free(endpoint *ep);
void nis_print_binding(nis_bound_directory *binding);
char *__nis_get_server_address(struct netconfig *ncp, endpoint *ep);
char **__nis_path(char *from, char *to, int *length);
void __nis_path_free(char **names, int length);
int32_t __nis_librand(void);
int __nis_host_is_server(nis_server *srv, int nsrv);
int __nis_parse_path(char *path, nis_name *list, int max);
void __nis_print_result(nis_result *res);
void __nis_print_rpc_result(enum clnt_stat status);
void __nis_print_call(CLIENT *clnt, int proc);
void __nis_print_fdreq(fd_args *);
void __nis_print_req(ib_request *req);
void __nis_print_nsreq(ns_request *req);
void __nis_init_call_state(nis_call_state *state);
void __nis_reset_call_state(nis_call_state *state);
nis_error nis_bind_server(nis_server *srv, int nsrv,
		nis_bound_directory **binding);
nis_error nis_call(nis_call_state *state, rpcproc_t func,
	xdrproc_t req_proc, char *req, xdrproc_t res_proc, char *res);
nis_name __nis_nextsep_of(char *);
int _thr_setspecific(thread_key_t key, void *value);
int _thr_getspecific(thread_key_t key, void **valuep);
int _rw_rdlock(rwlock_t *rwlp);
int _rw_unlock(rwlock_t *rwlp);
int __rpc_timeval_to_msec(struct timeval *t);
AUTH *authdes_pk_seccreate(char *servername, netobj *pkey, u_int window,
		char *timehost, des_block *ckey, nis_server *srvr);
void __nis_netconfig2ep(struct netconfig *nc, endpoint *ep);
bool_t __nis_netconfig_matches_ep(struct netconfig *nc, endpoint *ep);

/*
 * Internal variables
 */
extern mutex_t __nis_callback_lock;

/*
 * External functions without prototypes.
 */
extern int	_rw_wrlock(rwlock_t *);
extern unsigned	_sleep(unsigned);
extern int	_mutex_lock(mutex_t *);
extern int	_mutex_unlock(mutex_t *);
extern int	_fstat(int, struct stat *);
extern int	_fcntl(int, int, ...);

#ifdef __cplusplus
}
#endif

#endif /* _NIS_LOCAL_H */
