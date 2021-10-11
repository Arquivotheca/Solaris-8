/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ident	"@(#)sun4d.il.cpp	1.35	97/05/24 SMI"

#ifndef	INLINE

#include <sys/asm_linkage.h>

#else	INLINE

#define	ENTRY(x)	.inline	x, 0
#define	retl		/* nop */
#define	SET_SIZE(x)	.end

/*
 * An apparent bug in cc arranges for only the last of multiple
 * inlines to be applied to the file being compiled.  All
 * C files use sparc.il and some use this inline too.  This
 * inline is later on the command line so sparc.il is ignored.
 * We workaround this by including sparc.il in this inline.
 */
#include <ml/sparc.il>
#endif	INLINE

#if defined(lint)
u_char
lduba_2f(u_char *addr) { return (*addr); }

u_short
lduha_2f(u_short *addr) { return (*addr); }

u_int
lda_2f(u_int *addr) { return (*addr); }

u_longlong_t
ldda_2f(u_longlong_t *src) { return (*src); }

void
stba_2f(u_char value, u_char *addr) { *addr = value; return; }

void
stha_2f(u_short value, u_short *addr) { *addr = value; return; }

void
sta_2f(u_int value, u_int *addr) { *addr = value; return; }

void
stda_2f(u_longlong_t value, u_longlong_t *addr) { *addr = value; return; }

u_int
swapa_2f(u_int value, u_int offset) {
	u_int tmp = *(caddr_t)offset;
	*(caddr_t)offset = value;
	return (tmp);
}

u_longlong_t
ldda_02(u_longlong_t *src) { return (0); }

void
sta_28(u_int value, u_int *addr) { *addr = value; return; }

void
sta_29(u_int value, u_int *addr) { *addr = value; return; }

void
sta_2a(u_int value, u_int *addr) { *addr = value; return; }

void
sta_2b(u_int value, u_int *addr) { *addr = value; return; }

#else	/* lint */

	/*
	 * prototypes for these ASI inlines are in physaddr.h
	 */

	ENTRY(lduba_2f)
	retl
	lduba	[%o0]0x2f, %o0
	SET_SIZE(lduba_2f)

	ENTRY(lduha_2f)
	retl
	lduha	[%o0]0x2f, %o0
	SET_SIZE(lduha_2f)

	ENTRY(lda_2f)
	retl
	lda	[%o0]0x2f, %o0
	SET_SIZE(lda_2f)

	ENTRY(ldda_2f)
	retl
	ldda	[%o0]0x2f, %o0
	SET_SIZE(ldda_2f)

	ENTRY(stba_2f)
	retl
	stba	%o0, [%o1]0x2f
	SET_SIZE(stba_2f)

	ENTRY(stha_2f)
	retl
	stha	%o0, [%o1]0x2f
	SET_SIZE(stha_2f)

	ENTRY(sta_2f)
	retl
	sta	%o0, [%o1]0x2f
	SET_SIZE(sta_2f)

	ENTRY(stda_2f)
	retl
	stda	%o0, [%o2]0x2f
	SET_SIZE(stda_2f)

	ENTRY(swapa_2f)
	retl
	swapa	[%o1]0x2f,  %o0
	SET_SIZE(swapa_2f)

	ENTRY(ldda_02)
	retl
	ldda	[%o0]0x2, %o0		/* delay slot */
	SET_SIZE(ldda_02)

	ENTRY(sta_28)
	retl
	sta	%o0, [%o1]0x28		/* delay slot */
	SET_SIZE(sta_28)

	ENTRY(sta_29)
	retl
	sta	%o0, [%o1]0x29		/* delay slot */
	SET_SIZE(sta_29)

	ENTRY(sta_2a)
	retl
	sta	%o0, [%o1]0x2a		/* delay slot */
	SET_SIZE(sta_2a)

	ENTRY(sta_2b)
	retl
	sta	%o0, [%o1]0x2b		/* delay slot */
	SET_SIZE(sta_2b)

#endif	/* lint */

