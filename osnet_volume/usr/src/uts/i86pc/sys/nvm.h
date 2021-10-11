/*
 *  Copyrighted as an unpublished work.
 *  (c) Copyright 1990 INTERACTIVE Systems Corporation
 *  All rights reserved.
 *
 *  RESTRICTED RIGHTS
 *
 *  These programs are supplied under a license.  They may be used,
 *  disclosed, and/or copied only as permitted under such license
 *  agreement.  Any copy must contain the above copyright notice and
 *  this restricted rights notice.  Use, copying, and/or disclosure
 *  of the programs is strictly prohibited unless otherwise provided
 *  in the license agreement.
 */

#ifndef _SYS_NVM_H
#define	_SYS_NVM_H

#pragma ident	"@(#)nvm.h	1.3	94/03/31 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	(C) Copyright COMPAQ Computer Corporation 1985, 1989
 *
 *   Title:	EISA Configuration
 *
 *   Module:	NVM.H
 *
 *   Version:	1.00
 *
 *   Date:
 *
 *   Author:	Ali Ezzet
 *
 *   Change Log:
 *
 * Date	    DevSTR  Comment
 * --------  ------  ----------------------------------------------------------
 * mm/dd/yy  abc000
 *
 * 10/13/89  AE392	Definition change for freeform data handling.
 *
 * 11/01/89  AExx1	Clean up for free form data.
 *
 * 01/03/89  RSG654	Change function declaration for NVM_setup_totalmem.
 *
 *
 *   Functional Description:
 *
 */

#define	NVM_MAX_SELECTIONS	26
#define	NVM_MAX_TYPE		80
#define	NVM_MAX_MEMORY		9
#define	NVM_MAX_IRQ		7
#define	NVM_MAX_DMA		4
#define	NVM_MAX_PORT		20
#define	NVM_MAX_INIT		60
#define	NVM_MAX_DATA		203

#pragma pack(1)

/* Duplicate ID information byte format */

typedef
struct nvm_dupid	{
	unsigned int	dup_id	:4,	/* Duplicate ID number */
			type	:2,	/* Slot type */
			readid	:1,	/* Readable ID */
			dups	:1,	/* Duplicates exist */
			disable :1,	/* Board disable is supported */
			IOcheck :1,	/* EISA I/O check supported */
				:5,	/* Reserved */
			partial :1;	/* Configuration incomplete */
} NVM_DUPID;

/* Function information byte	*/

typedef
struct nvm_fib	{
	unsigned int	type	:1,	/* Type string present		*/
			memory	:1,	/* Memory configuration present */
			irq	:1,	/* IRQ configuration present	*/
			dma	:1,	/* DMA configuration present	*/
			port	:1,	/* Port configuration present	*/
			init	:1,	/* Port initialization present	*/
			data	:1,	/* Free form data		*/
			disable :1;	/* Function is disabled 	*/
} NVM_FIB;

/* Returned information from an INT 15h call "Read Slot" (AH=D8h, AL=0) */

typedef
struct nvm_slotinfo	{
	unsigned char		boardid[4];	/* Compressed board ID */
	unsigned short		revision;	/* Utility version */
	unsigned char		functions;	/* Number of functions */
	NVM_FIB 		fib;		/* Function information byte */
	unsigned short		checksum;	/* CFG checksum */
	NVM_DUPID		dupid;		/* Duplicate ID information */
} NVM_SLOTINFO;

typedef
struct nvm_memory	{
	struct	{
		unsigned int	write	:1,	/* Memory is read only */
				cache	:1,	/* Memory is cached */
					:1,	/* Reserved */
				type	:2,	/* Memory type */
				share	:1,	/* Shared Memory */
					:1,	/* Reserved */
				more	:1;	/* More entries follow */
	} config;

	struct	{
		unsigned int	width	:2,	/* Data path size */
				decode	:2;	/* Address decode */
	} datapath;

	unsigned char		start[3];	/* Start address DIV 100h */
	unsigned short		size;		/* Memory size in 1K bytes */
} NVM_MEMORY;

typedef
struct nvm_irq {
	unsigned int	line	:4,	/* IRQ line */
				:1,	/* Reserved */
			trigger :1,	/* Trigger (EGDE=0, LEVEL=1) */
			share	:1,	/* Sharable */
			more	:1,	/* More follow */
				:8;	/* Reserved */
} NVM_IRQ;

typedef
struct nvm_dma		{
	unsigned int	channel :3,	/* DMA channel number */
				:3,	/* Reserved */
			share	:1,	/* Shareable */
			more	:1,	/* More entries follow */
				:2,	/* Reserved */
			width	:2,	/* Transfer size */
			timing	:2,	/* Transfer timing */
				:2;	/* Reserved */
} NVM_DMA;

typedef
struct nvm_port {
	unsigned int	count	:5,	/* Number of sequential ports - 1 */
				:1,	/* Reserved */
			share	:1,	/* Shareable */
			more	:1;	/* More entries follow */
	unsigned short	address;	/* IO port address */
} NVM_PORT;

