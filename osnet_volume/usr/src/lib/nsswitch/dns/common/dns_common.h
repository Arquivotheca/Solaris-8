/*
 * Copyright (c) 1993, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Common code and structures used by name-service-switch "dns" backends.
 */

#ifndef _DNS_COMMON_H
#define	_DNS_COMMON_H

#pragma ident	"@(#)dns_common.h	1.5	99/09/20 SMI"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <thread.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <syslog.h>
#include <nsswitch.h>
#include <nss_dbdefs.h>
#include <stdlib.h>
#include <signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dns_backend *dns_backend_ptr_t;
typedef nss_status_t (*dns_backend_op_t)(dns_backend_ptr_t, void *);

struct dns_backend {
	dns_backend_op_t	*ops;
	nss_dbop_t		n_ops;
};

/* multithreaded libresolv2 related functions and variables */
extern void	(*set_no_hosts_fallback)();
extern struct __res_state	*(*set_res_retry)();
extern int	(*enable_mt)();
extern int	(*disable_mt)();
extern int	*(*get_h_errno)();
extern mutex_t	one_lane;

extern int _thr_sigsetmask(int, const sigset_t *, sigset_t *);
extern int _mutex_lock(mutex_t *);
extern int _mutex_unlock(mutex_t *);
extern const char *inet_ntop(int, const void *, char *, size_t);

extern int ent2result(struct hostent *, nss_XbyY_args_t *, int);

nss_backend_t *_nss_dns_constr(dns_backend_op_t *, int);
extern	nss_status_t _herrno2nss(int);

#ifdef	__cplusplus
}
#endif

#endif /* _DNS_COMMON_H */
