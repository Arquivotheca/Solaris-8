/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SOCMAP_H
#define	_SYS_SOCMAP_H

#pragma ident	"@(#)socmap.h	1.6	95/02/10 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	SOC EEPROM Map
 */
#define	SOC_EEPROM_4TH_SELF_TST	0x00000 /* 0x05000 thru 0x05fff forth code */
#define	SOC_EEPROM_4TH_OBP_DRV	0x01000	/* thru 0x09fff forth OBP driver */
#define	SOC_EEPROM_OBP_HDR	0x05000	/* thru 0x002ff */
#define	SOC_EEPROM_FW_DATE_CODE	0x05300	/* thru 0x00303 FW date code */
#define	SOC_EEPROM_SRVC_PARM	0x05304	/* thru 0x00343 SOC service params */
#define	SOC_EEPROM_LA_BIT_MASK	0x05344	/* thru 0x0034b link app bit mask */
#define	SOC_EEPROM_RSRV1	0x0534c	/* thru 0x00fff */
#define	SOC_EEPROM_SOC_CODE	0x06000	/* thru 0x04fff SOC code */
#define	SOC_EEPROM_RSRV2	0x0f000	/* thru 0x0ffff */

/*
 *	SOC XRam Map
 */
#define	SOC_XRAM_REQ_DESC	0x00200	/* req circular que descriptors */
#define	SOC_XRAM_RSP_DESC	0x00220	/* req circular que descriptors */
#define	SOC_XRAM_LESB_P0	0x00240
#define	SOC_XRAM_LESB_P1	0x00258 /* thru 0x1026f */
#define	SOC_XRAM_SERVICE_PARAMS	0x00280
#define	SOC_XRAM_FW_DATE_STR	0x002d0	/* ctime() format date code */
#define	SOC_XRAM_FW_DATE_CODE	0x002f8	/* thru 0x102fb FW date code */
#define	SOC_XRAM_HW_REV		0x002fc	/* thru 0x102ff HW revision */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_SOCMAP_H */
