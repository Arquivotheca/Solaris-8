/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nca.c	1.13	99/12/06 SMI"

const char nca_version[] = "@(#)nca.c	1.13	99/12/06 SMI";

/*
 * NCA (Network Cache and Accelerator) for HTTP/CONN/TCP/IP.
 *
 * This module is autopush(1M)ed onto the specified (see /etc/au.ap)
 * STREAMS device driver to provide caching of HTTP GETs by packet
 * switching on input TCP/IP packets for the specified HTTP port(s)
 * to our HTTP/CONN/TCP/IP protocol stack while others are passed on.
 *
 * When a HTTP request is received if it's a GET and it's in the cache
 * of HTTP byte-stream objects then the request is served from the
 * cache.
 *
 * Else, an doors up-call is made to the httpd.
 *
 * Note, only drivers which use the IP M_DATA Fast-Path to send and receive
 * mblks will have their mblks switched.
 *
 * Source files:
 *
 *	nca.conf - KMOD module configuration
 *
 *	ncaddi.c - DDI definitions
 *
 *	nca.c - STREAMS module infrastructure and packet switching code
 *
 *	ncaproto.c - CONN/TCP/IP
 *
 *	ncahttp.c - HTTP
 */

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

#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/isa_defs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/tcp.h>

#include <sys/dlpi.h>
#include <sys/systm.h>
#include <sys/param.h>

#include <sys/strick.h>

#include <netinet/igmp_var.h>
#include <inet/ip.h>

#include <sys/atomic.h>
#include <sys/condvar_impl.h>
#include <sys/callb.h>

#include <sys/strsun.h>

#include "nca.h"
#include "ncadoorhdr.h"


int nca_debug = 0;
int nca_debug_counter = 0;

#ifdef	BEANS

nca_beans_t *nca_beans;
nca_tstamps_t *nca_tstamps;

#endif

#ifdef	NCA_COUNTER_TRACE

nca_counter_t nca_counter_tv[NCA_COUNTER_TRACE_SZ];
nca_counter_t *nca_counter_tp = nca_counter_tv;

#endif	/* NCA_COUNTER_TRACE */

/*
 * NCA bean counters, see ndd nca_counters.
 */

pgcnt_t nca_ppmax = 0;		/* Max phys pages max for cache */
pgcnt_t nca_vpmax = 0;		/* Max virt pages max for cache */
pgcnt_t nca_ppmem = 0;		/* Phys pages used for cache */
pgcnt_t nca_vpmem = 0;		/* Virt pages used for cache */
ssize_t nca_kbmem = 0;		/* Kmem bytes used for cache */
ssize_t nca_mbmem = 0;		/* Mblk bytes used for cache */
ssize_t nca_cbmem = 0;		/* Conn_t bytes used for cache */
ssize_t nca_lbmem = 0;		/* Log bytes used for logging */
size_t  nca_maxkmem = 0;

ulong_t nca_hits = 0;		/* Hit, node_t with phys/virt/mblk mappings */
ulong_t nca_304hits = 0;	/* 304 hit, node_t not modified since last */
ulong_t nca_missfast = 0;	/* Miss, processed by miss(NOSLEEP) */
ulong_t nca_miss = 0;		/* Miss, processed by miss(SLEEP) */
ulong_t nca_filehits = 0;	/* File node_t hit with all mappings */
ulong_t nca_filemissfast1 = 0;	/* File node_t processed by vmap(NOSLEEP) */
ulong_t nca_filemissfast2 = 0;	/* File node_t processed by vmap(SLEEP) */
ulong_t nca_filemiss = 0;	/* File node_t miss */
ulong_t nca_missed1 = 0;	/* Miss, no node_t found */
ulong_t nca_missed2 = 0;	/* Miss, node_t busy, enque request */
ulong_t nca_missed3 = 0;	/* Miss, no phys mapping */
ulong_t nca_missed4 = 0;	/* Miss, no phys|virt|mblk mapping */
ulong_t nca_missed5 = 0;	/* Miss, node_t safed */
ulong_t nca_missed6 = 0;	/* Miss, file node_t no virt */
ulong_t nca_miss1 = 0;		/* Miss, processed, conn_t no ref */
ulong_t nca_miss2 = 0;		/* Miss, process failed */
ulong_t nca_missnot = 0;	/* Miss, processed, conn_t no ref */
ulong_t nca_missbad = 0;	/* Miss fill failed */

ulong_t nca_nocache1 = 0;	/* No cache, req, GET with query (i.e. "?") */
ulong_t nca_nocache2 = 0;	/* No cache, req, not GET method */
ulong_t nca_nocache3 = 0;	/* No cache, req, "Pragma: no-cache" */
ulong_t nca_nocache4 = 0;	/* No cache, req, "Range: ..." */
ulong_t nca_nocache5 = 0;	/* No cache, req, "Authorization: ..." */
ulong_t nca_nocache6 = 0;	/* No cache, req, expired cache entry */
ulong_t nca_nocache6nomp = 0;	/* No cache, req, expired cache entry */
ulong_t nca_nocache7 = 0;	/* No cache, res, invalid header */
ulong_t nca_nocache8 = 0;	/* No cache, res, no "Last-Modified: ..." */
ulong_t nca_nocache9 = 0;	/* No cache, res, "Expire: ..." now */
ulong_t nca_nocache10 = 0;	/* No cache, res, "Authenticate: ..." */
ulong_t nca_nocache11 = 0;	/* No cache, res, "Control: ..." */
ulong_t nca_nocache12 = 0;	/* No cache, res, not code 200 */
ulong_t nca_nocache13 = 0;	/* No cache, res, "Cookie: ..." */
ulong_t nca_nocache14 = 0;	/* No cache, req, content negotiation */
ulong_t nca_nodes = 0;		/* Number of allocated node_t's */
ulong_t nca_desballoc = 0;	/* Number of active desballoc()ed mblk()s */

ulong_t nca_plrucnt = 0;	/* Number of node_t's in the phys LRU */
ulong_t nca_vlrucnt = 0;	/* Number of node_t's in the virt LRU */
ulong_t nca_rpcall = 0;		/* Mark node_t for reclaim PHYS */
ulong_t nca_rvcall = 0;		/* Mark node_t for reclaim VIRT */
ulong_t nca_rpbusy = 0;		/* node_t reclaim busy PHYS */
ulong_t nca_rvbusy = 0;		/* node_t reclaim busy VIRT */
ulong_t nca_rpempty = 0;	/* node_t reclaim empty bucket PHYS */
ulong_t nca_rvempty = 0;	/* node_t reclaim empty bucket VIRT */
ulong_t nca_rpdone = 0;		/* node_t reclaim PHYS done */
ulong_t nca_rvdone = 0;		/* node_t reclaim VIRT done */
ulong_t nca_rmdone = 0;		/* node_t reclaim MBLK done */
ulong_t nca_rkdone = 0;		/* node_t reclaim KMEM done */
ulong_t nca_rndone = 0;		/* node_t reclaim node_t done */
ulong_t nca_rpfail = 0;		/* node_t reclaim PHYS failed */
ulong_t nca_rvfail = 0;		/* node_t reclaim VIRT failed */
ulong_t nca_rmfail = 0;		/* node_t reclaim MBLK failed */
ulong_t nca_rkfail = 0;		/* node_t reclaim KMEM failed */
ulong_t nca_rnh = 0;		/* node_t reclaim no hash */
ulong_t nca_ref[8];		/* node_t reference counters */

ulong_t nca_mapinfail = 0;	/* Virt mapin() failed */
ulong_t nca_mapinfail1 = 0;	/* Not enough Virt mem */
ulong_t nca_mapinfail2 = 0;	/* Not enough contiguous Virt mem */
ulong_t nca_mapinfail3 = 0;	/* Not enough contiguous Virt mem, return */

ulong_t nca_httpd_http;		/* Count of httpd upcalls returning http data */
ulong_t nca_httpd_badsz;	/* "Content-Length:" != file size */
ulong_t nca_httpd_nosz;		/* No data for iop */
ulong_t nca_httpd_filename;	/* Count of httpd upcalls returning filename */
ulong_t nca_httpd_filename1;	/* Count of httpd filename reads of 64K */
ulong_t nca_httpd_filename2;	/* Count of httpd filename reads of <= 512 */
ulong_t nca_httpd_trailer;	/* Count of httpd upcalls returning trailer */

ulong_t nca_logit = 0;
ulong_t nca_logit_nomp = 0;
ulong_t nca_logit_fail = 0;
ulong_t nca_logit_flush_NULL1 = 0;
ulong_t nca_logit_flush_NULL2 = 0;
ulong_t nca_logit_flush_NULL3 = 0;
ulong_t nca_logit_noupcall = 0; /* no of log door upcalls */

ulong_t nca_conn_count = 0;
ulong_t nca_conn_kmem = 0;
ulong_t nca_conn_kmem_fail = 0;
ulong_t nca_conn_allocb_fail = 0;
ulong_t nca_conn_tw = 0;
ulong_t nca_conn_tw1 = 0;
ulong_t nca_conn_tw2 = 0;
ulong_t nca_conn_reinit_cnt = 0;
ulong_t nca_conn_NULL1 = 0;

