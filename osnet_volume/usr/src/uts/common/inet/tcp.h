/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_TCP_H
#define	_INET_TCP_H

#pragma ident	"@(#)tcp.h	1.52	99/11/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/inttypes.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <inet/tcp_sack.h>

/*
 * Private (and possibly temporary) ioctl used by configuration code
 * to lock in the "default" stream for detached closes.
 */
#define	TCP_IOC_DEFAULT_Q	(('T' << 8) + 51)

/* TCP states */
#define	TCPS_CLOSED		-6
#define	TCPS_IDLE		-5	/* idle (opened, but not bound) */
#define	TCPS_BOUND		-4	/* bound, ready to connect or accept */
#define	TCPS_LISTEN		-3	/* listening for connection */
#define	TCPS_SYN_SENT		-2	/* active, have sent syn */
#define	TCPS_SYN_RCVD		-1	/* have received syn (and sent ours) */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define	TCPS_ESTABLISHED	0	/* established */
#define	TCPS_CLOSE_WAIT		1	/* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define	TCPS_FIN_WAIT_1		2	/* have closed and sent fin */
#define	TCPS_CLOSING		3	/* closed, xchd FIN, await FIN ACK */
#define	TCPS_LAST_ACK		4	/* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define	TCPS_FIN_WAIT_2		5	/* have closed, fin is acked */
#define	TCPS_TIME_WAIT		6	/* in 2*msl quiet wait after close */

/* Bit values in 'th_flags' field of the TCP packet header */
#define	TH_FIN			0x01	/* Sender will not send more */
#define	TH_SYN			0x02	/* Synchronize sequence numbers */
#define	TH_RST			0x04	/* Reset the connection */
#define	TH_PSH			0x08	/* This segment requests a push */
#define	TH_ACK			0x10	/* Acknowledgement field is valid */
#define	TH_URG			0x20	/* Urgent pointer field is valid */
/*
 * Internal flags used in conjunction with the packet header flags above.
 * Used in tcp_rput to keep track of what needs to be done.
 */
#define	TH_ACK_ACCEPTABLE	0x0400
#define	TH_XMIT_NEEDED		0x0800	/* Window opened - send queued data */
#define	TH_REXMIT_NEEDED	0x1000	/* Time expired for unacked data */
#define	TH_ACK_NEEDED		0x2000	/* Send an ack now. */
#define	TH_NEED_SACK_REXMIT	0x4000	/* Use SACK info to retransmission */
#define	TH_TIMER_NEEDED 0x8000	/* Start the delayed ack/push bit timer */
#define	TH_ORDREL_NEEDED	0x10000	/* Generate an ordrel indication */
#define	TH_MARKNEXT_NEEDED	0x20000	/* Data should have MSGMARKNEXT */
#define	TH_SEND_URP_MARK	0x40000	/* Send up tcp_urp_mark_mp */

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SEQ_LT(a, b)	((int32_t)((a)-(b)) < 0)
#define	SEQ_LEQ(a, b)	((int32_t)((a)-(b)) <= 0)
#define	SEQ_GT(a, b)	((int32_t)((a)-(b)) > 0)
#define	SEQ_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)

/* TCP Protocol header */
typedef	struct tcphdr_s {
	uint8_t		th_lport[2];	/* Source port */
	uint8_t		th_fport[2];	/* Destination port */
	uint8_t		th_seq[4];	/* Sequence number */
	uint8_t		th_ack[4];	/* Acknowledgement number */
	uint8_t		th_offset_and_rsrvd[1]; /* Offset to the packet data */
	uint8_t		th_flags[1];
	uint8_t		th_win[2];	/* Allocation number */
	uint8_t		th_sum[2];	/* TCP checksum */
	uint8_t		th_urp[2];	/* Urgent pointer */
} tcph_t;

