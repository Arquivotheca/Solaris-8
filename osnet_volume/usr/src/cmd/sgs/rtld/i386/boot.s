/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)boot.s	1.11	98/11/13 SMI"

/*
 * Bootstrap routine for run-time linker.
 * We get control from exec which has loaded our text and
 * data into the process' address space and created the process
 * stack.
 *
 * On entry, the process stack looks like this:
 *
 *	#			# <- %esp
 *	#_______________________#  high addresses
 *	#	strings		#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Auxiliary	#
 *	#	entries		#
 *	#	...		#
 *	#	(size varies)	#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Environment	#
 *	#	pointers	#
 *	#	...		#
 *	#	(one word each)	#
 *	#_______________________#
 *	#	0 word		#
 *	#_______________________#
 *	#	Argument	# low addresses
 *	#	pointers	#
 *	#	Argc words	#
 *	#_______________________#
 *	#	argc		#
 *	#_______________________# <- %ebp
 *
 *
 * We must calculate the address at which ld.so was loaded,
 * find the addr of the dynamic section of ld.so, of argv[0], and  of
 * the process' environment pointers - and pass the thing to _setup
 * to handle.  We then call _rtld - on return we jump to the entry
 * point for the a.out.
 */

#if	defined(lint)

extern	unsigned long	_setup();
extern	void		atexit_fini();
void
main()
{
	(void) _setup();
	atexit_fini();
}

#else

#include	<link.h>

	.file	"boot.s"
	.text
	.globl	_rt_boot
	.globl	_setup
	.globl	_GLOBAL_OFFSET_TABLE_
	.type	_rt_boot,@function
	.align	4

_rt_alias:
	jmp	.get_ip			/ in case we were invoked from libc.so
_rt_boot:
	movl	%esp,%ebp		/ save for referencing args
	subl	$[8 \* EB_MAX],%esp	/ make room for a max sized boot vector
	movl	%esp,%esi		/ use esi as a pointer to &eb[0]
	movl	$EB_ARGV,0(%esi)	/ set up tag for argv
	leal	4(%ebp),%eax		/ get address of argv
	movl	%eax,4(%esi)		/ put after tag
	movl	$EB_ENVP,8(%esi)	/ set up tag for envp
	movl	(%ebp),%eax		/ get # of args
	addl	$2,%eax			/ one for the zero & one for argc
	leal	(%ebp,%eax,4),%edi	/ now points past args & @ envp
	movl	%edi,12(%esi)		/ set envp
.L0:	addl	$4,%edi			/ next
	cmpl	$0,-4(%edi)		/ search for 0 at end of env
	jne	.L0
	movl	$EB_AUXV,16(%esi)	/ set up tag for auxv
	movl	%edi,20(%esi)		/ point to auxv
	movl	$EB_NULL,24(%esi)	/ set up NULL tag
.get_ip:
	call	.L1			/ only way to get IP into a register
.L1:
	popl	%ebx			/ pop the IP we just "pushed"
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L1],%ebx
	pushl	(%ebx)			/ address of dynamic structure
	pushl	%esi			/ push &eb[0]

	call	_setup@PLT		/ _setup(&eb[0], _DYNAMIC)
	movl	%ebp,%esp		/ release stack frame

	movl	atexit_fini@GOT(%ebx), %edx
	jmp	*%eax 			/ transfer control to a.out
	.size	_rt_boot,.-_rt_boot

#endif
