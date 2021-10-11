/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ncaproto.c	1.19	99/12/06 SMI"

const char ncaproto_version[] = "@(#)ncaproto.c	1.19	99/12/06 SMI";

#define	_IP_C

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#define	_SUN_TPI_VERSION 2
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/atomic.h>

#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/isa_defs.h>
#include <sys/md5.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/tcp.h>

#include <sys/systm.h>
#include <sys/param.h>

#include <sys/strick.h>

#include <netinet/igmp_var.h>
#include <inet/ip.h>

#include <sys/atomic.h>

#include "nca.h"
#include "ncadoorhdr.h"
#include "ncalogd.h"

/*
 * TCP options struct returned from nca_tcp_parse_options().
 */
typedef struct tcp_opt_s {
	uint32_t	tcp_opt_mss;
	uint32_t	tcp_opt_wscale;
	uint32_t	tcp_opt_ts_val;
	uint32_t	tcp_opt_ts_ecr;
} tcp_opt_t;

extern sqfan_t nca_if_sqf;
extern nca_cpu_t *nca_gv;
extern void nca_cpu_g(if_t *);

extern int servicing_interrupt();
extern void nca_logger_init(void *);
extern void nca_wput(queue_t *, mblk_t *);
extern int nca_http(conn_t *);
extern boolean_t nca_logger_help;
extern ulong_t nca_logger_help_given;
extern squeue_t nca_log_squeue;
extern nca_fio_t logfio;

static mblk_t *nca_tcp_get_seg_mp(conn_t *, uint32_t, int32_t *);
static int nca_tcp_ss_rexmit(conn_t *, boolean_t *, uint_t *);
static int nca_tcp_output(conn_t *, boolean_t *, uint_t *);
void nca_tcp_input(void *, mblk_t *, void *);
static void nca_tcp_init_values(conn_t *);
static mblk_t *nca_tcp_xmit_mp(conn_t *, mblk_t *, int32_t, int32_t *,
    mblk_t **, uint32_t, int32_t);
static void nca_tcp_xmit_ctl(char *, conn_t *, mblk_t *, uint32_t, uint32_t,
    int);
static void nca_tcp_xmit_early_reset(char *, if_t *, mblk_t *, uint32_t,
    uint32_t, int);
static void nca_tcp_xmit_listeners_reset(if_t *, mblk_t *);
static int nca_tcp_clean_death(conn_t *, int);
static void nca_tcp_iss_init(conn_t *);
static mblk_t *nca_tcp_ack_mp(conn_t *);
static char *nca_tcp_display(conn_t *);
static unsigned int nca_tcp_timer(conn_t *);
static int nca_tcp_parse_options(tcph_t *, tcp_opt_t *);
static mblk_t *nca_tcp_reass(conn_t *, mblk_t *, uint32_t, uint32_t *);
static void nca_tcp_reass_elim_overlap(conn_t *, mblk_t *);

void nca_tw_reap(tw_t *);
static void nca_tw_delete(conn_t *);
static void nca_tw_add(conn_t *);
static void nca_tw_timer(tw_t *);
static void nca_tw_fire(void *);

static void nca_ti_delete(te_t *);
static void nca_ti_reap(ti_t *);
static void nca_ti_add(conn_t *, clock_t);
static void nca_ti_timer(ti_t *);

static void nca_tcp_close_mpp(mblk_t **);

static struct kmem_cache *nca_tb_ti_cache;


#define	TCP_CHECKSUM_OFFSET		16
#ifdef	_BIG_ENDIAN
#define	IP_TCP_CSUM_COMP	IPPROTO_TCP
#else
#define	IP_TCP_CSUM_COMP	(IPPROTO_TCP << 8)
#endif

/*
 * This implementation follows the 4.3BSD interpretation of the urgent
 * pointer and not RFC 1122. Switching to RFC 1122 behavior would cause
 * incompatible changes in protocols like telnet and rlogin.
 */
#define	TCP_OLD_URP_INTERPRETATION	1

#define	TCP_XMIT_LOWATER	2048
#define	TCP_XMIT_HIWATER	8192
#define	TCP_RECV_LOWATER	2048
#define	TCP_RECV_HIWATER	8192

/*
 * ndd(1M) definitions - for now a subset of ip/tcp.
 */

#define	MS	1L
#define	SECONDS	(1000 * MS)
#define	MINUTES	(60 * SECONDS)
#define	HOURS	(60 * MINUTES)
#define	DAYS	(24 * HOURS)

#define	PARAM_MAX (~(uint32_t)0)
#define	PARAML_MAX (~(ulong_t)0)

#include "ncandd.h"

/* Max size IP datagram is 64k - 1 */
#define	TCP_MSS_MAX	((64 * 1024 - 1) - (sizeof (ipha_t) + sizeof (tcph_t)))

/* Largest TCP port number */
#define	TCP_MAX_PORT	(64 * 1024 - 1)

uint32_t nca_major_version = 1;
uint32_t nca_minor_version = 2;
uint32_t nca_httpd_version = NCA_HTTP_VERSION1;
uint32_t nca_logd_version = NCA_LOG_VERSION1;

/* The door path to HTTP server. */
#define	PATH_MAX	1024
char nca_httpd_door_path[PATH_MAX];

/*
 * All of these are alterable, within the min/max values given, at run time.
 * Note that the default value of "tcp_time_wait_interval" is four minutes,
 * per the TCP spec.
 */
/* BEGIN CSTYLED */
ncaparam_t	nca_param_arr[] = {
 /*min		max		value		name */
 { 0,		1,		0,	"ip_forwarding"},
 { 0,		1,		0,	"ip_respond_to_address_mask_broadcast"},
 { 0,		1,		1,	"ip_respond_to_echo_broadcast"},
 { 0,		1,		1,	"ip_respond_to_timestamp"},
 { 0,		1,		1,	"ip_respond_to_timestamp_broadcast"},
 { 0,		1,		1,	"ip_send_redirects"},
 { 0,		1,		1,	"ip_forward_directed_broadcasts"},
 { 0,		10,		0,	"ip_debug"},
 { 0,		10,		0,	"ip_mrtdebug"},
 { 5000,	999999999,	30000,	"ip_ire_cleanup_interval" },
 { 60000,	999999999,	1200000,"ip_ire_flush_interval" },
 { 60000,	999999999,	60000,	"ip_ire_redirect_interval" },
 { 1,		255,		255,	"ip_def_ttl" },
 { 0,		1,		1,	"ip_forward_src_routed"},
 { 0,		256,		32,	"ip_wroff_extra" },
 { 5000,	999999999,	600000, "ip_ire_pathmtu_interval" },
 { 8,		65536,		64,	"ip_icmp_return_data_bytes" },
 { 0,		1,		0,	"ip_send_source_quench" },
 { 0,		1,		1,	"ip_path_mtu_discovery" },
 { 0,		240,		30,	"ip_ignore_delete_time" },
 { 0,		1,		0,	"ip_ignore_redirect" },
 { 0,		1,		1,	"ip_output_queue" },
 { 1,		254,		1,	"ip_broadcast_ttl" },
 { 0,		99999,		500,	"ip_icmp_err_interval" },
 { 0,		999999999,	1000000,"ip_reass_queue_bytes" },
 { 0,		1,		0,	"ip_strict_dst_multihoming" },
 { 1,		8192,		256,	"ip_addrs_per_if"},
 { 1*SECONDS,	10*MINUTES,	1*MINUTES,	"tcp_time_wait_interval"},
 { 1,		PARAM_MAX,	128,		"tcp_conn_req_max_q" },
 { 0,		PARAM_MAX,	1024,		"tcp_conn_req_max_q0" },
 { 1,		1024,		1,		"tcp_conn_req_min" },
 { 0*MS,	20*SECONDS,	0*MS,		"tcp_conn_grace_period" },
 { 128, 	(1<<30),	256*1024,	"tcp_cwnd_max" },
 { 0,		10,		0,		"tcp_debug" },
 { 1024,	(32*1024),	1024,		"tcp_smallest_nonpriv_port"},
 { 1*SECONDS,	PARAM_MAX,	3*MINUTES,	"tcp_ip_abort_cinterval"},
 { 1*SECONDS,	PARAM_MAX,	3*MINUTES,	"tcp_ip_abort_linterval"},
 { 500*MS,	PARAM_MAX,	8*MINUTES,	"tcp_ip_abort_interval"},
 { 1*SECONDS,	PARAM_MAX,	10*SECONDS,	"tcp_ip_notify_cinterval"},
 { 500*MS,	PARAM_MAX,	10*SECONDS,	"tcp_ip_notify_interval"},
 { 1,		255,		255,		"tcp_ip_ttl"},
 { 10*SECONDS,	10*DAYS,	2*HOURS,	"tcp_keepalive_interval"},
 { 1,		100,		2,		"tcp_maxpsz_multiplier" },
 { 1,		TCP_MSS_MAX,	536,		"tcp_mss_def"},
 { 1,		TCP_MSS_MAX,	TCP_MSS_MAX,	"tcp_mss_max"},
 { 1,		TCP_MSS_MAX,	1,		"tcp_mss_min"},
 { 1,		(64*1024)-1,	(4*1024)-1,	"tcp_naglim_def"},
 { 1*MS,	20*SECONDS,	3*SECONDS,	"tcp_rexmit_interval_initial"},
 { 1*MS,	2*HOURS,	60*SECONDS,	"tcp_rexmit_interval_max"},
 { 1*MS,	2*HOURS,	500,		"tcp_rexmit_interval_min"},
 { 0,		256,		32,		"tcp_wroff_xtra" },
 { 1*MS,	1*MINUTES,	50*MS,		"tcp_deferred_ack_interval" },
 { 0,		16,		0,		"tcp_snd_lowat_fraction" },
 { 0,		128000,		0,		"tcp_sth_rcv_hiwat" },
 { 0,		128000,		0,		"tcp_sth_rcv_lowat" },
 { 1,		10000,		3,		"tcp_dupack_fast_retransmit" },
 { 0,		1,		0,		"tcp_ignore_path_mtu" },
 { 0,		128*1024,	16384,		"tcp_rcv_push_wait" },
 { 1024,	TCP_MAX_PORT,	32*1024,	"tcp_smallest_anon_port"},
 { 1024,	TCP_MAX_PORT,	TCP_MAX_PORT,	"tcp_largest_anon_port"},
 { TCP_XMIT_HIWATER, (1<<30), TCP_XMIT_HIWATER,"tcp_xmit_hiwat"},
 { TCP_XMIT_LOWATER, (1<<30), TCP_XMIT_LOWATER,"tcp_xmit_lowat"},
 { TCP_RECV_HIWATER, (1<<30), TCP_RECV_HIWATER,"tcp_recv_hiwat"},
 { 1,		65536,		4,		"tcp_recv_hiwat_minmss"},
 { 1*SECONDS,	PARAM_MAX,	675*SECONDS,	"tcp_fin_wait_2_flush_interval"},
 { 0,		TCP_MSS_MAX,	64,		"tcp_co_min"},
 { 8192,	(1<<30),	1024*1024,	"tcp_max_buf"},
 { 1,		PARAM_MAX,	1,		"tcp_zero_win_probesize"},
/*
 * Question:  What default value should I set for tcp_strong_iss?
 */
 { 0,		2,		0,		"tcp_strong_iss"},
 { 0,		65536,		12,		"tcp_rtt_updates"},
 { 0,		1,		0,		"tcp_wscale_always"},
 { 0,		1,		0,		"tcp_tstamp_always"},
 { 0,		1,		0,		"tcp_tstamp_if_wscale"},
 { 0*MS,	2*HOURS,	0*MS,		"tcp_rexmit_interval_extra"},
 { 0,		16,		8,		"tcp_deferred_acks_max"},
 { 1,		16384,		2,		"tcp_slow_start_after_idle"},
 { 1,		4,		2,		"tcp_slow_start_initial"},
 { 10*MS,	50*MS,		20*MS,		"tcp_co_timer_interval"},
 { 0,		2,		1,		"tcp_sack_permitted"},
 { 0,		1,		1,		"nca_log_cycle"},
 { 0,		1,		0,		"no_caching"},
 { 0,		PARAM_MAX,     	0,		"nca_log_size"},
/*
 * tcp_drop_oob MUST be last in the list. This variable is only used
 * when using tcp to test another tcp. The need for it will go away
 * once we have packet shell scripts to test urgent pointers.
 */
#ifdef DEBUG
 { 0,		1,		0,		"tcp_drop_oob"},
#endif
};
/* END CSTYLED */


static int		nca_param_get(queue_t *q, mblk_t *mp, caddr_t cp);
static boolean_t	nca_param_register(ncaparam_t *ncapa, int cnt);
static int		nca_param_set(queue_t *q, mblk_t *mp, char *value,
				caddr_t cp);

caddr_t	nca_g_nd;	/* Head of 'named dispatch' variable list */

/*
 * TCP reassembly macros.  We hide starting and ending sequence numbers in
 * b_next and b_prev of messages on the reassembly queue.  The messages are
 * chained using b_cont.  These macros are used in tcp_reass() so we don't
 * have to see the ugly casts and assignments.
 */
#define	TCP_REASS_SEQ(mp)		((uint32_t)((mp)->b_next))
#define	TCP_REASS_SET_SEQ(mp, u)	((mp)->b_next = (mblk_t *)(u))
#define	TCP_REASS_END(mp)		((uint32_t)((mp)->b_prev))
#define	TCP_REASS_SET_END(mp, u)	((mp)->b_prev = (mblk_t *)(u))

#define	TCP_TIMER_RESTART(connp, ms) {					\
	clock_t exec = lbolt + MSEC_TO_TICK(ms);			\
	void	*ep = connp->tcp_ti.ep;					\
									\
	if (ep == NULL || exec != connp->tcp_ti.tbp->exec) {		\
		if (ep != NULL) {					\
			nca_ti_delete(&connp->tcp_ti);			\
		}							\
		nca_ti_add(connp, exec);				\
	}								\
}

/*
 * RFC1323-recommended phrasing of TSTAMP option, for easier parsing
 */

#ifdef _BIG_ENDIAN
#define	TCPOPT_NOP_NOP_TSTAMP ((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | \
	(TCPOPT_TSTAMP << 8) | 10)
#else
#define	TCPOPT_NOP_NOP_TSTAMP ((10 << 24) | (TCPOPT_TSTAMP << 16) | \
	(TCPOPT_NOP << 8) | TCPOPT_NOP)
#endif

/*
 * Parameters for TCP Initial Sequence Number (ISS) generation.  The
 * ISS is calculated by adding two components: a time component which
 * grows by 128,000 every second; and an "extra" component that grows
 * either by 64,000 or by a random amount centered approximately on
 * 64,000, for every connection.  This causes the the ISS generator to
 * cycle every 9 hours if no TCP connections are made, and faster if
 * connections are made.
 *
 * A third method for generating ISS is prescribed by Steve Bellovin.
 * This involves adding time, the 64,000 per connection, and a
 * one-way hash (MD5) of the connection ID <sport, dport, src, dst>, a
 * "truly" random (per RFC 1750) number, and a console-entered password.
 */
#define	ISS_INCR (125*1024)
#define	ISS_NSEC_DIV  (8000)

static uint32_t nca_tcp_iss_incr_extra;	/* Incremented for each connection */

/*
 * Flags returned from tcp_parse_options.
 */
#define	TCP_OPT_MSS_PRESENT	1
#define	TCP_OPT_WSCALE_PRESENT	2
#define	TCP_OPT_TSTAMP_PRESENT	4
#define	TCP_OPT_SACK_OK_PRESENT	8
#define	TCP_OPT_SACK_PRESENT	16

/* TCP option length */
#define	TCPOPT_NOP_LEN		1
#define	TCPOPT_MAXSEG_LEN	4
#define	TCPOPT_WS_LEN		3
#define	TCPOPT_REAL_WS_LEN	(TCPOPT_WS_LEN+1)
#define	TCPOPT_TSTAMP_LEN	10
#define	TCPOPT_REAL_TS_LEN	(TCPOPT_TSTAMP_LEN+2)
#define	TCPOPT_SACK_OK_LEN	2
#define	TCPOPT_REAL_SACK_OK_LEN	(TCPOPT_SACK_OK_LEN+2)
#define	TCPOPT_REAL_SACK_LEN	4
#define	TCPOPT_MAX_SACK_LEN	36
#define	TCPOPT_HEADER_LEN	2

/* TCP cwnd burst factor. */
#define	TCP_CWND_INFINITE	65535
#define	TCP_CWND_SS		3

static struct kmem_cache *conn_cache;

/*
 * TCP_CONN_HASH_SIZE - make it a prime number.
 */

int nca_conn_hash_size = 0;		/* set in /etc/system */

static connf_t *nca_conn_fanout;
int nca_conn_fanout_size;

#define	IP_TCP_CONN_HASH(ip_src, ports) \
	((unsigned)(ntohl(ip_src) ^ (ports >> 24) ^ (ports >> 16) \
	^ (ports >> 8) ^ ports) % nca_conn_fanout_size)

#define	IP_TCP_CONN_MATCH(p, src, dst, ports)			\
	((p)->conn_ports == (ports) &&				\
	    (p)->faddr == (src) &&				\
	    (p)->laddr == (dst))

/* Counter for the number of segment with URG set. */
static uint32_t nca_tcp_in_urg = 0;

