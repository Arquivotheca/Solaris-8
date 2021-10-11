/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This is where all the interfaces that are internal to libc
 * which do not have a better home live
 */

#ifndef _LIBC_H
#define	_LIBC_H

#pragma ident	"@(#)libc.h	1.44	99/09/20 SMI"

#include <thread.h>
#include <stdio.h>
#include <sys/dirent.h>
#include <ucontext.h>
#include <nsswitch.h>
#include <stddef.h>
#include <sys/dl.h>
#include <sys/door.h>
#include <sys/ieeefp.h>
#include <sys/localedef.h>
#include <sys/mount.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void _rewind_unlocked(FILE *);
extern void	*_malloc_unlocked(size_t);
extern int	_rename(const char *, const char *);
extern long	_sysconfig(int);
extern unsigned	__dtou(double);
extern unsigned	__ftou(float);
extern int kill(pid_t pid, int sig);

extern int thr_main(void);
extern int thr_kill(thread_t tid, int sig);
extern thread_t thr_self(void);
extern int mutex_lock(mutex_t *mp);
extern int mutex_unlock(mutex_t *mp);
extern int _sigwait(sigset_t *);
extern int _thr_getspecific(thread_key_t key, void **valuep);
extern int _thr_setspecific(unsigned int key, void *value);
extern int _thr_keycreate(thread_key_t *pkey, void (*destructor)(void *));

#if !defined(_LP64)
/*
 * getdents64 transitional interface is intentionally internal to libc
 */
extern int getdents64(int, struct dirent64 *, size_t);
#endif

/*
 * Internal routine from tsdalloc.c
 */
extern int _scrwidth(wchar_t);

/*
 * Internal routine from tsdalloc.c
 */
extern int	*_tsdalloc(thread_key_t *, int);

extern int64_t __div64(int64_t, int64_t);
extern int64_t __rem64(int64_t, int64_t);
extern uint64_t __udiv64(uint64_t, uint64_t);
extern uint64_t __urem64(uint64_t, uint64_t);
extern uint64_t _umul32x32to64(uint32_t, uint32_t);
extern int64_t __mul64(int64_t, int64_t);
extern uint64_t __umul64(uint64_t, uint64_t);


/*
 * Rounding direction functions
 */
#if defined(i386) || defined(__ia64)
extern enum fp_direction_type __xgetRD(void);
#elif defined(sparc)
extern enum fp_direction_type _QgetRD(void);
#else
#error Unknown architecture!
#endif


/*
 * defined in ctime.c
 */
extern char	*__posix_asctime_r(const struct tm *, char *);

/*
 * Internal routine from fsync.c
 */
extern int __fdsync(int, int);	/* 2nd arg may be wrong in 64bit mode */

/*
 * Internal routine from _xregs_clrptr.c
 */
extern void _xregs_clrptr(ucontext_t *);

/*
 * Internal routine from nfssys.c
 */
extern int _nfssys(int, void *); /* int in 64bit mode ???, void * ??? */

/*
 * Internal routine from psetsys.c
 */
extern int _pset(int, ...); /* int in 64bit mode ??? */

/*
 * Internal routine from sleep.c
 */
extern unsigned int _libc_sleep(unsigned int);

/*
 * defined in sigpending.s
 */
extern int __sigfillset(sigset_t *);

/*
 * defined in sparc/fp/_Q_add.c
 */
extern _Q_set_exception(unsigned int);

/*
 * defined in nsparse.c
 */
extern struct __nsw_switchconfig *_nsw_getoneconfig(const char *name,
	char *linep, enum __nsw_parse_err *);
extern struct __nsw_switchconfig_v1 *_nsw_getoneconfig_v1(const char *name,
	char *linep, enum __nsw_parse_err *);

/*
 * Internal routine from getusershell.c
 */
extern char *getusershell(void);

/*
 * defined in _sigaction.s
 */
extern int __sigaction(int, const struct sigaction *, struct sigaction *);

/*
 * defined in _getsp.s
 */
extern greg_t _getsp(void);

/*
 * defined in _semsys.s
 */
extern int _semsys(int, ...);

/*
 * defined in _so_setsockopt.s
 */
extern int _so_setsockopt(int, int, int, const char *, int);

/*
 * defined in _so_getsockopt.s
 */
extern int _so_getsockopt(int, int, int, char *, int *);

/*
 * defined in lsign.s
 */
extern int lsign(dl_t);

/*
 * defined in lsign.s
 */
extern int __getcontext(ucontext_t *);

/*
 * defined in door.s
 */
extern int __door_info(int, door_info_t *);
extern int __door_call(int, door_arg_t *);

/*
 * defined in xpg4.c
 */
extern int __xpg4;

/*
 * i18n prototypes
 */
extern int _getcolval(_LC_collate_t *, wchar_t *, wchar_t, const char *, int);
extern int __regcomp_C(_LC_collate_t *, regex_t *, const char *, int);
extern int __fnmatch_C(_LC_collate_t *, const char *, const char *,
	const char *, int);
