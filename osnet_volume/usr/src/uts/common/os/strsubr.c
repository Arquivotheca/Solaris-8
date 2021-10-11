/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strsubr.c	1.227	99/11/20 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/conf.h>  /* for the fmod routines */
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/session.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/poll.h>
#include <sys/termio.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/sad.h>
#include <sys/priocntl.h>
#include <sys/procset.h>
#include <sys/vmem.h>
#include <sys/bitmap.h>
#include <sys/kmem.h>
#include <sys/siginfo.h>
#include <sys/vtrace.h>
#include <sys/callb.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/vmsystm.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <sys/swap.h>
#include <sys/atomic.h>
#include <sys/suntpi.h>
#include <sys/strlog.h>
#include <sys/promif.h>

#define	O_SAMESTR(q)	(((q)->q_next) && \
	(((q)->q_flag & QREADR) == ((q)->q_next->q_flag & QREADR)))

#ifdef	STREAMSDEBUG
int	streamsdebug = 0;
#define	STRASSERT(EX)	if (streamsdebug) ASSERT(EX)
#else
#define	STRASSERT(x)
#endif	STREAMSDEBUG

/*
 * WARNING:
 * The variables and routines in this file are private, belonging
 * to the STREAMS subsystem. These should not be used by modules
 * or drivers. Compatibility will not be guaranteed.
 */

#define	ncopyin(A, B, C, D)	copyin(A, B, C)		/* temporary */
#define	ncopyout(A, B, C, D)	copyout(A, B, C)	/* temporary */

/*
 * Id value used to distinguish between different multiplexor links.
 */
static int32_t lnk_id = 0;

/*
 * The hash table for the sync queues waiting to be serviced. hash function is
 * the syncq priority. each hash bucket is independently serviced by its own
 * thread(s).
 *
 * Hash buckets are sorted by sqserv_pri field. Each syncq will be put in the
 * bucket with sq_pri <= sqserv_pri.
 */
typedef struct syncq_servlist {
	struct syncq	*sqserv_head;
	struct syncq	*sqserv_tail;
	uint_t		 sqserv_threadcount;	/* # of threads created */
	uint_t		 sqserv_awake;		/* # of threads awake */
	uint_t		 sqserv_size;		/* # of queues on the list */
	pri_t		 sqserv_pri;		/* Associated Priority */
	pri_t		 sqserv_threadpri;	/* Associated thread Priority */
	kthread_id_t	*sqserv_threads;	/* Service thread per cpu */
	uint_t		 sqserv_nthreads;	/* # of elements in above */
	kmutex_t	 sqserv_lock;		/* Protects the structure */
	kcondvar_t	 sqserv_cv;		/* Wake up background thread */
} sqserv_t;

#define	NSQSERVERS 2
uint_t nsqservers = NSQSERVERS;

static sqserv_t *sqservers;	/* Array of sqservers */
static void sqservinit(void);

/*
 * Flag to display debug information
 */

int	strdebug = 0;

/*
 * Queue scheduling control variables.
 */
char qrunflag;			/* set iff queues are enabled */
struct queue *qhead;		/* head of queues to run */
struct queue *qtail;		/*  last queue */
kcondvar_t services_to_run;	/* wake up background service thread */
kmutex_t service_queue;		/* protects qhead, qtail.  used with */
				/* services_to_run to dispatch threads */

static kmutex_t	freebs_lock;	/* protects queue of freebs */
static kcondvar_t freebs_cv;	/* freebs sleep/wakeup */
static mblk_t	*freebs_list;	/* list of buffers free */
kthread_id_t	liberator;	/* freebs() thread id */
char		strbcflag;	/* bufcall functions ready to go */
struct bclist	strbcalls;	/* list of waiting bufcalls */
kmutex_t	bcall_monitor;	/* sleep/wakeup style monitor */
kcondvar_t	bcall_cv;	/* wait 'till executing bufcall completes */
int strscanflag;		/* true when strscan timeout is pending */
kmutex_t	muxifier;	/* single-threads multiplexor creation */
kmutex_t	sad_lock;	/* protects sad drivers autopush */
kthread_id_t	bkgrnd_thread;
void		mp_strinit(void);
int 		putctl_wait(queue_t *, int);
const int	streams_queue_size = sizeof (queue_t);
extern void	str_ftflow(dblk_t *);

/*
 * Counters background_{count,awake} protected by the kmutex_t service_queue.
 *
 * background_count is the count of the number of background threads created
 * at streams initialization.
 *
 * background_awake is the count of the number of background threads awake.
 *
 * The sqthread counters are used identically in the sqthread() routine.
 */
int background_count = 0;
int background_awake = 0;

/*
 * run_queues is set in known spots before we  do a putnext() on
 * STREAM head.  The idea is to fire off queuerun(), after putnext()
 * as opposed to wakeup the background() thread, and therefore save
 * the extra context switching. To avoid missed wakeups, or face the
 * overhead of acquiring service_queue lock now we do a queuerun()
 * after the putnext() without checking qready().
 */
int run_queues = 0;

/*
 * sq_max_size is the depth of the syncq (in number of messages) before
 * fill_syncq() starts QFULL'ing destination queues. The default value
 * has been set at 2 to be consistent with the old algorithm which evaluated
 * to the value of 2. The original value of 2 was an arbitrary number. For
 * potential performance gain, this value is tunable in /etc/system.
 */
int sq_max_size = 2;

/*
 * the number of ciputctrl structures per syncq and stream we create when
 * needed.
 */
int n_ciputctrl;
int max_n_ciputctrl = 16;
/*
 * if n_ciputctrl is < min_n_ciputctrl don't even create ciputctrl_cache.
 */
int min_n_ciputctrl = 2;

/*
 * Outer perimeter - this is a linked list of outer syncqs that are handed of
 * to the qwriter_outer_thread for it to process.
 */
struct writer_work {
	struct writer_work	*ww_next;
	syncq_t			*ww_outer;
};
struct writer_work	*writer_work, *writer_tail;
kmutex_t	writer_lock;
kcondvar_t	writer_wait;
kthread_id_t	writer_thread;

static struct mux_node *mux_nodes;	/* mux info for cycle checking */
kmutex_t strresources;			/* protects global resources */

perdm_t *perdev_syncq;
perdm_t *permod_syncq;
static kmutex_t perdm_lock;

extern struct qinit strdata;
extern struct qinit stwdata;

static void runservice(queue_t *);
static void background(void);
static void freebs(void);
static syncq_t *new_syncq(void);
static syncq_t *dq_syncq(sqserv_t *, syncq_t *);
static void free_syncq(syncq_t *);
static void outer_insert(syncq_t *, syncq_t *);
static void outer_remove(syncq_t *, syncq_t *);
static void qwriter_outer_thread(void);
static void write_now(syncq_t *);
static void queue_writer_work(syncq_t *);
#ifdef UNUSED
static void set_qfull(queue_t *);
#endif
static void clr_qfull(queue_t *);
static void enable_svc(queue_t *);
static queue_t *dq_service(void);
static void runbufcalls(void);
static void sqenable(syncq_t *);
static void sqthread(caddr_t);
static void sqfill_events(syncq_t *, queue_t *, mblk_t *, void (*)());
static void sq_sv_disable(syncq_t *);
static void wait_sq_svc(syncq_t *);
static void rm_sq_svc(syncq_t *);
static void wait_q_syncq(queue_t *);


void set_nfsrv_ptr(queue_t *, queue_t *, queue_t *, queue_t *);
void set_nbsrv_ptr(queue_t *, queue_t *, queue_t *, queue_t *);
void reset_nfsrv_ptr(queue_t *, queue_t *, stdata_t *);
void reset_nbsrv_ptr(queue_t *, queue_t *, stdata_t *);

static void sq_run_events(syncq_t *);
static int propagate_syncq(queue_t *);
static syncq_t *setq_finddm(perdm_t *, struct streamtab *);
static void sqlist_insert(sqlist_t *, syncq_t *);

#ifdef TRACE
int	enqueued;		/* count of enqueued services */
#endif /* TRACE */

static void	blocksq(syncq_t *, ushort_t, int);
static void	unblocksq(syncq_t *, ushort_t, int);
static int	dropsq(syncq_t *, uint16_t);
static void	emptysq(syncq_t *);
static sqlist_t	*build_sqlist(queue_t *, struct stdata *, int);
static void	free_sqlist(sqlist_t *);
static void	strsetuio(stdata_t *);
static size_t	struiomapin(stdata_t *, struct uio *, size_t, mblk_t **);

struct kmem_cache *stream_head_cache;
struct kmem_cache *queue_cache;
struct kmem_cache *syncq_cache;
struct kmem_cache *qband_cache;
struct kmem_cache *linkinfo_cache;
struct kmem_cache *strsig_cache;
struct kmem_cache *bufcall_cache;
struct kmem_cache *callbparams_cache;
struct kmem_cache *ciputctrl_cache = NULL;

static stdata_t *stream_head_list;
static linkinfo_t *linkinfo_list;

/*
 *  Qinit structure and Module_info structures
 *	for passthru read and write queues
 */


static void pass_wput(queue_t *, mblk_t *);
static queue_t *link_addpassthru(stdata_t *);
static void link_rempassthru(queue_t *);

struct  module_info passthru_info = {
	0,
	"passthru",
	0,
	INFPSZ,
	STRHIGH,
	STRLOW
};
struct  qinit passthru_rinit = {
	(int (*)())putnext,
	NULL,
	NULL,
	NULL,
	NULL,
	&passthru_info,
	NULL
};
struct  qinit passthru_winit = {
	(int (*)()) pass_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&passthru_info,
	NULL
};

/* To turn off cow protection, simply define USECOW to 0. */
#define	USECOW 1

int strzc_on = 1;		/* If this is off, don't use zero-copy. */
uint_t strzc_write_threshold = 0x4000;
uint_t strzc_cow_check_period = 48;	/* how often we check cow faults */
uint_t strzc_cowfault_allowed = 6;	/* if exceeded, turn off zero-copy */
uint_t strzc_minblk = 8192;
struct zero_copy_kstat *zckstat = NULL;

static void zckstat_create(void);

#ifdef ZC_TEST
int zcdebug = ZC_ERROR, zcperf = 0, zcslice = 1;
int syncstream = 1;
int usecow = 1;
#undef	USECOW
#define	USECOW usecow
#endif

/* Handling of delayed messages on the inner syncq. */

/*
 * DEBUG versions should use function versions (to simplify tracing) and
 * non-DEBUG kernels should use macro versions.
 */

/*
 * Put a queue on the syncq list of queues.
 * Assumes SQLOCK held.
 */
#define	SQPUT_Q(sq, qp)							\
{									\
	ASSERT(MUTEX_HELD(SQLOCK(sq)));					\
	if (!(qp->q_sqflags & Q_SQQUEUED)) {				\
		/* The queue should not be linked anywhere */		\
		ASSERT((qp->q_sqprev == NULL) && (qp->q_sqnext == NULL)); \
		/* Head and tail may only be NULL simultaneously */	\
		ASSERT(sq->sq_head != NULL || sq->sq_tail == NULL);	\
		ASSERT(sq->sq_tail != NULL || sq->sq_head == NULL);	\
		/* Queue may be only enqueyed on its syncq */		\
		ASSERT(sq == qp->q_syncq);				\
		/* Check the correctness of SQ_MESSAGES flag */		\
		ASSERT(sq->sq_head == NULL ||				\
		    (sq->sq_flags & SQ_MESSAGES) != 0);			\
		/* Sanity check first/last elements of the list */	\
		ASSERT(sq->sq_head == NULL || sq->sq_head->q_sqprev == NULL); \
		ASSERT(sq->sq_tail == NULL || sq->sq_tail->q_sqnext == NULL); \
		/*							\
		 * Sanity check of priority field: empty queue should	\
		 * have zero priority					\
		 * and nqueues equal to zero.				\
		 */	\
		ASSERT(sq->sq_head != NULL || sq->sq_pri == 0);		\
		/* Sanity check of sq_nqueues field */			\
		ASSERT(sq->sq_head == NULL || sq->sq_nqueues != 0);	\
		ASSERT(sq->sq_head != NULL || sq->sq_nqueues == 0);	\
		if (sq->sq_head == NULL) {				\
			sq->sq_head = sq->sq_tail = qp;			\
			sq->sq_flags |= SQ_MESSAGES;			\
		} else if (qp->q_spri == 0) {				\
			qp->q_sqprev = sq->sq_tail;			\
			sq->sq_tail->q_sqnext = qp;			\
			sq->sq_tail = qp;				\
		} else {						\
			/*						\
			 * Put this queue in priority order: higher	\
			 * priority gets closer to the head.		\
			 */						\
			queue_t **qpp = &sq->sq_tail;			\
			queue_t *qnext = NULL;				\
									\
			while (*qpp != NULL && qp->q_spri > (*qpp)->q_spri) { \
				qnext = *qpp;				\
				qpp = &(*qpp)->q_sqprev;		\
			}						\
			qp->q_sqnext = qnext;				\
			qp->q_sqprev = *qpp;				\
			if (*qpp != NULL) {				\
				(*qpp)->q_sqnext = qp;			\
			} else {					\
				sq->sq_head = qp;			\
				sq->sq_pri = sq->sq_head->q_spri;	\
			}						\
			*qpp = qp;					\
		}							\
		qp->q_sqflags |= Q_SQQUEUED;				\
		sq->sq_nqueues++;					\
	}								\
}

/*
 * Remove a queue from the syncq list
 * Assumes SQLOCK held.
 */
#define	SQRM_Q(sq, qp)							\
	{								\
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				\
		ASSERT(qp->q_sqflags & Q_SQQUEUED);			\
		ASSERT(sq->sq_head != NULL && sq->sq_tail != NULL);	\
		ASSERT((sq->sq_flags & SQ_MESSAGES) != 0);		\
		/* Check that the queue is actually in the list */	\
		ASSERT(qp->q_sqnext != NULL || sq->sq_tail == qp);	\
		ASSERT(qp->q_sqprev != NULL || sq->sq_head == qp);	\
		ASSERT(sq->sq_nqueues != 0);				\
		if (qp->q_sqprev == NULL) {				\
			/* First queue on list, make head q_sqnext */	\
			sq->sq_head = qp->q_sqnext;			\
		} else {						\
			/* Make prev->next == next */			\
			qp->q_sqprev->q_sqnext = qp->q_sqnext;		\
		}							\
		if (qp->q_sqnext == NULL) {				\
			/* Last queue on list, make tail sqprev */	\
			sq->sq_tail = qp->q_sqprev;			\
		} else {						\
			/* Make next->prev == prev */			\
			qp->q_sqnext->q_sqprev = qp->q_sqprev;		\
		}							\
		/* clear out references on this queue */		\
		qp->q_sqprev = qp->q_sqnext = NULL;			\
		qp->q_sqflags &= ~Q_SQQUEUED;				\
		/* If there is nothing queued, clear SQ_MESSAGES */	\
		if (sq->sq_head != NULL) {				\
			sq->sq_pri = sq->sq_head->q_spri;		\
		} else	{						\
			sq->sq_flags &= ~SQ_MESSAGES;			\
			sq->sq_pri = 0;					\
		}							\
		sq->sq_nqueues--;					\
		ASSERT(sq->sq_head != NULL || sq->sq_evhead != NULL ||	\
		    (sq->sq_flags & SQ_QUEUED) == 0);			\
	}

/* Hide the definition from the header file. */
#ifdef SQPUT_MP
#undef SQPUT_MP
#endif

/*
 * Put a message on the queue syncq.
 * Assumes QLOCK held.
 */
#define	SQPUT_MP(qp, mp)						\
	{								\
		ASSERT(MUTEX_HELD(QLOCK(qp)));				\
		ASSERT(qp->q_sqhead == NULL ||				\
		    (qp->q_sqtail != NULL &&				\
		    qp->q_sqtail->b_next == NULL));			\
		qp->q_syncqmsgs++;					\
		ASSERT(qp->q_syncqmsgs != 0);	/* Wraparound */	\
		if (qp->q_sqhead == NULL) {				\
			qp->q_sqhead = qp->q_sqtail = mp;		\
		} else {						\
			qp->q_sqtail->b_next = mp;			\
			qp->q_sqtail = mp;				\
		}							\
		ASSERT(qp->q_syncqmsgs > 0);				\
	}

/*
 * constructor/destructor routines for the stream head cache
 */
/* ARGSUSED */
static int
stream_head_constructor(void *buf, void *cdrarg, int kmflags)
{
	stdata_t *stp = buf;

	mutex_init(&stp->sd_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&stp->sd_reflock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&stp->sd_monitor, NULL, CV_DEFAULT, NULL);
	cv_init(&stp->sd_iocmonitor, NULL, CV_DEFAULT, NULL);
	stp->sd_wrq = NULL;

	mutex_enter(&strresources);
	stp->sd_next = stream_head_list;
	stp->sd_prev = NULL;
	stp->sd_kcp = NULL;
	stp->sd_anchor = 0;
	if (stp->sd_next)
		stp->sd_next->sd_prev = stp;
	stream_head_list = stp;
	mutex_exit(&strresources);

	return (0);
}

/* ARGSUSED */
static void
stream_head_destructor(void *buf, void *cdrarg)
{
	stdata_t *stp = buf;

	ASSERT(stp->sd_kcp == NULL);
	mutex_enter(&strresources);
	if (stp->sd_next)
		stp->sd_next->sd_prev = stp->sd_prev;
	if (stp->sd_prev)
		stp->sd_prev->sd_next = stp->sd_next;
	else
		stream_head_list = stp->sd_next;
	mutex_exit(&strresources);

	mutex_destroy(&stp->sd_lock);
	mutex_destroy(&stp->sd_reflock);
	cv_destroy(&stp->sd_monitor);
	cv_destroy(&stp->sd_iocmonitor);
}

/*
 * constructor/destructor routines for the queue cache
 */
/* ARGSUSED */
static int
queue_constructor(void *buf, void *cdrarg, int kmflags)
{
	queinfo_t *qip = buf;
	queue_t *qp = &qip->qu_rqueue;
	queue_t *wqp = &qip->qu_wqueue;
	syncq_t	*sq = &qip->qu_syncq;

	qp->q_first = NULL;
	qp->q_link = NULL;
	qp->q_count = 0;
	qp->q_mblkcnt = 0;
	qp->q_sqhead = NULL;
	qp->q_sqtail = NULL;
	qp->q_sqnext = NULL;
	qp->q_sqprev = NULL;
	qp->q_sqflags = 0;
	qp->q_refcnt = 0;
	qp->q_spri = 0;

	mutex_init(QLOCK(qp), NULL, MUTEX_DEFAULT, NULL);
	cv_init(&qp->q_wait, NULL, CV_DEFAULT, NULL);
	cv_init(&qp->q_sync, NULL, CV_DEFAULT, NULL);

	wqp->q_first = NULL;
	wqp->q_link = NULL;
	wqp->q_count = 0;
	wqp->q_mblkcnt = 0;
	wqp->q_sqhead = NULL;
	wqp->q_sqtail = NULL;
	wqp->q_sqnext = NULL;
	wqp->q_sqprev = NULL;
	wqp->q_sqflags = 0;
	wqp->q_refcnt = 0;
	wqp->q_spri = 0;

	mutex_init(QLOCK(wqp), NULL, MUTEX_DEFAULT, NULL);
	cv_init(&wqp->q_wait, NULL, CV_DEFAULT, NULL);
	cv_init(&wqp->q_sync, NULL, CV_DEFAULT, NULL);

	sq->sq_head = NULL;
	sq->sq_tail = NULL;
	sq->sq_evhead = NULL;
	sq->sq_evtail = NULL;
	sq->sq_callbpend = NULL;
	sq->sq_outer = NULL;
	sq->sq_onext = NULL;
	sq->sq_oprev = NULL;
	sq->sq_next = NULL;
	sq->sq_svcflags = 0;
	sq->sq_needexcl = 0;
	sq->sq_nqueues = 0;
	sq->sq_pri = 0;

	mutex_init(&sq->sq_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sq->sq_wait, NULL, CV_DEFAULT, NULL);
	cv_init(&sq->sq_exitwait, NULL, CV_DEFAULT, NULL);

	return (0);
}

/* ARGSUSED */
static void
queue_destructor(void *buf, void *cdrarg)
{
	queinfo_t *qip = buf;
	queue_t *qp = &qip->qu_rqueue;
	queue_t *wqp = &qip->qu_wqueue;
	syncq_t	*sq = &qip->qu_syncq;

	ASSERT(qp->q_sqhead == NULL);
	ASSERT(wqp->q_sqhead == NULL);
	ASSERT(qp->q_sqnext == NULL);
	ASSERT(wqp->q_sqnext == NULL);

	mutex_destroy(&qp->q_lock);
	cv_destroy(&qp->q_wait);
	cv_destroy(&qp->q_sync);

	mutex_destroy(&wqp->q_lock);
	cv_destroy(&wqp->q_wait);
	cv_destroy(&wqp->q_sync);

	mutex_destroy(&sq->sq_lock);
	cv_destroy(&sq->sq_wait);
	cv_destroy(&sq->sq_exitwait);
}

/*
 * constructor/destructor routines for the syncq cache
 */
/* ARGSUSED */
static int
syncq_constructor(void *buf, void *cdrarg, int kmflags)
{
	syncq_t	*sq = buf;

	sq->sq_head = NULL;
	sq->sq_tail = NULL;
	sq->sq_evhead = NULL;
	sq->sq_evtail = NULL;
	sq->sq_callbpend = NULL;
	sq->sq_outer = NULL;
	sq->sq_onext = NULL;
	sq->sq_oprev = NULL;
	sq->sq_next = NULL;
	sq->sq_needexcl = 0;
	sq->sq_svcflags = 0;
	sq->sq_nqueues = 0;
	sq->sq_pri = 0;

	mutex_init(&sq->sq_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&sq->sq_wait, NULL, CV_DEFAULT, NULL);
	cv_init(&sq->sq_exitwait, NULL, CV_DEFAULT, NULL);

	return (0);
}

/* ARGSUSED */
static void
syncq_destructor(void *buf, void *cdrarg)
{
	syncq_t	*sq = buf;

	ASSERT(sq->sq_head == NULL && sq->sq_tail == NULL);
	ASSERT(sq->sq_callbpend == NULL);
	ASSERT(sq->sq_outer == NULL);
	ASSERT(sq->sq_onext == NULL && sq->sq_oprev == NULL);
	ASSERT(sq->sq_evhead == NULL);

	mutex_destroy(&sq->sq_lock);
	cv_destroy(&sq->sq_wait);
	cv_destroy(&sq->sq_exitwait);
}

/* ARGSUSED */
static int
ciputctrl_constructor(void *buf, void *cdrarg, int kmflags)
{
	ciputctrl_t *cip = buf;
	int i;

	for (i = 0; i < n_ciputctrl; i++) {
		cip[i].ciputctrl_count = 0;
		mutex_init(&cip[i].ciputctrl_lock, NULL, MUTEX_DEFAULT, NULL);
	}

	return (0);
}

/* ARGSUSED */
static void
ciputctrl_destructor(void *buf, void *cdrarg)
{
	ciputctrl_t *cip = buf;
	int i;

	for (i = 0; i < n_ciputctrl; i++)
		mutex_destroy(&cip[i].ciputctrl_lock);
}

/*
 * Initialize syncq service lists
*/
static void
sqservinit(void)
{
	int i;
	short quantum;
	pri_t bucket_pri = kpreemptpri - 1;

	sqservers = kmem_zalloc(sizeof (sqserv_t) * nsqservers, KM_SLEEP);
	quantum = (SHRT_MAX - bucket_pri) / (nsqservers - 1);

	for (i = 0; i < nsqservers; i++, bucket_pri += quantum) {
		pri_t pri = bucket_pri < kpreemptpri ?
		    60 :
		    kpreemptpri;

		sqserv_t *servlist = &(sqservers[i]);

		mutex_init(&servlist->sqserv_lock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&servlist->sqserv_cv, NULL, CV_DEFAULT, NULL);

		servlist->sqserv_pri = bucket_pri;
		servlist->sqserv_threadpri = pri;
		servlist->sqserv_nthreads = max_ncpus;
		servlist->sqserv_threads =
		    kmem_alloc(sizeof (kthread_id_t) * max_ncpus, KM_SLEEP);
	}
}

/*
 * Init routine run from main at boot time.
 */
