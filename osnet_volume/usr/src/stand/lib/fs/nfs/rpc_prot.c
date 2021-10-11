/*
 * Copyright (C) 1984-1999, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpc_prot.c	1.8	99/02/23 SMI" /* from SunOS 4.1 1.41 90/03/30 */

/*
 * rpc_prot.c
 *
 * Modified for use in the nfs boot program.
 *
 * This set of routines implements the rpc message definition,
 * its serializer and some common rpc utility routines.
 * The routines are meant for various implementations of rpc -
 * they are NOT for the rpc client or rpc service implementations!
 * Because authentication stuff is easy and is part of rpc, the opaque
 * routines are also in this program.
 */

#include <sys/param.h>
#include <rpc/types.h>		/* spell 'em out for make depend */
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <netinet/in.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/bootdebug.h>

#define	dprintf	if (boothowto & RB_DEBUG) printf

/* * * * * * * * * * * * * * XDR Authentication * * * * * * * * * * * */

struct opaque_auth _null_auth;

/*
 * XDR an opaque authentication struct
 * (see auth.h)
 */
bool_t
xdr_opaque_auth(XDR *xdrs, struct opaque_auth *ap)
{
	if (xdr_enum(xdrs, &(ap->oa_flavor))) {
		return (xdr_bytes(xdrs, &ap->oa_base,
		    &ap->oa_length, MAX_AUTH_BYTES));
	}
	return (FALSE);
}

/* * * * * * * * * * * * * * XDR RPC MESSAGE * * * * * * * * * * * * * * * */

/*
 * XDR the MSG_ACCEPTED part of a reply message union
 */
bool_t
xdr_accepted_reply(XDR *xdrs, struct accepted_reply *ar)
{
	/* personalized union, rather than calling xdr_union */
	if (!xdr_opaque_auth(xdrs, &(ar->ar_verf)))
		return (FALSE);
	if (!xdr_enum(xdrs, (enum_t *)&(ar->ar_stat)))
		return (FALSE);

	switch (ar->ar_stat) {
	case SUCCESS:
		return ((*(ar->ar_results.proc))(xdrs, ar->ar_results.where));

	case PROG_MISMATCH:
		if (!xdr_rpcvers(xdrs, &(ar->ar_vers.low)))
			return (FALSE);
		return (xdr_rpcvers(xdrs, &(ar->ar_vers.high)));
	}
	return (TRUE);  /* TRUE => open ended set of problems */
}

/*
 * XDR the MSG_DENIED part of a reply message union
 */
bool_t
xdr_rejected_reply(XDR *xdrs, struct rejected_reply *rr)
{
	/* personalized union, rather than calling xdr_union */
	if (!xdr_enum(xdrs, (enum_t *)&(rr->rj_stat)))
		return (FALSE);
	switch (rr->rj_stat) {

	case RPC_MISMATCH:
		if (!xdr_rpcvers(xdrs, &(rr->rj_vers.low)))
			return (FALSE);
		return (xdr_rpcvers(xdrs, &(rr->rj_vers.high)));

	case AUTH_ERROR:
		return (xdr_enum(xdrs, (enum_t *)&(rr->rj_why)));
	}
	return (FALSE);
}

/*
 * XDR a reply message
 */
