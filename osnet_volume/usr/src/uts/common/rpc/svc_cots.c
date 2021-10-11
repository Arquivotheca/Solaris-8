/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)svc_cots.c	1.41	99/08/13 SMI"	/* SVr4.0 1.10 */

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  Copyright (c) 1986-1989, 1994-1996, 1998-1999 by Sun Microsystems, Inc.
 *  Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *  All rights reserved.
 */

/*
 * svc_cots.c
 * Server side for connection-oriented RPC in the kernel.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <sys/tihdr.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/vtrace.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/rpc_msg.h>
#include <rpc/svc.h>

#define	COTS_MAX_ALLOCSIZE	2048
#define	MSG_OFFSET		128	/* offset of call into the mblk */
#define	RM_HDR_SIZE		4	/* record mark header size */

/*
 * Routines exported through ops vector.
 */
static bool_t		svc_cots_krecv(SVCXPRT *, mblk_t *, struct rpc_msg *);
static bool_t		svc_cots_ksend(SVCXPRT *, struct rpc_msg *);
static bool_t		svc_cots_kgetargs(SVCXPRT *, xdrproc_t, caddr_t);
static bool_t		svc_cots_kfreeargs(SVCXPRT *, xdrproc_t, caddr_t);
static void		svc_cots_kdestroy(SVCMASTERXPRT *);
static int		svc_cots_kdup(struct svc_req *, caddr_t, int,
				struct dupreq **);
static void		svc_cots_kdupdone(struct dupreq *, caddr_t, int, int);
static int32_t		*svc_cots_kgetres(SVCXPRT *, int);
static void		svc_cots_kfreeres(SVCXPRT *);
static void		svc_cots_kclone_destroy(SVCXPRT *);
static void		svc_cots_kstart(SVCMASTERXPRT *);

/*
 * Server transport operations vector.
 */
struct svc_ops svc_cots_op = {
	svc_cots_krecv,		/* Get requests */
	svc_cots_kgetargs,	/* Deserialize arguments */
	svc_cots_ksend,		/* Send reply */
	svc_cots_kfreeargs,	/* Free argument data space */
	svc_cots_kdestroy,	/* Destroy transport handle */
	svc_cots_kdup,		/* Check entry in dup req cache */
	svc_cots_kdupdone,	/* Mark entry in dup req cache as done */
	svc_cots_kgetres,	/* Get pointer to response buffer */
	svc_cots_kfreeres,	/* Destroy pre-serialized response header */
	svc_cots_kclone_destroy, /* Destroy a clone xprt */
	svc_cots_kstart		/* Tell `ready-to-receive' to rpcmod */
};

/*
 * Transport private data.
 * Kept in xprt->xp_p2.
 */
typedef struct cots_data {
	mblk_t	*cd_mp;		/* pre-allocated reply message */
	mblk_t	*cd_req_mp;	/* request message */
	char	*cd_src_addr;	/* client's address */
	int	cd_xprt_started; /* flag for clone routine to call */
				/* rpcmod's start routine. */
} cots_data_t;

/*
 * Server statistics
 * NOTE: This structure type is duplicated in the NFS fast path.
 */
struct {
	kstat_named_t	rscalls;
	kstat_named_t	rsbadcalls;
	kstat_named_t	rsnullrecv;
	kstat_named_t	rsbadlen;
	kstat_named_t	rsxdrcall;
	kstat_named_t	rsdupchecks;
	kstat_named_t	rsdupreqs;
} cotsrsstat = {
	{ "calls",	KSTAT_DATA_UINT64 },
	{ "badcalls",	KSTAT_DATA_UINT64 },
	{ "nullrecv",	KSTAT_DATA_UINT64 },
	{ "badlen",	KSTAT_DATA_UINT64 },
	{ "xdrcall",	KSTAT_DATA_UINT64 },
	{ "dupchecks",	KSTAT_DATA_UINT64 },
	{ "dupreqs",	KSTAT_DATA_UINT64 }
};

kstat_named_t *cotsrsstat_ptr = (kstat_named_t *)&cotsrsstat;
uint_t cotsrsstat_ndata = sizeof (cotsrsstat) / sizeof (kstat_named_t);

