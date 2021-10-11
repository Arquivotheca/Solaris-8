/*
 * Copyright (c) 1991,1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)intr_misc.s	1.43	99/06/05 SMI"

#if defined(lint)
#include <sys/types.h>
#endif

#include <sys/asm_linkage.h>
#include <sys/physaddr.h>
#include <sys/psr.h>

/*
 * SBus interrupt level for corresponding SPARC level (SPARC -> SBus)
 * It's subtle; we approximate the inverse of SBus -> SPARC
 */

#if defined(lint)

/* ARGSUSED */
int
intr_sbus_level(int sparc_level)
{ return (0); }

#else	/* lint */

/*
 * int
 * intr_sbus_level(sparc_level)
 *	int sparc_level;
 * {
 *	return ((sparc_level + 1) >> 1);
 * }
 */
	ENTRY_NP(intr_sbus_level)
	inc	%o0
	retl
	sra	%o0, 1, %o0			! delay slot
	SET_SIZE(intr_sbus_level)

#endif	/* lint */

/*
 * SPARC Trap Enable/Disable
 */
#define	PSR_DISABLE	0x26		/* ref - locore.s, bottom 7 bits */

#if defined(lint)

u_int
disable_traps(void)
{ return (0); }

/* ARGSUSED */
void
enable_traps(u_int psr_value)
{}

#else	/* lint */

	ENTRY_NP(enable_traps)
	.volatile
	wr	%o0, %psr		/* delay slot */
	nop
	nop
	retl
	nop
	.nonvolatile
	SET_SIZE(enable_traps)

	ENTRY_NP(disable_traps)
	rd	%psr, %o0		/* old value (maybe)		*/
	andcc	%o0, PSR_ET, %g0	/* already disabled?		*/
	bnz,a	1f			/* enabled, don't annul		*/
	.volatile
	ta	PSR_DISABLE		/* delay slot			*/
	.nonvolatile
	retl				/* fast case, already disabled	*/
	nop				/* delay slot			*/
1:	and	%o0, PSR_PS, %o2	/* old previous bit (don't lose it) */
	rd	%psr, %o0		/* new value (w/ET=0, PS=1)	*/
	or	%o0, %o2, %o0		/* restore PSR_PS from before trap */
	retl				/* slow case, traps had been enabled */
	or	%o0, PSR_ET, %o0	/* delay slot, ET=1		*/
	SET_SIZE(disable_traps)

#endif	/* lint */

/* system bus identifier */
#define	BUS_SHIFT	8
#define	BUS_BIT	(1 << BUS_SHIFT)

/*
 * BW Timer routines
 */
#define	ASI_BW		0x2f
#define	BW_CSR_BASE	0xe0000000	/* CSR */
#define	BW_LOCAL_BASE	0xfff00000
#define	BW1_LOCAL_BASE	(BW_LOCAL_BASE + BUS_BIT)
#define	BW_CONTROL	0x1000
#define	BW_COMPID	0x0000
#define	BW_UCEN		0x2018
#define	BW_XD_CSR	0x8

/*
 * SHIFT(s) - distance to shift device_id's to create qualifier, pg. 21
 */

#define	PROF_LIMIT	0x2000
#define	PROF_NDLIMIT	0x2008
#define PROF_COUNTER	0x2010
#define	PROF_CONTROL	0x2018

/*
 * CPU id To BW CSR address  Macros
 */

#define	CPUidtoBWcsrid(cid, dest)		\
	or	cid, CSR_CPU_PREFIX, dest;	\
	sll	dest, CPU_2_CSR_SHIFT, dest;

/*
 * CPU id To ECSR address Macros
 */

#define	CPUidtoECSR(cid, dest)	sll cid, CPU_2_ECSR_SHIFT, dest

/* get bw control register given cpu id and bus id */
#if defined(lint)