#define	TCP_HDR_LENGTH(tcph) (((tcph)->th_offset_and_rsrvd[0] >>2) &(0xF << 2))
#define	TCP_MAX_COMBINED_HEADER_LENGTH	(60 + 60) /* Maxed out ip + tcp */
#define	TCP_MAX_IP_OPTIONS_LENGTH	(60 - IP_SIMPLE_HDR_LENGTH)
#define	TCP_MAX_HDR_LENGTH		60
#define	TCP_MAX_TCP_OPTIONS_LENGTH	(60 - sizeof (tcph_t))
#define	TCP_MIN_HEADER_LENGTH		20
#define	TCP_MAXWIN			65535
#define	TCP_PORT_LEN			sizeof (in_port_t)
#define	TCP_MAX_WINSHIFT		14
#define	TCP_MAX_LARGEWIN		(TCP_MAXWIN << TCP_MAX_WINSHIFT)

#define	TCPIP_HDR_LENGTH(mp, n)					\
	(n) = IPH_HDR_LENGTH((mp)->b_rptr),			\
	(n) += TCP_HDR_LENGTH((tcph_t *)&(mp)->b_rptr[(n)])

/* TCP Protocol header (used if the header is known to be 32-bit aligned) */
typedef	struct tcphdra_s {
	in_port_t	tha_lport;	/* Source port */
	in_port_t	tha_fport;	/* Destination port */
	uint32_t	tha_seq;	/* Sequence number */
	uint32_t	tha_ack;	/* Acknowledgement number */
	uint8_t tha_offset_and_reserved; /* Offset to the packet data */
	uint8_t		tha_flags;
	uint16_t	tha_win;	/* Allocation number */
	uint16_t	tha_sum;	/* TCP checksum */
	uint16_t	tha_urp;	/* Urgent pointer */
} tcpha_t;

struct tcp_base_s;

/*
 * Control structure for each open TCP stream,
 * defined only within the kernel or for a kmem user.
 * NOTE: tcp_reinit_values MUST have a line for each field in this structure!
 */
#if (defined(_KERNEL) || defined(_KMEMUSER))

