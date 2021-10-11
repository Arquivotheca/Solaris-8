/*
 * Copyright (c) 1990,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MMU_H
#define	_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.65	98/01/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/* Can map all addresses */

#define	good_addr(a)  (1)
#endif /* !_ASM */

/*
 * Definitions for the Sparc Reference MMU
 */

#ifndef _ASM
extern unsigned int nctxs;
#endif /* _ASM */

/*
 * Address Space Identifiers (see SPARC Arch Manual, Appendix I)
 */

/*			0x0	reserved */
/*			0x1	unassigned */
#define	ASI_MXCC	0x2	/* Viking MXCC registers */
#define	ASI_FLPR	0x3	/* Reference MMU flush/probe */
#define	ASI_MOD		0x4	/* Modlule control/status register */
#define	ASI_ITLB	0x5	/* RefMMU diagnostic for Instruction TLB */
#define	ASI_DTLB	0x6	/* RefMMU diag for Data TLB */
#define	ASI_IOTLB	0x7	/* RefMMU diag for I/O TLB */
#define	ASI_UI		0x8	/* user instruction */
#define	ASI_SI		0x9	/* supervisor instruction */
#define	ASI_UD		0xA	/* user data */
#define	ASI_SD		0xB	/* supervisor data */
#define	ASI_ICT		0xC	/* I-Cache Tag */
#define	ASI_ICD		0xD	/* I-Cache Data */
#define	ASI_DCT		0xE	/* D-Cache Tag */
#define	ASI_DCD		0xF	/* D-Cache Data */
#define	ASI_FCP		0x10	/* flush cache page */
#define	ASI_FCS		0x11	/* flush cache segment */
#define	ASI_FCR		0x12	/* flush cache region */
#define	ASI_FCC		0x13	/* flush cache context */
#define	ASI_FCU		0x14	/* flush cache user */
/*			0x15 - 0x16	reserved */
#define	ASI_BC		0x17	/* Block copy */
#define	ASI_FDCP	0x18	/* flush D cache page */
#define	ASI_FDCS	0x19	/* flush D cache segment */
#define	ASI_FDCR	0x1A	/* flush D cache region */
#define	ASI_FDCC	0x1B	/* flush D cache context */
#define	ASI_FDCU	0x1C	/* flush D cache user */
/*			0x1D - 0x1E	reserved */
#define	ASI_BF		0x1F	/* Block Fill */
#define	ASI_PASS	0x20	/* MMU Bypass (low digit of ASI is PA[35:32]) */
#define	ASI_PASSMEM	0x20	/* MMU Bypass (low digit of ASI is PA[35:32]) */
#define	ASI_PASSMOD	0x2F	/* MMU Bypass (low digit of ASI is PA[35:32]) */

/*
 * MMU Pass-Through ASIs (small4m ASI 0x20 only)
 */
#define	ASI_MEM		0x20	/* direct access to physical memory */
#define	ASI_VIDEO	0x29	/* memory-based video (TBD) */
#define	ASI_SBUS	0x2E	/* S-bus direct access */
#define	ASI_CTL		0x2F	/* Control Space */

#define	ASI_SBT		0x30	/* store buffer tags */
#define	ASI_SBD		0x31	/* store buffer data */
#define	ASI_SBC		0x32	/* store buffer control */
/*			0x33 - 0x35	unassigned */
#define	ASI_ICFCLR	0x36	/* I-Cache Flash Clear */
#define	ASI_DCFCLR	0x37	/* D-Cache Flash Clear */
#define	ASI_MDIAG	0x38	/* Viking MMU Breakpoint Diagnostics */
#define	ASI_BIST	0x39	/* Viking BIST diagnostics */
/*			0x3A - 0x3F	reserved */
#define	ASI_MTMP1	0x40	/* Viking Emulation Temp1 */
#define	ASI_MTMP2	0x41	/* Viking Emulation Temp2 */
/*			0x42 - 0x43	reserved */
#define	ASI_MDIN	0x44	/* Viking Emulation Data In */
/*			0x45		reserved */
#define	ASI_MDOUT	0x46	/* Viking Emulation Data Out */
#define	ASI_MPC		0x47	/* Viking Emulation Exit PC */
#define	ASI_MNPC	0x48	/* Viking Emulation Exit NPC */
#define	ASI_MCTRV	0x49	/* Viking Emulation Counter Value */
#define	ASI_MCTRC	0x4A	/* Viking Emulation Counter Mode */
#define	ASI_MCTRS	0x4B	/* Viking Emulation Counter Status */
#define	ASI_MBAR	0x4C	/* Viking Breakpoint Action Reg */
/*			0x4D - 0x7F	unassigned */
/*			0x80 - 0xFF	reserved */