/* ARGSUSED */
int
intr_bw_cntrl_get(int cpuid, int whichbus)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bw_cntrl_get)
	sll	%o1, BUS_SHIFT, %o1	! shift to bus id bit (bit 8)
	CPUidtoBWcsrid(%o0, %o0)	! get CSR header from cpu id
	or	%o0, %o1, %o0		! BW control register
	set	BW_CONTROL, %o1
	or	%o0, %o1, %o0		! BW control register
	retl
	lda	[%o0]ASI_BW, %o0	! delay slot
	SET_SIZE(intr_bw_cntrl_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_bw_cntrl_set(int cpuid, int busid, int value)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bw_cntrl_set)
	sll	%o1, BUS_SHIFT, %o1	! shift to bus id bit (bit 8)
	CPUidtoBWcsrid(%o0, %o0)	! get CSR header from cpu id
	or	%o0, %o1, %o0
	set	BW_CONTROL, %o1		! BW control register
	or	%o0, %o1, %o0
	retl
	sta	%o2, [%o0]ASI_BW	! delay slot
	SET_SIZE(intr_bw_cntrl_set)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
bw_compid_get(int cpuid, int busid)
{ return (0); }

#else	/* lint */

	ENTRY_NP(bw_compid_get)
	sll	%o1, BUS_SHIFT, %o1	! shift to bus id bit (bit 8)
	CPUidtoBWcsrid(%o0, %o0)	! get CSR header from cpu id
	or	%o0, %o1, %o0
	set	BW_COMPID, %o1		! BW profile control register
	or	%o0, %o1, %o0
	retl
	lda	[%o0]ASI_BW, %o0	! delay slot
	SET_SIZE(bw_compid_get)

#endif	/* lint */

#define	BW_PRESCALER	0x10c0

#if defined(lint)

int
intr_prescaler_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prescaler_get)
	set	BW_LOCAL_BASE+BW_PRESCALER, %o0
	retl
	lduha	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_prescaler_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_prescaler_set(int value)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prescaler_set)
	set	BW_LOCAL_BASE+BW_PRESCALER, %o1
	retl
	stha	%o0, [%o1]ASI_BW		! delay slot
	SET_SIZE(intr_prescaler_set)

#endif	/* lint */


#if defined(lint)

int
intr_prof_getlimit_local(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prof_getlimit_local)
	set	BW_LOCAL_BASE+PROF_LIMIT, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_prof_getlimit_local)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_prof_getlimit(int cpuid, int whichbus)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prof_getlimit)
	sll	%o1, BUS_SHIFT, %o1
	CPUidtoBWcsrid(%o0, %o0)
	or	%o0, %o1, %o0
	set	PROF_LIMIT, %o1
	or	%o0, %o1, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_prof_getlimit)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_prof_setlimit(int cpuid, int whichbus, int value)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prof_setlimit)
	sll	%o1, BUS_SHIFT, %o1
	CPUidtoBWcsrid(%o0, %o0)
	or	%o0, %o1, %o0
	set	PROF_LIMIT, %o1
	or	%o0, %o1, %o0
	retl
	sta	%o2, [%o0]ASI_BW		! delay slot
	SET_SIZE(intr_prof_setlimit)

#endif	/* lint */

#if defined(lint)

int
intr_prof_getndlimit(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prof_getndlimit)
	set	BW_LOCAL_BASE+PROF_NDLIMIT, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_prof_getndlimit)

#endif	/* lint */

#define	BWB_LOCALBASE	(BW_LOCAL_BASE + (1 << 8))

#if defined(lint)

int
intr_bwb_cntrl_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bwb_cntrl_get)
	set	BWB_LOCALBASE+BW_CONTROL, %o0	/* temporary - xdbus #1 */
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_bwb_cntrl_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_bwb_cntrl_set(int value)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bwb_cntrl_set)
	set	BWB_LOCALBASE+BW_CONTROL, %o1	/* temporary - xdbus #1 */
	retl
	sta	%o0, [%o1]ASI_BW		! delay slot
	SET_SIZE(intr_bwb_cntrl_set)

#endif	/* lint */

#if defined(lint)

longlong_t
intr_usertimer_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_usertimer_get)
	set	BWB_LOCALBASE+PROF_LIMIT, %o0	/* temporary - xdbus #1 */
	retl
	ldda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_usertimer_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_usertimer_set(longlong_t count)
{}

#else	/* lint */

	ENTRY_NP(intr_usertimer_set)
	set	BWB_LOCALBASE+PROF_LIMIT, %o2	/* temporary - xdbus #1 */
	retl
	stda	%o0, [%o2]ASI_BW		! delay slot
	SET_SIZE(intr_usertimer_set)

#endif	/* lint */

#if defined(lint)

