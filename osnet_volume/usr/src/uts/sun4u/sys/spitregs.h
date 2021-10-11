/*
 * Copyright (c) 1995, 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SPITREGS_H
#define	_SYS_SPITREGS_H

#pragma ident	"@(#)spitregs.h	1.22	99/11/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file is cpu dependent.
 */

/*
 * LSU Control Register
 *
 * +------+----+----+----+----+----+----+-----+------+----+----+----+---+
 * | Resv | PM | VM | PR | PW | VR | VW | Rsv |  FM  | DM | IM | DC | IC|
 * +------+----+----+----+----+----+----+-----+------+----+----+----+---+
 *  63  41   33   25   24   23	 22   21   20  19   4	3    2	  1   0
 *
 */

#define	LSU_IC		0x00000001	/* icache enable */
#define	LSU_DC		0x00000002	/* dcache enable */
#define	LSU_IM		0x00000004	/* immu enable */
#define	LSU_DM		0x00000008	/* dmmu enable */
#define	LSU_FM		0x000FFFF0	/* parity mask */
#define	LSU_VW		0x00200000	/* virtual watchpoint write enable */
#define	LSU_VR		0x00400000	/* virtual watchpoint read enable */
#define	LSU_PW		0x00800000	/* phys watchpoint write enable */
#define	LSU_PR		0x01000000	/* phys watchpoint read enable */

/*
 * Defines for the different types of dcache_flush
 * it is stored in dflush_type
 */
#define	FLUSHALL_TYPE	0x0		/* blasts all cache lines */
#define	FLUSHMATCH_TYPE	0x1		/* flush entire cache but check each */
					/* each line for a match */
#define	FLUSHPAGE_TYPE	0x2		/* flush only one page and check */
					/* each line for a match */

/*
 * D-Cache Tag Data Register
 *
 * +----------+--------+----------+
 * | Reserved | DC_Tag | DC_Valid |
 * +----------+--------+----------+
 *  63	    30 29    2	1	 0
 *
 */
#define	ICACHE_FLUSHSZ	0x20	/* one line in i$ */
#define	DC_PTAG_SHIFT	34
#define	DC_LINE_SHIFT	30
#define	DC_VBIT_SHIFT	2
#define	DC_VBIT_MASK	0x3
#define	IC_LINE_SHIFT	3
#define	IC_LINE		512
#define	INDEX_BIT_SHIFT	13

/*
 * Definitions of sun4u cpu implementations as specified in version register
 */
#define	SPITFIRE_IMPL	0x10
#define	IS_SPITFIRE(impl)	((impl) == SPITFIRE_IMPL)
#define	SPITFIRE_MAJOR_VERSION(rev)	(((rev) >> 4) & 0xf)
#define	SPITFIRE_MINOR_VERSION(rev)	((rev) & 0xf)

#define	BLACKBIRD_IMPL	0x11
#define	IS_BLACKBIRD(impl)	((impl) == BLACKBIRD_IMPL)
#define	BLACKBIRD_MAJOR_VERSION(rev)	(((rev) >> 4) & 0xf)
#define	BLACKBIRD_MINOR_VERSION(rev)	((rev) & 0xf)

/*
 * Bits of Spitfire Asynchronous Fault Status Register
 */
#define	P_AFSR_STICKY	0xC0000001FEE00000ULL /* mask for sticky bits, not CP */
#define	P_AFSR_ERRS	0x000000001EE00000ULL /* mask for remaining errors */
#define	P_AFSR_ME	0x0000000100000000ULL /* errors > 1, same type!=CE */
#define	P_AFSR_PRIV	0x0000000080000000ULL /* priv/supervisor access */
#define	P_AFSR_ISAP	0x0000000040000000ULL /* incoming system addr. parity */
#define	P_AFSR_ETP	0x0000000020000000ULL /* ecache tag parity */
#define	P_AFSR_IVUE	0x0000000010000000ULL /* interrupt vector with UE */
#define	P_AFSR_TO	0x0000000008000000ULL /* bus timeout */
#define	P_AFSR_BERR	0x0000000004000000ULL /* bus error */
#define	P_AFSR_LDP	0x0000000002000000ULL /* data parity error from SDB */
#define	P_AFSR_CP	0x0000000001000000ULL /* copyout parity error */
#define	P_AFSR_WP	0x0000000000800000ULL /* writeback ecache data parity */
#define	P_AFSR_EDP	0x0000000000400000ULL /* ecache data parity */
#define	P_AFSR_UE	0x0000000000200000ULL /* uncorrectable ECC error */
#define	P_AFSR_CE	0x0000000000100000ULL /* correctable ECC error */
#define	P_AFSR_ETS	0x00000000000F0000ULL /* cache tag parity syndrome */
#define	P_AFSR_P_SYND	0x000000000000FFFFULL /* data parity syndrome */

/*
 * Shifts for Spitfire Asynchronous Fault Status Register
 */
#define	P_AFSR_D_SIZE_SHIFT	(57)
#define	P_AFSR_ETS_SHIFT	(16)

/*
 * Bits of Spitfire Asynchronous Fault Status Register
 */
#define	S_AFSR_MASK	0x00000001FFFFFFFFULL /* <33:0>: valid AFSR bits */

/*
 * Bits of Spitfire Asynchronous Fault Address Register
 * The Sabre AFAR includes more bits since it only has a UDBH, no UDBL
 */
#define	S_AFAR_PA	0x000001FFFFFFFFF0ULL /* PA<40:4>: physical address */
#define	SABRE_AFAR_PA	0x000001FFFFFFFFF8ULL /* PA<40:3>: physical address */

/*
 * Bits of Spitfire/Sabre Error Enable Registers
 */
#define	EER_EPEN	0x00000000000000010ULL /* enable ETP, EDP, WP, CP */
#define	EER_UEEN	0x00000000000000008ULL /* enable UE */
#define	EER_ISAPEN	0x00000000000000004ULL /* enable ISAP */
#define	EER_NCEEN	0x00000000000000002ULL /* enable the other errors */
#define	EER_CEEN	0x00000000000000001ULL /* enable CE */
#define	EER_DISABLE	0x00000000000000000ULL /* no errors enabled */
#define	EER_ECC_DISABLE	(EER_EPEN|EER_UEEN|EER_ISAPEN)
#define	EER_CE_DISABLE	(EER_EPEN|EER_UEEN|EER_ISAPEN|EER_NCEEN)
#define	EER_ENABLE	(EER_EPEN|EER_UEEN|EER_ISAPEN|EER_NCEEN|EER_CEEN)

/*
 * Bits and vaddrs of Spitfire Datapath Error Registers
 */
#define	P_DER_UE	0x00000000000000200ULL	/* UE has occurred */
#define	P_DER_CE	0x00000000000000100ULL	/* CE has occurred */
#define	P_DER_E_SYND	0x000000000000000FFULL	/* SYND<7:0>: ECC syndrome */
#define	P_DER_H		0x0			/* datapath error reg upper */
#define	P_DER_L		0x18			/* datapath error reg upper */

/*
 * Bits of Spitfire Datapath Control Register
 */
#define	P_DCR_VER	0x000001E00		/* datapath version */
#define	P_DCR_F_MODE	0x000000100		/* send FCB<7:0> */
#define	P_DCR_FCB	0x0000000FF		/* ECC check bits to force */
#define	P_DCR_H		0x20			/* datapath control reg upper */
#define	P_DCR_L		0x38			/* datapath control reg lower */

#ifdef _KERNEL

#ifndef _ASM

void	get_udb_errors(uint64_t *udbh, uint64_t *udbl);

#endif /* !_ASM */

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPITREGS_H */
