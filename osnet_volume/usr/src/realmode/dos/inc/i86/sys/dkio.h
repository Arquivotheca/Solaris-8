/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)dkio.h	1.6	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		dkio.h
 *
 *   Description:	disk ioctl data structures and definitions.
 *
 */

#ifndef _SYS_DKIO_H
#define	_SYS_DKIO_H

#include <sys/dklabel.h>	/* Needed for DKNMAP define */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structures and definitions for disk io control commands
 */

/*
 * Structures used as data by ioctl calls.
 */

#define	DK_DEVLEN	16		/* device name max length, including */
					/* unit # & NULL (ie - "xyc1") */

/*
 * Used for controller info
 */
struct dk_cinfo {
	char	dki_cname[DK_DEVLEN];	/* controller name (no unit #) */
	u_short	dki_ctype;		/* controller type */
	u_short	dki_flags;		/* flags */
	u_short	dki_cnum;		/* controller number */
	u_int	dki_addr;		/* controller address */
	u_int	dki_space;		/* controller bus type */
	u_int	dki_prio;		/* interrupt priority */
	u_int	dki_vec;		/* interrupt vector */
	char	dki_dname[DK_DEVLEN];	/* drive name (no unit #) */
	u_int	dki_unit;		/* unit number */
	u_int	dki_slave;		/* slave number */
	u_short	dki_partition;		/* partition number */
	u_short	dki_maxtransfer;	/* max. transfer size in DEV_BSIZE */
};

/*
 * Controller types
 */
#define	DKC_UNKNOWN	0
#define	DKC_CDROM	1	/* CD-ROM, SCSI or otherwise */
#define	DKC_WDC2880	2
#define	DKC_XXX_0	3	/* unassigned */
#define	DKC_XXX_1	4	/* unassigned */
#define	DKC_DSD5215	5
#define	DKC_XY450	6
#define	DKC_ACB4000	7
#define	DKC_MD21	8
#define	DKC_XXX_2	9	/* unassigned */
#define	DKC_NCRFLOPPY	10
#define	DKC_XD7053	11
#define	DKC_SMSFLOPPY	12
#define	DKC_SCSI_CCS	13	/* SCSI CCS compatible */
#define	DKC_INTEL82072	14	/* native floppy chip */
#define	DKC_PANTHER	15
#define	DKC_SUN_IPI1	DKC_PANTHER	/* Sun Panther VME/IPI controller */
#define	DKC_MD		16	/* meta-disk (virtual-disk) driver */
#define	DKC_CDC_9057	17	/* CDC 9057-321 (CM-3) IPI String Controller */
#define	DKC_FJ_M1060	18	/* Fujitsu/Intellistor M1060 IPI-3 SC */
#define	DKC_INTEL82077	19	/* 82077 floppy disk controller */

/*
 * Sun reserves up through 1023
 */

#define	DKC_CUSTOMER_BASE	1024

/*
 * Flags
 */
#define	DKI_BAD144	0x01	/* use DEC std 144 bad sector fwding */
#define	DKI_MAPTRK	0x02	/* controller does track mapping */
#define	DKI_FMTTRK	0x04	/* formats only full track at a time */
#define	DKI_FMTVOL	0x08	/* formats only full volume at a time */
#define	DKI_FMTCYL	0x10	/* formats only full cylinders at a time */
#define	DKI_HEXUNIT	0x20	/* unit number is printed as 3 hex digits */

/*
 * Used for all partitions
 */
struct dk_allmap {
	struct dk_map	dka_map[NDKMAP];
};

/*
 * Definition of a disk's geometry
 */
struct dk_geom {
	unsigned short	dkg_ncyl;	/* # of data cylinders */
	unsigned short	dkg_acyl;	/* # of alternate cylinders */
	unsigned short	dkg_bcyl;	/* cyl offset (for fixed head area) */
	unsigned short	dkg_nhead;	/* # of heads */
	unsigned short	dkg_obs1;	/* obsolete */
	unsigned short	dkg_nsect;	/* # of data sectors per track */
	unsigned short	dkg_intrlv;	/* interleave factor */
	unsigned short	dkg_obs2;	/* obsolete */
	unsigned short	dkg_obs3;	/* obsolete */
	unsigned short	dkg_apc;	/* alternates per cyl (SCSI only) */
	unsigned short	dkg_rpm;	/* revolutions per minute */
	unsigned short	dkg_pcyl;	/* # of physical cylinders */
	unsigned short	dkg_write_reinstruct;	/* # sectors to skip, writes */
	unsigned short	dkg_read_reinstruct;	/* # sectors to skip, reads */
	unsigned short	dkg_extra[7];	/* for compatible expansion */
};
/*
 * These defines are for historic compatibility with old drivers.
 */
#define	dkg_bhead	dkg_obs1	/* used to be head offset */
#define	dkg_gap1	dkg_obs2	/* used to be gap1 */
#define	dkg_gap2	dkg_obs3	/* used to be gap2 */

/*
 * Disk io control commands
 * Warning: some other ioctls with the DIOC prefix exist elsewhere.
 * The Generic DKIOC numbers are from	0   -  50.
 *	The Floppy Driver uses		51  - 100.
 *	The Hard Disk (except SCSI)	101 - 150.
 *	The CDROM Driver		151 - 200.
 *	The USCSI ioctl			201 - 250.
 */
#define	DKIOC		(0x04 << 8)
/*
 * The following ioctls are generic in nature and need to be
 * suported as appropriate by all disk drivers
 */
#define	DKIOCGGEOM	(DKIOC|1)		/* Get geometry */
#define	DKIOCINFO	(DKIOC|3)		/* Get info */
#define	DKIOCEJECT	(DKIOC|6)		/* Generic 'eject' */
#define	DKIOCGVTOC	(DKIOC|11)		/* Get VTOC */
#define	DKIOCSVTOC	(DKIOC|12)		/* Set VTOC & Write to Disk */

/*
 * The following ioctls are used by Sun drivers to communicate
 * with their associated format routines. Support of these ioctls
 * is not required of foreign drivers
 */
#define	DKIOCSGEOM	(DKIOC|2)		/* Set geometry */
#define	DKIOCSAPART	(DKIOC|4)		/* Get all partitions */
#define	DKIOCGAPART	(DKIOC|5)		/* Set all partitions */
#define DKIOCG_PHYGEOM	(DKIOC|32)		/* get physical geometry */
#define DKIOCG_VIRTGEOM	(DKIOC|33)		/* get virtual geometry */

/*
 * The following ioctl's are removable media support
 */
#define	DKIOCLOCK	(DKIOC|7)	/* Generic 'lock' */
#define	DKIOCUNLOCK	(DKIOC|8)	/* Generic 'unlock' */

/*
 * Ioctl to force driver to re-read the alternate partition and rebuild
 * the internal defect map.
 */
#define DKIOCADDBAD	(DKIOC|20)	/* Re-read the alternate map (IDE) */
#define DKIOCGETDEF	(DKIOC|21)	/* read defect list (IDE)	   */

/*
 * Used by applications to get disk defect information from IDE
 * drives.
 */
struct defect_header {
	int		head;
	unsigned char	*buffer;
};

#define DKIOCPARTINFO	(DKIOC|22)	/* Get partition or slice parameters */

/*
 * Used by applications to get partition or slice information
 */
struct part_info {
	daddr_t		p_start;
	int		p_length;
};


#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKIO_H */

