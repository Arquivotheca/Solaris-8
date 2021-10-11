/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FDISK_H
#define	_FDISK_H

#ident "@(#)fdisk.h   1.4   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		fdisk.h
 *
 *   Description:	contains data structures that pertain to the
 *			PC's fdisk table.
 *
 *
 */
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All rights reserved. 	*/

/*
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
 *	UNIX System Laboratories, Inc.
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */


#define	BOOTSZ		446	/* size of boot code in master boot block */
#define	FD_NUMPART	4	/* number of 'partitions' in fdisk table */
#define	MBB_MAGIC	0xAA55	/* magic number for mboot.signature */
#define	DEFAULT_INTLV	4	/* default interleave for testing tracks */
#define	MINPSIZE	4	/* minimum number of cylinders in a partition */
#define	TSTPAT		0xE5	/* test pattern for verifying disk */

#pragma	pack(1)

/*
 * structure to hold the fdisk partition table
 */
struct partentry {
	unsigned char bootid;	/* bootable or not */
	unsigned char beghead;	/* beginning head, sector, cylinder */
	unsigned char begsect;	/* begcyl is a 10-bit number. High 2 bits */
	unsigned char begcyl;	/* are in begsect. */
	unsigned char systid;	/* OS type */
	unsigned char endhead;	/* ending head, sector, cylinder */
	unsigned char endsect;	/* endcyl is a 10-bit number.  High 2 bits */
	unsigned char endcyl;	/* are in endsect. */
	long    relsect;	/* first sector relative to start of disk */
	long    numsect;	/* number of sectors in partition */
};
/*
 * Values for bootid.
 */
#define	NOTACTIVE	0
#define	ACTIVE		128
/*
 * Values for systid.
 */
#define	DOSOS12		1	/* DOS partition, 12-bit FAT */
#define	PCIXOS		2	/* PC/IX partition */
#define	DOSDATA		86	/* DOS data partition */
#define	DOSOS16		4	/* DOS partition, 16-bit FAT */
#define	EXTDOS		5	/* EXT-DOS partition */
#define	DOSHUGE		6	/* Huge DOS partition  > 32MB */
#define	OTHEROS		98	/* part. type for appl. needs raw partition */
/*
 * ID was 0 but conflicted with DOS 3.3 fdisk
 */
#define	UNIXOS		99	/* UNIX V.x partition */
#define	UNUSED		100	/* unassigned partition */
#define	SUNIXOS		130	/* Solaris UNIX partition */
#define	MAXDOS		65536L	/* max size (sectors) for DOS partition */

/*
 * structure to hold master boot block in physical sector 0 of the disk.
 */
struct  mboot {		/* master boot block */
	char	bootinst[BOOTSZ];
	struct	partentry parts[FD_NUMPART];  /* DOS has structure packing */
	unsigned short	signature;
};
#pragma pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _FDISK_H */
