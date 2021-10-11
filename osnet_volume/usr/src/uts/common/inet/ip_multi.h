/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_MULTI_H
#define	_INET_IP_MULTI_H

#pragma ident	"@(#)ip_multi.h	1.18	99/10/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

/*
 * Extern functions
 */
extern	int		igmp_input(queue_t *, mblk_t *, ill_t *);
extern	void		igmp_joingroup(ilm_t *);
extern	void		igmp_leavegroup(ilm_t *);
extern	void		igmp_slowtimo(void *);
extern	int		igmp_timeout_handler(void);
extern	void		igmp_timeout_start(int);

extern	void		ilg_delete_all(ipc_t *);
extern	ilg_t		*ilg_lookup_ill(ipc_t *, ipaddr_t, ill_t *);
extern	ilg_t		*ilg_lookup_ill_v6(ipc_t *, const in6_addr_t *,
			    ill_t *);

extern void		ill_recover_multicast(ill_t *);

extern	void		ilm_free(ipif_t *);
extern	ilm_t		*ilm_lookup_ill(ill_t *, ipaddr_t);
extern	ilm_t		*ilm_lookup_ill_v6(ill_t *, const in6_addr_t *);
extern	ilm_t		*ilm_lookup_ipif(ipif_t *, ipaddr_t);

extern	int		ip_addmulti(ipaddr_t, ipif_t *);
extern	int		ip_addmulti_v6(const in6_addr_t *, ipif_t *);
extern	int		ip_delmulti(ipaddr_t, ipif_t *);
extern	int		ip_delmulti_v6(const in6_addr_t *, ipif_t *);
extern	void		ip_multicast_loopback(queue_t *, ill_t *, mblk_t *);
extern	int		ip_mforward(ill_t *, ipha_t *, mblk_t *);
extern	void		ip_mroute_decap(queue_t *, mblk_t *);
extern	int		ip_mroute_mrt(mblk_t *);
extern	int		ip_mroute_stats(mblk_t *);
extern	int		ip_mroute_vif(mblk_t *);
extern	int		ip_mrouter_done(void);
extern	int		ip_mrouter_get(int, queue_t *, uchar_t *);
extern	int		ip_mrouter_set(int, queue_t *, int, uchar_t *, int);

extern	int		ip_opt_add_group(ipc_t *, boolean_t, ipaddr_t,
			    ipaddr_t);
extern	int		ip_opt_delete_group(ipc_t *, boolean_t, ipaddr_t,
			    ipaddr_t);
extern	int		ip_opt_add_group_v6(ipc_t *, boolean_t,
			    const in6_addr_t *, int);
extern	int		ip_opt_delete_group_v6(ipc_t *, boolean_t,
			    const in6_addr_t *, int);

extern void		ip_wput_ctl(queue_t *, mblk_t *);

extern	int		mrt_ioctl(int, intptr_t);
extern	int		pim_input(queue_t *, mblk_t *);
extern	void		reset_ipc_ipif(ipif_t *);
extern	void		reset_ipc_ill(ill_t *);
extern	void		reset_mrt_ill(ill_t *);
extern	void		reset_mrt_vif_ipif(ipif_t *);

/*
 * Extern variables
 */
extern  queue_t *ip_g_mrouter;

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_MULTI_H */
