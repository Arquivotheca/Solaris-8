/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)strcalls.c	1.26	98/03/01 SMI"	/* SVr4 1.20	*/

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/fs/fifonode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/debug.h>

/*
 * STREAMS system calls.
 */

int getmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int *flagsp);
int putmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int flags);
int getpmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int *prip,
    int *flagsp);
int putpmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int pri,
    int flags);

static int msgio(int fdes, struct strbuf *ctl, struct strbuf *data, int *rval,
    int mode, unsigned char *prip, int *flagsp);

int
getmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int *flagsp)
{
	int error;
	int localflags;
	int realflags = 0;
	unsigned char pri = 0;
	int rv = 0;

	/*
	 * Convert between old flags (localflags) and new flags (realflags).
	 */
	if (copyin(flagsp, &localflags, sizeof (*flagsp)))
		return (set_errno(EFAULT));
	switch (localflags) {
	case 0:
		realflags = MSG_ANY;
		break;

	case RS_HIPRI:
		realflags = MSG_HIPRI;
		break;

	default:
		return (set_errno(EINVAL));
	}

	if ((error = msgio(fdes, ctl, data, &rv, FREAD, &pri,
	    &realflags)) == 0) {
		/*
		 * massage realflags based on localflags.
		 */
		if (realflags == MSG_HIPRI)
			localflags = RS_HIPRI;
		else
			localflags = 0;
		if (copyout(&localflags, flagsp, sizeof (*flagsp)))
			error = EFAULT;
	}
	if (error != 0)
		return (set_errno(error));
	return (rv);
}

int
putmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int flags)
{
	unsigned char pri = 0;
	int realflags;
	int error;
	int rv = 0;

	switch (flags) {
	case RS_HIPRI:
		realflags = MSG_HIPRI;
		break;
	case (RS_HIPRI|MSG_XPG4):
		realflags = MSG_HIPRI|MSG_XPG4;
		break;
	case MSG_XPG4:
		realflags = MSG_BAND|MSG_XPG4;
		break;
	case 0:
		realflags = MSG_BAND;
		break;

	default:
		return (set_errno(EINVAL));
	}
	error = msgio(fdes, ctl, data, &rv, FWRITE, &pri, &realflags);
	if (error != 0)
		return (set_errno(error));
	return (rv);
}


int
getpmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int *prip,
    int *flagsp)
{
	int error;
	int flags;
	int intpri;
	unsigned char pri;
	int rv = 0;

	if (copyin(flagsp, &flags, sizeof (flags)))
		return (set_errno(EFAULT));
	if (copyin(prip, &intpri, sizeof (intpri)))
		return (set_errno(EFAULT));
	if ((intpri > 255) || (intpri < 0))
		return (set_errno(EINVAL));
	pri = (unsigned char)intpri;
	error = msgio(fdes, ctl, data, &rv, FREAD, &pri, &flags);
	if (error != 0)
		return (set_errno(error));
	if (copyout(&flags, flagsp, sizeof (flags)))
		return (set_errno(EFAULT));
	intpri = (int)pri;
	if (copyout(&intpri, prip, sizeof (intpri)))
		return (set_errno(EFAULT));
	return (rv);
}

int
putpmsg(int fdes, struct strbuf *ctl, struct strbuf *data, int intpri,
    int flags)
{
	unsigned char pri;
	int rv = 0;
	int error;

	if ((intpri > 255) || (intpri < 0))
		return (set_errno(EINVAL));
	pri = (unsigned char)intpri;
	error = msgio(fdes, ctl, data, &rv, FWRITE, &pri, &flags);
	if (error != 0)
		return (set_errno(error));
	return (rv);
}

/*
 * Common code for getmsg and putmsg calls: check permissions,
 * copy in args, do preliminary setup, and switch to
 * appropriate stream routine.
 */
