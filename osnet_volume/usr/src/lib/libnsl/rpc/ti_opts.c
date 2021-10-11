/*
 * Copyright (c) 1986-1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)ti_opts.c	1.14	99/12/17 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <tiuser.h>
#include <rpc/rpc.h>
#include <sys/tl.h>
#include <sys/stropts.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>

static const char __rpc_neg_uid_str[] =
	"rpc_negotiate_uid (%s): t_error %d err %d";

/*
 * This routine is typically called on the server side if the server
 * wants to know the caller uid.  Called typically by rpcbind.  It
 * depends upon the t_optmgmt call to local transport driver so that
 * return the uid value in options in T_CONN_IND, T_CONN_CON and
 * T_UNITDATA_IND.
 */

/*
 * Version for Solaris with new local transport code
 */
int
__rpc_negotiate_uid(fd)
	int fd;
{
	struct strioctl strioc;
	unsigned int set = 1;

	trace2(TR___rpc_negotiate_uid, 0, fd);

	strioc.ic_cmd = TL_IOC_CREDOPT;
	strioc.ic_timout = -1;
	strioc.ic_len = (int) sizeof (unsigned int);
	strioc.ic_dp = (char *) &set;

	if (ioctl(fd, I_STR, &strioc) == -1) {
			syslog(LOG_ERR, __rpc_neg_uid_str,
				"ioctl:I_STR:TL_IOC_CREOPT", errno);
		trace2(TR___rpc_negotiate_uid, 1, fd);
		return (-1);
	}

	trace2(TR___rpc_negotiate_uid, 1, fd);
	return (0);
}

/*
 * This returns the uid of the caller.  It assumes that the optbuf information
 * is stored at xprt->xp_p2.
 */

/*
 * Version for Solaris with new local transport code
 */

int
__rpc_get_local_uid(trans, uid_out)
	SVCXPRT *trans;
	uid_t *uid_out;
{
	struct netbuf *abuf;
	struct opthdr *opt_out;
	tl_credopt_t *credoptp;

	trace1(TR___rpc_get_local_uid, 0);
/* LINTED pointer alignment */
	abuf = (struct netbuf *) trans->xp_p2;
	if (abuf == (struct netbuf *) 0) {
#ifdef RPC_DEBUG
		syslog(LOG_ERR, "rpc_get_local_uid:  null xp_p2");
#endif
		trace1(TR___rpc_get_local_uid, 1);
		return (-1);
	}
	if (abuf->buf == 0) {
#ifdef RPC_DEBUG
		syslog(LOG_ERR,
			"rpc_get_local_uid (T_COTS): null data");
#endif
		trace1(TR___rpc_get_local_uid, 1);
		return (-1);
	}
/* LINTED pointer alignment */
	opt_out = (struct  opthdr *) abuf->buf;
	if (abuf->len != (opt_out->len + sizeof (struct opthdr))) {
#ifdef RPC_DEBUG
		syslog(LOG_ERR,
			"rpc_get_local_uid: len %d is wrong, want %d",
			abuf->len, opt_out->len);
#endif
		trace1(TR___rpc_get_local_uid, 1);
		return (-1);
	}
	if ((opt_out->level != TL_PROT_LEVEL) &&
	    (opt_out->name != TL_OPT_PEER_CRED)) {
#ifdef RPC_DEBUG
		syslog(LOG_ERR,
			"rpc_get_local_uid : level,name:",
			"%d,%d is wrong, want %d, %d",
			opt_out->level, out_out->name,
			TL_PROT_LEVEL, TL_OPT_CREDATA);
#endif
		trace1(TR___rpc_get_local_uid, 1);
		return (-1);
	}
	credoptp = (tl_credopt_t *) &opt_out[1];
	*uid_out = credoptp->tc_uid;

	trace1(TR___rpc_get_local_uid, 1);
	return (0);
}

/*
 * Return local credentials.
 */
bool_t
__rpc_get_local_cred(xprt, lcred)
	SVCXPRT			*xprt;
	svc_local_cred_t	*lcred;
{
	struct netbuf		*abuf;
	struct opthdr		*opt_out;
	tl_credopt_t		*credoptp;

/* LINTED pointer alignment */
	if ((abuf = (struct netbuf *)xprt->xp_p2) == NULL || abuf->buf == NULL)
		return (FALSE);
/* LINTED pointer alignment */
	opt_out = (struct  opthdr *)abuf->buf;
	if (abuf->len != (opt_out->len + sizeof (struct opthdr)))
		return (FALSE);
	if (opt_out->level != TL_PROT_LEVEL &&
					opt_out->name != TL_OPT_PEER_CRED)
		return (FALSE);
	credoptp = (tl_credopt_t *)&opt_out[1];
	lcred->euid = credoptp->tc_uid;
	lcred->egid = credoptp->tc_gid;
	lcred->ruid = credoptp->tc_ruid;
	lcred->rgid = credoptp->tc_rgid;
	lcred->pid = (pid_t)-1;
	return (TRUE);
}
