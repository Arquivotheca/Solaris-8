/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)t_optmgmt.c	1.24	98/05/24 SMI"
		/* SVr4.0 1.3.4.1	*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <rpc/trace.h>
#include <errno.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <syslog.h>
#include <stropts.h>
#include "timt.h"
#include "tx.h"

/*
 * The following is based on XTI standard.
 */
#define	ALIGN_XTI_opthdr_size	(sizeof (t_uscalar_t))

#define	ROUNDUP_XTI_opthdr(p)	(((p) +\
		(ALIGN_XTI_opthdr_size-1)) & ~(ALIGN_XTI_opthdr_size-1))
#define	ISALIGNED_XTI_opthdr(p)	\
	(((u_long)(p) & (ALIGN_XTI_opthdr_size - 1)) == 0)

int
_tx_optmgmt(
	int fd,
	const struct t_optmgmt *req,
	struct t_optmgmt *ret,
	int api_semantics
)
{
	int size, sv_errno;
	struct strbuf ctlbuf;
	struct T_optmgmt_req *optreq;
	struct T_optmgmt_ack *optack;
	struct _ti_user *tiptr;
	sigset_t procmask;
	int didalloc, retlen;
	struct t_opthdr *opt, *next_opt;
	struct t_opthdr *opt_start, *opt_end;
	t_uscalar_t first_opt_level;
	t_scalar_t optlen;

	trace2(TR_t_optmgmt, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_optmgmt, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * We need to block all signals for the process later.
	 * We do it early to avoid doing it twice.
	 *
	 * We block all signals during the TI_OPTMGMT operation
	 * as option change being done could potentially be a
	 * non-idempotent operation.
	 */
	MUTEX_LOCK_PROCMASK(&tiptr->ti_lock, procmask);

	/*
	 * Acquire buf for use in sending/receiving of the message.
	 * Note: assumes (correctly) that ti_ctlsize is large enough
	 * to hold sizeof (struct T_bind_req)
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
		trace2(TR_t_optmgmt, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * effective option length in local variable "optlen"
	 * Note: can change for XTI for T_ALLOPT. XTI spec states
	 * that options after the T_ALLOPT option are to be ignored
	 * therefore we trncate the option buffer there and modify
	 * the effective length accordingly later.
	 */
	optlen = req->opt.len;

	if (_T_IS_XTI(api_semantics) && (optlen > 0)) {
		/*
		 * Verify integrity of option buffer according to
		 * XTI t_optmgmt() semantics.
		 */

		if (req->opt.buf == NULL ||
		    optlen < (t_scalar_t)sizeof (struct t_opthdr)) {
			/* option buffer should atleast have an t_opthdr */
			t_errno = TBADOPT;
			goto err_out;
		}

		opt_start = (struct t_opthdr *)req->opt.buf;

		/*
		 * XXX We interpret that an option has to start on an
		 * aligned buffer boundary. This is not very explcit in
		 * XTI spec in text but the picture in Section 6.2 shows
		 * "opt.buf" at start of buffer and in combination with
		 * text can be construed to be restricting it to start
		 * on an aligned boundary. [Whether similar restriction
		 * applies to output buffer "ret->opt.buf" is an "interesting
		 * question" but we ignore it for now as that is the problem
		 * for the application not our implementation which will
		 * does not enforce any alignment requirement.]
		 *
		 * If start of buffer is not aligned, we signal an error.
		 */
		if (! (ISALIGNED_XTI_opthdr(opt_start))) {
			t_errno = TBADOPT;
			goto err_out;
		}

		opt_end = (struct t_opthdr *)((char *)opt_start +
						optlen);

		/*
		 * Make sure we have enough in the message to dereference
		 * the option header.
		 */
		if ((u_char *)opt_start + sizeof (struct t_opthdr)
		    > (u_char *)opt_end) {
			t_errno = TBADOPT;
			goto err_out;
		}
		/*
		 * If there are multiple options, they all have to be
		 * the same level (so says XTI semantics).
		 */
		first_opt_level = opt_start->level;

		for (opt = opt_start; opt < opt_end; opt = next_opt) {
			/*
			 * Make sure we have enough in the message to
			 * dereference the option header.
			 */
			if ((u_char *)opt_start + sizeof (struct t_opthdr)
			    > (u_char *)opt_end) {
				t_errno = TBADOPT;
				goto err_out;
			}
			/*
			 * We now compute pointer to next option in buffer
			 * 'next_opt' the next_opt computation above below
			 * 'opt->len' initialized by application which cannot
			 * be trusted. The usual value too large will be
			 * captured by the loop termination condition above.
			 * We check for the following which it will miss.
			 *	(1)pointer space wraparound arithmetic overflow
			 *	(2)last option in buffer with 'opt->len' being
			 *	  too large
			 *	(only reason 'next_opt' should equal or exceed
			 *	'opt_end' for last option is roundup unless
			 *	length is too-large/invalid)
			 *	(3) we also enforce the XTI restriction that
			 *	   all options in the buffer have to be the
			 *	   same level.
			 */
			next_opt = (struct t_opthdr *)((u_char *)opt +
			    ROUNDUP_XTI_opthdr(opt->len));

			if ((u_char *)next_opt < (u_char *)opt || /* (1) */
			    ((next_opt >= opt_end) &&
				(((u_char *)next_opt - (u_char *)opt_end) >=
				    ALIGN_XTI_opthdr_size)) || /* (2) */
			    (opt->level != first_opt_level)) { /* (3) */
				t_errno = TBADOPT;
				goto err_out;
			}

			/*
			 * XTI semantics: options in the buffer after
			 * the T_ALLOPT option can be ignored
			 */
			if (opt->name == T_ALLOPT) {
				if (next_opt < opt_end) {
					/*
					 * there are options following, ignore
					 * them and truncate input
					 */
					optlen = (t_scalar_t)((u_char *)
					    next_opt - (u_char *) opt_start);
					opt_end = next_opt;
				}
			}
		}
	}

	optreq = (struct T_optmgmt_req *)ctlbuf.buf;
	if (_T_IS_XTI(api_semantics))
		optreq->PRIM_type = T_OPTMGMT_REQ;
	else
		optreq->PRIM_type = T_SVR4_OPTMGMT_REQ;

	optreq->OPT_length = optlen;
	optreq->OPT_offset = 0;
	optreq->MGMT_flags = req->flags;
	size = (int)sizeof (struct T_optmgmt_req);

	if (optlen) {
		if (_t_aligned_copy(&ctlbuf, optlen, size,
		    req->opt.buf, &optreq->OPT_offset) < 0) {
			/*
			 * Aligned copy will overflow buffer allocated
			 * based on maximum transport option size information
			 */
			t_errno = TBADOPT;
			goto err_out;
		}
		size = optreq->OPT_offset + optreq->OPT_length;
	}

	if (_t_do_ioctl(fd, ctlbuf.buf, size, TI_OPTMGMT, &retlen) < 0)
		goto err_out;

	if (retlen < (int)sizeof (struct T_optmgmt_ack)) {
		t_errno = TSYSERR;
		errno = EIO;
		goto err_out;
	}

	optack = (struct T_optmgmt_ack *)ctlbuf.buf;

	if (_T_IS_TLI(api_semantics) || ret->opt.maxlen > 0) {
		if (TLEN_GT_NLEN(optack->OPT_length, ret->opt.maxlen)) {
			t_errno = TBUFOVFLW;
			goto err_out;
		}
		(void) memcpy(ret->opt.buf,
		    (char *)(ctlbuf.buf + optack->OPT_offset),
		    (unsigned int) optack->OPT_length);
		ret->opt.len = optack->OPT_length;
	}

	/*
	 * Note: TPI is not clear about what really is carries in the
	 * T_OPTMGMT_ACK MGMT_flags fields. For T_OPTMGMT_ACK in response
	 * to T_SVR4_OPTMGMT_REQ, the Internet protocols in Solaris 2.X return
	 * the result code only (T_SUCCESS). For T_OPTMGMT_ACK in response
	 * to T_OPTMGMT_REQ, currently "worst status" code required for
	 * XTI is carried from the set of options OR'd with request flag.
	 * (This can change in future and "worst status" computation done
	 * with a scan in this routine.
	 *
	 * Note: Even for T_OPTMGMT_ACK is response to T_SVR4_OPTMGMT_REQ,
	 * removing request flag should be OK though it will not be set.
	 */
	ret->flags = optack->MGMT_flags & ~req->flags;

	/*
	 * NOTE:
	 * There is no real change of state in state table for option
	 * management. The state change macro is used below only for its
	 * debugging and logging capabilities.
	 * The TLI "(mis)feature" (option management only in T_IDLE state)
	 * has been deprecated in XTI and our state table reflect updated for
	 * both TLI and XTI to reflect that.
	 * TLI semantics can be enforced by the transport providers that
	 * desire it at TPI level.
	 * There is no need to enforce this in the library since
	 * sane transport providers that do allow it (e.g TCP and it *needs*
	 * to allow it) should be allowed to work fine.
	 * The only transport providers that return TOUTSTATE for TLI
	 * t_optmgmt() are the drivers used for conformance testing to the
	 * broken TLI standard.
	 * These are /dev/{ticots,ticotsord,ticlts} used by the Sparc ABI test
	 * suite. Others are /dev/{tivc,tidg} used by the SVVS test suite.
	 */

	_T_TX_NEXTSTATE(T_OPTMGMT, tiptr,
	    "t_optmgmt: invalid state event T_OPTMGMT");

	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
	trace2(TR_t_optmgmt, 1, fd);
	return (0);
	/* NOTREACHED */

err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_PROCMASK(&tiptr->ti_lock, procmask);
	trace2(TR_t_optmgmt, 1, fd);
	errno = sv_errno;
	return (-1);
}