void
nca_conn_init(void)
{
	int i;
	int sizes[] = P2Ps();


	conn_cache = kmem_cache_create("nca_conn_cache", sizeof (conn_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);
	nca_tb_ti_cache = kmem_cache_create("nca_tb_ti_cache", sizeof (tb_t),
	    0, NULL, NULL, NULL, NULL, NULL, 0);

	/*
	 * Find the nearst P2P value >= nca_conn_hash_size, start at
	 * 2^9 (or 383) as a reasonable (read small) size, for high
	 * throughput systems it's recomended that nca_conn_hash_size
	 * be set in /etc/system to a value > connections/S * 60 / 4.
	 */
	for (i = 9; i < sizeof (sizes) / sizeof (*sizes) - 1; i++) {
		if (sizes[i] >= nca_conn_hash_size) {
			break;
		}
	}
	if ((nca_conn_fanout_size = sizes[i]) == 0) {
		/* Out of range, use the 2^16 value */
		nca_conn_fanout_size = sizes[16];
	}
	nca_conn_fanout = (connf_t *)kmem_zalloc(nca_conn_fanout_size *
				sizeof (*nca_conn_fanout), KM_SLEEP);
	for (i = 0; i < nca_conn_fanout_size; i++) {
		mutex_init(&nca_conn_fanout[i].lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
}

void
nca_conn_fini(void)
{
	int	i;
	connf_t	*connfp;
	conn_t	*connp;
	mblk_t	*mp;

	for (i = 0; i < nca_conn_fanout_size; i++) {
		connfp = &nca_conn_fanout[i];
		mutex_destroy(&connfp->lock);
		while ((connp = connfp->head) != NULL) {
			CONN_REFHOLD(connp);
			connfp->head = connp->hashnext;
			if ((mp = connp->mac_mp) != NULL) {
				freeb(mp);
			}
			if ((mp = connp->tcp_rcv_head) != NULL) {
				freemsg(mp);
			}
			if ((mp = connp->fill_mp) != NULL) {
				freemsg(mp);
			}
			if ((mp = connp->fill_fmp) != NULL) {
				freemsg(mp);
			}
			if ((mp = connp->tcp_xmit_head) != NULL) {
				freemsg(mp);
			}
			if (connp->twlbolt != NCA_TW_NONE) {
				/* On a if_t's tcp_tw list */
				nca_tw_delete(connp);
			}
			if (connp->tcp_ti.ep != NULL) {
				/* On a if_t's tcp_ti list */
				nca_ti_delete(&connp->tcp_ti);
			}
			if ((mp = connp->req_mp) != NULL) {
				freeb(mp);
			}
			kmem_cache_free(conn_cache, connp);
		}
	}
	kmem_free(nca_conn_fanout,
	    nca_conn_fanout_size * sizeof (*nca_conn_fanout));
	kmem_cache_destroy(nca_tb_ti_cache);
	kmem_cache_destroy(conn_cache);
}

/*
 * Clean up the b_next and b_prev fields of every mblk pointed at by *mpp.
 * Some stream heads get upset if they see these later on as anything but NULL.
 */
static void
nca_tcp_close_mpp(mblk_t **mpp)
{
	mblk_t	*mp;

	if ((mp = *mpp) != NULL) {
		do {
			mp->b_next = NULL;
			mp->b_prev = NULL;
		} while ((mp = mp->b_cont) != NULL);
		freemsg(*mpp);
		*mpp = NULL;
	}
}

static void
nca_conn_fr(conn_t *connp, boolean_t unlock)
{
	mblk_t	*mp;

	ASSERT(MUTEX_HELD(&connp->hashfanout->lock));
	ASSERT(connp->ref == 0);
	ASSERT(connp->tcp_refed == 0);

	if (connp->hashnext)
		connp->hashnext->hashprev = connp->hashprev;
	if (connp->hashprev)
		connp->hashprev->hashnext = connp->hashnext;
	else
		connp->hashfanout->head = connp->hashnext;
	if (unlock)
		mutex_exit(&connp->hashfanout->lock);

	if ((mp = connp->mac_mp) != NULL) {
		connp->mac_mp = NULL;
		freeb(mp);
	}
	nca_tcp_close_mpp(&connp->tcp_xmit_head);
	nca_tcp_close_mpp(&connp->tcp_rcv_head);
	nca_tcp_close_mpp(&connp->tcp_reass_head);
	if ((mp = connp->fill_mp) != NULL) {
		connp->fill_mp = NULL;
		freemsg(mp);
	}
	if ((mp = connp->fill_fmp) != NULL) {
		connp->fill_fmp = NULL;
		freemsg(mp);
	}
	if (connp->twlbolt != NCA_TW_NONE) {
		/* On a if_t's tcp_tw list */
		nca_tw_delete(connp);
	}
	if (connp->tcp_ti.ep != NULL) {
		/* On a if_t's tcp_ti list */
		nca_ti_delete(&connp->tcp_ti);
	}
	if ((mp = connp->req_mp) != NULL) {
		connp->req_mp = NULL;
		freeb(mp);
	}
}

static conn_t *
nca_conn_alloc(if_t *ifp, connf_t *connfp)
{
	conn_t	*p;
	tw_t	*twp = nca_gv[CPU->cpu_seqid].tcp_tw;
	mblk_t	*mac_mp = NULL;
	boolean_t have_reclaim = false;

	/* Try to reclaim a TIME_WAIT conn_t before allocating memory */
	for (p = twp->head; p != NULL && p->twlbolt <= lbolt; p = p->twnext) {
		if (connfp != p->hashfanout) {
			if (mutex_tryenter(&p->hashfanout->lock) == 0) {
				/* Avoid A <> B deadlock; try next */
				NCA_DEBUG_COUNTER(&nca_conn_tw2, 1);
				continue;
			}
		}

		if (p->twprev == NULL) {
			if ((twp->head = p->twnext) != NULL) {
				twp->head->twprev = NULL;
				NCA_TW_LBOLTS(twp, twp->head->twlbolt);
			} else {
				twp->tail = NULL;
				NCA_TW_LBOLTS(twp, NCA_TW_NONE);
			}
		} else {
			if ((p->twprev->twnext = p->twnext) != NULL) {
				p->twnext->twprev = p->twprev;
			} else {
				twp->tail = p->twprev;
			}
		}
		p->twlbolt = NCA_TW_NONE;
		NCA_DEBUG_COUNTER(&tw_on, -1);
		NCA_DEBUG_COUNTER(&nca_conn_tw, 1);
		NCA_DEBUG_COUNTER(&tw_reclaim, 1);

		/*
		 * CONN_REFRELE() is inlined here since we need to
		 * free any resources referenced by the conn_t without
		 * freeing the conn_t itself.
		 */
		p->ref--;
		ASSERT(p->ref == 0);
		if (ifp->mac_length == p->mac_length) {
			/* Same mac_length, so save and reuse mac_mp */
			mac_mp = p->mac_mp;
			p->mac_mp = NULL;
		}

		/*
		 * If connfp is the same as the fanout for the reclaimed
		 * conn_t, we need to leave the connfp locked.
		 */
		nca_conn_fr(p, (connfp != p->hashfanout));
		have_reclaim = true;
		break;
	}

	if (!have_reclaim) {
		if (twp->head == NULL || twp->head->twlbolt > lbolt)
			NCA_DEBUG_COUNTER(&nca_conn_tw1, 1);

		p = kmem_cache_alloc(conn_cache, KM_NOSLEEP);
		if (p == NULL) {
			NCA_DEBUG_COUNTER(&nca_conn_kmem_fail, 1);
			return (NULL);
		}
		NCA_COUNTER((ulong_t *)&nca_cbmem, sizeof (*p));
		NCA_DEBUG_COUNTER(&nca_conn_count, 1);
		NCA_DEBUG_COUNTER(&nca_conn_kmem, 1);
		p->ref = 0;
	}

	bzero(p, sizeof (*p));

	p->mac_length = ifp->mac_length;
	if (p->mac_length == 0) {
		if (mac_mp != NULL) {
			freeb(mac_mp);
		}
		p->mac_mp = NULL;
	} else if (mac_mp != NULL) {
		p->mac_mp = mac_mp;
	} else {
		if ((p->mac_mp = allocb(p->mac_length, BPRI_HI)) == NULL) {
			/* allocb() failure, cleanup and return */
			kmem_cache_free(conn_cache, p);
			NCA_COUNTER((ulong_t *)&nca_cbmem, -(sizeof (*p)));
			NCA_DEBUG_COUNTER(&nca_conn_count, -1);
			NCA_DEBUG_COUNTER(&nca_conn_allocb_fail, 1);
			return (NULL);
		}
	}

	return (p);
}

static void
nca_conn_init_values(conn_t *p, if_t *ifp, connf_t *connfp, mblk_t *mp,
    ipaddr_t src, ipaddr_t dst, uint32_t ports)
{
	p->ref = 1;
	p->tcp_refed = 1;
	p->tcp_close = 0;

	p->http_persist = 0;
	p->http_was_persist = 0;

	p->create = lbolt;

	/* insert on head of list */
	if ((p->hashnext = connfp->head) != NULL)
		connfp->head->hashprev = p;
	p->hashprev = NULL;
	p->hashfanout = connfp;
	connfp->head = p;

	p->laddr = dst;
	p->faddr = src;
	p->conn_ports = ports;
	p->ifp = ifp;

	if (p->mac_length > 0) {
		bcopy(ifp->mac_mp->b_rptr, p->mac_mp->b_rptr, p->mac_length);
		p->mac_mp->b_wptr += p->mac_length;
		/* stuff incoming src phys addr into the mac's dst */
		{
			int	addr_len = ifp->mac_addr_len;
			uint8_t *pre = mp->b_rptr - ifp->mac_length + addr_len;
			uint8_t *mac = p->mac_mp->b_rptr;

			while (addr_len-- > 0)
				*mac++ = *pre++;
		}
	}
}

static void
nca_conn_reinit_values(conn_t *p)
{
	/* Save any state we want to preserve across the bzero() below */
	conn_t	*hashnext = p->hashnext;
	conn_t	*hashprev = p->hashprev;
	connf_t	*hashfanout = p->hashfanout;
	ipaddr_t laddr = p->laddr;
	ipaddr_t faddr = p->faddr;
	uint32_t ports = p->conn_ports;
	if_t	*ifp = p->ifp;
	size_t	mac_length = p->mac_length;
	mblk_t	*mac_mp = p->mac_mp;

	/*
	 * Put a REFHOLD on the conn_t, then cleanup any leftover
	 * state from the previous instance and reinitialize.
	 */
	p->ref++;
	if (p->tcp_xmit_head != NULL) {
		NCA_DEBUG_COUNTER(&nca_conn_reinit_cnt, 1);
		nca_tcp_close_mpp(&p->tcp_xmit_head);
		p->tcp_xmit_tail = NULL;
		p->tcp_xmit_tail_unsent = 0;
		p->tcp_unsent = 0;
	}
	nca_tcp_close_mpp(&p->tcp_rcv_head);
	p->tcp_rcv_tail = NULL;
	p->tcp_rcv_cnt = 0;
	nca_tcp_close_mpp(&p->tcp_reass_head);
	p->tcp_reass_tail = NULL;
	if (p->twlbolt != NCA_TW_NONE) {
		/* On a if_t's tcp_tw list */
		nca_tw_delete(p);
	}
	if (p->tcp_ti.ep != NULL) {
		/* On a if_t's tcp_ti list */
		nca_ti_delete(&p->tcp_ti);
	}

	/* Zero the conn_t and restore any preserved state from above */
	bzero(p, sizeof (*p));
	p->hashnext = hashnext;
	p->hashprev = hashprev;
	p->hashfanout = hashfanout;
	p->laddr = laddr;
	p->faddr = faddr;
	p->conn_ports = ports;
	p->ifp = ifp;
	p->mac_length = mac_length;
	p->mac_mp = mac_mp;

	/* Init any other state */
	p->ref = 2;
	p->tcp_refed = 1;
	p->tcp_close = 0;

	p->http_persist = 0;
	p->http_was_persist = 0;

	p->create = lbolt;
}

/*
 */

void
nca_conn_free(conn_t *connp)
{
	nca_conn_fr(connp, true);
	kmem_cache_free(conn_cache, connp);
	NCA_COUNTER((ulong_t *)&nca_cbmem, -(sizeof (*connp)));
	NCA_DEBUG_COUNTER(&nca_conn_count, -1);
}

/*
 * ip_input() - called as the per if_t squeue_t inq proc function.
 *
 */

boolean_t
nca_ip_input(if_t *ifp, mblk_t *mp)
{
	ipha_t	*ipha = NULL;
	ipaddr_t dst;
	mblk_t	*mp1;
	uint_t	pkt_len;
	ssize_t	len;
	uint32_t u1;
	uint32_t u2;
	uint16_t *up;
	int	offset;
	int	refed = 0;

#ifdef	BEANS
	bean_ts_t ts;
	bean_ts_t *tsp;
#endif

#define	rptr	((uchar_t *)ipha)

	/* mblk has multiple references, do we care? */
	if (mp->b_datap->db_ref > 1)
		refed++;

	ipha = (ipha_t *)mp->b_rptr;

	len = mp->b_wptr - rptr;

	/* IP header ptr not aligned? */
	if (!OK_32PTR(rptr))
		goto notaligned;

	/* IP header not complete in first mblock */
	if (len < IP_SIMPLE_HDR_LENGTH)
		goto not1st;

	pkt_len = ntohs(ipha->ipha_length);

	/* too short? */
	if (len < pkt_len)
		goto tooshort;

	/* set dst before fragment handling - it needs dst as well */
	dst = ipha->ipha_dst;

	/* packet is multicast */
	if (CLASSD(dst))
		goto multicast;

	/* u1 is # words of IP options */
	u1 = ipha->ipha_version_and_hdr_length - (uchar_t)((IP_VERSION << 4)
	    + IP_SIMPLE_HDR_LENGTH_IN_WORDS);

	/* IP options present */
	if (u1)
		goto ipoptions;

	/* Check the IP header checksum.  */
#define	uph	((uint16_t *)ipha)
	u1 = uph[0] + uph[1] + uph[2] + uph[3] + uph[4] + uph[5] + uph[6] +
		uph[7] + uph[8] + uph[9];
#undef  uph

	/* finish doing IP checksum */
	u1 = (u1 & 0xFFFF) + (u1 >> 16);
	u1 = ~(u1 + (u1 >> 16)) & 0xFFFF;

	/* verify checksum */
	if (u1 && u1 != 0xFFFF)
		goto cksumerror;

	/* packet part of fragmented IP packet? */
	u2 = ntohs(ipha->ipha_fragment_offset_and_flags);
	u1 = u2 & (IPH_MF | IPH_OFFSET);
	if (u1)
		goto fragmented;

	/* we only talk TCP!!! */
	if (ipha->ipha_protocol != IPPROTO_TCP) goto pass;

	/* does packet contain IP+TCP headers? */
	if (len < (IP_SIMPLE_HDR_LENGTH + TCP_MIN_HEADER_LENGTH))
		goto tcppullup;

	up = (uint16_t *)(rptr + IP_SIMPLE_HDR_LENGTH);

	/* TCP options present? */
	offset = ((uchar_t *)up)[12] >> 4;
	if (offset != 5) {
		if (offset < 5) goto tcpbadoff;
		/*
		 * There must be TCP options.
		 * Make sure we can grab them.
		 */
		offset <<= 2;
		offset += IP_SIMPLE_HDR_LENGTH;
		if (len < offset) goto tcppullup2;
	}

	/* multiple mblocks of tcp data? */
	if ((mp1 = mp->b_cont) != NULL) {
		/* more then two? */
		if (mp1->b_cont) goto multipkttcp;
		len += mp1->b_wptr - mp1->b_rptr;
	}

	/* too long? */
	if (len > pkt_len) {
		len -= pkt_len;
		/*
		 * Make sure we have data length consistent with the IP header.
		 */
		if (!mp->b_cont)
			mp->b_wptr = rptr + pkt_len;
		else
			(void) adjmsg(mp, -len);
	}

	/* part of pseudo checksum */
	u1 = (uint_t)pkt_len - IP_SIMPLE_HDR_LENGTH;
#ifdef  _BIG_ENDIAN
	u1 += IPPROTO_TCP;
#else
	u1 = ((u1 >> 8) & 0xFF) + (((u1 & 0xFF) + IPPROTO_TCP) << 8);
#endif

#define	iphs	((uint16_t *)ipha)

	u1 += iphs[6] + iphs[7] + iphs[8] + iphs[9];

	/* verify TCP checksum */
	if (IP_CSUM(mp, (int32_t)((uchar_t *)up - rptr), u1)) goto tcpcksumerr;

	/* At this point, we accept and deliver the packet to TCP */
	BUMP_MIB(ip_mib.ipInReceives);
	BUMP_MIB(ip_mib.ipInDelivers);

#undef	iphs
#undef	rptr

	TDELTATS(*TSP(mp), ip_input[0]);

	/*
	 * Send the packet onto TCP.
	 */
	if (nca_deferred_oq_if) {
		if (ifp->ouq == NULL) {
			nca_cpu_g(ifp);
		}
		squeue_willproxy(ifp->ouq);

		TDELTATS(*TSP(mp), rput[1]);
	}

	if (ifp->inq == NULL) {
		nca_cpu_g(ifp);
	}
	if (nca_fanout_iq_if) {
		squeue_willproxy(ifp->inq);
		sqfan_fill(&nca_if_sqf, mp, ifp);
		squeue_proxy(NULL, ifp->inq);

		TDELTATS(*TSP(mp), rput[2]);
	} else {
		if (ifp->inq->sq_bind != CPU->cpu_id) {
			/*
			 * Interrupt not on the same CPU as last, either
			 * interrupts aren't bound to (or have affinity
			 * for) a CPU, or interrupts have affinity for a
			 * CPU but the affinity has changed.
			 *
			 * For now just punt, do a fill() on the inq so
			 * the packet processng will be deferred to the
			 * correct CPU (either by a worker thread or some
			 * other interrupt thread currently in the inq).
			 *
			 * XXX need to support migration of if_t's from
			 * one CPU (read inq) to another as the default
			 * interrupt mode on Intel platforms is affinity
			 * vs bind on sparc and the affinity may change.
			 */
			if (nca_gv[CPU->cpu_seqid].if_inq == NULL) {
				nca_cpu_g(ifp);
			}
			NCA_COUNTER(&ipwrongcpu, 1);
			squeue_fill(ifp->inq, mp, ifp);
		} else {
			NCA_DEBUG_COUNTER(&iponcpu, 1);
			squeue_enter(ifp->inq, mp, ifp);
		}

		TDELTATS(*TSP(mp), rput[3]);
	}

	if (nca_deferred_oq_if) {
		squeue_proxy(NULL, ifp->ouq);

		TDELTATS(*TSP(mp), rput[4]);
	}
	return (true);

notaligned:
	goto pass;

not1st:
	goto pass;

tooshort:
	BUMP_MIB(ip_mib.ipInHdrErrors);
	goto error;

ipoptions:
	goto pass;

multicast:
	goto pass;

cksumerror:
	BUMP_MIB(ip_mib.ipInCksumErrs);
	goto error;

tcppullup:
	goto pass;

tcppullup2:
	goto pass;

tcpbadoff:
	BUMP_MIB(ip_mib.tcpInErrs);
	goto error;

multipkttcp:;
{
	int n = 2;

	while ((mp1 = mp1->b_cont) != NULL)
		n++;
	goto pass;
}

tcpcksumerr:
	BUMP_MIB(ip_mib.tcpInErrs);
	goto error;

fragmented:
	goto pass;
pass:
	NCA_DEBUG_COUNTER(&ipsendup, 1);
	return (false);

error:
	freemsg(mp);
	return (true);
}

static void
nca_ip_output(conn_t *connp, mblk_t *mp)
{
	if_t	*ifp = connp->ifp;
	ipha_t	*ipha;
#define	rptr	((uchar_t *)ipha)
	uint32_t	v_hlen_tos_len;
	ipaddr_t	dst;
	uint32_t	ttl_protocol;
	ipaddr_t	src;
	size_t		hlen;
	uint16_t	*up;

/* Macros to extract header fields from data already in registers */
#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#define	LENGTH	(v_hlen_tos_len & 0xFFFF)
#define	PROTO	(ttl_protocol & 0xFF)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#define	LENGTH	((v_hlen_tos_len >> 24) | ((v_hlen_tos_len >> 8) & 0xFF00))
#define	PROTO	(ttl_protocol >> 8)
#endif

	ipha = (ipha_t *)mp->b_rptr;
	v_hlen_tos_len = ((uint32_t *)ipha)[0];
	dst = ipha->ipha_dst;

	ipha->ipha_ttl = connp->tcp_ipha.ipha_ttl;

	ASSERT(ipha->ipha_ident == 0 || ipha->ipha_ident == NO_IP_TP_CKSUM);

	src = ipha->ipha_src;

	ttl_protocol =  atomic_add_32_nv(&ifp->ip_ident, 1);

#ifndef _BIG_ENDIAN
	ttl_protocol = (ttl_protocol << 8) | ((ttl_protocol >> 8) & 0xff);
#endif
	ipha->ipha_ident = (uint16_t)ttl_protocol;

	if (!src) goto nosrcaddr;

	ttl_protocol = ((uint16_t *)ipha)[4];

	/* pseudo checksum (do it in parts for IP header checksum) */
	dst = (dst >> 16) + (dst & 0xFFFF);
	src = (src >> 16) + (src & 0xFFFF);
	dst += src;

#define	IPH_TCPH_CHECKSUMP(ipha, hlen) \
	    ((uint16_t *)(((uchar_t *)ipha)+(hlen+TCP_CHECKSUM_OFFSET)))

	hlen = (V_HLEN & 0xF) << 2;
	up = IPH_TCPH_CHECKSUMP(ipha, hlen);
	mp->b_datap->db_struioflag = NULL;
	*up = IP_CSUM(mp, hlen, dst + IP_TCP_CSUM_COMP);

	/* checksum */
	dst += ttl_protocol;

	/* NCA does not do PMTU discovery.  Zero out the info in IP header. */
	ipha->ipha_fragment_offset_and_flags = 0;

	/* checksum */
	dst += ipha->ipha_ident;

	/* checksum */
	dst += (v_hlen_tos_len >> 16)+(v_hlen_tos_len & 0xFFFF);
	dst += ipha->ipha_fragment_offset_and_flags;

	/* calculate hdr checksum */
	dst = ((dst & 0xFFFF) + (dst >> 16));
	dst = ~(dst + (dst >> 16));
	{
		ipaddr_t u1 = dst;
		ipha->ipha_hdr_checksum = (uint16_t)u1;
	}

	if (! ip_g_forward)
		hlen = connp->mac_length;
	else
		hlen = 0;

	if (hlen > 0) {
		mblk_t	*mp1 = connp->mac_mp;

		/* if fp hdr doesn'f fit */
		if ((rptr - mp->b_datap->db_base) < hlen) goto noprepend;
		mp->b_rptr -= hlen;
		bcopy(mp1->b_rptr, mp->b_rptr, hlen);
	}

#if	0
{
	uint8_t *p = mp->b_rptr;

	prom_printf("nca_ip_output: %x: %02x%02x %02x%02x %02x%02x %02x%02x ",
	    connp->ifp->wqp, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	prom_printf("%02x%02x %02x%02x %02x%02x %02x%02x\n",
	    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	prom_printf("\t: %02x%02x %02x%02x %02x%02x %02x%02x ",
	    p[16], p[17], p[18], p[19], p[20], p[21], p[22], p[23]);
	prom_printf("%02x%02x %02x%02x %02x%02x %02x%02x\n",
	    p[24], p[25], p[26], p[27], p[28], p[29], p[30], p[31]);
	prom_printf("\t: %02x%02x %02x%02x %02x%02x %02x%02x ",
	    p[32], p[33], p[34], p[35], p[36], p[37], p[38], p[39]);
	prom_printf("%02x%02x %02x%02x %02x%02x %02x%02x\n",
	    p[40], p[41], p[42], p[43], p[44], p[45], p[46], p[47]);
}
#endif

	if (! ip_g_forward && hlen > 0) {
		/*
		 * Send our output packets to the NIC.
		 */
		if (nca_deferred_oq_if) {
			squeue_fill(ifp->ouq, mp, ifp->wqp);
		} else {
			nca_wput(ifp->wqp, mp);
		}
	} else {
		/*
		 * IP forwarding enabled or no mac (e.g. MCI Net),
		 * so send our output packets up to IP for routing.
		 */
		putnext(ifp->rqp, mp);
	}
	return;

#undef	rptr
#undef	V_HLEN
#undef	LENGTH
#undef	PROTO
#undef	IPH_TCPH_CHECKSUMP

nosrcaddr:
	printf("nca_ip_output(): no IP source address\n");
	goto error;

noprepend:
	printf("nca_ip_output(): no mac prepend\n");
	goto error;

error:
	freemsg(mp);
}

/* The minimum of smoothed mean deviation in RTO calculation. */
#define	TCP_SD_MIN	400

/*
 * Set RTO for this connection.  The formula is from Jacobson and Karels'
 * "Congestion Avoidance and Control" in SIGCOMM '88.  The variable names
 * are the same as those in Appendix A.2 of that paper.
 *
 * m = new measurement
 * sa = smoothed RTT average (8 * average estimates).
 * sv = smoothed mean deviation (mdev) of RTT (4 * deviation estimates).
 */
static void
tcp_set_rto(conn_t *connp, clock_t rtt)
{
	long m = TICK_TO_MSEC(rtt);
	clock_t sa = connp->tcp_rtt_sa;
	clock_t sv = connp->tcp_rtt_sd;
	clock_t rto;

	BUMP_MIB(tcp_mib.tcpRttUpdate);
	connp->tcp_rtt_update++;

	/* tcp_rtt_sa is not 0 means this is a new sample. */
	if (sa != 0) {
		/*
		 * Update average estimator:
		 *	new rtt = 7/8 old rtt + 1/8 Error
		 */

		/* m is now Error in estimate. */
		m -= sa >> 3;
		if ((sa += m) <= 0) {
			/*
			 * Don't allow the smoothed average to be negative.
			 * We use 0 to denote reinitialization of the
			 * variables.
			 */
			sa = 1;
		}

		/*
		 * Update deviation estimator:
		 *	new mdev = 3/4 old mdev + 1/4 (abs(Error) - old mdev)
		 */
		if (m < 0)
			m = -m;
		m -= sv >> 2;
		sv += m;
	} else {
		/*
		 * This follows BSD's implementation.  So the reinitialized
		 * RTO is 3 * m.  We cannot go less than 2 because if the
		 * link is bandwidth dominated, doubling the window size
		 * during slow start means doubling the RTT.  We want to be
		 * more conservative when we reinitialize our estimates.  3
		 * is just a convenient number.
		 */
		sa = m << 3;
		sv = m << 1;
	}
	if (sv < TCP_SD_MIN) {
		/*
		 * We do not know that if sa captures the delay ACK
		 * effect as in a long train of segments, a receiver
		 * does not delay its ACKs.  So set the minimum of sv
		 * to be TCP_SD_MIN, which is default to 400 ms, twice
		 * of BSD DATO.  That means the minimum of mean
		 * deviation is 100 ms.
		 *
		 */
		sv = TCP_SD_MIN;
	}
	connp->tcp_rtt_sa = sa;
	connp->tcp_rtt_sd = sv;
	/*
	 * RTO = average estimates (sa / 8) + 4 * deviation estimates (sv)
	 *
	 * Add tcp_rexmit_interval extra in case of extreme environment
	 * where the algorithm fails to work.  The default value of
	 * tcp_rexmit_interval_extra should be 0.
	 *
	 * As we use a finer grained clock than BSD and update
	 * RTO for every ACKs, add in another .25 of RTT to the
	 * deviation of RTO to accomodate burstiness of 1/4 of
	 * window size.
	 */
	rto = (sa >> 3) + sv + tcp_rexmit_interval_extra + (sa >> 5);

	if (rto > tcp_rexmit_interval_max) {
		connp->tcp_rto = tcp_rexmit_interval_max;
	} else if (rto < tcp_rexmit_interval_min) {
		connp->tcp_rto = tcp_rexmit_interval_min;
	} else {
		connp->tcp_rto = rto;
	}

	/* Now, we can reset tcp_timer_backoff to use the new RTO... */
	connp->tcp_timer_backoff = 0;
}

static boolean_t
nca_tcp_xmit(conn_t *p, int usable)
{
	mblk_t	*mp;
	int	len;
	dblk_t	*dbp;
	node_t	*np;

	int	bytes = usable - p->tcp_unsent;
	int	n = 0;

	ASSERT(p->req_np != NULL);

	for (;;) {
		if (p->xmit.sz == 0) {
			/*
			 * No more data for current data area, each node_t
			 * can access up to 3 data areas, the first has
			 * already been processed (initalized below), here
			 * we sequence through the second and third (if any).
			 */
			if (p->xmit.fnp != NULL) {
				/*
				 * Second data area is the node_t.fileback
				 * file node_t, we save the current xmit
				 * struct and process the fnp as np.
				 */
				len = p->xmit.np->fileoff;
				p->xmit_save = p->xmit;	/* struct assignment */
				p->xmit_save.fnp = NULL;
				p->xmit.np = p->xmit.fnp;
				p->xmit.dp = p->xmit.np->data;
				p->xmit.cp = (p->xmit.np->cksum) + 1;
				p->xmit.sz = p->xmit.np->datasz;
				if (len > 0) {
					p->xmit.dp += len;
					p->xmit.cp += (len / p->tcp_mss);
					p->xmit.sz -= len;
					if (p->tcp_mss < p->xmit.np->datasz)
						p->xmit.len = p->tcp_mss - len;
					else
						p->xmit.len = p->xmit.sz;
				} else {
					p->xmit.len = 0;
				}
				p->xmit.fnp = NULL;
			} else if (p->xmit_save.np != NULL) {
				/*
				 * Done processing the second data area, so
				 * restore the saved xmit struct for processing
				 * the third data are (if any).
				 */
				p->xmit = p->xmit_save;
				p->xmit_save.np = NULL;
				p->xmit.sz = p->xmit.np->datasz -
				    p->xmit.np->hlen;
				p->xmit.len = 0;
			} else {
				/*
				 * Third data area is the remainder of the
				 * node_t.data (i.e. datasz - hlen).
				 */
				len = p->xmit.np->datasz;
				len -= (p->xmit.dp - p->xmit.np->data);
				if (len > 0)
					p->xmit.sz = len;
				else
					p->xmit.sz = 0;
				p->xmit.len = 0;
			}
		}
		if (p->xmit.sz == 0) {
			/*
			 * No more data for current node_t, walk the singly
			 * linked list of node_t's and initialize the state
			 * for the data area sequencing above.
			 */
			if (p->xmit.np == NULL ||
			    (p->xmit.np = p->xmit.np->next) == NULL) {
				/* No more data */
				break;
			}
			p->xmit.dp = p->xmit.np->data;
			p->xmit.cp = (p->xmit.np->cksum) + 1;
			if ((p->xmit.sz = p->xmit.np->hlen) == 0) {
				p->xmit.sz = p->xmit.np->datasz;
			}
			p->xmit.fnp = p->xmit.np->fileback;
			/*
			 * First data area is node_t.hlen, if 0
			 * all, of the node_t.data (i.e. datasz).
			 */
			if (p->xmit.sz == 0) {
				/*
				 * No data in the first data area,
				 * so back to the top to sequence.
				 */
				continue;
			}
			p->xmit.len = 0;
		}
		if (bytes == 0) {
			/* No more bytes needed */
			break;
		}
		if ((len = p->xmit.len) == 0 &&
		    (len = p->xmit.sz) > p->tcp_mss) {
			/*
			 * No initial segment length and xmit data size
			 * greater then max segment size, so use mss.
			 */
			len = p->tcp_mss;
		}
		if (len > bytes) {
			/*
			 * Current segment size > bytes left, so wait til
			 * next time (more bytes requested).
			 */
			break;
		}
		if (nca_debug) {
			prom_printf("nca_tcp_xmit: %d: %p %p %p %d %d %p\n",
			    bytes, (void *)p->xmit.np, (void *)p->xmit.dp,
			    (void *)p->xmit.cp, p->xmit.sz, p->xmit.len,
			    (void *)p->xmit.fnp);
		}
		/* Get node_t for ref, if not a head use the head */
		if ((np = p->xmit.np)->frtn.free_arg == NULL)
			np = np->back;

		mutex_enter(&np->lock);
		np->ref++;
		mutex_exit(&np->lock);
		mp = desballoc((unsigned char *)p->xmit.dp, len, BPRI_HI,
		    &np->frtn);


		if (mp == NULL) {
			mutex_enter(&np->lock);
			np->ref--;
			mutex_exit(&np->lock);
			return (false);
		}

		mp->b_queue = (queue_t *)np;
		mp->b_wptr += len;
		dbp = mp->b_datap;
		if (p->xmit.len > 0) {
			/*
			 * A segment with an offset, so segment
			 * can't use the pre calculated cksum.
			 */
			p->xmit.len = 0;
			dbp->db_struioflag = NULL;
			p->xmit.cp++;
		} else {
			/* Pre claculated cksum valid */
			dbp->db_struioflag = STRUIO_IP;
			dbp->db_struiobase = mp->b_rptr;
			dbp->db_struiolim = mp->b_wptr;
			dbp->db_struioptr = mp->b_wptr;
			*(uint16_t *)dbp->db_struioun.data = *p->xmit.cp++;
		}
		p->xmit.dp += len;
		p->xmit.sz -= len;
		bytes -= len;
		n++;
		if (p->tcp_xmit_head == NULL) {
			p->tcp_xmit_head = mp;
			p->tcp_xmit_last = mp;
		} else {
			p->tcp_xmit_last->b_cont = mp;
		}
		if (p->tcp_xmit_tail == NULL) {
			p->tcp_xmit_tail = mp;
			p->tcp_xmit_tail_unsent = len;
		}
		p->tcp_xmit_last = mp;
		p->tcp_unsent += len;
	}
	NCA_COUNTER((ulong_t *)&nca_mbmem,
	    (sizeof (mblk_t) + sizeof (dblk_t)) * n);
	NCA_DEBUG_COUNTER(&nca_desballoc, n);
	if (p->xmit.np == NULL && (mp = p->fill_mp) != NULL) {
		/* All done with the node_t data, so freeb() our ref */
		p->fill_mp = NULL;
		freeb(mp);
		p->req_np = NULL;
		p->req_tag = 0;
		if ((mp = p->fill_fmp) != NULL) {
			/* And our file node_t ref */
			p->fill_fmp = NULL;
			freeb(mp);
		}
	}
	return (true);
}

/*
 * Add a new piece to the tcp reassembly queue.  If the gap at the beginning
 * is filled, return as much as we can.  The message passed in may be
 * multi-part, chained using b_cont.  "start" is the starting sequence
 * number for this piece.
 */
static mblk_t *
nca_tcp_reass(conn_t *connp, mblk_t *mp, uint32_t start, uint32_t *flagsp)
{
	uint32_t	end;
	mblk_t		*mp1;
	mblk_t		*mp2;
	mblk_t		*next_mp;
	uint32_t	u1;

	/* Walk through all the new pieces. */
	do {
		ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
		    (uintptr_t)INT_MAX);
		end = start + (int)(mp->b_wptr - mp->b_rptr);
		next_mp = mp->b_cont;
		if (start == end) {
			/* Empty.  Blast it. */
			freeb(mp);
			continue;
		}
		mp->b_cont = NULL;
		TCP_REASS_SET_SEQ(mp, start);
		TCP_REASS_SET_END(mp, end);
		mp1 = connp->tcp_reass_tail;
		if (!mp1) {
			connp->tcp_reass_tail = mp;
			connp->tcp_reass_head = mp;
			/* A fresh gap.	 Make sure we get an ACK out. */
			*flagsp |= TH_ACK_NEEDED;
			BUMP_MIB(tcp_mib.tcpInDataUnorderSegs);
			UPDATE_MIB(tcp_mib.tcpInDataUnorderBytes, end - start);
			continue;
		}
		/* New stuff completely beyond tail? */
		if (SEQ_GEQ(start, TCP_REASS_END(mp1))) {
			/* Link it on end. */
			mp1->b_cont = mp;
			connp->tcp_reass_tail = mp;
			BUMP_MIB(tcp_mib.tcpInDataUnorderSegs);
			UPDATE_MIB(tcp_mib.tcpInDataUnorderBytes, end - start);
			continue;
		}
		mp1 = connp->tcp_reass_head;
		u1 = TCP_REASS_SEQ(mp1);
		/* New stuff at the front? */
		if (SEQ_LT(start, u1)) {
			/* Yes... Check for overlap. */
			mp->b_cont = mp1;
			connp->tcp_reass_head = mp;
			nca_tcp_reass_elim_overlap(connp, mp);
			continue;
		}
		/*
		 * The new piece fits somewhere between the head and tail.
		 * We find our slot, where mp1 precedes us and mp2 trails.
		 */
		for (; (mp2 = mp1->b_cont) != NULL; mp1 = mp2) {
			u1 = TCP_REASS_SEQ(mp2);
			if (SEQ_LEQ(start, u1))
				break;
		}
		/* Link ourselves in */
		mp->b_cont = mp2;
		mp1->b_cont = mp;

		/* Trim overlap with following mblk(s) first */
		nca_tcp_reass_elim_overlap(connp, mp);

		/* Trim overlap with preceding mblk */
		nca_tcp_reass_elim_overlap(connp, mp1);

	} while (start = end, mp = next_mp);
	mp1 = connp->tcp_reass_head;
	/* Anything ready to go? */
	if (TCP_REASS_SEQ(mp1) != connp->tcp_rnxt)
		return (NULL);
	/* Eat what we can off the queue */
	for (;;) {
		mp = mp1->b_cont;
		end = TCP_REASS_END(mp1);
		TCP_REASS_SET_SEQ(mp1, 0);
		TCP_REASS_SET_END(mp1, 0);
		if (!mp) {
			connp->tcp_reass_tail = NULL;
			break;
		}
		if (end != TCP_REASS_SEQ(mp)) {
			mp1->b_cont = NULL;
			break;
		}
		mp1 = mp;
	}
	mp1 = connp->tcp_reass_head;
	connp->tcp_reass_head = mp;
	if (mp) {
		/*
		 * We filled in the hole at the front, but there is still
		 * a gap.  Make sure another ACK goes out.
		 */
		*flagsp |= TH_ACK_NEEDED;
	}
	return (mp1);
}

/* Eliminate any overlap that mp may have over later mblks */
static void
nca_tcp_reass_elim_overlap(conn_t *connp, mblk_t *mp)
{
	uint32_t	end;
	mblk_t		*mp1;
	uint32_t	u1;

	end = TCP_REASS_END(mp);
	while ((mp1 = mp->b_cont) != NULL) {
		u1 = TCP_REASS_SEQ(mp1);
		if (!SEQ_GT(end, u1))
			break;
		if (!SEQ_GEQ(end, TCP_REASS_END(mp1))) {
			mp->b_wptr -= end - u1;
			TCP_REASS_SET_END(mp, u1);
			BUMP_MIB(tcp_mib.tcpInDataPartDupSegs);
			UPDATE_MIB(tcp_mib.tcpInDataPartDupBytes, end - u1);
			break;
		}
		mp->b_cont = mp1->b_cont;
		TCP_REASS_SET_SEQ(mp1, 0);
		TCP_REASS_SET_END(mp1, 0);
		freeb(mp1);
		BUMP_MIB(tcp_mib.tcpInDataDupSegs);
		UPDATE_MIB(tcp_mib.tcpInDataDupBytes, end - u1);
	}
	if (!mp1)
		connp->tcp_reass_tail = mp;
}

/*
 * Extract option values from a tcp header.  We put any found values into the
 * tcpopt struct and return a bitmask saying which options were found.
 */
static int
nca_tcp_parse_options(tcph_t *tcph, tcp_opt_t *tcpopt)
{
	uchar_t		*endp;
	int		len;
	uint32_t	mss;
	uchar_t		*up = (uchar_t *)tcph;
	int		found = 0;

	endp = up + TCP_HDR_LENGTH(tcph);
	up += TCP_MIN_HEADER_LENGTH;
	while (up < endp) {
		len = endp - up;
		switch (*up) {
		case TCPOPT_EOL:
			break;

		case TCPOPT_NOP:
			up++;
			continue;

		case TCPOPT_MAXSEG:
			if (len < TCPOPT_MAXSEG_LEN ||
			    up[1] != TCPOPT_MAXSEG_LEN)
				break;

			mss = BE16_TO_U16(up+2);
			/* Caller must handle tcp_mss_min and tcp_mss_max_* */
			tcpopt->tcp_opt_mss = mss;
			found |= TCP_OPT_MSS_PRESENT;

			up += TCPOPT_MAXSEG_LEN;
			continue;

		case TCPOPT_WSCALE:
			if (len < TCPOPT_WS_LEN || up[1] != TCPOPT_WS_LEN)
				break;

			if (up[2] > TCP_MAX_WINSHIFT)
				tcpopt->tcp_opt_wscale = TCP_MAX_WINSHIFT;
			else
				tcpopt->tcp_opt_wscale = up[2];
			found |= TCP_OPT_WSCALE_PRESENT;

			up += TCPOPT_WS_LEN;
			continue;

		case TCPOPT_SACK_PERMITTED:
			if (len < TCPOPT_SACK_OK_LEN ||
			    up[1] != TCPOPT_SACK_OK_LEN)
				break;
			found |= TCP_OPT_SACK_OK_PRESENT;
			up += TCPOPT_SACK_OK_LEN;
			continue;

		case TCPOPT_TSTAMP:
			if (len < TCPOPT_TSTAMP_LEN ||
			    up[1] != TCPOPT_TSTAMP_LEN)
				break;

			tcpopt->tcp_opt_ts_val = BE32_TO_U32(up+2);
			tcpopt->tcp_opt_ts_ecr = BE32_TO_U32(up+6);

			found |= TCP_OPT_TSTAMP_PRESENT;

			up += TCPOPT_TSTAMP_LEN;
			continue;

		default:
			if (len <= 1 || len < (int)up[1] || up[1] == 0)
				break;
			up += up[1];
			continue;
		}
		break;
	}
	return (found);
}

/*
 * nca_tcp_get_seg_mp() is called to get the pointer to a segment in the
 * send queue which starts at the given seq. no.
 *
 * Parameters:
 *	conn_t *connp: the conn instance pointer.
 *	uint32_t seq: the starting seq. no of the requested segment.
 *	int32_t *off: after the execution, *off will be the offset to
 *		the returned mblk which points to the requested seq no.
 *
 * Return:
 *	A mblk_t pointer pointing to the requested segment in send queue.
 */
static mblk_t *
nca_tcp_get_seg_mp(conn_t *connp, uint32_t seq, int32_t *off)
{
	int32_t	cnt;
	mblk_t	*mp;

	/* Defensive coding.  Make sure we don't send incorrect data. */
	if (SEQ_LT(seq, connp->tcp_suna) || SEQ_GEQ(seq, connp->tcp_snxt) ||
	    off == NULL) {
		return (NULL);
	}
	cnt = seq - connp->tcp_suna;
	mp = connp->tcp_xmit_head;
	while (cnt > 0 && mp) {
		cnt -= mp->b_wptr - mp->b_rptr;
		if (cnt < 0) {
			cnt += mp->b_wptr - mp->b_rptr;
			break;
		}
		mp = mp->b_cont;
	}
	ASSERT(mp != NULL);
	*off = cnt;
	return (mp);
}

/*
 * nca_tcp_ss_rexmit() is called in nca_tcp_input() to do slow start
 * retransmission after a timeout.
 *
 * To limit the number of duplicate segments, we limit the number of segment
 * to be sent in one time to tcp_snd_burst, the burst variable.
 */
static int
nca_tcp_ss_rexmit(conn_t *connp, boolean_t *http_check_needed, uint_t *flags)
{
	uint32_t	snxt;
	uint32_t	smax;
	int32_t		win;
	int32_t		mss;
	int32_t		off;
	int32_t		burst = connp->tcp_snd_burst;
	mblk_t		*snxt_mp;

	/*
	 * Note that tcp_rexmit can be set even though TCP has retransmitted
	 * all unack'ed segments.
	 */
	if (SEQ_LT(connp->tcp_rexmit_nxt, connp->tcp_rexmit_max)) {
		smax = connp->tcp_rexmit_max;
		snxt = connp->tcp_rexmit_nxt;
		if (SEQ_LT(snxt, connp->tcp_suna)) {
			snxt = connp->tcp_suna;
		}
		win = MIN(connp->tcp_cwnd, connp->tcp_swnd);
		win -= snxt - connp->tcp_suna;
		mss = connp->tcp_mss;
		snxt_mp = nca_tcp_get_seg_mp(connp, snxt, &off);

		while (SEQ_LT(snxt, smax) && (win > 0) &&
		    (burst > 0) && (snxt_mp != NULL)) {
			mblk_t	*xmit_mp;
			mblk_t	*old_snxt_mp = snxt_mp;
			int32_t	cnt = mss;

			if (win < cnt) {
				cnt = win;
			}
			if (SEQ_GT(snxt + cnt, smax)) {
				cnt = smax - snxt;
			}
			xmit_mp = nca_tcp_xmit_mp(connp, snxt_mp, cnt, &off,
			    &snxt_mp, snxt, B_TRUE);
			if (xmit_mp == NULL) {
				return (0);
			}
			nca_ip_output(connp, xmit_mp);

			snxt += cnt;
			win -= cnt;
			/*
			 * Update the send timestamp to avoid false
			 * retransmission.
			 */
			old_snxt_mp->b_prev = (mblk_t *)lbolt;
			BUMP_MIB(tcp_mib.tcpRetransSegs);
			UPDATE_MIB(tcp_mib.tcpRetransBytes, cnt);

			connp->tcp_rexmit_nxt = snxt;
			burst--;
		}
		TCP_TIMER_RESTART(connp, connp->tcp_rto);
		*flags &= ~TH_ACK_NEEDED;
		/*
		 * If we have transmitted all we have at the time
		 * we started the retranmission, we can leave
		 * the rest of the job to tcp_wput_slow().  But we
		 * need to check the send window first.  If the
		 * win is not 0, go on with tcp_wput_slow().
		 */
		if (SEQ_LT(snxt, smax) || win == 0) {
			return (0);
		}
	}
	/* Only call nca_tcp_output() if there is data to be sent. */
	if (connp->tcp_unsent || connp->xmit.np != NULL) {
		return (nca_tcp_output(connp, http_check_needed, flags));
	}
	return (0);
}

/*
 * To send new segments out.  Called in both nca_tcp_input() and
 * nca_tcp_ss_rexmit().
 */
static int
nca_tcp_output(conn_t *connp, boolean_t *http_check_needed, uint_t *flags)
{
	int	tail_unsent;
	mblk_t	*xmit_tail;
	int32_t	len;
	int	usable = connp->tcp_swnd;
	int32_t	offset;
	mblk_t	*offmp;
	mblk_t	*mp1;
	uchar_t *rptr;

	if ((connp->tcp_suna == connp->tcp_snxt) &&
	    (TICK_TO_MSEC(lbolt - connp->tcp_last_recv_time) >=
	    connp->tcp_rto)) {
		len = connp->tcp_mss;
		connp->tcp_cwnd = MAX(tcp_slow_start_after_idle * len,
		    MIN(4 * len, MAX(2 * len, 4380 / len * len)));
	}
	if (usable > connp->tcp_cwnd)
		usable = connp->tcp_cwnd;
	usable -= connp->tcp_snxt;
	usable += connp->tcp_suna;

	if (usable > connp->tcp_unsent &&
	    connp->xmit.np != NULL) {
		/* Need some data to xmit.  Try to get at least 5 segments. */
		if (connp->xmit.np->size == -1 ||
		    ! nca_tcp_xmit(connp, MAX(usable, 5 * connp->tcp_mss))) {
			(void) nca_tcp_clean_death(connp, ECONNRESET);
			return (1);
		}
		if (connp->xmit.np == NULL &&
		    connp->http_persist) {
			/* An HTTP persistent connection */
			*http_check_needed = true;
		}
	}

	if (usable > connp->tcp_unsent)
		usable = connp->tcp_unsent;

	tail_unsent = connp->tcp_xmit_tail_unsent;
	xmit_tail = connp->tcp_xmit_tail;
	for (;;) {
		/*
		 * Note that the way the send queue is filled
		 * in is such that each mblk is at most 1 MSS
		 * large.  That is why we can assume here that
		 * tail_unsent should not be larger than 1
		 * MSS.  And NCA does not do PMTUd, so
		 * MSS size will never change...
		 */
		len = tail_unsent;
		if (len > usable) {
			if (usable == 0)
				break;
			len = usable;
			if (nca_debug)
				printf("len > usable %d %d\n", len, usable);
		}

		if (len == 0) {
			/* This should not happen! */
			if (nca_debug) {
				if (xmit_tail == NULL) {
					printf("xmit tail NULL\n");
				} else {
					printf("xmit tail non NULL\n");
				}
			}
			break;
		}

		/* If no more data to send, then TH_FIN */
		if (connp->tcp_unsent == len &&
		    connp->xmit.np == NULL &&
		    connp->tcp_state == TCPS_ESTABLISHED &&
		    ! connp->http_persist) {
			connp->tcp_valid_bits |= TCP_FSS_VALID;
			connp->tcp_fss = connp->tcp_snxt + len;
		}
		rptr = xmit_tail->b_wptr - tail_unsent;
		offset = rptr - xmit_tail->b_rptr;

		if ((offset || len < tail_unsent) &&
		    connp->tcp_mss > len &&
		    !(connp->tcp_valid_bits & TCP_FSS_VALID)) {
			/*
			 * Sender silly-window avoidance.
			 * Ignore this if we are going to send
			 * a zero window probe out.
			 */
			if (len < (connp->tcp_max_swnd >> 1) &&
			    ! connp->tcp_zero_win_probe) {
				/*
				 * If the retransmit timer is
				 * not running we start it so
				 * that we will retransmit in
				 * the case when the receiver
				 * has decremented the window.
				 */
				TCP_TIMER_RESTART(connp, connp->tcp_rto);
				break;
			}
		}
		ASSERT(xmit_tail != NULL);

		mp1 = nca_tcp_xmit_mp(connp, xmit_tail, len,
		    &offset, &offmp, connp->tcp_snxt, 1);

		if (mp1 == NULL) {
			if_t	*ifp = connp->ifp;
			squeue_t *sqp = connp->inq;

			NCA_DEBUG_COUNTER(&nca_tcp_xmit_null, 1);
			if (! sqp->sq_isintr)
				squeue_nointr(sqp, NULL, ifp, 0);
			break;
		}
		if (mp1->b_cont == NULL) {
			NCA_DEBUG_COUNTER(&nca_tcp_xmit_null1, 1);
			break;
		}

		usable -= len;
		connp->tcp_unsent -= len;

		if (connp->tcp_fin_sent != true)
			connp->tcp_snxt += len;

		xmit_tail->b_prev = (mblk_t *)lbolt;
		nca_ip_output(connp, mp1);

		tail_unsent -= len;
		if (tail_unsent) {
			/*
			 * Didn't send the full mblk, so
			 * see if there's more to send?
			 */
			continue;
		}
		xmit_tail = xmit_tail->b_cont;
		if (xmit_tail == NULL) {
			/* All done? */
			ASSERT(connp->tcp_unsent == 0);
			tail_unsent = 0;
			break;
		}
		tail_unsent = (int)(xmit_tail->b_wptr -
				    xmit_tail->b_rptr);

	}
	connp->tcp_xmit_tail_unsent = tail_unsent;
	connp->tcp_xmit_tail = xmit_tail;
	if (connp->tcp_suna != connp->tcp_snxt) {
		*flags &= ~TH_ACK_NEEDED;
		TCP_TIMER_RESTART(connp, connp->tcp_rto);
	}
	return (0);
}

/*
 * Function called one of three ways:
 *
 * 1) a conn_t ctl call: (conn_t *)p, NULL, (int)arg
 *
 *	This method is used for a recursive call back into tcp_input() for
 * 	proxy processing of a conn_t by another, while a squeue_ctl() could
 *	be used it requires additional resources be allocated and tail proc
 *	on return, as we are already within the squeue_t perimeter it's more
 *	efficient and timely (e.g. a TCP_TIMER).
 *
 * 2) a squeue_ctl() via a squeue_t: (if_t *)p, mp, (squeue *)arg
 *
 *	This method is used for a call back into tcp_input() from outside
 *	the squeue_t perimeter.
 *
 * 3) a ip_input() via a squeue_t: (if_t *)p, mp, (squeue *)arg
 *
 *	This method is used by IP for in-bound TCP datagrams.
 */
void
nca_tcp_input(void *p, mblk_t *mp, void *arg)
{
	if_t		*ifp;
	conn_t		*connp;
	int32_t		bytes_acked;
	int32_t		gap;
	mblk_t		*mp1;
	mblk_t		*mp2;
	uint_t		flags;
	uint32_t	new_swnd = 0;
	uchar_t		*rptr;
	int32_t		rgap;
	uint32_t	seg_ack;
	int		seg_len;
	uint32_t	seg_seq;
	tcph_t		*tcph;
	int		urp;
	squeue_t	*inq;
	uint32_t	ports;
	uint32_t	u1;
	ipaddr_t	src;
	ipaddr_t	dst;
	ipha_t		*ipha;
	connf_t		*connfp;
	int		options = 0;
	tcp_opt_t	tcpopt;

#ifdef	BEANS
	bean_ts_t	ts = *TSP(mp);
	bean_ts_t	tsp = &ts;
#endif

	if (mp == NULL) {
		connp = (conn_t *)p;
		ifp = connp->ifp;
		if ((int)arg == CONN_TCP_TIMER) {
			CONN_REFHOLD(connp);
			flags = nca_tcp_timer(connp);
			goto xmit_check;
		} else {
			cmn_err(CE_PANIC,
			    "nca_tcp_input: unknown squeue_ctl of %d for %p:%p",
			    (int)arg, (void *)ifp, (void *)connp);
		}
	} else if (mp->b_rptr == (unsigned char *)&mp->b_datap) {
		/*
		 * A squeue_ctl() mblk, mblk contains a conn_t pointer
		 * and a flag value, do what's requested !!!
		 */
#ifdef	BEANS
		ts = *TSP(mp);
		tsp = &ts;
#endif
		ifp = (if_t *)p;
		inq = (squeue_t *)arg;
		arg = mp->b_datap;
		flags = mp->b_flag;
		mp1 = mp->b_cont;
		kmem_free(mp, sizeof (*mp));
		if (flags == CONN_MISS_DONE) {
			/*
			 * Post miss processing, first release the conn_t
			 * ref hold then if needed xmit kickoff processing.
			 */
			connp = (conn_t *)arg;
			if (! connp->tcp_refed) {
				/*
				 * Not refed by TCP, so the connection must
				 * have gone away while processing a request,
				 * so just free up any resources and return.
				 */
				if ((mp = connp->fill_mp) != NULL) {
					connp->fill_mp = NULL;
					freemsg(mp);
				}
				if ((mp = connp->fill_fmp) != NULL) {
					connp->fill_fmp = NULL;
					freemsg(mp);
				}
				if ((mp = connp->req_mp) != NULL) {
					connp->req_mp = NULL;
					freemsg(mp);
				}
				CONN_REFRELE(connp);
				return;
			}
			if (connp->fill_mp == NULL) {
				/*
				 * Empty response ???
				 *
				 * XXX Need to send an error response
				 * and move on !!!
				 */
				if ((mp = connp->req_mp) != NULL) {
					connp->req_mp = NULL;
					freemsg(mp);
				}
				CONN_REFRELE(connp);
				return;
			}

			if ((mp = connp->req_mp) != NULL) {
				connp->req_mp = NULL;
				freemsg(mp);
			}
			/*
			 * Note: the REFRELE for the REFHOLD applied
			 * for miss processing will be done later by
			 * the post tcp_input() code (and this REFHOLD
			 * will be used as the tcp_input() REFHOLD).
			 */
			flags = TH_XMIT_NEEDED;
			goto xmit_check;
		} else if (flags == IF_TIME_WAIT) {
			/*
			 * TIME_WAIT expiration post processing for all
			 * if_t(s) bound to a CPU, just do it and return.
			 */
			tw_t	*twp = (tw_t *)arg;

			twp->tid = 0;
			nca_tw_timer(twp);
			return;
		} else if (flags == IF_TCP_TIMER) {
			/*
			 * TCP TIMER expiration post processing for all
			 * if_t(s) bound to a CPU, just do it and return.
			 */
			ti_t	*tip = (ti_t *)arg;

			tip->tid = 0;
			nca_ti_timer(tip);
			return;
		} else {
			cmn_err(CE_PANIC,
			    "nca_tcp_input: unknown squeue_ctl of %d", flags);
			return;
		}
	} else {
		ifp = (if_t *)p;
		inq = (squeue_t *)arg;
	}

	/* Find a TCP connection for this packet. */
	rptr = mp->b_rptr;
	ipha = (ipha_t *)rptr;
	seg_len = IPH_HDR_LENGTH(rptr);
	tcph = (tcph_t *)&rptr[seg_len];
	src = ipha->ipha_src;
	dst = ipha->ipha_dst;
	ports = *(uint32_t *)tcph;

#ifdef _LITTLE_ENDIAN
	u1 = ((uint16_t *)tcph)[1] | (((uint16_t *)tcph)[0] << 16);
#else	/* _LITTLE_ENDIAN */
	u1 = ports;
#endif	/* _LITTLE_ENDIAN */

	connfp = &nca_conn_fanout[IP_TCP_CONN_HASH(src, u1)];
	mutex_enter(&connfp->lock);
	for (connp = connfp->head; connp != NULL; connp = connp->hashnext) {
		if (IP_TCP_CONN_MATCH(connp, src, dst, ports))
			break;
	}

	TDELTATS(*TSP(mp), ip_input[1]);

	flags = (unsigned int)tcph->th_flags[0] & 0xFF;
	if (connp == NULL) {
		if ((flags & (TH_ACK | TH_SYN)) != TH_SYN) {
			/* Not TCP SYN packet */
			mutex_exit(&connfp->lock);
			nca_tcp_xmit_listeners_reset(ifp, mp);
			return;
		}
		connp = nca_conn_alloc(ifp, connfp);
		if (connp == NULL) {
			mutex_exit(&connfp->lock);
			NCA_DEBUG_COUNTER(&nca_conn_NULL1, 1);
			freemsg(mp);
			return;
		}
		nca_conn_init_values(connp, ifp, connfp, mp, src, dst, ports);
		nca_tcp_init_values(connp);
		connp->inq = inq;

		connp->tcp_ipha.ipha_ttl = ipha->ipha_ttl;

		TDELTATS(*TSP(mp), ip_input[2]);

	}
	/*
	 * Put a REFHOLD on the conn_t for the remainder of tcp_input()
	 * processing, all returns from this point on must goto done.
	 */
	CONN_REFHOLD(connp);
	mutex_exit(&connfp->lock);

#ifdef	BEANS
	tsp = TSP(mp);
#endif

	ASSERT(connp->inq != NULL);

	TDELTATS(*tsp, ip_input[4]);

	if (!OK_32PTR(rptr)) {
		seg_seq = BE32_TO_U32(tcph->th_seq);
		seg_ack = BE32_TO_U32(tcph->th_ack);
	} else {
		seg_seq = ABE32_TO_U32(tcph->th_seq);
		seg_ack = ABE32_TO_U32(tcph->th_ack);
	}
	seg_len += TCP_HDR_LENGTH(tcph);
	seg_len = -seg_len;
	ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
	seg_len += (int)(mp->b_wptr - rptr);
	if ((mp1 = mp->b_cont) != NULL && mp1->b_datap->db_type == M_DATA) {
		do {
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			seg_len += (int)(mp1->b_wptr - mp1->b_rptr);
		} while ((mp1 = mp1->b_cont) != NULL &&
		    mp1->b_datap->db_type == M_DATA);
	}

	/*
	 * For NCA, it is safe to just clear the urgent flag and
	 * continue processing.  Otherwise, a HTTP client which
	 * "accidentally" set the urgent flag will not be able to
	 * continue.
	 */
	if (flags & TH_URG) {
		flags &= ~TH_URG;
		nca_tcp_in_urg++;
	}

	/* Parse TCP options. */
	if (TCP_HDR_LENGTH(tcph) > TCP_MIN_HEADER_LENGTH)
		options = nca_tcp_parse_options(tcph, &tcpopt);

	switch (connp->tcp_state) {
	case TCPS_LISTEN:
		if ((flags & (TH_RST | TH_ACK | TH_SYN)) != TH_SYN) {
			if (flags & TH_RST) {
				freemsg(mp);
				connp->tcp_close = 1;
				goto done;
			}
			if (flags & TH_ACK) {
				nca_tcp_xmit_early_reset("TCPS_LISTEN-TH_ACK",
				    connp->ifp, mp, seg_ack, 0, TH_RST);
				connp->tcp_close = 1;
				goto done;
			}
			if (!(flags & TH_SYN)) {
				freemsg(mp);
				connp->tcp_close = 1;
				goto done;
			}
		}
reclaim:
		connp->tcp_irs = seg_seq;
		connp->tcp_rack = seg_seq;
		connp->tcp_rnxt = seg_seq + 1;
		U32_TO_ABE32(connp->tcp_rnxt, connp->tcp_tcph->th_ack);
		goto syn_rcvd;
	case TCPS_SYN_SENT:
		if (flags & TH_ACK) {
			if (SEQ_LEQ(seg_ack, connp->tcp_iss) ||
			    SEQ_GT(seg_ack, connp->tcp_snxt)) {
				if (flags & TH_RST) {
					freemsg(mp);
					connp->tcp_close = 1;
					goto done;
				}
				nca_tcp_xmit_ctl("TCPS_SYN_SENT-Bad_seq",
				    connp, mp, seg_ack, 0, TH_RST);
				goto done;
			}
			if (SEQ_LEQ(connp->tcp_suna, seg_ack))
				flags |= TH_ACK_ACCEPTABLE;
		}
		if (flags & TH_RST) {
			freemsg(mp);
			if (flags & TH_ACK_ACCEPTABLE) {
				(void) nca_tcp_clean_death(connp, ECONNREFUSED);
				goto done;
			} else {
				connp->tcp_close = 1;
			}
			goto done;
		}
		if (!(flags & TH_SYN)) {
			freemsg(mp);
			goto done;
		}

		connp->tcp_irs = seg_seq;
		connp->tcp_rack = seg_seq;
		connp->tcp_rnxt = seg_seq + 1;
		U32_TO_ABE32(connp->tcp_rnxt, connp->tcp_tcph->th_ack);
		if (flags & TH_ACK_ACCEPTABLE) {
			/* One for the SYN */
			connp->tcp_suna = connp->tcp_iss + 1;
			connp->tcp_valid_bits &= ~TCP_ISS_VALID;
			connp->tcp_state = TCPS_ESTABLISHED;
			if (options & TCP_OPT_MSS_PRESENT) {
				connp->tcp_mss = MIN(tcpopt.tcp_opt_mss,
				    connp->tcp_mss);
			} else {
				connp->tcp_mss = tcp_mss_def;
			}
			/*
			 * If SYN was retransmitted, need to reset all
			 * retransmission info.  This is because this
			 * segment will be treated as a dup ACK.
			 */
			if (connp->tcp_rexmit) {
				connp->tcp_rexmit = 0;
				connp->tcp_rexmit_nxt = connp->tcp_snxt;
				connp->tcp_rexmit_max = connp->tcp_snxt;
				connp->tcp_snd_burst = TCP_CWND_INFINITE;
				/*
				 * Set tcp_cwnd back to 1 MSS, per
				 * recommendation from
				 * draft-floyd-incr-init-win-01.txt,
				 * Increasing TCP's Initial Window.
				 */
				connp->tcp_cwnd = connp->tcp_mss;
			}

			connp->tcp_swl1 = seg_seq;
			connp->tcp_swl2 = seg_ack;

			new_swnd = BE16_TO_U16(tcph->th_win);
			connp->tcp_swnd = new_swnd;
			if (new_swnd > connp->tcp_max_swnd)
				connp->tcp_max_swnd = new_swnd;

			/*
			 * Always send the three-way handshake ack immediately
			 * in order to make the connection complete as soon as
			 * possible on the accepting host.
			 */
			flags |= TH_ACK_NEEDED;
			flags &= ~(TH_SYN | TH_ACK_ACCEPTABLE);
			seg_seq++;
			break;
		}
	syn_rcvd:
		/* Common code path for passive and simultaneous open. */
		if (options & TCP_OPT_MSS_PRESENT) {
			connp->tcp_mss = MIN(tcpopt.tcp_opt_mss,
			    connp->tcp_mss);
		} else {
			connp->tcp_mss = tcp_mss_def;
		}
		connp->tcp_state = TCPS_SYN_RCVD;
		mp1 = nca_tcp_xmit_mp(connp, connp->tcp_xmit_head,
		    connp->tcp_mss, NULL, NULL, connp->tcp_iss, 0);
		if (mp1) {
			nca_ip_output(connp, mp1);
			TCP_TIMER_RESTART(connp, connp->tcp_rto);
		}
		freemsg(mp);
		goto done;
	case TCPS_TIME_WAIT:
		if ((flags & TH_FIN) && (seg_seq == connp->tcp_rnxt - 1)) {
			/*
			 * When TCP receives a duplicate FIN in TIME_WAIT
			 * state, restart the 2 MSL timer.  See page 73
			 * in RFC 793.  Make sure this TCP is already on
			 * the TIME-WAIT list.  If not, just restart the
			 * timer.
			 */
#ifdef	NOTYET
			if (TCP_IS_DETACHED(tcp)) {
				tcp_time_wait_remove(tcp);
				tcp_time_wait_append(tcp);
			} else {
				TCP_TIMER_RESTART(tcp,
				    tcp_time_wait_interval);
			}
#endif
			flags |= TH_ACK_NEEDED;
			if (mp != NULL) {
				freemsg(mp);
				mp = NULL;
			}
			BUMP_MIB(tcp_mib.tcpInDataDupSegs);
			goto ack_check;
		}
		if (!(flags & TH_SYN))
			break;
		gap = seg_seq - connp->tcp_rnxt;
		rgap = connp->tcp_rwnd - (gap + seg_len);
		if (gap > 0 && rgap < 0) {
			/*
			 * Make sure that when we accept the connection pick
			 * a number greater then the rnxt for the old
			 * connection.
			 *
			 * First, calculate a minimal iss value.
			 *
			 */
			squeue_t *inq = connp->inq;
			time_t adj = (connp->tcp_rnxt + ISS_INCR);

			/* Subtract out next iss */
			adj -= ISS_INCR/2;
			adj -= (int32_t)hrestime.tv_sec * ISS_INCR +
			    nca_tcp_iss_incr_extra;

			if (adj > 0) {
				/*
				 * New iss not guaranteed to be ISS_INCR
				 * ahead of the current rnxt, so add the
				 * difference to incr_extra just in case.
				 */
				nca_tcp_iss_incr_extra += adj;
			}
			nca_conn_reinit_values(connp);
			nca_tcp_init_values(connp);
			connp->inq = inq;
			goto reclaim;
		}
		break;
	default:
#if 0
		if (flags & TH_SYN) {
			nca_tcp_xmit_ctl("Bad_syn",
			    connp, mp, seg_ack, 0, TH_RST);
			return (1);
		}
#endif
		break;
	}
	mp->b_rptr = (uchar_t *)tcph + TCP_HDR_LENGTH(tcph);
	urp = BE16_TO_U16(tcph->th_urp) - TCP_OLD_URP_INTERPRETATION;
	new_swnd = BE16_TO_U16(tcph->th_win);
	connp->tcp_last_recv_time = lbolt;

try_again:;
	gap = seg_seq - connp->tcp_rnxt;
	rgap = connp->tcp_rwnd - (gap + seg_len);
	/*
	 * gap is the amount of sequence space between what we expect to see
	 * and what we got for seg_seq.  A positive value for gap means
	 * something got lost.  A negative value means we got some old stuff.
	 */
	if (gap < 0)
		goto gaplt;
	/*
	 * rgap is the amount of stuff received out of window.  A negative
	 * value is the amount out of window.
	 */
do_rgap:;
	if (rgap < 0)
		goto rgaplt;

ok:;
	if (seg_seq != connp->tcp_rnxt || connp->tcp_reass_head) {
		/*
		 * Clear the FIN bit in case it was set since we can not
		 * handle out-of-order FIN yet. This will cause the remote
		 * to retransmit the FIN.
		 * TODO: record the out-of-order FIN in the reassembly
		 * queue to avoid the remote having to retransmit.
		 */
		flags &= ~TH_FIN;
		if (seg_len > 0) {
			uint32_t old_flags = flags;

			/*
			 * Attempt reassembly and see if we have something
			 * ready to go.
			 */
			mp = nca_tcp_reass(connp, mp, seg_seq, &old_flags);
			flags = old_flags;

			/* Always ack out of order packets */
			flags |= TH_ACK_NEEDED;

			/* The packet fills a hole. */
			if (mp) {
				ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
				    (uintptr_t)INT_MAX);
				seg_len = mp->b_cont ? msgdsize(mp) :
					(int)(mp->b_wptr - mp->b_rptr);
				seg_seq = connp->tcp_rnxt;
			} else {
				/*
				 * Keep going even with NULL mp.
				 * There may be a useful ACK or something else
				 * we don't want to miss.
				 *
				 * But TCP should not perform fast retransmit
				 * because of the ack number.  TCP uses
				 * seg_len == 0 to determine if it is a pure
				 * ACK.  And this is not a pure ACK.
				 */
				seg_len = 0;
			}
		}
		if (nca_debug)
			printf("nca_tcpinput(): seg_seq (%u) != rnxt (%u)"
			    " || reass_head(%p)\n", seg_seq, connp->tcp_rnxt,
			    (void *)connp->tcp_reass_head);
	} else if (seg_len > 0) {
		BUMP_MIB(tcp_mib.tcpInDataInorderSegs);
		UPDATE_MIB(tcp_mib.tcpInDataInorderBytes, seg_len);
	}
	if ((flags & (TH_RST | TH_SYN | TH_URG | TH_ACK)) != TH_ACK) {
		if (flags & TH_RST)
			goto notackrst;
		if (flags & TH_SYN) {
			if (seg_seq != connp->tcp_irs)
				goto notacksyn;
			flags &= ~TH_SYN;
			seg_seq++;
		}
		if (flags & TH_URG && urp >= 0)
			goto notackurg;
process_ack:
		if (!(flags & TH_ACK)) {
			if (mp) {
				freemsg(mp);
				mp = NULL;
			}
			goto xmit_check;
		}
	}
	bytes_acked = (int)(seg_ack - connp->tcp_suna);
	if (connp->tcp_state == TCPS_SYN_RCVD) {
		/*
		 * NOTE: RFC 793 pg. 72 says this should be 'bytes_acked < 0'
		 * but that would mean we have an ack that ignored our SYN.
		 */
		if (bytes_acked < 1 || SEQ_GT(seg_ack, connp->tcp_snxt)) {
			freemsg(mp);
			nca_tcp_xmit_ctl("TCPS_SYN_RCVD-bad_ack", connp, NULL,
			    seg_ack, 0, TH_RST);
			/* CAS - q is undefined */
			/*
			 * TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
			 *		"tcp_rput end:  q %p", q);
			 */
			goto done;
		}
		/* 3-way handshake complete */
		connp->tcp_suna = connp->tcp_iss + 1;	/* One for the SYN */
		bytes_acked--;

		/*
		 * If SYN was retransmitted, need to reset all
		 * retransmission info as this segment will be
		 * treated as a dup ACK.
		 */
		if (connp->tcp_rexmit) {
			connp->tcp_rexmit = 0;
			connp->tcp_rexmit_nxt = connp->tcp_snxt;
			connp->tcp_rexmit_max = connp->tcp_snxt;
			connp->tcp_snd_burst = TCP_CWND_INFINITE;
			connp->tcp_cwnd = connp->tcp_mss;
		}

		connp->tcp_swl1 = seg_seq;
		connp->tcp_swl2 = seg_ack;
		connp->tcp_state = TCPS_ESTABLISHED;
		connp->tcp_valid_bits &= ~TCP_ISS_VALID;
	}
	/* This code follows 4.4BSD-Lite2 mostly. */
	if (bytes_acked < 0) {
		goto est;
	}
	mp1 = connp->tcp_xmit_head;
	if (bytes_acked == 0) {
		if (seg_len == 0 && new_swnd == connp->tcp_swnd) {
			BUMP_MIB(tcp_mib.tcpInDupAck);
			/*
			 * Fast retransmit.  When we have seen exactly three
			 * identical ACKs while we have unacked data
			 * outstanding we take it as a hint that our peer
			 * dropped something.
			 *
			 * If TCP is retransmitting, don't do fast retransmit.
			 */
			if (mp1 && connp->tcp_suna != connp->tcp_snxt &&
			    connp->tcp_rexmit == 0) {
				goto fastretransmit;
			}
		} else if (connp->tcp_zero_win_probe) {
			/*
			 * If the window has opened, need to arrange
			 * to send additional data.
			 */
			if (new_swnd != 0) {
				/* tcp_suna != tcp_snxt */
				/* Packet contains a window update */
				BUMP_MIB(tcp_mib.tcpInWinUpdate);
				connp->tcp_zero_win_probe = 0;
				connp->tcp_timer_backoff = 0;
				connp->tcp_ms_we_have_waited = 0;

				/*
				 * Transmit starting with tcp_suna since
				 * the one byte probe is not ack'ed.
				 * If TCP has sent more than one identical
				 * probe, tcp_rexmit will be set.  That means
				 * tcp_ss_rexmit() will send out the one
				 * byte along with new data.  Otherwise,
				 * fake the retransmission.
				 */
				flags |= TH_XMIT_NEEDED;
				if (! connp->tcp_rexmit) {
					connp->tcp_rexmit = 1;
					connp->tcp_dupack_cnt = 0;
					connp->tcp_rexmit_nxt = connp->tcp_suna;
					connp->tcp_rexmit_max = connp->tcp_suna
					    + tcp_zero_win_probesize;
				}
			}
		}
		goto swnd_update;
	}

	/*
	 * Check for "acceptability" of ACK value per RFC 793, pages 72 - 73.
	 * If the ACK value acks something that we have not yet sent, it might
	 * be an old duplicate segment.  Send an ACK to re-synchronize the
	 * other side.
	 * Note: reset in response to unacceptable ACK in SYN_RECEIVE
	 * state is handled above, so we can always just drop the segment and
	 * send an ACK here.
	 *
	 * Should we send ACKs in response to ACK only segments?
	 */
	if (SEQ_GT(seg_ack, connp->tcp_snxt)) {
		/* drop the received segment */
		if (mp)
			freemsg(mp);
#if 0
		if (connp->tcp_ackonly == seg_ack) {
			/* same ACK only as last time ? */
			nca_tcp_xmit_ctl("bad_ack", connp, NULL,
			    seg_ack, 0, TH_RST);
			goto done;
		}
		connp->tcp_ackonly = seg_ack;
#endif
		/* send an ACK */
		mp = nca_tcp_ack_mp(connp);
		if (mp) {
			nca_ip_output(connp, mp);
			BUMP_LOCAL(connp->tcp_obsegs);
			BUMP_MIB(tcp_mib.tcpOutAck);
		}
		goto done;
	}

	/*
	 * If we got an ACK after fast retransmit, check to see
	 * if it is a partial ACK.  If it is not and the congestion
	 * window was inflated to account for the other side's
	 * cached packets, retract it.  If it is, do Hoe's algorithm.
	 */
	if (connp->tcp_dupack_cnt >= tcp_dupack_fast_retransmit) {
		if (SEQ_GEQ(seg_ack, connp->tcp_rexmit_max)) {
			connp->tcp_dupack_cnt = 0;
			/*
			 * Restore the orig tcp_cwnd_ssthresh after
			 * fast retransmit phase.
			 */
			if (connp->tcp_cwnd > connp->tcp_cwnd_ssthresh) {
				connp->tcp_cwnd = connp->tcp_cwnd_ssthresh;
			}
			connp->tcp_rexmit_max = seg_ack;
			connp->tcp_cwnd_cnt = 0;
			connp->tcp_snd_burst = TCP_CWND_INFINITE;
		} else {
			/*
			 * Hoe's algorithm:
			 *
			 * Retransmit the unack'ed segment and
			 * restart fast recovery.  Note that we
			 * need to scale back tcp_cwnd to the
			 * original value when we started fast
			 * recovery.  This is to prevent overly
			 * aggressive behaviour in sending new
			 * segments.
			 */
			connp->tcp_cwnd = connp->tcp_cwnd_ssthresh +
			    tcp_dupack_fast_retransmit * connp->tcp_mss;
			connp->tcp_cwnd_cnt = connp->tcp_cwnd;
			BUMP_MIB(tcp_mib.tcpOutFastRetrans);
			flags |= TH_REXMIT_NEEDED;
		}
	} else {
		connp->tcp_dupack_cnt = 0;
		if (connp->tcp_rexmit) {
			/*
			 * TCP is retranmitting.  If the ACK ack's all
			 * outstanding data, update tcp_rexmit_max and
			 * tcp_rexmit_nxt.  Otherwise, update tcp_rexmit_nxt
			 * to the correct value.
			 *
			 * Note that SEQ_LEQ() is used.  This is to avoid
			 * unnecessary fast retransmit caused by dup ACKs
			 * received when TCP does slow start retransmission
			 * after a time out.  During this phase, TCP may
			 * send out segments which are already received.
			 * This causes dup ACKs to be sent back.
			 */
			if (SEQ_LEQ(seg_ack, connp->tcp_rexmit_max)) {
				if (SEQ_GT(seg_ack, connp->tcp_rexmit_nxt)) {
					connp->tcp_rexmit_nxt = seg_ack;
				}
				if (seg_ack != connp->tcp_rexmit_max) {
					flags |= TH_XMIT_NEEDED;
				}
			} else {
				connp->tcp_rexmit = 0;
				connp->tcp_rexmit_nxt = connp->tcp_snxt;
				connp->tcp_snd_burst = TCP_CWND_INFINITE;
			}
			connp->tcp_ms_we_have_waited = 0;
		}
	}

	BUMP_MIB(tcp_mib.tcpInAckSegs);
	UPDATE_MIB(tcp_mib.tcpInAckBytes, bytes_acked);
	connp->tcp_suna = seg_ack;
	if (connp->tcp_zero_win_probe != 0) {
		connp->tcp_zero_win_probe = 0;
		connp->tcp_timer_backoff = 0;
	}
	if (!mp1) {
		/*
		 * Something was acked, but we had no transmitted data
		 * outstanding.  Either the FIN or the SYN must have been
		 * acked.  If it was the FIN, we want to note that.
		 * The TCP_FSS_VALID bit will be on in this case.  Otherwise
		 * it must have been the SYN acked, and we have nothing
		 * special to do here.
		 */
		if (connp->tcp_fin_sent)
			connp->tcp_fin_acked = true;
		else if (!(connp->tcp_valid_bits & TCP_ISS_VALID))
			BUMP_MIB(tcp_mib.tcpInAckUnsent);
		goto swnd_update;
	}

	/* Update the congestion window */
	{
	uint32_t cwnd = connp->tcp_cwnd;
	uint32_t add = connp->tcp_mss;

	if (cwnd >= connp->tcp_cwnd_ssthresh) {
		/*
		 * This is to prevent an increase of less than 1 MSS of
		 * tcp_cwnd.  With partial increase, tcp_wput_slow() may
		 * send out tinygrams in order to preserve mblk boundaries.
		 * By initializing tcp_cwnd_cnt to new tcp_cwnd and
		 * decrementing it by 1 MSS for every ACKs, tcp_cwnd is
		 * increased by 1 MSS for every RTTs.
		 */
		if (connp->tcp_cwnd_cnt <= 0) {
			connp->tcp_cwnd_cnt = cwnd + add;
		} else {
			connp->tcp_cwnd_cnt -= add;
			add = 0;
		}
	}

	connp->tcp_cwnd = MIN(cwnd + add, connp->tcp_cwnd_max);
	}

	/* See if the latest urgent data has been acknowledged */
	if ((connp->tcp_valid_bits & TCP_URG_VALID) &&
	    SEQ_GT(seg_ack, connp->tcp_urg))
		connp->tcp_valid_bits &= ~TCP_URG_VALID;

	/* Can we update the RTT estimates? */
	if (SEQ_GT(seg_ack, connp->tcp_csuna)) {
		/*
		 * An ACK sequence we haven't seen before, so get the RTT
		 * and update the RTO.
		 */
		tcp_set_rto(connp, (int32_t)lbolt - (int32_t)mp1->b_prev);

		/* Remeber the last sequence to be ACKed */
		connp->tcp_csuna = seg_ack;
		if (connp->tcp_set_timer == 1) {
			TCP_TIMER_RESTART(connp, connp->tcp_rto);
			connp->tcp_set_timer = 0;
		}
	} else {
		BUMP_MIB(tcp_mib.tcpRttNoUpdate);
	}

	/* Eat acknowledged bytes off the xmit queue. */
	for (;;) {
		uchar_t	*wptr;

		wptr = mp1->b_wptr;
		ASSERT((uintptr_t)(wptr - mp1->b_rptr) <= (uintptr_t)INT_MAX);
		bytes_acked -= (int)(wptr - mp1->b_rptr);
		if (bytes_acked < 0) {
			mp1->b_rptr = wptr + bytes_acked;
			break;
		}
		mp1->b_prev = nilp(mblk_t);
		mp2 = mp1;
		mp1 = mp1->b_cont;
		freeb(mp2);
		if (bytes_acked == 0) {
			if (!mp1) {
				if (connp->xmit.np != NULL &&
				    connp->req_mp == NULL)
					flags |= TH_XMIT_NEEDED;
				goto pre_swnd_update;
			}
			if (mp2 != connp->tcp_xmit_tail)
				break;
			connp->tcp_xmit_tail = mp1;
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			connp->tcp_xmit_tail_unsent = (int)(mp1->b_wptr -
			    mp1->b_rptr);
			break;
		}
		if (!mp1) {
			/* TODO check that tcp_fin_sent has been set */
			/*
			 * More was acked but there is nothing more
			 * outstanding.  This means that the FIN was
			 * just acked or that we're talking to a clown.
			 */
			if (connp->tcp_fin_sent)
				connp->tcp_fin_acked = true;
			else
				BUMP_MIB(tcp_mib.tcpInAckUnsent);
			goto pre_swnd_update;
		}
		ASSERT(mp2 != connp->tcp_xmit_tail);
	}
	if (connp->tcp_unsent || connp->xmit.np != NULL) {
		flags |= TH_XMIT_NEEDED;
	}

pre_swnd_update:
	connp->tcp_xmit_head = mp1;
swnd_update:
	if (SEQ_LT(connp->tcp_swl1, seg_seq) || connp->tcp_swl1 == seg_seq &&
	    (SEQ_LT(connp->tcp_swl2, seg_ack) ||
	    connp->tcp_swl2 == seg_ack && new_swnd > connp->tcp_swnd)) {
		/*
		 * A segment in, or immediately to the right of, the window
		 * with a seq > then the last window update seg or a dup
		 * seq and either a ack > then the last window update seg
		 * or a dup seq with a larger window.
		 */
		if ((connp->tcp_unsent || connp->xmit.np != NULL) &&
		    new_swnd > connp->tcp_swnd) {
			flags |= TH_XMIT_NEEDED;
		}
		connp->tcp_swnd = new_swnd;
		connp->tcp_swl1 = seg_seq;
		connp->tcp_swl2 = seg_ack;
		if (new_swnd > connp->tcp_max_swnd)
			connp->tcp_max_swnd = new_swnd;
	}
est:

	TDELTATS(*tsp, tcp_input[0]);

	if (connp->tcp_state > TCPS_ESTABLISHED) {
		switch (connp->tcp_state) {
		case TCPS_FIN_WAIT_1:
			if (connp->tcp_fin_acked) {
				connp->tcp_state = TCPS_FIN_WAIT_2;
				/*
				 * We implement the non-standard BSD/SunOS
				 * FIN_WAIT_2 flushing algorithm.
				 * If there is no user attached to this
				 * TCP endpoint, then this TCP struct
				 * could hang around forever in FIN_WAIT_2
				 * state if the peer forgets to send us
				 * a FIN.  To prevent this, we wait only
				 * 2*MSL (a convenient time value) for
				 * the FIN to arrive.  If it doesn't show up,
				 * we flush the TCP endpoint.  This algorithm,
				 * though a violation of RFC-793, has worked
				 * for over 10 years in BSD systems.
				 * Note: SunOS 4.x waits 675 seconds before
				 * flushing the FIN_WAIT_2 connection.
				 */
				TCP_TIMER_RESTART(connp,
				    tcp_fin_wait_2_flush_interval);
			}
			break;
		case TCPS_FIN_WAIT_2:
			break;
		case TCPS_LAST_ACK:
			if (mp) {
				freemsg(mp);
				mp = NULL;
			}
			if (connp->tcp_fin_acked) {
				(void) nca_tcp_clean_death(connp, 0);
				goto done;
			}
			goto xmit_check;
		case TCPS_CLOSING:
			connp->tcp_close = 1;
			if (connp->tcp_fin_acked) {
				if (connp->tcp_ti.ep != NULL) {
					nca_ti_delete(&connp->tcp_ti);
				}
				connp->tcp_state = TCPS_TIME_WAIT;
				if (connp->twlbolt == NCA_TW_NONE) {
					nca_tw_add(connp);
				}
			}
			/*FALLTHRU*/
		case TCPS_TIME_WAIT:
		case TCPS_CLOSE_WAIT:
			if (mp) {
				freemsg(mp);
				mp = NULL;
			}
			goto xmit_check;
		default:
			break;
		}
	}
	if (flags & TH_FIN) {
		/* Make sure we ack the fin */
		flags |= TH_ACK_NEEDED;
		if (!connp->tcp_fin_rcvd) {
			connp->tcp_fin_rcvd = true;
			connp->tcp_rnxt++;
			tcph = connp->tcp_tcph;
			U32_TO_ABE32(connp->tcp_rnxt, tcph->th_ack);

			/*
			 * End of connection, anything to do ???
			 */
			flags |= TH_ORDREL_NEEDED;
			switch (connp->tcp_state) {
			case TCPS_SYN_RCVD:
			case TCPS_ESTABLISHED:
				connp->tcp_state = TCPS_CLOSE_WAIT;
				/* Keepalive? */
				break;
			case TCPS_FIN_WAIT_1:
				if (!connp->tcp_fin_acked) {
					connp->tcp_state = TCPS_CLOSING;
					break;
				}
				/* FALLTHRU */
			case TCPS_FIN_WAIT_2:
				connp->tcp_close = 1;
				if (connp->tcp_ti.ep != NULL) {
					nca_ti_delete(&connp->tcp_ti);
				}
				connp->tcp_state = TCPS_TIME_WAIT;
				if (connp->twlbolt == NCA_TW_NONE) {
					nca_tw_add(connp);
				}

				if (seg_len) {
					/*
					 * implies data piggybacked on FIN.
					 * break to handle data.
					 */
					break;
				}
				if (mp) {
					freemsg(mp);
					mp = NULL;
				}
				goto ack_check;
			}
		}
	}
	if (mp == NULL)
		goto xmit_check;
	if (seg_len == 0) {
		freemsg(mp);
		mp = NULL;
		goto xmit_check;
	}
	if (mp->b_rptr == mp->b_wptr) {
		/*
		 * The header has been consumed, so we remove the
		 * zero-length mblk here.
		 */
		mp1 = mp;
		mp = mp->b_cont;
		freeb(mp1);
	}
	tcph = connp->tcp_tcph;
	connp->tcp_rack_cnt += seg_len;
	/* NCA does not do delay ACK at this moment.... */
#if 0
	{
		uint32_t cur_max;
		cur_max = connp->tcp_rack_cur_max;
		if (connp->tcp_rack_cnt >= cur_max) {
			/*
			 * We have more unacked data than we should - send
			 * an ACK now.
			 */
			flags |= TH_ACK_NEEDED;
			cur_max += connp->tcp_mss;
			if (cur_max > connp->tcp_rack_abs_max)
				cur_max = connp->tcp_rack_abs_max;
			connp->tcp_rack_cur_max = cur_max;
		} else if (seg_len < connp->tcp_mss) {
			/*
			 * If we get a segment that is less than an mss, and we
			 * already have unacknowledged data, and the amount
			 * unacknowledged is not a multiple of mss, then we
			 * better generate an ACK now.  Otherwise, this may be
			 * the tail piece of a transaction, and we would rather
			 * wait for the response.
			 */
			uint32_t udif;
			ASSERT((uintptr_t)(connp->tcp_rnxt - connp->tcp_rack) <=
			    (uintptr_t)INT_MAX);
			udif = (int)(connp->tcp_rnxt - connp->tcp_rack);
			if (udif && (udif % connp->tcp_mss))
				flags |= TH_ACK_NEEDED;
			else
				flags |= TH_TIMER_NEEDED;
		} else {
			/* Start delayed ack timer */
			flags |= TH_TIMER_NEEDED;
		}
	}
#endif
	connp->tcp_rnxt += seg_len;
	U32_TO_ABE32(connp->tcp_rnxt, tcph->th_ack);

	/* Received data ? */
	if (seg_len) {
		/* Add to (or init) receive list */
		if (connp->tcp_rcv_cnt != 0) {
			connp->tcp_rcv_tail->b_cont = mp;
		} else {
			connp->tcp_rcv_head = mp;
		}
		while (mp->b_cont)
			mp = mp->b_cont;
		connp->tcp_rcv_tail = mp;
		connp->tcp_rcv_cnt += seg_len;
		connp->tcp_rwnd -= seg_len;
		U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);

		/* NCA does not do delay ACK. */
		flags |= TH_ACK_NEEDED;
		mp = NULL;
		/*
		 * Send the received data up only when we might have
		 * something of interest (i.e. a complete record):
		 *
		 * 1) remote TCP indication (PSH | FIN)
		 *
		 * 2) current segment len < a mss
		 *
		 * 3) XXX pending data count > then some push count?
		 *
		 * 4) XXX push timer expired.
		 *
		 */
		if (((flags & (TH_PSH|TH_FIN)) || seg_len < connp->tcp_mss)) {
http_check:;

			TDELTATS(*tsp, tcp_input[1]);

			if (nca_http(connp)) {
				/*
				 * nca_http() has indicated a successful
				 * parse, so if there's data to xmit, do it.
				 */
				mp = connp->req_mp;
				connp->req_mp = NULL;
				freemsg(mp);
				if (connp->fill_mp != NULL) {
					flags |= TH_XMIT_NEEDED;
					goto xmit_check;
				}
			}
		}
		goto ack_check;
	}
	if (mp) {
		freemsg(mp);
		mp = NULL;
	}

xmit_check:

	TDELTATS(*tsp, tcp_input[2]);

	/* Is there anything left to do? */
	ASSERT(!(flags & TH_MARKNEXT_NEEDED));
	if ((flags & (TH_REXMIT_NEEDED|TH_XMIT_NEEDED|TH_ACK_NEEDED|
	    TH_NEED_SACK_REXMIT|TH_TIMER_NEEDED|TH_ORDREL_NEEDED|
	    TH_SEND_URP_MARK)) == 0)
		goto done;

	/* Any transmit work to do and a non-zero window? */
	if ((flags & (TH_REXMIT_NEEDED|TH_XMIT_NEEDED|TH_NEED_SACK_REXMIT)) &&
	    connp->tcp_swnd != 0) {
		boolean_t http_check_needed = false;

		if (flags & TH_REXMIT_NEEDED) {
			uint32_t mss = connp->tcp_snxt - connp->tcp_suna;
			if (mss > connp->tcp_mss)
				mss = connp->tcp_mss;
			if (mss > connp->tcp_swnd)
				mss = connp->tcp_swnd;
			mp1 = nca_tcp_xmit_mp(connp, connp->tcp_xmit_head, mss,
			    NULL, NULL, connp->tcp_suna, 1);
			if (mp1) {
				connp->tcp_xmit_head->b_prev = (mblk_t *)lbolt;
				connp->tcp_csuna = connp->tcp_snxt;
				BUMP_MIB(tcp_mib.tcpRetransSegs);
				UPDATE_MIB(tcp_mib.tcpRetransBytes,
				    msgdsize(mp1->b_cont));
				nca_ip_output(connp, mp1);
			}
		}
		if (flags & TH_XMIT_NEEDED) {
			if (connp->tcp_rexmit) {
				if (nca_tcp_ss_rexmit(connp,
				    &http_check_needed, &flags)) {
					goto done;
				}
			} else {
				if (nca_tcp_output(connp, &http_check_needed,
				    &flags)) {
					goto done;
				}
			}
		}

		TDELTATS(*tsp, tcp_input[3]);

		/* Need http check and have data? */
		if (http_check_needed && connp->tcp_rcv_cnt)
			goto http_check;

		/* Anything more to do? */
		if ((flags & (TH_ACK_NEEDED|TH_TIMER_NEEDED|
		    TH_ORDREL_NEEDED|TH_SEND_URP_MARK)) == 0)
			goto done;
	}
ack_check:

	TDELTATS(*tsp, tcp_input[4]);

	if (flags & TH_SEND_URP_MARK) {
		flags &= ~TH_SEND_URP_MARK;
		printf("nca_tcpinput(): TH_SEND_URP_MARK?\n");
	}
	if (flags & TH_ACK_NEEDED) {
		/*
		 * Time to send an ack for some reason.
		 */
		mp1 = nca_tcp_ack_mp(connp);

		if (mp1) {
			nca_ip_output(connp, mp1);
			BUMP_LOCAL(connp->tcp_obsegs);
			BUMP_MIB(tcp_mib.tcpOutAck);
		}
	}
#ifdef	NOTYET
	if (flags & TH_TIMER_NEEDED) {
		/*
		 * Arrange for deferred ACK or push wait timeout.
		 * Start timer if it is not already running.
		 */
		if (tcp->tcp_ack_timer_running == 0) {
			mi_timer(tcp->tcp_wq, tcp->tcp_ack_mp,
			    (clock_t)tcp_deferred_ack_interval);
			tcp->tcp_ack_timer_running = 1;
		}
		/* Record the time when the delayed timer is set. */
		if (tcp->tcp_rnxt == tcp->tcp_rack + seg_len) {
			tcp->tcp_dack_set_time = lbolt;
		}
	}
#endif
	if (flags & TH_ORDREL_NEEDED) {
		/* Connection is closing down, cleanup the xmit list */
		flags &= ~TH_ORDREL_NEEDED;
		mp = connp->tcp_xmit_head;
		connp->tcp_xmit_head = NULL;
		connp->tcp_xmit_tail = NULL;
		connp->tcp_xmit_tail_unsent = 0;
		connp->tcp_unsent = 0;
		while (mp) {
			mp->b_prev = NULL;
			mp1 = mp->b_cont;
			freeb(mp);
			mp = mp1;
		}
		if (!connp->tcp_fin_sent) {
			connp->tcp_valid_bits |= TCP_FSS_VALID;
			connp->tcp_fss = connp->tcp_snxt;
			mp1 = nca_tcp_xmit_mp(connp, nilp(mblk_t), 0,
			    NULL, NULL, connp->tcp_snxt, 0);
			if (mp1 != NULL) {
				nca_ip_output(connp, mp1);
				flags &= ~TH_ACK_NEEDED;
			}
		}
	}
done:

	TDELTATS(*tsp, tcp_input[5]);

	if (connp->tcp_close && connp->tcp_refed) {
		/* Do a REFRELE for the tcp_refed REFHOLD (i.e. init hold) */
		connp->tcp_refed = 0;
		CONN_REFRELE(connp);
	}
	/* Do a REFRELE for our REFHOLD */
	CONN_REFRELE(connp);

	/*
	 * Run any per CPU bound services here.
	 */
	{
		processorid_t	seqid = CPU->cpu_seqid;
		ti_t		*tip = nca_gv[seqid].tcp_ti;
		tw_t		*twp = nca_gv[seqid].tcp_tw;

		if (tip->exec > NCA_TI_NONE && lbolt >= tip->lbolt2) {
			/* One of more expired TCP_TIMER conn_t's pending */
			nca_ti_reap(tip);
		}

		TDELTATS(*tsp, tcp_input[6]);

		if (twp->lbolt1 != NCA_TW_NONE && lbolt >= twp->lbolt2) {
			/* One of more expired TIME_WAIT conn_t's expired */
			nca_tw_reap(twp);
		}

		TDELTATS(*tsp, tcpip_input[7]);
	}
	/*
	 * If the NCA logger needs help and we're not an interrupt thread
	 * then do an squeue_t proxy call to process any logbuf(s).
	 */
	if (nca_logger_help && ! servicing_interrupt()) {
		/* Logger needs some help */
		NCA_COUNTER(&nca_logger_help_given, 1);
		nca_logger_help = false;
		squeue_proxy(inq, &nca_log_squeue);

		TDELTATS(*tsp, ip_input[8]);
	}

	return;

fastretransmit:
	if (++connp->tcp_dupack_cnt == tcp_dupack_fast_retransmit) {
		int npkt;
		BUMP_MIB(tcp_mib.tcpOutFastRetrans);
		/*
		 * Adjust cwnd since the duplicate
		 * ack indicates that a packet was
		 * dropped (due to congestion.)
		 *
		 * Here we perform congestion avoidance,
		 * but NOT slow start. This is known
		 * as the Fast Recovery Algorithm.
		 */
		npkt = (MIN(connp->tcp_cwnd,
		    connp->tcp_swnd) >> 1) / connp->tcp_mss;
		if (npkt < 2)
			npkt = 2;
		connp->tcp_cwnd_ssthresh = npkt * connp->tcp_mss;
		connp->tcp_cwnd = (npkt + connp->tcp_dupack_cnt) *
			connp->tcp_mss;

		if (connp->tcp_cwnd > connp->tcp_cwnd_max)
			connp->tcp_cwnd = connp->tcp_cwnd_max;

		/*
		 * We do Hoe's algorithm.  Refer to her
		 * paper "Improving the Start-up Behavior
		 * of a Congestoin Control Scheme for TCP,"
		 * appeared in SINGCOMM'96.
		 *
		 * Save highest seq no we have sent so far.
		 * Be careful about the invisible FIN byte.
		 */
		if ((connp->tcp_valid_bits & TCP_FSS_VALID) &&
		    (connp->tcp_unsent == 0 && connp->xmit.np == NULL)) {
			connp->tcp_rexmit_max = connp->tcp_snxt - 1;
		} else {
			connp->tcp_rexmit_max = connp->tcp_snxt;
		}

		/*
		 * Do not allow bursty traffic during.
		 * fast recovery.  Refer to Fall and Floyd's
		 * paper "Simulation-based Comparisons of
		 * Tahoe, Reno and SACK TCP" (in CCR ??)
		 * This is a best current practise.
		 */
		connp->tcp_snd_burst = TCP_CWND_SS;

		flags |= TH_REXMIT_NEEDED;

	} else if (connp->tcp_dupack_cnt > tcp_dupack_fast_retransmit) {
		/*
		 * We know that one more packet has
		 * left the pipe thus we can update
		 * cwnd.
		 */
		uint32_t cwnd = connp->tcp_cwnd;
		cwnd += connp->tcp_mss;
		if (cwnd > connp->tcp_cwnd_max)
			cwnd = connp->tcp_cwnd_max;
		connp->tcp_cwnd = cwnd;
		flags |= TH_XMIT_NEEDED;
	}
	goto swnd_update;

gaplt:
	/* Old stuff present.  Is the SYN in there? */
	if (seg_seq == connp->tcp_irs && (flags & TH_SYN) && (seg_len != 0)) {
		flags &= ~TH_SYN;
		seg_seq++;
		urp--;
		/* Recompute the gaps after noting the SYN. */
		goto try_again;
	}
	BUMP_MIB(tcp_mib.tcpInDataDupSegs);
	UPDATE_MIB(tcp_mib.tcpInDataDupBytes,
	    (seg_len > -gap ? -gap : seg_len));
	/* Remove the old stuff from seg_len. */
	seg_len += gap;
	/*
	 * Anything left?
	 * Make sure to check for unack'd FIN when rest of data
	 * has been previously ack'd.
	 */
	if (seg_len < 0 || (seg_len == 0 && !(flags & TH_FIN))) {
		/*
		 * Resets are only valid if they lie within our offered
		 * window.  If the RST bit is set, we just ignore this
		 * segment.
		 */
		if (flags & TH_RST) {
			freemsg(mp);
			goto done;
		}

		/*
		 * The arriving of dup data packets indicate that we
		 * may have postponed an ack for too long, or the other
		 * side's RTT estimate is out of shape. Start acking
		 * more often.
		 */
		if (SEQ_GEQ(seg_seq + seg_len - gap, connp->tcp_rack) &&
		    connp->tcp_rack_cnt >= connp->tcp_mss &&
		    connp->tcp_rack_abs_max > (connp->tcp_mss << 1))
			connp->tcp_rack_abs_max -= connp->tcp_mss;
		/*
		 * This segment is "unacceptable".  None of its
		 * sequence space lies within our advertized window.
		 *
		 * Adjust seg_len to be the original value.
		 */
		if (gap < 0)
			seg_len -= gap;
		if ((flags & TH_SYN) && nca_debug) {
			(void) printf("nca_tcp_input(): SYN, irs %u\n",
			    connp->tcp_irs);
		}
		if (nca_debug)
			(void) printf(
			    "nca_tcp_input(): unacceptable, gap %d, rgap %d, "
			    "flags 0x%x, seg_seq %u, seg_ack %u, seg_len %d, "
			    "rnxt %u, snxt %u, %s\n",
			    gap, rgap, flags, seg_seq, seg_ack, seg_len,
			    connp->tcp_rnxt, connp->tcp_snxt,
			    nca_tcp_display(connp));
		if (seg_len < 72 && nca_debug) {
			*(mp->b_rptr + seg_len) = 0;
			printf("%s\n", mp->b_rptr);
		}

		connp->tcp_rack_cur_max = connp->tcp_mss;

		/*
		 * Arrange to send an ACK in response to the
		 * unacceptable segment per RFC 793 page 69. There
		 * is only one small difference between ours and the
		 * acceptability test in the RFC - we accept ACK-only
		 * packet with SEG.SEQ = RCV.NXT+RCV.WND and no ACK
		 * will be generated.
		 *
		 * Note that we have to ACK an ACK-only packet at least
		 * for stacks that send 0-length keep-alives with
		 * SEG.SEQ = SND.NXT-1 as recommended by RFC1122,
		 * section 4.2.3.6. As long as we don't ever generate
		 * an unacceptable packet in response to an incoming
		 * packet that is unacceptable, it should not cause
		 * "ACK wars".
		 */

		flags |=  TH_ACK_NEEDED;

		/*
		 * Continue processing this segment in order to use the
		 * ACK information it contains, but skip all other
		 * sequence-number processing.	Processing the ACK
		 * information is necessary is necessary in order to
		 * re-synchronize connections that may have lost
		 * synchronization.
		 * We clear seg_len and flag fields related to
		 * sequence number processing as they are not
		 * to be trusted for an unacceptable segment.
		 */
		seg_len = 0;
		flags &= ~(TH_SYN | TH_FIN | TH_URG);
		goto process_ack;
	}

	/* Fix seg_seq, and chew the gap off the front. */
	seg_seq = connp->tcp_rnxt;
	urp += gap;
	do {
		ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
		    (uintptr_t)UINT_MAX);
		gap += (uint_t)(mp->b_wptr - mp->b_rptr);
		if (gap > 0) {
			mp->b_rptr = mp->b_wptr - gap;
			break;
		}
		mp2 = mp;
		mp = mp->b_cont;
		freeb(mp2);
	} while (gap < 0);

rgaplt:
#if 0
	if (tcp->tcp_rwnd == 0)
		BUMP_MIB(tcp_mib.tcpInWinProbe);
	else {
		BUMP_MIB(tcp_mib.tcpInDataPastWinSegs);
		UPDATE_MIB(tcp_mib.tcpInDataPastWinBytes, -rgap);
	}
#endif
	/*
	 * seg_len does not include the FIN, so if more than
	 * just the FIN is out of window, we act like we don't
	 * see it.  (If just the FIN is out of window, rgap
	 * will be zero and we will go ahead and acknowledge
	 * the FIN.)
	 */
	flags &= ~(TH_FIN | TH_PUSH);

	/* Fix seg_len and make sure there is something left. */
	seg_len += rgap;
	if (seg_len <= 0) {
		/*
		 * Resets are only valid if they lie within our offered
		 * window.  If the RST bit is set, we just ignore this
		 * segment.
		 */
		if (flags & TH_RST) {
			freemsg(mp);
			goto done;
		}

		/*
		 * If this is a zero window probe, continue to
		 * process the ACK part.  Otherwise just drop the
		 * segment and send back an ACK.
		 */
		flags |= TH_ACK_NEEDED;
		if (connp->tcp_rwnd == 0 && seg_seq == connp->tcp_rnxt) {
			flags &= ~(TH_SYN | TH_URG);
			seg_len = 0;
			goto process_ack;
		} else {
			if (mp)
				freemsg(mp);
			goto ack_check;
		}
	}
	/* Pitch out of window stuff off the end. */
	rgap = seg_len;
	mp2 = mp;

	do {
		ASSERT((uintptr_t)(mp2->b_wptr - mp2->b_rptr) <=
		    (uintptr_t)INT_MAX);
		rgap -= (int)(mp2->b_wptr - mp2->b_rptr);
		if (rgap < 0) {
			mp2->b_wptr += rgap;
			if ((mp1 = mp2->b_cont) != NULL) {
				mp2->b_cont = nilp(mblk_t);
				freemsg(mp1);
			}
			break;
		}
	} while ((mp2 = mp2->b_cont) != NULL);
	goto ok;

notackrst:
	if (mp)
		freemsg(mp);
	switch (connp->tcp_state) {
	case TCPS_SYN_RCVD:
		(void) nca_tcp_clean_death(connp, ECONNREFUSED);
		break;
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
		(void) nca_tcp_clean_death(connp, ECONNRESET);
		break;
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		(void) nca_tcp_clean_death(connp, 0);
		break;
	default:
		(void) nca_tcp_clean_death(connp, ENXIO);
		break;
	}
	goto done;

notacksyn:
	if (mp != NULL)
		freemsg(mp);
	/* See RFC793, Page 71 */
	nca_tcp_xmit_ctl("TH_SYN", connp, NULL, seg_ack, seg_seq + 1,
	    TH_ACK | TH_RST);
	if (connp->tcp_state != TCPS_TIME_WAIT) {
		(void) nca_tcp_clean_death(connp, ECONNRESET);
	}
	goto done;

notackurg:
	printf("nca_tcp_input(): TCP urgent data1\n");
	if (mp)
		freemsg(mp);
	goto done;

urgent:
	printf("nca_tcp_input(): TCP urgent data2\n");
	if (mp)
		freemsg(mp);
	goto done;
}

static void
nca_tcp_init_values(conn_t *connp)
{
	tcph_t	*tcph;
	uint32_t u32;

	connp->tcp_state = TCPS_LISTEN;
	/*
	 * Initialize tcp_rtt_sa and tcp_rtt_sd so that the calculated RTO
	 * will be close to tcp_rexmit_interval_initial.  By doing this, we
	 * allow the algorithm to adjust slowly to large fluctuations of RTT
	 * during first few transmissions of a connection as seen in slow
	 * links.
	 */
	connp->tcp_rtt_sa = tcp_rexmit_interval_initial << 2;
	connp->tcp_rtt_sd = tcp_rexmit_interval_initial >> 1;
	connp->tcp_rto = (connp->tcp_rtt_sa >> 3) + connp->tcp_rtt_sd +
	    tcp_rexmit_interval_extra + (connp->tcp_rtt_sa >> 5) +
	    tcp_conn_grace_period;
	connp->tcp_timer_backoff = 0;
	connp->tcp_ms_we_have_waited = 0;
	connp->tcp_last_recv_time = lbolt;
	connp->tcp_cwnd_max = tcp_cwnd_max_;
	connp->tcp_snd_burst = TCP_CWND_INFINITE;

	connp->tcp_first_timer_threshold = tcp_ip_notify_interval;
	connp->tcp_first_ctimer_threshold = tcp_ip_notify_cinterval;
	connp->tcp_second_timer_threshold = tcp_ip_abort_interval;
	/*
	 * Fix it to tcp_ip_abort_linterval later if it turns out to be a
	 * passive open.
	 */
	connp->tcp_second_ctimer_threshold = tcp_ip_abort_cinterval;
	connp->tcp_keepalive_intrvl = tcp_keepalive_interval;

	connp->tcp_naglim = tcp_naglim_def;

	/* NOTE:  ISS is now set in tcp_adapt(). */

	connp->tcp_hdr_len = sizeof (ipha_t) + sizeof (tcph_t);
	connp->tcp_tcp_hdr_len = sizeof (tcph_t);
	connp->tcp_ip_hdr_len = sizeof (ipha_t);

	/* Initialize the header template */
	connp->tcp_ipha.ipha_length = htons(sizeof (ipha_t) + sizeof (tcph_t));
	connp->tcp_ipha.ipha_version_and_hdr_length
		= (IP_VERSION << 4) | IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	connp->tcp_ipha.ipha_ttl = tcp_ip_ttl;
	connp->tcp_ipha.ipha_protocol = IPPROTO_TCP;

	connp->tcp_ipha.ipha_dst = connp->faddr;
	connp->tcp_ipha.ipha_src = connp->laddr;

	tcph = (tcph_t *)&connp->tcp_iphc[sizeof (ipha_t)];
	connp->tcp_tcph = tcph;
	tcph->th_offset_and_rsrvd[0] = (5 << 4);
	bcopy(&connp->conn_fport, tcph->th_fport, 2);
	bcopy(&connp->conn_lport, tcph->th_lport, 2);

	/*
	 * IP wants our header length in the checksum field to
	 * allow it to perform a single pseudo-header+checksum
	 * calculation on behalf of TCP.
	 * Include the adjustment for a source route if any.
	 */
	connp->tcp_sum = 0;
	u32 = sizeof (tcph_t) + connp->tcp_sum;
	u32 = (u32 >> 16) + (u32 & 0xFFFF);
	U16_TO_ABE16(u32, tcph->th_sum);

	/*
	 * tcp_adapt()
	 */
	u32 = connp->ifp->mac_mtu;
	u32 -= IP_SIMPLE_HDR_LENGTH;
	u32 -= TCP_MIN_HEADER_LENGTH;
	connp->tcp_cwnd_ssthresh = TCP_MAXWIN;
	connp->tcp_localnet = 1;
	/*
	 * Initialize the ISS here now that we have the full connection ID.
	 * The RFC 1948 method of initial sequence number generation requires
	 * knowledge of the full connection ID before setting the ISS.
	 */
	if (tcp_strong_iss)
		nca_tcp_iss_init(connp);
	else {
		nca_tcp_iss_incr_extra += ISS_INCR/2;
		connp->tcp_iss = (uint_t)hrestime.tv_sec * ISS_INCR +
		    nca_tcp_iss_incr_extra;
		connp->tcp_valid_bits = TCP_ISS_VALID;
		connp->tcp_fss = connp->tcp_iss - 1;
		connp->tcp_suna = connp->tcp_iss;
		connp->tcp_snxt = connp->tcp_iss + 1;
		connp->tcp_rexmit_nxt = connp->tcp_snxt;
		connp->tcp_csuna = connp->tcp_snxt;
	}
	/*
	 * tcp_mss_set()
	 */
	if (u32 < tcp_mss_min)
		u32 = tcp_mss_min;
	if (u32 > tcp_mss_max)
		u32 = tcp_mss_max;
	/*
	 * Unless naglim has been set by our client to
	 * a non-mss value, force naglim to track mss.
	 * This can help to aggregate small writes.
	 */
	if (u32 < connp->tcp_naglim || connp->tcp_mss == connp->tcp_naglim)
		connp->tcp_naglim = u32;
	if (u32 > connp->tcp_xmit_hiwater)
		connp->tcp_xmit_hiwater = u32;
	connp->tcp_mss = u32;
	/*
	 * BSD derived TCP stacks have the interesting feature that if
	 * the connection is established thru passive open, the initial
	 * cwnd will be equal to 2 MSS.  We make this value controllable
	 * by a ndd param.
	 *
	 * Also refer to Sally Floyd's proposal of larger initial cwnd
	 * size.
	 */
	connp->tcp_cwnd = MIN(tcp_slow_start_initial*u32, connp->tcp_cwnd_max);
	connp->tcp_cwnd_cnt = 0;
	/*
	 *tcp_rwnd_set()
	 */
	connp->tcp_rwnd = MAX(u32 * tcp_recv_hiwat_minmss, tcp_recv_hiwat);
	/* Insist on a receive window that is a multiple of mss. */
	connp->tcp_rwnd = (((connp->tcp_rwnd - 1) / u32) + 1) * u32;
	connp->tcp_rwnd_max = connp->tcp_rwnd;

	if (connp->tcp_localnet) {
		connp->tcp_rack_abs_max =
		    MIN(tcp_deferred_acks_max, connp->tcp_rwnd / u32 / 2) * u32;
	} else {
		/*
		 * For a remote host on a different subnet (through a router),
		 * we ack every other packet to be conforming to RFC1122.
		 */
		connp->tcp_rack_abs_max = MIN(tcp_deferred_acks_max, 2) * u32;
	}
	if (connp->tcp_rack_cur_max > connp->tcp_rack_abs_max)
		connp->tcp_rack_cur_max = connp->tcp_rack_abs_max;
	else
		connp->tcp_rack_cur_max = 0;

	U32_TO_ABE16(connp->tcp_rwnd, connp->tcp_tcph->th_win);
}

/*
 * tcp_xmit_mp is called to return a pointer to an mblk chain complete with
 * ip and tcp header ready to pass down to IP.  If the mp passed in is
 * non-nil, then up to max_to_send bytes of data will be dup'ed off that
 * mblk. (If sendall is not set the dup'ing will stop at an mblk boundary
 * otherwise it will dup partial mblks.)
 * Otherwise, an appropriate ACK packet will be generated.  This
 * routine is not usually called to send new data for the first time.  It
 * is mostly called out of the timer for retransmits, and to generate ACKs.
 *
 * If offset is not NULL, the returned mblk chain's first mblk's b_rptr will
 * be adjusted by *offset.  And after dupb(), the offset and the ending mblk
 * of the original mblk chain will be returned in *offset and *end_mp.
 */
static mblk_t *
nca_tcp_xmit_mp(conn_t *connp, mblk_t *mp, int32_t max_to_send, int32_t *offset,
    mblk_t **end_mp, uint32_t seq, int32_t sendall)
{
	int32_t	off = 0;
	uint_t	flags;
	mblk_t	*mp1;
	mblk_t	*mp2;
	uchar_t	*rptr;
	tcph_t	*tcph;
	int	data_length = TCP_MAX_COMBINED_HEADER_LENGTH + tcp_wroff_xtra;
	int32_t	sack_opt_len = 0;
	int	kmflag = connp->inq->sq_isintr ? KM_NOSLEEP : KM_SLEEP;

	/* Allocate for our maximum TCP header + link-level */
	if (kmflag == KM_SLEEP) {
		int	error = 0;

		mp1 = allocb_wait(data_length, BPRI_MED, STR_NOSIG, &error);
		if (mp1 == NULL) {
			return (NULL);
		}
	} else {
		mp1 = allocb(data_length, BPRI_MED);
		if (mp1 == NULL) {
			return (NULL);
		}
	}
	data_length = 0;

	if (offset != NULL) {
		off = *offset;
		/* We use offset as an indicator that end_mp is not NULL. */
		*end_mp = NULL;
	}
	for (mp2 = mp1; mp && data_length != max_to_send; mp = mp->b_cont) {
		/* This could be faster with cooperation from downstream */
		if (mp2 != mp1 && sendall == 0 &&
		    data_length + (int)(mp->b_wptr - mp->b_rptr) >
		    max_to_send)
			/*
			 * Don't send the next mblk since the whole mblk
			 * does not fit.
			 */
			break;
		mp2->b_cont = dupb(mp);
		mp2 = mp2->b_cont;
		if (!mp2) {
			freemsg(mp1);
			return (nilp(mblk_t));
		}
		mp2->b_rptr += off;
		ASSERT((uintptr_t)(mp2->b_wptr - mp2->b_rptr) <=
		    (uintptr_t)INT_MAX);
		data_length += (int)(mp2->b_wptr - mp2->b_rptr);
		if (data_length > max_to_send) {
			mp2->b_wptr -= data_length - max_to_send;
			data_length = max_to_send;
			off = mp2->b_wptr - mp->b_rptr;
			break;
		} else {
			off = 0;
		}
	}
	if (offset != NULL) {
		*offset = off;
		*end_mp = mp;
	}

	rptr = mp1->b_rptr + tcp_wroff_xtra;
	mp1->b_rptr = rptr;
	mp1->b_wptr = rptr + connp->tcp_hdr_len + sack_opt_len;
	bcopy(connp->tcp_iphc, rptr, connp->tcp_hdr_len);
	tcph = (tcph_t *)&rptr[connp->tcp_ip_hdr_len];
	U32_TO_ABE32(seq, tcph->th_seq);
	/*
	 * Use tcp_unsent to determine if the PSH bit should be used assumes
	 * that this function was called from tcp_wput_slow. Thus, when called
	 * to retransmit data the setting of the PSH bit may appear some
	 * what random in that it might get set when it should not. This
	 * should not pose any performance issues.
	 */
	if (data_length != 0 &&
	    ((connp->tcp_unsent == 0 && connp->xmit.np == NULL) ||
	    connp->tcp_unsent == data_length))
		flags = TH_ACK | TH_PSH;
	else
		flags = TH_ACK;
	if (connp->tcp_valid_bits) {
		uint32_t u1;

		if ((connp->tcp_valid_bits & TCP_ISS_VALID) &&
		    seq == connp->tcp_iss) {
			uchar_t	*wptr;
			switch (connp->tcp_state) {
			case TCPS_SYN_SENT:
				flags = TH_SYN;
				break;
			case TCPS_SYN_RCVD:
				flags |= TH_SYN;
				break;
			default:
				break;
			}

			/* Tack on the mss option */
			wptr = mp1->b_wptr;
			wptr[0] = TCPOPT_MAXSEG;
			wptr[1] = TCPOPT_MAXSEG_LEN;
			wptr += 2;
			/* Need to adjust tcp_mss to the max interface mss. */
			u1 = connp->tcp_mss;
			U16_TO_BE16(u1, wptr);
			mp1->b_wptr = wptr + 2;

			/* Update the offset to cover the additional word */
			tcph->th_offset_and_rsrvd[0] += (1 << 4);

			/* allocb() of adequate mblk assures space */
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			u1 = (int)(mp1->b_wptr - mp1->b_rptr);
			/*
			 * Get IP set to checksum on our behalf
			 * Include the adjustment for a source route if any.
			 */
			u1 += connp->tcp_sum;
			u1 = (u1 >> 16) + (u1 & 0xFFFF);
			U16_TO_BE16(u1, tcph->th_sum);
			if (connp->tcp_state < TCPS_ESTABLISHED)
				flags |= TH_SYN;
			if (flags & TH_SYN)
				BUMP_MIB(tcp_mib.tcpOutControl);
		}
		if ((connp->tcp_valid_bits & TCP_FSS_VALID) &&
		    (seq + data_length) == connp->tcp_fss) {
			if (!connp->tcp_fin_acked) {
				flags |= TH_FIN;
				BUMP_MIB(tcp_mib.tcpOutControl);
			}
			if (!connp->tcp_fin_sent) {
				connp->tcp_fin_sent = true;
				switch (connp->tcp_state) {
				case TCPS_SYN_RCVD:
				case TCPS_ESTABLISHED:
					connp->tcp_state = TCPS_FIN_WAIT_1;
					break;
				case TCPS_CLOSE_WAIT:
					connp->tcp_state = TCPS_LAST_ACK;
					break;
				}
				if (connp->tcp_suna == connp->tcp_snxt)
					TCP_TIMER_RESTART(connp,
								connp->tcp_rto);
				connp->tcp_snxt = connp->tcp_fss + 1;
			}
		}
		u1 = connp->tcp_urg - seq + TCP_OLD_URP_INTERPRETATION;
		if ((connp->tcp_valid_bits & TCP_URG_VALID) &&
		    u1 < (uint32_t)(64 * 1024)) {
			flags |= TH_URG;
			BUMP_MIB(tcp_mib.tcpOutUrg);
			U32_TO_ABE16(u1, tcph->th_urp);
		}
	}
	tcph->th_flags[0] = (uchar_t)flags;
	connp->tcp_rack = connp->tcp_rnxt;
	connp->tcp_rack_cnt = 0;

	ASSERT((uintptr_t)(mp1->b_wptr - rptr) <= (uintptr_t)INT_MAX);
	data_length += (int)(mp1->b_wptr - rptr);
	((ipha_t *)rptr)->ipha_length = htons(data_length);

	/*
	 * Prime pump for IP
	 * Include the adjustment for a source route if any.
	 */
	data_length -= connp->tcp_ip_hdr_len;
	data_length += connp->tcp_sum;
	data_length = (data_length >> 16) + (data_length & 0xFFFF);
	U16_TO_ABE16(data_length, tcph->th_sum);
	return (mp1);
}

/* Non overlapping byte exchanger */
static void
tcp_xchg(uchar_t *a, uchar_t *b, int len)
{
	uchar_t	uch;

	while (len-- > 0) {
		uch = a[len];
		a[len] = b[len];
		b[len] = uch;
	}
}

/*
 * Send out a control packet on the tcp connection specified.  This routine
 * is typically called where we need a simple ACK or RST generated.
 *
 * Note: we free the msg pointed to by mp.
 */
static void
nca_tcp_xmit_ctl(char *str, conn_t *connp, mblk_t *mp, uint32_t seq,
    uint32_t ack, int ctl)
{
	uchar_t		*rptr;
	tcph_t		*tcph;
	ipha_t		*ipha;
	uint32_t	sum;

	/*
	 * Save sum for use in source route later.
	 */
	sum = connp->tcp_tcp_hdr_len + connp->tcp_sum;

	if (mp) {
		ipha = (ipha_t *)mp->b_rptr;
		ASSERT(((ipha->ipha_version_and_hdr_length) & 0xf0) == 0x40);
		tcph = (tcph_t *)(mp->b_rptr + IPH_HDR_LENGTH(ipha));
		if (tcph->th_flags[0] & TH_RST) {
			freemsg(mp);
			return;
		}
		freemsg(mp);
	}
	/* If a text string is passed in with the request, pass it to strlog. */
	if (str && nca_debug) {
		(void) printf(
		    "nca_tcp_xmit_ctl: '%s', seq 0x%x, ack 0x%x, ctl 0x%x",
		    str, seq, ack, ctl);
	}
	mp = allocb(TCP_MAX_COMBINED_HEADER_LENGTH + tcp_wroff_xtra, BPRI_MED);
	if (!mp)
		return;
	rptr = &mp->b_rptr[tcp_wroff_xtra];
	mp->b_rptr = rptr;
	mp->b_wptr = &rptr[connp->tcp_hdr_len];
	bcopy(connp->tcp_iphc, rptr, connp->tcp_hdr_len);
	ipha = (ipha_t *)rptr;
	ipha->ipha_length = htons(connp->tcp_hdr_len);
	tcph = (tcph_t *)&rptr[connp->tcp_ip_hdr_len];
	tcph->th_flags[0] = (uint8_t)ctl;
	if (ctl & TH_RST) {
		BUMP_MIB(tcp_mib.tcpOutRsts);
		BUMP_MIB(tcp_mib.tcpOutControl);
	}
	if (ctl & TH_ACK) {
		connp->tcp_rack = ack;
		connp->tcp_rack_cnt = 0;
		BUMP_MIB(tcp_mib.tcpOutAck);
	}
	BUMP_LOCAL(connp->tcp_obsegs);
	U32_TO_BE32(seq, tcph->th_seq);
	U32_TO_BE32(ack, tcph->th_ack);
	/*
	 * Include the adjustment for a source route if any.
	 */
	sum = (sum >> 16) + (sum & 0xFFFF);
	U16_TO_BE16(sum, tcph->th_sum);
	nca_ip_output(connp, mp);
}

/*
 * Generate a reset based on an inbound packet for which there is no active
 * tcp state that we can find.
 *
 * Note: we free the msg pointed to by mp.
 */
static void
/*ARGSUSED*/
nca_tcp_xmit_early_reset(char *str, if_t *ifp, mblk_t *mp, uint32_t seq,
    uint32_t ack, int ctl)
{
	ipha_t		*ipha;
	tcph_t		*tcph;
	uint32_t	v_hlen_tos_len;
	ipaddr_t	dst;
	uint32_t	ttl_protocol;
	ipaddr_t	src;
	size_t		hlen;
	uint16_t	*up;
	int		i;

	if (mp->b_datap->db_ref != 1) {
		mblk_t *mp1 = copyb(mp);
		freemsg(mp);
		mp = mp1;
		if (!mp)
			return;
	} else if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = nilp(mblk_t);
	}
	ipha = (ipha_t *)mp->b_rptr;
	dst = ipha->ipha_dst;
	src = ipha->ipha_src;
	/*
	 * We skip reversing source route here.
	 * (for now we replace all IP options with EOL)
	 */
	hlen = IPH_HDR_LENGTH(ipha);
	for (i = IP_SIMPLE_HDR_LENGTH; i < (int)hlen; i++)
		mp->b_rptr[i] = IPOPT_EOL;
	/*
	 * Make sure that src address is not a limited broadcast address.
	 * Not all broadcast address checking for the src address is possible,
	 * since we don't know the netmask of the src addr.
	 * No check for destination address is done, since IP will not pass up
	 * a packet with a broadcast dest address to TCP.
	 */
	if (ipha->ipha_src == 0 || ipha->ipha_src == INADDR_BROADCAST) {
		freemsg(mp);
		return;
	}

	tcph = (tcph_t *)&mp->b_rptr[hlen];
	if (tcph->th_flags[0] & TH_RST) {
		freemsg(mp);
		return;
	}
	tcph->th_offset_and_rsrvd[0] = (5 << 4);
	hlen += sizeof (tcph_t);
	mp->b_wptr = &mp->b_rptr[hlen];
	ipha->ipha_length = htons(hlen);
	/* Swap addresses */
	ipha->ipha_src = dst;
	ipha->ipha_dst = src;
	ipha->ipha_ident = 0;
	tcp_xchg(tcph->th_fport, tcph->th_lport, 2);
	U32_TO_BE32(ack, tcph->th_ack);
	U32_TO_BE32(seq, tcph->th_seq);
	U16_TO_BE16(0, tcph->th_win);
	U16_TO_BE16(sizeof (tcph_t), tcph->th_sum);
	tcph->th_flags[0] = (uint8_t)ctl;
	if (ctl & TH_RST) {
		BUMP_MIB(tcp_mib.tcpOutRsts);
		BUMP_MIB(tcp_mib.tcpOutControl);
	}

/* Macros to extract header fields from data already in registers */
#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#define	LENGTH	(v_hlen_tos_len & 0xFFFF)
#define	PROTO	(ttl_protocol & 0xFF)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#define	LENGTH	((v_hlen_tos_len >> 24) | ((v_hlen_tos_len >> 8) & 0xFF00))
#define	PROTO	(ttl_protocol >> 8)
#endif

	v_hlen_tos_len = ((uint32_t *)ipha)[0];

	ttl_protocol =  atomic_add_32_nv(&ifp->ip_ident, 1);

#ifndef _BIG_ENDIAN
	ttl_protocol = (ttl_protocol << 8) | ((ttl_protocol >> 8) & 0xff);
#endif
	ipha->ipha_ident = (uint16_t)ttl_protocol;

	if (!src) {
		freemsg(mp);
		return;
	}

	ttl_protocol = ((uint16_t *)ipha)[4];

	/* pseudo checksum (do it in parts for IP header checksum) */
	dst = (dst >> 16) + (dst & 0xFFFF);
	src = (src >> 16) + (src & 0xFFFF);
	dst += src;

#define	IPH_TCPH_CHECKSUMP(ipha, hlen) \
	    ((uint16_t *)(((uchar_t *)ipha)+(hlen+TCP_CHECKSUM_OFFSET)))

	hlen = (V_HLEN & 0xF) << 2;
	up = IPH_TCPH_CHECKSUMP(ipha, hlen);
	mp->b_datap->db_struioflag = NULL;
	*up = IP_CSUM(mp, hlen, dst + IP_TCP_CSUM_COMP);

	/* checksum */
	dst += ttl_protocol;

	ipha->ipha_fragment_offset_and_flags = 0;

	/* checksum */
	dst += ipha->ipha_ident;

	/* checksum */
	dst += (v_hlen_tos_len >> 16)+(v_hlen_tos_len & 0xFFFF);
	dst += ipha->ipha_fragment_offset_and_flags;

	/* calculate hdr checksum */
	dst = ((dst & 0xFFFF) + (dst >> 16));
	dst = ~(dst + (dst >> 16));
	{
		ipaddr_t u1 = dst;
		ipha->ipha_hdr_checksum = (uint16_t)u1;
	}

	if (! ip_g_forward)
		hlen = ifp->mac_length;
	else
		hlen = 0;

	if (hlen > 0) {
		/*
		 * Need to construct a mac hdr, we're using the incoming
		 * mblk_t for the out-bound packet, so reuse the mac prepend.
		 */
		uint8_t *src;
		uint8_t *dst;

		if ((mp->b_rptr - mp->b_datap->db_base) < hlen) {
			/* Not enough room for mac prepend ? */
			freemsg(mp);
			return;
		}
		/* Adjust back to the begining of the mac prepend */
		mp->b_rptr -= hlen;

		/* Stuff incoming src phys addr into the mac's dst */
		hlen = ifp->mac_addr_len;
		src = mp->b_rptr + hlen;
		dst = mp->b_rptr;
		while (hlen-- > 0)
			*dst++ = *src++;

		/* Stuff if_t's src phys addr into the mac's src */
		hlen = ifp->mac_addr_len;
		src = ifp->mac_mp->b_rptr + hlen;
		dst = mp->b_rptr + hlen;
		while (hlen-- > 0)
			*dst++ = *src++;
	}

	if (! ip_g_forward && hlen > 0) {
		/*
		 * Send our output packets to the NIC.
		 */
		if (nca_deferred_oq_if) {
			squeue_fill(ifp->ouq, mp, ifp->wqp);
		} else {
			nca_wput(ifp->wqp, mp);
		}
	} else {
		/*
		 * IP forwarding enabled or no mac (e.g. MCI Net),
		 * so send our output packets up to IP for routing.
		 */
		putnext(ifp->rqp, mp);
	}
	return;

#undef	rptr
#undef	V_HLEN
#undef	LENGTH
#undef	PROTO
#undef	IPH_TCPH_CHECKSUMP

}

