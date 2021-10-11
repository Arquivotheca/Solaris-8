/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)map.s	1.13	96/02/27 SMI" /* From SunOS 4.1.1 */

/*
 * Miscellaneous routines.
 */

#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/mmu.h>

#if !defined(lint)

	.seg	".text"
	.align	4

/*
 * u_int
 * stphys36(phys_addr, val)
 *	unsigned long long phys_addr;
 *	unsigned int val;
 * 
 * return 32-bit contents at the 36-bit physical address, the upper 4 bits
 * will be in the %o0 and the lower 32 in %o1, and write new contents from
 * %o2.  We use a swapa so we can force this to be synchronous (so the fault
 * is detected right away) and because we want to return the old contents.
 */

	ENTRY(stphys36)
	and	%o0, 0xF, %o0		! make sure no bogus bits are set
#ifdef sun4d
	! Since we're going to be messing with the AC bit and interrupt
	! handling routines expect it in a certain state, we can't
	! allow interrupts (even level 15) so disable traps.

	mov	%psr, %o5		! save psr in %o5
	andn	%o5, PSR_ET, %g1	! clear enable traps bit
	mov	%g1, %psr		! new psr
	nop; nop

	! Dragon has the requirement that all memory accesses must
	! be cached and all I/O accesses must be non-cached.
	! The 36-bit physical address space is divided into memory space
	! and I/O space (see section 3.2 of Dragon Arch):
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

#define	PAGE_MASK	0xFFF

/*
 * u_int
 * srmmu_probe_type(vaddr, type)
 *	caddr_t vaddr;
 *	int type;
 */
	ENTRY(srmmu_probe_type)
	andn	%o0, PAGE_MASK, %o0	! make sure lower 12 bits are clear
	and	%o1, 0xF, %o1		! make sure type is valid
	sll	%o1, 8, %o1		! shift type over
	or	%o0, %o1, %o0		! or type into address
	retl
	lda	[%o0]ASI_FLPR, %o0	! do the probe
	SET_SIZE(srmmu_probe_type)

/*
 * void
 * sparc_flush(addr)
 *	caddr_t addr;
 *
 * Perform sparc flush instruction on addr.  The spec says "by the
 * time five instructions subsequent to a flush have executed ..."
 * so I put in four nops.  Hey, a little paranoia doesn't hurt and
 * this way by the time this function has returned the flush is
 * guaranteed to have taken effect.
 */
	ENTRY(sparc_flush)
	flush	%o0
	nop
	nop
	nop
	retl
	nop
	SET_SIZE(sparc_flush)

#endif	/* !defined(lint) */
