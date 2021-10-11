/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)ata_blacklist.c	1.4	99/02/17 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/pci.h>

#include "ata_blacklist.h"

pcibl_t	ata_pciide_blacklist[] = {
	/*
	 * The Nat SEMI PC87415 doesn't handle data and status byte
	 * synchornization correctly if an I/O error occurs that
	 * stops the request before the last sector.  I think it can
	 * cause lockups. See section 7.4.5.3 of the PC87415 spec.
	 * It's also rumored to be a "single fifo" type chip that can't
	 * DMA on both channels correctly.
	 */
	{ 0x100b, 0xffff, 0x2, 0xffff, ATA_BL_BOGUS},

	/*
	 * The CMD chip 0x646 does not support the use of interrupt bit
	 * in the busmaster ide status register when PIO is used
	 */
	{ 0x1095, 0xffff, 0x0646, 0xffff, ATA_BL_BMSTATREG_PIO_BROKEN},


	{ 0, 0, 0, 0, 0 }
};

/*
 * add drives that have DMA or other problems to this list
 */

atabl_t	ata_drive_blacklist[] = {
	{ "NEC CD-ROM DRIVE:260",	ATA_BL_1SECTOR },
	{ "NEC CD-ROM DRIVE:272",	ATA_BL_1SECTOR },
	{ "NEC CD-ROM DRIVE:273",	ATA_BL_1SECTOR },
	{ /* Mitsumi */ "FX001DE",	ATA_BL_1SECTOR },

	{ "fubar",		(ATA_BL_NODMA | ATA_BL_1SECTOR) },
	NULL
};
