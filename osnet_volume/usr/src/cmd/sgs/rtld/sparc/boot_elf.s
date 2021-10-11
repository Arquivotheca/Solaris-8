/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)boot_elf.s	1.33	99/05/27 SMI"

#include	<link.h>
#include	"machdep.h"
#include	"profile.h"
#include	"_audit.h"

#if	defined(lint)
#include	<sys/types.h>
#include	"_rtld.h"
#else
#include	<sys/stack.h>

	.file	"boot_elf.s"
	.seg	".text"
#endif

/*
 * We got here because the initial call to a function resolved to a procedure
 * linkage table entry.  That entry did a branch to the first PLT entry, which
 * in turn did a call to elf_rtbndr (refer elf_plt_init()).
 *
 * the code sequence that got us here was:
 *
 * PLT entry for foo():
 *	sethi	(.-PLT0), %g1			! not changed by rtld
 *	ba,a	.PLT0				! patched atomically 2nd
 *	nop					! patched first
 *
 * Therefore on entry, %i7 has the address of the call, which will be added
 * to the offset to the plt entry in %g1 to calculate the plt entry address
 * we must also subtract 4 because the address of PLT0 points to the
 * save instruction before the call.
 *
 * the plt entry is rewritten:
 *
 * PLT entry for foo():
 *	sethi	(.-PLT0), %g1
 *	sethi	%hi(entry_pt), %g1
 *	jmpl	%g1 + %lo(entry_pt), %g0
 */

#if	defined(lint)

extern unsigned long	elf_bndr(caddr_t, unsigned long, Rt_map *, caddr_t);

static void
elf_rtbndr(caddr_t pc, unsigned long reloc, Rt_map * lmp, caddr_t from)
{
	(void) elf_bndr(pc, reloc, lmp, from);
}


#else
	.weak	_elf_rtbndr		! keep dbx happy as it likes to
	_elf_rtbndr = elf_rtbndr	! rummage around for our symbols

	.global	elf_rtbndr
	.type   elf_rtbndr, #function
	.align	4

elf_rtbndr:
	mov	%i7, %o0		! Save callers address(profiling)
	save	%sp, -SA(MINFRAME), %sp	! Make a frame
	mov	%i0, %o3		! Callers address is arg 4
	add	%i7, -0x4, %o0		! %o0 now has address of PLT0
	srl	%g1, 10, %g1		! shift offset set by sethi
	add	%o0, %g1, %o0		! %o0 has the address of jump slot
	mov	%g1, %o1		! %o1 has offset from jump slot
					! to PLT0 which will be used to
					! calculate plt relocation entry
					! by elf_bndr
	call	elf_bndr		! returns function address in %o0
	ld	[%i7 + 8], %o2		! %o2 has ptr to lm
	mov	%o0, %g1		! save address of routine binded
	restore				! how many restores needed ? 2
	jmp	%g1			! jump to it
	restore
	.size 	elf_rtbndr, . - elf_rtbndr

#endif


#if defined(lint)
void
iflush_range(caddr_t addr, size_t len)
{
	caddr_t i;
	for (i = addr; i <= addr + len; i += 4)
		/* iflush i */;
}
#else
	.global	iflush_range
	.type	iflush_range, #function
	.align	4

iflush_range:
	save	%sp, -SA(MINFRAME), %sp	! Make a frame
	mov	%i0,%l0
	add	%i0,%i1, %l1
1:
	cmp	%l0, %l1
	bg	2f
	nop
	iflush	%l0
	ba	1b
	add	%l0,0x4,%l0
2:
	ret
	restore
	.size	iflush_range, . - iflush_range
#endif

/*
 * Initialize the first plt entry so that function calls go to elf_rtbndr
 *
 * The first plt entry (PLT0) is:
 *
 *	save	%sp, -64, %sp
 *	call	elf_rtbndr
 *	nop
 *	address of lm
 */

#if	defined(lint)

void
elf_plt_init(unsigned int * plt, caddr_t lmp)
{
	*(plt + 0) = (unsigned long) M_SAVESP64;
	*(plt + 4) = M_CALL | (((unsigned long)elf_rtbndr -
			((unsigned long)plt)) >> 2);
	*(plt + 8) = M_NOP;
	*(plt + 12) = (unsigned long) lmp;
}

#else
	.global	elf_plt_init
	.type	elf_plt_init, #function
	.align	4

elf_plt_init:
	save	%sp, -SA(MINFRAME), %sp	! Make a frame
1:
	call	2f
	sethi	%hi((_GLOBAL_OFFSET_TABLE_ - (1b - .))), %l7
