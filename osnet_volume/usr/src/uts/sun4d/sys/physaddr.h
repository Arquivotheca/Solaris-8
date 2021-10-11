/*
 * Copyright (c) 1990,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * sun4d physical address defines
 */

#ifndef _SYS_PHYSADDR_H
#define	_SYS_PHYSADDR_H

#pragma ident	"@(#)physaddr.h	1.28	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * sun4d systems have 36 bit physical addresses.
 * The kernel generally represents them with u_longlong's.
 * However, fixed I/O space addresses are often accessed via
 * ASI 0x2* for paddr[35:32] with a uint_t for paddr[31:0].
 */

/*
 * I/O space addresses have paddr[35] = 1;
 */

#define	PA_IS_IO_SPACE(pa) ((pa) & ((u_longlong_t)1 << 35))

/*
 * CPU Unit's device_id can be quickly derived from cpu_id
 */

#define	PROP_DEVICE_ID "device-id"

#ifdef sun4d
#define	CPUID_2_DEVID(cpu_id)	((cpu_id) << 3)
#else /* cray4d */
#define	CPUID_ABCD_MASK	0x3
#define	CPUID_SLOT_MASK	0x3c
#define	CPUID_2_DEVID(cpu_id)	((cpu_id) & CPUID_ABCD_MASK) << 5) | \
				((cpu_id) & CPUID_SLOT_MASK) >> 1)
#endif

/*
 * Control Status and Register (CSR) Space
 * for functional units replicated per XDBUS
 */
#define	ASI_CSR		0x2f
#define	CSR_ADDR_BASE 0xE0000000	/* paddr[35:28] = 0xFE */
#define	CSR_DEVID_SHIFT	20		/* paddr[27:20] = devid[7:0] */
#define	CSR_BUS_SHIFT	8		/* paddr[9:8] = XDBUS index */

/*
 * Extended Control Status and Register (ECSR) Space
 * for functional units not replicated per XDBUS
 */

#define	ASI_ECSR	0x2f
#define	ECSR_ADDR_BASE 0x00000000	/* paddr[35:32] = 0xF */
#define	ECSR_DEVID_SHIFT	24	/* paddr[31:25] = devid[7:1] */
					/* devid[0] is 0 in ECSR space */

/* consolidate this stuff */
#define	CSR_WIDTH	4
#define	DEV_ID_WIDTH	8

#define	CPU_INDEX_WIDTH	5  /* status3 slot + A/B encoding */

#define	CSR_CPU_PREFIX		(0xe << CPU_INDEX_WIDTH)
#define	CPU_2_CSR_SHIFT		((32 - CSR_WIDTH) - CPU_INDEX_WIDTH)
#define	CPU_2_ECSR_SHIFT	(32 - CPU_INDEX_WIDTH)
#define	DEV_2_ECSR_SHIFT	(32 - DEV_ID_WIDTH)

#if !defined(_ASM)

/*
 * prototypes for physical address ASI inlines in sun4d.il.cpp
 */
extern uchar_t	lduba_2f(uchar_t *addr);
extern ushort_t	lduha_2f(ushort_t *addr);
extern uint_t	lda_2f(uint_t *addr);
extern u_longlong_t	ldda_2f(u_longlong_t *src);
extern void	stba_2f(uchar_t value, uchar_t *addr);
extern void	stha_2f(ushort_t value, ushort_t *addr);
extern void	sta_2f(uint_t value, uint_t *addr);
extern void	stda_2f(u_longlong_t value, u_longlong_t *addr);
extern uint_t	swapa_2f(uint_t value, uint_t offset);
extern u_longlong_t	ldda_02(u_longlong_t *src);
extern void	sta_28(uint_t value, uint_t *addr);
extern void	sta_29(uint_t value, uint_t *addr);
extern void	sta_2a(uint_t value, uint_t *addr);
extern void	sta_2b(uint_t value, uint_t *addr);

#endif _ASM

/*
 * Decode SBus Space physical addresses.
 * Valid for all sun4d variants, -- system board numbers are [0 - 15].
 */
#define	PA_SBUS_SPACE_BASE ((u_longlong_t)0x800000000)
#define	PA_SBUS_SPACE_MASK ((u_longlong_t)0xc00000000)

#define	PA_SBUS_SYSBRD_SHIFT 30
#define	PA_SBUS_SYSBRD_MASK ((u_longlong_t)0xF << PA_SBUS_SYSBRD_SHIFT)
#define	PA_SBUS_TO_SYSBRD(pa) \
	((uint_t)(((pa) & PA_SBUS_SYSBRD_MASK) >> PA_SBUS_SYSBRD_SHIFT))

