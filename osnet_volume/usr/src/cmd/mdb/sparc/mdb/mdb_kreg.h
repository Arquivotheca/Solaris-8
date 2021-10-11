/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_KREG_H
#define	_MDB_KREG_H

#pragma ident	"@(#)mdb_kreg.h	1.1	99/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __sparcv9cpu
#define	KREG_NGREG	50
typedef uint64_t kreg_t;
#else
#define	KREG_NGREG	38
typedef uint32_t kreg_t;
#endif

#define	KREG_G0		0
#define	KREG_G1		1
#define	KREG_G2		2
#define	KREG_G3		3
#define	KREG_G4		4
#define	KREG_G5		5
#define	KREG_G6		6
#define	KREG_G7		7
#define	KREG_O0		8
#define	KREG_O1		9
#define	KREG_O2		10
#define	KREG_O3		11
#define	KREG_O4		12
#define	KREG_O5		13
#define	KREG_O6		14
#define	KREG_O7		15
#define	KREG_L0		16
#define	KREG_L1		17
#define	KREG_L2		18
#define	KREG_L3		19
#define	KREG_L4		20
#define	KREG_L5		21
#define	KREG_L6		22
#define	KREG_L7		23
#define	KREG_I0		24
#define	KREG_I1		25
#define	KREG_I2		26
#define	KREG_I3		27
#define	KREG_I4		28
#define	KREG_I5		29
#define	KREG_I6		30
#define	KREG_I7		31

#define	KREG_SP		KREG_O6
#define	KREG_FP		KREG_I6

#ifdef __sparcv9cpu
#define	KREG_CCR	32
#else
#define	KREG_PSR	32
#endif

#define	KREG_PC		33
#define	KREG_NPC	34
#define	KREG_Y		35

#ifdef __sparcv9cpu

#define	KREG_ASI	36
#define	KREG_FPRS	37
#define	KREG_TICK	38
#define	KREG_PSTATE	39
#define	KREG_TL		40
#define	KREG_PIL	41
#define	KREG_TBA	42
#define	KREG_VER	43
#define	KREG_CWP	44
#define	KREG_CANSAVE	45
#define	KREG_CANRESTORE	46
#define	KREG_OTHERWIN	47
#define	KREG_WSTATE	48
#define	KREG_CLEANWIN	49

#else	/* __sparcv9cpu */

#define	KREG_WIM	36
#define	KREG_TBR	37

#endif	/* __sparcv9cpu */

#ifdef __sparcv9cpu

#define	KREG_CCR_XCC_N_MASK	0x80
#define	KREG_CCR_XCC_Z_MASK	0x40
#define	KREG_CCR_XCC_V_MASK	0x20
#define	KREG_CCR_XCC_C_MASK	0x10

#define	KREG_CCR_ICC_N_MASK	0x08
#define	KREG_CCR_ICC_Z_MASK	0x04
#define	KREG_CCR_ICC_V_MASK	0x02
#define	KREG_CCR_ICC_C_MASK	0x01

#define	KREG_FPRS_FEF_MASK	0x4
#define	KREG_FPRS_FEF_SHIFT	2

#define	KREG_FPRS_DU_MASK	0x2
#define	KREG_FPRS_DU_SHIFT	1

#define	KREG_FPRS_DL_MASK	0x1
#define	KREG_FPRS_DL_SHIFT	0

#define	KREG_TICK_NPT_MASK	0x8000000000000000ULL
#define	KREG_TICK_NPT_SHIFT	63

#define	KREG_TICK_CNT_MASK	0x7fffffffffffffffULL
#define	KREG_TICK_CNT_SHIFT	0

#define	KREG_PSTATE_CLE_MASK	0x200
#define	KREG_PSTATE_CLE_SHIFT	9

#define	KREG_PSTATE_TLE_MASK	0x100
#define	KREG_PSTATE_TLE_SHIFT	8

#define	KREG_PSTATE_MM_MASK	0x0c0
#define	KREG_PSTATE_MM_SHIFT	6

