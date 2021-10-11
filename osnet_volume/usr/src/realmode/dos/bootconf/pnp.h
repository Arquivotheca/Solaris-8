/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pnp.h -- public definitions for PnP ISA routines
 */

#ifndef	_PNP_H
#define	_PNP_H

#ident	"@(#)pnp.h	1.10	97/08/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_pnp(void);
void enumerator_pnp(void);
void program_pnp(struct board *bp);
void unprogram_pnp(struct board *bp);

extern u_char PnpCardCnt;

/*  Resource type tags:							    */
/*    Short format ...							    */
#define	VERSION_NO			(0x1 << 3)
#define	LOGICAL_DEVICE_ID		(0x2 << 3)
#define	COMPATIBLE_DEVICE_ID		(0x3 << 3)
#define	IRQ_FORMAT			(0x4 << 3)
#define	DMA_FORMAT			(0x5 << 3)
#define	START_DEPENDENT_TAG		(0x6 << 3)
#define	END_DEPENDENT_TAG		(0x7 << 3)
#define	IO_PORT_DESCRIPTOR		(0x8 << 3)
#define	FIXED_LOCATION_IO_DESCRIPTOR	(0x9 << 3)
#define	RESERVED_TYPE0			(0xA << 3)
#define	RESERVED_TYPE1			(0xB << 3)
#define	RESERVED_TYPE2			(0xC << 3)
#define	RESERVED_TYPE3			(0xD << 3)
#define	VENDOR_DEFINED			(0xE << 3)

/*    Long format ...							    */
#define	MEMORY_RANGE_DESCRIPTOR		0x81
#define	CARD_IDENTIFIER_ANSI		0x82
#define	CARD_IDENTIFIER_UNICODE		0x83
#define	VENDOR_SPECIFIC_DATA		0x84
#define	MEMORY32_RANGE_DESCRIPTOR	0x85
#define	MEMORY32_FIXED_DESCRIPTOR	0x86

/*    End of resources tag ...						    */
#define	END_TAG 0x79


#ifdef	__cplusplus
}
#endif

#endif	/* _PNP_H */
