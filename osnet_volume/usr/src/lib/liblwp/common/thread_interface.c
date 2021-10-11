/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)thread_interface.c	1.1	99/10/14 SMI"

#include "liblwp.h"
#include "thr_int.h"

typedef	int (*intf_t)();

extern void	_libc_threads_interface(Thr_interface *);
extern void	_ld_concurrency(void *);

extern	pid_t	_liblwp_fork();
extern	pid_t	_liblwp_fork1();
extern	int	_liblwp_poll();
extern	ssize_t	_liblwp_read();
extern	ssize_t	_liblwp_write();
extern	int	_liblwp_getmsg();
extern	int	_liblwp_putmsg();
extern	int	_liblwp__xpg4_putmsg();
extern	int	_liblwp_getpmsg();
extern	int	_liblwp_putpmsg();
extern	int	_liblwp__xpg4_putpmsg();
extern	uint_t	_liblwp_sleep();
extern	int	_liblwp_usleep();

extern	int	_liblwp_alarm();
extern	int	_liblwp_close();
extern	int	_liblwp_creat();
extern	int	_liblwp_fcntl();
extern	int	_liblwp_fsync();
extern	int	_liblwp_kill();
extern	int	_liblwp_lockf();
extern	ssize_t	_liblwp_msgrcv();
extern	int	_liblwp_msgsnd();
extern	int	_liblwp_msync();
extern	int	_liblwp__nanosleep();
extern	int	_liblwp_open();
extern	int	_liblwp_pause();
extern	ssize_t	_liblwp_pread();
extern	ssize_t	_liblwp_pwrite();
extern	ssize_t	_liblwp_readv();
extern	int	_liblwp_select();
extern	int	_liblwp_setitimer();
extern	int	_liblwp_sigaction();
extern	int	_liblwp_sigpause();
extern	int	_liblwp_sigpending();
extern	int	_liblwp__sigtimedwait();
extern	int	_liblwp_sigprocmask();
extern	int	_liblwp_sigwait();
extern	int	_liblwp_sigsuspend();
extern	int	_liblwp_tcdrain();
extern	pid_t	_liblwp_wait();
extern	pid_t	_liblwp_wait3();
extern	int	_liblwp_waitid();
extern	pid_t	_liblwp_waitpid();
extern	ssize_t	_liblwp_writev();

#if !defined(_LP64)
extern	int	_liblwp_creat64();
extern	int	_liblwp_lockf64();
extern	int	_liblwp_open64();
extern	ssize_t	_liblwp_pread64();
extern	ssize_t	_liblwp_pwrite64();
#endif

extern	int	_liblwp_bind_guard();
extern	int	_liblwp_bind_clear();