void
strinit(void)
{
	int i;

	/*
	 * Set up mux_node structures.
	 */
	mux_nodes = kmem_zalloc((sizeof (struct mux_node) * devcnt), KM_SLEEP);
	for (i = 0; i < devcnt; i++)
		mux_nodes[i].mn_imaj = i;

	perdev_syncq = kmem_zalloc(devcnt * sizeof (perdm_t), KM_SLEEP);
	permod_syncq = kmem_zalloc(fmodcnt * sizeof (perdm_t), KM_SLEEP);

	stream_head_cache = kmem_cache_create("stream_head_cache",
		sizeof (stdata_t), 0,
		stream_head_constructor, stream_head_destructor, NULL,
		NULL, NULL, 0);

	queue_cache = kmem_cache_create("queue_cache", sizeof (queinfo_t), 0,
		queue_constructor, queue_destructor, NULL, NULL, NULL, 0);

	syncq_cache = kmem_cache_create("syncq_cache", sizeof (syncq_t), 0,
		syncq_constructor, syncq_destructor, NULL, NULL, NULL, 0);

	qband_cache = kmem_cache_create("qband_cache",
		sizeof (qband_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	linkinfo_cache = kmem_cache_create("linkinfo_cache",
		sizeof (linkinfo_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	/*
	 * For now strsig_cache, bufcall_cache, and callbparams_cache all
	 * just point to the same cache, since they're all lightly used
	 * and vaguely related. To change this, just do separate calls
	 * to kmem_cache_create().
	 */
	strsig_cache = bufcall_cache = callbparams_cache =
		kmem_cache_create("strevent_cache",
		sizeof (strevent_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	if (boot_max_ncpus != -1)
		n_ciputctrl = boot_max_ncpus;
	else
		n_ciputctrl = max_ncpus;
	n_ciputctrl = 1 << highbit(n_ciputctrl - 1);
	ASSERT(n_ciputctrl >= 1);
	n_ciputctrl = MIN(n_ciputctrl, max_n_ciputctrl);
	if (n_ciputctrl >= min_n_ciputctrl) {
		ciputctrl_cache = kmem_cache_create("ciputctrl_cache",
			sizeof (ciputctrl_t) * n_ciputctrl,
			sizeof (ciputctrl_t), ciputctrl_constructor,
			ciputctrl_destructor, NULL, NULL, NULL, 0);
	}
	sqservinit();
	/*
	 * since ncpus at this point is one, this will create
	 * one cpu worth of service threads for now. If there
	 * are more cpus, mp_strinit() from main will create more.
	 */
	mp_strinit();

	/*
	 * The following is statistics for zero-copy.
	 */
	zckstat_create();

	/*
	 * TPI support routine initialisation.
	 */
	tpi_init();
}

void
str_sendsig(vnode_t *vp, int event, uchar_t band, int errno)
{
	struct stdata *stp;

	ASSERT(vp->v_stream);
	stp = vp->v_stream;
	/* Have to hold sd_lock to prevent siglist from changing */
	mutex_enter(&stp->sd_lock);
	if (stp->sd_sigflags & event)
		strsendsig(stp->sd_siglist, event, band, errno);
	mutex_exit(&stp->sd_lock);
}

/*
 * Send the "sevent" set of signals to a process.
 * This might send more than one signal if the process is registered
 * for multiple events. The caller should pass in an sevent that only
 * includes the events for which the process has registered.
 */
static void
dosendsig(proc_t *proc, int events, int sevent, k_siginfo_t *info,
	uchar_t band, int errno)
{
	ASSERT(MUTEX_HELD(&proc->p_lock));

	info->si_band = 0;
	info->si_errno = 0;

	if (sevent & S_ERROR) {
		sevent &= ~S_ERROR;
		info->si_code = POLL_ERR;
		info->si_errno = errno;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
		info->si_errno = 0;
	}
	if (sevent & S_HANGUP) {
		sevent &= ~S_HANGUP;
		info->si_code = POLL_HUP;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
	}
	if (sevent & S_HIPRI) {
		sevent &= ~S_HIPRI;
		info->si_code = POLL_PRI;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
	}
	if (sevent & S_RDBAND) {
		sevent &= ~S_RDBAND;
		if (events & S_BANDURG)
			sigtoproc(proc, NULL, SIGURG);
		else
			sigtoproc(proc, NULL, SIGPOLL);
	}
	if (sevent & S_WRBAND) {
		sevent &= ~S_WRBAND;
		sigtoproc(proc, NULL, SIGPOLL);
	}
	if (sevent & S_INPUT) {
		sevent &= ~S_INPUT;
		info->si_code = POLL_IN;
		info->si_band = band;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
		info->si_band = 0;
	}
	if (sevent & S_OUTPUT) {
		sevent &= ~S_OUTPUT;
		info->si_code = POLL_OUT;
		info->si_band = band;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
		info->si_band = 0;
	}
	if (sevent & S_MSG) {
		sevent &= ~S_MSG;
		info->si_code = POLL_MSG;
		info->si_band = band;
		TRACE_3(TR_FAC_STREAMS_FR, TR_STRSENDSIG,
			"strsendsig:addq %X code %X band %X",
			proc, info->si_code, info->si_band);
		sigaddq(proc, NULL, info, KM_NOSLEEP);
		info->si_band = 0;
	}
	if (sevent & S_RDNORM) {
		sevent &= ~S_RDNORM;
		sigtoproc(proc, NULL, SIGPOLL);
	}
	if (sevent != 0) {
		cmn_err(CE_PANIC,
			"strsendsig: unknown event(s) %x\n",
			sevent);
	}
}

/*
 * Send SIGPOLL/SIGURG signal to all processes and process groups
 * registered on the given signal list that want a signal for at
 * least one of the specified events.
 *
 * Must be called with exclusive access to siglist (caller holding sd_lock).
 *
 * strioctl(I_SETSIG/I_ESETSIG) will only change siglist when holding
 * sd_lock and the ioctl code maintains a PID_HOLD on the pid structure
 * while it is in the siglist.
 *
 * For performance reasons (MP scalability) the code drops pidlock
 * when sending signals to a single process.
 * When sending to a process group the code holds
 * pidlock to prevent the membership in the process group from changing
 * while walking the p_pglink list.
 */
void
strsendsig(strsig_t *siglist, int event, uchar_t band, int errno)
{
	strsig_t *ssp;
	k_siginfo_t info;
	struct pid *pidp;
	proc_t  *proc;

	info.si_signo = SIGPOLL;
	info.si_errno = 0;
	for (ssp = siglist; ssp; ssp = ssp->ss_next) {
		int sevent;

		sevent = ssp->ss_events & event;
		if (sevent == 0)
			continue;

		if ((pidp = ssp->ss_pidp) == NULL) {
			/* pid was released but still on event list */
			continue;
		}


		if (ssp->ss_pid > 0) {
			/*
			 * XXX This unfortunately still generates
			 * a signal when a fd is closed but
			 * the proc is active.
			 */
			ASSERT(ssp->ss_pid == pidp->pid_id);

			mutex_enter(&pidlock);
			proc = prfind(pidp->pid_id);
			if (proc == NULL) {
				mutex_exit(&pidlock);
				continue;
			}
			mutex_enter(&proc->p_lock);
			mutex_exit(&pidlock);
			dosendsig(proc, ssp->ss_events, sevent, &info,
				band, errno);
			mutex_exit(&proc->p_lock);
		} else {
			/*
			 * Send to process group. Hold pidlock across
			 * calls to dosendsig().
			 */
			pid_t pgrp = -ssp->ss_pid;

			mutex_enter(&pidlock);
			proc = pgfind(pgrp);
			while (proc != NULL) {
				mutex_enter(&proc->p_lock);
				dosendsig(proc, ssp->ss_events, sevent,
					&info, band, errno);
				mutex_exit(&proc->p_lock);
				proc = proc->p_pglink;
			}
			mutex_exit(&pidlock);
		}
	}
}

/*
 * Attach a stream device or module.
 * qp is a read queue; the new queue goes in so its next
 * read ptr is the argument, and the write queue corresponding
 * to the argument points to this queue. Return 0 on success,
 * or a non-zero errno on failure.
 * sflag should be either 0 or CLONEOPEN.
 */
int
qattach(queue_t *qp, dev_t *devp, int flag, int sflg, int table, int idx,
	cred_t *crp, boolean_t is_insert)
{
	queue_t *rq, *wrq;
	struct streamtab *qinfop;
	perdm_t *dmp;
	int error = 0;
	uint32_t oqflag, qflag, sqtype;
	stdata_t *stp = STREAM(qp);

	rq = allocq();
	wrq = _WR(rq);
	if (table == CDEVSW) {
		oqflag = qflag = devopsp[idx]->devo_cb_ops->cb_flag;
		qinfop = STREAMSTAB(idx);
		dmp = &perdev_syncq[idx];
		error = devflg_to_qflag(qinfop, qflag, &qflag, &sqtype);
		qflag |= QISDRV;
		ASSERT((sflg & ~CLONEOPEN) == 0);
	} else {
		ASSERT(table == FMODSW);
		oqflag = qflag = fmodsw[idx].f_flag;
		qinfop = fmodsw[idx].f_str;
		dmp = &permod_syncq[idx];

		/*
		 * Note: To keep the number of OPEN/CLOSES straight, a
		 * module's f_count is incremented during pushes and
		 * decremented during pops. The only way to access
		 * the f_count field given a queue is to use the modname
		 * in streamtabs. So, the module name in streamtabs and
		 * fmodsw *MUST* be the same.
		 */
#ifdef DEBUG
		if (strcmp(fmodsw[idx].f_str->st_rdinit->qi_minfo->mi_idname,
		    fmodsw[idx].f_name) != 0) {
			cmn_err(CE_WARN,
			    "qattach: modname mismatch f_name: %s"
			    ", mi_idname : %s",
			    fmodsw[idx].f_name,
			    fmodsw[idx].f_str->st_rdinit->qi_minfo->mi_idname);
		}
#endif
		ASSERT(sflg == 0);
		sflg = MODOPEN;
		error = devflg_to_qflag(qinfop, qflag, &qflag, &sqtype);
	}
	if (error)
		return (error);
	TRACE_2(TR_FAC_STREAMS_FR, TR_QATTACH_FLAGS,
		"qattach:qflag == %X(%X)", qflag, *devp);
	STREAM(rq) = STREAM(wrq) = stp;

	/*
	 * As long as all the modules including the driver
	 * says that it is QNEXTLESS, we call it a QNEXTLESS
	 * stream. If one or modules do not set QNEXTLESS we
	 * turn off STRQNEXTLESS for the stream.
	 *
	 * NOTE : If a module without QNEXTLESS set is pushed
	 * on a STRQNEXTLESS stream and that module is later
	 * popped, the STRQNEXTLESS is NOT restored since
	 * it is merely an optimization for insertq and removeq.
	 */
	if (qflag & QISDRV) {
		/* Driver is being attached */
		if (oqflag & _D_QNEXTLESS)
			stp->sd_flag |= STRQNEXTLESS;
	} else {
		if ((stp->sd_flag & STRQNEXTLESS) && !(oqflag & _D_QNEXTLESS)) {
			stp->sd_flag &= ~STRQNEXTLESS;
		}
	}

	/* setq might sleep in allocator - avoid holding locks. */
	setq(rq, qinfop->st_rdinit, qinfop->st_wrinit, qinfop, dmp,
		qflag, sqtype, 0);

	/*
	 * Before calling the module's open routine, set up the q_next
	 * pointer for inserting a module in the middle of a stream.
	 *
	 * Note that we can always set _QINSERTING and set up q_next
	 * pointer for both inserting and pushing a module.  Then there
	 * is no need for the is_insert parameter.  In insertq(), called
	 * by qprocson(), assume that q_next of the new module always points
	 * to the corect queue and use it for insertion.  Everything should
	 * work out fine.  But in the first release of _I_INSERT, we
	 * distinguish between inserting and pushing to make sure that
	 * pushing a module follows the same code path as before.
	 */
	if (is_insert) {
		rq->q_flag |= _QINSERTING;
		rq->q_next = qp;
	}

	/*
	 * If there is an outer perimeter get exclusive access during
	 * the open procedure.  Bump up the reference count on the queue.
	 */
	entersq(rq->q_syncq, SQ_OPENCLOSE);

	if (error = (*rq->q_qinfo->qi_qopen)(rq, devp, flag, sflg, crp)) {
		leavesq(rq->q_syncq, SQ_OPENCLOSE);

		if (is_insert) {
			rq->q_flag &= ~_QINSERTING;
		}
		if (backq(wrq))
			if (backq(wrq)->q_next == wrq)
				qprocsoff(rq);
		rq->q_next = wrq->q_next = NULL;
		qdetach(rq, 0, 0, crp, B_FALSE);
		return (error);
	}
	leavesq(rq->q_syncq, SQ_OPENCLOSE);
	ASSERT(qprocsareon(rq));
	return (0);
}

/*
 * Handle second open of stream. For modules, set the
 * last argument to MODOPEN and do not pass any open flags.
 * Ignore dummydev since this is not the first open.
 */
int
qreopen(queue_t *qp, dev_t *devp, int flag, cred_t *crp)
{
	int	error;
	dev_t dummydev;
	queue_t *wqp = _WR(qp);

	ASSERT(qp->q_flag & QREADR);
	entersq(qp->q_syncq, SQ_OPENCLOSE);

	dummydev = *devp;
	if (error = ((*qp->q_qinfo->qi_qopen)(qp, &dummydev,
	    (wqp->q_next ? 0 : flag), (wqp->q_next ? MODOPEN : 0), crp))) {
		leavesq(qp->q_syncq, SQ_OPENCLOSE);
		mutex_enter(&STREAM(qp)->sd_lock);
		qp->q_stream->sd_flag |= STREOPENFAIL;
		mutex_exit(&STREAM(qp)->sd_lock);
		return (error);
	}
	leavesq(qp->q_syncq, SQ_OPENCLOSE);

	/*
	 * successful open should have done qprocson()
	 */
	ASSERT(qprocsareon(_RD(qp)));
	return (0);
}

/*
 * Detach a stream module or device.
 * If clmode == 1 then the module or driver was opened and its
 * close routine must be called. If clmode == 0, the module
 * or driver was never opened or the open failed, and so its close
 * should not be called.
 */
void
qdetach(queue_t *qp, int clmode, int flag, cred_t *crp, boolean_t is_remove)
{
	queue_t *wqp = _WR(qp);
	ASSERT(STREAM(qp)->sd_flag & (STRCLOSE|STWOPEN|STRPLUMB));

	if (qready())
		queuerun();

	if (clmode) {
		entersq(qp->q_syncq, SQ_OPENCLOSE);
		if (is_remove) {
			qp->q_flag |= _QREMOVING;
		}
		(*qp->q_qinfo->qi_qclose)(qp, flag, crp);
		leavesq(qp->q_syncq, SQ_OPENCLOSE);
	}

	/*
	 * remove it from the service queue, if it is there.
	 */
	remove_runlist(qp);

	/*
	 * Allow any threads blocked in entersq to proceed and discover
	 * the QWCLOSE is set.
	 * Note: This assumes that all users of entersq check QWCLOSE.
	 * Currently runservice is the only entersq that can happen
	 * after removeq has finished.
	 * Removeq will have discarded all messages destined to the closing
	 * pair of queues from the syncq.
	 * NOTE: Calling a function inside an assert is unconventional.
	 * However, it does not cause any problem since flush_syncq() does
	 * not change any state except when it returns non-zero i.e.
	 * when the assert will trigger.
	 */
	ASSERT(flush_syncq(qp->q_syncq, qp) == 0);
	ASSERT(flush_syncq(wqp->q_syncq, wqp) == 0);
	ASSERT((qp->q_flag & QPERMOD) ||
		((qp->q_syncq->sq_head == NULL) &&
		(wqp->q_syncq->sq_head == NULL)));

	/*
	 * Flush the queues before q_next is set to NULL. This is needed
	 * in order to backenable any downstream queue before we go away.
	 * Note: we are already removed from the stream so that the
	 * backenabling will not cause any messages to be delivered to our
	 * put procedures.
	 */
	flushq(qp, FLUSHALL);
	flushq(_WR(qp), FLUSHALL);

	/*
	 * wait for any pending service processing to complete
	 */
	wait_svc(qp);

	/* Tidy up - removeq only does a half-remove from stream */
	qp->q_next = wqp->q_next = NULL;
	ASSERT(!(qp->q_flag & QENAB));
	ASSERT(!(wqp->q_flag & QENAB));

	/* freeq removes us from the outer perimeter if any */
	freeq(qp);
}

/* Prevent service procedures from being called */
void
disable_svc(queue_t *qp)
{
	queue_t *wqp = _WR(qp);

	ASSERT(qp->q_flag & QREADR);
	mutex_enter(QLOCK(qp));
	qp->q_flag |= QWCLOSE;
	mutex_exit(QLOCK(qp));
	mutex_enter(QLOCK(wqp));
	wqp->q_flag |= QWCLOSE;
	mutex_exit(QLOCK(wqp));
}

/* allow service procedures to be called again */
void
enable_svc(queue_t *qp)
{
	queue_t *wqp = _WR(qp);

	ASSERT(qp->q_flag & QREADR);
	mutex_enter(QLOCK(qp));
	qp->q_flag &= ~QWCLOSE;
	mutex_exit(QLOCK(qp));
	mutex_enter(QLOCK(wqp));
	wqp->q_flag &= ~QWCLOSE;
	mutex_exit(QLOCK(wqp));
}

/*
 * Remove queues from runlist if they are enabled.
 * Only reset QENAB if the queue was removed from the runlist.
 * A queue goes through 3 stages:
 *	It is on the service list and QENAB is set.
 *	It is removed from the service list but QENAB is still set.
 *	QENAB gets changed to QINSERVICE.
 *	QINSERVICE is reset (when the service procedure is done)
 * Thus we can not reset QENAB unless we actually removed it from the service
 * queue.
 */
void
remove_runlist(queue_t *qp)
{
	queue_t *wqp = _WR(qp);
	int did_rq = 0, did_wq = 0;
	ASSERT(qp->q_flag & QREADR);

	mutex_enter(&service_queue);
	if (qp->q_flag & QENAB)
		did_rq = rmv_qp(&qhead, &qtail, qp);
	if (wqp->q_flag & QENAB)
		did_wq = rmv_qp(&qhead, &qtail, wqp);
	mutex_exit(&service_queue);

	if (did_rq) {
		mutex_enter(QLOCK(qp));
		qp->q_flag &= ~QENAB;
		mutex_exit(QLOCK(qp));
	}
	if (did_wq) {
		mutex_enter(QLOCK(wqp));
		wqp->q_flag &= ~QENAB;
		mutex_exit(QLOCK(wqp));
	}
}

/*
 * wait for any pending service processing to complete.
 * The removal of queues from the runlist is not atomic with the
 * clearing of the QENABLED flag and setting the INSERVICE flag.
 * consequently it is possible for remove_runlist in strclose
 * to not find the queue on the runlist but for it to be QENABLED
 * and not yet INSERVICE -> hence wait_svc needs to check QENABLED
 * as well as INSERVICE.
 */
void
wait_svc(queue_t *qp)
{
	queue_t *wqp = _WR(qp);

	ASSERT(qp->q_flag & QREADR);

	/*
	 * Wait till this queue will dissapear from its syncq list.
	 */
	wait_q_syncq(qp);

	mutex_enter(QLOCK(qp));
	while (qp->q_flag & (QINSERVICE|QENAB))
		cv_wait(&qp->q_wait, QLOCK(qp));
	mutex_exit(QLOCK(qp));
	mutex_enter(QLOCK(wqp));
	while (wqp->q_flag & (QINSERVICE|QENAB))
		cv_wait(&wqp->q_wait, QLOCK(wqp));
	mutex_exit(QLOCK(wqp));
}

/*
 * Put ioctl data from user land to ioctl buffers. Return non-zero
 * errno for failure, 1 for success.
 * flag must be K_TO_K or U_TO_K. In addition, STR_NOSIG is also
 * supported; i.e. allocb_wait() will not give up if a signal
 * arrives.
 */
int
putiocd(mblk_t *bp, mblk_t *ebp, char *arg, int flag, char *fmt)
{
	mblk_t *tmp;
	ssize_t  count;
	size_t n;
	mblk_t *obp = bp;
	int error = 0;

	ASSERT((flag & (U_TO_K | K_TO_K)) == U_TO_K ||
		(flag & (U_TO_K | K_TO_K)) == K_TO_K);

	if (bp->b_datap->db_type == M_IOCTL)
		count = ((struct iocblk *)bp->b_rptr)->ioc_count;
	else {
		ASSERT(bp->b_datap->db_type == M_COPYIN);
		count = ((struct copyreq *)bp->b_rptr)->cq_size;
	}
	/*
	 * strdoioctl validates ioc_count, so if this assert fails it
	 * cannot be due to user error.
	 */
	ASSERT(count >= 0);

	while (count) {
		n = MIN(MAXIOCBSZ, count);
		if (!(tmp = allocb_wait(n, BPRI_HI, (flag & STR_NOSIG),
		    &error))) {
			return (error);
		}
		error = strcopyin((caddr_t)arg, tmp->b_wptr, n,
				fmt, flag & (U_TO_K | K_TO_K));
		if (error) {
			freeb(tmp);
			return (error);
		}
		if (fmt && (count > MAXIOCBSZ) && (flag & U_TO_K))
			adjfmtp(&fmt, tmp, n);
		arg += n;
		tmp->b_datap->db_type = M_DATA;
		if (CRED() != NULL) {
			tmp->b_datap->db_uid = CRED()->cr_uid;
		}
		tmp->b_wptr += n;
		count -= n;
		bp = (bp->b_cont = tmp);
	}

	/*
	 * If ebp was supplied, place it between the
	 * M_IOCTL block and the (optional) M_DATA blocks.
	 */
	if (ebp) {
		ebp->b_cont = obp->b_cont;
		obp->b_cont = ebp;
	}
	return (0);
}

/*
 * Copy ioctl data to user-land. Return non-zero errno on failure,
 * 0 for success.
 */
int
getiocd(mblk_t *bp, char *arg, int copymode, char *fmt)
{
	ssize_t count;
	size_t  n;
	int	error;

	if (bp->b_datap->db_type == M_IOCACK)
		count = ((struct iocblk *)bp->b_rptr)->ioc_count;
	else {
		ASSERT(bp->b_datap->db_type == M_COPYOUT);
		count = ((struct copyreq *)bp->b_rptr)->cq_size;
	}
	ASSERT(count >= 0);

	for (bp = bp->b_cont; bp && count;
	    count -= n, bp = bp->b_cont, arg += n) {
		n = MIN(count, bp->b_wptr - bp->b_rptr);
		error = strcopyout((caddr_t)bp->b_rptr, arg, n, fmt, copymode);
		if (error)
			return (error);
		if (fmt && bp->b_cont && (copymode == U_TO_K))
			adjfmtp(&fmt, bp, n);
	}
	ASSERT(count == 0);
	return (0);
}

/*
 * Allocate a linkinfo table entry given the write queue of the
 * bottom module of the top stream and the write queue of the
 * stream head of the bottom stream.
 *
 * linkinfo table entries are freed by nulling the li_lblk.l_qbot field.
 */
linkinfo_t *
alloclink(queue_t *qup, queue_t *qdown, file_t *fpdown)
{
	linkinfo_t *linkp;

	linkp = kmem_cache_alloc(linkinfo_cache, KM_SLEEP);

	linkp->li_lblk.l_qtop = qup;
	linkp->li_lblk.l_qbot = qdown;
	linkp->li_fpdown = fpdown;

	mutex_enter(&strresources);
	linkp->li_next = linkinfo_list;
	linkp->li_prev = NULL;
	if (linkp->li_next)
		linkp->li_next->li_prev = linkp;
	linkinfo_list = linkp;
	linkp->li_lblk.l_index = ++lnk_id;
	ASSERT(lnk_id != 0);	/* this should never wrap in practice */
	mutex_exit(&strresources);

	return (linkp);
}

/*
 * Free a linkinfo entry.
 */
void
lbfree(linkinfo_t *linkp)
{
	mutex_enter(&strresources);
	if (linkp->li_next)
		linkp->li_next->li_prev = linkp->li_prev;
	if (linkp->li_prev)
		linkp->li_prev->li_next = linkp->li_next;
	else
		linkinfo_list = linkp->li_next;
	mutex_exit(&strresources);

	kmem_cache_free(linkinfo_cache, linkp);
}

/*
 * Check for a potential linking cycle.
 * Return 1 if a link will result in a cycle,
 * and 0 otherwise.
 */
int
linkcycle(stdata_t *upstp, stdata_t *lostp)
{
	struct mux_node *np;
	struct mux_edge *ep;
	int i;
	major_t lomaj;
	major_t upmaj;
	/*
	 * if the lower stream is a pipe/FIFO, return, since link
	 * cycles can not happen on pipes/FIFOs
	 */
	if (lostp->sd_vnode->v_type == VFIFO)
		return (0);

	for (i = 0; i < devcnt; i++) {
		np = &mux_nodes[i];
		MUX_CLEAR(np);
	}
	lomaj = getmajor(lostp->sd_vnode->v_rdev);
	upmaj = getmajor(upstp->sd_vnode->v_rdev);
	np = &mux_nodes[lomaj];
	for (;;) {
		if (!MUX_DIDVISIT(np)) {
			if (np->mn_imaj == upmaj)
				return (1);
			if (np->mn_outp == NULL) {
				MUX_VISIT(np);
				if (np->mn_originp == NULL)
					return (0);
				np = np->mn_originp;
				continue;
			}
			MUX_VISIT(np);
			np->mn_startp = np->mn_outp;
		} else {
			if (np->mn_startp == NULL) {
				if (np->mn_originp == NULL)
					return (0);
				else {
					np = np->mn_originp;
					continue;
				}
			}
			ep = np->mn_startp;
			np->mn_startp = ep->me_nextp;
			ep->me_nodep->mn_originp = np;
			np = ep->me_nodep;
		}
	}
}

/*
 * Find linkinfo table entry corresponding to the parameters.
 */
linkinfo_t *
findlinks(stdata_t *stp, int index, int type)
{
	linkinfo_t *linkp;
	struct mux_edge *mep;
	struct mux_node *mnp;
	queue_t *qup;

	mutex_enter(&strresources);
	if ((type & LINKTYPEMASK) == LINKNORMAL) {
		qup = getendq(stp->sd_wrq);
		for (linkp = linkinfo_list; linkp; linkp = linkp->li_next) {
			if ((qup == linkp->li_lblk.l_qtop) &&
			    (!index || (index == linkp->li_lblk.l_index))) {
				mutex_exit(&strresources);
				return (linkp);
			}
		}
	} else {
		ASSERT((type & LINKTYPEMASK) == LINKPERSIST);
		mnp = &mux_nodes[getmajor(stp->sd_vnode->v_rdev)];
		mep = mnp->mn_outp;
		while (mep) {
			if ((index == 0) || (index == mep->me_muxid))
				break;
			mep = mep->me_nextp;
		}
		if (!mep) {
			mutex_exit(&strresources);
			return (NULL);
		}
		for (linkp = linkinfo_list; linkp; linkp = linkp->li_next) {
			if ((!linkp->li_lblk.l_qtop) &&
			    (mep->me_muxid == linkp->li_lblk.l_index)) {
				mutex_exit(&strresources);
				return (linkp);
			}
		}
	}
	mutex_exit(&strresources);
	return (NULL);
}

/*
 * Given a queue ptr, follow the chain of q_next pointers until you reach the
 * last queue on the chain and return it.
 */
queue_t *
getendq(queue_t *q)
{
	ASSERT(q != NULL);
	while (_SAMESTR(q))
		q = q->q_next;
	return (q);
}

/*
 * wait for the syncq count to drop to zero.
 * sq could be either outer or inner.
 */

static void
wait_syncq(syncq_t *sq)
{
	uint16_t count;

	mutex_enter(SQLOCK(sq));
	count = sq->sq_count;
	SQ_PUTLOCKS_ENTER(sq);
	SUM_SQ_PUTCOUNTS(sq, count);
	while (count != 0) {
		sq->sq_flags |= SQ_WANTWAKEUP;
		SQ_PUTLOCKS_EXIT(sq);
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
	}
	SQ_PUTLOCKS_EXIT(sq);
	mutex_exit(SQLOCK(sq));
}

int
mlink(vnode_t *vp, int cmd, int arg, cred_t *crp, int *rvalp)
{
	struct stdata *stp;
	struct file *fpdown;
	struct strioctl strioc;
	struct linkinfo *linkp;
	struct stdata *stpdown;
	queue_t *passq;
	queue_t *rq;
	uint32_t qflag;
	uint32_t sqtype;
	perdm_t *dmp;
	int error = 0;

	stp = vp->v_stream;
	TRACE_1(TR_FAC_STREAMS_FR,
		TR_I_LINK, "I_LINK/I_PLINK:stp %X", stp);
	/*
	 * Test for invalid upper stream
	 */
	if (stp->sd_flag & STRHUP) {
		return (ENXIO);
	}
	if (vp->v_type == VFIFO) {
		return (EINVAL);
	}
	if (stp->sd_strtab == NULL) {
		return (EINVAL);
	}
	if (!stp->sd_strtab->st_muxwinit) {
		return (EINVAL);
	}
	if ((fpdown = getf(arg)) == NULL) {
		return (EBADF);
	}
	mutex_enter(&muxifier);
	if (stp->sd_flag & STPLEX) {
		mutex_exit(&muxifier);
		releasef(arg);
		return (ENXIO);
	}

	/*
	 * Test for invalid lower stream.
	 */
	if (((stpdown = fpdown->f_vnode->v_stream) == NULL) ||
	    (stpdown == stp) || (stpdown->sd_flag &
	    (STPLEX|STRHUP|STRDERR|STWRERR|IOCWAIT)) ||
	    linkcycle(stp, stpdown)) {
		mutex_exit(&muxifier);
		releasef(arg);
		return (EINVAL);
	}
	TRACE_1(TR_FAC_STREAMS_FR,
		TR_STPDOWN, "stpdown:%X", stpdown);
	rq = getendq(stp->sd_wrq);
	qflag = rq->q_flag & QMT_TYPEMASK;
	sqtype = rq->q_syncq->sq_type;
	if (cmd == I_PLINK)
		rq = NULL;

	linkp = alloclink(rq, stpdown->sd_wrq, fpdown);

	strioc.ic_cmd = cmd;
	strioc.ic_timout = INFTIM;
	strioc.ic_len = sizeof (struct linkblk);
	strioc.ic_dp = (char *)&linkp->li_lblk;

	/*
	 * Add passthru queue below lower mux. This will block
	 * syncqs of lower muxs read queue during I_LINK/I_UNLINK.
	 */
	passq = link_addpassthru(stpdown);

	/*
	 * STPLEX prevents any threads from entering the stream from
	 * above.
	 */
	mutex_enter(&stpdown->sd_lock);
	stpdown->sd_flag |= STPLEX;
	mutex_exit(&stpdown->sd_lock);

	rq = _RD(stpdown->sd_wrq);
	ASSERT((rq->q_flag & QMT_TYPEMASK) == QMTSAFE);
	ASSERT(rq->q_syncq == SQ(rq) && _WR(rq)->q_syncq == SQ(rq));
	rq->q_ptr = _WR(rq)->q_ptr = NULL;
	/* setq might sleep in allocator - avoid holding locks. */
	/* Note: we are holding muxifier here. */
	dmp = &perdev_syncq[getmajor(vp->v_rdev)];
	setq(rq, stp->sd_strtab->st_muxrinit,
		stp->sd_strtab->st_muxwinit,
		stp->sd_strtab, dmp, qflag, sqtype, 1);

	/*
	 * XXX Remove any "odd" messages from the queue.
	 * Keep only M_DATA, M_PROTO, M_PCPROTO.
	 */
	if (error = strdoioctl(stp, &strioc, NULL, FNATIVE,
	    K_TO_K | STR_NOERROR | STR_NOSIG, STRLINK, crp, rvalp)) {

		lbfree(linkp);

		/*
		 * Restore the stream head queue and then remove
		 * the passq. Turn off STPLEX before we turn on
		 * the stream by removing the passq.
		 */
		rq->q_ptr = _WR(rq)->q_ptr = stpdown;
		setq(rq, &strdata, &stwdata, NULL, NULL,
		    QMTSAFE, SQ_CI|SQ_CO, 1);

		mutex_enter(&stpdown->sd_lock);
		stpdown->sd_flag &= ~STPLEX;
		mutex_exit(&stpdown->sd_lock);

		link_rempassthru(passq);
		mutex_exit(&muxifier);
		releasef(arg);
		return (error);
	}
	mutex_enter(&fpdown->f_tlock);
	fpdown->f_count++;
	mutex_exit(&fpdown->f_tlock);

	link_rempassthru(passq);
	releasef(arg);
	mux_addedge(stp, stpdown, linkp->li_lblk.l_index);

	/*
	 * Mark the upper stream as having dependent links
	 * so that strclose can clean it up.
	 */
	if (cmd == I_LINK) {
		mutex_enter(&stp->sd_lock);
		stp->sd_flag |= STRHASLINKS;
		mutex_exit(&stp->sd_lock);
	}
	/*
	 * Wake up any other processes that may have been
	 * waiting on the lower stream. These will all
	 * error out.
	 */
	mutex_enter(&stpdown->sd_lock);
	cv_broadcast(&rq->q_wait);
	cv_broadcast(&_WR(rq)->q_wait);
	cv_broadcast(&stpdown->sd_monitor);
	mutex_exit(&stpdown->sd_lock);
	mutex_exit(&muxifier);
	*rvalp = linkp->li_lblk.l_index;
	return (0);
}
/*
 * Unlink a multiplexor link. Stp is the controlling stream for the
 * link, and linkp points to the link's entry in the linkinfo table.
 * The muxifier lock must be held on entry and is dropped on exit.
 *
 * NOTE : Currently it is assumed that mux would process all the messages
 * sitting on it's queue before ACKing the UNLINK. It is the responsibility
 * of the mux to handle all the messages that arrive before UNLINK.
 * If the mux has to send down messages on its lower stream before
 * ACKing I_UNLINK, then it *should* know to handle messages even
 * after the UNLINK is acked (actually it should be able to handle till we
 * re-block the read side of the pass queue here). If the mux does not
 * open up the lower stream, any messages that arrive during UNLINK
 * will be put in the stream head. In the case of lower stream opening
 * up, some messages might land in the stream head depending on when
 * the message arrived and when the read side of the pass queue was
 * re-blocked.
 */
int
munlink(stdata_t *stp, linkinfo_t *linkp, int flag, cred_t *crp, int *rvalp)
{
	struct strioctl strioc;
	struct stdata *stpdown;
	queue_t *rq, *wrq;
	queue_t	*passq;
	syncq_t *passyncq;
	int error = 0;

	ASSERT(MUTEX_HELD(&muxifier));

	stpdown = linkp->li_fpdown->f_vnode->v_stream;
	/*
	 * Add passthru queue below lower mux. This will block
	 * syncqs of lower muxs read queue during I_LINK/I_UNLINK.
	 */
	passq = link_addpassthru(stpdown);

	if ((flag & LINKTYPEMASK) == LINKNORMAL)
		strioc.ic_cmd = I_UNLINK;
	else
		strioc.ic_cmd = I_PUNLINK;
	strioc.ic_timout = INFTIM;
	strioc.ic_len = sizeof (struct linkblk);
	strioc.ic_dp = (char *)&linkp->li_lblk;

	error = strdoioctl(stp, &strioc, NULL, FNATIVE,
	    K_TO_K | STR_NOERROR | STR_NOSIG, STRLINK, crp, rvalp);

	/*
	 * If there was an error and this is not called via strclose,
	 * return to the user. Otherwise, pretend there was no error
	 * and close the link.
	 */
	if (error) {
		if (flag & LINKCLOSE) {
			cmn_err(CE_WARN, "KERNEL: munlink: could not perform "
			    "unlink ioctl, closing anyway (%d)\n", error);
		} else {
			link_rempassthru(passq);
			mutex_exit(&muxifier);
			return (error);
		}
	}

	mux_rmvedge(stp, linkp->li_lblk.l_index);
	/*
	 * We go ahead and drop muxifier here--it's a nasty global lock that
	 * can slow others down. It's okay to since attempts to mlink() this
	 * stream will be stopped because STPLEX is still set in the stdata
	 * structure, and munlink() is stopped because mux_rmvedge has
	 * removed a record of this mux.
	 */
	mutex_exit(&muxifier);

	wrq = stpdown->sd_wrq;
	rq = _RD(wrq);

	/*
	 * Get rid of outstanding service procedure runs, before we make
	 * it a stream head, since a stream head doesn't have any service
	 * procedure.
	 */
	disable_svc(rq);
	wait_svc(rq);

	passyncq = passq->q_syncq;
	if (!(passyncq->sq_flags & SQ_BLOCKED)) {

		syncq_t *sq, *outer;


		/*
		 * Messages could be flowing from underneath. We will
		 * block the read side of the passq. This would be
		 * sufficient for QPAIR and QPERQ muxes to ensure
		 * that no data is flowing up into this queue
		 * and hence no thread active in this instance of
		 * lower mux. But for QPERMOD and QMTOUTPERIM there
		 * could be messages on the inner and outer/inner
		 * syncqs respectively. We will wait for them to drain.
		 * Because passq is blocked messages end up in the syncq
		 * And fill_syncq could possibly end up calling set_qfull
		 * which will access the rq->q_flag. Hence, we have to
		 * acquire the QLOCK in setq.
		 *
		 * XXX Messages can also flow from top into this
		 * queue though the unlink is over (Ex. some instance
		 * in putnext() called from top that has still not
		 * accessed this queue. And also putq(lowerq) ???).
		 * Solution : How about blocking the l_qtop queue ?
		 * Do we really care about such pure D_MP muxes ??
		 */

		blocksq(passyncq, SQ_BLOCKED, 0);

		sq = rq->q_syncq;
		if ((outer = sq->sq_outer) != NULL) {

			/*
			 * We have to just wait for the outer sq_count
			 * drop to zero. As this does not prevent new
			 * messages to enter the outer perimeter, this
			 * is subject to starvation.
			 *
			 * NOTE :Because of blocksq above, messages could
			 * be in the inner syncq only because of some
			 * thread holding the outer perimeter exclusively.
			 * Hence it would be sufficient to wait for the
			 * exclusive holder of the outer perimeter to drain
			 * the inner and outer syncqs. But we will not depend
			 * on this feature and hence check the inner syncqs
			 * separately.
			 */
			wait_syncq(outer);
		}


		/*
		 * There could be messages destined for
		 * this queue. Let the exclusive holder
		 * drain it.
		 */

		wait_syncq(sq);
		ASSERT((rq->q_flag & QPERMOD) ||
			((rq->q_syncq->sq_head == NULL) &&
			(_WR(rq)->q_syncq->sq_head == NULL)));
	}

	/*
	 * No body else should know about this queue now.
	 * If the mux did not process the messages before
	 * acking the I_UNLINK, free them now.
	 */

	flushq(rq, FLUSHALL);
	flushq(_WR(rq), FLUSHALL);

	/*
	 * Convert the mux lower queue into a stream head queue.
	 * Turn off STPLEX before we turn on the stream by removing the passq.
	 */
	rq->q_ptr = wrq->q_ptr = stpdown;
	setq(rq, &strdata, &stwdata, NULL, NULL, QMTSAFE, SQ_CI|SQ_CO, 1);

	ASSERT((rq->q_flag & QMT_TYPEMASK) == QMTSAFE);
	ASSERT(rq->q_syncq == SQ(rq) && _WR(rq)->q_syncq == SQ(rq));

	enable_svc(rq);

	mutex_enter(&stpdown->sd_lock);
	stpdown->sd_flag &= ~STPLEX;
	mutex_exit(&stpdown->sd_lock);

	link_rempassthru(passq);

	(void) closef(linkp->li_fpdown);
	lbfree(linkp);
	return (0);
}

/*
 * Unlink all multiplexor links for which stp is the controlling stream.
 * Return 0, or a non-zero errno on failure.
 */
int
munlinkall(stdata_t *stp, int flag, cred_t *crp, int *rvalp)
{
	linkinfo_t *linkp;
	int error = 0;

	mutex_enter(&muxifier);
	while (linkp = findlinks(stp, 0, flag)) {
		/*
		 * munlink() releases the muxifier lock.
		 */
		if (error = munlink(stp, linkp, flag, crp, rvalp))
			return (error);
		mutex_enter(&muxifier);
	}
	mutex_exit(&muxifier);
	return (0);
}

/*
 * A multiplexor link has been made. Add an
 * edge to the directed graph.
 */
void
mux_addedge(stdata_t *upstp, stdata_t *lostp, int muxid)
{
	struct mux_node *np;
	struct mux_edge *ep;
	major_t upmaj;
	major_t lomaj;

	upmaj = getmajor(upstp->sd_vnode->v_rdev);
	lomaj = getmajor(lostp->sd_vnode->v_rdev);
	np = &mux_nodes[upmaj];
	if (np->mn_outp) {
		ep = np->mn_outp;
		while (ep->me_nextp)
			ep = ep->me_nextp;
		ep->me_nextp = kmem_alloc(sizeof (struct mux_edge), KM_SLEEP);
		ep = ep->me_nextp;
	} else {
		np->mn_outp = kmem_alloc(sizeof (struct mux_edge), KM_SLEEP);
		ep = np->mn_outp;
	}
	ep->me_nextp = NULL;
	ep->me_muxid = muxid;
	ep->me_nodep = &mux_nodes[lomaj];
}

/*
 * A multiplexor link has been removed. Remove the
 * edge in the directed graph.
 */
void
mux_rmvedge(stdata_t *upstp, int muxid)
{
	struct mux_node *np;
	struct mux_edge *ep;
	struct mux_edge *pep = NULL;
	major_t upmaj;

	upmaj = getmajor(upstp->sd_vnode->v_rdev);
	np = &mux_nodes[upmaj];
	ASSERT(np->mn_outp != NULL);
	ep = np->mn_outp;
	while (ep) {
		if (ep->me_muxid == muxid) {
			if (pep)
				pep->me_nextp = ep->me_nextp;
			else
				np->mn_outp = ep->me_nextp;
			kmem_free(ep, sizeof (struct mux_edge));
			return;
		}
		pep = ep;
		ep = ep->me_nextp;
	}
	ASSERT(0);	/* should not reach here */
}

/*
 * Translate the device flags (from conf.h) to the corresponding
 * qflag and sq_flag (type) values.
 */
int
devflg_to_qflag(struct streamtab *stp, uint32_t devflag, uint32_t *qflagp,
	uint32_t *sqtypep)
{
	uint32_t qflag = 0;
	uint32_t sqtype = 0;

	if (devflag & _D_OLD)
		goto bad;

	/* Inner perimeter presence and scope */
	switch (devflag & D_MTINNER_MASK) {
	case D_MP:
		qflag |= QMTSAFE;
		sqtype |= SQ_CI;
		break;
	case D_MTPERQ|D_MP:
		qflag |= QPERQ;
		break;
	case D_MTQPAIR|D_MP:
		qflag |= QPAIR;
		break;
	case D_MTPERMOD|D_MP:
		qflag |= QPERMOD;
		break;
	default:
		goto bad;
	}

	/* Outer perimeter */
	if (devflag & D_MTOUTPERIM) {
		switch (devflag & D_MTINNER_MASK) {
		case D_MP:
		case D_MTPERQ|D_MP:
		case D_MTQPAIR|D_MP:
			break;
		default:
			goto bad;
		}
		qflag |= QMTOUTPERIM;
	}

	/* Inner perimeter modifiers */
	if (devflag & D_MTINNER_MOD) {
		switch (devflag & D_MTINNER_MASK) {
		case D_MP:
			goto bad;
		default:
			break;
		}
		if (devflag & D_MTPUTSHARED)
			sqtype |= SQ_CIPUT;
		if (devflag & _D_MTOCSHARED) {
			/*
			 * The code in putnext assumes that it has the
			 * highest concurrency by not checking sq_count.
			 * Thus _D_MTOCSHARED can only be supported when
			 * D_MTPUTSHARED is set.
			 */
			if (!(devflag & D_MTPUTSHARED))
				goto bad;
			sqtype |= SQ_CIOC;
		}
		if (devflag & _D_MTCBSHARED) {
			/*
			 * The code in putnext assumes that it has the
			 * highest concurrency by not checking sq_count.
			 * Thus _D_MTCBSHARED can only be supported when
			 * D_MTPUTSHARED is set.
			 */
			if (!(devflag & D_MTPUTSHARED))
				goto bad;
			sqtype |= SQ_CICB;
		}
	}

	/* Default outer perimeter concurrency */
	sqtype |= SQ_CO;

	/* Outer perimeter modifiers */
	if (devflag & D_MTOCEXCL) {
		if (!(devflag & D_MTOUTPERIM)) {
			/* No outer perimeter */
			goto bad;
		}
		sqtype &= ~SQ_COOC;
	}

	/* Synchronous Streams extended qinit structure */
	if (devflag & D_SYNCSTR)
		qflag |= QSYNCSTR;

	*qflagp = qflag;
	*sqtypep = sqtype;
	return (0);

bad:
	cmn_err(CE_WARN,
	    "stropen: bad MT flags (0x%x) in driver '%s'",
	    (int)(qflag & D_MTSAFETY_MASK),
	    stp->st_rdinit->qi_minfo->mi_idname);

	return (EINVAL);
}

/*
 * sq_max_draincnt is the global/tunable for identifying the maximum
 * number of threads that can drain a syncq.
 *
 * In addition, we have a hiwat and lowat value.  I would prefer these
 * to be constants, but this way we can actually experiment with these
 * values.  The lowat value was chosen from some earlier tests with
 * changing the sq_max_draincnt, and it was determined that a value
 * of 4 causes the system to drag.  The hiwat was chosen at random
 * (and because I suspect that there will not be any performance gain
 * from any higher number).
 * NOTE:
 * We no longer use this variable, but it was identified to customers
 * as a syncq optimization.  For that reason, we need to go through
 * the deprication process.  For this reason, this variable will
 * stay in the kernel until officially depricated.
 */
uint_t sq_max_draincnt = 10;

/*
 * Set the interface values for a pair of queues (qinit structure,
 * packet sizes, water marks).
 * setq assumes that the caller does not have a claim (entersq or claimq)
 * on the queue.
 * The syncqp pointer is non-NULL only for the QPERMOD case.
 */
void
setq(
	queue_t *rq,
	struct qinit *rinit,
	struct qinit *winit,
	struct streamtab *stp,
	perdm_t *dmp,
	uint32_t  qflag,
	uint32_t sqtype,
	int lock_needed)
{
	queue_t *wq;
	syncq_t	*sq, *outer;

	ASSERT(rq->q_flag & QREADR);
	ASSERT((qflag & QMT_TYPEMASK) != 0);

	wq = _WR(rq);
	rq->q_qinfo = rinit;
	rq->q_hiwat = rinit->qi_minfo->mi_hiwat;
	rq->q_lowat = rinit->qi_minfo->mi_lowat;
	rq->q_minpsz = rinit->qi_minfo->mi_minpsz;
	rq->q_maxpsz = rinit->qi_minfo->mi_maxpsz;
	wq->q_qinfo = winit;
	wq->q_hiwat = winit->qi_minfo->mi_hiwat;
	wq->q_lowat = winit->qi_minfo->mi_lowat;
	wq->q_minpsz = winit->qi_minfo->mi_minpsz;
	wq->q_maxpsz = winit->qi_minfo->mi_maxpsz;

	/* Remove old syncqs */
	sq = rq->q_syncq;
	outer = sq->sq_outer;
	if (outer != NULL) {
		ASSERT(wq->q_syncq->sq_outer == outer);
		outer_remove(outer, rq->q_syncq);
		if (wq->q_syncq != rq->q_syncq)
			outer_remove(outer, wq->q_syncq);
	}
	ASSERT(sq->sq_outer == NULL);
	ASSERT(sq->sq_onext == NULL && sq->sq_oprev == NULL);

	if (sq != SQ(rq)) {
		if (!(rq->q_flag & QPERMOD))
			free_syncq(sq);
		if (wq->q_syncq == rq->q_syncq)
			wq->q_syncq = NULL;
		rq->q_syncq = NULL;
	}
	if (wq->q_syncq != NULL && wq->q_syncq != sq &&
	    wq->q_syncq != SQ(rq)) {
		free_syncq(wq->q_syncq);
		wq->q_syncq = NULL;
	}
	ASSERT(rq->q_syncq == NULL || (rq->q_syncq->sq_head == NULL &&
				rq->q_syncq->sq_tail == NULL));
	ASSERT(wq->q_syncq == NULL || (wq->q_syncq->sq_head == NULL &&
				wq->q_syncq->sq_tail == NULL));

	if (!(rq->q_flag & QPERMOD) &&
	    rq->q_syncq != NULL && rq->q_syncq->sq_ciputctrl != NULL) {
		ASSERT(rq->q_syncq->sq_nciputctrl == n_ciputctrl - 1);
		SUMCHECK_CIPUTCTRL_COUNTS(rq->q_syncq->sq_ciputctrl,
		    rq->q_syncq->sq_nciputctrl, 0);
		ASSERT(ciputctrl_cache != NULL);
		kmem_cache_free(ciputctrl_cache, rq->q_syncq->sq_ciputctrl);
		rq->q_syncq->sq_ciputctrl = NULL;
		rq->q_syncq->sq_nciputctrl = 0;
	}

	if (!(wq->q_flag & QPERMOD) &&
	    wq->q_syncq != NULL && wq->q_syncq->sq_ciputctrl != NULL) {
		ASSERT(wq->q_syncq->sq_nciputctrl == n_ciputctrl - 1);
		SUMCHECK_CIPUTCTRL_COUNTS(wq->q_syncq->sq_ciputctrl,
		    wq->q_syncq->sq_nciputctrl, 0);
		ASSERT(ciputctrl_cache != NULL);
		kmem_cache_free(ciputctrl_cache, wq->q_syncq->sq_ciputctrl);
		wq->q_syncq->sq_ciputctrl = NULL;
		wq->q_syncq->sq_nciputctrl = 0;
	}

	sq = SQ(rq);
	ASSERT(sq->sq_head == NULL && sq->sq_tail == NULL);
	ASSERT(sq->sq_outer == NULL);
	ASSERT(sq->sq_onext == NULL && sq->sq_oprev == NULL);

	/*
	 * Create syncqs based on qflag and sqtype. Set the SQ_TYPES_IN_FLAGS
	 * bits in sq_flag based on the sqtype.
	 */
	ASSERT((sq->sq_flags & ~SQ_TYPES_IN_FLAGS) == 0);

	rq->q_syncq = wq->q_syncq = sq;
	sq->sq_type = sqtype;
	sq->sq_flags = (sqtype & SQ_TYPES_IN_FLAGS);

	/*
	 * We need to acquire the lock here for the mlink
	 * and munlink case, where set_qfull, canputnext
	 * backenable, etc can access the q_flag.
	 */
	if (lock_needed) {
		mutex_enter(QLOCK(rq));
		rq->q_flag = (rq->q_flag & ~QMT_TYPEMASK) | QWANTR | qflag;
		mutex_exit(QLOCK(rq));
		mutex_enter(QLOCK(wq));
		wq->q_flag = (wq->q_flag & ~QMT_TYPEMASK) | QWANTR | qflag;
		mutex_exit(QLOCK(wq));
	} else {
		rq->q_flag = (rq->q_flag & ~QMT_TYPEMASK) | QWANTR | qflag;
		wq->q_flag = (wq->q_flag & ~QMT_TYPEMASK) | QWANTR | qflag;
	}

	if (qflag & QPERQ) {
		/* Allocate a separate syncq for the write side */
		sq = new_syncq();
		sq->sq_type = rq->q_syncq->sq_type;
		sq->sq_flags = rq->q_syncq->sq_flags;
		ASSERT(sq->sq_outer == NULL && sq->sq_onext == NULL &&
		    sq->sq_oprev == NULL);
		wq->q_syncq = sq;
	}
	if (qflag & QPERMOD) {
		ASSERT(dmp);
		if ((sq = dmp->dm_sq) == NULL) {
			/*
			 * Allocate the per-module syncq. This is a
			 * once-in-the-life-of-the-system operation.
			 * XXX -- this should be done as part of the
			 * modload/unload callback into the streams
			 * framework -- once we have one.
			 */
			syncq_t *newsq = new_syncq();
			mutex_enter(&perdm_lock);
			if ((sq = dmp->dm_sq) != NULL ||
			    (sq = setq_finddm(dmp, stp)) != NULL) {
				free_syncq(newsq);
			} else {
				dmp->dm_sq = sq = newsq;
				dmp->dm_str = stp;
				sq->sq_type = sqtype;
				sq->sq_flags = sqtype & SQ_TYPES_IN_FLAGS;
			}
			mutex_exit(&perdm_lock);
		}
		/*
		 * Assert that we do have an inner perimeter syncq and that it
		 * does not have an outer perimeter associated with it.
		 */
		ASSERT(sq->sq_outer == NULL && sq->sq_onext == NULL &&
		    sq->sq_oprev == NULL);
		rq->q_syncq = wq->q_syncq = sq;
	}
	if (qflag & QMTOUTPERIM) {
		ASSERT(dmp);
		if ((outer = dmp->dm_sq) == NULL) {
			/*
			 * Allocate the outer syncq. This is a
			 * once-in-the-life-of-the-system operation.
			 */
			syncq_t *newsq = new_syncq();
			mutex_enter(&perdm_lock);
			if ((outer = dmp->dm_sq) != NULL ||
			    (outer = setq_finddm(dmp, stp)) != NULL) {
				free_syncq(newsq);
			} else {
				dmp->dm_sq = outer = newsq;
				dmp->dm_str = stp;
				outer->sq_onext = outer->sq_oprev = outer;
				outer->sq_type = 0;
				outer->sq_flags = 0;
			}
			mutex_exit(&perdm_lock);
		}
		ASSERT(outer->sq_outer == NULL);
		outer_insert(outer, rq->q_syncq);
		if (wq->q_syncq != rq->q_syncq)
			outer_insert(outer, wq->q_syncq);
	}
	ASSERT((rq->q_syncq->sq_flags & SQ_TYPES_IN_FLAGS) ==
		(rq->q_syncq->sq_type & SQ_TYPES_IN_FLAGS));
	ASSERT((wq->q_syncq->sq_flags & SQ_TYPES_IN_FLAGS) ==
		(wq->q_syncq->sq_type & SQ_TYPES_IN_FLAGS));
	ASSERT((rq->q_flag & QMT_TYPEMASK) == (qflag & QMT_TYPEMASK));
	/*
	 * Initialize struio() types.
	 */
	if (rq->q_flag & QSYNCSTR)
		rq->q_struiot = rinit->qi_struiot;
	else
		rq->q_struiot = STRUIOT_NONE;
	if (wq->q_flag & QSYNCSTR)
		wq->q_struiot = winit->qi_struiot;
	else
		wq->q_struiot = STRUIOT_NONE;
}

static syncq_t *
setq_finddm(perdm_t *dmp, struct streamtab *stp)
{
	int i;

	ASSERT(MUTEX_HELD(&perdm_lock));

	for (i = 0; i < devcnt; i++) {
		if (perdev_syncq[i].dm_str == stp) {
			dmp->dm_sq = perdev_syncq[i].dm_sq;
			dmp->dm_str = stp;
			return (dmp->dm_sq);
		}
	}
	for (i = 0; i < fmodcnt; i++) {
		if (permod_syncq[i].dm_str == stp) {
			dmp->dm_sq = permod_syncq[i].dm_sq;
			dmp->dm_str = stp;
			return (dmp->dm_sq);
		}
	}
	return (NULL);
}

/*
 * Make a protocol message given control and data buffers.
 * n.b., this can block; be careful of what locks you hold when calling it.
 *
 * If sd_maxblk is less than *iosize this routine can fail part way through
 * (due to an allocation failure). In this case on return *iosize will contain
 * the amount that was consumed. Otherwise *iosize will not be modified
 * i.e. it will contain the amount that was consumed.
 */
int
strmakemsg(
	struct strbuf *mctl,
	ssize_t *iosize,
	struct uio *uiop,
	stdata_t *stp,
	int32_t flag,
	mblk_t **mpp)
{
	mblk_t *mpctl = NULL;
	mblk_t *mpdata = NULL;
	int error;

	ASSERT(uiop != NULL);

	*mpp = NULL;
	/* Create control part, if any */
	if ((mctl != NULL) && (mctl->len >= 0)) {
		error = strmakectl(mctl, flag, uiop->uio_fmode, &mpctl);
		if (error)
			return (error);
	}
	/* Create data part, if any */
	if (*iosize >= 0) {
		error = strmakedata(iosize, uiop, stp, flag, &mpdata);
		if (error) {
			freemsg(mpctl);
			return (error);
		}
	}
	if (mpctl != NULL) {
		if (mpdata != NULL)
			linkb(mpctl, mpdata);
		*mpp = mpctl;
	} else {
		*mpp = mpdata;
	}
	return (0);
}

/*
 * Make a the control part of a protocol message given a control buffer.
 * n.b., this can block; be careful of what locks you hold when calling it.
 */
int
strmakectl(
	struct strbuf *mctl,
	int32_t flag,
	int32_t fflag,
	mblk_t **mpp)
{
	mblk_t *bp = NULL;
	unsigned char msgtype;
	int error = 0;

	*mpp = NULL;
	/*
	 * Create control part of message, if any.
	 */
	if ((mctl != NULL) && (mctl->len >= 0)) {
		caddr_t base;
		int ctlcount;
		int allocsz;

		if (flag & RS_HIPRI)
			msgtype = M_PCPROTO;
		else
			msgtype = M_PROTO;

		ctlcount = mctl->len;
		base = mctl->buf;

		/*
		 * Give modules a better chance to reuse M_PROTO/M_PCPROTO
		 * blocks by increasing the size to something more usable.
		 */
		allocsz = MAX(ctlcount, 64);

		/*
		 * Range checking has already been done; simply try
		 * to allocate a message block for the ctl part.
		 */
		while (!(bp = allocb(allocsz, BPRI_MED))) {
			if (fflag & (FNDELAY|FNONBLOCK))
				return (EAGAIN);
			if (error = strwaitbuf(allocsz, BPRI_MED))
				return (error);
		}

		bp->b_datap->db_type = msgtype;
		if (copyin(base, (caddr_t)bp->b_wptr, ctlcount)) {
			freeb(bp);
			return (EFAULT);
		}
		bp->b_wptr += ctlcount;
	}
	*mpp = bp;
	return (0);
}

/*
 * Make a protocol message given data buffers.
 * n.b., this can block; be careful of what locks you hold when calling it.
 *
 * If sd_maxblk is less than *iosize this routine can fail part way through
 * (due to an allocation failure). In this case on return *iosize will contain
 * the amount that was consumed. Otherwise *iosize will not be modified
 * i.e. it will contain the amount that was consumed.
 */
int
strmakedata(
	ssize_t   *iosize,
	struct uio *uiop,
	stdata_t *stp,
	int32_t flag,
	mblk_t **mpp)
{
	mblk_t *mp = NULL;
	mblk_t *bp;
	int wroff = (int)stp->sd_wroff;
	int error = 0;
	ssize_t maxblk;
	ssize_t count;
#ifdef ZC_TEST
	hrtime_t start;
#endif

	*mpp = NULL;
	count = *iosize;
#ifdef ZC_TEST
	if ((count & PAGEOFFSET) == 0 && (zcperf & 1) &&
	    (flag & STRUIO_POSTPONE) == 0) {
		start = gethrtime();
	} else start = 0ll;
#endif
	/*
	 * We insist count to be an integer number of pages to avoid
	 * sending small packets.
	 */
	if ((flag & STRUIO_MAPIN) && count && (count & PAGEOFFSET) == 0) {
		count = struiomapin(stp, uiop, count, &mp);
		/* count returns # of bytes that didn't get mapped */
		if (count == 0)
			goto exit;
		zckstat->zc_misses.value.ul += count >> PAGESHIFT;
	}
	maxblk = stp->sd_maxblk;
	if (maxblk == INFPSZ)
		maxblk = count;

	/*
	 * Create data part of message, if any.
	 */
	while (count >= 0) {
		ssize_t size;
		dblk_t  *dp;

		ASSERT(uiop);

		size = MIN(count, maxblk);

		while ((bp = allocb(size + wroff, BPRI_MED)) == NULL) {
			if (uiop->uio_fmode & (FNDELAY|FNONBLOCK)) {
				if (count == *iosize) {
					freemsg(mp);
					return (EAGAIN);
				} else {
					*iosize -= count;
					*mpp = mp;
					return (0);
				}
			}
			if (error = strwaitbuf(size + wroff, BPRI_MED)) {
				if (count == *iosize) {
					freemsg(mp);
					return (error);
				} else {
					*iosize -= count;
					*mpp = mp;
					return (0);
				}
			}
		}
		dp = bp->b_datap;
		if (CRED() != NULL) {
			dp->db_uid = CRED()->cr_uid;
		}
		ASSERT(wroff <= dp->db_lim - bp->b_wptr);
		bp->b_wptr = bp->b_rptr = bp->b_rptr + wroff;
		if (flag & STRUIO_POSTPONE) {
			/*
			 * Setup the stream uio portion of the
			 * dblk for subsequent use by struioget().
			 */
			dp->db_struioflag = STRUIO_SPEC;
			dp->db_struiobase = bp->b_rptr;
			dp->db_struioptr = bp->b_rptr;
			dp->db_struiolim = bp->b_wptr + size;
#ifdef	_NO_LONGLONG
			bzero(&dp->db_struioun, sizeof (dp->db_struioun));
#else
			*(long long *)dp->db_struioun.data = 0ll;
#endif
		} else {
			if (size != 0 && (error = uiomove((caddr_t)bp->b_wptr,
			    size, UIO_WRITE, uiop))) {
				freeb(bp);
				freemsg(mp);
				return (error);
			}
		}

		bp->b_wptr += size;
		count -= size;
		if (!mp)
			mp = bp;
		else
			linkb(mp, bp);
		if (count == 0)
			break;
	}
exit:
	*mpp = mp;
#ifdef ZC_TEST
	if (start) {
		zckstat->zc_count.value.ul++;
		zckstat->zc_hrtime.value.ull += gethrtime() - start;
	}
#endif
	return (0);
}

/*
 * Wait for a buffer to become available. Return non-zero errno
 * if not able to wait, 0 if buffer is probably there.
 */
int
strwaitbuf(size_t size, int pri)
{
	bufcall_id_t id;

	mutex_enter(&bcall_monitor);
	if ((id = bufcall(size, pri, (void (*)(void *))cv_broadcast,
	    &ttoproc(curthread)->p_flag_cv)) == 0) {
		mutex_exit(&bcall_monitor);
		return (ENOSR);
	}
	if (!cv_wait_sig(&(ttoproc(curthread)->p_flag_cv), &bcall_monitor)) {
		unbufcall(id);
		mutex_exit(&bcall_monitor);
		return (EINTR);
	}
	unbufcall(id);
	mutex_exit(&bcall_monitor);
	return (0);
}

/*
 * This function waits for a read or write event to happen on a stream.
 * fmode can specify FNDELAY and/or FNONBLOCK.
 * The timeout is in ms with -1 meaning infinite.
 * The flag values work as follows:
 *	READWAIT	Check for read side errors, send M_READ
 *	GETWAIT		Check for read side errors, no M_READ
 *	WRITEWAIT	Check for write side errors.
 *	NOINTR		Do not return error if nonblocking or timeout.
 * 	STR_NOERROR	Ignore all errors except STPLEX.
 *	STR_NOSIG	Ignore/hold signals during the duration of the call.
 *	STR_PEEK	Pass through the strgeterr().
 */
int
strwaitq(stdata_t *stp, int flag, ssize_t count, int fmode, clock_t timout,
    int *done)
{
	int slpflg, errs;
	int error;
	kcondvar_t *sleepon;
	mblk_t *mp;
	ssize_t *rd_count;
	clock_t rval;

	ASSERT(MUTEX_HELD(&stp->sd_lock));
	if ((flag & READWAIT) || (flag & GETWAIT)) {
		slpflg = RSLEEP;
		sleepon = &_RD(stp->sd_wrq)->q_wait;
		errs = STRDERR|STPLEX;
	} else {
		slpflg = WSLEEP;
		sleepon = &stp->sd_wrq->q_wait;
		errs = STWRERR|STRHUP|STPLEX;
	}
	if (flag & STR_NOERROR)
		errs = STPLEX;

	if (stp->sd_wakeq & slpflg) {
		/*
		 * A strwakeq() is pending, no need to sleep.
		 */
		stp->sd_wakeq &= ~slpflg;
		*done = 0;
		return (0);
	}

	if (fmode & (FNDELAY|FNONBLOCK)) {
		if (!(flag & NOINTR))
			error = EAGAIN;
		else
			error = 0;
		*done = 1;
		return (error);
	}

	if (stp->sd_flag & errs) {
		/*
		 * Check for errors before going to sleep since the
		 * caller might not have checked this while holding
		 * sd_lock.
		 */
		error = strgeterr(stp, errs, (flag & STR_PEEK));
		if (error != 0) {
			*done = 1;
			return (error);
		}
	}

	if (slpflg == RSLEEP) {
		/*
		 * If any module downstream has requested read notification
		 * by setting SNDMREAD flag using M_SETOPTS, send a message
		 * down stream.
		 */
		if ((flag & READWAIT) && (stp->sd_flag & SNDMREAD)) {
			mutex_exit(&stp->sd_lock);
			if (!(mp = allocb_wait(sizeof (ssize_t), BPRI_MED,
			    (flag & STR_NOSIG), &error))) {
				mutex_enter(&stp->sd_lock);
				*done = 1;
				return (error);
			}
			mp->b_datap->db_type = M_READ;
			rd_count = (ssize_t *)mp->b_wptr;
			*rd_count = count;
			mp->b_wptr += sizeof (ssize_t);
			/*
			 * Send the number of bytes requested by the
			 * read as the argument to M_READ.
			 */
			putnext(stp->sd_wrq, mp);
			if (qready())
				queuerun();
			mutex_enter(&stp->sd_lock);

			/*
			 * If any data arrived due to inline processing
			 * of putnext(), don't sleep.
			 */
			mp = _RD(stp->sd_wrq)->q_first;
			while (mp) {
				if (!(mp->b_flag & MSGNOGET))
					break;
				mp = mp->b_next;
			}
			if (mp != NULL) {
				*done = 0;
				return (0);
			}
		}
	}

	stp->sd_flag |= slpflg;
	TRACE_5(TR_FAC_STREAMS_FR, TR_STRWAITQ_WAIT2,
		"strwaitq sleeps (2):%X, %X, %X, %X, %X",
		stp, flag, count, fmode, done);

	rval = str_cv_wait(sleepon, &stp->sd_lock, timout, flag & STR_NOSIG);
	if (rval > 0) {
		/* EMPTY */
		TRACE_5(TR_FAC_STREAMS_FR, TR_STRWAITQ_WAKE2,
			"strwaitq awakes(2):%X, %X, %X, %X, %X",
			stp, flag, count, fmode, done);
	} else if (rval == 0) {
		TRACE_5(TR_FAC_STREAMS_FR, TR_STRWAITQ_INTR2,
			"strwaitq interrupt #2:%X, %X, %X, %X, %X",
			stp, flag, count, fmode, done);
		stp->sd_flag &= ~slpflg;
		cv_broadcast(sleepon);
		if (!(flag & NOINTR))
			error = EINTR;
		else
			error = 0;
		*done = 1;
		return (error);
	} else {
		/* timeout */
		TRACE_5(TR_FAC_STREAMS_FR, TR_STRWAITQ_TIME,
			"strwaitq timeout:%X, %X, %X, %X, %X",
			stp, flag, count, fmode, done);
		*done = 1;
		if (!(flag & NOINTR))
			return (ETIME);
		else
			return (0);
	}
	/*
	 * If the caller implements delayed errors (i.e. queued after data)
	 * we can not check for errors here since data as well as an
	 * error might have arrived at the stream head. We return to
	 * have the caller check the read queue before checking for errors.
	 */
	if ((stp->sd_flag & errs) && !(flag & STR_DELAYERR)) {
		error = strgeterr(stp, errs, (flag & STR_PEEK));
		if (error != 0) {
			*done = 1;
			return (error);
		}
	}
	*done = 0;
	return (0);
}

int
str2num(char **str)		/* string to number, updating pointer */
{
	int n;

	for (n = 0; **str >= '0' && **str <= '9'; (*str)++)
		n = 10 * n + **str - '0';
	return (n);
}

/*
 * Update canon format pointer with "bytes" worth of data.
 */
void
adjfmtp(char **str, mblk_t *bp, size_t bytes)
{
	caddr_t addr;
	caddr_t lim;
	int32_t num;

	addr = (caddr_t)bp->b_rptr;
	lim = addr + bytes;
	while (addr < lim) {
		switch (*(*str)++) {
		case 's':			/* short */
			addr = SALIGN(addr);
			addr = SNEXT(addr);
			break;
		case 'i':			/* integer */
			addr = IALIGN(addr);
			addr = INEXT(addr);
			break;
		case 'l':			/* long */
			addr = LALIGN(addr);
			addr = LNEXT(addr);
			break;
		case 'b':			/* byte */
			addr++;
			break;
		case 'c':			/* character */
			if ((num = str2num(str)) == 0) {
				while (*addr++)
					;
			} else
				addr += num;
			break;
		case 0:
			return;
		default:
			break;
		}
	}
}

/*
 * Perform job control discipline access checks.
 * Return 0 for success and the errno for failure.
 */

#define	cantsend(pp, sig) \
	(sigismember(&pp->p_ignore, sig) || sigisheld(pp, sig))

int
straccess(struct stdata *stp, enum jcaccess mode)
{
	extern kcondvar_t lbolt_cv;	/* XXX: should be in a header file */

	proc_t *pp;
	sess_t *sp;

	if (stp->sd_sidp == NULL || stp->sd_vnode->v_type == VFIFO)
		return (0);

	pp = ttoproc(curthread);
	mutex_enter(&pp->p_lock);
	sp = pp->p_sessp;

	for (;;) {

		/*
		 * if this is not the calling process's controlling terminal
		 * or the calling process is already in the foreground
		 * then allow access
		 */

		if (sp->s_dev != stp->sd_vnode->v_rdev ||
		    pp->p_pgidp == stp->sd_pgidp) {
			mutex_exit(&pp->p_lock);
			return (0);
		}

		/*
		 * check to see if controlling terminal has been deallocated
		 */

		if (sp->s_vp == NULL) {
			if (cantsend(pp, SIGHUP)) {
				mutex_exit(&pp->p_lock);
				return (EIO);
			}
			mutex_exit(&pp->p_lock);
			pgsignal(pp->p_pgidp, SIGHUP);
			mutex_enter(&pp->p_lock);
		}

		else if (mode == JCGETP) {
			mutex_exit(&pp->p_lock);
			return (0);
		    }

		else if (mode == JCREAD) {
			if (cantsend(pp, SIGTTIN) || pp->p_detached) {
				mutex_exit(&pp->p_lock);
				return (EIO);
			}
			mutex_exit(&pp->p_lock);
			pgsignal(pp->p_pgidp, SIGTTIN);
			mutex_enter(&pp->p_lock);
		}

		else {  /* mode == JCWRITE or JCSETP */
			if ((mode == JCWRITE && !(stp->sd_flag & STRTOSTOP)) ||
			    cantsend(pp, SIGTTOU)) {
				mutex_exit(&pp->p_lock);
				return (0);
			}
			if (pp->p_detached) {
				mutex_exit(&pp->p_lock);
				return (EIO);
			}
			mutex_exit(&pp->p_lock);
			pgsignal(pp->p_pgidp, SIGTTOU);
			mutex_enter(&pp->p_lock);
		}

		/*
		 * We call cv_wait_sig_swap() to cause the appropriate
		 * action for the jobcontrol signal to take place.
		 * We will not actually go to sleep on &lbolt_cv;
		 * we will either stop or get a failure return.
		 */
		if (!cv_wait_sig_swap(&lbolt_cv, &pp->p_lock)) {
			mutex_exit(&pp->p_lock);
			return (EINTR);
		}
	}
}

/*
 * Return size of message of block type (bp->b_datap->db_type)
 */
size_t
xmsgsize(mblk_t *bp)
{
	unsigned char type;
	size_t count = 0;

	type = bp->b_datap->db_type;

	for (; bp; bp = bp->b_cont) {
		if (type != bp->b_datap->db_type)
			break;
		ASSERT(bp->b_wptr >= bp->b_rptr);
		count += bp->b_wptr - bp->b_rptr;
	}
	return (count);
}

/*
 * Allocate a stream head.
 */
struct stdata *
shalloc(queue_t *qp)
{
	stdata_t *stp;

	stp = kmem_cache_alloc(stream_head_cache, KM_SLEEP);

	stp->sd_wrq = _WR(qp);
	stp->sd_strtab = NULL;
	stp->sd_iocid = 0;
	stp->sd_mate = NULL;
	stp->sd_freezer = NULL;
	stp->sd_refcnt = 0;
	stp->sd_wakeq = 0;
	stp->sd_struiowrq = NULL;
	stp->sd_struiordq = NULL;
	stp->sd_struiodnak = 0;
	stp->sd_struionak = NULL;
#ifdef C2_AUDIT
	stp->sd_t_audit_data = NULL;
#endif
	stp->sd_rput_opt = 0;
	stp->sd_wput_opt = 0;
	stp->sd_read_opt = 0;
	stp->sd_rprotofunc = strrput_proto;
	stp->sd_rmiscfunc = strrput_misc;
	stp->sd_rderrfunc = stp->sd_wrerrfunc = NULL;
	stp->sd_ciputctrl = NULL;
	stp->sd_nciputctrl = 0;
	return (stp);
}

/*
 * Free a stream head.
 */
void
shfree(stdata_t *stp)
{
	ASSERT(MUTEX_NOT_HELD(&stp->sd_lock));

	stp->sd_wrq = NULL;
	if (stp->sd_ciputctrl != NULL) {
		ASSERT(stp->sd_nciputctrl == n_ciputctrl - 1);
		SUMCHECK_CIPUTCTRL_COUNTS(stp->sd_ciputctrl,
		    stp->sd_nciputctrl, 0);
		ASSERT(ciputctrl_cache != NULL);
		kmem_cache_free(ciputctrl_cache, stp->sd_ciputctrl);
		stp->sd_ciputctrl = NULL;
		stp->sd_nciputctrl = 0;
	}
	kmem_cache_free(stream_head_cache, stp);
}

/*
 * Allocate a pair of queues and a syncq for the pair
 */
queue_t *
allocq(void)
{
	queinfo_t *qip;
	queue_t *qp, *wqp;
	syncq_t	*sq;

	qip = kmem_cache_alloc(queue_cache, KM_SLEEP);

	qp = &qip->qu_rqueue;
	wqp = &qip->qu_wqueue;
	sq = &qip->qu_syncq;

	qp->q_last	= NULL;
	qp->q_next	= NULL;
	qp->q_ptr	= NULL;
	qp->q_flag	= QUSE | QREADR;
	qp->q_bandp	= NULL;
	qp->q_stream	= NULL;
	qp->q_syncq	= sq;
	qp->q_nband	= 0;
	qp->q_nfsrv	= NULL;
	qp->q_nbsrv	= NULL;
	qp->q_draining	= 0;
	qp->q_syncqmsgs	= 0;
	qp->q_spri = 0;

	wqp->q_last	= NULL;
	wqp->q_next	= NULL;
	wqp->q_ptr	= NULL;
	wqp->q_flag	= QUSE;
	wqp->q_bandp	= NULL;
	wqp->q_stream	= NULL;
	wqp->q_syncq	= sq;
	wqp->q_nband	= 0;
	wqp->q_nfsrv	= NULL;
	wqp->q_nbsrv	= NULL;
	wqp->q_draining	= 0;
	wqp->q_syncqmsgs = 0;
	wqp->q_spri = 0;

	sq->sq_count	= 0;
	sq->sq_occount	= 0;
	sq->sq_flags	= 0;
	sq->sq_type	= 0;
	sq->sq_callbflags = 0;
	sq->sq_cancelid	= 0;
	sq->sq_private = NULL;
	sq->sq_ciputctrl = NULL;
	sq->sq_nciputctrl = 0;
	sq->sq_needexcl = 0;
	sq->sq_svcflags = 0;

	return (qp);
}

/*
 * Free a pair of queues and the "attached" syncq.
 * Discard any messages left on the syncq(s), remove the syncq(s) from the
 * outer perimeter, and free the syncq(s) if they are not the "attached" syncq.
 */
void
freeq(queue_t *qp)
{
	qband_t *qbp, *nqbp;
	syncq_t *sq, *outer;
	queue_t *wqp = _WR(qp);

	ASSERT(qp->q_flag & QREADR);

	(void) flush_syncq(qp->q_syncq, qp);
	(void) flush_syncq(wqp->q_syncq, wqp);
	ASSERT(qp->q_syncqmsgs == 0 && wqp->q_syncqmsgs == 0);

	outer = qp->q_syncq->sq_outer;
	if (outer != NULL) {
		outer_remove(outer, qp->q_syncq);
		if (wqp->q_syncq != qp->q_syncq)
			outer_remove(outer, wqp->q_syncq);
	}
	/*
	 * Free any syncqs that are outside what allocq returned.
	 */
	if (qp->q_syncq != SQ(qp) && !(qp->q_flag & QPERMOD))
		free_syncq(qp->q_syncq);
	if (qp->q_syncq != wqp->q_syncq && wqp->q_syncq != SQ(qp))
		free_syncq(wqp->q_syncq);

	ASSERT((qp->q_sqflags & (Q_SQQUEUED | Q_SQDRAINING)) == 0);
	ASSERT((wqp->q_sqflags & (Q_SQQUEUED | Q_SQDRAINING)) == 0);
	ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));
	ASSERT(MUTEX_NOT_HELD(QLOCK(wqp)));
	sq = SQ(qp);
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
	ASSERT(sq->sq_head == NULL && sq->sq_tail == NULL);
	ASSERT(sq->sq_outer == NULL);
	ASSERT(sq->sq_onext == NULL && sq->sq_oprev == NULL);
	ASSERT(sq->sq_callbpend == NULL);
	ASSERT(sq->sq_needexcl == 0);

	if (sq->sq_ciputctrl != NULL) {
		ASSERT(sq->sq_nciputctrl == n_ciputctrl - 1);
		SUMCHECK_CIPUTCTRL_COUNTS(sq->sq_ciputctrl,
		    sq->sq_nciputctrl, 0);
		ASSERT(ciputctrl_cache != NULL);
		kmem_cache_free(ciputctrl_cache, sq->sq_ciputctrl);
		sq->sq_ciputctrl = NULL;
		sq->sq_nciputctrl = 0;
	}

	ASSERT(qp->q_first == NULL && wqp->q_first == NULL);
	ASSERT(qp->q_link == NULL && wqp->q_link == NULL);
	ASSERT(qtail != qp && qtail != wqp);
	ASSERT(qp->q_count == 0 && wqp->q_count == 0);
	ASSERT(qp->q_mblkcnt == 0 && wqp->q_mblkcnt == 0);

	/* NOTE: Uncomment the assert below once bugid 1159635 is fixed. */
	/* ASSERT((qp->q_flag & QWANTW) == 0 && (wqp->q_flag & QWANTW) == 0); */

	qbp = qp->q_bandp;
	while (qbp) {
		nqbp = qbp->qb_next;
		freeband(qbp);
		qbp = nqbp;
	}
	qbp = wqp->q_bandp;
	while (qbp) {
		nqbp = qbp->qb_next;
		freeband(qbp);
		qbp = nqbp;
	}
	kmem_cache_free(queue_cache, qp);
}

/*
 * Allocate a qband structure.
 */
qband_t *
allocband(void)
{
	qband_t *qbp;

	qbp = kmem_cache_alloc(qband_cache, KM_NOSLEEP);
	if (qbp == NULL)
		return (NULL);

	qbp->qb_next	= NULL;
	qbp->qb_count	= 0;
	qbp->qb_mblkcnt	= 0;
	qbp->qb_first	= NULL;
	qbp->qb_last	= NULL;
	qbp->qb_flag	= 0;

	return (qbp);
}

/*
 * Free a qband structure.
 */
void
freeband(qband_t *qbp)
{
	kmem_cache_free(qband_cache, qbp);
}

/*
 * This routine is consolidation private for STREAMS internal use
 * send a control message down
 * This routine may block in allocb_wait() and therefore
 * may only be called from the stream head or routines
 * that may block.
 */
int
putnextctl_wait(queue_t *q, int type)
{
	return (prn_putnextctl_wait(q, type, allocb_wait));
}
/*
 * XXX future consolidation function for putnextctl() too.
 * (as soon as unsafe stuff is removed)
 */
int
prn_putnextctl_wait(queue_t *q, int type, mblk_t *(*allocb_fn)())
{
	int	ret;

	claimstr(q);
	ret = prn_putctl_wait(q->q_next, type, allocb_fn);
	releasestr(q);
	return (ret);
}

/*
 * This routine is consolidation private for STREAMS internal use
 * Create and put a control message on queue.
 * Note: this routine may only be called from the stream head
 * or routines that may block.
 */
int
putctl_wait(queue_t *q, int type)
{
	return (prn_putctl_wait(q, type, allocb_wait));
}
/*
 * XXX future consolidation function for putctl() too.
 * (as soon as unsafe stuff is removed)
 */
int
prn_putctl_wait(queue_t *q, int type, mblk_t *(*allocb_fn)())
{
	mblk_t *bp;
	int error;

	if ((datamsg(type) && (type != M_DELAY)) ||
		(bp = allocb_fn(0, BPRI_HI, 0, &error)) == NULL)
		return (0);
	bp->b_datap->db_type = (unsigned char) type;
	put(q, bp);
	return (1);
}

/*
 * Check linked list of strhead write queues with held messages;
 * put downstream those that have been held long enough.
 *
 * Run the service procedures of each enabled queue
 *	-- must not be reentered
 *
 * Called by service mechanism (processor dependent) if there
 * are queues to run. The mechanism is reset.
 * See comment above background() procedure.
 */

int queuerun_max_credit = 10;

void
queuerun(void)
{
	queue_t *q;
	int	threads;
	int	credit;
	static int in_queuerun = 0;	/* protected by service_queue */

	mutex_enter(&service_queue);
	if (curthread->t_pri >= kpreemptpri) {
		/*
		 * A high priority thread ...
		 */
		threads = in_queuerun + 1;

		if (threads > background_count) {
			/*
			 * Already enough threads running
			 * in queuerun() (background_count worth).
			 * Note that since there are enough
			 * threads doing work, we don't have to
			 * clear flags, or send a cv_signal().
			 */
			mutex_exit(&service_queue);
			return;
		}
		/*
		 * Try to awake a background thread, and if the last
		 * background wakeup have this thread do some work
		 * (by giving credit) before returning.
		 */
		cv_signal(&services_to_run);
		credit = queuerun_max_credit;
	} else {
		threads = 0;
	}
	in_queuerun++;

	TRACE_3(TR_FAC_STREAMS_FR, TR_QRUN_START,
		"queuerun starts:enqueued %d, threads %d %d",
		enqueued, in_queuerun, background_awake);

	do {
		if (strbcflag) {
			mutex_exit(&service_queue);
			runbufcalls();
			mutex_enter(&service_queue);
		}
		for (;;) {
			if (threads && (credit && ! --credit ||
			    in_queuerun > threads)) {
				/*
				 * A high priority thread and either had
				 * credit and ran out of credit or enough
				 * other threads now running queuerun.
				 * If there are not enough threads running,
				 * wakeup a background thread.
				 */
				if (in_queuerun <= threads)
					cv_signal(&services_to_run);
				in_queuerun--;
				mutex_exit(&service_queue);
				return;
			}
			q = dq_service();
			if (q != NULL) {
				mutex_exit(&service_queue);
				mutex_enter(QLOCK(q));
				q->q_link = NULL;
				q->q_flag &= ~QENAB;
				q->q_flag |= QINSERVICE;
				mutex_exit(QLOCK(q));
				TRACE_1(TR_FAC_STREAMS_FR, TR_QRUN_DQ,
					"queuerun dq's:q %p", q);
				runservice(q);
				TRACE_1(TR_FAC_STREAMS_FR, TR_QRUN_DONE,
					"queuerun finishes:q %p", q);
				mutex_enter(&service_queue);
			} else {
				run_queues = 0;
				break;
			}
		}
	} while (strbcflag);

	in_queuerun--;
	mutex_exit(&service_queue);
	TRACE_3(TR_FAC_STREAMS_FR, TR_QRUN_LEAVES,
		"queuerun leaves:enqueued %d, threads %d %d",
		enqueued, in_queuerun, background_awake);

}

/*
 * Function to kick off queue scheduling for those system calls
 * that cause queues to be enabled (read, recv, write, send, ioctl).
 */
void
runqueues(void)
{
	TRACE_1(TR_FAC_STREAMS_FR, TR_RUNQUEUES,
		"runqueues: %x\n", (short)qrunflag);

	if (qrunflag) {
		queuerun();
		return;
	}
}

/*
 * dequeue service at head of service queue "qhead"
 */
queue_t *
dq_service(void)
{
	queue_t	*q = NULL;
	queue_t	*chaseq = NULL;

	ASSERT(MUTEX_HELD(&service_queue));
	for (q = qhead; q != NULL; q = q->q_link) {
		if (!(q->q_flag & QINSERVICE))
			break;
		TRACE_1(TR_FAC_STREAMS_FR,
			TR_DQ_SERVICE, "skipped q:%X", q);
		chaseq = q;
	}
	if (q != NULL) {
		ASSERT(!(q->q_flag & QINSERVICE));
		if (qhead == q)
			qhead = q->q_link;
		else {
			ASSERT(chaseq != NULL);
			chaseq->q_link = q->q_link;
		}
		if (qtail == q) {
			qtail = chaseq;
		}
		if (qhead == NULL) {
			qtail = NULL;
			qrunflag = 0;
		}

#ifdef TRACE
		enqueued--;
#endif /* TRACE */
	}
	return (q);
}

/*
 * remove "qp" from list "qhead". Assumes qhead properly protected.
 * Returns non-zero if found; zero otherwise.
 */
int
rmv_qp(queue_t **qhead, queue_t **qtail, queue_t *qp)
{
	struct queue *q, *prev = NULL;

	TRACE_3(TR_FAC_STREAMS_FR,
		TR_RMV_QP, "rmv_qp:(%X, %X, %X)", *qhead, *qtail, qp);
	for (q = *qhead; q; q = q->q_link)  {
		if (q == qp) {
			if (prev)
				prev->q_link = q->q_link;
			else
				*qhead = q->q_link;
			if (q == *qtail)
				*qtail = prev;
			q->q_link = NULL;
#ifdef TRACE
			enqueued--;
#endif /* TRACE */
			return (1);
		}
		prev = q;
	}
	return (0);
}

/*
 * run any possible bufcalls.
 */
void
runbufcalls(void)
{
	strbufcall_t *bcp;

	mutex_enter(&bcall_monitor);
	mutex_enter(&service_queue);
	strbcflag = 0;

	if (strbcalls.bc_head) {
		size_t count;
		int nevent;

		/*
		 * count how many events are on the list
		 * now so we can check to avoid looping
		 * in low memory situations
		 */
		nevent = 0;
		for (bcp = strbcalls.bc_head; bcp; bcp = bcp->bc_next)
			nevent++;

		/*
		 * get estimate of available memory from kmem_avail().
		 * awake all bufcall functions waiting for
		 * memory whose request could be satisfied
		 * by 'count' memory and let 'em fight for it.
		 */
		count = kmem_avail();
		while ((bcp = strbcalls.bc_head) != NULL && nevent) {
			--nevent;
			if (bcp->bc_size <= count) {
				bcp->bc_executor = curthread;
				mutex_exit(&service_queue);
				(*bcp->bc_func)(bcp->bc_arg);
				mutex_enter(&service_queue);
				bcp->bc_executor = NULL;
				cv_broadcast(&bcall_cv);
				strbcalls.bc_head = bcp->bc_next;
				kmem_cache_free(bufcall_cache, bcp);
			} else {
				/*
				 * too big, try again later - note
				 * that nevent was decremented above
				 * so we won't retry this one on this
				 * iteration of the loop
				 */
				if (bcp->bc_next != NULL) {
					strbcalls.bc_head = bcp->bc_next;
					bcp->bc_next = NULL;
					strbcalls.bc_tail->bc_next = bcp;
					strbcalls.bc_tail = bcp;
				}
			}
		}
		if (strbcalls.bc_head == NULL)
			strbcalls.bc_tail = NULL;
	}

	mutex_exit(&service_queue);
	mutex_exit(&bcall_monitor);
}


/*
 * actually run queue's service routine.
 */
static void
runservice(queue_t *q)
{
	qband_t *qbp;

	ASSERT(q->q_qinfo->qi_srvp);
again:
	entersq(q->q_syncq, SQ_SVC);
	TRACE_2(TR_FAC_STREAMS_FR, TR_QRUNSERVICE_START,
		"runservice starts:%s(%X)", QNAME(q), q);

	if (!(q->q_flag & QWCLOSE))
		(*q->q_qinfo->qi_srvp)(q);

	TRACE_1(TR_FAC_STREAMS_FR, TR_QRUNSERVICE_END,
		"runservice ends:(%X)", q);

	leavesq(q->q_syncq, SQ_SVC);

	mutex_enter(QLOCK(q));
	if (q->q_flag & QENAB) {
		q->q_flag &= ~QENAB;
		mutex_exit(QLOCK(q));
		goto again;
	}
	q->q_flag &= ~QINSERVICE;
	q->q_flag &= ~QBACK;
	for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next)
		qbp->qb_flag &= ~QB_BACK;
	/*
	 * Wakeup thread waiting for the service procedure
	 * to be run (strclose and qdetach).
	 */
	cv_broadcast(&q->q_wait);

	mutex_exit(QLOCK(q));
}

/*
 * "background" service processing thread.
 * The removal of queues from the runlist is not atomic with the
 * clearing of the QENABLED flag and setting the INSERVICE flag.
 * consequently it is possible for remove_runlist in strclose
 * to not find the queue on the runlist but for it to be QENABLED
 * and not yet INSERVICE -> hence wait_svc needs to check QENABLED
 * as well as INSERVICE.
 */
void
background(void)
{
	queue_t	*q;
	int	ate = 0;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &service_queue, callb_generic_cpr, "bckgrnd");
	mutex_enter(&service_queue);
	background_awake++;
	for (;;) {
		if (strbcflag) {
			mutex_exit(&service_queue);
			runbufcalls();
			mutex_enter(&service_queue);
		}
		if ((q = dq_service()) != NULL) {
			TRACE_1(TR_FAC_STREAMS_FR, TR_BACKGROUND_DQ,
				"background dq:%X", q);
			mutex_exit(&service_queue);
			mutex_enter(QLOCK(q));
			q->q_link = NULL;
			q->q_flag &= ~QENAB;
			q->q_flag |= QINSERVICE;
			mutex_exit(QLOCK(q));
			runservice(q);
			ate++;
			TRACE_1(TR_FAC_STREAMS_FR, TR_BACKGROUND_DONE,
				"background finishes:%X", q);
			mutex_enter(&service_queue);
		} else {
			TRACE_2(TR_FAC_STREAMS_FR, TR_BACKGROUND_AWAKE,
			"background:%s qlen %d", "sleeps", ate);
			background_awake--;
			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			cv_wait(&services_to_run, &service_queue);
			CALLB_CPR_SAFE_END(&cprinfo, &service_queue);
			background_awake++;
			ate = 0;
			TRACE_2(TR_FAC_STREAMS_FR, TR_BACKGROUND_AWAKE,
			"background:%s qlen %d", "awakes", enqueued);
		}
	}
}

/*
 * Prevent syncq from being put on the service list.
 */
static void
sq_sv_disable(syncq_t *sq)
{
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
	mutex_enter(SQLOCK(sq));
	sq->sq_svcflags |= SQ_DISABLED;
	mutex_exit(SQLOCK(sq));
} 

/*
 * Wait for any pending service processing to complete.
 */
static void
wait_sq_svc(syncq_t *sq)
{
	uint_t svcflags;

	mutex_enter(SQLOCK(sq));
	svcflags = sq->sq_svcflags;
	if ((svcflags & SQ_SERVICE)) {
		ASSERT(!(svcflags & SQ_INSERVICE));
		rm_sq_svc(sq);
	}
	while (sq->sq_svcflags & (SQ_INSERVICE | SQ_SERVICE)) {
		/* Spin out others = we are closing */
		sq->sq_flags |= SQ_WANTWAKEUP;
		cv_wait(&sq->sq_wait, SQLOCK(sq));
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Remove sq from the service queue.
 */
static void
rm_sq_svc(syncq_t *sq)
{
	int i;
	syncq_t *sq1;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT(sq->sq_svcflags & SQ_SERVICE);
	ASSERT(!(sq->sq_svcflags & SQ_INSERVICE));
	for (i = 0; i < nsqservers; i++) {
		sqserv_t *servicelist = &(sqservers[i]);
		mutex_enter(&servicelist->sqserv_lock);
		sq1 = dq_syncq(servicelist, sq);
		ASSERT(sq1 == NULL || sq1 == sq);
		mutex_exit(&servicelist->sqserv_lock);
		if (sq1 != NULL)
			sq1->sq_svcflags &= ~SQ_SERVICE;
	}
}

/*
 * Wait till the queue will dissapear from all syncq lists.
 */
static void
wait_q_syncq(queue_t *qp)
{
	queue_t *wqp = _WR(qp);
	queue_t *rqp = _RD(qp);
	syncq_t *rsq = rqp->q_syncq;
	syncq_t *wsq = wqp->q_syncq;

	/*
	 * Disable any sq background processing for per queue and queue pair
	 * perimeters.
	 */
	if (!(rqp->q_flag & QPERMOD)) {
		sq_sv_disable(rsq);
		if (wsq != rsq)
			sq_sv_disable(wsq);
	}
	wait_sq_svc(rsq);
	if (wsq != rsq)
		wait_sq_svc(wsq);
}

/*
 * Put a syncq on the list of syncq's to be serviced by the sqthread.
 * Add the argument to the end of the sqhead list and set the flag
 * indicating this syncq has been enabled.  If it has already been
 * enabled, don't do anything.
 * This routine assumes that SQLOCK is held.
 * NOTE that the lock order is to have the SQLOCK first,
 * so if the service_syncq lock is held, we need to release it
 * before aquiring the SQLOCK (mostly relevant for the background
 * thread, and this seems to be common among the STREAMS global locks).
 * Note the the sq_svcflags are protected by the SQLOCK.
 */
void
sqenable(syncq_t *sq)
{
	pri_t spri = sq->sq_pri;	/* Sq priority */
	int i;
	sqserv_t *servicelist;

	/*
	 * This is probably not important except for where I believe it
	 * is being called.  At that point, it should be held (and it
	 * is a pain to release it just for this routine, so don't do
	 * it).
	 */
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	/*
	 * Do not put on list if already servicing or is disabled.
	 */
	if (sq->sq_svcflags & (SQ_SERVICE | SQ_INSERVICE | SQ_DISABLED)) {
		return;
	}
	sq->sq_svcflags |= SQ_SERVICE;

	/*
	 * Find the service list that handles sq with given priority.
	 */
	for (i = 0; i < nsqservers && spri > sqservers[i].sqserv_pri; i++)
		;

	if (i == nsqservers) {
		/*
		 * Priority is too high, put into the highest bucket.
		 */
		i = nsqservers - 1;
	}
	servicelist = &(sqservers[i]);
	ASSERT(servicelist != NULL);
	/*
	 * Already hold the SQLOCK, but we need to aquire the
	 * service_syncq lock.  This OK since we will release the
	 * service_syncq lock if we intend to aqure the SQLOCK.
	 */
	mutex_enter(&servicelist->sqserv_lock);
	if (servicelist->sqserv_head == NULL) {
		ASSERT(servicelist->sqserv_tail == NULL);
		servicelist->sqserv_head = servicelist->sqserv_tail = sq;
	} else {
		servicelist->sqserv_tail->sq_next = sq;
		servicelist->sqserv_tail = sq;
	}
	sq->sq_next = NULL;
	servicelist->sqserv_size++;
	mutex_exit(&servicelist->sqserv_lock);
	cv_signal(&servicelist->sqserv_cv);
}

/*
 * Dequeue specified syncq or the head of the list if NULL is specified as an
 * argument.
 * If the syncq cannot be run, or there is no longer work,
 * remove the syncq from the list.  If we return null, this means
 * that there are no sync'q's that can be run, and there are none
 * in the list.
 */
syncq_t *
dq_syncq(sqserv_t *servicelist, syncq_t *sq)
{
	syncq_t *prev_sq = NULL;
	syncq_t *sqp = servicelist->sqserv_head;

	ASSERT(servicelist != NULL);
	ASSERT(MUTEX_HELD(&servicelist->sqserv_lock));

	if (sq != NULL) {
		while ((sqp != NULL) && (sqp != sq)) {
			prev_sq = sqp;
			sqp = sqp->sq_next;
		}
	}

	/*
	 * At this point sqp points to the sq to be removed and prev_sq is
	 * either NULL (if sq is at the head), or its sq_next points to sqp.
	 */
	ASSERT(sq == NULL || sqp == NULL || sq == sqp);
	ASSERT(prev_sq == NULL || prev_sq->sq_next == sqp);

	if (sqp != NULL) {

		ASSERT(servicelist->sqserv_size != 0);
		servicelist->sqserv_size--;

		/*
		 * Remove this syncq from the list.
		 */
		if (sqp == servicelist->sqserv_head)
			servicelist->sqserv_head = sqp->sq_next;

		if (sqp == servicelist->sqserv_tail) {
			ASSERT(sqp->sq_next == NULL);
			servicelist->sqserv_tail = prev_sq;
		}

		/* Head and tail may only be NULL simultaneously */
		ASSERT(servicelist->sqserv_head != NULL ||
		    (servicelist->sqserv_tail == NULL &&
			servicelist->sqserv_size == 0));
		ASSERT(servicelist->sqserv_tail != NULL ||
		    (servicelist->sqserv_head == NULL &&
			servicelist->sqserv_size == 0));

		if (prev_sq != NULL)
			prev_sq->sq_next = sqp->sq_next;

		sqp->sq_next = NULL;
	}
	return (sqp);
}

/*
 * sqthread()
 * Used to service PERMOD syncq's.  This can be just about anything, but
 * for the time being is used to call drain_syncq for each syncq in the
 * sqlist.  It is important to drop the service_syncq lock before actually
 * calling a drain_syncq, since drain_syncq might try and aquire the lock
 * to request service.
 */
void
sqthread(caddr_t arg)
{
	syncq_t	*sq;
	callb_cpr_t cprinfo;
	sqserv_t *servicelist = (sqserv_t *)arg;

	ASSERT(servicelist != NULL);

	CALLB_CPR_INIT(&cprinfo, &servicelist->sqserv_lock,
	    callb_generic_cpr, "syncqbg");
	mutex_enter(&servicelist->sqserv_lock);
	servicelist->sqserv_awake++;
	for (;;) {
		if ((sq = dq_syncq(servicelist, NULL)) != NULL) {
			pri_t curpri = curthread->t_pri;
			pri_t sqpri;

			mutex_exit(&servicelist->sqserv_lock);
			mutex_enter(SQLOCK(sq));

			ASSERT(sq->sq_svcflags & SQ_SERVICE);
			sq->sq_svcflags &= ~SQ_SERVICE;

			sqpri = sq->sq_pri;
			/*
			 * The test below should be more "generic, looking for
			 * all service lists priorities values. But this one
			 * will work in case of two service queues.
			 *
			 * If the queue is being disabled, do not reschedule it
			 * since it will not be put on the service list anyway.
			 *
			 */
			if (((curpri >= kpreemptpri && sqpri < kpreemptpri) ||
			    (curpri < kpreemptpri && sqpri >= kpreemptpri)) &&
			    !(sq->sq_svcflags & SQ_DISABLED)) {
				/*
				 * Priority inversion or hijacking detected.
				 * reschedule the syncq to the proper
				 * background thread.
				 */
				sqenable(sq);
			} else {
				sq->sq_svcflags |= SQ_INSERVICE;
				/*
				 * We are chartered to drain anything that is
				 * drainable on the syncq, including the qwriter
				 * callbacks.
				 */
				drain_syncq(sq);
				sq->sq_svcflags &= ~SQ_INSERVICE;
			}
			mutex_exit(SQLOCK(sq));
			mutex_enter(&servicelist->sqserv_lock);
		} else {
			servicelist->sqserv_awake--;
			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			cv_wait(&servicelist->sqserv_cv,
			    &servicelist->sqserv_lock);
			CALLB_CPR_SAFE_END(&cprinfo, &servicelist->sqserv_lock);
			servicelist->sqserv_awake++;
		}
	}
}

/* ARGSUSED */
void
freebs_enqueue(mblk_t *mp, dblk_t *dbp)
{
	ASSERT(dbp->db_mblk == mp);
	mutex_enter(&freebs_lock);
	cv_signal(&freebs_cv);
	mp->b_next = freebs_list;
	freebs_list = mp;
	mutex_exit(&freebs_lock);
}

/*
 * Main function of asynchronous freeb callback thread.
 * Dequeue message from freebs_list, call free routine, and free message.
 */
void
freebs(void)
{
	mblk_t *mp;
	dblk_t *dbp;
	frtn_t *frp;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &freebs_lock, callb_generic_cpr, "freebs");
	for (;;) {
		mutex_enter(&freebs_lock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		while (freebs_list == NULL)
			cv_wait(&freebs_cv, &freebs_lock);
		CALLB_CPR_SAFE_END(&cprinfo, &freebs_lock);
		mp = freebs_list;
		freebs_list = mp->b_next;
		mutex_exit(&freebs_lock);

		mp->b_next = NULL;
		dbp = mp->b_datap;
		if (dbp->db_fthdr) {
			str_ftflow(dbp);
		}
		ASSERT(dbp->db_fthdr == NULL);
		frp = dbp->db_frtnp;
		frp->free_func(frp->free_arg);
		ASSERT(dbp->db_mblk == mp);
		kmem_cache_free(dbp->db_cache, dbp);
	}
}

/*
 * Allocate an fmodsw[] entry for a loadable streams module. First
 * check if one is already allocated.
 */
int
allocate_fmodsw(char *name)
{
	fmodsw_impl_t *fmp;
	int mid;

	mutex_enter(&fmodsw_lock);
	if ((mid = findmodbyname(name)) != -1) {
		mutex_exit(&fmodsw_lock);
		return (mid);
	}

	for (mid = 0, fmp = fmodsw; mid < fmodcnt; mid++, fmp++)
		if (!ALLOCATED_STREAM(fmp))
			break;

	if (mid == fmodcnt) {
		mutex_exit(&fmodsw_lock);
		return (-1);
	}

	/*
	 * copy in no more chars than will fit in destination
	 */
	(void) strncpy(fmp->f_name, name, sizeof (fmp->f_name) - 1);
	fmp->f_name[FMNAMESZ] = 0;	/* guarantee NULL-termination */
	fmp->f_lock = kmem_alloc(sizeof (kmutex_t), KM_SLEEP);
	fmp->f_count = 0;
	mutex_init(fmp->f_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_exit(&fmodsw_lock);
	return (mid);
}

/*
 * Currently not used. Allocated entries are never released even if
 * the module is uninstalled/unloaded. The same entry will be reused
 * if the module is reloaded.
 */
void
free_fmodsw(fmodsw_impl_t *fmp)
{
	ASSERT(MUTEX_HELD(&fmodsw_lock));

	mutex_destroy(fmp->f_lock);
	kmem_free(fmp->f_lock, sizeof (kmutex_t));
	fmp->f_lock = NULL;
	fmp->f_flag = 0;
	fmp->f_str = NULL;
	bzero(fmp->f_name, sizeof (fmp->f_name));
}

int
fmod_hold(int idx)
{
	fmodsw_impl_t *fmp;

	fmp = &fmodsw[idx];
	if (LOADABLE_STREAM(fmp)) {
		if (ALLOCATED_STREAM(fmp)) {
			mutex_enter(fmp->f_lock);
			fmp->f_count++;
			ASSERT(fmp->f_count != 0);	/* Wraparound */
			mutex_exit(fmp->f_lock);
		} else {
			return (1);
		}
	}
	return (0);
}

int
fmod_release(int idx)
{
	fmodsw_impl_t *fmp;

	fmp = &fmodsw[idx];
	if (LOADABLE_STREAM(fmp)) {
		if (ALLOCATED_STREAM(fmp)) {
			mutex_enter(fmp->f_lock);
			ASSERT(fmp->f_count > 0);
			fmp->f_count--;
			mutex_exit(fmp->f_lock);
		} else {
			return (1);
		}
	}
	return (0);
}

/*
 * Return index into fmodsw for a given module's name.
 * If not found and it's one we'll autoload, then try to autoload it.
 * If not found, return -1.
 * The f_count is incremented for loadable streams. This makes sure
 * that it will not get unloaded till this instance is in use.
 */
int
findmod(char *name)
{
	int mid;
	fmodsw_impl_t *fmp;

	/*
	 * mid will be => 0 if the module has been previously loaded
	 */
	mutex_enter(&fmodsw_lock);
	mid = findmodbyname(name);
	mutex_exit(&fmodsw_lock);
	if (mid == -1) {
		/*
		 * The module has never been loaded.
		 * First try and load the module.
		 * modload() will call mod_installstrmod() to
		 * create a new fmodsw entry. If it does not, then
		 * this is not a streams module or some other
		 * failure occured; In either case fail.
		 */
		if (modload("strmod", name) == -1) {
			return (-1);
		}
		mutex_enter(&fmodsw_lock);
		mid = findmodbyname(name);
		mutex_exit(&fmodsw_lock);
		if (mid == -1)
			return (-1);
	}
	/*
	 * Entries are never removed, so no need for any locks here
	 */
	fmp = &fmodsw[mid];

	/*
	 * XXX I don't think there's such a thing
	 * as a non-loadable streams module anymore
	 */
	if (LOADABLE_STREAM(fmp)) {
		mutex_enter(fmp->f_lock);
		/*
		 * another thread could unload the module
		 * after we just loaded it, so loop.
		 */
		while (!STREAM_INSTALLED(fmp)) {
			mutex_exit(fmp->f_lock);
			if (modload("strmod", name) == -1) {
				return (-1);
			}
			mutex_enter(fmp->f_lock);
		}
		fmp->f_count++;
		ASSERT(fmp->f_count != 0);	/* Wraparound */
		mutex_exit(fmp->f_lock);
	}
	return (mid);
}

/*
 * Lookup a module by name and return the fmodsw index.
 */
int
findmodbyname(char *name)
{
	int i, j;

	ASSERT(MUTEX_HELD(&fmodsw_lock));

	for (i = 0; i < fmodcnt; i++) {
		if (fmodsw[i].f_name[0] == '\0')
			continue;
		for (j = 0; j < FMNAMESZ + 1; j++) {
			if (fmodsw[i].f_name[j] != name[j])
				break;
			if (name[j] == '\0')
				return (i);
		}
	}
	return (-1);
}

/*
 * This routine only works if the module has already been
 * loaded once before
 */
int
findmodbyindex(int idx)
{
	fmodsw_impl_t *fmp;

	if (idx < 0 || idx >= fmodcnt)
		return (0);

	fmp = &fmodsw[idx];
	if (!ALLOCATED_STREAM(fmp))
		return (0);

	if (LOADABLE_STREAM(fmp)) {
		mutex_enter(fmp->f_lock);
		while (!STREAM_INSTALLED(fmp)) {
			mutex_exit(fmp->f_lock);
			if (modload("strmod", fmp->f_name) == -1) {
				return (0);
			}
			mutex_enter(fmp->f_lock);
		}
		fmp->f_count++;
		ASSERT(fmp->f_count != 0);	/* Wraparound */
		mutex_exit(fmp->f_lock);
	}
	return (1);
}

/*
 * Set the QBACK or QB_BACK flag in the given queue for
 * the given priority band.
 */
void
setqback(queue_t *q, unsigned char pri)
{
	int i;
	qband_t *qbp;
	qband_t **qbpp;

	ASSERT(MUTEX_HELD(QLOCK(q)));
	if (pri != 0) {
		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					cmn_err(CE_WARN,
					    "setqback: can't allocate qband\n");
					return;
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;
		qbp->qb_flag |= QB_BACK;
	} else {
		q->q_flag |= QBACK;
	}
}

/* ARGSUSED */
int
strcopyin(void *from, void *to, size_t len, char *fmt, int copyflag)
{
	if (copyflag & U_TO_K) {
		ASSERT((copyflag & K_TO_K) == 0);
		if (ncopyin(from, to, len, fmt))
			return (EFAULT);
	} else {
		ASSERT(copyflag & K_TO_K);
		bcopy(from, to, len);
	}
	return (0);
}

/* ARGSUSED */
int
strcopyout(void *from, void *to, size_t len, char *fmt, int copyflag)
{
	if (copyflag & U_TO_K) {
		if (ncopyout(from, to, len, fmt))
			return (EFAULT);
	} else {
		ASSERT(copyflag & K_TO_K);
		bcopy(from, to, len);
	}
	return (0);
}

/*
 * strsignal_nolock() posts a signal to the process(es) at the stream head.
 * It assumes that the stream head lock is already held, whereas strsignal()
 * acquires the lock first.  This routine was created because a few callers
 * release the stream head lock before calling only to re-acquire it after
 * it returns.
 */
void
strsignal_nolock(stdata_t *stp, int sig, int32_t band)
{
	ASSERT(MUTEX_HELD(&stp->sd_lock));
	switch (sig) {
	case SIGPOLL:
		if (stp->sd_sigflags & S_MSG)
			strsendsig(stp->sd_siglist, S_MSG, (uchar_t)band, 0);
		break;

	default:
		if (stp->sd_pgidp) {
			pgsignal(stp->sd_pgidp, sig);
		}
		break;
	}
}

void
strsignal(stdata_t *stp, int sig, int32_t band)
{
	TRACE_3(TR_FAC_STREAMS_FR, TR_SENDSIG,
		"strsignal:%X, %X, %X", stp, sig, band);

	mutex_enter(&stp->sd_lock);
	switch (sig) {
	case SIGPOLL:
		if (stp->sd_sigflags & S_MSG)
			strsendsig(stp->sd_siglist, S_MSG, (uchar_t)band, 0);
		break;

	default:
		if (stp->sd_pgidp) {
			pgsignal(stp->sd_pgidp, sig);
		}
		break;
	}
	mutex_exit(&stp->sd_lock);
}

void
strhup(stdata_t *stp)
{
	mutex_enter(&stp->sd_lock);
	if (stp->sd_sigflags & S_HANGUP)
		strsendsig(stp->sd_siglist, S_HANGUP, 0, 0);
	mutex_exit(&stp->sd_lock);
	pollwakeup(&stp->sd_pollist, POLLHUP);
}

void
stralloctty(sess_t *sp, stdata_t *stp)
{
	mutex_enter(&stp->sd_lock);
	mutex_enter(&pidlock);
	stp->sd_sidp = sp->s_sidp;
	stp->sd_pgidp = sp->s_sidp;
	PID_HOLD(stp->sd_pgidp);
	PID_HOLD(stp->sd_sidp);
	mutex_exit(&pidlock);
	mutex_exit(&stp->sd_lock);
}

void
strfreectty(stdata_t *stp)
{
	mutex_enter(&stp->sd_lock);
	pgsignal(stp->sd_pgidp, SIGHUP);
	mutex_enter(&pidlock);
	PID_RELE(stp->sd_pgidp);
	PID_RELE(stp->sd_sidp);
	stp->sd_pgidp = NULL;
	stp->sd_sidp = NULL;
	mutex_exit(&pidlock);
	mutex_exit(&stp->sd_lock);
	if (!(stp->sd_flag & STRHUP))
		strhup(stp);
}

/*
 * Unlink "all" persistent links.
 * This is only called from specvfsops.c.
 */
void
strpunlink(cred_t *crp)
{
	stdata_t *stp;
	int rval;

	/*
	 * for each allocated stream head, call munlinkall()
	 * with flag of LINKPERSIST to unlink any/all persistent
	 * links for the device.
	 */
	mutex_enter(&strresources);
	for (stp = stream_head_list; stp; stp = stp->sd_next) {
		if (stp->sd_wrq == NULL)
			continue;
		(void) munlinkall(stp, LINKIOCTL|LINKPERSIST, crp, &rval);
	}
	mutex_exit(&strresources);
}

void
strctty(stdata_t *stp)
{
	extern vnode_t *makectty();
	proc_t *pp = curproc;
	sess_t *sp = pp->p_sessp;

	mutex_enter(&stp->sd_lock);
	/*
	 * No need to hold the session lock or do a TTYHOLD,
	 * because this is the only thread that can be the
	 * session leader and not have a controlling tty.
	 */
	if ((stp->sd_flag & (STRHUP|STRDERR|STWRERR|STPLEX)) == 0 &&
	    stp->sd_sidp == NULL &&		/* not allocated as ctty */
	    sp->s_sidp == pp->p_pidp &&		/* session leader */
	    sp->s_flag != SESS_CLOSE &&		/* session is not closing */
	    sp->s_vp == NULL) {			/* without ctty */
		mutex_exit(&stp->sd_lock);
		ASSERT(stp->sd_pgidp == NULL);
		alloctty(pp, makectty(stp->sd_vnode));
		stralloctty(sp, stp);
		mutex_enter(&stp->sd_lock);
		stp->sd_flag |= STRISTTY;	/* just to be sure */
	}
	mutex_exit(&stp->sd_lock);
}

/*
 * Special freemsg that handles M_PASSFP messages.
 * Used by flushq, flushband and flush_syncq.
 */
void
freemsg_flush(mblk_t *mp)
{
	if (mp->b_datap->db_type == M_PASSFP) {
		file_t *fp = ((struct k_strrecvfd *)mp->b_rptr)->fp;

		(void) closef(fp);
	}
	freemsg(mp);
}

/*
 * enable first back queue with svc procedure.
 * Use pri == -1 to avoid the setqback
 */
void
backenable(queue_t *q, int pri)
{
	queue_t	*nq;

	/*
	 * our presence might not prevent other modules in our own
	 * stream from popping/pushing since the caller of getq might not
	 * have a claim on the queue (some drivers do a getq on somebody
	 * else's queue - they know that the queue itself is not going away
	 * but the framework has to guarantee q_next in that stream.)
	 */
	claimstr(q);

	/* find nearest back queue with service proc */
	for (nq = backq(q); nq && !nq->q_qinfo->qi_srvp; nq = backq(nq)) {
		ASSERT(STRMATED(q->q_stream) || STREAM(q) == STREAM(nq));
	}

	if (nq) {
		kthread_id_t freezer;
		/*
		 * backenable can be called either with no locks held
		 * or with the stream frozen (the latter occurs when a module
		 * calls rmvq with the stream frozen.) If the stream is frozen
		 * by the caller the caller will hold all qlocks in the stream.
		 */
		freezer = STREAM(q)->sd_freezer;
		if (freezer != curthread) {
			mutex_enter(QLOCK(nq));
		}
#ifdef DEBUG
		else {
			ASSERT(frozenstr(q));
			ASSERT(MUTEX_HELD(QLOCK(q)));
			ASSERT(MUTEX_HELD(QLOCK(nq)));
		}
#endif
		if (pri != -1)
			setqback(nq, pri);
		qenable_locked(nq);
		if (freezer != curthread)
			mutex_exit(QLOCK(nq));
	}
	releasestr(q);
}

/*
 * Return the appropriate errno when one of flags_to_check is set
 * in sd_flags. Uses the exported error routines if they are set.
 * Will return 0 if non error is set (or if the exported error routines
 * do not return an error).
 *
 * If there is both a read and write error to check we prefer the read error.
 * Also, give preference to recorded errno's over the error functions.
 * The flags that are handled are:
 *	STPLEX		return EINVAL
 *	STRDERR		return sd_rerror (and clear if STRDERRNONPERSIST)
 *	STWRERR		return sd_werror (and clear if STWRERRNONPERSIST)
 *	STRHUP		return sd_werror
 *
 * If the caller indicates that the operation is a peek a nonpersistent error
 * is not cleared.
 */
int
strgeterr(stdata_t *stp, int32_t flags_to_check, int ispeek)
{
	int32_t sd_flag = stp->sd_flag & flags_to_check;
	int error = 0;

	ASSERT(MUTEX_HELD(&stp->sd_lock));
	ASSERT((flags_to_check & ~(STRDERR|STWRERR|STRHUP|STPLEX)) == 0);
	if (sd_flag & STPLEX)
		error = EINVAL;
	else if (sd_flag & STRDERR) {
		error = stp->sd_rerror;
		if ((stp->sd_flag & STRDERRNONPERSIST) && !ispeek) {
			/*
			 * Read errors are non-persistent i.e. discarded once
			 * returned to a non-peeking caller,
			 */
			stp->sd_rerror = 0;
			stp->sd_flag &= ~STRDERR;
		}
		if (error == 0 && stp->sd_rderrfunc != NULL) {
			int clearerr = 0;

			error = (*stp->sd_rderrfunc)(stp->sd_vnode, ispeek,
						&clearerr);
			if (clearerr) {
				stp->sd_flag &= ~STRDERR;
				stp->sd_rderrfunc = NULL;
			}
		}
	} else if (sd_flag & STWRERR) {
		error = stp->sd_werror;
		if ((stp->sd_flag & STWRERRNONPERSIST) && !ispeek) {
			/*
			 * Write errors are non-persistent i.e. discarded once
			 * returned to a non-peeking caller,
			 */
			stp->sd_werror = 0;
			stp->sd_flag &= ~STWRERR;
		}
		if (error == 0 && stp->sd_wrerrfunc != NULL) {
			int clearerr = 0;

			error = (*stp->sd_wrerrfunc)(stp->sd_vnode, ispeek,
						&clearerr);
			if (clearerr) {
				stp->sd_flag &= ~STWRERR;
				stp->sd_wrerrfunc = NULL;
			}
		}
	} else if (sd_flag & STRHUP) {
		/* sd_werror set when STRHUP */
		error = stp->sd_werror;
	}
	return (error);
}


/*
 * single-thread open/close/push/pop
 * for twisted streams also
 */
int
strsyncplumb(stdata_t *stp, int flag, int cmd)
{
	int waited = 1;
	int error = 0;

	if (STRMATED(stp)) {
		struct stdata *stmatep = stp->sd_mate;

		STRLOCKMATES(stp);
		while (waited) {
			waited = 0;
			while (stmatep->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
				if ((cmd == I_POP) &&
				    (flag & (FNDELAY|FNONBLOCK))) {
					STRUNLOCKMATES(stp);
					return (EAGAIN);
				}
				waited = 1;
				mutex_exit(&stp->sd_lock);
				if (!cv_wait_sig(&stmatep->sd_monitor,
				    &stmatep->sd_lock)) {
					mutex_exit(&stmatep->sd_lock);
					return (EINTR);
				}
				mutex_exit(&stmatep->sd_lock);
				STRLOCKMATES(stp);
			}
			while (stp->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
				if ((cmd == I_POP) &&
					(flag & (FNDELAY|FNONBLOCK))) {
					STRUNLOCKMATES(stp);
					return (EAGAIN);
				}
				waited = 1;
				mutex_exit(&stmatep->sd_lock);
				if (!cv_wait_sig(&stp->sd_monitor,
				    &stp->sd_lock)) {
					mutex_exit(&stp->sd_lock);
					return (EINTR);
				}
				mutex_exit(&stp->sd_lock);
				STRLOCKMATES(stp);
			}
			if (stp->sd_flag & (STRDERR|STWRERR|STRHUP|STPLEX)) {
				error = strgeterr(stp,
					STRDERR|STWRERR|STRHUP|STPLEX, 0);
				if (error != 0) {
					STRUNLOCKMATES(stp);
					return (error);
				}
			}
		}
		stp->sd_flag |= STRPLUMB;
		STRUNLOCKMATES(stp);
	} else {
		mutex_enter(&stp->sd_lock);
		while (stp->sd_flag & (STWOPEN|STRCLOSE|STRPLUMB)) {
			if (((cmd == I_POP) || (cmd == _I_REMOVE)) &&
			    (flag & (FNDELAY|FNONBLOCK))) {
				mutex_exit(&stp->sd_lock);
				return (EAGAIN);
			}
			if (!cv_wait_sig(&stp->sd_monitor, &stp->sd_lock)) {
				mutex_exit(&stp->sd_lock);
				return (EINTR);
			}
			if (stp->sd_flag & (STRDERR|STWRERR|STRHUP|STPLEX)) {
				error = strgeterr(stp,
					STRDERR|STWRERR|STRHUP|STPLEX, 0);
				if (error != 0) {
					mutex_exit(&stp->sd_lock);
					return (error);
				}
			}
		}
		stp->sd_flag |= STRPLUMB;
		mutex_exit(&stp->sd_lock);
	}
	return (0);
}

/*
 * This describes how the STREAMS framework handles synchronization
 * during open/push and close/pop.
 * The key interfaces for open and close are qprocson and qprocsoff,
 * respectively. While the close case in general is harder both open
 * have close have significant similarities.
 *
 * During close the STREAMS framework has to both ensure that there
 * are no stale references to the queue pair (and syncq) that
 * are being closed and also provide the guarantees that are documented
 * in qprocsoff(9F).
 * If there are stale references to the queue that is closing it can
 * result in kernel memory corruption or kernel panics.
 *
 * Note that is it up to the module/driver to ensure that it itself
 * does not have any stale references to the closing queues once its close
 * routine returns. This includes:
 *  - Cancelling any timeout/bufcall/qtimeout/qbufcall callback routines
 *    associated with the queues. For timeout and bufcall callbacks the
 *    module/driver also has to ensure (or wait for) any callbacks that
 *    are in progress.
 *  - If the module/driver is using esballoc it has to ensure that any
 *    esballoc free functions do not refer to a queue that has closed.
 *    (Note that in general the close routine can not wait for the esballoc'ed
 *    messages to be freed since that can cause a deadlock.)
 *  - Cancelling any interrupts that refer to the closing queues and
 *    also ensuring that there are no interrupts in progress that will
 *    refer to the closing queues once the close routine returns.
 *  - For multiplexors removing any driver global state that refers to
 *    the closing queue and also ensuring that there are no threads in
 *    the multiplexor that has picked up a queue pointer but not yet
 *    finished using it.
 *
 * In addition, a driver/module can only reference the q_next pointer
 * in its open, close, put, or service procedures or in a
 * qtimeout/qbufcall callback procedure executing "on" the correct
 * stream. Thus it can not reference the q_next pointer in an interrupt
 * routine or a timeout, bufcall or esballoc callback routine. Likewise
 * it can not reference q_next of a different queue e.g. in a mux that
 * passes messages from one queues put/service procedure to another queue.
 * In all the cases when the driver/module can not access the q_next
 * field it must use the *next* versions e.g. canputnext instead of
 * canput(q->q_next) and putnextctl instead of putctl(q->q_next, ...).
 *
 *
 * Assuming that the driver/module conforms to the above constraints
 * the STREAMS framework has to avoid stale references to q_next for all
 * the framework internal cases which include (but are not limited to):
 *  - Threads in canput/canputnext/backenable and elsewhere that are
 *    walking q_next.
 *  - Messages on a syncq that have a reference to the queue through b_queue.
 *  - Messages on an outer perimeter (syncq) that have a reference to the
 *    queue through b_queue.
 *  - Threads that use q_nfsrv (e.g. canput and set_qfull) to find a queue.
 *    Note that only canput and bcanput use q_nfsrv without any locking.
 *
 * The STREAMS framework providing the qprocsoff(9F) guarantees means that
 * after qprocsoff returns, the framework has to ensure that no threads can
 * enter the put or service routines for the closing read or write-side queue.
 * In addition to preventing "direct" entry into the put procedures
 * the framework also has to prevent messages being drained from
 * the syncq or the outer perimeter.
 * XXX Note that currently qdetach does relies on D_MTOCEXCL as the only
 * mechanism to prevent qwriter(PERIM_OUTER) from running after
 * qprocsoff has returned.
 * Note that if a module/driver uses put(9F) on one of its own queues
 * it is up to the module/driver to ensure that the put() doesn't
 * get called when the queue is closing.
 *
 *
 * The framework aspects of the above "contract" is implemented by
 * qprocsoff, removeq, and strlock:
 *  - qprocsoff (disable_svc) sets QWCLOSE to prevent runservice from
 *    entering the service procedures.
 *  - strlock acquires the sd_lock and sd_reflock to prevent putnext,
 *    canputnext, backenable etc from dereferencing the q_next that will
 *    soon change.
 *  - strlock waits for sd_refcnt to be zero to wait for e.g. any canputnext
 *    or other q_next walker that uses claimstr/releasestr to finish.
 *  - optionally for every syncq in the stream strlock acquires all the
 *    sq_lock's and waits for all sq_counts to drop to a value that indicates
 *    that no thread executes in the put or service procedures and that no
 *    thread is draining into the module/driver. This ensures that no
 *    open, close, put, service, or qtimeout/qbufcall callback procedure is
 *    currently executing hence no such thread can end up with the old stale
 *    q_next value and no canput/backenable can have the old stale
 *    q_nfsrv/q_next. Note that this step is optional as described
 *    below for QNEXTLESS streams.
 *  - qdetach (wait_svc) makes sure that any scheduled or running threads
 *    have either finished or observed the QWCLOSE flag and gone away.
 *
 * If all the modules and driver in the stream has set _D_QNEXTLESS
 * the modules/driver guarantee that:
 *  - they will never access the read side q_next. Thus for instance, they
 *    will use canputnext(q) instead of canput(q->q_next).
 *  - the driver will cut of all its sources of threads (like interrupts)
 *    that might issue calls (e.g. put, putnext, qwriter) on the closing queue
 *    before calling qprocsoff. (Non-QNEXTLESS drivers only have to ensure
 *    this before their close routing returns.)
 * Note that the above constraints have been carefully crafted to be able to
 * optimize the open of /dev/ip, autopush of tcp and close of the resulting
 * stream so that there is no need to acquiring the sq_lock on the D_MTPERMOD
 * IP syncq.
 *
 * In a stream where all the modules and the driver has set _D_QNEXTLESS
 * (we call this a QNEXTLESS stream) the removeq/strlock work can be
 * significantly reduced based on the following set of observations:
 *  - No thread will directly or indirectly access the read side q_next or
 *    q_nfsrv. The modules/driver will not directly use q_next hence not
 *    use canput and all the framework routines use claimstr when accessing
 *    q_next and q_nfsrv except canput and bcanput.
 *    Thus for a module being closed there is no need to use
 *    sq_lock plus a zero sq_count for queues except the closing queue
 *    pair itself.
 *  - Since the service procedures always run exclusively (at the inner
 *    perimeter) a service procedure will not be running once the close
 *    routine has started. Should this change i.e. should SQ_CISVC be
 *    implemented then qprocsoff would have to call wait_svc after calling
 *    disable_svc in order to ensure that the service procedures are done
 *    when qprocsoff returns.
 *  - For a driver being closed there is no need to wait for its own sq_count
 *    to reach zero since:
 *	- there is no module below the driver calling putnext
 *	- there is no module above it calling putnext (the stream head
 *	  is closing the device hence all references to the stream head
 *	  are gone).
 *	- the driver has asserted that it will not itself call e.g. put(9F) or
 *	  qwriter on the closing queues after it calls qprocsoff. (This
 *	  includes the drivers put and service procedures as well as any
 *	  (q)timeout/(q)bufcall callback and interrupt routine.)
 *    This implies that once qprocsoff has been called there can be no
 *    more messages being added to the syncq thus removeq just has to handle
 *    the set of messages already on the syncq and does not need to hold
 *    sq_lock to prevent any fill_syncq/drain_syncq attempts.
 *
 * Note that qprocson/insertq by default uses the same strong locking
 * as qprocsoff. This ensures that an open, close, put or service procedure
 * in the stream will not see any change to any q_next in the stream.
 * In practice such a strong requirement might not be needed but it
 * is preserved for backwards compatibility.
 * For a QNEXTLESS stream insertq avoids acquiring any sq_lock's.
 */


/*
 * Get all the locks necessary to change q_next.
 *
 * Wait for sd_refcnt to reach 0 and, if do_sqlock is set, wait for sq_count in
 * all syncqs to reach an acceptable level: sq_occount for mysq and 0 for the
 * other syncqs. This allows multiple threads in a D_MTPERMOD syncq with
 * hot open/close routines by letting qprocsoff/strlock wait until
 * all threads except the open/close ones go away.
 * Currently do_sqlock is not set for the open of the drivers/modules of
 * the QNEXTLESS stream and during the driver's close of a QNEXTLESS stream.
 *
 * This routine is subject to starvation since it does not set any flag to
 * prevent threads from entering a module in the stream(i.e. sq_count can
 * increase on some syncq while it is waiting on some other syncq.)
 *
 * Assumes that only one thread attempts to call strlock for a given
 * stream. If this is not the case the two threads would deadlock.
 * This assumption is guaranteed since strlock is only called by insertq
 * and removeq and streams plumbing changes are single-threaded for
 * a given stream using the STWOPEN, STRCLOSE, and STRPLUMB flags.
 *
 * For pipes, it is not difficult to atomically designate a pair of streams
 * to be mated. Once mated atomically by the framework the twisted pair remain
 * configured that way until dismantled atomically by the framework.
 * When plumbing takes place on a twisted stream it is necessary to ensure that
 * this operation is done exclusively on the twisted stream since two such
 * operations, each initiated on different ends of the pipe will deadlock
 * waiting for each other to complete.
 *
 * On entry, no locks should be held.
 * The locks acquired and held by strlock depends on a few factors.
 * - If do_sqlock is set all the syncq locks in the sqlist will be acquired
 *   and held on exit and all sq_count are at an acceptable level.
 *   If !do_sqlock, these locks are not held on exit.
 * - In all cases, sd_lock and sd_reflock are acquired and held on exit with
 *   sd_refcnt being zero.
 */

static void
strlock(struct stdata *stp, sqlist_t *sqlist, syncq_t *mysq,
	boolean_t do_sqlock)
{
	syncql_t *sql, *sql2;
retry:
	/*
	 * Wait for any claimstr to go away.
	 */
	if (STRMATED(stp)) {
		struct stdata *stp1, *stp2;

		STRLOCKMATES(stp);
		/*
		 * Note that the selection of locking order is not
		 * important, just that they are always aquired in
		 * the same order.  To assure this, we choose this
		 * order based on the value of the pointer, and since
		 * the pointer will not change for the life of this
		 * pair, we will always grab the locks in the same
		 * order (and hence, prevent deadlocks).
		 */
		if (&(stp->sd_lock) > &((stp->sd_mate)->sd_lock)) {
			stp1 = stp;
			stp2 = stp->sd_mate;
		} else {
			stp2 = stp;
			stp1 = stp->sd_mate;
		}
		mutex_enter(&stp1->sd_reflock);
		if (stp1->sd_refcnt > 0) {
			STRUNLOCKMATES(stp);
			cv_wait(&stp1->sd_monitor, &stp1->sd_reflock);
			mutex_exit(&stp1->sd_reflock);
			goto retry;
		}
		mutex_enter(&stp2->sd_reflock);
		if (stp2->sd_refcnt > 0) {
			STRUNLOCKMATES(stp);
			mutex_exit(&stp1->sd_reflock);
			cv_wait(&stp2->sd_monitor, &stp2->sd_reflock);
			mutex_exit(&stp2->sd_reflock);
			goto retry;
		}
		STREAM_PUTLOCKS_ENTER(stp1);
		STREAM_PUTLOCKS_ENTER(stp2);
	} else {
		mutex_enter(&stp->sd_lock);
		mutex_enter(&stp->sd_reflock);
		while (stp->sd_refcnt > 0) {
			mutex_exit(&stp->sd_lock);
			cv_wait(&stp->sd_monitor, &stp->sd_reflock);
			if (mutex_tryenter(&stp->sd_lock) == 0) {
				mutex_exit(&stp->sd_reflock);
				mutex_enter(&stp->sd_lock);
				mutex_enter(&stp->sd_reflock);
			}
		}
		STREAM_PUTLOCKS_ENTER(stp);
	}

	if (!do_sqlock)
		return;

	for (sql = sqlist->sqlist_head; sql; sql = sql->sql_next) {
		syncq_t *sq = sql->sql_sq;
		uint16_t count;
		uint_t maxcnt;

		mutex_enter(SQLOCK(sq));
		count = sq->sq_count;
		ASSERT(sq->sq_occount <= count);
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
		if (count == 0)
			continue;
		/*
		 * Only for _D_MTOCSHARED should sq_occount ever exceed 1.
		 */
		ASSERT(sq != mysq || (sq->sq_type & SQ_CIOC) ||
		    sq->sq_occount == 1);
		if ((sq == mysq) && (count == sq->sq_occount))
			continue;
		/* Failed - drop all locks that we have acquired so far */
		if (STRMATED(stp)) {
			STREAM_PUTLOCKS_EXIT(stp);
			STREAM_PUTLOCKS_EXIT(stp->sd_mate);
			STRUNLOCKMATES(stp);
			mutex_exit(&stp->sd_reflock);
			mutex_exit(&stp->sd_mate->sd_reflock);
		} else {
			STREAM_PUTLOCKS_EXIT(stp);
			mutex_exit(&stp->sd_lock);
			mutex_exit(&stp->sd_reflock);
		}
		for (sql2 = sqlist->sqlist_head; sql2 != sql;
		    sql2 = sql2->sql_next) {
			SQ_PUTLOCKS_EXIT(sql2->sql_sq);
			mutex_exit(SQLOCK(sql2->sql_sq));
		}
		/*
		 * Wait until the count in this syncq drops to an acceptable
		 * level
		 */
		maxcnt = (sq == mysq) ? sq->sq_occount : 0;
		while (count > maxcnt) {
			sq->sq_flags |= SQ_WANTWAKEUP;
			SQ_PUTLOCKS_EXIT(sq);
			cv_wait(&sq->sq_wait, SQLOCK(sq));
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
			maxcnt = (sq == mysq) ? sq->sq_occount : 0;
		}
		ASSERT(count == maxcnt);
		SQ_PUTLOCKS_EXIT(sq);
		mutex_exit(SQLOCK(sq));
		goto retry;
	}
}

/*
 * Drop all the locks that strlock acquired.
 */
static void
strunlock(struct stdata *stp, sqlist_t *sqlist, boolean_t do_sqlock)
{
	syncql_t *sql;

	if (STRMATED(stp)) {
		STREAM_PUTLOCKS_EXIT(stp);
		STREAM_PUTLOCKS_EXIT(stp->sd_mate);
		STRUNLOCKMATES(stp);
		mutex_exit(&stp->sd_reflock);
		mutex_exit(&stp->sd_mate->sd_reflock);
	} else {
		STREAM_PUTLOCKS_EXIT(stp);
		mutex_exit(&stp->sd_lock);
		mutex_exit(&stp->sd_reflock);
	}

	if (!do_sqlock)
		return;

	for (sql = sqlist->sqlist_head; sql; sql = sql->sql_next) {
		SQ_PUTLOCKS_EXIT(sql->sql_sq);
		mutex_exit(SQLOCK(sql->sql_sq));
	}
}


/*
 * Given two read queues, insert a new single one after another.
 *
 * This routine acquires all the necessary locks in order to change
 * q_next and related pointer using strlock().
 * It depends on the stream head ensuring that there are no concurrent
 * insertq or removeq on the same stream. The stream head ensures this
 * using the flags STWOPEN, STRCLOSE, and STRPLUMB.
 *
 * Note that for QNEXTLESS streams, no syncq locks are held across
 * the q_next change. This change could be applied to all streams
 * since, unlike removeq, there is no problem of stale pointers
 * when adding a module to the stream. Thus drivers/modules that
 * do a canput(rq->q_next) would never get a closed/freed queue
 * pointer even if we applied this optimization to all streams.
 *
 * XXX : In order to reduce the risk, this optimization is applied
 * only to QNEXTLESS streams at the moment.
 */
void
insertq(struct stdata *stp, queue_t *new)
{
	queue_t	*after;
	queue_t *wafter;
	queue_t *wnew = _WR(new);
	sqlist_t *sqlist;
	boolean_t have_fifo = B_FALSE;
	boolean_t qnextless;

	if (new->q_flag & _QINSERTING) {
		ASSERT(stp->sd_vnode->v_type != VFIFO);
		after = new->q_next;
		wafter = _WR(new->q_next);
	} else {
		after = _RD(stp->sd_wrq);
		wafter = stp->sd_wrq;
	}

	TRACE_4(TR_FAC_STREAMS_FR, TR_INSERTQ,
		"insertq:%X, %X %X, %X",
		after->q_qinfo, new->q_qinfo, after, new);
	ASSERT(after->q_flag & QREADR);
	ASSERT(new->q_flag & QREADR);

	qnextless = (stp->sd_flag & STRQNEXTLESS);
	if (qnextless) {
		strlock(stp, NULL, NULL, B_FALSE);
	} else {
		sqlist = build_sqlist(new, stp, STRMATED(stp));
		strlock(stp, sqlist, new->q_syncq, B_TRUE);
	}

	/* Do we have a FIFO? */
	if (wafter->q_next == after) {
		have_fifo = B_TRUE;
		wnew->q_next = new;
	} else {
		wnew->q_next = wafter->q_next;
	}
	new->q_next = after;

	set_nfsrv_ptr(new, wnew, after, wafter);
	set_nbsrv_ptr(new, wnew, after, wafter);
	/*
	 * set_nfsrv_ptr()/set_nbsrv_ptr() needs to know if it is
	 * an insertion or not.  So only reset this flag after calling
	 * them.
	 */
	new->q_flag &= ~_QINSERTING;

	if (have_fifo) {
		wafter->q_next = wnew;
	} else {
		if (wafter->q_next)
			_OTHERQ(wafter->q_next)->q_next = new;
		wafter->q_next = wnew;
	}

	set_qend(new);
	/* The QEND flag might have to be updated for the upstream guy */
	set_qend(after);

	/*
	 * update QNEXTHOT bit value for affected queues.
	 */
	set_qnexthot(new);
	set_qnexthot(new->q_next);
	set_qnexthot(wnew->q_next);

	ASSERT(_SAMESTR(new) == O_SAMESTR(new));
	ASSERT(_SAMESTR(wnew) == O_SAMESTR(wnew));
	ASSERT(_SAMESTR(after) == O_SAMESTR(after));
	ASSERT(_SAMESTR(wafter) == O_SAMESTR(wafter));
	strsetuio(stp);

	if (qnextless) {
		strunlock(stp, NULL, B_FALSE);
	} else {
		strunlock(stp, sqlist, B_TRUE);
		free_sqlist(sqlist);
	}
}


/*
 * This is called for a QNEXTLESS stream to make sure that
 * there are no threads currently running in the perimeter
 * except close. It returns B_TRUE if there are messages
 * in the syncq that needs to be propagated.
 */
static boolean_t
strlockqpair(stdata_t *stp, queue_t *qp)
{
	syncq_t *rsq = qp->q_syncq;
	syncq_t *wsq = _WR(qp)->q_syncq;
	sqlist_t syncqlist;
	sqlist_t *sqlist;
	boolean_t do_sqlock;

	sqlist = &syncqlist;
	sqlist->sqlist_head = NULL;
	sqlist->sqlist_index = 0;
	sqlist->sqlist_size = sizeof (sqlist_t);
	sqlist_insert(sqlist, rsq);
	if (rsq != wsq)
		sqlist_insert(sqlist, wsq);
	strlock(stp, sqlist, rsq, B_TRUE);
	if (qp->q_syncqmsgs != 0 || _WR(qp)->q_syncqmsgs != 0) {
		do_sqlock = B_TRUE;
	} else {
		/*
		 * NOTE : If a module uses put(9F) on one of
		 * its own queues, messages can appear on the
		 * syncq or even put might call the module's
		 * put procedure, after we made the check above.
		 * It is up to the module to ensure that the put()
		 * doesn't get called when the queue is closing.
		 * Otherwise put might get called after qprocsoff
		 * returns.
		 */
		do_sqlock = B_FALSE;
	}
	SQ_PUTLOCKS_EXIT(rsq);
	mutex_exit(SQLOCK(rsq));
	if (rsq != wsq) {
		SQ_PUTLOCKS_EXIT(wsq);
		mutex_exit(SQLOCK(wsq));
	}
	return (do_sqlock);
}

/*
 * Given a read queue, unlink it from any neighbors.
 *
 * This routine acquires all the necessary locks in order to
 * change q_next and related pointers and also guard against
 * stale references (e.g. through q_next) to the queue that
 * is being removed. It also plays part of the role in ensuring
 * that the module's/driver's put procedure doesn't get called
 * after qprocsoff returns.
 *
 * Removeq depends on the stream head ensuring that there are
 * no concurrent insertq or removeq on the same stream. The
 * stream head ensures this using the flags STWOPEN, STRCLOSE and
 * STRPLUMB.
 *
 * The set of locks needed to remove the queue is different in
 * different cases:
 *
 * If we are popping the *module* of a QNEXTLESS stream, we have
 * strlockqpair() acquire the sd_lock and the sd_reflock to make
 * sure that no new messages can appear through putnext. strlockqpair()
 * then waits for the sq_count in the queue being removed to drop to zero
 * to make sure that no put, service, or drain_syncq is running, which
 * the streams framework has to ensure as part of qprocsoff. If we have
 * any messages on the syncq for the closing queue they need to be
 * propagated to the next queue. If this is the case we acquire the syncq
 * locks of all the syncqs in the stream and do the propagation.
 *
 * If we are closing the *driver* of a QNEXTLESS stream and if we have
 * messages that could possibly drain into the queue being removed, we
 * take the slow path of acquiring all the syncq locks, wait for syncq
 * count to drop to zero and propagate the messages. If there are no
 * messages that could possibly drain, we neither acquire the syncq lock
 * nor wait for the sq_count to drop to zero. This relies on the QNEXTLESS
 * driver ensuring that it has closed off any sources of messages prior
 * to calling qprocsoff so that no new messages will arrive on the syncq
 * destined to the closing queue while qprocsoff is running.
 * As the service procedure currently always runs exclusively,
 * service can never get called while the close is in progress.
 * And, like for all other close cases qprocsoff + qdetach always
 * ensures that QENAB|QINSERVICE go away without running the service
 * procedure. See comment for SQ_CISVC in strsubr.h should SQ_CISVC
 * be implemented.
 *
 * If it is not a QNEXTLESS stream, then acquire sd_lock, sd_reflock,
 * and all the syncq locks in the stream after waiting for the syncq
 * reference count to drop to 0 indicating that no non-close threads are
 * present anywhere in the stream. This ensures that any module/driver
 * can reference q_next in its open, close, put, or service procedures.
 */
void
removeq(queue_t *qp)
{
	queue_t *wqp = _WR(qp);
	struct stdata *stp = STREAM(qp);
	sqlist_t *sqlist = NULL;
	boolean_t isdriver;
	boolean_t do_sqlock;
	boolean_t qnextless;
	int moved;
	syncq_t *rsq;
	syncql_t *sql;

	ASSERT(stp);

	TRACE_4(TR_FAC_STREAMS_FR, TR_REMOVEQ,
		"removeq:%X (%X) from %X and %X",
		qp->q_qinfo, qp, wqp->q_next, qp->q_next);
	ASSERT(qp->q_flag&QREADR);

	isdriver = (qp->q_flag & QISDRV);
	qnextless = (stp->sd_flag & STRQNEXTLESS);
	rsq = qp->q_syncq;

	if (qnextless) {
		if (!isdriver) {
			do_sqlock = strlockqpair(stp, qp);
			if (do_sqlock) {
				/*
				 * This is the slow path. Acquire all the
				 * syncq locks in the right order.
				 */
				sqlist = build_sqlist(qp, stp, STRMATED(stp));
				for (sql = sqlist->sqlist_head; sql;
					sql = sql->sql_next) {
					syncq_t *sq = sql->sql_sq;
					mutex_enter(SQLOCK(sq));
					SQ_PUTLOCKS_ENTER(sq);
				}
			}
		} else {
			/*
			 * Before calling the close of the driver, entersq()
			 * waits for SQ_GOAWAY to ensure that there will be
			 * no messages in the syncq for this queue. But
			 * messages can land on the syncq before qprocsoff is
			 * called by the driver. We need to check for the
			 * messages and if they are present, we take the slow
			 * path of acquiring all the locks and propagating
			 * them. Note that no new messages can appear, once
			 * qprocsoff is called by the QNEXTLESS driver as it
			 * ensures that it will not send any new messages.
			 *
			 * NOTE: If both Q_SQQUEUED and Q_SQDRAINING are not set
			 * in q_sqflags, this queue is not being drained
			 * currently by background thread and no one will pick
			 * it for draining in the future since it is not on the
			 * syncq list and nothing new may appear on this list at
			 * since the stream is closing. So, when both flags are
			 * not set it means that there is no background activity
			 * for this queue and this activity will not get started
			 * in the future, so no one is going to reference this
			 * queue. QNEXTLESS driver also makes a guarantee that
			 * all putnexts are finished at this point.
			 */
			if ((qp->q_sqflags & (Q_SQQUEUED | Q_SQDRAINING)) ||
			    (wqp->q_sqflags & (Q_SQQUEUED | Q_SQDRAINING))) {
				do_sqlock = B_TRUE;
				sqlist = build_sqlist(qp, stp, STRMATED(stp));
				strlock(stp, sqlist, rsq, B_TRUE);
			} else {
				ASSERT(qp->q_sqhead == NULL);
				ASSERT(_WR(qp)->q_sqhead == NULL);
				ASSERT(qp->q_syncqmsgs == 0);
				ASSERT(_WR(qp)->q_syncqmsgs == 0);
				ASSERT(! qp->q_draining);
				ASSERT(! _WR(qp)->q_draining);
				do_sqlock = B_FALSE;
				strlock(stp, NULL, NULL, B_FALSE);
			}
		}
		/*
		 * Assert that no thread is active in qp for a module
		 * other than this thread.
		 */
		ASSERT(isdriver || rsq->sq_count == 1);
	} else {
		do_sqlock = B_TRUE;
		sqlist = build_sqlist(qp, stp, STRMATED(stp));
		strlock(stp, sqlist, rsq, B_TRUE);
	}


	reset_nfsrv_ptr(qp, wqp, stp);
	reset_nbsrv_ptr(qp, wqp, stp);

	ASSERT(wqp->q_next == NULL || backq(qp)->q_next == qp);
	ASSERT(qp->q_next == NULL || backq(wqp)->q_next == wqp);
	/* Do we have a FIFO? */
	if (wqp->q_next == qp) {
		stp->sd_wrq->q_next = _RD(stp->sd_wrq);
	} else {
		if (wqp->q_next)
			backq(qp)->q_next = qp->q_next;
		if (qp->q_next)
			backq(wqp)->q_next = wqp->q_next;
	}

	/* The QEND flag might have to be updated for the upstream guy */
	if (qp->q_next)
		set_qend(qp->q_next);

	/*
	 * update QNEXTHOT bit value for affected queues.
	 */
	set_qnexthot(qp->q_next);
	if (wqp->q_next != qp)
		set_qnexthot(wqp->q_next);

	ASSERT(_SAMESTR(stp->sd_wrq) == O_SAMESTR(stp->sd_wrq));
	ASSERT(_SAMESTR(_RD(stp->sd_wrq)) == O_SAMESTR(_RD(stp->sd_wrq)));

	/*
	 * Move any messages destined for the put procedures to the next
	 * syncq in line. Otherwise free them.
	 */
	moved = 0;
	if (do_sqlock) {
		/*
		 * Quick check to see whether there are any messages or
		 * events.
		 */
		if (qp->q_syncqmsgs != 0 || (qp->q_syncq->sq_flags & SQ_EVENTS))
			moved += propagate_syncq(qp);
		if (wqp->q_syncqmsgs != 0 ||
		    (wqp->q_syncq->sq_flags & SQ_EVENTS))
			moved += propagate_syncq(wqp);
	}
	strsetuio(stp);
	strunlock(stp, sqlist, do_sqlock);
	if (do_sqlock)
		free_sqlist(sqlist);
	/*
	 * Make sure any messages that were propagated are drained.
	 * Also clear any QFULL bit caused by messages that were propagated.
	 */

	if (qp->q_next != NULL) {
		clr_qfull(qp);
		/*
		 * For the driver calling qprocsoff, propagate_syncq
		 * frees all the messages instead of putting it in
		 * the stream head
		 */
		if (!isdriver && (moved > 0))
			emptysq(qp->q_next->q_syncq);
	}
	if (wqp->q_next != NULL) {
		clr_qfull(wqp);
		/*
		 * We come here for any pop of a module except for the
		 * case of driver being removed. We don't call emptysq
		 * if we did not move any messages. This will avoid holding
		 * PERMOD syncq locks in emptysq
		 */
		if (moved > 0)
			emptysq(wqp->q_next->q_syncq);
	}
}

/*
 * Prevent further entry by setting a flag (like SQ_FROZEN, SQ_BLOCKED or
 * SQ_WRITER) on a syncq.
 * If maxcnt is not -1 it assumes that caller has "maxcnt" claim(s) on the
 * sync queue and waits until sq_count reaches maxcnt.
 *
 * if maxcnt is -1 there's no need to grab sq_putlocks since the caller
 * does not care about putnext threads that are in the middle of calling put
 * entry points.
 *
 * This routine is used for both inner and outer syncqs.
 */
static void
blocksq(syncq_t *sq, ushort_t flag, int maxcnt)
{
	uint16_t count = 0;

	mutex_enter(SQLOCK(sq));
	/*
	 * Wait for SQ_FROZEN/SQ_BLOCKED to be reset.
	 * SQ_FROZEN will be set if there is a frozen stream that has a
	 * queue which also refers to this "shared" syncq.
	 * SQ_BLOCKED will be set if there is "off" queue which also
	 * refers to this "shared" syncq.
	 */
	if (maxcnt != -1) {
		SQ_PUTCOUNT_CLRFAST(sq);
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
	}
	sq->sq_needexcl++;
	ASSERT(sq->sq_needexcl != 0);	/* wraparound */

	while ((sq->sq_flags & flag) ||
	    (maxcnt != -1 && count > (unsigned)maxcnt)) {
		sq->sq_flags |= SQ_WANTWAKEUP;
		if (maxcnt != -1) {
			SQ_PUTLOCKS_EXIT(sq);
		}
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		if (maxcnt != -1) {
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
		}
	}
	sq->sq_needexcl--;
	sq->sq_flags |= flag;
	ASSERT(maxcnt == -1 || count == maxcnt);
	if (maxcnt != -1) {
		SQ_PUTLOCKS_EXIT(sq);
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Reset a flag that was set with blocksq.
 *
 * Can not use this routine to reset SQ_WRITER.
 *
 * If "isouter" is set then the syncq is assumed to be an outer perimeter
 * and drain_syncq is not called. Instead we rely on the qwriter_outer thread
 * to handle the queued qwriter operations.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
static void
unblocksq(syncq_t *sq, uint16_t resetflag, int isouter)
{
	uint16_t flags;

	mutex_enter(SQLOCK(sq));
	ASSERT(resetflag != SQ_WRITER);
	ASSERT(sq->sq_flags & resetflag);
	flags = sq->sq_flags & ~resetflag;
	sq->sq_flags = flags;
	if (flags & (SQ_QUEUED | SQ_WANTWAKEUP)) {
		if (flags & SQ_WANTWAKEUP) {
			flags &= ~SQ_WANTWAKEUP;
			cv_broadcast(&sq->sq_wait);
		}
		sq->sq_flags = flags;
		if ((flags & SQ_QUEUED) && !(flags & (SQ_STAYAWAY|SQ_EXCL))) {
			if (!isouter)
				drain_syncq(sq);
		}
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Reset a flag that was set with blocksq.
 * Does not drain the syncq. Use emptysq() for that.
 * Returns 1 if SQ_QUEUED is set. Otherwise 0.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
static int
dropsq(syncq_t *sq, uint16_t resetflag)
{
	uint16_t flags;

	mutex_enter(SQLOCK(sq));
	ASSERT(sq->sq_flags & resetflag);
	flags = sq->sq_flags & ~resetflag;
	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	sq->sq_flags = flags;
	mutex_exit(SQLOCK(sq));
	if (flags & SQ_QUEUED)
		return (1);
	return (0);
}

/*
 * Empty all the messages on a syncq.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
static void
emptysq(syncq_t *sq)
{
	uint16_t flags;

	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	if ((flags & SQ_QUEUED) && !(flags & (SQ_STAYAWAY|SQ_EXCL))) {
		drain_syncq(sq);
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Ordered insert while removing duplicates.
 */
static void
sqlist_insert(sqlist_t *sqlist, syncq_t *sqp)
{
	syncql_t *sqlp, **prev_sqlpp, *new_sqlp;

	prev_sqlpp = &sqlist->sqlist_head;
	while ((sqlp = *prev_sqlpp) != NULL) {
		if (sqlp->sql_sq >= sqp) {
			if (sqlp->sql_sq == sqp)	/* duplicate */
				return;
			break;
		}
		prev_sqlpp = &sqlp->sql_next;
	}
	new_sqlp = &sqlist->sqlist_array[sqlist->sqlist_index++];
	ASSERT((char *)new_sqlp < (char *)sqlist + sqlist->sqlist_size);
	new_sqlp->sql_next = sqlp;
	new_sqlp->sql_sq = sqp;
	*prev_sqlpp = new_sqlp;
}

/*
 * Walk the write side queues until we hit either the driver
 * or a twist in the stream (_SAMESTR will return false in both
 * these cases) then turn around and walk the read side queues
 * back up to the stream head.
 */
static void
sqlist_insertall(sqlist_t *sqlist, queue_t *q)
{
	while (q != NULL) {
		sqlist_insert(sqlist, q->q_syncq);
		if (_SAMESTR(q))
			q = q->q_next;
		else if (!(q->q_flag & QREADR))
			q = _RD(q);
		else
			return;
	}
}

/*
 * Allocate and build a list of all syncqs in a stream and the syncq(s)
 * associated with the "q" parameter. The resulting list is sorted in a
 * canonical order and is free of duplicates.
 * Assumes the passed queue is a _RD(q).
 */
static sqlist_t *
build_sqlist(queue_t *q, struct stdata *stp, int do_twist)
{
	size_t sqlist_size;
	sqlist_t *sqlist;

	/*
	 * Allocate 2 syncql_t's for each pushed module. Note that
	 * the sqlist_t structure already has 4 syncql_t's built in:
	 * 2 for the stream head, and 2 for the driver/other stream head.
	 */
	sqlist_size = 2 * sizeof (syncql_t) * stp->sd_pushcnt +
		sizeof (sqlist_t);
	if (do_twist)
		sqlist_size += 2 * sizeof (syncql_t) * stp->sd_mate->sd_pushcnt;
	sqlist = kmem_alloc(sqlist_size, KM_SLEEP);

	sqlist->sqlist_head = NULL;
	sqlist->sqlist_size = sqlist_size;
	sqlist->sqlist_index = 0;

	/*
	 * start with the current queue/qpair
	 */
	ASSERT(q->q_flag & QREADR);

	sqlist_insert(sqlist, q->q_syncq);
	sqlist_insert(sqlist, _WR(q)->q_syncq);

	sqlist_insertall(sqlist, stp->sd_wrq);
	if (do_twist)
		sqlist_insertall(sqlist, stp->sd_mate->sd_wrq);

	return (sqlist);
}

/*
 * Free the list created by build_sqlist()
 */
static void
free_sqlist(sqlist_t *sqlist)
{
	kmem_free(sqlist, sqlist->sqlist_size);
}

/*
 * Prevent any new entries into any syncq in this stream.
 * Used by freezestr.
 */
void
strblock(queue_t *q)
{
	struct stdata	*stp;
	syncql_t	*sql;
	sqlist_t	*sqlist;

	q = _RD(q);

	stp = STREAM(q);
	ASSERT(stp != NULL);

	/*
	 * Get a sorted list with all the duplicates removed containing
	 * all the syncqs referenced by this stream.
	 */
	sqlist = build_sqlist(q, stp, 0);
	for (sql = sqlist->sqlist_head; sql != NULL; sql = sql->sql_next)
		blocksq(sql->sql_sq, SQ_FROZEN, -1);
	free_sqlist(sqlist);
}

/*
 * Release the block on new entries into this stream
 */
void
strunblock(queue_t *q)
{
	struct stdata	*stp;
	syncql_t	*sql;
	sqlist_t	*sqlist;
	int		drain_needed;

	q = _RD(q);

	/*
	 * Get a sorted list with all the duplicates removed containing
	 * all the syncqs referenced by this stream.
	 * Have to drop the SQ_FROZEN flag on all the syncqs before
	 * starting to drain them; otherwise the draining might
	 * cause a freezestr in some module on the stream (which
	 * would deadlock.)
	 */
	stp = STREAM(q);
	ASSERT(stp != NULL);
	sqlist = build_sqlist(q, stp, 0);
	drain_needed = 0;
	for (sql = sqlist->sqlist_head; sql != NULL; sql = sql->sql_next)
		drain_needed += dropsq(sql->sql_sq, SQ_FROZEN);
	if (drain_needed) {
		for (sql = sqlist->sqlist_head; sql != NULL;
		    sql = sql->sql_next)
			emptysq(sql->sql_sq);
	}
	free_sqlist(sqlist);
}

/* XXX should be #ifdef DEBUG but it is used by zs_hdlc.c! */
int
qprocsareon(queue_t *rq)
{
	if (rq->q_next == NULL)
		return (0);
	return (_WR(rq->q_next)->q_next == _WR(rq));
}

#ifdef DEBUG
int
qclaimed(queue_t *q)
{
	uint_t count;

	count = q->q_syncq->sq_count;
	SUM_SQ_PUTCOUNTS(q->q_syncq, count);
	return (count != 0);
}

/*
 * Check if anyone has frozen this stream with freezestr
 */
int
frozenstr(queue_t *q)
{
	return ((q->q_syncq->sq_flags & SQ_FROZEN) != 0);
}
#endif /* DEBUG */

/*
 * Enter a queue.
 * Obsoleted interface. Should not be used.
 */
void
enterq(queue_t *q)
{
	entersq(q->q_syncq, SQ_CALLBACK);
}

void
leaveq(queue_t *q)
{
	leavesq(q->q_syncq, SQ_CALLBACK);
}

/*
 * Enter a perimeter. c_inner and c_outer specifies which concurrency bits
 * to check.
 * Wait if SQ_QUEUED is set to preserve ordering between messages and qwriter
 * calls and the running of open, close and service procedures.
 *
 * if c_inner bit is set no need to grab sq_putlocks since we don't care
 * if other threads have entered or are entering put entry point.
 *
 * if c_inner bit is set it might have been posible to use
 * sq_putlocks/sq_putcounts instead of SQLOCK/sq_count (e.g. to optimize
 * open/close path for IP) but since the count may need to be decremented in
 * qwait() we wouldn't know which counter to decrement. Currently counter is
 * selected by current cpu_seqid and current CPU can change at any moment. XXX
 * in the future we might use curthread id bits to select the counter and this
 * would stay constant across routine calls.
 */
void
entersq(syncq_t *sq, int entrypoint)
{
	uint16_t	count = 0;
	uint16_t	flags;
	uint16_t	type;
	uint_t		c_inner = entrypoint & SQ_CI;
	uint_t		c_outer = entrypoint & SQ_CO;

	/*
	 * Increment ref count to keep closes out of this queue.
	 */
	ASSERT(sq);
	ASSERT(c_inner && c_outer);
	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	type = sq->sq_type;
	if (!(type & c_inner)) {
		/* Make sure all putcounts now use slowlock. */
		SQ_PUTCOUNT_CLRFAST(sq);
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
	}
	/*
	 * Wait until we can enter the inner perimeter.
	 * If we want exclusive access we wait until sq_count is 0.
	 * We have to do this before entering the outer perimeter in order
	 * to preserve put/close message ordering.
	 */
	sq->sq_needexcl++;
	ASSERT(sq->sq_needexcl != 0);	/* wraparound */

	while ((flags & SQ_GOAWAY) || (!(type & c_inner) && count != 0)) {
		sq->sq_flags = flags | SQ_WANTWAKEUP;
		if (!(type & c_inner)) {
			SQ_PUTLOCKS_EXIT(sq);
		}
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		if (!(type & c_inner)) {
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
		}
		flags = sq->sq_flags;
	}
	sq->sq_needexcl--;

	/* Check if we need to enter the outer perimeter */
	if (!(type & c_outer)) {
		/*
		 * We have to enter the outer perimeter exclusively before
		 * we can increment sq_count to avoid deadlock. This implies
		 * that we have to re-check sq_flags and sq_count.
		 *
		 * is it possible to have c_inner set when c_outer is not set?
		 */
		if (!(type & c_inner)) {
			SQ_PUTLOCKS_EXIT(sq);
		}
		mutex_exit(SQLOCK(sq));
		outer_enter(sq->sq_outer, SQ_GOAWAY);
		mutex_enter(SQLOCK(sq));
		flags = sq->sq_flags;
		/*
		 * there should be no need to recheck sq_putcounts
		 * because outer_enter() has already waited for them to clear
		 * after setting SQ_WRITER.
		 */
		count = sq->sq_count;
#ifdef DEBUG
		/*
		 * SUMCHECK_SQ_PUTCOUNTS should return the sum instead
		 * of doing an ASSERT internally. Others should do
		 * something like
		 *	 ASSERT(SUMCHECK_SQ_PUTCOUNTS(sq) == 0);
		 * without the need to #ifdef DEBUG it.
		 */
		SUMCHECK_SQ_PUTCOUNTS(sq, 0);
#endif
		while ((flags & (SQ_EXCL|SQ_BLOCKED|SQ_FROZEN)) ||
		    (!(type & c_inner) && count != 0)) {
			sq->sq_flags = flags | SQ_WANTWAKEUP;
			cv_wait(&sq->sq_wait, SQLOCK(sq));
			count = sq->sq_count;
			flags = sq->sq_flags;
		}
	}

	sq->sq_count++;
	ASSERT(sq->sq_count != 0);	/* Wraparound */
	if (entrypoint == SQ_OPENCLOSE) {
		sq->sq_occount++;
		if (sq->sq_occount == 0)
			cmn_err(CE_PANIC, "entersq: sq_occount wraparound");
	}
	ASSERT(sq->sq_occount <= sq->sq_count);
	if (!(type & c_inner)) {
		/* Exclusive entry */
		ASSERT(sq->sq_count == 1);
		sq->sq_flags |= SQ_EXCL;
		if (type & c_outer) {
			SQ_PUTLOCKS_EXIT(sq);
		}
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * leave a syncq. announce to framework that closes may proceed.
 * c_inner and c_outer specifies which concurrency bits
 * to check.
 *
 * must never be called from driver or module put entry point.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
void
leavesq(syncq_t *sq, int entrypoint)
{
	uint16_t	flags;
	uint16_t	type;
	uint_t		c_outer = entrypoint & SQ_CO;
#ifdef DEBUG
	uint_t		c_inner = entrypoint & SQ_CI;
#endif

	/*
	 * decrement ref count, drain the syncq if possible, and wake up
	 * any waiting close.
	 */
	ASSERT(sq);
	ASSERT(c_inner && c_outer);
	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	type = sq->sq_type;
	if (flags & (SQ_QUEUED|SQ_WANTWAKEUP|SQ_WANTEXWAKEUP)) {

		if (flags & SQ_WANTWAKEUP) {
			flags &= ~SQ_WANTWAKEUP;
			cv_broadcast(&sq->sq_wait);
		}
		if (flags & SQ_WANTEXWAKEUP) {
			flags &= ~SQ_WANTEXWAKEUP;
			cv_broadcast(&sq->sq_exitwait);
		}

		if ((flags & SQ_QUEUED) && !(flags & SQ_STAYAWAY)) {
			/*
			 * The syncq needs to be drained. "Exit" the syncq
			 * before calling drain_syncq.
			 */
			ASSERT(sq->sq_count != 0);
			sq->sq_count--;
			if (entrypoint == SQ_OPENCLOSE) {
				ASSERT(sq->sq_occount != 0);
				sq->sq_occount--;
			}
			ASSERT(sq->sq_occount <= sq->sq_count);
			ASSERT((flags & SQ_EXCL) || (type & c_inner));
			sq->sq_flags = flags & ~SQ_EXCL;
			if (!sq->sq_needexcl)
				SQ_PUTCOUNT_SETFAST(sq);
			drain_syncq(sq);
			mutex_exit(SQLOCK(sq));
			/* Check if we need to exit the outer perimeter */
			/* XXX will this ever be true? */
			if (!(type & c_outer))
				outer_exit(sq->sq_outer);
			return;
		}
	}
	ASSERT(sq->sq_count != 0);
	sq->sq_count--;
	if (entrypoint == SQ_OPENCLOSE) {
		ASSERT(sq->sq_occount != 0);
		sq->sq_occount--;
	}
	ASSERT(sq->sq_occount <= sq->sq_count);
	ASSERT((flags & SQ_EXCL) || (type & c_inner));
	sq->sq_flags = flags & ~SQ_EXCL;
	if (!sq->sq_needexcl)
		SQ_PUTCOUNT_SETFAST(sq);
	mutex_exit(SQLOCK(sq));

	/* Check if we need to exit the outer perimeter */
	if (!(sq->sq_type & c_outer))
		outer_exit(sq->sq_outer);
}

/*
 * Prevent q_next from changing in this stream by incrementing sq_count.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
void
claimq(queue_t *qp, boolean_t openclose)
{
	syncq_t	*sq = qp->q_syncq;

	mutex_enter(SQLOCK(sq));
	sq->sq_count++;
	ASSERT(sq->sq_count != 0);	/* Wraparound */
	if (openclose) {
		sq->sq_occount++;
		if (sq->sq_occount == 0)
			cmn_err(CE_PANIC, "claimq: sq_occount wraparound");
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Undo claimq.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 */
void
releaseq(queue_t *qp, boolean_t openclose)
{
	syncq_t	*sq = qp->q_syncq;
	uint16_t flags;

	mutex_enter(SQLOCK(sq));
	ASSERT(sq->sq_count > 0);
	sq->sq_count--;
	if (openclose) {
		ASSERT(sq->sq_occount != 0);
		sq->sq_occount--;
	}

	flags = sq->sq_flags;
	if (flags & (SQ_WANTWAKEUP|SQ_QUEUED)) {
		if (flags & SQ_WANTWAKEUP) {
			flags &= ~SQ_WANTWAKEUP;
			cv_broadcast(&sq->sq_wait);
		}
		sq->sq_flags = flags;
		if ((flags & SQ_QUEUED) && !(flags & (SQ_STAYAWAY|SQ_EXCL))) {
			drain_syncq(sq);
		}
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * Prevent q_next from changing in this stream by incrementing sd_refcnt.
 */
void
claimstr(queue_t *qp)
{
	struct stdata *stp = STREAM(qp);

	mutex_enter(&stp->sd_reflock);
	stp->sd_refcnt++;
	ASSERT(stp->sd_refcnt != 0);	/* Wraparound */
	mutex_exit(&stp->sd_reflock);
}

/*
 * Undo claimstr.
 */
void
releasestr(queue_t *qp)
{
	struct stdata *stp = STREAM(qp);

	mutex_enter(&stp->sd_reflock);
	ASSERT(stp->sd_refcnt != 0);
	stp->sd_refcnt--;
	cv_broadcast(&stp->sd_monitor);
	mutex_exit(&stp->sd_reflock);
}

static syncq_t *
new_syncq(void)
{
	syncq_t	*sq;

	sq = kmem_cache_alloc(syncq_cache, KM_SLEEP);

	sq->sq_count = 0;
	sq->sq_occount = 0;
	sq->sq_callbflags = 0;
	sq->sq_cancelid = 0;
	sq->sq_private = NULL;
	sq->sq_ciputctrl = NULL;
	sq->sq_nciputctrl = 0;
	sq->sq_next = NULL;
	sq->sq_needexcl = 0;
	sq->sq_svcflags = 0;
	sq->sq_nqueues = 0;
	sq->sq_pri = 0;

	return (sq);
}

static void
free_syncq(syncq_t *sq)
{
	ASSERT(sq->sq_head == NULL);
	ASSERT(sq->sq_outer == NULL);
	ASSERT(sq->sq_callbpend == NULL);
	ASSERT(sq->sq_onext == NULL && sq->sq_oprev == NULL);

	if (sq->sq_ciputctrl != NULL) {
		ASSERT(sq->sq_nciputctrl == n_ciputctrl - 1);
		SUMCHECK_CIPUTCTRL_COUNTS(sq->sq_ciputctrl,
		    sq->sq_nciputctrl, 0);
		ASSERT(ciputctrl_cache != NULL);
		kmem_cache_free(ciputctrl_cache, sq->sq_ciputctrl);
		sq->sq_ciputctrl = NULL;
		sq->sq_nciputctrl = 0;
	}
	kmem_cache_free(syncq_cache, sq);
}

/* Outer perimeter code */

/*
 * The outer syncq uses the fields and flags in the syncq slightly
 * differently from the inner syncqs.
 *	sq_count	Incremented when there are pending or running
 *			writers at the outer perimeter to prevent the set of
 *			inner syncqs that belong to the outer perimeter from
 *			changing.
 *	sq_head/tail	List of deferred qwriter(OUTER) operations.
 *
 *	SQ_BLOCKED	Set to prevent traversing of sq_next,sq_prev while
 *			inner syncqs are added to or removed from the
 *			outer perimeter.
 *	SQ_QUEUED	sq_head/tail has messages or eventsqueued.
 *
 *	SQ_WRITER	A thread is currently traversing all the inner syncqs
 *			setting the SQ_WRITER flag.
 */

/*
 * Get write access at the outer perimeter.
 * Note that read access is done by entersq, putnext, and put by simply
 * incrementing sq_count in the inner syncq.
 *
 * Waits until "flags" is no longer set in the outer to prevent multiple
 * threads from having write access at the same time. SQ_WRITER has to be part
 * of "flags".
 *
 * Increases sq_count on the outer syncq to keep away outer_insert/remove
 * until the outer_exit is finished.
 *
 * outer_enter is vulnerable to starvation since it does not prevent new
 * threads from entering the inner syncqs while it is waiting for sq_count to
 * go to zero.
 */
void
outer_enter(syncq_t *outer, uint16_t flags)
{
	syncq_t	*sq;
	int	wait_needed;
	uint16_t	count;

	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);
	ASSERT(flags & SQ_WRITER);

retry:
	mutex_enter(SQLOCK(outer));
	while (outer->sq_flags & flags) {
		outer->sq_flags |= SQ_WANTWAKEUP;
		cv_wait(&outer->sq_wait, SQLOCK(outer));
	}

	ASSERT(!(outer->sq_flags & SQ_WRITER));
	outer->sq_flags |= SQ_WRITER;
	outer->sq_count++;
	ASSERT(outer->sq_count != 0);	/* wraparound */
	wait_needed = 0;
	/*
	 * Set SQ_WRITER on all the inner syncqs while holding
	 * the SQLOCK on the outer syncq. This ensures that the changing
	 * of SQ_WRITER is atomic under the outer SQLOCK.
	 */
	for (sq = outer->sq_onext; sq != outer; sq = sq->sq_onext) {
		mutex_enter(SQLOCK(sq));
		SQ_PUTCOUNT_CLRFAST(sq);
		sq->sq_flags |= SQ_WRITER;
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
		if (count != 0)
			wait_needed = 1;
		SQ_PUTLOCKS_EXIT(sq);
		mutex_exit(SQLOCK(sq));
	}
	mutex_exit(SQLOCK(outer));

	/*
	 * Get everybody out of the syncqs sequentially.
	 * Note that we don't actually need to aqiure the PUTLOCKS, since
	 * we have already cleared the fastbit, and set QWRITER.  By
	 * definition, the count can not increase since putnext will
	 * take the slowlock path (and the purpose of aquiring the
	 * putlocks was to make sure it didn't increase while we were
	 * waiting).
	 *
	 * Note that we still aquire the PUTLOCKS to be safe.
	 */
	if (wait_needed) {
		for (sq = outer->sq_onext; sq != outer; sq = sq->sq_onext) {
			mutex_enter(SQLOCK(sq));
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
			while (count != 0) {
				sq->sq_flags |= SQ_WANTWAKEUP;
				SQ_PUTLOCKS_EXIT(sq);
				cv_wait(&sq->sq_wait, SQLOCK(sq));
				count = sq->sq_count;
				SQ_PUTLOCKS_ENTER(sq);
				SUM_SQ_PUTCOUNTS(sq, count);
			}
			SQ_PUTLOCKS_EXIT(sq);
			mutex_exit(SQLOCK(sq));
		}
		/*
		 * Verify that none of the flags got set while we
		 * were waiting for the sq_counts to drop.
		 * If this happens we exit and retry entering the
		 * outer perimeter.
		 */
		mutex_enter(SQLOCK(outer));
		if (outer->sq_flags & (flags & ~SQ_WRITER)) {
			mutex_exit(SQLOCK(outer));
			outer_exit(outer);
			goto retry;
		}
		mutex_exit(SQLOCK(outer));
	}
}

/*
 * Drop the write access at the outer perimeter.
 * Read access is dropped implicitly (by putnext, put, and leavesq) by
 * decrementing sq_count.
 */
/* ARGSUSED */
void
outer_exit(syncq_t *outer)
{
	syncq_t	*sq;
	int	 drain_needed;
	uint16_t flags;

	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);
	ASSERT(MUTEX_NOT_HELD(SQLOCK(outer)));

	/*
	 * Atomically (from the perspective of threads calling become_writer)
	 * drop the write access at the outer perimeter by holding
	 * SQLOCK(outer) across all the dropsq calls and the resetting of
	 * SQ_WRITER.
	 * This defines a locking order between the outer perimeter
	 * SQLOCK and the inner perimeter SQLOCKs.
	 */
	mutex_enter(SQLOCK(outer));
	flags = outer->sq_flags;
	ASSERT(outer->sq_flags & SQ_WRITER);
	if (flags & SQ_QUEUED) {
		write_now(outer);
		flags = outer->sq_flags;
	}

	/*
	 * sq_onext is stable since sq_count has not yet been decreased.
	 * Reset the SQ_WRITER flags in all syncqs.
	 * After dropping SQ_WRITER on the outer syncq we empty all the
	 * inner syncqs.
	 */
	drain_needed = 0;
	for (sq = outer->sq_onext; sq != outer; sq = sq->sq_onext)
		drain_needed += dropsq(sq, SQ_WRITER);
	ASSERT(!(outer->sq_flags & SQ_QUEUED));
	flags &= ~SQ_WRITER;
	if (drain_needed) {
		outer->sq_flags = flags;
		mutex_exit(SQLOCK(outer));
		for (sq = outer->sq_onext; sq != outer; sq = sq->sq_onext)
			emptysq(sq);
		mutex_enter(SQLOCK(outer));
		flags = outer->sq_flags;
	}
	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&outer->sq_wait);
	}
	outer->sq_flags = flags;
	ASSERT(outer->sq_count > 0);
	outer->sq_count--;
	mutex_exit(SQLOCK(outer));
}

/*
 * Add another syncq to an outer perimeter.
 * Block out all other access to the outer perimeter while it is being
 * changed using blocksq.
 * Assumes that the caller has *not* done an outer_enter.
 *
 * Vulnerable to starvation in blocksq.
 */
static void
outer_insert(syncq_t *outer, syncq_t *sq)
{
	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);
	ASSERT(sq->sq_outer == NULL && sq->sq_onext == NULL &&
	    sq->sq_oprev == NULL);	/* Can't be in an outer perimeter */

	/* Get exclusive access to the outer perimeter list */
	blocksq(outer, SQ_BLOCKED, 0);
	ASSERT(outer->sq_flags & SQ_BLOCKED);
	ASSERT(!(outer->sq_flags & SQ_WRITER));

	mutex_enter(SQLOCK(sq));
	sq->sq_outer = outer;
	outer->sq_onext->sq_oprev = sq;
	sq->sq_onext = outer->sq_onext;
	outer->sq_onext = sq;
	sq->sq_oprev = outer;
	mutex_exit(SQLOCK(sq));
	unblocksq(outer, SQ_BLOCKED, 1);
}

/*
 * Remove a syncq from an outer perimeter.
 * Block out all other access to the outer perimeter while it is being
 * changed using blocksq.
 * Assumes that the caller has *not* done an outer_enter.
 *
 * Vulnerable to starvation in blocksq.
 */
static void
outer_remove(syncq_t *outer, syncq_t *sq)
{
	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);
	ASSERT(sq->sq_outer == outer);

	/* Get exclusive access to the outer perimeter list */
	blocksq(outer, SQ_BLOCKED, 0);
	ASSERT(outer->sq_flags & SQ_BLOCKED);
	ASSERT(!(outer->sq_flags & SQ_WRITER));

	mutex_enter(SQLOCK(sq));
	sq->sq_outer = NULL;
	sq->sq_onext->sq_oprev = sq->sq_oprev;
	sq->sq_oprev->sq_onext = sq->sq_onext;
	sq->sq_oprev = sq->sq_onext = NULL;
	mutex_exit(SQLOCK(sq));
	unblocksq(outer, SQ_BLOCKED, 1);
}

/*
 * Queue a deferred qwriter(OUTER) callback for this outer perimeter.
 * If this is the first callback for this outer perimeter then add
 * this outer perimeter to the list of outer perimeters that
 * the qwriter_outer_thread will process.
 *
 * Increments sq_count in the outer syncq to prevent the membership
 * of the outer perimeter (in terms of inner syncqs) to change while
 * the callback is pending.
 */
static void
queue_writer(syncq_t *outer, void (*func)(), queue_t *q, mblk_t *mp)
{
	ASSERT(MUTEX_HELD(SQLOCK(outer)));

	mp->b_prev = (mblk_t *)func;
	mp->b_queue = q;
	mp->b_next = NULL;
	outer->sq_count++;	/* Decremented when dequeued */
	ASSERT(outer->sq_count != 0);	/* Wraparound */
	if (outer->sq_evhead == NULL) {
		/* First message. */
		outer->sq_evhead = outer->sq_evtail = mp;
		outer->sq_flags |= SQ_EVENTS;
		mutex_exit(SQLOCK(outer));
		/* signal qwriter_outer thread */
		queue_writer_work(outer);
	} else {
		ASSERT(outer->sq_flags & SQ_EVENTS);
		outer->sq_evtail->b_next = mp;
		outer->sq_evtail = mp;
		mutex_exit(SQLOCK(outer));
	}
}

/*
 * Try and upgrade to write access at the outer perimeter. If this can
 * not be done without blocking then queue the callback to be done
 * by the qwriter_outer_thread.
 *
 * This routine can only be called from put or service procedures plus
 * asynchronous callback routines that have properly entered to
 * queue (with entersq.) Thus qwriter(OUTER) assumes the caller has one claim
 * on the syncq associated with q.
 */
void
qwriter_outer(queue_t *q, mblk_t *mp, void (*func)())
{
	syncq_t	*osq, *sq, *outer;
	int	failed;
	uint16_t flags;

	osq = q->q_syncq;
	outer = osq->sq_outer;
	if (outer == NULL)
		panic("qwriter(PERIM_OUTER): no outer perimeter");
	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);

	mutex_enter(SQLOCK(outer));
	flags = outer->sq_flags;
	/*
	 * If some thread is traversing sq_next, or if we are blocked by
	 * outer_insert or outer_remove, or if the we already have queued
	 * callbacks, then queue this callback for later processing.
	 *
	 * Also queue the qwriter for an interrupt thread in order
	 * to reduce the time spent running at high IPL.
	 * to identify there are events.
	 */
	if ((flags & SQ_GOAWAY) || (curthread->t_pri >= kpreemptpri)) {
		/*
		 * Queue the become_writer request.
		 * The queueing is atomic under SQLOCK(outer) in order
		 * to synchronize with outer_exit.
		 * queue_writer will drop the outer SQLOCK
		 */
		if (flags & SQ_BLOCKED) {
			/* Must set SQ_WRITER on inner perimeter */
			mutex_enter(SQLOCK(osq));
			osq->sq_flags |= SQ_WRITER;
			mutex_exit(SQLOCK(osq));
		} else {
			if (!(flags & SQ_WRITER)) {
				/*
				 * The outer could have been SQ_BLOCKED thus
				 * SQ_WRITER might not be set on the inner.
				 */
				mutex_enter(SQLOCK(osq));
				osq->sq_flags |= SQ_WRITER;
				mutex_exit(SQLOCK(osq));
			}
			ASSERT(osq->sq_flags & SQ_WRITER);
		}
		queue_writer(outer, func, q, mp);
		return;
	}
	/*
	 * We are half-way to exclusive access to the outer perimeter.
	 * Prevent any outer_enter, qwriter(OUTER), or outer_insert/remove
	 * while the inner syncqs are traversed.
	 */
	outer->sq_count++;
	ASSERT(outer->sq_count != 0);	/* wraparound */
	flags |= SQ_WRITER;
	/*
	 * Check if we can run the function immediately. Mark all
	 * syncqs with the writer flag to prevent new entries into
	 * put and service procedures.
	 *
	 * Set SQ_WRITER on all the inner syncqs while holding
	 * the SQLOCK on the outer syncq. This ensures that the changing
	 * of SQ_WRITER is atomic under the outer SQLOCK.
	 */
	failed = 0;
	for (sq = outer->sq_onext; sq != outer; sq = sq->sq_onext) {
		uint16_t count;
		uint_t	maxcnt = (sq == osq) ? 1 : 0;

		mutex_enter(SQLOCK(sq));
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
		if (sq->sq_count > maxcnt)
			failed = 1;
		sq->sq_flags |= SQ_WRITER;
		SQ_PUTLOCKS_EXIT(sq);
		mutex_exit(SQLOCK(sq));
	}
	if (failed) {
		/*
		 * Some other thread has a read claim on the outer perimeter.
		 * Queue the callback for deferred processing.
		 *
		 * queue_writer will set SQ_QUEUED before we drop SQ_WRITER
		 * so that other qwriter(OUTER) calls will queue their
		 * callbacks as well. queue_writer increments sq_count so we
		 * decrement to compensate for the our increment.
		 *
		 * Dropping SQ_WRITER enables the writer thread to work
		 * on this outer perimeter.
		 */
		outer->sq_flags = flags;
		queue_writer(outer, func, q, mp);
		/* queue_writer dropper the lock */
		mutex_enter(SQLOCK(outer));
		ASSERT(outer->sq_count > 0);
		outer->sq_count--;
		ASSERT(outer->sq_flags & SQ_WRITER);
		flags = outer->sq_flags;
		flags &= ~SQ_WRITER;
		if (flags & SQ_WANTWAKEUP) {
			flags &= ~SQ_WANTWAKEUP;
			cv_broadcast(&outer->sq_wait);
		}
		outer->sq_flags = flags;
		mutex_exit(SQLOCK(outer));
		return;
	} else {
		outer->sq_flags = flags;
		mutex_exit(SQLOCK(outer));
	}

	/* Can run it immediately */
	(*func)(q, mp);

	outer_exit(outer);
}

/*
 * Dequeue all writer callbacks from the outer perimeter and run them.
 */
static void
write_now(syncq_t *outer)
{
	mblk_t		*mp;
	queue_t		*q;
	void	(*func)();

	ASSERT(MUTEX_HELD(SQLOCK(outer)));
	ASSERT(outer->sq_outer == NULL && outer->sq_onext != NULL &&
	    outer->sq_oprev != NULL);
	while ((mp = outer->sq_evhead) != NULL) {
		/*
		 * queues cannot be placed on the queuelist on the outer
		 * perimiter.
		 */
		ASSERT(!(outer->sq_flags & SQ_MESSAGES));
		ASSERT((outer->sq_flags & SQ_EVENTS));

		outer->sq_evhead = mp->b_next;
		if (outer->sq_evhead == NULL) {
			outer->sq_evtail = NULL;
			outer->sq_flags &= ~SQ_EVENTS;
		}
		ASSERT(outer->sq_count != 0);
		outer->sq_count--;	/* Incremented when enqueued. */
		mutex_exit(SQLOCK(outer));
		/*
		 * Drop the message if the queue is closing.
		 * Make sure that the queue is "claimed" when the callback
		 * is run in order to satisfy various ASSERTs.
		 */
		q = mp->b_queue;
		func = (void (*)())mp->b_prev;
		ASSERT(func != NULL);
		mp->b_next = mp->b_prev = NULL;
		if (q->q_flag & QWCLOSE) {
			freemsg(mp);
		} else {
			claimq(q, B_FALSE);
			(*func)(q, mp);
			releaseq(q, B_FALSE);
		}
		mutex_enter(SQLOCK(outer));
	}
	ASSERT(MUTEX_HELD(SQLOCK(outer)));
}


/*
 * Writer thread operations
 */

/*
 * Queue an outer perimeter to be processes by the qwriter_outer thread.
 */
static void
queue_writer_work(syncq_t *outer)
{
	struct writer_work	*ww;

	ASSERT(MUTEX_NOT_HELD(SQLOCK(outer)));

	ww = kmem_alloc(sizeof (struct writer_work), KM_SLEEP);
	ww->ww_outer = outer;
	ww->ww_next = NULL;
	mutex_enter(&writer_lock);
	if (writer_work == NULL) {
		writer_work = writer_tail = ww;
		cv_signal(&writer_wait);
	} else {
		writer_tail->ww_next = ww;
		writer_tail = ww;
	}
	mutex_exit(&writer_lock);
}

static void
qwriter_outer_thread(void)
{
	struct writer_work *ww;
	syncq_t	*outer;
	callb_cpr_t cprinfo;

	CALLB_CPR_INIT(&cprinfo, &writer_lock, callb_generic_cpr, "qot");
	for (;;) {
		mutex_enter(&writer_lock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		while ((ww = writer_work) == NULL) {
			cv_wait(&writer_wait, &writer_lock);
		}
		CALLB_CPR_SAFE_END(&cprinfo, &writer_lock);
		writer_work = ww->ww_next;
		mutex_exit(&writer_lock);
		outer = ww->ww_outer;
		kmem_free((caddr_t)ww, sizeof (struct writer_work));

		/*
		 * Note that SQ_WRITER is used on the outer perimeter
		 * to signal that a qwriter(OUTER) is either investigating
		 * running or that it is actually running a function.
		 */
		outer_enter(outer, SQ_BLOCKED|SQ_WRITER);

		/*
		 * All inner syncq are empty and have SQ_WRITER set
		 * to block entering the outer perimeter.
		 *
		 * We do not need to explicitly call write_now since
		 * outer_exit does it for us.
		 */
		outer_exit(outer);
	}
	/* NOTREACHED */
}

/*
 * The list of messages on the inner syncq is effectively hashed
 * by destination queue.  These destination queues are doubly
 * linked lists (hopefully) in priority order.  Messages are then
 * put on the queue referenced by the q_sqhead/q_sqtail elements.
 * Additional messages are linked together by the b_next/b_prev
 * elements in the mblk, with (similar to putq()) the first message
 * having a NULL b_prev and the last message having a NULL b_next.
 *
 * Events, such as qwriter callbacks, are put onto a list in FIFO
 * order referenced by sq_evhead, and sq_evtail.  This is a singly
 * linked list, and messages here MUST be processed in the order queued.
 */

/*
 * Run the events on the syncq event list (sq_evhead).
 * Assumes there is only one claim on the syncq, it is
 * already exclusive (SQ_EXCL set), and the SQLOCK held.
 * Messages here are processed in order, with the SQ_EXCL bit
 * held all the way through till the last message is processed.
 */
void
sq_run_events(syncq_t *sq)
{
	mblk_t		*bp;
	queue_t		*qp;
	uint16_t	flags = sq->sq_flags;
	void		(*func)();

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT((sq->sq_outer == NULL && sq->sq_onext == NULL &&
		sq->sq_oprev == NULL) ||
		(sq->sq_outer != NULL && sq->sq_onext != NULL &&
		sq->sq_oprev != NULL));

	ASSERT(flags & SQ_EXCL);
	ASSERT(sq->sq_count == 1);

	/*
	 * We need to process all of the events on this list.  It
	 * is possible that new events will be added while we are
	 * away processing a callback, so on every loop, we start
	 * back at the beginning of the list.
	 */
	/*
	 * We have to reaccess sq_evhead since there is a
	 * possibility of a new entry while we were running
	 * the callback.
	 */
	for (bp = sq->sq_evhead; bp != NULL; bp = sq->sq_evhead) {
		ASSERT(bp->b_queue->q_syncq == sq);
		ASSERT(sq->sq_flags & SQ_EVENTS);

		qp = bp->b_queue;
		func = (void (*)())bp->b_prev;
		ASSERT(func != NULL);

		/*
		 * Messages from the event queue must be taken off in
		 * FIFO order.
		 */
		ASSERT(sq->sq_evhead == bp);
		sq->sq_evhead = bp->b_next;

		if (bp->b_next == NULL) {
			/* Deleting last */
			ASSERT(sq->sq_evtail == bp);
			sq->sq_evtail = NULL;
			sq->sq_flags &= ~SQ_EVENTS;
		}
		bp->b_prev = bp->b_next = NULL;
		ASSERT(bp->b_datap->db_ref != 0);

		mutex_exit(SQLOCK(sq));

		(*func)(qp, bp);

		mutex_enter(SQLOCK(sq));
		/*
		 * re-read the flags, since they could have changed.
		 */
		flags = sq->sq_flags;
		ASSERT(flags & SQ_EXCL);
	}
	ASSERT(sq->sq_evhead == NULL && sq->sq_evtail == NULL);
	ASSERT(!(sq->sq_flags & SQ_EVENTS));

	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	if (flags & SQ_WANTEXWAKEUP) {
		flags &= ~SQ_WANTEXWAKEUP;
		cv_broadcast(&sq->sq_exitwait);
	}
	sq->sq_flags = flags;
}

/*
 * Put messages on the event list.
 * If we can go exclusive now, do so and process the event list, otherwise
 * let the last claim service this list (or wake the sqthread).
 * This procedure assumes SQLOCK is held.  To run the event list, it
 * must be called with no claims.
 */
static void
sqfill_events(syncq_t *sq, queue_t *q, mblk_t *mp, void (*func)())
{
	uint16_t count;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT(func != NULL);

	/*
	 * This is a callback.  Add it to the list of callbacks
	 * and see about upgrading.
	 */
	SQ_PUTCOUNT_CLRFAST(sq);
	mp->b_prev = (mblk_t *)func;
	mp->b_queue = q;
	mp->b_next = NULL;
	if (sq->sq_evhead == NULL) {
		sq->sq_evhead = sq->sq_evtail = mp;
		sq->sq_flags |= SQ_EVENTS;
	} else {
		ASSERT(sq->sq_evtail != NULL);
		ASSERT(sq->sq_evtail->b_next == NULL);
		ASSERT(sq->sq_flags & SQ_EVENTS);
		sq->sq_evtail->b_next = mp;
		sq->sq_evtail = mp;
	}
	/*
	 * We have set SQ_EVENTS, so threads will have to
	 * unwind out of the perimiter, and new entries will
	 * not grab a putlock.  But we still need to know
	 * how many threads have already made a claim to the
	 * syncq, so grab the putlocks, and sum the counts.
	 * If there are no claims on the syncq, we can upgrade
	 * to exclusive, and run the event list.
	 * NOTE: We hold the SQLOCK, so we can just grab the
	 * putlocks.
	 */
	count = sq->sq_count;
	SQ_PUTLOCKS_ENTER(sq);
	SUM_SQ_PUTCOUNTS(sq, count);
	/*
	 * We have no claim, so we need to check if there
	 * are no others, then we can upgrade.
	 */
	/*
	 * There are currently no claims on
	 * the syncq by this thread (at least on this entry). The thread who has
	 * the claim should drain syncq.
	 */
	if (count > 0) {
		/*
		 * Can't upgrade - other threads inside.
		 */
		SQ_PUTLOCKS_EXIT(sq);
		return;
	}
	/*
	 * Need to set SQ_EXCL and make a claim on the syncq.
	 */
	ASSERT((sq->sq_flags & SQ_EXCL) == 0);
	sq->sq_flags |= SQ_EXCL;
	ASSERT(sq->sq_count == 0);
	sq->sq_count++;
	SQ_PUTLOCKS_EXIT(sq);

	/* Process the events list */
	sq_run_events(sq);

	/*
	 * Release our claim...
	 */
	sq->sq_count--;

	/*
	 * And release SQ_EXCL.
	 * We don't need to acquire the putlocks to release
	 * SQ_EXCL, since we are exclusive, and hold the SQLOCK.
	 */
	sq->sq_flags &= ~SQ_EXCL;

	/*
	 * sq_run_events should have released SQ_EXCL
	 */
	ASSERT(!(sq->sq_flags & SQ_EXCL));

	/*
	 * If anything happened while we were running the
	 * events (or was there before), we need to process
	 * them now.  We shouldn't be exclusive sine we
	 * released the perimiter above (plus, we asserted
	 * for it).
	 */
	if (!(sq->sq_flags & SQ_STAYAWAY) && (sq->sq_flags & SQ_QUEUED))
		drain_syncq(sq);
	STRASSERT(sq);
}

/*
 * Perform delayed processing. The caller has to make sure that it is safe
 * to enter the syncq (e.g. by checking that none of the SQ_STAYAWAY bits are
 * set.)
 *
 * Assume that the caller has NO claims on the syncq.  However, a claim
 * on the syncq does not indicate that a thread is draining the syncq.
 * There may be more claims on the syncq than there are threads draining
 * (i.e.  #_threads_draining <= sq_count)
 *
 * drain_syncq has to terminate when one of the SQ_STAYAWAY bits gets set
 * in order to preserve qwriter(OUTER) ordering constraints.
 *
 * sq_putcount only needs to be checked when dispatching the queued
 * writer call for CIPUT sync queue, but this is handled in sq_run_events.
 */
void
drain_syncq(syncq_t *sq)
{
	queue_t		*qp;
	uint16_t	count;
	uint16_t	type;
	pri_t 		curpri = curthread->t_pri;
	pri_t 		sqpri = sq->sq_pri;
	uint16_t	flags = sq->sq_flags;

	TRACE_1(TR_FAC_STREAMS_FR, TR_DRAIN_SYNCQ_START,
		"drain_syncq start:%X", sq);
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT((sq->sq_outer == NULL && sq->sq_onext == NULL &&
		sq->sq_oprev == NULL) ||
		(sq->sq_outer != NULL && sq->sq_onext != NULL &&
		sq->sq_oprev != NULL));
	/*
	 * If SQ_EXCL is set, someone else is processing this syncq - let him
	 * finish the job.
	 */
	if (flags & SQ_EXCL) {
		return;
	}

	/*
	 * This routine can be called by a background thread if
	 * it was scheduled by a hi-priority thread.  SO, if there are
	 * NOT messages queued, return (remember, we have the SQLOCK,
	 * and it cannot change until we release it). Wakeup any waiters also.
	 */
	if (!(flags & SQ_QUEUED)) {
		if (flags & SQ_WANTWAKEUP) {
			flags &= ~SQ_WANTWAKEUP;
			cv_broadcast(&sq->sq_wait);
		}
		if (flags & SQ_WANTEXWAKEUP) {
			flags &= ~SQ_WANTEXWAKEUP;
			cv_broadcast(&sq->sq_exitwait);
		}
		sq->sq_flags = flags;
		return;
	}


	/*
	 * The test below should be more "generic, looking for all service lists
	 * priorities values. But this one will work in case of two service
	 * queues.
	 *
	 * If the queue is being disabled, to not reschedule it since it will
	 * not be put on the service list anyway.
	 */
	if ((curpri >= kpreemptpri && sqpri < kpreemptpri) ||
	    (curpri < kpreemptpri && sqpri >= kpreemptpri)) {
		/*
		 * Priority inversion or hijacking detected.
		 * reschedule the syncq to the proper background thread.
		 * If syncq is closing, drain it with whatever priority.
		 */
		if (! (sq->sq_svcflags & SQ_DISABLED)) {
			sqenable(sq);
			return;
		}
	}

	/*
	 * If this is not a concurrent put perimiter, we need to
	 * become exclusive to drain.  Also, if not CIPUT, we would
	 * not have acquired a putlock, so we don't need to check
	 * the putcounts.  If not entering with a claim, we test
	 * for sq_count == 0.
	 */
	type = sq->sq_type;
	if (!(type & SQ_CIPUT)) {
		if (sq->sq_count > 1) {
			return;
		}
		sq->sq_flags |= SQ_EXCL;
	}

	/*
	 * This is where we make a claim to the syncq.
	 * This can either be done by incrementing a putlock, or
	 * the sq_count.  But since we already have the SQLOCK
	 * here, we just bump the sq_count.
	 *
	 * Note that after we make a claim, we need to let the code
	 * fall through to the end of this routine to clean itself
	 * up.  A return in the while loop will put the syncq in a
	 * very bad state.
	 */
	sq->sq_count++;
	ASSERT(sq->sq_count != 0);	/* wraparound */

	while ((flags = sq->sq_flags) & SQ_QUEUED) {
		/*
		 * If we are told to stayaway or went exclusive,
		 * we are done.
		 */
		if (flags & (SQ_STAYAWAY)) {
			break;
		}

		if (sq->sq_needexcl) {
			/*
			 * This is a request to unwind out of the perimiter
			 * because there is a thread waiting for exclusive
			 * activities (i.e. entersq(), qwriter_outer()).
			 */
			SQ_PUTCOUNT_CLRFAST(sq);
		}
		/*
		 * If there are events to run, do so.
		 * We have one claim to the syncq, so if there are
		 * more than one, other threads are running.
		 */
		if (sq->sq_evhead != NULL) {
			ASSERT(sq->sq_flags & SQ_EVENTS);

			SQ_PUTCOUNT_CLRFAST(sq);
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
			if (count > 1) {
				SQ_PUTLOCKS_EXIT(sq);
				/* Can't upgrade - other threads inside */
				break;
			}
			ASSERT((flags & SQ_EXCL) == 0);
			sq->sq_flags = flags | SQ_EXCL;
			SQ_PUTLOCKS_EXIT(sq);
			/*
			 * we have the only claim, run the events,
			 * sq_run_events will clear the SQ_EXCL flag.
			 */
			sq_run_events(sq);

			/*
			 * If this is a CIPUT perimiter, we need
			 * to drop the SQ_EXCL flag so we can properly
			 * continue draining the syncq.
			 */
			if (type & SQ_CIPUT) {
				ASSERT(sq->sq_flags & SQ_EXCL);
				sq->sq_flags &= ~SQ_EXCL;
			}

			/*
			 * And go back to the beginning just in case
			 * anything changed while we were away.
			 */
			ASSERT((sq->sq_flags & SQ_EXCL) || (type & SQ_CIPUT));
			continue;
		}

		ASSERT(sq->sq_evhead == NULL);
		ASSERT(!(sq->sq_flags & SQ_EVENTS));

		/*
		 * Find the queue that is not draining.
		 *
		 * q_draining is protected by QLOCK which we do not hold.
		 * But if it was set, then a thread was draining, and if it gets
		 * cleared, then it was because the thread has successfully
		 * drained the syncq, or a GOAWAY state occured. For the GOAWAY
		 * state to happen, a thread needs the SQLOCK which we hold, and
		 * if there was such a flag, we whould have already seen it.
		 */

		for (qp = sq->sq_head;
		    qp != NULL && (qp->q_draining ||
			(qp->q_sqflags & Q_SQDRAINING));
		    qp = qp->q_sqnext)
			;

		if (qp == NULL)
			break;

		/*
		 * We have a queue to work on, and we hold the
		 * SQLOCK and one claim, call qdrain_syncq.
		 * This means we need to release the SQLOCK and
		 * aquire the QLOCK (OK since we have a claim).
		 * Notice that for QNEXTLESS streams removeq doesn't
		 * care about the claim, but it checks for Q_SQDRAINING
		 * flag, so setting it should protect the queue from
		 * closing while we are draining it.
		 * Note that qdrain_syncq will actually dequeue
		 * this queue from the sq_head list when it is
		 * convinced all the work is done and release
		 * the QLOCK before returning.
		 */
		qp->q_sqflags |= Q_SQDRAINING;
		mutex_exit(SQLOCK(sq));
		mutex_enter(QLOCK(qp));
		qdrain_syncq(sq, qp);
		mutex_enter(SQLOCK(sq));

		/* The queue is drained */
		ASSERT(qp->q_sqflags & Q_SQDRAINING);
		qp->q_sqflags &= ~Q_SQDRAINING;

		/*
		 * NOTE: After this point qp should not be used since it may be
		 * closed. The QNEXTLESS driver path in removeq() assumes that
		 * when no one is draining the queue and there are no messages
		 * in it it is safe to free the queue.
		 */

		/*
		 * For now we just drain the whole syncq with whatever priority
		 * we are now.  The qdrain_syncq() may lower priority of sq and
		 * it may need to be rescheduled (with sqenable()), which means
		 * that we may exit from drain_syncq without fully draining
		 * it. For now it seems too much pain to do so.
		 */
	}

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	flags = sq->sq_flags;

	/*
	 * sq->sq_head cannot change because we hold the
	 * sqlock. However, a thread CAN decide that it is no longer
	 * going to drain that queue.  However, this should be due to
	 * a GOAWAY state, and we should see that here.
	 *
	 * This loop is not very efficient. One solution may be adding a second
	 * pointer to the "draining" queue, but it is difficult to do when
	 * queues are inserted in the middle due to priority ordering. Another
	 * possibility is to yank the queue out of the sq list and put it onto
	 * the "draining list" and then put it back if it can't be drained.
	 */

	ASSERT((sq->sq_head == NULL) || (flags & SQ_GOAWAY) ||
		(type & SQ_CI) || sq->sq_head->q_draining);

	/* Drop SQ_EXCL for non-CIPUT perimiters */
	if (!(type & SQ_CIPUT))
		flags &= ~SQ_EXCL;
	ASSERT((flags & SQ_EXCL) == 0);

	/* Set the FASTPUT bit if we can. */
	if (!(flags & SQ_GOAWAY) && (sq->sq_needexcl == 0)) {
		SQ_PUTCOUNT_SETFAST(sq);
	}

	/* Wake up any waiters. */
	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	if (flags & SQ_WANTEXWAKEUP) {
		flags &= ~SQ_WANTEXWAKEUP;
		cv_broadcast(&sq->sq_exitwait);
	}
	sq->sq_flags = flags;

	ASSERT(sq->sq_count != 0);
	/* Release our claim. */
	sq->sq_count--;

	TRACE_1(TR_FAC_STREAMS_FR, TR_DRAIN_SYNCQ_END,
		"drain_syncq end:%X", sq);
}


/*
 *
 * qdrain_syncq can be called (currently) from only one of two places:
 *	drain_syncq
 * 	putnext  (or some variation of it).
 * and eventually
 * 	qwait(_sig)
 *
 * If called from drain_syncq, we found it in the list
 * of queue's needing service, so there is work to be done (or it
 * wouldn't be on the list).
 *
 * If called from some putnext variation, it was because the
 * perimiter is open, but messages are blocking a putnext and
 * there is not a thread working on it.  Now a thread could start
 * working on it while we are getting ready to do so ourself, but
 * the thread would set the q_draining flag, and we can spin out.
 *
 * As for qwait(_sig), I think I shall let it continue to call
 * drain_syncq directly (after all, it will get here eventually).
 *
 * qdrain_syncq has to terminate when one of the SQ_STAYAWAY bits gets set
 * in order to preserve qwriter(OUTER) ordering constraints.
 *
 * ASSUMES:
 *	One claim
 * 	QLOCK held
 * 	SQLOCK not held
 *	Will release QLOCK before returning
 */
void
qdrain_syncq(syncq_t *sq, queue_t *q)
{
	mblk_t		*bp;
	boolean_t	do_clr;
#ifdef DEBUG
	uint16_t	count;
	queue_t		*qp;
#endif

	TRACE_1(TR_FAC_STREAMS_FR, TR_DRAIN_SYNCQ_START,
		"drain_syncq start:%X", sq);
	ASSERT(q->q_syncq == sq);
	ASSERT(MUTEX_HELD(QLOCK(q)));
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));
	/*
	 * For non-CIPUT perimiters, we should be called with the
	 * exclusive bit set already.  For non-CIPUT perimiters we
	 * will be doing a concurrent drain, so it better not be set.
	 */
	ASSERT((sq->sq_flags & (SQ_EXCL|SQ_CIPUT)));
	ASSERT(!((sq->sq_type & SQ_CIPUT) && (sq->sq_flags & SQ_EXCL)));
	ASSERT((sq->sq_type & SQ_CIPUT) || (sq->sq_flags & SQ_EXCL));
	/*
	 * All outer pointers are set, or none of them are
	 */
	ASSERT((sq->sq_outer == NULL && sq->sq_onext == NULL &&
		sq->sq_oprev == NULL) ||
		(sq->sq_outer != NULL && sq->sq_onext != NULL &&
		sq->sq_oprev != NULL));
#ifdef DEBUG
	count = sq->sq_count;
	/*
	 * This is OK without the putlocks, because we have one
	 * claim either from the sq_count, or a putcount.  We could
	 * get an erroneous value from other counts, but ours won't
	 * change, so one way or another, we will have at least a
	 * value of one.
	 */
	SUM_SQ_PUTCOUNTS(sq, count);
	ASSERT(count >= 1);
#endif DEBUG

	/*
	 * The first thing to do here, is find out if a thread
	 * is already draining this queue.  If so, we are done,
	 * just return.  Also, if there are no messages, we are
	 * done as well.  Note that we check the q_sqhead since
	 * there is s window of opportunity for us to enter here
	 * because Q_SQQUEUED was set, but is not anymore.
	 */
	if (q->q_draining || (q->q_sqhead == NULL)) {
		mutex_exit(QLOCK(q));
		return;
	}

	/*
	 * If the perimiter is exclusive, there is nothing we can
	 * do right now, go away.
	 * Note that there is nothing to prevent this case from changing
	 * right after this check, but the spin-out will catch it.
	 */

	/* Tell other threads that we are draining this queue */
	q->q_draining = 1;	/* Protected by QLOCK */

	for (bp = q->q_sqhead; bp != NULL; bp = q->q_sqhead) {
		ASSERT(bp->b_queue == q);
		ASSERT(bp->b_queue->q_syncq == sq);
		/*
		 * Because we can enter this routine just because
		 * a putnext is blocked, we need to spin out if
		 * the perimiter wants to go exclusive as well
		 * as just blocked.
		 * Don't check for SQ_EXCL, because non-CIPUT
		 * perimiters would set it, and it can't become
		 * exclusive while we hold a claim.
		 */
		if (sq->sq_flags & SQ_STAYAWAY) {
			break;
		}

#ifdef DEBUG
		/*
		 * qp is used for a sanitiy check.  Since we are in
		 * qdrain_syncq, we already know the queue, but for
		 * sanity, we want to check this against the qp that
		 * was passed in by  bp->b_queue.  qp is only used in
		 * debug statements, and we ALWAYS always assume that
		 * messages on the queue go to that queue.
		 */
		qp = bp->b_queue;
		ASSERT(qp == q);
		/*
		 * We would have the following check in the DEBUG code:
		 *
		 * if (bp->b_prev != NULL)  {
		 *	ASSERT(bp->b_prev == (void (*)())q->q_qinfo->qi_putp);
		 * }
		 *
		 * This can't be done, however, since IP modifies qinfo
		 * structure at run-time (switching between IPv4 qinfo and IPv6
		 * qinfo), invalidating the check.
		 * So the assignment to func is left here, but the ASSERT itself
		 * is removed until the whole issue is resolved.
		 */
#endif
		ASSERT(q->q_sqhead == bp);
		q->q_sqhead = bp->b_next;
		bp->b_prev = bp->b_next = NULL;
		ASSERT(q->q_syncqmsgs > 0);
		q->q_syncqmsgs--;

		/*
		 * Clear QFULL in the next service procedure queue if
		 * this is the last message destined to that queue.
		 *
		 * It would make better sense to have some sort of
		 * tunable for the low water mark, but these symantics
		 * are not yet defined.  So, alas, we use a constant.
		 */
		do_clr = (q->q_syncqmsgs == 0);
		mutex_exit(QLOCK(q));

		if (do_clr)
			clr_qfull(q);

		ASSERT(bp->b_datap->db_ref != 0);

		(void) (*q->q_qinfo->qi_putp)(q, bp);

		mutex_enter(QLOCK(q));
		/*
		 * Always clear SQ_EXCL when CIPUT in order to handle
		 * qwriter(INNER).
		 */
		/*
		 * The putp() can call qwriter and get exclusive access
		 * IFF this is the only claim.  So, we need to test for
		 * this possibility so we can aquire the mutex and clear
		 * the bit.
		 */
		if ((sq->sq_type & SQ_CIPUT) && (sq->sq_flags & SQ_EXCL)) {
			mutex_enter(SQLOCK(sq));
			sq->sq_flags &= ~SQ_EXCL;
			if (!sq->sq_needexcl)
				SQ_PUTCOUNT_SETFAST(sq);
			mutex_exit(SQLOCK(sq));
		}
	}

	/*
	 * We should either have no queues on the syncq, or we were
	 * told to goaway by a waiter (which we will wake up at the
	 * end of this function).
	 */
	ASSERT((q->q_sqhead == NULL) || (sq->sq_flags & SQ_GOAWAY));

	ASSERT(MUTEX_HELD(QLOCK(q)));
	ASSERT(MUTEX_NOT_HELD(SQLOCK(sq)));

	/*
	 * Remove the q from the syncq list if all the messages are
	 * drained.
	 */
	if (q->q_sqhead == NULL) {
		mutex_enter(SQLOCK(sq));
		if (q->q_sqflags & Q_SQQUEUED)
			SQRM_Q(sq, q);
		mutex_exit(SQLOCK(sq));
		/*
		 * Since the queue is removed from the list, reset its priority.
		 */
		q->q_spri = 0;
	}

	/*
	 * Remember, the q_draining flag is used to let another
	 * thread know that there is a thread currently draining
	 * the messages for a queue.  Since we are now done with
	 * this queue (even if there may be messages still there),
	 * we need to clear this flag so some thread will work
	 * on it if needed.
	 */
	ASSERT(q->q_draining);
	q->q_draining = 0;

	/* called with a claim, so OK to drop all locks. */
	mutex_exit(QLOCK(q));

	TRACE_1(TR_FAC_STREAMS_FR, TR_DRAIN_SYNCQ_END,
		"drain_syncq end:%X", sq);
}

/* END OF QDRAIN_SYNCQ  */


/*
 * Add a message to the syncq. If func is NULL it is assumed to be the put
 * procedure of the specified queue. When the message is drained from the syncq
 * the framework will call:
 *	(*func)(q, mp);
 *
 * We don't  grab sq_putlocks here since it is not important if the exit part
 * of putnext misses SQ_QUEUED and doesn't call drain_syncq(). We call
 * drain_syncq() here.
 *
 * If func is not NULL, it is a callback, so put it on the events list.
 * Otherwise call qfill_syncq with the appropriate queue.
 *
 * NOTE: Any function calling fill_syncq() might be called upon to
 *	 drain the entire syncq, and therefore could be hijacked.
 *
 * NOTE: This routine is called without assuption to claims on
 *	 a syncq, but one is made if qfill_syncq is called.
 *	 However, it might be easier just to assume a claim, since
 *	 ALL callers of this procedure drop the claim just before
 *	 entering (could save time).
 *
 * NOTE: We drop the SQLOCK in sqfill_events IFF we run the event list.
 *	 But this is done exclusively, so will not cause a problem.
 *	 We also drop it so we can aquire the QLOCK and preserve lock
 *	 ordering.  We do increment the sq_count so we won't close.
 */
void
fill_syncq(syncq_t *sq, queue_t *q, mblk_t *mp, void (*func)())
{
	uint16_t flags;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT(q->q_syncq == sq);
	ASSERT((sq->sq_outer == NULL && sq->sq_onext == NULL &&
		sq->sq_oprev == NULL) ||
		(sq->sq_outer != NULL && sq->sq_onext != NULL &&
		sq->sq_oprev != NULL));

	TRACE_4(TR_FAC_STREAMS_FR, TR_FILL_SYNCQ,
		"fill_syncq:%X %X %X %X", sq, q, mp, func);

	if (func != NULL) {
		/*
		 *This is a message destined for the events queue,
		 * let that routine handle it.
		 */
		sqfill_events(sq, q, mp, func);
	} else {
		/*
		 * This is a message for the destination queue,
		 * so put it there, and let that function deal with
		 * it.
		 *
		 * Because we entered here from the global fill,
		 * we are responsible for draining if the perimiter
		 * has opened.
		 * Note that qfill_syncq will release the QLOCK.
		 */
		if (mutex_tryenter(QLOCK(q)) == 0) {
			sq->sq_count++;
			mutex_exit(SQLOCK(sq));
			mutex_enter(QLOCK(q));
			mutex_enter(SQLOCK(sq));
			sq->sq_count--;
		}
		qfill_syncq(sq, q, mp);
	}
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	flags = sq->sq_flags;

	/*
	 * If the perimiter has opened up, try and drain the syncq.
	 * If there is any other reason not to drain, drain_syncq()
	 * will return.
	 */
	if (!(flags & (SQ_STAYAWAY|SQ_EXCL)) && (flags & SQ_QUEUED))
		drain_syncq(sq);
	/*
	 * Signal waiters if there are any.
	 */
	if (sq->sq_flags & SQ_WANTWAKEUP) {
		sq->sq_flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	TRACE_4(TR_FAC_STREAMS_FR, TR_FILL_SYNCQ,
		"fill_syncq:%X %X %X %X", sq, q, mp, func);
}
/* End fill_syncq */

/*
 * This is the mate to qdrain_syncq, except that it is putting the
 * message onto the the queue instead draining.  Since the
 * message is destined for the queue that is selected, there is
 * no need to identify the function because the message is
 * intended for the put routine for the queue.  But this
 * routine will do it anyway just in case (but only for debug kernels).
 * We assume the SQLOCK and QLOCK are held.  The QLOCK is released
 * before we return.  And since we never drop the SQLOCK, we assume
 * that the state that caused the fill still exists, and the caller
 * will take care of it if it changes.
 */
void
qfill_syncq(syncq_t *sq, queue_t *q, mblk_t *mp)
{
	queue_t		*fq = NULL;
	int		qnextless = (STREAM(q)->sd_flag & STRQNEXTLESS);

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT(MUTEX_HELD(QLOCK(q)));
	ASSERT(q->q_syncq == sq);
	ASSERT((sq->sq_outer == NULL && sq->sq_onext == NULL &&
		sq->sq_oprev == NULL) ||
		(sq->sq_outer != NULL && sq->sq_onext != NULL &&
		sq->sq_oprev != NULL));

	/*
	 * Set QFULL in next service procedure queue (that cares) if not
	 * already set and if there are already more messages on the syncq
	 * than sq_max_size.
	 *
	 * NOTE: If sq_max_size is 0, no flow control will be asserted on
	 * any syncq. Accessing q_nfsrv without doing a claimstr is not
	 * right for a QNEXTLESS stream as q_nfsrv could change while we
	 * are here. But it is okay as q->q_nfsrv of all the queues in the
	 * QNEXTLESS stream points to itself. When that assumption changes
	 * we will revisit this. Currently, set_qfull and clr_qfull know
	 * about this and does thing right.
	 * Note that fq here is the next queue with a service procedure.
	 * This is where we would fail canputnext, so this is where we
	 * need to set QFULL.
	 */
	if ((sq_max_size != 0) && (!(q->q_nfsrv->q_flag & QFULL))) {
		if (q->q_syncqmsgs > sq_max_size) {
			if ((fq = q->q_nfsrv) == q) {
				fq->q_flag |= QFULL;
			} else {
				ASSERT(fq->q_syncq != sq);
				if (qnextless)
					claimstr(fq);
				mutex_enter(QLOCK(fq));
				fq->q_flag |= QFULL;
				mutex_exit(QLOCK(fq));
				if (qnextless)
					releasestr(fq);
			}
		}
	}

#ifdef DEBUG
	/*
	 * This is used for debug in the qfill_syncq/qdrain_syncq case
	 * to trace the queue that the message is intended for.  Note
	 * that the original use was to identify the queue and function
	 * to call on the drain.  In the new syncq, we have the context
	 * of the queue that we are draining, so call it's putproc and
	 * don't rely on the saved values.  But for debug this is still
	 * usefull information.
	 */
	mp->b_prev = (mblk_t *)q->q_qinfo->qi_putp;
	mp->b_queue = q;
	mp->b_next = NULL;
#endif
	ASSERT(q->q_syncq == sq);
	/*
	 * Enqueue the message on the list.
	 */
	SQPUT_MP(q, mp);

	/*
	 * And queue on syncq for scheduling, if not already queued.
	 * Note that we need the SQLOCK for this, and for testing flags
	 * at the end to see if we will drain.  So grab it now, and
	 * release it before we call qdrain_syncq or return.
	 */
	if (!(q->q_sqflags & Q_SQQUEUED)) {
		q->q_spri = curthread->t_pri;
		SQPUT_Q(sq, q);
	}
#ifdef DEBUG
	else {
		/*
		 * All of these conditions MUST be true!
		 */
		ASSERT(sq->sq_tail != NULL);
		if (sq->sq_tail == sq->sq_head) {
			ASSERT((q->q_sqprev == NULL) &&
			    (q->q_sqnext == NULL));
		} else {
			ASSERT((q->q_sqprev != NULL) ||
			    (q->q_sqnext != NULL));
		}
		ASSERT(sq->sq_flags & SQ_QUEUED);
		ASSERT(q->q_syncqmsgs != 0);
		ASSERT(q->q_sqflags & Q_SQQUEUED);
	}
#endif
	/*
	 * We release the QLOCK here since the caller is no longer
	 * interested in it.  However, we continue to hold the SQLOCK.
	 */
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	mutex_exit(QLOCK(q));
}

/*  End of qfill_syncq  */

/*
 * Remove all messages from a syncq (if qp is NULL) or remove all messages
 * that would be put into qp by drain_syncq.
 * Used when deleting the syncq (qp == NULL) or when detaching
 * a queue (qp != NULL).
 * Return non-zero if one or more messages were freed.
 *
 * no need to grab sq_putlocks here. See comment in strsubr.h that explains when
 * sq_putlocks are used.
 *
 * NOTE: This function assumes that it is called from the close() context and
 * that all the queues in the syncq are going aay. For this reason it doesn't
 * acquire QLOCK for modifying q_sqhead/q_sqtail fields. This assumption is
 * currently valid, but it is useful to rethink this function to behave properly
 * in other cases.
 */
int
flush_syncq(syncq_t *sq, queue_t *qp)
{
	mblk_t		*bp, *mp_head, *mp_next, *mp_prev;
	queue_t		*q;
	int		ret = 0;

	mutex_enter(SQLOCK(sq));

	/*
	 * Before we leave, we need to make sure there are no
	 * events listed for this queue.  All events for this queue
	 * will just be freed.
	 */
	if (qp != NULL && sq->sq_evhead != NULL) {
		ASSERT(sq->sq_flags & SQ_EVENTS);

		mp_prev = NULL;
		for (bp = sq->sq_evhead; bp != NULL; bp = mp_next) {
			mp_next = bp->b_next;
			if (bp->b_queue == qp) {
				/* Delete this message */
				if (mp_prev != NULL) {
					mp_prev->b_next = mp_next;
					/*
					 * Update sq_evtail if the last element
					 * is removed.
					 */
					if (bp == sq->sq_evtail) {
						ASSERT(mp_next == NULL);
						sq->sq_evtail = mp_prev;
					}
				} else
					sq->sq_evhead = mp_next;
				if (sq->sq_evhead == NULL)
					sq->sq_flags &= ~SQ_EVENTS;
				bp->b_prev = bp->b_next = NULL;
				freemsg_flush(bp);
				ret++;
			} else {
				mp_prev = bp;
			}
		}
	}

	/*
	 * Walk sq_head and:
	 *	- match qp if qp is set, remove it's messages
	 *	- all if qp is not set
	 */
	q = sq->sq_head;
	while (q != NULL) {
		ASSERT(q->q_syncq == sq);
		if ((qp == NULL) || (qp == q)) {
			/*
			 * Yank the messages as a list off the queue
			 */
			mp_head = q->q_sqhead;
			/*
			 * We do not have QLOCK(q) here (which is safe due to
			 * assumptions mentioned above). To obtain the lock we
			 * need to release SQLOCK which may allow lots of things
			 * to change upon us. This place requires more analysis.
			 */
			q->q_sqhead = q->q_sqtail = NULL;
			ASSERT(mp_head->b_queue &&
			    mp_head->b_queue->q_syncq == sq);

			/*
			 * Free each of the messages.
			 */
			for (bp = mp_head; bp != NULL; bp = mp_next) {
				mp_next = bp->b_next;
				bp->b_prev = bp->b_next = NULL;
				freemsg_flush(bp);
				ret++;
			}
			/*
			 * Now remove the queue from the syncq.
			 */
			ASSERT(q->q_sqflags & Q_SQQUEUED);
			SQRM_Q(sq, q);
			q->q_spri = 0;
			q->q_syncqmsgs = 0;

			/*
			 * If qp was specified, we are done with it and are
			 * going to drop SQLOCK(sq) and return. We wakeup syncq
			 * waiters while we still have the SQLOCK.
			 */
			if ((qp != NULL) && (sq->sq_flags & SQ_WANTWAKEUP)) {
				sq->sq_flags &= ~SQ_WANTWAKEUP;
				cv_broadcast(&sq->sq_wait);
			}
			/* Drop SQLOCK across clr_qfull */
			mutex_exit(SQLOCK(sq));

			/*
			 * We avoid doing the test that drain_syncq does and
			 * unconditionally clear qfull for every flushed
			 * message. Since flush_syncq is only called during
			 * close this should not be a problem.
			 */
			clr_qfull(q);
			if (qp != NULL) {
				return (ret);
			} else {
				mutex_enter(SQLOCK(sq));
				/*
				 * The head was removed by SQRM_Q above.
				 * reread the new head and flush it.
				 */
				q = sq->sq_head;
			}
		} else {
			q = q->q_sqnext;
		}
		ASSERT(MUTEX_HELD(SQLOCK(sq)));
	}

	if (sq->sq_flags & SQ_WANTWAKEUP) {
		sq->sq_flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}

	mutex_exit(SQLOCK(sq));
	return (ret);
}

/*
 * Propagate all messages from a syncq to the next syncq that are associated
 * with the specified queue. If the queue is attached to a driver or if the
 * messages have been added due to a qwriter(PERIM_INNER), free the messages.
 *
 * Assumes that the stream is strlock()'ed. We don't come here if there
 * are no messages to propagate.
 *
 * NOTE : If the queue is attached to a driver, all the messages are freed
 * as there is no point in propagating the messages from the driver syncq
 * to the closing stream head which will in turn get freed later.
 */
static int
propagate_syncq(queue_t *qp)
{
	mblk_t		*bp, *head, *tail, *prev, *next;
	queue_t		*q, *qnext;
	syncq_t 	*sq;
	queue_t		*nqp;
	syncq_t		*nsq;
	boolean_t	isdriver;
	int 		moved = 0;
	uint16_t	flags;
	pri_t		priority = curthread->t_pri;
#ifdef DEBUG
	void		(*func)();
#endif

	sq = qp->q_syncq;
	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	/* debug macro */
	SQ_PUTLOCKS_HELD(sq);
	/*
	 * As entersq() does not increment the sq_count for
	 * the write side, check sq_count for non-QPERQ
	 * perimeters alone.
	 */
	ASSERT((qp->q_flag & QPERQ) || (sq->sq_count >= 1));

	isdriver = (qp->q_flag & QISDRV);

	if (!isdriver) {
		nqp = qp->q_next;
		nsq = nqp->q_syncq;
		ASSERT(MUTEX_HELD(SQLOCK(nsq)));
		/* debug macro */
		SQ_PUTLOCKS_HELD(nsq);
#ifdef DEBUG
		func = (void (*)())nqp->q_qinfo->qi_putp;
#endif
	}

	for (q = sq->sq_head; q != NULL; q = qnext) {
		qnext = q->q_sqnext;
		ASSERT(q && q->q_syncq == sq);
		if (qp == q) {
			SQRM_Q(sq, q);
			priority = MAX(q->q_spri, priority);
			q->q_spri = 0;
			head = q->q_sqhead;
			tail = q->q_sqtail;
			q->q_sqhead = q->q_sqtail = NULL;
			q->q_syncqmsgs = 0;

			/*
			 * Walk the list of messages, and free them if
			 * this is a driver, otherwise reset the b_prev
			 * and b_queue value to the new putp.
			 * Afterward, we will just add the head to
			 * the end of the next syncq, and point the
			 * tail to the end of this one.
			 */
			for (bp = head; bp != NULL; bp = next) {
				next = bp->b_next;
				if (isdriver) {
					bp->b_prev = bp->b_next = NULL;
					freemsg_flush(bp);
					continue;
				}
				/* Change the q values for this message */
				bp->b_queue = nqp;
#ifdef DEBUG
				bp->b_prev = (mblk_t *)func;
#endif
				moved++;
			}
			/*
			 * Attach list of messages to the end of
			 * the new queue (if there is a list of messages).
			 */
			if (!isdriver && head != NULL) {
				ASSERT(tail != NULL);
				if (nqp->q_sqhead == NULL) {
					nqp->q_sqhead = head;
				} else {
					ASSERT(nqp->q_sqtail != NULL);
					nqp->q_sqtail->b_next = head;
				}
				nqp->q_sqtail = tail;
				/*
				 * When messages are moved from high priority
				 * queue to another queue, the destination queue
				 * priority is upgraded.
				 */
				if (priority > nqp->q_spri)
					nqp->q_spri = priority;

				SQPUT_Q(nsq, nqp);

				nqp->q_syncqmsgs += moved;
				ASSERT(nqp->q_syncqmsgs != 0);
			}
		}
	}
	/*
	 * Before we leave, we need to make sure there are no
	 * events listed for this queue.  All events for this queue
	 * will just be freed.
	 */
	if (sq->sq_evhead != NULL) {
		ASSERT(sq->sq_flags & SQ_EVENTS);
		prev = NULL;
		for (bp = sq->sq_evhead; bp != NULL; bp = next) {
			next = bp->b_next;
			if (bp->b_queue == qp) {
				/* Delete this message */
				if (prev != NULL) {
					prev->b_next = next;
					/*
					 * Update sq_evtail if the last element
					 * is removed.
					 */
					if (bp == sq->sq_evtail) {
						ASSERT(next == NULL);
						sq->sq_evtail = prev;
					}
				} else
					sq->sq_evhead = next;
				if (sq->sq_evhead == NULL)
					sq->sq_flags &= ~SQ_EVENTS;
				bp->b_prev = bp->b_next = NULL;
				freemsg_flush(bp);
			} else {
				prev = bp;
			}
		}
	}

	flags = sq->sq_flags;

	/* Wake up any waiter before leaving. */
	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	sq->sq_flags = flags;

	return (moved);
}

/*
 * Try and upgrade to exclusive access at the inner perimeter. If this can
 * not be done without blocking then request will be queued on the syncq
 * and drain_syncq will run it later.
 *
 * This routine can only be called from put or service procedures plus
 * asynchronous callback routines that have properly entered to
 * queue (with entersq.) Thus qwriter_inner assumes the caller has one claim
 * on the syncq associated with q.
 */
void
qwriter_inner(queue_t *q, mblk_t *mp, void (*func)())
{
	syncq_t	*sq = q->q_syncq;
	uint16_t count;

	mutex_enter(SQLOCK(sq));
	SQ_PUTCOUNT_CLRFAST(sq);
	count = sq->sq_count;
	SQ_PUTLOCKS_ENTER(sq);
	SUM_SQ_PUTCOUNTS(sq, count);
	ASSERT(count >= 1);
	ASSERT(sq->sq_type & (SQ_CIPUT|SQ_CISVC));

	if (count == 1) {
		/*
		 * Can upgrade. This case also handles nested qwriter calls
		 * (when the qwriter callback function calls qwriter). In that
		 * case SQ_EXCL is already set.
		 */
		sq->sq_flags |= SQ_EXCL;
		SQ_PUTLOCKS_EXIT(sq);
		mutex_exit(SQLOCK(sq));
		(*func)(q, mp);
		/*
		 * Assumes that leavesq, putnext, and drain_syncq will reset
		 * SQ_EXCL for SQ_CIPUT/SQ_CISVC queues. We leave SQ_EXCL on
		 * until putnext, leavesq, or drain_syncq drops it.
		 * That way we handle nested qwriter(INNER) without dropping
		 * SQ_EXCL until the outermost qwriter callback routine is
		 * done.
		 */
		return;
	}
	SQ_PUTLOCKS_EXIT(sq);
	sqfill_events(sq, q, mp, func);
	mutex_exit(SQLOCK(sq));
}

/*
 * Synchronous callback support functions
 */

/*
 * Allocate a callback parameter structure.
 * Assumes that caller initializes the flags and the id.
 * Acquires SQLOCK(sq).
 */
callbparams_t *
callbparams_alloc(syncq_t *sq, void (*func)(void *), void *arg)
{
	callbparams_t *cbp;

	cbp = kmem_cache_alloc(callbparams_cache, KM_SLEEP);
	cbp->cbp_sq = sq;
	cbp->cbp_func = func;
	cbp->cbp_arg = arg;
	mutex_enter(SQLOCK(sq));
	cbp->cbp_next = sq->sq_callbpend;
	sq->sq_callbpend = cbp;
	return (cbp);
}

void
callbparams_free(syncq_t *sq, callbparams_t *cbp)
{
	callbparams_t **pp, *p;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));

	for (pp = &sq->sq_callbpend; (p = *pp) != NULL; pp = &p->cbp_next) {
		if (p == cbp) {
			*pp = p->cbp_next;
			kmem_cache_free(callbparams_cache, p);
			return;
		}
	}
	(void) (STRLOG(0, 0, 0, SL_CONSOLE,
	    "callbparams_free: not found\n"));
}

void
callbparams_free_id(syncq_t *sq, callbparams_id_t id, int32_t flag)
{
	callbparams_t **pp, *p;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));

	for (pp = &sq->sq_callbpend; (p = *pp) != NULL; pp = &p->cbp_next) {
		if (p->cbp_id == id && p->cbp_flags == flag) {
			*pp = p->cbp_next;
			kmem_cache_free(callbparams_cache, p);
			return;
		}
	}
	(void) (STRLOG(0, 0, 0, SL_CONSOLE,
	    "callbparams_free_id: not found\n"));
}

/*
 * Callback wrapper function used by once-only callbacks that can be
 * cancelled (qtimeout and qbufcall)
 * Contains inline version of entersq(sq, SQ_CALLBACK) that can be
 * cancelled by the qun* functions.
 */
void
qcallbwrapper(void *arg)
{
	callbparams_t *cbp = arg;
	syncq_t	*sq;
	uint16_t count = 0;
	uint16_t waitflags, type;

	sq = cbp->cbp_sq;
	mutex_enter(SQLOCK(sq));
	type = sq->sq_type;
	if (!(type & SQ_CICB)) {
		count = sq->sq_count;
		SQ_PUTLOCKS_ENTER(sq);
		SUM_SQ_PUTCOUNTS(sq, count);
	}
	/* Can not handle exlusive entry at outer perimeter */
	ASSERT(type & SQ_COCB);
	waitflags = SQ_STAYAWAY | SQ_QUEUED | SQ_EXCL;

	sq->sq_needexcl++;
	ASSERT(sq->sq_needexcl != 0);	/* wraparound */

	while ((sq->sq_flags & waitflags) || (!(type & SQ_CICB) &&count != 0)) {
		if ((sq->sq_callbflags & cbp->cbp_flags) &&
		    (sq->sq_cancelid == cbp->cbp_id)) {
			/* timeout has been cancelled */
			sq->sq_callbflags |= SQ_CALLB_BYPASSED;
			callbparams_free(sq, cbp);
			if (!(type & SQ_CICB)) {
				SQ_PUTLOCKS_EXIT(sq);
			}
			ASSERT(sq->sq_needexcl > 0);
			sq->sq_needexcl--;
			mutex_exit(SQLOCK(sq));
			return;
		}
		sq->sq_flags |= SQ_WANTWAKEUP;
		if (!(type & SQ_CICB)) {
			SQ_PUTLOCKS_EXIT(sq);
		}
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		if (!(type & SQ_CICB)) {
			count = sq->sq_count;
			SQ_PUTLOCKS_ENTER(sq);
			SUM_SQ_PUTCOUNTS(sq, count);
		}
	}

	sq->sq_needexcl--;

	sq->sq_count++;
	ASSERT(sq->sq_count != 0);	/* Wraparound */
	if (!(type & SQ_CICB)) {
		ASSERT(count == 0);
		sq->sq_flags |= SQ_EXCL;
		SQ_PUTLOCKS_EXIT(sq);
	}

	mutex_exit(SQLOCK(sq));

	cbp->cbp_func(cbp->cbp_arg);

	/*
	 * We drop the lock only for leavesq to re-acquire it.
	 * Possible optimization is inline of leavesq.
	 */
	mutex_enter(SQLOCK(sq));
	callbparams_free(sq, cbp);
	mutex_exit(SQLOCK(sq));
	leavesq(sq, SQ_CALLBACK);
}

/*
 * no need to grab sq_putlocks here. See comment in strsubr.h that
 * explains when sq_putlocks are used.
 *
 * sq_count (or one of the sq_putcounts) has already been
 * decremented by the caller, and if SQ_QUEUED, we need to call
 * drain_syncq (the global syncq drain).
 * If putnext_tail is called with the SQ_EXCL bit set, we are in
 * one of two states, non-CIPUT perimiter, and we need to clear
 * it, or we went exclusive in the put procedure.  In any case,
 * we want to clear the bit now, and it is probably easier to do
 * this at the beginning of this function (remember, we hold
 * the SQLOCK).  Lastly, if there are other messages queued
 * on the syncq (and not for our destination), enable the syncq
 * for background work.
 */

/* ARGSUSED */
void
putnext_tail(syncq_t *sq, queue_t *qp, uint32_t passflags)
{
	uint16_t	flags = sq->sq_flags;

	ASSERT(MUTEX_HELD(SQLOCK(sq)));
	ASSERT(MUTEX_NOT_HELD(QLOCK(qp)));

	/* Clear SQ_EXCL if set in passflags */
	if (passflags & SQ_EXCL) {
		flags &= ~SQ_EXCL;
	}
	if (flags & SQ_WANTWAKEUP) {
		flags &= ~SQ_WANTWAKEUP;
		cv_broadcast(&sq->sq_wait);
	}
	if (flags & SQ_WANTEXWAKEUP) {
		flags &= ~SQ_WANTEXWAKEUP;
		cv_broadcast(&sq->sq_exitwait);
	}
	sq->sq_flags = flags;

	/*
	 * We have cleared SQ_EXCL if we were asked to, and started
	 * the wakeup process for waiters.  If there are no writers
	 * then we need to drain the syncq if we were told to, or
	 * enable the background thread to do it.
	 */
	if (!(flags & (SQ_STAYAWAY|SQ_EXCL))) {
		if (!sq->sq_needexcl)
			SQ_PUTCOUNT_SETFAST(sq);
		if ((passflags & SQ_QUEUED) ||
		    (sq->sq_svcflags & SQ_DISABLED)) {
			/* drain_syncq will take care of events in the list */
			drain_syncq(sq);
		} else if (flags & SQ_QUEUED) {
			sqenable(sq);
		}
	}
	/* Drop the SQLOCK on exit */
	mutex_exit(SQLOCK(sq));
	TRACE_3(TR_FAC_STREAMS_FR, TR_PUTNEXT_END,
		"putnext_end:(%X, %X, %X) done", NULL, qp, sq);
}

void
set_qend(queue_t *q)
{
	mutex_enter(QLOCK(q));
	if (!O_SAMESTR(q))
		q->q_flag |= QEND;
	else
		q->q_flag &= ~QEND;
	mutex_exit(QLOCK(q));
	q = _OTHERQ(q);
	mutex_enter(QLOCK(q));
	if (!O_SAMESTR(q))
		q->q_flag |= QEND;
	else
		q->q_flag &= ~QEND;
	mutex_exit(QLOCK(q));
}

void
set_qnexthot(queue_t *q)
{
	queue_t	*bq;

	if (q == NULL)
		return;
	if (!(q->q_flag & QHOT)) {
		if (((bq = backq(q)) != NULL) && (bq->q_flag & QNEXTHOT)) {
			mutex_enter(QLOCK(bq));
			bq->q_flag &= ~QNEXTHOT;
			mutex_exit(QLOCK(bq));
		}
	} else if (((bq = backq(q)) != NULL) && !(bq->q_flag & QNEXTHOT)) {
		mutex_enter(QLOCK(bq));
		bq->q_flag |= QNEXTHOT;
		mutex_exit(QLOCK(bq));
	}
	q = _OTHERQ(q);
	if (!(q->q_flag & QHOT)) {
		if (((bq = backq(q)) != NULL) && (bq->q_flag & QNEXTHOT)) {
			mutex_enter(QLOCK(bq));
			bq->q_flag &= ~QNEXTHOT;
			mutex_exit(QLOCK(bq));
		}
	} else if (((bq = backq(q)) != NULL) && !(bq->q_flag & QNEXTHOT)) {
		mutex_enter(QLOCK(bq));
		bq->q_flag |= QNEXTHOT;
		mutex_exit(QLOCK(bq));
	}
}

#ifdef UNUSED
static void
set_qfull(queue_t *q)
{
	stdata_t *stp = STREAM(q);
	kthread_id_t freezer;
	queue_t *oq = q;
	queue_t *cq;

	q = q->q_nfsrv;
	/*
	 * The caller has already incremented the sq_count
	 * which prevents close from happening for non-QNEXTLESS
	 * streams as strlock would wait for sq_count to drop
	 * to zero. For QNEXTLESS streams, we can avoid doing a
	 * claimstr if q_nfsrv points to itself. See comments
	 * above strlock().
	 */
	if ((stp->sd_flag & STRQNEXTLESS) && oq != q) {
		cq = oq;
		claimstr(cq);
		q = cq->q_nfsrv;
	} else
		cq = NULL;
	freezer = STREAM(q)->sd_freezer;
	if (freezer != curthread) {
		mutex_enter(QLOCK(q));
	}
#ifdef DEBUG
	else {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	}
#endif

	q->q_flag |= QFULL;

	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	if (cq)
		releasestr(cq);
}
#endif

void
clr_qfull(queue_t *q)
{
	stdata_t *stp = STREAM(q);
	queue_t	*oq = q;
	queue_t *cq;

	q = q->q_nfsrv;
	if ((stp->sd_flag & STRQNEXTLESS) && oq != q) {
		cq = oq;
		claimstr(cq);
		q = cq->q_nfsrv;
	} else
		cq = NULL;
	/* Fast check if there is any work to do before getting the lock. */
	if ((q->q_flag & (QFULL|QWANTW)) == 0) {
		if (cq)
			releasestr(cq);
		return;
	}

	/*
	 * Do not reset QFULL (and backenable) if the q_count is the reason
	 * for QFULL being set.
	 */
	mutex_enter(QLOCK(q));
	/*
	 * If both q_count and q_mblkcnt are less than the hiwat mark
	 */
	if ((q->q_count < q->q_hiwat) && (q->q_mblkcnt < q->q_hiwat)) {
		q->q_flag &= ~QFULL;
		/*
		 * A little more confusing, how about this way:
		 * if someone wants to write,
		 * AND
		 *    both counts are less than the lowat mark
		 *    OR
		 *    the lowat mark is zero
		 * THEN
		 * backenable
		 */
		if ((q->q_flag & QWANTW) &&
		    (((q->q_count < q->q_lowat) &&
		    (q->q_mblkcnt < q->q_lowat)) || q->q_lowat == 0)) {
			q->q_flag &= ~QWANTW;
			mutex_exit(QLOCK(q));
			backenable(oq, 0);
		} else
			mutex_exit(QLOCK(q));
	} else
		mutex_exit(QLOCK(q));
	if (cq)
		releasestr(cq);
}

/*
 * This is a general comment describing how the performance
 * enhancement to STREAMS by caching pointers at the queue
 * level for a queue's next service procedure forward, "q_nfsrv",
 * (canput, canputnext, etc.) and next service procedure
 * backward, "q_nbsrv", (backenable) works.
 *
 * Basically, we determine at the time a queue is inserted
 * where its next forward service procedure and backward
 * service procedures are and then update them.
 *
 * The q_nfsrv pointer is set as per the way canput() and canputnext() were
 * originally coded. If the queue to be inserted has a service procedure
 * then the q_nfsrv pointer points to itself. If queue to be inserted
 * does not have a service procedure, then the q_nfsrv pointer points
 * to the next queue forward that has a service procedure. If the queue
 * at the logical end of the stream(driver for write side, stream head for
 * the read side) does not have a service procedure then the q_nfsrv
 * pointer for the end points to itself.
 *
 * Regarding the q_nbsrv pointer, it is set as per the way backenable()
 * was originally coded. Unlike canput and canputnext, backenabling does
 * not concern itself with the fact that the queue in question might have
 * a service procedure. It begins its search at the queue immediately
 * behind it. Since backenable may accept a NULL pointer (i.e. there is
 * no queue to backenable), the queues at the beginning of a stream
 * (the stream head for the write side, the driver for the read side)
 * are set to NULL.
 *
 * Pipes, fifos and connld have their q_nbsrv and q_nfsrv
 * pointers initialized when they are created.
 */

/*
 * Insert a module/driver. Set the forward service procedure pointer.
 */
void
set_nfsrv_ptr(
	queue_t  *rnew,		/* read queue pointer to new module */
	queue_t  *wnew,		/* write queue pointer to new module */
	queue_t  *prev_rq,	/* read queue pointer to the module above */
	queue_t  *prev_wq)	/* write queue pointer to the module above */
{
	queue_t *qp;

	if (prev_wq->q_next == NULL) {
		/*
		 * Insert the driver, initialize the driver and stream head.
		 * In thise case, prev_rq/prev_wq should be the stream head.
		 * _I_INSERT does not allow inserting a driver.  Make sure
		 * that it is not an insertion.
		 */
		ASSERT(!(rnew->q_flag & _QINSERTING));
		wnew->q_nfsrv = wnew;
		if (rnew->q_qinfo->qi_srvp)
			rnew->q_nfsrv = rnew;
		else
			rnew->q_nfsrv = prev_rq;
		prev_rq->q_nfsrv = prev_rq;
		prev_wq->q_nfsrv = prev_wq;
	} else {
		/* set up write side q_nfsrv pointer */
		if (wnew->q_qinfo->qi_srvp) {
			wnew->q_nfsrv = wnew;

			/*
			 * For insertion, need to update nfsrv of the modules
			 * above which do not have a service routine.
			 */
			if (rnew->q_flag & _QINSERTING) {
				for (qp = prev_wq;
				    qp != NULL && qp->q_nfsrv != qp;
				    qp = backq(qp)) {
					qp->q_nfsrv = wnew->q_nfsrv;
				}
			}
		} else {
			wnew->q_nfsrv = prev_wq->q_next->q_nfsrv;
		}

		/* set up read side q_nfsrv pointer */
		if (rnew->q_qinfo->qi_srvp) {
			qp = _RD(prev_wq->q_next);
			while (qp && qp->q_nfsrv != qp) {
				qp->q_nfsrv = rnew;
				qp = backq(qp);
			}
			rnew->q_nfsrv = rnew;
		} else
			rnew->q_nfsrv = prev_rq;
	}
}

/*
 * Insert a module/driver.  Set the backward service procedure pointer.
 */
void
set_nbsrv_ptr(
	queue_t  *rnew,		/* read queue pointer to new module */
	queue_t  *wnew,		/* write queue pointer to new module */
	queue_t  *prev_rq,	/* read queue pointer to the module above */
	queue_t  *prev_wq)	/* write queue pointer to the module above */
{
	queue_t *qp;

	if (prev_wq->q_next == NULL) {
		/*
		 * Insert the driver, initialize the driver and stream head
		 * In thise case, prev_rq/prev_wq should be the stream head.
		 * _I_INSERT does not allow inserting a driver.  Make sure
		 * that it is not an insertion.
		 */
		ASSERT(!(rnew->q_flag & _QINSERTING));
		wnew->q_nbsrv = prev_wq;
		if (rnew->q_qinfo->qi_srvp) {
			prev_rq->q_nbsrv = rnew;
		} else {
			prev_rq->q_nbsrv = NULL;
		}
		prev_wq->q_nbsrv = NULL;
		rnew->q_nbsrv = NULL;
	} else {
		/* set up write side q_nbsrv pointer */
		wnew->q_nbsrv = prev_wq->q_next->q_nbsrv;
		if (wnew->q_qinfo->qi_srvp) {
			qp = prev_wq->q_next;
			while (qp && qp->q_nbsrv == prev_wq) {
				qp->q_nbsrv = wnew;
				qp = qp->q_next;
			}
		}

		/* set up read side q_nbsrv pointer */
		rnew->q_nbsrv = prev_rq->q_nbsrv;
		if (rnew->q_qinfo->qi_srvp) {
			prev_rq->q_nbsrv = rnew;

			/*
			 * For insertion, need to update nbsrv of the module
			 * above.
			 */
			if (rnew->q_flag & _QINSERTING) {
				for (qp = prev_rq;
				    qp->q_next != NULL &&
				    qp->q_qinfo->qi_srvp == NULL;
				    qp = qp->q_next) {
					qp->q_next->q_nbsrv = rnew;
				}
			}
		}
	}
}

/*
 * Remove a module/driver. Reset the forward service procedure pointer.
 */
/* ARGSUSED */
void
reset_nfsrv_ptr(queue_t *rqp, queue_t *wqp, stdata_t *stream)
{
	queue_t *tmp_qp;

	/* Reset the write side q_nfsrv pointer for _I_REMOVE */
	if ((rqp->q_flag & _QREMOVING) && (wqp->q_qinfo->qi_srvp != NULL)) {
		for (tmp_qp = backq(wqp);
		    tmp_qp != NULL && tmp_qp->q_nfsrv == wqp;
		    tmp_qp = backq(tmp_qp)) {
			tmp_qp->q_nfsrv = wqp->q_nfsrv;
		}
	}

	/* reset the read side q_nfsrv pointer */
	if (rqp->q_qinfo->qi_srvp) {
		if (wqp->q_next) {	/* non-driver case */
			tmp_qp = _RD(wqp->q_next);
			while (tmp_qp && tmp_qp->q_nfsrv == rqp) {
				/* Note that rqp->q_next cannot be NULL */
				ASSERT(rqp->q_next != NULL);
				tmp_qp->q_nfsrv = rqp->q_next->q_nfsrv;
				tmp_qp = backq(tmp_qp);
			}
		}
	}
}

/*
 * Remove a module/driver. Reset the backward service procedure pointer.
 */
/* ARGSUSED */
void
reset_nbsrv_ptr(queue_t *rqp, queue_t *wqp, stdata_t *stream)
{
	queue_t *tmp_qp;

	/* reset the write side q_nbsrv pointer */
	if (wqp->q_qinfo->qi_srvp) {
		tmp_qp = wqp->q_next;
		while (tmp_qp && tmp_qp->q_nbsrv == wqp) {
			tmp_qp->q_nbsrv = rqp->q_nbsrv;
			tmp_qp = tmp_qp->q_next;
		}
	}
	/* reset the read side q_nbsrv pointer */
	if (rqp->q_qinfo->qi_srvp != NULL) {
		for (tmp_qp = rqp->q_next;
		    tmp_qp != NULL && tmp_qp->q_nbsrv == rqp;
		    tmp_qp = tmp_qp->q_next) {
			tmp_qp->q_nbsrv = rqp->q_nbsrv;
		}
	}
}

static int
mp_strinit_mkthreads(int cpu)
{
	uint_t i;

	bkgrnd_thread = thread_create(NULL, _defaultstksz, background,
		0, 0, &p0, TS_RUN, 60);
	if (bkgrnd_thread == (kthread_id_t)NULL)
		return (ENOMEM);
	background_count++;

	/* Initialize syncq hash threads */
	for (i = 0; i < nsqservers; i++) {
		sqserv_t *servicelist = &(sqservers[i]);
		pri_t pri = servicelist->sqserv_pri < kpreemptpri ?
		    60 :
		    kpreemptpri;

		mutex_enter(&servicelist->sqserv_lock);
		if (cpu <= servicelist->sqserv_nthreads) {
			servicelist->sqserv_threads[cpu] =
			    thread_create(NULL, _defaultstksz, sqthread,
				(caddr_t)servicelist, 0, &p0, TS_RUN, pri);
			if (servicelist->sqserv_threads[cpu] ==
			    (kthread_id_t)NULL) {
				cmn_err(CE_NOTE,
				    "mp_strinit_mkthreads:"
				    "no memory for thread creation\n");
				mutex_exit(&servicelist->sqserv_lock);
				return (ENOMEM);
			}
			servicelist->sqserv_threadcount++;
		}
		mutex_exit(&servicelist->sqserv_lock);
	}

	liberator = thread_create(NULL, _defaultstksz, freebs, 0, 0, &p0,
		TS_RUN, 60);
	if (liberator == (kthread_id_t)NULL)
		return (ENOMEM);

	writer_thread = thread_create(NULL, _defaultstksz,
		qwriter_outer_thread, 0, 0, &p0, TS_RUN, 60);
	if (writer_thread == (kthread_id_t)NULL)
		return (ENOMEM);

	return (0);
}

static uint_t last_ncpu = 0;

/*
 * This creates "cpus" worth of stream threads
 * called from main after all cpus are detected
 * and from main for initial stream init.
 * mp_strinit() is called single threaded.
 */
void
mp_strinit(void)
{
	while (last_ncpu < ncpus) {
		if (mp_strinit_mkthreads(last_ncpu) != 0)
			cmn_err(CE_PANIC, "strinit:thread_create() failed\n");
		last_ncpu++;
	}
}

/*
 * This does this same as mp_strinit(), only will avoid panicing.
 */
int
mp_dr_strinit(void)
{
	int retval;

	while (last_ncpu < ncpus) {
		if ((retval = mp_strinit_mkthreads(last_ncpu)) != 0)
			return (retval);
		last_ncpu++;
	}

	return (0);
}

/*
 * This routine should be called after all stream geometry changes to update
 * the stream head cached struio() rd/wr queue pointers. Note must be called
 * with the streamlock()ed.
 *
 * Note: only enables Synchronous STREAMS for a side of a Stream which has
 *	 an explicit synchronous barrier module queue. That is, a queue that
 *	 has specified a struio() type.
 */
static void
strsetuio(stdata_t *stp)
{
	queue_t *wrq;

	if (stp->sd_flag & STPLEX) {
		/*
		 * Not stremahead, but a mux, so no Synchronous STREAMS.
		 */
		stp->sd_struiowrq = NULL;
		stp->sd_struiordq = NULL;
		return;
	}
	/*
	 * Scan the write queue(s) while synchronous
	 * until we find a qinfo uio type specified.
	 */
	wrq = stp->sd_wrq->q_next;
	while (wrq) {
		if (wrq->q_struiot == STRUIOT_NONE) {
			wrq = 0;
			break;
		}
		if (wrq->q_struiot != STRUIOT_DONTCARE)
			break;
		if (! _SAMESTR(wrq)) {
			wrq = 0;
			break;
		}
		wrq = wrq->q_next;
	}
	stp->sd_struiowrq = wrq;
	/*
	 * Scan the read queue(s) while synchronous
	 * until we find a qinfo uio type specified.
	 */
	wrq = stp->sd_wrq->q_next;
	while (wrq) {
		if (_RD(wrq)->q_struiot == STRUIOT_NONE) {
			wrq = 0;
			break;
		}
		if (_RD(wrq)->q_struiot != STRUIOT_DONTCARE)
			break;
		if (! _SAMESTR(wrq)) {
			wrq = 0;
			break;
		}
		wrq = wrq->q_next;
	}
	stp->sd_struiordq = wrq ? _RD(wrq) : 0;
}

/*
 * pass_wput, unblocks the passthru queues, so that
 * messages can arrive at muxs lower read queue, before
 * I_LINK/I_UNLINK is acked/nacked.
 */
static void
pass_wput(queue_t *q, mblk_t *mp)
{
	syncq_t *sq;

	sq = _RD(q)->q_syncq;
	if (sq->sq_flags & SQ_BLOCKED)
		unblocksq(sq, SQ_BLOCKED, 0);
	putnext(q, mp);
}

/*
 * Set up queues for the link/unlink.
 * Create a new queue and block it and then insert it
 * below the stream head on the lower stream.
 * This prevents any messages from arriving during the setq
 * as well as while the mux is processing the LINK/I_UNLINK.
 * The blocked passq is unblocked once the LINK/I_UNLINK has
 * been acked or nacked or if a message is generated and sent
 * down muxs write put procedure.
 * see pass_wput().
 */
static queue_t *
link_addpassthru(stdata_t *stpdown)
{
	queue_t *passq;

	passq = allocq();
	STREAM(passq) = STREAM(_WR(passq)) = stpdown;
	/*
	 * We must make sure pushcnt is accurate, so that build_sqlist()
	 * computes the correct number of syncq's on the stream. Also,
	 * clear STRQNEXTLESS to make sure syncq's are held to prevent
	 * thread from entering from below.
	 */
	mutex_enter(&STREAM(passq)->sd_lock);
	stpdown->sd_pushcnt++;
	stpdown->sd_flag &= ~STRQNEXTLESS;
	mutex_exit(&STREAM(passq)->sd_lock);
	/* setq might sleep in allocator - avoid holding locks. */
	setq(passq, &passthru_rinit, &passthru_winit, NULL, NULL,
		QPERQ, SQ_CI|SQ_CO, 0);
	claimq(passq, B_TRUE);
	blocksq(passq->q_syncq, SQ_BLOCKED, 1);
	insertq(STREAM(passq), passq);
	releaseq(passq, B_TRUE);
	return (passq);
}
/*
 * Let messages flow up into the mux by removing
 * the passq.
 */
static void
link_rempassthru(queue_t *passq)
{
	claimq(passq, B_TRUE);
	removeq(passq);
	mutex_enter(&STREAM(passq)->sd_lock);
	STREAM(passq)->sd_pushcnt--;
	mutex_exit(&STREAM(passq)->sd_lock);
	releaseq(passq, B_TRUE);
	freeq(passq);
}

/*
 * wait for an event with optional timeout and optional return if
 * a signal is sent to the thread
 * tim:  -1 : no timeout
 *       otherwise the value is relative time in milliseconds to wait
 * nosig:  if 0 then signals will be ignored, otherwise signals
 *       will terminate wait
 * returns >0 on success, 0 if signal was encountered, -1 if timeout
 * was reached.
 */
clock_t
str_cv_wait(kcondvar_t *cvp, kmutex_t *mp, clock_t tim, int nosigs)
{
	clock_t ret, now, tick;
	extern void time_to_wait(clock_t *, clock_t);

	if (tim < 0) {
		if (nosigs) {
			cv_wait(cvp, mp);
			ret = 1;
		} else {
			ret = cv_wait_sig(cvp, mp);
		}
	} else if (tim > 0) {
		/*
		 * convert milliseconds to clock ticks
		 */
		tick = MSEC_TO_TICK_ROUNDUP(tim);
		time_to_wait(&now, tick);
		if (nosigs) {
			ret = cv_timedwait(cvp, mp, now);
		} else {
			ret = cv_timedwait_sig(cvp, mp, now);
		}
	} else {
		ret = -1;
	}
	return (ret);
}

/*
 * This function determines when to deallocate the kmap cache using
 * kc_refcnt. When the cache is first created, kc_refcnt is set
 * to 1. Each active use of a kc_slot increments kc_refcnt by 1. Each call
 * back (kcfree) decrement kc_refcnt by 1. And finally, when the Stream
 * is closed, kc_refcnt is decremented.
 */
void
kmap_cache_free(kmap_cache_t *kcp)
{
	ASSERT((kcp->kc_refcnt > 0) && (kcp->kc_refcnt <= KMAP_PAGES+1));
	mutex_enter(&kcp->kc_lock);
	kcp->kc_refcnt--;
	if (kcp->kc_refcnt != 0) {
		mutex_exit(&kcp->kc_lock);
	} else {
#ifdef ZC_TEST
		if (zcdebug & ZC_DEBUG_ALL)
			debug_enter("kmap_cache_free");
#endif
		mutex_destroy(&kcp->kc_lock);
		hat_unload(kas.a_hat, kcp->kc_base, KMAP_SIZE,
		    (HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK));
		vmem_free(heap_arena, kcp->kc_base, ptob(KMAP_PAGES));
		kmem_free(kcp, sizeof (kmap_cache_t));

		/*LINTED: constant in conditional context*/
		if (USECOW)
			anon_unresv(KMAP_SIZE);
	}
}

/*
 * When TCP is done with the user buffer, "freeb" calls back here.
 * This routine unlocks the pages and tears down the cow mappings.
 * After that, the only thing left to be done is to restore PROT_WRITE on
 * user mappings if necessary. For performance reason, we leave that
 * to "struiomapin" in the top half.
 */
static void
kcfree(kc_slot_t *kc_slot)
{
	kmap_cache_t *kcp = kc_slot->ks_kcp;
	ushort_t n = kc_slot->ks_npages;
#ifdef ZC_TEST
	hrtime_t start;

	if (zcperf & 1)
		start = gethrtime();
#endif
	do {
		ASSERT(n != 0);
		page_unlock(kc_slot->ks_pp);
		ASSERT(kc_slot->ks_flag == KS_BUSY);
		/*
		 * Since we don't have mutex protecting "ks_flag", it can
		 * only be reset AFTER we've done with the rest of stuff.
		 */
		if (kc_slot->ks_ap) {
			/*
			 * This has to be called after page_unlock.
			 * Otherwise it may deadlock.
			 */
			anon_decref(kc_slot->ks_ap);
			/*
			 * Need to restore user protection. We don't need any
			 * lock when resetting kc_flag because nobody will
			 * mess with a KS_BUSY slot.
			 */
			kc_slot->ks_flag = KS_RESTORE_PROT;

			/* tell struiomapin() to clean up the user prot. */
			kcp->kc_state = KC_RESTORE_PROT;
		} else {
			/* No COW was set up. So no need to restore uprot. */
			kc_slot->ks_flag = KS_FREE;
		}
		kc_slot++;
	} while (--n != 0);

	kmap_cache_free(kcp);
#ifdef ZC_TEST
	if (zcperf & 1) {
		zckstat->zc_hrtime.value.ull += gethrtime() - start;
	}
#endif
}

/*
 * With the help of sd_kcp (kernel mapping cache), it allocates and maps
 * user pages behind uaddr into kaddr. Then it creates chain of desballoca
 * mblks returned in mpp.
 * This routine serves as a performance fast path. It will NOT try hard to
 * allocate resources. Upon any failure, it simply returns and falls back
 * to the slow path.
 *
 * On exit, it returns # of bytes successfully mapped and accounted for
 * in mpp.
 */
static size_t
strmapmsg(
	stdata_t *stp,
	caddr_t uaddr,
	caddr_t kaddr,
	size_t  count,
	mblk_t **mpp)
{
	kmap_cache_t	*kcp;
	caddr_t		kc_base;
	kc_slot_t	*kc_slot;
	page_t		*pp[MAX_MAPIN_PAGES];
	struct anon	*ap[MAX_MAPIN_PAGES];
	ssize_t		maxblk;
	size_t		size, slice;
	short		index, i;

	kcp = (kmap_cache_t *)stp->sd_kcp;
	kc_base = kcp->kc_base;
	index = (kaddr - kc_base) >> PAGESHIFT;
	kc_slot = &kcp->kc_slot[index];
	count = MIN(count >> PAGESHIFT, MAX_MAPIN_PAGES);
	mutex_enter(&kcp->kc_lock);

	/*
	 * Here we try to allocate free kmap slot(s) to use.
	 * kc_lock is used to protect ks_flag.
	 */
	for (i = 0; i < count; ++i, ++kc_slot) {
		if (kc_slot->ks_flag == KS_BUSY) {
			zckstat->zc_busyslots.value.ul++;
#if 0
			if (uaddr + i*PAGESIZE == kc_slot->ks_uaddr) {
				/*
				 * Most likely a cow fault has occurred on
				 * this uaddr. No need to restore protection
				 * on this uaddr later.
				 */
				zckstat->zc_cowfaults.value.ul++;
				kcp->kc_cowfault++;
				kc_slot->ks_uaddr = 0;
			}
#endif
			if (i == 0) {
				mutex_exit(&kcp->kc_lock);
				return (0);
			}
			count = i;
			break;
		} else {
			/*
			 * This is the only place the slot will be claimed by
			 * setting KS_BUSY. This is protected by kc_lock.
			 */
			kc_slot->ks_flag = KS_BUSY;
			/*
			 * Record the old pp, so we can tell if pp has changed.
			 */
			pp[i] = kc_slot->ks_pp;
		}
	}
	mutex_exit(&kcp->kc_lock);
	size = count << PAGESHIFT;
	/*
	 * On return, size contains # of bytes successfully mapped.
	 * For this version, USECOW is #define	to 1.
	 */
	if (cow_mapin(curproc->p_as, uaddr, kaddr, &pp[0], &ap[0],
	    &size, USECOW) == ENOTSUP) {
		mutex_enter(&stp->sd_lock);
		stp->sd_flag &= ~STMAPINOK;
		mutex_exit(&stp->sd_lock);
	}
	size = size >> PAGESHIFT;
	while (count > size) {
		/* release the kernel map slots that we won't use */
		kc_slot->ks_flag = KS_FREE;
		count--;
		kc_slot--;
	}
	if (size == 0)
		return (0);
	kcp->kc_usecnt += size;
	kc_slot = &kcp->kc_slot[index];
	for (i = 0; i < size; ++i, ++kc_slot) {
		if (kc_slot->ks_uaddr != uaddr) {
			/*
			 * Different user addr used this slot last time.
			 */
			kc_slot->ks_uaddr = uaddr;
			kc_slot->ks_pp = pp[i];
		} else if (kc_slot->ks_pp != pp[i]) {
			kc_slot->ks_pp = pp[i];
			/*
			 * Page has changed. Assume this is due to a cow fault.
			 */
			kcp->kc_cowfault++;
			zckstat->zc_cowfaults.value.ul++;
		}
		kc_slot->ks_ap = ap[i];
		uaddr += PAGESIZE;
	}
	size = size << PAGESHIFT;

	maxblk = stp->sd_maxblk;
	if (maxblk == INFPSZ)
		maxblk = size;
	/*
	 * slice is the size of each esballoc mblk. We'll get one call back
	 * for each slice.
	 */
#ifdef ZC_TEST
	slice = MAX(maxblk & PAGEMASK, zcslice * 8192);
#else
	slice = MAX(maxblk & PAGEMASK, 8192);
#endif
	count = 0;
	do {
		mblk_t	*bp, *mp;
		size_t	blksiz, count1;

		if (slice > size)
			slice = size;
		kc_slot = &kcp->kc_slot[index];
		bp = desballoca((unsigned char *)kaddr, slice, BPRI_LO,
		    &kc_slot->ks_frtn);
		mutex_enter(&kcp->kc_lock);
		kcp->kc_refcnt++;
		mutex_exit(&kcp->kc_lock);
		if (bp == NULL) {
			/*
			 * We have "size" pages left. We have to explicitly
			 * call kcfree() to unlock them.
			 */
			kc_slot->ks_npages = size >> PAGESHIFT;
			kcfree(kc_slot);
			return (count);
		}
		/*
		 * chop up the slice into maxblk size pieces
		 */
		blksiz = MIN(slice, maxblk);
		kaddr += blksiz;
		bp->b_wptr = (unsigned char *)kaddr;
		count1 = slice - blksiz;
		while (count1) {
			mp = dupb(bp);
			if (mp == NULL) {
				kc_slot->ks_npages = size >> PAGESHIFT;
				freemsg(bp);
				return (count);
			}
			mp->b_rptr = (unsigned char *)kaddr;
			blksiz = MIN(count1, maxblk);
			kaddr += blksiz;
			mp->b_wptr = (unsigned char *)kaddr;
			count1 -= blksiz;
			linkb(bp, mp);
		}
		size -= slice;
		kc_slot->ks_npages = slice >> PAGESHIFT;
		index += kc_slot->ks_npages;
		if (!*mpp)
			*mpp = bp;
		else
			linkb(*mpp, bp);
		count += slice;
	} while (size);

	return (count);
}

/*
 * For now we simply define kmap_addr() as a macro that returns
 * some address in the range without worrying about VAC alias rule.
 * The premise is that on a VAC platform the kernel mapping will be
 * uncached, and cow_mapin will flush dirty data in a write-back cache.
 */
#define	kmap_addr(addr, base, range) ((base) + (uintptr_t)(addr) % (range))

/*
 * This function tries to map in "count" bytes from "uiop" structure
 * and creates chain of mblks returned in "mpp". For better performance,
 * it caches kernel mappings in sd_kcp so if a user page has been mapped
 * in before, and the kernel map still exists in kcp, it simply reuses it.
 *
 * On exit, it returns number of bytes failed. The caller is expected to
 * call allocb/copyin() to complete the rest of mblk chain.
 */
static size_t
struiomapin(
	stdata_t *stp,
	struct uio *uiop,
	size_t	count,
	mblk_t **mpp)
{
	caddr_t		uaddr, addr, kc_base;
	struct iovec	*iov = uiop->uio_iov;
	struct as	*as;
	kmap_cache_t	*kcp;
	kc_slot_t	*kc_slot;
	size_t		size, span, completed;

	while (uiop->uio_iov->iov_len == 0) {
		uiop->uio_iov++;
		uiop->uio_iovcnt--;
	}
	iov = uiop->uio_iov;
	uaddr = iov->iov_base;
	/* round the size down to page boundary */
	size = MIN(count, iov->iov_len) & PAGEMASK;
	if (((uintptr_t)uaddr & PAGEOFFSET) || (size == 0)) {
		return (count);
	}

	as = curproc->p_as;
	mutex_enter(&stp->sd_lock);
	/*
	 * sd_lock protects allocation of sd_kcp and its owner (kc_as).
	 */
	if (stp->sd_kcp == 0) {
		short i;

		/* for this version, USECOW is #define	to 1 */
		if (USECOW && anon_resv(KMAP_SIZE) == 0) {
			mutex_exit(&stp->sd_lock);
			return (count);
		}
		kc_base = vmem_alloc(heap_arena, ptob(KMAP_PAGES), VM_NOSLEEP);
		if (kc_base == 0) {
			cmn_err(CE_WARN, "struiomapin: no kernel heap arena");

			/*LINTED: constant in conditional context*/
			if (USECOW)
				anon_unresv(KMAP_SIZE);
			mutex_exit(&stp->sd_lock);
			return (count);
		}
		kcp = kmem_zalloc(sizeof (kmap_cache_t), KM_SLEEP);
		stp->sd_kcp = kcp;
		kcp->kc_base = kc_base;
		kcp->kc_refcnt = 1;
		kcp->kc_as = as;
		mutex_init(&kcp->kc_lock, NULL, MUTEX_DEFAULT, NULL);
		for (i = 0, kc_slot = &kcp->kc_slot[0]; i < KMAP_PAGES;
		    i++, kc_slot++) {
			kc_slot->ks_frtn.free_func = kcfree;
			kc_slot->ks_frtn.free_arg = (caddr_t)kc_slot;
			kc_slot->ks_kcp = kcp;
			kc_slot->ks_index = (uchar_t)i;

		}
		zckstat->zc_kmapcaches.value.ul++;
#ifdef ZC_TEST
		if (zcdebug & ZC_DEBUG_ALL) {
			printf("stp=%X/%X\n", stp, stp->sd_kcp);
			debug_enter("struiomapin");
		}
#endif
	} else {
		kcp = (kmap_cache_t *)stp->sd_kcp;
		if (kcp->kc_as != as) {
			/*
			 * Even if kc_refcnt == 1, the kmap cache can't be
			 * reused because another thread might be running
			 * in kc_as and tampering with kcp.
			 */
			mutex_exit(&stp->sd_lock);
			return (count);
		}
		/*
		 * We check periodically to see if too many cow faults have
		 * occurred that we should turn off zero-copy (mapin).
		 */
		if (kcp->kc_usecnt > strzc_cow_check_period) {
			if (kcp->kc_cowfault > strzc_cowfault_allowed) {
				zckstat->zc_disabled.value.ul++;
#ifdef ZC_TEST
				if (zcdebug & ZC_DEBUG) {
					printf("disable zero-copy, kcp=%X\n",
					    kcp);
					debug_enter("struiomapin");
				}
#endif
				stp->sd_flag &= ~STMAPINOK;
				/*
				 * We can't deallocate sd_kcp here because
				 * other threads might be using it. It will
				 * be freed when the stream is closed.
				 */
				mutex_exit(&stp->sd_lock);
				return (count);
			}
			kcp->kc_usecnt = 0;
			kcp->kc_cowfault = 0;
		}
		kc_base = kcp->kc_base;
	}
	mutex_exit(&stp->sd_lock);
	/*
	 * kmap_addr returns addr in the
	 * [kc_base..kc_base+KMAP_SIZE-PAGESIZE] range.
	 * We use KMAP_SIZE-PAGESIZE here to ensure that addr can accommodate
	 * at least two pages. This is solely for performance purpose.
	 */
	addr = kmap_addr(uaddr, kc_base, KMAP_SIZE-PAGESIZE);
	if ((intptr_t)addr == -1) {
		return (count);
	}
	/*
	 * If we reach the high end of the kernel addr range, we may have to
	 * split the processing into two parts, since strmapmsg() only takes
	 * contiguous addr.
	 */
	if (addr + size > kc_base + KMAP_SIZE)
		span = kc_base + KMAP_SIZE - addr;
	else
		span = size;

	completed = strmapmsg(stp, uaddr, addr, span, mpp);
	if ((size > span) && (completed == span)) {
		/*
		 * The first one succeeded. We wrap around to kc_base and
		 * do the second one.
		 */
		uaddr += span;
		addr = kmap_addr(uaddr, kc_base, KMAP_SIZE-PAGESIZE);
		if ((intptr_t)addr != -1) {
			span = size - span;
			completed += strmapmsg(stp, uaddr, addr, span, mpp);
		}
	}
	zckstat->zc_mapins.value.ul += completed >> PAGESHIFT;
	iov->iov_base += completed;
	iov->iov_len -= completed;
	uiop->uio_resid -= completed;
	uiop->uio_loffset += completed;
	if (kcp->kc_state == KC_CLEAN)
		return (count-completed);

	/*
	 * Here we check if PROT_WRITE needs to be restored on a user
	 * buffer after we've done with it. We could just let it
	 * fault when the user tries to write to it, and the fault
	 * handler would restore the correct protection. But the
	 * performance penalty is too high.
	 *
	 * We have to go though the segment driver to do the job
	 * correctly because things might have changed since the time
	 * we last mapin the user buffer.
	 */
	kcp->kc_state = KC_CLEAN;
	kc_slot = &kcp->kc_slot[0];
	mutex_enter(&kcp->kc_lock);
	do {
		if ((kc_slot->ks_flag == KS_RESTORE_PROT) &&
		    (addr = kc_slot->ks_uaddr) != 0) {
			kc_slot->ks_flag = KS_FREE;
			size = PAGESIZE;
			while ((++kc_slot < &kcp->kc_slot[KMAP_PAGES]) &&
			    kc_slot->ks_flag == KS_RESTORE_PROT) {
				if (kc_slot->ks_uaddr != (addr + size)) {
					break;
				}
				kc_slot->ks_flag = KS_FREE;
				size += PAGESIZE;
			}
			span = as_fault(as->a_hat, as, addr, size,
			    F_PROT, S_READ);
#ifdef ZC_TEST
			if (span && (zcdebug & ZC_ERROR)) {
				printf("error=%X addr=%X/%X\n",
				    span, addr, size);
				debug_enter("struiomapin: as_fault");
			}
#endif
		} else {
			kc_slot++;
		}
	} while (kc_slot < &kcp->kc_slot[KMAP_PAGES]);
	mutex_exit(&kcp->kc_lock);
	return (count-completed);
}

static void
zckstat_create(void)
{
	kstat_t *ksp;

	ksp = kstat_create("streams", 0, "zero_copy", "net", KSTAT_TYPE_NAMED,
	    sizeof (struct zero_copy_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_WRITABLE);
	if (ksp == NULL) {
		return;
	}
	zckstat = (struct zero_copy_kstat *)KSTAT_NAMED_PTR(ksp);
	kstat_named_init(&zckstat->zc_mapins, "mapins", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_pageflips, "pageflips", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_misses, "misses", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_kmapcaches, "kmapcaches",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_cowfaults, "cowfaults", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_disabled, "disabled", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_busyslots, "busyslots", KSTAT_DATA_ULONG);
#ifdef ZC_TEST
	kstat_named_init(&zckstat->zc_count, "count", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_hrtime, "hrtime", KSTAT_DATA_ULONGLONG);
	kstat_named_init(&zckstat->zc_hwcksum_w, "hwcksum_w", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_swcksum_w, "swcksum_w", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_hwcksum_r, "hwcksum_r", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_swcksum_r, "swcksum_r", KSTAT_DATA_ULONG);
	kstat_named_init(&zckstat->zc_slowpath_r, "slowpath_r",
	    KSTAT_DATA_ULONG);
#endif
	kstat_install(ksp);
}

/*
 * Wait until the stream head can determine if it is at the mark but
 * don't wait forever to prevent a race condition between the "mark" state
 * in the stream head and any mark state in the caller/user of this routine.
 *
 * This is used by sockets and for a socket it would be incorrect
 * to return a failure for SIOCATMARK when there is no data in the receive
 * queue and the marked urgent data is traveling up the stream.
 *
 * This routine waits until the mark is known by waiting for one of these
 * three events:
 *	The stream head read queue becoming non-empty (including an EOF)
 *	The STRATMARK flag being set. (Due to a MSGMARKNEXT message.)
 *	The STRNOTATMARK flag being set (which indicates that the transport
 *	has sent a MSGNOTMARKNEXT message to indicate that it is not at
 *	the mark).
 *
 * The routine returns 1 if the stream is at the mark; 0 if it can
 * be determined that the stream is not at the mark.
 * If the wait times out and it can't determine
 * whether or not the stream might be at the mark the routine will return -1.
 *
 * Note: This routine should only be used when a mark is pending i.e.,
 * in the socket case the SIGURG has been posted.
 * Note2: This can not wakeup just because synchronous streams indicate
 * that data is available since it is not possible to use the synchronous
 * streams interfaces to determine the b_flag value for the data queued below
 * the stream head.
 */
int
strwaitmark(vnode_t *vp)
{
	struct stdata *stp = vp->v_stream;
	queue_t *rq = _RD(stp->sd_wrq);
	int mark;

	mutex_enter(&stp->sd_lock);
	while (rq->q_first == NULL &&
	    !(stp->sd_flag & (STRATMARK|STRNOTATMARK|STREOF))) {
		stp->sd_flag |= RSLEEP;

		/* Wait for 100 milliseconds for any state change. */
		if (str_cv_wait(&rq->q_wait, &stp->sd_lock, 100, 1) == -1) {
			mutex_exit(&stp->sd_lock);
			return (-1);
		}
	}
	if (stp->sd_flag & STRATMARK)
		mark = 1;
	else if (rq->q_first != NULL && (rq->q_first->b_flag & MSGMARK))
		mark = 1;
	else
		mark = 0;

	mutex_exit(&stp->sd_lock);
	return (mark);
}

/*
 * Set a read side error. If persist is set change the socket error
 * to persistent. If errfunc is set install the function as the exported
 * error handler.
 */
void
strsetrerror(vnode_t *vp, int error, int persist, errfunc_t errfunc)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);
	stp->sd_rerror = error;
	if (error == 0 && errfunc == NULL)
		stp->sd_flag &= ~STRDERR;
	else
		stp->sd_flag |= STRDERR;
	if (persist) {
		stp->sd_flag &= ~STRDERRNONPERSIST;
	} else {
		stp->sd_flag |= STRDERRNONPERSIST;
	}
	stp->sd_rderrfunc = errfunc;
	if (error != 0 || errfunc != NULL) {
		cv_broadcast(&_RD(stp->sd_wrq)->q_wait);	/* readers */
		cv_broadcast(&stp->sd_wrq->q_wait);		/* writers */
		cv_broadcast(&stp->sd_monitor);			/* ioctllers */

		if (stp->sd_sigflags & S_ERROR)
			strsendsig(stp->sd_siglist, S_ERROR, 0, error);
		mutex_exit(&stp->sd_lock);
		pollwakeup(&stp->sd_pollist, POLLERR);
	} else
		mutex_exit(&stp->sd_lock);
}

/*
 * Set a write side error. If persist is set change the socket error
 * to persistent.
 */
void
strsetwerror(vnode_t *vp, int error, int persist, errfunc_t errfunc)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);
	stp->sd_werror = error;
	if (error == 0 && errfunc == NULL)
		stp->sd_flag &= ~STWRERR;
	else
		stp->sd_flag |= STWRERR;
	if (persist) {
		stp->sd_flag &= ~STWRERRNONPERSIST;
	} else {
		stp->sd_flag |= STWRERRNONPERSIST;
	}
	stp->sd_wrerrfunc = errfunc;
	if (error != 0 || errfunc != NULL) {
		cv_broadcast(&_RD(stp->sd_wrq)->q_wait);	/* readers */
		cv_broadcast(&stp->sd_wrq->q_wait);		/* writers */
		cv_broadcast(&stp->sd_monitor);			/* ioctllers */

		if (stp->sd_sigflags & S_ERROR)
			strsendsig(stp->sd_siglist, S_ERROR, 0, error);
		mutex_exit(&stp->sd_lock);
		pollwakeup(&stp->sd_pollist, POLLERR);
	} else
		mutex_exit(&stp->sd_lock);
}

