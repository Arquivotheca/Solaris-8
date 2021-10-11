/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident	"@(#)pcic.h	1.1	96/11/21 SMI\n"

#define	PCIC_INDEX		0
#define	PCIC_DATA		1

#define	PCIC_IDREV		0x00
#define	PCIC_IFSTATUS		0x01
#define	PCIC_PWRRSTCON		0x02
#define	PCIC_CARDSTATCHANGE	0x04
#define	PCIC_ADDRWINENABLE	0x06
#define	PCIC_CARDDETGENCON	0x16
#define	PCIC_GLOBALCON		0x1E

#define	PCIC_INTRGENCON		0x03
#define	PCIC_CARDSTATCONF	0x05

#define	PCIC_IOCON		0x07

#define	PCIC_CHIP_INFO		0x1f
#define	PCIC_MISC_CTL1		0x16
#define	PCIC_MISC_CTL2		0x1e

/*
 * Offsets in the I/O window registers
 */
#define	PCIC_IO_STARTL		0
#define	PCIC_IO_STARTH		1
#define	PCIC_IO_STOPL		2
#define	PCIC_IO_STOPH		3

#define	PCIC_IOWIN_BASE		0x08
#define	PCIC_IOWIN_SIZE		0x04

#define	PCIC_IO_START0L		(PCIC_IO_STARTL + PCIC_IOWIN_BASE)
#define	PCIC_IO_START0H		(PCIC_IO_STARTH + PCIC_IOWIN_BASE)

/*
 * Offsets in the memory window registers
 */
#define	PCIC_SM_STARTL		0
#define	PCIC_SM_STARTH		1
#define	PCIC_SM_STOPL		2
#define	PCIC_SM_STOPH		3
#define	PCIC_CM_L		4
#define	PCIC_CM_H		5

#define	PCIC_MEMWIN_BASE	0x10
#define	PCIC_MEMWIN_SIZE	0x08

#define	PCIC_SM_DS		0x8000
#define	PCIC_SM_ZWS		0x4000
#define	PCIC_SM_WS_SHIFT	14
#define	PCIC_SM_MASK		0x0FFF

#define	PCIC_CM_RA		0x4000
#define	PCIC_CM_WP		0x8000
#define	PCIC_CM_MASK		0x3FFF

#define	PCIC_IO_DS		0x01
#define	PCIC_IO_IOCS16		0x02
#define	PCIC_IO_ZWS		0x04
#define	PCIC_IO_WS		0x08

#define	IFS_BVD1		0x01
#define	IFS_BVD2		0x02
#define	IFS_CD1			0x04
#define	IFS_CD2			0x08
#define	IFS_MWP			0x10
#define	IFS_RDY			0x20
#define	IFS_PWRACT		0x40

#define	CSC_BD			0x01
#define	CSC_BW			0x02
#define	CSC_RDY			0x04
#define	CSC_CD			0x08
#define	CSC_IRQ_MASK		0xF0

#define	IGCR_INTEN		0x10
#define	IGCR_IO			0x20
#define	IGCR_RESET		0x40
#define	IGCR_IRQ_MASK		0x0F
#define	IGCR_DEFAULT		(0)

#define	GCON_POWER_DOWN		0x01
#define	GCON_LEVEL_INTR		0x02
#define	GCON_EXP_CSC_WB		0x04
#define	GCON_IRQ14_PULSE	0x08

#define	PCIC_CI_ID		0xc0
#define	PCIC_CI_SLOTS		0x20

#define	PWR_CARD_ENABLE		0x10
#define	PWR_OUTPUT_ENABLE	0x80
#define	PWR_VPP1_EN0		0x01
#define	PWR_VPP1_EN1		0x02
#define	PWR_VPP2_EN0		0x04
#define	PWR_VPP2_EN1		0x08
#define	PWR_DEFAULT		(PWR_CARD_ENABLE|PWR_OUTPUT_ENABLE| \
				 PWR_VPP1_EN0|PWR_VPP2_EN0)

#define	CSICR_BDE		0x01
#define	CSICR_BWE		0x02
#define	CSICR_RE		0x04
#define	CSICR_CDE		0x08
#define	CSICR_DEFAULT		(CSICR_BDE|CSICR_BWE|CSICR_RE|CSICR_CDE)

#define	PCIC_BASE0		0x00
#define	PCIC_BASE1		0x80
#define	PCIC_SOCKET0		0x00
#define	PCIC_SOCKET1		0x40

#define	PCIC_SOCKETS		16	/* maximum sockets */
#define	PCIC_MEMWINS		5	/* mem windows/socket */
#define	PCIC_IOWINS		2	/* I/O windows/socket */
#define	PCIC_SOCKWINS		(PCIC_MEMWINS+PCIC_IOWINS)

#define	PCIC_IO		0	/* window maps I/O space */
#define	PCIC_MEM	1	/* window maps memory space */

#define	PCIC_ID_I365SL		0x83	/* ID byte for real Intel PCIC */

#define	PCIC_TYPE_NONE		(-1)	/* no controller or unknown */
#define	PCIC_TYPE_I365SL	0	/* Intel controller */
#define	PCIC_TYPE_CL67XX	1	/* Cirrus Logic CL-PD67XX */