ulong_t ipsendup = 0;
ulong_t ipwrongcpu = 0;
ulong_t iponcpu = 0;

ulong_t nca_tcp_xmit_null = 0;
ulong_t nca_tcp_xmit_null1 = 0;

ulong_t tw_on = 0;
ulong_t tw_fire = 0;
ulong_t tw_fire1 = 0;
ulong_t tw_fire2 = 0;
ulong_t tw_fire3 = 0;
ulong_t tw_add = 0;
ulong_t tw_add1 = 0;
ulong_t tw_delete = 0;
ulong_t tw_reclaim = 0;
ulong_t tw_reap = 0;
ulong_t tw_reap1 = 0;
ulong_t tw_reap2 = 0;
ulong_t tw_reap3 = 0;
ulong_t tw_reap4 = 0;
ulong_t tw_reap5 = 0;
ulong_t tw_timer = 0;
ulong_t tw_timer1 = 0;
ulong_t tw_timer2 = 0;
ulong_t tw_timer3 = 0;
ulong_t tw_timer4 = 0;
ulong_t tw_timer5 = 0;

ulong_t ti_on = 0;
ulong_t ti_fire = 0;
ulong_t ti_fire1 = 0;
ulong_t ti_fire2 = 0;
ulong_t ti_fire3 = 0;
ulong_t ti_fire4 = 0;
ulong_t ti_add = 0;
ulong_t ti_add1 = 0;
ulong_t ti_add2 = 0;
ulong_t ti_add3 = 0;
ulong_t ti_add4 = 0;
ulong_t ti_add5 = 0;
ulong_t ti_add_reuse = 0;
ulong_t ti_add_failed = 0;
ulong_t ti_delete = 0;
ulong_t ti_delete1 = 0;
ulong_t ti_delete2 = 0;
ulong_t ti_reap = 0;
ulong_t ti_reap1 = 0;
ulong_t ti_reap2 = 0;
ulong_t ti_reap3 = 0;
ulong_t ti_reap4 = 0;
ulong_t ti_reap5 = 0;
ulong_t ti_timer = 0;
ulong_t ti_timer1 = 0;
ulong_t ti_timer2 = 0;
ulong_t ti_timer3 = 0;
ulong_t ti_timer4 = 0;
ulong_t ti_timer5 = 0;
ulong_t ti_timer6 = 0;

extern caddr_t nca_g_nd;

extern int servicing_interrupt();

extern boolean_t nca_ip_input(if_t *, mblk_t *);
extern void nca_conn_init(void);
extern void nca_conn_fini(void);
extern void nca_http_init(void);
extern void nca_http_fini(void);
extern void nca_ndd_init(void);
extern void nca_ndd_fini(void);
extern void nca_vmem_init();
extern void nca_vmem_fini();
extern ti_t *nca_ti_init(ti_t *, squeue_t *);
extern tw_t *nca_tw_init(tw_t *, squeue_t *);
extern void nca_ti_fini(ti_t *);
extern void nca_tw_fini(tw_t *);
extern void nca_tcp_input(void *, mblk_t *, void *);

#define	BIND_FLAT	1
#define	BIND_ID		2

static processorid_t nca_worker_bind(int, processorid_t);
static int nca_open(queue_t *, dev_t *, int, int, cred_t *);
static int nca_close(queue_t *);
static void nca_dlinfo_req(queue_t *);
static void nca_rput(queue_t *, mblk_t *);
static void nca_rsrv(queue_t *);
static void nca_wsrv(queue_t *);
static boolean_t psmatch(unsigned char *, size_t);
static uint_t squeue_intr(caddr_t);
static void squeue_fire(void *);

/* Called from ncaproto.c */
void nca_wput(queue_t *, mblk_t *);

/*
 * nca_fanout_iq_if: If set, fanout received packets to other cpu's
 *	squeue_t's (based on a hash of the connection tuple).
 *	Useful on machines with more cpus than interfaces.
 *
 * nca_deferred_oq_if: If set, defer sending packets out an interface
 *	to a worker thread running at minclsyspri, or until additional
 *	packets on this interface are received.
 */
boolean_t nca_fanout_iq_if = false;
boolean_t nca_deferred_oq_if = false;

nca_cpu_t *nca_gv;		/* global per CPU state */
sqfan_t	nca_if_sqf;		/* global if_t input sqfan_t */

kmutex_t nca_dcb_lock;		/* global dcb_list lock */
kcondvar_t nca_dcb_wait;	/* global dcb_list writer wait */
kmutex_t nca_dcb_readers;	/* global dcb_list reader wait */

void
nca_cpu_g(if_t *ifp)
{
	processorid_t	seqid = CPU->cpu_seqid;

	mutex_enter(&nca_gv[seqid].lock);
	if (nca_gv[seqid].if_inq == NULL) {
		squeue_t	*inq;
		squeue_t	*ouq;
		ti_t		*tip;
		tw_t		*twp;
		ddi_softintr_t	soft_id;
		uint32_t	flag;

		if (nca_fanout_iq_if) {
			flag = SQT_DEFERRED | SQT_BIND_TO;
			inq = sqfan_ixinit(&nca_if_sqf, CPU->cpu_seqid,
			    NULL, flag, CPU->cpu_id, NULL, NULL,
			    nca_tcp_input, 10, minclsyspri);
		} else {
			flag = SQT_BIND_TO;
			inq = squeue_init(NULL, flag, CPU->cpu_id, NULL,
				NULL, nca_tcp_input, 10, minclsyspri);
		}
		tip = nca_ti_init(NULL, inq);
		twp = nca_tw_init(NULL, inq);
		if (nca_deferred_oq_if) {
			flag = SQT_BIND_TO;
			ouq = squeue_init(NULL, flag, CPU->cpu_id, NULL,
			    NULL, nca_wput, 10, minclsyspri);
		} else {
			ouq = NULL;
		}
		if (! SQ_STATE_IS(inq, SQS_SOFTINTR) ||
		    ddi_add_softintr(NULL, DDI_SOFTINT_HIGH, &soft_id, NULL,
		    NULL, squeue_intr, (caddr_t)inq) != DDI_SUCCESS) {
			soft_id = NULL;
		}
		nca_gv[seqid].if_ouq = ouq;
		nca_gv[seqid].soft_id = soft_id;
		inq->sq_softid = soft_id;
		nca_gv[seqid].tcp_tw = twp;
		nca_gv[seqid].tcp_ti = tip;
		nca_gv[seqid].if_inq = inq;
	}
	mutex_exit(&nca_gv[seqid].lock);
	if (ifp != NULL) {
		ifp->inq = nca_gv[seqid].if_inq;
	}
	if (nca_deferred_oq_if && ifp != NULL) {
		ifp->ouq = nca_gv[seqid].if_ouq;
	}
}

static void
nca_if_init()
{
	processorid_t	seqid;

	nca_gv = kmem_zalloc(sizeof (*nca_gv) * CPUS, KM_SLEEP);
	for (seqid = 0; seqid < CPUS; seqid++) {
		mutex_init(&nca_gv[seqid].lock, NULL, MUTEX_DEFAULT, NULL);
	}
	mutex_init(&nca_dcb_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&nca_dcb_wait, NULL, CV_DEFAULT, NULL);
	mutex_init(&nca_dcb_readers, NULL, MUTEX_DEFAULT, NULL);
	if (nca_fanout_iq_if) {
		cpu_t	*cpu = cpu_list;

		sqfan_init(&nca_if_sqf, CPUS, SQF_DIST_IPv4, 0);
		do {
			affinity_set(cpu->cpu_id);
			nca_cpu_g(NULL);
			affinity_clear();
			cpu = cpu->cpu_next;
		} while (cpu != cpu_list);
	}
}

static void
nca_if_fini()
{
	squeue_t	*sqp;
	tw_t		*twp;
	ti_t		*tip;
	processorid_t	seqid;

	if (nca_fanout_iq_if) {
		sqfan_fini(&nca_if_sqf);
	}
	mutex_destroy(&nca_dcb_readers);
	cv_destroy(&nca_dcb_wait);
	mutex_destroy(&nca_dcb_lock);
	for (seqid = 0; seqid < CPUS; seqid++) {
		if ((twp = nca_gv[seqid].tcp_tw) != NULL) {
			nca_tw_fini(twp);
		}
		if ((tip = nca_gv[seqid].tcp_ti) != NULL) {
			nca_ti_fini(tip);
		}
		if ((sqp = nca_gv[seqid].if_inq) != NULL) {
			squeue_fini(sqp);
		}
		mutex_destroy(&nca_gv[seqid].lock);
	}
	kmem_free(nca_gv, sizeof (*nca_gv) * CPUS);
}

static struct module_info info = {
	0, "nca", 1, INFPSZ, 65536, 1024
};

static struct qinit rinit = {
	(pfi_t)nca_rput, (pfi_t)nca_rsrv, nca_open, nca_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)nca_wput, (pfi_t)nca_wsrv, nca_open, nca_close, nil(pfi_t), &info
};

struct streamtab ncainfo = {
	&rinit, &winit
};

