/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)svc_clts.c	1.59	99/08/13 SMI"

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
 *  Copyright (c) 1986-1989, 1995, 1996, 1998-1999 by Sun Microsystems, Inc.
 *  Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *  All rights reserved.
 */

/*
 * svc_clts.c
 * Server side for RPC in the kernel.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/kstat.h>
#include <sys/vtrace.h>
#include <sys/debug.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/svc.h>

/*
 * Routines exported through ops vector.
 */
static bool_t		svc_clts_krecv(SVCXPRT *, mblk_t *, struct rpc_msg *);
static bool_t		svc_clts_ksend(SVCXPRT *, struct rpc_msg *);
static bool_t		svc_clts_kgetargs(SVCXPRT *, xdrproc_t, caddr_t);
static bool_t		svc_clts_kfreeargs(SVCXPRT *, xdrproc_t, caddr_t);
static void		svc_clts_kdestroy(SVCMASTERXPRT *);
static int		svc_clts_kdup(struct svc_req *, caddr_t, int,
				struct dupreq **);
static void		svc_clts_kdupdone(struct dupreq *, caddr_t, int, int);
static int32_t		*svc_clts_kgetres(SVCXPRT *, int);
static void		svc_clts_kclone_destroy(SVCXPRT *);
static void		svc_clts_kfreeres(SVCXPRT *);
static void		svc_clts_kstart(SVCMASTERXPRT *);

/*
 * Server transport operations vector.
 */
struct svc_ops svc_clts_op = {
	svc_clts_krecv,		/* Get requests */
	svc_clts_kgetargs,	/* Deserialize arguments */
	svc_clts_ksend,		/* Send reply */
	svc_clts_kfreeargs,	/* Free argument data space */
	svc_clts_kdestroy,	/* Destroy transport handle */
	svc_clts_kdup,		/* Check entry in dup req cache */
	svc_clts_kdupdone,	/* Mark entry in dup req cache as done */
	svc_clts_kgetres,	/* Get pointer to response buffer */
	svc_clts_kfreeres,	/* Destroy pre-serialized response header */
	svc_clts_kclone_destroy, /* Destroy a clone xprt */
	svc_clts_kstart		/* Tell `ready-to-receive' to rpcmod */
};

/*
 * Pointer to a transport specific `send response' function in rpcmod
 * (set from rpcmod).
 */
int	(*rpc_send)(queue_t *, mblk_t *) = NULL;

/*
 * Transport private data.
 * Kept in xprt->xp_p2.
 */
struct udp_data {
	mblk_t	*ud_resp;			/* buffer for response */
	mblk_t	*ud_inmp;			/* mblk chain of request */
};

#define	UD_MAXSIZE	8800
#define	UD_INITSIZE	2048

/*
 * Connectionless server statistics
 */
struct {
	kstat_named_t	rscalls;
	kstat_named_t	rsbadcalls;
	kstat_named_t	rsnullrecv;
	kstat_named_t	rsbadlen;
	kstat_named_t	rsxdrcall;
	kstat_named_t	rsdupchecks;
	kstat_named_t	rsdupreqs;
} rsstat = {
	{ "calls",	KSTAT_DATA_UINT64 },
	{ "badcalls",	KSTAT_DATA_UINT64 },
	{ "nullrecv",	KSTAT_DATA_UINT64 },
	{ "badlen",	KSTAT_DATA_UINT64 },
	{ "xdrcall",	KSTAT_DATA_UINT64 },
	{ "dupchecks",	KSTAT_DATA_UINT64 },
	{ "dupreqs",	KSTAT_DATA_UINT64 }
};

kstat_named_t	*rsstat_ptr = (kstat_named_t *)&rsstat;
uint_t		rsstat_ndata = sizeof (rsstat) / sizeof (kstat_named_t);

#ifdef accurate_stats
#define	RSSTAT_INCR(x)			\
	mutex_enter(&rsstat_lock);	\
	rsstat.x.value.ui64++;		\
	mutex_exit(&rsstat_lock);

