/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STRSUBR_H
#define	_SYS_STRSUBR_H

#pragma ident	"@(#)strsubr.h	1.107	99/08/10 SMI"	/* SVr4.0 1.17	*/

/*
 * WARNING:
 * Everything in this file is private, belonging to the
 * STREAMS subsystem.  The only guarantee made about the
 * contents of this file is that if you include it, your
 * code will not port to the next release.
 */
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/session.h>
#include <sys/kstat.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/anon.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * In general, the STREAMS locks are disjoint; they are only held
 * locally, and not simultaneously by a thread.  However, module
 * code, including at the stream head, requires some locks to be
 * acquired in order for its safety.
 *	1. Stream level claim.  This prevents the value of q_next
 *		from changing while module code is executing.
 *	2. Queue level claim.  This prevents the value of q_ptr
 *		from changing while put or service code is executing.
 *		In addition, it provides for queue single-threading
 *		for QPAIR and PERQ MT-safe modules.
 *	3. Stream head lock.  May be held by the stream head module
 *		to implement a read/write/open/close monitor.
 *	   Note: that the only types of twisted stream supported are
 *	   the pipe and transports which have read and write service
 *	   procedures on both sides of the twist.
 *	4. Queue lock.  May be acquired by utility routines on
 *		behalf of a module.
 */

/*
 * In general, sd_lock protects the consistency of the stdata
 * structure.  Additionally, it is used with sd_monitor
 * to implement an open/close monitor.  In particular, it protects
 * the following fields:
 *	sd_iocblk
 *	sd_flag
 *	sd_iocid
 *	sd_iocwait
 *	sd_sidp
 *	sd_pgidp
 *	sd_wroff
 *	sd_rerror
 *	sd_werror
 *	sd_pushcnt
 *	sd_sigflags
 *	sd_siglist
 *	sd_pollist
 *	sd_mark
 *	sd_closetime
 *	sd_wakeq
 *	sd_uiordq
 *	sd_uiowrq
 *	sd_maxblk
 *	sd_kcp
 *
 * The following fields are modified only by the allocator, which
 * has exclusive access to them at that time:
 *	sd_wrq
 *	sd_strtab
 *
 * The following field is protected by the overlying file system
 * code, guaranteeing single-threading of opens:
 *	sd_vnode
 *
 * Stream-level locks should be acquired before any queue-level locks
 *	are acquired.
 *
 * The stream head write queue lock(sd_wrq) is used to protect the
 * fields qn_maxpsz and qn_minpsz because freezestr() which is
 * necessary for strqset() only gets the queue lock.
 */

/*
 * Function types for the parameterized stream head.
 * The msgfunc_t takes the parameters:
 * 	msgfunc(vnode_t *vp, mblk_t *mp, strwakeup_t *wakeups,
 *		strsigset_t *firstmsgsigs, strsigset_t *allmsgsigs,
 *		strpollset_t *pollwakeups);
 * It returns an optional message to be processed by the stream head.
 *
 * The parameters for errfunc_t are:
 *	errfunc(vnode *vp, int ispeek, int *clearerr);
 * It returns an errno and zero if there was no pending error.
 */
typedef uint_t	strwakeup_t;
typedef uint_t	strsigset_t;
typedef short	strpollset_t;
typedef uintptr_t callbparams_id_t;
typedef	mblk_t	*(*msgfunc_t)(vnode_t *, mblk_t *, strwakeup_t *,
			strsigset_t *, strsigset_t *, strpollset_t *);
typedef int 	(*errfunc_t)(vnode_t *, int, int *);

/*
 * Per stream sd_lock in putnext may be replaced by per cpu stream_putlocks
 * each living in a separate cache line. putnext/canputnext grabs only one of
 * stream_putlocks while strlock() (called on behalf of insertq()/removeq())
 * acquires all stream_putlocks. Normally stream_putlocks are only employed
 * for highly contended streams that have SQ_CIPUT queues in the critical path
 * (e.g. NFS/UDP stream).
 *
 * stream_putlocks are dynamically assigned to stdata structure through
 * sd_ciputctrl pointer possibly when a stream is already in use. Since
 * strlock() uses stream_putlocks only under sd_lock acquiring sd_lock when
 * assigning stream_putlocks to the stream ensures syncronization with
 * strlock().
 *
 * For lock ordering purposes stream_putlocks are treated as the extension of
 * sd_lock and are always grabbed right after grabbing sd_lock and released
 * right before releasing sd_lock except putnext/canputnext where only one of
 * stream_putlocks locks is used and where it is the first lock to grab.
 */

typedef struct ciputctrl_str {
	union _ciput_un {
		uchar_t	pad[64];
		struct _ciput_str {
			kmutex_t	ciput_lck;
			ushort_t	ciput_cnt;
		} ciput_str;
	} ciput_un;
} ciputctrl_t;

#define	ciputctrl_lock	ciput_un.ciput_str.ciput_lck
#define	ciputctrl_count	ciput_un.ciput_str.ciput_cnt

/*
 * Header for a stream: interface to rest of system.
 */
typedef struct stdata {
	struct queue	*sd_wrq;	/* write queue */
	struct msgb	*sd_iocblk;	/* return block for ioctl */
	struct vnode	*sd_vnode;	/* pointer to associated vnode */
	struct streamtab *sd_strtab;	/* pointer to streamtab for stream */
	uint_t		sd_flag;	/* state/flags */
	uint_t		sd_iocid;	/* ioctl id */
	struct pid	*sd_sidp;	/* controlling session info */
	struct pid	*sd_pgidp;	/* controlling process group info */
	ushort_t	dummy;		/* XXX UNUSED */
	ushort_t	sd_wroff;	/* write offset */
	int		sd_rerror;	/* read error to set u.u_error */
	int		sd_werror;	/* write error to set u.u_error */
	int		sd_pushcnt;	/* number of pushes done on stream */
	int		sd_sigflags;	/* logical OR of all siglist events */
	struct strsig	*sd_siglist;	/* pid linked list to rcv SIGPOLL sig */
	struct pollhead sd_pollist;	/* list of all pollers to wake up */
	struct msgb	*sd_mark;	/* "marked" message on read queue */
	clock_t		sd_closetime;	/* time to wait to drain q in close */
	kmutex_t	sd_lock;	/* protect head consistency */
	kcondvar_t	sd_monitor;	/* open/close/push/pop monitor */
	kcondvar_t	sd_iocmonitor;	/* ioctl single-threading */
	ssize_t		sd_qn_minpsz;	/* These two fields are a performance */
	ssize_t		sd_qn_maxpsz;	/* enhancements, cache the values in */
					/* the stream head so we don't have */
					/* to ask the module below the stream */
					/* head to get this information. */
	struct stdata	*sd_mate;	/* pointer to twisted stream mate */
	kthread_id_t	sd_freezer;	/* thread that froze stream */
	kmutex_t	sd_reflock;	/* Protects sd_refcnt */
	int		sd_refcnt;	/* number of claimstr */
	struct stdata	*sd_next;	/* next in list of all stream heads */
	struct stdata	*sd_prev;	/* prev in list of all stream heads */
	uint_t		sd_wakeq;	/* strwakeq()'s copy of sd_flag */
	struct queue	*sd_struiordq;	/* sync barrier struio() read queue */
	struct queue	*sd_struiowrq;	/* sync barrier struio() write queue */
	char		sd_struiodnak;	/* defer NAK of M_IOCTL by rput() */
	struct msgb	*sd_struionak;	/* pointer M_IOCTL mblk(s) to NAK */
	caddr_t		sd_t_audit_data; /* For audit purposes only */
	struct vnode	*sd_vnfifo;	/* fifo vnode held */
	ssize_t		sd_maxblk;	/* maximum message block size */
	struct kmap_cache *sd_kcp;	/* point to the kernel mapping cache */
					/* when sender zero-copy is enabled. */
	uint_t		sd_rput_opt;	/* options/flags for strrput */
	uint_t		sd_wput_opt;	/* options/flags for write/putmsg */
	uint_t		sd_read_opt;	/* options/flags for strread */
	msgfunc_t	sd_rprotofunc;	/* rput M_*PROTO routine */
	msgfunc_t	sd_rmiscfunc;	/* rput routine (non-data/proto) */
	errfunc_t	sd_rderrfunc;	/* read side error callback */
	errfunc_t	sd_wrerrfunc;	/* write side error callback */
	/*
	 * support for low contention concurrent putnext.
	 */
	ciputctrl_t	*sd_ciputctrl;
	uint_t		sd_nciputctrl;

	int		sd_anchor;	/* position of anchor in stream */
	/*
	 * Service scheduling at the stream head.
	 */
	struct stdata	*sd_stnext;	/* Next in list of stream heads	*/
	struct stdata	*sd_stprev;	/* Previous in list of stream heads */
	struct queue	*sd_qhead;	/* Head of queues to be serviced. */
	ssize_t		sd_svcpri;	/* Priority of service		*/
	/*
	 * Streams Flow Trace varibales.
	 */
	uint_t		sd_ftflags;	/* Flow Trace flags		*/
	ftflw_t	***sd_ftflw_head;	/* Pointer to array of flow heads */
					/* Array is indexed by hash value */
} stdata_t;

