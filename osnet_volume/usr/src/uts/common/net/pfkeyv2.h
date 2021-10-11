/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_NET_PFKEYV2_H
#define	_NET_PFKEYV2_H

#pragma ident	"@(#)pfkeyv2.h	1.1	98/12/16 SMI"

/*
 * Definitions and structures for PF_KEY version 2.  See RFC 2367 for
 * more details.  SA == Security Association, which is what PF_KEY provides
 * an API for managing.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	PF_KEY_V2		2
#define	PFKEYV2_REVISION	199806L

/*
 * Base PF_KEY message.
 */

typedef struct sadb_msg {
	uint8_t sadb_msg_version;	/* Version, currently PF_KEY_V2 */
	uint8_t sadb_msg_type;		/* ADD, UPDATE, etc. */
	uint8_t sadb_msg_errno;		/* Error number from UNIX errno space */
	uint8_t sadb_msg_satype;	/* ESP, AH, etc. */
	uint16_t sadb_msg_len;		/* Length in 64-bit words. */
	uint16_t sadb_msg_reserved;	/* must be zero */
	uint32_t sadb_msg_seq;		/* Set by message originator */
	uint32_t sadb_msg_pid;		/* Set by message originator */
} sadb_msg_t;

/*
 * Generic extension header.
 */

typedef struct sadb_ext {
	uint16_t sadb_ext_len;		/* In 64-bit words, inclusive */
	uint16_t sadb_ext_type;		/* 0 is reserved */
} sadb_ext_t;

/*
 * Security Association information extension.
 */

typedef struct sadb_sa {
	uint16_t sadb_sa_len;
	uint16_t sadb_sa_exttype;	/* ASSOCIATION */
	uint32_t sadb_sa_spi;		/* Security Parameters Index value */
	uint8_t sadb_sa_replay;		/* Replay counter */
	uint8_t sadb_sa_state;		/* MATURE, DEAD, DYING, LARVAL */
	uint8_t sadb_sa_auth;		/* Authentication algorithm */
	uint8_t sadb_sa_encrypt;	/* Encryption algorithm */
	uint32_t sadb_sa_flags;		/* SA flags. */
} sadb_sa_t;

/*
 * SA Lifetime extension.
 */

typedef struct sadb_lifetime {
	uint16_t sadb_lifetime_len;
	uint16_t sadb_lifetime_exttype;		/* SOFT, HARD, CURRENT */
	uint32_t sadb_lifetime_allocations;
	uint64_t sadb_lifetime_bytes;
	uint64_t sadb_lifetime_addtime;	/* These fields are assumed to hold */
	uint64_t sadb_lifetime_usetime;	/* >= sizeof (time_t). */
} sadb_lifetime_t;

/*
 * SA address information.
 */

typedef struct sadb_address {
	uint16_t sadb_address_len;
	uint16_t sadb_address_exttype;		/* SRC, DST, PROXY */
	uint8_t sadb_address_proto;		/* Proto for ports... */
	uint8_t sadb_address_prefixlen;		/* Prefix length. */
	uint16_t sadb_address_reserved;		/* Padding */
	/* Followed by a sockaddr structure which may contain ports. */
} sadb_address_t;

/*
 * SA key information.
 */

typedef struct sadb_key {
	uint16_t sadb_key_len;
	uint16_t sadb_key_exttype;		/* AUTH, ENCRYPT */
	uint16_t sadb_key_bits;			/* Actual key length in bits */
	uint16_t sadb_key_reserved;
	/* Followed by actual key(s) in canonical (outbound proc.) order. */
} sadb_key_t;

/*
 * SA Identity information.
 */

typedef struct sadb_ident {
	uint16_t sadb_ident_len;
	uint16_t sadb_ident_exttype;	/* SRC, DST, PROXY */
	uint16_t sadb_ident_type;	/* FQDN, USER_FQDN, etc. */
	uint16_t sadb_ident_reserved;	/* Padding */
	uint64_t sadb_ident_id;		/* For userid, etc. */
	/* Followed by an identity null-terminate C string if present. */
} sadb_ident_t;

/*
 * SA sensitivity information.  This is mostly useful on MLS systems.
 */