static kmutex_t	rsstat_lock;
#else
#define	RSSTAT_INCR(x)	rsstat.x.value.ui64++
#endif

/*
 * Create a transport record.
 * The transport record, output buffer, and private data structure
 * are allocated.  The output buffer is serialized into using xdrmem.
 * There is one transport record per user process which implements a
 * set of services.
 */
/* ARGSUSED */
int
svc_clts_kcreate(file_t *fp, uint_t sendsz, struct T_info_ack *tinfo,
    SVCMASTERXPRT **nxprt)
{
	struct udp_data *ud;
	SVCMASTERXPRT *xprt;

	if (nxprt == NULL)
		return (EINVAL);
	/*
	 * Check to make sure that the clts private data will fit into
	 * the stack buffer allocated by svc_run.  The compiler should
	 * remove this check, but it's a safety net if the udp_data
	 * structure ever changes.
	 */
	/*CONSTANTCONDITION*/
	ASSERT(sizeof (struct udp_data) <= SVC_P2LEN);

	xprt = kmem_zalloc(sizeof (*xprt), KM_SLEEP);
	ud = kmem_zalloc(sizeof (*ud), KM_SLEEP);
	xprt->xp_p2 = (caddr_t)ud;
	xprt->xp_ops = &svc_clts_op;
	xprt->xp_msg_size = tinfo->TSDU_size;

	xprt->xp_rtaddr.buf = NULL;
	xprt->xp_rtaddr.maxlen = tinfo->ADDR_size;
	xprt->xp_rtaddr.len = 0;

	*nxprt = xprt;

	return (0);
}

/*
 * Destroy a transport record.
 * Frees the space allocated for a transport record.
 */
static void
svc_clts_kdestroy(SVCMASTERXPRT *xprt)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)xprt->xp_p2;

	if (ud->ud_resp) {
		/*
		 * There should not be any left over results buffer.
		 */
		ASSERT(ud->ud_resp->b_cont == NULL);

		/*
		 * Free the T_UNITDATA_{REQ/IND} that svc_clts_krecv
		 * saved.
		 */
		freeb(ud->ud_resp);
	}
	if (ud->ud_inmp)
		freemsg(ud->ud_inmp);

	if (xprt->xp_netid)
		kmem_free(xprt->xp_netid, strlen(xprt->xp_netid) + 1);
	if (xprt->xp_addrmask.maxlen)
		kmem_free(xprt->xp_addrmask.buf, xprt->xp_addrmask.maxlen);

	mutex_destroy(&xprt->xp_req_lock);
	mutex_destroy(&xprt->xp_thread_lock);

	kmem_free(ud, sizeof (struct udp_data));
	kmem_free(xprt, sizeof (SVCMASTERXPRT));
}

/*
 * Transport-type specific part of svc_xprt_cleanup().
 * Frees the message buffer space allocated for a clone of a transport record
 */
static void
svc_clts_kclone_destroy(SVCXPRT *clone_xprt)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;

	if (ud->ud_resp) {
		/*
		 * There should not be any left over results buffer.
		 */
		ASSERT(ud->ud_resp->b_cont == NULL);

		/*
		 * Free the T_UNITDATA_{REQ/IND} that svc_clts_krecv
		 * saved.
		 */
		freeb(ud->ud_resp);
	}
	if (ud->ud_inmp)
		freemsg(ud->ud_inmp);
}

/*
 * svc_tli_kcreate() calls this function at the end to tell
 * rpcmod that the transport is ready to receive requests.
 */
/* ARGSUSED */
static void
svc_clts_kstart(SVCMASTERXPRT *xprt)
{
}

/*
 * Receive rpc requests.
 * Pulls a request in off the socket, checks if the packet is intact,
 * and deserializes the call packet.
 */