#define	RSSTAT_INCR(x)	cotsrsstat.x.value.ui64++

/*
 * Pointer to a transport specific `ready to receive' function in rpcmod
 * (set from rpcmod).
 */
void    (*mir_start)(queue_t *);
uint_t	*svc_max_msg_sizep;

/*
 * the address size of the underlying transport can sometimes be
 * unknown (tinfo->ADDR_size == -1).  For this case, it is
 * necessary to figure out what the size is so the correct amount
 * of data is allocated.  This is an itterative process:
 *	1. take a good guess (use T_MINADDRSIZE)
 *	2. try it.
 *	3. if it works then everything is ok
 *	4. if the error is ENAMETOLONG, double the guess
 *	5. go back to step 2.
 */
#define	T_UNKNOWNADDRSIZE	(-1)
#define	T_MINADDRSIZE	32

/*
 * Create a transport record.
 * The transport record, output buffer, and private data structure
 * are allocated.  The output buffer is serialized into using xdrmem.
 * There is one transport record per user process which implements a
 * set of services.
 */
static kmutex_t cots_kcreate_lock;

int
svc_cots_kcreate(file_t *fp, uint_t max_msgsize, struct T_info_ack *tinfo,
    SVCMASTERXPRT **nxprt)
{
	cots_data_t *cd;
	int err;
	int retval;
	SVCMASTERXPRT *xprt;
	int addr_size;

	if (nxprt == NULL)
		return (EINVAL);

	/*
	 * Check to make sure that the cots private data will fit into
	 * the stack buffer allocated by svc_run.  The ASSERT is a safety
	 * net if the cots_data_t structure ever changes.
	 */
	/*CONSTANTCONDITION*/
	ASSERT(sizeof (cots_data_t) <= SVC_P2LEN);

	addr_size = tinfo->ADDR_size;
	if (addr_size == T_UNKNOWNADDRSIZE) {
	    addr_size = T_MINADDRSIZE;
	}

allocate_space:

	xprt = kmem_zalloc(sizeof (SVCMASTERXPRT), KM_SLEEP);
	cd = kmem_zalloc(sizeof (cots_data_t) + addr_size, KM_SLEEP);

	/* cd_src_addr is set to the end of cots_data_t struct */
	cd->cd_src_addr = (char *)&cd[1];

	if ((tinfo->TIDU_size > COTS_MAX_ALLOCSIZE) ||
	    (tinfo->TIDU_size <= 0))
		xprt->xp_msg_size = COTS_MAX_ALLOCSIZE;
	else {
		xprt->xp_msg_size = tinfo->TIDU_size -
			(tinfo->TIDU_size % BYTES_PER_XDR_UNIT);
	}

	xprt->xp_ops = &svc_cots_op;
	xprt->xp_p2 = (caddr_t)cd;
	cd->cd_xprt_started = 0;

	xprt->xp_rtaddr.maxlen = addr_size;
	xprt->xp_rtaddr.len = 0;
	xprt->xp_rtaddr.buf = cd->cd_src_addr;

	/*
	 * Get the address of the client for duplicate request
	 * cache processing. Note that the TI_GETPEERNAME ioctl should
	 * be replaced with a T_ADDR_REQ/T_ADDR_ACK handshake when
	 * TCP supports these standard TPI primitives.
	 */
	retval = 0;
	err = strioctl(fp->f_vnode, TI_GETPEERNAME,
	    (intptr_t)&xprt->xp_rtaddr, 0, K_TO_K, CRED(), &retval);
	if (err) {
		kmem_free(xprt, sizeof (SVCMASTERXPRT));
		kmem_free(cd, sizeof (cots_data_t) + addr_size);
		if ((err == ENAMETOOLONG) &&
			(tinfo->ADDR_size == T_UNKNOWNADDRSIZE)) {
			addr_size *= 2;
			goto allocate_space;
		}
		return (err);
	}

	/*
	 * If the current sanity check size in rpcmod is smaller
	 * than the size needed for this xprt, then increase
	 * the sanity check.
	 */
	if (max_msgsize != 0 && svc_max_msg_sizep &&
	    max_msgsize > *svc_max_msg_sizep) {

		/* This check needs a lock */
		mutex_enter(&cots_kcreate_lock);
		if (svc_max_msg_sizep && max_msgsize > *svc_max_msg_sizep)
			*svc_max_msg_sizep = max_msgsize;
		mutex_exit(&cots_kcreate_lock);
	}

	*nxprt = xprt;
	return (0);
}

