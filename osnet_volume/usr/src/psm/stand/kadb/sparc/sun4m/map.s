/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)map.s	1.7	96/02/27 SMI" /* From SunOS 4.1.1 */

/*
 * Additional memory mapping routines for use by standalone debugger,
 * setpgmap(), getpgmap() are taken from the boot code.
 */

#include "assym.s"
#include <sys/param.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/privregs.h>
#include <sys/enable.h>
#include <sys/cpu.h>
#include <sys/eeprom.h>
#include <sys/asm_linkage.h>
#include <sys/debug/debug.h>

#if !defined(lint)

	.seg	".text"
	.align	4

#endif

#if defined(lint)
/*ARGSUSED*/
int
ldphys(int paddr)
{ return (0); }
#else	/* defined(lint) */
        ENTRY(ldphys)
	sethi	%hi(mxcc), %o2
	ld	[%o2+%lo(mxcc)], %o2
	tst	%o2			! do we have an mxcc?
	bz,a	1f			! no, do ld and return
	lda	[%o0]ASI_MEM, %o0
	lda	[%g0]ASI_MOD, %o2
	set	CPU_VIK_TC, %o3
	andcc	%o2, %o3, %g0		! is the TC bit on?
	bz,a	1f			! no, do ld and return
	lda	[%o0]ASI_MEM, %o0
	!
	! disable intrs and set AC for the ld
	!
	mov	%psr, %o3		! old psr -> %o3
	or	%o3, PSR_PIL, %o4
	mov	%o4, %psr		! intrs disabled
	nop; nop; nop
	set	CPU_VIK_AC, %o4
	or	%o2, %o4, %o4		! %o2 still has mcr
	sta	%o4, [%g0]ASI_MOD	! cached asis now enabled
	lda	[%o0]ASI_MEM, %o0	! do ld
	sta	%o2, [%g0]ASI_MOD	! restore mcr
	mov	%o3, %psr		! restore psr
	nop; nop; nop
1:
        retl
	nop
	SET_SIZE(ldphys)
#endif	/* !defined(lint) */

#if defined(lint)
/*ARGSUSED*/
void
stphys(int paddr, int data)
{ return; }
#else
        ENTRY(stphys)
	sethi	%hi(mxcc), %o2
	ld	[%o2+%lo(mxcc)], %o2
	tst	%o2			! do we have an mxcc?
	bz,a	1f			! no, do st and return
        sta     %o1, [%o0]ASI_MEM
	lda	[%g0]ASI_MOD, %o2
	set	CPU_VIK_TC, %o3
	andcc	%o2, %o3, %g0		! is the TC bit on?
	bz,a	1f			! no, do st and return
        sta     %o1, [%o0]ASI_MEM
	!
	! disable intrs and set AC for the st
	!
	mov	%psr, %o3		! old psr -> %o3
	or	%o3, PSR_PIL, %o4
	mov	%o4, %psr		! intrs disabled
	nop; nop; nop
	set	CPU_VIK_AC, %o4
	or	%o2, %o4, %o4		! %o2 still has mcr
	sta	%o4, [%g0]ASI_MOD	! cached asis now enabled
        sta     %o1, [%o0]ASI_MEM	! do st
	sta	%o2, [%g0]ASI_MOD	! restore mcr
	mov	%o3, %psr		! restore psr
	nop; nop; nop
1:
        retl
	nop
	SET_SIZE(stphys)
#endif	/* defined(lint) */

#if defined(lint)
/*ARGSUSED*/
void
iflush(int addr)
{ return; }
#else
	ENTRY(iflush)
	retl
	iflush	%o0
	SET_SIZE(iflush)
#endif	/* defined(lint) */
