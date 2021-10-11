/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)streams.c	1.4	99/10/04 SMI"

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_ks.h>

#include <sys/strsubr.h>
#include <sys/stream.h>
#include <sys/modctl.h>

#include "streams.h"

typedef struct str_flags {
	const char *strf_name;
	const char *strf_descr;
} strflags_t;

typedef void (qprint_func)(queue_t *, queue_t *);

static const strflags_t qf[] = {
	{ "QENAB",		"Queue is already enabled to run" },
	{ "QWANTR",		"Someone wants to read Q" },
	{ "QWANTW",		"Someone wants to write Q" },
	{ "QFULL",		"Q is considered full" },
	{ "QREADR",		"This is the reader (first) Q" },
	{ "QUSE",		"This queue in use (allocation)" },
	{ "QNOENB",		"Don't enable Q via putq" },
	{ "0x80",		"unused" },
	{ "QBACK",		"queue has been back-enabled" },
	{ "QHLIST",		"sd_wrq is on \"scanqhead\"" },
	{ "0x400",		"unused" },
	{ "QPAIR",		"per queue-pair syncq" },
	{ "QPERQ",		"per queue-instance syncq" },
	{ "QPERMOD",		"per module syncq" },
	{ "QMTSAFE",		"stream module is MT-safe" },
	{ "QMTOUTPERIM",	"Has outer perimeter" },
	{ "QINSERVICE",		"service routine executing" },
	{ "QWCLOSE",		"will not be enabled" },
	{ "QEND",		"last queue in stream" },
	{ "QWANTWSYNC",		"Streamhead wants to write Q" },
	{ "QSYNCSTR",		"Q supports Synchronous STREAMS" },
	{ "QISDRV",		"the Queue is attached to a driver" },
	{ "QHOT",		"sq_lock and sq_count are split" },
	{ "QNEXTHOT",		"q_next is QHOT" },
	{ "0x1000000",		"unused" },
	{ "0x2000000",		"unused" },
	{ NULL }
};

static const struct str_flags sqf[] = {
	{ "SQ_EXCL",		"Exclusive access to inner oerimeter" },
	{ "SQ_BLOCKED",		"qprocsoff in progress" },
	{ "SQ_FROZEN",		"freezestr in progress" },
	{ "SQ_WRITER",		"qwriter(OUTER) pending or running" },
	{ "SQ_MESSAGES",	"There are messages on syncq" },
	{ "SQ_WANTWAKEUP",	"Someone is waiting for this sq_wait" },
	{ "SQ_WANTEXWAKEUP",	"Someone is waiting for this sq_exwait" },
	{ "SQ_EVENTS",		"There are events on syncq" },
	{ NULL },
};

static const struct str_flags sqt[] = {
	{ "SQ_CIPUT",		"Concurrent inner put procedure" },
	{ "SQ_CISVC",		"Concurrent inner svc procedure" },
	{ "SQ_CIOC",		"Concurrent inner open/close" },
	{ "SQ_CICB",		"Concurrent inner callback" },
	{ "SQ_COPUT",		"Concurrent outer put procedure" },
	{ "SQ_COSVC",		"Concurrent outer svc procedure" },
	{ "SQ_COOC",		"Concurrent outer open/close" },
	{ "SQ_COCB",		"Concurrent outer callback" },
	{ NULL },
};

static int
streams_parse_flag(const strflags_t ftable[], const char *arg, uint32_t *flag)
{
	int i;

	for (i = 0; ftable[i].strf_name != NULL; i++) {
		if (strcasecmp(arg, ftable[i].strf_name) == 0) {
			*flag |= (1 << i);
			return (0);
		}
	}

	return (-1);
}

static void
streams_flag_usage(const strflags_t ftable[])
{
	int i;

	for (i = 0; ftable[i].strf_name != NULL; i++)
		mdb_printf("%12s %s\n",
		    ftable[i].strf_name, ftable[i].strf_descr);
}