/*
 * Destroy a master transport record.
 * Frees the space allocated for a transport record.
 */
static void
svc_cots_kdestroy(SVCMASTERXPRT *xprt)
{
	cots_data_t *cd = (cots_data_t *)xprt->xp_p2;

	ASSERT(cd);

	if (cd->cd_req_mp) {
		freemsg(cd->cd_req_mp);
		cd->cd_req_mp = (mblk_t *)0;
	}
	ASSERT(cd->cd_mp == NULL);

	if (xprt->xp_netid)
		kmem_free(xprt->xp_netid, strlen(xprt->xp_netid) + 1);
	if (xprt->xp_addrmask.maxlen)
		kmem_free(xprt->xp_addrmask.buf, xprt->xp_addrmask.maxlen);

	mutex_destroy(&xprt->xp_req_lock);
	mutex_destroy(&xprt->xp_thread_lock);

	kmem_free(cd, sizeof (cots_data_t) + xprt->xp_rtaddr.maxlen);
	kmem_free(xprt, sizeof (SVCMASTERXPRT));
}

/*
 * svc_tli_kcreate() calls this function at the end to tell
 * rpcmod that the transport is ready to receive requests.
 */
static void
svc_cots_kstart(SVCMASTERXPRT *xprt)
{
	cots_data_t *cd = (cots_data_t *)xprt->xp_p2;
	if (cd->cd_xprt_started == 0) {
		/*
		 * Acquire the xp_req_lock in order to use xp_wq
		 * safely (we don't want to qenable a queue that has
		 * already been closed).
		 */
		mutex_enter(&xprt->xp_req_lock);
		if (cd->cd_xprt_started == 0 &&
			xprt->xp_wq != NULL) {
			(*mir_start)(xprt->xp_wq);
			cd->cd_xprt_started = 1;
		}
		mutex_exit(&xprt->xp_req_lock);
	}
}

/*
 * Transport-type specific part of svc_xprt_cleanup().
 */
static void
svc_cots_kclone_destroy(SVCXPRT *clone_xprt)
{
	cots_data_t *cd = (cots_data_t *)clone_xprt->xp_p2buf;

	if (cd->cd_req_mp) {
		freemsg(cd->cd_req_mp);
		cd->cd_req_mp = (mblk_t *)0;
	}
	ASSERT(cd->cd_mp == NULL);
}

/*
 * Receive rpc requests.
 * Checks if the message is intact, and deserializes the call packet.
 */