#ifdef	SAS
/*
 * SAS routines
 */
#define	ASI_SAS		0x50
#define	MMU_PGSHIFT	12

/*
 * int
 * sas_mem_pages()
 */
#if defined(lint)

void
sas_mem_pages(void)
{}

#else	/* lint */

	ENTRY(sas_mem_pages)
	lda	[%g0]ASI_SAS, %o0		! mpsas "magic" asi
	retl
	srl	%o0, MMU_PGSHIFT, %o0		! delay slot
	SET_SIZE(sas_mem_pages)
#endif	/* lint */
#endif	SAS


#if	0
/*
 * A bug in the optimizer causes it to generate code which does not
 * conform to the SPARC V8 specification for wr %psr operations,
 * therefore we cannot inline these operations
 */

/*
 * SPARC Trap Enable/Disable
 */
#define	PSR_PIL_BIT	5		/* ref - assym.h */
#define	PSR_DISABLE	0xA6		/* ref - locore.s */

#if defined(lint)
u_int		disable_traps(void) { return (0); }
void		enable_traps(u_int psr_value) { }
#else	/* lint */
	ENTRY(enable_traps)
	.volatile
	wr	%o0, %psr		/* delay slot */
	nop
	nop
	retl
	nop
	.nonvolatile
	SET_SIZE(enable_traps)

	ENTRY(disable_traps)
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
#endif	/* 0 */

/*
 * SRMMU/SuperSPARC Unit
 */
#define	ASI_ICACHE	0x36
#define	ICACHE_CLEAR	(1 << 31)
#define	ASI_DCACHE	0x37
#define	CACHE_CLEAR	(1 << 31)

/*
 * void
 * vik_icache_clear()
 */
#if defined(lint)

void
vik_icache_clear(void)
{}

#else	/* lint */

	ENTRY(vik_icache_clear)
	set	CACHE_CLEAR, %o0
	sta	%g0, [%o0]ASI_ICACHE		! clear lock bits
	retl
	sta	%g0, [%g0]ASI_ICACHE		! delay slot, invalidate entries
	SET_SIZE(vik_icache_clear)
#endif	/* lint */

/*
 * void
 * vik_dcache_clear()
 */
#if defined(lint)

void
vik_dcache_clear(void)
{}

#else	/* lint */

	ENTRY(vik_dcache_clear)
	set	CACHE_CLEAR, %o0
	sta	%g0, [%o0]ASI_DCACHE		! clear lock bits
	retl
	sta	%g0, [%g0]ASI_DCACHE		! delay slot, invalidate entries
	SET_SIZE(vik_dcache_clear)
#endif	/* lint */


/*
 * SHIFT(s) - distance to shift device_id's to create qualifier, pg. 21
 */
#define	CSR_SHIFT	20
#define	ECSR_SHIFT	24


/*
 * Processor Unit
 */
#define	ASI_BB		0x2f
#define	BB_BASE		0xf0000000
#define	ASI_BW		0x2f
#define	BW_BASE		0xe0000000
#define	BW_LOCALBASE	0xfff00000
#define	ASI_CC		0x02
#define	CC_BASE		0x01f00000

/*
 * CC E$ routines
 */

/*
 * void
 * xdb_ecache_tag_set(value, line)
 *	ecache_tag_t value;
 *	int line;
 */
#if defined(lint)

void
xdb_ecache_tag_set(ecache_tag_t value, int line)
{}

#else	/* lint */

	ENTRY(xdb_ecache_tag_set)
	!set	(0xC << 21), %o2
	retl
	stda	%o0, [%o2]ASI_CC		! delay slot
	SET_SIZE(xdb_ecache_tag_set)
#endif	/* lint */

/*
 * CC Block Copy routines
 */
#define	CC_SDR	0x00000000
#define	CC_SSAR	0x00000100
#define	CC_SDAR	0x00000200