#define	ASI_RMMU	ASI_MOD		/* More descriptive ASI name */

#define	ASI_IBUF_FLUSH	0x31	/* Flush instruction buffer on HyperSparc */
#if defined(SAS)
#define	ASI_SAS 0x50
#endif /* SAS */

/*
 * Module Control Register bit mask defines - ASI_MOD
 * XXX - These were listed as CPU_*
 */
#define	MCR_IMPL	0xF0000000	/* SRMMU implementation number */
#define	MCR_VER		0x0F000000	/* SRMMU version number */
#define	MCR_BM		0x00004000	/* Boot mode */
#define	MCR_NF		0x00000002	/* No fault */
#define	MCR_ME		0x00000001	/* MMU enable */

/*
 * Module virtual addresses defined by the architecture (ASI_MOD)
 */
#define	RMMU_CTL_REG		0x000
#define	RMMU_CTP_REG		0x100
#define	RMMU_CTX_REG		0x200
#define	RMMU_FSR_REG		0x300
#define	RMMU_FAV_REG		0x400
#define	RMMU_AFS_REG		0x500
#define	RMMU_AFA_REG		0x600
#define	RMMU_RST_REG		0x700
#define	RMMU_TRCR		0x1400	/* TLB replacement control register */

/*
 * XXX-Need other Emulation (Debug) definitions added.
 */
#define	MBAR_MIX	0x00001000	/* Turn on SuperScalar */

/*
 * VIKING specific module control definitions (XXX - don't belong here)
 */
#define	CPU_VIK_PF	0x00040000	/* data prefetcher enable */
#define	CPU_VIK_TC	0x00010000	/* table-walk cacheable */
#define	CPU_VIK_AC	0x00008000	/* alternate cacheable */
#define	CPU_VIK_SE	0x00004000	/* snoop enable */
#define	CPU_VIK_BT	0x00002000	/* boot mode 0=boot 1=normal */
#define	CPU_VIK_PE	0x00001000	/* parity check enable */
#define	CPU_VIK_MB	0x00000800	/* copy-back mode enable (w/o E$) */
#define	CPU_VIK_SB	0x00000400	/* store buffer enable */
#define	CPU_VIK_IE	0x00000200	/* i-$ enable */
#define	CPU_VIK_DE	0x00000100	/* d-$ enable */
#define	CACHE_VIK_ON	(CPU_VIK_SE|CPU_VIK_SB|CPU_VIK_IE|CPU_VIK_DE)
#define	CACHE_VIK_ON_E	(CACHE_VIK_ON|CPU_VIK_TC|CPU_VIK_PF)
#define	VIK_PTAGADDR	0x80000000	/* Physical tag address format */
#define	VIK_STAGADDR	0x40000000	/* Set tag address format */
#define	VIK_PTAGVALID	0x01000000	/* Ptag Valid Bit */
#define	VIK_PTAGDIRTY	0x00010000	/* Ptag Dirty Bit */
#define	MXCC_TAGSADDR	0x01800000	/* MXCC Physical tag address format */
#define	MXCC_TAGSVALID	0x00002222	/* MXCC valid bits for its tags */
#define	MXCC_TAGSOWNED	0x00004444	/* MXCC owned bits for its tags */
/*
 * Viking MXCC registers addresses defined by the architecture (ASI_MXCC)
 */