int
queue(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const int QUEUE_FLGDELT = (int)(sizeof (uintptr_t) * 2 + 15);

	char name[MODMAXNAMELEN];
	int nblks = 0;
	uintptr_t maddr;
	mblk_t mblk;
	queue_t q;

	const char *mod = NULL, *flag = NULL, *not_flag = NULL;
	uint_t verbose = FALSE;
	uint32_t mask = 0, not_mask = 0;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &verbose,
	    'm', MDB_OPT_STR, &mod,
	    'f', MDB_OPT_STR, &flag,
	    'F', MDB_OPT_STR, &not_flag, NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags) && flag == NULL && not_flag == NULL &&
	    mod == NULL) {
		mdb_printf("%?s %-13s %6s %4s\n",
		    "ADDR", "MODULE", "FLAGS", "NBLK");
	}

	if (flag != NULL && streams_parse_flag(qf, flag, &mask) == -1) {
		mdb_warn("unrecognized queue flag '%s'\n", flag);
		streams_flag_usage(qf);
		return (DCMD_USAGE);
	}

	if (not_flag != NULL &&
	    streams_parse_flag(qf, not_flag, &not_mask) == -1) {
		mdb_warn("unrecognized queue flag '%s'\n", flag);
		streams_flag_usage(qf);
		return (DCMD_USAGE);
	}

	if (mdb_vread(&q, sizeof (q), addr) == -1) {
		mdb_warn("couldn't read queue at %p", addr);
		return (DCMD_ERR);
	}

	for (maddr = (uintptr_t)q.q_first; maddr != NULL; nblks++) {
		if (mdb_vread(&mblk, sizeof (mblk), maddr) == -1) {
			mdb_warn("couldn't read mblk %p for queue %p",
			    maddr, addr);
			break;
		}
		maddr = (uintptr_t)mblk.b_next;
	}

	(void) mdb_qname(&q, name, sizeof (name));

	if (mod != NULL && strcmp(mod, name) != 0)
		return (DCMD_OK);

	if (mask != 0 && !(q.q_flag & mask))
		return (DCMD_OK);

	if (not_mask != 0 && (q.q_flag & not_mask))
		return (DCMD_OK);

	/*
	 * Options are specified for filtering, so If any option is specified on
	 * the command line, just print address and exit.
	 */
	if (flag != NULL || not_flag != NULL || mod != NULL) {
		mdb_printf("%0?p\n", addr);
		return (DCMD_OK);
	}

	mdb_printf("%0?p %-13s %06x %4d %0?p\n",
	    addr, name, q.q_flag, nblks, q.q_first);

	if (verbose) {
		int i, arm = 0;

		for (i = 0; qf[i].strf_name != NULL; i++) {
			if (!(q.q_flag & (1 << i)))
				continue;
			if (!arm) {
				mdb_printf("%*s|\n%*s+-->  ",
				    QUEUE_FLGDELT, "", QUEUE_FLGDELT, "");
				arm = 1;
			} else
				mdb_printf("%*s      ", QUEUE_FLGDELT, "");

			mdb_printf("%-12s %s\n",
			    qf[i].strf_name, qf[i].strf_descr);
		}
	}

	return (DCMD_OK);
}

