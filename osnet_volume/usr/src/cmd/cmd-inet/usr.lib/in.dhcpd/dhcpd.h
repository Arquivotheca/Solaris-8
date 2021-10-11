/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCPD_H
#define	_DHCPD_H

#pragma ident	"@(#)dhcpd.h	1.71	99/10/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * dhcpd.h -- common header file for all the modules of the in.dhcpd program.
 */

#ifndef	TRUE
#define	TRUE	1
#endif	/* TRUE */

#ifndef	FALSE
#define	FALSE	0
#endif	/* FALSE */

/*
 * Raw encoded packet data. The final state. Note that 'code' not only
 * describes options: predefinied: 1-60, site: 128-254, vendor: 42(*),
 * but it also defines packet fields for packet data as well.
 */
typedef	struct encoded {
	ushort_t	code;	/* Option code: 1--254, pkt loc */
	uchar_t		len;	/* len of data */
	uchar_t		*data;	/* Encoded DHCP packet field / option */
	struct encoded	*prev;	/* previous in list */
	struct encoded	*next;	/* next in list */
} ENCODE;

#define	DHCP_CLASS_SIZE		128
#define	DHCP_MAX_CLASS_SIZE	1280
typedef struct {
	char	class[DHCP_CLASS_SIZE + 1];	/* client class */
	ENCODE	*head;				/* options of this class */
} VNDLIST;

#define	DHCP_MACRO_SIZE	64			/* Max Len of a macro name */
typedef struct {
	char	nm[DHCP_MACRO_SIZE + 1];	/* Macro name */
	ENCODE	*head;				/* head of encoded opts */
	int	classes;			/* num of client classes */
	VNDLIST	**list;				/* table of client classes */
} MACRO;

/* logging message categories */
typedef enum {
	L_ASSIGN =	0,	/* New assignment */
	L_REPLY =	1,	/* respond to existing client */
	L_RELEASE =	2,	/* client released IP */
	L_DECLINE =	3,	/* client declined IP */
	L_INFORM =	4,	/* client requested information only */
	L_NAK =		5,	/* client NAK'ed */
	L_ICMP_ECHO =	6,	/* Server detected IP in use */
	L_RELAY_REQ =	7,	/* Relay request to server(s) */
	L_RELAY_REP =	8	/* Relay reply to client */
} DHCP_MSG_CATEGORIES;

typedef enum {
	P_BOOTP =	0,	/* BOOT Protocol */
	P_DHCP =	1	/* DHC Protocol */
} DHCP_PROTO;

#define	DHCPD			"in.dhcpd"	/* daemon's name */
#define	DAEMON_VERS		"3.3"		/* daemon's version number */
#define	BCAST_MASK		0x8000		/* BROADCAST flag */
#define	ENC_COPY		0		/* Copy encode list */
#define	ENC_DONT_COPY		1		/* don't copy encode list */
#define	DHCP_MAX_REPLY_SIZE	8192		/* should be big enough */
#define	NTOABUF			20		/* Len of an IP addr string */
#define	DHCP_ICMP_ATTEMPTS	2		/* Number of ping attempts */
#define	DHCP_ICMP_TIMEOUT	1000		/* Wait # millisecs for resp */
#define	DHCP_ARP_ADD		0		/* Add an ARP table entry */
#define	DHCP_ARP_DEL		1		/* Del an ARP table entry */
#define	DHCP_IDLE_TIME		60		/* daemon idle timeout */
#define	DHCP_SCRATCH		128		/* scratch buffer size */
#define	NEW_DHCPTAB		0		/* load initial dhcptab */
#define	PRESERVE_DHCPTAB	1		/* preserve previous dhcptab */
#define	DEFAULT_LEASE		3600		/* Default if not specified */
#define	INIT_STATE		1		/* Client state: INIT */
#define	INIT_REBOOT_STATE	2		/* Client state: INIT-REBOOT */
#define	DHCP_RDDFLT_RETRIES	3		/* Attempts to read defaults */