u_int
intr_usercntrl_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_usercntrl_get)
	set	BWB_LOCALBASE+PROF_CONTROL, %o0	/* temporary - xdbus #1 */
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_usercntrl_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_usercntrl_set(int cntrl)
{}

#else	/* lint */

	ENTRY_NP(intr_usercntrl_set)
	set	BWB_LOCALBASE+PROF_CONTROL, %o1	/* temporary - xdbus #1 */
	retl
	sta	%o0, [%o1]ASI_BW		! delay slot
	SET_SIZE(intr_usercntrl_set)

#endif	/* lint */

#define	TICK_LIMIT	0x3000
#define	TICK_COUNTER	0x3010
#define	TICK_NDLIMIT	0x3008

#if defined(lint)

/* ARGSUSED */
int
intr_prof_addr(int cpuid)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_prof_addr)
	CPUidtoBWcsrid(%o0, %o0)
	set PROF_COUNTER, %o1
	retl
	or %o0, %o1, %o0
	SET_SIZE(intr_prof_addr)

#endif	/* lint */

#if defined(lint)

int
intr_tick_getlimit_local(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_tick_getlimit_local)
	set	BW_LOCAL_BASE+TICK_LIMIT, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_tick_getlimit_local)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_tick_setlimit_local(int value)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_tick_setlimit_local)
	set	BW_LOCAL_BASE+TICK_LIMIT, %o1
	retl
	sta	%o0, [%o1]ASI_BW		! delay slot
	SET_SIZE(intr_tick_setlimit_local)

#endif	/* lint */

#if defined(lint)

int
intr_tick_getndlimit_local(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_tick_getndlimit_local)
	set	BW_LOCAL_BASE+TICK_NDLIMIT, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_tick_getndlimit_local)

#endif	/* lint */

/*
 * BW Interrupt Table routines
 * Set of SBusses requesting service at this level.
 */
#define	INTR_TABLE	0x1040
#define	INTR_TBL_CLEAR	0x1080

#if defined(lint)

/* ARGSUSED */
int
intr_clear_table(int sbus_level, int mask)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_clear_table)
	set	BW_LOCAL_BASE+INTR_TBL_CLEAR, %o2	! %o2 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o2, %o0, %o0			! add/or either will do
	retl
	stha	%o1, [%o0]ASI_BW		! delay slot
	SET_SIZE(intr_clear_table)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_get_table(int sbus_level)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_get_table)
	set	BW_LOCAL_BASE+INTR_TABLE, %o1	! %o1 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o1, %o0, %o0			! add/or either will do
	retl
	lduha	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_get_table)

#endif	/* lint */

/*
 * Interrupt Table routines for Bus Watcher on XDBus 1
 * Only MHQ's on XDBus register interrupts here
 */

#if defined(lint)

/* ARGSUSED */
int
intr_clear_table_bwb(int sbus_level, int mask)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_clear_table_bwb)
	set	BWB_LOCALBASE+INTR_TBL_CLEAR, %o2	! %o2 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o2, %o0, %o0			! add/or either will do
	retl
	stha	%o1, [%o0]ASI_BW		! delay slot
	SET_SIZE(intr_clear_table_bwb)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_get_table_bwb(int sbus_level)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_get_table_bwb)
	set	BWB_LOCALBASE+INTR_TABLE, %o1	! %o1 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o1, %o0, %o0			! add/or either will do
	retl
	lduha	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(intr_get_table_bwb)

#endif	/* lint */

/*
 * CC Interrupt Mask routines
 */
#define	ASI_CC		0x02
#define	CC_BASE		OFF_CC_REGS
#define	INTR_MASK	OFF_CC_INTR_MASK

#if defined(lint)

/* ARGSUSED */
void
intr_set_mask_bits(int mask)
{}

#else	/* lint */

	/* locore assumes we use only %o0, %o1, %o2 */
	ENTRY_NP(intr_set_mask_bits)
	set	CC_BASE+INTR_MASK, %o1		! %o1 = temporary
	lduha	[%o1]ASI_CC, %o2		! previous IMR value
	or	%o0, %o2, %o0
	retl
	stha	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(intr_set_mask_bits)

#endif	/* lint */
#if defined(lint)

/* ARGSUSED */
void
intr_clear_mask_bits(int mask)
{}