static unsigned long nca_modopens = 0;
static boolean_t nca_modfirst = true;
static kmutex_t nca_modopen;

void
nca_ddi_init(void)
{
	mutex_init(&nca_modopen, NULL, MUTEX_DEFAULT, NULL);
	nca_if_init();
	/*
	 * Delay the remainder of the *_init() calls to open_init() which
	 * will be called on the first mod open. This is done to minimize
	 * the memory footprint of a loaded NCA kmod that's not being used
	 * (i.e. plumbed).
	 */
}

void
nca_open_init(void)
{
	nca_modfirst = false;
#ifdef	BEANS
	/* Allocate CPUS worth of nca_beans_t's */
	nca_beans = kmem_zalloc(sizeof (*nca_beans) * CPUS, KM_SLEEP);
	/* Allocate and initialize CPUS worth of nca_tstamps_t's */
	nca_tstamps = kmem_zalloc(sizeof (*nca_tstamps) * CPUS, KM_SLEEP);
	TSP_INIT(CPUS);
#endif
	nca_ndd_init();
	nca_vmem_init();
	nca_http_init();
	nca_conn_init();
#ifdef	PAUSE_IF
	mutex_init(&pause_if_lock, NULL, MUTEX_DEFAULT, NULL);
#endif	/* PAUSE_IF */
}

void
nca_ddi_fini(void)
{
	if (! nca_modfirst) {
		/* Only _fini if open_init() has been called open() */
#ifdef	PAUSE_IF
		mutex_destroy(&pause_if_lock);
#endif	/* PAUSE_IF */
		nca_conn_fini();
		nca_http_fini();
		nca_vmem_fini();
		nca_ndd_fini();
	}
	nca_if_fini();
	mutex_destroy(&nca_modopen);
}

static int
nca_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	if_t	*ifp;

	if (nca_debug)
		prom_printf("nca_open(0x%p, 0x%p, 0x%x, 0x%x, 0x%p):\n",
		    (void *)q, (void *)devp, flag, sflag, (void *)credp);

	ifp = (if_t *)kmem_zalloc(sizeof (*ifp), KM_SLEEP);

	ifp->dev = false;

	ifp->rqp = RD(q);
	ifp->wqp = WR(q);

	RD(q)->q_ptr = ifp;
	WR(q)->q_ptr = ifp;

	if (! (sflag & MODOPEN)) {
		/* Device instance */
		ifp->dev = true;
		if (drv_priv(credp) == 0)
			ifp->dev_priv = true;
	} else {
		/*
		 * Module instance, to minimize the memory footprint of
		 * a loaded NCA kmod that's not being used (i.e. plumed)
		 * we delay the ddi_init() call of *_init() functions to
		 * open_init() which is called on the first mod open.
		 */
		if (nca_modopens == 0 && nca_modfirst) {
			/* First open */
			mutex_enter(&nca_modopen);
			if (nca_modfirst) {
				/* We won the race to be first */
				nca_open_init();
			}
			mutex_exit(&nca_modopen);
		}
		atomic_add_long(&nca_modopens, 1);
	}
	qprocson(q);

	if (sflag & MODOPEN) {
		nca_dlinfo_req(WR(q));
	}
	return (0);
}

static int
nca_close(queue_t *q)
{
	if_t	*ifp = (if_t *)q->q_ptr;
	int	error = 0;

	if (nca_debug)
		prom_printf("nca_close(0x%p):\n", (void *)q);

	qprocsoff(q);

	if (! ifp->dev) {
		if (ifp->mac_mp)
			freemsg(ifp->mac_mp);

		/*
		 * XXX need to do tw, ti, inq cleanup,
		 * for now don't down an active if_t.
		 */
	} else {
		/* Module instance */
		atomic_add_long(&nca_modopens, -1);
	}

	kmem_free(ifp, sizeof (*ifp));

	RD(q)->q_ptr = NULL;
	WR(q)->q_ptr = NULL;
	return (error);
}

/* Send down a DL_INFO_REQ request to the device. */
static void
nca_dlinfo_req(queue_t *q)
{
	mblk_t	*info_mp;
	dl_info_req_t *dl_req;

	if ((info_mp = allocb(MAX(sizeof (dl_info_req_t),
	    sizeof (dl_info_ack_t)), BPRI_HI)) != NULL) {
		info_mp->b_datap->db_type = M_PCPROTO;
		dl_req = (dl_info_req_t *)info_mp->b_rptr;
		info_mp->b_wptr = (uchar_t *)&dl_req[1];
		dl_req->dl_primitive = DL_INFO_REQ;
		putnext(q, info_mp);
	}
}

static void
nca_rput(queue_t *q, mblk_t *mp)
{
	if_t	*ifp = (if_t *)q->q_ptr;

	TSP_ALLOC(mp);

	switch (mp->b_datap->db_type) {
	case M_DATA: {
		ssize_t prepend;

		/*
		 * If the fast path code has not been initialized or
		 * this packet does not belong to us, just skip it.
		 */
		if (ifp->mac_mp == NULL ||
		    !psmatch(mp->b_rptr, (mp->b_wptr - mp->b_rptr))) {
			break;
		}

		prepend = mp->b_rptr - mp->b_datap->db_base;
		/* DLPI IP M_DATA Fast-Path mac prepend wrong size? */
		if (ifp->mac_length > 0 && prepend < ifp->mac_length) {
			if (nca_debug)
				prom_printf("nca_rput: mac prepend too short"
				    " (%d<%u)\n", (int)prepend,
				    (uint_t)ifp->mac_length);
			break;
		}

		TDELTATS(*TSP(mp), rput[0]);

		if (nca_ip_input(ifp, mp))
			return;
		/* IP didn't consume the packet ? */
		break;
	}
	case M_IOCACK:
		/*
		 * Only interested in DL_IOC_HDR_INFO info and only when
		 * we still do not know about the fast path Ethernet
		 * header and only when we already know about mac_addr_len.
		 * Note that if the underlying device is non Ethernet,
		 * mac_addr_length will not be initialized so NCA should not
		 * intercept any packets.
		 */
		if (((struct iocblk *)mp->b_rptr)->ioc_cmd != DL_IOC_HDR_INFO ||
		    ifp->mac_mp != NULL || ifp->mac_addr_len == 0) {
			break;
		}
		if ((mp->b_cont) && (mp->b_cont->b_cont)) {
			if (ifp->mac_mp)
				freemsg(ifp->mac_mp);
			ifp->mac_mp = dupb(mp->b_cont->b_cont);
			ifp->mac_length = (ifp->mac_mp->b_wptr -
			    ifp->mac_mp->b_rptr);
		} else {
			if (nca_debug)
				prom_printf("nca_rput: no b_cont->b_cont\n");

		}
		break;
	case M_PCPROTO: {
		dl_info_ack_t	*dlp;
		int32_t		i32;
		static boolean_t	warn = B_FALSE;

		/*
		 * If we have been initialized, the reply is not for us, pass
		 * it up.  And we are only interested in DL_INFO_ACK.
		 */
		if ((ifp->mac_addr_len != 0) ||
		    (((dl_unitdata_ind_t *)mp->b_rptr)->dl_primitive !=
		    DL_INFO_ACK)) {
			break;
		}

		dlp = (dl_info_ack_t *)mp->b_rptr;
		if (dlp->dl_mac_type != DL_ETHER && !warn) {
			cmn_err(CE_WARN, "NCA will not work with non-"
			    "Ethernet network device");
			warn = B_TRUE;
			break;
		}

		if (nca_debug) {
			prom_printf("\nprim %u maxsdu %u minsdu %u addrlen %u "
			    "mactype %u state %u saplen %d\n",
			    dlp->dl_primitive, dlp->dl_max_sdu, dlp->dl_min_sdu,
			    dlp->dl_addr_length, dlp->dl_mac_type,
			    dlp->dl_current_state, dlp->dl_sap_length);

			prom_printf("mode %u qoslen %u qosoff %u qosrnlen %u "
			    "qosrnoff %u style %u addroff %u ver %u\n",
			    dlp->dl_service_mode, dlp->dl_qos_length,
			    dlp->dl_qos_offset, dlp->dl_qos_range_length,
			    dlp->dl_qos_range_offset, dlp->dl_provider_style,
			    dlp->dl_addr_offset, dlp->dl_version);

			prom_printf("bcastlen %u bcastoff %u\n\n",
			    dlp->dl_brdcst_addr_length,
			    dlp->dl_brdcst_addr_offset);
		}

		ifp->mac_mtu = dlp->dl_max_sdu;
		if (dlp->dl_version == DL_VERSION_2) {
			i32 = dlp->dl_sap_length;
			ifp->mac_addr_len = dlp->dl_addr_length +
				(i32 > 0 ? -i32 : i32);
		} else {
			ifp->mac_addr_len = dlp->dl_brdcst_addr_length;
		}
		/* Should not pass up the reply because it will confuse IP. */
		freemsg(mp);
		return;
	}
	default:
		break;
	}
	putnext(q, mp);
}

static void
/*ARGSUSED*/
nca_rsrv(queue_t *q)
{
	cmn_err(CE_PANIC, "nca_rsrv: no read-side STREAMS service");
}