#define	MXCC_STRM_SIZE	0x20		/* # bytes per transfer */
#define	MXCC_STRM_DATA	0x01C00000	/* stream data register */
#define	MXCC_STRM_SRC	0x01C00100	/* stream source */
#define	MXCC_STRM_DEST	0x01C00200	/* stream destination */
#define	MXCC_REF_MISS	0x01C00300	/* Reference/Miss count */
#define	MXCC_BIST	0x01C00804	/* Built-in Selftest */
#define	MXCC_CNTL	0x01C00A04	/* MXCC control register */
#define	MXCC_STATUS	0x01C00B00	/* MXCC status register */
#define	MXCC_RESET	0x01C00C04	/* module reset register */
#define	MXCC_ERROR	0x01C00E00	/* error register */
#define	MXCC_PORT	0x01C00F04	/* MBux port address register */

/*
 * Viking MXCC specific control definitions
 */
#define	MXCC_CTL_HC	0x00000001	/* Half Cache */
#define	MXCC_CTL_CS	0x00000002	/* Cache Size */
#define	MXCC_CE		0x00000004	/* E$ enable */
#define	MXCC_PE		0x00000008	/* Parity enable */
#define	MXCC_MC		0x00000010	/* Multiple command enable */
#define	MXCC_PF		0x00000020	/* Prefetch enable */
#define	MXCC_RC		0x00000200	/* Read reference count */
#define	CACHE_MXCC_ON	(MXCC_CE|MXCC_MC|MXCC_PF)

/*
 * Viking MXCC specific error register definitions (bit<63:32>)
 */
#define	MXCC_ERR_ME	0x80000000	/* multiple errors */
#define	MXCC_ERR_CC	0x20000000	/* cache consistency error */
#define	MXCC_ERR_VP	0x10000000	/* parity err on viking write(UD) */
#define	MXCC_ERR_CP	0x08000000	/* parity error on E$ access */
#define	MXCC_ERR_AE	0x04000000	/* Async. error */
#define	MXCC_ERR_EV	0x02000000	/* Error information valid */
#define	MXCC_ERR_CCOP	0x01FF8000	/* MXCC operation code */
#define	MXCC_ERR_ERR	0x00007F80	/* Error code */
#define	MXCC_ERR_S	0x00000040	/* supervisor mode */
#define	MXCC_ERR_PA	0x0000000F	/* physical address <35:32> */

#define	MXCC_ERR_ERR_SHFT	7
#define	MXCC_ERR_CCOP_SHFT	15

/*
 * Ross605 specific module control register definitions (XXX - don't belong)
 */
#define	CPU_PT		0x00040000	/* Parity Test */
#define	CPU_PE		0x00020000	/* Parity Enable */
#define	CPU_DE		0x00010000	/* Dual Directory enable */
#define	CPU_IC		0x00008000	/* Instruction/Data Cache */
#define	CPU_C		0x00002000	/* Cacheable bit for 2nd level cache */
#define	CPU_CP		0x00001800	/* Cache Parameters */
#define	CPU_CB		0x00000400	/* Write back cache */
#define	CPU_CE		0x00000100	/* Cache enable */

#define	CACHE_ON	CPU_DE + CPU_CP + CPU_CB + CPU_CE

/* Some control register shift values */
/*
 * context table register is bits 35-6 of pa of context table in bits 31-2 of
 * context table register
 */
#define	RMMU_CTP_SHIFT	2

/*
 * Reset Register Layout
 */
#define	RSTREG_WD	0x00000004 /* Watchdog Reset */
#define	RSTREG_SI	0x00000002 /* Software internal Reset */

/*
 * Synchronous Fault Status Register Layout
 */