static bool_t
svc_cots_krecv(SVCXPRT *clone_xprt, mblk_t *mp, struct rpc_msg *msg)
{
	cots_data_t *cd = (cots_data_t *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrin;
	mblk_t *tmp, *tmp_prev;

	TRACE_0(TR_FAC_KRPC, TR_SVC_COTS_KRECV_START,
	    "svc_cots_krecv_start:");
	RPCLOG(4, "svc_cots_krecv_start clone_xprt = %p:\n",
	    (void *)clone_xprt);

	RSSTAT_INCR(rscalls);

	if (mp->b_datap->db_type != M_DATA) {
		RPCLOG(16, "svc_cots_krecv bad db_type %d\n",
		    mp->b_datap->db_type);
		goto bad;
	}

	/*
	 * Make sure the mblks are aligned correctly.
	 */
	tmp = mp;
	tmp_prev = NULL;
	while (tmp) {
		if (str_aligned(tmp->b_rptr) == 0)
			break;
		/*
		 * The length of each mblk needs to aligned. Since b_rptr
		 * aligned, so must b_wptr. However, don't bother worrying
		 * about extra bytes on the last mblk.
		 */
		if (str_aligned(tmp->b_wptr) == 0 && tmp->b_cont != NULL)
			break;
		/*
		 * Save the previous mblk.
		 */
		tmp_prev = tmp;
		tmp = tmp->b_cont;
	}

	if (tmp) {
		mblk_t *old_mp;

		RPCLOG0(16, "svc_cots_krecv unaligned mblk\n");

		old_mp = tmp;
pull_again:
		tmp = msgpullup(old_mp, -1);
		if (tmp)
			freemsg(old_mp);
		else {
			RPCLOG0(1, "svc_cots_krecv msgpullup failure\n");
			delay(hz);
			goto pull_again;
		}
		if (tmp_prev)
			tmp_prev->b_cont = tmp;
		else
			mp = tmp;
	}

	xdrmblk_init(xdrs, mp, XDR_DECODE, 0);

	TRACE_0(TR_FAC_KRPC, TR_XDR_CALLMSG_START,
	    "xdr_callmsg_start:");
	RPCLOG0(4, "xdr_callmsg_start:\n");
	if (!xdr_callmsg(xdrs, msg)) {
		TRACE_1(TR_FAC_KRPC, TR_XDR_CALLMSG_END,
		    "xdr_callmsg_end:(%S)", "bad");
		RPCLOG0(1, "svc_cots_krecv xdr_callmsg failure\n");
		RSSTAT_INCR(rsxdrcall);
		goto bad;
	}
	TRACE_1(TR_FAC_KRPC, TR_XDR_CALLMSG_END,
	    "xdr_callmsg_end:(%S)", "good");

	clone_xprt->xp_xid = msg->rm_xid;
	cd->cd_req_mp = mp;

	TRACE_1(TR_FAC_KRPC, TR_SVC_COTS_KRECV_END,
	    "svc_cots_krecv_end:(%S)", "good");
	RPCLOG0(4, "svc_cots_krecv_end:good\n");
	return (TRUE);

bad:
	if (mp)
		freemsg(mp);

	RSSTAT_INCR(rsbadcalls);
	TRACE_1(TR_FAC_KRPC, TR_SVC_COTS_KRECV_END,
	    "svc_cots_krecv_end:(%S)", "bad");
	return (FALSE);
}

/*
 * Send rpc reply.
 */
static bool_t
svc_cots_ksend(SVCXPRT *clone_xprt, struct rpc_msg *msg)
{
	/* LINTED pointer alignment */
	cots_data_t *cd = (cots_data_t *)clone_xprt->xp_p2buf;
	XDR *xdrs = &(clone_xprt->xp_xdrout);
	int retval = FALSE;
	mblk_t *mp;
	xdrproc_t xdr_results;
	caddr_t xdr_location;
	bool_t has_args;

	TRACE_0(TR_FAC_KRPC, TR_SVC_COTS_KSEND_START,
	    "svc_cots_ksend_start:");

	/*
	 * If there is a result procedure specified in the reply message,
	 * it will be processed in the xdr_replymsg and SVCAUTH_WRAP.
	 * We need to make sure it won't be processed twice, so we null
	 * it for xdr_replymsg here.
	 */
	has_args = FALSE;
	if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
		if ((xdr_results = msg->acpted_rply.ar_results.proc) != NULL) {
			has_args = TRUE;
			xdr_location = msg->acpted_rply.ar_results.where;
			msg->acpted_rply.ar_results.proc = xdr_void;
			msg->acpted_rply.ar_results.where = NULL;
		}
	}

	mp = cd->cd_mp;
	if (mp) {
		/*
		 * The program above pre-allocated an mblk and put
		 * the data in place.
		 */
		cd->cd_mp = (mblk_t *)NULL;
		if (!(xdr_replymsg_body(xdrs, msg) &&
		    (!has_args || SVCAUTH_WRAP(&clone_xprt->xp_auth, xdrs,
		    xdr_results, xdr_location)))) {
			RPCLOG0(1, "svc_cots_ksend: "
			    "xdr_replymsg_body/SVCAUTH_WRAP failed\n");
			freemsg(mp);
			goto out;
		}
	} else {
		int	len;
		int	mpsize;

		/*
		 * Leave space for protocol headers.
		 */
		len = MSG_OFFSET + clone_xprt->xp_msg_size;

		/*
		 * Allocate an initial mblk for the response data.
		 */
		while (!(mp = allocb(len, BPRI_LO))) {
			RPCLOG0(16, "svc_cots_ksend: allocb failed failed\n");
			if (strwaitbuf(len, BPRI_LO)) {
				TRACE_1(TR_FAC_KRPC, TR_SVC_COTS_KSEND_END,
				    "svc_cots_ksend_end:(%S)", "strwaitbuf");
				RPCLOG0(1,
				    "svc_cots_ksend: strwaitbuf failed\n");
				goto out;
			}
		}

		/*
		 * Initialize the XDR decode stream.  Additional mblks
		 * will be allocated if necessary.  They will be TIDU
		 * sized.
		 */
		xdrmblk_init(xdrs, mp, XDR_ENCODE, clone_xprt->xp_msg_size);
		mpsize = (int)bpsize(mp);
		ASSERT(mpsize >= len);
		ASSERT(mp->b_rptr == mp->b_datap->db_base);

		/*
		 * If the size of mblk is not appreciably larger than what we
		 * asked, then resize the mblk to exactly len bytes. Reason for
		 * this: suppose len is 1600 bytes, the tidu is 1460 bytes
		 * (from TCP over ethernet), and the arguments to RPC require
		 * 2800 bytes. Ideally we want the protocol to render two
		 * ~1400 byte segments over the wire. If allocb() gives us a 2k
		 * mblk, and we allocate a second mblk for the rest, the
		 * protocol module may generate 3 segments over the wire:
		 * 1460 bytes for the first, 448 (2048 - 1600) for the 2nd, and
		 * 892 for the 3rd. If we "waste" 448 bytes in the first mblk,
		 * the XDR encoding will generate two ~1400 byte mblks, and the
		 * protocol module is more likely to produce properly sized
		 * segments.
		 */
		if ((mpsize >> 1) <= len) {
			mp->b_rptr += (mpsize - len);
		}

		/*
		 * Adjust b_rptr to reserve space for the non-data protocol
		 * headers that any downstream modules might like to add, and
		 * for the record marking header.
		 */
		mp->b_rptr += (MSG_OFFSET + RM_HDR_SIZE);

		XDR_SETPOS(xdrs, (uint_t)(mp->b_rptr - mp->b_datap->db_base));
		ASSERT(mp->b_wptr == mp->b_rptr);

		msg->rm_xid = clone_xprt->xp_xid;

		TRACE_0(TR_FAC_KRPC, TR_XDR_REPLYMSG_START,
		    "xdr_replymsg_start:");
		if (!(xdr_replymsg(xdrs, msg) &&
		    (!has_args || SVCAUTH_WRAP(&clone_xprt->xp_auth, xdrs,
		    xdr_results, xdr_location)))) {
			TRACE_1(TR_FAC_KRPC, TR_XDR_REPLYMSG_END,
			    "xdr_replymsg_end:(%S)", "bad");
			freemsg(mp);
			RPCLOG0(1, "svc_cots_ksend: xdr_replymsg/SVCAUTH_WRAP "
				"failed\n");
			goto out;
		}
		TRACE_1(TR_FAC_KRPC, TR_XDR_REPLYMSG_END,
		    "xdr_replymsg_end:(%S)", "good");
	}

	put(clone_xprt->xp_wq, mp);
	retval = TRUE;

out:
	/*
	 * This is completely disgusting.  If public is set it is
	 * a pointer to a structure whose first field is the address
	 * of the function to free that structure and any related
	 * stuff.  (see rrokfree in nfs_xdr.c).
	 */
	if (xdrs->x_public) {
		/* LINTED pointer alignment */
		(**((int (**)())xdrs->x_public))(xdrs->x_public);
	}

	TRACE_1(TR_FAC_KRPC, TR_SVC_COTS_KSEND_END,
	    "svc_cots_ksend_end:(%S)", "done");
	return (retval);
}

