#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)private.spec	1.14	99/12/04 SMI"
#
# lib/libc/spec/private.spec

function	_QgetRD
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_QgetRP
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_QswapRD
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_QswapRP
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	___lwp_cond_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	___lwp_mutex_unlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__alloc_selector
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__charmap_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__class_quadruple
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__clock_getres
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__clock_gettime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__clock_settime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__collate_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__ctype_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__divrem64
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__door_bind
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_call
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_cred
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_info
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_return
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_revoke
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__door_unbind
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__errno_fix
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__eucpctowc_gen
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fdsync
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fgetwc_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fgetwc_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fgetwc_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fltrounds
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__fnmatch_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fnmatch_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__fnmatch_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__free_all_selectors
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__freegs
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__freegs_lock
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__freegs_unlock
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__getcontext
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__getdate_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__iswctype_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__iswctype_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__iswctype_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__ldt_lock
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__ldt_unlock
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__locale_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__localeconv_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lock_clear
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__lock_try
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__lwp_cond_broadcast
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_cond_signal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_cond_timedwait # extends libc/spec/sys.spec _lwp_cond_timedwait
weak		_lwp_cond_timedwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_cond_wait # extends libc/spec/sys.spec _lwp_cond_wait
weak		_lwp_cond_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_continue
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_exit
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_getprivate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_info
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_kill
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_makecontext # extends libc/spec/sys.spec _lwp_makecontext
weak		_lwp_makecontext
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_mutex_lock # extends libc/spec/sys.spec _lwp_mutex_lock
weak		_lwp_mutex_lock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_mutex_trylock # extends libc/spec/sys.spec _lwp_mutex_trylock
weak		_lwp_mutex_trylock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_mutex_unlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_schedctl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_self
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_sema_init # extends libc/spec/sys.spec _lwp_sema_init
weak		_lwp_sema_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_sema_post
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_sema_trywait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_sema_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_setprivate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_sigredirect
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_suspend
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_suspend2
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__lwp_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbftowc_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbftowc_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbftowc_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mblen_gen
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mblen_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbstowcs_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbstowcs_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbstowcs_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbtowc_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbtowc_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mbtowc_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__messages_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__monetary_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__mt_sigpending
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__multi_innetgr
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__nanosleep
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__nl_langinfo_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__numeric_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regcomp_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regcomp_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regerror_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regexec_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regexec_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__regfree_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__setupgs
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__sigaction
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__signotify
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__signotifywait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__sigqueue
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__sigtimedwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strcoll_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strcoll_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strcoll_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strfmon_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strftime_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strptime_dontzero
declaration     char *__strptime_dontzero(const char *buf, const char *format, \
                        struct tm *tm)
version		SUNWprivate_1.1
exception       $return == 0
end

function	__strptime_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strxfrm_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strxfrm_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__strxfrm_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__swapEX
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__swapRD
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__swapTE
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__time_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__timer_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__timer_delete
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__timer_getoverrun
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__timer_gettime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__timer_settime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towctrans_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towctrans_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towlower_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towlower_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towupper_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__towupper_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__trwctype_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__udivrem64
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__wcscoll_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcscoll_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcscoll_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcsftime_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcstombs_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcstombs_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcstombs_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcswidth_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcswidth_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcswidth_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcsxfrm_C
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcsxfrm_bc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcsxfrm_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctoeucpc_gen
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctomb_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctomb_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctomb_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctrans_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wctype_std
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcwidth_dense
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcwidth_euc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__wcwidth_sb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	__xgetRD
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__xtol
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__xtoll
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__xtoul
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	__xtoull
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	_a64l # extends libc/spec/gen.spec a64l
weak		a64l
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_acl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_adjtime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ascftime # extends libc/spec/gen.spec ascftime
weak		ascftime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_asctime_r # extends libc/spec/gen.spec asctime_r
weak		asctime_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_brk # extends libc/spec/sys.spec brk
weak		brk
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_bufsync
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cerror
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_cerror64
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1
end