static int
msgio(int fdes, struct strbuf *ctl, struct strbuf *data, int *rval,
    int mode, unsigned char *prip, int *flagsp)
{
	file_t *fp;
	vnode_t *vp;
	struct strbuf msgctl, msgdata;
	int error;
	int flag;
	klwp_t *lwp = ttolwp(curthread);
	rval_t rv;

	if ((fp = getf(fdes)) == NULL)
		return (EBADF);
	if ((fp->f_flag & mode) == 0) {
		releasef(fdes);
		return (EBADF);
	}
	vp = fp->f_vnode;
	if (vp->v_type == VFIFO) {
		if (vp->v_stream) {
			/*
			 * must use sd_vnode, could be named pipe
			 */
			(void) fifo_vfastoff(vp->v_stream->sd_vnode);
		} else {
			releasef(fdes);
			return (ENOSTR);
		}
	} else if ((vp->v_type != VCHR && vp->v_type != VSOCK) ||
		    vp->v_stream == NULL) {
		releasef(fdes);
		return (ENOSTR);
	}
	if ((ctl != NULL) &&
	    copyin(ctl, &msgctl, sizeof (struct strbuf))) {
		releasef(fdes);
		return (EFAULT);
	}
	if ((data != NULL) &&
	    copyin(data, &msgdata, sizeof (struct strbuf))) {
		releasef(fdes);
		return (EFAULT);
	}

	if (mode == FREAD) {
		if (ctl == NULL)
			msgctl.maxlen = -1;
		if (data == NULL)
			msgdata.maxlen = -1;
		flag = fp->f_flag;
		rv.r_val1 = 0;
		if (vp->v_type == VSOCK) {
			error = sock_getmsg(vp, &msgctl, &msgdata, prip,
			    flagsp, flag, &rv);
		} else {
			error = strgetmsg(vp, &msgctl, &msgdata, prip,
			    flagsp, flag, &rv);
		}
		*rval = rv.r_val1;
		if (error != 0) {
			releasef(fdes);
			return (error);
		}
		if (lwp != NULL)
			lwp->lwp_ru.msgrcv++;
		if (((ctl != NULL) &&
		    copyout(&msgctl, ctl, sizeof (struct strbuf))) ||
		    ((data != NULL) &&
		    copyout(&msgdata, data, sizeof (struct strbuf)))) {
			releasef(fdes);
			return (EFAULT);
		}
		releasef(fdes);
		return (0);
	}

	/*
	 * FWRITE case
	 */
	if (ctl == NULL)
		msgctl.len = -1;
	if (data == NULL)
		msgdata.len = -1;
	flag = fp->f_flag;
	if (vp->v_type == VSOCK) {
		error = sock_putmsg(vp, &msgctl, &msgdata, *prip, *flagsp,
		    flag);
	} else {
		error = strputmsg(vp, &msgctl, &msgdata, *prip, *flagsp, flag);
	}
	releasef(fdes);
	if (error == 0 && lwp != NULL)
		lwp->lwp_ru.msgsnd++;
	return (error);
}


#if defined(_LP64) && defined(_SYSCALL32)

static int msgio32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data,
    int *rval, int mode, unsigned char *prip, int *flagsp);

int
getmsg32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data, int32_t *flagsp)
{
	int error;
	int32_t localflags;
	int realflags = 0;
	unsigned char pri = 0;
	int rv = 0;

	/*
	 * Convert between old flags (localflags) and new flags (realflags).
	 */
	if (copyin(flagsp, &localflags, sizeof (*flagsp)))
		return (set_errno(EFAULT));
	switch (localflags) {
	case 0:
		realflags = MSG_ANY;
		break;

	case RS_HIPRI:
		realflags = MSG_HIPRI;
		break;

	default:
		return (set_errno(EINVAL));
	}

	if ((error = msgio32(fdes, ctl, data, &rv, FREAD, &pri,
	    &realflags)) == 0) {
		/*
		 * massage realflags based on localflags.
		 */
		if (realflags == MSG_HIPRI)
			localflags = RS_HIPRI;
		else
			localflags = 0;
		if (copyout(&localflags, flagsp, sizeof (*flagsp)))
			error = EFAULT;
	}
	if (error != 0)
		return (set_errno(error));
	return (rv);
}

int
putmsg32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data, int32_t flags)
{
	unsigned char pri = 0;
	int realflags;
	int error;
	int rv = 0;

	switch (flags) {
	case RS_HIPRI:
		realflags = MSG_HIPRI;
		break;
	case (RS_HIPRI|MSG_XPG4):
		realflags = MSG_HIPRI|MSG_XPG4;
		break;
	case MSG_XPG4:
		realflags = MSG_BAND|MSG_XPG4;
		break;
	case 0:
		realflags = MSG_BAND;
		break;

	default:
		return (set_errno(EINVAL));
	}
	error = msgio32(fdes, ctl, data, &rv, FWRITE, &pri, &realflags);
	if (error != 0)
		return (set_errno(error));
	return (rv);
}


int
getpmsg32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data, int32_t *prip,
    int32_t *flagsp)
{
	int error;
	int32_t flags;
	int32_t intpri;
	unsigned char pri;
	int rv = 0;

	if (copyin(flagsp, &flags, sizeof (*flagsp)))
		return (set_errno(EFAULT));
	if (copyin(prip, &intpri, sizeof (intpri)))
		return (set_errno(EFAULT));
	if ((intpri > 255) || (intpri < 0))
		return (set_errno(EINVAL));
	pri = (unsigned char)intpri;
	error = msgio32(fdes, ctl, data, &rv, FREAD, &pri, &flags);
	if (error != 0)
		return (set_errno(error));
	if (copyout(&flags, flagsp, sizeof (flags)))
		return (set_errno(EFAULT));
	intpri = (int)pri;
	if (copyout(&intpri, prip, sizeof (intpri)))
		return (set_errno(EFAULT));
	return (rv);
}