extern	int	_liblwp_cond_broadcast();
extern	int	_liblwp_cond_destroy();
extern	int	_liblwp_cond_init();
extern	int	_liblwp_cond_signal();
extern	int	_liblwp_cond_timedwait();
extern	int	_liblwp_cond_wait();
extern	int	_liblwp_lpthread_atfork();
extern	int	_liblwp_mutex_destroy();
extern	int	_liblwp_mutex_held();
extern	int	_liblwp_mutex_init();
extern	int	_liblwp_mutex_lock();
extern	int	_liblwp_mutex_trylock();
extern	int	_liblwp_mutex_unlock();
extern	int	_liblwp_pthread_atfork();
extern	int	_liblwp_pthread_equal();
extern	int	_liblwp_pthread_getspecific();
extern	int	_liblwp_pthread_setspecific();
extern	int	_liblwp_pthread_join();
extern	int	_liblwp_pthread_setcancelstate();
extern	int	_liblwp_pthread_attr_init();
extern	int	_liblwp_pthread_attr_destroy();
extern	int	_liblwp_pthread_attr_setstacksize();
extern	int	_liblwp_pthread_attr_getstacksize();
extern	int	_liblwp_pthread_attr_setstackaddr();
extern	int	_liblwp_pthread_attr_getstackaddr();
extern	int	_liblwp_pthread_attr_setdetachstate();
extern	int	_liblwp_pthread_attr_getdetachstate();
extern	int	_liblwp_pthread_attr_setscope();
extern	int	_liblwp_pthread_attr_getscope();
extern	int	_liblwp_pthread_attr_setinheritsched();
extern	int	_liblwp_pthread_attr_getinheritsched();
extern	int	_liblwp_pthread_attr_setschedpolicy();
extern	int	_liblwp_pthread_attr_getschedpolicy();
extern	int	_liblwp_pthread_attr_setschedparam();
extern	int	_liblwp_pthread_attr_getschedparam();
extern	int	_liblwp_pthread_attr_setguardsize();
extern	int	_liblwp_pthread_attr_getguardsize();
extern	int	_liblwp_pthread_mutexattr_destroy();
extern	int	_liblwp_pthread_mutexattr_getprioceiling();
extern	int	_liblwp_pthread_mutexattr_getprotocol();
extern	int	_liblwp_pthread_mutexattr_getpshared();
extern	int	_liblwp_pthread_mutexattr_init();
extern	int	_liblwp_pthread_mutexattr_setprioceiling();
extern	int	_liblwp_pthread_mutexattr_setprotocol();
extern	int	_liblwp_pthread_mutexattr_setpshared();
extern	int	_liblwp_pthread_mutexattr_gettype();
extern	int	_liblwp_pthread_mutexattr_settype();
extern	int	_liblwp_pthread_mutex_setprioceiling();
extern	int	_liblwp_pthread_mutex_getprioceiling();
extern	int	_liblwp_pthread_mutex_init();
extern	int	_liblwp_pthread_condattr_init();
extern	int	_liblwp_pthread_condattr_destroy();
extern	int	_liblwp_pthread_condattr_setpshared();
extern	int	_liblwp_pthread_condattr_getpshared();
extern	int	_liblwp_pthread_cond_init();
extern	int	_liblwp_pthread_rwlockattr_init();
extern	int	_liblwp_pthread_rwlockattr_destroy();
extern	int	_liblwp_pthread_rwlockattr_setpshared();
extern	int	_liblwp_pthread_rwlockattr_getpshared();
extern	int	_liblwp_pthread_rwlock_init();
extern	int	_liblwp_pthread_create();
extern	int	_liblwp_pthread_once();
extern	int	_liblwp_pthread_getschedparam();
extern	int	_liblwp_pthread_setschedparam();
extern	int	_liblwp_pthread_getconcurrency();
extern	int	_liblwp_pthread_setconcurrency();
extern	int	_liblwp_pthread_rwlock_destroy();
extern	int	_liblwp_pthread_rwlock_rdlock();
extern	int	_liblwp_pthread_rwlock_tryrdlock();
extern	int	_liblwp_pthread_rwlock_wrlock();
extern	int	_liblwp_pthread_rwlock_trywrlock();
extern	int	_liblwp_pthread_rwlock_unlock();
extern	int	_liblwp_pthread_cond_destroy();
extern	int	_liblwp_pthread_cond_wait();
extern	int	_liblwp_pthread_cond_signal();
extern	int	_liblwp_pthread_cond_broadcast();
extern	int	_liblwp_pthread_cond_timedwait();
extern	int	_liblwp_pthread_key_create();
extern	int	_liblwp_pthread_key_delete();
extern	int	_liblwp_pthread_exit();
extern	int	_liblwp_pthread_kill();
extern	int	_liblwp_pthread_self();
extern	int	_liblwp_pthread_sigmask();
extern	int	_liblwp_pthread_mutex_destroy();
extern	int	_liblwp_pthread_mutex_lock();
extern	int	_liblwp_pthread_mutex_trylock();
extern	int	_liblwp_pthread_mutex_unlock();
extern	int	_liblwp_pthread_detach();
extern	int	_liblwp_pthread_cancel();
extern	int	_liblwp_pthread_setcanceltype();
extern	int	_liblwp_pthread_testcancel();
extern	int	_liblwp__pthread_cleanup_pop();
extern	int	_liblwp__pthread_cleanup_push();