#if defined(lint)
longlong_t	xdb_cc_ssar_get(void) { return (0); }
void		xdb_cc_ssar_set(longlong_t src) { }
longlong_t	xdb_cc_sdar_get(void) { return (0); }
void		xdb_cc_sdar_set(longlong_t src) { }
#else	/* lint */

	ENTRY(xdb_cc_ssar_get)
	set	CC_BASE+CC_SSAR, %o0
	retl
	ldda	[%o0]ASI_CC, %o0	/* delay slot */
	SET_SIZE(xdb_cc_ssar_get)

	ENTRY(xdb_cc_ssar_set)
	set	CC_BASE+CC_SSAR, %o2
	retl
	stda	%o0, [%o2]ASI_CC	/* delay slot */
	SET_SIZE(xdb_cc_ssar_set)

	ENTRY(xdb_cc_sdar_get)
	set	CC_BASE+CC_SDAR, %o0
	retl
	ldda	[%o0]ASI_CC, %o0	/* delay slot */
	SET_SIZE(xdb_cc_sdar_get)

	ENTRY(xdb_cc_sdar_set)
	set	CC_BASE+CC_SDAR, %o2
	retl
	stda	%o0, [%o2]ASI_CC	/* delay slot */
	SET_SIZE(xdb_cc_sdar_set)
#endif	/* lint */

/*
 * CC Ref/Miss routines
 */
#define	REF_MISS	0x300

/*
 * refmiss_t
 * xdb_cc_refmiss_get()
 */
#if defined(lint)

refmiss_t
xdb_cc_refmiss_get(void)
{}

#else	/* lint */

	ENTRY(xdb_cc_refmiss_get)
	set	CC_BASE+REF_MISS, %o0
	retl
	ldda	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(xdb_cc_refmiss_get)
#endif	/* lint */

/*
 * void
 * xdb_cc_refmiss_set(value)
 *	refmiss_t value;
 */
#if defined(lint)

void
xdb_cc_refmiss_set(refmiss_t value)
{}

#else	/* lint */

	ENTRY(xdb_cc_refmiss_set)
	set	CC_BASE+REF_MISS, %o2
	retl
	stda	%o0, [%o2]ASI_CC		! delay slot
	SET_SIZE(xdb_cc_refmiss_set)
#endif	/* lint */

/*
 * CC Interrupt Pending routines
 */
#define	CC_IPR	0x00000406

#if defined(lint)
u_short		xdb_cc_ipr_get(void) { return (0); }
#else	/* lint */

	ENTRY(xdb_cc_ipr_get)
	set	CC_BASE+CC_IPR, %o0
	retl
	lduha	[%o0]ASI_CC, %o0;
	SET_SIZE(xdb_cc_ipr_get)
#endif	/* lint */

/*
 * CC Interrupt Mask routines
 */
#define	INTR_MASK	0x506	/* card16 - OFF_CC_INTR_MASK */

/*
 * void
 * xdb_imr_set(mask)
 *	int mask;
 */
#if defined(lint)

void
xdb_imr_set(int mask)
{}

#else	/* lint */

	ENTRY(xdb_imr_set)
	set	CC_BASE+INTR_MASK, %o1		! %o1 = temporary
	retl
	stha	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(xdb_imr_set)
#endif	/* lint */

/*
 * int
 * xdb_imr_get()
 */
#if defined(lint)

void
xdb_imr_get(void)
{}

#else	/* lint */

	ENTRY(xdb_imr_get)
	set	CC_BASE+INTR_MASK, %o0
	retl
	lduha	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(xdb_imr_get)
#endif	/* lint */

/*
 * CC Interrupt Pending/Clear routines
 */
#define	INTR_PEND	0x406	/* card16 - OFF_CC_INTR_PENDING */
#define	INTR_CLEAR	0x606	/* card16 - OFF_CC_INTR_PENDING_CLEAR */

/*
 * int
 * xdb_ipr_clear(level)
 *	int level;
 */
#if defined(lint)

int
xdb_ipr_clear(int level)
{}