/*
 * Make the stream return 0 (EOF) when all data has been read.
 * No effect on write side.
 */
void
strseteof(vnode_t *vp, int eof)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);
	if (!eof) {
		stp->sd_flag &= ~STREOF;
		mutex_exit(&stp->sd_lock);
		return;
	}
	stp->sd_flag |= STREOF;
	if (stp->sd_flag & RSLEEP) {
		stp->sd_flag &= ~RSLEEP;
		cv_broadcast(&_RD(stp->sd_wrq)->q_wait);
	}

	if (stp->sd_sigflags & (S_INPUT|S_RDNORM))
		strsendsig(stp->sd_siglist, S_INPUT|S_RDNORM, 0, 0);
	mutex_exit(&stp->sd_lock);
	pollwakeup(&stp->sd_pollist, POLLIN|POLLRDNORM);
}

void
strflushrq(vnode_t *vp, int flag)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);
	flushq(_RD(stp->sd_wrq), flag);
	mutex_exit(&stp->sd_lock);
}

void
strsetrputhooks(vnode_t *vp, uint_t flags,
		msgfunc_t protofunc, msgfunc_t miscfunc)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);

	if (protofunc == NULL)
		stp->sd_rprotofunc = strrput_proto;
	else
		stp->sd_rprotofunc = protofunc;

	if (miscfunc == NULL)
		stp->sd_rmiscfunc = strrput_misc;
	else
		stp->sd_rmiscfunc = miscfunc;

	if (flags & SH_CONSOL_DATA)
		stp->sd_rput_opt |= SR_CONSOL_DATA;
	else
		stp->sd_rput_opt &= ~SR_CONSOL_DATA;

	if (flags & SH_SIGALLDATA)
		stp->sd_rput_opt |= SR_SIGALLDATA;
	else
		stp->sd_rput_opt &= ~SR_SIGALLDATA;

	if (flags & SH_IGN_ZEROLEN)
		stp->sd_rput_opt |= SR_IGN_ZEROLEN;
	else
		stp->sd_rput_opt &= ~SR_IGN_ZEROLEN;

	mutex_exit(&stp->sd_lock);
}