#define	PA_SBUS_SLOT_SHIFT 28
#define	PA_SBUS_SLOT_MASK (0x3 << PA_SBUS_SLOT_SHIFT)
#define	PA_SBUS_TO_SBUS_SLOT(pa) \
	(((uint_t)(pa) & PA_SBUS_SLOT_MASK) >> PA_SBUS_SLOT_SHIFT)

#define	PA_SBUS_OFFSET_MASK 0x0FFFFFFF
#define	PA_SBUS_TO_SBUS_OFF(pa) \
	((uint_t)(pa) & PA_SBUS_OFFSET_MASK)


/*
 * Cache Controller registers
 */
#define	OFF_CC_REGS			0x01f00000
#define	OFF_CC_STREAM_DATA		0x0
#define	OFF_CC_STREAM_DATA_SIZE		0x3f
#define	OFF_CC_STREAM_SRC_ADDR		0x100
#define	OFF_CC_STREAM_DEST_ADDR		0x200
#define	OFF_CC_REF_COUNT		0x300
#define	OFF_CC_INTR_PENDING		0x406
#define	OFF_CC_INTR_MASK		0x506
#define	OFF_CC_INTR_PENDING_CLEAR	0x606
#define	OFF_CC_INTR_GEN_ASI2		0x704	/* ASI 0x2 access only */
#define	OFF_CC_BIST			0x804	/* ASI 0x2 access only */
#define	OFF_CC_CONTROL			0xa04
#define	OFF_CC_STATUS			0xb00
#define	OFF_CC_RESET			0xc04
#define	OFF_CC_ERROR			0xe00
#define	OFF_CC_COMPONENT_ID		0xf04

#define	MXCC_CID_MREV_MASK	0xF0
#define	MXCC_CID_MREV_SHIFT	4
#define	MXCC_CID_MREV_1	0
#define	MXCC_CID_MREV_2	4
#define	MXCC_CID_MREV_3	8
#define	MXCC_CID_MREV_4	12

#if !defined(_ASM)

#define	MXCC_MC_OK(mrev) \
	((mrev) >= MXCC_CID_MREV_4)

#define	mxcc_cid_get_ecsr(cpuid) \
	lda_2f((uint_t *)(ECSR_ADDR_BASE + \
			(CPUID_2_DEVID(cpuid) << ECSR_DEVID_SHIFT) + \
			OFF_CC_REGS + OFF_CC_COMPONENT_ID))

#define	intr_clear_pend_ecsr(id, level) \
	stha_2f(1 << (level), \
		(ushort_t *)(OFF_CC_REGS | OFF_CC_INTR_PENDING_CLEAR | \
		((id) << CPU_2_ECSR_SHIFT)))

#define	nvram_get_byte(id, off) \
	lduba_2f((uchar_t *)(((id) << CPU_2_ECSR_SHIFT) | (off) | OFF_BB_NVRAM))

#define	nvram_set_byte(id, off, val) \
	stba_2f((val), \
		(uchar_t *)(((id) << CPU_2_ECSR_SHIFT) | (off) | OFF_BB_NVRAM))

#define	bb_ecsr_read_stat2(id) \
	lduba_2f((uchar_t *)((CPUID_2_DEVID(id) << ECSR_DEVID_SHIFT) | \
	XOFF_FASTBUS_STATUS2))

#endif _ASM

/*
 * Bus Watcher registers
 */
#define	OFFSET_TIMER_LIMIT		0x0
#define	OFFSET_TIMER_COUNTER		0x4
#define	OFFSET_TIMER_NONDEST_LIMIT	0x8
#define	OFFSET_TIMER_CONTROL		0xc
#define	OFFSET_USER_TIMER_MSW	OFFSET_TIMER_LIMIT
#define	OFFSET_USER_TIMER_LSW	OFFSET_TIMER_COUNTER

#define	OFF_BW_COMPOENT_ID	0
#define	OFF_BW_DBUS_CTL_STAT	0x8
#define	OFF_BW_DBUS_DATA	0x10
#define	OFF_BW_CONTROL		0x1000
#define	OFF_BW_INTR_TABLE	0x1040
#define	OFF_BW_INTR_TABLE_CLEAR 0x1080
#define	OFF_BW_PRESCALER	0x10c0
#define	OFF_BW_PROFILE_TIMER	0x2000
#define	OFF_BW_TICK_TIMER	0x3000
#define	OFF_BW_PROFILE_TIMER_LIMIT	\
		(OFF_BW_PROFILE_TIMER | OFFSET_TIMER_LIMIT)