#define	MMU_SFSR_OW		0x00000001	/* multiple errors occured */
#define	MMU_SFSR_FAV		0x00000002	/* fault address valid */
#define	MMU_SFSR_FT_SHIFT	2
#define	MMU_SFSR_FT_MASK	0x0000001C	/* fault type mask */
#define	MMU_SFSR_FT_NO		0x00000000	/* .. no error */
#define	MMU_SFSR_FT_INV		0x00000004	/* .. invalid address */
#define	MMU_SFSR_FT_PROT	0x00000008	/* .. protection error */
#define	MMU_SFSR_FT_PRIV	0x0000000C	/* .. privilege violation */
#define	MMU_SFSR_FT_TRAN	0x00000010	/* .. translation error */
#define	MMU_SFSR_FT_BUS		0x00000014	/* .. bus access error */
#define	MMU_SFSR_FT_INT		0x00000018	/* .. internal error */
#define	MMU_SFSR_FT_RESV	0x0000001C	/* .. reserved code */
#define	MMU_SFSR_AT_SHFT	5
#define	MMU_SFSR_AT_SUPV	0x00000020	/* access type: SUPV */
#define	MMU_SFSR_AT_INSTR	0x00000040	/* access type: INSTR */
#define	MMU_SFSR_AT_STORE	0x00000080	/* access type: STORE */
#define	MMU_SFSR_LEVEL_SHIFT	8
#define	MMU_SFSR_LEVEL		0x00000300	/* table walk level */
#define	MMU_SFSR_BE		0x00000400	/* M-Bus Bus Error */
#define	MMU_SFSR_TO		0x00000800	/* M-Bus Timeout */
#define	MMU_SFSR_UC		0x00001000	/* M-Bus Uncorrectable */
#define	MMU_SFSR_UD		0x00002000	/* Viking: Undefined Error */
#define	MMU_SFSR_FATAL		0x00003C00	/* guaranteed fatal cond. */
#define	MMU_SFSR_P		0x00004000	/* Viking: Parity Error, */
						/* also set MMU_SFSR_UC */
#define	MMU_SFSR_SB		0x00008000	/* Viking: Store Buffer Error */
#define	MMU_SFSR_CS		0x00010000	/* Viking: Control Space */
						/* Access Error */
#define	MMU_SFSR_EM		0x00020000	/* Error Mode Reset */
#define	MMU_SFSR_EBE		0x0001fc00	/* extended bus error bits */
/*
 * Asynchronous Fault Status Register Layout
 */
#define	MMU_AFSR_AFV		0x00000001	/* async fault occured */
#define	MMU_AFSR_AFA_SHIFT	4
#define	MMU_AFSR_AFA		0x000000F0	/* high 4 bits of fault addr */
#define	MMU_AFSR_BE		0x00000400	/* M-Bus Bus Error */
#define	MMU_AFSR_TO		0x00000800	/* M-Bus Timeout */
#define	MMU_AFSR_UC		0x00001000	/* M-Bus Uncorrectable */

#define	MMU_AFSR_FATAL		0x00001C00	/* guaranteed fatal cond. */

#ifdef	__cplusplus
}
#endif

/*
 * XXX	Most of following macros don't belong here.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * GETCPU(r)
 * Returns cpu id (8..11) in register r.
 * Uses the module-id register
 *
 * FIXME: This is not correct for 5.0MT.  If it is ever used,
 * we need it to get the module id out of the cpu structure.
 */
#ifdef	FIXME
#define	GETCPU(r)			\
	sethi	%hi(V_MODID_ADDR), r;	\
	ld	[r+%lo(V_MODID_ADDR)], r
#endif	/* FIXME */