/*
 * Note: this function is called by ncaproto.c directly !!!
 */
void
nca_wput(queue_t *q, mblk_t *mp)
{
	if_t	*ifp = (if_t *)q->q_ptr;
	struct iocblk *iocp;

	if (! ifp->dev) {
		putnext(q, mp);
		return;
	}
	switch (DB_TYPE(mp)) {
	case M_IOCTL:
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case ND_SET:
			if (! ifp->dev_priv) {
				miocnak(q, mp, 0, EPERM);
				return;
			}
			/* FALLTHRU */
		case ND_GET:
			if (! nd_getset(q, nca_g_nd, mp)) {
				miocnak(q, mp, 0, ENOENT);
				return;
			}
			qreply(q, mp);
			return;
		default:
			miocnak(q, mp, 0, EINVAL);
			break;
		}
		break;
	default:
		freemsg(mp);
		break;
	}
}

static void
/*ARGSUSED*/
nca_wsrv(queue_t *q)
{
	cmn_err(CE_PANIC, "nca_wsrv: no write-side STREAMS service");
}

/*
 * The default dcb_t (and it's encapsulated hcb_t) is intialized to use
 * the default httpd door (nca_httpd_door_path, nca_httpd_door_hand) for
 * IPaddr == INADDR_ANY, TCPport == 80, Host: == *, DocRoot = /.
 */
dcb_t dcb_list =
	{NULL, NULL, 0, NULL, {NULL, INADDR_ANY, 80, NULL, 0, NULL, 0}};

static boolean_t
psmatch(unsigned char *p, size_t sz)
{
	iph_t	*iph = (iph_t *)p;
	ipaddr_t addr = ABE32_TO_U32(iph->iph_src);
	uint16_t port;
	size_t	iph_sz;
	dcb_t	*dp;
	hcb_t	*hp;

	if (sz < IP_SIMPLE_HDR_LENGTH ||
	    iph->iph_version_and_hdr_length >> 4 != IP_VERSION ||
	    iph->iph_protocol != IPPROTO_TCP ||
	    (iph_sz = IPH_HDR_LENGTH(p), sz < iph_sz + TCP_MIN_HEADER_LENGTH)) {
		/* Not a min sized IPv4/TCP buffer, so nothing todo ? */
		return (false);
	}
	port = BE16_TO_U16(((tcph_t *)(p + iph_sz))->th_fport);
	DCB_RD_ENTER();
	dp = &dcb_list;
	do {
		hp = &dp->list;
		do {
			if (hp->addr != INADDR_BROADCAST &&
			    (hp->addr == INADDR_ANY || hp->addr == addr) &&
			    hp->port == port) {
				/* Match */
				DCB_RD_EXIT();
				return (true);
			}
			hp = hp->next;
		} while (hp != NULL);
		dp = dp->next;
	} while (dp != NULL);
	DCB_RD_EXIT();
	return (false);
}

extern char nca_httpd_door_path[];
extern door_handle_t nca_httpd_door_hand;

void
nca_dcb_report(mblk_t *mp)
{
	dcb_t	*dp;
	char	*door;
	door_handle_t hand;

	DCB_RD_ENTER();
	for (dp = &dcb_list; dp != NULL; dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((hand = dp->hand) == NOHANDLE) {
			hand = NULL;
		} else if (hand == NULL) {
			hand = nca_httpd_door_hand;
		}
		if ((door = dp->door) == NULL) {
			door = nca_httpd_door_path;
		}
		(void) mi_mpprintf(mp, "%x: %s", (int)hand, door);
	}
	DCB_RD_EXIT();
}

int
nca_dcb_add(char *door)
{
	dcb_t	*dp, *pdp;
	char	*edoor;
	door_handle_t ehand;
	int	len;
	int	error = 0;

	DCB_WR_ENTER();
	/* Search for duplicate */
	for (pdp = NULL, dp = &dcb_list; dp != NULL; pdp = dp, dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((edoor = dp->door) == NULL) {
			edoor = nca_httpd_door_path;
		}
		if (strcmp(door, edoor) == 0) {
			/* Found a dup */
			error = EEXIST;
			goto done;
		}
	}
	/* No dup */
	if (dcb_list.doorsz == -1) {
		/* List head free, use it */
		dp = &dcb_list;
	} else {
		/* List head not free, so alloc and insert at tail */
		dp = kmem_zalloc(sizeof (*dp), KM_SLEEP);
		dp->next = pdp->next;
		pdp->next = dp;
	}
	/* Initialize for NULL hcb_t */
	if (strcmp(door, nca_httpd_door_path) != 0) {
		/* Not default door, so dup the string */
		len = strlen(door) + 1;
		edoor = kmem_alloc(len, KM_SLEEP);
		bcopy(door, edoor, len);
		ehand = NOHANDLE;
	} else {
		len = 0;
		edoor = NULL;
		ehand = NULL;
	}
	dp->door = edoor;
	dp->doorsz = len;
	dp->hand = ehand;
	dp->list.addr = INADDR_BROADCAST;
done:
	DCB_WR_EXIT();
	return (error);
}

int
nca_dcb_del(char *door)
{
	dcb_t	*dp, *pdp;
	hcb_t	*hp, *nhp;
	char	*edoor;
	int	error = 0;

	DCB_WR_ENTER();
	/* Search for match */
	for (pdp = NULL, dp = &dcb_list; dp != NULL; pdp = dp, dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((edoor = dp->door) == NULL) {
			edoor = nca_httpd_door_path;
		}
		if (strcmp(door, edoor) == 0) {
			/* Found a match */
			if (dp->door != NULL) {
				kmem_free(dp->door, dp->doorsz);
				dp->door = NULL;
			}
			if (dp->hand != NOHANDLE && dp->hand != NULL) {
				/* Release the door handle */
				/* XXX add the door rele call */
				dp->hand = NULL;
			}
			/* Free any hcb_t list entries */
			for (hp = &dp->list; hp != NULL; hp = nhp) {
				if (hp->root != NULL) {
					kmem_free(hp->root, hp->rootsz);
					hp->root = NULL;
				}
				if (hp->host != NULL) {
					kmem_free(hp->host, hp->hostsz);
					hp->host = NULL;
				}
				nhp = hp->next;
				if (hp == &dp->list) {
					/* It's the dcb_t's hcb_t, invalidate */
					hp->addr = INADDR_BROADCAST;
					hp->port = 0;
				} else {
					kmem_free(hp, sizeof (*hp));
				}
			}
			dp->list.next = NULL;
			if (pdp == NULL) {
				/* List head, invalidate it */
				dp->doorsz = -1;
			} else {
				pdp->next = dp->next;
				kmem_free(dp, sizeof (*dp));
			}
			goto done;
		}
	}
	error = ENXIO;
done:
	DCB_WR_EXIT();
	return (error);
}

void
nca_hcb_report(mblk_t *mp)
{
	dcb_t	*dp;
	hcb_t	*hp;
	char	*door;
	door_handle_t hand;
	char	addr[32];
	char	*host;
	char	*root;

	(void) mi_mpprintf(mp,
	    " Door     Path to Door File    IPaddr:TCPport  Host:  Doc Root");

	DCB_RD_ENTER();
	for (dp = &dcb_list; dp != NULL; dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((hand = dp->hand) == NOHANDLE) {
			hand = NULL;
		} else if (hand == NULL) {
			hand = nca_httpd_door_hand;
		}
		if ((door = dp->door) == NULL) {
			door = nca_httpd_door_path;
		}
		for (hp = &dp->list; hp != NULL; hp = hp->next) {
			if (hp->addr == INADDR_ANY) {
				(void) strcpy(addr, "*");
			} else {
				int a1 = (hp->addr >> 24) & 0xFF;
				int a2 = (hp->addr >> 16) & 0xFF;
				int a3 = (hp->addr >> 8) & 0xFF;
				int a4 = hp->addr & 0xFF;

				(void) mi_sprintf(addr, "%d.%d.%d.%d",
					a1, a2, a3, a4);
			}
			(void) mi_sprintf(addr, "%s:%d", addr, hp->port);
			if ((host = hp->host) == NULL) {
				host = "*";
			}
			if ((root = hp->root) == NULL) {
				root = "/";
			}
			(void) mi_mpprintf(mp, "%x: \"%s\" \"%s\" \"%s\" \""
			    "%s\"", (int)hand, door, addr, host, root);
		}
	}
	DCB_RD_EXIT();
}