function	_cfree # extends libc/spec/gen.spec cfree
weak		cfree
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cftime # extends libc/spec/gen.spec cftime
weak		cftime
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_closelog # extends libc/spec/gen.spec closelog
weak		closelog
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_broadcast
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_signal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_timedwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_cond_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ctermid # extends libc/spec/stdio.spec ctermid
weak		ctermid
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_ctermid_r # extends libc/spec/stdio.spec ctermid_r
weak		ctermid_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ctime_r # extends libc/spec/gen.spec ctime_r
weak		ctime_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_decimal_to_double # extends libc/spec/fp.spec decimal_to_double
weak		decimal_to_double
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_decimal_to_extended # extends libc/spec/fp.spec decimal_to_extended
weak		decimal_to_extended
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_decimal_to_quadruple # extends libc/spec/fp.spec decimal_to_quadruple
weak		decimal_to_quadruple
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_decimal_to_single # extends libc/spec/fp.spec decimal_to_single
weak		decimal_to_single
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_dgettext # extends libc/spec/i18n.spec dgettext
weak		dgettext
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_bind
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_call
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_cred
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_info
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_return
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_revoke
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_door_unbind
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_doprnt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_doscan
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_double_to_decimal # extends libc/spec/fp.spec double_to_decimal
weak		double_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_drand48 # extends libc/spec/gen.spec drand48
weak		drand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_econvert # extends libc/spec/gen.spec econvert
weak		econvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ecvt # extends libc/spec/gen.spec ecvt
weak		ecvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_endgrent # extends libc/spec/gen.spec endgrent
weak		endgrent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_endpwent # extends libc/spec/gen.spec endpwent
weak		endpwent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_endspent # extends libc/spec/gen.spec endspent
weak		endspent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_endutent # extends libc/spec/gen.spec endutent
weak		endutent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_endutxent # extends libc/spec/gen.spec endutxent
weak		endutxent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_erand48 # extends libc/spec/gen.spec erand48
weak		erand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_exportfs
weak		exportfs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_extended_to_decimal # extends libc/spec/fp.spec extended_to_decimal
weak		extended_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_facl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fchroot
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fconvert # extends libc/spec/gen.spec fconvert
weak		fconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fcvt # extends libc/spec/gen.spec fcvt
weak		fcvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ffs # extends libc/spec/gen.spec ffs
weak		ffs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetgrent # extends libc/spec/gen.spec fgetgrent
weak		fgetgrent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetgrent_r # extends libc/spec/gen.spec fgetgrent_r
weak		fgetgrent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetpwent # extends libc/spec/gen.spec fgetpwent
weak		fgetpwent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetpwent_r # extends libc/spec/gen.spec fgetpwent_r
weak		fgetpwent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetspent # extends libc/spec/gen.spec fgetspent
weak		fgetspent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fgetspent_r # extends libc/spec/gen.spec fgetspent_r
weak		fgetspent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_file_to_decimal # extends libc/spec/fp.spec file_to_decimal
weak		file_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_findbuf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_findiop
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_finite # extends libc/spec/i18n.spec finite
weak		finite
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_flockfile # extends libc/spec/stdio.spec flockfile
weak		flockfile
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fork1
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpclass # extends libc/spec/i18n.spec fpclass
weak		fpclass
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpgetmask
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpgetround
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpgetsticky
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fprintf # extends libc/spec/print.spec fprintf
weak		fprintf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpsetmask
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpsetround
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fpsetsticky
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_fstatfs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_func_to_decimal # extends libc/spec/fp.spec func_to_decimal
weak		func_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_funlockfile # extends libc/spec/stdio.spec funlockfile
weak		funlockfile
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_gconvert # extends libc/spec/gen.spec gconvert
weak		gconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_gcvt # extends libc/spec/gen.spec gcvt
weak		gcvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getarg
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getc_unlocked # extends libc/spec/stdio.spec getc_unlocked
weak		getc_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getchar_unlocked # extends libc/spec/stdio.spec getchar_unlocked
weak		getchar_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getdents
version		SUNWprivate_1.1
end

