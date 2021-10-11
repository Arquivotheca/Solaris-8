/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)subr_4d.s	1.68	99/01/20 SMI"

#if defined(lint)
#include <sys/types.h>
#include <sys/privregs.h>
#endif

/*
 * General machine architecture & implementation specific
 * assembly language routines.
 */

#include <sys/asm_linkage.h>
#include <sys/psr.h>
#include <sys/mmu.h>

/*
 * return 32-bit contents at the 36-bit physical address, the upper 4 bits
 * will be in the %o0 and the lower 32 in %o1
 */

#if defined(lint)

/* ARGSUSED */
u_int
asm_ldphys36(unsigned long long phys_addr)
{ return (0); }

#else	/* lint */

	ENTRY_NP(asm_ldphys36)
	and	%o0, 0xF, %o0		! make sure no bogus bits are set
#ifdef sun4d
	! Since we're going to be messing with the AC bit and interrupt
	! handling routines expect it in a certain state, we can't
	! allow interrupts (even level 15) so disable traps.

#ifdef	XXX
	mov	%psr, %o5		! save psr in %o5
	andn	%o5, PSR_ET, %o2	! clear enable traps bit
	mov	%o2, %psr		! new psr
	nop; nop
#endif	XXX

	! sun4d has the requirement that all memory accesses must
	! be cached and all I/O accesses must be non-cached.
	! The 36-bit physical address space is divided into memory space
	! and I/O space (see section 3.2 of sun4d arch manual):
	! PA [35:32] 0x0 - 0x7 is memory space and the AC bit should be on
	! PA [35:32] 0x8 - 0xF is I/O space and the AC bit should be off
	!
	! Disabling traps mean we will watchdog if we get another
	! trap.  To prevent this we turn on the no-fault bit so
	! the load doesn't cause a data access exception trap.  We
	! clear the fault status register so after the load we can
	! check for faults.

	lda	[%g0]ASI_MOD, %o4	! get MMU CSR
	cmp	%o0, 8			! check upper 4 bits
	bge	3f
	mov	0x2, %g1		! NF (no-fault) bit
	set	CPU_VIK_AC, %o2
	or	%g1, %o2, %g1		! AC bit
3:
	or	%g1, %o4, %g1		! or in NF bit and AC bit for mem
	sta	%g1, [%g0]ASI_MOD	! store new CSR

	set	RMMU_FSR_REG, %g1	! fault status reg address
	lda	[%g1]ASI_MOD, %g0	! read it to clear it
#endif
	set	1f, %o2			! set base of table
	sll	%o0, 3, %o3		! multiply by 8 to get offset
	jmp	%o2 + %o3		! jump to base + offset
	nop
1:
	ba	2f;	lda	[%o1]0x20, %o0
	ba	2f;	lda	[%o1]0x21, %o0
	ba	2f;	lda	[%o1]0x22, %o0
	ba	2f;	lda	[%o1]0x23, %o0
	ba	2f;	lda	[%o1]0x24, %o0
	ba	2f;	lda	[%o1]0x25, %o0
	ba	2f;	lda	[%o1]0x26, %o0
	ba	2f;	lda	[%o1]0x27, %o0
	ba	2f;	lda	[%o1]0x28, %o0
	ba	2f;	lda	[%o1]0x29, %o0
	ba	2f;	lda	[%o1]0x2A, %o0
	ba	2f;	lda	[%o1]0x2B, %o0
	ba	2f;	lda	[%o1]0x2C, %o0
	ba	2f;	lda	[%o1]0x2D, %o0
	ba	2f;	lda	[%o1]0x2E, %o0
	ba	2f;	lda	[%o1]0x2F, %o0
2:
#ifdef sun4d
	! The load may have caused a fault, let's check.
	set	RMMU_FSR_REG, %g1	! fault status reg address
	lda	[%g1]ASI_MOD, %g1
	tst	%g1
	bz	3f
	nop

	! we had a fault, time for some magic
	sta	%o4, [%g0]ASI_MOD	! restore MMU CSR
	set	0x1000 + RMMU_FSR_REG, %o4	! writable fsr
	or	%g1, MMU_SFSR_AT_SUPV, %g1	! make it look like supv data
	sta	%g1, [%o4]ASI_MOD	! put back fsr for trap()
	set	0x1000 + RMMU_FAV_REG, %o4	! writable far
	sta	%o1, [%o4]ASI_MOD	! put something in far for trap()

	! fake a data access exception trap
	set	2b, %o4			! this will be the nPC
	mov	%wim, %o5		! save wim
	mov	0, %wim
	save
	add	%i2, %i3, %l1		! add %o2 and %o3 from table jump
	add	%l1, 4, %l1		! add 4 to get PC at lda instr
	mov	%i4, %l2		! set nPC
	mov	%i5, %wim		! put back wim
	mov	%psr, %i2		! get psr
	or	%i2, PSR_PS, %i2	! set PS bit
	mov	%i2, %psr		! load new psr
	mov	%tbr, %i2		! get tbr
	andn	%i2, 0xFFF, %i2		! clear tt field
	add	%i2, 0x90, %i2		! add offset of data fault
	jmp	%i2
	nop