/*
 * stdata flow trace flags
 */
#define	STRFT_TRACE	0x00000001	/* This stream is flow traced */


/*
 * stdata flag field defines
 */
#define	IOCWAIT		0x00000001	/* Someone wants to do ioctl */
#define	RSLEEP		0x00000002	/* Someone wants to read/recv msg */
#define	WSLEEP		0x00000004	/* Someone wants to write */
#define	STRPRI		0x00000008	/* An M_PCPROTO is at stream head */
#define	STRHUP		0x00000010	/* Device has vanished */
#define	STWOPEN		0x00000020	/* waiting for 1st open */
#define	STPLEX		0x00000040	/* stream is being multiplexed */
#define	STRISTTY	0x00000080	/* stream is a terminal */
#define	STRGETINPROG	0x00000100	/* (k)strgetmsg is running */
					/* Unused (0x200) */
#define	STRDERR		0x00000400	/* fatal read error from M_ERROR */
#define	STWRERR		0x00000800	/* fatal write error from M_ERROR */
#define	STRDERRNONPERSIST 0x00001000	/* nonpersistent read errors */
#define	STWRERRNONPERSIST 0x00002000	/* nonpersistent write errors */
#define	STRCLOSE	0x00004000	/* wait for a close to complete */
#define	SNDMREAD	0x00008000	/* used for read notification */
#define	OLDNDELAY	0x00010000	/* use old TTY semantics for */
					/* NDELAY reads and writes */
#define	STRQNEXTLESS	0x00020000	/* Read side qnext is accesed safely */
#define	STRSNDZERO	0x00040000	/* send 0-length msg. down pipe/FIFO */
#define	STRTOSTOP	0x00080000	/* block background writes */
#define	STMAPINOK	0x00100000	/* ok to mapin instead of copyin */
#define	STREMAPOK	0x00200000	/* ok to pageflip instead of copyout */
#define	STRMOUNT	0x00400000	/* stream is mounted */
#define	STRNOTATMARK	0x00800000	/* Not at mark (when empty read q) */
#define	STRDELIM	0x01000000	/* generate delimited messages */
#define	STRATMARK	0x02000000	/* At mark (due to MSGMARKNEXT) */
#define	STRPLUMB	0x08000000	/* push/pop pending */
#define	STREOF		0x10000000	/* End-of-file indication */
#define	STREOPENFAIL	0x20000000	/* indicates if re-open has failed */
#define	STRMATE		0x40000000	/* this stream is a mate */
#define	STRHASLINKS	0x80000000	/* I_LINKs under this stream */

/*
 * Options and flags for strrput (sd_rput_opt)
 */
#define	SR_POLLIN	0x00000001	/* pollwakeup needed for band0 data */
#define	SR_SIGALLDATA	0x00000002	/* Send SIGPOLL for all M_DATA */
#define	SR_CONSOL_DATA	0x00000004	/* Consolidate M_DATA onto q_last */
#define	SR_IGN_ZEROLEN	0x00000008	/* Ignore zero-length M_DATA */

/*
 * Options and flags for strwrite/strputmsg (sd_wput_opt)
 */
#define	SW_SIGPIPE	0x00000001	/* Send SIGPIPE for write error */
#define	SW_RECHECK_ERR	0x00000002	/* Recheck errors in strwrite loop */

/*
 * Options and flags for strread (sd_read_opt)
 */
#define	RD_MSGDIS	0x00000001	/* read msg discard */
#define	RD_MSGNODIS	0x00000002	/* read msg no discard */
#define	RD_PROTDAT	0x00000004	/* read M_[PC]PROTO contents as data */
#define	RD_PROTDIS	0x00000008	/* discard M_[PC]PROTO blocks and */
					/* retain data blocks */
/*
 * Flags parameter for strsetrputhooks() and strsetwputhooks().
 * These flags define the interface for setting the above internal
 * flags in sd_rput_opt and sd_wput_opt.
 */
#define	SH_CONSOL_DATA	0x00000001	/* Consolidate M_DATA onto q_last */
#define	SH_SIGALLDATA	0x00000002	/* Send SIGPOLL for all M_DATA */
#define	SH_IGN_ZEROLEN	0x00000004	/* Drop zero-length M_DATA */

#define	SH_SIGPIPE	0x00000100	/* Send SIGPIPE for write error */
#define	SH_RECHECK_ERR	0x00000200	/* Recheck errors in strwrite loop */

