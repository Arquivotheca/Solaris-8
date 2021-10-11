/*	Copyright (c) 1998 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_rcvv.c	1.2	98/04/19 SMI"

/*
 * t_rcv.c and t_rcvv.c are very similar and contain common code.
 * Any changes to either of them should be reviewed to see whether they
 * are applicable to the other file.
 */
#include <stdlib.h>
#include <rpc/trace.h>
#include <errno.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <syslog.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"

int
_tx_rcvv(int fd, struct t_iovec *tiov, unsigned int tiovcount,  int *flags,
    int api_semantics)
{
	struct strbuf ctlbuf, databuf;
	int retval, flg = 0;
	int msglen;
	union T_primitives *pptr;
	struct _ti_user *tiptr;
	sigset_t mask;
	int sv_errno;
	int didalloc;
	unsigned int nbytes;
	char *dataptr;

	trace5(TR_t_rcvv, 0, fd, tiov, tiovcount, flags);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		return (-1);
	}

	assert(api_semantics == TX_XTI_XNS5_API);

	if (tiovcount == 0 || tiovcount > T_IOV_MAX) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		return (-1);
	}

	if (! (tiptr->ti_state == T_DATAXFER ||
		tiptr->ti_state == T_OUTREL)) {
		t_errno = TOUTSTATE;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		return (-1);
	}

	/*
	 * Check in lookbuf for stuff
	 */
	if (tiptr->ti_lookcnt > 0) {
		/*
		 * Implied preference rules give priority to
		 * T_DISCON_IND over T_ORDREL_IND. Also certain errors like
		 * data received after T_ORDREL_IND or a duplicate T_ORDREL_IND
		 * after a T_ORDRELING have priority over TLOOK.
		 * This manifests in following code behavior.
		 *
		 * (1)  If something in lookbuf then check
		 *	the stream head also. This may result
		 *	in retuning a TLOOK error but only if there are
		 *	  - message at stream head but look buffer
		 *	    has a T_DISCON_IND event.
		 *	  - no messages are on the stream head
		 *
		 * (2)  If there are messages on the stream head and
		 *	all of them are T_ORDREL_IND(i.e. no message in
		 *	look buffer is T_DISCON_IND), there
		 *	could be data on stream head to be picked up and
		 *	we work on the stream head and not return TLOOK.
		 *	We remove the event on the stream head and queue it.
		 *
		 */
		do {
			retval = _ioctl(fd, I_NREAD, &msglen);
		} while (retval < 0 && errno == EINTR);

		if (retval < 0) {
			sv_errno = errno;
			t_errno = TSYSERR;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
			errno = sv_errno;
			return (-1);
		}

		if (retval > 0) {
			/*
			 * If any T_DISCON_IND event in look buffer
			 * list then return TLOOK. Else continue
			 * processing as what could be on the stream
			 * head might be a possible T_DISCON_IND (which
			 * would have priority over the T_ORDREL_INDs
			 * on the look buffer.)
			 */
			struct _ti_lookbufs *tlbs;

			tlbs = &tiptr->ti_lookbufs;
			do {
				if (*((t_scalar_t *)tlbs->tl_lookcbuf)
				    == T_DISCON_IND) {
					t_errno = TLOOK;
					MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock,
						mask);
					trace5(TR_t_rcvv, 1, fd, tiov,
					    tiovcount, flags);
					return (-1);
				}
			} while ((tlbs = tlbs->tl_next) != NULL);

		} else {	/* retval == 0 */
			/*
			 * Nothing on stream head so whatever in
			 * look buffer has nothing that might override
			 * it.
			 */
			t_errno = TLOOK;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
			return (-1);
		}
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		errno = sv_errno;
		return (-1);
	}

	nbytes = _t_bytecount_upto_intmax(tiov, tiovcount);
	dataptr = NULL;
	if (nbytes != 0 && ((dataptr = malloc(nbytes)) == NULL)) {
		sv_errno = errno;
		t_errno = TSYSERR;
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
		errno = sv_errno;
		return (-1);
	}

	databuf.maxlen = (int)nbytes;
	databuf.len = 0;
	databuf.buf = dataptr;

	*flags = 0;

	/*
	 * This is a call that may block indefinitely so we drop the
	 * lock and allow signals in MT case here and reacquire it.
	 * Error case should roll back state changes done above
	 * (happens to be no state change here)
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	if ((retval = getmsg(fd, &ctlbuf, &databuf, &flg)) < 0) {
		if (errno == EAGAIN)
			t_errno = TNODATA;
		else
			t_errno = TSYSERR;
		sv_errno = errno;
		MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);
		errno = sv_errno;
		goto err_out;
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	assert((retval & MORECTL) == 0); /* MORECTL should not be on */

	if (databuf.len == -1) databuf.len = 0;

	if (ctlbuf.len > 0) {
		if (ctlbuf.len < (int)sizeof (t_scalar_t)) {
			t_errno = TSYSERR;
			errno = EPROTO;
			goto err_out;
		}

		pptr = (union T_primitives *)ctlbuf.buf;

		switch (pptr->type) {

		case T_EXDATA_IND:
			*flags |= T_EXPEDITED;
			if (retval > 0)
				tiptr->ti_flags |= EXPEDITED;
			/* FALLTHROUGH */
		case T_DATA_IND:
			/*
			 * Uses the fact T_DATA_IND and T_EXDATA_IND
			 * are same in size
			 */
			if ((ctlbuf.len < (int)sizeof (struct T_data_ind)) ||
			    (tiptr->ti_lookcnt > 0)) {
				/*
				 * ti_lookcnt > 0 implies data
				 * received after T_DISCON_IND or
				 * T_ORDREL_IND hence error
				 */
				t_errno = TSYSERR;
				errno = EPROTO;
				goto err_out;
			}

			if ((pptr->data_ind.MORE_flag) || retval)
				*flags |= T_MORE;
			if ((pptr->data_ind.MORE_flag) && retval)
				tiptr->ti_flags |= MORE;
			/*
			 * No real state change on T_RCV event (noop)
			 *
			 * We invoke the macro only for error logging
			 * part of its capabilities when in a bad state.
			 */
			_T_TX_NEXTSTATE(T_RCV, tiptr,
					"t_rcvv: invalid state event T_RCV");
			if (didalloc)
				free(ctlbuf.buf);
			else
				tiptr->ti_ctlbuf = ctlbuf.buf;
			_t_scatter(&databuf, tiov, tiovcount);
			if (dataptr != NULL)
				free(dataptr);
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace5(TR_t_rcvv, 0, fd, tiov, tiovcount, flags);
			return (databuf.len);

		case T_ORDREL_IND:
			if (tiptr->ti_lookcnt > 0) {
				/*
				 * ti_lookcnt > 0 implies T_ORDREL_IND
				 * received after T_DISCON_IND or
				 * another T_ORDREL_IND hence error.
				 */
				t_errno = TSYSERR;
				errno = EPROTO;
				goto err_out;
			}
			/* FALLTHROUGH */
		case T_DISCON_IND:
			/*
			 * Post event (T_ORDREL_IND/T_DISCON_IND) to
			 * the lookbuffer list.
			 */

			if (_t_register_lookevent(tiptr, databuf.buf,
					databuf.len,
					ctlbuf.buf, ctlbuf.len) < 0) {
				t_errno = TSYSERR;
				errno = ENOMEM;
				goto err_out;
			}
			/*
			 * We know that T_DISCON_IND is stored in
			 * last look buffer. If there is more data
			 * that follows, we try to append it to
			 * the same look buffer
			 */
			if (retval & MOREDATA) {
				ctlbuf.maxlen = 0; /* XXX why ? */
				ctlbuf.len = 0;

				/*
				 * XXX Will break (-ve maxlen) for
				 * transport provider with unbounded
				 * T_DISCON_IND data part (-1).
				 */
				databuf.maxlen =
					tiptr->ti_rcvsize - databuf.len;

				databuf.len = 0;
				databuf.buf =
					tiptr->ti_lookbufs.tl_lookdbuf +
					tiptr->ti_lookbufs.tl_lookdlen;
				*flags = 0;

				/*
				 * Since MOREDATA was set, we assume
				 * that this getmsg will not block
				 * indefinitely
				 */
				do {
					retval = getmsg(fd, &ctlbuf,
							&databuf, &flg);
				} while (retval < 0 && errno == EINTR);

				if (retval < 0) {
					t_errno = TSYSERR;
					goto err_out;
				}
				if (databuf.len == -1) databuf.len = 0;
				if (retval > 0) {
					/* MORECTL should not be on */
					assert((retval & MORECTL) == 0);
					/*
					 * XXX - Why ?
					 * No support for unbounded data
					 * on T_DISCON_IND ?
					 */
					t_errno = TSYSERR;
					errno = EPROTO;
					goto err_out;
				}
				tiptr->ti_lookbufs.tl_lookdlen +=
					databuf.len;
			}

			t_errno = TLOOK;
			goto err_out;

		default:
			break;
		}

		t_errno = TSYSERR;
		errno = EPROTO;
		goto err_out;

	} else {		/* else for "if (ctlbuf.len > 0)" */
		if (!retval && (tiptr->ti_flags & MORE)) {
			*flags |= T_MORE;
			tiptr->ti_flags &= ~MORE;
		}
		if (retval & MOREDATA)
			*flags |= T_MORE;

		/*
		 * If inside an ETSDU, set expedited flag and turn
		 * of internal version when reach end of "ETIDU".
		 */
		if (tiptr->ti_flags & EXPEDITED) {
			*flags |= T_EXPEDITED;
			if (!retval)
				tiptr->ti_flags &= ~EXPEDITED;
		}

		/*
		 * No real state change on T_RCV events (It is a NOOP)
		 *
		 * We invoke the macro only for error logging
		 * part of its capabilities when in a bad state.
		 */
		_T_TX_NEXTSTATE(T_RCV, tiptr,
			"t_rcvv: state invalid T_RCV event");
		if (didalloc)
			free(ctlbuf.buf);
		else
			tiptr->ti_ctlbuf = ctlbuf.buf;
		_t_scatter(&databuf, tiov, tiovcount);
		if (dataptr != NULL)
			free(dataptr);
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace5(TR_t_rcvv, 0, fd, tiov, tiovcount, flags);
		return (databuf.len);
	}
	/* NOTREACHED */

err_out:
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	if (dataptr != NULL)
		free(dataptr);
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);

	trace5(TR_t_rcvv, 1, fd, tiov, tiovcount, flags);
	errno = sv_errno;
	return (-1);
}
