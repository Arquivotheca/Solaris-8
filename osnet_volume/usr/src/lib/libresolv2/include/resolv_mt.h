/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 */

#ifndef	_RESOLV_MT_H
#define	_RESOLV_MT_H

#pragma ident	"@(#)resolv_mt.h	1.2	99/02/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	SUNW_MT_RESOLVER

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <irs_data.h>
/*
 * This undef may fail if irs_data.h changes. That's good, since this code must
 * be re-examined in that case.
 */
#undef	net_data


/* Access functions for the libresolv private interface */

int			__res_enable_mt(void);
int			__res_disable_mt(void);
int			*__res_get_h_errno(void);
struct __res_state	*__res_get_res(void);

/* Per-thread context */

typedef struct {
	u_int				enabled;
	struct __res_state		_res_private;
	int				h_errno_private;
#ifdef SUNW_HOSTS_FALLBACK
	int				no_hosts_fallback_private;
#endif /* SUNW_HOSTS_FALLBACK */
	struct __net_data		net_data;
	char				inet_nsap_ntoa_tmpbuf[255*3];
	char				sym_ntos_unname[20];
	char				sym_ntop_unname[20];
	char				p_option_nbuf[40];
	char				p_time_nbuf[40];
	char				precsize_ntoa_retbuf[sizeof "90000000.00"];
	char				p_secstodate_output[15];
	char				hostalias_abuf[MAXDNAME];
	int				res_send_s;
	int				res_send_connected;
	int				res_send_vc;
	res_send_qhook			res_send_Qhook;
	res_send_rhook			res_send_Rhook;
} mtctxres_t;

/* Thread-specific data (TSD) */

extern mtctxres_t			*___mtctxres();
#define	mtctxres			(___mtctxres())

#define	h_errno				(*(__res_get_h_errno()))
#define	_res				(*(__res_get_res()))

/* Various static data that should be TSD */

#define	net_data			(mtctxres->net_data)
#define	sym_ntos_unname			(mtctxres->sym_ntos_unname)
#define	sym_ntop_unname			(mtctxres->sym_ntop_unname)
#define	inet_nsap_ntoa_tmpbuf		(mtctxres->inet_nsap_ntoa_tmpbuf)
#define	p_option_nbuf			(mtctxres->p_option_nbuf)
#define	p_time_nbuf			(mtctxres->p_time_nbuf)
#define	precsize_ntoa_retbuf		(mtctxres->precsize_ntoa_retbuf)
#define	p_secstodate_output		(mtctxres->p_secstodate_output)
#define	hostalias_abuf			(mtctxres->hostalias_abuf)
#define	res_send_s			(mtctxres->res_send_s)
#define	res_send_connected		(mtctxres->res_send_connected)
#define	res_send_vc			(mtctxres->res_send_vc)
#define	res_send_Qhook			(mtctxres->res_send_Qhook)
#define	res_send_Rhook			(mtctxres->res_send_Rhook)

#endif /* SUNW_MT_RESOLVER */

#ifdef	__cplusplus
}
#endif

#endif /* _RESOLV_MT_H */
