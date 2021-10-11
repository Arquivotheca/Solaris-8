/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_IRE_H
#define	_INET_IP_IRE_H

#pragma ident	"@(#)ip_ire.h	1.35	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	IPV6_LL_PREFIXLEN	10	/* Number of bits in link-local pref */

#define	IP_FTABLE_HASH_SIZE	32	/* size of each hash table in ptrs */
#define	IP_CACHE_TABLE_SIZE	256
#define	IP_MASK_TABLE_SIZE	(IP_ABITS + 1)		/* 33 ptrs */

#define	IP6_FTABLE_HASH_SIZE	32	/* size of each hash table in ptrs */
#define	IP6_CACHE_TABLE_SIZE	256
#define	IP6_MASK_TABLE_SIZE	(IPV6_ABITS + 1)	/* 129 ptrs */

/*
 * Exclusive-or all bytes in the address thus independent of the byte
 * order as long as table_size does not exceed 256.
 */
#define	IRE_ADDR_HASH(addr, table_size) \
	(((((addr) >> 16) ^ (addr)) ^ ((((addr) >> 16) ^ (addr))>> 8)) \
	% (table_size))

/*
 * Exclusive-or the 6 bytes that are likely to contain the MAC
 * address. Assumes table_size does not exceed 256.
 * Assumes EUI-64 format for good hashing.
 */
#define	IRE_ADDR_HASH_V6(addr, table_size) 				\
	(((addr).s6_addr8[8] ^ (addr).s6_addr8[9] ^			\
	(addr).s6_addr8[10] ^ (addr).s6_addr8[13] ^			\
	(addr).s6_addr8[14] ^ (addr).s6_addr8[15]) % (table_size))

#define	IRE_ADDR_MASK_HASH_V6(addr, mask, table_size) 			\
	((((addr).s6_addr8[8] & (mask).s6_addr8[8]) ^ 		\
	((addr).s6_addr8[9] & (mask).s6_addr8[9]) ^			\
	((addr).s6_addr8[10] & (mask).s6_addr8[10]) ^ 		\
	((addr).s6_addr8[13] & (mask).s6_addr8[13]) ^ 		\
	((addr).s6_addr8[14] & (mask).s6_addr8[14]) ^ 		\
	((addr).s6_addr8[15] & (mask).s6_addr8[15])) % (table_size))

/*
 * match parameter definitions for
 * IRE lookup routines.
 */
#define	MATCH_IRE_DSTONLY	0x0000	/* Match just the address */
#define	MATCH_IRE_TYPE		0x0001	/* Match IRE type */
#define	MATCH_IRE_SRC		0x0002	/* Match IRE source address */
#define	MATCH_IRE_MASK		0x0004	/* Match IRE mask */
#define	MATCH_IRE_WQ		0x0008	/* Match IRE Write Q */
#define	MATCH_IRE_GW		0x0010	/* Match IRE gateway */
#define	MATCH_IRE_IPIF		0x0020	/* Match IRE ipif */
#define	MATCH_IRE_RECURSIVE	0x0040	/* Do recursive lookup if necessary */
#define	MATCH_IRE_DEFAULT	0x0080	/* Return default route if no route */
					/* found. */
#define	MATCH_IRE_RJ_BHOLE	0x0100	/* During lookup if we hit an ire */
					/* with RTF_REJECT or RTF_BLACKHOLE, */
					/* return the ire. No recursive */
					/* lookup should be done. */
#define	MATCH_IRE_ILL		0x0200	/* Match IRE on ill */
#define	MATCH_IRE_IHANDLE	0x0400	/* Match IRE on ihandle */

/* Structure for ire_cache_count() */
typedef struct {
	int	icc_total;	/* Total number of IRE_CACHE */
	int	icc_unused;	/* # off/no PMTU unused since last reclaim */
	int	icc_offlink;	/* # offlink without PMTU information */
	int	icc_pmtu;	/* # offlink with PMTU information */
	int	icc_onlink;	/* # onlink */
} ire_cache_count_t;

/*
 * Structure for ire_cache_reclaim(). Each field is a fraction i.e. 1 meaning
 * reclaim all, N meaning reclaim 1/Nth of all entries, 0 meaning reclaim none.
 */
typedef struct {
	int	icr_unused;	/* Fraction for unused since last reclaim */
	int	icr_offlink;	/* Fraction for offlink without PMTU info */
	int	icr_pmtu;	/* Fraction for offlink with PMTU info */
	int	icr_onlink;	/* Fraction for onlink */
} ire_cache_reclaim_t;

typedef struct {
	uint64_t ire_stats_alloced;	/* # of ires alloced */
	uint64_t ire_stats_freed;	/* # of ires freed */
	uint64_t ire_stats_inserted;	/* # of ires inserted in the bucket */
	uint64_t ire_stats_deleted;	/* # of ires deleted from the bucket */
} ire_stats_t;

/*
 * We use atomics so that we get an accurate accounting on the ires.
 * Otherwise we can't determine leaks correctly.
 */
#define	BUMP_IRE_STATS(ire_stats, x) atomic_add_64(&(ire_stats).x, 1)

extern irb_t *ip_forwarding_table_v6[];
extern irb_t *ip_cache_table_v6;
extern kmutex_t ire_ft_init_lock;
extern ire_stats_t ire_stats_v6;

#ifdef _KERNEL
extern	ipaddr_t	ip_index_to_mask(uint_t);
extern	in6_addr_t	*ip_index_to_mask_v6(uint_t, in6_addr_t *);

