/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)t_kbind.c	1.15	97/09/01 SMI"	/* SVr4.0 1.4  */

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
 *  	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 */

/*
 * Kernel TLI-like function to bind a transport endpoint
 * to an address.
 *
 * Returns 0 on success or positive error code.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <sys/vnode.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>

int
t_kbind(TIUSER *tiptr, struct t_bind *req, struct t_bind *ret)
{
	struct T_bind_req	*bind_req;
	struct T_bind_ack	*bind_ack;
	int 			bindsz;
	vnode_t 		*vp;
	file_t 			*fp;
	char 			*buf;
	struct strioctl 	strioc;
	int			retval;
	int			error;

	error = 0;
	retval = 0;
	fp = tiptr->fp;
	vp = fp->f_vnode;

	/*
	 * send the ioctl request and wait
	 * for a reply.
	 */
	bindsz = (req == NULL) ? 0 : req->addr.len;
	bindsz = MAX(bindsz, tiptr->tp_info.addr);
	bindsz += MAX(TBINDREQSZ, TBINDACKSZ);
	buf = kmem_alloc(bindsz, KM_SLEEP);

	/* LINTED pointer alignment */
	bind_req = (struct T_bind_req *)buf;
	bind_req->PRIM_type = T_BIND_REQ;
	bind_req->ADDR_length = (req == NULL ? 0 : req->addr.len);
	bind_req->ADDR_offset = TBINDREQSZ;
	bind_req->CONIND_number = (req == NULL ? 0 : req->qlen);

	if (bind_req->ADDR_length)
		bcopy(req->addr.buf, buf + bind_req->ADDR_offset,
		    bind_req->ADDR_length);

	strioc.ic_cmd = TI_BIND;
	strioc.ic_timout = 0;
	strioc.ic_dp = buf;
	strioc.ic_len = (int)TBINDREQSZ + bind_req->ADDR_length;

	error = strdoioctl(vp->v_stream, &strioc, NULL, FNATIVE, K_TO_K,
	    NULL, CRED(), &retval);
	if (error)
		goto badbind;
	if (retval) {
		if ((retval & 0xff) == TSYSERR)
			error = (retval >> 8) & 0xff;
		else
			error = t_tlitosyserr(retval & 0xff);
		goto badbind;
	}

	/* LINTED pointer alignment */
	bind_ack = (struct T_bind_ack *)strioc.ic_dp;
	if (strioc.ic_len < TBINDACKSZ || bind_ack->ADDR_length == 0) {
		error = EIO;
		goto badbind;
	}

	/*
	 * copy bind data into users buffer
	 */
	if (ret) {
		if (ret->addr.maxlen > bind_ack->ADDR_length)
			ret->addr.len = bind_ack->ADDR_length;
		else
			ret->addr.len = ret->addr.maxlen;

		bcopy(buf + bind_ack->ADDR_offset, ret->addr.buf,
		    ret->addr.len);

		ret->qlen = bind_ack->CONIND_number;
	}

badbind:
	kmem_free(buf, bindsz);
	return (error);
}
