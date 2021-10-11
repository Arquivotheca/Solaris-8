/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subr_4m.s	1.82	99/10/22 SMI"

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */
#if defined(lint)
#include <sys/types.h>
#include <sys/machsystm.h>
#include <sys/t_lock.h>
#include <sys/privregs.h>
#include <sys/memctl.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/mmu.h>
#include <sys/eeprom.h>
#include <sys/param.h>
#include <sys/physaddr.h>
#include <sys/intreg.h>
#include <sys/machthread.h>
#include <sys/async.h>
#include <sys/iommu.h>
#include <sys/devaddr.h>
#include <sys/auxio.h>

#if defined(lint)

/*ARGSUSED*/
void
set_intmask(int bit, int flag)
{}

#else	/* lint */

/*
 * Turn on or off bits in the system interrupt mask register.
 * no need to lock out interrupts, since set/clr softint is atomic.
 * === sipr as specified in 15dec89 sun4m arch spec, sec 5.7.3.2
 *
 * set_intmask(bit, which)
 *	int bit;		bit mask in interrupt mask
 *	int which;		0 = clear_mask (enable), 1 = set_mask (disable)
 */
	ENTRY_NP(set_intmask)
	mov     %psr, %g2
	or      %g2, PSR_PIL, %g1       ! spl hi to protect intreg update
	mov     %g1, %psr
	nop;nop;nop
	tst	%o1
	set	v_sipr_addr, %o5
	ld	[%o5], %o5
	set	IR_CLEAR_OFFSET, %o3
	bnz,a	1f
	add	%o3, 4, %o3		! bump to "set mask"
1:
	st	%o0, [%o5 + %o3]	! set/clear interrupt
	ld	[%o5 + IR_MASK_OFFSET], %o1	! make sure mask bit set
	!
	! Need to use an inline version of splx since an splx call will
	! fail if we are profiling.
	!
	! Have to check CPU->cpu_base_spl because we might have been
	! interrupted between reading and setting the psr.
	!
	ld	[THREAD_REG + T_CPU], %o1	! get base priority level
	ld	[%o1 + CPU_BASE_SPL], %o1
	and	%g2, PSR_PIL, %o2	! get old pri 
	andn	%g2, PSR_PIL, %o3	! clear PIL from old PSR
	cmp	%o1, %o2
	bl,a	1f
	wr	%o3, %o2, %psr		! base priority is less, use old pri
	wr	%o3, %o1, %psr		! use base priority
1:
	nop
	retl
	nop
	SET_SIZE(set_intmask)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
set_intreg(int bit, int flag)
{}

#else	/* lint */

/*
 * Turn on or off bits in the pending interrupt register.
 * no need to lock out interrupts, since set/clr softint is atomic.
 * === MID as specified in 15dec89 sun4m spec, sec 5.4.3
 * === int regs as specified in 15dec89 sun4m arch spec, sec 5.7.1
 *
 * set_intreg(bit, flag)
 *	int bit;		bit mask in interrupt reg
 *	int flag;		0 = off, otherwise on
 */
	ENTRY(set_intreg)
	CPU_INDEX(%o2)			! get cpu id number 0..3
	sll	%o2, 2, %o2		! convert to word offset
	set	v_interrupt_addr, %o3	! want *v_interrupt_addr[cpu]
	ld	[%o3 + %o2], %o3	! get address per module
	inc	IR_CPU_CLEAR, %o3	! bump up to CLEAR pseudo-reg
	tst	%o1			! set or clear?
	bnz,a	1f			! if set,
	add	%o3, 4, %o3		!   use set reg, not clear reg
1:
	retl
	st	%o0, [%o3]		! set/clear interrupt
	SET_SIZE(set_intreg)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
int
getprocessorid(void)
{ return (0); }

#else	/* lint */

/*
 * Get the processor ID.
 * === MID reg as specified in 15dec89 sun4m spec, sec 5.4.3
 */

	ENTRY(getprocessorid)
	CPU_INDEX(%o0)			! get cpu id number 0..3
	retl
	nop
	SET_SIZE(getprocessorid)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
stphys(int physaddr, int value)
{}

