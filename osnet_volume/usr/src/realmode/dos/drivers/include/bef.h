/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/* The #ident directive confuses the DOS linker */
/*
#ident "@(#)bef.h	1.4	99/11/12 SMI"
*/

/*
 * Definitions for the realmode driver interface.
 */

#ifndef _BEF_H
#define	_BEF_H

/* Definitions for standard BIOS disk operations */
#define	BEF_RESET		0
#define	BEF_READ		2
#define	BEF_GETPARMS		8
#define	BEF_CHKEXT		0x41
#define	BEF_EXTREAD		0x42
#define	BEF_EXTWRITE		0x43
#define	BEF_EXTVERIFY		0x44
#define	BEF_EXTSEEK		0x47
#define	BEF_EXTGETPARMS		0x48

/* Definitions for BEF device operations */
#define	BEF_IDENT		0xF8

/* for BEF_EXTGETPARMS */

#define	MAXBLOCKS_FOR_GEOM	15482880L
#define	INFO_DMAERRS_HANDLED	0x0001
#define	INFO_PHYSGEOM_VALID	0x0002
#define INFO_REMOVABLE		0x0004
#define INFO_WRITE_VERIFY	0x0008
#define	INFO_MEDIA_CHANGE	0x0010
#define	INFO_LOCKABLE		0x0020
#define	INFO_NO_MEDIA		0x0040

/* 
 * This defines the Device Address Packet in Table 1 of the
 * BIOS Enhanced Disk Drive Specification, version 3.0
 */
	
struct ext_dev_addr_pkt {
	unchar	pktsize;
	unchar	reserved1;
	unchar	numblocks;
	unchar	reserved2;
	ulong	bufaddr;
	ulong	lba_addr_lo;
	ulong	lba_addr_hi;
	ulong	bigbufaddr_lo;
	ulong	bigbufaddr_hi;
};

/* 
 * Result buffer for the extended getparams (13/48h).  So far,
 * this structure only supports what's needed for EDD-1.1 compliance.
 */

struct ext_getparm_resbuf {
	ushort	bufsize;
	ushort	info_flags;
	ulong	cyls;
	ulong	heads;
	ulong	secs;
	ulong	num_secs_lo;
	ulong	num_secs_hi;
	ushort	bytes_per_sec;
	void __far *dpte;			/* Only if bufsize == 30 */
};

/* Definition for magic value returned by BEF_IDENT */
#define	BEF_MAGIC	0xBEF1

/* Definitions for ax error bits */
#define	BEF_UNDEF_ERROR	0xBB


extern void  pcopy(unsigned long src, unsigned long dest,
		unsigned short nbytes, unsigned short flags);

#define	RP_COPY8	0
#define	RP_COPY16	1
#define	RP_COPY32	2

#endif /* _BEF_H */
