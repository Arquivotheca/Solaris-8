/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IPSECESP_H
#define	_INET_IPSECESP_H

#pragma ident	"@(#)ipsecesp.h	1.1	98/12/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Named Dispatch Parameter Management Structure */
typedef struct ipsecesppparam_s {
	uint_t	ipsecesp_param_min;
	uint_t	ipsecesp_param_max;
	uint_t	ipsecesp_param_value;
	char	*ipsecesp_param_name;
} ipsecespparam_t;

/*
 * Instance of ESP.  They come in two varieties.  The first is a keysock
 * instance (where there should only be one) and the other is an AUTH PI
 * algorithm instance (of which there can be many).
 */
typedef struct espstate_s {
	queue_t *esps_rq;	/* To AUTH PI module or keysock. */
	queue_t *esps_wq;	/* To IP. */

	/* Copied from auth_pi_hello_t */
	uint8_t esps_id;	/* 0 if keysock, pending, or broken. */
	uint8_t esps_ivlen;
	uint16_t esps_minbits;
	uint16_t esps_maxbits;
	uint16_t esps_datalen;
	/* More state?  If so, should probably also be in auth_pi_hello_t. */

	boolean_t esps_keycheck;
} espstate_t;

/*
 * For now, only provide "aligned" version of header.
 * If aligned version is needed, we'll go with the naming conventions then.
 */

typedef struct esph {
	uint32_t esph_spi;
	uint32_t esph_replay;
} esph_t;

/* No need for "old" ESP, just point a uint32_t *. */


#ifdef	__cplusplus
}
#endif

#endif /* _INET_IPSECESP_H */
