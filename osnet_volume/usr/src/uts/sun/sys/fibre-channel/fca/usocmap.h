/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FIBRE_CHANNEL_FCA_USOCMAP_H
#define	_SYS_FIBRE_CHANNEL_FCA_USOCMAP_H

#pragma ident	"@(#)usocmap.h	1.2	99/07/26 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	USOC EEPROM Map
 */
#define	USOC_PROM_4TH_SELF_TST	0x00000 /* 0x05000 thru 0x05fff forth code */
#define	USOC_PROM_4TH_OBP_DRV	0x01000	/* thru 0x09fff forth OBP driver */
#define	USOC_PROM_OBP_HDR	0x05000	/* thru 0x002ff */
#define	USOC_PROM_FW_DATE_CODE	0x05300	/* thru 0x00303 FW date code */
#define	USOC_PROM_SRVC_PARM	0x05304	/* thru 0x00343 SOC+ Service params */
#define	USOC_PROM_LA_BIT_MASK	0x05344	/* thru 0x0034b link app bit mask */
#define	USOC_PROM_RSRV1		0x0534c	/* thru 0x00fff */
#define	USOC_PROM_USOC_CODE	0x06000	/* thru 0x04fff SOC+ code */
#define	USOC_PROM_RSRV2		0x0f000	/* thru 0x0ffff */

/*
 *	USOC XRam Map
 */
#define	USOC_XRAM_REQ_DESC	0x00200	/* req circular que descriptors */
#define	USOC_XRAM_RSP_DESC	0x00220	/* req circular que descriptors */
#define	USOC_XRAM_LESB_P0	0x00240
#define	USOC_XRAM_LESB_P1	0x00258 /* thru 0x1026f */
#define	USOC_XRAM_SERV_PARAMS	0x00280
#define	USOC_XRAM_FW_DATE_STR	0x002dc	/* ctime() format date code */
#define	USOC_XRAM_FW_DATE_CODE	0x002f8	/* thru 0x002fb FW date code */
#define	USOC_XRAM_HW_REV	0x002fc	/* thru 0x002ff HW revision */
#define	USOC_XRAM_UCODE		0x00300	/* thru 0x03fff SOC+ microcode */
#define	USOC_XRAM_PORTA_WWN	0x00300	/* thru 0x00307, port A wwn */
#define	USOC_XRAM_PORTB_WWN	0x00308	/* thru 0x0030f, port B wwn */
#define	USOC_XRAM_NODE_WWN	0x00310	/* thru 0x00317, Node worldwide name */
#define	USOC_XRAM_BUF_POOL	0x04000	/* thru 0x0bfff	soc+ buffer pool */
#define	USOC_XRAM_EXCH_POOL	0x0c000	/* thru 0x0ffff soc+ exchange pool */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_FIBRE_CHANNEL_FCA_USOCMAP_H */