/*
 * Generate a "no listener here" reset in response to the
 * connection request contained within 'mp'
 */
static void
nca_tcp_xmit_listeners_reset(if_t *ifp, mblk_t *mp)
{
	uchar_t		*rptr	= mp->b_rptr;
	uint32_t	seg_len = IPH_HDR_LENGTH(rptr);
	tcph_t		*tcph	= (tcph_t *)&rptr[seg_len];
	uint32_t	seg_seq = BE32_TO_U32(tcph->th_seq);
	uint32_t	seg_ack = BE32_TO_U32(tcph->th_ack);
	uint_t		flags = tcph->th_flags[0];

	seg_len = msgdsize(mp) - (TCP_HDR_LENGTH(tcph) + seg_len);
	if (flags & TH_RST)
		freemsg(mp);
	else if (flags & TH_ACK) {
		nca_tcp_xmit_early_reset("no tcp, reset",
		    ifp, mp, seg_ack, 0, TH_RST);
	} else {
		if (flags & TH_SYN)
			seg_len++;
		nca_tcp_xmit_early_reset("no tcp, reset/ack",
		    ifp, mp, 0, seg_seq + seg_len, TH_RST | TH_ACK);
	}
}

/*
 * We are dying for some reason.  Try to do it gracefully.
 */
