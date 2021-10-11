/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * dhcp.h - Generic DHCP definitions exported to windows and dos programs.
 */

#ifndef	_DHCP_H
#define	_DHCP_H

#pragma ident	"@(#)dhcp.h	1.22	99/08/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && defined(DHCP_CLIENT)
#include <sys/dhcpboot.h>
#endif	/* _KERNEL && DHCP_CLIENT */

#ifdef	_REENTRANT
#include <thread.h>
#endif	/* _REENTRANT */

/*
 * Generic DHCP option structure.
 */
typedef struct {
	uint8_t    code;
	uint8_t    len;
	uint8_t    value[1];
} DHCP_OPT;

/*
 * DHCP option codes.
 */

#define	CD_PAD			0
#define	CD_END			255
#define	CD_SUBNETMASK		1
#define	CD_TIMEOFFSET		2
#define	CD_ROUTER		3
#define	CD_TIMESERV		4
#define	CD_IEN116_NAME_SERV	5
#define	CD_DNSSERV		6
#define	CD_LOG_SERV		7
#define	CD_COOKIE_SERV		8
#define	CD_LPR_SERV		9
#define	CD_IMPRESS_SERV		10
#define	CD_RESOURCE_SERV	11
#define	CD_HOSTNAME		12
#define	CD_BOOT_SIZE		13
#define	CD_DUMP_FILE		14
#define	CD_DNSDOMAIN		15
#define	CD_SWAP_SERV		16
#define	CD_ROOT_PATH		17
#define	CD_EXTEND_PATH		18

/* IP layer parameters */
#define	CD_IP_FORWARDING_ON	19
#define	CD_NON_LCL_ROUTE_ON	20
#define	CD_POLICY_FILTER	21
#define	CD_MAXIPSIZE		22
#define	CD_IPTTL		23
#define	CD_PATH_MTU_TIMEOUT	24
#define	CD_PATH_MTU_TABLE_SZ	25

/* IP layer parameters per interface */
#define	CD_MTU			26
#define	CD_ALL_SUBNETS_LCL_ON	27
#define	CD_BROADCASTADDR	28
#define	CD_MASK_DISCVRY_ON	29
#define	CD_MASK_SUPPLIER_ON	30
#define	CD_ROUTER_DISCVRY_ON	31
#define	CD_ROUTER_SOLICIT_SERV	32
#define	CD_STATIC_ROUTE		33

/* Link Layer Parameters per Interface */
#define	CD_TRAILER_ENCAPS_ON	34
#define	CD_ARP_TIMEOUT		35
#define	CD_ETHERNET_ENCAPS_ON	36

/* TCP Parameters */
#define	CD_TCP_TTL		37
#define	CD_TCP_KALIVE_INTVL	38
#define	CD_TCP_KALIVE_GRBG_ON	39

/* Application layer parameters */
#define	CD_NIS_DOMAIN		40
#define	CD_NIS_SERV		41
#define	CD_NTP_SERV		42
#define	CD_VENDOR_SPEC		43

/* NetBIOS parameters */
#define	CD_NETBIOS_NAME_SERV	44
#define	CD_NETBIOS_DIST_SERV	45
#define	CD_NETBIOS_NODE_TYPE	46
#define	CD_NETBIOS_SCOPE	47

/* X Window parameters */
#define	CD_XWIN_FONT_SERV	48
#define	CD_XWIN_DISP_SERV	49

/* DHCP protocol extension options */
#define	CD_REQUESTED_IP_ADDR	50
#define	CD_LEASE_TIME		51
#define	CD_OPTION_OVERLOAD	52
#define	CD_DHCP_TYPE		53
#define	CD_SERVER_ID		54
#define	CD_REQUEST_LIST		55
#define	CD_MESSAGE		56
#define	CD_MAX_DHCP_SIZE	57
#define	CD_T1_TIME		58
#define	CD_T2_TIME		59
#define	CD_CLASS_ID		60
#define	CD_CLIENT_ID		61

/* Netware options */
#define	CD_NW_IP_DOMAIN		62
#define	CD_NW_IP_OPTIONS	63

/* Nisplus options */
#define	CD_NISPLUS_DMAIN	64
#define	CD_NISPLUS_SERVS	65

/* Optional sname/bootfile options */
#define	CD_TFTP_SERV_NAME	66
#define	CD_OPT_BOOTFILE_NAME	67

/* Additional server options */
#define	CD_MOBILE_IP_AGENT	68
#define	CD_SMTP_SERVS		69
#define	CD_POP3_SERVS		70
#define	CD_NNTP_SERVS		71
#define	CD_WWW_SERVS		72
#define	CD_FINGER_SERVS		73
#define	CD_IRC_SERVS		74

/* Streettalk options */
#define	CD_STREETTALK_SERVS	75
#define	CD_STREETTALK_DA_SERVS	76

/* User class identifier */
#define	CD_USER_CLASS_ID	77

#define	DHCP_FIRST_OPT		CD_SUBNETMASK
#define	DHCP_LAST_STD		CD_USER_CLASS_ID
#define	DHCP_SITE_OPT		128		/* inclusive */
#define	DHCP_END_SITE		254
#define	DHCP_LAST_OPT		DHCP_END_SITE	/* last op code */

#define	DHCP_MAX_OPT_SIZE	255	/* maximum option size in octets */

/* Packet fields */
#define	CD_PACKET_START		256
#define	CD_YIADDR		256	/* client's ip address */
#define	CD_SIADDR		257	/* Bootserver's ip address */
#define	CD_SNAME		258	/* Hostname of Bootserver, or opts */
#define	CD_GIADDR		259	/* BOOTP relay agent address */
#define	CD_BOOTFILE		260	/* File to boot or opts */
#define	CD_PACKET_END		260

