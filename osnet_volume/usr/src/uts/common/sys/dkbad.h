/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKBAD_H
#define	_SYS_DKBAD_H

#pragma ident	"@(#)dkbad.h	2.12	98/01/06 SMI"	/* UCB 4.3 81/05/10 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions needed to perform bad sector
 * revectoring ala DEC STD 144.
 *
 * The bad sector information is located in the
 * first 5 even numbered sectors of the last
 * track of the disk pack.  There are five
 * identical copies of the information, described
 * by the dkbad structure.
 *
 * Replacement sectors are allocated starting with
 * the first sector before the bad sector information
 * and working backwards towards the beginning of
 * the disk.  A maximum of 126 bad sectors are supported.
 * The position of the bad sector in the bad sector table
 * determines which replacement sector it corresponds to.
 *
 * The bad sector information and replacement sectors
 * are conventionally only accessible through the
 * 'c' file system partition of the disk.  If that
 * partition is used for a file system, the user is
 * responsible for making sure that it does not overlap
 * the bad sector information or any replacement sector.s
 */

#define	NDKBAD		126		/* # of entries maximum */

struct dkbad {
	long	bt_csn;			/* cartridge serial number */
	ushort_t bt_mbz;		/* unused; should be 0 */
	ushort_t bt_flag;		/* -1 => alignment cartridge */
	struct bt_bad {
		short	bt_cyl;		/* cylinder number of bad sector */
		short	bt_trksec;	/* track and sector number */
	} bt_bad[NDKBAD];
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKBAD_H */
