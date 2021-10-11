/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_AIO_REQ_H
#define	_SYS_AIO_REQ_H

#pragma ident	"@(#)aio_req.h	1.2	94/11/11 SMI"

#include <sys/buf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * async I/O request struct exposed to drivers.
 */
struct aio_req {
	struct uio	*aio_uio;		/* UIO for this request */
	void 		*aio_private;
};

extern int aphysio(int (*)(), int (*)(), dev_t, int, void (*)(),
		struct aio_req *);
extern int anocancel(struct buf *);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AIO_REQ_H */
