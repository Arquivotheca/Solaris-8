#pragma ident	"@(#)boot_1275entry.s	1.1	97/06/30 SMI"

/*
 * This is the entry point passed to a client of boot services. The client
 * jumps here with  %o0 containing a pointer to an array of boot_cell_t
 * arguments.
 *
 * This routine is compiled in with boot_1275 implementations, and adjusts the
 * stack to match the compilation model used.
 */

#include <sys/asm_linkage.h>
#include <sys/privregs.h>
#include <sys/stack.h>

#if defined(lint)

extern int boot1275_entry(void *);

int
boot1275_entry_asm(void *p)
{
	return (boot1275_entry(p));
}


extern void boot_fail_gracefully(void);

void
boot_fail_gracefully_asm(void)
{
	boot_fail_gracefully();
}

#else

	.seg	".text"
	.align	4

	ENTRY(boot1275_entry_asm)
	andcc	%sp, 1, %g0			! 32-bit or 64-bit caller?
	bnz	6f				! Odd means a 64-bit caller
	nop					! Even means a 32-bit caller

	/* 32-bit caller: XXX: Assumes PSTATE.AM is set */

	save	%sp, -SA(MINFRAME), %sp
	srl	%i0, 0, %o0			! zero extend the arg array.
	srl	%sp, 0, %sp
1:	rdpr	%pstate, %l1			! Get the present pstate value
	andn	%l1, PSTATE_AM, %l2
	wrpr	%l2, 0, %pstate			! Set PSTATE.AM to zero
	setn	boot1275_entry, %g1, %o1	! Call boot service with ...
	jmpl	%o1, %o7			! ... arg array ptr in %o0
	sub	%sp, STACK_BIAS, %sp		! delay: Now a 64-bit frame
	add	%sp, STACK_BIAS, %sp		! back to a 32-bit frame
	wrpr	%l1, 0, %pstate			! Set PSTATE.AM to one.
	ret					! Return ...
	restore	%o0, %g0, %o0			! ... result in %o0

6:
	/*
	 * 64-bit caller:
	 * Just jump to boot-services with the arg array in %o0.
	 * boot-services will return to the original 64-bit caller
	 */
	set	boot1275_entry, %o1
	jmp	%o1
	nop
	/* NOTREACHED */
	SET_SIZE(boot1275_entry_asm)

	.global boot_fail_gracefully
	.type   boot_fail_gracefully, #function

	ENTRY(boot_fail_gracefully_asm)
	/* 32-bit caller: XXX: Assumes PSTATE.AM is set */

	save	%sp, -SA(MINFRAME), %sp
	sub	%sp, STACK_BIAS, %sp		! delay: Now a 64-bit frame
	rdpr	%pstate, %l1			! Get the present pstate value
	andn	%l1, PSTATE_AM, %l2
	wrpr	%l2, 0, %pstate			! Set PSTATE.AM to zero

	call    boot_fail_gracefully
	nop
	SET_SIZE(boot_fail_gracefully_asm)
#endif