function	_getgrent # extends libc/spec/gen.spec getgrent
weak		getgrent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getgrent_r # extends libc/spec/gen.spec getgrent_r
weak		getgrent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getgrgid # extends libc/spec/gen.spec getgrgid
weak		getgrgid
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_getgrgid_r # extends libc/spec/gen.spec getgrgid_r
weak		getgrgid_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getgrnam # extends libc/spec/gen.spec getgrnam
weak		getgrnam
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_getgrnam_r # extends libc/spec/gen.spec getgrnam_r
weak		getgrnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getgroupsbymember
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getlogin # extends libc/spec/gen.spec getlogin
weak		getlogin
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_getlogin_r # extends libc/spec/gen.spec getlogin_r
weak		getlogin_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getmntany # extends libc/spec/gen.spec getmntany
weak		getmntany
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getmntent # extends libc/spec/gen.spec getmntent
weak		getmntent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getpw # extends libc/spec/gen.spec getpw
weak		getpw
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getpwent # extends libc/spec/gen.spec getpwent
weak		getpwent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getpwent_r # extends libc/spec/gen.spec getpwent_r
weak		getpwent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getpwnam # extends libc/spec/gen.spec getpwnam
weak		getpwnam
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_getpwnam_r # extends libc/spec/gen.spec getpwnam_r
weak		getpwnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getpwuid # extends libc/spec/gen.spec getpwuid
weak		getpwuid
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_getpwuid_r # extends libc/spec/gen.spec getpwuid_r
weak		getpwuid_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getsp
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getspent # extends libc/spec/gen.spec getspent
weak		getspent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getspent_r # extends libc/spec/gen.spec getspent_r
weak		getspent_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getspnam # extends libc/spec/gen.spec getspnam
weak		getspnam
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getspnam_r # extends libc/spec/gen.spec getspnam_r
weak		getspnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutent # extends libc/spec/gen.spec getutent
weak		getutent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutid # extends libc/spec/gen.spec getutid
weak		getutid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutline # extends libc/spec/gen.spec getutline
weak		getutline
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutmp # extends libc/spec/gen.spec getutmp
weak		getutmp
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutmpx # extends libc/spec/gen.spec getutmpx
weak		getutmpx
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutxent # extends libc/spec/gen.spec getutxent
weak		getutxent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutxid # extends libc/spec/gen.spec getutxid
weak		getutxid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getutxline # extends libc/spec/gen.spec getutxline
weak		getutxline
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getvfsany # extends libc/spec/gen.spec getvfsany
weak		getvfsany
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getvfsent # extends libc/spec/gen.spec getvfsent
weak		getvfsent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getvfsfile # extends libc/spec/gen.spec getvfsfile
weak		getvfsfile
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_getvfsspec # extends libc/spec/gen.spec getvfsspec
weak		getvfsspec
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_gmtime_r # extends libc/spec/gen.spec gmtime_r
weak		gmtime_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_gsignal # extends libc/spec/gen.spec gsignal
weak		gsignal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_gtty
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_halt
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_hasmntopt # extends libc/spec/gen.spec hasmntopt
weak		hasmntopt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_iconv # extends libc/spec/gen.spec iconv
weak		iconv
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_iconv_close # extends libc/spec/gen.spec iconv_close
weak		iconv_close
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_iconv_open # extends libc/spec/gen.spec iconv_open
weak		iconv_open
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_install_utrap
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_is_euc_fc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_is_euc_pc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_isnanf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_iswctype
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_jrand48 # extends libc/spec/gen.spec jrand48
weak		jrand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_kaio
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_l64a # extends libc/spec/gen.spec l64a
weak		l64a
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ladd
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lckpwdf # extends libc/spec/gen.spec lckpwdf
weak		lckpwdf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lcong48 # extends libc/spec/gen.spec lcong48
weak		lcong48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ldivide # extends libc/spec/sys.spec ldivide
weak		ldivide
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lexp10 # extends libc/spec/sys.spec lexp10
weak		lexp10
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_libc_nanosleep
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_libc_sigtimedwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_libc_sigprocmask
include		<signal.h>
declaration	int _libc_sigprocmask(int, const sigset_t *, sigset_t *)
version		SUNWprivate_1.1
end

function	_libc_threads_interface
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_llabs # extends libc/spec/gen.spec llabs
weak		llabs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lldiv # extends libc/spec/gen.spec lldiv
weak		lldiv
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_llog10 # extends libc/spec/sys.spec llog10
weak		llog10
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_llseek
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lmul # extends libc/spec/sys.spec lmul
weak		lmul
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_loadtab
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end