/*
 * CPU_INDEX(r)
 * Returns cpu id (0..3) in register r.
 * It uses the address in the tbr to determine which cpu it is.
 * The current addresses for cpu's 0-3 are scb, V_TBR_ADDR_CPU1,
 * V_TBR_ADDR_CPU2 and V_TBR_ADDR_CPU3 which have the values
 * f8004000, fefd5000, fefd6000 and fefd7000.  If scb ever changes
 * location so that bits 13 and 14 are no longer 0 this macro will
 * stop working for cpu0.  If that happens the easiest way to fix
 * it is to map fefd4000 to the scb just like the others and store
 * that addr in cpu0's tbr.  I didn't bother doing that now because
 * I didn't have to.
 */
#if defined(sun4m)
#define	CPU_INDEX(r)			\
	mov	%tbr, r;		\
	srl	r, 12, r;		\
	and	r, 3, r
#endif /* sun4m */
/*
 * Get the Module Offset macro
 *
 * GETMOFF(r)
 * Returns module space offset in register r
 * suitable for "or"ing into a system space address.
 * Uses the new module-id register
 * replace GETMID calls with GETMOFF calls
 */
#define	GETMOFF(r)		\
	CPU_INDEX(r);		\
	sll	r, 24, r

/*
 * Memory related Addresses - ASI_MMUPASS
 */

#define	ECC_EN			0x0		/* ECC Memory Enable Register */
#define	ECC_FSR			0x8		/* ECC Memory Fault Status */
#define	ECC_FAR			0x10		/* ECC Memory Fault Address */
#define	ECC_DIAG		0x18		/* ECC Diagnostic Register */
#define	MBUS_ARB_EN		0x20		/* Mbus Arbiter Enable */
#define	DIAG_MESS		0x1000		/* Diagnostic Message Passing */

/*
 * Mbus Arbiter bit fields
 */
#define	EN_ARB_P1		0x2		/* Enable Mbus Arb. Module 1 */
#define	EN_ARB_P2		0x4		/* Enable Mbus Arb. Module 2 */
#define	EN_ARB_P3		0x8		/* Enable Mbus Arb. Module 3 */
#define	EN_ARB_SBUS		0x10		/* Enable Mbus Arb. Sbus */

/* XXX */
#if 0
#define	EN_ARB_SBUS		0x1F0000	/* Enable Mbus Arb. Sbus */
#endif

#define	SYSCTLREG_WD		0x0010		/* sys control/status WDOG */

/* flags used in srmmu_tlbflush */
#define	FL_ALLCPUS	0x0		/* flush all cpus */
#define	FL_LOCALCPU	0x1		/* flush local cpu only */

/* flags used in vac_XXX() functions */
#define	FL_ALLCPUS	0x0			/* flush all cpus */
#define	FL_LOCALCPU	0x1			/* flush local cpu only */
#define	FL_TLB		0x2			/* flush tlb */
#define	FL_CACHE	0x4			/* flush cache */
#define	FL_TLB_CACHE	(FL_TLB|FL_CACHE)	/* flush tlb and cache */


#if defined(_KERNEL) && !defined(_ASM)
/*
 * Low level mmu-specific functions
 */
/*
 * Cache specific routines - ifdef'ed out if there is no chance
 * of running on a machine with a virtual address cache.
 */
#ifdef VAC
void	vac_init();
void	vac_usrflush(/* uint_t flags */);
void	vac_ctxflush(/* int cxn, uint_t flags */);
void	vac_rgnflush(/* caddr_t va, int cxn, uint_t flags */);
void	vac_segflush(/* caddr_t va, int cxn, uint_t flags */);
void	vac_pageflush(/* caddr_t va, int cxn, uint_t flags */);
void	vac_flush(/* caddr_t va, int size */);
void	vac_allflush(/* uint_t flags */);

#else /* !VAC */
#define	vac_init()
#define	vac_usrflush(flags)
#define	vac_ctxflush(cxn, flags)
#define	vac_rgnflush(va, cxn, flags)
#define	vac_segflush(va, cxn, flags)
#define	vac_pageflush(va, cxn, flags)
#define	vac_flush(va, size)
#define	vac_allflush(flags)
#endif /* !VAC */

int	valid_va_range(/* basep, lenp, minlen, dir */);

