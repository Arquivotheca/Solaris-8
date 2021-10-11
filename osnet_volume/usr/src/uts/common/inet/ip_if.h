/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_IF_H
#define	_INET_IP_IF_H

#pragma ident	"@(#)ip_if.h	1.29	99/11/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	PREFIX_INFINITY	0xffffffffUL
#define	IP_MAX_HW_LEN	40

#define	IP_LOOPBACK_MTU	(8*1024)

/* DLPI SAPs are in host byte order for all systems */
#define	IP_DL_SAP	0x0800
#define	IP6_DL_SAP	0x86dd

#ifdef	_KERNEL

extern	ipif_t	*ifgrp_scheduler(ipif_t *);
extern	mblk_t	*ill_arp_alloc(ill_t *, uchar_t *, ipaddr_t);
extern	void	ill_dlpi_done(ill_t *);
extern	void	ill_dlpi_send(ill_t *, mblk_t *);
extern	mblk_t	*ill_dlur_gen(uchar_t *, uint_t, t_uscalar_t, t_scalar_t);
extern	ill_t	*ill_lookup_on_ifindex(uint_t, boolean_t);
extern	ill_t	*ill_lookup_on_name(char *, size_t, boolean_t, boolean_t);
extern	char	*ipif_get_name(ipif_t *, char *, int);
extern	ipif_t	*ipif_lookup_addr(ipaddr_t, ill_t *);
extern	ipif_t	*ipif_lookup_addr_v6(const in6_addr_t *, ill_t *);
extern	ipif_t	*ipif_lookup_group(ipaddr_t);
extern	ipif_t	*ipif_lookup_group_v6(const in6_addr_t *);
extern	ipif_t	*ipif_lookup_interface(ipaddr_t, ipaddr_t);
extern	ipif_t	*ipif_lookup_interface_v6(const in6_addr_t *,
    const in6_addr_t *);
extern	ipif_t	*ipif_lookup_on_name(char *, size_t, boolean_t, boolean_t *,
    boolean_t);
extern	ipif_t	*ipif_lookup_remote(ill_t *, ipaddr_t);
extern	ipif_t	*ipif_lookup_remote_v6(ill_t *, const in6_addr_t *);
extern	ipif_t	*ipif_lookup_scope_v6(ill_t *, const in6_addr_t *,
    uint_t, uint_t, uint_t, uint_t, uint_t, uint_t *);
extern	ipif_t	*ipif_select_source(ill_t *, ipaddr_t);
extern	void	ifgrp_delete(ipif_t *);
extern	int	ifgrp_get(queue_t *, mblk_t *, void *);
extern	boolean_t	ifgrp_insert(ipif_t *);
extern	int	ifgrp_report(queue_t *, mblk_t *, void *);
extern	int	ifgrp_set(queue_t *, mblk_t *, char *, void *);
extern	void	ill_cache_delete(ire_t *, char *);
extern	void	ill_delete(ill_t *);
extern	int	ill_dl_phys(ill_t *, ipif_t *, mblk_t *, queue_t *);
extern	int	ill_dls_info(struct sockaddr_dl *, ipif_t *);
extern	void	ill_down(ill_t *);
extern	void	ill_fastpath_ack(ill_t *, mblk_t *);
extern	void	ill_fastpath_probe(ill_t *, mblk_t *);
extern	void	ill_frag_prune(ill_t *, uint_t);
extern	boolean_t	ill_frag_timeout(ill_t *, time_t);
extern	int	ill_init(queue_t *, ill_t *);
extern	boolean_t	ill_setdefaulttoken(ill_t *);
extern	int	ip_ill_report(queue_t *, mblk_t *, void *);
extern	int	ip_ipif_report(queue_t *, mblk_t *, void *);
extern	void	ip_ll_subnet_defaults(ill_t *, mblk_t *);
extern	int	ip_rt_add(ipaddr_t, ipaddr_t, ipaddr_t, int, ipif_t *,
    ire_t **, boolean_t);
extern	int	ip_rt_add_v6(const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, int, ipif_t *, ire_t **);
extern	int	ip_rt_delete(ipaddr_t, ipaddr_t, ipaddr_t, uint_t, int,
    ipif_t *, boolean_t);
extern	int	ip_rt_delete_v6(const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, uint_t, int, ipif_t *);
extern	int	ip_siocdelndp_v6(ipif_t *, struct lif_nd_req *, queue_t *);
extern	int	ip_siocqueryndp_v6(ipif_t *, struct lif_nd_req *, queue_t *,
    mblk_t  *);
extern	int	ip_siocsetndp_v6(ipif_t *, lif_nd_req_t *, queue_t *,
    mblk_t  *);

extern	int	ip_sioctl_addr(ipif_t *, sin_t *, queue_t *, mblk_t *);
extern	void	ip_sioctl_copyin_done(queue_t *, mblk_t *);
extern	void	ip_sioctl_copyin_setup(queue_t *, mblk_t *);
extern	int	ip_sioctl_copyin_writer(mblk_t *);
extern	void	ip_sioctl_iocack(queue_t *, mblk_t *);
extern	boolean_t	ipif_arp_up(ipif_t *, ipaddr_t);
extern	void	ipif_down(ipif_t *);
extern	void	ipif_multicast_down(ipif_t *);
extern	void	ipif_multicast_up(ipif_t *);
extern	void	ipif_ndp_down(ipif_t *);
extern	int	ipif_ndp_up(ipif_t *, const in6_addr_t *);
extern	void	ipif_recover_ire(ipif_t *);
extern	void	ipif_recover_ire_v6(ipif_t *);
extern	int	ipif_up(ipif_t *, queue_t *, mblk_t *);
extern	int	ipif_up_done(ipif_t *);
extern	int	ipif_up_done_v6(ipif_t *);
extern	void	ipif_update_other_ipifs(ipif_t *);
extern	void	ipif_update_other_ipifs_v6(ipif_t *);
extern	ipif_t	*ipif_select_source_v6(ill_t *, const in6_addr_t *);
extern	int	ipif_setlinklocal(ipif_t *);
extern	void	ipif_set_tun_llink(ill_t *, struct iftun_req *);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_IF_H */