extern ssize_t  __strfmon_std(_LC_monetary_t *, char *, size_t,
	const char *, va_list);
/* -------------------- CHARMAP BEGIN -------------------- */
extern _LC_charmap_t	*__charmap_init(_LC_locale_t *);
extern int	__charmap_destructor(_LC_locale_t *);
/* ------- CHARMAP METHODS ------- */
extern int	__mbtowc_sb(_LC_charmap_t *, wchar_t *, const char *,
	size_t);
extern size_t	__mbstowcs_sb(_LC_charmap_t *, wchar_t *,
	const char *, size_t);
extern int	__wctomb_sb(_LC_charmap_t *, char *, wchar_t);
extern size_t	__wcstombs_sb(_LC_charmap_t *, char *,
	const wchar_t *, size_t);
extern int	__mblen_sb(_LC_charmap_t *, const char *, size_t);
extern int	__wcswidth_sb(_LC_charmap_t *, const wchar_t *,
	size_t);
extern int	__wcwidth_sb(_LC_charmap_t *, const wchar_t);
extern int	__mbftowc_sb(_LC_charmap_t *, char *, wchar_t *,
	int (*)(void), int *);
extern wint_t	__fgetwc_sb(_LC_charmap_t *, FILE *);
extern wint_t	__btowc_sb(_LC_charmap_t *, int);
extern int	__wctob_sb(_LC_charmap_t *, wint_t);
extern int	__mbsinit_gen(_LC_charmap_t *, const mbstate_t *);
extern size_t	__mbrlen_sb(_LC_charmap_t *, const char *,
	size_t, mbstate_t *);
extern size_t	__mbrtowc_sb(_LC_charmap_t *, wchar_t *,
	const char *, size_t, mbstate_t *);
extern size_t	__wcrtomb_sb(_LC_charmap_t *, char *, wchar_t,
	mbstate_t *);
extern size_t	__mbsrtowcs_sb(_LC_charmap_t *, wchar_t *,
	const char **, size_t, mbstate_t *);
extern size_t	__wcsrtombs_sb(_LC_charmap_t *, char *,
	const wchar_t **, size_t, mbstate_t *);
/* -------------------- CHARMAP END -------------------- */

/* -------------------- CTYPE BEGIN -------------------- */
extern _LC_ctype_t	*__ctype_init(_LC_locale_t *);
extern int	__ctype_destructor(_LC_locale_t *);
/* ------- CTYPE METHODS ------- */
extern wint_t	__towlower_std(_LC_ctype_t *, wint_t);
extern wint_t	__towupper_std(_LC_ctype_t *, wint_t);
extern wctype_t	__wctype_std(_LC_ctype_t *, const char *);
extern int	__iswctype_std(_LC_ctype_t *, wchar_t, wctype_t);
extern wchar_t	__trwctype_std(_LC_ctype_t *, wchar_t, int);
extern wint_t	__towctrans_std(_LC_ctype_t *, wint_t, wctrans_t);
extern wctrans_t __wctrans_std(_LC_ctype_t *, const char *);

/* -------------------- CTYPE END -------------------- */

/* -------------------- COLLATE BEGIN -------------------- */
extern _LC_collate_t	*__collate_init(_LC_locale_t *);
extern int	__collate_destructor(_LC_locale_t *);
/* ------- COLLATE METHODS ------- */
extern int	__strcoll_C(_LC_collate_t *, const char *, const char *);
extern size_t	__strxfrm_C(_LC_collate_t *, char *, const char *, size_t);
extern int	__wcscoll_C(_LC_collate_t *, const wchar_t *,
	const wchar_t *);
extern size_t	__wcsxfrm_C(_LC_collate_t *, wchar_t *, const wchar_t *,
	size_t);
extern int	__fnmatch_C(_LC_collate_t *, const char *, const char *,
	const char *, int);
extern int	__regcomp_C(_LC_collate_t *, regex_t *, const char *,
	int);
extern size_t	__regerror_std(_LC_collate_t *, int, const regex_t *,
	char *, size_t);
extern int	__regexec_C(_LC_collate_t *, const regex_t *, const char *,
	size_t, regmatch_t *, int);
extern void	__regfree_std(_LC_collate_t *, regex_t *);
/* -------------------- COLLATE END -------------------- */
/* internal routines */
extern char	*do_replacement(_LC_collate_t *, const char *, int, char *);
extern int	forward_collate_sb(_LC_collate_t *, const char *, const char *,
					int);
extern int	forward_collate_std(_LC_collate_t *, const char *, const char *,
					int);
extern int	forw_pos_collate_sb(_LC_collate_t *, const char *, const char *,
					int);
extern int	forw_pos_collate_std(_LC_collate_t *, const char *,
					const char *, int);
extern int	backward_collate_sb(_LC_collate_t *, const char *, const char *,
					int);
extern int	backward_collate_std(_LC_collate_t *, const char *,
					const char *, int);
extern int	back_pos_collate_sb(_LC_collate_t *, const char *, const char *,
					int);
