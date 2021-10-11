/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SPITASI_H
#define	_SYS_SPITASI_H

#pragma ident	"@(#)spitasi.h	1.5	99/03/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __sparcv9
/*
 * Spitfire ancillary state registers, for asrset_t
 */
#define	ASR_GSR	(3)
#endif

/*
 * alternate address space identifiers
 *
 * 0x00 - 0x7F are privileged
 * 0x80 - 0xFF can be used by users
 */

/*
 * Spitfire asis
 */
#define	ASI_MEM			0x14	/* memory (e$, no d$) */
#define	ASI_IO			0x15	/* I/O (uncached, side effect) */
#define	ASI_MEML		0x1C	/* memory little */
#define	ASI_IOL			0x1D	/* I/O little */

#define	ASI_NQUAD_LD		0x24	/* 128-bit atomic load */
#define	ASI_NQUAD_LD_L		0x2c	/* 128-bit atomic load little */

#define	ASI_LSU			0x45	/* load-store unit control */
#define	ASI_DC_INVAL		0x42	/* d$ invalidate */


#define	ASI_DC_DATA		0x46	/* d$ data */
#define	ASI_DC_TAG		0x47	/* d$ tag */

#define	ASI_INTR_DISPATCH_STATUS 0x48	/* interrupt vector dispatch status */
#define	ASI_INTR_RECEIVE_STATUS	0x49	/* interrupt vector receive status */

#define	ASI_UPA_CONFIG		0x4A	/* upa configuration reg */

#define	ASI_ESTATE_ERR		0x4B	/* estate error enable reg */

#define	ASI_AFSR		0x4C	/* asynchronous fault status */
#define	ASI_AFAR		0x4D	/* asynchronous fault address */

#define	ASI_IMMU		0x50	/* instruction mmu */
#define	ASI_IMMU_TSB_8K		0x51	/* immu tsb 8k ptr */
#define	ASI_IMMU_TSB_64K	0x52	/* immu tsb 64k ptr */
#define	ASI_ITLB_IN		0x54	/* immu tlb data in */
#define	ASI_ITLB_ACCESS		0x55	/* immu tlb data access */
#define	ASI_ITLB_TAGREAD	0x56	/* immu tlb tag read */
#define	ASI_ITLB_DEMAP		0x57	/* immu tlb demap */

#define	ASI_DMMU		0x58	/* data mmu */
#define	ASI_DMMU_TSB_8K		0x59	/* dmmu tsb 8k ptr */
#define	ASI_DMMU_TSB_64K	0x5A	/* dmmu tsb 64k ptr */
#define	ASI_DMMU_TSB_DIRECT	0x5B	/* dmmu tsb direct ptr */
#define	ASI_DTLB_IN		0x5C	/* dmmu tlb data in */
#define	ASI_DTLB_ACCESS		0x5D	/* dmmu tlb data access */
#define	ASI_DTLB_TAGREAD	0x5E	/* dmmu tlb tag read */
#define	ASI_DTLB_DEMAP		0x5F	/* dmmu tlb demap */

#define	ASI_IC_DATA		0x66	/* i$ data */
#define	ASI_IC_TAG		0x67	/* i$ tag */
#define	ASI_IC_DECODE		0x6E	/* i$ pre-decode */
#define	ASI_IC_NEXT		0x6F	/* i$ next field */

#define	ASI_BLK_AIUP		0x70	/* block as if user primary */
#define	ASI_BLK_AIUS		0x71	/* block as if user secondary */

#define	ASI_EC_ACCESS		0x76	/* e$ access reg */
#define	ASI_EC_DIAG		0x4E	/* e$ diagnostic reg */

#define	ASI_SDB_INTR_W		0x77	/* interrupt vector dispatch */
#define	ASI_SDB_INTR_R		0x7F	/* incoming interrupt vector */
#define	ASI_INTR_DISPATCH	ASI_SDB_INTR_W
#define	ASI_INTR_RECEIVE	ASI_SDB_INTR_R

#define	ASI_BLK_AIUPL		0x78	/* block as if user primary little */
#define	ASI_BLK_AIUSL		0x79	/* block as if user secondary little */

#define	ASI_PST8_P		0xC0	/* primary 8bit partial store */
#define	ASI_PST8_S		0xC1	/* secondary 8bit partial store */
#define	ASI_PST16_P		0xC2	/* primary 16bit partial store */
#define	ASI_PST16_S		0xC3	/* secondary 16bit partial store */
#define	ASI_PST32_P		0xC4	/* primary 32bit partial store */
#define	ASI_PST32_S		0xC5	/* secondary 32bit partial store */
#define	ASI_PST8_PL		0xC8	/* primary 8bit partial little */
#define	ASI_PST8_SL		0xC9	/* secondary 8bit partial little */
#define	ASI_PST16_PL		0xCA	/* primary 16bit partial little */
#define	ASI_PST16_SL		0xCB	/* secondary 16bit partial little */
#define	ASI_PST32_PL		0xCC	/* primary 32bit partial little */
#define	ASI_PST32_SL		0xCD	/* secondary 32bit partial little */

#define	ASI_FL8_P		0xD0	/* primary 8bit floating store */
#define	ASI_FL8_S		0xD1	/* secondary 8bit floating store */
#define	ASI_FL16_P		0xD2	/* primary 16bit floating store */
#define	ASI_FL16_S		0xD3	/* secondary 16bit floating store */
#define	ASI_FL8_PL		0xD8	/* primary 8bit floating little */
#define	ASI_FL8_SL		0xD9	/* secondary 8bit floating little */
#define	ASI_FL16_PL		0xDA	/* primary 16bit floating little */
#define	ASI_FL16_SL		0xDB	/* secondary 16bit floating little */

#define	ASI_BLK_COMMIT_P	0xE0	/* block commit primary */
#define	ASI_BLK_COMMIT_S	0xE1	/* block commit secondary */
#define	ASI_BLK_P		0xF0	/* block primary */
#define	ASI_BLK_S		0xF1	/* block secondary */
#define	ASI_BLK_PL		0xF8	/* block primary little */
#define	ASI_BLK_SL		0xF9	/* block secondary little */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPITASI_H */
