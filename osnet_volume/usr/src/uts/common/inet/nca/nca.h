/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_NCA_H
#define	_INET_NCA_H

#pragma ident	"@(#)nca.h	1.9	99/12/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/door.h>
#include <sys/disp.h>
#include "osvers.h"

extern int nca_logging_on;
extern int nca_conn_fanout_size;
extern boolean_t nca_deferred_oq_if;
extern boolean_t nca_fanout_iq_if;

#undef	BEANS

#include "bean.h"

#ifdef	BEANS

typedef struct nca_beans_s {
	beans_tdelta_t rput[5];
	beans_tdelta_t ip_input[9];
	beans_tdelta_t tcp_input[6];
	beans_tdelta_t pad[2];	/* pad between CPUs */
} nca_beans_t;

extern nca_beans_t *nca_beans;

#define	TDELTA(ts, tv) tdelta((ts), nca_beans[CPU->cpu_seqid].tv)
#define	TDELTATS(ts, tv) tdeltats((ts), nca_beans[CPU->cpu_seqid].tv)
#define	TDIS(ts, tv) tdis((ts), nca_beans[CPU->cpu_seqid].tv)

#define	TS(p) ((p)->b_queue)
#define	TSP(p) (bean_ts_t *)TS(p)

#define	TSTAMP_SZ 4092	/* as many as outstanding mblk_t's from rput() */

typedef struct nca_tstamps_s {
	beans_ts_t *next;
	beans_ts_t tstamps[TSTAMP_SZ];
	beans_ts_t pad[64];
} nca_tstamps_t;

extern nca_tstamps_t *nca_tstamps;

#define	TSP_INIT(cpus) {						\
	int _ix;							\
									\
	for (_ix = 0; _ix < cpus; _ix++) {				\
		nca_tstamps[_ix].next = nca_tstamps[_ix].tstamps;	\
	}								\
}

#define	TSP_ALLOC(p) {							\
	TS(p) = (queue_t *)nca_tstamps[CPU->cpu_seqid].next++;		\
	if (nca_tstamps[CPU->cpu_seqid].next ==				\
	    &nca_tstamps[CPU->cpu_seqid].tstamps[TSTAMP_SZ]) {		\
		nca_tstamps[CPU->cpu_seqid].next =			\
		    nca_tstamps[CPU->cpu_seqid].tstamps;		\
	}								\
	*TSP(p) = tstamp();						\
}

#else

#define	TDELTA(ts, tv)
#define	TDELTATS(ts, tv)
#define	TDIS(ts, tv)

#define	TS(p)
#define	TSP(p)
#define	TSP_INIT(cpus)
#define	TSP_ALLOC(p)

#endif

#if	SunOS == SunOS_5_8

/* undef any tcp.h:tcp_t members overloaded by the Solaris 8 tcp.h */

#undef	tcp_last_rcv_lbolt
#undef	tcp_state
#undef	tcp_rto
#undef	tcp_snd_ts_ok
#undef	tcp_snd_ws_ok
#undef	tcp_snxt
#undef	tcp_swnd
#undef	tcp_mss
#undef	tcp_iss
#undef	tcp_rnxt
#undef	tcp_rwnd
#undef	tcp_lport
#undef	tcp_fport
#undef	tcp_ports

/* the iph_t is no longer defined in ip.h for Solaris 8 ? */

/* Unaligned IP header */
typedef struct iph_s {
	uchar_t	iph_version_and_hdr_length;
	uchar_t	iph_type_of_service;
	uchar_t	iph_length[2];
	uchar_t	iph_ident[2];
	uchar_t	iph_fragment_offset_and_flags[2];
	uchar_t	iph_ttl;
	uchar_t	iph_protocol;
	uchar_t	iph_hdr_checksum[2];
	uchar_t	iph_src[4];
	uchar_t	iph_dst[4];
} iph_t;

#endif	/* SunOS == SunOS_5_8 */

extern int nca_debug;

#define	true	B_TRUE			/* used with type boolean_t */
#define	false	B_FALSE			/* used with type boolean_t */

#if defined(i386)
#define	CPUS	ncpus
#else
#define	CPUS	max_ncpus
#endif

/*
 * Power of 2^N Primes useful for hashing for N of 0-28,
 * these primes are the nearest prime <= 2^N - 2^(N-2).
 */

#define	P2Ps() {0, 0, 0, 5, 11, 23, 47, 89, 191, 383, 761, 1531, 3067,	\
		6143, 12281, 24571, 49139, 98299, 196597, 393209,	\
		786431, 1572853, 3145721, 6291449, 12582893, 25165813,	\
		50331599, 100663291, 201326557, 0}

/*
 * Serialization queue type (move to strsubr.h (stream.h?) as a general
 * purpose lightweight mechanism for mblk_t serialization ???).
 *
 * 32-bit adb: XXnnDXXXXnXXnnDDDDnXxxXXnnD
 */
typedef struct squeue_s {
	uint32_t	sq_state;	/* state flags and message count */
	uint32_t	sq_type;	/* type flags */
	processorid_t	sq_bind;	/* processor to bind to */
	ddi_softintr_t	sq_softid;	/* softintr() id */
	void		(*sq_init)();	/* initialize function */
	void		*sq_init_arg;	/* initialize argument */
	void		(*sq_proc)();	/* process function */
	mblk_t		*sq_first;	/* first mblk chain or NULL */
	mblk_t		*sq_last;	/* last mblk chain or NULL */
	clock_t		sq_wait;	/* lbolts to wait after a fill() */
	clock_t		sq_iwait;	/* lbolt after nointr() */
	clock_t		sq_pwait;	/* lbolt after pause() */
	int		sq_isintr;	/* is being or was serviced by */
	timeout_id_t	sq_tid;		/* timer id of pending timeout() */
	kcondvar_t	sq_async;	/* async thread blocks on */
	kmutex_t	sq_lock;	/* lock before using any member */
	clock_t		sq_awaken;	/* time async thread was awakened */
} squeue_t;

/*
 * State flags and message count (i.e. things that change):
 */