function	_localtime_r # extends libc/spec/gen.spec localtime_r
weak		localtime_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lock_clear
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lock_try
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_logb # extends libc/spec/sys.spec logb
weak		logb
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lrand48 # extends libc/spec/gen.spec lrand48
weak		lrand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lshiftl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lsub
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ltzset
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_lwp_schedctl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_madvise # extends libc/spec/gen.spec madvise
weak		madvise
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_makeut
weak		makeut
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_makeutx
weak		makeutx
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mbftowc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_memalign # extends libc/spec/gen.spec memalign
weak		memalign
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_memcmp
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_memcpy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_memmove
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_memset
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mincore
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mkarglst
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mlockall # extends libc/spec/gen.spec mlockall
weak		mlockall
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_modff # extends libc/spec/gen.spec modff
weak		modff
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mrand48 # extends libc/spec/gen.spec mrand48
weak		mrand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_munlockall # extends libc/spec/gen.spec munlockall
weak		munlockall
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mutex_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mutex_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mutex_trylock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_mutex_unlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nfs_getfh
weak		nfs_getfh
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nfssvc
weak		nfssvc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nfssys
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nrand48 # extends libc/spec/gen.spec nrand48
weak		nrand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_delete
weak		nss_delete
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_endent
weak		nss_endent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_getent
weak		nss_getent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_initf_netgroup
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_search
weak		nss_search
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_nss_setent
weak		nss_setent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_openlog # extends libc/spec/gen.spec openlog
weak		openlog
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_plock # extends libc/spec/gen.spec plock
weak		plock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pread
version		SUNWprivate_1.1
end

function	_psiginfo # extends libc/spec/gen.spec psiginfo
weak		psiginfo
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_psignal # extends libc/spec/gen.spec psignal
weak		psignal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_atfork
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getdetachstate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getinheritsched
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getschedparam
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getschedpolicy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getscope
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getstackaddr
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_getstacksize
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setdetachstate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setinheritsched
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setschedparam
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setschedpolicy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setscope
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setstackaddr
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_attr_setstacksize
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cancel
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_broadcast
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_signal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_timedwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_cond_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_condattr_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_condattr_getpshared
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_condattr_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_condattr_setpshared
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_detach
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_equal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_exit
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_getschedparam
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_getspecific
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_join
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_key_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_key_delete
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_kill
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_getprioceiling
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_lock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_setprioceiling
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_trylock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutex_unlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_destroy
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getprioceiling
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getprotocol
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_getpshared
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setprioceiling
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setprotocol
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_setpshared
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_once
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_self
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_setcancelstate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_setcanceltype
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_setschedparam
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_setspecific
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_sigmask
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pthread_testcancel
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_putc_unlocked # extends libc/spec/stdio.spec putc_unlocked
weak		putc_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_putchar_unlocked # extends libc/spec/stdio.spec putchar_unlocked
weak		putchar_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_putpwent # extends libc/spec/gen.spec putpwent
weak		putpwent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_putspent # extends libc/spec/gen.spec putspent
weak		putspent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pututline # extends libc/spec/gen.spec pututline
weak		pututline
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pututxline # extends libc/spec/gen.spec pututxline
weak		pututxline
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_pwrite
version		SUNWprivate_1.1
end

function	_qeconvert # extends libc/spec/gen.spec qeconvert
weak		qeconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_qecvt # extends libc/spec/sys.spec qecvt
weak		qecvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_qfconvert # extends libc/spec/gen.spec qfconvert
weak		qfconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_qfcvt # extends libc/spec/sys.spec qfcvt
weak		qfcvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_qgconvert # extends libc/spec/gen.spec qgconvert
weak		qgconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_qgcvt # extends libc/spec/sys.spec qgcvt
weak		qgcvt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_quadruple_to_decimal # extends libc/spec/fp.spec quadruple_to_decimal
weak		quadruple_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rand_r # extends libc/spec/gen.spec rand_r
weak		rand_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_realbufend
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_realpath # extends libc/spec/gen.spec realpath
weak		realpath
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rpcsys
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rw_rdlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rw_tryrdlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rw_trywrlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rw_unlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rw_wrlock
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_rwlock_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sbrk # extends libc/spec/sys.spec sbrk
weak		sbrk
#Prototype	/* Unknown. */
version		i386=SYSVABI_1.3 sparc=SISCD_2.3 sparcv9=SUNW_0.7
end

