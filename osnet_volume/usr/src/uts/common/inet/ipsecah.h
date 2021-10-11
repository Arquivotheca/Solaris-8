/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IPSECAH_H
#define	_INET_IPSECAH_H

#pragma ident	"@(#)ipsecah.h	1.1	98/12/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Named Dispatch Parameter Management Structure */
typedef struct ipsecahpparam_s {
	uint_t	ipsecah_param_min;
	uint_t	ipsecah_param_max;
	uint_t	ipsecah_param_value;
	char	*ipsecah_param_name;
} ipsecahparam_t;

/*
 * Instance of AH.  They come in two varieties, keysock (where there should
 * only be one), and AUTH PI algorithm.
 */
typedef struct ahstate_s {
	queue_t *ahs_rq;	/* To AUTH PI module or keysock. */
	queue_t *ahs_wq;	/* To IP. */

	/* Copied from auth_pi_hello_t */
	uint8_t ahs_id;	/* If 0, then this is keysock, pending, or broken. */
	uint8_t ahs_ivlen;
	uint16_t ahs_minbits;
	uint16_t ahs_maxbits;
	uint16_t ahs_datalen;
	/* More state?  If so, should probably also be in auth_pi_hello_t. */

	boolean_t ahs_keycheck;
} ahstate_t;

/*
 * For now, only provide "aligned" version of header.
 * If aligned version is needed, we'll go with the naming conventions then.
 */

typedef struct ah {
	uint8_t ah_nexthdr;
	uint8_t ah_length;
	uint16_t ah_reserved;
	uint32_t ah_spi;
	uint32_t ah_replay;
} ah_t;

#define	AH_BASELEN	12
#define	AH_TOTAL_LEN(ah)	(((ah)->ah_length << 2) + AH_BASELEN - \
					sizeof ((ah)->ah_replay))

/* "Old" AH, without replay.  For 1827-29 compatibility. */

typedef struct ahold {
	uint8_t ah_nexthdr;
	uint8_t ah_length;
	uint16_t ah_reserved;
	uint32_t ah_spi;
} ahold_t;

#define	AHOLD_BASELEN	8
#define	AHOLD_TOTAL_LEN(ah)	(((ah)->ah_length << 2) + AH_BASELEN)

#ifdef	__cplusplus
}
#endif

#endif /* _INET_IPSECAH_H */