/*
 * Each queue points to a sync queue (the inner perimeter) which keeps
 * track of the number of threads that are inside a given queue (sq_count)
 * and also is used to implement the asynchronous putnext
 * (by queuing messages if the queue can not be entered.)
 *
 * Messages are queued on sq_head/sq_tail including deferred qwriter(INNER)
 * messages. The sq_head/sq_tail list is a singly-linked list with
 * b_queue recording the queue and b_prev recording the function to
 * be called (either the put procedure or a qwriter callback function.)
 *
 * The counters sq_count/sq_occount track the number of threads that are either
 * executing inside the perimeter or (in the case of outer perimeters)
 * have some work queued for them relating to the perimeter. sq_occount
 * tracks the subset which is executing in the open or close procedure
 * of the module/driver's perimeter.
 *
 * In addition a module writer can declare that the module has an outer
 * perimeter (by setting D_MTOUTPERIM) in which case all inner perimeter
 * syncq's for the module point (through sq_outer) to an outer perimeter
 * syncq. The outer perimeter consists of the doubly linked list (sq_onext and
 * sq_oprev) linking all the inner perimeter syncq's with out outer perimeter
 * syncq. This is used to implement qwriter(OUTER) (an asynchronous way of
 * getting exclusive access at the outer perimeter) and outer_enter/exit
 * which are used by the framework to acquire exclusive access to the outer
 * perimeter during open and close of modules that have set D_MTOUTPERIM.
 *
 * In the inner perimeter case sq_save is available for use by machine
 * dependent code. sq_head/sq_tail are used to queue deferred messages on
 * the inner perimeter syncqs and to queue become_writer requests on the
 * outer perimeter syncqs.
 *
 * Note: machine dependent optimized versions of putnext may depend
 * on the order of sq_flags and sq_count (so that they can e.g.
 * read these two fields in a single load instruction.)
 *
 * Per perimeter SQLOCK/sq_count in putnext/put may be replaced by per cpu
 * sq_putlocks/sq_putcounts each living in a separate cache line. Obviously
 * sq_putlock[x] protects sq_putcount[x]. putnext/put routine will grab only 1
 * of sq_putlocks and update only 1 of sq_putcounts. strlock() and many
 * other routines in strsubr.c and ddi.c will grab all sq_putlocks (as well as
 * SQLOCK) and figure out the count value as the sum of sq_count and all of
 * sq_putcounts. The idea is to make critical fast path -- putnext -- much
 * faster at the expense of much less often used slower path like
 * strlock(). One known case where entersq/strlock is executed pretty often is
 * SpecWeb but since IP is SQ_CIOC and socket TCP/IP stream is nextless
 * there's no need to grab multiple sq_putlocks and look at sq_putcounts. See
 * strsubr.c for more comments.
 *
 * Note regular SQLOCK and sq_count are still used in many routines
 * (e.g. entersq(), rwnext()) in the same way as before sq_putlocks were
 * introduced.
 *
 * To understand when all sq_putlocks need to be held and all sq_putcounts
 * need to be added up one needs to look closely at putnext code. Basically if
 * a routine like e.g. wait_syncq() needs to be sure that perimeter is empty
 * all sq_putlocks/sq_putcounts need to be held/added up. On the other hand
 * there's no need to hold all sq_putlocks and count all sq_putcounts in
 * routines like leavesq()/dropsq() and etc. since the are usually exit
 * counterparts of entersq/outer_enter() and etc. which have already either
 * prevented put entry poins from executing or did not care about put
 * entrypoints. entersq() doesn't need to care about sq_putlocks/sq_putcounts
 * if the entry point has a shared access since put has the highest degree of
 * concurrency and such entersq() does not intend to block out put
 * entrypoints.
 *
 * Before sq_putcounts were introduced the standard way to wait for perimeter
 * to become empty was:
 *
 *	mutex_enter(SQLOCK(sq));
 *	while (sq->sq_count > 0) {
 *		sq->sq_flags |= SQ_WANTWAKEUP;
 *		cv_wait(&sq->sq_wait, SQLOCK(sq));
 *	}
 *	mutex_exit(SQLOCK(sq));
 *
 * The new way is:
 *
 * 	mutex_enter(SQLOCK(sq));
 *	count = sq->sq_count;
 *	SQ_PUTLOCKS_ENTER(sq);
 *	SUM_SQ_PUTCOUNTS(sq, count);
 *	while (count != 0) {
 *		sq->sq_flags |= SQ_WANTWAKEUP;
 *		SQ_PUTLOCKS_EXIT(sq);
 *		cv_wait(&sq->sq_wait, SQLOCK(sq));
 *		count = sq->sq_count;
 *		SQ_PUTLOCKS_ENTER(sq);
 *		SUM_SQ_PUTCOUNTS(sq, count);
 *	}
 *	SQ_PUTLOCKS_EXIT(sq);
 *	mutex_exit(SQLOCK(sq));
 *
 * Note that SQ_WANTWAKEUP is set before dropping SQ_PUTLOCKS. This makes sure
 * putnext won't skip a wakeup.
 *
 * sq_putlocks are treated as the extension of SQLOCK for lock ordering
 * purposes and are always grabbed right after grabbing SQLOCK and released
 * right before releasing SQLOCK. This also allows dynamic creation of
 * sq_putlocks while holding SQLOCK (by making sq_ciputctrl non null even when
 * the stream is already in use). Only in putnext one of sq_putlocks
 * is grabbed instead of SQLOCK. putnext return path remembers what counter it
 * incremented and decrements the right counter on its way out.
 */

struct syncq {
	kmutex_t	sq_lock;	/* atomic access to syncq */
	struct _sq_save {
		void	*l4;		/* saved sparc %l4 reg */
		void	*l5;		/* saved sparc %l5 reg */
	}		sq_save;	/* used by machine dependent, */
					/* optimized putnext */
	uint16_t	sq_count;	/* # threads inside */
	uint16_t	sq_flags;	/* state and some type info */
	/*
	 * Distributed syncq scheduling
	 *  The list of queue's is handled by sq_head and
	 *  sq_tail fields.
	 *
	 *  The list of events is handled by the sq_evhead and sq_evtail
	 *  fields.
	 */
	queue_t		*sq_head;	/* queue of deferred messages */
	queue_t		*sq_tail;	/* queue of deferred messages */
	mblk_t		*sq_evhead;	/* Event message on the syncq */
	mblk_t		*sq_evtail;
	uint_t		sq_nqueues;	/* # of queues on this sq */
	/*
	 * Concurrency and condition variables
	 */
	uint16_t	sq_type;	/* type (concurrency) of syncq */
	ushort_t	sq_occount;	/* # threads inside open/close */
	kcondvar_t 	sq_wait;	/* block on this sync queue */
	kcondvar_t 	sq_exitwait;	/* waiting for thread to leave the */
					/* inner perimeter */
	/*
	 * Handling synchronous callbacks such as qtimeout and qbufcall
	 */
	ushort_t	sq_callbflags;	/* flags for callback synchronization */
	callbparams_id_t sq_cancelid;	/* id of callback being cancelled */
	struct callbparams *sq_callbpend;	/* Pending callbacks */

	/*
	 * Links forming an outer perimeter from one outer syncq and
	 * a set of inner sync queues.
	 */
	struct syncq	*sq_outer;	/* Pointer to outer perimeter */
	struct syncq	*sq_onext;	/* Linked list of syncq's making */
	struct syncq	*sq_oprev;	/* up the outer perimeter. */
	/*
	 * support for low contention concurrent putnext.
	 */
	ciputctrl_t	*sq_ciputctrl;
	uint_t		sq_nciputctrl;
	/*
	 * Counter for the number of threads wanting to become exclusive.
	 */
	uint_t		sq_needexcl;
	/*
	 * Any extra information needed by STREAMS framework.
	 * (e.g. statistics).
	 */
	void		*sq_private;
	/*
	 * These two fields are used for scheduling a syncq for
	 * background processing. The sq_svcflag is protected by
	 * SQLOCK lock.
	 */
	struct syncq	*sq_next;	/* for syncq scheduling */
	uint_t		sq_svcflags;	/* Scheduling flags	*/
	/*
	 * Maximum priority of the queues on this syncq.
	 */
	pri_t		sq_pri;
};
typedef struct syncq syncq_t;

