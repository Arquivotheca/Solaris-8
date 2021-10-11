/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)logsubr.c	1.9	99/11/24 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/varargs.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/strsun.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/log.h>
#include <sys/spl.h>
#include <sys/syslog.h>
#include <sys/console.h>
#include <sys/debug.h>
#include <sys/utsname.h>

log_t log_log[LOG_MAX];
short log_active;
queue_t *log_consq;
queue_t *log_backlog;
queue_t *log_recent;
queue_t *log_freeq;
queue_t *log_intrq;

#define	LOG_PRISIZE	8	/* max priority size: 7 characters + null */
#define	LOG_FACSIZE	9	/* max priority size: 8 characters + null */

static krwlock_t log_rwlock;
static int log_rwlock_depth;
static int log_seq_no[SL_CONSOLE + 1];
static stdata_t log_fakestr;

static char log_overflow_msg[] = "message overflow on /dev/log minor #%d%s\n";

static char log_pri[LOG_PRIMASK + 1][LOG_PRISIZE] = {
	"emerg",	"alert",	"crit",		"error",
	"warning",	"notice",	"info",		"debug"
};

static char log_fac[LOG_NFACILITIES + 1][LOG_FACSIZE] = {
	"kern",		"user",		"mail",		"daemon",
	"auth",		"syslog",	"lpr",		"news",
	"uucp",		"resv9",	"resv10",	"resv11",
	"resv12",	"resv13",	"resv14",	"cron",
	"local0",	"local1",	"local2",	"local3",
	"local4",	"local5",	"local6",	"local7",
	"unknown"
};

/*
 * Get exclusive access to the logging system; this includes all log_log[]
 * minor devices.  We use an rwlock rather than a mutex because hold times
 * are potentially long, so we don't want to waste cycles in adaptive mutex
 * spin (rwlocks always block when contended).  Note that we explicitly
 * support recursive calls (e.g. printf() calls foo() calls printf()).
 *
 * Clients may use log_enter() / log_exit() to guarantee that a group
 * of messages is treated atomically (i.e. they appear in order and are
 * not interspersed with any other messages), e.g. for multiline printf().
 */
void
log_enter(void)
{
	if (rw_owner(&log_rwlock) != curthread)
		rw_enter(&log_rwlock, RW_WRITER);
	log_rwlock_depth++;
}

void
log_exit(void)
{
	if (--log_rwlock_depth == 0)
		rw_exit(&log_rwlock);
}

void
log_flushq(queue_t *q)
{
	mblk_t *mp;

	while ((mp = getq_noenab(q)) != NULL)
		log_sendmsg(mp);
}

/*
 * Create a minimal queue with just enough fields filled in to support
 * canput(9F), putq(9F), and getq_noenab(9F).  We set QNOENB to ensure
 * that the queue will never be enabled.
 */
static queue_t *
log_makeq(size_t lowat, size_t hiwat, void *ibc)
{
	queue_t *q;

	q = kmem_zalloc(sizeof (queue_t), KM_SLEEP);
	q->q_stream = &log_fakestr;
	q->q_flag = QISDRV | QMTSAFE | QNOENB | QREADR | QUSE;
	q->q_nfsrv = q;
	q->q_lowat = lowat;
	q->q_hiwat = hiwat;
	mutex_init(QLOCK(q), NULL, MUTEX_DRIVER, ibc);

	return (q);
}

void
log_init(void)
{
	/*
	 * Create a backlog queue to consume console messages during periods
	 * when there is no console reader (e.g. before syslogd(1M) starts).
	 */
	log_backlog = log_consq = log_makeq(0, LOG_HIWAT, NULL);

	/*
	 * Create a queue to hold free message of size <= LOG_MSGSIZE.
	 * Calls from high-level interrupt handlers will do a getq_noenab()
	 * from this queue, so its q_lock must be a maximum SPL spin lock.
	 */
	log_freeq = log_makeq(LOG_MINFREE, LOG_MAXFREE, (void *)ipltospl(SPL8));

	/*
	 * Create a queue for messages from high-level interrupt context.
	 * These messages are drained via softcall, or explicitly by panic().
	 */
	log_intrq = log_makeq(0, LOG_HIWAT, (void *)ipltospl(SPL8));

	/*
	 * Create a queue to hold the most recent 8K of console messages.
	 * Useful for debugging.  Required by the "$<msgbuf" adb macro.
	 */
	log_recent = log_makeq(0, LOG_RECENTSIZE, NULL);

	/*
	 * Let the logging begin.
	 */
	log_update(&log_log[LOG_BACKLOG], log_backlog, SL_CONSOLE, log_console);

	/*
	 * Now that logging is enabled, emit the SunOS banner.
	 */
	printf("\rSunOS Release %s Version %s %u-bit\n",
	    utsname.release, utsname.version, NBBY * (uint_t)sizeof (void *));
	printf("Copyright 1983-2000 Sun Microsystems, Inc.  "
		"All rights reserved.\n");
#ifdef DEBUG
	printf("DEBUG enabled\n");
#endif
#ifdef TRACE
	printf("TRACE enabled\n");
#endif
}