bool_t
xdr_replymsg(XDR *xdrs, struct rpc_msg *rmsg)
{
	struct xdr_discrim reply_dscrm[3];
	int32_t *buf;
	struct accepted_reply *ar;
	struct opaque_auth *oa;
	u_int rndup;

	if (xdrs->x_op == XDR_ENCODE &&
	    rmsg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    rmsg->rm_direction == REPLY &&
	    (buf = XDR_INLINE(xdrs, 6 * BYTES_PER_XDR_UNIT + (rndup =
	    RNDUP(rmsg->rm_reply.rp_acpt.ar_verf.oa_length)))) != NULL) {
		IXDR_PUT_INT32(buf, rmsg->rm_xid);
		IXDR_PUT_ENUM(buf, rmsg->rm_direction);
		IXDR_PUT_ENUM(buf, rmsg->rm_reply.rp_stat);
		ar = &rmsg->rm_reply.rp_acpt;
		oa = &ar->ar_verf;
		IXDR_PUT_ENUM(buf, oa->oa_flavor);
		IXDR_PUT_INT32(buf, oa->oa_length);
		if (oa->oa_length) {
			bcopy(oa->oa_base, buf, oa->oa_length);
			buf = (int32_t *)(((caddr_t)buf) + oa->oa_length);
			if ((rndup = (rndup - oa->oa_length)) > 0) {
				bzero(buf, rndup);
				buf = (int32_t *)(((caddr_t)buf) + rndup);
			}
		}
		/*
		 * stat and rest of reply, copied from xdr_accepted_reply
		 */
		IXDR_PUT_ENUM(buf, ar->ar_stat);
		switch (ar->ar_stat) {
		case SUCCESS:
			return ((*(ar->ar_results.proc))(xdrs,
			    ar->ar_results.where));

		case PROG_MISMATCH:
			if (!xdr_rpcvers(xdrs, &(ar->ar_vers.low)))
				return (FALSE);
			return (xdr_rpcvers(xdrs, &(ar->ar_vers.high)));
		}
		return (TRUE);
	}

	reply_dscrm[0].value = MSG_ACCEPTED;
	reply_dscrm[0].proc = xdr_accepted_reply;
	reply_dscrm[1].value = MSG_DENIED;
	reply_dscrm[1].proc = xdr_rejected_reply;
	reply_dscrm[2].value = __dontcare__;
	reply_dscrm[2].proc = NULL_xdrproc_t;

	if (xdrs->x_op == XDR_DECODE &&
	    (buf = XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT)) != NULL) {
		rmsg->rm_xid = IXDR_GET_INT32(buf);
		rmsg->rm_direction = IXDR_GET_ENUM(buf, enum msg_type);
		if (rmsg->rm_direction != REPLY) {
			return (FALSE);
		}
		rmsg->rm_reply.rp_stat = IXDR_GET_ENUM(buf, enum reply_stat);
		if (rmsg->rm_reply.rp_stat != MSG_ACCEPTED) {
			if (rmsg->rm_reply.rp_stat == MSG_DENIED) {
				return (xdr_rejected_reply(xdrs,
				    &rmsg->rm_reply.rp_rjct));
			}
			return (FALSE);
		}
		ar = &rmsg->rm_reply.rp_acpt;
		oa = &ar->ar_verf;
		buf = XDR_INLINE(xdrs, 2 * BYTES_PER_XDR_UNIT);
		if (buf != NULL) {
			oa->oa_flavor = IXDR_GET_ENUM(buf, enum_t);
			oa->oa_length = IXDR_GET_INT32(buf);
		} else {
			if (xdr_enum(xdrs, &oa->oa_flavor) == FALSE ||
			    xdr_u_int(xdrs, &oa->oa_length) == FALSE)
				return (FALSE);
		}
		if (oa->oa_length) {
			if (oa->oa_length > MAX_AUTH_BYTES)
				return (FALSE);
			if (oa->oa_base == NULL) {
				dprintf("xdr_replymsg: no buffer allocated "
				    "for oa_base. Needs %d bytes.\n",
				    oa->oa_length);
				return (FALSE);
			}
			buf = XDR_INLINE(xdrs, RNDUP(oa->oa_length));
			if (buf == NULL) {
				if (xdr_opaque(xdrs, oa->oa_base,
				    oa->oa_length) == FALSE) {
					return (FALSE);
				}
			} else {
				bcopy(buf, oa->oa_base, oa->oa_length);
			}
		}
		/*
		 * stat and rest of reply, copied from
		 * xdr_accepted_reply
		 */
		if (!xdr_enum(xdrs, (enum_t *)&ar->ar_stat))
			return (FALSE);
		switch (ar->ar_stat) {
		case SUCCESS:
			return ((*(ar->ar_results.proc))(xdrs,
			    ar->ar_results.where));

		case PROG_MISMATCH:
			if (!xdr_rpcvers(xdrs, &ar->ar_vers.low))
				return (FALSE);
			return (xdr_rpcvers(xdrs, &ar->ar_vers.high));
		}
		return (TRUE);
	}

	if (xdr_u_int(xdrs, &(rmsg->rm_xid)) &&
	    xdr_enum(xdrs, (enum_t *)&(rmsg->rm_direction)) &&
	    (rmsg->rm_direction == REPLY))
		return (xdr_union(xdrs, (enum_t *)&(rmsg->rm_reply.rp_stat),
		    (caddr_t)&(rmsg->rm_reply.ru), reply_dscrm,
		    NULL_xdrproc_t));
	return (FALSE);
}