/*
 * sync queue scheduling flags (for sq_svcflags).
 */
#define	SQ_SERVICE	0x1		/* syncq is scheduled for service */
#define	SQ_INSERVICE	0x2		/* syncq is being serviced */
#define	SQ_DISABLED	0x4		/* don't put syncq in service list */

/*
 * FASTPUT bit in sd_count/putcount.
 */
#define	SQ_FASTPUT	0x8000
#define	SQ_FASTMASK	0x7FFF

/*
 * sync queue state flags
 */
#define	SQ_EXCL		0x0001		/* exclusive access to inner */
					/*	perimeter */
#define	SQ_BLOCKED	0x0002		/* qprocsoff */
#define	SQ_FROZEN	0x0004		/* freezestr */
#define	SQ_WRITER	0x0008		/* qwriter(OUTER) pending or running */
#define	SQ_MESSAGES	0x0010		/* messages on syncq */
#define	SQ_WANTWAKEUP	0x0020		/* do cv_broadcast on sq_wait */
#define	SQ_WANTEXWAKEUP	0x0040		/* do cv_broadcast on sq_exitwait */
#define	SQ_EVENTS	0x0080		/* Events pending */
#define	SQ_QUEUED	(SQ_MESSAGES | SQ_EVENTS)
#define	SQ_FLAGMASK	0x00FF

/*
 * If any of these flags are set it is not possible for a thread to
 * enter a put or service procedure. Instead it must either block
 * or put the message on the syncq.
 */
#define	SQ_GOAWAY	(SQ_EXCL|SQ_BLOCKED|SQ_FROZEN|SQ_WRITER|\
			SQ_QUEUED)
/*
 * If any of these flags are set it not possible to drain the syncq
 */
#define	SQ_STAYAWAY	(SQ_BLOCKED|SQ_FROZEN|SQ_WRITER)

/*
 * Flags to trigger syncq tail processing.
 */
#define	SQ_TAIL		(SQ_QUEUED|SQ_WANTWAKEUP|SQ_WANTEXWAKEUP)

/*
 * Syncq types (stored in sq_type)
 * The SQ_TYPES_IN_FLAGS (ciput) are also stored in sq_flags
 * for performance reasons. Thus these type values have to be in the low
 * 16 bits and not conflict with the sq_flags values above.
 *
 * Notes:
 *  - putnext() and put() assume that the put procedures have the highest
 *    degree of concurrency. Thus if any of the SQ_CI* are set then SQ_CIPUT
 *    has to be set. This restriction can be lifted by adding code to putnext
 *    and put that check that sq_count == 0 like entersq does.
 *  - putnext() and put() does currently not handle !SQ_COPUT
 *  - SQ_CIOC can be implemented (even for D_MTPERMOD) since qprocsoff allows
 *    multiple open/close threads in the same inner perimeter (using
 *    sq_occount to track open/close separate from other entry).
 *  - In order to implement !SQ_COCB outer_enter has to be fixed so that
 *    the callback can be cancelled while cv_waiting in outer_enter.
 *  - If SQ_CISVC needs to be implemented, qprocsoff() needs to wait
 *    for the currently running services to stop (wait for QINSERVICE
 *    to go off). disable_svc called from qprcosoff disables only
 *    services that will be run in future.
 *
 * All the SQ_CO flags are set when there is no outer perimeter.
 */
#define	SQ_CIPUT	0x0100		/* Concurrent inner put proc */
#define	SQ_CISVC	0x0200		/* Concurrent inner svc proc */
#define	SQ_CIOC		0x0400		/* Concurrent inner open/close */
#define	SQ_CICB		0x0800		/* Concurrent inner callback */
#define	SQ_COPUT	0x1000		/* Concurrent outer put proc */
#define	SQ_COSVC	0x2000		/* Concurrent outer svc proc */
#define	SQ_COOC		0x4000		/* Concurrent outer open/close */
#define	SQ_COCB		0x8000		/* Concurrent outer callback */

/* Types also kept in sq_flags for performance */
#define	SQ_TYPES_IN_FLAGS	(SQ_CIPUT)

#define	SQ_CI		(SQ_CIPUT|SQ_CISVC|SQ_CIOC|SQ_CICB)
#define	SQ_CO		(SQ_COPUT|SQ_COSVC|SQ_COOC|SQ_COCB)
#define	SQ_TYPEMASK	(SQ_CI|SQ_CO)

/*
 * Flag combinations passed to entersq and leavesq to specify the type
 * of entry point.
 */
#define	SQ_PUT		(SQ_CIPUT|SQ_COPUT)
#define	SQ_SVC		(SQ_CISVC|SQ_COSVC)
#define	SQ_OPENCLOSE	(SQ_CIOC|SQ_COOC)
#define	SQ_CALLBACK	(SQ_CICB|SQ_COCB)

/*
 * Asynchronous callback qun*** flag.
 * The mechanism these flags are used in is one where callbacks enter
 * the perimeter thanks to framework support. To use this mechanism
 * the q* and qun* flavors of the callback routines must be used.
 * e.g. qtimeout and quntimeout. The synchronization provided by the flags
 * avoids deadlocks between blocking qun* routines and the perimeter
 * lock.
 */
#define	SQ_CALLB_BYPASSED	0x01		/* bypassed callback fn */

/*
 * Cancel callback mask.
 * The mask expands as the number of cancelable callback types grows
 * Note - separate callback flag because different callbacks have
 * overlapping id space.
 */
#define	SQ_CALLB_CANCEL_MASK	(SQ_CANCEL_TOUT|SQ_CANCEL_BUFCALL)

#define	SQ_CANCEL_TOUT		0x02		/* cancel timeout request */
#define	SQ_CANCEL_BUFCALL	0x04		/* cancel bufcall request */

typedef struct callbparams {
	syncq_t		*cbp_sq;
	void		(*cbp_func)(void *);
	void		*cbp_arg;
	callbparams_id_t cbp_id;
	uint_t		cbp_flags;
	struct callbparams *cbp_next;
} callbparams_t;

typedef struct strbufcall {
	void		(*bc_func)(void *);
	void		*bc_arg;
	size_t		bc_size;
	bufcall_id_t	bc_id;
	struct strbufcall *bc_next;
	kthread_id_t	bc_executor;
} strbufcall_t;

/*
 * Structure of list of processes to be sent SIGPOLL/SIGURG signal
 * on request.  The valid S_* events are defined in stropts.h.
 */
typedef struct strsig {
	struct pid	*ss_pidp;	/* pid/pgrp pointer */
	pid_t		ss_pid;		/* positive pid, negative pgrp */
	int		ss_events;	/* S_* events */
	struct strsig	*ss_next;
} strsig_t;

/*
 * Since all of these events are fairly rare, we allocate them all
 * from a single cache to save memory.
 */
typedef struct strevent {
	union {
		callbparams_t	c;
		strbufcall_t	b;
		strsig_t	s;
	} un;
} strevent_t;

/*
 * bufcall list
 */
struct bclist {
	strbufcall_t	*bc_head;
	strbufcall_t	*bc_tail;
};

