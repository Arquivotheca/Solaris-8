/*
 * Copyright (c) 1992-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_XBOXIF_H
#define	_SYS_XBOXIF_H

#pragma ident	"@(#)xboxif.h	1.4	98/01/06 SMI"

/*
 * This file describes the external interface supported by the XBox
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ioctls
 * reset: cause a reset of specified length
 * type is SRST_XARS, SRST_CRES, SRST_HRES
 */
struct xac_ioctl_reset {
	uchar_t			xac_reset_type;
	uint_t			xac_reset_length;
};

/*
 * issue a write0 cmd
 */
struct xac_ioctl_write0 {
	uint_t			xac_address;		/* offset */
	uint_t			xac_data;
};

/*
 * wait for error for a specified time; if timeout == 0, wait forever
 */
struct xac_ioctl_wait_for_error {
	struct xc_errs		xac_errpkt;
	clock_t			xac_timeout;
};

/*
 * get shadow copy of register values
 */
struct xac_ioctl_get_reg_values {
	uint_t			xac_ctl0;
	uint_t			xbc_ctl0;
	uint_t			xac_epkt_dma_addr;
	uint_t			xbc_epkt_dma_addr;
};


#define	XAC_RESET		_IO('x', 0)
#define	XAC_REG_CHECK		_IO('x', 2)
#define	XAC_TRANSPARENT		_IO('x', 3)
#define	XAC_NON_TRANSPARENT	_IO('x', 4)
#define	XAC_WRITE0		_IOW('x', 5, struct xac_ioctl_write0)
#ifdef XBOX_DEBUG
#define	XAC_DUMP_REGS		_IO('x', 6)
#endif
#define	XAC_WAIT_FOR_ERROR_PKT	_IOWR('x', 7, struct xac_ioctl_wait_for_error)
#define	XAC_GET_ERROR_PKT	_IOR('x', 8, struct xc_errs)
#define	XAC_CLEAR_WAIT_FOR_ERROR _IO('x', 9)
#define	XAC_GET_REG_VALUES	_IOR('x', 10, struct xac_ioctl_get_reg_values)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XBOXIF_H */