extern	int	_liblwp_rw_read_held();
extern	int	_liblwp_rw_write_held();
extern	int	_liblwp_rw_rdlock();
extern	int	_liblwp_rw_tryrdlock();
extern	int	_liblwp_rw_trywrlock();
extern	int	_liblwp_rw_unlock();
extern	int	_liblwp_rw_wrlock();
extern	int	_liblwp_rwlock_destroy();
extern	int	_liblwp_rwlock_init();
extern	int	_liblwp_sema_held();
extern	int	_liblwp_sema_init();
extern	int	_liblwp_sema_post();
extern	int	_liblwp_sema_trywait();
extern	int	_liblwp_sema_wait();
extern	int	_liblwp_sema_destroy();
extern	int	_liblwp_thr_continue();
extern	int	_liblwp_thr_create();
extern	int	_liblwp_thr_errnop();
extern	int	_liblwp_thr_exit();
extern	int	_liblwp_thr_getprio();
extern	int	_liblwp_thr_getspecific();
extern	int	_liblwp_thr_join();
extern	int	_liblwp_thr_keycreate();
extern	int	_liblwp_thr_kill();
extern	int	_liblwp_thr_main();
extern	int	_liblwp_thr_self();
extern	int	_liblwp_thr_getconcurrency();
extern	int	_liblwp_thr_setconcurrency();
extern	int	_liblwp_thr_setprio();
extern	int	_liblwp_thr_setspecific();
extern	int	_liblwp_thr_sigsetmask();
extern	int	_liblwp_thr_stksegment();
extern	int	_liblwp_thr_suspend();
extern	int	_liblwp_thr_yield();
extern	int	_liblwp_thr_min_stack();