int
nca_hcb_add(char *door, ipaddr_t addr, uint16_t port, char *host, char *root)
{
	dcb_t	*dp, *idp = NULL;
	hcb_t	*hp, *ihp = NULL, *pihp = NULL;
	char	*edoor;
	char	*ehost;
	char	*eroot;
	int	len;
	int	error = 0;

	DCB_WR_ENTER();
	/*
	 * Check for a duplicate and find insertion point,
	 * we insert in a most specific match order.
	 */
	for (dp = &dcb_list; dp != NULL; dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((edoor = dp->door) == NULL) {
			edoor = nca_httpd_door_path;
		}
		if (strcmp(door, edoor) == 0) {
			/* Found dcb_t */
			idp = dp;
		}
		hp = &dp->list;
		do {
			if ((ehost = hp->host) == NULL) {
				ehost = "*";
			}
			if ((eroot = hp->root) == NULL) {
				eroot = "/";
			}
			if (addr == hp->addr && port == hp->port &&
			    strcmp(host, ehost) == 0 &&
			    strcmp(root, eroot) == 0) {
				/* Found a dup */
				error = EEXIST;
				goto done;
			}
			if (idp != NULL && ihp == NULL) {
				int a = 0, b = 0;

				if (addr != INADDR_ANY)
					a++;
				if (strcmp(host, "*") != NULL)
					a++;
				if (hp->addr != INADDR_BROADCAST) {
					if (hp->addr != INADDR_ANY)
						b++;
					if (hp->host != NULL)
						b++;
					if (a > b) {
						ihp = hp;
					} else {
						pihp = hp;
					}
				} else
					pihp = hp;
			}
			hp = hp->next;
		} while (hp != NULL);
	}
	if (idp == NULL) {
		/* No door found */
		error = ENXIO;
	}
	if (pihp == NULL) {
		/* Insert before the dcb_t's hcb_t, allocate a new hcb_t */
		hp = ihp;
		ihp = kmem_zalloc(sizeof (*hp), KM_SLEEP);
		/* Copy the dcb_t's hcb_t to the new hcb_t */
		*ihp = *hp;
	} else if (pihp->addr == INADDR_BROADCAST) {
		/* Use the dcb_t's free hcb_t */
		hp = pihp;
	} else {
		/* Allocate a new hcb_t */
		hp = kmem_zalloc(sizeof (*hp), KM_SLEEP);
		pihp->next = hp;
	}
	hp->next = ihp;
	hp->addr = addr;
	hp->port = port;
	if (strcmp(host, "*") == 0) {
		len = 0;
		ehost = NULL;
	} else {
		len = strlen(host) + 1;
		ehost = kmem_alloc(len, KM_SLEEP);
		bcopy(host, ehost, len);
	}
	hp->host = ehost;
	hp->hostsz = len;
	if (strcmp(root, "/") == 0) {
		len = 0;
		eroot = NULL;
	} else {
		len = strlen(root) + 1;
		eroot = kmem_alloc(len, KM_SLEEP);
		bcopy(root, eroot, len);
	}
	hp->root = eroot;
	hp->rootsz = len;
done:
	DCB_WR_EXIT();
	return (error);
}

int
nca_hcb_del(char *door, ipaddr_t addr, uint16_t port, char *host, char *root)
{
	dcb_t	*dp;
	hcb_t	*php, *hp, *mphp, *mhp = NULL;
	char	*edoor;
	char	*ehost;
	char	*eroot;
	int	error = 0;

	DCB_WR_ENTER();
	/* Check for multiple match */
	for (dp = &dcb_list; dp != NULL; dp = dp->next) {
		if (dp->doorsz == -1)
			continue;
		if ((edoor = dp->door) == NULL) {
			edoor = nca_httpd_door_path;
		}
		if (strcmp(door, edoor) == 0) {
			/* Found the door, look for a host entry */
			php = NULL;
			hp = &dp->list;
			do {
				if ((ehost = hp->host) == NULL) {
					ehost = "*";
				}
				if ((eroot = hp->root) == NULL) {
					eroot = "/";
				}
				if (addr == hp->addr && port == hp->port &&
				    strcmp(host, ehost) == 0 &&
				    strcmp(root, eroot) == 0) {
					/* Found a match */
					if (mhp != NULL) {
						/* Not exact enough match */
						error = EFAULT;
						goto done;
					}
					mphp = php;
					mhp = hp;
				}
				php = hp;
				hp = hp->next;
			} while (hp != NULL);
			if (mhp == NULL) {
				error = ENODEV;
				goto done;
			}
			/* Found only one match, delete it */
			if (mhp->root != NULL) {
				kmem_free(mhp->root, mhp->rootsz);
				mhp->root = NULL;
			}
			if (mhp->host != NULL) {
				kmem_free(mhp->host, mhp->hostsz);
				mhp->host = NULL;
			}
			if (mphp == NULL) {
				/* It's the dcb_t's hcb_t, so just invalidate */
				mhp->addr = INADDR_BROADCAST;
				mhp->port = 0;
				goto done;
			}
			/* Unlink it and free it */
			mphp->next = mhp->next;
			kmem_free(mhp, sizeof (*mhp));
			goto done;
		}
	}
	/* No door found */
	error = ENXIO;
done:
	DCB_WR_EXIT();
	return (error);
}


squeue_t *
squeue_init(
	squeue_t *sqp,
	uint32_t type,
	processorid_t bind,
	void (*init)(),
	void *init_arg,
	void (*proc)(),
	clock_t wait,
	pri_t pri
)
{
	kthread_id_t thread;

	if (sqp == NULL) {
		/* Allocate and note that we did the alloc() */
		sqp = kmem_zalloc(sizeof (*sqp), KM_SLEEP);
		type |= SQT_KMEM;
	} else {
		/* Caller supplied squeue_t, just zero it */
		bzero(sqp, sizeof (*sqp));
	}

	sqp->sq_type = type;
	sqp->sq_bind = bind;
	sqp->sq_init = init;
	sqp->sq_init_arg = init_arg;
	sqp->sq_proc = proc;
	sqp->sq_wait = MSEC_TO_TICK(wait);

	mutex_init(&sqp->sq_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sqp->sq_async, NULL, CV_DEFAULT, NULL);
	if (proc) {
		thread = thread_create(NULL, PAGESIZE * 4, squeue_worker,
				(caddr_t)sqp, 0, &p0, TS_RUN, pri);

		if (thread == (kthread_id_t)NULL)
			cmn_err(CE_PANIC,
				"squeue_init: thread_create(%p) failed",
				(void *)sqp);
	}
	return (sqp);
}

void
squeue_fini(squeue_t *sqp)
{
	if (sqp->sq_tid)
		(void) untimeout(sqp->sq_tid);
	squeue_exit(sqp);
	cv_destroy(&sqp->sq_async);
	mutex_destroy(&sqp->sq_lock);
	if (sqp->sq_type & SQT_KMEM) {
		/* We allocated this squeue_t, so free it */
		kmem_free(sqp, sizeof (*sqp));
	}
}

void
squeue_bind(squeue_t *sqp, uint32_t type, processorid_t bind)
{
	sqp->sq_type |= type;
	sqp->sq_bind = bind;
}

static void
squeue_drain(squeue_t *sqp, uint32_t proc_type, void *proc_arg)
{
	kmutex_t *lock = &sqp->sq_lock;
	uint32_t state;
	mblk_t	*mp;
	void	*arg;
	timeout_id_t tid;
	int	interrupt = servicing_interrupt();
	clock_t	expire;

	ASSERT(MUTEX_HELD(lock));
	ASSERT(! (sqp->sq_state & SQS_PROC));

	if (proc_type == SQS_PROXY && proc_arg) {
		expire = (clock_t)proc_arg + lbolt;
	} else {
		expire = 0;
	}
	sqp->sq_isintr = interrupt;
	if ((tid = sqp->sq_tid) != 0)
		sqp->sq_tid = 0;

	sqp->sq_state |= SQS_PROC | proc_type;
	while ((mp = sqp->sq_first) != NULL &&
	    ! (sqp->sq_state & (SQS_PAUSE | SQS_INTRWAIT | SQS_NOPROC)) &&
	    (! (sqp->sq_state & SQS_NOINTR) || ! interrupt) &&
	    (! expire || lbolt < expire)) {
		if ((sqp->sq_first = mp->b_next) == NULL)
			sqp->sq_last = NULL;
		else
			mp->b_next = NULL;
		sqp->sq_state--;
		arg = (void *)mp->b_prev;
		mp->b_prev = NULL;
		mutex_exit(lock);
		if (tid != 0) {
			(void) untimeout(tid);
			tid = 0;
		}

		(*(sqp->sq_proc))(arg, mp, (void *)sqp);

		mutex_enter(lock);
	}
	state = sqp->sq_state & ~(SQS_PROC | proc_type);
	if (state & SQS_INTRWAIT) {
		state &= ~SQS_INTRWAIT;
	}
	sqp->sq_state = state;
	if (mp != NULL) {
		/*
		 * We bailed early for some reason,
		 * so wakeup a worker if need be.
		 */
		if (CV_HAS_WAITERS(&sqp->sq_async)) {
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
		}
	}
}

static uint_t
squeue_intr(caddr_t arg)
{
	squeue_t *sqp = (squeue_t *)arg;
	kmutex_t *lock = &sqp->sq_lock;

	mutex_enter(lock);
	squeue_drain(sqp, SQS_SOFTINTR, NULL);
	sqp->sq_awaken = 0;
	mutex_exit(lock);
	return (DDI_INTR_CLAIMED);
}

/*
 * squeue_enter() - enter squeue *sqp with mblk *mp with argument of *arg.
 *
 */