#define	KREG_PSTATE_MM_TSO(x)	(((x) & KREG_PSTATE_MM_MASK) == 0x000)
#define	KREG_PSTATE_MM_PSO(x)	(((x) & KREG_PSTATE_MM_MASK) == 0x040)
#define	KREG_PSTATE_MM_RMO(x)	(((x) & KREG_PSTATE_MM_MASK) == 0x080)
#define	KREG_PSTATE_MM_UNDEF(x)	(((x) & KREG_PSTATE_MM_MASK) == 0x0c0)

#define	KREG_PSTATE_RED_MASK	0x020
#define	KREG_PSTATE_RED_SHIFT	5

#define	KREG_PSTATE_PEF_MASK	0x010
#define	KREG_PSTATE_PEF_SHIFT	4

#define	KREG_PSTATE_AM_MASK	0x008
#define	KREG_PSTATE_AM_SHIFT	3

#define	KREG_PSTATE_PRIV_MASK	0x004
#define	KREG_PSTATE_PRIV_SHIFT	2

#define	KREG_PSTATE_IE_MASK	0x002
#define	KREG_PSTATE_IE_SHIFT	1

#define	KREG_PSTATE_AG_MASK	0x001
#define	KREG_PSTATE_AG_SHIFT	0

#define	KREG_TSTATE_CCR(x)	(((x) >> 32) & 0xff)
#define	KREG_TSTATE_ASI(x)	(((x) >> 24) & 0xff)
#define	KREG_TSTATE_PSTATE(x)	(((x) >> 8) & 0xfff)
#define	KREG_TSTATE_CWP(x)	((x) & 0x1f)

#define	KREG_TBA_TBA_MASK	0xffffffffffff8000ULL
#define	KREG_TBA_TBA_SHIFT	0

#define	KREG_TBA_TLG0_MASK	0x4000
#define	KREG_TBA_TLG0_SHIFT	14

#define	KREG_TBA_TT_MASK	0x3fd0
#define	KREG_TBA_TT_SHIFT	5

#define	KREG_VER_MANUF_MASK	0xffff000000000000ULL
#define	KREG_VER_MANUF_SHIFT	48

#define	KREG_VER_IMPL_MASK	0x0000ffff00000000ULL
#define	KREG_VER_IMPL_SHIFT	32

#define	KREG_VER_MASK_MASK	0xff000000
#define	KREG_VER_MASK_SHIFT	24

#define	KREG_VER_MAXTL_MASK	0x0000ff00
#define	KREG_VER_MAXTL_SHIFT	8

#define	KREG_VER_MAXWIN_MASK	0x0000000f
#define	KREG_VER_MAXWIN_SHIFT	0

#else	/* __sparcv9cpu */

#define	KREG_PSR_IMPL_MASK	0xf0000000
#define	KREG_PSR_IMPL_SHIFT	28

#define	KREG_PSR_VER_MASK	0x0f000000
#define	KREG_PSR_VER_SHIFT	24

#define	KREG_PSR_ICC_MASK	0x00f00000
#define	KREG_PSR_ICC_N_MASK	0x00800000
#define	KREG_PSR_ICC_Z_MASK	0x00400000
#define	KREG_PSR_ICC_V_MASK	0x00200000
#define	KREG_PSR_ICC_C_MASK	0x00100000
#define	KREG_PSR_ICC_SHIFT	20

#define	KREG_PSR_EC_MASK	0x00002000
#define	KREG_PSR_EC_SHIFT	13

#define	KREG_PSR_EF_MASK	0x00001000
#define	KREG_PSR_EF_SHIFT	12

#define	KREG_PSR_PIL_MASK	0x00000f00
#define	KREG_PSR_PIL_SHIFT	8

#define	KREG_PSR_S_MASK		0x00000080
#define	KREG_PSR_S_SHIFT	7

#define	KREG_PSR_PS_MASK	0x00000040
#define	KREG_PSR_PS_SHIFT	6

#define	KREG_PSR_ET_MASK	0x00000020
#define	KREG_PSR_ET_SHIFT	5

#define	KREG_PSR_CWP_MASK	0x0000001f
#define	KREG_PSR_CWP_SHIFT	0

#define	KREG_TBR_TBA_MASK	0xfffff000
#define	KREG_TBR_TBA_SHIFT	0

#define	KREG_TBR_TT_MASK	0x00000ff0
#define	KREG_TBR_TT_SHIFT	4

#endif	/* __sparcv9cpu */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_KREG_H */
