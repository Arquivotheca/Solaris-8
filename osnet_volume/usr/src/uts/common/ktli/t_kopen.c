/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)t_kopen.c	1.23	97/04/29 SMI"	/* SVr4.0 1.6  */

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
 * Kernel TLI-like function to initialize a transport
 * endpoint using the protocol specified.
 *
 * Returns:
 * 	0 on success and "tiptr" is set to a valid transport pointer,
 * 	else a positive error code.
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
#include <sys/kmem.h>
#include <sys/cmn_err.h>

static t_scalar_t _t_setsize(t_scalar_t);

int
t_kopen(file_t *fp, dev_t rdev, int flags, TIUSER **tiptr, cred_t *cr)
{
	int			madefp = 0;
	struct T_info_ack	inforeq;
	int			retval;
	vnode_t			*vp;
	struct strioctl		strioc;
	int			error;
	TIUSER			*ntiptr;
	int			rtries = 0;

	KTLILOG(2, "t_kopen: fp %x, ", fp);
	KTLILOG(2, "rdev %x, ", rdev);
	KTLILOG(2, "flags %x\n", flags);

	*tiptr = NULL;
	error = 0;
	retval = 0;
	if (fp == NULL) {
		if (rdev == 0 || rdev == NODEV) {
			KTLILOG(1, "t_kopen: null device\n", 0);
			return (EINVAL);
		}

		/*
		 * allocate a file pointer, but
		 * no file descripter.
		 */
		if ((error = falloc(NULL, flags, &fp, NULL)) != 0) {
			KTLILOG(1, "t_kopen: falloc: %d\n", error);
			return (error);
		}
		vp = makespecvp(rdev, VCHR);

		/*
		 * this will call the streams open for us.
		 * Want to retry if error is EAGAIN, the streams open routine
		 * might fail due to temporarely out of memory.
		 */
		do {
			if ((error = VOP_OPEN(&vp, flags, cr)) == EAGAIN) {
				(void) delay(hz);
			}
		} while (error == EAGAIN && ++rtries < 5);

		if (error) {
			KTLILOG(1, "t_kopen: VOP_OPEN: %d\n", error);
			unfalloc(fp);
			VN_RELE(vp);
			return (error);
		}
		/*
		 * fp is completely initialized so drop the write lock.
		 * I actually don't need any locking on fp in here since
		 * there is no fd pointing at it.  However, since I could
		 * call closef if there is an error and closef requires
		 * the fp read locked, I will acquire the read lock here
		 * and make sure I release it before I leave this routine.
		 */
		fp->f_vnode = vp;
		mutex_exit(&fp->f_tlock);

		madefp = 1;
	} else {
		vp = fp->f_vnode;
	}

	if (vp->v_stream == NULL) {
		if (madefp)
			(void) closef(fp);
		KTLILOG(1, "t_kopen: not a streams device\n", 0);
		return (ENOSTR);
	}

	/*
	 * allocate a new transport structure
	 */
	ntiptr = kmem_alloc(TIUSERSZ, KM_SLEEP);
	ntiptr->fp = fp;
	ntiptr->flags = madefp ? MADE_FP : 0;

	KTLILOG(2, "t_kopen: vp %x, ", vp);
	KTLILOG(2, "stp %x\n", vp->v_stream);

	/*
	 * see if TIMOD is already pushed
	 */
	error = strioctl(vp, I_FIND, (intptr_t)"timod", 0, K_TO_K, cr, &retval);
	if (error) {
		kmem_free(ntiptr, TIUSERSZ);
		if (madefp)
			(void) closef(fp);
		KTLILOG(1, "t_kopen: strioctl(I_FIND, timod): %d\n", error);
		return (error);
	}

	if (retval == 0) {
tryagain:
		error = strioctl(vp, I_PUSH, (intptr_t)"timod", 0, K_TO_K, cr,
		    &retval);
		if (error) {
			switch (error) {
			case ENOSPC:
			case EAGAIN:
			case ENOSR:
				/*
				 * This probably means the master file
				 * should be tuned.
				 */
				cmn_err(CE_WARN,
				"t_kopen: I_PUSH of timod failed, error %d\n",
				    error);
				(void) delay(hz);
				error = 0;
				goto tryagain;

			default:
				kmem_free(ntiptr, TIUSERSZ);
				if (madefp)
					(void) closef(fp);
				KTLILOG(1, "t_kopen: I_PUSH (timod): %d",
				    error);
				return (error);
			}
		}
	}

	inforeq.PRIM_type = T_INFO_REQ;
	strioc.ic_cmd = TI_GETINFO;
	strioc.ic_timout = 0;
	strioc.ic_dp = (char *)&inforeq;
	strioc.ic_len = (int)sizeof (struct T_info_req);

	error = strdoioctl(vp->v_stream, &strioc, NULL, FNATIVE, K_TO_K,
	    NULL, cr, &retval);
	if (error) {
		kmem_free(ntiptr, TIUSERSZ);
		if (madefp)
			(void) closef(fp);
		KTLILOG(1, "t_kopen: strdoioctl(T_INFO_REQ): %d\n", error);
		return (error);
	}

	if (retval) {
		if ((retval & 0xff) == TSYSERR)
			error = (retval >> 8) & 0xff;
		else
			error = t_tlitosyserr(retval & 0xff);
		kmem_free(ntiptr, TIUSERSZ);
		if (madefp)
			(void) closef(fp);
		KTLILOG(1, "t_kopen: strdoioctl(T_INFO_REQ): retval: 0x%x\n",
		    retval);
		return (error);
	}

	if (strioc.ic_len != sizeof (struct T_info_ack)) {
		kmem_free(ntiptr, TIUSERSZ);
		if (madefp)
			(void) closef(fp);
		KTLILOG(1,
		"t_kopen: strioc.ic_len != sizeof (struct T_info_ack): %d\n",
		    strioc.ic_len);
		return (EPROTO);
	}

	ntiptr->tp_info.addr = _t_setsize(inforeq.ADDR_size);
	ntiptr->tp_info.options = _t_setsize(inforeq.OPT_size);
	ntiptr->tp_info.tsdu = _t_setsize(inforeq.TSDU_size);
	ntiptr->tp_info.etsdu = _t_setsize(inforeq.ETSDU_size);
	ntiptr->tp_info.connect = _t_setsize(inforeq.CDATA_size);
	ntiptr->tp_info.discon = _t_setsize(inforeq.DDATA_size);
	ntiptr->tp_info.servtype = inforeq.SERV_type;

	*tiptr = ntiptr;

	return (0);
}

#define	DEFSIZE	128

static t_scalar_t
_t_setsize(t_scalar_t infosize)
{
	switch (infosize) {
	case -1:
		return (DEFSIZE);
	case -2:
		return (0);
	default:
		return (infosize);
	}
}
