/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ASYNC_H
#define	_SYS_ASYNC_H

#pragma ident	"@(#)async.h	1.20	98/01/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM
struct async_flt {
	ushort_t aflt_type;	/* type of async. fault */
	ushort_t aflt_subtype;	/* id of particular module error */
	uint_t	aflt_stat;	/* async. fault stat. reg. */
	uint_t	aflt_addr0;	/* 1st async. fault addr. reg. */
	uint_t	aflt_addr1;	/* 2nd async. fault addr. reg. */
	uint_t	aflt_err0;	/* 1st error reg. <63:32> */
	uint_t	aflt_err1;	/* 2nd error reg. <31:0> */
};

/*
 * Virtual addresses where various asynchronous fault
 * registers are mapped.
 */

extern caddr_t v_memerr_addr;
extern caddr_t v_sbusctl_addr;

#define	EFSR_VADDR	(v_memerr_addr+0x8)
#define	EFAR0_VADDR	(v_memerr_addr+0x10)
#define	EFAR1_VADDR	(v_memerr_addr+0x14)
#define	MTS_AFSR_VADDR  (v_sbusctl_addr+0x0)
#define	MTS_AFAR_VADDR  (v_sbusctl_addr+0x4)

#endif	/* !_ASM */

#define	VFSR_OFFSET	0x8

/*
 * The maximum number of async_flt structures for each CPU.
 * XXX - This number is still probably overkill.
 */
#define	MAX_AFLTS	64

/*
 * Interrupt level at which non-fatal faults are handled
 */
/* 12 is low enough to manipulate hat data structures */
#define	AFLT_HANDLER_LEVEL	12

/*
 * Types of asynchronous faults (for aflt_type field)
 */
#define	AFLT_MODULE		0x1	/* Module Error */
#define	AFLT_MEM_IF		0x2	/* ECC Memory Error */
#define	AFLT_M_TO_S		0x4	/* M-to-S Write Buffer Error */
#define	AFLT_SX_CORE		0x10	/* Graphics Error (SS-20) */

/*
 * Sub-Types of AFLT_MODULE asynchronous faults
 * (for aflt_subtype)
 */
/* indicates module with lowest MID had the fault */
#define	AFLT_LOMID		0x0
/* indicates module with highest MID had the fault */
#define	AFLT_HIMID		0x1


/*
 * Bits of EFSR (ECC Memory Fault Status Register)
 */
#define	EFSR_GE		0x00020000	/* C2 SMC, graphics error */
#define	EFSR_ME		0x00010000	/* multiple errors */
#define	EFSR_SYND	0x0000FF00	/* syndrome */
#define	EFSR_DW		0x000000F0	/* double word for CE or UE */
#define	EFSR_UE		0x00000008	/* uncorrectable error */
#define	EFSR_TO		0x00000004	/* timeout */
#define	EFSR_SE		0x00000002	/* misreferenced slot error */
#define	EFSR_CE		0x00000001	/* correctable error */

/*
 * Various shifts for EFSR
 */
#define	EFSR_SYND_SHFT	(8)
/*
 * Bits of EFAR0 (ECC Memory Fault Address Register 0)
 */
#define	EFAR0_MID	0xF0000000	/* MID for owner of fault */
#define	EFAR0_S		0x08000000	/* supervisor access */
#define	EFAR0_VA	0x003FC000	/* VA<19:12> superset bits for VAC */
#define	EFAR0_MBL	0x00002000	/* boot mode */
#define	EFAR0_LOCK	0x00001000	/* occurred in atomic cycle */
#define	EFAR0_C		0x00000800	/* address was mapped cacheable */
#define	EFAR0_SIZ	0x00000700	/* SIZE<2:0>: transaction size */
#define	EFAR0_TYPE	0x000000F0	/* TYPE<3:0>: transaction type */
#define	EFAR0_PA	0x0000000F	/* PA<35:32>: physical address */

/*
 * Various shifts for EFAR0
 */
#define	EFAR0_MID_SHFT	(28)

/*
 * Bits of M-to-S AFSR (Asynchronous Fault Status Register)
 */
#define	MTSAFSR_ERR	0x80000000	/* summary bit, LE, TO, or BERR */
#define	MTSAFSR_LE	0x40000000	/* late error */
#define	MTSAFSR_TO	0x20000000	/* timed out */
#define	MTSAFSR_BERR	0x10000000	/* sbus received BERR */
#define	MTSAFSR_SIZ	0x0E000000	/* size of error transaction */
#define	MTSAFSR_S	0x01000000	/* supervisor access */
#define	MTSAFSR_MID	0x00F00000	/* MID for owner of fault */
#define	MTSAFSR_ME	0x00080000	/* multiple errors */
#define	MTSAFSR_SA	0x0001F000	/* sbus addr bits <4:0> */
#define	MTSAFSR_SSIZ	0x00000E00	/* size of error trans. */
#define	MTSAFSR_PA	0x0000000F	/* PA<35:32> of fault */

/*
 * Various shifts for M-to-S AFSR
 */
#define	MTSAFSR_SIZ_SHFT	(25)
#define	MTSAFSR_MID_SHFT	(20)
#define	MTSAFSR_SA_SHFT		(12)
#define	MTSAFSR_SSIZ_SHFT	(8)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASYNC_H */
