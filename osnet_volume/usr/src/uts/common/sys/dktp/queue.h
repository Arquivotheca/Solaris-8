/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_QUEUE_H
#define	_SYS_DKTP_QUEUE_H

#pragma ident	"@(#)queue.h	1.4	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	que_obj {
	opaque_t		que_data;
	struct que_objops	*que_ops;
};

struct	que_objops {
	int	(*que_init)();
	int	(*que_free)();
	int	(*que_ins)();
	struct buf *(*que_del)();
	int	que_res[2];
};

#define	QUE_INIT(X, lkarg) (*((struct que_obj *)(X))->que_ops->que_init) \
	(((struct que_obj *)(X))->que_data, (lkarg))
#define	QUE_FREE(X) (*((struct que_obj *)(X))->que_ops->que_free) ((X))
#define	QUE_ADD(X, bp) (*((struct que_obj *)(X))->que_ops->que_ins) \
	(((struct que_obj *)(X))->que_data, (bp))
#define	QUE_DEL(X) (*((struct que_obj *)(X))->que_ops->que_del) \
	(((struct que_obj *)(X))->que_data)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_QUEUE_H */
