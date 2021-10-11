/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef _PING_H
#define	_PING_H

#pragma ident	"@(#)ping.h	1.3	99/09/24 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_PORT	65535   /* max port number for UDP probes */
#define	MAX_ICMP_SEQ	65535   /* max icmp sequence value */

/*
 * Maximum number of source route space. Please note that because of the API,
 * we can only specify 8 gateways, the last address has to be the target
 * address.
 */
#define	MAX_GWS		9

/*
 * This is the max it can be. But another limiting factor is the PMTU,
 * so in any instance, it can be less than 127.
 */
#define	MAX_GWS6 	127

/* maximum of above two */
#define	MAXMAX_GWS	MAX(MAX_GWS, MAX_GWS6)

/* size of buffer to store the IPv4 gateway addresses */
#define	ROUTE_SIZE 	(IPOPT_OLEN + IPOPT_OFFSET + \
			MAX_GWS * sizeof (struct in_addr))

#define	A_CNT(ARRAY) (sizeof (ARRAY) / sizeof ((ARRAY)[0]))


#define	Printf (void) printf
#define	Fprintf (void) fprintf

/*
 * For each target IP address we are going to probe, we store required info,
 * such as address family, IP address of target, source IP address to use
 * for that target address, and number of probes to send in the targetaddr
 * structure.
 * All target addresses are also linked to each other and used in
 * scheduling probes. Each targetaddr structure identifies a batch of probes to
 * send : where to send, how many to send. We capture state information, such as
 * number of probes already sent (in this batch only), whether target replied
 * as we probe it, whether we are done with probing this address (can happen
 * in regular (!stats) mode when we get a reply for a probe sent in current
 * batch), and starting sequence number which is used together with number of
 * probes sent to determine if the incoming reply is for a probe we sent in
 * current batch.
 */
struct targetaddr {
	int family;
	union any_in_addr dst_addr;	/* dst address for the probe */
	union any_in_addr src_addr;	/* src addr to use for this dst addr */
	int num_probes;			/* num of probes to send to this dst */
	int num_sent;			/* number of probes already sent */
	boolean_t got_reply;		/* received a reply from dst while */
					/* still probing it */
	boolean_t probing_done;		/* skip without sending all probes */
	ushort_t starting_seq_num;	/* initial icmp_seq/UDP port, used */
					/* for authenticating replies */
	struct targetaddr *next;	/* next targetaddr item in the list */
};

struct hostinfo {
	char *name;			/* hostname */
	int family;			/* address family */
	int num_addr;			/* number of addresses */
	union any_in_addr *addrs;	/* address list */
};

struct icmptype_table {
	int type;		/* ICMP type */
	char *message;		/* corresponding string message */
};

extern struct targetaddr *current_targetaddr;
extern int nreceived;
extern int nreceived_last_target;
extern int npackets;
extern boolean_t is_alive;
extern int datalen;
extern boolean_t nflag;
extern int ident;
extern boolean_t probe_all;
extern char *progname;
extern boolean_t rr_option;
extern boolean_t stats;
extern boolean_t strict;
extern char *targethost;
extern int tmax;
extern int tmin;
extern int ts_flag;
extern boolean_t ts_option;
extern int tsum;
extern boolean_t use_icmp_ts;
extern boolean_t use_udp;
extern boolean_t verbose;
extern boolean_t send_reply;

#ifdef __cplusplus
}
#endif

#endif /* _PING_H */
