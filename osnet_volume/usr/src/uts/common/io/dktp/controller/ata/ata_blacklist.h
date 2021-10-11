
/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _ATA_BLACKLIST_H
#define	_ATA_BLACKLIST_H

#pragma ident	"@(#)ata_blacklist.h	1.5	99/02/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This is the PCI-IDE chip blacklist
 */
typedef struct {
	uint_t	b_vendorid;
	uint_t	b_vmask;
	uint_t	b_deviceid;
	uint_t	b_dmask;
	uint_t	b_flags;
} pcibl_t;

extern	pcibl_t	ata_pciide_blacklist[];

/*
 * This is the drive blacklist
 */
typedef	struct {
	char	*b_model;
	uint_t	 b_flags;
} atabl_t;

extern	atabl_t	ata_drive_blacklist[];

/*
 * use the same flags for both lists
 */
#define	ATA_BL_BOGUS	0x1	/* only use in compatibility mode */
#define	ATA_BL_NODMA	0x2	/* don't use DMA on this one */
#define	ATA_BL_1SECTOR	0x4	/* limit PIO transfers to 1 sector */
#define	ATA_BL_BMSTATREG_PIO_BROKEN	0x8
				/*
				 * do not use bus master ide status register
				 * if not doing dma
				 */


#ifdef	__cplusplus
}
#endif

#endif /* _ATA_BLACKLIST_H */