#define	SQS_FLG_MASK	0xFFFF0000	/* state flags mask */
#define	SQS_CNT_MASK	0x0000FFFF	/* message count mask */

#define	SQS_CNT_TOOMANY	0x00008000	/* message count toomany */

#define	SQS_PROC	0x00010000	/* being processed */
#define	SQS_WORKER	0x00020000	/* worker thread */
#define	SQS_ENTER	0x00040000	/* enter thread */
#define	SQS_FAST	0x00080000	/* enter-fast thread */
#define	SQS_PROXY	0x00100000	/* proxy thread */
#define	SQS_SOFTINTR	0x00200000	/* softint thread */
					/* 0x00C00000 bits not used */

#define	SQS_NOINTR	0x01000000	/* no interrupt processing */
#define	SQS_PAUSE	0x02000000	/* paused */
#define	SQS_INTRWAIT	0x04000000	/* interrupt waiting */
#define	SQS_NOPROC	0x08000000	/* no processing */
					/* 0x70000000 bits not used */
#define	SQS_EXIT	0x80000000	/* worker(s) exit */

/*
 * Type flags (i.e. things that don't change):
 */

#define	SQT_BIND_MASK	0xFF000000	/* bind flags mask */

#define	SQT_KMEM	0x00000001	/* was kmem_alloc()ed */
#define	SQT_DEFERRED	0x00000002	/* deferred processing */
#define	SQT_SOFTINTR	0x00000004	/* use softintr() */

#define	SQT_BIND_ANY	0x01000000	/* bind worker thread to any CPU */
#define	SQT_BIND_TO	0x02000000	/* bind worker thread to speced CPU */

#define	SQ_STATE_IS(sqp, flags) ((sqp)->sq_state & (flags))
#define	SQ_TYPE_IS(sqp, flags) ((sqp)->sq_type & (flags))


typedef struct sqfan_s {
	uint32_t	flgcnt;		/* flags and squeue_t vector count */
	uint32_t	ix;		/* next sqv[] to process */
	uint32_t	drain;		/* max mblk(s) draind per */
	squeue_t	**sqv;		/* pointer to squeue_t pointer vector */
} sqfan_t;

#define	SQF_CNT_MASK	0x0000FFFF	/* sqfan_t squeue_t count mask */
#define	SQF_CNT_TOOMANY	0x00008000	/* sqfan_t squeue_t count toomany */

#define	SQF_DIST_MASK	0x000F0000	/* sqfan_t distribution flags mask */
#define	SQF_DIST_CNT	0x00010000	/* sqfan_t dist by queue count */
#define	SQF_DIST_IPv4	0x00020000	/* sqfan_t dist by IPv4 src addr */

/*
 * A multiphase timer is implemented using the te_t, tb_t, and ti_t structs.
 *
 * The multiple phases of timer entry execution are:
 *
 * 1) resource, execution is done from resource reclaim when the timer event
 *    is the freeing of the timed resource.
 *
 * 2) process, execution is done from process thread yield (idle/return).
 *
 * 3) time, execution is done from a timeout callback thread.
 *
 * Each of the phases have a seperate timer fire time represented by the
 * the ti_t members lbolt1, lbolt2, and lbolt3. Each lbolt is an absolute
 * lbolt value with lbolt1 <= lbolt2 <= lbolt3.
 */

/*
 * te_t - timer entry.
 *
 * 32-bit kadb: 4X
 */

typedef struct te_s {
	struct te_s *prev;	/* prev te_t */
	struct te_s *next;	/* next te_t */
	struct tb_s *tbp;	/* pointer to timer bucket */
	void	*ep;		/* pointer to encapsulating struct */
} te_t;

/*
 * tb_t - timer bucket.
 *
 * 32-bit kadb: XDXX
 */

typedef struct tb_s {
	struct tb_s *next;	/* next tb_t in ascending time order */
	clock_t	exec;		/* te_t lbolt exec value for bucket */
	te_t	*head;		/* head of te_t list (first timer) */
	te_t	*tail;		/* tail of te_t list (last timer) */
} tb_t;

/*
 * ti_t - timer state.
 *
 * 32-bit kadb: 4DXXDX
 */

typedef struct ti_s {
	clock_t	exec;		/* next te_t exec value (0 = NONE) */
	clock_t	lbolt1;		/* phase1 lbolt1 (0 = NONE) */
	clock_t	lbolt2;		/* phase2 lbolt2 (0 = NONE) */
	clock_t	lbolt3;		/* phase3 lbolt3 (0 = NONE) */
	tb_t	*head;		/* head of tb_t list (first timer bucket) */
	tb_t	*tail;		/* tail of tb_t list (last timer bucket) */
	timeout_id_t tid;	/* timer id of pending timeout() (0 = NONE) */
	void	*ep;		/* pointer to encapsulating struct */
} ti_t;

#define	NCA_TI_INPROC	-1	/* Processing going on */
#define	NCA_TI_NONE	0	/* no lbolt */

/*
 * TIME_WAIT grounded doubly linked list of conn_t's awaiting TIME_WAIT
 * expiration for. This list is used for reclaim, reap, and timer based
 * processing.
 *
 * A multiphase timer is used:
 *
 * phase 1) reclaim of connections during connection allocation
 *
 * phase 2) reaping of connections during squeue_t inq thread unwind
 *
 * phase 3) timeout of connections as a result of a timeout().
 *
 * Each of the phases have a seperate timer fire lbolt represented by the
 * the members lbolt1, lbolt2, and lbolt3, each is an absolute lbolt value
 * with lbolt1 <= lbolt2 <= lbolt3.
 */

typedef struct tw_s {
	clock_t	lbolt1;		/* phase1 lbolt value (0 = NONE) */
	clock_t	lbolt2;		/* phase2 lbolt value  */
	clock_t	lbolt3;		/* phase3 lbolt value  */
	struct conn_s *head;	/* Head of conn_t list */
	struct conn_s *tail;	/* Tail of conn_t list */
	timeout_id_t tid;	/* Timer id of pending timeout() (0 = NONE) */
	void	*ep;		/* pointer to encapsulating struct */
} tw_t;