/*
 * Move console messages from src to dst.  The time of day isn't known
 * early in boot, so fix up the message timestamps if necessary.
 */
static void
log_conswitch(log_t *src, log_t *dst)
{
	mblk_t *mp;
	mblk_t *hmp = NULL;
	mblk_t *tmp = NULL;
	log_ctl_t *hlc;

	while ((mp = getq_noenab(src->log_q)) != NULL) {
		log_ctl_t *lc = (log_ctl_t *)mp->b_rptr;
		lc->flags |= SL_LOGONLY;
		if (lc->ttime == 0) {
			/*
			 * Look ahead to first early boot message with time.
			 */
			lc->ltime = lbolt;
			if (hmp) {
				tmp->b_next = mp;
				tmp = mp;
			} else
				hmp = tmp = mp;
			continue;
		} else while (hmp) {
			tmp = hmp->b_next;
			hmp->b_next = NULL;
			hlc = (log_ctl_t *)hmp->b_rptr;
			hlc->ttime = lc->ttime - (lbolt - hlc->ltime) / hz;
			(void) putq(dst->log_q, hmp);
			hmp = tmp;
		}
		(void) putq(dst->log_q, mp);
	}
	while (hmp) {
		tmp = hmp->b_next;
		hmp->b_next = NULL;
		hlc = (log_ctl_t *)hmp->b_rptr;
		hlc->ttime = hrestime.tv_sec - (lbolt - hlc->ltime) / hz;
		(void) putq(dst->log_q, hmp);
		hmp = tmp;
	}
	dst->log_overflow = src->log_overflow;
	src->log_flags = 0;
	dst->log_flags = SL_CONSOLE;
	log_consq = dst->log_q;
}

/*
 * Set the fields in the 'target' clone to the specified values.
 * Then, look at all clones to determine which message types are
 * currently active and which clone is the primary console queue.
 * If the primary console queue changes to or from the backlog
 * queue, copy all messages from backlog to primary or vice versa.
 */
void
log_update(log_t *target, queue_t *q, short flags, log_filter_t *filter)
{
	log_t *lp;
	short active = SL_CONSOLE;

	log_enter();

	if (q != NULL)
		target->log_q = q;
	target->log_wanted = filter;
	target->log_flags = flags;
	target->log_overflow = 0;

	for (lp = &log_log[LOG_MAX - 1]; lp >= &log_log[LOG_CLONEMIN]; lp--) {
		if (lp->log_flags & SL_CONSOLE)
			log_consq = lp->log_q;
		active |= lp->log_flags;
	}
	log_active = active;

	if (log_consq == target->log_q) {
		if (flags & SL_CONSOLE)
			log_conswitch(&log_log[LOG_BACKLOG], target);
		else
			log_conswitch(target, &log_log[LOG_BACKLOG]);
	}
	target->log_q = q;

	log_exit();
}

/*ARGSUSED*/
int
log_error(log_t *lp, log_ctl_t *lc)
{
	if ((lc->pri & LOG_FACMASK) == LOG_KERN)
		lc->pri = LOG_KERN | LOG_ERR;
	return (1);
}

int
log_trace(log_t *lp, log_ctl_t *lc)
{
	trace_ids_t *tid = (trace_ids_t *)lp->log_data->b_rptr;
	trace_ids_t *tidend = (trace_ids_t *)lp->log_data->b_wptr;

	for (; tid < tidend; tid++) {
		if (tid->ti_level < lc->level && tid->ti_level >= 0)
			continue;
		if (tid->ti_mid != lc->mid && tid->ti_mid >= 0)
			continue;
		if (tid->ti_sid != lc->sid && tid->ti_sid >= 0)
			continue;
		if ((lc->pri & LOG_FACMASK) == LOG_KERN)
			lc->pri = LOG_KERN | LOG_DEBUG;
		return (1);
	}
	return (0);
}

/*ARGSUSED*/
int
log_console(log_t *lp, log_ctl_t *lc)
{
	if ((lc->pri & LOG_FACMASK) == LOG_KERN) {
		if (lc->flags & SL_FATAL)
			lc->pri = LOG_KERN | LOG_CRIT;
		else if (lc->flags & SL_ERROR)
			lc->pri = LOG_KERN | LOG_ERR;
		else if (lc->flags & SL_WARN)
			lc->pri = LOG_KERN | LOG_WARNING;
		else if (lc->flags & SL_NOTE)
			lc->pri = LOG_KERN | LOG_NOTICE;
		else if (lc->flags & SL_TRACE)
			lc->pri = LOG_KERN | LOG_DEBUG;
		else
			lc->pri = LOG_KERN | LOG_INFO;
	}
	return (1);
}

