/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_NDP_H
#define	_INET_NDP_H

#pragma ident	"@(#)ip_ndp.h	1.3	99/08/19 SMI"

/*
 * Internal definitions for the kernel implementation of the IPv6
 * Neighbor Discovery Protocol (NDP).
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL
/*
 * ndp_g_lock protects neighbor cache tables access and
 * insertion/removal of cache entries into/from these tables.
 * nce_lock protects nce_pcnt, nce_rcnt, nce_qd_mp nce_state,
 * nce_refcnt and nce_last.
 * nce_refcnt is incremented for every ire pointing to this nce and
 * every time ndp_lookup() finds an nce.
 * Should there be a need to obtain nce_lock and ndp_g_lock, ndp_g_lock is
 * acquired first.
 * To avoid becoming exclusive when deleting NCEs, ndp_walk() routine holds
 * the ndp_g_lock (i.e global lock) and marks NCEs to be deleted with
 * NCE_F_CONDEMNED.  When all active users of such NCEs are gone the walk
 * routine passes a list for deletion to nce_ire_delete_list().
 */
kmutex_t	ndp_g_lock; /* Lock protecting nighbor cache hash table */
/* NDP Cache Entry */
typedef struct nce_s {
	struct	nce_s	*nce_next;	/* Hash chain next pointer */
	struct	nce_s	**nce_ptpn;	/* Pointer to previous next */
	struct 	ill_s	*nce_ill;	/* Associated ill */
	uint16_t	nce_flags;	/* See below */
	uint16_t	nce_state;	/* See reachability states in if.h */
	int16_t		nce_pcnt;	/* Probe counter */
	uint16_t	nce_rcnt;	/* Retransmit counter */
	in6_addr_t	nce_addr;	/* address of the nighbor */
	in6_addr_t	nce_mask;	/* If not all ones, mask allows an */
	    /* entry  to respond to requests for a group of addresses, for */
	    /* instantance multicast addresses				   */
	in6_addr_t	nce_extract_mask; /* For mappings */
	uint32_t	nce_ll_extract_start;	/* For mappings */
#define	nce_first_mp_to_free	nce_fp_mp
	mblk_t		*nce_fp_mp;	/* link layer fast path mp */
	mblk_t		*nce_res_mp;	/* DL_UNITDATA_REQ or link layer mp */
	mblk_t		*nce_qd_mp;	/* Head outgoing queued packets */
#define	nce_last_mp_to_free	nce_qd_mp
	mblk_t		*nce_timer_mp;	/* NDP timer mblk */
	mblk_t		*nce_mp;	/* mblk we are in, last to be freed */
	uint64_t	nce_last;	/* Time last reachable in msec */
	uint32_t	nce_refcnt;	/* nce active usage count */
	kmutex_t	nce_lock;	/* See comments on top for what */
					/* this field protects */
} nce_t;

/* nce_flags  */
#define	NCE_F_PERMANENT		0x1
#define	NCE_F_MAPPING		0x2
#define	NCE_F_ISROUTER		0x4
#define	NCE_F_PROXY		0x8
#define	NCE_F_NONUD		0x10
#define	NCE_F_ANYCAST		0x20
#define	NCE_F_CONDEMNED		0x40

#define	NCE_EXTERNAL_FLAGS_MASK \
	(NCE_F_PERMANENT|NCE_F_MAPPING|NCE_F_ISROUTER|NCE_F_NONUD)

/* State REACHABLE, STALE, DELAY or PROBE */
#define	NCE_ISREACHABLE(nce)			\
	(((((nce)->nce_state) >= ND_REACHABLE) &&	\
	((nce)->nce_state) <= ND_PROBE))

/* NDP flags set in SOL/ADV requests */
#define	NDP_UNICAST		0x1
#define	NDP_ISROUTER		0x2
#define	NDP_SOLICITED		0x4
#define	NDP_ORIDE		0x8

/* Number of packets queued in NDP for a neighbor */
#define	ND_MAX_Q		4