#else	/* lint */

	/* locore assumes we use only %o0, %o1, %o2 */
	ENTRY_NP(intr_clear_mask_bits)
	set	CC_BASE+INTR_MASK, %o1		! %o1 = temporary
	not	%o0
	lduha	[%o1]ASI_CC, %o2		! previous IMR value
	and	%o0, %o2, %o0
	retl
	stha	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(intr_clear_mask_bits)

#endif	/* lint */

/*
 * CC Interrupt Pending/Clear routines
 */
#define	INTR_PEND	0x406	/* card16 - OFF_CC_INTR_PENDING */
#define	INTR_CLEAR	0x606	/* card16 - OFF_CC_INTR_PENDING_CLEAR */

#if defined(lint)

/* ARGSUSED */
int
intr_clear_pend_local(int level)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_clear_pend_local)
	set	1, %o1				! %o1 = temporary
	sll	%o1, %o0, %o0			! set bit[level]
	set	CC_BASE+INTR_CLEAR, %o1		! %o1 = temporary
	retl
	stha	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(intr_clear_pend_local)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_clear_pend(int cpuid, int level)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_clear_pend)
	CPUidtoECSR(%o0, %o0)
	set	CC_BASE+INTR_CLEAR, %o2		! %o2 = temporary
	or	%o2, %o0, %o0
	mov	1, %o2
	sll %o2, %o1, %o2
	retl
	stha	%o2, [%o0]ASI_ECSR		! delay slot

	SET_SIZE(intr_clear_pend)

#endif	/* lint */

#if defined(lint)

int
intr_get_pend_local(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_get_pend_local)
	set	CC_BASE+INTR_PEND, %o0		! %o0 = temporary
	retl
	lduha	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(intr_get_pend_local)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
intr_get_pend(int cpuid)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_get_pend)
	CPUidtoECSR(%o0, %o0)
	set	CC_BASE+INTR_PEND, %o1		! %o1 = temporary
	or	%o1, %o0, %o1
	retl
	lduha	[%o1]ASI_ECSR, %o0		! delay slot

	SET_SIZE(intr_get_pend)

#endif	/* lint */

/*
 * Interprocessor interrupts
 */
#define	INTR_GEN	0x704	/* card32 - OFF_CC_INTR_GEN_ASI2 */

#if defined(lint)

/* ARGSUSED */
void
set_all_itr_by_cpuid(u_int cpuid)
{}

#else	/* lint */

	ENTRY_NP(set_all_itr_by_cpuid)

	sll	%o0, 3, %o0
	set	(0xff << 23), %o1
	or	%o0, %o1, %o0
	set	CC_BASE+INTR_GEN, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(set_all_itr_by_cpuid)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_igr_set(u_int value)
{}

#else	/* lint */

	ENTRY_NP(intr_igr_set)

	set	CC_BASE+INTR_GEN, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(intr_igr_set)

#endif	/* lint */

#if defined(lint)

