/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_BEAN_H
#define	_BEAN_H

#pragma ident	"@(#)bean.h	1.1	99/08/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/time.h>

#define	bean_tdelta_t_sz 12
typedef hrtime_t bean_tdelta_t [bean_tdelta_t_sz][2];

typedef hrtime_t bean_ts_t;

#ifdef	BEANS

#define	bean_tstamp() gethrtime()

#define	bean_tdeltats(td, tv) {						\
	bean_ts_t _td = td;						\
									\
	td = bean_tstamp();						\
	_td = td - _td;							\
	_bean_tdelta(tv);						\
}

#define	bean_tdelta(td, tv) {						\
	bean_ts_t _td = bean_tstamp() - td;				\
									\
	_bean_tdelta(tv);						\
}

#define	bean_tdis(td, tv) {						\
	bean_ts_t _td = td;						\
									\
	_bean_tdelta(tv);						\
}

#define	_bean_tdelta(tv) {						\
	int _ix;							\
									\
	if (_td) {							\
		if (_td < 10)						\
			_ix = 0;					\
		else if (_td < 100)					\
			_ix = 1;					\
		else if (_td < 1000)					\
			_ix = 2;					\
		else if (_td < 10000)					\
			_ix = 3;					\
		else if (_td < 100000)					\
			_ix = 4;					\
		else if (_td < 1000000)					\
			_ix = 5;					\
		else if (_td < 10000000)				\
			_ix = 6;					\
		else if (_td < 100000000)				\
			_ix = 7;					\
		else if (_td < 1000000000)				\
			_ix = 8;					\
		else if (_td < 10000000000)				\
			_ix = 9;					\
		else if (_td < 100000000000)				\
			_ix = 10;					\
		else							\
			_ix = 11;					\
		tv[_ix][0]++;						\
		tv[_ix][1] += _td;					\
	}								\
}

#define	bean_printdelta(what, tv) {					\
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

#define	bean_zerotdelta(tv) {						\
	int _ix;							\
	for (_ix = 0; _ix < tdelta_t_sz; _ix++) {			\
		tv[_ix][0] = 0ll;					\
		tv[_ix][1] = 0ll;					\
	}								\
}

#define	bean_cdelta_t_sz 12
typedef long long bean_cdelta_t [bean_cdelta_t_sz][2];

typedef long long bean_count_t;

#define	bean_cdelta(n, v) {						\
	int _ix;							\
									\
	if (n) {							\
		if (n < 10ll)						\
			_ix = 0;					\
		else if (n < 100ll)					\
			_ix = 1;					\
		else if (n < 1000ll)					\
			_ix = 2;					\
		else if (n < 10000ll)					\
			_ix = 3;					\
		else if (n < 100000ll)					\
			_ix = 4;					\
		else if (n < 1000000ll)					\
			_ix = 5;					\
		else if (n < 10000000ll)				\
			_ix = 6;					\
		else if (n < 100000000ll)				\
			_ix = 7;					\
		else if (n < 1000000000ll)				\
			_ix = 8;					\
		else if (n < 10000000000ll)				\
			_ix = 9;					\
		else if (n < 100000000000ll)				\
			_ix = 10;					\
		else							\
			_ix = 11;					\
		v[_ix][0]++;						\
		v[_ix][1] += n;						\
	}								\
}

#define	bean_printcdelta(what, v) {					\
	int _ix;							\
	long long _c = 10;						\
	long long _toe = 0;						\
	long long _toc = 0;						\
	long long _e, _el;						\
	char *_t = "< NNNNNNNNNNNN";					\
									\
	for (_ix = 0; _ix < beans_t_sz; _ix++) {			\
		_toe += v[_ix][0];					\
		_toc += v[_ix][1];					\
	}								\
	printf("%s: %lld events for %lld count\n", what, _toe, _toc);	\
	for (_ix = 0; _toe != 0ll && _ix < beans_t_sz; _ix++) {		\
		if ((_ix + 1) == beans_t_sz) {				\
			*_t = '>';					\
		} else {						\
			sprintf(_t, "< %12lld", _c);			\
		}							\
		_e = ((v[_ix][0] * 10000ll / _toe) + 5ll) / 10ll;	\
		_el = _e / 10ll;					\
		printf("%s: %lld (%lld.%lld%%) of", _t,			\
		    v[_ix][0], _el, _e - _el * 10ll);			\
		if (v[_ix][0] != 0)					\
			_e = (v[_ix][1] * 10ll) / v[_ix][0];		\
		else							\
			_e = 0;						\
		_el = _e / 10;						\
		printf(" %lld.%1lld", _el, _e - _el * 10ll);		\
		_e = ((v[_ix][1] * 10000ll / _toc) + 5ll) / 10ll;	\
		_el = _e / 10ll;					\
		printf(" (%lld.%lld%%)\n", _el, _e - _el * 10ll);	\
		_c *= 10ll;						\
	}								\
}

#define	bean_zerocdetla(v) {						\
	int _ix;							\
	for (_ix = 0; _ix < beans_t_sz; _ix++) {			\
		v[_ix][0] = 0ll;					\
		v[_ix][1] = 0ll;					\
	}								\
}

#define	bean_tsinitb(mp) mp->b_datap->db_struioflag = 0; \
		(*((ts_t *)&mp->b_datap->db_struioun.data))

#define	bean_tspb(mp) (*((ts_t *)&mp->b_datap->db_struioun.data))

#else	/* BEANS */

#define	bean_tstamp() 0
#define	bean_tdeltats(td, tv)
#define	bean_tdelta(td, tv)
#define	bean_tdis(td, tv)
#define	bean_printdelta(what, tv)
#define	bean_zerotdelta(tv)
#define	bean_tsinitb(mp)
#define	bean_tspb(mp)

#endif	/* BEANS */

#ifdef	__cplusplus
}
#endif

#endif	/* _BEAN_H */
