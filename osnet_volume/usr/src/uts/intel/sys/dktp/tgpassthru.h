/*
 * Copyright (c) 1992,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_TGPASSTHRU_H
#define	_SYS_DKTP_TGPASSTHRU_H

#pragma ident	"@(#)tgpassthru.h	1.3	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	tgpassthru_obj {
	opaque_t			pt_data;
	struct tgpassthru_objops	*pt_ops;
};

struct	tgpassthru_objops {
	int	(*pt_init)();
	int	(*pt_free)();
	int	(*pt_transport)();
	int	pt_resv[2];
};

#define	TGPASSTHRU_INIT(X) (*((struct tgpassthru_obj *)(X))->pt_ops->pt_init)\
	(((struct tgpassthru_obj *)(X))->pt_data)
#define	TGPASSTHRU_FREE(X) (*((struct tgpassthru_obj *)(X))->pt_ops->pt_free)\
	((X))
#define	TGPASSTHRU_TRANSPORT(X, cmdp, dev, flag) \
	(*((struct tgpassthru_obj *)(X))->pt_ops->pt_transport) \
	(((struct tgpassthru_obj *)(X))->pt_data, (cmdp), (dev), (flag))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_TGPASSTHRU_H */
