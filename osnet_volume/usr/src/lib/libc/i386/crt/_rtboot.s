	.ident	"@(#)_rtboot.s	1.4	96/06/01 SMI"

/ bootstrap routine for run-time linker
/ we get control from exec which has loaded our text and
/ data into the process' address space and created the process 
/ stack
/
/ on entry, the process stack looks like this:
/
/			# <- %esp
/_______________________#  high addresses
/	strings		#  
/_______________________#
/	0 word		#
/_______________________#
/	Auxiliary	#
/	entries		#
/	...		#
/	(size varies)	#
/_______________________#
/	0 word		#
/_______________________#
/	Environment	#
/	pointers	#
/	...		#
/	(one word each)	#
/_______________________#
/	0 word		#
/_______________________#
/	Argument	# low addresses
/	pointers	#
/	Argc words	#
/_______________________#
/	argc		# 
/_______________________# <- %ebp

	.set	EB_NULL,0
	.set	EB_DYNAMIC,1
	.set	EB_LDSO_BASE,2
	.set	EB_ARGV,3
	.set	EB_ENVP,4
	.set	EB_AUXV,5
	.set	EB_DEVZERO,6
	.set	EB_PAGESIZE,7
	.set	EB_MAX,8

	.text
	.globl	__rtboot
	.globl	__rtld
	.type	__rtboot,@function
	.align	4
__rtboot:
	movl	%esp,%ebp
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
.L00:	addl	$4,%edi			/ next
	cmpl	$0,(%edi)		/ search for 0 at end of env
	jne	.L00
	addl	$4,%edi			/ advance past 0
	movl	$EB_AUXV,16(%esi)	/ set up tag for auxv
	movl	%edi,20(%esi)		/ point to auxv
	movl	$EB_NULL,24(%esi)	/ set up NULL tag
	call	.L01		/ only way to get IP into a register
.L01:	popl	%ebx		/ pop the IP we just "pushed"
	leal	[s.EMPTY - .L01](%ebx),%eax
	pushl	%eax
	leal	[s.ZERO - .L01](%ebx),%eax
	pushl	%eax
	leal	[s.LDSO - .L01](%ebx),%eax
	pushl	%eax
	movl	%esp,%edi	/ save pointer to strings
	leal	[f.MUNMAP - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.CLOSE - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.SYSCONFIG - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.FSTAT - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.MMAP - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.OPEN - .L01](%ebx),%eax
	pushl	%eax
	leal	[f.PANIC - .L01](%ebx),%eax
	pushl	%eax
	movl	%esp,%ecx	/ save pointer to functions

	pushl	%ecx		/ address of functions
	pushl	%edi		/ address of strings
	pushl	%esi		/ &eb[0]
	call	__rtld		/ __rtld(&eb[0], strings, funcs)
	movl	%esi,%esp	/ restore the stack (but leaving boot vector)
	jmp	*%eax 		/ transfer control to ld.so.1
	.size	__rtboot,.-__rtboot

	.align	4
s.LDSO:		.string	"/usr/lib/ld.so.1"
s.ZERO:		.string	"/dev/zero"
s.EMPTY:	.string	"(null)"
s.ERROR:	.string	": no (or bad) /usr/lib/ld.so.1\n"
l.ERROR:

	.align	4
f.PANIC:
	movl	%esp,%ebp
/ Add using of argument string
	pushl	$[l.ERROR - s.ERROR]
	call	.L02
.L02:	popl	%ebx
	leal	[s.ERROR - .L02](%ebx),%eax
	pushl	%eax
	pushl	$2
	call	f.WRITE
	jmp	f.EXIT
/ Not reached
	
f.OPEN:
	movl	$OPEN,%eax
	jmp	__syscall
f.MMAP:
	movl	$MMAP,%eax
	jmp	__syscall
f.MUNMAP:
	movl	$MUNMAP,%eax
	jmp	__syscall
f.READ:
	movl	$READ,%eax
	jmp	__syscall
f.WRITE:
	movl	$WRITE,%eax
	jmp	__syscall
f.LSEEK:
	movl	$LSEEK,%eax
	jmp	__syscall
f.CLOSE:
	movl	$CLOSE,%eax
	jmp	__syscall
f.FSTAT:
	movl	$FXSTAT,%eax	/ NEEDSWORK: temp kludge for G6
	jmp	__syscall
f.SYSCONFIG:
	movl	$SYSCONFIG,%eax
	jmp	__syscall
f.EXIT:
	movl	$EXIT,%eax
/	jmp	__syscall
__syscall:
	lcall   $SYSCALL_TRAPNUM,$0
	jc	__err_exit
	ret
__err_exit:
	movl	$-1,%eax
	ret
