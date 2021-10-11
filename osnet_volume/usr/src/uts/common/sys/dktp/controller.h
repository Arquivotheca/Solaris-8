/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_CONTROLLER_H
#define	_SYS_DKTP_CONTROLLER_H

#pragma ident	"@(#)controller.h	1.3	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	ctl_ext {
	opaque_t	c_type_cookie;	/* controller info 		*/
	dev_info_t	*c_ctldip;	/* dip to controller driver	*/
	dev_info_t	*c_devdip;	/* dip to target device driver	*/
	int		c_targ;		/* device target number		*/
	int		c_blksz;	/* device unit size (secsz)	*/
};

struct	ctl_obj {
	opaque_t		c_data;
	struct ctl_objops	*c_ops;
	struct ctl_ext		*c_ext;
	struct ctl_ext		c_extblk;	/* extended blk defined	*/
						/* for easy of alloc	*/
};

struct	ctl_objops {
	struct 	cmpkt *(*c_pktalloc)();	/* packet allocation		*/
	void	(*c_pktfree)();		/* packet free			*/
	struct 	cmpkt *(*c_memsetup)();	/* memory setup			*/
	void	(*c_memfree)();		/* free memory			*/
	struct 	cmpkt *(*c_iosetup)();	/* set up io transfer		*/
	int	(*c_transport)();	/* transport packet		*/
	int	(*c_reset)();		/* reset controller		*/
	int	(*c_abort)();		/* abort packet			*/
	int	(*c_getcap)();		/* get capabilities		*/
	int	(*c_setcap)();		/* set capabilities		*/
	int	(*c_ioctl)();		/* io control			*/
	int	c_resv[2];
};

#define	CTL_DIP_CTL(X) (((struct ctl_obj *)(X))->c_ext->c_ctldip)
#define	CTL_DIP_DEV(X) (((struct ctl_obj *)(X))->c_ext->c_devdip)
#define	CTL_GET_TYPE(X) (((struct ctl_obj *)(X))->c_ext->c_type_cookie)
#define	CTL_GET_LKARG(X) (((struct ctl_obj *)(X))->c_ext->c_lkarg)
#define	CTL_GET_TARG(X) (((struct ctl_obj *)(X))->c_ext->c_targ)
#define	CTL_GET_BLKSZ(X) (((struct ctl_obj *)(X))->c_ext->c_blksz)

#define	CTL_PKTALLOC(X, callback, arg) \
	(*((struct ctl_obj *)(X))->c_ops->c_pktalloc) \
	(((struct ctl_obj *)(X))->c_data, (callback), (arg))
#define	CTL_PKTFREE(X, pktp) \
	(*((struct ctl_obj *)(X))->c_ops->c_pktfree) \
	(((struct ctl_obj *)(X))->c_data, (pktp))
#define	CTL_MEMSETUP(X, pktp, bp, callback, arg) \
	(*((struct ctl_obj *)(X))->c_ops->c_memsetup) \
	(((struct ctl_obj *)(X))->c_data, (pktp), (bp), (callback), (arg))
#define	CTL_MEMFREE(X, pktp) (*((struct ctl_obj *)(X))->c_ops->c_memfree) \
	(((struct ctl_obj *)(X))->c_data, (pktp))
#define	CTL_IOSETUP(X, pktp) (*((struct ctl_obj *)(X))->c_ops->c_iosetup) \
	(((struct ctl_obj *)(X))->c_data, (pktp))
#define	CTL_TRANSPORT(X, pktp) (*((struct ctl_obj *)(X))->c_ops->c_transport) \
	(((struct ctl_obj *)(X))->c_data, (pktp))
#define	CTL_ABORT(X, pktp) (*((struct ctl_obj *)(X))->c_ops->c_abort) \
	(((struct ctl_obj *)(X))->c_data, (pktp))
#define	CTL_RESET(X, level) (*((struct ctl_obj *)(X))->c_ops->c_reset) \
	(((struct ctl_obj *)(X))->c_data, (level))
#define	CTL_IOCTL(X, cmd, arg, flag) \
	(*((struct ctl_obj *)(X))->c_ops->c_ioctl) \
	(((struct ctl_obj *)(X))->c_data, (cmd), (arg), (flag))

/*	transport return code						*/
#define	CTL_SEND_SUCCESS	0
#define	CTL_SEND_FAILURE	1
#define	CTL_SEND_BUSY		2

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CONTROLLER_H */
