/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IPSEC_CONF_H
#define	_INET_IPSEC_CONF_H

#pragma ident	"@(#)ipsec_conf.h	1.2	99/09/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/types.h>
#include	<netinet/in.h>

/*
 * In future we may support nesting of algorithms. We need to
 * store algorithms for more than one level.
 */
#define	MAX_ALGS_LEVEL	1

typedef struct ipsec_conf {
	uint8_t		ipsc_src_addr[16];
	uint8_t 	ipsc_src_mask[16];
	uint8_t		ipsc_dst_addr[16];
	uint8_t 	ipsc_dst_mask[16];
	in_port_t	ipsc_src_port;
	in_port_t	ipsc_dst_port;
	uint8_t		ipsc_ulp_prot;
	uint8_t		ipsc_ipsec_prot;
	unsigned int
		ipsc_policy:2,
		ipsc_sa_attr:2,
		ipsc_dir:1,
		ipsc_isv4:1,
		ipsc_padding_bits:26;
	int		ipsc_no_of_ah_algs;
	uint8_t		ipsc_ah_algs[MAX_ALGS_LEVEL];
	int 		ipsc_no_of_esp_algs;
	uint8_t		ipsc_esp_algs[MAX_ALGS_LEVEL];
	int 		ipsc_no_of_esp_auth_algs;
	uint8_t		ipsc_esp_auth_algs[MAX_ALGS_LEVEL];
	uint32_t	ipsc_policy_index;
} ipsec_conf_t;

#ifdef	_KERNEL
typedef	struct ipsec_policy {
	struct ipsec_policy *ipsec_policy_next;
	ipsec_conf_t ipsec_conf;
} ipsec_policy_t;
#endif


#define	IPSEC_CONF_SRC_ADDRESS		0	/* Source Address */
#define	IPSEC_CONF_SRC_PORT		1	/* Source Port */
#define	IPSEC_CONF_DST_ADDRESS		2	/* Dest Address */
#define	IPSEC_CONF_DST_PORT		3	/* Dest Port */
#define	IPSEC_CONF_SRC_MASK		4	/* Source Address Mask */
#define	IPSEC_CONF_DST_MASK		5	/* Destination Address Mask */
#define	IPSEC_CONF_ULP			6	/* Upper layer Port */
#define	IPSEC_CONF_IPSEC_PROT		7	/* AH or ESP or AH_ESP */
#define	IPSEC_CONF_IPSEC_AALGS		8	/* Auth Algorithms - MD5 etc. */
#define	IPSEC_CONF_IPSEC_EALGS		9	/* Encr Algorithms - DES etc. */
#define	IPSEC_CONF_IPSEC_EAALGS		10	/* Encr Algorithms - MD5 etc. */
#define	IPSEC_CONF_IPSEC_SA		11	/* Shared or unique SA */
#define	IPSEC_CONF_IPSEC_DIR		12	/* Direction of traffic */

/* Type of an entry */

#define	IPSEC_NTYPES			0x02
#define	IPSEC_TYPE_OUTBOUND		0x00
#define	IPSEC_TYPE_INBOUND		0x01

/* Policy */
#define	IPSEC_POLICY_APPLY	0x01
#define	IPSEC_POLICY_DISCARD	0x02
#define	IPSEC_POLICY_BYPASS	0x03

/* Shared or unique SA */
#define	IPSEC_SHARED_SA		0x01
#define	IPSEC_UNIQUE_SA		0x02

/* IPSEC protocols and combinations */
#define	IPSEC_AH_ONLY		0x01
#define	IPSEC_ESP_ONLY		0x02
#define	IPSEC_AH_ESP		0x03

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IPSEC_CONF_H */