typedef struct tcp_s {
	struct tcp_s	*tcp_listen_hash; /* Listen hash chain */
	struct tcp_s **tcp_ptplhn; /* Pointer to previous listen hash next. */
	struct tcp_s	*tcp_acceptor_hash; /* Acceptor hash chain */
	struct tcp_s **tcp_ptpahn; /* Pointer to previous accept hash next. */

	queue_t	*tcp_rq;		/* Our upstream neighbor (client) */
	queue_t	*tcp_wq;		/* Our downstream neighbor */

	/* Fields arranged in approximate access order along main paths */
	mblk_t	*tcp_xmit_head;		/* Head of rexmit list */
	mblk_t	*tcp_xmit_last;		/* last valid data seen by tcp_wput */
	uint32_t tcp_unsent;		/* # of bytes in hand that are unsent */
	mblk_t	*tcp_xmit_tail;		/* Last rexmit data sent */
	uint32_t tcp_xmit_tail_unsent;	/* # of unsent bytes in xmit_tail */

	uint32_t tcp_suna;		/* Sender unacknowledged */
	uint32_t tcp_rexmit_nxt;	/* Next rexmit seq num */
	uint32_t tcp_rexmit_max;	/* Max retran seq num */
	int32_t	tcp_snd_burst;		/* Send burst factor */
	uint32_t tcp_cwnd;		/* Congestion window */
	uint32_t tcp_cwnd_cnt;		/* cwnd cnt in congestion avoidance */

	uint32_t tcp_ibsegs;		/* Inbound segments on this stream */
	uint32_t tcp_obsegs;		/* Outbound segments on this stream */

	uint32_t tcp_naglim;		/* Tunable nagle limit */
	uint32_t	tcp_valid_bits;
#define	TCP_ISS_VALID	0x1	/* Is the tcp_iss seq num active? */
#define	TCP_FSS_VALID	0x2	/* Is the tcp_fss seq num active? */
#define	TCP_URG_VALID	0x4	/* If the tcp_urg seq num active? */


	int32_t	tcp_xmit_hiwater;	/* Send buffer high water mark. */
	mblk_t	*tcp_flow_mp;		/* mp to exert flow control upstream */

	mblk_t	*tcp_timer_mp;		/* Control block for timer service */
	uchar_t	tcp_timer_backoff;	/* Backoff shift count. */
	clock_t tcp_last_recv_time;	/* Last time we receive a segment. */
	clock_t	tcp_dack_set_time;	/* When delayed ACK timer is set. */

	uint32_t
		tcp_urp_last_valid : 1,	/* Is tcp_urp_last valid? */
		tcp_hard_binding : 1,	/* If we've started a full bind */
		tcp_hard_bound : 1,	/* If we've done a full bind with IP */
		tcp_priv_stream : 1, 	/* If stream was opened by priv user */

		tcp_fin_acked : 1,	/* Has our FIN been acked? */
		tcp_fin_rcvd : 1,	/* Have we seen a FIN? */
		tcp_fin_sent : 1,	/* Have we sent our FIN yet? */
		tcp_ordrel_done : 1,	/* Have we sent the ord_rel upstream? */

		tcp_flow_stopped : 1,	/* Have we flow controlled xmitter? */
		tcp_debug : 1,		/* SO_DEBUG "socket" option. */
		tcp_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		tcp_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		tcp_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		tcp_oobinline : 1,	/* SO_OOBINLINE "socket" option. */
		tcp_dgram_errind : 1,	/* SO_DGRAM_ERRIND option */
		tcp_detached : 1,	/* If we're detached from a stream */

		tcp_bind_pending : 1,	/* Client is waiting for bind ack */
		tcp_unbind_pending : 1, /* Client sent T_UNBIND_REQ */
		tcp_deferred_clean_death : 1,
					/* defer tcp endpoint cleanup etc. */
		tcp_co_wakeq_done : 1,	/* A strwakeq() has been done */

		tcp_co_wakeq_force : 1,	/* A strwakeq() must be done */
		tcp_co_norm : 1,	/* In normal mode, putnext() done */
		tcp_co_wakeq_need : 1,	/* A strwakeq() needs to be done */
		tcp_conn_def_q0: 1,	/* move from q0 to q deferred */

		tcp_tracing: 1,		/* tracing is active for this stream */
		tcp_linger : 1,		/* SO_LINGER turned on */
		tcp_zero_win_probe: 1,	/* Zero win probing is in progress */
		tcp_loopback: 1,	/* src and dst are the same machine */

		tcp_localnet: 1,	/* src and dst are on the same subnet */
		tcp_syn_defense: 1,	/* For defense against SYN attack */
#define	tcp_dontdrop	tcp_syn_defense
		tcp_set_timer : 1,
		tcp_1_junk_fill_thru_bit_31 : 1;

	uint32_t
		tcp_active_open: 1,	/* This is a active open */
		tcp_timeout : 1,	/* qbufcall failed, qtimeout pending */
		tcp_ack_timer_running: 1,	/* Delayed ACK timer running */
		tcp_rexmit : 1,		/* TCP is retransmitting */

		tcp_snd_sack_ok : 1,	/* Can use SACK for this connection */
		tcp_bind_proxy_addr : 1,	/* proxy addr is being used */
		tcp_recvdstaddr : 1,	/* return T_EXTCONN_IND with dst addr */
		tcp_hwcksum : 1,	/* The NIC is capable of hwcksum */

		tcp_ip_forward_progress : 1,
		tcp_anon_priv_bind : 1,

		tcp_pad_to_bit_31 : 22;

	uint32_t	tcp_if_mtu;	/* Outgoing interface MTU. */

	mblk_t	*tcp_reass_head;	/* Out of order reassembly list head */
	mblk_t	*tcp_reass_tail;	/* Out of order reassembly list tail */

	tcp_sack_info_t	*tcp_sack_info;

#define	tcp_pipe	tcp_sack_info->tcp_pipe
#define	tcp_fack	tcp_sack_info->tcp_fack
#define	tcp_sack_snxt	tcp_sack_info->tcp_sack_snxt
#define	tcp_max_sack_blk	tcp_sack_info->tcp_max_sack_blk
#define	tcp_num_sack_blk	tcp_sack_info->tcp_num_sack_blk
#define	tcp_sack_list		tcp_sack_info->tcp_sack_list
#define	tcp_num_notsack_blk	tcp_sack_info->tcp_num_notsack_blk
#define	tcp_cnt_notsack_list	tcp_sack_info->tcp_cnt_notsack_list
#define	tcp_notsack_list		tcp_sack_info->tcp_notsack_list

	mblk_t	*tcp_rcv_list;		/* Queued until push, urgent data, */
	mblk_t	*tcp_rcv_last_head;	/* optdata, or the count exceeds */
	mblk_t	*tcp_rcv_last_tail;	/* tcp_rcv_push_wait. */
	uint32_t tcp_rcv_cnt;		/* tcp_rcv_list is b_next chain. */

	mblk_t	*tcp_co_head;		/* Co (copyout) queue head */
	mblk_t	*tcp_co_tail;		/*  "     "       "   tail */
	mblk_t	*tcp_co_tmp;		/* Co timer mblk */
	/*
	 * Note: tcp_co_imp is used to both indicate the read-side stream
	 *	 data flow state (synchronous/asynchronous) as well as a
	 *	 pointer to a reusable iocblk sized mblk.
	 *
	 *	 The mblk is allocated (if need be) at initialization time
	 *	 and is used by the read-side when a copyout queue eligible
	 *	 mblk arrives (synchronous data flow) but previously one or
	 *	 more mblk(s) have been putnext()ed (asynchronous data flow).
	 *	 In this case, the mblk pointed to by tcp_co_imp is putnext()ed
	 *	 as an M_IOCTL of I_SYNCSTR after first nullifying tcp_co_imp.
	 *	 The streamhead will putnext() the mblk down the write-side
	 *	 stream as an M_IOCNAK of I_SYNCSTR and when (if) it arrives at
	 *	 the write-side its pointer will be saved again in tcp_co_imp.
	 *
	 *	 If an instance of tcp is closed while its tcp_co_imp is null,
	 *	 then the mblk will be freed elsewhere in the stream. Else, it
	 *	 will be freed (close) or saved (reinit) for future use.
	 */
	mblk_t	*tcp_co_imp;		/* Co ioctl mblk */
	clock_t tcp_co_tintrvl;		/* Co timer interval */
	uint32_t tcp_co_rnxt;		/* Co seq we expect to recv next */
	int	tcp_co_cnt;		/* Co enqueued byte count */

	uint32_t tcp_cwnd_ssthresh;	/* Congestion window */
	uint32_t tcp_cwnd_max;
	uint32_t tcp_csuna;		/* Clear (no rexmits in window) suna */

	clock_t	tcp_rtt_sa;		/* Round trip smoothed average */
	clock_t	tcp_rtt_sd;		/* Round trip smoothed deviation */
	clock_t	tcp_rtt_update;		/* Round trip update(s) */
	clock_t tcp_ms_we_have_waited;	/* Total retrans time */

	uint32_t tcp_swl1;		/* These help us avoid using stale */
	uint32_t tcp_swl2;		/*  packets to update state */

	uint32_t tcp_rack;		/* Seq # we have acked */
	uint32_t tcp_rack_cnt;		/* # of bytes we have deferred ack */
	uint32_t tcp_rack_cur_max;	/* # bytes we may defer ack for now */
	uint32_t tcp_rack_abs_max;	/* # of bytes we may defer ack ever */
	mblk_t	*tcp_ack_mp;		/* Delayed ACK timer block */

	uint32_t tcp_max_swnd;		/* Maximum swnd we have seen */

	struct tcp_s *tcp_listener;	/* Our listener */

	int32_t	tcp_xmit_lowater;	/* Send buffer low water mark. */

	uint32_t tcp_irs;		/* Initial recv seq num */
	uint32_t tcp_fss;		/* Final/fin send seq num */
	uint32_t tcp_urg;		/* Urgent data seq num */

	clock_t	tcp_first_timer_threshold;  /* When to prod IP */
	clock_t	tcp_second_timer_threshold; /* When to give up completely */
	clock_t	tcp_first_ctimer_threshold; /* 1st threshold while connecting */
	clock_t tcp_second_ctimer_threshold; /* 2nd ... while connecting */

	int	tcp_lingertime;		/* Close linger time (in seconds) */

	uint32_t tcp_urp_last;		/* Last urp for which signal sent */
	mblk_t	*tcp_urp_mp;		/* T_EXDATA_IND for urgent byte */
	mblk_t	*tcp_urp_mark_mp;	/* zero-length marked/unmarked msg */

	int tcp_conn_req_cnt_q0;	/* # of conn reqs in SYN_RCVD */
	int tcp_conn_req_cnt_q;	/* # of conn reqs in ESTABLISHED */
	int tcp_conn_req_max;	/* # of ESTABLISHED conn reqs allowed */
	t_scalar_t tcp_conn_req_seqnum;	/* Incrementing pending conn req ID */
#define	tcp_ip_addr_cache	tcp_reass_tail
					/* Cache ip addresses that */
					/* complete the 3-way handshake */
	struct tcp_s *tcp_eager_next_q; /* next eager in ESTABLISHED state */
	struct tcp_s *tcp_eager_last_q;	/* last eager in ESTABLISHED state */
	struct tcp_s *tcp_eager_next_q0; /* next eager in SYN_RCVD state */
	struct tcp_s *tcp_eager_prev_q0; /* prev eager in SYN_RCVD state */
					/* all eagers form a circular list */
	uint32_t tcp_syn_rcvd_timeout;	/* How many SYN_RCVD timeout in q0 */
	union {
	    mblk_t *tcp_eager_conn_ind; /* T_CONN_IND waiting for 3rd ack. */
	    mblk_t *tcp_opts_conn_req; /* T_CONN_REQ w/ options processed */
	} tcp_conn;

	int32_t	tcp_keepalive_intrvl;	/* Zero means don't bother */
	mblk_t	*tcp_keepalive_mp;	/* Timer block for keepalive */

	int32_t	tcp_client_errno;	/* How the client screwed up */

	char	*tcp_iphc;		/* Buffer holding tcp/ip hdr template */
	int	tcp_iphc_len;		/* actual allocated buffer size */
	int32_t	tcp_hdr_len;		/* Byte len of combined TCP/IP hdr */
	ipha_t	*tcp_ipha;		/* IPv4 header in the buffer */
	ip6_t	*tcp_ip6h;		/* IPv6 header in the buffer */
	int	tcp_ip_hdr_len;		/* Byte len of our current IPvx hdr */
	tcph_t	*tcp_tcph;		/* tcp header within combined hdr */
	int32_t	tcp_tcp_hdr_len;	/* tcp header len within combined */

	uint32_t tcp_sum;		/* checksum to compensate for source */
					/* routed packets. Host byte order */
	uint16_t tcp_last_sent_len;	/* Record length for nagle */
	uint16_t tcp_dupack_cnt;	/* # of consequtive duplicate acks */

	kcondvar_t	tcp_refcv;	/* Wait for refcnt decrease */

	kmutex_t	*tcp_bind_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_listen_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_conn_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_acceptor_lockp;	/* Ptr to tf_lock */

	timeout_id_t	tcp_ordrelid;		/* qbufcall/qtimeout id */
	t_uscalar_t	tcp_acceptor_id;	/* ACCEPTOR_id */
	/*
	 * tcp_ipsec_policy is set if a IPSEC_POLICY_SET mp is present
	 * with the SYN. Used by accept() code to set policy on the newly
	 * accepted connection.
	 */
	mblk_t		*tcp_ipsec_policy;
	int		tcp_ipsec_options_size;
	struct tcp_base_s *tcp_base;
	/*
	 * Address family that app wishes returned addrsses to be in.
	 * Currently taken from address family used in T_BIND_REQ, but
	 * should really come from family used in original socket() call.
	 * Value can be AF_INET or AF_INET6.
	 */
	uint_t	tcp_family;
	/*
	 * used for a quick test to determine if any ancillary bits are
	 * set
	 */
	uint_t		tcp_ipv6_recvancillary;		/* Flags */
#define	TCP_IPV6_RECVPKTINFO	0x01	/* IPV6_RECVPKTINFO option  */
#define	TCP_IPV6_RECVHOPLIMIT	0x02	/* IPV6_RECVHOPLIMIT option */
#define	TCP_IPV6_RECVHOPOPTS	0x04	/* IPV6_RECVHOPOPTS option */
#define	TCP_IPV6_RECVDSTOPTS	0x08	/* IPV6_RECVDSTOPTS option */
#define	TCP_IPV6_RECVRTHDR	0x10	/* IPV6_RECVRTHDR option */
#define	TCP_IPV6_RECVRTDSTOPTS	0x20	/* IPV6_RECVRTHDRDSTOPTS option */

	uint_t		tcp_recvifindex; /* Last received IPV6_RCVPKTINFO */
	uint_t		tcp_recvhops;	/* Last received IPV6_RECVHOPLIMIT */
	ip6_hbh_t	*tcp_hopopts;	/* Last received IPV6_RECVHOPOPTS */
	ip6_dest_t	*tcp_dstopts;	/* Last received IPV6_RECVDSTOPTS */
	ip6_dest_t	*tcp_rtdstopts;	/* Last recvd IPV6_RECVRTHDRDSTOPTS */
	ip6_rthdr_t	*tcp_rthdr;	/* Last received IPV6_RECVRTHDR */
	uint_t		tcp_hopoptslen;
	uint_t		tcp_dstoptslen;
	uint_t		tcp_rtdstoptslen;
	uint_t		tcp_rthdrlen;

	uint_t		tcp_drop_opt_ack_cnt; /* # tcp generated optmgmt */
	ip6_pkt_t	tcp_sticky_ipp;			/* Sticky options */
#define	tcp_ipp_fields	tcp_sticky_ipp.tcp_ipp_fields	/* valid fields */
#define	tcp_ipp_ifindex	tcp_sticky_ipp.tcp_ipp_ifindex	/* pktinfo ifindex */
#define	tcp_ipp_addr	tcp_sticky_ipp.tcp_ipp_addr  /* pktinfo src/dst addr */
#define	tcp_ipp_hoplimit	tcp_sticky_ipp.tcp_ipp_hoplimit
#define	tcp_ipp_hopoptslen	tcp_sticky_ipp.tcp_ipp_hopoptslen
#define	tcp_ipp_rtdstoptslen	tcp_sticky_ipp.tcp_ipp_rtdstoptslen
#define	tcp_ipp_rthdrlen	tcp_sticky_ipp.tcp_ipp_rthdrlen
#define	tcp_ipp_dstoptslen	tcp_sticky_ipp.tcp_ipp_dstoptslen
#define	tcp_ipp_hopopts		tcp_sticky_ipp.tcp_ipp_hopopts
#define	tcp_ipp_rtdstopts	tcp_sticky_ipp.tcp_ipp_rtdstopts
#define	tcp_ipp_rthdr		tcp_sticky_ipp.tcp_ipp_rthdr
#define	tcp_ipp_dstopts		tcp_sticky_ipp.tcp_ipp_dstopts
#define	tcp_ipp_nexthop		tcp_sticky_ipp.tcp_ipp_nexthop
} tcp_t;