#else	/* lint */

	ENTRY(xdb_ipr_clear)
	set	1, %o1				! %o1 = temporary
	sll	%o1, %o0, %o0			! set bit[level]
	set	CC_BASE+INTR_CLEAR, %o1		! %o1 = temporary
	retl
	stha	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(xdb_ipr_clear)
#endif	/* lint */

/*
 * int
 * xdb_ipr_get()
 */
#if defined(lint)

int
xdb_ipr_get(void)
{}

#else	/* lint */

	ENTRY(xdb_ipr_get)
	set	CC_BASE+INTR_PEND, %o0		! %o0 = temporary
	retl
	lduha	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(xdb_ipr_get)
#endif	/* lint */

/*
 * Interprocessor interrupts
 */
#define	INTR_GEN	0x704	/* card32 - OFF_CC_INTR_GEN_ASI2 */

/*
 * non-ambiguous (cpu) interrupt source id's are:
 * [6..15], [32*sbus+14..15]
 */
#define	INTR_MESSAGE(broadcast, target, intsid, levels)	\
	(((broadcast	& 0x0001) << 31) +		\
	((target	& 0x00ff) << 23) +		\
	((intsid	& 0x00ff) << 15) +		\
	(levels		& 0x7fff))


#define	INTR_MYSELF(level)	\
	INTR_MESSAGE(0, 0, 6, (1 << (level-1)))

/*
 * void
 * xdb_igr_set(level, assert)
 *	int level;
 *	int assert;
 * asserts corresponding intsid entry
 */
#if defined(lint)

void
xdb_igr_set(int level, int assert)
{}

#else	/* lint */

	ENTRY(xdb_igr_set)
	set	INTR_MYSELF(1), %o0		! level-1 interrupt
	set	CC_BASE+INTR_GEN, %o1		! %o1 = temporary
	retl
	sta	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(xdb_igr_set)
#endif	/* lint */

/*
 * CC Control routines
 */
#define	ECACHE_CNTRL	0xa04

/*
 * void
 * xdb_cc_cntrl_set(value)
 *	int value;
 */
#if defined(lint)

void
xdb_cc_cntrl_set(int value)
{}

#else	/* lint */

	ENTRY(xdb_cc_cntrl_set)
	set	CC_BASE+ECACHE_CNTRL, %o1
	retl
	sta	%o0, [%o1]ASI_CC		! delay slot
	SET_SIZE(xdb_cc_cntrl_set)
#endif	/* lint */

/*
 * Cache Controller Error routines
 */
#define	CC_ERROR	0xe00	/* card64 - OFF_CC_ERROR */

/*
 * cc_error_t
 * xdb_cc_error_get()
 */
#if defined(lint)

cc_error_t
xdb_cc_error_get(void)
{}

#else	/* lint */

	ENTRY(xdb_cc_error_get)
	set	CC_BASE+CC_ERROR, %o0
	retl
	ldda	[%o0]ASI_CC, %o0		! delay slot
	SET_SIZE(xdb_cc_error_get)
#endif	/* lint */


/*
 * BW "Snoopy" Tag routines
 */
#define	BW_TAGS_BASE	(0xFF << 24)

/*
 * void
 * xdb_bw_tag_set(value, line)
 *	bw_tag_t value;
 *	int line;
 */
#if defined(lint)

void
xdb_bw_tag_set(bw_tag_t value, int line)
{}

#else	/* lint */

	ENTRY(xdb_bw_tag_set)
	set	BW_TAGS_BASE, %o3
	or	%o2, %o3, %o3
	retl
	stda	%o0, [%o3]ASI_BW		! delay slot
	SET_SIZE(xdb_bw_tag_set)
#endif	/* lint */

/*
 * BW DynaData routines
 */
#define	BW_DYNADATA	0x0010

/*
 * bw_dynadata_t
 * xdb_bw_dynadata_get(device_id)
 *	int device_id;
 */
#if defined(lint)

bw_dynadata_t
xdb_bw_dynadata_get(int device_id)
{}