/*ARGSUSED*/
int
asm_ldphys(int physaddr)
{ return(0); }

/*ARGSUSED*/
void
scrubphys(int physaddr)
{}

#else	/* lint */

	!
	! Store value at physical address
	!
	! void	stphys(physaddr, value)
	!
	ENTRY(stphys)
	sethi	%hi(mxcc), %o4
	ld	[%o4+%lo(mxcc)], %o5
	tst	%o5
	bz,a	1f
	sta     %o1, [%o0]ASI_MEM
	sethi	%hi(use_table_walk), %o4
	ld	[%o4+%lo(use_table_walk)], %o5
	tst	%o5			! use_cache off in CC mode means
	bz,a	1f			! don't cache the srmmu tables.
	sta     %o1, [%o0]ASI_MEM

	! For Viking, it is necessary to set the AC bit of the
	! module control register to indicate that this access
	! is cacheable.
	mov     %psr, %o4               ! get psr
	or      %o4, PSR_PIL, %o5       ! block traps
	mov     %o5, %psr               ! new psr
	nop; nop; nop                   ! PSR delay
	lda     [%g0]ASI_MOD, %o3       ! get MMU CSR
	set     CPU_VIK_AC, %o5         ! AC bit
	or      %o3, %o5, %o5           ! or in AC bit
	sta     %o5, [%g0]ASI_MOD       ! store new CSR
	sta     %o1, [%o0]ASI_MEM       ! the actual stphys
	sta     %o3, [%g0]ASI_MOD       ! restore CSR
	mov     %o4, %psr               ! restore psr
	nop; nop; nop                   ! PSR delay
1:	retl
	nop
	SET_SIZE(stphys)


	!
	! load value at physical address
	!
	! int   asm_ldphys(physaddr)
	!
	ENTRY(asm_ldphys)
	sethi	%hi(mxcc), %o4
	ld	[%o4+%lo(mxcc)], %o5
	tst	%o5
	bz,a	1f
	lda     [%o0]ASI_MEM, %o0
	sethi	%hi(use_table_walk), %o4
	ld	[%o4+%lo(use_table_walk)], %o5
	tst	%o5			! use_cache off in CC mode means
	bz,a	1f			! don't cache the srmmu tables.
	lda     [%o0]ASI_MEM, %o0

	! For Viking, it is necessary to set the AC bit of the
	! module control register to indicate that this access
	! is cacheable.
	mov     %psr, %o4               ! get psr
	or      %o4, PSR_PIL, %o5       ! block traps
	mov     %o5, %psr               ! new psr
	nop; nop; nop                   ! PSR delay
	lda     [%g0]ASI_MOD, %o3       ! get MMU CSR
	set     CPU_VIK_AC, %o5         ! AC bit
	or      %o3, %o5, %o5           ! or in AC bit
	sta     %o5, [%g0]ASI_MOD       ! store new CSR
	lda     [%o0]ASI_MEM, %o0
	sta     %o3, [%g0]ASI_MOD       ! restore CSR
	mov     %o4, %psr               ! restore psr
	nop; nop; nop                   ! PSR delay
1:	retl
	nop
	SET_SIZE(asm_ldphys)

	!
	! read/write at physical address
	!
	! This routine is called by l15 ecc handler to scrub the
	! faulty address. 
	!
	! void	scrubphys(physaddr)
	!
	ENTRY(scrubphys)
	ldda	[%o0]ASI_MEM, %o2
	stda    %o2, [%o0]ASI_MEM
	retl
	nop
	SET_SIZE(scrubphys)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
int
swapl(int i, int *j)
{ return (0);}

/*ARGSUSED*/
int
atomic_tas(int *i)
{ return (0); }

#else	/* lint */

/*
 * Make the "SWAPL" instruction available
 * for C code to use. First parm is new value,
 * second parm is ptr to int; returns old value.
 */
	ENTRY(swapl)
        retl
        swap    [%o1], %o0
	SET_SIZE(swapl)

/*
 * Atomic "test and set" function
 */
	ENTRY(atomic_tas)
        mov     %o0, %o1
        sub     %g0, 1, %o0
	retl
        swap    [%o1], %o0
	SET_SIZE(atomic_tas)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
