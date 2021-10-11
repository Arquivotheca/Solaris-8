/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_ARP_H
#define	_INET_ARP_H

#pragma ident	"@(#)arp.h	1.21	99/03/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ARP_REQUEST	1
#define	ARP_RESPONSE	2
#define	RARP_REQUEST	3
#define	RARP_RESPONSE	4

#define	AR_IOCTL		(((unsigned)'A' & 0xFF)<<8)

/*
* The following ARP commands are private, and not part of a supported
* interface. They are subject to change without notice in any release.
*/
#define	AR_ENTRY_ADD		(AR_IOCTL + 1)
#define	AR_ENTRY_DELETE		(AR_IOCTL + 2)
#define	AR_ENTRY_QUERY		(AR_IOCTL + 3)
#define	AR_XMIT_REQUEST		(AR_IOCTL + 4)
#define	AR_XMIT_TEMPLATE	(AR_IOCTL + 5)
#define	AR_ENTRY_SQUERY		(AR_IOCTL + 6)
#define	AR_MAPPING_ADD		(AR_IOCTL + 7)
#define	AR_CLIENT_NOTIFY	(AR_IOCTL + 8)
#define	AR_INTERFACE_UP		(AR_IOCTL + 9)
#define	AR_INTERFACE_DOWN	(AR_IOCTL + 10)
#define	AR_XMIT_RESPONSE	(AR_IOCTL + 11)
#define	AR_INTERFACE_ON		(AR_IOCTL + 12)
#define	AR_INTERFACE_OFF	(AR_IOCTL + 13)
#define	AR_DLPIOP_DONE		(AR_IOCTL + 14)

/*
* The following ACE flags are private, and not part of a supported
* interface. They are subject to change without notice in any release.
*/
#define	ACE_F_PERMANENT		0x1
#define	ACE_F_PUBLISH		0x2
#define	ACE_F_DYING		0x4
#define	ACE_F_RESOLVED		0x8
/* Using bit mask extraction from target address */
#define	ACE_F_MAPPING		0x10
#define	ACE_F_MYADDR		0x20	/* Strong check for duplicate MACs */

/* ARP Cmd Table entry */
typedef struct arct_s {
	pfi_t	arct_pfi;
	uint32_t	arct_cmd;
	int	arct_min_len;
	boolean_t	arct_ioctl_aware;
	boolean_t	arct_priv_cmd;
} arct_t;

/* ARP Command Structures */

/* arc_t - Common command overlay */
typedef struct ar_cmd_s {
	uint32_t	arc_cmd;
	uint32_t	arc_name_offset;
	uint32_t	arc_name_length;
} arc_t;

/*
* The following ARP command structures are private, and not
* part of a supported interface. They are subject to change
* without notice in any release.
*/
typedef	struct ar_entry_add_s {
	uint32_t	area_cmd;
	uint32_t	area_name_offset;
	uint32_t	area_name_length;
	uint32_t	area_proto;
	uint32_t	area_proto_addr_offset;
	uint32_t	area_proto_addr_length;
	uint32_t	area_proto_mask_offset;
	uint32_t	area_flags;		/* Same values as ace_flags */
	uint32_t	area_hw_addr_offset;
	uint32_t	area_hw_addr_length;
} area_t;

typedef	struct ar_entry_delete_s {
	uint32_t	ared_cmd;
	uint32_t	ared_name_offset;
	uint32_t	ared_name_length;
	uint32_t	ared_proto;
	uint32_t	ared_proto_addr_offset;
	uint32_t	ared_proto_addr_length;
} ared_t;

typedef	struct ar_entry_query_s {
	uint32_t	areq_cmd;
	uint32_t	areq_name_offset;
	uint32_t	areq_name_length;
	uint32_t	areq_proto;
	uint32_t	areq_target_addr_offset;
	uint32_t	areq_target_addr_length;
	uint32_t	areq_flags;
	uint32_t	areq_sender_addr_offset;
	uint32_t	areq_sender_addr_length;
	uint32_t	areq_xmit_count;	/* 0 ==> cache lookup only */
	uint32_t	areq_xmit_interval; /* # of milliseconds; 0: default */
		/* # ofquests to buffer; 0: default */
	uint32_t	areq_max_buffered;
	uchar_t	areq_sap[8];		/* to insert in returned template */
} areq_t;

typedef	struct ar_mapping_add_s {
	uint32_t	arma_cmd;
	uint32_t	arma_name_offset;
	uint32_t	arma_name_length;
	uint32_t	arma_proto;
	uint32_t	arma_proto_addr_offset;
	uint32_t	arma_proto_addr_length;
	uint32_t	arma_proto_mask_offset;
	uint32_t	arma_proto_extract_mask_offset;
	uint32_t	arma_flags;
	uint32_t	arma_hw_addr_offset;
	uint32_t	arma_hw_addr_length;
		/* Offset were we start placing */
	uint32_t	arma_hw_mapping_start;
					/* the mask&proto_addr */
} arma_t;

/* Structure used to notify clients of interesting conditions. */
typedef struct ar_client_notify_s {
	uint32_t	arcn_cmd;
	uint32_t	arcn_name_offset;
	uint32_t	arcn_name_length;
	uint32_t	arcn_code;			/* Notification code. */
} arcn_t;

/* Client Notification Codes */
/*
* The following Client Notification codes are private, and not
* part of a supported interface. They are subject to change
* without notice in any release.
*/
#define	AR_CN_BOGON	1
#define	AR_CN_ANNOUNCE	2

/* ARP Header */
typedef struct arh_s {
	uchar_t	arh_hardware[2];
	uchar_t	arh_proto[2];
	uchar_t	arh_hlen;
	uchar_t	arh_plen;
	uchar_t	arh_operation[2];
	/* The sender and target hw/proto pairs follow */
} arh_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_ARP_H */
