/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SUNDDI_LYR_H
#define	_SYS_SUNDDI_LYR_H

#pragma ident	"@(#)sunddi_lyr.h	1.4	99/03/02 SMI"

/*
 * Layered driver support.
 *
 * Currently Sun Private interfaces, undocumented and subject to
 * more-or-less arbitrary change from release to release.
 *
 * You have been warned.
 *
 * NOT part of the Solaris DDI/DKI.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

typedef void *ddi_lyr_handle_t;

extern int ddi_lyr_open_by_dev_t(dev_t *devp, int, int *, cred_t *,
    ddi_lyr_handle_t *);
extern int ddi_lyr_open_by_name(char *pathname, int, int *, cred_t *,
    ddi_lyr_handle_t *);
extern int ddi_lyr_close(ddi_lyr_handle_t, cred_t *);
extern int ddi_lyr_strategy(ddi_lyr_handle_t, struct buf *);
extern int ddi_lyr_print(ddi_lyr_handle_t, char *);
extern int ddi_lyr_dump(ddi_lyr_handle_t, caddr_t, daddr_t, int);
extern int ddi_lyr_read(ddi_lyr_handle_t, struct uio *, cred_t *);
extern int ddi_lyr_write(ddi_lyr_handle_t, struct uio *, cred_t *);
/*
 * Layered ioctl's from the kernel can be achieved using FKIOCTL.
 * It is the responsibility of the layering driver to check for
 * the presence of the "ddi-kernel-ioctl" property before issuing
 * such ioctls to the underlying driver.
 */
extern int ddi_lyr_ioctl(ddi_lyr_handle_t, int, intptr_t, int, cred_t *, int *);
extern int ddi_lyr_mmap(ddi_lyr_handle_t, off_t, int);
extern int ddi_lyr_chpoll(ddi_lyr_handle_t, short, int, short *,
	struct pollhead **);
extern int ddi_lyr_prop_op(ddi_lyr_handle_t, ddi_prop_op_t, int,
    char *, caddr_t, int *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SUNDDI_LYR_H */
