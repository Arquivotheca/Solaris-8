/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SD_DDI_H
#define	_SD_DDI_H

#pragma ident	"@(#)sd_ddi.h	1.7	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These command codes need to be moved to sys/scsi/generic/commands.h
 */

/* Group 2 Mode Sense/Select commands */
#define	SCMD_MODE_SENSE2	0x5A
#define	SCMD_MODE_SELECT2	0x55

/* Both versions of the Read CD command */

/* the official SCMD_READ_CD now comes from cdio.h */
#define	SCMD_READ_CDD4		0xd4	/* the one used by some first */
					/* generation ATAPI CD drives */

#define	SCMD_SYNCHRONIZE_CACHE	0x35

/* expected sector type filter values for Play and Read CD CDBs */
#define	CDROM_SECTOR_TYPE_CDDA		(1<<2)	/* IEC 908:1987 (CDDA) */
#define	CDROM_SECTOR_TYPE_MODE1		(2<<2)	/* Yellow book 2048 bytes */
#define	CDROM_SECTOR_TYPE_MODE2		(3<<2)	/* Yellow book 2335 bytes */
#define	CDROM_SECTOR_TYPE_MODE2_FORM1	(4<<2)	/* 2048 bytes */
#define	CDROM_SECTOR_TYPE_MODE2_FORM2	(5<<2)	/* 2324 bytes */

/* READ CD filter bits (cdb[9]) */
#define	CDROM_READ_CD_SYNC	0x80	/* read sync field */
#define	CDROM_READ_CD_HDR	0x20	/* read four byte header */
#define	CDROM_READ_CD_SUBHDR	0x40	/* read sub-header */
#define	CDROM_READ_CD_ALLHDRS	0x60	/* read header and sub-header */
#define	CDROM_READ_CD_USERDATA	0x10	/* read user data */
#define	CDROM_READ_CD_EDC_ECC	0x08	/* read EDC and ECC field */
#define	CDROM_READ_CD_C2	0x02	/* read C2 error data */
#define	CDROM_READ_CD_C2_BEB	0x04	/* read C2 and Block Error Bits */

#define	SCMD_SET_CDROM_SPEED	0xbb	/* Set CD Speed command */


/*
 * These belong in sys/scsi/generic/mode.h
 */


/*
 * Mode Sense/Select Header response for Group 2 CDB.
 */

struct mode_header_grp2 {
	uchar_t length_msb;	/* MSB - number of bytes following */
	uchar_t length_lsb;
	uchar_t medium_type;	/* device specific */
	uchar_t device_specific; /* device specfic parameters */
	uchar_t resv[2];	/* reserved */
	uchar_t bdesc_length_hi; /* length of block descriptor(s), if any */
	uchar_t bdesc_length_lo;
};

/*
 * Length of the Mode Parameter Header for the Group 2 Mode Select command
 */
#define	MODE_HEADER_LENGTH_GRP2	(sizeof (struct mode_header_grp2))
#define	MODE_PARAM_LENGTH_GRP2 (MODE_HEADER_LENGTH_GRP2 + MODE_BLK_DESC_LENGTH)

/*
 * Mode Page 1 - Error Recovery Page
 */
#define	MODEPAGE_ERR_RECOVER	1


#ifdef	__cplusplus
}
#endif

#endif	/* _SD_DDI_H */
