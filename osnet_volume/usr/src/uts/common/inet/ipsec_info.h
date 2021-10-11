/*
 * Copyright (c) 1997-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IPSEC_INFO_H
#define	_INET_IPSEC_INFO_H

#pragma ident	"@(#)ipsec_info.h	1.1	98/12/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * IPsec informational messages.  These are M_CTL STREAMS messages, which
 * convey IPsec information between various IP and related modules.  The
 * messages come in a few flavors:
 *
 *	* IPSEC_{IN,OUT}  -  These show what IPsec action have been taken (for
 *	  inbound datagrams), or need to be taken (for outbound datagrams).
 *	  They flow between AH/ESP and IP.
 *
 *	* Keysock consumer interface  -  These messages are wrappers for
 *	  PF_KEY messages.  They flow between AH/ESP and keysock.
 *
 *	* Authentication Provider Interface (AUTH PI)  -  These messages
 *	  tie AH/ESP to their authentication modules.
 *
 */


/*
 * The IPsec M_CTL value MUST be something that will not be even close
 * to an IPv4 or IPv6 header.  This means the first byte must not be
 * 0x40 - 0x4f or 0x60-0x6f.  For big-endian machines, this is fixable with
 * the IPSEC_M_CTL prefix.  For little-endian machines, the actual M_CTL
 * _type_ must not be in the aforementioned ranges.
 *
 * The reason for this avoidance is because M_CTL's with a real IPv4/IPv6
 * datagram get sent from to TCP or UDP when an ICMP datagram affects a
 * TCP/UDP session.
 */

#define	IPSEC_M_CTL	(('!' << 24) + ('@' << 16) + ('@' << 8))

/*
 * M_CTL types for IPsec messages.  Remember, the values 0x40 - 0x4f and 0x60
 * - 0x6f are not to be used because of potential little-endian confusion.
 */

/*
 * IPSEC_{IN,OUT} policy expressors.
 */
#define	IPSEC_IN	(IPSEC_M_CTL + 1)
#define	IPSEC_OUT	(IPSEC_M_CTL + 2)

/*
 * This is used for communication between IP and IPSEC (AH/ESP)
 * for Inbound datagrams. IPSEC_IN is allocated by IP before IPSEC
 * processing begins. On return spi fields are initialized so that
 * IP can locate the security associations later on for doing policy
 * checks. For loopback case, IPSEC processing is not done. But the
 * attributes of the security are reflected in <foo>_done fields below.
 * The code in policy check infers that it is a loopback case and
 * would not try to get the associations.
 */
typedef struct ipsec_in_s {
	uint32_t ipsec_in_type;
	uint32_t ipsec_in_len;
	uint32_t ipsec_in_ah_spi;	/* AH SPI used for the packet */
	uint32_t ipsec_in_esp_spi;	/* SPI for ESP */
	unsigned int
		ipsec_in_secure : 1,	/* Is the message attached secure ? */
		ipsec_in_v4 : 1,	/* Is this an ipv4 packet ? */
		ipsec_in_loopback : 1,	/* Is this a loopback request ? */
		ipsec_in_dont_check : 1, /* Used by TCP to avoid policy check */

		ipsec_in_decaps : 1,	/* Was this packet decapsulated from */
					/* a matching inner packet? */

		ipsec_in_pad_bits : 27;

	uint_t ipsec_in_ah_done;	/* Loopback : AH operation done ? */
	uint_t ipsec_in_esp_done;	/* Loopback : ESP operation done ? */
	uint_t ipsec_in_self_encap_done; /* Loopback : Self-Encap done ? */
	int    ipsec_in_ill_index;	 /* Incoming packet's interface */
} ipsec_in_t;

/*
 * This is used for communication between IP and IPSEC (AH/ESP)
 * for Outbound datagrams. IPSEC_OUT is allocated by IP before IPSEC
 * processing begins. On return spi fields are initialized so that
 * IP can locate the security associations later on for doing policy
 * checks. <foo>_id fields are used to pass in the unique information
 * to the IPSEC code. <foo>_req fields reflects the client's security
 * options.
 */