#endif /* defined(_KERNEL) && !defined(_ASM) */

#if defined(_KERNEL) && !defined(_ASM)
struct ControlRegister {
	unsigned cr_sysControl:8;
	unsigned cr_reserved:22;
	unsigned cr_noFault:1;
	unsigned cr_enabled:1;
};

#define	X_SYS_CONTROL(x) (((struct ControlRegister *)&(x))->cr_sysControl)
#define	MSYS_CONTROL		X_SYS_CONTROL(CtrlRegister)
#define	X_CRRESERVED(x) (((struct ControlRegister *)&(x))->cr_reserved)
#define	MCRRESERVED		X_CRRESERVED(CtrlRegister)
#define	X_NO_FAULT(x) (((struct ControlRegister *)&(x))->cr_noFault)
#define	MNO_FAULT		X_NO_FAULT(CtrlRegister)
#define	X_ENABLED(x)  (((struct ControlRegister *)&(x))->cr_enabled)
#define	MENABLED		X_ENABLED(CtrlRegister)

	/* The context register */
struct CtxtTablePtr {
	unsigned ct_tablePointer:30;
	unsigned ct_reserved:2;
};

#define	X_TABLE_POINTER(x) (((struct CtxtTablePtr *)&(x))->ct_tablePointer)
#define	MTABLE_POINTER		X_TABLE_POINTER(CtxtTablePtr)
#define	X_CTRESERVED(x) (((struct CtxtTablePtr *)&(x))->ct_reserved)
#define	MCTRESERVED		X_CTRESERVED(CtxtTablePtr)


	/* The Fault status register */
struct FaultStatus {
	unsigned fs_Reserved:14;
	unsigned fs_ExternalBusError:8;
	unsigned fs_Level:2;
	unsigned fs_AccessType:3;
	unsigned fs_FaultType:3;
	unsigned fs_FaultAddressValid:1;
	unsigned fs_Overwrite:1;
};
#define	X_FSRESERVED(x) (((struct FaultStatus *)&(x))->fs_Reserved)
#define	MFSRESERVED		X_FSRESERVED(MMUFaultStatus)
#define	X_EBE(x) (((struct FaultStatus *)&(x))->fs_ExternalBusError)
#define	MEBUSERROR		X_EBE(MMUFaultStatus)
#define	X_LEVEL(x) (((struct FaultStatus *)&(x))->fs_Level)
#define	MLEVEL		X_LEVEL(MMUFaultStatus)
#define	X_ACCESS_TYPE(x) (((struct FaultStatus *)&(x))->fs_AccessType)
#define	MACCESS_TYPE		X_ACCESS_TYPE(MMUFaultStatus)
/* The following doesn't work with -O and register x, see below */
/* #define	X_FAULT_TYPE(x) (((struct FaultStatus *)&(x))->fs_FaultType) */
#define	MFAULT_TYPE		X_FAULT_TYPE(MMUFaultStatus)
#define	X_FAULT_AV(x) (((struct FaultStatus *)&(x))->fs_FaultAddressValid)
#define	MFAULT_AV		X_FAULT_AV(MMUFaultStatus)
#define	X_OVERWRITE(x) (((struct FaultStatus *)&(x))->fs_Overwrite)
#define	MOVERWRITE		X_OVERWRITE(MMUFaultStatus)

#endif /* defined(_KERNEL) && !defined(_ASM) */

/*
 * Mbus Port Address Register Layout
 */
#define	MPAREG_MCA	0xF0000000 /* Master Control Address */
#define	MPAREG_MN	0x0E000000 /* Module Number */
#define	MPAREG_DI	0x01000000 /* D-Cache/I-Cache field (0=I, 1=D) */
#define	MPAREG_MID	0x0F000000 /* MID field (MN and DI fields) */
#define	MPAREG_IMASK	0xFE000000 /* Mask to get I-side base address */
#define	MPAREG_MDEV	0x0000FF00 /* Mbus Device Number */
#define	MPAREG_MREV	0x000000F0 /* Device Revision Number */
#define	MPAREG_MVEND	0x0000000F /* Mbus vendor number */