function	_sbrk_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_seconvert # extends libc/spec/gen.spec seconvert
weak		seconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_seed48 # extends libc/spec/gen.spec seed48
weak		seed48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_select # extends libc/spec/gen.spec select
weak		select
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sema_init
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sema_post
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sema_trywait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sema_wait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setbufend
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setegid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_seteuid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setgrent # extends libc/spec/gen.spec setgrent
weak		setgrent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setlogmask # extends libc/spec/gen.spec setlogmask
weak		setlogmask
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setpwent # extends libc/spec/gen.spec setpwent
weak		setpwent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setregid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setreuid
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setspent # extends libc/spec/gen.spec setspent
weak		setspent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_settimeofday # extends libc/spec/gen.spec settimeofday
weak		settimeofday
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setutent # extends libc/spec/gen.spec setutent
weak		setutent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_setutxent # extends libc/spec/gen.spec setutxent
weak		setutxent
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sfconvert # extends libc/spec/gen.spec sfconvert
weak		sfconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sgconvert # extends libc/spec/gen.spec sgconvert
weak		sgconvert
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sig2str # extends libc/spec/gen.spec sig2str
weak		sig2str
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sigflag
weak		sigflag
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sigfpe # extends libc/spec/sys.spec sigfpe
weak		sigfpe
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_signal # extends libc/spec/gen.spec signal
weak		signal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sigwait
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_single_to_decimal # extends libc/spec/fp.spec single_to_decimal
weak		single_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_accept
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_bind
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_connect
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_getpeername
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_getsockname
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_getsockopt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_listen
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_recv
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_recvfrom
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_recvmsg
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_send
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_sendmsg
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_sendto
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_setsockopt
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_shutdown
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_socket
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_so_socketpair
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sockconfig
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_srand48 # extends libc/spec/gen.spec srand48
weak		srand48
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ssignal # extends libc/spec/gen.spec ssignal
weak		ssignal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_statfs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_str2sig # extends libc/spec/gen.spec str2sig
weak		str2sig
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_strerror # extends libc/spec/gen.spec strerror
weak		strerror
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_string_to_decimal # extends libc/spec/fp.spec string_to_decimal
weak		string_to_decimal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_strsignal # extends libc/spec/gen.spec strsignal
weak		strsignal
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_strtok_r # extends libc/spec/gen.spec strtok_r
weak		strtok_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_strtoll # extends libc/spec/gen.spec strtoll
weak		strtoll
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_strtoull # extends libc/spec/gen.spec strtoull
weak		strtoull
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_stty
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_swapctl # extends libc/spec/gen.spec swapctl
weak		swapctl
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sysconfig
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sysfs
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_sysi86
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1
end

function	_syssun
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_thr_continue
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_create
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_errnop
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_exit
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_get_inf_read
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_get_nan_read
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_getconcurrency
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_getprio
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_getspecific
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_join
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_keycreate
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_kill
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_libthread
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_main
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_min_stack
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_self
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_setconcurrency
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_setprio
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_setspecific
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_sigsetmask
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_stksegment
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_suspend
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_thr_yield
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_tmpnam_r # extends libc/spec/stdio.spec tmpnam_r
weak		tmpnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_trwctype
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ttyname # extends libc/spec/gen.spec ttyname
weak		ttyname
#Prototype	/* Unknown. */
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7
end

function	_ttyname_r # extends libc/spec/gen.spec ttyname_r
weak		ttyname_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ttyslot # extends libc/spec/gen.spec ttyslot
weak		ttyslot
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_uadmin # extends libc/spec/sys.spec uadmin
weak		uadmin
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ulckpwdf # extends libc/spec/gen.spec ulckpwdf
weak		ulckpwdf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ulltostr # # extends libc/spec/gen.spec ulltostr
weak		ulltostr
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_uncached_getgrgid_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_uncached_getgrnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_uncached_getpwnam_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_uncached_getpwuid_r
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ungetc_unlocked
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_unordered # extends libc/spec/i18n.spec unordered
weak		unordered
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_updwtmp
weak		updwtmp
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_updwtmpx
weak		updwtmpx
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_ustat
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_utimes
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_utmpname # extends libc/spec/gen.spec utmpname
weak		utmpname
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_utmpxname # extends libc/spec/gen.spec utmpxname
weak		utmpxname
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_utssys
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_valloc # extends libc/spec/gen.spec valloc
weak		valloc
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_vfork
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_vhangup
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_vsyslog # extends libc/spec/gen.spec vsyslog
weak		vsyslog
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_wctomb # extends libc/spec/i18n.spec wctomb
weak		wctomb
#Prototype	/* Unknown. */
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end

