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

#ifndef	_SYS_EISAROM_H
#define	_SYS_EISAROM_H

#pragma ident	"@(#)eisarom.h	1.2	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Copyright 1990 Compaq Computer Corporation.
 */

/*
 * This defines the mask used to determine what arguments are passed in
 * to nvm_eisa().
 */

typedef struct key_mask {
	/*
	 * This mask is scanned from bit 0 to bit 7. If a bit is set, it
	 * means that an argument of the type corresponding to the bit
	 * position in the key mask is next in the argument list.
	 */

	unsigned int	slot	 : 1,	/* EISA Slot Number. */
			function : 1,	/* Function Record Number within */
					/* an EISA slot. */
			board_id : 1,	/* EISA readable Board ID. */
			revision : 1,	/* EISA Board Revision Number. */
			checksum : 1,	/* EISA Board Firmware Checksum. */
			type	 : 1,	/* EISA Board Type String. */
			sub_type : 1,	/* EISA Board Sub-type String. */
			: 25;
} KEY_MASK;

typedef struct {
	char *data;
	int length;
} eisanvm;

#pragma pack(1)

typedef struct {
	union {
		unsigned int eax;
		struct {
			unsigned short ax;
		} word;
		struct {
			unsigned char al;
			unsigned char ah;
		} byte;
	} eax;
	union {
		unsigned int ebx;
		struct {
			unsigned short bx;
		} word;
		struct {
			unsigned char bl;
			unsigned char bh;
		} byte;
	} ebx;
	union {
		unsigned int ecx;
		struct {
			unsigned short cx;
		} word;
		struct {
			unsigned char cl;
			unsigned char ch;
		} byte;
	} ecx;
	union {
		unsigned int edx;
		struct {
			unsigned short dx;
		} word;
		struct {
			unsigned char dl;
			unsigned char dh;
		} byte;
	} edx;
	union {
		unsigned int edi;
		struct {
			unsigned short di;
		} word;
	} edi;
	union {
		unsigned int esi;
		struct {
			unsigned short si;
		} word;
	} esi;
} regs;

#pragma pack()

#define	EISA			('E'<<8)
#define	EISA_CMOS_QUERY		(EISA|1)
#define	EISA_SYSTEM_MEMORY	(EISA|2)
#define	EISA_ELCR_GET		(EISA|3)
#define	EISA_ELCR_SET		(EISA|4)
#define	EISA_DMAEMR_SET		(EISA|5)
#define	EISA_GET_BUF_LEN	(EISA|6)
#define	EISA_INTEGER 		0x1f
#define	EISA_SLOT		0x01
#define	EISA_CFUNCTION		0x02
#define	EISA_BOARD_ID		0x04
#define	EISA_REVISION		0x08
#define	EISA_CHECKSUM		0x10
#define	EISA_TYPE		0x20
#define	EISA_SUB_TYPE		0x40

/* Position definitions for Edge/Level Control Register. */

#define	INT_0			0x0001
#define	INT_1			0x0002
#define	INT_2			0x0004
#define	INT_3			0x0008
#define	INT_4			0x0010
#define	INT_5			0x0020
#define	INT_6			0x0040
#define	INT_7			0x0080
#define	INT_8			0x0100
#define	INT_9			0x0200
#define	INT_10			0x0400
#define	INT_11			0x0800
#define	INT_12			0x1000
#define	INT_13			0x2000
#define	INT_14			0x4000
#define	INT_15			0x8000

/* DMA stuff. */

#define	DMA_0_3			0x40b
#define	DMA_4_7			0x4d6

/* DMA Channel Selectors */

#define	DMA_CHANNEL		0x03
#define	DMA_0			0x00
#define	DMA_1			0x01
#define	DMA_2			0x02
#define	DMA_3			0x03
#define	DMA_4			DMA_0
#define	DMA_5			DMA_1
#define	DMA_6			DMA_2
#define	DMA_7			DMA_3

/* DMA Addressing Modes */

#define	DMA_ADDRESSING		0x0c
#define	DMA_8_BYTE		0x00	/* 8-bit i/o, count by bytes. */
#define	DMA_16_WORD		0x04	/* 16-bit i/o, count by words. */
#define	DMA_32_BYTE		0x08	/* 32-bit i/o, count by bytes. */
#define	DMA_16_BYTE		0x0c	/* 16-bit i/o, count by bytes. */

/* DMA Cycle Timing Modes */

#define	DMA_TIMING		0x30
#define	DMA_ISA			0x00
#define	DMA_TYPE_A		0x10
#define	DMA_TYPE_B		0x20
#define	DMA_BURST		0x30

/* DMA Terminal Count modes. */

#define	DMA_T_C			0x40
#define	DMA_T_C_OUT		0x00
#define	DMA_T_C_IN		0x40

/* DMA Stop Register modes. */

#define	DMA_S_R			0x80
#define	DMA_S_R_ON		0x00
#define	DMA_S_R_OFF		0x80

/* DMA Defaults. */

#define	DMA_DEF_0_3	(DMA_S_R_OFF | DMA_T_C_OUT | DMA_ISA | DMA_8_BYTE)
#define	DMA_DEF_4_7	(DMA_S_R_OFF | DMA_T_C_OUT | DMA_ISA | DMA_16_WORD)

#define	SYSTEM_MEMORY		0
#define	MEMORY_ADDRESS_UNITS	256
#define	MEMORY_SIZE_UNITS	1024
#define	EISA_READ_SLOT_CONFIG	0xd880
#define	EISA_READ_FUNC_CONFIG	0xd881
#define	EISA_CALL_ADDRESS	0xff859
#define	EISA_STRING_ADDRESS	0xfffd9
#define	EISA_STRING		'EISA'
#define	EISA_MAXSLOT		0x40

typedef struct {
	long address;
	long length;
} MEMORY_BLOCK;

typedef struct {
	unsigned char ints_7_0;
	unsigned char ints_15_8;
} ELCR_IO;

typedef struct {
	unsigned char channel;
	unsigned char mode;
} DMAEMR_IO;

struct es_slot {
	regs	es_slotinfo;
	int	es_funcoffset;
};

#define	EFBUFSZ	320
#pragma pack(1)
struct es_func {
	union {
		unsigned int eax;
		struct {
			unsigned short ax;
		} word;
		struct {
			unsigned char al;
			unsigned char ah;
		} byte;
	} eax;
	char	ef_buf[EFBUFSZ];
};

#pragma pack()

#define	ES_SLOT_SZ	(sizeof (struct es_slot))
#define	ES_FUNC_SZ	(sizeof (struct es_func))


#define	EISAPORTS	1
#define	EISA_OPEN	0x1
#define	EISA_INIT	0x2

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EISAROM_H */
