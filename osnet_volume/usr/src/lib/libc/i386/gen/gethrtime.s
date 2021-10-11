/
/ Copyright (c) 1994 by Sun Microsystems, Inc.
/

	.ident	"@(#)gethrtime.s	1.4	98/07/08 SMI"

	.file	"gethrtime.s"

	.set	T_GETHRTIME, 3
	.set	T_GETHRVTIME, 4

/
/ hrtime_t gethrtime(void)
/
/ Returns the current hi-res real time.
/

	.globl	gethrtime
	.text
	.align	4

_fgdef_(gethrtime):
	MCOUNT
	movl	$T_GETHRTIME,%eax
	int	$T_FASTTRAP
	ret
	_fg_setsize_(`gethrtime')

/
/ hrtime_t gethrvtime(void)
/
/ Returns the current hi-res LWP virtual time.
/

	.globl	gethrvtime
	.text
	.align	4

_fgdef_(gethrvtime):
	movl	$T_GETHRVTIME,%eax
	int	$T_FASTTRAP
	ret
	_fg_setsize_(`gethrvtime')
