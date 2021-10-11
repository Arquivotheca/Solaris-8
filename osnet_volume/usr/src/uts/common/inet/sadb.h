/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_SADB_H
#define	_INET_SADB_H

#pragma ident	"@(#)sadb.h	1.3	99/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	IPSA_MAX_ADDRLEN 16	/* Maximum address length for an association. */

/*
 * IP security association.  Synchronization assumes 32-bit loads, so
 * the 64-bit quantities can't even be be read w/o locking it down!
 */

typedef struct ipsa_s {
	struct ipsa_s *ipsa_next;	/* Next in hash bucket */
	struct ipsa_s **ipsa_ptpn;	/* Pointer to previous next pointer. */
	kmutex_t *ipsa_linklock;	/* Pointer to hash-chain lock. */
	void (*ipsa_freefunc)(struct ipsa_s *); /* freeassoc function */

	/*
	 * NOTE: I may need more pointers, depending on future SA
	 * requirements.
	 */
	void *ipsa_commonstore;	/* Common storage, to reduce malloc. */
	void *ipsa_authkey;	/* Opaque pointer to authentication key(s). */
	void *ipsa_encrkey;	/* Opaque pointer to encryption key(s). */
	void *ipsa_iv;		/* Opaque pointer to initialization vector. */
	char *ipsa_src_cid;	/* Source certificate identity */
	char *ipsa_dst_cid;	/* Destination certificate identity */
	char *ipsa_proxy_cid;	/* Proxy (for source) agent's cert. id. */
	void *ipsa_authmisc;	/* Authentication algorithm misc. storage. */
	void *ipsa_encrmisc;	/* Encryption algorithm misc. storage. */
	uint64_t *ipsa_integ;	/* Integrity bitmap */
	uint64_t *ipsa_sens;	/* Sensitivity bitmap */

	/*
	 * PF_KEYv2 supports a replay window size of 255.  Hence there is a
	 * need a bit vector to support a replay window of 255.  256 is a nice
	 * round number, so I support that.
	 *
	 * Use an array of uint64_t for best performance on 64-bit
	 * processors.  (And hope that 32-bit compilers can handle things
	 * okay.)  The " >> 6 " is to get the appropriate number of 64-bit
	 * ints.
	 */
#define	SADB_MAX_REPLAY 256	/* Must be 0 mod 64. */
	uint64_t ipsa_replay_arr[SADB_MAX_REPLAY >> 6];

	uint64_t ipsa_unique_id;	/* Non-zero for unique SAs */

	/*
	 * Reference count semantics:
	 *
	 *	An SA has a reference count of 1 if something's pointing
	 *	to it.  This includes being in a hash table.  So if an
	 *	SA is in a hash table, it has a reference count of at least 1.
	 *
	 *	When a ptr. to an IPSA is assigned, you MUST REFHOLD after
	 *	said assignment.  When a ptr. to an IPSA is released
	 *	you MUST REFRELE.  When the refcount hits 0, REFRELE
	 *	will free the IPSA.
	 */
	kmutex_t ipsa_lock;	/* Locks non-linkage/refcnt fields. */
	/* Q:  Since I may be doing refcnts differently, will I need cv? */
	uint_t ipsa_refcnt;	/* Reference count. */

	/*
	 * The following four time fields are the ones monitored by ah_ager()
	 * and esp_ager() respectively.  They are all absolute wall-clock
	 * times.  The times of creation (i.e. add time) and first use are
	 * pretty straightforward.  The soft and hard expire times are
	 * derived from the times of first use and creation, plus the minimum
	 * expiration times in the fields that follow this.
	 *
	 * For example, if I had a hard add time of 30 seconds, and a hard
	 * use time of 15, the ipsa_hardexpiretime would be time of add, plus
	 * 30 seconds.  If I USE the SA such that time of first use plus 15
	 * seconds would be earlier than the add time plus 30 seconds, then
	 * ipsa_hardexpiretime would become this earlier time.
	 */
	time_t ipsa_addtime;	/* Time I was added. */
	time_t ipsa_usetime;	/* Time of my first use. */
	time_t ipsa_softexpiretime;	/* Time of my first soft expire. */
	time_t ipsa_hardexpiretime;	/* Time of my first hard expire. */

	/*
	 * The following fields are directly reflected in PF_KEYv2 LIFETIME
	 * extensions.  The time_ts are in number-of-seconds, and the bytes
	 * are in... bytes.
	 */
	time_t ipsa_softaddlt;	/* Seconds of soft lifetime after add. */
	time_t ipsa_softuselt;	/* Seconds of soft lifetime after first use. */
	time_t ipsa_hardaddlt;	/* Seconds of hard lifetime after add. */
	time_t ipsa_harduselt;	/* Seconds of hard lifetime after first use. */
	uint64_t ipsa_softbyteslt;	/* Bytes of soft lifetime. */
	uint64_t ipsa_hardbyteslt;	/* Bytes of hard lifetime. */
	uint64_t ipsa_bytes;	/* Bytes encrypted/authed by this SA. */

	/*
	 * "Allocations" are a concept mentioned in PF_KEYv2.  We do not
	 * support them, except to record them per the PF_KEYv2 spec.
	 */
	uint_t ipsa_softalloc;	/* Allocations allowed (soft). */
	uint_t ipsa_hardalloc;	/* Allocations allowed (hard). */
	uint_t ipsa_alloc;	/* Allocations made. */

	uint_t ipsa_commonstorelen;	/* Length of common storage. */
	uint_t ipsa_authkeylen;	/* Length of auth. key, in bytes, for */
				/* allocation and deallocation purposes. */
	uint_t ipsa_authkeybits; /* ... in bits, for precision reporting. */
	uint_t ipsa_encrkeylen;	/* Length of encr. key, in bytes. */
	uint_t ipsa_encrkeybits; /* ... in bits, for precision reporting. */
	uint_t ipsa_ivlen;	/* IV length. */
	uint_t ipsa_authmisclen; /* Length of auth. misc. data, in bytes. */
	uint_t ipsa_encrmisclen; /* Length of encr. misc. data, in bytes. */
	uint_t ipsa_integlen;	/* Length of the integrity bitmap (bytes). */
	uint_t ipsa_senslen;	/* Length of the sensitivity bitmap (bytes). */
	/*
	 * Certificate ID strings are guaranteed by keysock.c to be null-
	 * terminated, so lengths aren't needed.  But certificate ID types are
	 * needed.
	 */
	uint_t ipsa_scid_type;	/* Source CID type. */
	uint_t ipsa_dcid_type;	/* Destination CID type. */
	uint_t ipsa_pcid_type;	/* Proxy CID type. */

	uint_t ipsa_type;	/* Type of security association. (AH/etc.) */
	uint_t ipsa_dpd;	/* Domain for sensitivity bit vectors. */
	uint_t ipsa_senslevel;	/* Sensitivity level. */
	uint_t ipsa_integlevel;	/* Integrity level. */
	uint_t ipsa_state;	/* State of my association. */
	uint_t ipsa_encr_alg;	/* Algorithm identifier for an ESP SA. */
	uint_t ipsa_auth_alg;	/* Auth. algorithm for AH, or ESP. */
	uint_t ipsa_replay_wsize; /* Size of replay window */
	uint32_t ipsa_flags;	/* Flags for security association. */
	uint32_t ipsa_spi;	/* Security parameters index. */
	uint32_t ipsa_replay;	/* Highest seen replay value for this SA. */

	boolean_t ipsa_haspeer;	/* Has peer in another table. */

	/*
	 * Address storage.
	 * The source address can be INADDR_ANY, IN6ADDR_ANY, etc.
	 */
	int ipsa_addrlen;

	uint8_t ipsa_srcaddr[IPSA_MAX_ADDRLEN];
	uint8_t ipsa_dstaddr[IPSA_MAX_ADDRLEN];
	uint8_t ipsa_proxyaddr[IPSA_MAX_ADDRLEN];

	/* MLS boxen will probably need more fields in here. */

} ipsa_t;