2:
	sethi	%hi(M_SAVESP64), %o0	! Get save instruction
	or	%o0, %lo(M_SAVESP64), %o0
	or	%l7, %lo((_GLOBAL_OFFSET_TABLE_ - (1b - .))), %l7
	st	%o0, [%i0]		! Store in plt[0]
	iflush	%i0
	add	%l7, %o7, %l7
	ld	[%l7 + elf_rtbndr], %l7
	inc	4, %i0			! Bump plt to point to plt[1]
	sub	%l7, %i0, %o0		! Determine -pc so as to produce
					! offset from plt[1]
	srl	%o0, 2, %o0		! Express offset as number of words
	sethi	%hi(M_CALL), %o4	! Get sethi instruction
	or	%o4, %o0, %o4		! Add elf_rtbndr address
	st	%o4, [%i0]		! Store instruction in plt
	iflush	%i0
	sethi	%hi(M_NOP), %o0		! Generate nop instruction
	st	%o0, [%i0 + 4]		! Store instruction in plt[2]
	iflush	%i0 + 4
	st	%i1, [%i0 + 8]		! Store instruction in plt[3]
	iflush	%i0 + 8
	ret
	restore
	.size	elf_plt_init, . - elf_plt_init
#endif

#if	defined(lint)

ulong_t
elf_plt_trace()
{
	return (0);
}
#else
	.global	elf_plt_trace
	.type   elf_plt_trace, #function
	.align	4

/*
 * The dyn_plt that called us has already created a stack-frame for
 * us and placed the following entries in it:
 *
 *	[%fp - 0x4]	* dyndata
 *	[%fp - 0x8]	* prev stack size
 *
 * dyndata currently contains:
 *
 *	dyndata:
 *		uintptr_t	*reflmp
 *		uintptr_t	*deflmp
 *		ulong_t		symndx
 *		ulong_t		sb_flags
 *		Sym		symdef
 */

elf_plt_trace:
1:	call	2f
	sethi	%hi(_GLOBAL_OFFSET_TABLE_+(.-1b)), %l7
2:	or	%l7, %lo(_GLOBAL_OFFSET_TABLE_+(.-1b)), %l7
	add	%l7, %o7, %l7

	ld	[%l7+audit_flags], %l3
	ld	[%l3], %l3		! %l3 = audit_flags
	andcc	%l3, AF_PLTENTER, %g0
	beq	.end_pltenter
	ld	[%fp + -0x4], %l1	! l1 = * dyndata
	ld	[%l1 + 0xc], %l2	! l2 = sb_flags
	andcc	%l2, LA_SYMB_NOPLTENTER, %g0
	beq	.start_pltenter
	ld	[%l1+0x10], %l0		! l0 = sym.st_value(calling address)
	ba	.end_pltenter
	nop

	/*
	 * save all registers into La_sparcv8_regs
	 */
.start_pltenter:
	sub	%sp, 0x20, %sp		! create space for La_sparcv8_regs
					! storage on the stack.

	sub	%fp, 0x28, %o4		

	st	%i0, [%o4]
	st	%i1, [%o4 + 0x4]
	st	%i2, [%o4 + 0x8]
	st	%i3, [%o4 + 0xc]	! because a regwindow shift has
	st	%i4, [%o4 + 0x10]	! already occured our current %i*
	st	%i5, [%o4 + 0x14]	! register's are the equivalent of
	st	%i6, [%o4 + 0x18]	! the %o* registers that the final
	st	%i7, [%o4 + 0x1c]	! procedure shall see.

	ld	[%fp + -0x4], %l1	! %l1 == * dyndata
	ld	[%l1], %o0		! %o0 = reflmp
	ld	[%l1 + 0x4], %o1	! %o1 = deflmp
	add	%l1, 0x10, %o2		! %o2 = symp
	ld	[%l1 + 0x8], %o3	! %o3 = symndx
	call	audit_pltenter
	add	%l1, 0xc, %o5		! %o3 = * sb_flags

	mov	%o0, %l0		! %l0 == calling address

	add	%sp, 0x20, %sp		! cleanup La_sparcv8_regs off
					! of the stack.

.end_pltenter:
	/*
	 * If *no* la_pltexit() routines exist we do not need to keep the
	 * stack frame before we call the actual routine.  Instead we jump to
	 * it and remove our self from the stack at the same time.
	 */
	ld	[%l7+audit_flags], %l3
	ld	[%l3], %l3		! %l3 = audit_flags
	andcc	%l3, AF_PLTEXIT, %g0
	beq	.bypass_pltexit
	ld	[%fp + -0x4], %l1	! %l1 = * dyndata
	ld	[%l1 + 0xc], %l2	! %l2 = sb_flags
	andcc	%l2, LA_SYMB_NOPLTEXIT, %g0
	bne	.bypass_pltexit
	nop

	ba	.start_pltexit
	nop
.bypass_pltexit:
	jmpl	%l0, %g0
	restore

