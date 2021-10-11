/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#define	_T_GETDATE		1
#define	_T_WCSTOK		2
#define	_T_FP_GET		3
#define	_T_NL_LANINFO		4
#define	_T_GETPASS		5
#define	_T_FGETGRENT		6
#define	_T_PTSNAME		7
#define	_T_L64A			8
#define	_T_GETVFSENT		9
#define	_T_GETMNTENT		10
#define	_T_GETDATE_ERR_ADDR	11
#define	_T_CRYPT		12
#define	_T_GET_EXCEPTIONS	12
#define	_T_GET_DIRECTION	13
#define	_T_GET_PRECISION	14
#define	_T_GET_NAN_WRITTEN	15
#define	_T_GET_NAN_READ		16
#define	_T_GET_INF_WRITTEN	17
#define	_T_GET_INF_READ		18

/*
 * Internal routine from tsdalloc.c
 */

typedef int     __tsd_item;
extern void *_tsdbufalloc(__tsd_item, size_t, size_t);

typedef struct tsdbuf_t {
	__tsd_item	item;
	void		*buf;
	struct tsdbuf_t	*next;
} _tsdbuf_t;