void
squeue_enter(squeue_t *sqp, mblk_t *mp, void *arg)
{
	uint32_t state;
	boolean_t process;
	int	interrupt = servicing_interrupt();

	mutex_enter(&sqp->sq_lock);

	if (sqp->sq_iwait && lbolt >= sqp->sq_iwait) {
		/*
		 * squeue_nointr() has expired,
		 * enable interrupt mode.
		 */
		sqp->sq_state &= ~SQS_NOINTR;
		sqp->sq_iwait = 0;
	}
	if (sqp->sq_pwait && lbolt >= sqp->sq_pwait) {
		/*
		 * squeue_pause() has expired,
		 * enable processing.
		 */
		sqp->sq_state &= ~SQS_PAUSE;
		sqp->sq_pwait = 0;
		if (CV_HAS_WAITERS(&sqp->sq_async)) {
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
		}
	}

	state = sqp->sq_state;

	if (! (state & (SQS_PROC|SQS_NOINTR)) &&
	    ! (sqp->sq_type & SQT_DEFERRED) &&
	    (state & (SQS_WORKER|SQS_PROXY)) && interrupt) {
		/*
		 * Some other thread is processing the squeue_t and
		 * interrupts are allowed and the other thread is
		 * a worker or proxy thread and we are an interrupt
		 * thread so we could have process the squeue_t.
		 */
		sqp->sq_state |= SQS_INTRWAIT;
	}

	/*
	 * Need to process the queue now ?
	 *
	 * We in-line processing only if:
	 *
	 *	o the queue isn't being processed
	 *
	 *	o the queue isn't paused
	 *
	 *	o the queue isn't deferred processing only
	 *
	 *	o the queue isn't marked for no interrupt thread or
	 *	  current thread isn't an interrupt thread.
	 */
	if (! (state & (SQS_PROC|SQS_PAUSE|SQS_NOPROC)) &&
	    ! (sqp->sq_type & SQT_DEFERRED) &&
	    (! (state & SQS_NOINTR) || ! interrupt))
		process = true;
	else
		process = false;

	if (process && ! sqp->sq_first) {
		/*
		 * Fast-path, ok to process and nothing queued.
		 */
		sqp->sq_isintr = interrupt;
		sqp->sq_state |= SQS_PROC|SQS_FAST;
		mutex_exit(&sqp->sq_lock);

		(*(sqp->sq_proc))(arg, mp, (void *)sqp);

		mutex_enter(&sqp->sq_lock);
		sqp->sq_state &= ~(SQS_PROC|SQS_FAST);
		if (! sqp->sq_first) {
			/*
			 * Still nothing queued, so just return.
			 */
			mutex_exit(&sqp->sq_lock);
			return;
		}
		state = sqp->sq_state;
		/*
		 * Still need to process now ?
		 */
		if (! (state & (SQS_PROC|SQS_PAUSE)) &&
		    ! (sqp->sq_type & SQT_DEFERRED) &&
		    (! (state & SQS_NOINTR) || ! interrupt))
			process = true;
		else
			process = false;
	} else {
		/*
		 * Enque our mblk.
		 */
		mp->b_prev = (mblk_t *)arg;
		mp->b_next = NULL;
		if (sqp->sq_last)
			sqp->sq_last->b_next = mp;
		else
			sqp->sq_first = mp;
		sqp->sq_last = mp;
		sqp->sq_state++;
		if (sqp->sq_state & SQS_CNT_TOOMANY) {
			/* > 32767 mblks enqueed ??? */
			cmn_err(CE_NOTE, "squeue_enter: %p: squeue overflow",
			    (void *)sqp);
		}
	}
	if (process) {
		squeue_drain(sqp, SQS_ENTER, NULL);
	} else if (! (state & (SQS_PROC | SQS_PAUSE | SQS_NOPROC))) {
		/*
		 * Queue isn't being processed or in pause, so take
		 * any post enqueue/process actions needed before leaving.
		 */
		timeout_id_t tid = sqp->sq_tid;

		if (! (sqp->sq_type & SQT_DEFERRED) &&
		    tid == 0 && sqp->sq_wait != 0) {
			/*
			 * Wait up to sqp->sq_wait ms for an
			 * enter() to process this queue.
			 */
			sqp->sq_awaken = lbolt;
			sqp->sq_tid = timeout(squeue_fire, sqp, sqp->sq_wait);
		} else if (tid) {
			/*
			 * Waiting for an enter() to process mblk(s).
			 */
			clock_t	waited = lbolt - sqp->sq_awaken;

			if (TICK_TO_MSEC(waited) >= sqp->sq_wait &&
				CV_HAS_WAITERS(&sqp->sq_async)) {
				/*
				 * Times up and have a worker thread
				 * waiting for work, so schedule it now.
				 */
				sqp->sq_tid = 0;
				if (sqp->sq_awaken == 0) {
					sqp->sq_awaken = lbolt;
				}
				cv_signal(&sqp->sq_async);
				mutex_exit(&sqp->sq_lock);
				(void) untimeout(tid);
				return;
			}
		} else if (sqp->sq_type & SQT_SOFTINTR) {
			/*
			 * Schedule a soft interrupt if need be.
			 */
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
				ddi_trigger_softintr(sqp->sq_softid);
			}
		} else if (CV_HAS_WAITERS(&sqp->sq_async)) {
			/*
			 * Schedule the worker thread.
			 */
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
		}
	}
	mutex_exit(&sqp->sq_lock);
}

void
squeue_willproxy(squeue_t *proxy)
{
	mutex_enter(&proxy->sq_lock);
	proxy->sq_state |= SQS_NOPROC;
	mutex_exit(&proxy->sq_lock);
}

void
squeue_proxy(squeue_t *sqp, squeue_t *proxy)
{
	clock_t	wait;

	if (sqp != NULL) {
		wait = sqp->sq_wait;
	} else {
		wait = 0;
	}
	mutex_enter(&proxy->sq_lock);
	proxy->sq_state &= ~SQS_NOPROC;
	if (! (proxy->sq_state & SQS_PROC)) {
		squeue_drain(proxy, SQS_PROXY, (void *)wait);
	}
	mutex_exit(&proxy->sq_lock);
}

static void
squeue_fire(void *arg)
{
	squeue_t	*sqp = arg;

	mutex_enter(&sqp->sq_lock);
	if (sqp->sq_tid == 0) {
		mutex_exit(&sqp->sq_lock);
		return;
	}
	sqp->sq_tid = 0;
	if (sqp->sq_pwait) {
		int ticks = sqp->sq_pwait - lbolt;

		if (ticks <= 0) {
			/*
			 * squeue_pause() has expired,
			 * enable processing.
			 */
			sqp->sq_state &= ~SQS_PAUSE;
			if (CV_HAS_WAITERS(&sqp->sq_async)) {
				if (sqp->sq_awaken == 0) {
					sqp->sq_awaken = lbolt;
				}
				cv_signal(&sqp->sq_async);
			}
		} else {
			/*
			 * squeue_pause() still pending,
			 * set up a new timeout().
			 */
			sqp->sq_tid = timeout(squeue_fire, sqp, ticks);
		}
	} else if (! (sqp->sq_state & (SQS_PROC | SQS_PAUSE | SQS_NOPROC)) &&
	    CV_HAS_WAITERS(&sqp->sq_async)) {
		if (sqp->sq_awaken == 0) {
			sqp->sq_awaken = lbolt;
		}
		cv_signal(&sqp->sq_async);
	}
	mutex_exit(&sqp->sq_lock);
}

/*
 * squeue_fill() - fill squeue *sqp with mblk *mp with argument of *arg
 * without processing the squeue.
 */

void
squeue_fill(squeue_t *sqp, mblk_t *mp, void *arg)
{
	mutex_enter(&sqp->sq_lock);
	/*
	 * Enque our mblk.
	 */
	mp->b_prev = (mblk_t *)arg;
	mp->b_next = NULL;
	if (sqp->sq_last)
		sqp->sq_last->b_next = mp;
	else
		sqp->sq_first = mp;
	sqp->sq_last = mp;
	sqp->sq_state++;
	if (sqp->sq_state & SQS_CNT_TOOMANY) {
		/* > 32767 mblks enqueed ??? */
		cmn_err(CE_NOTE, "squeue_fill: %p: squeue overflow",
		    (void *)sqp);
	}
	if (sqp->sq_iwait && lbolt >= sqp->sq_iwait) {
		/*
		 * squeue_nointr() has expired,
		 * enable interrupt mode.
		 */
		sqp->sq_state &= ~SQS_NOINTR;
		sqp->sq_iwait = 0;
	}
	if (sqp->sq_pwait && lbolt >= sqp->sq_pwait) {
		/*
		 * squeue_pause() has expired,
		 * enable processing.
		 */
		sqp->sq_state &= ~SQS_PAUSE;
		sqp->sq_pwait = 0;
		if (CV_HAS_WAITERS(&sqp->sq_async)) {
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
		}
	}
	if (! (sqp->sq_state & (SQS_PROC | SQS_PAUSE | SQS_NOPROC))) {
		/*
		 * Queue isn't being processed or in pause, so take
		 * any post enqueue actions needed before leaving.
		 */
		timeout_id_t tid = sqp->sq_tid;

		if (! (sqp->sq_type & SQT_DEFERRED) &&
		    tid == 0 && sqp->sq_wait != 0) {
			/*
			 * Wait up to sqp->sq_wait ms for an
			 * enter() to process this queue.
			 */
			sqp->sq_awaken = lbolt;
			sqp->sq_tid = timeout(squeue_fire, sqp, sqp->sq_wait);
		} else if (tid) {
			/*
			 * Waiting for an enter() to process mblk(s).
			 */
			clock_t	waited = lbolt - sqp->sq_awaken;

			if (TICK_TO_MSEC(waited) >= sqp->sq_wait &&
				CV_HAS_WAITERS(&sqp->sq_async)) {
				/*
				 * Times up and have a worker thread
				 * waiting for work, so schedule it now.
				 */
				sqp->sq_tid = 0;
				if (sqp->sq_awaken == 0) {
					sqp->sq_awaken = lbolt;
				}
				cv_signal(&sqp->sq_async);
				mutex_exit(&sqp->sq_lock);
				(void) untimeout(tid);
				return;
			}
		} else if (sqp->sq_type & SQT_SOFTINTR) {
			/*
			 * Schedule a soft interrupt if need be.
			 */
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
				ddi_trigger_softintr(sqp->sq_softid);
			}
		} else if (CV_HAS_WAITERS(&sqp->sq_async)) {
			/*
			 * Schedule the worker thread.
			 */
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
		}
	}
	mutex_exit(&sqp->sq_lock);
}

