/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _PCIIDE_H
#define	_PCIIDE_H

#pragma ident	"@(#)pciide.h	1.2	99/02/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Bus Mastering devices have a PCI class-code of 0x010180 to 0x0101ff
 */
#define	PCIIDE_BM_CLASS	((PCI_CLASS_MASS << 16) | (PCI_MASS_IDE << 8) | 0x80)
#define	PCIIDE_BM_CLASS_MASK	0xffffff80


#define	PCIIDE_BMICX_REG	0	/* Bus Master IDE Command Register */

#define	PCIIDE_BMICX_SSBM	0x01	/* Start/Stop Bus Master */
#define	PCIIDE_BMICX_SSBM_E		0x01	/* 1=Start (Enable) */
						/* 0=Start (Disable) */

/*
 * NOTE: "read" and "write" are the actions of the DMA
 * engine on the PCI bus. Not the DMA engine's action on the ATA
 * BUS. Therefore for a ATA READ command, program the DMA engine to
 * "write to memory" mode (and vice versa).
 */
#define	PCIIDE_BMICX_RWCON	0x08	/* Read/Write Control */
#define	PCIIDE_BMICX_RWCON_WRITE_TO_MEMORY	0x08 /* 1=Write (dev to host) */
#define	PCIIDE_BMICX_RWCON_READ_FROM_MEMORY	0x00 /* 0=Read  (host to dev) */

/* preserve these bits during updates */
#define	PCIIDE_BMICX_MASK	(~(PCIIDE_BMICX_SSBM | PCIIDE_BMICX_RWCON))



#define	PCIIDE_BMISX_REG	2	/* Bus Master IDE Status Register */

#define	PCIIDE_BMISX_BMIDEA	0x01	/* Bus Master IDE Active */
#define	PCIIDE_BMISX_IDERR	0x02	/* IDE DMA Error */
#define	PCIIDE_BMISX_IDEINTS	0x04	/* IDE Interrupt Status */
#define	PCIIDE_BMISX_DMA0CAP	0x20	/* Drive 0 DMA Capable */
#define	PCIIDE_BMISX_DMA1CAP	0x40	/* Drive 1 DMA Capable */
#define	PCIIDE_BMISX_SIMPLEX	0x80	/* Simplex only */

/* preserve these bits during updates */
#define	PCIIDE_BMISX_MASK	0xf8

#define	PCIIDE_BMIDTPX_REG	4	/* Bus Master IDE Desc. Table Ptr */
#define	PCIIDE_BMIDTPX_MASK	0x00000003	/* must be zeros */


typedef struct PhysicalRegionDescriptorTableEntry {
	uint_t	p_address;	/* physical address */
	uint_t	p_count;	/* byte count, EOT in high order bit */
} prde_t;

/*
 * Some specs say the p_address must 32-bit aligned, and some claim
 * 16-bit alignment. Use 32-bit alignment just to be safe.
 */
#ifdef __not_yet__
#define	PCIIDE_PRDE_ADDR_MASK	((uint_t)(sizeof (short) -1))
#else
#define	PCIIDE_PRDE_ADDR_MASK	((uint_t)(sizeof (int) -1))
#endif

#define	PCIIDE_PRDE_CNT_MASK	((uint_t)0x0001)	/* must be even */
#define	PCIIDE_PRDE_CNT_MAX	((uint_t)0x10000)	/* 0 == 64k */
#define	PCIIDE_PRDE_EOT		((uint_t)0x80000000)

#ifdef	__cplusplus
}
#endif

#endif /* _PCIIDE_H */