typedef struct ipsec_out_s {
	uint32_t ipsec_out_type;
	uint32_t ipsec_out_len;
	uint_t ipsec_out_ah_req;	/* AH operation  required ? */
	uint_t ipsec_out_ah_alg;	/* AH algorithm */
	uint_t ipsec_out_esp_req;	/* ESP operation required ? */
	uint_t ipsec_out_esp_alg;	/* ESP algorithm */
	uint_t ipsec_out_esp_ah_alg;	/* Auth algorithm used with ESP */
	uint_t ipsec_out_self_encap_req; /* Self-Encapsulation required */
	/*
	 * NOTE: "Source" and "Dest" are w.r.t. outbound datagrams.  Ports can
	 *	 be zero, and the protocol number is needed to make the ports
	 *	 significant.
	 */
	uint16_t ipsec_out_src_port;	/* Source port number of d-gram. */
	uint16_t ipsec_out_dst_port;	/* Destination port number of d-gram. */
	uint32_t ipsec_out_ah_spi;	/* AH SPI used for the packet */
	uint32_t ipsec_out_esp_spi;	/* SPI for ESP */
	uint_t ipsec_out_ill_index;	/* ill index used for multicast */
	uint8_t ipsec_out_proto;	/* IP protocol number for d-gram. */
	unsigned int
		ipsec_out_encaps : 1,	/* Encapsualtion done ? */
		ipsec_out_use_global_policy : 1, /* Inherit global policy ? */
		ipsec_out_secure : 1,	/* Is this secure ? */
		ipsec_out_proc_begin : 1, /* IPSEC processing begun */
		/*
		 * Following three values reflects the values stored
		 * in ipc.
		 */
		ipsec_out_multicast_loop : 1,
		ipsec_out_dontroute : 1,
		ipsec_out_policy_cached : 1,

		ipsec_out_pad_bits : 25;
} ipsec_out_t;

/*
 * This is used to mark the ipsec_out_t *req* fields
 * when the operation is done without affecting the
 * requests.
 */
#define	IPSEC_REQ_DONE		0x80000000
/*
 * Operation could not be performed by the AH/ESP
 * module.
 */
#define	IPSEC_REQ_FAILED	0x40000000

/*
 * Keysock consumer interface.
 *
 * The driver/module keysock (which is a driver to PF_KEY sockets, but is
 * a module to 'consumers' like AH and ESP) uses keysock consumer interface
 * messages to pass on PF_KEY messages to consumers who process and act upon
 * them.
 */
#define	KEYSOCK_IN		(IPSEC_M_CTL + 3)
#define	KEYSOCK_OUT		(IPSEC_M_CTL + 4)
#define	KEYSOCK_OUT_ERR		(IPSEC_M_CTL + 5)
#define	KEYSOCK_HELLO		(IPSEC_M_CTL + 6)
#define	KEYSOCK_HELLO_ACK	(IPSEC_M_CTL + 7)

/*
 * KEYSOCK_HELLO is sent by keysock to a consumer when it is pushed on top
 * of one (i.e. opened as a module).
 *
 * NOTE: Keysock_hello is simply an ipsec_info_t
 */

/*
 * KEYSOCK_HELLO_ACK is sent by a consumer to acknowledge a KEYSOCK_HELLO.
 * It contains the PF_KEYv2 sa_type, so keysock can redirect PF_KEY messages
 * to the right consumer.
 */
typedef struct keysock_hello_ack_s {
	uint32_t ks_hello_type;
	uint32_t ks_hello_len;
	uint8_t ks_hello_satype;	/* PF_KEYv2 sa_type of ks client */
} keysock_hello_ack_t;

