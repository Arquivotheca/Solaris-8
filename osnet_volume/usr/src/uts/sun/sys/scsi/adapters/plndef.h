/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SCSI_ADAPTERS_PLNDEF_H
#define	_SYS_SCSI_ADAPTERS_PLNDEF_H

#pragma ident	"@(#)plndef.h	1.7	98/01/06 SMI"

/*
 * Pluto (Sparc Storage Array) definitions and data structures
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Pluto FC4S Addressing
 *
 * The addresses we use are overlaid on top of the
 * generic FC4S addressing structure.
 */
typedef struct pln_address {
	union {
		fcp_ent_addr_t	fcp_addr;	/* fcp generic address */
		struct {
			ushort_t pa_entity;	/* entity within pluto */
			ushort_t pa_port;	/* port address */
			ushort_t pa_target;	/* target address */
			ushort_t pa_reserved;	/* reserved */
		} pln_addr;
	} un;
} pln_address_t;

/*
 * Convenience macros for addressing
 */
#define	pln_entity	un.pln_addr.pa_entity
#define	pln_port	un.pln_addr.pa_port
#define	pln_target	un.pln_addr.pa_target
#define	pln_reserved	un.pln_addr.pa_reserved

#define	fcp_addr0	un.fcp_addr.ent_addr_0
#define	fcp_addr1	un.fcp_addr.ent_addr_1
#define	fcp_addr2	un.fcp_addr.ent_addr_2
#define	fcp_addr3	un.fcp_addr.ent_addr_3

/*
 * Pluto addressing pa_entity types
 */
#define	PLN_ENTITY_CONTROLLER		0x0000
#define	PLN_ENTITY_DISK_SINGLE		0x0001
#define	PLN_ENTITY_DISK_GROUPED		0x0002





/*
 * SSA INQUIRY structure
 */
typedef struct p_inquiry {
	/*
	* byte 0
	*
	* Bits 7-5 are the Peripheral Device Qualifier
	* Bits 4-0 are the Peripheral Device Type
	*
	*/
	uchar_t	inq_dtype;
	/* byte 1 */
	uchar_t	inq_rmb		: 1,	/* removable media */
		inq_qual	: 7;	/* device type qualifier */

	/* byte 2 */
	uchar_t	inq_iso		: 2,	/* ISO version */
		inq_ecma	: 3,	/* ECMA version */
		inq_ansi	: 3;	/* ANSI version */

	/* byte 3 */
	uchar_t	inq_aenc	: 1,	/* async event notification cap. */
		inq_trmiop	: 1,	/* supports TERMINATE I/O PROC msg */
				: 2,	/* reserved */
		inq_rdf		: 4;	/* response data format */

	/* bytes 4-7 */

	uchar_t	inq_len;		/* additional length */
	uchar_t			: 8;	/* reserved */
	uchar_t			: 8;	/* reserved */
	uchar_t	inq_reladdr	: 1,	/* supports relative addressing */
		inq_wbus32	: 1,	/* supports 32 bit wide data xfers */
		inq_wbus16	: 1,	/* supports 16 bit wide data xfers */
		inq_sync	: 1,	/* supports synchronous data xfers */
		inq_linked	: 1,	/* supports linked commands */
				: 1,	/* reserved */
		inq_cmdque	: 1,	/* supports command queueing */
		inq_sftre	: 1;	/* supports Soft Reset option */

	/* bytes 8-15 */
	char	inq_vid[8];		/* vendor ID */

	/* bytes 16-31 */
	char	inq_pid[16];		/* product ID */

	/* bytes 32-35 */
	char	inq_revision[4];	/* Product Revision level */

	/* bytes 36-39 */
	char	inq_firmware_rev[4];	/* firmware revision level */

	/* bytes 40-51 */
	char	inq_serial[12];		/* serial number */

	/* bytes 52-53 */
	char	reserved[2];

	/* byte 54 */
	char	inq_ports;		/* number of ports */

	/* byte 55 */
	char	inq_tgts;		/* number of targets */

	/*
	 * Bytes 36-55 are vendor-specific.
	 * Bytes 56-95 are reserved.
	 * 96 to 'n' are vendor-specific parameter bytes
	 */
} p_inquiry_t;
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_PLNDEF_H */
