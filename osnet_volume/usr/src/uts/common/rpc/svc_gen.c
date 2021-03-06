/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)svc_gen.c	1.41	99/10/11 SMI"	/* SVr4.0 1.5   */

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  Copyright (c) 1986-1989, 1994-1996, 1998-1999 by Sun Microsystems, Inc.
 *  Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *  All rights reserved.
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <rpc/types.h>
#include <netinet/in.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <rpc/svc.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/sunddi.h>
#include <sys/fcntl.h>
#include <sys/errno.h>

/*
 * Create server-side kernel RPC `master' transport handle
 *
 * This is public interface for creation of a server RPC transport handle
 * for a given file descriptor. This function is called from nfs_svc()
 * and lm_svc().
 *
 * PSARC 1999/553-01 Contract Private Interface
 * svc_tli_kcreate
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-1999-553-01@sun.com
 *
 * Arguments:
 * - fp		 - connection end point
 * - max_msgsize - max receive size
 * - netid	 - netid
 * - addrmask	 - address mask
 * - nxprt       - filled with outgoing transport handle
 * - sct	 - callout table to be registered with this transport handle
 * - closeproc	 - optional pointer to a closeproc for this transport or NULL
 * - id	         - RPC pool id (currently only NFS_SVCPOOL_ID or LM_SVCPOOL_ID)
 * - hotstream	 - very MT-hot flag (TRUE for NFS, FALSE for Lock Manager)
 *
 * Description:
 * - make sure rpcmod is on the stream
 * - call T_INFO_REQ to get the transport service type info
 * - call transport-type specific `create' routine (svc_clts_kcreate(),
 *   svc_cots_kcreate()) to create and initialize transport for the stream
 * - call svc_xprt_register() to register the transport handle into the
 *   service thread pool
 * - initialize transport-type independent fields (synchronization objects,
 *   thread counts, callout table, closeproc)
 * - optionally, for CLTS transports tell streams framework that the
 *   stream can be MT-hot
 * - call transport-type specific `start' function to tell rpcmod that
 *   the transport is ready to receive.
 */
int
svc_tli_kcreate(
	struct file	*fp,		/* connection end point */
	uint_t		max_msgsize,	/* max receive size */
	char		*netid,
	struct netbuf	*addrmask,
	SVCMASTERXPRT	**nxprt,
	SVC_CALLOUT_TABLE *sct,
	void		(*closeproc)(const SVCMASTERXPRT *),
	int		id,		/* thread pool  */
	bool_t		hotstream)
{
	queue_t		*wq;
	SVCMASTERXPRT	*xprt = NULL;	/* service handle */
	int		retval;
	struct strioctl strioc;
	struct T_info_ack tinfo;
	int		error;
	void		**vp;
	major_t		udpmaj;

	RPCLOG(16, "svc_tli_kcreate: on file %p\n", (void *)fp);

	if (fp == NULL || nxprt == NULL)
		return (EINVAL);

	if (fp->f_vnode->v_stream == NULL)
		return (ENOSTR);

	/*
	 * Make sure that an RPC interface module is on the stream.
	 */
	wq = fp->f_vnode->v_stream->sd_wrq;
	while ((wq = wq->q_next) != NULL) {
		if (strcmp(wq->q_qinfo->qi_minfo->mi_idname, "rpcmod") == 0)
			break;
	}
	if (!wq) {
		RPCLOG0(1, "svc_tli_kcreate: no RPC module on stream\n");
		return (EINVAL);
	}

	/*
	 * Find out what type of transport this is.
	 */
	strioc.ic_cmd = TI_GETINFO;
	strioc.ic_timout = -1;
	strioc.ic_len = sizeof (tinfo);
	strioc.ic_dp = (char *)&tinfo;
	tinfo.PRIM_type = T_INFO_REQ;

	error = strioctl(fp->f_vnode, I_STR, (intptr_t)&strioc, 0, K_TO_K,
	    CRED(), &retval);
	if (error || retval) {
		RPCLOG(1, "svc_tli_kcreate: getinfo ioctl: %d\n", error);
		return (error);
	}

	/*
	 * Call transport-type specific `create' function.
	 * It will allocate transport structure.
	 */
	switch (tinfo.SERV_type) {
	case T_CLTS:
		error = svc_clts_kcreate(fp, max_msgsize, &tinfo, &xprt);
		break;
	case T_COTS:
	case T_COTS_ORD:
		error = svc_cots_kcreate(fp, max_msgsize, &tinfo, &xprt);
		break;
	default:
		RPCLOG(1, "svc_tli_kcreate: Bad service type %d\n",
		    tinfo.SERV_type);
		error = EINVAL;
	}
	if (error)
		return (error);

	/*
	 * Initialize transport-type independent fields.
	 */
	xprt->xp_req_head = (mblk_t *)0;
	xprt->xp_req_tail = (mblk_t *)0;
	mutex_init(&xprt->xp_req_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&xprt->xp_thread_lock, NULL, MUTEX_DEFAULT, NULL);
	xprt->xp_type = tinfo.SERV_type;
	xprt->xp_threads = 0;
	xprt->xp_detached_threads = 0;
	xprt->xp_fp = fp;
	xprt->xp_wq = wq;
	xprt->xp_closeproc = closeproc;
	xprt->xp_sct = sct;
	xprt->xp_netid = NULL;
	if (netid != NULL) {
		xprt->xp_netid = kmem_alloc(strlen(netid) + 1, KM_SLEEP);
		(void) strcpy(xprt->xp_netid, netid);
	}

	xprt->xp_addrmask.len = 0;
	xprt->xp_addrmask.maxlen = 0;
	xprt->xp_addrmask.buf = NULL;

	if (addrmask != NULL) {
		xprt->xp_addrmask = *addrmask;
	}

	/*
	 * Register this transport handle after all fields have been
	 * initialized. The registration can fail only if we try to register
	 * with a non-existent pool (ENOENT) or a closing pool (EBUSY).
	 */
	if (error = svc_xprt_register(xprt, id)) {
		SVC_DESTROY(xprt);
		cmn_err(CE_WARN, "svc_tli_kcreate: xprt_register failed");

		return (error);
	}

	/*
	 * Set the private RPC cell in the module's data.
	 */
	vp = (void **)wq->q_ptr;
	vp[0] = xprt;

	/*
	 * Inform the streams framework that the stream may be very MT hot.
	 */
	if (hotstream && tinfo.SERV_type == T_CLTS) {
		udpmaj = ddi_name_to_major("udp");
		if (udpmaj != (major_t)-1 &&
			getmajor(fp->f_vnode->v_rdev) == udpmaj)
			create_putlocks(wq, 1);
	}

	*nxprt = xprt;

	/*
	 * Tell rpcmod that the transport is fully initialized and
	 * ready to process requests.
	 */
	SVC_START(xprt);

	return (0);
}
