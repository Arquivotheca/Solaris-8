/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_RTS_H
#define	_INET_IP_RTS_H

#pragma ident	"@(#)ip_rts.h	1.8	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL
extern	void	ip_rts_change(int, ipaddr_t, ipaddr_t,
    ipaddr_t, ipaddr_t, ipaddr_t, int, int,
    int);

extern	void	ip_rts_change_v6(int, const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, const in6_addr_t *, const in6_addr_t *, int, int, int);

extern	void	ip_rts_ifmsg(ipif_t *);

extern	void	ip_rts_newaddrmsg(int, int, ipif_t *);

extern	int	ip_rts_request(queue_t *, mblk_t *);

extern	void	ip_rts_rtmsg(int, ire_t *, int);

extern	mblk_t	*rts_alloc_msg(int, int, sa_family_t);

extern	size_t	rts_data_msg_size(int, sa_family_t);

extern	void	rts_fill_msg_v6(int, int, const in6_addr_t *,
    const in6_addr_t *, const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, const in6_addr_t *, ipif_t *, mblk_t *);

extern	size_t	rts_header_msg_size(int);

extern	void	rts_queue_input(mblk_t *, queue_t *, sa_family_t);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_RTS_H */