3:
	sta	%o4, [%g0]ASI_MOD	! restore MMU CSR
#ifdef	XXX
	mov	%o5, %psr		! restore psr
	nop; nop; nop			! paranoia
#endif	XXX

#endif
	retl
	nop
	SET_SIZE(asm_ldphys36)

#endif	/* lint */

/*
 * return 32-bit contents at the 36-bit physical address, the upper 4 bits
 * will be in the %o0 and the lower 32 in %o1, and write new contents from
 * %o2.  We use a swapa so we can force this to be synchronous (so the fault
 * is detected right away) and because we want to return the old contents.
 */

#if defined(lint)

/* ARGSUSED */
u_int
stphys36(unsigned long long phys_addr, unsigned int val)
{ return (0); }

#else	/* lint */

	ENTRY_NP(stphys36)
	and	%o0, 0xF, %o0		! make sure no bogus bits are set
#ifdef sun4d
	! Since we're going to be messing with the AC bit and interrupt
	! handling routines expect it in a certain state, we can't
	! allow interrupts (even level 15) so disable traps.

	mov	%psr, %o5		! save psr in %o5
	andn	%o5, PSR_ET, %g1	! clear enable traps bit
	mov	%g1, %psr		! new psr
	nop; nop

	! sun4d has the requirement that all memory accesses must
	! be cached and all I/O accesses must be non-cached.
	! The 36-bit physical address space is divided into memory space
	! and I/O space (see section 3.2 of sun4d arch manaul):
	! PA [35:32] 0x0 - 0x7 is memory space and the AC bit should be on
	! PA [35:32] 0x8 - 0xF is I/O space and the AC bit should be off
	!
	! Disabling traps mean we will watchdog if we get another
	! trap.  To prevent this we turn on the no-fault bit so
	! the load doesn't cause a data access exception trap.  We
	! clear the fault status register so after the load we can
	! check for faults.

	lda	[%g0]ASI_MOD, %o4	! get MMU CSR
	cmp	%o0, 8			! check upper 4 bits
	bge	3f
	mov	0x2, %g1		! NF (no-fault) bit
	set	CPU_VIK_AC, %o3
	or	%g1, %o3, %g1		! AC bit
3:
	or	%g1, %o4, %g1		! or in NF bit and AC bit for mem
	sta	%g1, [%g0]ASI_MOD	! store new CSR

	set	RMMU_FSR_REG, %g1	! fault status reg address
	lda	[%g1]ASI_MOD, %g0	! read it to clear it
#endif
	set	1f, %g1			! set base of table
	sll	%o0, 3, %o3		! multiply by 8 to get offset
	jmp	%g1 + %o3		! jump to base + offset
	mov	%o2, %o0
1:
	ba	2f;	swapa	[%o1]0x20, %o0
	ba	2f;	swapa	[%o1]0x21, %o0
	ba	2f;	swapa	[%o1]0x22, %o0
	ba	2f;	swapa	[%o1]0x23, %o0
	ba	2f;	swapa	[%o1]0x24, %o0
	ba	2f;	swapa	[%o1]0x25, %o0
	ba	2f;	swapa	[%o1]0x26, %o0
	ba	2f;	swapa	[%o1]0x27, %o0
	ba	2f;	swapa	[%o1]0x28, %o0
	ba	2f;	swapa	[%o1]0x29, %o0
	ba	2f;	swapa	[%o1]0x2A, %o0
	ba	2f;	swapa	[%o1]0x2B, %o0
	ba	2f;	swapa	[%o1]0x2C, %o0
	ba	2f;	swapa	[%o1]0x2D, %o0
	ba	2f;	swapa	[%o1]0x2E, %o0
	ba	2f;	swapa	[%o1]0x2F, %o0
2:
#ifdef sun4d
	! The load may have caused a fault, let's check.
	set	RMMU_FSR_REG, %g1	! fault status reg address
	lda	[%g1]ASI_MOD, %g1
	tst	%g1
	bz	3f
	nop

	! we had a fault, time for some magic
	sta	%o4, [%g0]ASI_MOD	! restore MMU CSR
	set	0x1000 + RMMU_FSR_REG, %o4	! writable fsr
	or	%g1, MMU_SFSR_AT_SUPV, %g1	! make it look like supv data
	sta	%g1, [%o4]ASI_MOD	! put back fsr for trap()
	set	0x1000 + RMMU_FAV_REG, %o4	! writable far
	sta	%o1, [%o4]ASI_MOD	! put something in far for trap()

	! fake a data access exception trap
	set	2b, %o4			! this will be the nPC
	mov	%wim, %o5		! save wim
	mov	0, %wim
	save
	add	%i2, %i3, %l1		! add %o2 and %o3 from table jump
	add	%l1, 4, %l1		! add 4 to get PC at lda instr
	mov	%i4, %l2		! set nPC
	mov	%i5, %wim		! put back wim
	mov	%psr, %i2		! get psr
	or	%i2, PSR_PS, %i2	! set PS bit
	mov	%i2, %psr		! load new psr
	mov	%tbr, %i2		! get tbr
	andn	%i2, 0xFFF, %i2		! clear tt field
	add	%i2, 0x90, %i2		! add offset of data fault
	jmp	%i2
	nop