.start_pltexit:
	/*
	 * In order to call la_pltexit() we must duplicate the
	 * arguments from the 'callers' stack on our stack frame.
	 *
	 * First we check the size of the callers stack and grow
	 * our stack to hold any of the arguments.  That need
	 * duplicating (these are arguments 6->N), because the
	 * first 6 (0->5) are passed via register windows on sparc.
	 */

	/*
	 * The first calculation is to determine how large the
	 * argument passing area might be.  Since there is no
	 * way to distinquish between 'argument passing' and
	 * 'local storage' from the previous stack this amount must
	 * cover both.
	 */
	ld	[%fp + -0x8], %l1	! %l1 = callers stack size
	sub	%l1, 0x58, %l1		! %l1 = argument space on caller's
					!	stack
	/*
	 * Next we compare the prev. stack size against the audit_argcnt.
	 * We copy at most 'audit_argcnt' arguments.
	 *
	 * NOTE: on sparc we always copy at least six args since these
	 *	 are in reg-windows and not on the stack.
	 *
	 * NOTE: Also note that we multiply (shift really) the arg count
	 *	 by 4 which is the 'word size' to calculate the amount
	 *	 of stack space needed.
	 */
	ld	[%l7 + audit_argcnt], %l2
	ld	[%l2], %l2		! %l2 = audit_arg_count
	cmp	%l2, 6
	ble	.grow_stack
	sub	%l2, 6, %l2
	sll	%l2, 2, %l2
	cmp	%l1, %l2
	ble	.grow_stack
	nop
	mov	%l2, %l1
.grow_stack:
	/*
	 * When duplicating the stack we skip the first '0x5c' bytes.
	 * This is the space on the stack reserved for preserving
	 * the register windows and such and do not need to be duplicated
	 * on this new stack frame.  We start duplicating at the
	 * portion of the stack reserved for argument's above 6.
	 */
	sub	%sp, %l1, %sp		! grow our stack by amount required.
	sra	%l1, 0x2, %l1		! %l1 = %l1 / 4 (words to copy)
	mov	0x5c, %l2		! %l2 = index into stack & frame

1:
	cmp	%l1, 0
	ble	2f
	nop
	ld	[%fp + %l2], %l3	! duplicate args from previous
	st	%l3, [%sp + %l2]	! stack onto current stack
	add	%l2, 0x4, %l2
	ba	1b
	sub	%l1, 0x1, %l1
2:
	mov	%i0, %o0		! copy ins to outs
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	call	%l0			! call routine
	mov	%i5, %o5
	mov	%o1, %l2		! l2 = second 1/2 of return value
					! for those those 64 bit operations
					! link div64 - yuck...

					! %o0 = retval
	ld	[%fp + -0x4], %l1
	ld	[%l1], %o1		! %o1 = reflmp
	ld	[%l1 + 4], %o2		! %o2 = deflmp
	add	%l1, 0x10, %o3		! %o3 = symp
	call	audit_pltexit
	ld	[%l1 + 0x8], %o4	! %o4 = symndx

	mov	%o0, %i0		! pass on return code
	mov	%l2, %i1
	ret
	restore
	.size	elf_plt_trace, . - elf_plt_trace

#endif

/*
 * After the first call to a plt, elf_bndr() will have determined the true
 * address of the function being bound.  The plt is now rewritten so that
 * any subsequent calls go directly to the bound function.  If the library
 * to which the function belongs is being profiled refer to _plt_cg_write.
 *
 * the new plt entry is:
 *
 *	sethi	(.-PLT0), %g1			! constant
 *	sethi	%hi(function address), %g1	! patched second
 *	jmpl	%g1 + %lo(function address, %g0	! patched first
 */

#if	defined(lint)

void
/* ARGSUSED2 */
elf_plt_write(unsigned long * pc, unsigned long * symval, unsigned long * gp)
{
	*(pc + 8) = (M_JMPL | ((unsigned long)symval & S_MASK(10)));
	*(pc + 4) = (M_SETHIG1 | ((unsigned long)symval >> (32 - 22)));
}

#else
	.global	elf_plt_write
	.type	elf_plt_write, #function
	.align	4

elf_plt_write:
	sethi	%hi(M_JMPL), %o3	! Get jmpl instruction
	and	%o1, 0x3ff, %o2		! Lower part of function address
	or	%o3, %o2, %o3		!	is or'ed into instruction
	st	%o3, [%o0 + 8]		! Store instruction in plt[2]
	iflush	%o0 + 8
	stbar
	srl	%o1, 10, %o1		! Get high part of function address
	sethi	%hi(M_SETHIG1), %o3	! Get sethi instruction
	or	%o3, %o1, %o3		! Add sethi and function address
	st	%o3, [%o0 + 4]		! Store instruction in plt[1]
	retl
	iflush	%o0 + 4
	.size 	elf_plt_write, . - elf_plt_write

#endif

