	.file	"cuexit.s"

	.ident	"@(#)cuexit.s	1.10	99/10/07 SMI"

/ C library -- exit
/ exit(code)
/ code is return in %edx to system


	.globl	exit
	.globl	__exit_frame_monitor
	.align	4

_fgdef_(exit):
	MCOUNT
	_prologue_
_m4_ifdef_(`DSHLIB',
`	movl	_daref_(__exit_frame_monitor),%ecx
	movl	%ebp,(%ecx)
',
	movl	%ebp,__exit_frame_monitor
')
	call	_fref_(_exithandle)
	movl	_esp_(4),%edx
	_epilogue_
	movl	$EXIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',

	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	_fg_setsize_(`exit')
