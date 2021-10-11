	
.ident	"@(#)getcontext.s	1.2	98/07/08 SMI"

	.file	"getcontext.s"

	.text

/ getcontext() is written in assembler since it has to capture the correct
/ machine state of the caller, including the registers: %edi, %esi and %ebx.
/
/ If getcontext() were to be written in C, these registers would be pushed on
/ the stack by the C compiler as follows:
/
/_getcontext:    pushl  %ebp
/_getcontext+1:  movl   %esp,%ebp
/_getcontext+3:  pushl  %edi
/_getcontext+4:  pushl  %esi
/_getcontext+5:  pushl  %ebx		===> caller's %ebx is at top of stack
/_getcontext+6:  call   _getcontext+0xb <0x8003cf97>
/_getcontext+0xb:        popl   %ebx
/_getcontext+0xc:        addl   $0x4ff8d,%ebx

/ Now, the SVR4 ABI does not guarantee the exact placement of %edi, %esi, %ebx
/ on the stack. So, the routine cannot reliably extract these registers off the
/ stack, if it were written in C.
/
/ However, if it is written in assembler, %edi, %esi and %ebp are not touched,
/ since there is no C prologue - and so the syscall to getcontext() will save
/ the caller's values into the ucontext. Then, %ebx (GOT pointer), %eip and
/ %esp need to be fixed up in the ucontext.
/ 
/ Note that UC_ALL is always passed in as the safe, conservative setting for
/ the uc_flags field, to obtain all context information from the getcontext()
/ system call. Although this may return more context information than the
/ caller wanted, it is safe.
/
/ getcontext(ucontext_t *ucp)

_fwdef_(`getcontext'):
	MCOUNT
	movl	4(%esp),%eax	/ %eax <-- first arg: ucp
	_prologue_		/ for PIC code, compute GOT in %ebx
	movl    $UC_ALL,(%eax)	/ ucp->uc_flags = UC_ALL
	pushl   %eax		/ push ucp for system call
	call    _fref_(__getcontext) / call __getcontext: syscall
	addl    $4,%esp		/ pop arg
	andl   %eax,%eax	/ if (error_return_from_syscall)
	_epilogue_		/ 		if PIC code, pop %ebx
	jne	.L2		/ 	then return 
	movl    4(%esp),%eax	/ recompute first arg

	/ now fix up %ebx, %esp, and %eip; 
	/ %edi, %esi and %ebp are untouched here since
	/ this is in assembler
 	/ first, cpup = (greg_t *)&ucp->uc_mcontext.gregs;

        leal    [UC_MCONTEXT](%eax),%edx / %edx <-- &ucp->uc_mcontext.gregs
	movl	0(%esp),%eax		/ read return PC from stack
	movl	%eax,EIP\*4(%edx)	/ store ret PC in EIP of env var
	movl    %ebx,EBX\*4(%edx)	/ store caller's %ebx (unchanged)
	leal	4(%esp),%eax		/ get caller's sp at time of call
	movl	%eax,UESP\*4(%edx)	/ store this sp into UESP of env var
	xorl	%eax,%eax		/ return 0
	movl	%eax,EAX\*4(%edx)	/ getcontext returns 0 after a setcntext
.L2:
	ret
	_fw_setsize_(`getcontext')
