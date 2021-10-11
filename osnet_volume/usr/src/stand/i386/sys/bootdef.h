/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BOOTDEF_H
#define	_SYS_BOOTDEF_H

#pragma ident	"@(#)bootdef.h	1.20	99/10/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*	Definitions for key BIOS ram locations */

#define	FDB_ADDR 	0x78	/* Floppy base parameter table 		*/
#define	HD0V_ADDR 	0x104	/* hard disk drive 0 vector address	*/
#define	HD1V_ADDR 	0x118	/* hard disk drive 1 vector address	*/
#define	NUMHD_ADDR	0x475	/* number of HD drives 			*/
#define	MEMBASE_ADDR	0x413	/* Base memory size 			*/
#define	COMM_ADDR	0x400 	/* base address for communication port	*/
#define	LPT_ADDR	0x408 	/* base address for lpt port		*/

#define	RESERVED_SIZE	0x03000 /* size of the initial page tables 	*/

#define	BOOTDRV_MASK	0x80	/* mask for bootdriv 			*/
#define	BOOTDRV_HD	0x80	/* bootdriv - harddisk 			*/
#define	BOOTDRV_FP	0x00	/* bootdriv - floppy diskette		*/

#ifdef ELF
#define	BLP_ADDR	V0x3000	/* memory location for BLP		*/
#else
#define	BLP_ADDR	0x3000	/* memory location for BLP		*/
#endif
#define	SECBOOT_RELOCADDR	0x110000  /* Relocation address for non-srt0 */
#define	SECBOOT_ADDR	0x2e00	/* memory location for secondary boot	*/
#define	PRIBOOT_ADDR	0x7c00	/* memory location for primary boot 	*/
#define	SECBOOT_STACKSIZ 1024	/* secondary boot stack size		*/
#define	SECBOOT_STACKLOC 0x7FFC /* secondary boot stack top */
#define	SBOOT_INTSTACKLOC 0x4000 /* secondary boot interrupt stack top */
#define	SBOOT_NEWINTSTACKLOC 0x2000 /* secondary boot interrupt stack top */
#define	SBOOT_FARSTACKLOC 0x6000 /* secondary boot interrupt stack top */
#define	PROG_GAP	1024	/* reserved 1K between loadable module	*/

/*	fdisk information						*/
/*	B_BOOTSZ, B_ACTIVE and B_FD_NUMPART are from sys/fdisk.h	*/
#define	B_BOOTSZ	446	/* size of master boot in hard disk	*/
#define	B_FD_NUMPART	4	/* number of fdisk partitions		*/
#define	B_ACTIVE	0x80	/* active partition			*/

/*	disk buffer definition						*/
#define	GBUFSZ		(9*1024)	/* global disk buffer size	*/

/*	stack index of input parameters from priboot boot		*/
#define	STK_SBML	1	/* secboot_mem_loc			*/
#define	STK_SPC		2	/* spc					*/
#define	STK_SPT		3	/* spt					*/
#define	STK_BPS		4	/* bps					*/
#define	STK_EDS		5	/* entry ds:si location			*/
#define	STK_AP		6	/* active partition pointer		*/
#define	LOW_REGMASK	0x00FF	/* used to initialize register hibyte	*/

/*	ELF file header	identification					*/
#define	ELFMAGIC 0x457f

/*	routine return status code					*/
#define	SUCCESS	0
#define	FAILURE	1

/*	video segment address						*/
#define	EGA_SEGMENT	(unsigned short) 0xC000

/*	control register CR0 bit definition				*/
/*	full details in reg.h						*/
#define	CR0_PG		0x80000000
#define	PROTMASK	0x1
#define	NOPROTMASK	0x7ffffffe

/*	GDT definitions							*/
#define	B_GDT		0x08	/* big flat data descriptor		*/
#define	C_GDT		0x10	/* flat code descriptor			*/
#define	C16GDT		0x18	/* use 16 code descriptor		*/
#define	D_GDT		0x20	/* flat data descriptor			*/
#define	TSS1_GDT	0x28	/* normal TSS descriptor		*/
#define	TSS2_GDT	0x30	/* double fault TSS descriptor		*/

/*	Kernel paging data						*/
#define	KPD_LOC	0x2000

/*	BKI magic - from bootinfo.h					*/
#define	B_BKI_MAGIC	0xff1234ff
/*	selector definition - from seg.h				*/
#define	KTSSSEL		0x150	/* TSS for the scheduler		*/
#define	JTSSSEL		0x170

/*	disk read ecc code						*/
#define	ECC_COR_ERR	0x11    /* ECC corrected disk error 		*/
#define	I13_SEK_ERR	0x40	/* seek failed				*/
#define	I13_TMO_ERR	0x80	/* timeout (not ready)			*/

#define	RD_RETRY	0x3	/* retry count				*/
#define	FD_ADAPT_LEV	0x2	/* fd read adaptive level		*/

/*	memory test pattern						*/
#define	MEMTEST0	(ulong)0x00000000
#define	MEMTEST1	(ulong)0xA5A5A5A5
#define	MEMTEST2	(ulong)0x5A5A5A5A
#define	MEM16M		(ulong)0x1000000

/* Real and protected mode boundary values */
#define	TOP_RMMEM	0xA0000		/* Max possible RM RAM address */
#define	BOT_PMMEM	0x100000	/* Start of protected mode RAM */
#define	END_RMVIS	0x10FFF0	/* First non-RM visible address */

/* Allocated workspace expected by the configuration manager */
#define	CMWS_SIZE	((size_t)0X10000)
#define	CMWS_ADDR	((caddr_t)0x60000)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTDEF_H */