static bool_t
svc_clts_krecv(SVCXPRT *clone_xprt, mblk_t *mp, struct rpc_msg *msg)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrin;
	union T_primitives *pptr;
	int hdrsz;

	TRACE_0(TR_FAC_KRPC, TR_SVC_CLTS_KRECV_START,
	    "svc_clts_krecv_start:");

	RSSTAT_INCR(rscalls);

	/*
	 * The incoming request should start with an M_PROTO message.
	 */
	if (mp->b_datap->db_type != M_PROTO) {
		goto bad;
	}

	/*
	 * The incoming request should be an T_UNITDTA_IND.  There
	 * might be other messages coming up the stream, but we can
	 * ignore them.
	 */
	pptr = (union T_primitives *)mp->b_rptr;
	if (pptr->type != T_UNITDATA_IND) {
		goto bad;
	}
	/*
	 * Do some checking to make sure that the header at least looks okay.
	 */
	hdrsz = (int)(mp->b_wptr - mp->b_rptr);
	if (hdrsz < TUNITDATAINDSZ ||
	    hdrsz < (pptr->unitdata_ind.OPT_offset +
		    pptr->unitdata_ind.OPT_length) ||
	    hdrsz < (pptr->unitdata_ind.SRC_offset +
		    pptr->unitdata_ind.SRC_length)) {
		goto bad;
	}

	/*
	 * Make sure that the transport provided a usable address.
	 */
	if (pptr->unitdata_ind.SRC_length <= 0) {
		goto bad;
	}
	/*
	 * Point the remote transport address in the service_transport
	 * handle at the address in the request.
	 */
	clone_xprt->xp_rtaddr.buf = (char *)mp->b_rptr +
	    pptr->unitdata_ind.SRC_offset;
	clone_xprt->xp_rtaddr.len = pptr->unitdata_ind.SRC_length;

	/*
	 * Save the first mblk which contains the T_unidata_ind in
	 * ud_resp.  It will be used to generate the T_unitdata_req
	 * during the reply.
	 */
	if (ud->ud_resp) {
		if (ud->ud_resp->b_cont != NULL) {
			cmn_err(CE_WARN, "svc_clts_krecv: ud_resp %p, "
			    "b_cont %p", (void *)ud->ud_resp,
			    (void *)ud->ud_resp->b_cont);
		}
		freeb(ud->ud_resp);
	}
	ud->ud_resp = mp;
	mp = mp->b_cont;
	ud->ud_resp->b_cont = NULL;

	xdrmblk_init(xdrs, mp, XDR_DECODE, 0);

	TRACE_0(TR_FAC_KRPC, TR_XDR_CALLMSG_START,
	    "xdr_callmsg_start:");
	if (! xdr_callmsg(xdrs, msg)) {
		TRACE_1(TR_FAC_KRPC, TR_XDR_CALLMSG_END,
		    "xdr_callmsg_end:(%S)", "bad");
		RSSTAT_INCR(rsxdrcall);
		goto bad;
	}
	TRACE_1(TR_FAC_KRPC, TR_XDR_CALLMSG_END,
	    "xdr_callmsg_end:(%S)", "good");

	clone_xprt->xp_xid = msg->rm_xid;
	ud->ud_inmp = mp;

	TRACE_1(TR_FAC_KRPC, TR_SVC_CLTS_KRECV_END,
	    "svc_clts_krecv_end:(%S)", "good");
	return (TRUE);

bad:
	if (mp)
		freemsg(mp);
	if (ud->ud_resp) {
		/*
		 * There should not be any left over results buffer.
		 */
		ASSERT(ud->ud_resp->b_cont == NULL);
		freeb(ud->ud_resp);
		ud->ud_resp = NULL;
	}

	RSSTAT_INCR(rsbadcalls);
	TRACE_1(TR_FAC_KRPC, TR_SVC_CLTS_KRECV_END,
	    "svc_clts_krecv_end:(%S)", "bad");
	return (FALSE);
}

/*
 * Send rpc reply.
 * Serialize the reply packet into the output buffer then
 * call t_ksndudata to send it.
 */
