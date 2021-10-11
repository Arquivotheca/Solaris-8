/*
 * Copyright (c) 1992-1994 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		bootp2s.h
 *
 *   Description:	The Solaris Primary-to-Secondary boot phase interface.
 *
 */

#ident	"@(#)bootp2s.h	1.2	94/08/09 SMI"

#ifndef	_BOOTP2S_H
#define	_BOOTP2S_H

/*
 * This structure defines the interface between the Solaris primary and
 * secondary boots.
 *
 * At the time the primary boot transfers control to the secondary boot,
 * the real mode (segment:offset) address of this structure is contained
 * in ES:DI.
 */

struct pri_to_secboot {
	long magic;
	char version[28];
	union {
	    struct {
		daddr_t bslice_start;
		    /* the absolute sector at the start of the boot slice */
		unsigned long bslice_size;
		    /* the size (in sectors) of the boot slice */
		daddr_t	Sol_start;
		    /* the abs sector at the start of the Solaris partition */
		unsigned long Sol_size;
		    /* the size (in sectors) of the Solaris partition */
		daddr_t	root_start;
		    /* the abs sector at the start of the UFS root filesystem */
		unsigned long root_size;
		    /* size (in sectors) of the root slice */
		unsigned long  root_slice;
		    /* slice number from vtoc v_part array */
		daddr_t	alts_start;
		    /* the abs sector at start of alts part */
		unsigned long alts_size;
		    /* size (in sectors) of the alts partn */
		unsigned long alts_slice;
		    /* slice number from vtoc v_part array */
		unsigned short boot_dev;
		    /* 00-01 floppy, 0x80-0x81 hard drive */
		unsigned short boot_ctlr;	/* controller number */
		unsigned short boot_unit;	/* i.e., LUN for SCSI systems */
		unsigned short bytPerSec;	/* bytes per sector */
		unsigned long secPerTrk;	/* sectors per track */
		unsigned long trkPerCyl;	/* heads(tracks) per cylinder */
		unsigned long ncyls;		/* number of cylinders */
		unsigned short boot_part;
		    /* number of partition we booted from */
		unsigned short filler;
	    } ufs;		/* end of ufs member */

	    struct {
		u_short	irq;		/* irq number */
		u_short ioaddr;		/* base io port address */
		u_short	membase;	/* base shared memory address */
		u_short memsize;	/* memory size in K bytes */
		/*
		 * left open for expansion...
		 */
	    } nfs;		/* end of nfs member */
	} bootfrom;	/* end of union */

	struct bdev_info F8;	/* extended device info from function F8 */
	char junk[16];
};

/*
 * WARNING: The term "Active" partition has a different meaning under Solaris!
 *
 * The typical primary boot paradigm:
	reads the fdisk table within the master boot record, and
	loads and executes the partition boot record from the first physical
		fdisk partition that it finds that is marked "Active".
 *
 * In this simple case, the "active" fdisk partition, and the "actively
 * running" partition are one and the same.
 *
 * Some current bootstraps provide users with the capability of booting
 * from any fdisk partition, not just the first one encountered that is
 * marked "active".  In this case, the "active" fdisk partition, and the
 * current partition may be physically different.
 *
 * The "active" partition we refer to here is the one that is currently
 * active, i.e., the Solaris partition is the active partition.
 */

#endif		/* _BOOTP2S_H */