#define	NCA_TW_NONE	0	/* no lbolt */

#define	NCA_TW_MS 1000

#define	NCA_TW_LBOLT MSEC_TO_TICK(NCA_TW_MS)

#define	NCA_TW_LBOLTS(twp, future) {					\
	clock_t	_lbolt = (future);					\
	clock_t	_mod = _lbolt % NCA_TW_LBOLT;				\
									\
	if (_mod) {							\
		/* Roundup to next TIME_WAIT bucket */			\
		_lbolt += NCA_TW_LBOLT - _mod;				\
	}								\
	if ((twp)->lbolt1 != _lbolt) {					\
		(twp)->lbolt1 = _lbolt;					\
		_lbolt += NCA_TW_LBOLT;					\
		(twp)->lbolt2 = _lbolt;					\
		_lbolt += NCA_TW_LBOLT;					\
		(twp)->lbolt3 = _lbolt;					\
		if ((twp)->tid != 0) {					\
			(void) untimeout((twp)->tid);			\
			(twp)->tid = 0;					\
		}							\
		if ((_lbolt) != NCA_TW_NONE) {				\
			(twp)->tid = timeout((pfv_t)nca_tw_fire, (twp),	\
			    (twp)->lbolt3 - lbolt);			\
		}							\
	}								\
}

/*
 * The Node Fanout structure.
 *
 * The hash tables and their linkage (hashnext) are protected by the
 * per-bucket lock. Each node_t inserted in the list points back at
 * the nodef_t that heads the bucket (hashfanout).
 */

typedef struct nodef_s {
	struct node_s	*head;
	kmutex_t	lock;
} nodef_t;

/*
 * A node_t is used to represent a cached byte-stream object. A node_t is
 * in one of four active states:
 *
 * 1) path != NULL, member of a node_t hash list with an object description
 *    (hashnext, size, path, pathsz members valid).
 *
 * 2) pp != NULL, 1) + phys pages allocated (pp, plrupn, plrunn members valid).
 *
 * 3) data != NULL, 2) + virt mapping allocated (data, datasz, vlrupn, vlrunn
 *    members valid).
 *
 * 4) mp != NULL, 3) + mblk mapping allocated (mp, ref, frtn members valid).
 *
 * 32-bit kadb: XnnD5Xnn6XnnDDnnXXDXDXXDXDXDXDXDXnnXXDXXnnDDXXnnDXXnnaXX
 *
 */

typedef struct node_s {
	uint32_t ref;			/* mblk refs and state of node_t */

	int		size;		/* object size (-1 = UNKNOWN) */
	struct node_s	*plrunn;	/* Phys LRU list next node_t */
	struct node_s	*plrupn;	/* Phys LRU list previous node_t */
	struct node_s	*vlrunn;	/* Virt LRU list next node_t */
	struct node_s	*vlrupn;	/* Virt LRU list previous node_t */
	void		*bucket;	/* Pointer to containing bucket */

	nodef_t	*hashfanout;		/* hash bucket we're part off */
	struct node_s *hashnext;	/* hash list next node_t */
	struct conn_s *connhead;	/* head of list of conn_t(s) in miss */
	struct conn_s *conntail;	/* tail of list of conn_t(s) in miss */
	struct node_s *next;		/* needed if data is in chunks */
	struct node_s *back;		/* needed if data is in chunks */

	clock_t	expire;		/* lbolt node_t expires (0 = NOW, -1 = NEVER) */
	time_t	lastmod;	/* HTTP "Last-Modified:" value */

	squeue_t *sqp;		/* squeue_t node_t is being processed from */
	mblk_t	*req;		/* whole HTTP request (including headers) */
	int	reqsz;		/* size of above */
	char	*path;		/* URI path component */
	int	pathsz;		/* size of above */
	uint_t	method;		/* HTTP request method */
	uint_t	version;	/* HTTP request version */
	uint_t	reqcontl;	/* HTTP "Content-Length:" value */
	char	*reqhdr;	/* HTTP request header(s) */
	int	reqhdrsz;	/* size of above */
	char	*reqhost;	/* HTTP "Host:" string */
	int	reqhostsz;	/* size of above */
	char	*reqaccept;	/* HTTP "Accept:" string */
	int	reqacceptsz;	/* size of above */
	char	*reqacceptl;	/* HTTP "Accept-Language:" string */
	int	reqacceptlsz;	/* size of above */
	uint32_t
	    persist : 1,	/* HTTP persistent connection */
	    was_persist : 1,	/* HTTP was persustent connection */
	    fill_thru_bit_31 : 30;

	page_t	**pp;		/* page pointer vector for data */
	char	*data;		/* data buffer */
	int	datasz;		/* size of above */
	uint16_t *cksum;	/* cksum() vector for data by mss */
	mblk_t	*mp;		/* desballoc()ed mblk slef ref */

	int	hlen;		/* data buffer split header len */
	int	fileoff;	/* file include offset */
	struct node_s *fileback; /* head node_t of a file list (-1 for death) */
	struct node_s *filenext; /* next node_t of a file list */

	uint32_t mss;		/* mblk(s) in size mss */
	kmutex_t lock;		/* */
	frtn_t	frtn;		/* used by desballoc to freemsg() mp */
} node_t;

#define	REF_URI		0x80000000 /* & ref = node_t URI hashed */
#define	REF_PHYS	0x40000000 /* & ref = phys mapping in-use */
#define	REF_VIRT	0x20000000 /* & ref = virt mapping in-use */
#define	REF_MBLK	0x10000000 /* & ref = mblk mapping in-use */
#define	REF_KMEM	0x08000000 /* & ref = kmem mapped (PHYS|VIRT) */
#define	REF_DONE	0x04000000 /* & ref = node_t fill is done */
#define	REF_SAFE	0x02000000 /* & ref = node_t not safe for use */
#define	REF_FILE	0x01000000 /* & ref = node_t filename hashed */
#define	REF_RESP	0x00800000 /* & ref = node_t response header parsed */
#define	REF_NU3		0x00400000 /* --- Not Used */
#define	REF_NU2		0x00200000 /* --- Not Used */
#define	REF_NU1		0x00100000 /* --- Not Used */
#define	REF_TOOMANY	0x00080000 /* & ref = half of mblk mapping count used */
#define	REF_CNT		0x000FFFFF /* & ref = mblk mapping in-use (count of) */