/*
 * Adding a new field to tcp_t or tcpb_t :
 * If the information represented by the field is required even in the
 * TIME_WAIT state, it must be part of tcpb_t. Otherwise it must be part
 * of tcp_t. In other words, the tcp_t captures the information that is
 * not required, after a connection has entered the TIME_WAIT state.
 */
typedef struct tcp_base_s {
	struct tcp_base_s *tcpb_bind_hash; /* Bind hash chain */
	struct tcp_base_s **tcpb_ptpbhn;
				/* Pointer to previous bind hash next. */
	struct tcp_base_s *tcpb_conn_hash; /* Connect hash chain */
	struct tcp_base_s **tcpb_ptpchn;
				/* Pointer to previous conn hash next. */
	struct tcp_base_s *tcpb_time_wait_next;
				/* Pointer to next T/W block */
	struct tcp_base_s *tcpb_time_wait_prev;
				/* Pointer to previous T/W next */
	clock_t	tcpb_time_wait_expire;
				/* time in hz when t/w expires */
	clock_t	tcpb_last_rcv_lbolt;
				/* lbolt on last packet, used for PAWS */
	int32_t	tcpb_state;
	int32_t	tcpb_rcv_ws;		/* My window scale power */
	int32_t	tcpb_snd_ws;		/* Sender's window scale power */
	uint32_t tcpb_ts_recent;	/* Timestamp of earliest unacked */
					/*  data segment */
	clock_t	tcpb_rto;		/* Round trip timeout */

	uint32_t
		tcpb_snd_ts_ok  : 1,
		tcpb_snd_ws_ok  : 1,
		tcpb_is_secure  : 1,
		tcpb_reuseaddr	: 1,	/* SO_REUSEADDR "socket" option. */
		tcpb_exclbind	: 1,	/* ``exclusive'' binding */

		tcpb_junk_fill_thru_bit_31 : 27;
	uint32_t tcpb_snxt;		/* Senders next seq num */
	uint32_t tcpb_swnd;		/* Senders window (relative to suna) */
	uint32_t tcpb_mss;		/* Max segment size */
	uint32_t tcpb_iss;		/* Initial send seq num */
	uint32_t tcpb_rnxt;		/* Seq we expect to recv next */
	uint32_t tcpb_rwnd;		/* Current receive window */
	kmutex_t	tcpb_reflock;	/* Protects tcp_refcnt */
	ushort_t	tcpb_refcnt;	/* Number of pending upstream msg */
	union {
		struct {
			uchar_t	v4_ttl;
				/* Dup of tcp_ipha.iph_type_of_service */
			uchar_t	v4_tos; /* Dup of tcp_ipha.iph_ttl */
		} v4_hdr_info;
		struct {
			uint_t	v6_vcf;		/* Dup of tcp_ip6h.ip6h_vcf */
			uchar_t	v6_hops;	/* Dup of tcp_ip6h.ip6h_hops */
		} v6_hdr_info;
	} tcpb_hdr_info;
#define	tcpb_ttl	tcpb_hdr_info.v4_hdr_info.v4_ttl
#define	tcpb_tos	tcpb_hdr_info.v4_hdr_info.v4_tos
#define	tcpb_ip6_vcf	tcpb_hdr_info.v6_hdr_info.v6_vcf
#define	tcpb_ip6_hops	tcpb_hdr_info.v6_hdr_info.v6_hops
	in6_addr_t	tcpb_remote_v6;	/* true remote address - needed for */
					/* source routing. */
	in6_addr_t	tcpb_bound_source_v6;	/* IP address in bind_req */
	in6_addr_t	tcpb_ip_src_v6;	/* same as tcp_iph.iph_src. */
#ifdef _KERNEL
/* Note: V4_PART_OF_V6 is meant to be used only for _KERNEL defined stuff */
#define	tcpb_remote		V4_PART_OF_V6(tcpb_remote_v6)
#define	tcpb_bound_source	V4_PART_OF_V6(tcpb_bound_source_v6)
#define	tcpb_ip_src		V4_PART_OF_V6(tcpb_ip_src_v6)
#endif /* _KERNEL */
	/*
	 * These fields contain the same information as tcp_tcph->th_*port.
	 * However, the lookup functions can not use the header fields
	 * since during IP option manipulation the tcp_tcph pointer
	 * changes.
	 */
	union {
		struct {
			in_port_t	tcpu_fport;	/* Remote port */
			in_port_t	tcpu_lport;	/* Local port */
		} tcpu_ports1;
		uint32_t		tcpu_ports2;	/* Rem port, */
							/* local port */
					/* Used for TCP_MATCH performance */
	} tcpb_tcpu;
#define	tcpb_fport	tcpb_tcpu.tcpu_ports1.tcpu_fport
#define	tcpb_lport	tcpb_tcpu.tcpu_ports1.tcpu_lport
#define	tcpb_ports	tcpb_tcpu.tcpu_ports2
	/*
	 * IP sends back 2 mblks with the unbind ACK for handling
	 * IPSEC policy for detached connections. Following two fields
	 * are initialized then.
	 */
	mblk_t		*tcpb_ipsec_out;
	mblk_t		*tcpb_ipsec_req_in;
	tcp_t		*tcpb_tcp;
	/*
	 * IP format that packets transmitted from this struct should use.
	 * Value can be IPV4_VERSION or IPV6_VERSION.  Determines whether
	 * IP+TCP header template above stores an IPv4 or IPv6 header.
	 */
	ushort_t	tcpb_ipversion;
	uint_t		tcpb_bound_if;	/* IPV6_BOUND_IF */

	uid_t		tcpb_ownerid;	/* uid of process that did open */
#define	tcp_bind_hash		tcp_base->tcpb_bind_hash
#define	tcp_ptpbhn		tcp_base->tcpb_ptpbhn
#define	tcp_conn_hash		tcp_base->tcpb_conn_hash
#define	tcp_ptpchn		tcp_base->tcpb_ptpchn
#define	tcp_time_wait_next	tcp_base->tcpb_time_wait_next
#define	tcp_time_wait_prev	tcp_base->tcpb_time_wait_prev
#define	tcp_time_wait_expire	tcp_base->tcpb_time_wait_expire
#define	tcp_last_rcv_lbolt	tcp_base->tcpb_last_rcv_lbolt
#define	tcp_state		tcp_base->tcpb_state
#define	tcp_rcv_ws		tcp_base->tcpb_rcv_ws
#define	tcp_snd_ws		tcp_base->tcpb_snd_ws
#define	tcp_ts_recent		tcp_base->tcpb_ts_recent
#define	tcp_rto			tcp_base->tcpb_rto
#define	tcp_snd_ts_ok		tcp_base->tcpb_snd_ts_ok
#define	tcp_snd_ws_ok		tcp_base->tcpb_snd_ws_ok
#define	tcp_is_secure		tcp_base->tcpb_is_secure
#define	tcp_snxt		tcp_base->tcpb_snxt
#define	tcp_swnd		tcp_base->tcpb_swnd
#define	tcp_mss			tcp_base->tcpb_mss
#define	tcp_iss			tcp_base->tcpb_iss
#define	tcp_rnxt		tcp_base->tcpb_rnxt
#define	tcp_rwnd		tcp_base->tcpb_rwnd
#define	tcp_reflock		tcp_base->tcpb_reflock
#define	tcp_refcnt		tcp_base->tcpb_refcnt
#define	tcp_remote_v6		tcp_base->tcpb_remote_v6
#define	tcp_remote		tcp_base->tcpb_remote
#define	tcp_bound_source_v6	tcp_base->tcpb_bound_source_v6
#define	tcp_bound_source	tcp_base->tcpb_bound_source
#define	tcp_lport		tcp_base->tcpb_tcpu.tcpu_ports1.tcpu_lport
#define	tcp_fport		tcp_base->tcpb_tcpu.tcpu_ports1.tcpu_fport
#define	tcp_ports		tcp_base->tcpb_tcpu.tcpu_ports2
#define	tcp_ipsec_out		tcp_base->tcpb_ipsec_out
#define	tcp_ipsec_req_in	tcp_base->tcpb_ipsec_req_in
#define	tcp_ipversion		tcp_base->tcpb_ipversion
#define	tcp_bound_if		tcp_base->tcpb_bound_if
#define	tcp_reuseaddr		tcp_base->tcpb_reuseaddr
#define	tcp_exclbind		tcp_base->tcpb_exclbind
#define	tcp_ownerid		tcp_base->tcpb_ownerid
} tcpb_t;
#endif	/* (defined(_KERNEL) || defined(_KMEMUSER)) */

/* Contract private interface between TCP and Clustering. */

#define	CL_TCPI_V1	1	/* cl_tcpi_version number */

typedef struct cl_tcp_info_s {
	ushort_t	cl_tcpi_version;	/* cl_tcp_info_t's version no */
	ushort_t	cl_tcpi_ipversion;	/* IP version */
	int32_t		cl_tcpi_state;		/* TCP state */
	in_port_t	cl_tcpi_lport;		/* Local port */
	in_port_t	cl_tcpi_fport;		/* Remote port */
	in6_addr_t	cl_tcpi_laddr_v6;	/* Local IP address */
	in6_addr_t	cl_tcpi_faddr_v6;	/* Remote IP address */
#ifdef _KERNEL
/* Note: V4_PART_OF_V6 is meant to be used only for _KERNEL defined stuff */
#define	cl_tcpi_laddr	V4_PART_OF_V6(cl_tcpi_laddr_v6)
#define	cl_tcpi_faddr	V4_PART_OF_V6(cl_tcpi_faddr_v6)
#endif	/* _KERNEL */
} cl_tcp_info_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_TCP_H */
