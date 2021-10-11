/*
 * Copyright (c) 1986, 1991, by Sun Microsystems, Inc.
 */


#ident "@(#)_setjmp.s	1.3	93/08/31 SMI"

#if defined(lint)

#include <sys/debug/debugger.h>

#endif	/* lint */

/*
 * _setjmp( buf_ptr )
 * buf_ptr points to a six word array (typedef jmp_buf).
 *
 *		+----------------+
 *   0->	|      edi       |  0
 *		+----------------+
 *		|      esi       |  4
 *		+----------------+
 *		|      ebx       |  8
 *		+----------------+
 *		|      ebp       | 12
 *		+----------------+
 *		|      esp       | 16
 *		+----------------+
 *		|      eip       | 20
 *		+----------------+------  (sizeof)jmp_buf = 24 bytes
 */

#if defined(lint)

/* ARGSUSED */
int
_setjmp(jmp_buf_ptr buf_ptr)
{ return (0); }

#else	/* lint */

	.text

	.align	4
	.globl	_setjmp
_setjmp:
	movl	4(%esp), %edx		/ address of save area
	movl	%edi, (%edx)
	movl	%esi, 4(%edx)
	movl	%ebx, 8(%edx)
	movl	%ebp, 12(%edx)
	movl	%esp, 16(%edx)
	movl	(%esp), %ecx		/ %eip (return address)
	movl	%ecx, 20(%edx)
	subl	%eax, %eax		/ retval <- 0
	ret

#endif	/* lint */

/*
 * _longjmp ( buf_ptr , val)
 * buf_ptr points to an array which has been initialized by _setjmp.
 *
 * NOTE: the original sparc version of this call used a second argument,
 * (val) which was the value returned to _setjmp's caller.  Our INTEL
 * version returns  val.
 *
 */

#if defined(lint)

/* ARGSUSED */
void
_longjmp(jmp_buf_ptr buf_ptr, int val)
{}

#else	/* lint */

	.align	4
	.globl	_longjmp
_longjmp:
	movl	4(%esp), %edx		/ address of save area
	movl	8(%esp), %eax		/ return value
	movl	(%edx), %edi
	movl	4(%edx), %esi
	movl	8(%edx), %ebx
	movl	12(%edx), %ebp
	movl	16(%edx), %esp
	movl	20(%edx), %ecx		/ %eip (return address)
	addl	$4, %esp		/ pop ret adr
	jmp	*%ecx			/ indirect

#endif	/* lint */
