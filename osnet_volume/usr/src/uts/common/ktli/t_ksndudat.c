/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)t_ksndudat.c	1.19	97/04/29 SMI"	/* SVr4.0 1.5	*/

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
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 */

/*
 * TLI-like function to send datagrams over a specified
 * transport endpoint.
 *
 * Returns:
 * 	0 on success or positive error code.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <sys/debug.h>


int
t_ksndudata(TIUSER *tiptr, struct t_kunitdata *unitdata, frtn_t *frtn)
{
	int			msgsz;
	file_t			*fp;
	mblk_t			*bp;
	mblk_t			*dbp;
	struct T_unitdata_req	*udreq;
	int			error;
	int			flag;

	error = 0;
	fp = tiptr->fp;
	msgsz = unitdata->udata.len;

	/*
	 * See if Class 0 is required
	 */
	if (frtn != NULL) {
		ASSERT(unitdata->udata.udata_mp == NULL);
		ASSERT(unitdata->udata.buf != NULL);
		/*
		 * user has supplied their own buffer, all we have to
		 * do is allocate a class 0 streams buffer and set it
		 * up.
		 */
		if ((dbp = (mblk_t *)esballoc((uchar_t *)unitdata->udata.buf,
		    (size_t)msgsz, BPRI_LO, frtn)) == NULL)
			return (ENOSR);

		dbp->b_datap->db_type = M_DATA;
		KTLILOG(2, "t_ksndudata: bp %x, ", dbp);
		KTLILOG(2, "len %d, ", msgsz);
		KTLILOG(2, "free func %x\n", frtn->free_func);

	} else if (unitdata->udata.buf) {
		ASSERT(unitdata->udata.udata_mp == NULL);
		while (!(dbp = allocb(msgsz, BPRI_LO)))
			if (strwaitbuf((size_t)msgsz, BPRI_LO))
				return (ENOSR);

		bcopy(unitdata->udata.buf, dbp->b_wptr, unitdata->udata.len);
		dbp->b_datap->db_type = M_DATA;

	} else if (unitdata->udata.udata_mp) {
		ASSERT(unitdata->udata.buf == NULL);
		/*
		 * user has done it all
		 */
		dbp = unitdata->udata.udata_mp;
		goto gotdp;

	} else {
		/*
		 * zero length message.
		 */
		dbp = NULL;
	}

	if (dbp)
		dbp->b_wptr += msgsz;		/* on behalf of the user */

	/*
	 * Okay, put the control part in
	 */
gotdp:
	msgsz = (int)TUNITDATAREQSZ;
	while (!(bp = allocb(msgsz + unitdata->addr.len + unitdata->opt.len,
	    BPRI_LO))) {
		if (strwaitbuf(msgsz + unitdata->addr.len + unitdata->opt.len,
		    BPRI_LO)) {
			if (dbp && (dbp != unitdata->udata.udata_mp))
				freeb(dbp);
			return (ENOSR);
		}
	}

	/* LINTED pointer alignment */
	udreq = (struct T_unitdata_req *)bp->b_wptr;
	udreq->PRIM_type = T_UNITDATA_REQ;
	udreq->DEST_length = unitdata->addr.len;
	if (unitdata->addr.len) {
		bcopy(unitdata->addr.buf, bp->b_wptr + msgsz,
		    unitdata->addr.len);
		udreq->DEST_offset = (t_scalar_t)msgsz;
		msgsz += unitdata->addr.len;
	} else
		udreq->DEST_offset = 0;

	udreq->OPT_length = unitdata->opt.len;
	if (unitdata->opt.len) {
		bcopy(unitdata->opt.buf, bp->b_wptr + msgsz, unitdata->opt.len);
		udreq->OPT_offset = (t_scalar_t)msgsz;
		msgsz += unitdata->opt.len;
	} else
		udreq->OPT_offset = 0;

	bp->b_datap->db_type = M_PROTO;
	bp->b_wptr += msgsz;

	/*
	 * link the two.
	 */
	linkb(bp, dbp);

	/*
	 * Put it to the transport provider.
	 * tli_send() always consumes the message.
	 */
	flag = fp->f_flag;
	error = tli_send(tiptr, bp, flag);
	unitdata->udata.udata_mp = NULL;

	return (error);
}