/*
 * MMU/Cache Control Register
 */
#define	MMCREG_IMPL	0xF0000000 /* Implementation Number */
#define	MMCREG_VER	0x0F000000 /* Version number of the SRMMU */
#define	MMCREG_PT	0x00040000 /* Parity Test */
#define	MMCREG_PE	0x00020000 /* Parity Enable */
#define	MMCREG_DE	0x00010000 /* Dual Directory Enable */
#define	MMCREG_IC	0x00008000 /* Instruction Cache configuration bit */
#define	MMCREG_BM	0x00004000 /* Boot Mode indicator */
#define	MMCREG_C	0x00002000 /* Cacheable bit for 2nd level caches */
#define	MMCREG_CP	0x00001800 /* Cache parameter field (3=256k) */
#define	MMCREG_CB	0x00000400 /* Copy-Back cache indicator */
#define	MMCREG_CE	0x00000100 /* Cache Enable */
#define	MMCREG_NF	0x00000002 /* No-Fault */
#define	MMCREG_ME	0x00000001 /* MMU Enable */

/*
 * Synchronous Fault Status Register
 */
#define	SFSREG_CSA	0x00004000 /* Control Space Access error */
#define	SFSREG_UD	0x00002000 /* Undefined Error */
#define	SFSREG_UC	0x00001000 /* Uncorrectable */
#define	SFSREG_TO	0x00000800 /* TimeOut */
#define	SFSREG_BE	0x00000400 /* Bus Error */
#define	SFSREG_L	0x00000300 /* Level: 0=ctx 1=rgn 2=seg 3=page */
#define	SFSREG_AT	0x000000E0 /* Access type [ld=0/st=4 D=0/I=2 u=0/s=1] */
#define	SFSREG_FT	0x0000001C /* Fault Type */
#define	SFSREG_FAV	0x00000002 /* Fault Address Valid */
#define	SFSREG_OW	0x00000001 /* Overwrite */

/* Defines for Fault status register AccessType */
#define	AT_UREAD	0
#define	AT_SREAD	1
#define	AT_UEXECUTE	2
#define	AT_SEXECUTE	3
#define	AT_UWRITE	4
#define	AT_SWRITE	5

#define	FS_ATSHIFT	5	/* amt. to shift left to get access type */
#define	FS_ATMASK	0x7	/* After shift, mask by this amt. to get type */

/*
 * Fault Types for SFSREG_FT
 */
#define	FT_INVAL	0x04	/* invalid address */
#define	FT_PROT		0x08	/* protection */
#define	FT_PRIV		0x0C	/* privilege */
#define	FT_TRANS	0x10	/* translation */
#define	FT_ACCESS	0x14	/* access bus */
#define	FT_INTERN	0x18	/* internal */


/* Defines for Fault type field of the faultstatus register */

#define	FT_NONE			0
#define	FT_INVALID_ADDR		1
#define	FT_PROT_ERROR		2
#define	FT_PRIV_ERROR		3
#define	FT_TRANS_ERROR		4
#define	FT_ACC_BUSERR		5
#define	FT_INTERNAL		6

/*
 * Asynchronous Fault Status Register
 */
#define	AFSREG_PERR	0xFF000000 /* Cache parity error(s) */
#define	AFSREG_UD	0x00002000 /* Undefined Error */
#define	AFSREG_UC	0x00001000 /* Uncorrectable */
#define	AFSREG_TO	0x00000800 /* Timeout */
#define	AFSREG_BE	0x00000400 /* Bus Error */
#define	AFSREG_AFA	0x000000F0 /* bits 35:32 of the fault address */
#define	AFSREG_AOW	0x00000002 /* Asynchronous Overwrite */
#define	AFSREG_AFV	0x00000001 /* Asynchronous Fault Address Valid */
#define	AFSREG_FATAL	(AFSREG_UC|AFSREG_TO|AFSREG_BE)

