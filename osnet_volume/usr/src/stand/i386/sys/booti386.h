/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_BOOTI386_H
#define	_SYS_BOOTI386_H

#pragma ident	"@(#)booti386.h	1.7	96/01/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/* definitions for generic AT(386) hard/floppy disk boot code */

#ifdef BOOTDEBUG
#define	debug(x)	x
#else
#define	debug(x)
#endif

#define	TRUE	1
#define	FALSE	0

#define	NULL	0

#define	HINBL	(unchar)0xF0

/* Definitions for boot devices */
#define	BOOT_FROM_DISK		0
#define	BOOT_FROM_NET		1

/* Definitions for micro-channel HD type, Should be in cram.h */

#define	MC_FD0TB	0x11	/* Drive 0 type */
#define	MC_FD1TB	0x12	/* Drive 1 type */

/*	Definitions for key BIOS ram loactions */

#define	FDBvect	*(ulong *)FDB_ADDR	/* Floppy base parameter table ptr */
#define	HD0p	(faddr_t *)HD0V_ADDR	/* hard disk 0 parameter table ptr */
#define	HD1p	(faddr_t *)HD1V_ADDR	/* hard disk 1 parameter table pte */
#define	NUMHD()	*(short *)NUMHD_ADDR	/* number of HD drives		   */
#define	MEM_BASE() *(short *)MEMBASE_ADDR	/* Base memory size	   */
#define	COMM_B(x)  *(short *)(COMM_ADDR + (x-1)) /* base addr for com port */
#define	LPT_B(x)   *(short *)(LPT_ADDR + (x-1)) /* base addr for lpt port  */

#define	segoftop(s, o)	(paddr_t)((uint)(s<<4) + o)

/*	To read a word(short) from CMOS */

#define	CMOSreadwd(x)	(ushort) ((CMOSread(x+1) << 8)+CMOSread(x))

/* Real-Mode memory access related defines/structures */
#define	BOOTMEMBLKSIZE	1024

struct	bootmemgap {
	unsigned short  size;	/* size including this struct as header */
	caddr_t		start_addr;
	struct bootmemgap *next;
};

/* Macros for conversion of flat addrs to segment/offset style addrs */
#define	segpart(ea)	((ea)/0x10)
#define	offpart(ea)	((ea)%0x10)
#define	mk_farp(pp)	((unsigned long)((segpart(pp) << 16) | offpart(pp)))
#define	MK_FP(seg, off)	((unsigned long)(((seg) << 16) | (off)))

/* Macros for dealing with segment/offset style addrs */
#define	FARP_SEGMASK	0xFFFF0000
#define	FARP_OFFMASK	0x0000FFFF
#define	mk_ea(seg, off)	((unsigned long)(((seg) << 4) + (off)))
#define	mk_flatp(farp)	((((unsigned long)(farp) & FARP_SEGMASK) >> 12) + \
	((unsigned long)(farp) & FARP_OFFMASK))
#define	segpart_fp(a)	((a) >> 16)
#define	offpart_fp(a)	((a) & 0xff)

/* Important MDB boot defines */
#define	BEF_SIG	0x5A4D		/* BEF/(exe) header signature bytes */
#define	BEF_LOAD_AREA_SIZE 0x20000 /* Size of area we reserve to load bef's */

#pragma	pack(1)
struct	hdpt	{		/* hard disk parameter table entry */
	unsigned short	mxcyl;
	unsigned char	mxhead;
	unsigned char	dmy1[2];
	unsigned short	precomp;
	unsigned char	mxecc;
	unsigned char	drvcntl;
	unsigned char	dmy2[3];
	unsigned short	lz;
	unsigned char	spt;
	unsigned char	dmy3;
};

struct	fdpt	{		/* floppy disk parameter table entry */
	unsigned char	step;
	unsigned char	load;
	unsigned char	motor;
	unsigned char	secsiz;
	unsigned char	spt;
	unsigned char	dgap;
	unsigned char	dtl;
	unsigned char	fgap;
	unsigned char	fill;
	unsigned char	headsetl;
	unsigned char	motrsetl;
	unsigned char	mxtrk;
	unsigned char	dtr;
};
#pragma pack()

#define	physaddr(x)	(paddr_t)(x)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTI386_H */
