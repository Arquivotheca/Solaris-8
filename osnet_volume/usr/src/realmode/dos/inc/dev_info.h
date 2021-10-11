/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dev_info.h	1.14	97/11/14 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *	File name:		dev_info.h
 *
 *	Description:	This file contains the layout of the dev_info structure.
 *		An instance of this structure is initialized for each
 *		device detected during the MDB boot phase.
 *
 */

#ifndef _DEV_INFO_H
#define	_DEV_INFO_H

#define	MAX_BOOTPATH_LEN 80

#pragma pack (1)

struct bdev_info { /* Structure for storing information about MDB devices */
	/*
	 * The first few items here can also be treated as a single
	 * string.  Do not put anything else in here unless it is
	 * intended to be part of the string.
	 */
	unchar	vid[8];		/* Vendor ID from SC_INQUIRY command */
	unchar	blank1;		/* Blank character to separate fields */
	unchar	pid[16];	/* Product ID from SC_INQUIRY command */
	unchar	blank2;		/* Blank character to separate fields */
	unchar	prl[4];		/* Product revision level from SC_INQUIRY */
	unchar	term;		/* Null terminator for string */
	/* End of composite string */
	unchar	version;	/* Version id: see values below */

	/*
	 * The following union contains a component structure for each type of
	 * device that is supported by the MDB spec.  We currently support
	 * SCSI HBA's and network adapters.
	 */
	ushort	base_port;	/* Base I/O address of adapter */
	union {
		struct { /* structure for information about SCSI devices */
			ushort	bsize;	/* Device block size */
			unchar	targ;	/* SCSI target number */
			unchar	lun;	/* Device logical unit number */
			unchar	pdt;	/* Peripheral device type */
			unchar	dtq;	/* Device type qualifier */
		} scsi;		/* end of SCSI section */

		struct { /* structure for information about network devices */
			ushort	irq_level;	/* IRQ level used by device */
			ushort	mem_base;	/* Seg addr of base memory */
			ushort	mem_size;	/* Size of base memory */
			short	index;		/* multi-purpose field: */
						/* MC/EISA: slot number */
						/* ISA: index into port array */
		} net;		/* end of network section */
	} MDBdev;	/* end of union */
	unchar	dev_type;	/* indicates type of MDB device */
	unchar	bios_dev;	/* BIOS device ID number */
	unchar	hba_id[8];	/* HBA ID string */
	unchar	pci_valid;	/* pci fields valid indicator */
	unchar	pci_bus;	/* pci bus number */
	ushort	pci_ven_id;	/* pci vendor id */
	ushort	pci_vdev_id;	/* pci vendors device id */
	unchar	pci_dev;	/* pci device (on bus) */
	unchar	pci_func;	/* pci function (of devno) */
	char	user_bootpath[MAX_BOOTPATH_LEN]; /* user supplied bootpath */
	ushort	cyls;		/* Bios boot device geometry */
	unchar	heads;		/* ... */
	unchar	secs;		/* ... only set if handling boot device */
	unchar	bdev;		/* device BIOS booted us from */
	unchar	spare_align;
	ushort	misc_flags;	/* miscellaneous flag bits: see below */

/* Add any new members above this comment.  Add new version definition below */

	char	junk[8];	/* DOS alignment problem insurance */
};

#pragma pack ()

typedef struct bdev_info DEV_INFO;

#define	MDB_SCSI_HBA	0x01
#define	MDB_NET_CARD	0x02

#define	FIRST_BOOT_DEVNUM 0x10

/* values for inqd_pdt: */
#define	INQD_PDT_DA	0x00	/* Direct-access (DISK) device */
#define	INQD_PDT_SEQ    0x01    /* Sequential-access (TAPE) device */
#define	INQD_PDT_ROM    0x05    /* Read-only directe-access device */

/*
 * Values for the version stamp.  New members are always added to the
 * end of the structure (i.e. before the "junk" alignment field).
 * The version stamp names reflect the last non-junk member.  When
 * adding a new version, update MDB_VERS_CURRENT to match the
 * maximum value.
 *
 * Producers of dev_info structures (typically realmode drivers) will
 * normally set the version to MDB_VERS_CURRENT.  Consumers must
 * examine the version stamp before referencing any field beyond
 * hba_id.
 */
#define MDB_VERS_HBA_ID		0
#define	MDB_VERS_BDEV		1
#define	MDB_VERS_MISC_FLAGS	2

#define	MDB_VERS_CURRENT	2

/*
 * Miscellaneous flag bits.
 */
#define	MDB_MFL_DIRECT	1	/* direct-attached mass storage device */

#endif	/* _DEV_INFO_H */