int
syncq(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const int SYNC_FLGDELT = (int)(sizeof (uintptr_t) * 2 + 1);
	const int SYNC_TYPDELT = (int)(sizeof (uintptr_t) * 2 + 5);
	syncq_t sq;

	const char *flag = NULL, *not_flag = NULL;
	const char *typ = NULL, *not_typ = NULL;
	uint_t verbose = FALSE;
	uint32_t mask = 0, not_mask = 0;
	uint32_t tmask = 0, not_tmask = 0;
	uint8_t sqtype = 0;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &verbose,
	    'f', MDB_OPT_STR, &flag,
	    'F', MDB_OPT_STR, &not_flag,
	    't', MDB_OPT_STR, &typ,
	    'T', MDB_OPT_STR, &not_typ,
	    NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)	&& flag == NULL && not_flag == NULL &&
	    typ == NULL && not_typ == NULL) {
		mdb_printf("%?s %s %s %s %s %?s %s %s\n",
		    "ADDR", "FLG", "TYP", "CNT", "NQS", "OUTER", "SF", "PRI");
	}

	if (flag != NULL && streams_parse_flag(sqf, flag, &mask) == -1) {
		mdb_warn("unrecognized syncq flag '%s'\n", flag);
		streams_flag_usage(sqf);
		return (DCMD_USAGE);
	}

	if (typ != NULL && streams_parse_flag(sqt, typ, &tmask) == -1) {
		mdb_warn("unrecognized syncq type '%s'\n", typ);
		streams_flag_usage(sqf);
		return (DCMD_USAGE);
	}

	if (not_flag != NULL && streams_parse_flag(sqf, not_flag, &not_mask)
	    == -1) {
		mdb_warn("unrecognized syncq flag '%s'\n", not_flag);
		streams_flag_usage(sqf);
		return (DCMD_USAGE);
	}

	if (not_typ != NULL && streams_parse_flag(sqt, not_typ, &not_tmask)
	    == -1) {
		mdb_warn("unrecognized syncq type '%s'\n", not_typ);
		streams_flag_usage(sqt);
		return (DCMD_USAGE);
	}

	if (mdb_vread(&sq, sizeof (sq), addr) == -1) {
		mdb_warn("couldn't read syncq at %p", addr);
		return (DCMD_ERR);
	}

	if (mask != 0 && !(sq.sq_flags & mask))
		return (DCMD_OK);

	if (not_mask != 0 && (sq.sq_flags & not_mask))
		return (DCMD_OK);

	sqtype = (sq.sq_type >> 8) & 0xff;

	if (tmask != 0 && !(sqtype & tmask))
		return (DCMD_OK);

	if (not_tmask != 0 && (sqtype & not_tmask))
		return (DCMD_OK);

	/*
	 * Options are specified for filtering, so If any option is specified on
	 * the command line, just print address and exit.
	 */
	if (flag != NULL || not_flag != NULL || typ != NULL ||
	    not_typ != NULL) {
		mdb_printf("%0?p\n", addr);
		return (DCMD_OK);
	}

	mdb_printf("%0?p %02x  %02x  %-3u %-3u %0?p  %1x %-3d\n",
	    addr, sq.sq_flags & 0xff, sqtype, sq.sq_count,
	    sq.sq_nqueues, sq.sq_outer, sq.sq_svcflags, sq.sq_pri);

	if (verbose) {
		int i, arm = 0;

		for (i = 0; sqf[i].strf_name != NULL; i++) {
			if (!(sq.sq_flags & (1 << i)))
				continue;
			if (!arm) {
				mdb_printf("%*s|\n%*s+-->  ",
				    SYNC_FLGDELT, "", SYNC_FLGDELT, "");
				arm = 1;
			} else
				mdb_printf("%*s      ", SYNC_FLGDELT, "");

			mdb_printf("%-12s %s\n",
			    sqf[i].strf_name, sqf[i].strf_descr);
		}

		for (i = 0; sqt[i].strf_name != NULL; i++) {
			if (!(sqtype & (1 << i)))
				continue;
			if (!arm) {
				mdb_printf("%*s|\n%*s+-->  ",
				    SYNC_TYPDELT, "", SYNC_TYPDELT, "");
				arm = 1;
			} else
				mdb_printf("%*s      ", SYNC_TYPDELT, "");

			mdb_printf("%-12s %s\n",
			    sqt[i].strf_name, sqt[i].strf_descr);
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static void
qprint_syncq(queue_t *addr, queue_t *q)
{
	mdb_printf("%p\n", q->q_syncq);
}

static void
qprint_wrq(queue_t *addr, queue_t *q)
{
	mdb_printf("%p\n", ((q)->q_flag & QREADR? (addr)+1: (addr)));
}

static void
qprint_rdq(queue_t *addr, queue_t *q)
{
	mdb_printf("%p\n", ((q)->q_flag & QREADR? (addr): (addr)-1));
}

static void
qprint_otherq(queue_t *addr, queue_t *q)
{
	mdb_printf("%p\n", ((q)->q_flag & QREADR? (addr)+1: (addr)-1));
}

static int
q2x(uintptr_t addr, int argc, qprint_func prfunc)
{
	queue_t q;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_vread(&q, sizeof (q), addr) == -1) {
		mdb_warn("couldn't read queue at %p", addr);
		return (DCMD_ERR);
	}

	prfunc((queue_t *)addr, &q);

	return (DCMD_OK);
}


/*ARGSUSED*/
int
q2syncq(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (q2x(addr, argc, qprint_syncq));
}

/*ARGSUSED*/
int
q2rdq(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (q2x(addr, argc, qprint_rdq));
}

/*ARGSUSED*/
int
q2wrq(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (q2x(addr, argc, qprint_wrq));
}

/*ARGSUSED*/
int
q2otherq(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (q2x(addr, argc, qprint_otherq));
}

/*
 * If this syncq is a part of the queue pair structure, find the queue for it.
 */
/*ARGSUSED*/
int
syncq2q(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	syncq_t sq;
	queue_t q;
	queue_t *qp;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_vread(&sq, sizeof (sq), addr) == -1) {
		mdb_warn("couldn't read syncq at %p", addr);
		return (DCMD_ERR);
	}

	/* Try to find its queue */
	qp = (queue_t *)addr - 2;

	if ((mdb_vread(&q, sizeof (q), (uintptr_t)qp) == -1) ||
	    (q.q_syncq != (syncq_t *)addr)) {
		mdb_warn("syncq2q: %p is not part of any queue\n", addr);
		return (DCMD_ERR);
	} else
		mdb_printf("%p\n", qp);

	return (DCMD_OK);
}