mblk_t *
log_makemsg(int mid, int sid, int level, int sl, int pri, void *msg,
	size_t size, int on_intr)
{
	mblk_t *mp = NULL;
	mblk_t *mp2;
	log_ctl_t *lc;

	if (size <= LOG_MSGSIZE &&
	    (on_intr || log_freeq->q_count > log_freeq->q_lowat))
		mp = getq_noenab(log_freeq);

	if (mp == NULL) {
		if (on_intr ||
		    (mp = allocb(sizeof (log_ctl_t), BPRI_HI)) == NULL ||
		    (mp2 = allocb(MAX(size, LOG_MSGSIZE), BPRI_HI)) == NULL) {
			freemsg(mp);
			return (NULL);
		}
		DB_TYPE(mp) = M_PROTO;
		mp->b_wptr += sizeof (log_ctl_t);
		mp->b_cont = mp2;
	} else {
		mp2 = mp->b_cont;
		mp2->b_wptr = mp2->b_rptr;
	}

	lc = (log_ctl_t *)mp->b_rptr;
	lc->mid = mid;
	lc->sid = sid;
	lc->level = level;
	lc->flags = sl;
	lc->pri = pri;

	bcopy(msg, mp2->b_wptr, size - 1);
	mp2->b_wptr[size - 1] = '\0';
	mp2->b_wptr += strlen((char *)mp2->b_wptr) + 1;

	return (mp);
}

void
log_freemsg(mblk_t *mp)
{
	mblk_t *mp2 = mp->b_cont;

	ASSERT(MBLKL(mp) == sizeof (log_ctl_t));
	ASSERT(mp2->b_rptr == mp2->b_datap->db_base);

	if ((log_freeq->q_flag & QFULL) == 0 &&
	    MBLKL(mp2) <= LOG_MSGSIZE && MBLKSIZE(mp2) >= LOG_MSGSIZE)
		(void) putq(log_freeq, mp);
	else
		freemsg(mp);
}

void
log_sendmsg(mblk_t *mp)
{
	log_t *lp;
	char *src, *dst;
	mblk_t *mp2 = mp->b_cont;
	log_ctl_t *lc = (log_ctl_t *)mp->b_rptr;
	int flags, fac;
	off_t facility = 0;
	off_t body = 0;

	if ((lc->flags & log_active) == 0) {
		log_freemsg(mp);
		return;
	}

	if (panicstr) {
		/*
		 * Raise the console queue's q_hiwat to ensure that we
		 * capture all panic messages.
		 */
		log_consq->q_hiwat = 2 * LOG_HIWAT;
		log_consq->q_flag &= ~QFULL;
	}

	src = (char *)mp2->b_rptr;
	dst = strstr(src, "FACILITY_AND_PRIORITY] ");
	if (dst != NULL) {
		facility = dst - src;
		body = facility + 23; /* strlen("FACILITY_AND_PRIORITY] ") */
	}

	log_enter();

	lc->ltime = lbolt;
	lc->ttime = hrestime.tv_sec;

	flags = lc->flags & log_active;
	log_seq_no[flags & SL_ERROR]++;
	log_seq_no[flags & SL_TRACE]++;
	log_seq_no[flags & SL_CONSOLE]++;

	for (lp = &log_log[LOG_LOGMIN]; lp < &log_log[LOG_MAX]; lp++) {
		if ((lp->log_flags & flags) && lp->log_wanted(lp, lc)) {
			if (canput(lp->log_q)) {
				lp->log_overflow = 0;
				lc->seq_no = log_seq_no[lp->log_flags];
				if ((mp2 = copymsg(mp)) == NULL)
					break;
				if (facility != 0) {
					src = (char *)mp2->b_cont->b_rptr;
					dst = src + facility;
					fac = (lc->pri & LOG_FACMASK) >> 3;
					dst += snprintf(dst,
					    LOG_FACSIZE + LOG_PRISIZE, "%s.%s",
					    log_fac[MIN(fac, LOG_NFACILITIES)],
					    log_pri[lc->pri & LOG_PRIMASK]);
					src += body - 2; /* copy "] " too */
					while (*src != '\0')
						*dst++ = *src++;
					*dst++ = '\0';
					mp2->b_cont->b_wptr = (uchar_t *)dst;
				}
				(void) putq(lp->log_q, mp2);
			} else if (++lp->log_overflow == 1) {
				if (lp->log_q == log_consq) {
					console_printf(log_overflow_msg,
					    (int)(lp - log_log),
					    " -- is syslogd(1M) running?");
				} else {
					printf(log_overflow_msg,
					    (int)(lp - log_log), "");
				}
			}
		}
	}

	if ((flags & SL_CONSOLE) && (lc->pri & LOG_FACMASK) == LOG_KERN) {
		if ((mp2 == NULL || log_consq == log_backlog || panicstr) &&
		    (lc->flags & SL_LOGONLY) == 0)
			console_printf("%s", (char *)mp->b_cont->b_rptr + body);
		if ((lc->flags & SL_CONSONLY) == 0 &&
		    (mp2 = copymsg(mp)) != NULL) {
			mp2->b_cont->b_rptr += body;
			if (log_recent->q_flag & QFULL)
				freemsg(getq_noenab(log_recent));
			(void) putq(log_recent, mp2);
		}
	}

	log_freemsg(mp);

	log_exit();
}