void
strsetwputhooks(vnode_t *vp, uint_t flags, clock_t closetime)
{
	struct stdata *stp = vp->v_stream;

	mutex_enter(&stp->sd_lock);
	stp->sd_closetime = closetime;

	if (flags & SH_SIGPIPE)
		stp->sd_wput_opt |= SW_SIGPIPE;
	else
		stp->sd_wput_opt &= ~SW_SIGPIPE;
	if (flags & SH_RECHECK_ERR)
		stp->sd_wput_opt |= SW_RECHECK_ERR;
	else
		stp->sd_wput_opt &= ~SW_RECHECK_ERR;

	mutex_exit(&stp->sd_lock);
}

/*
 * Get process group id
 */

pid_t
strgetpgrp(struct vnode *vp)
{
	pid_t pid_id;
	struct stdata *stp;

	if ((stp = vp->v_stream) == NULL)
		return (-1);
	mutex_enter(&stp->sd_lock);
	if (stp->sd_pgidp == NULL) {
		mutex_exit(&stp->sd_lock);
		return (-1);
	}
	pid_id = stp->sd_pgidp->pid_id;
	mutex_exit(&stp->sd_lock);
	return (pid_id);
}

/*
 * Set process group id
 */

int
strsetpgrp(struct vnode *vp, pid_t pid_id)
{
	struct stdata *stp;

	if (((stp = vp->v_stream) == NULL) || (pid_id == -1))
		return (-1);
	mutex_enter(&stp->sd_lock);
	stp->sd_pgidp->pid_id = pid_id;
	mutex_exit(&stp->sd_lock);
	return (0);
}


/* NOTE: Do not add code after this point. */
#undef QLOCK

/*
 * replacement for QLOCK macro for those that can't use it.
 */
kmutex_t *
QLOCK(queue_t *q)
{
	return (&(q)->q_lock);
}