/* Internal server options */
#define	CD_INTRNL_START		1024
#define	CD_BOOL_HOSTNAME	1024	/* Entry wants hostname (Nameserv) */
#define	CD_BOOL_LEASENEG	1025	/* Entry's lease is negotiable */
#define	CD_BOOL_ECHO_VCLASS	1026	/* Echo Vendor class back to Entry */
#define	CD_BOOTPATH		1027	/* prefix path to File to boot */
#define	CD_INTRNL_END		1027

/*
 * DHCP Packet. What will fit in a ethernet frame. We may use a smaller
 * size, based on what our transport can handle.
 */
#define	DHCP_DEF_MAX_SIZE	576	/* as spec'ed in RFC 2131 */
#define	PKT_BUFFER		1486	/* max possible size of pkt buffer */
#define	BASE_PKT_SIZE		240	/* everything but the options */
typedef struct dhcp {
	uint8_t		op;		/* message opcode */
	uint8_t		htype;		/* Hardware address type */
	uint8_t		hlen;		/* Hardware address length */
	uint8_t		hops;		/* Used by relay agents */
	uint32_t	xid;		/* transaction id */
	uint16_t	secs;		/* Secs elapsed since client boot */
	uint16_t	flags;		/* DHCP Flags field */
	struct in_addr	ciaddr;		/* client IP addr */
	struct in_addr	yiaddr;		/* 'Your' IP addr. (from server) */
	struct in_addr	siaddr;		/* Boot server IP addr */
	struct in_addr	giaddr;		/* Relay agent IP addr */
	uint8_t		chaddr[16];	/* Client hardware addr */
	uint8_t		sname[64];	/* Optl. boot server hostname */
	uint8_t		file[128];	/* boot file name (ascii path) */
	uint8_t		cookie[4];	/* Magic cookie */
	uint8_t		options[60];	/* Options */
} PKT;

/*
 * async icmp echo check definitions
 */
enum dhcp_icmp_flag {
	DHCP_ICMP_NOENT,	/* No pending icmp check */
	DHCP_ICMP_PENDING,	/* Echo check is still pending */
	DHCP_ICMP_AVAILABLE,	/* Address is not in use */
	DHCP_ICMP_IN_USE,	/* Address is in use */
	DHCP_ICMP_FAILED,	/* Error; results unknown. */
	DHCP_ICMP_DONTCARE	/* icmp check in some !NOENT state */
};

typedef uint32_t	lease_t; /* DHCP lease time (32 bit quantity) */

#ifdef	DHCP_CLIENT
#include <sys/sunos_dhcp_class.h>
#endif	/* DHCP_CLIENT */

/*
 * Generic DHCP packet list. Ensure that _REENTRANT bracketed code stays at
 * bottom of this definition - the client doesn't include it. Scan.c in
 * libdhcp isn't aware of it either...
 */
#define	MAX_PKT_LIST	5	/* maximum list size */
typedef struct  dhcp_list {
	PKT			*pkt;		/* client packet */
	uint_t			len;		/* packet len */
	int			rfc1048;	/* RFC1048 options - boolean */
	struct dhcp_list 	*prev;
	struct dhcp_list 	*next;
	uint8_t			offset;		/* BOOTP packet offset */
				/*
				 * standard/site options
				 */
	DHCP_OPT		*opts[DHCP_LAST_OPT + 1];

#ifdef	DHCP_CLIENT
				/*
				 * Vendor specific options (client only)
				 */
	DHCP_OPT		*vs[VS_OPTION_END - VS_OPTION_START + 1];
#endif	/* DHCP_CLIENT */

#ifdef	_REENTRANT
	enum dhcp_icmp_flag	d_icmpflag;	/* icmp echo status - OFFER */
	struct in_addr		off_ip;		/* Address OFFERed */
	mutex_t			plp_mtx;	/* mutex protecting entry */
#endif	/* _REENTRANT */

} PKT_LIST;

#define	PKT_LIST_NULL		((PKT_LIST *)NULL)

/*
 * DHCP packet types. As per protocol.
 */
#define	DISCOVER	((uint8_t)1)
#define	OFFER		((uint8_t)2)
#define	REQUEST		((uint8_t)3)
#define	DECLINE		((uint8_t)4)
#define	ACK		((uint8_t)5)
#define	NAK		((uint8_t)6)
#define	RELEASE		((uint8_t)7)
#define	INFORM		((uint8_t)8)

/*
 * Generic DHCP protocol defines
 */
#define	DHCP_PERM	((lease_t)0xffffffff)	/* "permanent" lease time */
#define	BOOTREQUEST		(1)		/* BOOTP REQUEST opcode */
#define	BOOTREPLY		(2)		/* BOOTP REPLY opcode */
#define	BOOTMAGIC	{ 99, 130, 83, 99 }	/* rfc1048 magic cookie */

/* Error codes that could be generated while parsing packets */
#define	DHCP_ERR_OFFSET		512
#define	DHCP_GARBLED_MSG_TYPE	(DHCP_ERR_OFFSET+0)
#define	DHCP_WRONG_MSG_TYPE	(DHCP_ERR_OFFSET+1)
#define	DHCP_BAD_OPT_OVLD	(DHCP_ERR_OFFSET+2)

#ifdef	__STDC__
extern int _dhcp_options_scan(PKT_LIST *);
extern int octet_to_ascii(uchar_t *, int, char *, int *);
extern int ascii_to_octet(char *, int, uchar_t *, int *);
#else
extern int _dhcp_options_scan();
extern int octet_to_ascii();
extern int ascii_to_octet();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_H */