#define	SQS_NOWORK (SQS_PROC|SQS_NOPROC|SQS_PAUSE|SQS_EXIT)

void
squeue_worker(squeue_t *sqp)
{
	kmutex_t *lock = &sqp->sq_lock;
	kcondvar_t *async = &sqp->sq_async;
	processorid_t bind = sqp->sq_bind;
	int bind_flags = sqp->sq_type & SQT_BIND_MASK;
	callb_cpr_t cprinfo;

	if (bind_flags) {
		if (bind_flags & SQT_BIND_ANY) {
			sqp->sq_bind = nca_worker_bind(BIND_FLAT, 0);
		} else if (bind_flags & SQT_BIND_TO) {
			sqp->sq_bind = nca_worker_bind(BIND_ID, bind);
		} else {
			/* Bad Bind flag */
			ASSERT((bind_flags & SQT_BIND_ANY) ||
			    (bind_flags & SQT_BIND_TO));
		}
	}
	CALLB_CPR_INIT(&cprinfo, lock, callb_generic_cpr, "nca");
	mutex_enter(lock);
	if (sqp->sq_init) {
		(*sqp->sq_init)(sqp->sq_init_arg);
		sqp->sq_init = NULL;
	}
	for (;;) {
		while (sqp->sq_first == NULL || (sqp->sq_state & SQS_NOWORK)) {
			if (sqp->sq_state & SQS_EXIT) {
				cv_signal(async);
				CALLB_CPR_EXIT(&cprinfo);
				thread_exit();
				/* NOTREACHED */
			}
			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			cv_wait(async, lock);
			CALLB_CPR_SAFE_END(&cprinfo, lock);
			sqp->sq_awaken = 0;
		}
		if (! bind_flags && (sqp->sq_type & SQT_BIND_MASK)) {
			/*
			 * The bind state has changed, note currently
			 * only state change supported is from no bind.
			 */
			bind = sqp->sq_bind;
			bind_flags = sqp->sq_type & SQT_BIND_MASK;
			if (bind_flags & SQT_BIND_ANY) {
				sqp->sq_bind = nca_worker_bind(BIND_FLAT, 0);
			} else if (bind_flags & SQT_BIND_TO) {
				sqp->sq_bind = nca_worker_bind(BIND_ID, bind);
			} else {
				/* Bad Bind flag */
				ASSERT((bind_flags & SQT_BIND_ANY) ||
				    (bind_flags & SQT_BIND_TO));
			}
		}
		squeue_drain(sqp, SQS_WORKER, NULL);
	}
}

void
squeue_nointr(squeue_t *sqp, mblk_t *mp, void *arg, int ms)
{
	int ticks;

	if (ms <= 0)
		ticks = sqp->sq_wait;
	else
		ticks = MSEC_TO_TICK_ROUNDUP(ms);

	mutex_enter(&sqp->sq_lock);
	if (mp != NULL) {
		/*
		 * Enque on head of list (i.e. putback).
		 */
		mp->b_prev = (mblk_t *)arg;
		if ((mp->b_next = sqp->sq_first) == NULL)
			sqp->sq_last = mp;
		sqp->sq_first = mp;
		sqp->sq_state++;
		if (sqp->sq_state & SQS_CNT_TOOMANY) {
			/* > 32767 mblks enqueed ??? */
			cmn_err(CE_NOTE, "squeue_nointr: %p: squeue overflow",
			    (void *)sqp);
		}
	}
	if (! (sqp->sq_state & (SQS_PROC | SQS_PAUSE | SQS_NOPROC)) &&
	    CV_HAS_WAITERS(&sqp->sq_async)) {
		if (sqp->sq_awaken == 0) {
			sqp->sq_awaken = lbolt;
		}
		cv_signal(&sqp->sq_async);
	}
	ticks += lbolt;
	sqp->sq_state |= SQS_NOINTR;
	if (ticks < sqp->sq_iwait) {
		/* Update only if earler */
		sqp->sq_iwait = ticks;
	}
	mutex_exit(&sqp->sq_lock);
}

void
squeue_pause(squeue_t *sqp, mblk_t *mp, void *arg, int ms, boolean_t wait)
{
	int ticks;
	timeout_id_t tid;

	if (ms < 0)
		ticks = sqp->sq_wait;
	else
		ticks = MSEC_TO_TICK_ROUNDUP(ms);

	mutex_enter(&sqp->sq_lock);
	tid = sqp->sq_tid;
	sqp->sq_tid = 0;
	if (mp != NULL) {
		/*
		 * Enque on head of list (i.e. putback).
		 */
		mp->b_prev = (mblk_t *)arg;
		if ((mp->b_next = sqp->sq_first) == NULL)
			sqp->sq_last = mp;
		sqp->sq_first = mp;
		sqp->sq_state++;
		if (sqp->sq_state & SQS_CNT_TOOMANY) {
			/* > 32767 mblks enqueed ??? */
			cmn_err(CE_NOTE, "squeue_pause: %p: squeue overflow",
			    (void *)sqp);
		}
	}
	if (ms > 0) {
		/* Pause for msec worth of tick(s) */
		sqp->sq_state |= SQS_PAUSE;
		sqp->sq_pwait = ticks + lbolt;
		sqp->sq_tid = timeout(squeue_fire, sqp, ticks);
		if (wait) {
			/* Caller wants to wait */
			cv_wait(&sqp->sq_async, &sqp->sq_lock);
		}
	} else {
		/* Pause */
		sqp->sq_state |= SQS_PAUSE;
		sqp->sq_pwait = 0;
	}
	mutex_exit(&sqp->sq_lock);
	if (tid) {
		(void) untimeout(tid);
	}
}

void
squeue_signal(squeue_t *sqp)
{
	timeout_id_t tid;

	mutex_enter(&sqp->sq_lock);
	if (! (sqp->sq_state & SQS_PAUSE)) {
		mutex_exit(&sqp->sq_lock);
		return;
	}
	sqp->sq_state &= ~SQS_PAUSE;
	tid = sqp->sq_tid;
	if (! (sqp->sq_type & SQT_DEFERRED) && tid == 0 && sqp->sq_wait != 0) {
		/*
		 * If not deferred wait for an enter() to process this mblk.
		 */
		sqp->sq_awaken = lbolt;
		sqp->sq_tid = timeout(squeue_fire, sqp, sqp->sq_wait);
	} else if (tid) {
		/*
		 * Waiting for an enter() to process mblk(s).
		 */
		clock_t	waited = lbolt - sqp->sq_awaken;

		if (TICK_TO_MSEC(waited) >= sqp->sq_wait &&
			CV_HAS_WAITERS(&sqp->sq_async)) {
			/*
			 * Times up, schedule the worker thread.
			 */
			sqp->sq_tid = 0;
			if (sqp->sq_awaken == 0) {
				sqp->sq_awaken = lbolt;
			}
			cv_signal(&sqp->sq_async);
			mutex_exit(&sqp->sq_lock);
			(void) untimeout(tid);
			return;
		}
	} else if (! (sqp->sq_state & SQS_PROC) &&
	    CV_HAS_WAITERS(&sqp->sq_async)) {
		/*
		 * Schedule the worker thread.
		 */
		if (sqp->sq_awaken == 0) {
			sqp->sq_awaken = lbolt;
		}
		cv_signal(&sqp->sq_async);
	}
	mutex_exit(&sqp->sq_lock);
}

/*
 * Note: if squeue_ctl() allocates the mblk (i.e. mp == NULL) it does so
 * using the KM_SLEEP flag, so any caller which requires NOSLEEP must
 * provide an mblk_t.
 */