/*
 * Structure used to track mux links and unlinks.
 */
struct mux_node {
	major_t		 mn_imaj;	/* internal major device number */
	uint16_t	 mn_indegree;	/* number of incoming edges */
	struct mux_node *mn_originp;	/* where we came from during search */
	struct mux_edge *mn_startp;	/* where search left off in mn_outp */
	struct mux_edge *mn_outp;	/* list of outgoing edges */
	uint_t		 mn_flags;	/* see below */
};

/*
 * Flags for mux_nodes.
 */
#define	VISITED	1

/*
 * Edge structure - a list of these is hung off the
 * mux_node to represent the outgoing edges.
 */
struct mux_edge {
	struct mux_node	*me_nodep;	/* edge leads to this node */
	struct mux_edge	*me_nextp;	/* next edge */
	int		 me_muxid;	/* id of link */
};

/*
 * Queue info
 *
 * The syncq is included here to reduce memory fragmentation
 * for kernel memory allocators that only allocate in sizes that are
 * powers of two. If the kernel memory allocator changes this should
 * be revisited.
 */
typedef struct queinfo {
	struct queue	qu_rqueue;	/* read queue - must be first */
	struct queue	qu_wqueue;	/* write queue - must be second */
	struct syncq	qu_syncq;	/* syncq - must be third */
} queinfo_t;

/*
 * Multiplexed streams info
 */
typedef struct linkinfo {
	struct linkblk	li_lblk;	/* must be first */
	struct file	*li_fpdown;	/* file pointer for lower stream */
	struct linkinfo	*li_next;	/* next in list */
	struct linkinfo *li_prev;	/* previous in list */
} linkinfo_t;

/*
 * List of syncq's used by freeezestr/unfreezestr
 */
typedef struct syncql {
	struct syncql	*sql_next;
	syncq_t		*sql_sq;
} syncql_t;

typedef struct sqlist {
	syncql_t	*sqlist_head;
	size_t		sqlist_size;		/* structure size in bytes */
	size_t		sqlist_index;		/* next free entry in array */
	syncql_t	sqlist_array[4];	/* 4 or more entries */
} sqlist_t;

/* Per-device and per-module structure */
typedef struct perdm {
	syncq_t			*dm_sq;
	struct streamtab	*dm_str;
} perdm_t;

/*
 * Miscellaneous parameters and flags.
 */

/*
 * Finding related queues
 */
#define	STREAM(q)	((q)->q_stream)
#define	SQ(rq)		((syncq_t *)((rq) + 2))

/*
 * Locking macros
 */
#define	QLOCK(q)	(&(q)->q_lock)
#define	SQLOCK(sq)	(&(sq)->sq_lock)

#define	STREAM_PUTLOCKS_ENTER(stp) {					       \
		ASSERT(MUTEX_HELD(&(stp)->sd_lock));			       \
		if ((stp)->sd_ciputctrl != NULL) {			       \
			int i;						       \
			int nlocks = (stp)->sd_nciputctrl;		       \
			ciputctrl_t *cip = (stp)->sd_ciputctrl;		       \
			for (i = 0; i <= nlocks; i++) {			       \
				mutex_enter(&cip[i].ciputctrl_lock);	       \
			}						       \
		}							       \
	}

#define	STREAM_PUTLOCKS_EXIT(stp) {					       \
		ASSERT(MUTEX_HELD(&(stp)->sd_lock));			       \
		if ((stp)->sd_ciputctrl != NULL) {			       \
			int i;						       \
			int nlocks = (stp)->sd_nciputctrl;		       \
			ciputctrl_t *cip = (stp)->sd_ciputctrl;		       \
			for (i = 0; i <= nlocks; i++) {			       \
				mutex_exit(&cip[i].ciputctrl_lock);	       \
			}						       \
		}							       \
	}

#define	SQ_PUTLOCKS_ENTER(sq) {						       \
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				       \
		if ((sq)->sq_ciputctrl != NULL) {			       \
			int i;						       \
			int nlocks = (sq)->sq_nciputctrl;		       \
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		       \
			ASSERT((sq)->sq_type & SQ_CIPUT);		       \
			for (i = 0; i <= nlocks; i++) {			       \
				mutex_enter(&cip[i].ciputctrl_lock);	       \
			}						       \
		}							       \
	}

#define	SQ_PUTLOCKS_EXIT(sq) {						       \
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				       \
		if ((sq)->sq_ciputctrl != NULL) {			       \
			int i;						       \
			int nlocks = (sq)->sq_nciputctrl;		       \
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		       \
			ASSERT((sq)->sq_type & SQ_CIPUT);		       \
			for (i = 0; i <= nlocks; i++) {			       \
				mutex_exit(&cip[i].ciputctrl_lock);	       \
			}						       \
		}							       \
	}

#define	SQ_PUTCOUNT_SETFAST(sq) {					\
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				\
		if ((sq)->sq_ciputctrl != NULL) {			\
			int i;						\
			int nlocks = (sq)->sq_nciputctrl;		\
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		\
			ASSERT((sq)->sq_type & SQ_CIPUT);		\
			for (i = 0; i <= nlocks; i++) {			\
				mutex_enter(&cip[i].ciputctrl_lock);	\
				cip[i].ciputctrl_count |= SQ_FASTPUT;	\
				mutex_exit(&cip[i].ciputctrl_lock);	\
			}						\
		}							\
	}

#define	SQ_PUTCOUNT_CLRFAST(sq) {					\
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				\
		if ((sq)->sq_ciputctrl != NULL) {			\
			int i;						\
			int nlocks = (sq)->sq_nciputctrl;		\
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		\
			ASSERT((sq)->sq_type & SQ_CIPUT);		\
			for (i = 0; i <= nlocks; i++) {			\
				mutex_enter(&cip[i].ciputctrl_lock);	\
				cip[i].ciputctrl_count &= ~SQ_FASTPUT;	\
				mutex_exit(&cip[i].ciputctrl_lock);	\
			}						\
		}							\
	}


#ifdef	DEBUG

#define	SQ_PUTLOCKS_HELD(sq) {						       \
		ASSERT(MUTEX_HELD(SQLOCK(sq)));				       \
		if ((sq)->sq_ciputctrl != NULL) {			       \
			int i;						       \
			int nlocks = (sq)->sq_nciputctrl;		       \
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		       \
			ASSERT((sq)->sq_type & SQ_CIPUT);		       \
			for (i = 0; i <= nlocks; i++) {			       \
				ASSERT(MUTEX_HELD(&cip[i].ciputctrl_lock));    \
			}						       \
		}							       \
	}

#define	SUMCHECK_SQ_PUTCOUNTS(sq, countcheck) {				       \
		if ((sq)->sq_ciputctrl != NULL) {			       \
			int i;						       \
			uint_t count = 0;				       \
			int ncounts = (sq)->sq_nciputctrl;		       \
			ASSERT((sq)->sq_type & SQ_CIPUT);		       \
			for (i = 0; i <= ncounts; i++) {		       \
				count +=				       \
				    (((sq)->sq_ciputctrl[i].ciputctrl_count) & \
				    SQ_FASTMASK);			       \
			}						       \
			ASSERT(count == (countcheck));			       \
		}							       \
	}