static int
nca_tcp_clean_death(conn_t *connp, int err)
{
	te_t	*tep;

	/*
	 * If we are at least part way open and there is error
	 * (err==0 implies no error)
	 * notify our client by a T_DISCON_IND.
	 */
	if ((connp->tcp_state >= TCPS_SYN_SENT) && err) {
		if (connp->tcp_state <= TCPS_SYN_RCVD) {
			/* SYN_SENT or SYN_RCVD */
			BUMP_MIB(tcp_mib.tcpAttemptFails);
		} else if (connp->tcp_state <= TCPS_CLOSE_WAIT) {
			/* ESTABLISHED or CLOSE_WAIT */
			BUMP_MIB(tcp_mib.tcpEstabResets);
		}
	}
	if (connp->twlbolt != NCA_TW_NONE) {
		nca_tw_delete(connp);
	}
	if ((tep = &connp->tcp_ti)->ep != NULL) {
		nca_ti_delete(tep);
	}
	connp->tcp_close = 1;
	return (0);
}

/*
 * Generate ISS, taking into account NDD changes may happen halfway through.
 * (If the iss is not zero, set it.)
 */

static void
/*ARGSUSED*/
nca_tcp_iss_init(conn_t *connp)
{
	printf("nca_tcp_iss_init(): %d\n", tcp_strong_iss);
}