static bool_t
svc_clts_ksend(SVCXPRT *clone_xprt, struct rpc_msg *msg)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrout;
	int stat = FALSE;
	int error;
	mblk_t *mp;
	int msgsz;
	struct T_unitdata_req *udreq;
	xdrproc_t xdr_results;
	caddr_t xdr_location;
	bool_t has_args;


	TRACE_0(TR_FAC_KRPC, TR_SVC_CLTS_KSEND_START,
	    "svc_clts_ksend_start:");

	ASSERT(ud->ud_resp != NULL);

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

	if (ud->ud_resp->b_cont == NULL) {
		/*
		 * Allocate an initial mblk for the response data.
		 */
		while ((mp = allocb(UD_INITSIZE, BPRI_LO)) == NULL) {
			if (error = strwaitbuf(UD_INITSIZE, BPRI_LO)) {
				TRACE_1(TR_FAC_KRPC, TR_SVC_CLTS_KSEND_END,
				    "svc_clts_ksend_end:(%S)", "strwaitbuf");
				return (FALSE);
			}
		}

		/*
		 * Initialize the XDR decode stream.  Additional mblks
		 * will be allocated if necessary.  They will be UD_MAXSIZE
		 * sized.
		 */
		xdrmblk_init(xdrs, mp, XDR_ENCODE, UD_MAXSIZE);

		/*
		 * Leave some space for protocol headers.
		 */
		(void) XDR_SETPOS(xdrs, 512);
		mp->b_rptr += 512;

		msg->rm_xid = clone_xprt->xp_xid;

		ud->ud_resp->b_cont = mp;

		TRACE_0(TR_FAC_KRPC, TR_XDR_REPLYMSG_START,
		    "xdr_replymsg_start:");
		if (!(xdr_replymsg(xdrs, msg) &&
			(!has_args || SVCAUTH_WRAP(&clone_xprt->xp_auth, xdrs,
				xdr_results, xdr_location)))) {
			TRACE_1(TR_FAC_KRPC, TR_XDR_REPLYMSG_END,
			    "xdr_replymsg_end:(%S)", "bad");
			RPCLOG0(1, "xdr_replymsg/SVCAUTH_WRAP failed\n");
			goto out;
		}
		TRACE_1(TR_FAC_KRPC, TR_XDR_REPLYMSG_END,
		    "xdr_replymsg_end:(%S)", "good");

	} else if (!(xdr_replymsg_body(xdrs, msg) &&
		    (!has_args || SVCAUTH_WRAP(&clone_xprt->xp_auth, xdrs,
				xdr_results, xdr_location)))) {
		RPCLOG0(1, "xdr_replymsg_body/SVCAUTH_WRAP failed\n");
		goto out;
	}

	msgsz = (int)xmsgsize(ud->ud_resp->b_cont);

	if (msgsz <= 0 || (clone_xprt->xp_msg_size != -1 &&
	    msgsz > clone_xprt->xp_msg_size)) {
#ifdef	DEBUG
		cmn_err(CE_NOTE,
"KRPC: server response message of %d bytes; transport limits are [0, %d]",
			msgsz, clone_xprt->xp_msg_size);
#endif
		goto out;
	}

	/*
	 * Construct the T_unitdata_req.  We take advantage
	 * of the fact that T_unitdata_ind looks just like
	 * T_unitdata_req, except for the primitive type.
	 */
	udreq = (struct T_unitdata_req *)ud->ud_resp->b_rptr;
	udreq->PRIM_type = T_UNITDATA_REQ;

	error = (*rpc_send)(clone_xprt->xp_wq, ud->ud_resp);

	if (!error) {
		stat = TRUE;
		ud->ud_resp = NULL;
	}

out:
	if (stat == FALSE) {
		freemsg(ud->ud_resp);
		ud->ud_resp = NULL;
	}

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

	TRACE_1(TR_FAC_KRPC, TR_SVC_CLTS_KSEND_END,
	    "svc_clts_ksend_end:(%S)", "done");
	return (stat);
}

/*
 * Deserialize arguments.
 */