#define	SUMCHECK_CIPUTCTRL_COUNTS(ciput, nciput, countcheck) {		       \
		int i;							       \
		uint_t count = 0;					       \
		ASSERT((ciput) != NULL);				       \
		for (i = 0; i <= (nciput); i++) {			       \
			count += (((ciput)[i].ciputctrl_count) &	       \
			    SQ_FASTMASK);				       \
		}							       \
		ASSERT(count == (countcheck));				       \
	}

#else	/* DEBUG */

#define	SQ_PUTLOCKS_HELD(sq)
#define	SUMCHECK_SQ_PUTCOUNTS(sq, countcheck)
#define	SUMCHECK_CIPUTCTRL_COUNTS(sq, nciput, countcheck)

#endif	/* DEBUG */

#define	SUM_SQ_PUTCOUNTS(sq, count) {					       \
		if ((sq)->sq_ciputctrl != NULL) {			       \
			int i;						       \
			int ncounts = (sq)->sq_nciputctrl;		       \
			ciputctrl_t *cip = (sq)->sq_ciputctrl;		       \
			ASSERT((sq)->sq_type & SQ_CIPUT);		       \
			for (i = 0; i <= ncounts; i++) {		       \
				(count) += ((cip[i].ciputctrl_count) &	       \
				    SQ_FASTMASK);			       \
			}						       \
		}							       \
	}

#define	CLAIM_QNEXT_LOCK(stp)	mutex_enter(&(stp)->sd_lock)
#define	RELEASE_QNEXT_LOCK(stp)	mutex_exit(&(stp)->sd_lock)

/*
 * syncq message manipulation macros.
 */
/*
 * Put a message on the queue syncq.
 * Assumes QLOCK held.
 */
#define	SQPUT_MP(qp, mp)						\
	{								\
		qp->q_syncqmsgs++;					\
		if (qp->q_sqhead == NULL) {				\
			qp->q_sqhead = qp->q_sqtail = mp;		\
		} else {						\
			qp->q_sqtail->b_next = mp;			\
			qp->q_sqtail = mp;				\
		}							\
	}


/*
 * Default timeout in milliseconds for ioctls and close
 */
#define	STRTIMOUT 15000

/*
 * Flag values for stream io
 */
#define	WRITEWAIT	0x1	/* waiting for write event */
#define	READWAIT	0x2	/* waiting for read event */
#define	NOINTR		0x4	/* error is not to be set for signal */
#define	GETWAIT		0x8	/* waiting for getmsg event */

/*
 * These flags need to be unique for stream io name space
 * and copy modes name space.  These flags allow strwaitq
 * and strdoioctl to proceed as if signals or errors on the stream
 * head have not occurred; i.e. they will be detected by some other
 * means.
 * STR_NOSIG does not allow signals to interrupt the call
 * STR_NOERROR does not allow stream head read, write or hup errors to
 * affect the call.  When used with strdoioctl(), if a previous ioctl
 * is pending and times out, STR_NOERROR will cause strdoioctl() to not
 * return ETIME. If, however, the requested ioctl times out, ETIME
 * will be returned (use ic_timout instead)
 * STR_PEEK is used to inform strwaitq that the reader is peeking at data
 * and that a non-persistent error should not be cleared.
 * STR_DELAYERR is used to inform strwaitq that it should not check errors
 * after being awoken since, in addition to an error, there might also be
 * data queued on the stream head read queue.
 */
#define	STR_NOSIG	0x10	/* Ignore signals during strdoioctl/strwaitq */
#define	STR_NOERROR	0x20	/* Ignore errors during strdoioctl/strwaitq */
#define	STR_PEEK	0x40	/* Peeking behavior on non-persistent errors */
#define	STR_DELAYERR	0x80	/* Do not check errors on return */

/*
 * Copy modes for tty and I_STR ioctls
 */
#define	U_TO_K 	01			/* User to Kernel */
#define	K_TO_K  02			/* Kernel to Kernel */

/*
 * canonical structure definitions
 */

#define	STRLINK		"lli"
#define	STRIOCTL	"iiil"
#define	STRPEEK		"iiliill"
#define	STRFDINSERT	"iiliillii"
#define	O_STRRECVFD	"lssc8"
#define	STRRECVFD	"lllc8"
#define	STRNAME		"c0"
#define	STRINT		"i"
#define	STRTERMIO	"ssssc12"
#define	STRTERMCB	"c6"
#define	STRSGTTYB	"c4i"
#define	STRTERMIOS	"llllc20"
#define	STRLIST		"il"
#define	STRSEV		"issllc1"
#define	STRGEV		"ili"
#define	STREVENT	"lssllliil"
#define	STRLONG		"l"
#define	STRBANDINFO	"ci"

#define	STRPIDT		"l"

#define	STRMODCONF	"il"

/*
 * Tables we reference during open(2) processing.
 */
#define	CDEVSW	0
#define	FMODSW	1

/*
 * Mux defines.
 */
#define	LINKNORMAL	0x01		/* normal mux link */
#define	LINKPERSIST	0x02		/* persistent mux link */
#define	LINKTYPEMASK	0x03		/* bitmask of all link types */
#define	LINKCLOSE	0x04		/* unlink from strclose */
#define	LINKIOCTL	0x08		/* unlink from strioctl */
#define	LINKNOEDGE	0x10		/* no edge to remove from graph */

/*
 * Definitions of Streams macros and function interfaces.
 */

/*
 *  Queue scheduling macros
 */
#define	setqsched()	qrunflag = 1	/* set up queue scheduler */
#define	qready()	qrunflag	/* test if queues are ready to run */

/*
 * Macros dealing with mux_nodes.
 */
#define	MUX_VISIT(X)	((X)->mn_flags |= VISITED)
#define	MUX_CLEAR(X)	((X)->mn_flags &= (~VISITED)); \
			((X)->mn_originp = NULL)
#define	MUX_DIDVISIT(X)	((X)->mn_flags & VISITED)


/*
 * Twisted stream macros
 */
#define	STRMATED(X)	((X)->sd_flag & STRMATE)
#define	SETMATED(X, Y)  ((X)->sd_flag |= STRMATE); \
			((X)->sd_mate = (Y)); \
			((Y)->sd_flag |= STRMATE); \
			((Y)->sd_mate = (X))
#define	SETUNMATED(X, Y) STRLOCKMATES(X); \
			((X)->sd_flag &= ~STRMATE); \
			((Y)->sd_flag &= ~STRMATE); \
			((X)->sd_mate = NULL); \
			((Y)->sd_mate = NULL); \
			mutex_exit(&((X)->sd_lock)); \
			mutex_exit(&((Y)->sd_lock));
#define	STRLOCKMATES(X)	if (&((X)->sd_lock) > &(((X)->sd_mate)->sd_lock)) { \
				mutex_enter(&((X)->sd_lock)); \
				mutex_enter(&(((X)->sd_mate)->sd_lock));  \
			} else {  \
				mutex_enter(&(((X)->sd_mate)->sd_lock)); \
				mutex_enter(&((X)->sd_lock)); \
			}
#define	STRUNLOCKMATES(X)	mutex_exit(&((X)->sd_lock)); \
			mutex_exit(&(((X)->sd_mate)->sd_lock))

/*
 * Defines/Types for zero-copy
 */

