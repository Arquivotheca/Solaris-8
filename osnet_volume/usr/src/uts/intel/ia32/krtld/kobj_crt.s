/*
 * Copyright (c) 1986-1992,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj_crt.s	1.4	99/05/04 SMI"

/*
 * exit routine from linker/loader to kernel
 */

#include <sys/asm_linkage.h>


/*
 *  exitto is called from main() and does 1 things
 *	It then jumps directly to the just-loaded standalone.
 *	There is NO RETURN from exitto(). ????
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* lint */

save_esp2:
        .long   0

	ENTRY(exitto)
	/.globl exitto
/exitto:
	push	%ebp			/ save stack
	mov	%esp,%ebp
	pushal				/ protect secondary boot

	movl	%esp, %eax
	movl	%eax, save_esp2

	/holds address of array of pointers to functions
	movl	$romp, %eax
	movl    (%eax), %ecx

	/holds address of bootops structure
	movl	$bopp, %eax
	movl    (%eax), %ebx

	/hold address of debug vector
	movl	$dbvec, %eax
	movl	(%eax), %edx

	movl	8(%ebp), %eax		
	call   *%eax

	movl	save_esp2, %eax
	movl	%eax, %esp

	popal
	pop	%ebp			/ restore frame pointer

	ret
	SET_SIZE(exitto)
#endif