#else	/* lint */

	ENTRY(xdb_bw_dynadata_get)
	set	BW_BASE+BW_DYNADATA, %o2	! %o2 = temporary
	or	%o0, %o2, %o2			! device_id qualifier
	ldda	[%o2]ASI_BW, %o0		! xdbus #0
	add	%o2, 0x100, %o2			! xdbus #1
	retl
	ldda	[%o2]ASI_BW, %o2		! delay slot
	SET_SIZE(xdb_bw_dynadata_get)
#endif	/* lint */

/*
 * void
 * xdb_bw_dynadata_set(value, device_id)
 *	bw_dynadata_t value;
 *	int device_id;
 */
#if defined(lint)

void
xdb_bw_dynadata_set(bw_dynadata_t value, int device_id)
{}

#else	/* lint */

	ENTRY(xdb_bw_dynadata_set)
	set	BW_BASE+BW_DYNADATA, %o3	! %o3 = temporary
	or	%o2, %o3, %o2			! device_id qualifier
	stda	%o0, [%o2]ASI_BW		! xdbus #0
	add	%o2, 0x100, %o2			! xdbus #1
	retl
	stda	%o0, [%o2]ASI_BW		! delay slot
	SET_SIZE(xdb_bw_dynadata_set)
#endif	/* lint */

/*
 * BW Interrupt Table routines
 * Set of SBusses requesting service at this level.
 */
#define	INTR_TABLE	0x1040
#define	INTR_TBL_CLEAR	0x1080

/*
 * int
 * xdb_intsid_clear(sbus_level, mask)
 *	int sbus_level;
 *	int mask;
 */
#if defined(lint)

int
xdb_intsid_clear(int sbus_level, int mask)
{}

#else	/* lint */

	ENTRY(xdb_intsid_clear)
	set	BW_BASE+INTR_TBL_CLEAR, %o2	! %o2 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o2, %o0, %o0			! add/or either will do
	retl
	stha	%o1, [%o0]ASI_BW		! delay slot
	SET_SIZE(xdb_intsid_clear)
#endif	/* lint */

/*
 * int
 * xdb_intsid_get(sbus_level)
 *	int sbus_level;
 */
#if defined(lint)

int
xdb_intsid_get(int sbus_level)
{}

#else	/* lint */

	ENTRY(xdb_intsid_get)
	set	BW_BASE+INTR_TABLE, %o1		! %o1 = temporary
	sll	%o0, 3, %o0			! table[sbus_level].low
	or	%o1, %o0, %o0			! add/or either will do
	retl
	lduha	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(xdb_intsid_get)
#endif	/* lint */

/*
 * BW Timer routines
 */
#define	TICK_LIMIT	0x3000

/*
 * int
 * xdb_tick_get_limit()
 */
#if defined(lint)

int
xdb_tick_get_limit(void)
{}

#else	/* lint */

	ENTRY(xdb_tick_get_limit)
	set	BW_BASE+TICK_LIMIT, %o0
	retl
	lda	[%o0]ASI_BW, %o0		! delay slot
	SET_SIZE(xdb_tick_get_limit)
#endif	/* lint */

/*
 * int
 * xdb_tick_set_limit(value)
 *	int value;
 */
#if defined(lint)

int
xdb_tick_set_limit(int value)
{}

#else	/* lint */

	ENTRY(xdb_tick_set_limit)
	set	BW_BASE+TICK_LIMIT, %o1
	retl
	sta	%o0, [%o1]ASI_BW		! delay slot
	SET_SIZE(xdb_tick_set_limit)
#endif	/* lint */

/*
 * BootBus routines
 */
