/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef	_INET_TUN_H
#define	_INET_TUN_H

#pragma ident	"@(#)tun.h	1.4	99/08/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#define	TUN_MODID	5134
#define	ATUN_MODID	5135
#define	TUN_NAME	"tun"
#define	ATUN_NAME	"atun"

struct	tunstat {
	struct	kstat_named	tuns_nocanput;
	struct	kstat_named	tuns_xmtretry;
	struct	kstat_named	tuns_allocbfail;

	struct	kstat_named	tuns_ipackets;	/* ifInUcastPkts */
	struct	kstat_named	tuns_opackets;	/* ifOutUcastPkts */
	struct	kstat_named	tuns_InErrors;
	struct	kstat_named	tuns_OutErrors;

	struct  kstat_named	tuns_rcvbytes;	/* # octets received */
						/* MIB - ifInOctets */
	struct  kstat_named	tuns_xmtbytes;  /* # octets transmitted */
						/* MIB - ifOutOctets */
	struct  kstat_named	tuns_multircv;	/* # multicast packets */
						/* delivered to upper layer */
						/* MIB - ifInNUcastPkts */
	struct  kstat_named	tuns_multixmt;	/* # multicast packets */
						/* requested to be sent */
						/* MIB - ifOutNUcastPkts */
	struct  kstat_named	tuns_InDiscard;	/* # rcv packets discarded */
						/* MIB - ifInDiscards */
	struct  kstat_named	tuns_OutDiscard; /* # xmt packets discarded */
						/* MIB - ifOutDiscards */
	struct	kstat_named	tuns_HCInOctets;
	struct	kstat_named	tuns_HCInUcastPkts;
	struct	kstat_named	tuns_HCInMulticastPkts;
	struct	kstat_named	tuns_HCOutOctets;
	struct	kstat_named	tuns_HCOutUcastPkts;
	struct	kstat_named	tuns_HCOutMulticastPkts;
};

typedef struct tun_stats_s {
	/* protected by t_global_lock */
	struct tun_stats_s *ts_next;
	kmutex_t	ts_lock;		/* protects from here down */
	struct tun_s	*ts_atp;
	uint_t		ts_refcnt;
	uint_t		ts_lower;
	uint_t		ts_type;
	t_uscalar_t	ts_ppa;
	kstat_t		*ts_ksp;
} tun_stats_t;

/*  Used for recovery from memory allocation failure */
typedef struct eventid_s {
	bufcall_id_t	ev_wbufcid;		/* needed for recovery */
	bufcall_id_t	ev_rbufcid;		/* needed for recovery */
	timeout_id_t	ev_wtimoutid;		/* needed for recovery */
	timeout_id_t	ev_rtimoutid;		/* needed for recovery */
} eventid_t;

/* per-instance data structure */
/* Note: if t_recnt > 1, then t_indirect must be null */
typedef struct tun_s {
	struct tun_s	*tun_next;
	kmutex_t	tun_lock;		/* protects from here down */
	eventid_t	tun_events;
	t_uscalar_t	tun_state;		/* protected by qwriter */
	t_uscalar_t	tun_ppa;
	mblk_t		*tun_iocmp;
	ipsec_req_t	tun_secinfo;		/* Security preferences. */
	uint_t		tun_flags;
	in6_addr_t	tun_laddr;
	in6_addr_t	tun_faddr;
	uint32_t	tun_mtu;
	uint8_t		tun_encap_lim;
	uint8_t		tun_hop_limit;
	uint32_t	tun_extra_offset;
	union {
		ipha_t	tun_u_ipha;
		ip6_t	tun_u_ip6h;
		double	tun_u_aligner;
	} tun_u;
	dev_t		tun_dev;
#define	tun_ipha		tun_u.tun_u_ipha
#define	tun_ip6h		tun_u.tun_u_ip6h
	tun_stats_t	*tun_stats;
	uint32_t tun_nocanput;		/* # input canput() returned false */
	uint32_t tun_xmtretry;		/* # output canput() returned false */
	uint32_t tun_allocbfail;	/* # esballoc/allocb failed */

	/*
	 *  MIB II variables
	 */
	uint32_t tun_InDiscard;
	uint32_t tun_InErrors;
	uint32_t tun_OutDiscard;
	uint32_t tun_OutErrors;

	uint64_t tun_HCInOctets;	/* # Total Octets received */
	uint64_t tun_HCInUcastPkts;	/* # Packets delivered */
	uint64_t tun_HCInMulticastPkts;	/* # Mulitcast Packets delivered */
	uint64_t tun_HCOutOctets;	/* # Total Octets sent */
	uint64_t tun_HCOutUcastPkts;	/* # Packets requested */
	uint64_t tun_HCOutMulticastPkts; /* Multicast Packets requested */
} tun_t;


/*
 * First 4 bits of flags are used to determine what version of IP is
 * is above the tunnel or below the tunnel
 */

#define	TUN_U_V4	0x01		/* upper protocol is v4 */
#define	TUN_U_V6	0x02		/* upper protocol is v6 */
#define	TUN_L_V4	0x04		/* lower protocol is v4 */
#define	TUN_L_V6	0x08		/* lower protocol is v6 */
#define	TUN_UPPER_MASK	(TUN_U_V4 | TUN_U_V6)
#define	TUN_LOWER_MASK	(TUN_L_V4 | TUN_L_V6)

#define	TUN_UL_IPV4	0x01		/* upper / lower protocol is IPv4 */
#define	TUN_UL_IPV6	0x02		/* upper / lower protocol is IPv6 */

/*
 * tunnel flags
 * TUN_BOUND is set when we get the ok ack back for the T_BIND_REQ
 */
#define	TUN_BOUND		0x010	/* tunnel is bound */
#define	TUN_BIND_SENT		0x020	/* our version of dl pending */
#define	TUN_SRC			0x040	/* Source address set */
#define	TUN_DST			0x080	/* Destination address set */
#define	TUN_AUTOMATIC		0x100	/* tunnel is an automatic tunnel */
#define	TUN_FASTPATH		0x200	/* fastpath has been acked */
#define	TUN_SECURITY		0x400	/* Security properties present */

/* used to mask out upper and lower protocol version bits */
#define	TUN_LTYPE_MASK	0x3
#define	TUN_LMASK_SHIFT	0x0
#define	TUN_UTYPE_MASK	0xc
#define	TUN_UMASK_SHIFT	0x2
#define	TUN_ULMASK	0xf

int	tun_open(queue_t *, dev_t *, int, int, cred_t *);
int	tun_close(queue_t *, int, cred_t *);
void	tun_rput(queue_t *q, mblk_t  *mp);
void	tun_rsrv(queue_t *q);
void	tun_wput(queue_t *q, mblk_t  *mp);
void	tun_wsrv(queue_t *q);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_TUN_H */