/*
 * Per CPU instance structure.
 *
 * 32-bit adb: XXnnXXnnXnnXXnn228+na
 */

typedef struct nca_cpu_s {

	uint32_t dcb_readers;	/* count of dcb_list readers for this CPU */

	squeue_t *if_inq;	/* if_t input squeue_t */
	squeue_t *if_ouq;	/* if_t output squeue_t */

	ti_t	*tcp_ti;	/* TCP TIMER list */
	tw_t	*tcp_tw;	/* TCP TIME_WAIT list */

	ddi_softintr_t soft_id;	/* soft interrupt id for if_inq worker */

	kmutex_t lock;		/* Lock for initialization */

	char	pad[256 - sizeof (squeue_t *) - sizeof (squeue_t *) -
		    sizeof (ti_t *) - sizeof (tw_t *) -
		    sizeof (ddi_softintr_t) - sizeof (kmutex_t)];
} nca_cpu_t;

extern nca_cpu_t *nca_gv;	/* global per CPU state */

/*
 * hcb_t - host control block.
 *
 * Used early on in packet switching to select packets to be serviced by NCA
 * and optionally later on by the HTTP protocol layer to further select HTTP
 * request to be serviced.
 *
 * dcb_t - door control block.
 *
 * Used to associate one or more hcb_t(s) with a given httpd door instance.
 *
 * dcb_list - dcb_t global list, a singly linked grounded list of dcb_t's.
 *
 * Used to search for a hcb_t match, currently a singly linked grounded list
 * of dcb_t's with a linear walk of the list. While this is adequate for the
 * current httpd support (i.e. a single door) a move to either a hash or tree
 * will be required for multiple httpd instance support (i.e. multiple doors).
 *
 * The dcb_list is protected by a custom reader/writer lock, the motivation
 * for using a custom lock instead of a krwlock_t is that this lock is the
 * single hot spot in NCA (i.e. all in-bound packets must acquire this lock)
 * and a nonlocking atomic readers count scheme is used in the common case
 * (i.e. reader lock) with a fall-back to a conventional kmutex_t for writer
 * (i.e. ndd list add/delete).
 */

typedef struct hcb_s {
	struct hcb_s	*next;		/* Next hcb_t (none: NULL) */
	ipaddr_t	addr;		/* IP address (any: INADDR_ANY or 0) */
	uint16_t	port;		/* TCP port number */
	char		*host;		/* Host: name (any: NULL) */
	ssize_t		hostsz;		/* Size of above */
	char		*root;		/* Document root ("/": NULL) */
	ssize_t		rootsz;		/* Size of above */
} hcb_t;

typedef struct dcb_s {
	struct dcb_s	*next;		/* Next dcb_t (none: NULL) */
	char		*door;		/* Door file (default: NULL) */
	ssize_t		doorsz;		/* Size of above */
	door_handle_t	hand;		/* Door handle (default: NULL) */
	hcb_t		list;		/* Head of a hcb_t list (any: NULL) */
} dcb_t;

extern dcb_t dcb_list;
extern kmutex_t nca_dcb_lock;
extern kcondvar_t nca_dcb_wait;
extern kmutex_t nca_dcb_readers;

#define	NOHANDLE ((door_handle_t)-1)

#define	DCB_COUNT_USELOCK	0x80000000
#define	DCB_COUNT_MASK		0x3FFFFFFF

#define	DCB_RD_ENTER() {						\
	uint32_t *rp = &nca_gv[CPU->cpu_seqid].dcb_readers;		\
									\
	while (atomic_add_32_nv(rp, 1) & DCB_COUNT_USELOCK) {		\
		/* Need to use the lock, so do the dance */		\
		mutex_enter(&nca_dcb_lock);				\
		if (atomic_add_32_nv(rp, -1) == DCB_COUNT_USELOCK &&	\
		    CV_HAS_WAITERS(&nca_dcb_wait)) {			\
			/* May be the last reader for this CPU */	\
			cv_signal(&nca_dcb_wait);			\
		}							\
		mutex_exit(&nca_dcb_lock);				\
		mutex_enter(&nca_dcb_readers);				\
		/*							\
		 * We block above waiting for the writer to exit the	\
		 * readers lock, if we didn't block then while we were	\
		 * away in the nca_dcb_lock enter the writer exited,	\
		 * we could optimize for this case by checking USELOCK	\
		 * after the decrement, but as this is an exceptional	\
		 * case not in the fast-path we'll just take the hit	\
		 * of a needless readers enter/exit.			\
		 */							\
		mutex_exit(&nca_dcb_readers);				\
	}								\
}

#define	DCB_RD_EXIT() {							\
	uint32_t *rp = &nca_gv[CPU->cpu_seqid].dcb_readers;		\
									\
	if (atomic_add_32_nv(rp, -1) == DCB_COUNT_USELOCK) {		\
		mutex_enter(&nca_dcb_lock);				\
		if (CV_HAS_WAITERS(&nca_dcb_wait)) {			\
			/* May be the last reader for this CPU */	\
			cv_signal(&nca_dcb_wait);			\
		}							\
		mutex_exit(&nca_dcb_lock);				\
	}								\
}

#define	DCB_WR_ENTER() {						\
	int cpu;							\
	int readers;							\
									\
	mutex_enter(&nca_dcb_readers);					\
	mutex_enter(&nca_dcb_lock);					\
	for (;;) {							\
		readers = 0;						\
		for (cpu = 0; cpu < CPUS; cpu++) {			\
			int new;					\
			uint32_t *rp = &nca_gv[cpu].dcb_readers;	\
			int old = *rp;					\
									\
			if (old & DCB_COUNT_USELOCK) {			\
				readers += old & DCB_COUNT_MASK;	\
				continue;				\
			}						\
			new = old | DCB_COUNT_USELOCK;			\
			while (cas32(rp, old, new) != old) {		\
				old = *rp;				\
				new = old | DCB_COUNT_USELOCK;		\
			}						\
			readers += new;					\
		}							\
		if (readers == 0)					\
			break;						\
		cv_wait(&nca_dcb_wait, &nca_dcb_lock);			\
	}								\
	mutex_exit(&nca_dcb_lock);					\
}