#define	NCE_REFHOLD(nce) {		\
	mutex_enter(&(nce)->nce_lock);	\
	(nce)->nce_refcnt++;		\
	ASSERT((nce)->nce_refcnt != 0);	\
	mutex_exit(&(nce)->nce_lock);	\
}

/* nce_inactive destroys the mutex thus no mutex_exit is needed */
#define	NCE_REFRELE(nce) {		\
	mutex_enter(&(nce)->nce_lock);	\
	ASSERT((nce)->nce_refcnt != 0);	\
	if (--(nce)->nce_refcnt == 0)	\
		ndp_inactive(nce);	\
	else {				\
		mutex_exit(&(nce)->nce_lock);\
	}				\
}

/* Structure for ndp_cache_count() */
typedef struct {
	int	ncc_total;	/* Total number of NCEs */
	int	ncc_host;	/* NCE entries without R bit set */
} ncc_cache_count_t;

/*
 * Structure of ndp_cache_reclaim().  Each field is a fraction i.e. 1 means
 * reclaim all, N means reclaim 1/Nth of all entries, 0 means reclaim none.
 */
typedef struct {
	int	ncr_host;	/* Fraction for host entries */
} nce_cache_reclaim_t;

/* When SAP is greater than zero address appears before SAP */
#define	NCE_LL_ADDR_OFFSET(ill)	(((ill)->ill_sap_length) < 0 ? \
	(sizeof (dl_unitdata_req_t)) : \
	((sizeof (dl_unitdata_req_t)) + (ABS((ill)->ill_sap_length))))

#define	NCE_LL_SAP_OFFSET(ill) (((ill)->ill_sap_length) < 0 ? \
	((sizeof (dl_unitdata_req_t)) + ((ill)->ill_phys_addr_length)) : \
	(sizeof (dl_unitdata_req_t)))

/*
 * Exclusive-or the 6 bytes that are likely to contain the MAC
 * address. Assumes table_size does not exceed 256.
 * Assumes EUI-64 format for good hashing.
 */
#define	NCE_ADDR_HASH_V6(addr, table_size)				\
	(((addr).s6_addr8[8] ^ (addr).s6_addr8[9] ^			\
	(addr).s6_addr8[10] ^ (addr).s6_addr8[13] ^			\
	(addr).s6_addr8[14] ^ (addr).s6_addr8[15]) % (table_size))

extern	void	ndp_cache_count(nce_t *, char *);
extern	void	ndp_cache_reclaim(nce_t *, char *);
extern	void	ndp_delete(nce_t *);
extern	void	ndp_delete_per_ill(nce_t *, uchar_t *);
extern	void	ndp_fastpath_update(nce_t *, char  *);
extern	nd_opt_hdr_t *ndp_get_option(nd_opt_hdr_t *, int, int);
extern	void	ndp_inactive(nce_t *);
extern	void	ndp_input(ill_t *, mblk_t *);
extern	nce_t	*ndp_lookup(ill_t *, const in6_addr_t *);
extern	int	ndp_lookup_then_add(ill_t *, uchar_t *, const in6_addr_t *,
    const in6_addr_t *, const in6_addr_t *, uint32_t, uint32_t,
    uint16_t, nce_t **);
extern	int	ndp_mcastreq(ill_t *, const in6_addr_t *, uint32_t, uint32_t,
    mblk_t *);
extern	int	ndp_noresolver(ipif_t *, const in6_addr_t *);
extern	void	ndp_process(nce_t *, uchar_t *, uint32_t, boolean_t);
extern	int	ndp_query(ill_t *, lif_nd_req_t *);
extern	int	ndp_report(queue_t *, mblk_t *, caddr_t);
extern	int	ndp_resolver(ipif_t *, const in6_addr_t *, mblk_t *);
extern	int	ndp_sioc_update(ill_t *, lif_nd_req_t *);
extern	boolean_t	ndp_verify_optlen(nd_opt_hdr_t *, int);
extern	void	ndp_timer(void *);
extern	void	ndp_walk(ill_t *, pfi_t, uchar_t *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_NDP_H */
