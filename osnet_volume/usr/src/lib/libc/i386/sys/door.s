/ Copyright (c) 1998 by Sun Microsystems, Inc.
/ All rights reserved.

.ident "@(#)door.s	1.9	98/07/08 SMI"
	.file	"door.s"

	.bss
	.comm	__door_server_func, 4, 4
	.comm	__thr_door_server_func, 4, 4
	.comm	__door_create_pid, 4, 4

	.text
	.globl	__cerror
	.globl	_getpid
/
/ _door_create(void (*f)(void *, char *, size_t, door_desc_t *, uint_t),
/     void *cookie, u_int flag)
/
_fwdef_(`_door_create'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$0		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	16(%ebp)	/ flag	
	pushl	12(%ebp)	/ cookie
	pushl	8(%ebp)		/ f
	pushl	$0		/ dummy
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_create')

/
/ door_revoke(int d)
/
_fwdef_(`_door_revoke'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$1		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	8(%ebp)		/ d
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_revoke')

/
/ door_info(int d, door_info_t * dinfo)
/
_fwdef_(`_door_info'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$2		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	12(%ebp)	/ dinfo
	pushl	8(%ebp)		/ d
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_info')
/
/ door_cred(struct door_cred * d)
/
_fwdef_(`_door_cred'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$5		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	8(%ebp)		/ d
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_cred')


/
/ door_call(int	d, door_arg_t *arg)
/
_fwdef_(`_door_call'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$3		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	12(%ebp)	/ arg
	pushl	8(%ebp)		/ d
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_call')

/
/ door_return(
/	void 		*data_ptr,
/	int		data_size,	(in bytes)
/	door_desc_t	*door_ptr,
/	int		desc_size,	(number of desc's)
/	caddr_t		stack_base)
/
_fwdef_(`_door_return'):
	MCOUNT
	/ The base sp isn't really passed (yet)
	movl	%esp, %edx		/ Save pointer to args 
	movl	%gs:0, %eax		/ Get curthread (top of user stk)
	subl	$512, %eax		/ skip over some slop

	pushl	$4			/ syscall subcode
	pushl	%eax			/ base of user stack
	pushl	16(%edx)		/ desc size
	pushl	12(%edx)		/ desc ptr
	pushl	8(%edx)			/ data size
	pushl	4(%edx)			/ data ptr
	pushl	0(%edx)

_door_return_restart:
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	2f
	/
	/ Check if we should just repark this thread back in the kernel
	/ (should happen if the error code is EINTR and the thread is
	/ still in the same process).
	/
	cmpl	$EINTR, %eax
	jne	1f
	_prologue_
	call	_fref_(_getpid)		/ get current process id
	movl	_daref_(__door_create_pid), %edx
	movl	0(%edx), %edx
	_epilogue_
	cmpl	%eax, %edx		/ same process?
	je	_door_return_restart	/ restart
	movl	$EINTR, %eax		/ return EINTR to child of fork
1:
	/ Something bad happened during the door_return
	addl	$28, %esp
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
2:
	 /
	 / On return, we're serving a door_call. The stack looks like this:
	 /
	 /	descriptors (if any)
	 /	data (if any)
	 / sp->	struct door_results
	 /
	 / struct door_results has the arguments in place for the server proc
	 /
	movl	24(%esp), %eax
	andl	%eax, %eax	/ test nservers
	jg	3f
	/ this is the last server thread - call creation func for more
	_prologue_
	movl	_esp_(28), %eax
	pushl	%eax		/ door_info_t *
	movl	_daref_(__thr_door_server_func), %eax
	movl	0(%eax), %eax
	call	*%eax		/ call create function
	addl	$4, %esp
	_epilogue_
3:
	/ Call the door server function now
	movl	20(%esp), %eax
	call	*%eax
	/ Exit the thread if we return here
	_prologue_
	jmp	_fref_(thr_exit)
	_epilogue_
	/ NOTREACHED
	_fw_setsize_(`_door_return')

/
/ door_bind(int d)
/
_fwdef_(`_door_bind'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$6		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	8(%ebp)		/ d
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_bind')

/
/ door_unbind()
/
_fwdef_(`_door_unbind'):
	MCOUNT
	pushl	%ebp
	movl	%esp, %ebp
	pushl	$7		/ syscall subcode
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy	
	pushl	$0		/ dummy
	pushl	$0		/ dummy	
	movl	$DOOR, %eax
	lcall   $SYSCALL_TRAPNUM, $0
	jae	1f
	addl	$28, %esp
	leave
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	addl	$28, %esp
	leave
	ret
	_fw_setsize_(`_door_unbind')