#define	DCB_WR_EXIT() {							\
	int cpu;							\
									\
	mutex_enter(&nca_dcb_lock);					\
	for (cpu = 0; cpu < CPUS; cpu++) {				\
		int new;						\
		uint32_t *rp = &nca_gv[cpu].dcb_readers;		\
		int old = *rp;						\
									\
		new = old & ~DCB_COUNT_USELOCK;				\
		while (cas32(rp, old, new) != old) {			\
			old = *rp;					\
			new = old & ~DCB_COUNT_USELOCK;			\
		}							\
	}								\
	mutex_exit(&nca_dcb_lock);					\
	mutex_exit(&nca_dcb_readers);					\
}

/*
 * if_t - interface per instance data.
 *
 * 32-bit adb: DDnnXXnnDXDDnDnnXXn
 */

typedef struct if_s {

	boolean_t dev;		/* is a device instance */
	boolean_t dev_priv;	/* is a privileged device instance */

	queue_t	*rqp;		/* our read-side STREAMS queue */
	queue_t	*wqp;		/* our write-side STREAMS queue */

	/* DLPI M_DATA IP fastpath template */
	size_t	mac_length;
	mblk_t	*mac_mp;
	int32_t	mac_mtu;
	int32_t	mac_addr_len;

	uint32_t ip_ident;	/* our IP ident value */

	squeue_t *inq;		/* in-bound squeue_t */
	squeue_t *ouq;		/* out-bound squeue_t */

} if_t;

/*
 * connf_t - connection fanout data.
 *
 * The hash tables and their linkage (hashnextp, hashprevp) are protected
 * by the per-bucket lock. Each conn_t inserted in the list points back at
 * the connf_t that heads the bucket.
 */

typedef struct connf_s {
	struct conn_s	*head;
	kmutex_t	lock;
} connf_t;

/*
 * conn_t - connection per instance data.
 *
 * Note: hashlock is used to provide atomic access to all conn_t members
 * above it. All other members are protected by the per CPU inq squeue_t
 * which is used to serialize access to all conn_t's per interface.
 *
 * TODO: reorder elements in fast-path code access order.
 *
 * 32-bit adb: Dnn4XnXXDnnDnn3XnDXn3XnnXXnnDD3XDXDXDXnnXXnnXXXDDXnnXXXDDX
 *	       DXXnXDDn5DnDnDnDn3DXDX
 *	       Dnn20xnnXnddnXDDnXXDnXXnDDXn6DnDDnXXDXDn4X4DXn4XnX3DnX
 */