#define	KS_IN_ADDR_UNKNOWN 0
#define	KS_IN_ADDR_NOTTHERE 1
#define	KS_IN_ADDR_UNSPEC 2
#define	KS_IN_ADDR_ME 3
#define	KS_IN_ADDR_NOTME 4
#define	KS_IN_ADDR_MBCAST 5

/*
 * KEYSOCK_IN is a PF_KEY message from a PF_KEY socket destined for a consumer.
 */
typedef struct keysock_in_s {
	uint32_t ks_in_type;
	uint32_t ks_in_len;
	/*
	 * NOTE:	These pointers MUST be into the M_DATA that follows
	 *		this M_CTL message.  If they aren't, weirdness
	 *		results.
	 */
	struct sadb_ext *ks_in_extv[SADB_EXT_MAX + 1];
	int ks_in_srctype;	/* Source address type. */
	int ks_in_dsttype;	/* Dest address type. */
	int ks_in_proxytype;	/* Proxy address type. */
	minor_t ks_in_serial;	/* Serial # of sending socket. */
} keysock_in_t;

/*
 * KEYSOCK_OUT is a PF_KEY message from a consumer destined for a PF_KEY
 * socket.
 */
typedef struct keysock_out_s {
	uint32_t ks_out_type;
	uint32_t ks_out_len;
	minor_t ks_out_serial;	/* Serial # of sending socket. */
} keysock_out_t;

/*
 * KEYSOCK_OUT_ERR is sent to a consumer from keysock if for some reason
 * keysock could not find a PF_KEY socket to deliver a consumer-originated
 * message (e.g. SADB_ACQUIRE).
 */
typedef struct keysock_out_err_s {
	uint32_t ks_err_type;
	uint32_t ks_err_len;
	minor_t ks_err_serial;
	int ks_err_errno;
	/*
	 * Other, richer error information may end up going here eventually.
	 */
} keysock_out_err_t;

/*
 * Authentication Provider Interface (AUTH PI).
 *
 * AH and ESP use the AUTH PI to communicate with authentication algorithms.
 * (E.g. HMAC-MD5, HMAC-SHA-1.)
 */
#define	AUTH_PI_HELLO		(IPSEC_M_CTL + 8)
#define	AUTH_PI_ACK		(IPSEC_M_CTL + 9)
#define	AUTH_PI_OUT_AUTH_REQ	(IPSEC_M_CTL + 10)
#define	AUTH_PI_OUT_AUTH_ACK	(IPSEC_M_CTL + 11)
#define	AUTH_PI_IN_AUTH_REQ	(IPSEC_M_CTL + 12)
#define	AUTH_PI_IN_AUTH_ACK	(IPSEC_M_CTL + 13)
#define	AUTH_PI_KEY_CHECK	(IPSEC_M_CTL + 14)
#define	AUTH_PI_KEY_ACK		(IPSEC_M_CTL + 15)

/*
 * When an authentication algorithm is pushed over AH or ESP, it sends down
 * an AUTH_PI_HELLO to inform the consumer about its properties (e.g. key size,
 * digest size, etc.).
 */
typedef struct auth_pi_hello_s {
	uint32_t auth_hello_type;
	uint32_t auth_hello_len;
	/*
	 * NOTE:  The following fields roughly correspond to a PF_KEYv2
	 * sadb_alg structure.
	 */
	uint8_t auth_hello_id;	/* Algorithm id, from PF_KEYv2 space. */
	uint8_t auth_hello_iv;	/* IV length, probably unused. */
	uint16_t auth_hello_minbits;	/* Minimum key size in bits. */
	uint16_t auth_hello_maxbits;	/* Maximum key size in bits. */
	/* auth_hello_datalen corresponds to a 'reserved' field in PF_KEYv2. */
	uint16_t auth_hello_datalen;	/* Auth. result length in bytes. */
					/* If 0, check AUTH_ACK. */

	boolean_t auth_hello_keycheck;	/* Do I need a key check? */

	/*
	 * The number of keys may not be needed.  The above "key size" may
	 * be the required amount to represent the appropriate number of
	 * keys.  This field is presented for consistency with a design
	 * document.
	 */
	uint_t auth_hello_numkeys;
} auth_pi_hello_t;