/*
 * ipsa_t reference hold/release macros.
 *
 * If you have a pointer, you REFHOLD.  If you are releasing a pointer, you
 * REFRELE.  An ipsa_t that is newly inserted into the table should have
 * a reference count of 1 (for the table's pointer), plus 1 more for every
 * pointer that is referencing the ipsa_t.
 */

#define	IPSA_REFHOLD(ipsa) {			\
	ASSERT(((ipsa)->ipsa_linklock) != NULL);	\
	ASSERT(MUTEX_HELD((ipsa)->ipsa_linklock)); \
	mutex_enter(&((ipsa)->ipsa_lock));	\
	((ipsa)->ipsa_refcnt)++;		\
	ASSERT(((ipsa)->ipsa_refcnt) != 0);	\
	mutex_exit(&((ipsa)->ipsa_lock));	\
}

#define	IPSA_REFRELE(ipsa) {			\
	mutex_enter(&((ipsa)->ipsa_lock));	\
	ASSERT(((ipsa)->ipsa_refcnt) != 0);	\
	((ipsa)->ipsa_refcnt)--;		\
	if (((ipsa)->ipsa_refcnt) == 0)		\
		((ipsa)->ipsa_freefunc)(ipsa);	\
	else					\
		mutex_exit(&((ipsa)->ipsa_lock));	\
}