typedef
struct nvm_init {
	unsigned int		type	:2,	/* Port type */
				mask	:1,	/* Apply mask */
					:4,	/* Reserved */
				more	:1;	/* More entries follow */

	unsigned short		port;		/* Port address */

	union	{
		struct	{
			unsigned char	value;	/* Byte to write */
		} byte_v;

		struct	{
			unsigned char	value,	/* Byte to write */
					mask;	/* Mask to apply */
		} byte_vm;

		struct	{
			unsigned short	value;	/* Word to write */
		} word_v;

		struct	{
			unsigned short	value,	/* Word to write */
					mask;	/* Mask to apply */
		} word_vm;

		struct	{
			unsigned long	value;	/* Dword to write */
		} dword_v;

		struct	{
			unsigned long	value,	/* Dword to write */
					mask;	/* mask to apply */
		} dword_vm;
	} un;
} NVM_INIT;

typedef
struct nvm_funcinfo	{
	unsigned char	boardid[4];	/* Compressed board ID */
	NVM_DUPID	dupid;		/* Duplicate ID information */
	unsigned char	ovl_minor,	/* Minor revision of .OVL code */
			ovl_major,	/* Major revision of .OVL code */
			selects[NVM_MAX_SELECTIONS];	/* Current selections */
	NVM_FIB 	fib;		/* Combined function information byte */
	unsigned char	type[NVM_MAX_TYPE];	/* Function type/subtype */
/* AExx1 - START */
	union	{
		struct	{
			/* Memory configuration */
			NVM_MEMORY	memory[NVM_MAX_MEMORY];
			/* IRQ configuration */
			NVM_IRQ 	irq[NVM_MAX_IRQ];
			/* DMA configuration */
			NVM_DMA 	dma[NVM_MAX_DMA];
			/* PORT configuration */
			NVM_PORT	port[NVM_MAX_PORT];
			/* Initialization information */
			unsigned char	init[NVM_MAX_INIT];
		} r;

		unsigned char	freeform[NVM_MAX_DATA + 1];
	} un;
/* AExx1 - END */

} NVM_FUNCINFO;

#pragma pack()

#define	NVM_READ_SLOT		0xD800	/* Read slot information */
#define	NVM_READ_FUNCTION	0xD801	/* Read function information */
#define	NVM_CLEAR_CMOS		0xD802	/* Clear CMOS memory */
#define	NVM_WRITE_SLOT		0xD803	/* Write slot information */
#define	NVM_READ_SLOTID 	0xD804	/* Read board ID in specified slot */

#define	NVM_XREAD_SLOT		0xD810	/* Read slot info from current source */
#define	NVM_XREAD_FUNCTION	0xD811	/* Read function info from */
					/* current source */

#define	NVM_CURRENT		0	/* Use current SCI or CMOS */
#define	NVM_CMOS		1	/* Use CMOS */
#define	NVM_SCI 		2	/* Use SCI file */
#define	NVM_BOTH		3	/* Both CMOS and SCI (write/clear) */
#define	NVM_NONE		-1	/* Not established yet */

#define	NVM_SUCCESSFUL		0x00	/* No errors */
#define	NVM_INVALID_SLOT	0x80	/* Invalid slot number */
#define	NVM_INVALID_FUNCTION	0x81	/* Invalid function number */
#define	NVM_INVALID_CMOS	0x82	/* Nonvolatile memory corrupt */
#define	NVM_EMPTY_SLOT		0x83	/* Slot is empty */
#define	NVM_WRITE_ERROR 	0x84	/* Failure to write to CMOS */
#define	NVM_MEMORY_FULL 	0x85	/* CMOS memory is full */
#define	NVM_NOT_SUPPORTED	0x86	/* EISA CMOS not supported */
#define	NVM_INVALID_SETUP	0x87	/* Invalid Setup information */
#define	NVM_INVALID_VERSION	0x88	/* BIOS cannot support this version */

#define	NVM_OUT_OF_MEMORY	-1	/* Out of system memory */
#define	NVM_INVALID_BOARD	-2	/* Invalid board data */

#define	NVM_MEMORY_SYS		0
#define	NVM_MEMORY_EXP		1
#define	NVM_MEMORY_VIR		2
#define	NVM_MEMORY_OTH		3

#define	NVM_MEMORY_BYTE 	0
#define	NVM_MEMORY_WORD 	1
#define	NVM_MEMORY_DWORD	2

#define	NVM_DMA_BYTE		0
#define	NVM_DMA_WORD		1
#define	NVM_DMA_DWORD		2

#define	NVM_DMA_DEFAULT 	0
#define	NVM_DMA_TYPEA		1
#define	NVM_DMA_TYPEB		2
#define	NVM_DMA_TYPEC		3

#define	NVM_MEMORY_20BITS	0
#define	NVM_MEMORY_24BITS	1
#define	NVM_MEMORY_32BITS	2

#define	NVM_IOPORT_BYTE 	0
#define	NVM_IOPORT_WORD 	1
#define	NVM_IOPORT_DWORD	2

#define	NVM_IRQ_EDGE		0
#define	NVM_IRQ_LEVEL		1

#ifdef __cplusplus
}
#endif

#endif /* _SYS_NVM_H */