int
putpmsg32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data, int32_t intpri,
    int32_t flags)
{
	unsigned char pri;
	int rv = 0;
	int error;

	if ((intpri > 255) || (intpri < 0))
		return (set_errno(EINVAL));
	pri = (unsigned char)intpri;
	error = msgio32(fdes, ctl, data, &rv, FWRITE, &pri, &flags);
	if (error != 0)
		return (set_errno(error));
	return (rv);
}

/*
 * Common code for getmsg and putmsg calls: check permissions,
 * copy in args, do preliminary setup, and switch to
 * appropriate stream routine.
 */
static int
msgio32(int fdes, struct strbuf32 *ctl, struct strbuf32 *data, int *rval,
    int mode, unsigned char *prip, int *flagsp)
{
	file_t *fp;
	vnode_t *vp;
	struct strbuf32 msgctl32, msgdata32;
	struct strbuf msgctl, msgdata;
	int error;
	int flag;
	klwp_t *lwp = ttolwp(curthread);
	rval_t rv;

	if ((fp = getf(fdes)) == NULL)
		return (EBADF);
	if ((fp->f_flag & mode) == 0) {
		releasef(fdes);
		return (EBADF);
	}
	vp = fp->f_vnode;
	if (vp->v_type == VFIFO) {
		if (vp->v_stream) {
			/*
			 * must use sd_vnode, could be named pipe
			 */
			(void) fifo_vfastoff(vp->v_stream->sd_vnode);
		} else {
			releasef(fdes);
			return (ENOSTR);
		}
	} else if ((vp->v_type != VCHR && vp->v_type != VSOCK) ||
		    vp->v_stream == NULL) {
		releasef(fdes);
		return (ENOSTR);
	}
	if (ctl != NULL) {
		if (copyin(ctl, &msgctl32, sizeof (msgctl32))) {
			releasef(fdes);
			return (EFAULT);
		}
		msgctl.len = msgctl32.len;
		msgctl.maxlen = msgctl32.maxlen;
		msgctl.buf = (caddr_t)msgctl32.buf;
	}
	if (data != NULL) {
		if (copyin(data, &msgdata32, sizeof (msgdata32))) {
			releasef(fdes);
			return (EFAULT);
		}
		msgdata.len = msgdata32.len;
		msgdata.maxlen = msgdata32.maxlen;
		msgdata.buf = (caddr_t)msgdata32.buf;
	}

	if (mode == FREAD) {
		if (ctl == NULL)
			msgctl.maxlen = -1;
		if (data == NULL)
			msgdata.maxlen = -1;
		flag = fp->f_flag;
		rv.r_val1 = 0;
		if (vp->v_type == VSOCK) {
			error = sock_getmsg(vp, &msgctl, &msgdata, prip,
			    flagsp, flag, &rv);
		} else {
			error = strgetmsg(vp, &msgctl, &msgdata, prip,
			    flagsp, flag, &rv);
		}
		*rval = rv.r_val1;
		if (error != 0) {
			releasef(fdes);
			return (error);
		}
		if (lwp != NULL)
			lwp->lwp_ru.msgrcv++;
		if (ctl != NULL) {
			/* XXX64 - range check */
			msgctl32.len = msgctl.len;
			msgctl32.maxlen = msgctl.maxlen;
			msgctl32.buf = (caddr32_t)msgctl.buf;
			if (copyout(&msgctl32, ctl, sizeof (msgctl32))) {
				releasef(fdes);
				return (EFAULT);
			}
		}
		if (data != NULL) {
			/* XXX64 - range check */
			msgdata32.len = msgdata.len;
			msgdata32.maxlen = msgdata.maxlen;
			msgdata32.buf = (caddr32_t)msgdata.buf;
			if (copyout(&msgdata32, data, sizeof (msgdata32))) {
				releasef(fdes);
				return (EFAULT);
			}
		}
		releasef(fdes);
		return (0);
	}

	/*
	 * FWRITE case
	 */
	if (ctl == NULL)
		msgctl.len = -1;
	if (data == NULL)
		msgdata.len = -1;
	flag = fp->f_flag;
	if (vp->v_type == VSOCK) {
		error = sock_putmsg(vp, &msgctl, &msgdata, *prip, *flagsp,
		    flag);
	} else {
		error = strputmsg(vp, &msgctl, &msgdata, *prip, *flagsp, flag);
	}
	releasef(fdes);
	if (error == 0 && lwp != NULL)
		lwp->lwp_ru.msgsnd++;
	return (error);
}

#endif /* _LP64 && _SYSCALL32 */
