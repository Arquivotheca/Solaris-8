/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)t_kutil.c	1.28	99/10/21 SMI"	/* SVr4.0 1.5  */

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
 * Contains the following utility functions:
 * 	tli_send:
 * 	tli_recv:
 * 	get_ok_ack:
 *
 * Returns:
 * 	See individual functions.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/stream.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/tiuser.h>
#include <sys/t_kuser.h>
#include <inet/common.h>
#include <inet/mi.h>
#include <netinet/ip6.h>
#include <inet/ip.h>

extern int getiocseqno(void);

int
tli_send(TIUSER *tiptr, mblk_t *bp, int fmode)
{
	vnode_t	*vp;
	int	error;

	vp = tiptr->fp->f_vnode;

	/*
	 * Send data honoring flow control and errors
	 */
	error = kstrputmsg(vp, bp, NULL, 0, 0, MSG_BAND | MSG_HOLDSIG, fmode);
	return (error);
}

int
tli_recv(TIUSER *tiptr, mblk_t **bp, int fmode)
{
	vnode_t		*vp;
	int		error;
	u_char 		pri;
	int 		pflag;
	rval_t		rval;
	clock_t		timout;

	vp = tiptr->fp->f_vnode;
	if (fmode & (FNDELAY|FNONBLOCK))
		timout = 0;
	else
		timout = -1;

	pflag = MSG_ANY;
	pri = 0;
	*bp = NULL;
	error = kstrgetmsg(vp, bp, NULL, &pri, &pflag, timout, &rval);
	if (error == ETIME)
		error = EAGAIN;

	return (error);
}

int
get_ok_ack(TIUSER *tiptr, int type, int fmode)
{
	int			msgsz;
	union T_primitives	*pptr;
	mblk_t			*bp;
	int			error;

	error = 0;

	/*
	 * wait for ack
	 */
	bp = NULL;
	if ((error = tli_recv(tiptr, &bp, fmode)) != 0)
		return (error);

	if ((msgsz = (int)(bp->b_wptr - bp->b_rptr)) < sizeof (int)) {
		freemsg(bp);
		return (EPROTO);
	}

	pptr = (union T_primitives *)bp->b_rptr;
	switch (pptr->type) {
	case T_OK_ACK:
		if (msgsz < TOKACKSZ || pptr->ok_ack.CORRECT_prim != type)
			error = EPROTO;
		break;

	case T_ERROR_ACK:
		if (msgsz < TERRORACKSZ) {
			error = EPROTO;
			break;
		}

		if (pptr->error_ack.TLI_error == TSYSERR)
			error = pptr->error_ack.UNIX_error;
		else
			error = t_tlitosyserr(pptr->error_ack.TLI_error);
		break;

	default:
		error = EPROTO;
		break;
	}
	freemsg(bp);
	return (error);
}

/*
 * Translate a TLI error into a system error as best we can.
 */
static ushort tli_errs[] = {
	0,		/* no error	*/
	EADDRNOTAVAIL,  /* TBADADDR	*/
	ENOPROTOOPT,    /* TBADOPT	*/
	EACCES,		/* TACCES	*/
	EBADF,		/* TBADF	*/
	EADDRNOTAVAIL,	/* TNOADDR	*/
	EPROTO,		/* TOUTSTATE	*/
	EPROTO,		/* TBADSEQ	*/
	0,		/* TSYSERR - will never get */
	EPROTO,		/* TLOOK - should never be sent by transport */
	EMSGSIZE,	/* TBADDATA	*/
	EMSGSIZE,	/* TBUFOVFLW	*/
	EPROTO,		/* TFLOW	*/
	EWOULDBLOCK,    /* TNODATA	*/
	EPROTO,		/* TNODIS	*/
	EPROTO,		/* TNOUDERR	*/
	EINVAL,		/* TBADFLAG	*/
	EPROTO,		/* TNOREL	*/
	EOPNOTSUPP,	/* TNOTSUPPORT	*/
	EPROTO,		/* TSTATECHNG	*/
};

int
t_tlitosyserr(int terr)
{
	if (terr < 0 || terr > (sizeof (tli_errs) / sizeof (ushort)))
		return (EPROTO);
	return (tli_errs[terr]);
}

/*
 * Notify transport that we are having trouble with this connection.
 * If transport is TCP/IP, IP should delete the IRE and start over.
 */
void
t_kadvise(TIUSER *tiptr, u_char *addr, int addr_len)
{
	file_t		*fp;
	vnode_t		*vp;
	struct iocblk	*iocp;
	ipid_t		*ipid;
	mblk_t		*mp;

	fp = tiptr->fp;
	vp = fp->f_vnode;

	mp = mkiocb(IP_IOCTL);
	if (!mp)
		return;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_count = sizeof (ipid_t) + addr_len;

	mp->b_cont = allocb(iocp->ioc_count, BPRI_HI);
	if (!mp->b_cont) {
		freeb(mp);
		return;
	}

	ipid = (ipid_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += iocp->ioc_count;

	bzero(ipid, sizeof (*ipid));
	ipid->ipid_cmd = IP_IOC_IRE_DELETE_NO_REPLY;
	ipid->ipid_ire_type = IRE_CACHE;
	ipid->ipid_addr_offset = sizeof (ipid_t);
	ipid->ipid_addr_length = addr_len;

	bcopy(addr, &ipid[1], addr_len);

	/* Ignore flow control, signals and errors */
	(void) kstrputmsg(vp, mp, NULL, 0, 0,
	    MSG_BAND | MSG_IGNFLOW | MSG_HOLDSIG | MSG_IGNERROR, 0);
}

#ifdef KTLIDEBUG
int ktlilog = 0;

/*
 * Kernel level debugging aid. The global variable "ktlilog" is a bit
 * mask which allows various types of debugging messages to be printed
 * out.
 *
 *	ktlilog & 1 	will cause actual failures to be printed.
 *	ktlilog & 2	will cause informational messages to be
 *			printed.
 */
int
ktli_log(int level, char *str, int a1)
{
	if (level & ktlilog)
		printf(str, a1);
}
#endif