extern	int	ip_ire_advise(queue_t *, mblk_t *);
extern	void	ip_ire_append(mblk_t *, ipaddr_t);
extern	void	ip_ire_append_v6(mblk_t *, const in6_addr_t *);
extern	int	ip_ire_constructor(void *, void *, int);
extern	int	ip_ire_delete(queue_t *, mblk_t *);
extern	void	ip_ire_destructor(void *, void *);
extern	void	ip_ire_reclaim(void *);

extern	int	ip_ire_report(queue_t *, mblk_t *, caddr_t);
extern	int	ip_ire_report_v6(queue_t *, mblk_t *, caddr_t);

extern	void	ip_ire_req(queue_t *, mblk_t *);

extern	int	ip_mask_to_index(ipaddr_t);
extern	int	ip_mask_to_index_v6(const in6_addr_t *);

extern	ire_t	*ipif_to_ire(ipif_t *);
extern	ire_t	*ipif_to_ire_v6(ipif_t *);

extern	ire_t	*ire_add(ire_t *);
extern	void	ire_add_then_send(queue_t *, mblk_t *);
extern	ire_t	*ire_add_v6(ire_t *);

extern	void	ire_cache_count(ire_t *, char *);
extern	ire_t	*ire_cache_lookup(ipaddr_t);
extern	ire_t	*ire_cache_lookup_v6(const in6_addr_t *);
extern	void	ire_cache_reclaim(ire_t *, char *);

extern	ire_t	*ire_create(uchar_t *, uchar_t *, uchar_t *, uchar_t *, uint_t,
    mblk_t *, queue_t *, queue_t *, ushort_t, mblk_t *,
    ipif_t *, ipaddr_t, uint32_t, uint32_t, uint32_t, const iulp_t *);

extern	ire_t	**ire_create_bcast(ipif_t *, ipaddr_t, ire_t **, int);

extern	ire_t	*ire_create_common(ire_t *, uint_t, mblk_t *, queue_t *,
    queue_t *, ushort_t, mblk_t *, ipif_t *, uint32_t,
    uint32_t, uint32_t, ushort_t, const iulp_t *);

extern	ire_t	*ire_create_v6(const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, const in6_addr_t *, uint_t, mblk_t *, queue_t *,
    queue_t *, ushort_t, mblk_t *, ipif_t *,
    const in6_addr_t *, uint32_t, uint32_t, uint_t, const iulp_t *);

extern	ire_t	*ire_ctable_lookup(ipaddr_t, ipaddr_t, int, ipif_t *, queue_t *,
    int);

extern	ire_t	*ire_ctable_lookup_v6(const in6_addr_t *, const in6_addr_t *,
    int, ipif_t *, queue_t *, int);

extern	void	ire_delete(ire_t *);
extern	void	ire_delete_cache_gw(ire_t *, char *);
extern	void	ire_delete_cache_gw_v6(ire_t *, char *);
extern	void	ire_delete_host_redirects_v6(const in6_addr_t *);
extern	void	ire_delete_v6(ire_t *);

extern	void	ire_expire(ire_t *, char *);
extern	void	ire_fastpath_update(ire_t *, char *);

extern	void	ire_flush_cache_v4(ire_t *, int);
extern	void	ire_flush_cache_v6(ire_t *, int);

extern	ire_t	*ire_ftable_lookup(ipaddr_t, ipaddr_t, ipaddr_t, int, ipif_t *,
    ire_t **, queue_t *, uint32_t, int);

extern	ire_t	*ire_ftable_lookup_v6(const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, int, ipif_t *, ire_t **, queue_t *, uint32_t, int);

extern	ire_t	*ire_ihandle_lookup_onlink(ire_t *);
extern	ire_t	*ire_ihandle_lookup_offlink(ire_t *, ire_t *);
extern	ire_t	*ire_ihandle_lookup_offlink_v6(ire_t *, ire_t *);

extern	ire_t 	*ire_lookup_local(void);
extern	ire_t 	*ire_lookup_local_v6(void);

extern  ire_t	*ire_lookup_loop_multi(ipaddr_t);
extern  ire_t	*ire_lookup_loop_multi_v6(const in6_addr_t *);

extern	void	ire_pkt_count(ire_t *, char *);
extern	void	ire_pkt_count_v6(ire_t *, char *);

extern	void	ire_refrele(ire_t *);
extern	ire_t	*ire_route_lookup(ipaddr_t, ipaddr_t, ipaddr_t, int, ipif_t *,
    ire_t **, queue_t *, int);

extern	ire_t	*ire_route_lookup_v6(const in6_addr_t *, const in6_addr_t *,
    const in6_addr_t *, int, ipif_t *, ire_t **, queue_t *, int);

extern	void	ire_send(queue_t *, mblk_t *, ire_t *);
extern	void	ire_send_v6(queue_t *, mblk_t *, ire_t *);

extern ill_t	*ire_to_ill(ire_t *);

extern	void	ire_walk(pfv_t, char *);
extern	void	ire_walk_ill(ill_t *, pfv_t, char *);
extern	void	ire_walk_ill_v4(ill_t *, pfv_t, char *);
extern	void	ire_walk_ill_v6(ill_t *, pfv_t, char *);
extern	void	ire_walk_v4(pfv_t, char *);
extern	void	ire_walk_v6(pfv_t, char *);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_IRE_H */