u_int
intr_mxcc_cntrl_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_mxcc_cntrl_get)
	set	CC_BASE+OFF_CC_CONTROL, %o0
	retl
	lda	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(intr_mxcc_cntrl_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_mxcc_cntrl_set(u_int value)
{}

#else	/* lint */

	ENTRY_NP(intr_mxcc_cntrl_set)
	set	CC_BASE+OFF_CC_CONTROL, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(intr_mxcc_cntrl_set)

#endif	/* lint */

/*
 * Cache Controller Error routines
 */
#define	CC_ERROR	0xe00	/* card64 - OFF_CC_ERROR */

#if defined(lint)

u_longlong_t
intr_mxcc_error_get(void)
{ return (0); }

/* ARGSUSED */
void
intr_mxcc_error_set(u_longlong_t v)
{}

#else	/* lint */

	ENTRY_NP(intr_mxcc_error_get)
	set	CC_BASE+CC_ERROR, %o0
	retl
	ldda	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(intr_mxcc_error_get)

	ENTRY_NP(intr_mxcc_error_set)
	set	CC_BASE+CC_ERROR, %o2
	retl
	stda	%o0, [%o2]ASI_CC			! delay slot
	SET_SIZE(intr_mxcc_error_set)

#endif	/* lint */

/*
 * SuperSPARC MMU Control
 */
#define	ASI_VIK		0x04
#define	VIK_MCNTL	(0x00 << 8)

#if defined(lint)

u_int
intr_vik_mcntl_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_vik_mcntl_get)
	set	VIK_MCNTL, %o0
	retl
	lda	[%o0]ASI_VIK, %o0		! delay slot
	SET_SIZE(intr_vik_mcntl_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_vik_mcntl_set(u_int value)
{}

#else	/* lint */

	ENTRY_NP(intr_vik_mcntl_set)
	set	VIK_MCNTL, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_VIK		! delay slot
	SET_SIZE(intr_vik_mcntl_set)

#endif	/* lint */

/*
 * SuperSPARC Breakpoint ACTION
 */
#define	ASI_BKPT	0x4c
#define	VIK_ACTION	(0x00 << 8)

#if defined(lint)

u_int
intr_vik_action_get(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_vik_action_get)
	set	VIK_ACTION, %o0
	retl
	lda	[%o0]ASI_BKPT, %o0		! delay slot
	SET_SIZE(intr_vik_action_get)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_vik_action_set(u_int value)
{}

#else	/* lint */

	ENTRY_NP(intr_vik_action_set)
	set	VIK_ACTION, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_BKPT		! delay slot
	SET_SIZE(intr_vik_action_set)

#endif	/* lint */

/*
 * BootBus routines
 */
#define	ASI_BB		ASI_ECSR
#define	BB_LOCALBASE	0xf0000000
#define	XOFF_SLOWBUS_LED	0x2e

#if defined(lint)

/* ARGSUSED */
int
led_get_ecsr(int cpu_id)
{ return (0); }

#else	/* lint */
	ENTRY_NP(led_get_ecsr)
	sll	%o0, 27, %o0
	set	(XOFF_SLOWBUS_LED << 16), %o2
	add	%o0, %o2, %o0
	retl
	lduba	[%o0]ASI_ECSR, %o0		! delay slot
	SET_SIZE(led_get_ecsr)
#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
led_set_ecsr(int cpu_id, u_char pattern)
{}

#else	/* lint */
	ENTRY_NP(led_set_ecsr)
	sll	%o0, 27, %o0
	set	(XOFF_SLOWBUS_LED << 16), %o2
	add	%o0, %o2, %o0
	retl
	stba	%o1, [%o0]ASI_ECSR		! delay slot
	SET_SIZE(led_set_ecsr)
#endif	/* lint */

#define	XOFF_FASTBUS_SEMA	0x1a
#define	SEMA_0_STATUS		0x04
#define	STATUS_1		0x100000

#if defined(lint)

int
intr_bb_sema_status(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bb_sema_status)
	set	BB_LOCALBASE+(XOFF_FASTBUS_SEMA<< 16)+SEMA_0_STATUS, %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(intr_bb_sema_status)

#endif	/* lint */

#if defined(lint)

int
intr_bb_status1(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_bb_status1)
	set	BB_LOCALBASE+STATUS_1, %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(intr_bb_status1)

#endif	/* lint */

/*
 * JTAG low-level functions.
 * These routines assume that the CPU which BW's are being used to get
 * to the bootbus is the one with the SlowBus semaphore. Reading these
 * registers without the SlowBus semaphore gives unknown results. Writing
 * these registers without the SlowBus semaphore has no effect.
 */

#define	XOFF_SLOWBUS_JTAG	0x30
#define	JTAG_CMD		0x0
#define	JTAG_CTL		0x4

#if defined(lint)

/* ARGSUSED */
unsigned int
jtag_ctl_get_ecsr(int cpu_id)
{ return (0); }

#else   /* lint */

	ENTRY_NP(jtag_ctl_get_ecsr)
	sll	%o0, 27, %o0
	set	(XOFF_SLOWBUS_JTAG<< 16)+JTAG_CTL, %o2
	add	%o0, %o2, %o0
	retl
	lduba	[%o0]ASI_ECSR, %o0
	SET_SIZE(jtag_ctl_get_ecsr)

#endif  /* lint */

#if defined(lint)

/* ARGSUSED */
int
jtag_cmd_get_ecsr(unsigned int jtag_csr)
{ return (0); }

#else   /* lint */

	ENTRY_NP(jtag_cmd_get_ecsr)
	retl
	lda	[%o0]ASI_ECSR, %o0
	SET_SIZE(jtag_cmd_get_ecsr)

#endif  /* lint */

#if defined(lint)

/* ARGSUSED */
void
jtag_cmd_set_ecsr(unsigned int jtag_csr, int command)
{}

#else   /* lint */

	ENTRY_NP(jtag_cmd_set_ecsr)
	retl
	sta	%o1, [%o0]ASI_ECSR
	SET_SIZE(jtag_cmd_set_ecsr)

#endif  /* lint */

/*
 * Service SBusses which have interrupt requests pending
 */
#define	ECSR_SHIFT	24
#define	SBI_BASE	0x02800000	/* ECSR (0x[0-d]2) */

#if defined(lint)

/*
 * WARNING - This function depends upon POST's definition of device_id's.
 *	Perhaps we should have OBP/autoconf define this?
 *	We can't do something simple - we need swap & want performance
 */

/* ARGSUSED */
int
intr_sbusses_devicebase(table_bit_number)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_sbusses_devicebase)
	sll	%o0, 28, %o0		! allow for unit#
	sethi	%hi(SBI_BASE), %o1	! need to or-in device_id
	retl				!
	or	%o0, %o1, %o0		! delay slot
	SET_SIZE(intr_sbusses_devicebase)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_sbusses_scan(mask)
{}

