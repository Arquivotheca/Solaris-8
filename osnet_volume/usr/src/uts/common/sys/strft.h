/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STRFT_H
#define	_SYS_STRFT_H

#pragma ident	"@(#)strft.h	1.1	99/07/30 SMI"

/*
 * WARNING:
 * Everything in this file is private, belonging to the
 * STREAMS flow trace subsystem.  The only guarantee made
 * about the contents of this file is that if you include
 * it, your code will not port to the next release.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The flow trace subsystem is used to trace the flow of STREAMS messages
 * through a stream. First some time-stamp definitions.
 */

#include <sys/time.h>

#define	tdelta_t_sz 12
typedef hrtime_t tdelta_t [tdelta_t_sz][2];

typedef hrtime_t ts_t;

extern ts_t _tstamp_c;	/* Note: in the mod that calls inittstamp() a	*/
			/* declaration of "ts_t _tstamp_c" must be done. */

#define	inittstamp() _tstamp_c = 0; {					\
}

#define	tstamp() gethrtime()

#define	tdelta(td, tv)							\
	tdelta2(td, tstamp(), tv)

#define	tdeltats(td, tv) {						\
	ts_t _td = td;							\
									\
	td = tstamp();							\
	tdelta2(_td, td, tv)						\
	td = tstamp();							\
}

#define	tdelta2(td1, td2, tv) {						\
	ts_t _tdelta = td2 - td1 - _tstamp_c;				\
	int _ix;							\
									\
	if (_tdelta > 0) {						\
		if (_tdelta < 10ll)			/* < 10ns */	\
			_ix = 0;					\
		else if (_tdelta < 100ll)		/* < 100ns */	\
			_ix = 1;					\
		else if (_tdelta < 1000ll)		/* < 1us */	\
			_ix = 2;					\
		else if (_tdelta < 10000ll)		/* < 10us */	\
			_ix = 3;					\
		else if (_tdelta < 100000ll)		/* < 100us */	\
			_ix = 4;					\
		else if (_tdelta < 1000000ll)		/* < 1ms */	\
			_ix = 5;					\
		else if (_tdelta < 10000000ll)		/* < 10ms */	\
			_ix = 6;					\
		else if (_tdelta < 100000000ll)		/* < 100ms */	\
			_ix = 7;					\
		else if (_tdelta < 1000000000ll)	/* < 1s */	\
			_ix = 8;					\
		else if (_tdelta < 10000000000ll)	/* < 10s */	\
			_ix = 9;					\
		else if (_tdelta < 100000000000ll)	/* < 100s */	\
			_ix = 10;					\
		else					/* >= 100s */	\
			_ix = 11;					\
		tv[_ix][0]++;						\
		tv[_ix][1] += _tdelta;					\
	} else								\
		tv[0][0]++;						\
}

#define	printdelta(what, tv) {						\
	int _ix;							\
	long long _ns = 10;						\
	long long _toc = 0ll;						\
	long long _tot = 0ll;						\
	long long _n, _nl;						\
	char *_t = "< N.NNNNNNNNN";					\
									\
	for (_ix = 0; _ix < tdelta_t_sz; _ix++) {			\
		_toc += tv[_ix][0];					\
		_tot += tv[_ix][1];					\
	}								\
	printf("%s: %lld events for %lldns", what, _toc, _tot);		\
	if (_toc != 0)							\
		_n = _tot / _toc;					\
	else								\
		_n = 0ll;						\
	_nl = _n / 1000000000ll;					\
	printf(" (%lld.%09llds)\n", _nl, _n - _nl * 1000000000ll);	\
	for (_ix = 0; _tot != 0ll && _ix < tdelta_t_sz; _ix++) {	\
		if ((_ix + 1) == tdelta_t_sz) {				\
			*_t = '>';					\
		} else if (_ix < 8) {					\
			sprintf(_t, "< 0.%09llds", _ns);		\
		} else {						\
			sprintf(_t, "< %lld.%.*ss", _ns / 1000000000ll, \
			    9 - (_ix - 8), "000000000");		\
		}							\
		_n = ((tv[_ix][0] * 10000ll / _toc) + 5ll) / 10ll;	\
		_nl = _n / 10ll;					\
		printf("%s: %lld (%ll2d.%lld%%) of", _t,		\
		    tv[_ix][0], _nl, _n - _nl * 10ll);			\
		if (tv[_ix][0] != 0)					\
			_n = tv[_ix][1] / tv[_ix][0];			\
		else							\
			_n = 0ll;					\
		_nl = _n / 1000000000ll;				\
		printf(" %lld.%09llds", _nl, _n - _nl * 1000000000ll);	\
		_n = ((tv[_ix][1] * 10000ll / _tot) + 5ll) / 10ll;	\
		_nl = _n / 10ll;					\
		printf(" (%ll2d.%lld%%)\n", _nl, _n - _nl * 10ll);	\
		_ns *= 10;						\
	}								\
}