#define	XOFF_FASTBUS_EPROM	0x00
#define	XOFF_FASTBUS_RSVD_08	0x08
#define	XOFF_FASTBUS_STATUS1	0x10
#define	XOFF_FASTBUS_STATUS2	0x12
#define	XOFF_FASTBUS_STATUS3	0x14	/* dual only */
#define	XOFF_FASTBUS_SOFT_RESET	0x16
#define	XOFF_FASTBUS_BOARD_VERSION	0x18
#define	XOFF_FASTBUS_SEMAPHORE	0x1a	/* dual only */
#define	XOFF_FASTBUS_SRAM_SHARE	0x1c
#define	XOFF_FASTBUS_SRAM	0x1e
#define	CPU_INDEX_SHIFT		3	/* status3 slot + A/B encoding */
#define	SBUS_INDEX_SHIFT	4	/* status3 slot */

/*
 * int
 * xdb_bb_status1_get()
 */
#if defined(lint)

int
xdb_bb_status1_get(void)
{}

#else	/* lint */

	ENTRY(xdb_bb_status1_get)
	set	BB_BASE+(XOFF_FASTBUS_STATUS1 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status1_get)
#endif	/* lint */

/*
 * int
 * xdb_bb_status2_get()
 */
#if defined(lint)

int
xdb_bb_status2_get(void)
{}

#else	/* lint */

	ENTRY(xdb_bb_status2_get)
	set	BB_BASE+(XOFF_FASTBUS_STATUS2 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status2_get)
#endif	/* lint */

/*
 * int
 * xdb_bb_status3_get()
 */
#if defined(lint)

int
xdb_bb_status3_get(void)
{}

#else	/* lint */

	ENTRY(xdb_bb_status3_get)
	set	BB_BASE+(XOFF_FASTBUS_STATUS3 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status3_get)
#endif	/* lint */

/*
 * int
 * xdb_cpu_unit(int device_id)
 */
#if defined(lint)

int
xdb_cpu_unit(int device_id)
{}

#else	/* lint */

	ENTRY(xdb_cpu_unit)
	retl
	srl	%o0, CPU_INDEX_SHIFT, %o0	! delay slot
	SET_SIZE(xdb_cpu_unit)
#endif	/* lint */

/*
 * int
 * xdb_sbus_unit(int device_id)
 */
#if defined(lint)

int
xdb_sbus_unit(int device_id)
{}

#else	/* lint */

	ENTRY(xdb_sbus_unit)
	retl
	srl	%o0, SBUS_INDEX_SHIFT, %o0	! delay slot
	SET_SIZE(xdb_sbus_unit)
#endif	/* lint */


#define	XOFF_SLOWBUS_UART1	0x20
#define	XOFF_SLOWBUS_UART2	0x24
#define	XOFF_SLOWBUS_NVRAM	0x28
#define	XOFF_SLOWBUS_CONTROL	0x2c
#define	XOFF_SLOWBUS_JTAG	0x30
#define	XOFF_SLOWBUS_RSVD_34	0x34

/*
 * int
 * bb_ctl_get_ecsr(cpu_id)
 */
#if defined(lint)

int
bb_ctl_get_ecsr(int cpu_id)
{}

#else	/* lint */

	ENTRY(bb_ctl_get_ecsr)
	sll	%o0, 27, %o0
	set	(XOFF_SLOWBUS_CONTROL << 16), %o2
	add	%o0, %o2, %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(bb_ctl_get_ecsr)
#endif	/* lint */

/*
 * void
 * bb_ctl_set_ecsr(cpu_id, value)
 *	int value;
 */
#if defined(lint)

void
bb_ctl_set_ecsr(int cpu_id, int value)
{}

#else	/* lint */

	ENTRY(bb_ctl_set_ecsr)
	sll	%o0, 27, %o0
	set	(XOFF_SLOWBUS_CONTROL << 16), %o2
	add	%o0, %o2, %o0
	retl
	stba	%o1, [%o0]ASI_BB		! delay slot
	SET_SIZE(bb_ctl_set_ecsr)
#endif	/* lint */

/*
 * Memory Unit
 */
#define	ASI_MQH		0x2f
#define	MQH_BASE	0xe0800000
#define	MQH_MCSR	0x1020

/*
 * Memory Control and Status Register, pg. 76
 */