typedef struct conn_s {

	int32_t ref;			/* Reference counter */

	te_t	tcp_ti;			/* TCP TIMER timer entry */

	struct conn_s	*twnext;	/* TIME_WAIT next */
	struct conn_s	*twprev;	/* TIME_WAIT prev */
	clock_t	twlbolt;		/* TIME_WAIT lbolt */

	clock_t create;			/* Create lbolt time */

	connf_t	*hashfanout;		/* Hash bucket we're part off */
	struct conn_s	*hashnext;	/* Hash chain next */
	struct conn_s	*hashprev;	/* Hash chain prev */

	/*
	 * Note: atomic access of memebers above is guaranteed by the
	 * hashfanout->lock of the hash bucket that the conn_t is in.
	 */

	size_t	mac_length;		/* MAC prepend length */
	mblk_t	*mac_mp;		/* MAC prepend data */

	ipaddr_t	laddr;		/* Local address */
	ipaddr_t	faddr;		/* Remote address. 0 => not connected */

	union {
		struct {
			uint16_t u_fport; /* Remote port */
			uint16_t u_lport; /* Local port */
		} u_ports1;
		uint32_t u_ports2;	/* Rem port, local port */
					/* Used for TCP_MATCH performance */
	} u_port;
#define	conn_lport	u_port.u_ports1.u_lport
#define	conn_fport	u_port.u_ports1.u_fport
#define	conn_ports	u_port.u_ports2

	if_t	*ifp;			/* Interface for this connection */
	squeue_t *inq;			/* In-bound squeue_t */

	uint32_t req_tag;		/* nca_io_t request tag (0 == NONE) */
	int	req_parse;		/* HTTP request parse state */
	node_t	*req_np;		/* HTTP request node_t */
	mblk_t	*req_mp;		/* HTTP request mblk_t */
	char	*reqpath;		/* HTTP request URI path component */
	int	reqpathsz;		/* size of above */
	char	*reqrefer;		/* HTTP "Referer:" string */
	int	reqrefersz;		/* size of above */
	char	*requagent;		/* HTTP "User-Agent:" string */
	int	requagentsz;		/* size of above */
	struct conn_s *nodenext;	/* Node_t conn_t list */

	/*
	 * req_np state to save accross tcp_xmit() calls (i.e. a conn_t
	 * xmit descriptor), one for the current node_t being xmit'ed
	 * and one for saving the req_np when processing a file node_t.
	 */
	mblk_t	*fill_mp;		/* dup()ed uri node_t ref */
	mblk_t	*fill_fmp;		/* dup()ed file node_t ref */
	struct {
		node_t	*np;		/* != NULL if http_unsent */
		char	*dp;		/* data pointer */
		uint16_t *cp;		/* cksum array */
		int	len;		/* initial segment length */
		int	sz;		/* remaining data to xmit */
		node_t	*fnp;		/* file node_t to process */
	} xmit, xmit_save;

	/*
	 * TCP state.
	 */

	int32_t	tcp_state;

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
		tcp_snd_ws_ok : 1,	/* Received WSCALE from peer */

		tcp_snd_ts_ok : 1,	/* Received TSTAMP from peer */
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
		tcp_refed : 1,		/* conn_t refed by TCP */

		tcp_time_wait_comp : 1, /* TIME_WAIT compressed conn_t */
		http_persist : 1,	/* HTTP persistent connection */
		http_was_persist : 1,	/* HTTP was persustent connection */
		tcp_close : 1,		/* conn_t close */

		tcp_2_junk_fill_thru_bit_31 : 20;
/*
 * Note: all conn_t members to be accessed by a tcp_time_wait_comp conn_t
 *       must be above this point !!!
 */

	uchar_t	tcp_timer_backoff;	/* Backoff shift count. */
	clock_t tcp_last_recv_time;	/* Last time we receive a segment. */
	clock_t	tcp_dack_set_time;	/* When delayed ACK timer is set. */

	int	tcp_ip_hdr_len;		/* Byte len of our current IP header */
	clock_t	tcp_first_timer_threshold;  /* When to prod IP */
	clock_t	tcp_second_timer_threshold; /* When to give up completely */
	clock_t	tcp_first_ctimer_threshold; /* 1st threshold while connecting */
	clock_t tcp_second_ctimer_threshold; /* 2nd ... while connecting */

	clock_t	tcp_last_rcv_lbolt; /* lbolt on last packet, used for PAWS */

	int32_t	tcp_keepalive_intrvl;	/* Zero means don't bother */

	uint32_t tcp_obsegs;		/* Outbound segments on this stream */

	uint32_t tcp_mss;		/* Max segment size */
	uint32_t tcp_naglim;		/* Tunable nagle limit */
	int32_t	tcp_hdr_len;		/* Byte len of combined TCP/IP hdr */
	tcph_t	*tcp_tcph;		/* tcp header within combined hdr */
	int32_t	tcp_tcp_hdr_len;	/* tcp header len within combined */
	uint32_t	tcp_valid_bits;
#define	TCP_ISS_VALID	0x1	/* Is the tcp_iss seq num active? */
#define	TCP_FSS_VALID	0x2	/* Is the tcp_fss seq num active? */
#define	TCP_URG_VALID	0x4	/* If the tcp_urg seq num active? */

	int32_t	tcp_xmit_hiwater;	/* Send buffer high water mark. */

	union {				/* template ip header */
		ipha_t	tcp_u_ipha;
		char	tcp_u_buf[IP_SIMPLE_HDR_LENGTH+TCP_MIN_HEADER_LENGTH];
		double	tcp_u_aligner;
	} tcp_u;
#define	tcp_ipha	tcp_u.tcp_u_ipha
#define	tcp_iphc	tcp_u.tcp_u_buf

	uint32_t tcp_sum;		/* checksum to compensate for source */
					/* routed packets. Host byte order */

	uint16_t tcp_last_sent_len;	/* Record length for nagle */
	uint16_t tcp_dupack_cnt;	/* # of consequtive duplicate acks */

	uint32_t tcp_rnxt;		/* Seq we expect to recv next */
	uint32_t tcp_rwnd;		/* Current receive window */
	uint32_t tcp_rwnd_max;		/* Maximum receive window */

	mblk_t	*tcp_rcv_head;		/* Queued until push, urgent data or */
	mblk_t	*tcp_rcv_tail;		/* the count exceeds */
	uint32_t tcp_rcv_cnt;		/* tcp_rcv_push_wait. */

	mblk_t	*tcp_reass_head;	/* Out of order reassembly list head */
	mblk_t	*tcp_reass_tail;	/* Out of order reassembly list tail */

	uint32_t tcp_cwnd_ssthresh;	/* Congestion window */
	uint32_t tcp_cwnd_max;
	uint_t tcp_csuna;		/* Clear (no rexmits in window) suna */

	int	tcp_rttv_updates;
	clock_t	tcp_rto;		/* Round trip timeout */
	clock_t	tcp_rtt_sa;		/* Round trip smoothed average */
	clock_t	tcp_rtt_sd;		/* Round trip smoothed deviation */
	clock_t	tcp_rtt_update;		/* Round trip update(s) */
	clock_t tcp_ms_we_have_waited;	/* Total retrans time */

	uint32_t tcp_swl1;		/* These help us avoid using stale */
	uint32_t tcp_swl2;		/*  packets to update state */

	mblk_t	*tcp_xmit_head;		/* Head of rexmit list */
	mblk_t	*tcp_xmit_last;		/* last valid data seen by tcp_wput */
	uint32_t tcp_unsent;		/* # of bytes in hand that are unsent */
	mblk_t	*tcp_xmit_tail;		/* Last rexmit data sent */
	uint32_t tcp_xmit_tail_unsent;	/* # of unsent bytes in xmit_tail */

	uint32_t tcp_snxt;		/* Senders next seq num */
	uint32_t tcp_suna;		/* Sender unacknowledged */
	uint32_t tcp_rexmit_nxt;	/* Next rexmit seq num */
	uint32_t tcp_rexmit_max;	/* Max retran seq num */
	int32_t	tcp_snd_burst;		/* Send burst factor */
	uint32_t tcp_swnd;		/* Senders window (relative to suna) */
	uint32_t tcp_cwnd;		/* Congestion window */
	uint32_t tcp_cwnd_cnt;		/* cwnd cnt in congestion avoidance */
	uint32_t tcp_ackonly;		/* Senders last ack seq num */

	uint32_t tcp_irs;		/* Initial recv seq num */
	uint32_t tcp_iss;		/* Initial send seq num */
	uint32_t tcp_fss;		/* Final/fin send seq num */
	uint32_t tcp_urg;		/* Urgent data seq num */

	uint32_t tcp_rack;		/* Seq # we have acked */
	uint32_t tcp_rack_cnt;		/* # of bytes we have deferred ack */
	uint32_t tcp_rack_cur_max;	/* # bytes we may defer ack for now */
	uint32_t tcp_rack_abs_max;	/* # of bytes we may defer ack ever */

	uint32_t tcp_max_swnd;		/* Maximum swnd we have seen */

} conn_t;

/*
 * conn_t squeue_ctl() flag values:
 */

