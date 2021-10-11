/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_TGDK_H
#define	_SYS_DKTP_TGDK_H

#pragma ident	"@(#)tgdk.h	1.13	96/09/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	tgdk_ext {
	unsigned	tg_rmb	: 1;
	unsigned	tg_rdonly  : 1;
	unsigned	tg_flag    : 6;
	char		*tg_nodetype;
	char		tg_ctype;
};

struct	tgdk_obj {
	opaque_t		tg_data;
	struct tgdk_objops	*tg_ops;
	struct tgdk_ext		*tg_ext;
	struct tgdk_ext		tg_extblk;	/* extended blk defined	*/
						/* for easy of alloc	*/
};

struct	tgdk_iob {
	struct	buf *b_bp;
	daddr_t	b_lblk;
	long	b_xfer;
	daddr_t	b_psec;
	long	b_pbytecnt;
	short	b_pbyteoff;
	short	b_flag;
};
typedef struct tgdk_iob * tgdk_iob_handle;
#define	IOB_BPALLOC	0x0001
#define	IOB_BPBUFALLOC	0x0002

struct	tgdk_geom {
	long	g_cyl;
	long	g_acyl;
	long	g_head;
	long	g_sec;
	long	g_secsiz;
	long	g_cap;
};

struct	tgdk_objops {
	int	(*tg_init)();
	int	(*tg_free)();
	int	(*tg_probe)();
	int 	(*tg_attach)();
	int	(*tg_open)();
	int	(*tg_close)();
	int	(*tg_ioctl)();
	int	(*tg_strategy)();
	int	(*tg_setgeom)();
	int	(*tg_getgeom)();
	tgdk_iob_handle	(*tg_iob_alloc)();
	int	(*tg_iob_free)();
	caddr_t	(*tg_iob_htoc)();
	caddr_t	(*tg_iob_xfer)();
	int	(*tg_dump)();
	int	(*tg_getphygeom)();
	int	(*tg_set_bbhobj)();
	int	(*tg_check_media)();
	int	(*tg_inquiry)();
	int	tg_resv[2];
};

#define	TGDK_GETNODETYPE(X) (((struct tgdk_obj *)(X))->tg_ext->tg_nodetype)
#define	TGDK_SETNODETYPE(X, Y) \
	(((struct tgdk_obj *)(X))->tg_ext->tg_nodetype = (char *)(Y))
#define	TGDK_RMB(X) 	(((struct tgdk_obj *)(X))->tg_ext->tg_rmb)
#define	TGDK_RDONLY(X) 	(((struct tgdk_obj *)(X))->tg_ext->tg_rdonly)
#define	TGDK_GETCTYPE(X) (((struct tgdk_obj *)(X))->tg_ext->tg_ctype)


#define	TGDK_INIT(X, devp, flcobjp, queobjp, bbhobjp, lkarg) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_init) \
		(((struct tgdk_obj *)(X))->tg_data, (devp), (flcobjp), \
		(queobjp), (bbhobjp), (lkarg))
#define	TGDK_INIT_X(X, devp, flcobjp, queobjp, bbhobjp, lkarg, cbfunc, cbarg) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_init) \
		(((struct tgdk_obj *)(X))->tg_data, (devp), (flcobjp), \
		(queobjp), (bbhobjp), (lkarg), (cbfunc), (cbarg))
#define	TGDK_FREE(X) (*((struct tgdk_obj *)(X))->tg_ops->tg_free) ((X))
#define	TGDK_PROBE(X, WAIT) (*((struct tgdk_obj *)(X))->tg_ops->tg_probe) \
	(((struct tgdk_obj *)(X))->tg_data, (WAIT))
#define	TGDK_ATTACH(X) (*((struct tgdk_obj *)(X))->tg_ops->tg_attach) \
	(((struct tgdk_obj *)(X))->tg_data)
#define	TGDK_OPEN(X, flag) (*((struct tgdk_obj *)(X))->tg_ops->tg_open) \
	(((struct tgdk_obj *)(X))->tg_data, (flag))
#define	TGDK_CLOSE(X) (*((struct tgdk_obj *)(X))->tg_ops->tg_close) \
	(((struct tgdk_obj *)(X))->tg_data)
#define	TGDK_IOCTL(X, dev, cmd, arg, flag, cred_p, rval_p) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_ioctl) \
	(((struct tgdk_obj *)(X))->tg_data, (dev), (cmd), (arg), (flag), \
		(cred_p), (rval_p))
#define	TGDK_STRATEGY(X, bp) (*((struct tgdk_obj *)(X))->tg_ops->tg_strategy) \
	(((struct tgdk_obj *)(X))->tg_data, (bp))
#define	TGDK_GETGEOM(X, datap) (*((struct tgdk_obj *)(X))->tg_ops->tg_getgeom) \
	(((struct tgdk_obj *)(X))->tg_data, (datap))
#define	TGDK_SETGEOM(X, datap) (*((struct tgdk_obj *)(X))->tg_ops->tg_setgeom) \
	(((struct tgdk_obj *)(X))->tg_data, (datap))
#define	TGDK_IOB_ALLOC(X, logblk, xfer, sleep) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_iob_alloc) \
	(((struct tgdk_obj *)(X))->tg_data, (logblk), (xfer), (sleep))
#define	TGDK_IOB_FREE(X, datap) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_iob_free) \
	(((struct tgdk_obj *)(X))->tg_data, (datap))
#define	TGDK_IOB_HTOC(X, handle) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_iob_htoc) \
	(((struct tgdk_obj *)(X))->tg_data, (handle))
#define	TGDK_IOB_RD(X, handle) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_iob_xfer) \
	(((struct tgdk_obj *)(X))->tg_data, (handle), B_READ)
#define	TGDK_IOB_WR(X, handle) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_iob_xfer) \
	(((struct tgdk_obj *)(X))->tg_data, (handle), B_WRITE)
#define	TGDK_DUMP(X, bp) (*((struct tgdk_obj *)(X))->tg_ops->tg_dump) \
	(((struct tgdk_obj *)(X))->tg_data, (bp))
#define	TGDK_GETPHYGEOM(X, datap) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_getphygeom) \
	(((struct tgdk_obj *)(X))->tg_data, (datap))
#define	TGDK_SET_BBHOBJ(X, objp) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_set_bbhobj) \
	(((struct tgdk_obj *)(X))->tg_data, (objp))
#define	TGDK_CHECK_MEDIA(X, state) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_check_media) \
	(((struct tgdk_obj *)(X))->tg_data, (state))
#define	TGDK_INQUIRY(X, inqpp) \
	(*((struct tgdk_obj *)(X))->tg_ops->tg_inquiry) \
	(((struct tgdk_obj *)(X))->tg_data, (inqpp))

#define	LBLK2SEC(BLK, SHF) (daddr_t) ((BLK) >> (SHF))

#define	SETBPERR(bp, err)	\
	(bp)->b_error = (bp)->b_oerror = (err), (bp)->b_flags |= B_ERROR

#define	DK_MAXRECSIZE	(256<<10)	/* maximum io record size 	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_TGDK_H */
