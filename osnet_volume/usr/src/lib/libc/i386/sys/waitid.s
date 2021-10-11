	.ident	"@(#)waitid.s	1.10	98/07/08 SMI"

/ C library -- waitid

/ error = waitid(idtype,id,&info,options)

	.file	"waitid.s"
	
	.text

	.globl  __cerror

_fwpdef_(`_waitid', `_libc_waitid'):
	MCOUNT
	movl	$WAITID,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae 	noerror		/ all OK - normal return
	cmpb	$ERESTART,%al	/  else, if ERRESTART
	je	_waitid		/    then loop
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp 	_fref_(__cerror)	/  otherwise, error

noerror:
	ret
	_fwp_setsize_(`_waitid', `_libc_waitid')