/*
 * Serializes the "static part" of a call message header.
 * The fields include: rm_xid, rm_direction, rpcvers, prog, and vers.
 * The rm_xid is not really static, but the user can easily munge on the fly.
 */
bool_t
xdr_callhdr(XDR *xdrs, struct rpc_msg *cmsg)
{

	cmsg->rm_direction = CALL;
	cmsg->rm_call.cb_rpcvers = RPC_MSG_VERSION;
	if (xdrs->x_op == XDR_ENCODE &&
	    xdr_u_int(xdrs, &(cmsg->rm_xid)) &&
	    xdr_enum(xdrs, (enum_t *)&(cmsg->rm_direction)) &&
	    xdr_rpcvers(xdrs, &(cmsg->rm_call.cb_rpcvers)) &&
	    xdr_rpcprog(xdrs, &(cmsg->rm_call.cb_prog)))
		return (xdr_rpcvers(xdrs, &(cmsg->rm_call.cb_vers)));
	return (FALSE);
}

/* ************************** Client utility routine ************* */

static void
accepted(enum accept_stat acpt_stat, struct rpc_err *error)
{
	switch (acpt_stat) {
	case PROG_UNAVAIL:
		error->re_status = RPC_PROGUNAVAIL;
		return;

	case PROG_MISMATCH:
		error->re_status = RPC_PROGVERSMISMATCH;
		return;

	case PROC_UNAVAIL:
		error->re_status = RPC_PROCUNAVAIL;
		return;

	case GARBAGE_ARGS:
		error->re_status = RPC_CANTDECODEARGS;
		return;

	case SYSTEM_ERR:
		error->re_status = RPC_SYSTEMERROR;
		return;

	case SUCCESS:
		error->re_status = RPC_SUCCESS;
		return;
	}
	/* something's wrong, but we don't know what ... */
	error->re_status = RPC_FAILED;
	error->re_lb.s1 = (int32_t)MSG_ACCEPTED;
	error->re_lb.s2 = (int32_t)acpt_stat;
}

static void
rejected(enum reject_stat rjct_stat, struct rpc_err *error)
{
	switch (rjct_stat) {
	case RPC_MISMATCH:
		error->re_status = RPC_VERSMISMATCH;
		return;

	case AUTH_ERROR:
		error->re_status = RPC_AUTHERROR;
		return;
	}
	/* something's wrong, but we don't know what ... */
	error->re_status = RPC_FAILED;
	error->re_lb.s1 = (int32_t)MSG_DENIED;
	error->re_lb.s2 = (int32_t)rjct_stat;
}

/*
 * given a reply message, fills in the error
 */
void
_seterr_reply(struct rpc_msg *msg, struct rpc_err *error)
{
	/* optimized for normal, SUCCESSful case */
	switch (msg->rm_reply.rp_stat) {
	case MSG_ACCEPTED:
		if (msg->acpted_rply.ar_stat == SUCCESS) {
			error->re_status = RPC_SUCCESS;
			return;
		};
		accepted(msg->acpted_rply.ar_stat, error);
		break;

	case MSG_DENIED:
		rejected(msg->rjcted_rply.rj_stat, error);
		break;

	default:
		error->re_status = RPC_FAILED;
		error->re_lb.s1 = (int32_t)(msg->rm_reply.rp_stat);
		break;
	}
	switch (error->re_status) {
	case RPC_VERSMISMATCH:
		error->re_vers.low = msg->rjcted_rply.rj_vers.low;
		error->re_vers.high = msg->rjcted_rply.rj_vers.high;
		break;

	case RPC_AUTHERROR:
		error->re_why = msg->rjcted_rply.rj_why;
		break;

	case RPC_PROGVERSMISMATCH:
		error->re_vers.low = msg->acpted_rply.ar_vers.low;
		error->re_vers.high = msg->acpted_rply.ar_vers.high;
		break;
	}
}
