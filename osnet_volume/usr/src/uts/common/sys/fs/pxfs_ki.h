/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PXFS_KI_H
#define	_PXFS_KI_H

#pragma ident	"@(#)pxfs_ki.h	1.1	98/07/17 SMI"

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/aio_req.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kernel interface to pxfs routines: definition of stubs.
 */

/*
 * kaio interface to pxfs
 */

extern int clpxfs_aio_write(vnode_t *vp, struct aio_req *aio, cred_t *cred_p);
extern int clpxfs_aio_read(vnode_t *vp, struct aio_req *aio, cred_t *cred_p);

#ifdef __cplusplus
}
#endif

#endif /* _PXFS_KI_H */
