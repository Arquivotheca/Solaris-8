#ifndef _BEF_H
#define _BEF_H

/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)bef.h	1.14	99/03/29 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework Header
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *    File name: bef.h
 *
 * This file contains definitions for BEF device code assignments.
 * (used by all MDB devices).
 *
 */

typedef void (far *recv_callback_t)(ushort);

/*
 *[]------------------------------------------------------------[]
 * |                       PROTOTYPE AREA                       |
 *[]------------------------------------------------------------[]
 */
#include <dev_info.h>
#include <dostypes.h>
int	dev_init(DEV_INFO *);
int	dev_read(DEV_INFO *, long, ushort, ushort, ushort);
int	dev_sense(ushort, ushort, ushort);
int	dev_readcap(DEV_INFO *);
int	dev_motor(DEV_INFO *, int);
int	dev_lock(DEV_INFO *, int);
int	dev_inquire(ushort, ushort, ushort);
int	init_dev(ushort, ushort);
void	outl(ushort, ulong);
ulong	inl(ushort);

/* ---- The following routines are for the network framework only ---- */
void	AdapterIdentify(short, ushort *, ushort *, ushort *,
			ushort *);
void	AdapterReceive(unchar far *, ushort, recv_callback_t);
void	AdapterSend(char far *, ushort);
void	AdapterClose(void);
void	AdapterInterrupt(void);
int	ISAAddr(short, short *, short *);
int	ISAProbe(short);
void	ISAIdentify(short, short *, short *, short *);
int	InstallConfig(void);
void	dev_netboot(short, ushort, ushort, ushort, ushort, unchar,
		    struct bdev_info *info);

/* ---- end of network framework prototypes ---- */

/* ---- support routines for PCI buses ---- */
int	pci_find_device(ushort, ushort, ushort, ushort *);
int	pci_read_config_byte(ushort, ushort, unchar *);
int	pci_read_config_word(ushort, ushort, ushort *);
int	pci_read_config_dword(ushort, ushort, ulong *);
/* ---- end of pci prototypes ---- */
/*
 *[]------------------------------------------------------------[]
 * |                    END OF PROTOTYPE AREA			|
 *[]------------------------------------------------------------[]
 */

#define FIRSTDEV   0x10                 /* First device code to try */
#define STOPDEV    0                    /* Out of codes if reached  */
#define SKIPDEV(x) (((x)&0xF0)==0x80)   /* Codes to skip            */

/* Definitions for BEF device operations */
#define BEF_IDENT  0xF8                 /* Ident function code      */
#define BEF_RDBLKS 0xF9                 /* Read blocks function     */

/* Definitions for standard BIOS disk operations */
#define BEF_RESET        	0x00
#define BEF_READ         	0x02
#define BEF_GETPARMS     	0x08
#define	BEF_CHKEXT		0x41
#define	BEF_EXTREAD		0x42
#define	BEF_EXTWRITE		0x43
#define	BEF_EXTVERIFY		0x44
#define	BEF_EXTSEEK		0x47
#define	BEF_EXTGETPARMS		0x48

/* Definition for magic value returned by BEF_IDENT */
#define BEF_MAGIC	 0xBEF1

/* Definitions for ax error bits */
#define NO_SUCH_BLOCK    4
#define UNDEF_ERROR      0xBB

/* Blocksize 0 implies unknown/unchecked */
#define UNKNOWN_BSIZE	0

/* default geometry (if not overridden by BIOS) for (EXT)GETPARMS */

#define DEFAULT_NUMHEADS	256
#define	DEFAULT_NUMDRIVES	1
#define DEFAULT_NUMCYLS		1024
#define	DEFAULT_NUMSECS		63

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

#endif /* _BEF_H */