#define	MCSR_VALUE(RFEN, RDLY, RCNT, TST, ECI, DMPR)	\
	(((RFEN		& 0x0001) << 17) +		\
	((RDLY		& 0x000f) << 13) +		\
	((RCNT		& 0x03ff) <<  3) +		\
	((TST		& 0x0001) <<  2) +		\
	((ECI		& 0x0001) <<  1) +		\
	(DMPR		& 0x0001))

/*
 * mcsr_t
 * xdb_mcsr_get(device_id)
 *	int device_id;
 */
#if defined(lint)

mcsr_t
xdb_mcsr_get(int device_id)
{}

#else	/* lint */

	ENTRY(xdb_mcsr_get)
	set	MQH_BASE+MQH_MCSR, %o2
	or	%o0, %o2, %o2			! device_id qualifier
	ldda	[%o2]ASI_MQH, %o0		! xdbus #0
	add	%o2, 0x100, %o2			! xdbus #1
	retl
	ldda	[%o2]ASI_MQH, %o2		! delay slot
	SET_SIZE(xdb_mcsr_get)
#endif	/* lint */

/*
 * void
 * xdb_mcsr_set(value, device_id)
 *	mcsr_t value;
 *	int device_id;
 */
#if defined(lint)

void
xdb_mcsr_set(mcsr_t value, int device_id)
{}

#else	/* lint */

	ENTRY(xdb_mcsr_set)
	set	MQH_BASE+MQH_MCSR, %o3		! %o3 = temporary
	or	%o2, %o3, %o2			! device_id qualifier
	stda	%o0, [%o2]ASI_MQH		! xdbus #0
	add	%o2, 0x100, %o2			! xdbus #1
	retl
	stda	%o0, [%o2]ASI_MQH		! delay slot
	SET_SIZE(xdb_mcsr_set)
#endif	/* lint */


/*
 * I/O Unit
 * Service SBusses which have interrupt requests pending
 */
#define	ASI_IOC		0x2f
#define	IOC_BASE	0xe0000000
#define	ASI_SBI		0x2f
#define	SBI_BASE	0x02800000

/*
 * WARNING - This function depends upon POST's definition of device_id's.
 *	Perhaps we should have OBP/autoconf define this?
 *	We can't do something simple - we need swap & want performance
 * int
 * xdb_sbusses_devicebase(table_bit_number)
 *	int table_bit_number;
 */
#if defined(lint)

int
xdb_sbusses_devicebase(int table_bit_number)
{}

#else	/* lint */

	ENTRY(xdb_sbusses_devicebase)
	sll	%o0, 28, %o0			! allow for unit#
	sethi	%hi(SBI_BASE), %o1		! need to or-in device_id
	retl
	or	%o0, %o1, %o0			! delay slot
	SET_SIZE(xdb_sbusses_devicebase)
#endif	/* lint */

/*
 * SBI Interrupt State routines
 */
#define	INTR_STATE	0x30

/*
 * int
 * xdb_sbi_take(mask, device_id)
 *	int mask;
 *	int device_id;
 */
#if defined(lint)

int
xdb_sbi_take(int mask, int device_id)
{}

#else	/* lint */

	ENTRY(xdb_sbi_take)
	set	SBI_BASE+INTR_STATE, %o1	! need to or-in device_id
	retl
	swapa	[%o1]ASI_SBI, %o0		! delay slot
	SET_SIZE(xdb_sbi_take)
#endif	/* lint */

/*
 * void
 * xdb_sbi_release(mask, device_id)
 *	int mask;
 *	int device_id;
 */
#if defined(lint)

void
xdb_sbi_release(int mask, int device_id)
{}

#else	/* lint */

	ENTRY(xdb_sbi_release)
	set	SBI_BASE+INTR_STATE, %o1	! need to or-in device_id
	retl
	sta	%o0, [%o1]ASI_SBI		! delay slot
	SET_SIZE(xdb_sbi_release)
#endif	/* lint */

/*
 * SBI Interrupt Target routines
 */
#define	INTR_TARGET	0x34