function	_wrtchk
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_xflsbuf
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_xgetwidth
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	_xregs_clrptr
#Prototype	/* Unknown. */
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end

function	_yield
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	dbm_close_status
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	dbm_do_nextkey
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	dbm_setdefwrite
#Prototype	/* Unknown. */
version		SUNWprivate_1.1
end

function	gtty
version		SUNWprivate_1.1
end

function	htonl
version		i386=SUNWprivate_1.1 ia64=SUNW_0.7
end

function	htons
version		i386=SUNWprivate_1.1 ia64=SUNW_0.7
end

function	install_utrap
arch		sparc sparcv9
version		SUNWprivate_1.1
end

function	kaio
version		SUNWprivate_1.1
end

function	makeut
version		SUNWprivate_1.1
end

function	mcfiller
version		SUNWprivate_1.1
end

function	mntopt
version		SUNWprivate_1.1
end

function	mutex_held
version		SUNWprivate_1.1
end

function	nfssvc
version		SUNWprivate_1.1
end

function	nop
arch		sparc sparcv9
version		SUNWprivate_1.1
end

function	ntohl
version		i386=SUNWprivate_1.1 ia64=SUNW_0.7
end

function	ntohs
version		i386=SUNWprivate_1.1 ia64=SUNW_0.7
end

function	scrwidth
version		SUNWprivate_1.1
end

function	sigflag
version		SUNWprivate_1.1
end

function	str2spwd
version		SUNWprivate_1.1
end

function	stty
version		SUNWprivate_1.1
end

function	sysi86
version		i386=SUNWprivate_1.1
end

function	thr_errnop
version		SUNWprivate_1.1
end

function	utssys
version		SUNWprivate_1.1
end

function	wdbindf
version		SUNWprivate_1.1
end

function	wdchkind
version		SUNWprivate_1.1
end

function	wddelim
version		SUNWprivate_1.1
end

function	_delete
version		SUNWprivate_1.1
end

function	_insert
version		SUNWprivate_1.1
end

function	_lwp_sigtimedwait
include		<signal.h>, <sys/time_impl.h>
declaration	int _lwp_sigtimedwait(const sigset_t *set, \
		    siginfo_t *info, const struct timespec *timeout, \
		    int queued)
version		SUNWprivate_1.1
end

function	__lwp_sigtimedwait
weak		_lwp_sigtimedwait
version		SUNWprivate_1.1
end

function	_nss_XbyY_fgets
include		<nss_dbdefs.h>
declaration	void _nss_XbyY_fgets(FILE *f, nss_XbyY_args_t *b)
version		SUNWprivate_1.1
end

# PSARC/1998/452; Bug 4181371; NSS Lookup Control START

function	__nsw_getconfig_v1
include		"../../inc/nsswitch_priv.h"
prototype	struct __nsw_switchconfig_v1 \
		    *__nsw_getconfig_v1(const char *, enum __nsw_parse_err *)
version		SUNWprivate_1.1
end

function	__nsw_freeconfig_v1
include		"../../inc/nsswitch_priv.h"
prototype	int __nsw_freeconfig_v1(struct __nsw_switchconfig_v1 *)
version		SUNWprivate_1.1
end

function	__nsw_extended_action_v1
include		"../../inc/nsswitch_priv.h"
prototype	action_t __nsw_extended_action_v1(struct __nsw_lookup_v1 *,\
		    int)
version		SUNWprivate_1.1
end

# PSARC/1998/452; Bug 4181371; NSS Lookup Control END

function	_get_exit_frame_monitor
declaration	void * _get_exit_frame_monitor(void)
version		SUNWprivate_1.1
end

# Bugid 4296198, had to move code from libnsl/nis/cache/cache_api.cc

function	__nis_get_environment
declaration	void __nis_get_environment(void)
version		SUNWprivate_1.1
end
