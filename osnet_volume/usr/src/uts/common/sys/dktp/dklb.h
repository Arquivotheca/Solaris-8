/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_DKLB_H
#define	_SYS_DKTP_DKLB_H

#pragma ident	"@(#)dklb.h	1.8	99/03/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	dklb_ext {
	ushort_t		lb_numpart;
	ushort_t		lb_flag;
};
#define	DKLB_VALLB		0x0001

struct	dklb_obj {
	opaque_t		lb_data;
	struct dklb_objops	*lb_ops;
	struct dklb_ext		*lb_ext;
	struct dklb_ext		lb_extblk;
};

struct	dklb_objops {
	int	(*lb_init)();
	int	(*lb_free)();
	int	(*lb_open)();
	int	(*lb_ioctl)();
	void	(*lb_partinfo)();
	int	lb_resv[2];
};

#define	DKLB_NUMPART(X) (((struct dklb_obj *)(X))->lb_ext->lb_numpart)
#define	DKLB_VALIDLB(X) (((struct dklb_obj *)(X))->lb_ext->lb_flag & DKLB_VALLB)

#define	DKLB_INIT(X, dkobjp, lkarg) \
	(*((struct dklb_obj *)(X))->lb_ops->lb_init) \
	(((struct dklb_obj *)(X))->lb_data, (dkobjp), (lkarg))
#define	DKLB_FREE(X) (*((struct dklb_obj *)(X))->lb_ops->lb_free) ((X))
#define	DKLB_OPEN(X, dev, dip) (*((struct dklb_obj *)(X))->lb_ops->lb_open) \
	(((struct dklb_obj *)(X))->lb_data, dev, dip)
#define	DKLB_IOCTL(X, cmd, arg, flag, cred_p, rval_p) \
	(*((struct dklb_obj *)(X))->lb_ops->lb_ioctl) \
	(((struct dklb_obj *)(X))->lb_data, (cmd), (arg), (flag), \
		(cred_p), (rval_p))
#define	DKLB_PARTINFO(X, nblk, srtsec, part) \
	(*((struct dklb_obj *)(X))->lb_ops->lb_partinfo)\
	(((struct dklb_obj *)(X))->lb_data, (nblk), (srtsec), (part))

#define	PCFDISK		0

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_DKLB_H */