int
queue_walk_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr == NULL &&
	    mdb_readvar(&wsp->walk_addr, "qhead") == -1) {
		mdb_warn("failed to read 'qhead'");
		return (WALK_ERR);
	}

	wsp->walk_data = mdb_alloc(sizeof (queue_t), UM_SLEEP);
	return (WALK_NEXT);
}

int
queue_link_step(mdb_walk_state_t *wsp)
{
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(wsp->walk_data, sizeof (queue_t), wsp->walk_addr) == -1) {
		mdb_warn("failed to read queue at %p", wsp->walk_addr);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr, wsp->walk_data,
	    wsp->walk_cbdata);

	wsp->walk_addr = (uintptr_t)(((queue_t *)wsp->walk_data)->q_link);
	return (status);
}

int
queue_next_step(mdb_walk_state_t *wsp)
{
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(wsp->walk_data, sizeof (queue_t), wsp->walk_addr) == -1) {
		mdb_warn("failed to read queue at %p", wsp->walk_addr);
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr, wsp->walk_data,
	    wsp->walk_cbdata);

	wsp->walk_addr = (uintptr_t)(((queue_t *)wsp->walk_data)->q_next);
	return (status);
}

void
queue_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (queue_t));
}

int
str_walk_init(mdb_walk_state_t *wsp)
{
	stdata_t s;

	if (wsp->walk_addr == NULL) {
		mdb_warn("walk must begin at address of stdata_t\n");
		return (WALK_ERR);
	}

	if (mdb_vread(&s, sizeof (s), wsp->walk_addr) == -1) {
		mdb_warn("failed to read stdata at %p", wsp->walk_addr);
		return (WALK_ERR);
	}

	wsp->walk_addr = (uintptr_t)s.sd_wrq;
	wsp->walk_data = mdb_alloc(sizeof (queue_t) * 2, UM_SLEEP);

	return (WALK_NEXT);
}

