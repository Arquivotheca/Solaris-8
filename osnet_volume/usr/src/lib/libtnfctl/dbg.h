/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _DBG_H
#define	_DBG_H

#pragma ident	"@(#)dbg.h	1.5	96/08/20 SMI"

/*
 * Header file for debugging output.  This is compiled in only if the
 * code is compiled with -DDEBUG.  There are two kinds of debugging output.
 * The first is DBG, which is used to print output directly.  The second is
 * debug probes which can be controlled at run time via prex.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#include <stdio.h>

#define	DBG(x)		(x)
#else
#define	DBG(x)
#endif	/* DEBUG */

#ifdef DEBUG
#include <tnf/probe.h>

#define	DBG_TNF_PROBE_0(a, b, c)	\
	TNF_PROBE_0(a, b, c)
#define	DBG_TNF_PROBE_1(a, b, c, t1, n1, v1)	\
	TNF_PROBE_1(a, b, c, t1, n1, v1)
#define	DBG_TNF_PROBE_2(a, b, c, t1, n1, v1, t2, n2, v2)	\
	TNF_PROBE_2(a, b, c, t1, n1, v1, t2, n2, v2)
#define	DBG_TNF_PROBE_3(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3)	\
	TNF_PROBE_3(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3)
/* CSTYLED */
#define	DBG_TNF_PROBE_4(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3, t4, n4, v4)	\
	TNF_PROBE_4(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3, t4, n4, v4)

#else

#define	DBG_TNF_PROBE_0(a, b, c)	\
	((void)0)
#define	DBG_TNF_PROBE_1(a, b, c, t1, n1, v1)	\
	((void)0)
#define	DBG_TNF_PROBE_2(a, b, c, t1, n1, v1, t2, n2, v2)	\
	((void)0)
#define	DBG_TNF_PROBE_3(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3)	\
	((void)0)
/* CSTYLED */
#define	DBG_TNF_PROBE_4(a, b, c, t1, n1, v1, t2, n2, v2, t3, n3, v3, t4, n4, v4)	\
	((void)0)

#endif	/* DEBUG */

#ifdef __cplusplus
}
#endif

#endif	/* _DBG_H */