/* Generate an ACK-only (no data) segment for a TCP endpoint */
static mblk_t *
nca_tcp_ack_mp(conn_t *connp)
{

	if (connp->tcp_valid_bits) {
		/*
		 * For the complex case where we have to send some
		 * controls (FIN or SYN), let tcp_xmit_mp do it.
		 * When sending an ACK-only segment (no data)
		 * into a zero window, always set the seq number to
		 * suna, since snxt will be extended past the window.
		 * If we used snxt, the receiver might consider the ACK
		 * unacceptable.
		 */
		return (nca_tcp_xmit_mp(connp, nilp(mblk_t), 0, NULL, NULL,
		    (connp->tcp_zero_win_probe) ?
		    connp->tcp_suna :
		    connp->tcp_snxt, 0));
	} else {
		/* Generate a simple ACK */
		int	data_length;
		uchar_t	*rptr;
		tcph_t	*tcph;
		mblk_t	*mp1;
		int32_t	tcp_hdr_len;
		int32_t	tcp_tcp_hdr_len;

		/*
		 * Allocate space for TCP + IP headers
		 * and link-level header
		 */
		tcp_hdr_len = connp->tcp_hdr_len;
		tcp_tcp_hdr_len = connp->tcp_tcp_hdr_len;
		mp1 = allocb(tcp_hdr_len + tcp_wroff_xtra, BPRI_MED);
		if (!mp1)
			return (nilp(mblk_t));

		/* copy in prototype TCP + IP header */
		rptr = mp1->b_rptr + tcp_wroff_xtra;
		mp1->b_rptr = rptr;
		mp1->b_wptr = rptr + tcp_hdr_len;
		bcopy(connp->tcp_iphc, rptr, connp->tcp_hdr_len);

		tcph = (tcph_t *)&rptr[connp->tcp_ip_hdr_len];

		/*
		 * Set the TCP sequence number.
		 * When sending an ACK-only segment (no data)
		 * into a zero window, always set the seq number to
		 * suna, since snxt will be extended past the window.
		 * If we used snxt, the receiver might consider the ACK
		 * unacceptable.
		 */
		U32_TO_ABE32((connp->tcp_zero_win_probe) ?
		    connp->tcp_suna : connp->tcp_snxt, tcph->th_seq);

		/* set the TCP ACK flag */
		tcph->th_flags[0] = (uchar_t)TH_ACK;
		connp->tcp_rack = connp->tcp_rnxt;
		connp->tcp_rack_cnt = 0;

		/*
		 * set IP total length field equal to
		 * size of TCP + IP headers.
		 */
		((ipha_t *)rptr)->ipha_length = htons(tcp_hdr_len);

		/*
		 * Prime pump for checksum calculation in IP.  Include the
		 * adjustment for a source route if any.
		 */
		data_length = tcp_tcp_hdr_len + connp->tcp_sum;
		data_length = (data_length >> 16) + (data_length & 0xFFFF);
		U16_TO_ABE16(data_length, tcph->th_sum);

		return (mp1);
	}
}