#define	CONN_MISS_DONE	0x0001		/* The conn_t miss processing is done */
#define	IF_TIME_WAIT	0x0002		/* A TIME_WAIT has fired */
#define	IF_TCP_TIMER	0x0003		/* A TCP TIMER has fired */
#define	CONN_TCP_TIMER	0x0004		/* A TCP TIMER needs to be execed */

#define	CONN_REFHOLD(connp) {						\
	if (connp->ref <= 0)						\
		cmn_err(CE_PANIC,					\
		    "nca CONN_REFHOLD: %p has no references",		\
		    (void *)connp);					\
	connp->ref++;							\
}

#define	CONN_REFRELE(connp) {						\
	if (connp->ref <= 0)						\
		cmn_err(CE_PANIC,					\
		    "nca CONN_REFRELE: %p has no references",		\
		    (void *)connp);					\
	connp->ref--;							\
	if (connp->ref == 0) {						\
		/* Last ref of a conn_t, so free it */			\
		kmutex_t *lock = &connp->hashfanout->lock;		\
		mutex_enter(lock);					\
		nca_conn_free(connp);					\
		/* Note: nca_conn_free exits lock */			\
	}								\
}

#define	isdigit(c)	((c) >= '0' && (c) <= '9')
#define	tolower(c)	((c) >= 'A' && (c) <= 'Z' ? (c) | 0x20 : (c))
#define	isalpha(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && \
			(c) <= 'z'))
#define	isspace(c)	((c) == ' ' || (c) == '\t' || (c) == '\n' || \
			(c) == '\r' || (c) == '\f' || (c) == '\013')

#if	SunOS < SunOS_5_8
extern int strncasecmp(const char *, const char *, size_t);
#endif
extern char *strnchr(const char *, int, size_t);
extern char *strnstr(const char *, const char *, size_t);
extern char *strncasestr(const char *, const char *, size_t);
extern char *strrncasestr(const char *, const char *, size_t);
extern int atoin(const char *, size_t);
extern int digits(int);

extern void nca_conn_free(conn_t *);
extern void nca_logit_off(void);

extern squeue_t *squeue_init(squeue_t *, uint32_t, processorid_t, void (*)(),
    void *, void (*)(), clock_t, pri_t);
extern void squeue_fini(squeue_t *);
extern void squeue_enter(squeue_t *, mblk_t *, void *);
extern void squeue_fill(squeue_t *, mblk_t *, void *);
extern void squeue_worker(squeue_t *);
extern mblk_t *squeue_ctl(mblk_t *, void *, unsigned short);
extern void squeue_signal(squeue_t *);
extern void squeue_exit(squeue_t *);
extern void sqfan_init(sqfan_t *, uint32_t, uint32_t, uint32_t);
extern squeue_t *sqfan_ixinit(sqfan_t *, uint32_t, squeue_t *, uint32_t,
    processorid_t, void (*)(), void *, void (*)(), clock_t, pri_t);
extern void sqfan_fini(sqfan_t *);
extern void sqfan_fill(sqfan_t *, mblk_t *, void *);
extern void squeue_nointr(squeue_t *, mblk_t *, void *, int);
extern void squeue_pause(squeue_t *, mblk_t *, void *, int, boolean_t);
extern void squeue_willproxy(squeue_t *);
extern void squeue_proxy(squeue_t *, squeue_t *);
extern void squeue_bind(squeue_t *, uint32_t, processorid_t);

#if 1
/*
 * We want to use the tcp_mib in NCA. The ip_mib is already made extern
 * in ip.h so we don't need to declare it here.
 */
extern mib2_tcp_t tcp_mib;

#else
/*
 * Remove this when the mib issues have been resolved:
 *
 * 1) stubs or kernel def for tcp_mib & ip_mib have been done
 *
 * 2) ifdef for Solaris 8 only.
 *
 * 3) add _depends_on[]
 *
 */
#undef BUMP_MIB
#undef UPDATE_MIB
#define	UPDATE_MIB(v, n)
#define	BUMP_MIB(v)

#endif

/*
 * NCA_COUNTER() is used to add a signed long value to a unsigned long
 * counter, in general these counters are used to maintain NCA state.
 *
 * NCA_DEBUG_COUNTER() is used like NCA_COUNTER() but for counters used
 * to maintain additional debug state, by default these counters aren't
 * updated unless the global value nca_debug_counter is set to a value
 * other then zero.
 *
 * Also, if NCA_COUNTER_TRACE is defined a time ordered wrapping trace
 * buffer is maintained with hrtime_t stamps, counter address, value to
 * add, and new value entries for all NCA_COUNTER() and NCA_DEBUG_COUNTER()
 * use.
 */

extern int nca_debug_counter;

#undef	NCA_COUNTER_TRACE

#ifdef	NCA_COUNTER_TRACE

#define	NCA_COUNTER_TRACE_SZ	1024

typedef struct nca_counter_s {
	hrtime_t	t;
	unsigned long	*p;
	unsigned long	v;
	unsigned long	nv;
} nca_counter_t;

extern nca_counter_t nca_counter_tv[];
extern nca_counter_t *nca_counter_tp;

#define	NCA_COUNTER(_p, _v) {						\
	unsigned long	*p = _p;					\
	long		v = _v;						\
	unsigned long	_nv;						\
	nca_counter_t	*_otp;						\
	nca_counter_t	*_ntp;						\
									\
	_nv = atomic_add_long_nv(p, v);					\
	do {								\
		_otp = nca_counter_tp;					\
		_ntp = _otp + 1;					\
		if (_ntp == &nca_counter_tv[NCA_COUNTER_TRACE_SZ])	\
			_ntp = nca_counter_tv;				\
	} while (casptr((void *)&nca_counter_tp, (void *)_otp,		\
	    (void *)_ntp) != (void *)_otp);				\
	_ntp->t = gethrtime();						\
	_ntp->p = p;							\
	_ntp->v = v;							\
	_ntp->nv = _nv;							\
}

#else	/* NCA_COUNTER_TRACE */

#define	NCA_COUNTER(p, v) atomic_add_long((p), (v))

#endif	/* NCA_COUNTER_TRACE */

