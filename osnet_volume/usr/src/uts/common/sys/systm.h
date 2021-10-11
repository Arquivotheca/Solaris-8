/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SYSTM_H
#define	_SYS_SYSTM_H

#pragma ident	"@(#)systm.h	1.110	99/11/24 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/proc.h>
#include <sys/dditypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Random set of variables used by more than one routine.
 */

#ifdef _KERNEL
#include <sys/varargs.h>

extern int hz;			/* system clock rate */
extern struct vnode *rootdir;	/* pointer to vnode of root directory */
extern volatile clock_t lbolt;	/* time in HZ since last boot */
extern volatile int64_t lbolt64;	/* lbolt computed as 64-bit value */
extern int interrupts_unleashed;	/* set after the spl0() in main() */

extern char runin;		/* scheduling flag */
extern char runout;		/* scheduling flag */
extern char wake_sched;		/* causes clock to wake swapper on next tick */
extern char wake_sched_sec;	/* causes clock to wake swapper after a sec */

extern pgcnt_t	maxmem;		/* max available memory (pages) */
extern pgcnt_t	physmem;	/* physical memory (pages) on this CPU */
extern pfn_t	physmax;	/* highest numbered physical page present */
extern pgcnt_t	physinstalled;	/* physical pages including PROM/boot use */

extern caddr_t	s_text;		/* start of kernel text segment */
extern caddr_t	e_text;		/* end of kernel text segment */
extern caddr_t	s_data;		/* start of kernel text segment */
extern caddr_t	e_data;		/* end of kernel text segment */
#if defined(__ia64)
extern caddr_t	s_sdata;	/* start of kernel small data segment */
extern caddr_t	e_sdata;	/* end of kernel small data segment */
#endif /* __ia64 */

extern pgcnt_t	availrmem;	/* Available resident (not swapable)	*/
				/* memory in pages.			*/
extern pgcnt_t	availrmem_initial;	/* initial value of availrmem	*/
extern pgcnt_t	segspt_minfree;	/* low water mark for availrmem in seg_spt */
extern pgcnt_t	freemem;	/* Current free memory.			*/

extern dev_t	rootdev;	/* device of the root */
extern struct vnode *rootvp;	/* vnode of root filesystem */
extern char *volatile panicstr;	/* panic string pointer */
extern va_list  panicargs;	/* panic arguments */

extern int	rstchown;	/* 1 ==> restrictive chown(2) semantics */
extern int	klustsize;

extern int	abort_enable;	/* Platform input-device abort policy */

#ifdef C2_AUDIT
extern int	audit_active;	/* C2 auditing activate 1, absent 0. */
#endif

extern int	avenrun[];	/* array of load averages */

extern char *isa_list;		/* For sysinfo's isalist option */

extern int noexec_user_stack;		/* patchable via /etc/system */
extern int noexec_user_stack_log;	/* patchable via /etc/system */

extern void report_stack_exec(proc_t *, caddr_t);

extern void swtch_to(kthread_id_t);
extern void startup(void);
extern void clkstart(void);
extern void post_startup(void);
extern void kern_setup1(void);
extern void ka_init(void);

/*
 * for tod fault detection
 */
enum tod_fault_type {
	TOD_REVERSED = 0,
	TOD_STALLED,
	TOD_JUMPED,
	TOD_RATECHANGED,
	TOD_NOFAULT
};

extern time_t tod_validate(time_t, hrtime_t);
extern void tod_fault_reset(void);
extern void plat_tod_fault(enum tod_fault_type);