#define	KMAP_PAGES	37	/* # of slots in kernel mapping cache */
#define	KMAP_SIZE (KMAP_PAGES << PAGESHIFT)
#define	MAX_MAPIN_PAGES	8	/* # of pages struiomapin() supports per call */

typedef struct kc_slot {
	frtn_t	ks_frtn;	/* call back function */
	uchar_t	ks_flag;	/* Is the slot taken? Protected by kc_lock */
	uchar_t	ks_index;	/* its index in kc_slot array */
	ushort_t ks_npages;	/* # of pages this slot covers */
	struct kmap_cache *ks_kcp;	/* point to the kcp it belongs */
	caddr_t	ks_uaddr;	/* user addr double-mapped by this slot */
	struct page *ks_pp;	/* user page mapped by this slot */
	struct anon *ks_ap;	/* corresponding anon */
} kc_slot_t;

typedef struct kmap_cache {
	struct as	*kc_as;		/* current address space */
	caddr_t		kc_base;	/* starting kernel addr */
	kmutex_t	kc_lock;	/* protecting kc_refcnt and ks_flag */
	int16_t		kc_refcnt;	/* # of active refereces to slots */
	uint16_t	kc_state;	/* any dirty slot? */
	uint16_t	kc_usecnt;	/* how many times kc has been used */
	uint16_t	kc_cowfault;	/* # of cow faults encountered */
	kc_slot_t	kc_slot[KMAP_PAGES];
} kmap_cache_t;

/* flag defines for ks_flag in kc_slot_t */
#define	KS_FREE		0	/* cache slot is free */
#define	KS_BUSY		1	/* cache slot in use */
#define	KS_RESTORE_PROT	2	/* slot freed, but protection needs restored */

/* flag defines for kc_state in kmap_cache_t */
#define	KC_CLEAN	0	/* No user protection needs to be restored */
#define	KC_RESTORE_PROT	1	/* Some slot's protection needs to restore */

/*
 * Statistics for zero-copy
 * Not protected by locks
 */
struct zero_copy_kstat {
	kstat_named_t	zc_mapins;
	kstat_named_t	zc_pageflips;
	kstat_named_t	zc_misses;
	kstat_named_t	zc_kmapcaches;
	kstat_named_t	zc_cowfaults;
	kstat_named_t	zc_disabled;
	kstat_named_t	zc_busyslots;
#ifdef ZC_TEST
	kstat_named_t	zc_count;
	kstat_named_t	zc_hrtime;
	kstat_named_t	zc_hwcksum_w;
	kstat_named_t	zc_swcksum_w;
	kstat_named_t	zc_hwcksum_r;
	kstat_named_t	zc_swcksum_r;
	kstat_named_t	zc_slowpath_r;
#endif
};

#ifdef ZC_TEST
/*
 * The following declarations are only for zero-copy testing. Don't try
 * them otherwise. Note that zero-copy is only supported on limited platforms.
 */
extern int zcdebug, zcperf, zcslice;
extern int syncstream;

#define	ZC_DEBUG_ALL	0x1
#define	ZC_DEBUG	0x2
#define	ZC_WARN		0x4
#define	ZC_ERROR	0x8
#endif

#ifdef _KERNEL

extern void strinit(void);
extern int strdoioctl(struct stdata *, struct strioctl *, mblk_t *,
	int, int, char *, cred_t *, int *);
extern void strsendsig(struct strsig *, int, uchar_t, int);
extern void str_sendsig(vnode_t *, int, uchar_t, int);
extern void strhup(struct stdata *);
extern int qattach(queue_t *, dev_t *, int, int, int, int, cred_t *, boolean_t);
extern int qreopen(queue_t *, dev_t *, int, cred_t *);
extern void qdetach(queue_t *, int, int, cred_t *, boolean_t);
extern void enterq(queue_t *);
extern void leaveq(queue_t *);
extern int putiocd(mblk_t *, mblk_t *, caddr_t, int, char *);
extern int getiocd(mblk_t *, caddr_t, int, char *);
extern struct linkinfo *alloclink(queue_t *, queue_t *, struct file *);
extern void lbfree(struct linkinfo *);
extern int linkcycle(stdata_t *, stdata_t *);
extern struct linkinfo *findlinks(stdata_t *, int, int);
extern queue_t *getendq(queue_t *);
extern int mlink(vnode_t *, int, int, cred_t *, int *);
extern int munlink(struct stdata *, struct linkinfo *, int, cred_t *, int *);
extern int munlinkall(struct stdata *, int, cred_t *, int *);
extern void mux_addedge(stdata_t *, stdata_t *, int);
extern void mux_rmvedge(stdata_t *, int);
extern int devflg_to_qflag(struct streamtab *, uint32_t, uint32_t *,
	uint32_t *);
extern void setq(queue_t *, struct qinit *, struct qinit *, struct streamtab *,
	perdm_t *, uint32_t, uint32_t, int);
extern int strmakectl(struct strbuf *, int32_t, int32_t, mblk_t **);
extern int strmakedata(ssize_t *, struct uio *, stdata_t *, int32_t,
	mblk_t **);
extern int strmakemsg(struct strbuf *, ssize_t *, struct uio *,
	struct stdata *, int32_t, mblk_t **);
extern int strgetmsg(vnode_t *, struct strbuf *, struct strbuf *, uchar_t *,
	int *, int, rval_t *);
extern int strputmsg(vnode_t *, struct strbuf *, struct strbuf *, uchar_t,
	int flag, int fmode);
extern int strsyncplumb(struct stdata *, int, int);
extern struct streamtab	*fifo_getinfo(void);
extern int stropen(struct vnode *, dev_t *, int, int, cred_t *);
extern int strclose(struct vnode *, int, cred_t *);
extern int strpoll(register struct stdata *, short, int, short *,
	struct pollhead **);
extern void strclean(struct vnode *);
extern void str_cn_clean();	/* XXX hook for consoles signal cleanup */
extern int strwrite(struct vnode *, struct uio *, cred_t *);
extern int strread(struct vnode *, struct uio *, cred_t *);
extern int strioctl(struct vnode *, int, intptr_t, int, int, cred_t *, int *);
extern int strrput(queue_t *, mblk_t *);
extern int strrput_nondata(queue_t *, mblk_t *);
extern mblk_t *strrput_proto(vnode_t *, mblk_t *,
	strwakeup_t *, strsigset_t *, strsigset_t *, strpollset_t *);
extern mblk_t *strrput_misc(vnode_t *, mblk_t *,
	strwakeup_t *, strsigset_t *, strsigset_t *, strpollset_t *);