/*
 * Deserialize arguments.
 */
static bool_t
svc_cots_kgetargs(SVCXPRT *clone_xprt, xdrproc_t xdr_args,
    caddr_t args_ptr)
{
	return (SVCAUTH_UNWRAP(&clone_xprt->xp_auth, &clone_xprt->xp_xdrin,
	    xdr_args, args_ptr));
}

static bool_t
svc_cots_kfreeargs(SVCXPRT *clone_xprt, xdrproc_t xdr_args,
    caddr_t args_ptr)
{
	cots_data_t *cd = (cots_data_t *)clone_xprt->xp_p2buf;
	mblk_t *mp;
	bool_t retval;

	/*
	 * It is important to call the XDR routine before
	 * freeing the request mblk.  Structures in the
	 * XDR data may point into the mblk and require that
	 * the memory be intact during the free routine.
	 */
	if (args_ptr) {
		/* LINTED pointer alignment */
		XDR	*xdrs = &clone_xprt->xp_xdrin;

		xdrs->x_op = XDR_FREE;
		retval = (*xdr_args)(xdrs, args_ptr);
	} else
		retval = TRUE;

	if ((mp = cd->cd_req_mp) != NULL) {
		cd->cd_req_mp = (mblk_t *)0;
		freemsg(mp);
	}

	return (retval);
}

