/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thr_stub.m4	1.26	99/09/11 SMI"

/*
 * The table contained in this file defines the list of symbols
 * that libthread interposes on for libc.
 *
 * This table must be maintained in this manner in order to support
 * the linking of libthread behind libc.  If/when this happens
 * then the interface table below will be filled in with the
 * libthread routines and they will be called in the place of libc's
 * entry points.
 *
 * When libthread is linked in before libc this table is not
 * modified and we rely on libthread interposing upon libc's symbols
 * as usual.
 */

#include	<sys/isa_defs.h>
#include	<stdarg.h>
#include	<unistd.h>
#include	<sys/errno.h>
#include	<setjmp.h>
#include	<thread.h>
#include	<pthread.h>
#include	<strings.h>
#include	<time.h>
#include	<stropts.h>
#include	<poll.h>
#include	<signal.h>
#include	<sys/uio.h>
#include	<sys/msg.h>
#include	<sys/resource.h>
#include	<sys/wait.h>

#include	"thr_int.h"

typedef void (*PFrV)();
typedef void (*PFrVt)(void *);
extern ssize_t _write(int fildes, const void *buf, size_t nbyte);
extern unsigned	_libc_alarm(unsigned);
extern int	_libc_close(int);
extern int	_libc_creat(const char *, mode_t);
extern int	_libc_fcntl(int, int, ...);
extern int	_libc_fork();
extern int	_libc_fork1();
extern int	_libc_fsync(int);
extern int	_libc_msync(caddr_t, size_t, int);
extern int	_libc_open(const char *, int, ...);
extern int	_libc_pause(void);
extern ssize_t	_libc_read(int, void *, size_t);
extern int	_libc_setitimer(int, const struct itimerval *,
			struct itimerval *);
extern int	_libc_sigaction(int, const struct sigaction *,
			struct sigaction *);
extern int	_libc_siglongjmp(sigjmp_buf, int);
extern int	_libc_sigpending(sigset_t *);
extern int	_libc_sigprocmask(int, const sigset_t *, sigset_t *);
extern int	_libc_sigsetjmp(sigjmp_buf, int);
extern int	_libc_sigtimedwait(const sigset_t *, siginfo_t *,
			const struct timespec *);
extern int	_libc_sigsuspend(const sigset_t *);
extern int	_libc_sigtimedwait(const sigset_t *, siginfo_t *,
			const struct timespec *);
extern int	_libc_sigwait(sigset_t *);
extern int	_libc_sleep(unsigned);
extern int	_libc_thr_keycreate(thread_key_t *, void(*)(void *));
extern int	_libc_thr_key_delete(thread_key_t);
extern int	_libc_thr_setspecific(thread_key_t, void *);
extern int	_libc_thr_getspecific(thread_key_t, void **);
extern int	_libc_tcdrain(int);
extern pid_t	_libc_wait(int *);
extern pid_t	_libc_waitpid(pid_t, int *, int);
extern ssize_t	_libc_write(int, const void *, size_t);
extern int	_libc_nanosleep(const struct timespec *rqtp,
			struct timespec *rmtp);
#if !defined(_LP64)
extern int	_libc_open64(const char *, int, ...);
extern int	_libc_creat64(const char *, mode_t);
#endif	/* _LP64 */
extern void	*_libc_pthread_getspecific(thread_key_t);
extern int	_libc_kill(pid_t, int);

/* UNIX98 cancellation points */

extern int _libc_getmsg(int, struct strbuf *, struct strbuf *, int *);
extern int _libc_getpmsg(int, struct strbuf *, struct strbuf *, int *, int *);
extern int _libc_putmsg(int, const struct strbuf *, const struct strbuf *, int);
extern int _libc_xpg4_putmsg(int, const struct strbuf *, const struct strbuf *,
				int);
extern int _libc_putpmsg(int, const struct strbuf *, const struct strbuf *,
				int, int);
extern int _libc_xpg4_putpmsg(int, const struct strbuf *, const struct strbuf *,
				int, int);
extern int _libc_lockf(int, int, off_t);
extern ssize_t _libc_msgrcv(int, void *, size_t, long, int);
extern int _libc_msgsnd(int, const void *, size_t, int);
extern int _libc_poll(struct pollfd *, nfds_t, int);
extern ssize_t _libc_pread(int, void *, size_t, off_t);
extern ssize_t _libc_readv(int, const struct iovec *, int);
extern ssize_t _libc_pwrite(int, const void  *, size_t, off_t);
extern ssize_t _libc_writev(int, const struct iovec *, int);
extern int _libc_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int _libc_sigpause(int);
extern int _libc_usleep(useconds_t);
extern pid_t _libc_wait3(int *, int, struct rusage *);
extern int _libc_waitid(idtype_t, id_t, siginfo_t *, int);
#if !defined(_LP64)
extern int _libc_lockf64(int, int, off64_t);
extern ssize_t _libc_pread64(int, void *, size_t, off64_t);
extern ssize_t _libc_pwrite64(int, const void *, size_t, off64_t);
#endif	/* _LP64 */

extern int _libc_mutex_init(mutex_t *mp, int type, void *arg);
extern int _libc_cond_init(cond_t *cvp, int type, void *arg);
extern int _libc_sema_init(sema_t *sp, unsigned int count, int type, void *arg);
extern int _libc_rwlock_init(rwlock_t *rwlp, int type, void *arg);
extern int _libc_pthread_mutex_init(pthread_mutex_t *mutex,
		pthread_mutexattr_t *attr);
extern int _libc_pthread_cond_init(pthread_cond_t *cond,
		const pthread_condattr_t *attr);
extern int _libc_pthread_rwlock_init(pthread_rwlock_t *rwlock,
		pthread_rwlockattr_t *attr);
/*
 * M4 macros for the declaration of libc/libthread interface routines.
 *
 * DEFSTUB#	are used to declare both underscore and non-underscore symbol
 *		pairs, return type defaults to int.
 *
 * RDEFSTUB#	used to declare both underscore and non-underscore symbol
 *		pairs that have a non-int return type
 *
 * VDEFSTUB#	used to declare both underscore and non-underscore symbol
 *		pairs that have a void return type
 *
 * SDEFSTUB#	used to only create a 'strong' function declaration
 *
 * VSDEFSTUB#	used to only create a 'strong' function declaration,
 *		a non-int return type
 *
 * WDEFSTUB#	used to only create a 'weak' function declaration
 *
 * WRDEFSTUB#	used to only create a 'weak' function declaration
 *		that has a non-int return type
 */

