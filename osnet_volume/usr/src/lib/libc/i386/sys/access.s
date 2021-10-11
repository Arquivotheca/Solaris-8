.ident	"@(#)access.s	1.8	98/07/08 SMI"

/ access(file, request)
/ test ability to access file in all indicated ways
/ 1 - read
/ 2 - write
/ 4 - execute

	.file	"access.s"

	.text

/ access(file, request)
/ test ability to access file in all indicated ways
/ 1 - read
/ 2 - write
/ 4 - execute


	.globl  __cerror

_fwdef_(`access'):
	MCOUNT
	movl	$ACCESS,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)
noerror:
	ret
	_fw_setsize_(`access')
