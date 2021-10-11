	.ident	"@(#)__clock_timer.s	1.6 0 SMI"

	.file "__clock_timer.s"

	.text

	.globl	__cerror

/
/ int
/ __clock_getres (clockid_t clock_id, struct timespec *res)
/

_fwdef_(`__clock_getres'):
	movl	$CLOCK_GETRES,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__clock_getres')

/
/ int
/ __clock_gettime (clockid_t clock_id, timespec_t *tp)
/

	.set	GETHRESTIME,5		/ subcode for fast trap
	.set	CLOCK_REALTIME0,0	/ clock id for per-LWP timers
	.set	CLOCK_REALTIME3,3	/ clock id for per-process timers

_fwdef_(`__clock_gettime'):
	movl	4(%esp),%eax
	cmpl	$CLOCK_REALTIME0,%eax		/ if (clock_id)
	je	2f				/ equal to CLOCK_REALTIME0
	cmpl	$CLOCK_REALTIME3,%eax		/ or if (clock_id)
	jne	1f				/ equal to CLOCK_REALTIME3
2:
	movl	$GETHRESTIME,%eax		/ gethrestime()
	int	$T_FASTTRAP			/ returns in %eax, %edx
	pushl	%esi
	movl	12(%esp),%esi			/ save into tp
	movl	%eax,(%esi)
	movl	%edx,4(%esi)
	xorl	%eax,%eax
	popl	%esi
	ret
1:
	movl	$CLOCK_GETTIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	3f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
3:
	ret
	_fw_setsize_(`__clock_gettime')

/
/ int
/ __clock_settime (clockid_t clock_id, timespec_t *tp)
/

_fwdef_(`__clock_settime'):
	movl	$CLOCK_SETTIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__clock_settime')

/*
/* int
/* __timer_create(clock_id, evp, timerid)
/*	clockid_t clock_id;
/*	struct sigevent *evp;
/*	timer_t *timerid;
/*

_fwdef_(`__timer_create'):
	movl	$TIMER_CREATE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__timer_create')

/*
/* int
/* __timer_delete(timerid)
/*	timer_t timerid;
/*

_fwdef_(`__timer_delete'):
	movl	$TIMER_DELETE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__timer_delete')

/*
/* int
/* __timer_getoverrun(timerid)
/*	timer_t timerid;
/*

_fwdef_(`__timer_getoverrun'):
	movl	$TIMER_GETOVERRUN,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__timer_getoverrun')

/*
/* int
/* __timer_gettime(timerid, value)
/*	timer_t timerid;
/*	struct itimerspec *value;
/*

_fwdef_(`__timer_gettime'):
	movl	$TIMER_GETTIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__timer_gettime')

/*
/* int
/* __timer_settime(timerid, flags, value, ovalue)
/*	timer_t timerid;
/*	int flags;
/*	const struct itimerspec *value;
/*	struct itimerspec *ovalue;
/*

_fwdef_(`__timer_settime'):
	movl	$TIMER_SETTIME,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`__timer_settime')

/*
/* int
/* _libc_nanosleep(rqtp, rmtp)
/*	const struct timespec *rqtp;
/*	struct timespec *rqtp;
/*

_fwdef_(`_libc_nanosleep'):
	movl	$NANOSLEEP,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
1:
	ret
	_fw_setsize_(`_libc_nanosleep')