const Thr_interface _ti_funcs[] =   {
/*  01 */   { TI_MUTEX_LOCK,	_liblwp_mutex_lock },
/*  02 */   { TI_MUTEX_UNLOCK,	_liblwp_mutex_unlock },
/*  03 */   { TI_LRW_RDLOCK,	_liblwp_rw_rdlock },
/*  04 */   { TI_LRW_WRLOCK,	_liblwp_rw_wrlock },
/*  05 */   { TI_LRW_UNLOCK,	_liblwp_rw_unlock },
/*  06 */   { TI_BIND_GUARD,	_liblwp_bind_guard },
/*  07 */   { TI_BIND_CLEAR,	_liblwp_bind_clear },
/*  08 */   { TI_LATFORK,	_liblwp_lpthread_atfork },
/*  09 */   { TI_THRSELF,	_liblwp_thr_self },
/*  10 */   { TI_VERSION,	(intf_t)TI_V_CURRENT },
/*  11 */   { TI_COND_BROAD,	_liblwp_cond_broadcast },
/*  12 */   { TI_COND_DESTROY,	_liblwp_cond_destroy },
/*  13 */   { TI_COND_INIT,	_liblwp_cond_init },
/*  14 */   { TI_COND_SIGNAL,	_liblwp_cond_signal },
/*  15 */   { TI_COND_TWAIT,	_liblwp_cond_timedwait },
/*  16 */   { TI_COND_WAIT,	_liblwp_cond_wait },
/*  17 */   { TI_FORK,		(intf_t)_liblwp_fork },
/*  18 */   { TI_FORK1,		(intf_t)_liblwp_fork1 },
/*  19 */   { TI_MUTEX_DEST,	_liblwp_mutex_destroy },
/*  20 */   { TI_MUTEX_HELD,	_liblwp_mutex_held },
/*  21 */   { TI_MUTEX_INIT,	_liblwp_mutex_init },
/*  22 */   { TI_MUTEX_TRYLCK,	_liblwp_mutex_trylock },
/*  23 */   { TI_ATFORK,	_liblwp_pthread_atfork },
/*  24 */   { TI_RW_RDHELD,	_liblwp_rw_read_held },
/*  25 */   { TI_RW_RDLOCK,	_liblwp_rw_rdlock },
/*  26 */   { TI_RW_WRLOCK,	_liblwp_rw_wrlock },
/*  27 */   { TI_RW_UNLOCK,	_liblwp_rw_unlock },
/*  28 */   { TI_TRYRDLOCK,	_liblwp_rw_tryrdlock },
/*  29 */   { TI_TRYWRLOCK,	_liblwp_rw_trywrlock },
/*  30 */   { TI_RW_WRHELD,	_liblwp_rw_write_held },
/*  31 */   { TI_RWLOCKINIT,	_liblwp_rwlock_init },
/*  32 */   { TI_SEM_HELD,	_liblwp_sema_held },
/*  33 */   { TI_SEM_INIT,	_liblwp_sema_init },
/*  34 */   { TI_SEM_POST,	_liblwp_sema_post },
/*  35 */   { TI_SEM_TRYWAIT,	_liblwp_sema_trywait },
/*  36 */   { TI_SEM_WAIT,	_liblwp_sema_wait },
/*  37 */   { TI_SIGACTION,	_liblwp_sigaction },
/*  38 */   { TI_SIGPROCMASK,	_liblwp_sigprocmask },
/*  39 */   { TI_SIGWAIT,	_liblwp_sigwait },
/*  40 */   { TI_SLEEP,		(intf_t)_liblwp_sleep },
/*  41 */   { TI_THR_CONT,	_liblwp_thr_continue },
/*  42 */   { TI_THR_CREATE,	_liblwp_thr_create },
/*  43 */   { TI_THR_ERRNOP,	_liblwp_thr_errnop },
/*  44 */   { TI_THR_EXIT,	_liblwp_thr_exit },
/*  45 */   { TI_THR_GETCONC,	_liblwp_thr_getconcurrency },
/*  46 */   { TI_THR_GETPRIO,	_liblwp_thr_getprio },
/*  47 */   { TI_THR_GETSPEC,	_liblwp_thr_getspecific },
/*  48 */   { TI_THR_JOIN,	_liblwp_thr_join },
/*  49 */   { TI_THR_KEYCREAT,	_liblwp_thr_keycreate },
/*  50 */   { TI_THR_KILL,	_liblwp_thr_kill },
/*  51 */   { TI_THR_MAIN,	_liblwp_thr_main },
/*  52 */   { TI_THR_SETCONC,	_liblwp_thr_setconcurrency },
/*  53 */   { TI_THR_SETPRIO,	_liblwp_thr_setprio },
/*  54 */   { TI_THR_SETSPEC,	_liblwp_thr_setspecific },
/*  55 */   { TI_THR_SIGSET,	_liblwp_thr_sigsetmask },
/*  56 */   { TI_THR_STKSEG,	_liblwp_thr_stksegment },
/*  57 */   { TI_THR_SUSPEND,	_liblwp_thr_suspend },
/*  58 */   { TI_THR_YIELD,	_liblwp_thr_yield },
/*  59 */   { TI_CLOSE,		_liblwp_close },
/*  60 */   { TI_CREAT,		_liblwp_creat },
/*  61 */   { TI_FCNTL,		_liblwp_fcntl },
/*  62 */   { TI_FSYNC,		_liblwp_fsync },
/*  63 */   { TI_MSYNC,		_liblwp_msync },
/*  64 */   { TI_OPEN,		_liblwp_open },
/*  65 */   { TI_PAUSE,		_liblwp_pause },
/*  66 */   { TI_READ,		(intf_t)_liblwp_read },
/*  67 */   { TI_SIGSUSPEND,	_liblwp_sigsuspend },
/*  68 */   { TI_TCDRAIN,	_liblwp_tcdrain },
/*  69 */   { TI_WAIT,		(intf_t)_liblwp_wait },
/*  70 */   { TI_WAITPID,	(intf_t)_liblwp_waitpid },
/*  71 */   { TI_WRITE,		(intf_t)_liblwp_write },
/*  72 */   { TI_PCOND_BROAD,	_liblwp_pthread_cond_broadcast },
/*  73 */   { TI_PCOND_DEST,	_liblwp_pthread_cond_destroy },
/*  74 */   { TI_PCOND_INIT,	_liblwp_pthread_cond_init },
/*  75 */   { TI_PCOND_SIGNAL,	_liblwp_pthread_cond_signal },
/*  76 */   { TI_PCOND_TWAIT,	_liblwp_pthread_cond_timedwait },
/*  77 */   { TI_PCOND_WAIT,	_liblwp_pthread_cond_wait },
/*  78 */   { TI_PCONDA_DEST,	_liblwp_pthread_condattr_destroy },
/*  79 */   { TI_PCONDA_GETPS,	_liblwp_pthread_condattr_getpshared },
/*  80 */   { TI_PCONDA_INIT,	_liblwp_pthread_condattr_init },
/*  81 */   { TI_PCONDA_SETPS,	_liblwp_pthread_condattr_setpshared },
/*  82 */   { TI_PMUTEX_DEST,	_liblwp_pthread_mutex_destroy },
/*  83 */   { TI_PMUTEX_GPC,	_liblwp_pthread_mutex_getprioceiling },
/*  84 */   { TI_PMUTEX_INIT,	_liblwp_pthread_mutex_init },
/*  85 */   { TI_PMUTEX_LOCK,	_liblwp_pthread_mutex_lock },
/*  86 */   { TI_PMUTEX_SPC,	_liblwp_pthread_mutex_setprioceiling },
/*  87 */   { TI_PMUTEX_TRYL,	_liblwp_pthread_mutex_trylock },
/*  88 */   { TI_PMUTEX_UNLCK,	_liblwp_pthread_mutex_unlock },
/*  89 */   { TI_PMUTEXA_DEST,	_liblwp_pthread_mutexattr_destroy },
/*  90 */   { TI_PMUTEXA_GPC,	_liblwp_pthread_mutexattr_getprioceiling },
/*  91 */   { TI_PMUTEXA_GP,	_liblwp_pthread_mutexattr_getprotocol },
/*  92 */   { TI_PMUTEXA_GPS,	_liblwp_pthread_mutexattr_getpshared },
/*  93 */   { TI_PMUTEXA_INIT,	_liblwp_pthread_mutexattr_init },
/*  94 */   { TI_PMUTEXA_SPC,	_liblwp_pthread_mutexattr_setprioceiling },
/*  95 */   { TI_PMUTEXA_SP,	_liblwp_pthread_mutexattr_setprotocol },
/*  96 */   { TI_PMUTEXA_SPS,	_liblwp_pthread_mutexattr_setpshared },
/*  97 */   { TI_THR_MINSTACK,	_liblwp_thr_min_stack },
/*  98 */   { TI_SIGTIMEDWAIT,	_liblwp__sigtimedwait },
/*  99 */   { TI_ALARM,		_liblwp_alarm },
/* 100 */   { TI_SETITIMER,	_liblwp_setitimer },
/* 101 */ /* TI_SIGLONGJMP defaults to libc */
/* 102 */ /* TI_SIGSETJMP defaults to libc */
/* 103 */   { TI_SIGPENDING,	_liblwp_sigpending },
/* 104 */   { TI__NANOSLEEP,	_liblwp__nanosleep },
#if !defined(_LP64)
/* 105 */   { TI_OPEN64,	_liblwp_open64 },
/* 106 */   { TI_CREAT64,	_liblwp_creat64 },
#endif
/* 107 */   { TI_RWLCKDESTROY,	_liblwp_rwlock_destroy },
/* 108 */   { TI_SEMADESTROY,	_liblwp_sema_destroy },
/* 109 */   { TI_PSELF,		_liblwp_pthread_self },
/* 110 */   { TI_PSIGMASK,	_liblwp_pthread_sigmask },
/* 111 */   { TI_PATTR_DEST,	_liblwp_pthread_attr_destroy },
/* 112 */   { TI_PATTR_GDS,	_liblwp_pthread_attr_getdetachstate },
/* 113 */   { TI_PATTR_GIS,	_liblwp_pthread_attr_getinheritsched },
/* 114 */   { TI_PATTR_GSPA,	_liblwp_pthread_attr_getschedparam },
/* 115 */   { TI_PATTR_GSPO,	_liblwp_pthread_attr_getschedpolicy },
/* 116 */   { TI_PATTR_GSCOP,	_liblwp_pthread_attr_getscope },
/* 117 */   { TI_PATTR_GSTAD,	_liblwp_pthread_attr_getstackaddr },
/* 118 */   { TI_PATTR_GSTSZ,	_liblwp_pthread_attr_getstacksize },
/* 119 */   { TI_PATTR_INIT,	_liblwp_pthread_attr_init },
/* 120 */   { TI_PATTR_SDEST,	_liblwp_pthread_attr_setdetachstate },
/* 121 */   { TI_PATTR_SIS,	_liblwp_pthread_attr_setinheritsched },
/* 122 */   { TI_PATTR_SSPA,	_liblwp_pthread_attr_setschedparam },
/* 123 */   { TI_PATTR_SSPO,	_liblwp_pthread_attr_setschedpolicy },
/* 124 */   { TI_PATTR_SSCOP,	_liblwp_pthread_attr_setscope },
/* 125 */   { TI_PATTR_SSTAD,	_liblwp_pthread_attr_setstackaddr },
/* 126 */   { TI_PATTR_SSTSZ,	_liblwp_pthread_attr_setstacksize },
/* 127 */   { TI_PCANCEL,	_liblwp_pthread_cancel },
/* 128 */   { TI_PCLEANUP_POP,	_liblwp__pthread_cleanup_pop },
/* 129 */   { TI_PCLEANUP_PSH,	_liblwp__pthread_cleanup_push },
/* 130 */   { TI_PCREATE,	_liblwp_pthread_create },
/* 131 */   { TI_PDETACH,	_liblwp_pthread_detach },
/* 132 */   { TI_PEQUAL,	_liblwp_pthread_equal },
/* 133 */   { TI_PEXIT,		_liblwp_pthread_exit },
/* 134 */   { TI_PGSCHEDPARAM,	_liblwp_pthread_getschedparam },
/* 135 */   { TI_PGSPECIFIC,	_liblwp_pthread_getspecific },
/* 136 */   { TI_PJOIN,		_liblwp_pthread_join },
/* 137 */   { TI_PKEY_CREATE,	_liblwp_pthread_key_create },
/* 138 */   { TI_PKEY_DELETE,	_liblwp_pthread_key_delete },
/* 139 */   { TI_PKILL,		_liblwp_pthread_kill },
/* 140 */   { TI_PONCE,		_liblwp_pthread_once },
/* 141 */   { TI_PSCANCELST,	_liblwp_pthread_setcancelstate },
/* 142 */   { TI_PSCANCELTYPE,	_liblwp_pthread_setcanceltype },
/* 143 */   { TI_PSSCHEDPARAM,	_liblwp_pthread_setschedparam },
/* 144 */   { TI_PSETSPECIFIC,	_liblwp_pthread_setspecific },
/* 145 */   { TI_PTESTCANCEL,	_liblwp_pthread_testcancel },
/* 146 */   { TI_KILL,		_liblwp_kill },
/* 147 */   { TI_PGETCONCUR,	_liblwp_pthread_getconcurrency },
/* 148 */   { TI_PSETCONCUR,	_liblwp_pthread_setconcurrency },
/* 149 */   { TI_PATTR_GGDSZ,	_liblwp_pthread_attr_getguardsize },
/* 150 */   { TI_PATTR_SGDSZ,	_liblwp_pthread_attr_setguardsize },
/* 151 */   { TI_PMUTEXA_GTYP,	_liblwp_pthread_mutexattr_gettype},
/* 152 */   { TI_PMUTEXA_STYP,	_liblwp_pthread_mutexattr_settype},
/* 153 */   { TI_PRWL_INIT,	_liblwp_pthread_rwlock_init },
/* 154 */   { TI_PRWL_DEST,	_liblwp_pthread_rwlock_destroy },
/* 155 */   { TI_PRWLA_INIT,	_liblwp_pthread_rwlockattr_init },
/* 156 */   { TI_PRWLA_DEST,	_liblwp_pthread_rwlockattr_destroy },
/* 157 */   { TI_PRWLA_GPSH,	_liblwp_pthread_rwlockattr_getpshared},
/* 158 */   { TI_PRWLA_SPSH,	_liblwp_pthread_rwlockattr_setpshared},
/* 159 */   { TI_PRWL_RDLK,	_liblwp_pthread_rwlock_rdlock },
/* 160 */   { TI_PRWL_TRDLK,	_liblwp_pthread_rwlock_tryrdlock },
/* 161 */   { TI_PRWL_WRLK,	_liblwp_pthread_rwlock_wrlock },
/* 162 */   { TI_PRWL_TWRLK,	_liblwp_pthread_rwlock_trywrlock },
/* 163 */   { TI_PRWL_UNLK,	_liblwp_pthread_rwlock_unlock },
/* 164 */   { TI_GETMSG,	_liblwp_getmsg },
/* 165 */   { TI_GETPMSG,	_liblwp_getpmsg },
/* 166 */   { TI_PUTMSG,	_liblwp_putmsg },
/* 167 */   { TI_PUTPMSG,	_liblwp_putpmsg },
/* 168 */   { TI_LOCKF,		_liblwp_lockf },
/* 169 */   { TI_MSGRCV,	(intf_t)_liblwp_msgrcv },
/* 170 */   { TI_MSGSND,	_liblwp_msgsnd },
/* 171 */   { TI_POLL,		_liblwp_poll },
/* 172 */   { TI_PREAD,		(intf_t)_liblwp_pread },
/* 173 */   { TI_READV,		(intf_t)_liblwp_readv },
/* 174 */   { TI_PWRITE,	(intf_t)_liblwp_pwrite },
/* 175 */   { TI_WRITEV,	(intf_t)_liblwp_writev },
/* 176 */   { TI_SELECT,	_liblwp_select },
/* 177 */   { TI_SIGPAUSE,	_liblwp_sigpause },
/* 178 */   { TI_USLEEP,	_liblwp_usleep },
/* 179 */   { TI_WAIT3,		(intf_t)_liblwp_wait3 },
/* 180 */   { TI_WAITID,	_liblwp_waitid },
#if !defined(_LP64)
/* 181 */   { TI_LOCKF64,	_liblwp_lockf64 },
/* 182 */   { TI_PREAD64,	(intf_t)_liblwp_pread64 },
/* 183 */   { TI_PWRITE64,	(intf_t)_liblwp_pwrite64 },
#endif
/* 184 */   { TI_XPG4PUTMSG,	_liblwp__xpg4_putmsg },
/* 185 */   { TI_XPG4PUTPMSG,	_liblwp__xpg4_putpmsg },
	    { TI_NULL,		NULL }
};

void
_set_libc_interface()
{
	void (*	fptr)();
	if ((fptr = _libc_threads_interface) != 0)
		(*fptr)(_ti_funcs);
}

void
_unset_libc_interface()
{
	void (* fptr)();

	if ((fptr = _libc_threads_interface) != 0)
		(* fptr)(NULL);
}

void
_set_rtld_interface()
{
	void (* fptr)();

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)(_ti_funcs);
}

void
_unset_rtld_interface()
{
	void (* fptr)();

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)(NULL);
}