/*
 * Flush Types
 */
#define	FT_PAGE		0x00000000 /* page flush */
#define	FT_SEG		0x00000001 /* segment flush */
#define	FT_RGN		0x00000002 /* region flush */
#define	FT_CTX		0x00000003 /* context flush */
#define	FT_USER		0x00000004 /* user flush [cache] */
#define	FT_ALL		0x00000004 /* flush all [tlb] */

#define	FS_FTSHIFT	2	/* amt. to shift right to get flt type */
#define	FS_FTMASK	0x7	/* and then mask by this to get fault type */
/* See above */
#define	X_FAULT_TYPE(x) ((((unsigned)x) >> FS_FTSHIFT) & FS_FTMASK)

#define	FS_EBESHIFT	10	/* amt. to shift right to get ext. bus error */
#define	FS_EBEMASK	0xFF	/* and mask by this value */

#define	FS_FAVSHIFT	1	/* amt. to shift right to get address valid */
#define	FS_FAVMASK	0x1	/* and mask by this value to get av bit */

#define	FS_OWMASK	0x1	/* mask by this value to get ow bit */
#define	MMUERR_BITS	"\20\10EBE\9L23\8L01\7ST\6USER\5EXEC\1FAV\0OW"


#define	NL3PTEPERPT	64	/* entries in level 3 table */
#define	NL3PTESHIFT	6	/* entries in level 3 table */
#define	L3PTSIZE	(NL3PTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	L3PTOFFSET	(L3PTSIZE - 1)
#define	NL2PTEPERPT	64	/* entries in table */
#define	L2PTSIZE	(NL2PTEPERPT * L3PTSIZE)    /* bytes mapped */
#define	L2PTOFFSET	(L2PTSIZE - 1)
#define	NL1PTEPERPT	256	/* entries in table */

#define	L3PTSHIFT	8	/* size of l3 table */
#define	L1PTSHIFT	10	/* size of l1 table */

#define	IDPROMSIZE	0x20		/* size of id prom in bytes */

/*
 * The usable DVMA space size.
 */
#define	DVMASIZE	((1024*1024) - L3PTSIZE)
#define	DVMABASE	(0-(1024*1024))

/*
 * Context for kernel. On a Sun-4 the kernel is in every address space,
 * but KCONTEXT is magic in that there is never any user context there.
 */
#define	KCONTEXT	0

/*
 * PPMAPBASE is the base virtual address of the range which
 * the kernel uses to quickly map pages for operations such
 * as ppcopy, pagecopy, pagezero, and pagesum.
 */
#define	PPMAPSIZE	(512 * 1024)
#define	PPMAPBASE	(ARGSBASE - PPMAPSIZE)

#ifndef _ASM
struct pte;

/* functions in mmu.c and mmu_asi.s */
extern void mmu_setpte(/* caddr_t base, struct pte pte */);
extern void mmu_getpte(/* caddr_t base, struct pte *ppte */);
extern void mmu_getkpte(caddr_t base, struct pte *ppte);
extern void mmu_pdcflush_all(void);
extern void mmu_pdcflush_ctxt(uint_t c_num);
extern void mmu_pdcflush_entry(caddr_t a);

/* mmu_asi.s */
extern unsigned mmu_getcr(void);
extern void mmu_setcr(uint_t c);
extern unsigned mmu_getctp(void);
extern void mmu_setctp(uint_t c);
extern unsigned mmu_getctx(void);
extern void mmu_setctxreg(uint_t c_num);
extern unsigned mmu_getsyncflt(void);
extern void mmu_getasyncflt(uint_t *ptr);
/* See union flush_probe to see how to construct vals */
extern uint_t mmu_probe(caddr_t probe_val, uint_t *fsr);
extern void mmu_flush(uint_t flush_val);
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MMU_H */