/*
 * The consumer will send and AUTH_PI_ACK in response to an AUTH_PI_HELLO.
 *
 * NOTE: AUTH_PI_ACK is just an ipsec_info_t.
 */

/*
 * The following structure is used for both IN and OUT authentication requests.
 *
 * Auth requests have four mblks chained:
 *	AUTH_REQ -> IPSEC_{IN,OUT} -> Pseudo-hdr -> Actual Message
 *
 * The Pseudo-hdr is a "zeroed out" part.  The actual message contains
 * the rest of the data.  The IPSEC_{OUT,IN} is there because they need to
 * be preserved.  The IPSEC_IN and IPSEC_OUT are updated after the
 * authentication algorithm runs.
 */
typedef struct auth_req_s {
	uint32_t auth_req_type;
	uint32_t auth_req_len;
	uint_t auth_req_preahlen;	/* AH insert point, echoed in ACK */
	uint_t auth_req_startoffset;	/* If 0, use pseudo-hdr length. */
	struct ipsa_s *auth_req_assoc;	/* SA for this request. */
} auth_req_t;

/*
 * Default maximum authentication data size.  If an AUTH PI module generates
 * more than this much, it'll have to allocate a larger mblk.  This number
 * should be big enough such that auth_ack_t is the largest (or tied for the
 * largest) ipsec_info_t union member.
 */
#define	AUTH_PI_MAX 24

/*
 * The authentication algorithm sends back an AUTH_ACK.  This message
 * contains the same mblk chain as the AUTH_REQ.
 */
typedef struct auth_ack_s {
	uint32_t auth_ack_type;
	uint32_t auth_ack_len;
	uint_t auth_ack_preahlen;	/* Echoed back from REQ. */
	uint_t auth_ack_datalen;	/* Authentication data length. */
	struct ipsa_s *auth_ack_assoc;	/* SA for this ack. */
	uint8_t auth_ack_data[AUTH_PI_MAX]; /* Authentication data is here. */
} auth_ack_t;

/*
 * The "weak key" check will chain the following mblks:
 * AUTH_KEYCHECK -> KEYSOCK_IN -> PF_KEY message
 *
 * The algorithm module will use the keysock_in to get the keys, and perform
 * whatever sanity check it deems fit.  Since the algorithm defines its
 * key format (e.g. multi-keys in a single string), it can perform this check.
 *
 * A weak key ACK will be the same, though perhaps what is in errno
 * right now will have richer semantics.
 */
typedef struct auth_keycheck_s {
	uint32_t auth_keycheck_type;
	uint32_t auth_keycheck_len;
	int auth_keycheck_errno;
	/*
	 * Other, richer error information might go here.
	 */
} auth_keycheck_t;


/*
 * All IPsec informational messages are placed into the ipsec_info_t
 * union, so that allocation can be done once, and IPsec informational
 * messages can be recycled.
 */
typedef union ipsec_info_u {
	struct {
		uint32_t ipsec_allu_type;
		uint32_t ipsec_allu_len;	/* In bytes */
	} ipsec_allu;
	ipsec_in_t ipsec_in;
	ipsec_out_t ipsec_out;
	keysock_hello_ack_t keysock_hello_ack;
	keysock_in_t keysock_in;
	keysock_out_t keysock_out;
	keysock_out_err_t keysock_out_err;
	auth_pi_hello_t auth_pi_hello;
	auth_req_t auth_req;
	auth_ack_t auth_ack;
	auth_keycheck_t auth_keycheck;
} ipsec_info_t;
#define	ipsec_info_type ipsec_allu.ipsec_allu_type
#define	ipsec_info_len ipsec_allu.ipsec_allu_len

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IPSEC_INFO_H */