static int32_t *
svc_cots_kgetres(SVCXPRT *clone_xprt, int size)
{
	/* LINTED pointer alignment */
	cots_data_t *cd = (cots_data_t *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrout;
	mblk_t *mp;
	int32_t *buf;
	struct rpc_msg rply;
	int len;
	int mpsize;

	/*
	 * Leave space for protocol headers.
	 */
	len = MSG_OFFSET + clone_xprt->xp_msg_size;

	/*
	 * Allocate an initial mblk for the response data.
	 */
	while ((mp = allocb(len, BPRI_LO)) == NULL) {
		if (strwaitbuf(len, BPRI_LO))
			return (FALSE);
	}

	/*
	 * Initialize the XDR decode stream.  Additional mblks
	 * will be allocated if necessary.  They will be TIDU
	 * sized.
	 */
	xdrmblk_init(xdrs, mp, XDR_ENCODE, clone_xprt->xp_msg_size);
	mpsize = bpsize(mp);
	ASSERT(mpsize >= len);
	ASSERT(mp->b_rptr == mp->b_datap->db_base);

	/*
	 * If the size of mblk is not appreciably larger than what we
	 * asked, then resize the mblk to exactly len bytes. Reason for
	 * this: suppose len is 1600 bytes, the tidu is 1460 bytes
	 * (from TCP over ethernet), and the arguments to RPC require
	 * 2800 bytes. Ideally we want the protocol to render two
	 * ~1400 byte segments over the wire. If allocb() gives us a 2k
	 * mblk, and we allocate a second mblk for the rest, the
	 * protocol module may generate 3 segments over the wire:
	 * 1460 bytes for the first, 448 (2048 - 1600) for the 2nd, and
	 * 892 for the 3rd. If we "waste" 448 bytes in the first mblk,
	 * the XDR encoding will generate two ~1400 byte mblks, and the
	 * protocol module is more likely to produce properly sized
	 * segments.
	 */
	if ((mpsize >> 1) <= len) {
		mp->b_rptr += (mpsize - len);
	}

	/*
	 * Adjust b_rptr to reserve space for the non-data protocol
	 * headers that any downstream modules might like to add, and
	 * for the record marking header.
	 */
	mp->b_rptr += (MSG_OFFSET + RM_HDR_SIZE);

	XDR_SETPOS(xdrs, (uint_t)(mp->b_rptr - mp->b_datap->db_base));
	ASSERT(mp->b_wptr == mp->b_rptr);

	/*
	 * Assume a successful RPC since most of them are.
	 */
	rply.rm_xid = clone_xprt->xp_xid;
	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = clone_xprt->xp_verf;
	rply.acpted_rply.ar_stat = SUCCESS;

	if (!xdr_replymsg_hdr(xdrs, &rply)) {
		freeb(mp);
		return (NULL);
	}


	buf = XDR_INLINE(xdrs, size);
	if (buf == NULL) {
		ASSERT(cd->cd_mp == NULL);
		freemsg(mp);
	} else {
		cd->cd_mp = mp;
	}
	return (buf);
}

static void
svc_cots_kfreeres(SVCXPRT *clone_xprt)
{
	cots_data_t *cd;
	mblk_t *mp;

	cd = (cots_data_t *)clone_xprt->xp_p2buf;
	if ((mp = cd->cd_mp) != NULL) {
		cd->cd_mp = (mblk_t *)NULL;
		freemsg(mp);
	}
}

/*
 * the dup cacheing routines below provide a cache of non-failure
 * transaction id's.  rpc service routines can use this to detect
 * retransmissions and re-send a non-failure response.
 */

/*
 * MAXDUPREQS is the number of cached items.  It should be adjusted
 * to the service load so that there is likely to be a response entry
 * when the first retransmission comes in.
 */
#define	MAXDUPREQS	1024

/*
 * This should be appropriately scaled to MAXDUPREQS.
 */
#define	DRHASHSZ	257

#if ((DRHASHSZ & (DRHASHSZ - 1)) == 0)
#define	XIDHASH(xid)	((xid) & (DRHASHSZ - 1))
#else
#define	XIDHASH(xid)	((xid) % DRHASHSZ)
#endif
#define	DRHASH(dr)	XIDHASH((dr)->dr_xid)
#define	REQTOXID(req)	((req)->rq_xprt->xp_xid)

static int	cotsndupreqs = 0;
static int	cotsmaxdupreqs = MAXDUPREQS;
static kmutex_t cotsdupreq_lock;
static struct dupreq *cotsdrhashtbl[DRHASHSZ];
static int	cotsdrhashstat[DRHASHSZ];

static void unhash(struct dupreq *);

/*
 * cotsdrmru points to the head of a circular linked list in lru order.
 * cotsdrmru->dr_next == drlru
 */
struct dupreq *cotsdrmru;

/*
 * svc_cots_kdup searches the request cache and returns 0 if the
 * request is not found in the cache.  If it is found, then it
 * returns the state of the request (in progress or done) and
 * the status or attributes that were part of the original reply.
 */
static int
svc_cots_kdup(struct svc_req *req, caddr_t res, int size, struct dupreq **drpp)
{
	struct dupreq *dr;
	uint32_t xid;
	uint32_t drhash;
	int status;

	xid = REQTOXID(req);
	mutex_enter(&cotsdupreq_lock);
	RSSTAT_INCR(rsdupchecks);
	/*
	 * Check to see whether an entry already exists in the cache.
	 */
	dr = cotsdrhashtbl[XIDHASH(xid)];
	while (dr != NULL) {
		if (dr->dr_xid == xid &&
		    dr->dr_proc == req->rq_proc &&
		    dr->dr_prog == req->rq_prog &&
		    dr->dr_vers == req->rq_vers &&
		    dr->dr_addr.len == req->rq_xprt->xp_rtaddr.len &&
		    bcmp((caddr_t)dr->dr_addr.buf,
		    (caddr_t)req->rq_xprt->xp_rtaddr.buf,
		    dr->dr_addr.len) == 0) {
			status = dr->dr_status;
			if (status == DUP_DONE) {
				bcopy(dr->dr_resp.buf, res, size);
				TRACE_0(TR_FAC_KRPC, TR_SVC_COTS_KDUP_DONE,
				    "svc_cots_kdup: DUP_DONE");
			} else {
				dr->dr_status = DUP_INPROGRESS;
				*drpp = dr;
				TRACE_0(TR_FAC_KRPC,
				    TR_SVC_COTS_KDUP_INPROGRESS,
				    "svc_cots_kdup: DUP_INPROGRESS");
			}
			RSSTAT_INCR(rsdupreqs);
			mutex_exit(&cotsdupreq_lock);
			return (status);
		}
		dr = dr->dr_chain;
	}

	/*
	 * There wasn't an entry, either allocate a new one or recycle
	 * an old one.
	 */
	if (cotsndupreqs < cotsmaxdupreqs) {
		dr = kmem_alloc(sizeof (*dr), KM_NOSLEEP);
		if (dr == NULL) {
			mutex_exit(&cotsdupreq_lock);
			return (DUP_ERROR);
		}
		dr->dr_resp.buf = NULL;
		dr->dr_resp.maxlen = 0;
		dr->dr_addr.buf = NULL;
		dr->dr_addr.maxlen = 0;
		if (cotsdrmru) {
			dr->dr_next = cotsdrmru->dr_next;
			cotsdrmru->dr_next = dr;
		} else {
			dr->dr_next = dr;
		}
		cotsndupreqs++;
	} else {
		dr = cotsdrmru->dr_next;
		while (dr->dr_status == DUP_INPROGRESS)
			dr = dr->dr_next;
		unhash(dr);
	}
	cotsdrmru = dr;

	dr->dr_xid = REQTOXID(req);
	dr->dr_prog = req->rq_prog;
	dr->dr_vers = req->rq_vers;
	dr->dr_proc = req->rq_proc;
	if (dr->dr_addr.maxlen < req->rq_xprt->xp_rtaddr.len) {
		if (dr->dr_addr.buf != NULL)
			kmem_free(dr->dr_addr.buf, dr->dr_addr.maxlen);
		dr->dr_addr.maxlen = req->rq_xprt->xp_rtaddr.len;
		dr->dr_addr.buf = kmem_alloc(dr->dr_addr.maxlen, KM_NOSLEEP);
		if (dr->dr_addr.buf == NULL) {
			dr->dr_addr.maxlen = 0;
			dr->dr_status = DUP_DROP;
			mutex_exit(&cotsdupreq_lock);
			return (DUP_ERROR);
		}
	}
	dr->dr_addr.len = req->rq_xprt->xp_rtaddr.len;
	bcopy(req->rq_xprt->xp_rtaddr.buf, dr->dr_addr.buf, dr->dr_addr.len);
	if (dr->dr_resp.maxlen < size) {
		if (dr->dr_resp.buf != NULL)
			kmem_free(dr->dr_resp.buf, dr->dr_resp.maxlen);
		dr->dr_resp.maxlen = (unsigned int)size;
		dr->dr_resp.buf = kmem_alloc(size, KM_NOSLEEP);
		if (dr->dr_resp.buf == NULL) {
			dr->dr_resp.maxlen = 0;
			dr->dr_status = DUP_DROP;
			mutex_exit(&cotsdupreq_lock);
			return (DUP_ERROR);
		}
	}
	dr->dr_status = DUP_INPROGRESS;

	drhash = (uint32_t)DRHASH(dr);
	dr->dr_chain = cotsdrhashtbl[drhash];
	cotsdrhashtbl[drhash] = dr;
	cotsdrhashstat[drhash]++;
	mutex_exit(&cotsdupreq_lock);
	*drpp = dr;
	return (DUP_NEW);
}

/*
 * svc_cots_kdupdone marks the request done (DUP_DONE or DUP_DROP)
 * and stores the response.
 */
static void
svc_cots_kdupdone(struct dupreq *dr, caddr_t res, int size, int status)
{
	if (status == DUP_DONE)
		bcopy(res, dr->dr_resp.buf, size);
	dr->dr_status = status;
}

/*
 * This routine expects that the mutex, cotsdupreq_lock, is already held.
 */
static void
unhash(struct dupreq *dr)
{
	struct dupreq *drt;
	struct dupreq *drtprev = NULL;
	uint32_t drhash;

	ASSERT(MUTEX_HELD(&cotsdupreq_lock));

	drhash = (uint32_t)DRHASH(dr);
	drt = cotsdrhashtbl[drhash];
	while (drt != NULL) {
		if (drt == dr) {
			cotsdrhashstat[drhash]--;
			if (drtprev == NULL) {
				cotsdrhashtbl[drhash] = drt->dr_chain;
			} else {
				drtprev->dr_chain = drt->dr_chain;
			}
			return;
		}
		drtprev = drt;
		drt = drt->dr_chain;
	}
}

void
svc_cots_init()
{
	mutex_init(&cots_kcreate_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&cotsdupreq_lock, NULL, MUTEX_DEFAULT, NULL);
}
