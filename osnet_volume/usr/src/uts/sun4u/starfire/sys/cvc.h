/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _CVC_H
#define	_CVC_H

#pragma ident	"@(#)cvc.h	1.15	97/11/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	cvc_username		"ssp"
#define	CVCD_SERVICE		"cvc_hostd"
#define	CVC_CONN_SERVICE	"cvc"
#define	MAX_CVC_CONN		100
#define	MAXPKTSZ		4096

#define	TRUE			1
#define	FALSE			0

/*
 * Network Redirection driver ioctl to jump into debugger.
 */
#define	CVC	'N'
#define	CVC_BREAK	(CVC<<8)
#define	CVC_DISCONNECT	((CVC<<8)|0x1)

#define	CVC_CONN_BREAK		0x1	/* Break to OBP or kadb */
#define	CVC_CONN_DIS		0x2	/* disconnect */
#define	CVC_CONN_STAT		0x4	/* status of CVC connects */
#define	CVC_CONN_WRITE		0x8	/* ask write permission */
#define	CVC_CONN_RELW		0x10    /* release write permission */
#define	CVC_CONN_WRLK		0x20    /* Lock the Write */
#define	CVC_CONN_PRIVATE	0x40    /* Only one session is allowed */
#define	CVC_CONN_SWITCH		0x41	/* Switch communication path */


#define	TCP_DEV		"/dev/tcp"
#define	CVCREDIR_DEV	"/devices/pseudo/cvcredir@0:cvcredir"

/*
 * Layout of BBSRAM input and output buffers:
 *
 *
 *         |---------------|
 * 0x1f400 | control msg   |  Receive buffer is reduced by two bytes to
 *         |    1 byte     |  accomodate a control msg area in which
 *         |---------------|  information is sent from obp_helper to the
 *         |  send buffer  |  cvc driver (e.g. break to obp) when
 *         |               |  communication is over BBSRAM.
 *         |  1020 bytes   |
 *         |---------------|
 *         | send  count   |
 * 0x1f7fe |  2 bytes      |
 *         |---------------|
 *         | receive buffer|
 *         |               |
 *         | 1022 bytes    |
 *         |---------------|
 *         | output count  |
 * 0x1fbfe |  2 bytes      |
 *         |---------------|
 *
 */

#define	BBSRAM_COUNT_SIZE	sizeof (short)
#define	CVC_IN_SIZE		256
#define	CVC_OUT_SIZE		1024
#define	MAX_XFER_INPUT		(CVC_IN_SIZE - BBSRAM_COUNT_SIZE)
#define	MAX_XFER_OUTPUT		(CVC_OUT_SIZE - BBSRAM_COUNT_SIZE)

#define	BBSRAM_OUTPUT_COUNT_OFF (CVC_OUT_SIZE - BBSRAM_COUNT_SIZE)
#define	BBSRAM_INPUT_COUNT_OFF  (CVC_OUT_SIZE + CVC_IN_SIZE - BBSRAM_COUNT_SIZE)

/*
 * Control msgs sent across BBSRAM from obp_helper to cvc driver
 */
#define	CVC_BBSRAM_BREAK	1
#define	CVC_BBSRAM_DISCONNECT	2
#define	CVC_BBSRAM_VIA_NET	3
#define	CVC_BBSRAM_VIA_BBSRAM	4

#ifdef _KERNEL
extern void	cvc_assign_iocpu(int cpu_id);
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _CVC_H */
