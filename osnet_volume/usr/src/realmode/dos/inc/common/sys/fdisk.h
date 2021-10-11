/*
 * Copyright (c) 1993, 1994, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)fdisk.h	1.12	98/05/06 SMI\n"

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

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FDISK_H
#define _SYS_FDISK_H


#define BOOTSZ		446	   /* size of boot code in master boot block */
#define FD_NUMPART	4	   /* number of 'partitions' in fdisk table */
#define MBB_MAGIC	0xAA55	/* magic number for mboot.signature */
#define DEFAULT_INTLV	4	/* default interleave for testing tracks */
#define MINPSIZE	4	      /* minimum number of cylinders in a partition */
#define TSTPAT		0xE5	   /* test pattern for verifying disk */

/*
 * structure to hold the fdisk partition table
 */
struct partentry {
	unsigned char bootid;	/* bootable or not */
	unsigned char beghead;	/* beginning head, sector, cylinder */
	unsigned char begsect;	/* begcyl is a 10-bit number. High 2 bits */
	unsigned char begcyl;	/*     are in begsect. */
	unsigned char systid;	/* OS type */
	unsigned char endhead;	/* ending head, sector, cylinder */
	unsigned char endsect;	/* endcyl is a 10-bit number.  High 2 bits */
	unsigned char endcyl;	/*     are in endsect. */
	long    relsect;	      /* first sector relative to start of disk */
	long    numsect;	      /* number of sectors in partition */
};
/*
 * Values for bootid.
 */
#define NOTACTIVE	0
#define ACTIVE		128
/*
 * Values for systid.
 */
#define	DOSOS12		1	/* DOS partition, 12-bit FAT */
#define	PCIXOS		2	/* PC/IX partition */
#define	DOSOS16		4	/* DOS partition, 16-bit FAT */
#define	EXTDOS		5	/* EXT-DOS partition */
#define	DOSHUGE		6	/* Huge DOS partition  > 32MB */
#define	IFS		7	/* IFS: HPFS or NTFS */
#define	AIXBOOT		8	/* AIX Boot */
#define	AIXDATA		9	/* AIX Data */
#define	OS2BOOT		10	/* OS/2 Boot Manager */
#define	WINDOWS		11	/* Windows 95 FAT32 (up to 2047GB) */
#define	EXT_WIN		12	/* Windows 95 FAT32 (extended-INT13) */
#define	FAT95		14	/* DOS 16-bit FAT, LBA-mapped */
#define	EXTLBA		15	/* Extended partition, LBA-mapped */
#define	DIAGPART	18	/* Diagnostic partition */
#define	LINUX		65	/* Linux */
#define	LINUXDSWAP	66	/* Linux swap (sharing disk with DRDOS) */
#define	LINUXDNAT	67	/* Linux native (sharing disk with DRDOS) */
#define	CPM		82	/* CP/M */
#define	DOSDATA		86	/* DOS data partition */
#define	OTHEROS		98	/* part. type for app (DB?) needs raw part */
            			/* ID was 0 but conflicts w/ DOS 3.3 fdisk */
#define	UNIXOS		99	/* UNIX V.x partition */
#define	NOVELL3		101	/* Novell Netware 3.x and later */
#define	QNX4		119	/* QNX 4.x */
#define	QNX42		120	/* QNX 4.x 2nd part */
#define	QNX43		121	/* QNX 4.x 3rd part */
#define	UNUSED		100	/* unassigned partition */
#define	SUNIXOS		130	/* Solaris UNIX partition */
#define	LINUXNAT	131	/* Linux native */
#define	NTFSVOL1	134	/* NTFS volume set 1 */
#define	NTFSVOL2	135	/* NTFS volume set 2 */
#define	BSD		165	/* BSD/386, 386BSD, NetBSD, FreeBSD, etc. */
#define	NEXTSTEP	167	/* NeXTSTEP */
#define	BSDIFS		183	/* BSDI file system */
#define	BSDISWAP	184	/* BSDI swap */
#define	X86BOOT		190	/* x86 Solaris boot partition */
#define	MAXDOS		65536L	/* max size (sectors) for DOS partition */

/*
 * structure to hold master boot block in physical sector 0 of the disk.
 * Note that partitions stuff can't be directly included in the structure
 * because of lameo '386 compiler alignment design.
 */

struct  mboot {            /* master boot block */
	char    bootinst[BOOTSZ];
/*   struct  partentry vparts[FD_NUMPART];  /* DOS has structure packing */  
	char    parts[FD_NUMPART * sizeof(struct partentry)];
	ushort   signature;
};

#endif	/* _SYS_FDISK_H */
