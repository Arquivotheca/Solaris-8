/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DCD_CONF_AUTOCONF_H
#define	_SYS_DCD_CONF_AUTOCONF_H

#pragma ident	"@(#)autoconf.h	1.6	98/02/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Following are for debugging purposes (few Sun drivers support this)
 */
#define	DCD_DEBUG_TGT	0x1	/* debug statements in target drivers */
#define	DCD_DEBUG_LIB	0x2	/* debug statements in library */
#define	DCD_DEBUG_HA	0x4	/* debug statements in host adapters */

/*
 * DCD autoconfiguration definitions.
 *
 * The library routine dcd_probe() is provided as a service to target
 * driver to check for bare-bones existence of a dcd device. It is
 * defined as:
 *
 *	int dcd_probe(struct scsi_device *devp, int (*callback()))
 *
 * dcd_probe() only executes an inquiry.
 *
 * Both functions return one of the integer values as defined below:
 */

#define	DCDPROBE_EXISTS		0	/* device exists, inquiry data valid */
#define	DCDPROBE_NONCCS		1	/* device exists, no inquiry data */
#define	DCDPROBE_NORESP		2	/* device didn't respond */
#define	DCDPROBE_NOMEM		3	/* no space available for structures */
#define	DCDPROBE_FAILURE	4	/* polled cmnd failure- unspecified */
#define	DCDPROBE_BUSY		5	/* device was busy */
#define	DCDPROBE_NOMEM_CB	6
					/*
					 * no space available for structures
					 * but callback request has been
					 * queued
					 */
/*
 * The following are the defines for the dcd_options. The dcd_options
 * is a 32 bit quantity which is split in the following way to make it easier
 * for the current HBA driver to program it easily.
 * The following is the layout of the dcd_options bit.
 *
 * Bit 0, 1, 2 - Indicate the PIO mode that is requested.
 * Bit 3 and 4 - Indicate the DMA mode that is requested.
 * Bit 5 represent the support for Ultra ATA.
 * Bit 6 represent the support for Block Mode.
 * Bit 7 will indicate PIO or DMA (set means DMA).
 */


/* The following are the defines for the PIO Modes. */
#define	DCD_PIO_MODE0		0
#define	DCD_PIO_MODE1		1
#define	DCD_PIO_MODE2		2
#define	DCD_PIO_MODE3		3
#define	DCD_PIO_MODE4		4

/* The following are the defines for the DMA modes */
#define	DCD_MULT_DMA_MODE0	0
#define	DCD_MULT_DMA_MODE1	1
#define	DCD_MULT_DMA_MODE2	2


/* The following ate the generic defines for the dcd_options */
#define	DCD_BLOCK_MODE		0x40
#define	DCD_ULTRA_ATA		0x20
#define	DCD_DMA_MODE		0x80

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DCD_CONF_AUTOCONF_H */
