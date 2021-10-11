#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)missing.spec	1.3	99/06/10 SMI"
#
# lib/libc/spec/missing.spec

function	_Qp_add 
version		sparcv9=SUNW_0.7
end

function	_Qp_cmp 
version		sparcv9=SUNW_0.7
end

function	_Qp_cmpe 
version		sparcv9=SUNW_0.7
end

function	_Qp_div 
version		sparcv9=SUNW_0.7
end

function	_Qp_dtoq 
version		sparcv9=SUNW_0.7
end

function	_Qp_feq 
version		sparcv9=SUNW_0.7
end

function	_Qp_fge 
version		sparcv9=SUNW_0.7
end

function	_Qp_fgt 
version		sparcv9=SUNW_0.7
end

function	_Qp_fle 
version		sparcv9=SUNW_0.7
end

function	_Qp_flt 
version		sparcv9=SUNW_0.7
end

function	_Qp_fne 
version		sparcv9=SUNW_0.7
end

function	_Qp_itoq 
version		sparcv9=SUNW_0.7
end

function	_Qp_mul 
version		sparcv9=SUNW_0.7
end

function	_Qp_neg 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtod 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtoi 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtos 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtoui 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtoux 
version		sparcv9=SUNW_0.7
end

function	_Qp_qtox 
version		sparcv9=SUNW_0.7
end

function	_Qp_sqrt 
version		sparcv9=SUNW_0.7
end

function	_Qp_stoq 
version		sparcv9=SUNW_0.7
end

function	_Qp_sub 
version		sparcv9=SUNW_0.7
end

function	_Qp_uitoq 
version		sparcv9=SUNW_0.7
end

function	_Qp_uxtoq 
version		sparcv9=SUNW_0.7
end

function	_Qp_xtoq 
version		sparcv9=SUNW_0.7
end

function	____loc1 
version		SUNW_1.1
end

function	__align_cpy_1 
version		sparcv9=SUNW_0.7
end

function	__align_cpy_16 
version		sparcv9=SUNW_0.7
end

function	__align_cpy_2 
version		sparcv9=SUNW_0.7
end

function	__align_cpy_4 
version		sparcv9=SUNW_0.7
end

function	__align_cpy_8 
version		sparcv9=SUNW_0.7
end

function	__dtoul 
version		sparcv9=SUNW_0.7
end

function	__ftoul 
version		sparcv9=SUNW_0.7
end

function	__pthread_cleanup_pop 
version		SUNW_1.1
end

function	__pthread_cleanup_push 
version		SUNW_1.1
end

function	__sparc_utrap_install 
version		sparcv9=SUNW_0.7
end

function	_filbuf 
version		SUNW_0.7
end

function	_fp_hw 
arch		i386
version		SYSVABI_1.3
end

function	_fxstat 
arch		i386
version		SYSVABI_1.3
end

function	_lwp_sema_trywait 
version		SUNW_1.1
end

function	_lxstat 
arch		i386
version		SYSVABI_1.3
end

function	_nuname 
arch		i386
version		SYSVABI_1.3
end

function	_resolvepath 
version		SUNW_1.1
end

function	_thr_errno_addr 
arch		i386
version		SUNW_0.7
end

function	_xmknod 
arch		i386
version		SYSVABI_1.3
end

function	_xstat 
arch		i386
version		SYSVABI_1.3
end

function	nuname 
arch		i386
version		SYSVABI_1.3 
end

function	pthread_mutexattr_setprioceiling 
version		SUNW_0.9 
end

function	regex 
version		SUNW_1.1 
end

function	resolvepath 
version		SUNW_1.1 
end

function	rw_read_held 
version		SUNW_0.8 
end

function	rw_write_held 
version		SUNW_0.8 
end

function	s_fcntl 
arch		i386 sparc
version		SUNW_1.1 
end

function	sema_held 
version		SUNW_0.8 
end

function	statfs 
version		SUNW_0.7 
end