iommu_set_ctl(u_int value)
{}

#else	/* lint */

#define	IOMMU_SREG(Reg, Offset)	\
	set	v_iommu_addr, %o1				;\
	ld	[%o1], %o1					;\
	inc	Offset, %o1					;\
	retl							;\
	st	Reg, [%o1]
	
	/*
 	 * Set iommu ctlr reg.
 	 * iommu_set_ctl(value)
 	 */
	ENTRY(iommu_set_ctl)
#if (IOMMU_CTL_REG != 0)
	IOMMU_SREG(%o0, IOMMU_CTL_REG)
#else
	set	v_iommu_addr, %o1	
	ld	[%o1], %o1
	retl
	st	%o0, [%o1]
#endif
	SET_SIZE(iommu_set_ctl)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
iommu_set_base(u_int value)
{}

void
iommu_flush_all(void)
{}

/*ARGSUSED*/
void
iommu_addr_flush(int addr)
{}

#else	/* lint */

	/*
 	 * Set iommu base addr reg.
 	 */
	ENTRY(iommu_set_base)
	IOMMU_SREG(%o0, IOMMU_BASE_REG)
	SET_SIZE(iommu_set_base)

	/*
 	 * iommu flush all TLBs 
 	 */
	ENTRY(iommu_flush_all)
	IOMMU_SREG(%g0, IOMMU_FLUSH_ALL_REG)
	SET_SIZE(iommu_flush_all)

	/*
 	 * iommu addr flush 
	 */
	ENTRY(iommu_addr_flush)
	IOMMU_SREG(%o0, IOMMU_ADDR_FLUSH_REG)
	SET_SIZE(iommu_addr_flush)

#endif	/* lint */

/*
 * The next two functions are necessitated by a viking/mbus bug with
 * non-cached loads.  See mmu_writepte in module_vik.c for the gory
 * details.
 */

#if defined(lint)

/*ARGSUSED*/
void
memctl_getregs(u_int *afsr, u_int *afar0, u_int *afar1)
{}

/*ARGSUSED*/
void
sbusctl_getregs(u_int *afsr, u_int *afar0)
{}

/*ARGSUSED*/
void
sbus_set_64bit(u_int slot)
{}

/*ARGSUSED*/
void
memctl_set_enable(u_int setbits, u_int clrbits)
{}

void
msi_sync_mode(void)
{}