#define	IPSA_F_PFS	SADB_SAFLAGS_PFS	/* PFS in use for this SA? */
#define	IPSA_F_NOREPFLD	SADB_SAFLAGS_NOREPLAY	/* No replay field, for */
						/* backward compat. */
#define	IPSA_F_USED	SADB_X_SAFLAGS_USED	/* SA has been used. */
#define	IPSA_F_UNIQUE	SADB_X_SAFLAGS_UNIQUE	/* SA is unique */
#define	IPSA_F_AALG1	SADB_X_SAFLAGS_AALG1	/* Auth alg flag 1 */
#define	IPSA_F_AALG2	SADB_X_SAFLAGS_AALG2	/* Auth alg flag 2 */
#define	IPSA_F_EALG1	SADB_X_SAFLAGS_EALG1	/* Encrypt alg flag 1 */
#define	IPSA_F_EALG2	SADB_X_SAFLAGS_EALG2	/* Encrypt alg flag 2 */

/* SA states are important for handling UPDATE PF_KEY messages. */
#define	IPSA_STATE_LARVAL	SADB_SASTATE_LARVAL
#define	IPSA_STATE_MATURE	SADB_SASTATE_MATURE
#define	IPSA_STATE_DYING	SADB_SASTATE_DYING
#define	IPSA_STATE_DEAD		SADB_SASTATE_DEAD

/*
 * NOTE:  If the document authors do things right in defining algorithms, we'll
 *	  probably have flags for what all is here w.r.t. replay, ESP w/HMAC,
 *	  etc.
 */

#define	IPSA_T_ACQUIRE	SEC_TYPE_NONE	/* If this typed returned, sa needed */
#define	IPSA_T_AH	SEC_TYPE_AH	/* IPsec AH association */
#define	IPSA_T_ESP	SEC_TYPE_ESP	/* IPsec ESP association */

#define	IPSA_AALG_NONE	SADB_AALG_NONE		/* No auth. algorithm */
#define	IPSA_AALG_MD5H	SADB_AALG_MD5HMAC	/* MD5-HMAC algorithm */
#define	IPSA_AALG_SHA1H	SADB_AALG_SHA1HMAC	/* SHA1-HMAC algorithm */

#define	IPSA_EALG_NONE		SADB_EALG_NONE	/* No encryption algorithm */
#define	IPSA_EALG_DES_CBC	SADB_EALG_DESCBC
#define	IPSA_EALG_3DES		SADB_EALG_3DESCBC

/*
 * Protect each ipsa_t bucket (and linkage) with a lock.
 */

typedef struct isaf_s {
	ipsa_t *isaf_ipsa;
	kmutex_t isaf_lock;
} isaf_t;

/*
 * ACQUIRE record.  If AH/ESP/whatever cannot find an association for outbound
 * traffic, it sends up an SADB_ACQUIRE message and create an ACQUIRE record.
 */

#define	IPSACQ_MAXPACKETS 4	/* Number of packets that can be queued up */
				/* waiting for an ACQUIRE to finish. */

typedef struct ipsacq_s {
	struct ipsacq_s *ipsacq_next;
	struct ipsacq_s **ipsacq_ptpn;
	kmutex_t *ipsacq_linklock;

	int ipsacq_addrlen;		/* Length of addresses. */
	int ipsacq_numpackets;		/* How many packets queued up so far. */
	uint32_t ipsacq_seq;		/* PF_KEY sequence number. */
	uint64_t ipsacq_unique_id;	/* Unique ID for SAs that need it. */
	uint_t ipsacq_encr_alg;		/* Requested authentication algorithm */
	uint_t ipsacq_auth_alg;		/* Requested encryption algorithm. */

	kmutex_t ipsacq_lock;	/* Protects non-linkage fields. */
	time_t ipsacq_expire;	/* Wall-clock time when this record expires. */
	mblk_t *ipsacq_mp;	/* List of datagrams waiting for an SA. */

	/* These two point inside the last mblk inserted. */
	uint8_t *ipsacq_srcaddr;
	uint8_t *ipsacq_dstaddr;

	uint8_t ipsacq_proxyaddr[IPSA_MAX_ADDRLEN];	/* For later */

	/* These may change per-acquire. */
	uint16_t ipsacq_srcport;
	uint16_t ipsacq_dstport;
	uint8_t ipsacq_proto;
} ipsacq_t;