/*
 * void
 * xdb_itr_set(device_id, target_id)
 *	int device_id;
 *	int target_id;
 */
#if defined(lint)

void
xdb_itr_set(int device_id, int target_id)
{}

#else	/* lint */

	ENTRY(xdb_itr_set)
	set	0, %o0				! fake device_id
	set	0, %o1				! fake target_id
	set	SBI_BASE+INTR_TARGET, %o2	! %o2 = temporary
	retl
	sta	%o1, [%o2]ASI_SBI		! delay slot
	SET_SIZE(xdb_itr_set)
#endif	/* lint */

/*
 * IOC "Snoopy" Tag routines
 */
#define	IOC_DB_TAGS	(0x18 << 16)
#define	IOC_SB_TAGS	(0x1A << 16)

/*
 * void
 * xdb_ioc_dynatag_set(value, line)
 *	ioc_dynatag_t value;
 *	int line;
 */
#if defined(lint)

void
xdb_ioc_dynatag_set(ioc_dynatag_t value, int line)
{}

#else	/* lint */

	ENTRY(xdb_ioc_dynatag_set)
	set	IOC_BASE + IOC_DB_TAGS, %o3
	or	%o2, %o3, %o3
	retl
	stda	%o0, [%o3]ASI_IOC		! delay slot
	SET_SIZE(xdb_ioc_dynatag_set)
#endif	/* lint */

/*
 * void
 * xdb_ioc_sbustag_set(value, line)
 *	ioc_sbustag_t value;
 *	int line;
 */
#if defined(lint)

void
xdb_ioc_sbustag_set(ioc_sbustag_t value, int line)
{}

#else	/* lint */

	ENTRY(xdb_ioc_sbustag_set)
	set	IOC_BASE + IOC_SB_TAGS, %o3
	or	%o2, %o3, %o3
	retl
	stda	%o0, [%o3]ASI_IOC		! delay slot
	SET_SIZE(xdb_ioc_sbustag_set)
#endif	/* lint */

/*
 * See if the virtual address maps to memory.  Memory is
 * always cached and i/o is never cached so we return
 * non-zero if the mapping is marked as cached.  If uncached
 * or the probe fails we return zero.  It's ok to do the
 * ld with the probe argument because it is in the same page
 * and we want to make the time between the load and probe
 * as short as possible.
 */

#include <sys/mmu.h>

#define	PAGE_MASK	0xFFF

#if defined(lint)

/* ARGSUSED */
u_int
get_pte_fast(caddr_t vaddr)
{ return (0); }

#else   /* lint */

	ENTRY(get_pte_fast)
	andn    %o0, PAGE_MASK, %o0     ! make sure lower 12 bits are clear
	or      %o0, (4 << 8), %o0      ! set probe type to "entire"
	ld      [%o0], %g0              ! fault if necessary to get mapping
	lda     [%o0]ASI_FLPR, %o0      ! do the probe
	set	RMMU_FSR_REG, %o1	! setup to clear fsr
	retl
	lda	[%o1]ASI_MOD, %o1	! clear fsr
	SET_SIZE(get_pte_fast)

#endif  /* lint */

#define	MCNTL_PSO_BIT	(1<<7)

#if defined(lint)

void
set_pso(void)
{}

void
clear_pso(void)
{}

#else	/* lint */

	ENTRY(set_pso)
	lda	[%g0]ASI_MOD, %o0	! get SPARC.MCNTL
	or	%o0, MCNTL_PSO_BIT, %o0	! set PSO
	retl
	sta	%o0, [%g0]ASI_MOD	! set SPARC.MCNTL
	SET_SIZE(set_pso)

	ENTRY(clear_pso)
	lda	[%g0]ASI_MOD, %o0		! get SPARC.MCNTL
	and	%o0, ~MCNTL_PSO_BIT, %o0	! clear PSO
	retl
	sta	%o0, [%g0]ASI_MOD		! set SPARC.MCNTL
	SET_SIZE(clear_pso)
#endif	/* lint */