#else

	! Loads from the memory controller regs must be in single
	! instruction groups.
	ENTRY(memctl_getregs)
	sethi	%hi(v_memerr_addr), %o3
	ld	[%o3 + %lo(v_memerr_addr)], %o3
	ld	[%o3 + 0x8], %o4
	st	%o4, [%o0]
	ld	[%o3 + 0x10], %o4
	st	%o4, [%o1]
	ld	[%o3 + 0x14], %o4
	st	%o4, [%o2]
	retl
	nop
	SET_SIZE(memctl_getregs)

	! Loads from the sbus controller regs must be in single
	! instruction groups (for msbi).
	ENTRY(sbusctl_getregs)
	sethi	%hi(v_sbusctl_addr), %o3
	ld	[%o3 + %lo(v_sbusctl_addr)], %o3
	ld	[%o3 + 0x0], %o4
	st	%o4, [%o0]
	ld	[%o3 + 0x4], %o4
	st	%o4, [%o1]
	retl
	nop
	SET_SIZE(sbusctl_getregs)

	! Loads from the sbus controller regs must be in single
	! instruction groups (for msbi).
	ENTRY(sbus_set_64bit)
	sethi	%hi(v_sbusctl_addr), %o1
	ld	[%o1 + %lo(v_sbusctl_addr)], %o1
	sll	%o0, 2, %o2
	inc	0x10, %o2		! (slot# * 4) + 0x10
	add	%o2, %o1, %o2		! slot base address
	ld	[%o2], %o3
	sethi	%hi(0x4000), %o1	! set 64-bit support
	or	%o1, %o3, %o3
	st	%o3, [%o2]
	retl
	nop
	SET_SIZE(sbus_set_64bit)

	! set/clear bits in memory enable register
	! Loads from the memory controller regs must be in single
	! instruction groups.
	ENTRY(memctl_set_enable)
	sethi	%hi(v_memerr_addr), %o3
	ld	[%o3 + %lo(v_memerr_addr)], %o3
	ld	[%o3], %o4
	or	%o4, %o0, %o4		! set bits
	andn	%o4, %o1, %o4		! clear bits
	retl
	st	%o4, [%o3]
	SET_SIZE(memctl_set_enable)

	! Put the msi into synchronous mode.
	! This prevents the msi from doing a CR within 5 cycles of a
	! load.  This makes all loads thru the msi safe in multi-
	! instruction groups on viking.
	ENTRY(msi_sync_mode)
	set	PA_MBUS_ARBEN, %o0
	lda	[%o0]ASI_CTL, %o1
	set	0x80000000, %o2
	andn	%o1, %o2, %o1
	retl
	sta	%o1, [%o0]ASI_CTL
	SET_SIZE(msi_sync_mode)

#endif	/* lint */

#if defined(lint)

u_long
get_sfsr(void)
{ return (0); }

#else
	
	ENTRY(get_sfsr)
	set     RMMU_FSR_REG, %o0
	retl
	lda     [%o0]ASI_MOD, %o0
	SET_SIZE(get_sfsr)

#endif	/* lint */

#if defined(lint)

u_long
get_sysctl(void)
{ return (0); }

#else

/*
 *      Code used to vector to the correct routine when the
 *      action to be performed is done differently on different
 * 	Sun-4M system implementations. Module differences are
 *	handled in mmu_asi.s.
 *
 *      The default action is whatever conforms to the latest 
 *   	Sun4M System Architecture Specification; if an implementation
 * 	requires things to be done differently, then the kernel must 
 * 	install the proper function pointer in the v_ table. This
 *	happens in early_startup() once the system type has been
 *	retrieved from the PROM.
 */

#define VECT(s) .global v_/**/s; v_/**/s: .word sun4m_/**/s

        .seg    ".data"
        .align  4
v_sys_base:
VECT(get_sysctl)
VECT(set_sysctl)
VECT(get_diagmesg)
VECT(set_diagmesg)
VECT(enable_dvma)
VECT(disable_dvma)
VECT(l15_async_fault)
VECT(flush_writebuffers)
VECT(flush_writebuffers_to)
VECT(memerr_init)
VECT(memerr_disable)
VECT(impl_bustype)
VECT(process_aflt)
VECT(timeout_cei)

	.seg    ".text"
	.align  4

/*
 * Read the System Control & Status Register
 *
 *	u_long get_sysctl()
 */
	ENTRY(get_sysctl)
	sethi	%hi(v_get_sysctl), %o5
	ld	[%o5 + %lo(v_get_sysctl)], %o5
	jmp 	%o5
	nop
	SET_SIZE(get_sysctl)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
set_sysctl(u_long val)
{}

#else	/* lint */

/*
 * Write the System Control & Status Register
 *
 *	void set_sysctl(val)
 *	u_long val;
 */
	ENTRY(set_sysctl)
	sethi	%hi(v_set_sysctl), %o5
	ld	[%o5 + %lo(v_set_sysctl)], %o5
	jmp 	%o5
	nop
	SET_SIZE(set_sysctl)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
set_diagled(u_short val)
{}

#else	/* lint */

/* 
 * Write the Diagnostic LED Register (noop for some implementations)
 *
 *	void set_diagled(val)
 *	u_short val; XXX - 8/16/32bit access?
 */
	ENTRY_NP(set_diagled)
	set 	PA_DIAGLED, %o1
	retl
	stha	%o0, [%o1]ASI_CTL	! Sun-4M has 12 LEDs
	SET_SIZE(set_diagled)

#endif	/* lint */

#if defined(lint)

u_long
get_diagmesg(void)
{ return (0); }

#else	/* lint */

/*
 * Read the Diagnostic Message Register
 *
 *	u_long get_diagmesg()
 */
	ENTRY(get_diagmesg)
	sethi	%hi(v_get_diagmesg), %o5
	ld	[%o5 + %lo(v_get_diagmesg)], %o5
	jmp 	%o5
	nop
	SET_SIZE(get_diagmesg)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
set_diagmesg(u_long val)
{}

#else	/* lint */

/* 
 * Write the Diagnostic Message Register
 * XXX - should this use byte access only?
 *
 *	void set_diagmesg(val)
 *	u_long val;
 */
	ENTRY(set_diagmesg)
	sethi	%hi(v_set_diagmesg), %o5
	ld	[%o5 + %lo(v_set_diagmesg)], %o5
	jmp 	%o5
	nop
	SET_SIZE(set_diagmesg)

#endif	/* lint */

#if defined(lint)

void
enable_dvma(void)
{}

#else	/* lint */

/*
 * Enable DVMA Arbitration
 *
 *	void enable_dvma()
 */
	ENTRY(enable_dvma)
	sethi	%hi(v_enable_dvma), %o5
	ld	[%o5 + %lo(v_enable_dvma)], %o5
	jmp 	%o5
	nop
	SET_SIZE(enable_dvma)

#endif	/* lint */

#if defined(lint)

void
disable_dvma(void)
{}

#else	/* lint */

/*
 * Disable DVMA Arbitration 
 *
 *	void disable_dvma()
 */
	ENTRY(disable_dvma)
	sethi	%hi(v_disable_dvma), %o5
	ld	[%o5 + %lo(v_disable_dvma)], %o5
	jmp 	%o5
	nop
	SET_SIZE(disable_dvma)

#endif	/* lint */

#if defined(lint)

void
l15_async_fault(void)
{}

#else	/* lint */

/*
 * Level-15 Asynchronous Fault Handler
 *
 *	void l15_async_fault()
 */
	ENTRY(l15_async_fault)
	sethi	%hi(v_l15_async_fault), %o5
	ld	[%o5 + %lo(v_l15_async_fault)], %o5
	jmp 	%o5
	nop
	SET_SIZE(l15_async_fault)

#endif	/* lint */

#if defined(lint)

void
flush_writebuffers(void)
{}

#else	/* lint */

/*
 * Flush Writebuffers
 */
	ENTRY(flush_writebuffers)
	sethi	%hi(v_flush_writebuffers), %o5
	ld	[%o5 + %lo(v_flush_writebuffers)], %o5
	jmp 	%o5
	nop
	SET_SIZE(flush_writebuffers)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
flush_writebuffers_to(caddr_t arg)
{}

#else	/* lint */

/*
 * Flush Writebuffers after a poke-type probe
 */
        ENTRY(flush_writebuffers_to)
	sethi	%hi(v_flush_writebuffers_to), %o5
	ld	[%o5 + %lo(v_flush_writebuffers_to)], %o5
	jmp 	%o5
	nop
	SET_SIZE(flush_writebuffers_to)

#endif	/* lint */

#if defined(lint)

void
memerr_init(void)
{}

void
memerr_disable(void)
{}

#else

/*
 * Initialize memory error detection
 *
 *      void memerr_init()
 */
        ENTRY(memerr_init)
        sethi   %hi(v_memerr_init), %o5
        ld      [%o5 + %lo(v_memerr_init)], %o5
        jmp     %o5
        nop
        SET_SIZE(memerr_init)

/*
 * Initialize memory error detection
 *
 *      void memerr_disable()
 */
        ENTRY(memerr_disable)
        sethi   %hi(v_memerr_disable), %o5
        ld      [%o5 + %lo(v_memerr_disable)], %o5
        jmp     %o5
        nop
        SET_SIZE(memerr_disable)

#endif  /* lint */
 
#if defined(lint)

/*ARGSUSED*/
int
impl_bustype(u_int type)
{ return (0); }

#else

        ENTRY(impl_bustype)
        sethi   %hi(v_impl_bustype), %o5
        ld      [%o5 + %lo(v_impl_bustype)], %o5
        jmp     %o5
        nop
        SET_SIZE(impl_bustype)

#endif  /* lint */

#if defined(lint)

/*ARGSUSED*/
u_int
timeout_cei(caddr_t arg)
{ return (0); }

#else	/* lint */

/*
 * Level-8 Enable Correctable Memory Errors
 *
 *	void timeout_cei()
 */
	ENTRY(timeout_cei)
	sethi	%hi(v_timeout_cei), %o5
	ld	[%o5 + %lo(v_timeout_cei)], %o5
	jmp 	%o5
	nop
	SET_SIZE(timeout_cei)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
u_int
process_aflt(caddr_t arg)
{ return (0); }

#else	/* lint */

/*
 * Level-12 Asynchronous Fault Handler (Recoverable Level-15)
 *
 *	void process_aflt()
 */
	ENTRY(process_aflt)
	sethi	%hi(v_process_aflt), %o5
	ld	[%o5 + %lo(v_process_aflt)], %o5
	jmp 	%o5
	nop
	SET_SIZE(process_aflt)

#endif	/* lint */


/*
 * Sun-4M Stuff
 *
 *  generic sun4m routines
 */

/*
 *	Floppy disk interrupt handler's machine specific
 *	code. 
 *	Registers that can be used:
 *		%l5, %l6, %l7
 */

#if defined(lint)

int
impl_setintreg_on(void)
{ return (0); }

void
sun4m_get_sysctl(void)
{}

/*ARGSUSED*/
void
sun4m_set_sysctl(int i)
{}

#else	/* lint */

	DGDEF(fd_softintr_cookie)	.word IR_SOFT_INT4

	ENTRY_NP(impl_setintreg_on)

	/*
	 * set a pending software interrupt
	 * Use %l6 as the value of the interrupt
	 * Assumes %l4-%l6 are available for it's use
	 */   

#define Ptr %l5 /* len - length of data */
#define Inp %l6 /* misc ptr */
#define Tmp %l4 /* scratch */

	CPU_INDEX(Tmp)			! get cpu id number 0..3
	sll	Tmp, 2, Tmp		! convert to word offset
	set	v_interrupt_addr, Ptr
	ld	[Ptr + Tmp], Ptr	! v_interrupt_addr[cpu]
	jmp	%l7 + 8
	st	Inp, [Ptr + IR_CPU_SOFTINT]	! send the softint
	SET_SIZE(impl_setintreg_on)


	ENTRY(sun4m_get_sysctl)
	set 	PA_SYSCTL, %o1
	retl
	lda	[%o1]ASI_CTL, %o0
	SET_SIZE(sun4m_get_sysctl)


	ENTRY(sun4m_set_sysctl)
	set 	PA_SYSCTL, %o1
	retl
	sta	%o0, [%o1]ASI_CTL
	SET_SIZE(sun4m_set_sysctl)

#endif	/* lint */


#if defined(lint)

int
sun4m_get_diagmesg(void)
{ return (0); }

#else	/* lint */

	ENTRY(sun4m_get_diagmesg)
	set 	PA_DIAGMESG, %o1
	retl
	lda	[%o1]ASI_CTL, %o0
	SET_SIZE(sun4m_get_diagmesg)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
sun4m_setdiagmesg(int i)
{}

#else	/* lint */

	ENTRY(sun4m_set_diagmesg)
	set 	PA_DIAGMESG, %o1
	retl
	sta	%o0, [%o1]ASI_CTL
	SET_SIZE(sun4m_set_diagmesg)

#endif	/* lint */

/*
 * The next two are in assembly because of a viking non-cached ld bug.
 */

#if defined(lint)

void
sun4m_memerr_init(void)
{}

void
sun4m_memerr_disable(void)
{}

#else

	ENTRY(sun4m_memerr_init)
	set	0x3, %o2		! ecc checking and interrupt bits
	sethi	%hi(v_memerr_addr), %o0
	ld	[%o0 + %lo(v_memerr_addr)], %o0
        st      %g0, [%o0 +0x8]         ! write to EFSR clears error status
                                        ! and releases interrupt
	ld	[%o0], %o1		! must be in single instruction group
	or	%o1, %o2, %o1
	retl
	st	%o1, [%o0]
	SET_SIZE(sun4m_memerr_init)

	ENTRY(sun4m_memerr_disable)
	set	0x3, %o2		! ecc checking and interrupt bits
	sethi	%hi(v_memerr_addr), %o0
	ld	[%o0 + %lo(v_memerr_addr)], %o0
	ld	[%o0], %o1		! must be in single instruction group
	andn	%o1, %o2, %o1
	retl
	st	%o1, [%o0]
	SET_SIZE(sun4m_memerr_disable)

#endif	/* lint */

#if defined(lint)

void
sun4m_enable_dvma(void)
{}

#else	/* lint */

	ENTRY(sun4m_enable_dvma)
	set     PA_MBUS_ARBEN, %o0
	lda	[%o0]ASI_CTL, %o1	! read old value
	set     EN_ARB_SBUS, %o2
	or	%o1, %o2, %o1		! enable dvma
	retl
	sta     %o1, [%o0]ASI_CTL
	SET_SIZE(sun4m_enable_dvma)

#endif	/* lint */

#if defined(lint)

void
sun4m_disable_dvma(void)
{}

#else	/* lint */

	ENTRY(sun4m_disable_dvma)
        set     PA_MBUS_ARBEN, %o0
	lda	[%o0]ASI_CTL, %o1	! read old value
	set	EN_ARB_SBUS, %o2
	andn	%o1, %o2, %o1		! disable dvma
        retl
        sta     %o1, [%o0]ASI_CTL
	SET_SIZE(sun4m_disable_dvma)

#endif	/* lint */

#if defined(lint)
/* 
 * Flush module and M-to-S write buffers.  These are
 * the write buffers which MUST be flushed just before changing 
 * the MMU context.  Note that we don't need to flush the mem I/F
 * (ECC) write buffer because the context number at the time of the
 * fault is not needed to perform asynchronous fault handling.
 * The one exception where we don't need to call this
 * routine is when changing contexts temporarly to flush caches.
 */

void
sun4m_flush_writebuffers(void)
{}

#else	/* lint */

	ALTENTRY(sun4m_flush_writebuffers_to)
	ENTRY(sun4m_flush_writebuffers)
 	/*
	 * flush module write buffer by performing a swap on some
	 * dummy memory location which is cached.  This is supposed
	 * to work because the swap is supposed to be an atomic
	 * instruction, and so the hardware is supposed to stall
	 * for the write to complete, which implies that the write
	 * buffer for the write-back cache is flushed, since this
	 * is the last write.
 	 */
/* commented out 19sep90 impala@dcpower,
 * we don't really need to do this. */
!	set	module_wb_flush, %o0
!	swap	[%o0], %g0

	/*
	 * the hardware was designed to allow one to flush the
	 * M-to-S write buffer by simply reading the M-to-S
	 * Asynchronous Fault Status Register.
	 * According to the manual, gotta read this thrice.
	 */
	set	v_sbusctl_addr, %o0
	ld	[%o0], %o0
	!
	! This code knows that MTS_AFSR offset is 0
	! The extra add is to make sure none of the ld's here group on
	! viking.  This is done so that viking 2.x vo modules work on
	! msbi machines.  See bugid 1102800 for the viking bug, 1131285
	! for why msbi's are also affected.
	!
	ld	[%o0], %o1
	ld	[%o0], %o1
	ld	[%o0], %o1
	add	%o1, 1, %o1
	/*
	 * extra nops put in to ensure that the fault will occurr
	 * while executing instructions here (before we return).
	 * XXX -- need to chack with hardware to see if some of
	 * these nops can be removed.
	 */
	nop
	nop
	nop
	nop
	nop
	retl
	nop
	SET_SIZE(sun4m_flush_writebuffers)
	SET_SIZE(sun4m_flush_writebuffers_to)

#endif	/* lint */

#if defined(lint)

enum mc_type
memctl_type(void)
{ return (0); }

#else	/* lint */

	ENTRY(memctl_type)
	sethi	%hi(v_memerr_addr), %o0
	ld	[%o0 + %lo(v_memerr_addr)], %o0
	ld	[%o0], %o0
	srl	%o0, 28, %o0
	retl
	nop
	SET_SIZE(memctl_type)
	
#endif	/* lint */