#else	/* lint */

	ENTRY_NP(intr_sbusses_scan)
	! %l5 - temporary, about to get smashed by SBINTR
	mov	%o7, %l5		! leaf routine (almost)
	mov	%o0, %o1		! we smash this device list
!	mov	0xf, %o0		! take mask - all slots,
!	sll	%o0, (LEVEL << 2), %o0	! for this level only.
!	call	intr_sbi_take		! atomicly take responsibility
!	clr	%o1			! delay slot - device
	call	intr_sbi_release	! squelch all levels, slots
	clr	%o1			! delay slot - device
	mov	%l5, %o7		! leaf routine (almost)
	retl				!
	nop				! delay slot
	SET_SIZE(intr_sbusses_scan)

#endif	/* lint */

/*
 * SBI Interrupt State routines
 */
#define	ASI_SBI ASI_ECSR
#define	INTR_STATE	0x30

#if defined(lint)

/* ARGSUSED */
int
intr_sbi_take(mask, device_id)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_sbi_take)
	set	SBI_BASE+INTR_STATE, %o2
	sll	%o1, DEV_2_ECSR_SHIFT, %o1		! device_id qualifier
	or	%o1, %o2, %o1			! ECSR address
	retl
	swapa	[%o1]ASI_SBI, %o0		! delay slot
	SET_SIZE(intr_sbi_take)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_sbi_release(mask, device_id)
{}

#else	/* lint */

	ENTRY_NP(intr_sbi_release)
	set	SBI_BASE+INTR_STATE, %o2
	sll	%o1, DEV_2_ECSR_SHIFT, %o1		! device_id qualifier
	or	%o1, %o2, %o1			! ECSR address
	retl
	sta	%o0, [%o1]ASI_SBI		! delay slot
	SET_SIZE(intr_sbi_release)

#endif	/* lint */

/*
 * SBI Interrupt Target routines
 */
#define	INTR_TARGET	0x34

#if defined(lint)

/* ARGSUSED */
void
intr_set_target(device_id, target_id)
{}

#else	/* lint */

	ENTRY_NP(intr_set_target)
	set	SBI_BASE+INTR_TARGET, %o2	! %o2 = temporary
	sll	%o0, DEV_2_ECSR_SHIFT, %o0		! device_id qualifier
	or	%o0, %o2, %o0			! ECSR address
	retl
	sta	%o1, [%o0]ASI_SBI		! delay slot
	SET_SIZE(intr_set_target)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
intr_backdoor_set(u_int *addr, u_int value)
{}

#else	/* lint */

	ENTRY_NP(intr_backdoor_set)
	retl
	sta	%o1, [%o0]0x2f
	SET_SIZE(intr_backdoor_set)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
u_int
intr_backdoor_get(u_int *addr)
{ return (0); }

#else	/* lint */

	ENTRY_NP(intr_backdoor_get)
	retl
	lda	[%o0]0x2f, %o0
	SET_SIZE(intr_backdoor_get)

#endif	/* lint */
