/*
 * Copyright (c) 1992-1993, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_X86_ARCHEXT_H
#define	_SYS_X86_ARCHEXT_H

#pragma ident	"@(#)x86_archext.h	1.16	99/08/15 SMI"

#include <sys/regset.h>
#include <vm/seg_enum.h>
#include <vm/page.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	P5_PSE_SUPPORTED	0x08
#define	P5_TSC_SUPPORTED	0x10
#define	P5_MSR_SUPPORTED	0x20
#define	P6_PAE_SUPPORTED	0x40
#define	P6_MCE_SUPPORTED	0x80
#define	P6_CXS_SUPPORTED	0x100
#define	P6_APIC_SUPPORTED	0x200
#define	P6_MTRR_SUPPORTED	0x1000
#define	P6_PGE_SUPPORTED	0x2000
#define	P6_MCA_SUPPORTED	0x4000
#define	P6_CMOV_SUPPORTED	0x8000
#define	P6_PAT_SUPPORTED	0x10000
#define	P5_MMX_SUPPORTED	0x800000

#define	K5_PGE_SUPPORTED	0x20
#define	K5_SCE_SUPPORTED	0x40

/* K5 has different cr4 bit values than P5 and P6 */
#define	K5_PSE		0x10
#define	K5_GPE		0x20
#define	K5_PGE		0x200


#define	P5_MCHADDR	0x0
#define	P5_CESR		0x11
#define	P5_CTR0		0x12
#define	P5_CTR1		0x13

#define	K5_MCHADDR	0x0
#define	K5_MCHTYPE	0x01
#define	K5_TSC		0x10
#define	K5_TR12		0x12

#define	REG_MTRRCAP		0xfe
#define	REG_MTRRDEF		0x2ff
#define	REG_MTRR64K		0x250
#define	REG_MTRR16K1		0x258
#define	REG_MTRR16K2		0x259
#define	REG_MTRR4K1		0x268
#define	REG_MTRR4K2		0x269
#define	REG_MTRR4K3		0x26a
#define	REG_MTRR4K4		0x26b
#define	REG_MTRR4K5		0x26c
#define	REG_MTRR4K6		0x26d
#define	REG_MTRR4K7		0x26e
#define	REG_MTRR4K8		0x26f
#define	REG_MTRRPAT		0x277

#define	REG_MTRRPHYSBASE0	0x200
#define	REG_MTRRPHYSMASK7	0x20f
#define	REG_MC0_CTL		0x400
#define	REG_MC5_MISC		0x417
#define	REG_PERFCTR0		0xc1
#define	REG_PERFCTR1		0xc2

#define	REG_PERFEVNT0		0x186
#define	REG_PERFEVNT1		0x187

#define	REG_TSC			0x10
#define	REG_APIC_BASE_MSR	0x1b


#define	REG_MCG_CAP		0x179
#define	REG_MCG_STATUS		0x17a
#define	REG_MCG_CTL		0x17b

#define	REG_MC0_CTL		0x400
#define	REG_MC0_STATUS		0x401
#define	REG_MC0_ADDR		0x402
#define	REG_MC0_MISC		0x403
#define	REG_MC1_CTL		0x404
#define	REG_MC1_STATUS		0x405
#define	REG_MC1_ADDR		0x406
#define	REG_MC1_MISC		0x407
#define	REG_MC2_CTL		0x408
#define	REG_MC2_STATUS		0x409
#define	REG_MC2_ADDR		0x40a
#define	REG_MC2_MISC		0x40b
#define	REG_MC4_CTL		0x40c
#define	REG_MC4_STATUS		0x40d
#define	REG_MC4_ADDR		0x40e
#define	REG_MC4_MISC		0x40f
#define	REG_MC3_CTL		0x410
#define	REG_MC3_STATUS		0x411
#define	REG_MC3_ADDR		0x412
#define	REG_MC3_MISC		0x413

#define	P6_MCG_CAP_COUNT	5
#define	MCG_CAP_COUNT_MASK	0xff
#define	MCG_CAP_CTL_P		0x100

#define	MCG_STATUS_RIPV		0x01
#define	MCG_STATUS_EIPV		0x02
#define	MCG_STATUS_MCIP		0x04

#define	MCG_CTL_VALUE		0xffffffff