define(DEFSTUB0, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(RDEFSTUB0, `
typedef $3 (*_ti_$1_t)(void);

#pragma weak	_$1
#pragma weak	$1 = _$1
$3 _$1(void) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))());
}')dnl

define(VDEFSTUB0, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1(void) {
	(void) (*ti_jmp_table[$2])();
}')dnl

define(VSDEFSTUB0, `
void $1(void) {
	(void) (*ti_jmp_table[$2])();
}')dnl


define(SDEFSTUB0, `
int $1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(WDEFSTUB0, `
#pragma weak	$1
int $1(void) {
	return ((*ti_jmp_table[$2])());
}')dnl

define(DEFSTUB1, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(RDEFSTUB1, `
typedef $3 (*_ti_$1_t)($4);

#pragma weak	_$1
#pragma weak	$1 = _$1
$3 _$1($4 x1) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))(x1));
}')dnl

define(VDEFSTUB1, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1($3 x1) {
	(void) (*ti_jmp_table[$2])(x1);
}')dnl

define(VSDEFSTUB1, `
void $1($3 x1) {
	(void) (*ti_jmp_table[$2])(x1);
}')dnl

define(SDEFSTUB1, `
int $1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(WDEFSTUB1, `
#pragma weak $1
int $1($3 x1) {
	return ((*ti_jmp_table[$2])(x1));
}')dnl

define(DEFSTUB2, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl

define(VDEFSTUB2, `
#pragma weak	_$1
#pragma weak	$1 = _$1
void _$1($3 x1, $4 x2) {
	(void) (*ti_jmp_table[$2])(x1, x2);
}')dnl

define(VSDEFSTUB2, `
void $1($3 x1, $4 x2) {
	(void) (*ti_jmp_table[$2])(x1, x2);
}')dnl

define(SDEFSTUB2, `
int $1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl

define(WDEFSTUB2, `
#pragma weak	$1
int $1($3 x1, $4 x2) {
	return ((*ti_jmp_table[$2])(x1, x2));
}')dnl

define(DEFSTUB3, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl

define(SDEFSTUB3, `
int $1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl

define(WDEFSTUB3, `
#pragma weak	$1
int $1($3 x1, $4 x2, $5 x3) {
	return ((*ti_jmp_table[$2])(x1, x2, x3));
}')dnl

define(WDEFSTUB4, `
#pragma weak	$1
int $1($3 x1, $4 x2, $5 x3, $6 x4) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4));
}')dnl

define(WDEFSTUB5, `
#pragma weak	$1
int $1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5));
}')dnl


/*
 * This is dubious, but the only way I can think of to defeat the
 * C type system in portable way.  The idea is to cast the function
 * table entry (which contains pointers-to-functions-that-return-int)
 * to a pointer-to-function that returns the type we want, then
 * dereference that pointer.  This prevents the compiler from
 * inserting code to do sign extension or truncation.
 */
define(WRDEFSTUB1, `
typedef	$3 (*_ti_$1_t)($4);

#pragma weak	$1
$3 $1($4 x1) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))(x1));
}')dnl

define(WRDEFSTUB3, `
typedef	$3 (*_ti_$1_t)($4, $5, $6);

#pragma weak	$1
$3 $1($4 x1, $5 x2, $6 x3) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))(x1, x2, x3));
}')dnl

define(WRDEFSTUB4, `
typedef	$3 (*_ti_$1_t)($4, $5, $6, $7);

#pragma weak	$1
$3 $1($4 x1, $5 x2, $6 x3, $7 x4) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))(x1, x2, x3, x4));
}')dnl

define(WRDEFSTUB5, `
typedef	$3 (*_ti_$1_t)($4, $5, $6, $7, $8);

#pragma weak	$1
$3 $1($4 x1, $5 x2, $6 x3, $7 x4, $8 x5) {
	return ((*(*((_ti_$1_t *)(&ti_jmp_table[$2]))))(x1, x2, x3, x4, x5));
}')dnl


define(DEFSTUB4, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4));
}')dnl

define(VSDEFSTUB4, `
void $1($3 x1, $4 x2, $5 x3, $6 x4) {
	(void) (*ti_jmp_table[$2])(x1, x2, x3, x4);
}')dnl

define(SDEFSTUB4, `
int $1($3 x1, $4 x2, $5 x3, $6 x4) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4));
}')dnl

define(DEFSTUB5, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5));
}')dnl

define(SDEFSTUB5, `
int $1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5));
}')dnl

define(DEFSTUB6, `
#pragma weak	_$1
#pragma weak	$1 = _$1
int _$1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5, $8 x6) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5, x6));
}')dnl

