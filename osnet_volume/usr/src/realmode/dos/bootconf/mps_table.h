/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * mps_table.h -- MP Specification table definitions
 */

#ifndef	_MPS_TABLE_H
#define	_MPS_TABLE_H

#ident "@(#)mps_table.h   1.1   99/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


struct mps_fps_hdr {		/* MP Floating Pointer Structure	*/
	u_long	fps_sig;	/* _MP_ (0x5F4D505F)			*/
	u_long	fps_mpct_paddr;	/* paddr of MP Configuration Table	*/
	u_char	fps_len;	/* in paragraph (16-bytes units)	*/
	u_char	fps_spec_rev;	/* MP Spec. version no.			*/
	u_char	fps_cksum;	/* checksum of complete structure	*/
	u_char	fps_featinfo1;	/* mp feature info byte 1 		*/
	u_char	fps_featinfo2;	/* mp feature info byte 2		*/
	u_char	fps_featinfo3;	/* mp feature info byte 3		*/
	u_char	fps_featinfo4;	/* mp feature info byte 4		*/
	u_char	fps_featinfo5;	/* mp feature info byte 5		*/
};

struct mps_ct_hdr {		/* MP Configuration Table Header	*/
	u_long	ct_sig;		/* PCMP (				*/
	u_short	ct_len;		/* base configuration in bytes 		*/
	u_char	ct_spec_rev;	/* MP Spec. version no.			*/
	u_char	ct_cksum;	/* base configuration table checksum	*/
	char	ct_oem_id[8];	/* string identifies the manufacturer	*/
	char	ct_prod_id[12]; /* string identifies the product	*/
	u_long	ct_oem_ptr;	/* paddr to an OEM-defined table	*/
	u_short	ct_oem_tbl_len;	/* size of base OEM table in bytes	*/
	u_short	ct_entry_cnt;	/* no. of entries in the base table	*/
	u_long	ct_local_apic;	/* paddr of local APIC			*/
	u_short	ct_ext_tbl_len;	/* extended table in bytes 		*/
	u_char	ct_ext_cksum;	/* checksum for the extended table	*/
};

/* Base MP Configuration Table entry type definitions */
#define	CPU_TYPE	0
#define	BUS_TYPE	1
#define	IO_APIC_TYPE	2
#define	IO_INTR_TYPE	3
#define	LOCAL_INTR_TYPE	4

/* Base MP Configuration Table entry size definitions */
#define	CPU_SIZE	20
#define	BUS_SIZE	8
#define	IO_APIC_SIZE	8
#define	IO_INTR_SIZE	8
#define	LOCAL_INTR_SIZE	8

/* Extended MP Configuration Table entry type definitions */
#define	SYS_AS_MAPPING		128
#define	BUS_HIERARCHY_DESC	129
#define	COMP_BUS_AS_MODIFIER	130

/* Extended MP Configuration Table entry size definitions */
#define	SYS_AS_MAPPING_SIZE		20
#define	BUS_HIERARCHY_DESC_SIZE		8
#define	COMP_BUS_AS_MODIFIER_SIZE	8

struct sasm {			/* System Address Space Mapping Entry	*/
	u_char sasm_type;	/* type 128				*/
	u_char sasm_len;	/* entry length in bytes (20)		*/
	u_char sasm_bus_id;	/* bus id where this is mapped		*/
	u_char sasm_as_type;	/* system address type			*/
/* system address type definitions */
#define	IO_TYPE		0
#define	MEM_TYPE	1
#define	PREFETCH_TYPE	2
#define	BUSRANGE_TYPE	3
	u_long sasm_as_base;	/* starting address			*/
	u_long sasm_as_base_hi;
	u_long sasm_as_len;	/* no. of addresses visiblie to the bus	*/
	u_long sasm_as_len_hi;
};

struct bhd {			/* Bus Hierarchy Descriptor Entry	*/
	u_char bhd_type;	/* type 129				*/
	u_char bhd_len;		/* entry length in bytes (8)		*/
	u_char bhd_bus_id;	/* bus id of this bus			*/
	u_char bhd_bus_info;	/* bus information			*/
/* Bus Information bit definition */
#define	BHD_BUS_INFO_SD	1	/* Subtractive Decode Bus		*/
	u_char bhd_parent;
};

struct cbasm {	/* Compatibility Bus Address Space Modifier Entry */
	u_char cbasm_type;	/* type 130				*/
	u_char cbasm_len;	/* entry length in bytes (8)		*/
	u_char cbasm_bus_id;	/* bus to be modified			*/
	u_char cbasm_addr_mod;	/* address modifier			*/
/* Address Modifier bit definiton */
#define	CBASM_ADDR_MOD_PR	1	/* 1 = subtracted, 0 = added */
	u_long cbasm_pr_list;	/* identify list of predefined address ranges */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _MPS_TABLE_H */