/* Diagnostic routine used to return a string associated with the tcp state. */
static char *
nca_tcp_display(conn_t *connp)
{
	char	buf1[30];
	static char	buf[80];
	char	*cp;

	if (!connp)
		return ("NULL_TCP");
	switch (connp->tcp_state) {
	case TCPS_CLOSED:
		cp = "CLOSED";
		break;
	case TCPS_IDLE:
		cp = "IDLE";
		break;
	case TCPS_BOUND:
		cp = "BOUND";
		break;
	case TCPS_LISTEN:
		cp = "LISTEN";
		break;
	case TCPS_SYN_SENT:
		cp = "SYN_SENT";
		break;
	case TCPS_SYN_RCVD:
		cp = "SYN_RCVD";
		break;
	case TCPS_ESTABLISHED:
		cp = "ESTABLISHED";
		break;
	case TCPS_CLOSE_WAIT:
		cp = "CLOSE_WAIT";
		break;
	case TCPS_FIN_WAIT_1:
		cp = "FIN_WAIT_1";
		break;
	case TCPS_CLOSING:
		cp = "CLOSING";
		break;
	case TCPS_LAST_ACK:
		cp = "LAST_ACK";
		break;
	case TCPS_FIN_WAIT_2:
		cp = "FIN_WAIT_2";
		break;
	case TCPS_TIME_WAIT:
		cp = "TIME_WAIT";
		break;
	default:
		(void) mi_sprintf(buf1, "TCPUnkState(%d)", connp->tcp_state);
		cp = buf1;
		break;
	}
	(void) mi_sprintf(buf, "[%u, %u] %s",
	    ntohs(connp->conn_lport), ntohs(connp->conn_fport), cp);
	return (buf);
}

