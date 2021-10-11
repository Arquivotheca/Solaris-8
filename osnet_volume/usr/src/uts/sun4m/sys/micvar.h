/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MICVAR_H
#define	_SYS_MICVAR_H

#pragma ident	"@(#)micvar.h	1.8	97/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The soft state structure for the MIC chip
 */
#define	 PORTS	2
typedef struct mic_unit {
	dev_info_t	*mic_dip;	/* dev_info */
	ddi_iblock_cookie_t mic_intr;	/* for hard interrupts */
	ddi_iblock_cookie_t soft_intr;	/* for soft interrupts */
	ddi_softintr_t	soft_id;
	struct mic_port	*port[PORTS];	/* port data structures */
} mic_unit_t;

/*
 * Asychronous protocol private data structure for each port on the MIC.
 */
typedef struct mic_port {
	struct mic_unit	*unitp;		/* mic chip owning this port */
	struct mic_stat	*stat_regs;	/* pointer to MIC status regs */
	struct mic_ctl	*ctl_regs;	/* pointer to MIC control regs */
	struct mic_scc	*scc_regs;	/* pointer to MIC SCC regs */

	kmutex_t	port_lock;	/* protects port opens/closes */
	kcondvar_t	tx_done_cv;	/* notification when the tx is done */
	int		flags;		/* random flags for status of port */
	tty_common_t 	ttycommon;	/* tty driver common data */
	bufcall_id_t	ioctl_id;	/* id of pending ioctl bufcall */
	bufcall_id_t	send_id;	/* id of pending write-side bufcall */
	uchar_t		intr_en;	/* enabled interrupts */
	int		baud_div;	/* current baud rate divisor */

	kmutex_t	tx_lock;	/* protects tx blk */
	mblk_t		*tx_blk;	/* transmit: active msg block */
	uchar_t		tx_buf[64];	/* buffer to hold TX FIFO during cpr */
	int		tx_buf_cnt;	/* amount of data in buffer */
	uchar_t		ir_mode;
	uchar_t		ir_divisor;

	kmutex_t	rx_lock;	/* protects rx blk */
	mblk_t		*rx_blk;	/* receive: active msg block */
} mic_port_t;

/*
 * Definitions for flags field
 */
#define	PORT_OPEN	0x00000001	/* port is open */
#define	PORT_IR		0x00000002	/* port is an infra-red link */
#define	PORT_DELAY	0x00000004	/* waiting for delay to finish */
#define	PORT_BREAK	0x00000008	/* waiting for break to finish */
#define	PORT_TXCHK	0x00000010	/* soft interrupt - get next msg */
#define	PORT_RXCHK	0x00000020	/* soft interrupt - send next msg */
#define	PORT_TXDONE	0x00000040	/* soft interrupt - broadcast to cv */
#define	PORT_IRBUSY	0x00000080	/* infra-red tx in progress */
#define	PORT_LPBK	0x00000100	/* port in loopback mode (IR only) */

/* Mic chips which we support */
#define	MIC_ID		1

/*
 * Mic chip parameters, which we are going to use.
 * (May be tuned to suit different applications)
 */
#define	RX_WATER_MARK_HI	32	/* # bytes received before intr */
#define	RX_WATER_MARK_LO	1	/* # bytes received before intr */
#define	TX_WATER_MARK		32	/* # bytes remaining before intr */
#define	RX_STALE_TIME		65535	/* 3.3 ms with 20MHz SBUS (max) */
#define	RX_BUFFER_SZ		128	/* # bytes in receive msg buffer */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MICVAR_H */
