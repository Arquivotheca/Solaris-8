/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_FLOWCTRL_H
#define	_SYS_DKTP_FLOWCTRL_H

#pragma ident	"@(#)flowctrl.h	1.5	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	flc_obj {
	opaque_t		flc_data;
	struct flc_objops	*flc_ops;
};

struct	flc_objops {
	int	(*flc_init)();
	int	(*flc_free)();
	int	(*flc_enque)();
	int	(*flc_deque)();
	int	(*flc_start_kstat)();
	int	(*flc_stop_kstat)();
	int	flc_resv[2];
};

#define	FLC_INIT(X, tgcomobjp, queobjp, lkarg) \
	(*((struct flc_obj *)(X))->flc_ops->flc_init) \
	(((struct flc_obj *)(X))->flc_data, (tgcomobjp), (queobjp), (lkarg))
#define	FLC_FREE(X) (*((struct flc_obj *)(X))->flc_ops->flc_free) ((X))
#define	FLC_ENQUE(X, bp) (*((struct flc_obj *)(X))->flc_ops->flc_enque) \
	(((struct flc_obj *)(X))->flc_data, (bp))
#define	FLC_DEQUE(X, bp) (*((struct flc_obj *)(X))->flc_ops->flc_deque) \
	(((struct flc_obj *)(X))->flc_data, (bp))
#define	FLC_START_KSTAT(X, devtype, instance) \
	(*((struct flc_obj *)(X))->flc_ops->flc_start_kstat)\
	(((struct flc_obj *)(X))->flc_data, (devtype), (instance))
#define	FLC_STOP_KSTAT(X) (*((struct flc_obj *)(X))->flc_ops->flc_stop_kstat) \
	(((struct flc_obj *)(X))->flc_data)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_FLOWCTRL_H */