#define	zerotdelta(tv) {						\
	int _ix;							\
	for (_ix = 0; _ix < tdelta_t_sz; _ix++) {			\
		tv[_ix][0] = 0;						\
		tv[_ix][1] = 0;						\
	}								\
}

#define	addtdelta(tva, tvb) {						\
	int _ix;							\
	for (_ix = 0; _ix < tdelta_t_sz; _ix++) {			\
		tva[_ix][0] += tvb[_ix][0];				\
		tva[_ix][1] += tvb[_ix][1];				\
	}								\
}

#define	cpytdelta(tva, tvb) {						\
	int _ix;							\
	for (_ix = 0; _ix < tdelta_t_sz; _ix++) {			\
		tva[_ix][0] = tvb[_ix][0];				\
		tva[_ix][1] = tvb[_ix][1];				\
	}								\
}

/*
 * Some evnt defines, values 0..N are reserved for internal use,
 * (N+1)..0x1FFF are available for arbitrary module/drvier use,
 * 0x8000 (RD/WR q marker) and 0x4000 (thread cs marker) are or'ed
 * flag bits reserved for internal use.
 */
#define	FTEV_MASK	0x1FFF
#define	FTEV_ISWR	0x8000
#define	FTEV_CS		0x4000
#define	FTEV_PS		0x2000

#define	FTEV_QMASK	0x1F00

#define	FTEV_ALLOCMASK	0x1FF8
#define	FTEV_ALLOCB	0x0000
#define	FTEV_ESBALLOC	0x0001
#define	FTEV_DESBALLOC	0x0002
#define	FTEV_ESBALLOCA	0x0003
#define	FTEV_DESBALLOCA	0x0004
#define	FTEV_ALLOCBIG	0x0005
#define	FTEV_ALLOCBW	0x0006

#define	FTEV_FREEB	0x0008
#define	FTEV_DUPB	0x0009
#define	FTEV_COPYB	0x000A

#define	FTEV_CALLER	0x000F

#define	FTEV_PUT	0x0100
#define	FTEV_FSYNCQ	0x0103
#define	FTEV_DSYNCQ	0x0104
#define	FTEV_PUTQ	0x0105
#define	FTEV_GETQ	0x0106
#define	FTEV_RMVQ	0x0107
#define	FTEV_INSQ	0x0108
#define	FTEV_PUTBQ	0x0109
#define	FTEV_FLUSHQ	0x010A
#define	FTEV_REPLYQ	0x010B
#define	FTEV_PUTNEXT	0x010D
#define	FTEV_RWNEXT	0x010E
#define	FTEV_QWINNER	0x010F
#define	FTEV_GEWRITE	0x0101

#define	FTFLW_HASH(h) (((unsigned)(h))%ftflw_hash_sz)

#define	FTBLK_EVNTS	0x9


/*
 * Data structure that contains the timestamp, module, event and event
 * data (not certain as to it's use yet: RSF).  There is one per
 * event.  Everytime str_ftevent, one of the indexes is filled in
 * with this data.
 */
struct ftevnt {
	ts_t ts;	/* a time-stamp as returned by gethrtime() */
	char *mid;	/* the q->q_qinfo->qi_minfo->mi_idname pointer */
	ushort_t evnt;	/* what event occured (put, srv, freeb, ...) */
	ushort_t data;	/* event data */
};

/*
 * A linked list of ftvents.
 */
struct ftblk {
	struct ftblk *nxt;	/* next ftblk (or NULL if none) */
	int ix;			/* index of next free ev[] */
	struct ftevnt ev[FTBLK_EVNTS];
};

/*
 * The flow trace header (start of event list).  It consists of the
 *	current writable block (tail)
 *	a hash value (for recovering trace information)
 *	The last thread to process an event
 * 	The last cpu to process an event
 *	The start of the list
 * This structure is attached to a dblk, and traces a message through
 * a flow.
 */