static bool_t
svc_clts_kgetargs(SVCXPRT *clone_xprt, xdrproc_t xdr_args,
    caddr_t args_ptr)
{

	/* LINTED pointer alignment */
	return (SVCAUTH_UNWRAP(&clone_xprt->xp_auth, &clone_xprt->xp_xdrin,
				xdr_args, args_ptr));

}

static bool_t
svc_clts_kfreeargs(SVCXPRT *clone_xprt, xdrproc_t xdr_args,
    caddr_t args_ptr)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrin;
	bool_t retval;

	if (args_ptr) {
		xdrs->x_op = XDR_FREE;
		retval = (*xdr_args)(xdrs, args_ptr);
	} else
		retval = TRUE;

	if (ud->ud_inmp) {
		freemsg(ud->ud_inmp);
		ud->ud_inmp = NULL;
	}

	return (retval);
}

static int32_t *
svc_clts_kgetres(SVCXPRT *clone_xprt, int size)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;
	XDR *xdrs = &clone_xprt->xp_xdrout;
	mblk_t *mp;
	int32_t *buf;
	struct rpc_msg rply;

	/*
	 * Allocate an initial mblk for the response data.
	 */
	while ((mp = allocb(UD_INITSIZE, BPRI_LO)) == NULL) {
		if (strwaitbuf(UD_INITSIZE, BPRI_LO)) {
			return (FALSE);
		}
	}

	mp->b_cont = NULL;

	/*
	 * Initialize the XDR decode stream.  Additional mblks
	 * will be allocated if necessary.  They will be UD_MAXSIZE
	 * sized.
	 */
	xdrmblk_init(xdrs, mp, XDR_ENCODE, UD_MAXSIZE);

	/*
	 * Leave some space for protocol headers.
	 */
	(void) XDR_SETPOS(xdrs, 512);
	mp->b_rptr += 512;

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

	if (buf == NULL)
		freeb(mp);
	else
		ud->ud_resp->b_cont = mp;

	return (buf);
}