3:
	sta	%o4, [%g0]ASI_MOD	! restore MMU CSR
	mov	%o5, %psr		! restore psr
	nop; nop; nop			! paranoia
#endif
	retl
	nop
	SET_SIZE(stphys36)

#endif	/* lint */

/*
 * CC Interrupt Mask routines
 */
#define	ASI_CC		0x02
#define	CC_BASE		0x01f00000	/* ASI-02 */
#define	CC_STAT_REG	0xb00

/*
 * In TSO, a swap is sufficient to drain the CPU store buffer
 * and will no return until any pending write in the MXCC is done.
 *
 * In PSO, the swap must be preceeded by an stbar.
 * a suspected CPU hardware bug requires to to use two stbars.
 */

#if defined(lint)

/* ARGSUSED */
void
flush_writebuffers_to(caddr_t vaddr)
{}

void
flush_writebuffers(void)
{}

#else	/* lint */

	ALTENTRY(flush_writebuffers)
	ENTRY_NP(flush_writebuffers_to)
	stbar
	stbar
	set	module_wb_flush, %o0
	retl
	swap	[%o0], %g0
	SET_SIZE(flush_writebuffers_to)
	SET_SIZE(flush_writebuffers)

#endif	/* lint */

#if defined(lint)

unsigned long
get_sfsr(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(get_sfsr)
	set	RMMU_FSR_REG, %o0
	retl
	lda	[%o0]ASI_MOD, %o0
	SET_SIZE(get_sfsr)

#endif	/* lint */

/*
 * swap register contents with memory location contents
 */
#if defined(lint)

/* ARGSUSED */
int
swapl(int val, int* vaddr)
{ return (0); }

#else	/* lint */

	ENTRY_NP(swapl)
	retl
	swap	[%o1], %o0
	SET_SIZE(swapl)

#endif	/* lint */


/*
 *	defines also present in ml/xdbus.il.cpp
 */

#define	ASI_BB		0x2F
#define	BB_BASE		0xF0000000

#define	XOFF_FASTBUS_STATUS1	0x10
#define	XOFF_FASTBUS_STATUS2	0x12
#define	XOFF_FASTBUS_STATUS3	0x14

/*
 *	xdb_bb_status1_get: EEPROM status1 register read routine.
 */

#if defined(lint)
u_int
xdb_bb_status1_get(void)
{ return (0); }
#else   /* lint */

	ENTRY(xdb_bb_status1_get)

	set	BB_BASE+(XOFF_FASTBUS_STATUS1 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status1_get)

#endif  /* lint */


/*
 *	xdb_bb_status2_get: EEPROM status2 register read routine.
 */

#if defined(lint)
u_int
xdb_bb_status2_get(void)
{ return (0); }
#else   /* lint */

	ENTRY(xdb_bb_status2_get)

	set	BB_BASE+(XOFF_FASTBUS_STATUS2 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status2_get)

#endif  /* lint */


/*
 *	xdb_bb_status3_get: EEPROM status3 register read routine.
 */

#if defined(lint)
u_int
xdb_bb_status3_get(void)
{ return (0); }
#else   /* lint */

	ENTRY(xdb_bb_status3_get)

	set	BB_BASE+(XOFF_FASTBUS_STATUS3 << 16), %o0
	retl
	lduba	[%o0]ASI_BB, %o0		! delay slot
	SET_SIZE(xdb_bb_status3_get)

#endif  /* lint */


/*
 * Answer questions about any extended SPARC hardware capabilities.
 * On this platform, {s,u}{mul,div}, fsmuld all work. (Because we
 * only use the SuperSPARC pipeline)
 */

#if	defined(lint)

/*ARGSUSED*/
int
get_hwcap_flags(int inkernel)
{ return (0); }

#else	/* lint */

#include <sys/archsystm.h>

	/*
	 * sun4d only uses CPUs based on the Viking pipeline.
	 *
	 * These support hardware multiply and divide, but
	 * have this cunning "feature" that they don't quite support
	 * the full domain of 64-bit / 32-bit divide values, and
	 * generate illegal instruction traps outside that domain.
	 *
	 * For example, the TMS390Z50 only implements a 52-bit
	 * by 32-bit divide.
	 *
	 * In userland and the kernel we can cope with this since
	 * in the userland case the kernel will emulate it, and
	 * in the kernel case, .div and .udiv don't attempt to
	 * deal with arguments that big.
	 *
	 * XXX	Woe betide anyone who attempts to rewrite the kernels
	 *	version of e.g. div64 using sdiv/udiv!
	 *
	 * These CPUs also support the fsmuld instruction.
	 */
	ENTRY(get_hwcap_flags)
#define	FLAGS	AV_SPARC_HWMUL_32x32 | AV_SPARC_HWDIV_32x32 | AV_SPARC_HWFSMULD
	sethi	%hi(FLAGS), %o0
	retl
	or	%o0, %lo(FLAGS), %o0
#undef	FLAGS
	SET_SIZE(get_hwcap_flags)

#endif	/* lint */