#define	MCI_CTL_VALUE		0xffffffff
#define	MCI_STATUS_ERRCODE	0xffff
#define	MCI_STATUS_MSERRCODE	0xffff0000
#define	MCI_STATUS_PCC		((long long)0x200000000000000)
#define	MCI_STATUS_ADDRV	((long long)0x400000000000000)
#define	MCI_STATUS_MISCV	((long long)0x800000000000000)
#define	MCI_STATUS_EN		((long long)0x1000000000000000)
#define	MCI_STATUS_UC		((long long)0x2000000000000000)
#define	MCI_STATUS_O		((long long)0x4000000000000000)
#define	MCI_STATUS_VAL		((long long)0x8000000000000000)

#define	MSERRCODE_SHFT		16


#define	MTRRTYPE_MASK		0xff


#define	MTRRCAP_FIX		0x100
#define	MTRRCAP_VCNTMASK	0xff
#define	MTRRCAP_USWC		0x400

#define	MTRRDEF_E		0x800
#define	MTRRDEF_FE		0x400

#define	MTRRPHYSMASK_V		0x800

#define	MTRR_TYPE_UC		0
#define	MTRR_TYPE_WC		1
#define	MTRR_TYPE_WT		4
#define	MTRR_TYPE_WP		5
#define	MTRR_TYPE_WB		6

/*
 * Page attribute table is setup in the following way
 * PAT0	Write-BACK
 * PAT1	Write-Through
 * PAT2	Unchacheable
 * PAT3	Uncacheable
 * PAT4 Uncacheable
 * PAT5	Write-Protect
 * PAT6	Write-Combine
 * PAT7 Uncacheable
 */
#define	PAT_DEFAULT_ATTRIBUTE \
((uint64_t)MTRR_TYPE_WC << 48)|((uint64_t)MTRR_TYPE_WP << 40)|\
(MTRR_TYPE_WT << 8)|(MTRR_TYPE_WB)


#define	MTRR_SETTYPE(a, t)	((a &= (uint64_t)~0xff),\
				    (a |= ((t) & 0xff)))
#define	MTRR_SETVINVALID(a)	((a) &= ~MTRRPHYSMASK_V)


#define	MTRR_SETVBASE(a, b, t)	((a) =\
					((((uint64_t)(b)) & 0xffffff000)|\
					(((uint32_t)(t)) & 0xff)))

#define	MTRR_SETVMASK(a, s, v) ((a) =\
				((~(((uint64_t)(s)) - 1) & 0xffffff000)|\
					(((uint32_t)(v)) << 11)))

#define	MTRR_GETVBASE(a)	(((uint64_t)(a)) & 0xffffff000)
#define	MTRR_GETVTYPE(a)	(((uint64_t)(a)) & 0xff)
#define	MTRR_GETVSIZE(a)	((~((uint64_t)(a)) + 1) & 0xffffff000)


#define	MAX_MTRRVAR	8
typedef	struct	mtrrvar {
	uint64_t	mtrrphys_base;
	uint64_t	mtrrphys_mask;
} mtrrvar_t;


#define	X86_LARGEPAGE	0x01
#define	X86_TSC		0x02
#define	X86_MSR		0x04
#define	X86_MTRR	0x08
#define	X86_PGE		0x10
#define	X86_APIC	0x20
#define	X86_CMOV	0x40
#define	X86_MMX 	0x80
#define	X86_MCA		0x100
#define	X86_PAE		0x200
#define	X86_CXS		0x400
#define	X86_PAT		0x800

#define	X86_P5		0x10000
#define	X86_K5		0x20000
#define	X86_P6		0x40000
#define	X86_CPUID	0x1000000
#define	X86_INTEL	0x2000000
#define	X86_AMD		0x4000000

#define	X86_CPU_TYPE	0xff0000


#define	INST_RDMSR	0x320f
#define	INST_WRMSR	0x300f
#define	INST_RTSC	0x310f

#define	RDWRMSR_INST_LEN	2

extern int	x86_feature;
extern uint64_t rdmsr(uint_t, uint64_t *);
extern void  wrmsr(uint_t, uint64_t *);
extern int mca_exception(struct regs *);
extern unsigned int	cr4();
extern void setcr4();
extern	void	mtrr_sync(void);



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X86_ARCHEXT_H */