extern int getiocseqno(void);
extern int strwaitbuf(size_t, int);
extern int strwaitq(stdata_t *, int, ssize_t, int, clock_t, int *);
extern void strctty(struct stdata *);
extern void stralloctty(sess_t *, struct stdata *);
extern void strfreectty(struct stdata *);
extern struct stdata *shalloc(queue_t *);
extern void shfree(struct stdata *s);
extern queue_t *allocq(void);
extern void freeq(queue_t *);
extern qband_t *allocband(void);
extern void freeband(qband_t *);
extern void queuerun(void);
extern void runqueues(void);
extern void freebs_enqueue(mblk_t *, dblk_t *);
extern int findmod(char *);
extern void adjfmtp(char **, mblk_t *, size_t);
extern int str2num(char **);
extern void setqback(queue_t *, unsigned char);
extern int strcopyin(void *, void *, size_t, char *, int);
extern int strcopyout(void *, void *, size_t, char *, int);
extern void strsignal(struct stdata *, int, int32_t);
extern clock_t str_cv_wait(kcondvar_t *, kmutex_t *, clock_t, int);
extern void runbuffcalls(void);
extern int rmv_qp(queue_t **, queue_t **qtail, queue_t *qp);
extern void disable_svc(queue_t *);
extern void remove_runlist(queue_t *);
extern void wait_svc(queue_t *);
extern void freemsg_flush(mblk_t *);
extern void backenable(queue_t *, int);
extern void set_qend(queue_t *);
extern void set_qnexthot(queue_t *);
extern int strgeterr(stdata_t *, int32_t, int);
extern void qenable_locked(queue_t *);
extern mblk_t *getq_noenab(queue_t *);
extern void rmvq_noenab(queue_t *, mblk_t *);
extern void qbackenable(queue_t *, int);

extern void strblock(queue_t *);
extern void strunblock(queue_t *);
extern int qclaimed(queue_t *);
extern int straccess(struct stdata *, enum jcaccess);
extern int findmodbyindex(int);
extern int findmodbyname(char *);

extern void entersq(syncq_t *, int entrypoint);
extern void leavesq(syncq_t *, int entrypoint);
extern void claimq(queue_t *, boolean_t);
extern void releaseq(queue_t *, boolean_t);
extern void claimstr(queue_t *);
extern void releasestr(queue_t *);
extern void removeq(queue_t *);
extern void insertq(struct stdata *, queue_t *);
extern void fill_syncq(syncq_t *, queue_t *, mblk_t *, void (*fun)());
extern void drain_syncq(syncq_t *);
extern void qfill_syncq(syncq_t *, queue_t *, mblk_t *);
extern void qdrain_syncq(syncq_t *, queue_t *);
extern int flush_syncq(syncq_t *, queue_t *);

extern void outer_enter(syncq_t *outer, uint16_t flags);
extern void outer_exit(syncq_t *outer);
extern void qwriter_inner(queue_t *q, mblk_t *mp, void (*func)());
extern void qwriter_outer(queue_t *q, mblk_t *mp, void (*func)());

extern callbparams_t *callbparams_alloc(syncq_t *sq, void (*fun)(void *),
					void *arg);
extern void callbparams_free(syncq_t *sq, callbparams_t *params);
extern void callbparams_free_id(syncq_t *sq, callbparams_id_t id, int32_t flag);
extern void qcallbwrapper(void *);

extern mblk_t *desballoc(unsigned char *, size_t, uint_t, frtn_t *);
extern mblk_t *esballoca(unsigned char *, size_t, uint_t, frtn_t *);
extern mblk_t *desballoca(unsigned char *, size_t, uint_t, frtn_t *);
extern int do_sendfp(struct stdata *, struct file *, struct cred *);
extern int qprocsareon(queue_t *);
extern int frozenstr(queue_t *);
extern size_t xmsgsize(mblk_t *);

extern void putnext_tail(syncq_t *, queue_t *, uint32_t);

extern int str_mate(queue_t *, queue_t *);
extern queue_t *strvp2wq(vnode_t *);
extern vnode_t *strq2vp(queue_t *);
extern mblk_t *allocb_wait(size_t, uint_t, uint_t, int *);
void strpollwakeup(vnode_t *, short);
extern int putnextctl_wait(queue_t *, int);
int prn_putnextctl_wait(queue_t *, int, mblk_t *(*)());
int prn_putctl_wait(queue_t *, int, mblk_t *(*)());
extern void kmap_cache_free(kmap_cache_t *);

int kstrputmsg(struct vnode *, mblk_t *, struct uio *, ssize_t,
		unsigned char, int, int);
int kstrgetmsg(struct vnode *, mblk_t **, struct uio *,
		unsigned char *, int *, clock_t, rval_t *);

void strsetrerror(vnode_t *, int, int, errfunc_t);
void strsetwerror(vnode_t *, int, int, errfunc_t);
void strseteof(vnode_t *, int);
void strflushrq(vnode_t *, int);
void strsetrputhooks(vnode_t *, uint_t, msgfunc_t, msgfunc_t);
void strsetwputhooks(vnode_t *, uint_t, clock_t);
int strwaitmark(vnode_t *);
int strsetpgrp(struct vnode *, pid_t);
pid_t strgetpgrp(struct vnode *);
extern void strpunlink(cred_t *);
extern void strsignal_nolock(stdata_t *, int, int32_t);
extern void 	mp_strinit(void);
extern int 	mp_dr_strinit(void);


/*
 * shared or externally configured data structures
 */
extern ssize_t strmsgsz;		/* maximum stream message size */
extern ssize_t strctlsz;		/* maximum size of ctl message */
extern int nstrpush;			/* maximum number of pushes allowed */
extern int strzc_on;			/* allow/prohibit zero-copy */
extern uint_t strzc_minblk;		/* min blk before enabling zero-copy */
extern uint_t strzc_write_threshold;	/* min write size before enabling zc */
extern uint_t strzc_cow_check_period;	/* freq. of checking for cow faults */
extern uint_t strzc_cowfault_allowed;	/* max cow faults allowed before */
					/* turning off zero-copy */

extern queue_t	*qhead;		/* head of runnable services list */
extern queue_t	*qtail;		/* tail of runnable services list */
extern kmutex_t	service_queue;	/* protects qhead and qtail */

extern struct bclist strbcalls;
extern char strbcflag;
extern char qrunflag;
extern kcondvar_t bcall_cv;
extern int run_queues;
extern kcondvar_t services_to_run;
extern frtn_t frnop;
#ifdef TRACE
extern int enqueued;
#endif /* TRACE */

extern struct kmem_cache *strsig_cache;
extern struct kmem_cache *bufcall_cache;
extern struct kmem_cache *callbparams_cache;
extern struct kmem_cache *ciputctrl_cache;
extern int n_ciputctrl;
extern int max_n_ciputctrl;
extern int min_n_ciputctrl;

extern perdm_t *permod_syncq;
extern perdm_t *perdev_syncq;
extern struct zero_copy_kstat *zckstat;

#endif	/* _KERNEL */

/*
 * Note: Use of these macros are restricted to kernel/unix and
 * intended for the STREAMS framework.
 * All modules/drivers should include sys/ddi.h.
 *
 * Finding related queues
 */
#define		_OTHERQ(q)	((q)->q_flag&QREADR? (q)+1: (q)-1)
#define		_WR(q)		((q)->q_flag&QREADR? (q)+1: (q))
#define		_RD(q)		((q)->q_flag&QREADR? (q): (q)-1)
#define		_SAMESTR(q)	(!((q)->q_flag & QEND))

/*
 * These are also declared here for modules/drivers that eroneously
 * include strsubr.h after ddi.h or fail to include ddi.h at all.
 */
extern struct queue *OTHERQ(queue_t *); /* stream.h */
extern struct queue *RD(queue_t *);
extern struct queue *WR(queue_t *);
extern int SAMESTR(queue_t *);

#ifdef	__cplusplus
}
#endif


#endif	/* _SYS_STRSUBR_H */
