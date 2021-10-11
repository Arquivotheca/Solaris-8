/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _THR_INT_H
#define	_THR_INT_H

#pragma ident	"@(#)thr_int.h	1.15	98/02/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Thread/Libc/rtld Interface
 */
#define	TI_NULL		0	/* (void) last entry */
#define	TI_MUTEX_LOCK	1	/* _mutex_lock() address */
#define	TI_MUTEX_UNLOCK	2	/* _mutex_unlock() address */
#define	TI_LRW_RDLOCK	3	/* _llrw_rdlock() address */
#define	TI_LRW_WRLOCK	4	/* _llrw_wrlock() address */
#define	TI_LRW_UNLOCK	5	/* _llrw_unlock() address */
#define	TI_BIND_GUARD	6	/* _bind_guard() address */
#define	TI_BIND_CLEAR	7	/* _bind_clear() address */
#define	TI_LATFORK	8	/* _lpthread_atfork() */
#define	TI_THRSELF	9	/* _thr_self() address */
#define	TI_VERSION	10	/* current version of ti_interface */
#define	TI_COND_BROAD	11	/* _cond_broadcast() address */
#define	TI_COND_DESTROY	12	/* _cond_destroy() address */
#define	TI_COND_INIT	13	/* _cond_init() address */
#define	TI_COND_SIGNAL	14	/* _cond_signal() address */
#define	TI_COND_TWAIT	15	/* _cond_timedwait() address */
#define	TI_COND_WAIT	16	/* _cond_wait() address */
#define	TI_FORK		17	/* _fork() address */
#define	TI_FORK1	18	/* _fork1() address */
#define	TI_MUTEX_DEST	19	/* _mutex_destroy() address */
#define	TI_MUTEX_HELD	20	/* _mutex_held() address */
#define	TI_MUTEX_INIT	21	/* _mutex_init() address */
#define	TI_MUTEX_TRYLCK	22	/* _mutex_trylock() address */
#define	TI_ATFORK	23	/* _pthread_atfork() */
#define	TI_RW_RDHELD	24	/* _rw_read_held() address */
#define	TI_RW_RDLOCK	25	/* _rw_rdlock() address */
#define	TI_RW_WRLOCK	26	/* _rw_wrlock() address */
#define	TI_RW_UNLOCK	27	/* _rw_unlock() address */
#define	TI_TRYRDLOCK	28	/* _rw_tryrdlock() address */
#define	TI_TRYWRLOCK	29	/* _rw_trywrlock() address */
#define	TI_RW_WRHELD	30	/* _rw_write_held() address */
#define	TI_RWLOCKINIT	31	/* _rwlock_init() address */
#define	TI_SEM_HELD	32	/* _sema_held() address */
#define	TI_SEM_INIT	33	/* _sema_init() address */
#define	TI_SEM_POST	34	/* _sema_post() address */
#define	TI_SEM_TRYWAIT	35	/* _sema_trywait() address */
#define	TI_SEM_WAIT	36	/* _sema_wait() address */
#define	TI_SIGACTION	37	/* _sigaction() address */
#define	TI_SIGPROCMASK	38	/* _sigprocmask() address */
#define	TI_SIGWAIT	39	/* _sigwait() address */
#define	TI_SLEEP	40	/* _sleep() address */
#define	TI_THR_CONT	41	/* _thr_continue() address */
#define	TI_THR_CREATE	42	/* _thr_create() address */
#define	TI_THR_ERRNOP	43	/* _thr_errnop() address */
#define	TI_THR_EXIT	44	/* _thr_exit() address */
#define	TI_THR_GETCONC	45	/* _thr_getconcurrency() address */
#define	TI_THR_GETPRIO	46	/* _thr_getprio() address */
#define	TI_THR_GETSPEC	47	/* _thr_getspecific() address */
#define	TI_THR_JOIN	48	/* _thr_join() address */
#define	TI_THR_KEYCREAT	49	/* _thr_keycreate() address */
#define	TI_THR_KILL	50	/* _thr_kill() address */
#define	TI_THR_MAIN	51	/* _thr_main() address */
#define	TI_THR_SETCONC	52	/* _thr_setconcurrency() address */
#define	TI_THR_SETPRIO	53	/* _thr_setprio() address */
#define	TI_THR_SETSPEC	54	/* _thr_setspecific() address */
#define	TI_THR_SIGSET	55	/* _thr_sigsetmask() address */
#define	TI_THR_STKSEG	56	/* _thr_stksegment() address */
#define	TI_THR_SUSPEND	57	/* _thr_suspend() address */
#define	TI_THR_YIELD	58	/* _thr_yield() address */
#define	TI_CLOSE	59	/* _close() address */
#define	TI_CREAT	60	/* _creat() address */
#define	TI_FCNTL	61	/* _fcntl() address */
#define	TI_FSYNC	62	/* _fsync() address */
#define	TI_MSYNC	63	/* _msync() address */
#define	TI_OPEN		64	/* _open() address */
#define	TI_PAUSE	65	/* _pause() address */
#define	TI_READ		66	/* _read() address */
#define	TI_SIGSUSPEND	67	/* _sigsuspend() address */
#define	TI_TCDRAIN	68	/* _tcdrain() address */
#define	TI_WAIT		69	/* _wait() address */
#define	TI_WAITPID	70	/* _waitpid() address */
#define	TI_WRITE	71	/* _write() address */
#define	TI_PCOND_BROAD	72	/* _pthread_cond_broadcast() address */
#define	TI_PCOND_DEST	73	/* _pthread_cond_destroy() address */
#define	TI_PCOND_INIT	74	/* _pthread_cond_init() address */
#define	TI_PCOND_SIGNAL	75	/* _pthread_cond_signal() address */
#define	TI_PCOND_TWAIT	76	/* _pthread_cond_timedwait() address */
#define	TI_PCOND_WAIT	77	/* _pthread_cond_wait() address */
#define	TI_PCONDA_DEST	78	/* _pthread_condattr_destroy() address */
#define	TI_PCONDA_GETPS	79	/* _pthread_condattr_getpshared() address */
#define	TI_PCONDA_INIT	80	/* _pthread_condattr_init() address */
#define	TI_PCONDA_SETPS	81	/* _pthread_condattr_setpshared() address */
#define	TI_PMUTEX_DEST	82	/* _pthread_mutex_destroy() address */
#define	TI_PMUTEX_GPC	83	/* _pthread_mutex_getprioceiling() address */
#define	TI_PMUTEX_INIT	84	/* _pthread_mutex_init() address */
#define	TI_PMUTEX_LOCK	85	/* _pthread_mutex_lock() address */
#define	TI_PMUTEX_SPC	86	/* _pthread_mutex_setprioceiling() address */
#define	TI_PMUTEX_TRYL	87	/* _pthread_mutex_trylock() address */
#define	TI_PMUTEX_UNLCK	88	/* _pthread_mutex_unlock() address */
#define	TI_PMUTEXA_DEST	89	/* _pthread_mutexattr_destory() address */
#define	TI_PMUTEXA_GPC	90	/* _pthread_mutexattr_getprioceiling() */
#define	TI_PMUTEXA_GP	91	/* _pthread_mutexattr_getprotocol() address */
#define	TI_PMUTEXA_GPS	92	/* _pthread_mutexattr_getpshared() address */
#define	TI_PMUTEXA_INIT	93	/* _pthread_mutexattr_init() address */
#define	TI_PMUTEXA_SPC	94	/* _pthread_mutexattr_setprioceiling() */
#define	TI_PMUTEXA_SP	95	/* _pthread_mutexattr_setprotocol() address */
#define	TI_PMUTEXA_SPS	96	/* _pthread_mutexattr_setpshared() address */
#define	TI_THR_MINSTACK	97	/* _thr_min_stack() address */
#define	TI_SIGTIMEDWAIT	98	/* __sigtimedwait() address */
#define	TI_ALARM	99	/* _alarm() address */
#define	TI_SETITIMER	100	/* _setitimer() address */
#define	TI_SIGLONGJMP	101	/* _siglongjmp() address */
#define	TI_SIGSETJMP	102	/* _sigsetjmp() address */
#define	TI_SIGPENDING	103	/* _sigpending() address */
#define	TI__NANOSLEEP	104	/* __nanosleep() address */
#define	TI_OPEN64	105	/* _open64() address */
#define	TI_CREAT64	106	/* _creat64() address */
#define	TI_RWLCKDESTROY	107	/* _rwlock_destroy() address */
#define	TI_SEMADESTROY	108	/* _sema_destroy() address */
#define	TI_PSELF	109	/* _pthread_self() address */
#define	TI_PSIGMASK	110	/* _pthread_sigmask() address */
#define	TI_PATTR_DEST	111	/* _pthread_attr_destroy */
#define	TI_PATTR_GDS	112	/* _pthread_attr_getdetachstate() */
#define	TI_PATTR_GIS	113	/* _pthread_attr_getinheritsched() */
#define	TI_PATTR_GSPA	114	/* _pthread_attr_getschedparam() */
#define	TI_PATTR_GSPO	115	/* _pthread_attr_getschedpolicy() */
#define	TI_PATTR_GSCOP	116	/* _pthread_attr_getscope() */
#define	TI_PATTR_GSTAD	117	/* _pthread_attr_getstackaddr() */
#define	TI_PATTR_GSTSZ	118	/* _pthread_attr_getstacksize() */
#define	TI_PATTR_INIT	119	/* _pthread_attr_init() */
#define	TI_PATTR_SDEST	120	/* _pthread_attr_setdetachstate() */
#define	TI_PATTR_SIS	121	/* _pthread_attr_setinheritsched() */
#define	TI_PATTR_SSPA	122	/* _pthread_attr_setschedparam() */
#define	TI_PATTR_SSPO	123	/* _pthread_attr_setschedpolicy() */
#define	TI_PATTR_SSCOP	124	/* _pthread_attr_setscope() */
#define	TI_PATTR_SSTAD	125	/* _pthread_attr_setstackaddr() */
#define	TI_PATTR_SSTSZ	126	/* _pthread_attr_setstacksize() */
#define	TI_PCANCEL	127	/* _pthread_cancel() */
#define	TI_PCLEANUP_POP	128	/* __pthread_cleanup_pop() */
#define	TI_PCLEANUP_PSH	129	/* __pthread_cleanup_push() */
#define	TI_PCREATE	130	/* _pthread_create() */
#define	TI_PDETACH	131	/* _pthread_detach() */
#define	TI_PEQUAL	132	/* _pthread_equal() */
#define	TI_PEXIT	133	/* _pthread_exit() */
#define	TI_PGSCHEDPARAM	134	/* _pthread_getschedparam() */
#define	TI_PGSPECIFIC	135	/* _pthread_getspecific() */
#define	TI_PJOIN	136	/* _pthread_join() */
#define	TI_PKEY_CREATE	137	/* _pthread_key_create() */
#define	TI_PKEY_DELETE	138	/* _pthread_key_delete() */
#define	TI_PKILL	139	/* _pthread_kill() */
#define	TI_PONCE	140	/* _pthread_once() */
#define	TI_PSCANCELST	141	/* _pthread_setcancelstate() */
#define	TI_PSCANCELTYPE	142	/* _pthread_setcanceltype() */
#define	TI_PSSCHEDPARAM	143	/* _pthread_setschedparam() */
#define	TI_PSETSPECIFIC	144	/* _pthread_setspecific() */
#define	TI_PTESTCANCEL	145	/* _pthread_testcancel() */
#define	TI_KILL		146	/* _kill() */
#define	TI_PGETCONCUR	147	/* _pthread_getconcurrency() */
#define	TI_PSETCONCUR	148	/* _pthread_setconcurrency() */
#define	TI_PATTR_GGDSZ	149	/* _pthread_attr_getguardsize() */
#define	TI_PATTR_SGDSZ	150	/* _pthread_attr_setguardsize() */
#define	TI_PMUTEXA_GTYP	151	/* _pthread_mutexattr_gettype() */
#define	TI_PMUTEXA_STYP	152	/* _pthread_mutexattr_settype() */
#define	TI_PRWL_INIT	153	/* _pthread_rwlock_init() */
#define	TI_PRWL_DEST	154	/* _pthread_rwlock_destroy() */
#define	TI_PRWLA_INIT	155	/* _pthread_rwlockattr_init() */
#define	TI_PRWLA_DEST	156	/* _pthread_rwlockattr_destroy() */
#define	TI_PRWLA_GPSH	157	/* _pthread_rwlockattr_getpshared() */
#define	TI_PRWLA_SPSH	158	/* _pthread_rwlockattr_setpahared() */
#define	TI_PRWL_RDLK	159	/* _pthread_rwlock_rdlock() */
#define	TI_PRWL_TRDLK	160	/* _pthread_rwlock_tryrdlock() */
#define	TI_PRWL_WRLK	161	/* _pthread_rwlock_wrlock() */
#define	TI_PRWL_TWRLK	162	/* _pthread_rwlock_trywrlock() */
#define	TI_PRWL_UNLK	163	/* _pthread_rwlock_unlock() */
#define	TI_GETMSG	164	/* _getmsg() */
#define	TI_GETPMSG	165	/* _getpmsg() */
#define	TI_PUTMSG	166	/* _putmsg() */
#define	TI_PUTPMSG	167	/* _putpmsg() */
#define	TI_LOCKF	168	/* _lockf() */
#define	TI_MSGRCV	169	/* _msgrcv() */
#define	TI_MSGSND	170	/* _msgsnd() */
#define	TI_POLL		171	/* _poll() */
#define	TI_PREAD	172	/* _pread() */
#define	TI_READV	173	/* _readv() */
#define	TI_PWRITE	174	/* _pwrite() */
#define	TI_WRITEV	175	/* _writev() */
#define	TI_SELECT	176	/* _select() */
#define	TI_SIGPAUSE	177	/* _sigpause() */
#define	TI_USLEEP	178	/* _usleep() */
#define	TI_WAIT3	179	/* _wait3() */
#define	TI_WAITID	180	/* _waitid() */
#define	TI_LOCKF64	181	/* _lockf64() */
#define	TI_PREAD64	182	/* _pread64() */
#define	TI_PWRITE64	183	/* _pwrite64() */
#define	TI_XPG4PUTMSG	184	/* __xpg4_putmsg() */
#define	TI_XPG4PUTPMSG	185	/* __xpg4_putpmsg() */

#define	TI_MAX		186

#define	TI_V_NONE	0		/* ti_version versions */
#define	TI_V_CURRENT	1		/* current version of threads */
					/*	interface. */
#define	TI_V_NUM	2

/*
 * Threads Interface communication structure for threads library
 */
typedef struct {
	int	ti_tag;
	union {
		int (*	ti_func)();
		long	ti_val;
	} ti_un;
} Thr_interface;

#ifdef	__cplusplus
}
#endif

#endif /* _THR_INT_H */
