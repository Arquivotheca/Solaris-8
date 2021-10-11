/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * acpi_res.h -- public definitions for ACPI Resource Data Type
 */

#ifndef	_ACPI_RES_H
#define	_ACPI_RES_H

#pragma ident	"@(#)acpi_res.h	1.1	99/05/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Small Resource Data Type:
 */
#define	IRQ_FORMAT			(0x4 << 3)
#define	DMA_FORMAT			(0x5 << 3)
#define	START_DEPENDENT_TAG		(0x6 << 3)
#define	END_DEPENDENT_TAG		(0x7 << 3)
#define	IO_PORT_DESCRIPTOR		(0x8 << 3)
#define	FIXED_LOCATION_IO_DESCRIPTOR	(0x9 << 3)
#define	VENDOR_DEFINED_SMALL		(0xE << 3)
#define	END_TAG				((0xF << 3) | 1)

/*
 * Large Resource Data Type:
 */
#define	MEMORY24_RANGE_DESCRIPTOR	0x81
#define	VENDOR_DEFINED_LARGE		0x84
#define	MEMORY32_RANGE_DESCRIPTOR	0x85
#define	MEMORY32_FIXED_DESCRIPTOR	0x86
#define	DWORD_ADDR_SPACE_DESCRIPTOR	0x87
#define	WORD_ADDR_SPACE_DESCRIPTOR	0x88
#define	EXTENDED_IRQ_DESCRIPTOR		0x89
#define	QWORD_ADDR_SPACE_DESCRIPTOR	0x8A

/*
 * status definition returned by _STA
 */
#define	STA_PRESENT	0x1
#define	STA_ENABLE	0x2
#define	STA_DISPLAY	0x4
#define	STA_FUNCTION	0x8

/*
 * bus type definitions
 */
#define	RES_BUS_ISA	0x01		/* .. ISA (not self identifying) */
#define	RES_BUS_EISA	0x02		/* .. EISA			*/
#define	RES_BUS_PCI	0x04		/* .. PCI			*/
#define	RES_BUS_PCMCIA	0x08		/* .. PCMCIA			*/
#define	RES_BUS_PNPISA	0x10		/* .. Plug-n-Play ISA		*/
#define	RES_BUS_MCA	0x20		/* .. IBM Microchannel		*/

/*
 * acpi_bcopy - copy ACPI board infos from boot.bin (protected memory) to
 * bootconf.exe (realmode).
 * It takes a acpi_bc structure as noted below.
 */
struct acpi_bc {
	unsigned long bc_buf;		/* 32-bit linear buf addr */
	unsigned long bc_this;		/* this board address to copy */
	unsigned long bc_next;		/* next board addres to copy */
	unsigned short bc_buflen;	/* length of buf in bytes */
	unsigned short bc_nextlen;	/* length of next board */
	unsigned long bc_flag;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _ACPI_RES_H */
