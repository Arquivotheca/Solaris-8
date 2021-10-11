/*
 * Copyright (c) 1989,1991,1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MQH_H
#define	_SYS_MQH_H

#pragma ident	"@(#)mqh.h	3.15	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/physaddr.h>

#ifndef _ASM

#define	MEM_UNIT_MAX 10
#define	MQH_GROUP_PER_BOARD 4
#define	NGROUP (MQH_GROUP_PER_BOARD * MEM_UNIT_MAX)
#define	MQH_SIMM_PER_GROUP 4

#define	ADR_BASE(adr) (adr >> 19)
#define	ADR_IV(adr) ((adr & ((3)<<6)) >> 6)
#define	ADR_SSIZE(adr) ((adr & ((7)<<2)) >> 2)
#define	ADR_IF(adr) (adr & (3))

#define	BASE_2_ADR(base) (base << 19)
#define	IV_2_ADR(iv) (iv << 6)
#define	SSIZE_2_ADR(ssize) (ssize << 2)
#define	IF_2_ADR(if) (if)

/*
 * transform interleave factor into how many groups it represents 2/1/0 =>
 * 4/2/1
 */
#define	if2groups(IFF) ((IFF) ? (IFF << 1) : 1)

/* Group Type register SIZE field */
#define	GT_SIZE8MB	-1
#define	GT_SIZE32MB	0
#define	GT_SIZE128MB	1
#define	GT_SIZE512	2
#define	GT_SIZE2GB	3

/* used in ADR, as well as SW array index */
#define	SSIZE_RESERVE	0
#define	SSIZE_8MB	1
#define	SSIZE_32MB	2
#define	SSIZE_128MB	3
#define	SSIZE_512MB	4
#define	SSIZE_2GB	5
#define	SSIZE_ARRAY	6

/* SIMM types */
#define	DRAM		0
#define	NVRAM		1

#ifdef not
/* flag to put in size field of NVRAM chunk */
#define	NVRAM_CHUNK	0x80000000
#endif not

#define	GT_SIZE_2_SSIZE(gt_size) (gt_size + 2)

/* Interleave Factors */
#define	IF_4WAY	2
#define	IF_2WAY	1
#define	IF_0WAY	0

/*
 * MQH base addresses are in units of 8MB OBP likes to talk in 4K pages
 * convert from MQH base address to OBP base address
 */
#define	base_8M_to_4K(base) ((base)<<(11))

#endif	/* _ASM */

#define	MQH_DEVID_SHIFT 20	/* bbbbuuuu is bits 27:20 */
#define	MQH_BOARD_SHIFT 24	/* bbbb is bits 27:24 */
#define	MQH_CSR_BASE	0xe0100000
#define	MQH_CSR_BUS_SHIFT	8	/* CSR bus seclect, ss, is bits 9:8 */
#define	MQH_BASE_TO_BOARD(x) ((x & (0xF<<MQH_BOARD_SHIFT)) >> MQH_BOARD_SHIFT)
#define	mqh_board(devid) (devid >> 4)
/*
 * MQH register address map
 */
#define	MQH_OFF_CID		(0x00000)
#define	MQH_OFF_DCSR		(0x00008)
#define	MQH_OFF_DDATA		(0x00010)

#define	MQH_OFF_ADR_G0		(0x01000)
#define	MQH_OFF_ADR_G1		(0x01008)
#define	MQH_OFF_TYPE_G0		(0x01010)
#define	MQH_OFF_TYPE_G1		(0x01018)

#define	MQH_OFF_MCSR		(0x01020)

/*
 * MQHP only
 */
#define	MQH_OFF_ADR_G2		(0x01040)
#define	MQH_OFF_ADR_G3		(0x01048)
#define	MQH_OFF_TYPE_G2		(0x01050)
#define	MQH_OFF_TYPE_G3		(0x01058)

#define	MQH_OFF_CE_ADDR		(0x02000)
#define	MQH_OFF_CE_DATA		(0x02008)
#define	MQH_OFF_UE_ADDR		(0x02010)
#define	MQH_OFF_UE_DATA		(0x02018)
#define	MQH_OFF_ECC_DIAG	(0x02020)

/*
 * Component ID values
 */
#define	CID_VERS_MSK	0xF0000000	/* Mask for version bits of ASICs */
#define	MQH_CID_VAL	0x10ADC07D	/* MQH component ID value */
#define	MQHP_CID_VAL	0x10D8607D	/* MQH Prime component ID value */

#define	MQH_MCSR_ECI_BIT	(1 << 1)

#define	MQH_ERR_ME_SHIFT	63
#define	MQH_ERR_ERR_SHIFT	62
#define	MQH_ERR_ECC_SHIFT	54
#define	MQH_ERR_SYN_SHIFT	46

#define	MQH_ERR_ME_BIT		((u_longlong_t)0x1 << MQH_ERR_ME_SHIFT)
#define	MQH_ERR_ERR_BIT		((u_longlong_t)0x1 << MQH_ERR_ERR_SHIFT)
#define	MQH_ERR_ECC_MASK	((u_longlong_t)0xFF << MQH_ERR_ECC_SHIFT)
#define	MQH_ERR_SYN_MASK	((u_longlong_t)0xFF << MQH_ERR_SYN_SHIFT)
#define	MQH_ERR_TO_SYN(a)	(uint_t)(((a) & MQH_ERR_SYN_MASK) >> \
					MQH_ERR_SYN_SHIFT)
#define	MQH_ERR_ADDR_MASK	(0x0000000FFFFFFFFF)
#define	MQH_ERR_TO_PAGE(a)	(uint_t)((((a) & MQH_ERR_ADDR_MASK)) \
						>> MMU_PAGESHIFT)
#define	mqh_get_ce_data(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_CE_DATA))
#define	mqh_get_ce_addr(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_CE_ADDR))
#define	mqh_set_ce_addr(v, a) stda_2f(v, (u_longlong_t *)((a)|MQH_OFF_CE_ADDR))
#define	mqh_get_ue_data(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_UE_DATA))
#define	mqh_get_ue_addr(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_UE_ADDR))
#define	mqh_set_ue_addr(v, a) stda_2f(v, (u_longlong_t *)((a)|MQH_OFF_UE_ADDR))

#define	mqh_get_cid(a) (lda_2f((uint_t *)((a)|MQH_OFF_CID)) & ~CID_VERS_MSK)

#define	mqh_mcsr_get(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_MCSR))
#define	mqh_mcsr_set(v, a) stda_2f((u_longlong_t)(v), \
				(u_longlong_t *)((a)|MQH_OFF_MCSR))

#define	mqh_dcsr_get(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_DCSR))
#define	mqh_dcsr_set(v, a) stda_2f((u_longlong_t)(v), \
				(u_longlong_t *)((a)|MQH_OFF_DCSR))

#define	mqh_get_adr0(a) lda_2f((uint_t *)((a)|MQH_OFF_ADR_G0))
#define	mqh_get_adr1(a) lda_2f((uint_t *)((a)|MQH_OFF_ADR_G1))
#define	mqh_get_adr2(a) lda_2f((uint_t *)((a)|MQH_OFF_ADR_G2))
#define	mqh_get_adr3(a) lda_2f((uint_t *)((a)|MQH_OFF_ADR_G3))

#define	mqh_get_gtr0(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_TYPE_G0))
#define	mqh_get_gtr1(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_TYPE_G1))
#define	mqh_get_gtr2(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_TYPE_G2))
#define	mqh_get_gtr3(a) ldda_2f((u_longlong_t *)((a)|MQH_OFF_TYPE_G3))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MQH_H */