#ifndef _LP64
int min(int, int);
int max(int, int);
uint_t umin(uint_t, uint_t);
uint_t umax(uint_t, uint_t);
#endif /* !_LP64 */
int grow(caddr_t);
timeout_id_t timeout(void (*)(void *), void *, clock_t);
timeout_id_t realtime_timeout(void (*)(void *), void *, clock_t);
clock_t untimeout(timeout_id_t);
void delay(clock_t);
int delay_sig(clock_t);
int nodev();
int nulldev();
major_t getudev(void);
int cmpldev(dev32_t *, dev_t);
dev_t expldev(dev32_t);
int bcmp(const void *, const void *, size_t);
int stoi(char **);
void numtos(ulong_t, char *);
size_t strlen(const char *);
size_t ustrlen(const char *);
char *strcat(char *, const char *);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
char *knstrcpy(char *, const char *, size_t *);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strnrchr(const char *, int, size_t);
char *strstr(const char *, const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int strcasecmp(const char *, const char *);
int strncasecmp(const char *, const char *, size_t);
int getsubopt(char **optionsp, char * const *tokens, char **valuep);
char *append_subopt(const char *, size_t, char *, const char *);
int ffs(long);
int copyin(const void *, void *, size_t);
void copyin_noerr(const void *, void *, size_t);
int xcopyin(const void *, void *, size_t);
int copyout(const void *, void *, size_t);
void copyout_noerr(const void *, void *, size_t);
int xcopyout(const void *, void *, size_t);
int copyinstr(const char *, char *, size_t, size_t *);
char *copyinstr_noerr(char *, const char *, size_t *);
int copyoutstr(const char *, char *, size_t, size_t *);
int copystr(const char *, char *, size_t, size_t *);
void bcopy(const void *, void *, size_t);
void ucopy(const void *, void *, size_t);
void pgcopy(const void *, void *, size_t);
void ovbcopy(const void *, void *, size_t);
void bzero(void *, size_t);
void uzero(void *, size_t);
int kcopy(const void *, void *, size_t);
int kzero(void *, size_t);

int fuword8(const void *, uint8_t *);
int fuiword8(const void *, uint8_t *);
int fuword16(const void *, uint16_t *);
int fuword32(const void *, uint32_t *);
int fuiword32(const void *, uint32_t *);
int fulword(const void *, ulong_t *);
int fuword64(const void *, uint64_t *);
void fuword8_noerr(const void *, uint8_t *);
void fuword16_noerr(const void *, uint16_t *);
void fuword32_noerr(const void *, uint32_t *);
void fulword_noerr(const void *, ulong_t *);
void fuword64_noerr(const void *, uint64_t *);

int subyte(void *, uchar_t);
int suword8(void *, uint8_t);
int suiword8(void *, uint8_t);
int suword16(void *, uint16_t);
int suword32(void *, uint32_t);
int suiword32(void *, uint32_t);
int sulword(void *addr, ulong_t);
int suword64(void *, uint64_t);
void subyte_noerr(void *, uchar_t);
void suword8_noerr(void *, uint8_t);
void suword16_noerr(void *, uint16_t);
void suword32_noerr(void *, uint32_t);
void sulword_noerr(void *addr, ulong_t);
void suword64_noerr(void *, uint64_t);

int setjmp(label_t *);
void longjmp(label_t *);
caddr_t caller(void);
caddr_t callee(void);
int getpcstack(uintptr_t *, int);
int on_fault(label_t *);
void no_fault(void);
int on_data_trap(ddi_nofault_data_t *);
void no_data_trap(ddi_nofault_data_t *);
void halt(char *);
int scanc(size_t, uchar_t *, uchar_t *, uchar_t);
int movtuc(size_t, uchar_t *, uchar_t *, uchar_t *);
int splr(int);
int splhigh(void);
int splhi(void);
int splzs(void);
int spl0(void);
int spl6(void);
int spl7(void);
int spl8(void);
void splx(int);
void set_base_spl(void);
int __ipltospl(int);

void softcall_init(void);
void softcall(void (*)(void *), void *);
void softint(void);

extern void sync_icache(caddr_t, uint_t);

void _insque(caddr_t, caddr_t);
void _remque(caddr_t);

/* casts to keep lint happy */
#define	insque(q, p)	_insque((caddr_t)q, (caddr_t)p)
#define	remque(q)	_remque((caddr_t)q)

#pragma unknown_control_flow(setjmp)
#pragma unknown_control_flow(on_fault)
#pragma unknown_control_flow(on_data_trap)

struct timeval;
extern void	uniqtime(struct timeval *);
struct timeval32;
extern void	uniqtime32(struct timeval32 *);

uint_t page_num_pagesizes(void);
size_t page_get_pagesize(uint_t n);

extern int maxusers;
extern int pidmax;

extern void param_calc(int);
extern void param_init(void);
extern void param_check(void);

#endif /* _KERNEL */

/*
 * Structure of the system-entry table.
 *
 * 	Changes to struct sysent should maintain binary compatibility with
 *	loadable system calls, although the interface is currently private.
 *
 *	This means it should only be expanded on the end, and flag values
 * 	should not be reused.
 *
 *	It is desirable to keep the size of this struct a power of 2 for quick
 *	indexing.
 */
struct sysent {
	char		sy_narg;	/* total number of arguments */
#ifdef _LP64
	unsigned short	sy_flags;	/* various flags as defined below */
#else
	unsigned char	sy_flags;	/* various flags as defined below */
#endif
	int		(*sy_call)();	/* argp, rvalp-style handler */
	krwlock_t	*sy_lock;	/* lock for loadable system calls */
	int64_t		(*sy_callc)();	/* C-style call hander or wrapper */
};

extern struct sysent	sysent[];
#ifdef _SYSCALL32_IMPL
extern struct sysent	sysent32[];
#endif

extern struct sysent	nosys_ent;	/* entry for invalid system call */

#define	NSYSCALL 	256		/* number of system calls */

#define	LOADABLE_SYSCALL(s)	(s->sy_flags & SE_LOADABLE)
#define	LOADED_SYSCALL(s)	(s->sy_flags & SE_LOADED)

/*
 * sy_flags values
 * 	Values 1, 2, and 4 were used previously for SETJUMP, ASYNC, and IOSYS.
 */
#define	SE_32RVAL1	0x0		/* handler returns int32_t in rval1 */
#define	SE_32RVAL2	0x1		/* handler returns int32_t in rval2 */
#define	SE_64RVAL	0x2		/* handler returns int64_t in rvals */
#define	SE_RVAL_MASK	0x3		/* mask of rval_t bits */

#define	SE_LOADABLE	0x08		/* syscall is loadable */
#define	SE_LOADED	0x10		/* syscall is completely loaded */
#define	SE_NOUNLOAD	0x20		/* syscall never needs unload */
#define	SE_ARGC		0x40		/* syscall takes C-style args */

/*
 * Structure of the return-value parameter passed by reference to
 * system entries.
 */
union rval {
	struct	{
		int	r_v1;
		int	r_v2;
	} r_v;
	off_t	r_off;
	offset_t r_offset;
	time_t	r_time;
	int64_t	r_vals;
};
#define	r_val1	r_v.r_v1
#define	r_val2	r_v.r_v2

typedef union rval rval_t;

#ifdef	_KERNEL

extern int save_syscall_args(void);
extern uint_t get_syscall_args(klwp_t *lwp, long *argp, int *nargsp);
#ifdef _SYSCALL32_IMPL
extern uint_t get_syscall32_args(klwp_t *lwp, int *argp, int *nargp);
#endif
extern uint_t set_errno(uint_t errno);

extern int64_t syscall_ap(void);
extern int64_t loadable_syscall(long, long, long, long, long, long, long, long);
extern int64_t nosys(void);

extern void swtch(void);

extern uint_t	kcpc_key;	/* TSD key for performance counter context */

#ifdef lint
extern	int	__lintzero;	/* for spoofing lint */
#endif

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSTM_H */