int
strr_walk_step(mdb_walk_state_t *wsp)
{
	queue_t *rq = wsp->walk_data, *wq = rq + 1;
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(wsp->walk_data, sizeof (queue_t) * 2,
	    wsp->walk_addr - sizeof (queue_t)) == -1) {
		mdb_warn("failed to read queue pair at %p",
		    wsp->walk_addr - sizeof (queue_t));
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr - sizeof (queue_t),
	    rq, wsp->walk_cbdata);

	if (wq->q_next != NULL)
		wsp->walk_addr = (uintptr_t)wq->q_next;
	else
		wsp->walk_addr = mdb_qwnext(wq);

	return (status);
}

int
strw_walk_step(mdb_walk_state_t *wsp)
{
	queue_t *rq = wsp->walk_data, *wq = rq + 1;
	int status;

	if (wsp->walk_addr == NULL)
		return (WALK_DONE);

	if (mdb_vread(wsp->walk_data, sizeof (queue_t) * 2,
	    wsp->walk_addr - sizeof (queue_t)) == -1) {
		mdb_warn("failed to read queue pair at %p",
		    wsp->walk_addr - sizeof (queue_t));
		return (WALK_DONE);
	}

	status = wsp->walk_callback(wsp->walk_addr, wq, wsp->walk_cbdata);

	if (wq->q_next != NULL)
		wsp->walk_addr = (uintptr_t)wq->q_next;
	else
		wsp->walk_addr = mdb_qwnext(wq);

	return (status);
}

void
str_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (queue_t) * 2);
}

static int
print_qpair(uintptr_t addr, const queue_t *q, uint_t *depth)
{
	static const char box_lid[] =
	    "+-----------------------+-----------------------+\n";
	static const char box_sep[] =
	    "|                       |                       |\n";

	char wname[32], rname[32], info1[256], *info2;

	if (*depth != 0) {
		mdb_printf("            |                       ^\n");
		mdb_printf("            v                       |\n");
	} else
		mdb_printf("\n");

	(void) mdb_qname(_WR(q), wname, sizeof (wname));
	(void) mdb_qname(_RD(q), rname, sizeof (rname));

	mdb_qinfo(_WR(q), info1, sizeof (info1));
	if ((info2 = strchr(info1, '\n')) != NULL)
		*info2++ = '\0';
	else
		info2 = "";

	mdb_printf(box_lid);
	mdb_printf("| 0x%-19p | 0x%-19p | %s\n",
	    addr, addr - sizeof (queue_t), info1);

	mdb_printf("| %<b>%-21s%</b> | %<b>%-21s%</b> |", wname, rname);
	mdb_flush(); /* Account for buffered terminal sequences */

	mdb_printf(" %s\n", info2);
	mdb_printf(box_sep);

	mdb_qinfo(_RD(q), info1, sizeof (info1));
	if ((info2 = strchr(info1, '\n')) != NULL)
		*info2++ = '\0';
	else
		info2 = "";

	mdb_printf("| cnt = 0t%-13lu | cnt = 0t%-13lu | %s\n",
	    _WR(q)->q_count, _RD(q)->q_count, info1);

	mdb_printf("| flg = 0x%08x      | flg = 0x%08x      | %s\n",
	    _WR(q)->q_flag, _RD(q)->q_flag, info2);

	mdb_printf(box_lid);
	*depth += 1;
	return (0);
}

/*ARGSUSED*/
int
stream(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t d = 0;	/* Depth counter for print_qpair */

	if (argc != 0 || !(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_pwalk("writeq", (mdb_walk_cb_t)print_qpair, &d, addr) == -1) {
		mdb_warn("failed to walk writeq");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}