/*
 * Kernel-generated sequence numbers will be no less than 0x8000000 to
 * forestall any cretinous problems with manual keying accidentally updating
 * an ACQUIRE entry.
 */
#define	IACQF_LOWEST_SEQ 0x80000000

/*
 * ACQUIRE fanout.  Protect each linkage with a lock.
 */

typedef struct iacqf_s {
	ipsacq_t *iacqf_ipsacq;
	kmutex_t iacqf_lock;
} iacqf_t;

/*
 * All functions that return an ipsa_t will return it with IPSA_REFHOLD()
 * already called.
 */

/* SA retrieval (inbound and outbound) */
ipsa_t *sadb_getassocbyspi(isaf_t *, uint32_t, uint8_t *, uint8_t *, int);
ipsa_t *sadb_getassocbyipc(isaf_t *, ipsec_out_t *, uint8_t *, uint8_t *, int,
    uint8_t);

/* SA insertion and deletion. */
int sadb_insertassoc(ipsa_t *, isaf_t *);
void sadb_unlinkassoc(ipsa_t *);

/* SA table construction and destruction. */
void sadb_init(isaf_t *, uint_t);
void sadb_destroy(isaf_t *, uint_t);

/* Support routines to interface a keysock consumer to PF_KEY. */
boolean_t sadb_hardsoftchk(sadb_lifetime_t *, sadb_lifetime_t *);
void sadb_pfkey_echo(queue_t *, mblk_t *, sadb_msg_t *, struct keysock_in_s *,
    ipsa_t *);
void sadb_pfkey_error(queue_t *, mblk_t *, int, uint_t);
void sadb_keysock_hello(queue_t **, queue_t *, mblk_t *, void (*)(void *),
    timeout_id_t *, int);
int sadb_addrcheck(queue_t *, queue_t *, mblk_t *, sadb_ext_t *, uint_t);
int sadb_common_add(queue_t *, mblk_t *, sadb_msg_t *, keysock_in_t *,
    isaf_t *, isaf_t *, isaf_t *, ipsa_t *, boolean_t);
void sadb_set_usetime(ipsa_t *);
boolean_t sadb_age_bytes(queue_t *, ipsa_t *, uint64_t, boolean_t);
void sadb_age_larval(ipsa_t *, time_t);
ipsa_t *sadb_age_assoc(queue_t *, ipsa_t *, time_t, int);
void sadb_update_assoc(ipsa_t *, sadb_lifetime_t *, sadb_lifetime_t *);
ipsacq_t *sadb_new_acquire(iacqf_t *, uint32_t, mblk_t *, int, uint_t, int);
void sadb_destroy_acquire(ipsacq_t *);
void sadb_destroy_acqlist(iacqf_t *, uint_t, boolean_t);
void sadb_setup_acquire(sadb_msg_t *, ipsacq_t *);
ipsa_t *sadb_getspi(keysock_in_t *, uint32_t);
void sadb_in_acquire(sadb_msg_t *, iacqf_t *, iacqf_t *, int, queue_t *);
boolean_t sadb_replay_check(ipsa_t *, uint32_t);
boolean_t sadb_replay_peek(ipsa_t *, uint32_t);
mblk_t *sadb_sa2msg(ipsa_t *, sadb_msg_t *);
int sadb_dump(queue_t *, mblk_t *, minor_t, isaf_t *, int, boolean_t);
void sadb_flush(isaf_t *, int);
void sadb_replay_delete(ipsa_t *);
void sadb_get_random_bytes(void *, size_t);

/*
 * One IPsec -> IP linking routine, and one IPsec rate-limiting routine.
 */
boolean_t sadb_t_bind_req(queue_t *, int);
void ipsec_rl_strlog(short mid, short sid, char level, ushort_t sl,
    char *fmt, ...);

/*
 * External AH/ESP get association functions for IP.  Stubbed in modstubs.s.
 */
extern ipsa_t	*getahassoc(mblk_t *, uint8_t *, uint8_t *, int);
extern ipsa_t	*getespassoc(mblk_t *, uint8_t *, uint8_t *, int);

#ifdef	__cplusplus
}
#endif

#endif /* _INET_SADB_H */