static unsigned int
nca_tcp_timer(conn_t *connp)
{
	mblk_t		*mp;
	clock_t		first_threshold;
	clock_t		second_threshold;
	clock_t		ms;
	uint32_t	mss;

	first_threshold =  connp->tcp_first_timer_threshold;
	second_threshold = connp->tcp_second_timer_threshold;
	switch (connp->tcp_state) {
	case TCPS_IDLE:
	case TCPS_BOUND:
	case TCPS_LISTEN:
		return (0);
	case TCPS_SYN_RCVD:
		/* FALLTHRU */
	case TCPS_SYN_SENT:
		first_threshold =  connp->tcp_first_ctimer_threshold;
		second_threshold = connp->tcp_second_ctimer_threshold;
		break;
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_CLOSING:
	case TCPS_CLOSE_WAIT:
	case TCPS_LAST_ACK:
		/* If we have data to rexmit */
		if (connp->tcp_suna != connp->tcp_snxt) {
			clock_t	time_to_wait;

			BUMP_MIB(tcp_mib.tcpTimRetrans);
			if (!connp->tcp_xmit_head)
				break;
			time_to_wait = lbolt -
			    (clock_t)connp->tcp_xmit_head->b_prev;
			time_to_wait = MSEC_TO_TICK(connp->tcp_rto) -
			    time_to_wait;
			if (time_to_wait > 0) {
				/*
				 * Timer fired too early, so restart it.
				 */
				TCP_TIMER_RESTART(connp,
				    TICK_TO_MSEC(time_to_wait));
				return (0);
			}
			/*
			 * When we probe zero windows, we force the swnd open.
			 * If our peer acks with a closed window swnd will be
			 * set to zero by tcp_rput(). As long as we are
			 * receiving acks tcp_rput will
			 * reset 'tcp_ms_we_have_waited' so as not to trip the
			 * first and second interval actions.  NOTE: the timer
			 * interval is allowed to continue its exponential
			 * backoff.
			 */
			if (connp->tcp_swnd == 0 || connp->tcp_zero_win_probe) {
#ifdef	NOTYET
				(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
				    "tcp_timer: zero win");
#else
				/*EMPTY*/
				;
#endif
			} else {
				/*
				 * After retransmission, we need to do
				 * slow start.  Set the ssthresh to one
				 * half of current effective window and
				 * cwnd to one MSS.  Also reset
				 * tcp_cwnd_cnt.
				 */
				uint32_t npkt;

				npkt = (MIN((connp->tcp_timer_backoff ?
				    connp->tcp_cwnd_ssthresh : connp->tcp_cwnd),
				    connp->tcp_swnd) >> 1) / connp->tcp_mss;
				if (npkt < 2)
					npkt = 2;
				connp->tcp_cwnd_ssthresh = npkt*connp->tcp_mss;
				connp->tcp_cwnd = connp->tcp_mss;
				connp->tcp_cwnd_cnt = 0;
			}
			break;
		}
		/* TODO: source quench, sender silly window, ... */
		/* If we have a zero window */
		if ((connp->tcp_unsent || connp->xmit.np != NULL) &&
			connp->tcp_swnd == 0) {
			/* Extend window for probe */
			connp->tcp_swnd += MIN(connp->tcp_mss,
			    tcp_zero_win_probesize);
			connp->tcp_zero_win_probe = 1;
			BUMP_MIB(tcp_mib.tcpOutWinProbe);
			return (TH_XMIT_NEEDED);
		}
		/* Handle timeout from sender SWS avoidance. */
		if (connp->tcp_unsent != 0 || connp->xmit.np != NULL) {
			/*
			 * Reset our knowledge of the max send window since
			 * the receiver might have reduced its receive buffer.
			 * Avoid setting tcp_max_swnd to one since that
			 * will essentially disable the SWS checks.
			 */
			connp->tcp_max_swnd = MAX(connp->tcp_swnd, 2);
			return (TH_XMIT_NEEDED);
		}
		/* Is there a FIN that needs to be to re retransmitted? */
		if ((connp->tcp_valid_bits & TCP_FSS_VALID) &&
		    !connp->tcp_fin_acked)
			break;
		/* Nothing to do, return without restarting timer. */
		return (0);
	case TCPS_FIN_WAIT_2:
		/*
		 * User closed the TCP endpoint and peer ACK'ed our FIN.
		 * We waited some time for for peer's FIN, but it hasn't
		 * arrived.  We flush the connection now to avoid
		 * case where the peer has rebooted.
		 */
		(void) nca_tcp_clean_death(connp, 0);
		return (1);
	case TCPS_TIME_WAIT:
		/* FALLTHROUGH */
	default:
#ifdef	NOTYET
		(void) mi_strlog(tcp->tcp_wq, 1, SL_TRACE|SL_ERROR,
		    "tcp_timer: strange state (%d) %s",
		    tcp->tcp_state, tcp_display(tcp));
#else
		    printf("tcp_timer: strange state (%d) %s",
		    connp->tcp_state, nca_tcp_display(connp));
#endif
		return (0);
	}
	if ((ms = connp->tcp_ms_we_have_waited) > second_threshold) {
		/*
		 * For zero window probe, we need to send indefinitely,
		 * unless we have not heard from the other side for some
		 * time...
		 */
		if ((connp->tcp_zero_win_probe == 0) ||
		    (TICK_TO_MSEC(lbolt - connp->tcp_last_recv_time) >
		    second_threshold)) {
			BUMP_MIB(tcp_mib.tcpTimRetransDrop);
			/*
			 * If TCP is in SYN_RCVD state, send back a
			 * RST|ACK as BSD does.  Note that tcp_zero_win_probe
			 * should be zero in TCPS_SYN_RCVD state.
			 */
			if (connp->tcp_state == TCPS_SYN_RCVD) {

				nca_tcp_xmit_ctl("nca_tcp_timer: RST sent on "
				    "timeout in SYN_RCVD",
				    connp, NULL, connp->tcp_snxt,
				    connp->tcp_rnxt,
				    TH_RST | TH_ACK);
			}
			(void) nca_tcp_clean_death(connp, ETIMEDOUT);
			return (1);
		} else {
			/*
			 * Set tcp_ms_we_have_waited to second_threshold
			 * so that in next timeout, we will do the above
			 * check (lbolt - tcp_last_recv_time).  This is
			 * also to avoid overflow.
			 *
			 * We don't need to decrement tcp_timer_backoff
			 * to avoid overflow because it will be decremented
			 * later if new timeout value is greater than
			 * tcp_rexmit_interval_max.  In the case when
			 * tcp_rexmit_interval_max is greater than
			 * second_threshold, it means that we will wait
			 * longer than second_threshold to send the next
			 * window probe.
			 */
			connp->tcp_ms_we_have_waited = second_threshold;
		}
	} else if (ms > first_threshold && connp->tcp_rtt_sa != 0) {
		/*
		 * We have been retransmitting for too long...  The RTT
		 * we calculated is probably incorrect.  Reinitialize it.
		 * Need to compensate for 0 tcp_rtt_sa.  Reset
		 * tcp_rtt_update so that we won't accidentally cache a
		 * bad value.
		 */
		connp->tcp_rtt_sd += (connp->tcp_rtt_sa >> 3) +
		    (connp->tcp_rtt_sa >> 5);
		connp->tcp_rtt_sa = 0;
		connp->tcp_rtt_update = 0;
	}
	connp->tcp_timer_backoff++;
	if ((ms = (connp->tcp_rtt_sa >> 3) + connp->tcp_rtt_sd +
	    tcp_rexmit_interval_extra + (connp->tcp_rtt_sa >> 5)) <
	    tcp_rexmit_interval_min) {
		/*
		 * This means the original RTO is tcp_rexmit_interval_min.
		 * So we will use tcp_rexmit_interval_min as the RTO value
		 * and do the backoff.
		 */
		ms = tcp_rexmit_interval_min << connp->tcp_timer_backoff;
	} else {
		ms <<= connp->tcp_timer_backoff;
	}
	if (ms > tcp_rexmit_interval_max) {
		ms = tcp_rexmit_interval_max;
		/*
		 * ms is at max, decrement tcp_timer_backoff to avoid
		 * overflow.
		 */
		connp->tcp_timer_backoff--;
	}
	connp->tcp_ms_we_have_waited += ms;
	if (connp->tcp_zero_win_probe == 0) {
		connp->tcp_rto = ms;
	}
	TCP_TIMER_RESTART(connp, ms);
	/*
	 * This is after a timeout and tcp_rto is backed off.  Set
	 * tcp_set_timer to 1 so that next time RTO is updated, we will
	 * restart the timer with a correct value.
	 */
	connp->tcp_set_timer = 1;
	mss = connp->tcp_snxt - connp->tcp_suna;
	if (mss > connp->tcp_mss)
		mss = connp->tcp_mss;
	if (mss > connp->tcp_swnd && connp->tcp_swnd != 0)
		mss = connp->tcp_swnd;

	if ((mp = connp->tcp_xmit_head) != NULL)
		mp->b_prev = (mblk_t *)lbolt;
	mp = nca_tcp_xmit_mp(connp, mp, mss, NULL, NULL, connp->tcp_suna, 1);
	if (mp) {
		connp->tcp_csuna = connp->tcp_snxt;
		BUMP_MIB(tcp_mib.tcpRetransSegs);
		UPDATE_MIB(tcp_mib.tcpRetransBytes, msgdsize(mp->b_cont));
		nca_ip_output(connp, mp);
		/*
		 * When slow start after retransmission begins, start with
		 * this seq no.  tcp_rexmit_max marks the end of special slow
		 * start phase.  tcp_snd_burst controls how many segments
		 * can be sent because of an ack.
		 */
		connp->tcp_rexmit_nxt = connp->tcp_suna;
		connp->tcp_snd_burst = TCP_CWND_SS;
		if ((connp->tcp_valid_bits & TCP_FSS_VALID) &&
		    (connp->tcp_unsent == 0 && connp->xmit.np == NULL)) {
			connp->tcp_rexmit_max = connp->tcp_fss;
		} else {
			connp->tcp_rexmit_max = connp->tcp_snxt;
		}
		connp->tcp_rexmit = 1;
		connp->tcp_dupack_cnt = 0;

#ifdef	NOTYET
		/*
		 * Remove all rexmit SACK blk to start from fresh.
		 */
		if (connp->tcp_snd_sack_ok && connp->tcp_notsack_list != NULL) {
			TCP_NOTSACK_REMOVE_ALL(connp->tcp_notsack_list);
			connp->tcp_num_notsack_blk = 0;
			connp->tcp_cnt_notsack_list = 0;
		}
#endif
	}
	return (0);
}

tw_t *
nca_tw_init(tw_t *twp, squeue_t *sqp)
{
	if (twp == NULL) {
		twp = kmem_alloc(sizeof (*twp), KM_SLEEP);
		twp->lbolt1 = NCA_TW_NONE;
		twp->lbolt2 = NCA_TW_NONE;
		twp->lbolt3 = NCA_TW_NONE;
		twp->head = NULL;
		twp->tail = NULL;
		twp->tid = 0;
	}
	twp->ep = (void *)sqp;
	return (twp);
}

void
nca_tw_fini(tw_t *twp)
{
	if (twp->tid) {
		(void) untimeout(twp->tid);
		twp->tid = 0;
	}
}

static void
nca_tw_add(conn_t *connp)
{
	tw_t	*twp = nca_gv[CPU->cpu_seqid].tcp_tw;
	clock_t	exec = lbolt + MSEC_TO_TICK(tcp_time_wait_interval);

	NCA_DEBUG_COUNTER(&tw_add, 1);
	CONN_REFHOLD(connp);
	connp->twlbolt = exec;
	connp->twnext = NULL;
	if ((connp->twprev = twp->tail) != NULL) {
		twp->tail->twnext = connp;
	} else {
		NCA_DEBUG_COUNTER(&tw_add1, 1);
		twp->head = connp;
		NCA_TW_LBOLTS(twp, exec);
	}
	twp->tail = connp;
	NCA_DEBUG_COUNTER(&tw_on, 1);
}

static void
nca_tw_delete(conn_t *connp)
{
	tw_t	*twp = nca_gv[CPU->cpu_seqid].tcp_tw;

	NCA_DEBUG_COUNTER(&tw_delete, 1);
	connp->twlbolt = NCA_TW_NONE;

	if (connp->twprev != NULL) {
		connp->twprev->twnext = connp->twnext;
	} else {
		if ((twp->head = connp->twnext) != NULL) {
			NCA_TW_LBOLTS(twp, twp->head->twlbolt);
		} else {
			/* LINTED */
			NCA_TW_LBOLTS(twp, NCA_TW_NONE);
		}
	}
	if (connp->twnext != NULL) {
		connp->twnext->twprev = connp->twprev;
	} else {
		twp->tail = connp->twprev;
	}
	NCA_DEBUG_COUNTER(&tw_on, -1);
	CONN_REFRELE(connp);
}

void
nca_tw_reap(tw_t *twp)
{
	conn_t	*connp;
	clock_t	now;
	clock_t	exec;

	NCA_DEBUG_COUNTER(&tw_reap, 1);
	now = lbolt;
	exec = twp->lbolt2;
	twp->lbolt1 = NCA_TW_NONE;
	if (now > exec) {
		/* Round down to bucket containing lbolt */
		NCA_DEBUG_COUNTER(&tw_reap1, 1);
		exec = now;
		exec -= exec % NCA_TW_LBOLT;
	} else if (now < exec) {
		/* Why are we here? */
		NCA_DEBUG_COUNTER(&tw_reap2, 1);
		return;
	}
	if (twp->tid) {
		timeout_id_t tid = twp->tid;

		twp->tid = 0;
		(void) untimeout(tid);
	}
	while ((connp = twp->head) != NULL && connp->twlbolt <= exec) {
		if ((twp->head = connp->twnext) != NULL) {
			twp->head->twprev = NULL;
		} else {
			twp->tail = NULL;
		}
		NCA_DEBUG_COUNTER(&tw_on, -1);
		NCA_DEBUG_COUNTER(&tw_reap3, 1);
		connp->twlbolt = NCA_TW_NONE;
		CONN_REFRELE(connp);
	}
	if (connp != NULL) {
		NCA_TW_LBOLTS(twp, connp->twlbolt);
		NCA_DEBUG_COUNTER(&tw_reap4, 1);
	} else {
		/* LINTED */
		NCA_TW_LBOLTS(twp, NCA_TW_NONE);
		NCA_DEBUG_COUNTER(&tw_reap5, 1);
	}
}

static void
nca_tw_timer(tw_t *twp)
{
	conn_t	*connp;
	clock_t	now;
	clock_t	exec;

	NCA_DEBUG_COUNTER(&tw_timer, 1);
	now = lbolt;
	exec = twp->lbolt3;
	twp->lbolt1 = NCA_TW_NONE;
	if (now > exec) {
		/* Round down to bucket containing lbolt */
		NCA_DEBUG_COUNTER(&tw_timer1, 1);
		exec = now;
		exec -= exec % NCA_TW_LBOLT;
	} else if (now < exec) {
		/* Why are we here? */
		NCA_DEBUG_COUNTER(&tw_timer2, 1);
		return;
	}
	if (twp->tid) {
		timeout_id_t tid = twp->tid;

		twp->tid = 0;
		(void) untimeout(tid);
	}
	while ((connp = twp->head) != NULL && connp->twlbolt <= exec) {
		if ((twp->head = connp->twnext) != NULL) {
			twp->head->twprev = NULL;
		} else {
			twp->tail = NULL;
		}
		NCA_DEBUG_COUNTER(&tw_on, -1);
		NCA_DEBUG_COUNTER(&tw_timer3, 1);
		connp->twlbolt = NCA_TW_NONE;
		CONN_REFRELE(connp);
	}
	if (connp != NULL) {
		NCA_TW_LBOLTS(twp, connp->twlbolt);
		NCA_DEBUG_COUNTER(&tw_timer4, 1);
	} else {
		/* LINTED */
		NCA_TW_LBOLTS(twp, NCA_TW_NONE);
		NCA_DEBUG_COUNTER(&tw_timer5, 1);
	}
}

static void
nca_tw_fire(void *arg)
{
	tw_t	*twp = arg;
	conn_t	*connp = twp->head;
	mblk_t	*mp;

	NCA_DEBUG_COUNTER(&tw_fire, 1);
	if (twp->tid == 0) {
		/* Why are we here? (race with untimeout()?) */
		NCA_DEBUG_COUNTER(&tw_fire1, 1);
		return;
	}
	if (connp == NULL) {
		/* Why are we here? (race with reclain or reap?) */
		NCA_DEBUG_COUNTER(&tw_fire2, 1);
		return;
	}
	mp = kmem_zalloc(sizeof (*mp), KM_NOSLEEP);
	if (mp == NULL) {
		NCA_DEBUG_COUNTER(&tw_fire3, 1);
		return;
	}
	mp = squeue_ctl(mp, twp, IF_TIME_WAIT);
	squeue_fill(connp->inq, mp, NULL);
}


/*
 * TCP_TIMER code - we implement a per if_t (interface) doubly linked
 * list of tb_t's (timer buckets) which contain a doubly linked list of
 * pending te_t's (timer event).
 *
 * Note: we use only the 2nd and 3rd phase of the 3 phase timer.
 */

#define	NCA_TI_MS 100

#define	NCA_TI_LBOLT MSEC_TO_TICK(NCA_TI_MS)
#define	NCA_TI_PHASE3 NCA_TI_LBOLT

#define	NCA_TI_LBOLTS(tip, _lbolt) {					\
	(tip)->exec = _lbolt;						\
	(tip)->lbolt2 = _lbolt;						\
	(tip)->lbolt3 = _lbolt + NCA_TI_PHASE3;				\
}

ti_t *
nca_ti_init(ti_t *tip, squeue_t *sqp)
{
	if (tip == NULL) {
		tip = kmem_zalloc(sizeof (*tip), KM_SLEEP);
		tip->exec = NCA_TI_NONE;
		tip->lbolt2 = NCA_TI_NONE;
		tip->lbolt3 = NCA_TI_NONE;
		tip->head = NULL;
		tip->tail = NULL;
		tip->tid = 0;
	}
	tip->ep = (void *)sqp;
	return (tip);
}

void
nca_ti_fini(ti_t *tip)
{
	tb_t	*tbp;

	if (tip->tid) {
		(void) untimeout(tip->tid);
		tip->tid = 0;
	}
	while ((tbp = tip->head) != NULL) {
		if ((tip->head = tbp->next) == NULL)
			tip->tail = NULL;
		kmem_cache_free(nca_tb_ti_cache, tbp);
	}
}

static void
nca_ti_fire(void *arg)
{
	ti_t	*tip = arg;
	mblk_t	*mp;
	clock_t	exec;
	clock_t	now;

	NCA_DEBUG_COUNTER(&ti_fire, 1);
	if (tip->tid == 0) {
		/* Why are we here? (race with untimeout()?) */
		NCA_DEBUG_COUNTER(&ti_fire1, 1);
		return;
	}
	now = lbolt;
	exec = tip->lbolt3;
	if (now > exec) {
		/* Round down to bucket containing lbolt */
		exec = now;
		exec -= exec % NCA_TI_LBOLT;
		NCA_DEBUG_COUNTER(&ti_fire2, 1);
	} else if (now < exec) {
		/* Why are we here? (missed an untimeout()?) */
		NCA_DEBUG_COUNTER(&ti_fire3, 1);
		return;
	}
	mp = kmem_zalloc(sizeof (*mp), KM_NOSLEEP);
	if (mp == NULL) {
		NCA_DEBUG_COUNTER(&ti_fire4, 1);
		return;
	}
	mp = squeue_ctl(mp, tip, IF_TCP_TIMER);
	squeue_fill((squeue_t *)tip->ep, mp, NULL);
}