define(SDEFSTUB6, `
int $1($3 x1, $4 x2, $5 x3, $6 x4, $7 x5, $8 x6) {
	return ((*ti_jmp_table[$2])(x1, x2, x3, x4, x5, x6));
}')dnl

static int
_return_zero()
{
	return (0);
}

static int
_return_one()
{
	return (1);
}

static int
_return_negone()
{
	return (-1);
}

static int
_return_notsup()
{
	return (ENOTSUP);
}


/*
 * These are the 'default' values for libc's table that point
 * directly to libc's routines.  This table is used to re-initialize
 * the interface table if libthread is 'dlclosed'.
 *
 */
static	int (*	ti_def_table[TI_MAX])() = {
/* 0 */		0,			/* TI_NULL */
/* 1 */		_return_zero,		/* TI_MUTEX_LOCK */
/* 2 */		_return_zero,		/* TI_MUTEX_UNLOCK */
/* 3 */		_return_zero,		/* TI_LRW_RDLOCK */
/* 4 */		_return_zero,		/* TI_LRW_WRLOCK */
/* 5 */		_return_zero,		/* TI_LRW_UNLOCK */
/* 6 */		_return_zero,		/* TI_BIND_GUARD */
/* 7 */		_return_zero,		/* TI_BIND_CLEAR */
/* 8 */		_return_zero,		/* TI_LATFORK */
/* 9 */		_return_one,		/* TI_THRSELF */
/* 10 */	0,			/* TI_VERSION */
/* 11 */	_return_zero,		/* TI_COND_BROAD */
/* 12 */	_return_zero,		/* TI_COND_DESTROY */
/* 13 */	_libc_cond_init,	/* TI_COND_INIT */
/* 14 */	_return_zero,		/* TI_COND_SIGNAL */
/* 15 */	_return_zero,		/* TI_COND_TWAIT */
/* 16 */	_return_zero,		/* TI_COND_WAIT */
/* 17 */	_libc_fork,		/* TI_FORK */
/* 18 */	_libc_fork1,		/* TI_FORK1 */
/* 19 */	_return_zero,		/* TI_MUTEX_DEST */
/* 20 */	_return_one,		/* TI_MUTEX_HELD */
/* 21 */	_libc_mutex_init,	/* TI_MUTEX_INIT */
/* 22 */	_return_zero,		/* TI_MUTEX_TRYLCK */
/* 23 */	_return_zero,		/* TI_ATFORK */
/* 24 */	_return_one,		/* TI_RW_RDHELD */
/* 25 */	_return_zero,		/* TI_RW_RDLOCK */
/* 26 */	_return_zero,		/* TI_RW_WRLOCK */
/* 27 */	_return_zero,		/* TI_RW_UNLOCK */
/* 28 */	_return_zero,		/* TI_TRYRDLOCK */
/* 29 */	_return_zero,		/* TI_TRYWRLOCK */
/* 30 */	_return_one,		/* TI_RW_WRHELD */
/* 31 */	_libc_rwlock_init,	/* TI_RW_LOCKINIT */
/* 32 */	_return_one,		/* TI_SEM_HELD */
/* 33 */	_libc_sema_init,		/* TI_SEM_INIT */
/* 34 */	_return_zero,		/* TI_SEM_POST */
/* 35 */	_return_zero,		/* TI_SEM_TRYWAIT */
/* 36 */	_return_zero,		/* TI_SEM_WAIT */
/* 37 */	_libc_sigaction,	/* TI_SIGACTION */
/* 38 */	_libc_sigprocmask,	/* TI_SIGPROCMASK */
/* 39 */	_libc_sigwait,		/* TI_SIGWAIT */
/* 40 */	_libc_sleep,		/* TI_SLEEP */
/* 41 */	_return_zero,		/* TI_THR_CONT */
/* 42 */	_return_negone,		/* TI_THR_CREATE */
/* 43 */	_return_zero,		/* TI_THR_ERRNOP */
/* 44 */	_return_zero,		/* TI_THR_EXIT */
/* 45 */	_return_zero,		/* TI_THR_GETCONC */
/* 46 */	_return_zero,		/* TI_THR_GETPRIO */
/* 47 */	_libc_thr_getspecific,	/* TI_THR_GETSPEC */
/* 48 */	_return_zero,		/* TI_THR_JOIN */
/* 49 */	_libc_thr_keycreate,	/* TI_THR_KEYCREAT */
/* 50 */	_return_zero,		/* TI_THR_KILL */
/* 51 */	_return_negone,		/* TI_THR_MAIN */
/* 52 */	_return_zero,		/* TI_THR_SETCONC */
/* 53 */	_return_zero,		/* TI_THR_SETPRIO */
/* 54 */	_libc_thr_setspecific,	/* TI_THR_SETSPEC */
/* 55 */	_return_zero,		/* TI_THR_SIGSET */
/* 56 */	_return_notsup,		/* TI_THR_STKSEGMENT */
/* 57 */	_return_zero,		/* TI_THR_SUSPEND */
/* 58 */	_return_zero,		/* TI_THR_YIELD */
/* 59 */	_libc_close,		/* TI_CLOSE */
/* 60 */	_libc_creat,		/* TI_CREAT */
/* 61 */	(int (*)())_libc_fcntl,	/* TI_FCNTL */
/* 62 */	_libc_fsync,		/* TI_FSYNC */
/* 63 */	_libc_msync,		/* TI_MSYNC */
/* 64 */	(int (*)())_libc_open,	/* TI_OPEN */
/* 65 */	_libc_pause,		/* TI_PAUSE */
/* 66 */	(int (*)())_libc_read,	/* TI_READ */
/* 67 */	_libc_sigsuspend,	/* TI_SIGSUSPEND */
/* 68 */	_libc_tcdrain,		/* TI_TCDRAIN */
/* 69 */	(int (*)())_libc_wait,	/* TI_WAIT */
/* 70 */	(int (*)())_libc_waitpid,	/* TI_WAITPID */
/* 71 */	(int (*)())_libc_write,	/* TI_WRITE */
/* 72 */	_return_zero,		/* TI_PCOND_BROAD */
/* 73 */	_return_zero,		/* TI_PCOND_DEST */
/* 74 */	_libc_pthread_cond_init,	/* TI_PCOND_INIT */
/* 75 */	_return_zero,		/* TI_PCOND_SIGNAL */
/* 76 */	_return_zero,		/* TI_PCOND_TWAIT */
/* 77 */	_return_zero,		/* TI_PCOND_WAIT */
/* 78 */	_return_zero,		/* TI_PCONDA_DEST */
/* 79 */	_return_zero,		/* TI_PCONDA_GETPS */
/* 80 */	_return_zero,		/* TI_PCONDA_INIT */
/* 81 */	_return_zero,		/* TI_PCONDA_SETPS */
/* 82 */	_return_zero,		/* TI_PMUTEX_DESTROY */
/* 83 */	_return_zero,		/* TI_PMUTEX_GPC */
/* 84 */	_libc_pthread_mutex_init,	/* TI_PMUTEX_INIT */
/* 85 */	_return_zero,		/* TI_PMUTEX_LOCK */
/* 86 */	_return_zero,		/* TI_PMUTEX_SPC */
/* 87 */	_return_zero,		/* TI_PMUTEX_TRYL */
/* 88 */	_return_zero,		/* TI_PMUTEX_UNLCK */
/* 89 */	_return_zero,		/* TI_PMUTEXA_DEST */
/* 90 */	_return_zero,		/* TI_PMUTEXA_GPC */
/* 91 */	_return_zero,		/* TI_PMUTEXA_GP */
/* 92 */	_return_zero,		/* TI_PMUTEXA_GPS */
/* 93 */	_return_zero,		/* TI_PMUTEXA_INIT */
/* 94 */	_return_zero,		/* TI_PMUTEXA_SPC */
/* 95 */	_return_zero,		/* TI_PMUTEXA_SP */
/* 96 */	_return_zero,		/* TI_PMUTEXA_SPS */
/* 97 */	_return_negone,		/* TI_THR_MINSTACK */
/* 98 */	_libc_sigtimedwait,	/* TI_SIGTIMEDWAIT */
/* 99 */	(int (*)())_libc_alarm,	/* TI_ALARM */
/* 100 */	_libc_setitimer,	/* TI_SETITIMER */
/* 101 */	_libc_siglongjmp,	/* TI_SIGLONGJMP */
/* 102 */	0,			/* TI_SIGSETGJMP */
/* 103 */	_libc_sigpending,	/* TI_SIGPENDING */
/* 104 */	_libc_nanosleep,	/* TI__NANOSLEEP */
#if defined(_LP64)
/* 105 */	_return_negone,		/* TI_OPEN64 */
/* 106 */	_return_negone,		/* TI_CREAT64 */
#else
/* 105 */	(int (*)())_libc_open64,	/* TI_OPEN64 */
/* 106 */	_libc_creat64,		/* TI_CREAT64 */
#endif
/* 107 */	_return_zero,		/* TI_RWLCKDESTROY */
/* 108 */	_return_zero,		/* TI_SEMADESTROY */
/* 109 */	_return_one,		/* TI_PSELF */
/* 110 */	_return_zero,		/* TI_PSIGMASK */
/* 111 */	_return_zero,		/* TI_PATTR_DEST */
/* 112 */	_return_zero,		/* TI_PATTR_GDS */
/* 113 */	_return_zero,		/* TI_PATTR_GIS */
/* 114 */	_return_zero,		/* TI_PATTR_GSPA */
/* 115 */	_return_zero,		/* TI_PATTR_GSPO */
/* 116 */	_return_zero,		/* TI_PATTR_GSCOP */
/* 117 */	_return_zero,		/* TI_PATTR_GSTAD */
/* 118 */	_return_zero,		/* TI_PATTR_GSTSZ */
/* 119 */	_return_zero,		/* TI_PATTR_INIT */
/* 120 */	_return_zero,		/* TI_PATTR_SDEST */
/* 121 */	_return_zero,		/* TI_PATTR_SIS */
/* 122 */	_return_zero,		/* TI_PATTR_SSPA */
/* 123 */	_return_zero,		/* TI_PATTR_SSPO */
/* 124 */	_return_zero,		/* TI_PATTR_SSCOP */
/* 125 */	_return_zero,		/* TI_PATTR_SSTAD */
/* 126 */	_return_zero,		/* TI_PATTR_SSTSZ */
/* 127 */	_return_zero,		/* TI_PCANCEL */
/* 128 */	_return_zero,		/* TI_PCLEANUP_POP */
/* 129 */	_return_zero,		/* TI_PCLEANUP_PSH */
/* 130 */	_return_negone,		/* TI_PCREATE */
/* 131 */	_return_zero,		/* TI_PDETACH */
/* 132 */	_return_zero,		/* TI_PEQUAL */
/* 133 */	_return_zero,		/* TI_PEXIT */
/* 134 */	_return_zero,		/* TI_PGSCHEDPARAM */
/* 135 */	(int (*)()) _libc_pthread_getspecific,	/* TI_PGSPECIFIC */
/* 136 */	_return_zero,		/* TI_PJOIN */
/* 137 */	_libc_thr_keycreate,	/* TI_PKEY_CREATE */
/* 138 */	_libc_thr_key_delete,	/* TI_PKEY_DELETE */
/* 139 */	_return_zero,		/* TI_PKILL */
/* 140 */	_return_zero,		/* TI_PONCE */
/* 141 */	_return_zero,		/* TI_PSCANCELST */
/* 142 */	_return_zero,		/* TI_PSCANCELTYPE */
/* 143 */	_return_zero,		/* TI_PSSCHEDPARAM */
/* 144 */	_libc_thr_setspecific,	/* TI_PSETSPECIFIC */
/* 145 */	_return_zero,		/* TI_PTESTCANCEL */
/* 146 */	_libc_kill,		/* TI_KILL */
/* 147 */	_return_zero,		/* TI_PGETCONCUR */
/* 148 */	_return_zero,		/* TI_PSETCONCUR */
/* 149 */	_return_zero,		/* TI_PATTR_GGDSZ */
/* 150 */	_return_zero,		/* TI_PATTR_SGDSZ */
/* 151 */	_return_zero,		/* TI_PMUTEXA_GTYP */
/* 152 */	_return_zero,		/* TI_PMUTEXA_STYP */
/* 153 */	_libc_pthread_rwlock_init,	/* TI_PRWL_INIT */
/* 154 */	_return_zero,		/* TI_PRWL_DEST */
/* 155 */	_return_zero,		/* TI_PRWLA_INIT */
/* 156 */	_return_zero,		/* TI_PRWLA_DEST */
/* 157 */	_return_zero,		/* TI_PRWLA_GPSH */
/* 158 */	_return_zero,		/* TI_PRWLA_SPSH */
/* 159 */	_return_zero,		/* TI_PRWL_RDLK */
/* 160 */	_return_zero,		/* TI_PRWL_TRDLK */
/* 161 */	_return_zero,		/* TI_PRWL_WRLK */
/* 162 */	_return_zero,		/* TI_PRWL_TWRLK */
/* 163 */	_return_zero,		/* TI_PRWL_UNLK */
/* 164 */	_libc_getmsg,		/* TI_GETMSG */
/* 165 */	_libc_getpmsg,		/* TI_GETPMSG */
/* 166 */	_libc_putmsg,		/* TI_PUTMSG */
/* 167 */	_libc_putpmsg,		/* TI_PUTPMSG */
/* 168 */	_libc_lockf,		/* TI_LOCKF */
/* 169 */	(int (*)())_libc_msgrcv,	/* TI_MSGRCV */
/* 170 */	_libc_msgsnd,		/* TI_MSGSND */
/* 171 */	_libc_poll,		/* TI_POLL */
/* 172 */	(int (*)())_libc_pread,	/* TI_PREAD */
/* 173 */	(int (*)())_libc_readv,	/* TI_READV */
/* 174 */	(int (*)())_libc_pwrite,	/* TI_PWRITE */
/* 175 */	(int (*)())_libc_writev,	/* TI_WRITEV */
/* 176 */	_libc_select,		/* TI_SELECT */
/* 177 */	_libc_sigpause,		/* TI_SIGPAUSE */
/* 178 */	_libc_usleep,		/* TI_USLEEP */
/* 179 */	(int (*)())_libc_wait3,	/* TI_WAIT3 */
/* 180 */	_libc_waitid,		/* TI_WAITID */
#if defined(_LP64)
/* 181 */	_return_negone,		/* TI_LOCKF64 */
/* 182 */	_return_negone,		/* TI_PREAD64 */
/* 183 */	_return_negone,		/* TI_PWRITE64 */
#else
/* 181 */	_libc_lockf64,		/* TI_LOCKF64 */
/* 182 */	(int (*)())_libc_pread64,	/* TI_PREAD64 */
/* 183 */	(int (*)())_libc_pwrite64,	/* TI_PWRITE64 */
#endif
/* 184 */	_libc_xpg4_putmsg,	/* TI_XPG4PUTMSG */
/* 185 */	_libc_xpg4_putpmsg,	/* TI_XPG4PUTPMSG */
};

/*
 * Libc/Libthread interface table.
 */
static	int (*	ti_jmp_table[TI_MAX])() = {
/* 0 */		0,			/* TI_NULL */
/* 1 */		_return_zero,		/* TI_MUTEX_LOCK */
/* 2 */		_return_zero,		/* TI_MUTEX_UNLOCK */
/* 3 */		_return_zero,		/* TI_LRW_RDLOCK */
/* 4 */		_return_zero,		/* TI_LRW_WRLOCK */
/* 5 */		_return_zero,		/* TI_LRW_UNLOCK */
/* 6 */		_return_zero,		/* TI_BIND_GUARD */
/* 7 */		_return_zero,		/* TI_BIND_CLEAR */
/* 8 */		_return_zero,		/* TI_LATFORK */
/* 9 */		_return_one,		/* TI_THRSELF */
/* 10 */	0,			/* TI_VERSION */
/* 11 */	_return_zero,		/* TI_COND_BROAD */
/* 12 */	_return_zero,		/* TI_COND_DESTROY */
/* 13 */	_libc_cond_init,	/* TI_COND_INIT */
/* 14 */	_return_zero,		/* TI_COND_SIGNAL */
/* 15 */	_return_zero,		/* TI_COND_TWAIT */
/* 16 */	_return_zero,		/* TI_COND_WAIT */
/* 17 */	_libc_fork,		/* TI_FORK */
/* 18 */	_libc_fork1,		/* TI_FORK1 */
/* 19 */	_return_zero,		/* TI_MUTEX_DEST */
/* 20 */	_return_one,		/* TI_MUTEX_HELD */
/* 21 */	_libc_mutex_init,	/* TI_MUTEX_INIT */
/* 22 */	_return_zero,		/* TI_MUTEX_TRYLCK */
/* 23 */	_return_zero,		/* TI_ATFORK */
/* 24 */	_return_one,		/* TI_RW_RDHELD */
/* 25 */	_return_zero,		/* TI_RW_RDLOCK */
/* 26 */	_return_zero,		/* TI_RW_WRLOCK */
/* 27 */	_return_zero,		/* TI_RW_UNLOCK */
/* 28 */	_return_zero,		/* TI_TRYRDLOCK */
/* 29 */	_return_zero,		/* TI_TRYWRLOCK */
/* 30 */	_return_one,		/* TI_RW_WRHELD */
/* 31 */	_libc_rwlock_init,	/* TI_RW_LOCKINIT */
/* 32 */	_return_one,		/* TI_SEM_HELD */
/* 33 */	_libc_sema_init,		/* TI_SEM_INIT */
/* 34 */	_return_zero,		/* TI_SEM_POST */
/* 35 */	_return_zero,		/* TI_SEM_TRYWAIT */
/* 36 */	_return_zero,		/* TI_SEM_WAIT */
/* 37 */	_libc_sigaction,	/* TI_SIGACTION */
/* 38 */	_libc_sigprocmask,	/* TI_SIGPROCMASK */
/* 39 */	_libc_sigwait,		/* TI_SIGWAIT */
/* 40 */	_libc_sleep,		/* TI_SLEEP */
/* 41 */	_return_zero,		/* TI_THR_CONT */
/* 42 */	_return_negone,		/* TI_THR_CREATE */
/* 43 */	_return_zero,		/* TI_THR_ERRNOP */
/* 44 */	_return_zero,		/* TI_THR_EXIT */
/* 45 */	_return_zero,		/* TI_THR_GETCONC */
/* 46 */	_return_zero,		/* TI_THR_GETPRIO */
/* 47 */	_libc_thr_getspecific,	/* TI_THR_GETSPEC */
/* 48 */	_return_zero,		/* TI_THR_JOIN */
/* 49 */	_libc_thr_keycreate,	/* TI_THR_KEYCREAT */
/* 50 */	_return_zero,		/* TI_THR_KILL */
/* 51 */	_return_negone,		/* TI_THR_MAIN */
/* 52 */	_return_zero,		/* TI_THR_SETCONC */
/* 53 */	_return_zero,		/* TI_THR_SETPRIO */
/* 54 */	_libc_thr_setspecific,	/* TI_THR_SETSPEC */
/* 55 */	_return_zero,		/* TI_THR_SIGSET */
/* 56 */	_return_notsup,		/* TI_THR_STKSEGMENT */
/* 57 */	_return_zero,		/* TI_THR_SUSPEND */
/* 58 */	_return_zero,		/* TI_THR_YIELD */
/* 59 */	_libc_close,		/* TI_CLOSE */
/* 60 */	_libc_creat,		/* TI_CREAT */
/* 61 */	(int (*)())_libc_fcntl,	/* TI_FCNTL */
/* 62 */	_libc_fsync,		/* TI_FSYNC */
/* 63 */	_libc_msync,		/* TI_MSYNC */
/* 64 */	(int (*)())_libc_open,	/* TI_OPEN */
/* 65 */	_libc_pause,		/* TI_PAUSE */
/* 66 */	(int (*)())_libc_read,	/* TI_READ */
/* 67 */	_libc_sigsuspend,	/* TI_SIGSUSPEND */
/* 68 */	_libc_tcdrain,		/* TI_TCDRAIN */
/* 69 */	(int (*)())_libc_wait,	/* TI_WAIT */
/* 70 */	(int (*)())_libc_waitpid,	/* TI_WAITPID */
/* 71 */	(int (*)())_libc_write,	/* TI_WRITE */
/* 72 */	_return_zero,		/* TI_PCOND_BROAD */
/* 73 */	_return_zero,		/* TI_PCOND_DEST */
/* 74 */	_libc_pthread_cond_init,	/* TI_PCOND_INIT */
/* 75 */	_return_zero,		/* TI_PCOND_SIGNAL */
/* 76 */	_return_zero,		/* TI_PCOND_TWAIT */
/* 77 */	_return_zero,		/* TI_PCOND_WAIT */
/* 78 */	_return_zero,		/* TI_PCONDA_DEST */
/* 79 */	_return_zero,		/* TI_PCONDA_GETPS */
/* 80 */	_return_zero,		/* TI_PCONDA_INIT */
/* 81 */	_return_zero,		/* TI_PCONDA_SETPS */
/* 82 */	_return_zero,		/* TI_PMUTEX_DESTROY */
/* 83 */	_return_zero,		/* TI_PMUTEX_GPC */
/* 84 */	_libc_pthread_mutex_init,	/* TI_PMUTEX_INIT */
/* 85 */	_return_zero,		/* TI_PMUTEX_LOCK */
/* 86 */	_return_zero,		/* TI_PMUTEX_SPC */
/* 87 */	_return_zero,		/* TI_PMUTEX_TRYL */
/* 88 */	_return_zero,		/* TI_PMUTEX_UNLCK */
/* 89 */	_return_zero,		/* TI_PMUTEXA_DEST */
/* 90 */	_return_zero,		/* TI_PMUTEXA_GPC */
/* 91 */	_return_zero,		/* TI_PMUTEXA_GP */
/* 92 */	_return_zero,		/* TI_PMUTEXA_GPS */
/* 93 */	_return_zero,		/* TI_PMUTEXA_INIT */
/* 94 */	_return_zero,		/* TI_PMUTEXA_SPC */
/* 95 */	_return_zero,		/* TI_PMUTEXA_SP */
/* 96 */	_return_zero,		/* TI_PMUTEXA_SPS */
/* 97 */	_return_negone,		/* TI_THR_MINSTACK */
/* 98 */	_libc_sigtimedwait,	/* TI_SIGTIMEDWAIT */
/* 99 */	(int (*)())_libc_alarm,	/* TI_ALARM */
/* 100 */	_libc_setitimer,	/* TI_SETITIMER */
/* 101 */	_libc_siglongjmp,	/* TI_SIGLONGJMP */
/* 102 */	0,			/* TI_SIGSETGJMP */
/* 103 */	_libc_sigpending,	/* TI_SIGPENDING */
/* 104 */	_libc_nanosleep,	/* TI__NANOSLEEP */
#if defined(_LP64)
/* 105 */	_return_negone,		/* TI_OPEN64 */
/* 106 */	_return_negone,		/* TI_CREAT64 */
#else
/* 105 */	(int (*)())_libc_open64,	/* TI_OPEN64 */
/* 106 */	_libc_creat64,		/* TI_CREAT64 */
#endif
/* 107 */	_return_zero,		/* TI_RWLCKDESTROY */
/* 108 */	_return_zero,		/* TI_SEMADESTROY */
/* 109 */	_return_one,		/* TI_PSELF */
/* 110 */	_return_zero,		/* TI_PSIGMASK */
/* 111 */	_return_zero,		/* TI_PATTR_DEST */
/* 112 */	_return_zero,		/* TI_PATTR_GDS */
/* 113 */	_return_zero,		/* TI_PATTR_GIS */
/* 114 */	_return_zero,		/* TI_PATTR_GSPA */
/* 115 */	_return_zero,		/* TI_PATTR_GSPO */
/* 116 */	_return_zero,		/* TI_PATTR_GSCOP */
/* 117 */	_return_zero,		/* TI_PATTR_GSTAD */
/* 118 */	_return_zero,		/* TI_PATTR_GSTSZ */
/* 119 */	_return_zero,		/* TI_PATTR_INIT */
/* 120 */	_return_zero,		/* TI_PATTR_SDEST */
/* 121 */	_return_zero,		/* TI_PATTR_SIS */
/* 122 */	_return_zero,		/* TI_PATTR_SSPA */
/* 123 */	_return_zero,		/* TI_PATTR_SSPO */
/* 124 */	_return_zero,		/* TI_PATTR_SSCOP */
/* 125 */	_return_zero,		/* TI_PATTR_SSTAD */
/* 126 */	_return_zero,		/* TI_PATTR_SSTSZ */
/* 127 */	_return_zero,		/* TI_PCANCEL */
/* 128 */	_return_zero,		/* TI_PCLEANUP_POP */
/* 129 */	_return_zero,		/* TI_PCLEANUP_PSH */
/* 130 */	_return_negone,		/* TI_PCREATE */
/* 131 */	_return_zero,		/* TI_PDETACH */
/* 132 */	_return_zero,		/* TI_PEQUAL */
/* 133 */	_return_zero,		/* TI_PEXIT */
/* 134 */	_return_zero,		/* TI_PGSCHEDPARAM */
/* 135 */	(int (*)()) _libc_pthread_getspecific,	/* TI_PGSPECIFIC */
/* 136 */	_return_zero,		/* TI_PJOIN */
/* 137 */	_libc_thr_keycreate,	/* TI_PKEY_CREATE */
/* 138 */	_libc_thr_key_delete,	/* TI_PKEY_DELETE */
/* 139 */	_return_zero,		/* TI_PKILL */
/* 140 */	_return_zero,		/* TI_PONCE */
/* 141 */	_return_zero,		/* TI_PSCANCELST */
/* 142 */	_return_zero,		/* TI_PSCANCELTYPE */
/* 143 */	_return_zero,		/* TI_PSSCHEDPARAM */
/* 144 */	_libc_thr_setspecific,	/* TI_PSETSPECIFIC */
/* 145 */	_return_zero,		/* TI_PTESTCANCEL */
/* 146 */	_libc_kill,		/* TI_KILL */
/* 147 */	_return_zero,		/* TI_PGETCONCUR */
/* 148 */	_return_zero,		/* TI_PSETCONCUR */
/* 149 */	_return_zero,		/* TI_PATTR_GGDSZ */
/* 150 */	_return_zero,		/* TI_PATTR_SGDSZ */
/* 151 */	_return_zero,		/* TI_PMUTEXA_GTYP */
/* 152 */	_return_zero,		/* TI_PMUTEXA_STYP */
/* 153 */	_libc_pthread_rwlock_init,		/* TI_PRWL_INIT */
/* 154 */	_return_zero,		/* TI_PRWL_DEST */
/* 155 */	_return_zero,		/* TI_PRWLA_INIT */
/* 156 */	_return_zero,		/* TI_PRWLA_DEST */
/* 157 */	_return_zero,		/* TI_PRWLA_GPSH */
/* 158 */	_return_zero,		/* TI_PRWLA_SPSH */
/* 159 */	_return_zero,		/* TI_PRWL_RDLK */
/* 160 */	_return_zero,		/* TI_PRWL_TRDLK */
/* 161 */	_return_zero,		/* TI_PRWL_WRLK */
/* 162 */	_return_zero,		/* TI_PRWL_TWRLK */
/* 163 */	_return_zero,		/* TI_PRWL_UNLK */
/* 164 */	_libc_getmsg,		/* TI_GETMSG */
/* 165 */	_libc_getpmsg,		/* TI_GETPMSG */
/* 166 */	_libc_putmsg,		/* TI_PUTMSG */
/* 167 */	_libc_putpmsg,		/* TI_PUTPMSG */
/* 168 */	_libc_lockf,		/* TI_LOCKF */
/* 169 */	(int (*)())_libc_msgrcv,	/* TI_MSGRCV */
/* 170 */	_libc_msgsnd,		/* TI_MSGSND */
/* 171 */	_libc_poll,		/* TI_POLL */
/* 172 */	(int (*)())_libc_pread,	/* TI_PREAD */
/* 173 */	(int (*)())_libc_readv,	/* TI_READV */
/* 174 */	(int (*)())_libc_pwrite,	/* TI_PWRITE */
/* 175 */	(int (*)())_libc_writev,	/* TI_WRITEV */
/* 176 */	_libc_select,		/* TI_SELECT */
/* 177 */	_libc_sigpause,		/* TI_SIGPAUSE */
/* 178 */	_libc_usleep,		/* TI_USLEEP */
/* 179 */	(int (*)())_libc_wait3,	/* TI_WAIT3 */
/* 180 */	_libc_waitid,		/* TI_WAITID */
#if defined(_LP64)
/* 181 */	_return_negone,		/* TI_LOCKF64 */
/* 182 */	_return_negone,		/* TI_PREAD64 */
/* 183 */	_return_negone,		/* TI_PWRITE64 */
#else
/* 181 */	_libc_lockf64,		/* TI_LOCKF64 */
/* 182 */	(int (*)())_libc_pread64,	/* TI_PREAD64 */
/* 183 */	(int (*)())_libc_pwrite64,	/* TI_PWRITE64 */
#endif
/* 184 */	_libc_xpg4_putmsg,	/* TI_XPG4PUTMSG */
/* 185 */	_libc_xpg4_putpmsg,	/* TI_XPG4PUTPMSG */
};

int __threaded;

/*
 * _thr_libthread() is used to identify the link order of libc.so vs.
 * libthread.so.  There is a copy of this routine in both libraries:
 *
 *	libc:_thr_libthread():		returns 0
 *	libthread:_thr_libthread():	returns 1
 *
 * A call to this routine can be used to determine whether the libc threads
 * interface needs to be initialized or not.
 */
int
_thr_libthread()
{
	return (0);
}

void
_libc_threads_interface(Thr_interface * ti_funcs)
{
	int tag;
	if (ti_funcs) {
		__threaded = 1;
		if (_thr_libthread() != 0)
			return;
		for (tag = ti_funcs->ti_tag; tag; tag = (++ti_funcs)->ti_tag) {
			if (tag >= TI_MAX) {
				const char * err_mesg = "libc: warning: "
					"libc/libthread interface mismatch: "
					"unknown tag value ignored\n";
				_write(2, err_mesg, strlen(err_mesg));
			}
			if (ti_funcs->ti_un.ti_func != 0)
				ti_jmp_table[tag] = ti_funcs->ti_un.ti_func;
		}
	} else {
		__threaded = 0;
		if (_thr_libthread() != 0)
			return;
		for (tag = 0; tag < TI_MAX; tag++)
			ti_jmp_table[tag] = ti_def_table[tag];
	}
}


/*
 * m4 Macros to define 'stub' routines for all libthread/libc
 * interface routines.
 */

DEFSTUB1(mutex_lock, TI_MUTEX_LOCK, mutex_t *)
DEFSTUB1(mutex_unlock, TI_MUTEX_UNLOCK, mutex_t *)
DEFSTUB1(rw_rdlock, TI_RW_RDLOCK, rwlock_t *)
DEFSTUB1(rw_wrlock, TI_RW_WRLOCK, rwlock_t *)
DEFSTUB1(rw_unlock, TI_RW_UNLOCK, rwlock_t *)
RDEFSTUB0(thr_self, TI_THRSELF, thread_t)
DEFSTUB1(cond_broadcast, TI_COND_BROAD, cond_t *)
DEFSTUB1(cond_destroy, TI_COND_DESTROY, cond_t *)
DEFSTUB3(cond_init, TI_COND_INIT, cond_t *, int, void *)
DEFSTUB1(cond_signal, TI_COND_SIGNAL, cond_t *)
DEFSTUB3(cond_timedwait, TI_COND_TWAIT, cond_t *, mutex_t *, timestruc_t *)
DEFSTUB2(cond_wait, TI_COND_WAIT, cond_t *, mutex_t *)
DEFSTUB0(fork, TI_FORK)
DEFSTUB0(fork1, TI_FORK1)
DEFSTUB1(mutex_destroy, TI_MUTEX_DEST, mutex_t *)
DEFSTUB1(mutex_held, TI_MUTEX_HELD, mutex_t *)
DEFSTUB3(mutex_init, TI_MUTEX_INIT, mutex_t *, int, void *)
DEFSTUB1(mutex_trylock, TI_MUTEX_TRYLCK, mutex_t *)
DEFSTUB3(pthread_atfork, TI_ATFORK, PFrV, PFrV, PFrV)
DEFSTUB1(rw_read_held, TI_RW_RDHELD, rwlock_t *)
DEFSTUB1(rw_tryrdlock, TI_TRYRDLOCK, rwlock_t *)
DEFSTUB1(rw_trywrlock, TI_TRYWRLOCK, rwlock_t *)
DEFSTUB1(rw_write_held, TI_RW_WRHELD, rwlock_t *)
DEFSTUB3(rwlock_init, TI_RWLOCKINIT, rwlock_t *, int, void *)
DEFSTUB1(sema_held, TI_SEM_HELD, sema_t *)
DEFSTUB4(sema_init, TI_SEM_INIT, sema_t *, unsigned int, int, void *)
DEFSTUB1(sema_post, TI_SEM_POST, sema_t *)
DEFSTUB1(sema_trywait, TI_SEM_TRYWAIT, sema_t *)
DEFSTUB1(sema_wait, TI_SEM_WAIT, sema_t *)
DEFSTUB3(sigaction, TI_SIGACTION, int, const struct sigaction *,
		struct sigaction *)
DEFSTUB3(sigprocmask, TI_SIGPROCMASK, int, sigset_t *, sigset_t *)
DEFSTUB1(sigwait, TI_SIGWAIT, sigset_t *)
DEFSTUB1(sleep, TI_SLEEP, unsigned int)
DEFSTUB1(thr_continue, TI_THR_CONT, thread_t)
DEFSTUB6(thr_create, TI_THR_CREATE, void *, size_t, void *,
		void *, long, thread_t *)
DEFSTUB0(thr_errnop, TI_THR_ERRNOP)
VDEFSTUB1(thr_exit, TI_THR_EXIT, void *)
DEFSTUB0(thr_getconcurrency, TI_THR_GETCONC)
DEFSTUB2(thr_getprio, TI_THR_GETPRIO, thread_t, int *)
DEFSTUB2(thr_getspecific, TI_THR_GETSPEC, thread_key_t, void **)
DEFSTUB3(thr_join, TI_THR_JOIN, thread_t, thread_t *, void **)
DEFSTUB2(thr_keycreate, TI_THR_KEYCREAT, thread_key_t *, PFrV)
DEFSTUB2(thr_kill, TI_THR_KILL, thread_t, int)
DEFSTUB0(thr_main, TI_THR_MAIN)
DEFSTUB1(thr_setconcurrency, TI_THR_SETCONC, int)
DEFSTUB2(thr_setprio, TI_THR_SETPRIO, thread_t, int)
DEFSTUB2(thr_setspecific, TI_THR_SETSPEC, thread_key_t, void *)
DEFSTUB3(thr_sigsetmask, TI_THR_SIGSET, int, const sigset_t *, sigset_t *)
DEFSTUB1(thr_stksegment, TI_THR_STKSEG, stack_t *)
DEFSTUB1(thr_suspend, TI_THR_SUSPEND, thread_t)
VDEFSTUB0(thr_yield, TI_THR_YIELD)
WDEFSTUB1(close, TI_CLOSE, int)
WDEFSTUB2(creat, TI_CREAT, const char *, mode_t)
WDEFSTUB3(fcntl, TI_FCNTL, int, int, intptr_t)
WDEFSTUB1(fsync, TI_FSYNC, int)
WDEFSTUB3(msync, TI_MSYNC, caddr_t, size_t, int)
WDEFSTUB3(open, TI_OPEN, const char *, int, mode_t)
WDEFSTUB0(pause, TI_PAUSE)
WRDEFSTUB3(read, TI_READ, ssize_t, int, void *, size_t)
WDEFSTUB1(sigsuspend, TI_SIGSUSPEND, const sigset_t *)
WDEFSTUB1(tcdrain, TI_TCDRAIN, int)
WRDEFSTUB1(wait, TI_WAIT, pid_t, int *)
WRDEFSTUB3(waitpid, TI_WAITPID, pid_t, pid_t, int *, int)
WRDEFSTUB3(write, TI_WRITE, ssize_t, int, const void *, size_t)
DEFSTUB1(pthread_cond_broadcast, TI_PCOND_BROAD, pthread_cond_t *)
DEFSTUB1(pthread_cond_destroy, TI_PCOND_DEST, pthread_cond_t *)
DEFSTUB2(pthread_cond_init, TI_PCOND_INIT, pthread_cond_t *,
	const pthread_condattr_t *)
DEFSTUB1(pthread_cond_signal, TI_PCOND_SIGNAL, pthread_cond_t *)
DEFSTUB3(pthread_cond_timedwait, TI_PCOND_TWAIT, pthread_cond_t *,
	pthread_mutex_t *, const struct timespec *)
DEFSTUB2(pthread_cond_wait, TI_PCOND_WAIT, pthread_cond_t *,
	pthread_mutex_t *)
DEFSTUB1(pthread_condattr_destroy, TI_PCONDA_DEST, pthread_condattr_t *)
DEFSTUB2(pthread_condattr_getpshared, TI_PCONDA_GETPS,
	const pthread_condattr_t *, int *)
DEFSTUB1(pthread_condattr_init, TI_PCONDA_INIT, pthread_condattr_t *)
DEFSTUB2(pthread_condattr_setpshared, TI_PCONDA_SETPS,
	pthread_condattr_t *, int *)
DEFSTUB1(pthread_mutex_destroy, TI_PMUTEX_DEST, pthread_mutex_t *)
DEFSTUB2(pthread_mutex_getprioceiling, TI_PMUTEX_GPC,
	pthread_mutex_t *, int *)
DEFSTUB2(pthread_mutex_init, TI_PMUTEX_INIT, pthread_mutex_t *,
	const pthread_mutexattr_t *)
DEFSTUB1(pthread_mutex_lock, TI_PMUTEX_LOCK, pthread_mutex_t *)
DEFSTUB3(pthread_mutex_setprioceiling, TI_PMUTEX_SPC,
	pthread_mutex_t *, int, int *)
DEFSTUB1(pthread_mutex_trylock, TI_PMUTEX_TRYL, pthread_mutex_t *)
DEFSTUB1(pthread_mutex_unlock, TI_PMUTEX_UNLCK, pthread_mutex_t *)
DEFSTUB1(pthread_mutexattr_destroy, TI_PMUTEXA_DEST, pthread_mutexattr_t *)
DEFSTUB2(pthread_mutexattr_getprioceiling, TI_PMUTEXA_GPC,
	const pthread_mutexattr_t *, int *)
DEFSTUB2(pthread_mutexattr_getprotocol, TI_PMUTEXA_GP,
	const pthread_mutexattr_t *, int *)
DEFSTUB2(pthread_mutexattr_getpshared, TI_PMUTEXA_GPS,
	const pthread_mutexattr_t *, int *)
DEFSTUB1(pthread_mutexattr_init, TI_PMUTEXA_INIT, pthread_mutexattr_t *)
DEFSTUB2(pthread_mutexattr_setprioceiling, TI_PMUTEXA_SPC,
	pthread_mutexattr_t *, int)
DEFSTUB2(pthread_mutexattr_setprotocol, TI_PMUTEXA_SP,
	pthread_mutexattr_t *, int)
DEFSTUB2(pthread_mutexattr_setpshared, TI_PMUTEXA_SPS,
	pthread_mutexattr_t *, int)
RDEFSTUB0(thr_min_stack, TI_THR_MINSTACK, size_t)
SDEFSTUB3(__sigtimedwait, TI_SIGTIMEDWAIT, const sigset_t *,
	siginfo_t *, const struct timespec *)
RDEFSTUB1(alarm, TI_ALARM, unsigned, unsigned)
DEFSTUB3(setitimer, TI_SETITIMER, int, const struct itimerval *,
	struct itimerval *)
VDEFSTUB2(siglongjmp, TI_SIGLONGJMP, sigjmp_buf, int)
WDEFSTUB1(sigpending, TI_SIGPENDING, sigset_t *)
SDEFSTUB2(__nanosleep, TI__NANOSLEEP, const struct timespec *,
	struct timespec *)
#if !defined(_LP64)
WDEFSTUB3(open64, TI_OPEN64, const char *, int, mode_t)
WDEFSTUB2(creat64, TI_CREAT64, const char *, mode_t)
#endif	/* _LP64 */
DEFSTUB1(rwlock_destroy, TI_RWLCKDESTROY, rwlock_t *)
DEFSTUB1(sema_destroy, TI_SEMADESTROY, sema_t *)
RDEFSTUB0(pthread_self, TI_PSELF, pthread_t)
DEFSTUB3(pthread_sigmask, TI_PSIGMASK, int, const sigset_t *, sigset_t *)

DEFSTUB1(pthread_attr_destroy, TI_PATTR_DEST, pthread_attr_t *)
DEFSTUB2(pthread_attr_getdetachstate, TI_PATTR_GDS, const pthread_attr_t *,
	int *)
DEFSTUB2(pthread_attr_getinheritsched, TI_PATTR_GIS, const pthread_attr_t *,
	struct sched_param *)
DEFSTUB2(pthread_attr_getschedparam, TI_PATTR_GSPA, const pthread_attr_t *,
	struct sched_param *)
DEFSTUB2(pthread_attr_getschedpolicy, TI_PATTR_GSPO, const pthread_attr_t *,
	int *)
DEFSTUB2(pthread_attr_getscope, TI_PATTR_GSCOP, const pthread_attr_t *,
	int *)
DEFSTUB2(pthread_attr_getstackaddr, TI_PATTR_GSTAD, const pthread_attr_t *,
	void **)
DEFSTUB2(pthread_attr_getstacksize, TI_PATTR_GSTSZ, const pthread_attr_t *,
	size_t *)
DEFSTUB1(pthread_attr_init, TI_PATTR_INIT, pthread_attr_t *)
DEFSTUB2(pthread_attr_setdetachstate, TI_PATTR_SDEST, pthread_attr_t *, int *)
DEFSTUB2(pthread_attr_setinheritsched, TI_PATTR_SIS, pthread_attr_t *, int)
DEFSTUB2(pthread_attr_setschedparam, TI_PATTR_SSPA, pthread_attr_t *,
	const struct sched_param *)
DEFSTUB2(pthread_attr_setschedpolicy, TI_PATTR_SSPO, pthread_attr_t *, int)
DEFSTUB2(pthread_attr_setscope, TI_PATTR_SSCOP, pthread_attr_t *, int)
DEFSTUB2(pthread_attr_setstackaddr, TI_PATTR_SSTAD, pthread_attr_t *, void *)
DEFSTUB2(pthread_attr_setstacksize, TI_PATTR_SSTSZ, pthread_attr_t *, size_t)
DEFSTUB1(pthread_cancel, TI_PCANCEL, pthread_t)
VSDEFSTUB2(__pthread_cleanup_pop, TI_PCLEANUP_POP, int, _cleanup_t *)
VSDEFSTUB4(__pthread_cleanup_push, TI_PCLEANUP_PSH, PFrVt, void *,
	caddr_t, _cleanup_t *)
DEFSTUB4(pthread_create, TI_PCREATE, pthread_t *, const pthread_attr_t *,
		void *, void *)
DEFSTUB1(pthread_detach, TI_PDETACH, pthread_t)
DEFSTUB2(pthread_equal, TI_PEQUAL, pthread_t, pthread_t)
VDEFSTUB1(pthread_exit, TI_PEXIT, void *)
DEFSTUB3(pthread_getschedparam, TI_PGSCHEDPARAM, pthread_t, int *,
	struct sched_param *)
RDEFSTUB1(pthread_getspecific, TI_PGSPECIFIC, void *, pthread_key_t)
DEFSTUB2(pthread_join, TI_PJOIN, pthread_t, void**)
DEFSTUB2(pthread_key_create, TI_PKEY_CREATE, pthread_key_t *, PFrV)
DEFSTUB1(pthread_key_delete, TI_PKEY_DELETE, pthread_key_t)
DEFSTUB2(pthread_kill, TI_PKILL, pthread_t, int)
DEFSTUB2(pthread_once, TI_PONCE, pthread_once_t *, PFrV)
DEFSTUB2(pthread_setcancelstate, TI_PSCANCELST, int, int *)
DEFSTUB2(pthread_setcanceltype, TI_PSCANCELTYPE, int, int *)
DEFSTUB3(pthread_setschedparam, TI_PSSCHEDPARAM, pthread_t, int,
	const struct sched_param *)
DEFSTUB2(pthread_setspecific, TI_PSETSPECIFIC, pthread_key_t, const void *)
VDEFSTUB0(pthread_testcancel, TI_PTESTCANCEL)
DEFSTUB2(kill, TI_KILL, pid_t, int)

DEFSTUB2(pthread_attr_getguardsize, TI_PATTR_GGDSZ, pthread_attr_t *, size_t *)
DEFSTUB2(pthread_attr_setguardsize, TI_PATTR_SGDSZ, pthread_attr_t *, size_t)
DEFSTUB0(pthread_getconcurrency, TI_PGETCONCUR)
DEFSTUB1(pthread_setconcurrency, TI_PSETCONCUR, int)
DEFSTUB2(pthread_mutexattr_settype, TI_PMUTEXA_STYP, pthread_mutexattr_t *,
		int)
DEFSTUB2(pthread_mutexattr_gettype, TI_PMUTEXA_GTYP, pthread_mutexattr_t *,
		int *)
DEFSTUB2(pthread_rwlock_init, TI_PRWL_INIT, pthread_rwlock_t *,
				const pthread_rwlockattr_t *)
DEFSTUB1(pthread_rwlock_destroy, TI_PRWL_DEST, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlock_rdlock, TI_PRWL_RDLK, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlock_tryrdlock, TI_PRWL_TRDLK, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlock_wrlock, TI_PRWL_WRLK, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlock_trywrlock, TI_PRWL_TWRLK, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlock_unlock, TI_PRWL_UNLK, pthread_rwlock_t *)
DEFSTUB1(pthread_rwlockattr_init, TI_PRWLA_INIT, pthread_rwlockattr_t *)
DEFSTUB1(pthread_rwlockattr_destroy, TI_PRWLA_DEST, pthread_rwlockattr_t *)
DEFSTUB2(pthread_rwlockattr_getpshared, TI_PRWLA_GPSH,
		const pthread_rwlockattr_t *, int *)
DEFSTUB2(pthread_rwlockattr_setpshared, TI_PRWLA_SPSH, pthread_rwlockattr_t *,
		int)

WDEFSTUB4(getmsg, TI_GETMSG, int, struct strbuf *, struct strbuf *, int *)
WDEFSTUB5(getpmsg, TI_GETPMSG, int, struct strbuf *, struct strbuf *, int *,
		int *)
WDEFSTUB4(putmsg, TI_PUTMSG, int, const struct strbuf *, const struct strbuf *,
		int)
WDEFSTUB4(__xpg4_putmsg, TI_XPG4PUTMSG, int, const struct strbuf *,
		const struct strbuf *, int)
WDEFSTUB5(putpmsg, TI_PUTPMSG, int, const struct strbuf *,
		const struct strbuf *, int, int)
WDEFSTUB5(__xpg4_putpmsg, TI_XPG4PUTPMSG, int, const struct strbuf *,
		const struct strbuf *, int, int)
WDEFSTUB3(lockf, TI_LOCKF, int, int, off_t)
WRDEFSTUB5(msgrcv, TI_MSGRCV, ssize_t, int, void *, size_t, long, int)
WDEFSTUB4(msgsnd, TI_MSGSND, int, const void *, size_t, int)
WDEFSTUB3(poll, TI_POLL, struct pollfd *, nfds_t, int)
WRDEFSTUB4(pread, TI_PREAD, ssize_t, int, void *, size_t, off_t)
WRDEFSTUB3(readv, TI_READV, ssize_t, int, const struct iovec *, int)
WRDEFSTUB4(pwrite, TI_PWRITE, ssize_t, int, const void  *, size_t, off_t)
WRDEFSTUB3(writev, TI_WRITEV, ssize_t, int, const struct iovec *, int)
WDEFSTUB5(select, TI_SELECT, int, fd_set *, fd_set *, fd_set *,
		struct timeval *)
WDEFSTUB1(sigpause, TI_SIGPAUSE, int)
WDEFSTUB1(usleep, TI_USLEEP, useconds_t)
WRDEFSTUB3(wait3, TI_WAIT3, pid_t, int *, int, struct rusage *)
WDEFSTUB4(waitid, TI_WAITID, idtype_t, id_t, siginfo_t *, int)

#if !defined(_LP64)
WDEFSTUB3(lockf64, TI_LOCKF64, int, int, off64_t)
WRDEFSTUB4(pread64, TI_PREAD64, ssize_t, int, void *, size_t, off64_t)
WRDEFSTUB4(pwrite64, TI_PWRITE64, ssize_t, int, const void  *, size_t, off64_t)
#endif
