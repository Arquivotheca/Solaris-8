/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_BBH_H
#define	_SYS_DKTP_BBH_H

#pragma ident	"@(#)bbh.h	1.4	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	bbh_cookie {
	lldaddr_t	_ck_sector;	/* sector # on device (union) 	*/
	long		ck_seclen;	/* number of contiguous sec	*/
};
#define	ck_lsector	_ck_sector._f
#define	ck_sector	_ck_sector._p._l
typedef	struct  bbh_cookie * bbh_cookie_t;

struct	bbh_handle {
	int	h_totck;
	int	h_idx;
	struct	bbh_cookie *h_cktab;
};

struct	bbh_obj {
	opaque_t		bbh_data;
	struct bbh_objops	*bbh_ops;
};

struct	bbh_objops {
	int		(*bbh_init)();
	int		(*bbh_free)();
	opaque_t 	(*bbh_gethandle)();
	bbh_cookie_t	(*bbh_htoc)();
	void 		(*bbh_freehandle)();
	int		bbh_resv[2];
};

#define	BBH_GETCK_SECTOR(X, ckp) ((ckp)->ck_sector)
#define	BBH_GETCK_SECLEN(X, ckp) ((ckp)->ck_seclen)

#define	BBH_INIT(X) (*((struct bbh_obj *)(X))->bbh_ops->bbh_init)\
	(((struct bbh_obj *)(X))->bbh_data)
#define	BBH_FREE(X) (*((struct bbh_obj *)(X))->bbh_ops->bbh_free) ((X))
#define	BBH_GETHANDLE(X, bp) (*((struct bbh_obj *)(X))->bbh_ops->bbh_gethandle)\
	(((struct bbh_obj *)(X))->bbh_data, (bp))
#define	BBH_HTOC(X, handle) (*((struct bbh_obj *)(X))->bbh_ops->bbh_htoc) \
	(((struct bbh_obj *)(X))->bbh_data, (handle))
#define	BBH_FREEHANDLE(X, handle) \
	(*((struct bbh_obj *)(X))->bbh_ops->bbh_freehandle) \
	(((struct bbh_obj *)(X))->bbh_data, (handle))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_BBH_H */