/* load option flags */
#define	DHCP_DHCP_CLNT		1		/* It's a DHCP client */
#define	DHCP_SEND_LEASE		2		/* Send lease parameters */
#define	DHCP_NON_RFC1048	4		/* non-rfc1048 magic cookie */
#define	DHCP_MASK		7		/* legal values */
#define	DHCP_OVRLD_CLR		((uchar_t)0x00)	/* SNAME/FILE clear */
#define	DHCP_OVRLD_FILE		((uchar_t)0x01)	/* FILE in use */
#define	DHCP_OVRLD_SNAME	((uchar_t)0x02)	/* SNAME in use */
#define	DHCP_OVRLD_MASK		((uchar_t)0xfc)	/* Only last two bits */

/*
 * Number of seconds 'secs' field in packet must be before a DHCP server
 * responds to a client who is requesting verification of it's IP address
 * *AND* renegotiating its lease on an address that is owned by another
 * server. This is done to give the *OWNER* server time to respond to
 * the client first.
 */
#define	DHCP_RENOG_WAIT		20

extern int		debug;
extern boolean_t	verbose;
extern boolean_t	noping;
extern boolean_t	ethers_compat;
extern boolean_t	no_dhcptab;
extern boolean_t	server_mode;
extern boolean_t	be_automatic;
extern boolean_t	reinitialize;
extern int		max_hops;
extern int		log_local;
extern int		icmp_tries;
extern time_t		off_secs;
extern time_t		rescan_interval;
extern time_t		abs_rescan;
extern time_t		icmp_timeout;
extern ulong_t		npkts;		/* total packets awaiting processing */
extern mutex_t		npkts_mtx;	/* mutex to protect npkts */
extern cond_t		npkts_cv;	/* condition variable (npkts == 0) */
extern ulong_t		totpkts;	/* total packets received */
extern mutex_t		totpkts_mtx;	/* mutex to protect totpkts */
extern struct in_addr	server_ip;

extern int	idle(void);
extern PKT	*gen_bootp_pkt(int, PKT *);
extern int	inittab(void);
extern int	checktab(void);
extern int	readtab(int);
extern void	resettab(void);
extern int	relay_agent_init(char *);
extern void	dhcpmsg();
extern char 	*smalloc(unsigned);
extern ENCODE 	*combine_encodes(ENCODE *, ENCODE *, int);
extern MACRO	*get_macro(char *);
extern ENCODE	*find_encode(ENCODE *, ushort_t);
extern ENCODE	*dup_encode(ENCODE *);
extern ENCODE	*make_encode(ushort_t, uchar_t, void *, int);
extern ENCODE	*copy_encode_list(ENCODE *);
extern void	free_encode_list(ENCODE *);
extern void	free_encode(ENCODE *);
extern void	replace_encode(ENCODE **, ENCODE *, int);
extern ENCODE	*vendor_encodes(MACRO *, char *);
extern char 	*disp_cid(PKT_LIST *, char *, int);
extern ushort_t	ip_chksum(char *, ushort_t);
extern char	*inet_ntoa_r(struct in_addr, char *);	/* libnsl */
extern void	get_client_id(PKT_LIST *, uchar_t *, uint_t *);
extern char	*get_class_id(PKT_LIST *, char *, int);
extern uchar_t	*get_octets(uchar_t **, uchar_t *);
extern int	get_number(char **, void *, int);
extern int	load_options(int, PKT_LIST *, PKT *, int, uchar_t *, ENCODE *,
		    ENCODE *);
extern int	_dhcp_options_scan(PKT_LIST *);
extern void	free_plp(PKT_LIST *);
extern int	stat_boot_server(void);
extern void	logtrans(DHCP_PROTO, DHCP_MSG_CATEGORIES, time_t,
		    struct in_addr, struct in_addr, PKT_LIST *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCPD_H */