static void
nca_ti_add(conn_t *connp, clock_t exec)
{
	clock_t	mod = exec % NCA_TI_LBOLT;
	ti_t	*tip = nca_gv[CPU->cpu_seqid].tcp_ti;
	tb_t	*tbp;
	te_t	*tep;
	int	kmflag = connp->inq->sq_isintr ? KM_NOSLEEP : KM_SLEEP;

	NCA_DEBUG_COUNTER(&ti_add, 1);

	ASSERT(connp->tcp_ti.ep == NULL);

	if (mod) {
		/* Roundup to next TCP_TIMER bucket */
		exec += NCA_TI_LBOLT - mod;
	}

	/* Find a bucket to insert */

	tbp = tip->tail;
	if (tbp != NULL && exec > tbp->exec && tbp->head == NULL) {
		/*
		 * Have a tail bucket and timer is in the future and
		 * tail bucket is empty, so just reuse the bucket.
		 */
		tbp->exec = exec;
		if (tip->head == tbp && tip->tid) {
			timeout_id_t tid = tip->tid;

			tip->tid = 0;
			(void) untimeout(tid);
		}
		NCA_DEBUG_COUNTER(&ti_add_reuse, 1);
	} else if (tbp == NULL || exec != tbp->exec) {
		tb_t	*tbpp = NULL;

		/* See if we are able to insert a new bucket at the end. */
		if (tbp != NULL && exec > tbp->exec) {
			/* Append a new bucket */
			NCA_DEBUG_COUNTER(&ti_add1, 1);
			tbpp = tbp;
			tbp = NULL;
		} else {
			/*
			 * We have tried the 2 simple cases & failed
			 *	- an empty bucket at the end
			 *	- be able to insert a new bucket at the end.
			 * Now lets go through the entire list and find
			 * the insertion point.
			 */
			for (tbp = tip->head; tbp != NULL; ) {
				if (exec == tbp->exec) {
					/* Found the bucket to insert in */
					break;
				}
				if (exec < tbp->exec) {
					/* Insert a new bucket */
					NCA_DEBUG_COUNTER(&ti_add2, 1);
					tbp = NULL;
					break;
				}
				tbpp = tbp;
				tbp = tbp->next;
			}
		}
		if (tbp == NULL) {
			/*
			 * Insert after tbpp (previous tbp).
			 */
			tbp = kmem_cache_alloc(nca_tb_ti_cache, kmflag);
			if (tbp == NULL) {
				/* XXX need a fall-back plan here !!! */
				connp->tcp_ti.ep = NULL;
				NCA_DEBUG_COUNTER(&ti_add_failed, 1);
				return;
			}

			tbp->exec = exec;
			if (tbpp != NULL) {
				if ((tbp->next = tbpp->next) == NULL)
					tip->tail = tbp;
				tbpp->next = tbp;
			} else {
				if ((tbp->next = tip->head) == NULL) {
					tip->tail = tbp;
				} else if (tip->tid) {
					timeout_id_t tid = tip->tid;

					tip->tid = 0;
					(void) untimeout(tid);
				}
				tip->head = tbp;
			}
			tbp->head = NULL;
			tbp->tail = NULL;
		}
	}

	/* Add the te_t to the tail of the bucket */
	if ((tep = tbp->tail) != NULL) {
		tep->next = &connp->tcp_ti;
		connp->tcp_ti.prev = tep;
	} else {
		tbp->head = &connp->tcp_ti;
		connp->tcp_ti.prev = NULL;
	}
	tbp->tail = &connp->tcp_ti;
	connp->tcp_ti.next = NULL;
	connp->tcp_ti.tbp = tbp;
	connp->tcp_ti.ep = (void *)connp;
	if (tip->exec != NCA_TI_INPROC && (! tip->tid)) {
		/* New ti_t head, so restart the timers */
		NCA_TI_LBOLTS(tip, tip->head->exec);
		NCA_DEBUG_COUNTER(&ti_add3, 1);
		NCA_DEBUG_COUNTER(&ti_add4, tip->lbolt3 - lbolt);
		if (tip->lbolt3 < lbolt) {
			NCA_DEBUG_COUNTER(&ti_add5, 1);
		}
		tip->tid = timeout((pfv_t)nca_ti_fire, tip, tip->lbolt3-lbolt);
	}
	NCA_DEBUG_COUNTER(&ti_on, 1);
}

static void
nca_ti_delete(te_t *tep)
{
	NCA_DEBUG_COUNTER(&ti_delete, 1);

	tep->ep = NULL;
	if (tep->prev != NULL)
		tep->prev->next = tep->next;
	else
		tep->tbp->head = tep->next;
	if (tep->next != NULL)
		tep->next->prev = tep->prev;
	else
		tep->tbp->tail = tep->prev;
	NCA_DEBUG_COUNTER(&ti_on, -1);
	/*
	 * Note: if we just emptied the bucket we may want to unlink the
	 * bucket, but we don't currently have a pointer back to the ti_t
	 * (nor to the prev tb_t) so just leave it to one of the timer
	 * exec funs below.
	 */
	if (tep->tbp->head == NULL) {
		NCA_DEBUG_COUNTER(&ti_delete1, 1);
		if (tep->tbp->next == NULL) {
			NCA_DEBUG_COUNTER(&ti_delete2, 1);
		}
	}
}

static void
nca_ti_reap(ti_t *tip)
{
	tb_t	*tbp;
	te_t	*tep;
	conn_t	*connp;
	clock_t	now;
	clock_t	exec;

	NCA_DEBUG_COUNTER(&ti_reap, 1);
	now = lbolt;
	exec = tip->lbolt2;
	if (now > exec) {
		/* Round down to bucket containing lbolt */
		NCA_DEBUG_COUNTER(&ti_reap1, 1);
		exec = now;
		exec -= exec % NCA_TI_LBOLT;
	} else if (now < exec) {
		/* Why are we here? */
		NCA_DEBUG_COUNTER(&ti_reap2, 1);
		return;
	}

	/* Mark that ti's are being reaped */
	tip->exec = NCA_TI_INPROC;

	if (tip->tid) {
		timeout_id_t tid = tip->tid;

		tip->tid = 0;
		(void) untimeout(tid);
	}

	while ((tbp = tip->head) != NULL &&
		(tbp->exec <= exec || tbp->head == NULL)) {
		/*
		 * Have an expired or empty TCP TIMER tb_t, so unlink
		 * the tb_t, call into TCP for any TCP TIMER processing
		 * (if needed), then free the tb_t.
		 *
		 * This part is slightly tricky because we might call
		 * nca_tcp_input which might in turn call nca_ti_add.
		 * Though we have some protection (by setting NCA_TI_INPROC)
		 * we have to be careful since nca_ti_add is walking and
		 * changing (only inserting) these lists.
		 */

		/* Unlink the tb_t */
		if ((tip->head = tbp->next) == NULL)
			tip->tail = NULL;

		while ((tep = tbp->head) != NULL) {
			if ((tbp->head = tep->next) != NULL)
				tbp->head->prev = NULL;
			else
				tbp->tail = NULL;
			NCA_DEBUG_COUNTER(&ti_on, -1);
			NCA_DEBUG_COUNTER(&ti_reap3, 1);
			if ((connp = (conn_t *)tep->ep) == NULL) {
				NCA_DEBUG_COUNTER(&ti_reap4, 1);
				continue;
			}
			tep->ep = NULL;
			/* Put a REFHOLD on the conn_t while in tcp_input() */
			CONN_REFHOLD(connp);
			nca_tcp_input(connp, NULL, (void *)CONN_TCP_TIMER);
			/* Do a REFRELE for our REFHOLD */
			CONN_REFRELE(connp);
		}

		if (tbp->head == NULL) {
			/* Only free if empty (i.e. not reused) */
			NCA_DEBUG_COUNTER(&ti_reap5, 1);
			kmem_cache_free(nca_tb_ti_cache, tbp);
		}
	}

	if ((tbp = tip->head) != NULL) {
		NCA_TI_LBOLTS(tip, tbp->exec);
		tip->tid = timeout((pfv_t)nca_ti_fire, tip, tip->lbolt3-lbolt);
	} else {
		NCA_TI_LBOLTS(tip, NCA_TI_NONE);
	}
}

static void
nca_ti_timer(ti_t *tip)
{
	clock_t	exec = tip->lbolt3;
	tb_t	*tbp;
	clock_t	now = lbolt;
	te_t	*tep;
	conn_t	*connp;

	NCA_DEBUG_COUNTER(&ti_timer, 1);
	if (now > exec) {
		/* Round down to bucket containing lbolt */
		exec = now;
		exec -= exec % NCA_TI_LBOLT;
		NCA_DEBUG_COUNTER(&ti_timer1, 1);
	} else if (now < exec) {
		/* Why are we here? */
		NCA_DEBUG_COUNTER(&ti_timer2, 1);
		return;
	}

	tip->exec = NCA_TI_INPROC;

	/*
	 * We use our lbolt above so that we have a bounded amount
	 * of work to do (i.e. if we used lbolt in the compare and
	 * the rate of work production > consumtion we would never
	 * finish).
	 */
	while ((tbp = tip->head) != NULL &&
		(tbp->exec + NCA_TI_PHASE3 <= exec || tbp->head == NULL)) {
		/*
		 * Have an expired or empty TCP TIMER tb_t, so process
		 * any te_t's (if needed) then free up the tb_t.
		 */
		if ((tip->head = tbp->next) == NULL)
			tip->tail = NULL;

		while ((tep = tbp->head) != NULL) {
			if ((tbp->head = tep->next) != NULL)
				tbp->head->prev = NULL;
			else
				tbp->tail = NULL;
			NCA_DEBUG_COUNTER(&ti_on, -1);
			NCA_DEBUG_COUNTER(&ti_timer3, 1);
			if ((connp = (conn_t *)tep->ep) == NULL) {
				continue;
			}
			tep->ep = NULL;
			/* Put a REFHOLD on the conn_t while in tcp_input() */
			CONN_REFHOLD(connp);
			nca_tcp_input(connp, NULL, (void *)CONN_TCP_TIMER);
			/* Do a REFRELE for our REFHOLD */
			CONN_REFRELE(connp);
		}
		if (tbp->head == NULL) {
			/* Only free if empty (i.e. not reused) */
			NCA_DEBUG_COUNTER(&ti_timer4, 1);
			kmem_cache_free(nca_tb_ti_cache, tbp);
		}
	}
	if ((tbp = tip->head) != NULL) {
		NCA_TI_LBOLTS(tip, tbp->exec);
		NCA_DEBUG_COUNTER(&ti_timer5, 1);
		NCA_DEBUG_COUNTER(&ti_timer6, tip->lbolt3 - lbolt);
		tip->tid = timeout((pfv_t)nca_ti_fire, tip, tip->lbolt3-lbolt);
	} else {
		NCA_TI_LBOLTS(tip, NCA_TI_NONE);
	}
}

static int
/*ARGSUSED*/
nca_param_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	ncaparam_t	*ncapa = (ncaparam_t *)cp;

	(void) mi_mpprintf(mp, "%ld", ncapa->param_val);
	return (0);
}

static int
/*ARGSUSED*/
nca_version_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%d.%d", nca_major_version, nca_minor_version);
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_version_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%d", nca_httpd_version);
	return (0);
}

static int
/*ARGSUSED*/
nca_logd_version_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%d", nca_logd_version);
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_door_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%s", nca_httpd_door_path);
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_door_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	extern door_handle_t nca_httpd_door_hand;

	if (strlen(value) + 1 > PATH_MAX) {
		return (EOVERFLOW);
	}
	if (nca_httpd_door_hand != NULL) {
		cmn_err(CE_WARN, "NCA HTTP door has already been initialized,"
		    " door path cannot be changed.");
		return (EEXIST);
	}
	/*
	 * XXX Need to reinitialize the door if it is already initialized.
	 * We do not do it now because in the first release of NCA, we
	 * should not encounter this problem.
	 */
	bcopy(value, nca_httpd_door_path, strlen(value) + 1);
	return (0);
}

static int
/*ARGSUSED*/
nca_ppmax_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%lu", ptob(nca_ppmax));
	return (0);
}

static int
/*ARGSUSED*/
nca_availrmem_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%lu", ptob(availrmem));
	return (0);
}

static int
/*ARGSUSED*/
nca_maxkmem_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%lu", nca_maxkmem);
	return (0);
}

static int
/*ARGSUSED*/
nca_debug_counter_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%d", (int)nca_debug_counter);
	return (0);
}

static int
/*ARGSUSED*/
nca_ppmax_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char *end;
	pgcnt_t new_value;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value == 0 || new_value > (~(ulong_t)0))
		return (EINVAL);
	nca_ppmax = btopr(new_value);
	return (0);
}

static int
/*ARGSUSED*/
nca_debug_counter_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char *end;
	int new_value;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value < 0 || new_value > 1)
		return (EINVAL);
	nca_debug_counter = new_value;
	return (0);
}

static int
/*ARGSUSED*/
nca_vpmax_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char *end;
	pgcnt_t new_value;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value == 0 || new_value > (~(ulong_t)0))
		return (EINVAL);
	nca_vpmax = btopr(new_value);
	return (0);
}

static int
/*ARGSUSED*/
nca_vpmax_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%lu", ptob(nca_vpmax));
	return (0);
}

static int
/*ARGSUSED*/
nca_logging_on_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	(void) mi_mpprintf(mp, "%d", (int)nca_logging_on);
	return (0);
}

static int
/*ARGSUSED*/
nca_logging_on_set(queue_t *q, mblk_t *mp, char *value, caddr_t nu)
{
	char *end;
	int new_value;
	int old_value = nca_logging_on;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value < 0 || new_value > 1)
		return (EINVAL);
	if (nca_fio_cnt(&logfio) == 0)
		return (EINVAL);
	nca_logging_on = new_value;
	if (new_value == 0 && old_value == 1) {
		/* Advise logger that loggin is being turned off */
		nca_logit_off();
	} else {
		/* Logging is being turned on */
		nca_logger_init(NULL);
	}
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_door_inst_get(queue_t *q, mblk_t *mp, caddr_t nu)
{
	extern void nca_dcb_report(mblk_t *);

	/* Output meaningful data for all door instances */
	nca_dcb_report(mp);
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_door_inst_set(queue_t *q, mblk_t *mp, char *value, caddr_t nu)
{
	char	*cp = value;
	ssize_t	len = strlen(value);

	extern int nca_dcb_add(char *);
	extern int nca_dcb_del(char *);

	/*
	 * Parse 2 args, syntax:
	 *
	 *	add path_to_door
	 *
	 *	del[ete] path_to_door
	 */
	if ((cp = strncasestr(value, "add", len)) == value) {
		if ((cp = strnchr(value, ' ', len)) == NULL) {
			/* No arg ? */
			return (EINVAL);
		}
		while (*cp == ' ')
			cp++;
		if (*cp == 0) {
			/* No arg ? */
			return (EINVAL);
		}
		return (nca_dcb_add(cp));
	} else if ((cp = strncasestr(value, "del", len)) == value) {
		if ((cp = strnchr(value, ' ', len)) == NULL) {
			/* No arg ? */
			return (EINVAL);
		}
		while (*cp == ' ')
			cp++;
		if (*cp == 0) {
			/* No arg ? */
			return (EINVAL);
		}
		return (nca_dcb_del(cp));
	} else {
		/* Only support the verbs "add" and "del[ete]" */
		return (EINVAL);
	}
}

static int
/*ARGSUSED*/
nca_httpd_door_addr_get(queue_t *q, mblk_t *mp, caddr_t nu)
{
	extern void nca_hcb_report(mblk_t *);

	/* Output meaningful data for all door/host instances */
	nca_hcb_report(mp);
	return (0);
}

static int
/*ARGSUSED*/
nca_httpd_door_addr_set(queue_t *q, mblk_t *mp, char *value, caddr_t nu)
{
	ipaddr_t IPaddr;
	uint16_t TCPport;
	char	*cp = value;
	ssize_t	len = strlen(value);
	int ac = 0;
	char *av[6];

	extern int nca_hcb_add(char *, ipaddr_t, uint16_t, char *, char *);
	extern int nca_hcb_del(char *, ipaddr_t, uint16_t, char *, char *);

	/*
	 * Parse up to 5 args, syntax:
	 *
	 *	add path_to_door [IPaddr[:TCPport] [Host: [path_to_doc_root]]]
	 *
	 *	del[ete] path_to_door [IPaddr[:TCPport] [Host:]]
	 *
	 * Where:
	 *
	 *	add/del - add or delete an hcb_t.
	 *
	 *	path_to_door - absolute path to door instance.
	 *
	 *	IPaddr[:TCPport] - IP address using dot notation or '*'
	 *		for INADDR_ANY (defaults to INADDR_ANY), and an
	 *		optional TCPport in decimal (defaults to 80).
	 *
	 *	Host: - HTTP 'Host:' header argument or '*' for any
	 *		(defaults to '*').
	 *
	 *	path_to_doc_root - absolute path to the document root
	 *		(defaults to "/").
	 */
	for (ac = 0; ac < 6; ac++) {
		if (len < 0)
			break;
		av[ac] = cp;
		if ((cp = strnchr(cp, ' ', len)) == NULL) {
			ac++;
			break;
		}
		len -= cp - av[ac];
		while (len > 0 && *cp == ' ') {
			len--;
			*cp++ = NULL;
		}
	}
	if (ac < 2) {
		/* Not enough args */
		return (EINVAL);
	}
	if (ac >= 3) {
		/* Have an IPaddr[:TCPport] spec */
		cp = av[2];
		if (*cp == '*') {
			cp++;
			IPaddr = INADDR_ANY;
		} else {
			int	ix;
			char	c;
			int	n;

			IPaddr = 0;
			for (ix = 0; ix < 4; ix++) {
				n = 0;
				while ((c = *cp) != NULL && c >= '0' &&
								c <= '9') {
					cp++;
					n *= 10;
					n += c - '0';
				}
				if (c != NULL && c == '.')
					cp++;
				IPaddr <<= 8;
				IPaddr |= n;
				if (c == NULL || c != '.')
					break;
			}
			if (ix != 3)
				return (EINVAL);
		}
		if (*cp == ':') {
			cp++;
			TCPport = atoin(cp, strlen(cp));
		} else {
			TCPport = 80;
		}
	} else {
		ac++;
		IPaddr = INADDR_ANY;
		TCPport = 80;
	}
	if (ac < 4) {
		ac++;
		av[3] = "*";
	}
	if (ac < 5) {
		ac++;
		av[4] = "/";
	}
	if (ac > 5) {
		/* Too many args */
		return (E2BIG);
	}
	if (strncasestr(av[0], "add", len) == av[0]) {
		if (ac < 5) {
			/* Not enough args */
			return (EINVAL);
		}
		return (nca_hcb_add(av[1], IPaddr, TCPport, av[3], av[4]));
	} else if ((cp = strncasestr(value, "del", len)) == value) {
		return (nca_hcb_del(av[1], IPaddr, TCPport, av[3], av[4]));
	} else {
		/* Only support the verbs "add" and "del[ete]" */
		return (EINVAL);
	}
}

static int
/*ARGSUSED*/
nca_log_file_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	int i;

	if (nca_fio_cnt(&logfio) == 0)
		return (0);

	/*
	 * This can probably be allowed if needed ?? Won't be able to use
	 * the nca_fio_* macros once the logging is on.
	 */
	if (nca_logging_on == 1)
		return (EINVAL);

	nca_fio_ix(&logfio) = 0;
	for (i = 0; i < nca_fio_cnt(&logfio); i++) {
		(void) mi_mpprintf(mp, "%s", nca_fio_name(&logfio));
		nca_fio_ix(&logfio)++;
	}

	return (0);
}

static int
/*ARGSUSED*/
nca_log_file_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{

	/* Can't have more than NCA_FIOV_SZ log files */
	if (nca_fio_cnt(&logfio) == NCA_FIOV_SZ)
		return (EINVAL);

	/* Don't let anyone add more files if logging is already going on */
	if (nca_logging_on == 1)
		return (EINVAL);

	if (nca_fio_nxt_name(&logfio) != NULL)
		kmem_free(nca_fio_nxt_name(&logfio),
				strlen(nca_fio_nxt_name(&logfio)) + 1);
	nca_fio_nxt_name(&logfio) = kmem_alloc(strlen(value) + 1, KM_SLEEP);
	if (nca_fio_nxt_name(&logfio) == NULL) {
		cmn_err(CE_WARN, "nca_log_file_set: cannot allocate memory");
		return (EINVAL);
	}
	bcopy(value, nca_fio_nxt_name(&logfio), strlen(value) + 1);
	nca_fio_cnt(&logfio)++;
	return (0);
}

/* ARGSUSED */
static int
conn_status(queue_t *q, mblk_t *mp, caddr_t cp)
{
	boolean_t	noTIME_WAIT = (int)cp;
	connf_t		*connfp;
	conn_t		*connp;
	int		ix;
	int		cdl, cdr, idl, idr, wdl, wdr;
	int		skip = 0;

	(void) mi_mpprintf(mp, " conn_t " MI_COL_HDRPAD_STR "   ref:"
	    "create ti   tw  [lport, fport] TCP_STATE");

	for (ix = 0; ix < nca_conn_fanout_size; ix++) {
		connfp = &nca_conn_fanout[ix];
		mutex_enter(&connfp->lock);
		for (connp = connfp->head;
		    connp != NULL;
		    connp = connp->hashnext) {
			if (noTIME_WAIT && connp->tcp_state == TCPS_TIME_WAIT) {
				/* No TIME_WAIT please */
				skip++;
				continue;
			}
			cdr = lbolt - connp->create;
			if (connp->tcp_ti.ep != 0)
				idr = connp->tcp_ti.tbp->exec - lbolt;
			else
				idr = 0;
			if ((wdr = connp->twlbolt) != 0)
				wdr -= lbolt;
			cdl = cdr / hz;
			cdr -= cdl * hz;
			idl = idr / hz;
			idr -= idl * hz;
			wdl = wdr / hz;
			wdr -= wdl * hz;
			(void) mi_mpprintf(mp, MI_COL_PTRFMT_STR
			    " %d(%d): %d.%02d %d.%02d %d.%02d %s",
			    (void *)connp,
			    connp->ref, connp->tcp_refed,
			    cdl, cdr, idl, idr, wdl, wdr,
			    nca_tcp_display(connp));
		}
		mutex_exit(&connfp->lock);
	}
	if (noTIME_WAIT) {
		(void) mi_mpprintf(mp, "\nSkipped %d TIME_WAIT entries.", skip);
	}
	return (0);
}

static boolean_t
nca_param_register(ncaparam_t *ncapa, int cnt)
{
	for (; cnt-- > 0; ncapa++) {
		if (ncapa->param_name && ncapa->param_name[0]) {
			if (!nd_load(&nca_g_nd, ncapa->param_name,
			    nca_param_get, nca_param_set,
			    (caddr_t)ncapa)) {
				nd_free(&nca_g_nd);
				return (B_FALSE);
			}
		}

	}
	if (!nd_load(&nca_g_nd, "nca_version", nca_version_get, nil(pfi_t),
			nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_httpd_version", nca_httpd_version_get,
			nil(pfi_t), nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_logd_version", nca_logd_version_get,
			nil(pfi_t), nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "httpd_door_address", nca_httpd_door_addr_get,
			nca_httpd_door_addr_set, nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "httpd_door_instance", nca_httpd_door_inst_get,
			nca_httpd_door_inst_set, nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "httpd_door_path", nca_httpd_door_get,
			nca_httpd_door_set, (caddr_t)nca_httpd_door_path)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_ppmax", nca_ppmax_get, nca_ppmax_set,
			(caddr_t)nca_ppmax)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_vpmax", nca_vpmax_get, nca_vpmax_set,
			(caddr_t)nca_vpmax)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_availrmem", nca_availrmem_get, nil(pfi_t),
			nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_maxkmem", nca_maxkmem_get, nil(pfi_t),
			nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_debug_counter", nca_debug_counter_get,
			nca_debug_counter_set, (caddr_t)nca_debug_counter)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_logging_on", nca_logging_on_get,
			nca_logging_on_set, (caddr_t)nca_logging_on)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "nca_log_file", nca_log_file_get,
			nca_log_file_set, nil(caddr_t))) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "conn_status", conn_status, NULL,
	    (caddr_t)true)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	if (!nd_load(&nca_g_nd, "conn_status_all", conn_status, NULL,
	    (caddr_t)false)) {
		nd_free(&nca_g_nd);
		return (B_FALSE);
	}
	return (B_TRUE);
}

static int
/*ARGSUSED*/
nca_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char *end;
	int32_t	new_value;
	ncaparam_t	*ncapa = (ncaparam_t *)cp;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value < ncapa->param_min ||
	    new_value > ncapa->param_max)
		return (EINVAL);
	ncapa->param_val = new_value;
	return (0);
}

void
nca_ndd_init(void)
{
	if (!nca_g_nd) {
		if (!nca_param_register(nca_param_arr, A_CNT(nca_param_arr))) {
			nd_free(&nca_g_nd);
		}
	}
}

void
nca_ndd_fini(void)
{
	nd_free(&nca_g_nd);
}
