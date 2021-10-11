/*
 * Copyright (c) 1992-1995,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ECPPIO_H
#define	_SYS_ECPPIO_H

#pragma ident	"@(#)ecppio.h	2.9	98/04/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/bpp_io.h>
#include <sys/ecppsys.h>

#define	P1284IOC_ONE		_IO('p', 51)
#define	P1284IOC_TWO		_IO('p', 52)
#define	P1284IOC_THREE		_IOR('p', 53, P1284Ioctl)
#define	P1284IOC_FAKE_INTR	_IO('b', 54)

#define	ECPPIOC_CAPABILITY	_IOR('p', 72, uchar_t)
#define	ECPPIOC_SETREGS		_IOW('p', 73, struct ecpp_regs)
#define	ECPPIOC_GETREGS		_IOR('p', 74, struct ecpp_regs)
#define	ECPPIOC_SETPORT		_IOW('p', 77, uchar_t)
#define	ECPPIOC_GETPORT		_IOR('p', 78, uchar_t)
#define	ECPPIOC_SETDATA		_IOW('p', 79, uchar_t)
#define	ECPPIOC_GETDATA		_IOR('p', 80, uchar_t)

#define	ECPP_MAX_TIMEOUT 	604800  /* one week */
#define	ECPP_W_TIMEOUT_DEFAULT	60 /* 60 seconds */

struct ecpp_regs {
	uint8_t	dsr;	/* status reg */
	uint8_t	dcr;	/* control reg */
};

/* Values for dsr field */
#define	ECPP_reseverd1		0x01
#define	ECPP_reseverd2		0x02
#define	ECPP_reserved3		0x04
#define	ECPP_nERR		0x08
#define	ECPP_SLCT		0x10
#define	ECPP_PE			0x20
#define	ECPP_nACK		0x40
#define	ECPP_nBUSY		0x80

/*  Values for the dcr field */
#define	ECPP_STB		0x01
#define	ECPP_AFX		0x02
#define	ECPP_nINIT		0x04
#define	ECPP_SLCTIN		0x08
#define	ECPP_INTR_EN		0x10	/* 1=enable */
#define	ECPP_REV_DIR		0x20	/* 1=reverse dir */
#define	ECPP_reseverd4		0x40
#define	ECPP_reserved5		0x80
#define	ECPP_DCR_SET		0xc0

/* port types */
#define	ECPP_PORT_DMA	0x1	/* default */
#define	ECPP_PORT_PIO	0x2
#define	ECPP_PORT_TDMA	0x3	/* test fifo */

/*
 * write timeout;
 *
 * This parameter is a timer that monitors the dma period for a write operation.
 * the driver requires a timer in the event that the dma does not complete
 * successfully and the device fails to generate an system interrupt indicating
 * an error condition.  The value should be in seconds and initiated to be
 * ~ 60 seconds.
 */

/*
 * timeout_occurred is straight forward. it will be included.
 *
 * bus_error will not be used.
 *
 * 			pin_status ...
 * in bpp pin_status returns the value of the status pins.
 * The user would check the status of PE,SELECT & ERR and
 * determine if the device is offline, or failed.
 * This makes sense in compatibility mode.  We can continue
 * to provide that functionality for compatibility mode.
 * Should it be extended somehow to work with other modes?
 * if it is going to work with other modes, there needs
 * to be some way to sort out which mode the driver is in.
 * The driver knows, but the ioctl should be dumb-user-proof ...
 * i suppose.  maybe the ioctl simply returns a uchar_t
 * the uchar_t has meaning depending on what mode it is in.
 * compatibility mode: uchar_t  3 of 8 bits indicate PE, SELECT, & ERR.
 *
 * ECP mode
 * 1 bit of the uchar_t indicates if the back-channel buffer has any
 * unread buffers in it.
 *
 * nibble mode:
 * interesting ... very interesting ... this brings
 * up the question of who should be reading the
 * back-channel in nibble mode.  based on our POSTIO
 * discussions the driver will read the back channel.
 * Perhaps we should provide an access for an app
 * to read the status pins such that the app can
 * implement its own back channel...
 * because of our postio requirements we have to
 * have the driver implement the nibble-mode back
 * channel
 *
 * perhaps the union of pin_status for nibble
 * mode
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ECPPIO_H */