static void
svc_clts_kfreeres(SVCXPRT *clone_xprt)
{
	/* LINTED pointer alignment */
	struct udp_data *ud = (struct udp_data *)clone_xprt->xp_p2buf;

	if (ud->ud_resp == NULL || ud->ud_resp->b_cont == NULL)
		return;

	/*
	 * SVC_FREERES() is called whenever the server decides not to
	 * send normal reply. Thus, we expect only one mblk to be allocated,
	 * because we have not attempted any XDR encoding.
	 * If we do any XDR encoding and we get an error, then SVC_REPLY()
	 * will freemsg(ud->ud_resp);
	 */
	ASSERT(ud->ud_resp->b_cont->b_cont == NULL);
	freeb(ud->ud_resp->b_cont);
	ud->ud_resp->b_cont = NULL;
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

static int	ndupreqs = 0;
static int	maxdupreqs = MAXDUPREQS;
static kmutex_t dupreq_lock;
static struct dupreq *drhashtbl[DRHASHSZ];
static int	drhashstat[DRHASHSZ];

static void unhash(struct dupreq *);

/*
 * drmru points to the head of a circular linked list in lru order.
 * drmru->dr_next == drlru
 */
struct dupreq *drmru;

/*
 * svc_clts_kdup searches the request cache and returns 0 if the
 * request is not found in the cache.  If it is found, then it
 * returns the state of the request (in progress or done) and
 * the status or attributes that were part of the original reply.
 */
static int
svc_clts_kdup(struct svc_req *req, caddr_t res, int size, struct dupreq **drpp)
{
	struct dupreq *dr;
	uint32_t xid;
	uint32_t drhash;
	int status;

	xid = REQTOXID(req);
	mutex_enter(&dupreq_lock);
	RSSTAT_INCR(rsdupchecks);
	/*
	 * Check to see whether an entry already exists in the cache.
	 */
	dr = drhashtbl[XIDHASH(xid)];
	while (dr != NULL) {
		if (dr->dr_xid == xid &&
		    dr->dr_proc == req->rq_proc &&
		    dr->dr_prog == req->rq_prog &&
		    dr->dr_vers == req->rq_vers &&
		    dr->dr_addr.len == req->rq_xprt->xp_rtaddr.len &&
		    bcmp(dr->dr_addr.buf, req->rq_xprt->xp_rtaddr.buf,
		    dr->dr_addr.len) == 0) {
			status = dr->dr_status;
			if (status == DUP_DONE) {
				bcopy(dr->dr_resp.buf, res, size);
			} else {
				dr->dr_status = DUP_INPROGRESS;
				*drpp = dr;
			}
			RSSTAT_INCR(rsdupreqs);
			mutex_exit(&dupreq_lock);
			return (status);
		}
		dr = dr->dr_chain;
	}

	/*
	 * There wasn't an entry, either allocate a new one or recycle
	 * an old one.
	 */
	if (ndupreqs < maxdupreqs) {
		dr = kmem_alloc(sizeof (*dr), KM_NOSLEEP);
		if (dr == NULL) {
			mutex_exit(&dupreq_lock);
			return (DUP_ERROR);
		}
		dr->dr_resp.buf = NULL;
		dr->dr_resp.maxlen = 0;
		dr->dr_addr.buf = NULL;
		dr->dr_addr.maxlen = 0;
		if (drmru) {
			dr->dr_next = drmru->dr_next;
			drmru->dr_next = dr;
		} else {
			dr->dr_next = dr;
		}
		ndupreqs++;
	} else {
		dr = drmru->dr_next;
		while (dr->dr_status == DUP_INPROGRESS)
			dr = dr->dr_next;
		unhash(dr);
	}
	drmru = dr;

	dr->dr_xid = REQTOXID(req);
	dr->dr_prog = req->rq_prog;
	dr->dr_vers = req->rq_vers;
	dr->dr_proc = req->rq_proc;
	if (dr->dr_addr.maxlen < req->rq_xprt->xp_rtaddr.len) {
		if (dr->dr_addr.buf != NULL)
			kmem_free(dr->dr_addr.buf, dr->dr_addr.maxlen);
		dr->dr_addr.maxlen = req->rq_xprt->xp_rtaddr.len;
		dr->dr_addr.buf = kmem_alloc(dr->dr_addr.maxlen,
		    KM_NOSLEEP);
		if (dr->dr_addr.buf == NULL) {
			dr->dr_addr.maxlen = 0;
			dr->dr_status = DUP_DROP;
			mutex_exit(&dupreq_lock);
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
			mutex_exit(&dupreq_lock);
			return (DUP_ERROR);
		}
	}
	dr->dr_status = DUP_INPROGRESS;

	drhash = (uint32_t)DRHASH(dr);
	dr->dr_chain = drhashtbl[drhash];
	drhashtbl[drhash] = dr;
	drhashstat[drhash]++;
	mutex_exit(&dupreq_lock);
	*drpp = dr;
	return (DUP_NEW);
}

/*
 * svc_clts_kdupdone marks the request done (DUP_DONE or DUP_DROP)
 * and stores the response.
 */
static void
svc_clts_kdupdone(struct dupreq *dr, caddr_t res, int size, int status)
{

	if (status == DUP_DONE)
		bcopy(res, dr->dr_resp.buf, size);
	dr->dr_status = status;
}

/*
 * This routine expects that the mutex, dupreq_lock, is already held.
 */
static void
unhash(struct dupreq *dr)
{
	struct dupreq *drt;
	struct dupreq *drtprev = NULL;
	uint32_t drhash;

	ASSERT(MUTEX_HELD(&dupreq_lock));

	drhash = (uint32_t)DRHASH(dr);
	drt = drhashtbl[drhash];
	while (drt != NULL) {
		if (drt == dr) {
			drhashstat[drhash]--;
			if (drtprev == NULL) {
				drhashtbl[drhash] = drt->dr_chain;
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
svc_clts_init()
{
	mutex_init(&dupreq_lock, NULL, MUTEX_DEFAULT, NULL);
#ifdef accurate_stats
	mutex_init(&rsstat_lock, NULL, MUTEX_DEFAULT, NULL);
#endif
}