mblk_t *
squeue_ctl(mblk_t *mp, void *arg, unsigned short flag)
{
	if (mp == NULL) {
		mp = kmem_zalloc(sizeof (*mp), KM_SLEEP);
	}
	mp->b_rptr = (unsigned char *)&mp->b_datap;
	mp->b_datap = (dblk_t *)arg;
	mp->b_flag = flag;

	TSP_ALLOC(mp);

	return (mp);
}

void
squeue_exit(squeue_t *sqp)
{
	timeout_id_t tid;

	mutex_enter(&sqp->sq_lock);
	tid = sqp->sq_tid;
	sqp->sq_state |= SQS_EXIT;
	cv_signal(&sqp->sq_async);
	cv_wait(&sqp->sq_async, &sqp->sq_lock);
	mutex_exit(&sqp->sq_lock);
	if (tid)
		(void) untimeout(tid);
}

void
sqfan_init(
	sqfan_t *sqfp,
	uint32_t ix,
	uint32_t flgcnt,
	uint32_t drain
)
{

	bzero(sqfp, sizeof (*sqfp));

	sqfp->flgcnt = flgcnt | ix;
	sqfp->drain = drain;
	sqfp->sqv = (squeue_t **)kmem_alloc(sizeof (squeue_t *) * ix, KM_SLEEP);
}

void
sqfan_fini(sqfan_t *sqfp)
{
	uint32_t	cnt = sqfp->flgcnt & SQF_CNT_MASK;
	squeue_t	*sqp;
	int32_t		ix = (int)cnt;
	boolean_t	free;

	while (ix > 0) {
		ix--;
		if ((sqp = sqfp->sqv[ix]) != NULL) {
			if (sqp->sq_type & SQT_KMEM)
				free = false;
			else
				free = true;
			squeue_fini(sqp);
			if (free) {
				kmem_free(sqp, sizeof (*sqp));
			}
		}
	}
	kmem_free(sqfp->sqv, sizeof (squeue_t *) * cnt);
}

squeue_t *
sqfan_ixinit(
	sqfan_t *sqfp,
	uint32_t ix,
	squeue_t *sqp,
	uint32_t type,
	processorid_t bind,
	void (*init)(),
	void *init_arg,
	void (*proc)(),
	clock_t wait,
	pri_t pri
)
{
	if (sqp == NULL)
		sqp = kmem_zalloc(sizeof (*sqp), KM_SLEEP);
	else
		bzero(sqp, sizeof (*sqp));
	sqfp->sqv[ix] = sqp;
	(void) squeue_init(sqp, type, bind, init, init_arg, proc, wait, pri);
	return (sqp);
}

/*
 * Fanout to squeue_fill() using a flat (round-robin) distribution.
 */

void
sqfan_fill(sqfan_t *sqfp, mblk_t *mp, void *arg)
{
	int n;
	int ix;
	int new;
	int count = sqfp->flgcnt & SQF_CNT_MASK;
	int dist = sqfp->flgcnt & SQF_DIST_MASK;
	squeue_t *sqp;

	if (count > 1) {
		switch (dist) {

		default:
			/*
			 * No distribution scheme specified, so default
			 * to a flat (round-robin) distribution.
			 */
			do {
				ix = sqfp->ix;
				if ((new = ix + 1) == count)
					new = 0;
			} while (cas32(&sqfp->ix, ix, new) != ix);
			break;

		case SQF_DIST_CNT: {
			/*
			 * Distribute by count (i.e. least queue count).
			 */
			int min = INT_MAX;

			new = 0;
			n = 0;
			for (ix = 0; ix < count; ix++) {
				n = sqfp->sqv[ix]->sq_state & SQS_CNT_MASK;
				if (n < min) {
					min = n;
					new = ix;
				}
			}
			ix = new;
			break;
		}
		case SQF_DIST_IPv4: {
			/*
			 * Distribute by source IPv4 address and TCP port.
			 */
			ipha_t	*ipha = (ipha_t *)mp->b_rptr;
			ssize_t	len = mp->b_wptr - mp->b_rptr;
			ipaddr_t src;

			if (OK_32PTR(mp->b_rptr) &&
			    len >= IP_SIMPLE_HDR_LENGTH) {
				uint16_t port;
#ifdef _LITTLE_ENDIAN
				port = ((uint16_t *)
				    (mp->b_rptr + IP_SIMPLE_HDR_LENGTH))[1];
#else	/* _LITTLE_ENDIAN */
				port = *(uint16_t *)
				    (mp->b_rptr + IP_SIMPLE_HDR_LENGTH);
#endif	/* _LITTLE_ENDIAN */
				src = ipha->ipha_src;
				ix = (src ^ (src >> 8) ^ (src >> 4) ^
				    (src >> 3) ^ (src >> 2) ^ port) % count;
			} else {
				ix = 0;
			}
		}

		}
		sqp = sqfp->sqv[ix];
	} else {
		sqp = *sqfp->sqv;
	}
	if (sqp == NULL) {
		cmn_err(CE_PANIC, "sqfan_fill: bad index %d", ix);
	}
	squeue_fill(sqp, mp, arg);
}

/*
 * Bind a work thread to a processor.
 *
 * Currently supported bind types:
 *
 *	BIND_FLAT - pick the next processor in a round-robin-fashion to
 *	bind the caller to. This assumes that all worker threads are bound
 *	through this interface and all worker threads consume on average the
 *	same amount of CPU time (yes these assumtions are simplistic).
 *
 *	BIND_ID - bind to the specified processor id.
 */

static processorid_t
nca_worker_bind(int type, processorid_t bind)
{
	static cpu_t	*bind_cpu = NULL;
	cpu_t		*new_cpu;

	if (bind_cpu == NULL) {
		/* First time */
		bind_cpu = cpu_list;
	}
	switch (type) {

	case BIND_FLAT:
		/* Pick the next CPU */
		for (new_cpu = bind_cpu->cpu_next;
			new_cpu != bind_cpu;
			new_cpu = new_cpu->cpu_next) {
			if (new_cpu->cpu_flags & CPU_ENABLE) {
				bind_cpu = new_cpu;
				break;
			}
		}
		break;

	case BIND_ID:
		bind_cpu = cpu[bind];
		break;

	default:
		printf("nca_worker_bind(%d): unrecognized type.\n", type);
		break;
	}

	affinity_set(bind_cpu->cpu_id);

	return (bind_cpu->cpu_id);
}

/*
 * NCA libc like functions, note that our versions of these functions are
 * similar (or identical) to there user-land counterparts, differences are
 * mostly due to hardening (i.e. no faults in the kernel please, so caller
 * gives string length) or other data types (i.e. mblk_t vs char*).
 */

char *
strnchr(const char *string, int c, size_t n)
{
	size_t i;
	size_t cmp;

	for (i = 0; i < n && (cmp = string[i]) != 0; i++) {
		if (cmp == c)
			return ((char *)&string[i]);
	}
	return (NULL);
}

char *
strnstr(const char *string, const char *s, size_t n)
{
	ssize_t i, j;
	ssize_t len = strlen(s);

	if (n >= len) {
		n -= len;
		for (i = 0; i <= n; i++) {
			j = 0;
			while (string[i+j] == s[j]) {
				if (++j == len)
					return ((char *)&string[i]);
			}
		}
	}
	return (0);
}

char *
strncasestr(const char *string, const char *s, size_t n)
{
	ssize_t i, j;
	ssize_t len = strlen(s);

	if (n >= len) {
		n -= len;
		for (i = 0; i <= n; i++) {
			j = 0;
			while (tolower(string[i+j]) == tolower(s[j])) {
				if (++j == len)
					return ((char *)&string[i]);
			}
		}
	}
	return (0);
}

char *
strrncasestr(const char *string, const char *s, size_t n)
{
	ssize_t i, j;
	ssize_t len = strlen(s);

	if (n >= len) {
		n -= len;
		for (i = n; i >= 0; i--) {
			j = 0;
			while (tolower(string[i+j]) == tolower(s[j])) {
				if (++j == len)
					return ((char *)&string[i]);
			}
		}
	}
	return (0);
}

#if	SunOS == SunOS_5_7

int
strncasecmp(const char *string, const char *s, size_t n)
{
	size_t i, c;

	for (i = 0; i < n && string[i] != '\0'; i++) {
		c = tolower(string[i]) - tolower(s[i]);
		if (c != 0)
			break;
	}
	return (c);
}

#endif

int
atoin(const char *p, size_t n)
{
	size_t v = 0;
	size_t c, neg = 0;

	if (n == 0 || p == NULL)
		return (0);

	/* Skip any non digits, and check for '-' */
	while ((c = *p) != NULL && !isdigit(c)) {
		if (c == '-')
			neg++;
		p++;
		if (--n == 0)
			return (0);
	}

	if (c == NULL)
		return (0);

	/* Shift and add digit by digit */
	do {
		v *= 10;
		v += c - '0';
		p++;
		if (--n == 0)
			break;
	} while ((c = *p) != NULL && isdigit(c));

	return (neg ? -v : v);
}

int
digits(int n)
{
	int d = 1;

	while (n > 9) {
		n /= 10;
		d++;
	}
	return (d);
}