#define	NCA_DEBUG_COUNTER(p, v) {					\
	if (nca_debug_counter)						\
		NCA_COUNTER(p, v);					\
}

extern pgcnt_t nca_ppmax;
extern pgcnt_t nca_vpmax;
extern pgcnt_t nca_ppmem;
extern pgcnt_t nca_vpmem;
extern ssize_t nca_kbmem;
extern ssize_t nca_mbmem;
extern ssize_t nca_cbmem;
extern ssize_t nca_lbmem;
extern size_t  nca_maxkmem;

extern ulong_t nca_hits;
extern ulong_t nca_304hits;
extern ulong_t nca_missfast;
extern ulong_t nca_miss;
extern ulong_t nca_filehits;
extern ulong_t nca_filemissfast1;
extern ulong_t nca_filemissfast2;
extern ulong_t nca_filemiss;
extern ulong_t nca_filemiss;
extern ulong_t nca_missed1;
extern ulong_t nca_missed2;
extern ulong_t nca_missed3;
extern ulong_t nca_missed4;
extern ulong_t nca_missed5;
extern ulong_t nca_missed6;
extern ulong_t nca_miss1;
extern ulong_t nca_miss2;
extern ulong_t nca_missnot;
extern ulong_t nca_missbad;

extern ulong_t nca_nocache1;
extern ulong_t nca_nocache2;
extern ulong_t nca_nocache3;
extern ulong_t nca_nocache4;
extern ulong_t nca_nocache5;
extern ulong_t nca_nocache6;
extern ulong_t nca_nocache6nomp;
extern ulong_t nca_nocache7;
extern ulong_t nca_nocache8;
extern ulong_t nca_nocache9;
extern ulong_t nca_nocache10;
extern ulong_t nca_nocache11;
extern ulong_t nca_nocache12;
extern ulong_t nca_nocache13;
extern ulong_t nca_nocache14;
extern ulong_t nca_nodes;
extern ulong_t nca_desballoc;

extern ulong_t nca_plrucnt;
extern ulong_t nca_vlrucnt;
extern ulong_t nca_rpcall;
extern ulong_t nca_rvcall;
extern ulong_t nca_rpbusy;
extern ulong_t nca_rvbusy;
extern ulong_t nca_rpempty;
extern ulong_t nca_rvempty;
extern ulong_t nca_rpdone;
extern ulong_t nca_rvdone;
extern ulong_t nca_rmdone;
extern ulong_t nca_rkdone;
extern ulong_t nca_rndone;
extern ulong_t nca_rpfail;
extern ulong_t nca_rvfail;
extern ulong_t nca_rmfail;
extern ulong_t nca_rkfail;
extern ulong_t nca_rnh;
extern ulong_t nca_ref[];

extern ulong_t nca_mapinfail;
extern ulong_t nca_mapinfail1;
extern ulong_t nca_mapinfail2;
extern ulong_t nca_mapinfail3;

extern ulong_t nca_httpd_http;
extern ulong_t nca_httpd_badsz;
extern ulong_t nca_httpd_nosz;
extern ulong_t nca_httpd_filename;
extern ulong_t nca_httpd_filename1;
extern ulong_t nca_httpd_filename2;
extern ulong_t nca_httpd_trailer;

extern ulong_t nca_logit;
extern ulong_t nca_logit_nomp;
extern ulong_t nca_logit_fail;
extern ulong_t nca_logit_flush_NULL1;
extern ulong_t nca_logit_flush_NULL2;
extern ulong_t nca_logit_flush_NULL3;
extern ulong_t nca_logit_noupcall;

extern ulong_t nca_conn_count;
extern ulong_t nca_conn_kmem;
extern ulong_t nca_conn_kmem_fail;
extern ulong_t nca_conn_allocb_fail;
extern ulong_t nca_conn_tw;
extern ulong_t nca_conn_tw1;
extern ulong_t nca_conn_tw2;
extern ulong_t nca_conn_reinit_cnt;
extern ulong_t nca_conn_NULL1;

extern ulong_t ipsendup;
extern ulong_t ipwrongcpu;
extern ulong_t iponcpu;

extern ulong_t nca_tcp_xmit_null;
extern ulong_t nca_tcp_xmit_null1;

extern ulong_t tw_on;
extern ulong_t tw_fire;
extern ulong_t tw_fire1;
extern ulong_t tw_fire2;
extern ulong_t tw_fire3;
extern ulong_t tw_add;
extern ulong_t tw_add1;
extern ulong_t tw_delete;
extern ulong_t tw_reclaim;
extern ulong_t tw_reap;
extern ulong_t tw_reap1;
extern ulong_t tw_reap2;
extern ulong_t tw_reap3;
extern ulong_t tw_reap4;
extern ulong_t tw_reap5;
extern ulong_t tw_timer;
extern ulong_t tw_timer1;
extern ulong_t tw_timer2;
extern ulong_t tw_timer3;
extern ulong_t tw_timer4;
extern ulong_t tw_timer5;

extern ulong_t ti_on;
extern ulong_t ti_fire;
extern ulong_t ti_fire1;
extern ulong_t ti_fire2;
extern ulong_t ti_fire3;
extern ulong_t ti_fire4;
extern ulong_t ti_add;
extern ulong_t ti_add1;
extern ulong_t ti_add2;
extern ulong_t ti_add3;
extern ulong_t ti_add4;
extern ulong_t ti_add5;
extern ulong_t ti_add_reuse;
extern ulong_t ti_add_failed;
extern ulong_t ti_delete;
extern ulong_t ti_delete1;
extern ulong_t ti_delete2;
extern ulong_t ti_reap;
extern ulong_t ti_reap1;
extern ulong_t ti_reap2;
extern ulong_t ti_reap3;
extern ulong_t ti_reap4;
extern ulong_t ti_reap5;
extern ulong_t ti_timer;
extern ulong_t ti_timer1;
extern ulong_t ti_timer2;
extern ulong_t ti_timer3;
extern ulong_t ti_timer4;
extern ulong_t ti_timer5;
extern ulong_t ti_timer6;

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_NCA_H */
