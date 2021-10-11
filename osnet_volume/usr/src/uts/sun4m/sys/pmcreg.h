/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Power management chip
 *
 *   register and bit definitions of the PMC
 */

#ifndef	_SYS_PMCREG_H
#define	_SYS_PMCREG_H

#pragma ident	"@(#)pmcreg.h	1.8	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct pmc_reg {
	uchar_t	pmc_cpu;
	uchar_t	pmc_unass1;	/* Unassigned */
	uchar_t	pmc_kb;
	uchar_t	pmc_unass2;
	uchar_t	pmc_pwrkey;
	uchar_t	pmc_enet;
	uchar_t	pmc_esp;
	uchar_t	pmc_zsab;
	uchar_t	pmc_audio;
	uchar_t	pmc_isdn;
	uchar_t	pmc_a2dctrl;
	uchar_t	pmc_a2d;
	uchar_t	pmc_d2a;
	uchar_t	pmc_unass3;
	uchar_t	pmc_unass4;
	uchar_t	pmc_test;
};

#define	BW2OFFSET		0x200001
#define	CG6OFFSET		0x280008
#define	FBDELAY			(hz / 100)

#define	PMC_CPU			0x00
#define	PMC_UNASS1		0x01
#define	PMC_KBD			0x02
#define	PMC_UNASS2		0x03
#define	PMC_PWRKEY		0x04
#define	PMC_ENET		0x05
#define	PMC_ESP			0x06
#define	PMC_ZSAB		0x07
#define	PMC_AUDIO		0x08
#define	PMC_ISDN		0x09
#define	PMC_A2DCTRL		0x0a
#define	PMC_A2D			0x0b
#define	PMC_D2A			0x0c
#define	PMC_UNASS3		0x0c
#define	PMC_UNASS4		0x0c
#define	PMC_TST			0x0f


/* bit defines (masks) */

#define	PMC_CPU_PWR		0x01	/* bit0 */

#define	PMC_KBD_TEST		0x01	/* bit0 */
#define	PMC_KBD_INTEN		0x02	/* bit1 */
#define	PMC_KBD_STAT		0x04	/* bit2 */
#define	PMC_KBD_INTR		0x08	/* bit3 */

#define	PMC_PWRKEY_PWR		0x01	/* bit0 */
#define	PMC_PWRKEY_INTEN	0x02	/* bit1 */
#define	PMC_PWRKEY_INTR		0x04	/* bit2 */

#define	PMC_ENET_PWR		0x01	/* bit0 */
#define	PMC_ENET_INTEN		0x02	/* bit1 */
#define	PMC_ENET_TEST		0x04	/* bit2 */
#define	PMC_ENET_STAT		0x08	/* bit3 */
#define	PMC_ENET_INTR		0x10	/* bit4 */

#define	PMC_ESP_PWR		0x01	/* bit0 */

#define	PMC_ZSB_PWR		0x01	/* bit0 */
#define	PMC_ZSA_PWR		0x02	/* bit1 */

#define	PMC_AUDIO_PWR		0x01	/* bit0 */

#define	PMC_ISDN_PWR		0x01	/* bit0 */
#define	PMC_ISDN_INTEN		0x02	/* bit1 */
#define	PMC_ISDN_ST0		0x04	/* bit2 */
#define	PMC_ISDN_ST1		0x08	/* bit3 */
#define	PMC_ISDN_INTR		0x10	/* bit4 */

#define	PMC_A2D_0		0x01	/* bit0 */
#define	PMC_A2D_1		0x02	/* bit1 */
#define	PMC_A2D_2		0x04	/* bit2 */
#define	PMC_A2D_INTEN		0x08	/* bit3 */
#define	PMC_A2D_STRT		0x10	/* bit4 */
#define	PMC_A2D_INTR		0x20	/* bit5 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PMCREG_H */