extern int	back_pos_collate_std(_LC_collate_t *, const char *,
					const char *, int);

/* -------------------- TIME BEGIN -------------------- */
extern _LC_time_t	*__time_init(_LC_locale_t *);
extern int	__time_destructor(_LC_locale_t *);
/* ------- TIME METHODS ------- */
extern size_t	__strftime_std(_LC_time_t *, char *, size_t,
	const char *, const struct tm *);
extern char	*__strptime_std(_LC_time_t *, const char *, const char *,
	struct tm *);
extern struct tm	*__getdate_std(_LC_time_t *, const char *);
extern size_t	__wcsftime_std(_LC_time_t *, wchar_t *, size_t,
	const char *, const struct tm *);
/* -------------------- TIME END -------------------- */

/* -------------------- MONETARY BEGIN -------------------- */
extern _LC_monetary_t	*__monetary_init(_LC_locale_t *);
extern int	__monetary_destructor(_LC_locale_t *);
/* ------- MONETARY METHODS ------- */
extern ssize_t	__strfmon_std(_LC_monetary_t *, char *, size_t,
	const char *, va_list);
/* -------------------- MONETARY END -------------------- */

/* -------------------- NUMERIC BEGIN -------------------- */
extern _LC_numeric_t	*__numeric_init(_LC_locale_t *);
extern int	__numeric_destructor(_LC_locale_t *);
/* ------- NUMERIC METHOD ------- */
/* -------------------- NUMERIC END -------------------- */

/* -------------------- MESSAGES BEGIN -------------------- */
extern _LC_messages_t	*__messages_init(_LC_locale_t *);
extern int	__messages_destructor(_LC_locale_t *);
/* ------- MESSAGES METHOD ------- */
/* -------------------- MESSAGES END -------------------- */

/* -------------------- LOCALE BEGIN -------------------- */
extern _LC_locale_t	*__locale_init(_LC_locale_t *);
extern int	__locale_destructor(_LC_locale_t *);
/* ------- LOCALE METHODS ------- */
extern char *__nl_langinfo_std(_LC_locale_t *, nl_item);
extern struct lconv *__localeconv_std(_LC_locale_t *);
/* -------------------- LOCALE END -------------------- */


/*
 * i18n prototypes - strong symbols (weak symbols are in libintl.h)
 */
extern int _wdinit(void);
extern int _wdchkind(wchar_t);
extern int _wdbindf(wchar_t, wchar_t, int);
extern wchar_t *_wddelim(wchar_t, wchar_t, int);
extern wchar_t _mcfiller(void);
extern int _mcwrap(void);
extern char *_textdomain(const char *);
extern char *_bindtextdomain(const char *, const char *);
extern char *_dcgettext(const char *, const char *, const int);
extern char *_dgettext(const char *, const char *);
extern char *_gettext(const char *);
extern int _fnmatch(const char *, const char *, int);


/*
 * defined in port/stdio/doscan.c
 */
extern int _doscan(FILE *, const char *, va_list);
extern int __doscan_u(FILE *iop, const char *fmt, va_list va_Alist);
extern int __wdoscan_u(FILE *, const wchar_t *, va_list);

/*
 * defined in port/stdio/popen.c
 */
extern int _insert(pid_t pid, int fd);
extern pid_t _delete(int fd);

/*
 * defined in port/print/doprnt.c
 */
extern ssize_t	_wdoprnt(const wchar_t *, va_list, FILE *);

/*
 * defined in loc_setup.c
 */
extern const _LC_time_t __C_time_object;

/*
 * defined in get_lcbind.c
 */
extern const char *_lc_get_ctype_flag_name(_LC_ctype_t *, _LC_bind_tag_t,
							_LC_bind_value_t);

/*
 * defined in fgetwc.c
 */
extern wint_t _fgetwc_unlocked(FILE *);
extern wint_t __getwc_xpg5(FILE *);
extern wint_t __fgetwc_xpg5(FILE *);
extern wint_t _getwc(FILE *);

/*
 * defined in fputwc.c
 */
extern wint_t __putwc_xpg5(wint_t, FILE *);
extern wint_t _putwc(wint_t, FILE *);

/*
 * defined in ungetwc.c
 */
extern wint_t	__ungetwc_xpg5(wint_t, FILE *);

/*
 * defined in wscmp.c
 */
extern int	_wcscmp(const wchar_t *, const wchar_t *);

/*
 * defined in wslen.c
 */
extern size_t	_wcslen(const wchar_t *);

/*
 * defined in wscpy.c
 */
extern wchar_t	*_wcscpy(wchar_t *, const wchar_t *);

/*
 * TEMPORARY PROTOTYPES UNTIL WE FIGURE OUT SYNONYMS!!
 */
int _wctomb(char *, wchar_t);
wint_t _towlower(wint_t);
int _doscan(FILE *, const char *, va_list);
int _wcscoll(const wchar_t *, const wchar_t *);
size_t _wcsxfrm(wchar_t *, const wchar_t *, size_t);

#ifdef	__cplusplus
}
#endif

#endif /* _LIBC_H */