typedef struct sadb_sens {
	uint16_t sadb_sens_len;
	uint16_t sadb_sens_exttype;		/* SENSITIVITY */
	uint32_t sadb_sens_dpd;			/* Protection domain */
	uint8_t sadb_sens_sens_level;
	uint8_t sadb_sens_sens_len;		/* 64-bit words */
	uint8_t sadb_sens_integ_level;
	uint8_t sadb_sens_integ_len;		/* 64-bit words */
	uint32_t sadb_sens_reserved;
	/*
	 * followed by two uint64_t arrays
	 * uint64_t sadb_sens_bitmap[sens_bitmap_len];
	 * uint64_t sadb_integ_bitmap[integ_bitmap_len];
	 */
} sadb_sens_t;

/*
 * A proposal extension.  This is found in an ACQUIRE message, and it
 * proposes what sort of SA the kernel would like to ACQUIRE.
 */

typedef struct sadb_prop {
	uint16_t sadb_prop_len;
	uint16_t sadb_prop_exttype;	/* PROPOSAL */
	uint8_t sadb_prop_replay;	/* Replay win. size. */
	uint8_t sadb_prop_reserved[3];
	/* Followed by sadb_comb[] array. */
} sadb_prop_t;

/*
 * This is a proposed combination.  Many of these can follow a proposal
 * extension.
 */

typedef struct sadb_comb {
	uint8_t sadb_comb_auth;			/* Authentication algorithm */
	uint8_t sadb_comb_encrypt;		/* Encryption algorithm */
	uint16_t sadb_comb_flags;		/* Comb. flags (e.g. PFS) */
	uint16_t sadb_comb_auth_minbits;	/* Bit strengths for auth */
	uint16_t sadb_comb_auth_maxbits;
	uint16_t sadb_comb_encrypt_minbits;	/* Bit strengths for encrypt */
	uint16_t sadb_comb_encrypt_maxbits;
	uint32_t sadb_comb_reserved;
	uint32_t sadb_comb_soft_allocations;	/* Lifetime proposals for */
	uint32_t sadb_comb_hard_allocations;	/* this combination. */
	uint64_t sadb_comb_soft_bytes;
	uint64_t sadb_comb_hard_bytes;
	uint64_t sadb_comb_soft_addtime;
	uint64_t sadb_comb_hard_addtime;
	uint64_t sadb_comb_soft_usetime;
	uint64_t sadb_comb_hard_usetime;
} sadb_comb_t;

/*
 * When key mgmt. registers with the kernel, the kernel will tell key mgmt.
 * its supported algorithms.
 */

typedef struct sadb_supported {
	uint16_t sadb_supported_len;
	uint16_t sadb_supported_exttype;
	uint32_t sadb_supported_reserved;
} sadb_supported_t;

typedef struct sadb_alg {
	uint8_t sadb_alg_id;		/* Algorithm type. */
	uint8_t sadb_alg_ivlen;		/* IV len, in bits */
	uint16_t sadb_alg_minbits;	/* Min. key len (in bits) */
	uint16_t sadb_alg_maxbits;	/* Max. key length */
	uint16_t sadb_alg_reserved;
} sadb_alg_t;

/*
 * If key mgmt. needs an SPI in a range (including 0 to 0xFFFFFFFF), it
 * asks the kernel with this extension in the SADB_GETSPI message.
 */

typedef struct sadb_spirange {
	uint16_t sadb_spirange_len;
	uint16_t sadb_spirange_exttype;	/* SPI_RANGE */
	uint32_t sadb_spirange_min;
	uint32_t sadb_spirange_max;
	uint32_t sadb_spirange_reserved;
} sadb_spirange_t;

/*
 * Base message types.
 */

#define	SADB_RESERVED	0
#define	SADB_GETSPI	1
#define	SADB_UPDATE	2
#define	SADB_ADD	3
#define	SADB_DELETE	4
#define	SADB_GET	5
#define	SADB_ACQUIRE	6
#define	SADB_REGISTER	7
#define	SADB_EXPIRE	8
#define	SADB_FLUSH	9
#define	SADB_DUMP	10   /* not used normally */
#define	SADB_X_PROMISC	11
#define	SADB_X_PCHANGE	12

#define	SADB_MAX	12

/*
 * SA flags
 */

#define	SADB_SAFLAGS_PFS	0x1	/* Perfect forward secrecy? */
#define	SADB_SAFLAGS_NOREPLAY	0x2	/* Replay field NOT PRESENT. */