function	wcscspn 
include		<wchar.h>
declaration	size_t wcscspn(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wcsspn 
include		<wchar.h>
declaration	size_t wcsspn(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wcstok 
include		<wchar.h>
declaration	wchar_t *wcstok(wchar_t *ws1, const wchar_t *ws2, wchar_t **ptr)
version		SUNW_1.1 
end

function	wcswcs 
include		<wchar.h>
declaration	wchar_t *wcswcs(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wscspn 
include		<wchar.h>
declaration	size_t wscspn(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wsspn 
include		<wchar.h>
declaration	size_t wsspn(const wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	wstok 
include		<wchar.h>
declaration	wchar_t *wstok(wchar_t *ws1, const wchar_t *ws2)
version		SUNW_1.1 
end

function	_Q_lltoq
version		SISCD_2.3
end

function	_Q_qtoll
version		SISCD_2.3
end

function	_Q_qtoull
version		SISCD_2.3
end

function	_Q_ulltoq
version		SISCD_2.3
end

function	___lwp_mutex_init
version		SUNWprivate_1.1
end

function	___lwp_mutex_lock
version		SUNWprivate_1.1
end

function	___lwp_mutex_trylock
version		SUNWprivate_1.1
end

function	___lwp_mutex_wakeup
version		SUNWprivate_1.1
end

function	___xpg4_putmsg
version		SUNWprivate_1.1
end

function	___xpg4_putpmsg
version		SUNWprivate_1.1
end

function	__btowc_dense
version		SUNWprivate_1.1
end

function	__btowc_euc
version		SUNWprivate_1.1
end

function	__btowc_sb
version		SUNWprivate_1.1
end

function	__fbufsize
version		SUNW_1.18
end

function	__fgetwc_xpg5
version		SUNWprivate_1.1
end

function	__fgetws_xpg5
version		SUNWprivate_1.1
end

function	__flbf
version		SUNW_1.18
end

function	__fpending
version		SUNW_1.18
end

function	__fpurge
version		SUNW_1.18
end

function	__fputwc_xpg5
version		SUNWprivate_1.1
end

function	__fputws_xpg5
version		SUNWprivate_1.1
end

function	__freadable
version		SUNW_1.18
end

function	__freading
version		SUNW_1.18
end

function	__fsetlocking
include		<stdio_ext.h>
declaration	int __fsetlocking(FILE *stream, int type)
version		SUNW_1.18.1
end

function	__fwritable
version		SUNW_1.18
end

function	__fwriting
version		SUNW_1.18
end

function	__getloadavg
version		SUNWprivate_1.1
end

function	__getwc_xpg5
version		SUNWprivate_1.1
end

function	__getwchar_xpg5
version		SUNWprivate_1.1
end

function	__mbrlen_gen
version		SUNWprivate_1.1
end

function	__mbrlen_sb
version		SUNWprivate_1.1
end

function	__mbrtowc_dense
version		SUNWprivate_1.1
end

function	__mbrtowc_euc
version		SUNWprivate_1.1
end

function	__mbrtowc_sb
version		SUNWprivate_1.1
end

function	__mbsinit_gen
version		SUNWprivate_1.1
end

function	__mbsrtowcs_dense
version		SUNWprivate_1.1
end

function	__mbsrtowcs_euc
version		SUNWprivate_1.1
end

function	__mbsrtowcs_sb
version		SUNWprivate_1.1
end

function	__mbst_get_consumed_array
version		SUNWprivate_1.1
end

function	__mbst_get_locale
version		SUNWprivate_1.1
end

function	__mbst_get_nconsumed
version		SUNWprivate_1.1
end

function	__mbst_set_consumed_array
version		SUNWprivate_1.1
end

function	__mbst_set_locale
version		SUNWprivate_1.1
end

function	__mbst_set_nconsumed
version		SUNWprivate_1.1
end

function	__putwc_xpg5
version		SUNWprivate_1.1
end

function	__putwchar_xpg5
version		SUNWprivate_1.1
end

function	__sysconf_xpg5
version		SUNW_1.18
end

function	__ungetwc_xpg5
version		SUNWprivate_1.1
end

function	__wcrtomb_dense
version		SUNWprivate_1.1
end

function	__wcrtomb_euc
version		SUNWprivate_1.1
end

function	__wcrtomb_sb
version		SUNWprivate_1.1
end

function	__wcsftime_xpg5
version		SUNWprivate_1.1
end

function	__wcsrtombs_dense
version		SUNWprivate_1.1
end

function	__wcsrtombs_euc
version		SUNWprivate_1.1
end

function	__wcsrtombs_sb
version		SUNWprivate_1.1
end

function	__wcstok_xpg5
version		SUNWprivate_1.1
end

function	__wctob_dense
version		SUNWprivate_1.1
end

function	__wctob_euc
version		SUNWprivate_1.1
end

function	__wctob_sb
version		SUNWprivate_1.1
end

function	_flushlbf
version		SUNW_1.18
end

function	_ftrylockfile
version		SUNWprivate_1.1
end

function	_libc_child_atfork
version		SUNWprivate_1.1
end

function	_libc_parent_atfork
version		SUNWprivate_1.1
end

function	_libc_prepare_atfork
version		SUNWprivate_1.1
end

function	_pthread_attr_getguardsize
version		SUNWprivate_1.1
end

function	_pthread_attr_setguardsize
version		SUNWprivate_1.1
end

function	_pthread_getconcurrency
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_gettype
version		SUNWprivate_1.1
end

function	_pthread_mutexattr_settype
version		SUNWprivate_1.1
end

function	_pthread_rwlock_destroy
version		SUNWprivate_1.1
end

function	_pthread_rwlock_init
version		SUNWprivate_1.1
end

function	_pthread_rwlock_rdlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_tryrdlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_trywrlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_unlock
version		SUNWprivate_1.1
end

function	_pthread_rwlock_wrlock
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_destroy
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_getpshared
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_init
version		SUNWprivate_1.1
end

function	_pthread_rwlockattr_setpshared
version		SUNWprivate_1.1
end

function	_pthread_setconcurrency
version		SUNWprivate_1.1
end

function	_s_fcntl
arch		sparc	i386
version		SUNW_1.1
end

function	ftrylockfile
version		SUNW_1.1
end

function	fwide
version		SUNW_1.18
end

function	mbrlen
version		SUNW_1.18
end

function	mbrtowc
version		SUNW_1.18
end

function	mbsinit
version		SUNW_1.18
end

function	mbsrtowcs
version		SUNW_1.18
end

function	pcsample
version		SUNW_1.18
end

function	pthread_attr_getguardsize
version		SUNW_1.18
end

function	pthread_attr_setguardsize
version		SUNW_1.18
end

function	pthread_getconcurrency
version		SUNW_1.18
end

function	pthread_mutexattr_gettype
version		SUNW_1.18
end

function	pthread_mutexattr_settype
version		SUNW_1.18
end

function	pthread_rwlock_destroy
version		SUNW_1.18
end

function	pthread_rwlock_init
version		SUNW_1.18
end

function	pthread_rwlock_rdlock
version		SUNW_1.18
end

function	pthread_rwlock_tryrdlock
version		SUNW_1.18
end

function	pthread_rwlock_trywrlock
version		SUNW_1.18
end

function	pthread_rwlock_unlock
version		SUNW_1.18
end

function	pthread_rwlock_wrlock
version		SUNW_1.18
end

function	pthread_rwlockattr_destroy
version		SUNW_1.18
end

function	pthread_rwlockattr_getpshared
version		SUNW_1.18
end

function	pthread_rwlockattr_init
version		SUNW_1.18
end

function	pthread_rwlockattr_setpshared
version		SUNW_1.18
end

function	pthread_setconcurrency
version		SUNW_1.18
end

function	s_ioctl
arch		i386 sparc
version		SUNW_1.1
end

function	swprintf
version		SUNW_1.18
end

function	swscanf
version		SUNW_1.18
end

function	vfwprintf
version		SUNW_1.18
end

function	vswprintf
version		SUNW_1.18
end

function	vwprintf
version		SUNW_1.18
end

function	wcrtomb
version		SUNW_1.18
end

function	wcsrtombs
version		SUNW_1.18
end

function	wcsstr
version		SUNW_1.18
end

function	wctob
version		SUNW_1.18
end

function	wmemchr
version		SUNW_1.18
end

function	wmemcmp
version		SUNW_1.18
end

function	wmemcpy
version		SUNW_1.18
end

function	wmemmove
version		SUNW_1.18
end

function	wmemset
version		SUNW_1.18
end

function	_flsbuf
version		SUNW_0.7
end

function	__flsbuf
weak		_flsbuf 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	btowc
version		SUNW_1.18
end