struct fthdr {
	struct ftblk *tail;
	unsigned hash;		/* accumalated hash value (sum of mid's) */
	void *thread;
	int cpu_seqid;
	struct ftblk first;
};

/*
 * These two structures are used to capture the events provided by
 * a message.  They are analogous to the previous two structures except
 * that they are intended as a place to coelesce the message data.
 */
struct ftflwe {
	char *mid;
	ushort_t evnt;
	ushort_t data;
	tdelta_t td;
};

struct ftflw {
	struct ftflw *nxt;
	int hash;
	int count;
	struct ftflwe ev[1];	/* dynamic event vector ev[0..(count-1)] */
};

typedef struct ftevnt ftevnt_t;
typedef struct ftblk ftblk_t;
typedef struct fthdr fthdr_t;
typedef struct ftflwe ftflwe_t;
typedef struct ftflw ftflw_t;
typedef struct ftevents ftevents_t;

#ifdef _KERNEL

extern void str_ftevent(fthdr_t *, void *, unsigned short,
			unsigned short, ts_t);
extern int str_ftnever;		/* Don't do flow tracing */
extern int str_ftall;		/* Trace all streams, not selected */
extern int str_ftinherit;	/* Copy's inherit flow trace data */

#define	STR_FTEVENT_MBLK(mp, p, e, d) {					\
	fthdr_t *_hp;							\
									\
	if (str_ftnever == 0 && mp != NULL &&				\
	    ((_hp = mp->b_datap->db_fthdr) != NULL))			\
		str_ftevent(_hp, p, e, d, (ts_t)0);			\
}

#define	STR_TSEVENT_MBLK(mp, p, e, d, t) {				\
	fthdr_t *_hp;							\
									\
	if (str_ftnever == 0 && mp != NULL &&				\
	    ((_hp = (mp)->b_datap->db_fthdr) != NULL))			\
		str_ftevent(_hp, (p), (e), (d), (t));			\
}

/* Can only do the following if there is only one ftblk */
#define	STR_FTCLR_MBLK(mp) {						\
	fthdr_t	*_hp = (mp)->b_datap->db_fthdr;				\
	if (_hp && (_hp->first.nxt == NULL)) {				\
		int	_ix;						\
		ftblk_t	*_fp = _hp->tail;				\
		for (_ix = _fp->ix; _ix >= 0; _ix--) {			\
			_fp->ev[_ix].ts = (ts_t)0;			\
			_fp->ev[_ix].mid = NULL;			\
			_fp->ev[_ix].evnt = 0;				\
			_fp->ev[_ix].data = 0;				\
		}							\
		_hp->thread = curthread;				\
		_hp->cpu_seqid = CPU->cpu_seqid;			\
		_hp->first.ix = 0;					\
		_hp->hash = 0;						\
	}								\
}

#define	STR_FTEVENT_MSG(mp, p, e, d) {					\
	if (str_ftnever == 0) {						\
		mblk_t *_mp;						\
		fthdr_t *_hp;						\
									\
		for (_mp = (mp); _mp; _mp = _mp->b_cont) {		\
			if ((_hp = _mp->b_datap->db_fthdr) != NULL)	\
				str_ftevent(_hp, (p), (e), (d), (ts_t)0);\
		}							\
	}								\
}

#define	STR_TSEVENT_MSG(mp, p, e, d, t) {				\
	if (str_ftnever == 0) {						\
		mblk_t *_mp;						\
		fthdr_t *_hp;						\
									\
		for (_mp = (mp); _mp; _mp = _mp->b_cont) {		\
			if ((_hp = _mp->b_datap->db_fthdr) != NULL)	\
				str_ftevent(_hp, (p), (e), (d), (t));	\
		}							\
	}								\
}

#define	STR_FTALLOC(hpp, p, e, d, f) {					\
	if ((f) == B_TRUE) {						\
		fthdr_t *_hp = *(hpp);					\
									\
		ASSERT(_hp == NULL);					\
		_hp = kmem_cache_alloc(fthdr_cache, KM_NOSLEEP);	\
		if ((*hpp = _hp) != NULL) {				\
			_hp->tail = &_hp->first;			\
			_hp->hash = 0;					\
			_hp->thread = curthread;			\
			_hp->cpu_seqid = CPU->cpu_seqid;		\
			_hp->first.nxt = NULL;				\
			_hp->first.ix = 0;				\
			str_ftevent(_hp, (p), (e), (d), (ts_t)0);	\
		}							\
	}								\
}

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRFT_H */