#define	OFF_BW_PROFILE_TIMER_COUNTER	\
		(OFF_BW_PROFILE_TIMER | OFFSET_TIMER_COUNTER)
#define	OFF_BW_PROFILE_TIMER_NONDEST_LIMIT \
		(OFF_BW_PROFILE_TIMER | OFFSET_TIMER_NONDEST_LIMIT)
#define	OFF_BW_PROFILE_TIMER_CONTROL	\
		(OFF_BW_PROFILE_TIMER | OFFSET_TIMER_CONTROL)
#define	OFF_BW_TICK_TIMER_LIMIT		\
		(OFF_BW_TICK_TIMER | OFFSET_TIMER_LIMIT)
#define	OFF_BW_TICK_TIMER_COUNTER	\
		(OFF_BW_TICK_TIMER | OFFSET_TIMER_COUNTER)
#define	OFF_BW_TICK_TIMER_NONDEST_LIMIT \
		(OFF_BW_TICK_TIMER | OFFSET_TIMER_NONDEST_LIMIT)


#define	BW_LCCNT_WRITE_UPDATE		0
#define	BW_LCCNT_WRITE_INVALIDATE	63

#define	BW_CTL_LCCNT_SHIFT	6
#define	BW_CTL_LCCNT_MASK	((0x3f) << BW_CTL_LCCNT_SHIFT)

#define	bw_cntl_get(a) lda_2f((uint_t *)((a) | OFF_BW_CONTROL))
#define	bw_cntl_set(v, a) sta_2f((uint_t)(v), (uint_t *)((a) | \
	OFF_BW_CONTROL))

#define	BW_DCSR_RDTOL_SHIFT	8
#define	BW_DCSR_RDTOL_MASK	((0xf) << BW_DCSR_RDTOL_SHIFT)

#define	bw_dcsr_get(a)	ldda_2f((u_longlong_t *)((a) | OFF_BW_DBUS_CTL_STAT))
#define	bw_dcsr_set(v, a) \
	stda_2f((u_longlong_t)(v), (u_longlong_t *)((a) | \
		OFF_BW_DBUS_CTL_STAT))

/*
 * Boot Bus
 */
#define	OFF_BB_NVRAM			0x280000
#define	OFF_BB_NVRAM_CHECKSUM		0x0	/* OBP dependency */
#define	XOFF_FASTBUS_STATUS2		0x120000

/*
 * IOC registers
 */
#define	OFF_IOC_COMPONENT_ID		0x0
#define	OFF_IOC_DBUS_CONTROL_STATUS	0x8
#define	OFF_IOC_DBUS_DATA		0x10
#define	OFF_IOC_CONTROL			0x1000

#define	IOC_CID_DW_BUG	0x10ADD07D
#define	IOC_CID_NEW	0x20ADD07D

#define	ioc_get_cid(devid, bus) \
	lda_2f((uint_t *)(CSR_ADDR_BASE + ((devid) << CSR_DEVID_SHIFT) + \
		((bus) << CSR_BUS_SHIFT) + OFF_IOC_COMPONENT_ID))

/*
 * SBI registers
 */
#define	OFF_SBI_COMPONENT_ID		0x0
#define	OFF_SBI_CONTROL			0x4
#define	OFF_SBI_STATUS			0x8
#define	OFF_SBI_SLOT0_CONFIG		0x10
#define	OFF_SBI_SLOT1_CONFIG		0x14
#define	OFF_SBI_SLOT2_CONFIG		0x18
#define	OFF_SBI_SLOT3_CONFIG		0x1c
#define	OFF_SBI_SLOT0_STBUF_CONTROL	0x20
#define	OFF_SBI_SLOT1_STBUF_CONTROL	0x24
#define	OFF_SBI_SLOT2_STBUF_CONTROL	0x28
#define	OFF_SBI_SLOT3_STBUF_CONTROL	0x2c
#define	OFF_SBI_INTR_STATE		0x30
#define	OFF_SBI_INTR_TARGET_ID		0x34
#define	OFF_SBI_INTR_DIAG		0x38

/*
 * SBI control register setting macros.
 */
#define	VA_SBI_CNTL(va_sbi)	((va_sbi) + OFF_SBI_CONTROL)
#define	SDTOL_SHFT			17
#define	DFLT_SDTOL			0xD
#define	sbi_set_sdtol(ctl, sdtol) \
	((ctl) | ((sdtol) << SDTOL_SHFT))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PHYSADDR_H */
