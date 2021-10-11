/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DADA_IMPL_COMMANDS_H
#define	_SYS_DADA_IMPL_COMMANDS_H

#pragma ident	"@(#)commands.h	1.10	98/02/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Implementation dependent view of a ATA command descriptor block
 */

struct	dcd_cmd	{
	uchar_t	cmd;				/* The ATA command */
	uchar_t	address_mode;			/* Mode of addressing */
	uchar_t	direction;			/* Not SCSI to be indicated */
	uchar_t  features;			/* Any features to be enabled */
	uint_t	size;				/* size in bytes */
	uint_t	version;			/* version number */
	union {
		uint_t	lba_num;		/* LBA number if LBA */
						/* mode is used */
		struct	chs {
			ushort_t	cylinder;	/* Cylinder Number */
			uchar_t		head;		/* Head number */
			uchar_t		sector;		/* Sector Number */
		} chs_address;
	} sector_num;
};

#define	GETATACMD(cdb)	((cdb)->cmd)

/*
 * Direct Access Device Capacity Structure
 */
struct dcd_capacity {
	uchar_t		heads;
	uchar_t		sectors;
	ushort_t	ncyls;
	uint_t		capacity;
	uint_t		lbasize;
};


/* The following are the defines for the commands. */

#define	IDENTIFY		0xEC		/* Identify Device */
#define	IDENTIFY_DMA		0xEE		/* Identify DMA	   */
#define	ATA_RECALIBRATE		0x10		/* Recalibrate */
#define	ATA_READ		0x20		/* With retries */
#define	ATA_WRITE		0x30		/* With retries */
#define	ATA_SET_MULTIPLE 	0xC6		/* Set Multiple */
#define	ATA_READ_MULTIPLE	0xC4		/* Read Multiple */
#define	ATA_WRITE_MULTIPLE	0xC5		/* Write Multiple */
#define	ATA_READ_DMA		0xC9		/* Read Dma without retries */
#define	ATA_WRITE_DMA		0xCB		/* Write DMA without retry */
#define	ATA_SET_FEATURES	0xEF		/* Set features */


/* The following are the defines for the direction. */

#define	DATA_READ		0x01		/* READ from disk */
#define	DATA_WRITE		0x02		/* WRITE to disk */
#define	NO_DATA_XFER		0x00		/* No data xfer involved */

/* The following are the defines for the address mode */

#define	ADD_LBA_MODE		0x01		/* LBA Mode of addressing */
#define	ADD_CHS_MODE		0x02		/* Cylinder Head Sector mode */

/* The following are the usefull subcommands for set features command */
#define	ATA_FEATURE_SET_MODE 	0x03		/* This sets the mode */

/* The following are the masks which are used for enabling DMA or PIO */
#define	ENABLE_PIO_FEATURE	0x08		/* PIO with flow control */
#define	ENABLE_DMA_FEATURE 	0x20		/* Enable DMA */
#define	ENABLE_ULTRA_FEATURE 	0x40		/* Enable ULTRA DMA */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DADA_IMPL_COMMANDS_H */
