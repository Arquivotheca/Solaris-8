/
/	Copyright(c) 1994, by Sun Microsytems, Inc.
/
/
/ The following c routine has appropriate interface semantics
/ for the chaining combination template.  On the sparc architecture
/ the assembly routine is further tuned to make it tail recursive.
/
/ void
/ prb_chain_entry(void *a, void *b, void *c)
/ {
/ 	prb_chain_down(a, b, c);
/ 	prb_chain_next(a, b, c);
/ }

	.ident	"@(#)i386_assm.s 1.2	96/05/09 SMI"
	.file	"i386_assm.s"
	.data
	.align	4
	.globl	prb_callinfo
prb_callinfo:
	.4byte	1		/ offset
	.4byte	0		/ shift right
	.4byte	0xffffffff	/ mask

	.text
	.align	4
	.globl	prb_chain_entry
	.globl	prb_chain_down
	.local	chain_down
	.globl	prb_chain_next
	.local	chain_next
	.globl	prb_chain_end
prb_chain_entry:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	16(%ebp), %ebx
	pushl	%ebx
	movl	12(%ebp), %edi
	pushl	%edi
	movl	8(%ebp), %esi
	pushl	%esi
prb_chain_down:
chain_down:
	call	chain_down
	addl	$12, %esp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
prb_chain_next:
chain_next:
	call	chain_next
	addl	$12, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
prb_chain_end:
	nop
