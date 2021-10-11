/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)tcp_trace.h	1.4	98/11/16 SMI"

#ifndef _TCP_TRACE_H
#define	_TCP_TRACE_H

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * TCP trace buffer size definitions
 *
 * NOTE: Do NOT change the number of TCP_TRACE_NREC. Its is fixed to 10
 * because of limitation of strlog() function which max buffer size is 1024.
 */
#define	TCP_TRACE_NREC		10	/* # of trace packets in tcp_t	*/

/*
 * Flags for tcp_trec_pkttype
 */
#define	TCP_TRACE_NOENT		0	/* No data in this record	*/
#define	TCP_TRACE_SEND_PKT	1	/* Send data record		*/
#define	TCP_TRACE_RECV_PKT	2	/* received data record		*/

/*
 * Trace record structure
 *
 * NOTE: tcp_data has a IP packet size. When we format a data which is
 * loged by strlog(), TCP data size is calculated.
 */
typedef struct tcp_trace_rec {
	hrtime_t	tcptr_iotime;		/* Time of I/O */
	uint32_t	tcptr_tcp_seq;		/* Sequence number */
	uint32_t	tcptr_tcp_ack;		/* Acknowledgement number */
	uint16_t	tcptr_tcp_data;		/* TCP data size */
	uint16_t	tcptr_tcp_win;		/* Window size */
	uint8_t		tcptr_pkttype;		/* 1=sent, 2=received */
	uint8_t		tcptr_ip_hdr_len;	/* Byte len of IP header */
	uint8_t		tcptr_tcp_hdr_len;	/* Byte len of TCP header */
	uint8_t		tcptr_tcp_flags[1];	/* TCP packet flag */
} tcptrcrec_t;

/*
 * Trace buffer record structrure
 */
typedef struct tcp_trace_header {
	hrtime_t	tcptrh_conn_time;	/* time of connection init. */
	int		tcptrh_currec;		/* current trace record */
	int		tcptrh_send_total;	/* # traced sent packets */
	int		tcptrh_recv_total;	/* # traced received packets */
	tcptrcrec_t	tcptrh_evts[TCP_TRACE_NREC];	/* event records */
} tcptrch_t;

/*
 * Control structure for each open TCP stream (tcp_t) with TCP trace buffer
 */
typedef struct tcp_with_trace {
	tcp_t tcp;
	tcptrch_t tcp_traceinfo;
} tcptrc_t;

/*
 * tcp trace function
 */
extern	void	tcp_record_trace(tcp_t *tcp, mblk_t *mp, int flag);

/*
 * Macro for tcp trace
 */
#define	TCP_RECORD_TRACE(tcp, mp, flag) {	\
	if (tcp->tcp_tracing) {			\
		tcp_record_trace(tcp, mp, flag);	\
	}					\
}

#ifdef  __cplusplus
}
#endif

#endif  /* _TCP_TRACE_H */