/* Below flags are used by this implementation.  Grow from left-to-right. */
#define	SADB_X_SAFLAGS_USED	0x80000000	/* SA used/not used */
#define	SADB_X_SAFLAGS_UNIQUE	0x40000000	/* SA unique/reusable */
#define	SADB_X_SAFLAGS_AALG1	0x20000000	/* Auth-alg specific flag 1 */
#define	SADB_X_SAFLAGS_AALG2	0x10000000	/* Auth-alg specific flag 2 */
#define	SADB_X_SAFLAGS_EALG1	 0x8000000	/* Encr-alg specific flag 1 */
#define	SADB_X_SAFLAGS_EALG2	 0x4000000	/* Encr-alg specific flag 2 */

/*
 * SA state.
 */

#define	SADB_SASTATE_LARVAL	  0
#define	SADB_SASTATE_MATURE	  1
#define	SADB_SASTATE_DYING	  2
#define	SADB_SASTATE_DEAD	  3

#define	SADB_SASTATE_MAX	  3

/*
 * SA type.  Gaps are present in the number space because (for the time being)
 * these types correspond to the SA types in the IPsec DOI document.
 */

#define	SADB_SATYPE_UNSPEC	0
#define	SADB_SATYPE_AH		2  /* RFC-1826 */
#define	SADB_SATYPE_ESP		3  /* RFC-1827 */
#define	SADB_SATYPE_RSVP	5  /* RSVP Authentication */
#define	SADB_SATYPE_OSPFV2	6  /* OSPFv2 Authentication */
#define	SADB_SATYPE_RIPV2	7  /* RIPv2 Authentication */
#define	SADB_SATYPE_MIP		8  /* Mobile IPv4 Authentication */

#define	SADB_SATYPE_MAX		8

/*
 * Algorithm types.  Gaps are present because (for the time being) these types
 * correspond to the SA types in the IPsec DOI document.
 *
 * NOTE:  These are numbered to play nice with the IPsec DOI.  That's why
 *	  there are gaps.
 */

/* Authentication algorithms */
#define	SADB_AALG_NONE		0
#define	SADB_AALG_MD5HMAC	2
#define	SADB_AALG_SHA1HMAC	3

#define	SADB_AALG_MAX		3

/* Encryption algorithms */
#define	SADB_EALG_NONE		0
#define	SADB_EALG_DESCBC	2
#define	SADB_EALG_3DESCBC	3
#define	SADB_EALG_NULL		11
#define	SADB_EALG_MAX		11

/*
 * Extension header values.
 */

#define	SADB_EXT_RESERVED		0

#define	SADB_EXT_SA			1
#define	SADB_EXT_LIFETIME_CURRENT	2
#define	SADB_EXT_LIFETIME_HARD		3
#define	SADB_EXT_LIFETIME_SOFT		4
#define	SADB_EXT_ADDRESS_SRC		5
#define	SADB_EXT_ADDRESS_DST		6
#define	SADB_EXT_ADDRESS_PROXY		7
#define	SADB_EXT_KEY_AUTH		8
#define	SADB_EXT_KEY_ENCRYPT		9
#define	SADB_EXT_IDENTITY_SRC		10
#define	SADB_EXT_IDENTITY_DST		11
#define	SADB_EXT_SENSITIVITY		12
#define	SADB_EXT_PROPOSAL		13
#define	SADB_EXT_SUPPORTED_AUTH		14
#define	SADB_EXT_SUPPORTED_ENCRYPT	15
#define	SADB_EXT_SPIRANGE		16

#define	SADB_EXT_MAX			16

/*
 * Identity types.
 */

#define	SADB_IDENTTYPE_RESERVED 0

#define	SADB_IDENTTYPE_PREFIX		1
#define	SADB_IDENTTYPE_FQDN		2  /* Fully qualified domain name. */
#define	SADB_IDENTTYPE_USER_FQDN	3  /* e.g. root@domain.com */

#define	SADB_IDENTTYPE_MAX 	3

/*
 * Protection DOI values for the SENSITIVITY extension.  There are no values
 * currently, so the MAX is the only non-zero value available.
 */

#define	SADB_DPD_NONE	0

#define	SADB_DPD_MAX	1

/*
 * Handy conversion macros.  Not part of the PF_KEY spec...
 */

#define	SADB_64TO8(x)	((x) << 3)
#define	SADB_8TO64(x)	((x) >> 3)
#define	SADB_8TO1(x)	((x) << 3)
#define	SADB_1TO8(x)	((x) >> 3)

#ifdef	__cplusplus
}
#endif

#endif	/* _NET_PFKEYV2_H */
