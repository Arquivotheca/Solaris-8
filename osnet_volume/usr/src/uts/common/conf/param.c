/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)param.c	2.183	99/11/24 SMI"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/klwp.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/var.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/conf.h>
#include <sys/class.h>
#include <sys/ts.h>
#include <sys/rt.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/buf.h>
#include <sys/resource.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/vmparam.h>
#include <sys/machparam.h>
#include <sys/utsname.h>
#include <sys/kmem.h>
#include <sys/stack.h>
#include <sys/modctl.h>
#include <sys/fdbuffer.h>
#include <sys/cyclic_impl.h>
#include <sys/rce.h>
#include <sys/disp.h>
#include <sys/tuneable.h>

#include <sys/vmem.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/clock.h>

/*
 * The following few lines describe generic things that must be compiled
 * into the booted executable (unix) rather than genunix or any other
 * module because they're required by kadb, crash dump readers, etc.
 */
int mod_mix_changed;		/* consumed by kadb and ksyms driver */
struct modctl modules;		/* head of linked list of modules */
char *default_path;		/* default module loading path */
struct swapinfo *swapinfo;	/* protected by the swapinfo_lock */
proc_t *practive;		/* active process list */
uint_t nproc;			/* current number of processes */
proc_t p0;			/* process 0 */
struct plock p0lock;		/* p0's p_lock */
klwp_t lwp0;			/* t0's lwp */

/*
 * The following are "implementation architecture" dependent constants made
 * available here in the form of initialized data for use by "implementation
 * architecture" independent modules. See machparam.h.
 */
const unsigned long	_pagesize	= (unsigned long)PAGESIZE;
const unsigned int	_pageshift	= (unsigned int)PAGESHIFT;
const unsigned long	_pageoffset	= (unsigned long)PAGEOFFSET;
/*
 * XXX - This value pagemask has to be a 64bit size because
 * large file support uses this mask on offsets which are 64 bit size.
 * using unsigned leaves the higher 32 bits value as zero thus
 * corrupting offset calculations in the file system and VM.
 */
const u_longlong_t	_pagemask	= (u_longlong_t)PAGEMASK;
const unsigned long	_mmu_pagesize	= (unsigned long)MMU_PAGESIZE;
const unsigned int	_mmu_pageshift	= (unsigned int)MMU_PAGESHIFT;
const unsigned long	_mmu_pageoffset	= (unsigned long)MMU_PAGEOFFSET;
const unsigned long	_mmu_pagemask	= (unsigned long)MMU_PAGEMASK;
const uintptr_t		_kernelbase	= (uintptr_t)KERNELBASE;
const uintptr_t		_userlimit	= (uintptr_t)USERLIMIT;
const uintptr_t		_userlimit32	= (uintptr_t)USERLIMIT32;
#if !defined(__ia64)
const uintptr_t		_argsbase	= (uintptr_t)ARGSBASE;
#endif
const unsigned int	_diskrpm	= (unsigned int)DISKRPM;
const unsigned long	_dsize_limit	= (unsigned long)DSIZE_LIMIT;
const unsigned long	_ssize_limit	= (unsigned long)SSIZE_LIMIT;
const unsigned long	_pgthresh	= (unsigned long)PGTHRESH;
const unsigned int	_maxslp		= (unsigned int)MAXSLP;
const unsigned long	_maxhandspreadpages = (unsigned long)MAXHANDSPREADPAGES;
const int		_ncpu 		= (int)NCPU;
const unsigned long	_defaultstksz	= (unsigned long)DEFAULTSTKSZ;
const unsigned int	_nbpg		= (unsigned int)MMU_PAGESIZE;

/*
 * System parameter formulae.
 *
 * This file is copied into each directory where we compile
 * the kernel; it should be modified there to suit local taste
 * if necessary.
 */

/*
 * Default hz is 100, but if we set hires_tick we get higher resolution
 * clock behavior (currently defined to be 1000 hz).  Higher values seem
 * to work, but are not supported.
 *
 * If we do decide to play with higher values, remember that hz should
 * satisfy the following constraints to avoid integer round-off problems:
 *
 * (1) hz should be in the range 100 <= hz <= MICROSEC.  If hz exceeds
 *     MICROSEC, usec_per_tick will be zero and lots of stuff will break.
 *     Similarly, if hz < 100 then hz / 100 == 0 and stuff will break.
 *
 * (2) If hz <= 1000, it should be both a multiple of 100 and a
 *	divisor of 1000.
 *
 * (3) If hz > 1000, it should be both a multiple of 1000 and a
 *	divisor of MICROSEC.
 *
 * Thus the only reasonable values of hz (i.e. the values that won't
 * cause roundoff error) are: 100, 200, 500, 1000, 2000, 4000, 5000,
 * 8000, 10000, 20000, 25000, 40000, 50000, 100000, 125000, 200000,
 * 250000, 500000, 1000000.  As of this writing (1996) a clock rate
 * of more than about 10 kHz seems utterly ridiculous, although
 * this observation will no doubt seem quaintly amusing one day.
 */
int hz = 100;
int hires_hz = 1000;
int hires_tick = 0;
int cpu_decay_factor = (1 << 20);	/* ((1 << 20) * 100) / hz */
int tick_per_msec;	/* clock ticks per millisecond (zero if hz < 1000) */
int msec_per_tick;	/* millseconds per clock tick (zero if hz > 1000) */
int usec_per_tick;	/* microseconds per clock tick */
int nsec_per_tick;	/* nanoseconds per clock tick */
int max_hres_adj;	/* maximum adjustment of hrtime per tick */

/*
 * Setting "snooping" to a non-zero value will cause a deadman panic if
 * snoop_interval microseconds elapse without lbolt increasing.  The default
 * snoop_interval is 50 seconds.
 */
#define	SNOOP_INTERVAL_MIN	(MICROSEC)
#define	SNOOP_INTERVAL_DEFAULT	(50 * MICROSEC)

int snooping = 0;
uint_t snoop_interval = SNOOP_INTERVAL_DEFAULT;

/*
 * Tables of initialization functions, called from main().
 */

extern void binit(void);
extern void space_init(void);
extern void dnlc_init(void);
extern void vfsinit(void);
extern void finit(void);
extern void strinit(void);
extern void flk_init(void);
extern void ftrace_init(void);
#ifdef TRACE
extern void inittrace(void);
#endif /* TRACE */
extern void softcall_init(void);
extern void sadinit(void);
extern void ttyinit(void);
extern void mp_strinit(void);
extern void schedctl_init(void);
extern void deadman_init(void);
extern void clock_timer_init(void);
extern void clock_realtime_init(void);
extern void clock_highres_init(void);

void	(*init_tbl[])(void) = {
	binit,
	space_init,
	dnlc_init,
	vfsinit,
	finit,
	strinit,
#ifdef TRACE
	inittrace,
#endif /* TRACE */
	softcall_init,
	sadinit,
	ttyinit,
	as_init,
	anon_init,
	segvn_init,
	flk_init,
	schedctl_init,
	fdb_init,
	deadman_init,
	clock_timer_init,
	clock_realtime_init,
	clock_highres_init,
	0
};


/*
 * Any per cpu resources should be initialized via
 * an entry in mp_init_tbl().
 */

void	(*mp_init_tbl[])(void) = {
	mp_strinit,
	ftrace_init,
	cyclic_mp_init,
	0
};

int maxusers;		/* kitchen-sink knob for dynamic configuration */

/*
 * pidmax -- highest pid value assigned by the system
 * Settable in /etc/system
 */
int pidmax = DEFAULT_MAXPID;

/*
 * jump_pid - if set, this value is where pid numbers should start
 * after the first few system pids (0-3) are used.  If 0, pids are
 * chosen in the usual way. This variable can be used to quickly
 * create large pids (by setting it to 100000, for example). pids
 * less than this value will never be chosen.
 */
pid_t jump_pid = DEFAULT_JUMPPID;

/*
 * autoup -- used in struct var for dynamic config of the age a delayed-write
 * buffer must be in seconds before bdflush will write it out.
 */
#define	DEFAULT_AUTOUP	30
int autoup = DEFAULT_AUTOUP;

/*
 * bufhwm -- tuneable variable for struct var for v_bufhwm.
 * high water mark for buffer cache mem usage in units of K bytes.
 */
int bufhwm = 0;

/*
 * Process table.
 */
int maxpid;
int max_nprocs;		/* set in param_init() */
int maxuprc;		/* set in param_init() */
int reserved_procs;
int nthread = 0;

/*
 * UFS tunables
 */
int ufs_ninode;		/* declared here due to backwards compatibility */
int ndquot;		/* declared here due to backwards compatibility */

/*
 * Exec switch table. This is used by the generic exec module
 * to switch out to the desired executable type, based on the
 * magic number. The currently supported types are ELF, a.out
 * (both NMAGIC and ZMAGIC), interpreter (#!) files, COFF files,
 * and Java executables.
 */
/*
 * Magic numbers
 */
short elfmagic = 0x7f45;
short intpmagic = 0x2321;
short jmagic = 0x504b;

#ifdef sparc
short aout_nmagic = NMAGIC;
short aout_zmagic = ZMAGIC;
short aout_omagic = OMAGIC;
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
short coffmagic = 0x4c01;	/* octal 0514 byte-flipped */
#endif
short nomagic = 0;

/*
 * Magic strings
 */
#define	ELF32MAGIC_STRING	"\x7f""ELF\x1"
#define	ELF64MAGIC_STRING	"\x7f""ELF\x2"
#define	INTPMAGIC_STRING	"#!"
#define	JAVAMAGIC_STRING	"PK\003\004"
#define	AOUT_OMAGIC_STRING	"\x1""\x07"	/* 0407 */
#define	AOUT_NMAGIC_STRING	"\x1""\x08"	/* 0410 */
#define	AOUT_ZMAGIC_STRING	"\x1""\x0b"	/* 0413 */
#define	COFFMAGIC_STRING	"\x4c\x1"
#define	NOMAGIC_STRING		""

char elf32magicstr[] = ELF32MAGIC_STRING;
char elf64magicstr[] = ELF64MAGIC_STRING;
char intpmagicstr[] = INTPMAGIC_STRING;
char javamagicstr[] = JAVAMAGIC_STRING;
#ifdef sparc
char aout_nmagicstr[] = AOUT_NMAGIC_STRING;
char aout_zmagicstr[] = AOUT_ZMAGIC_STRING;
char aout_omagicstr[] = AOUT_OMAGIC_STRING;
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
char coffmagicstr[] = COFFMAGIC_STRING;	/* octal 0514 byte-flipped */
#endif
char nomagicstr[] = NOMAGIC_STRING;

char *execswnames[] = {
	"elfexec",	/* Elf32 */
#ifdef _LP64
	"elfexec",	/* Elf64 */
#endif
	"intpexec",
	"javaexec",
#ifdef sparc
	"aoutexec",
	"aoutexec",
	"aoutexec",
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
	"coffexec",
	NULL,
#endif
	NULL,
	NULL,
	NULL
};

struct execsw execsw[] = {
	elf32magicstr, 0, 5, NULL, NULL, NULL,
#ifdef _LP64
	elf64magicstr, 0, 5, NULL, NULL, NULL,
#endif
	intpmagicstr, 0, 2, NULL, NULL, NULL,
	javamagicstr, 0, 4, NULL, NULL, NULL,
#ifdef sparc
	aout_zmagicstr, 2, 2, NULL, NULL, NULL,
	aout_nmagicstr, 2, 2, NULL, NULL, NULL,
	aout_omagicstr, 2, 2, NULL, NULL, NULL,
#endif
#if defined(i386) || defined(__i386) || defined(__ia64)
	coffmagicstr, 0, 2, NULL, NULL, NULL,
	nomagicstr, 0, 0, NULL, NULL, NULL,
#endif
	nomagicstr, 0, 0, NULL, NULL, NULL,
	nomagicstr, 0, 0, NULL, NULL, NULL,
	nomagicstr, 0, 0, NULL, NULL, NULL,
};
int nexectype = sizeof (execsw) / sizeof (execsw[0]);	/* # of exec types */
kmutex_t execsw_lock;	/* Used for allocation of execsw entries */

/*
 * symbols added to make changing max-file-descriptors
 * simple via /etc/system
 */
#define	RLIM_FD_CUR 0x100
#define	RLIM_FD_MAX 0x400

uint_t rlim_fd_cur = RLIM_FD_CUR;
uint_t rlim_fd_max = RLIM_FD_MAX;


/*
 * Default resource limits.
 *
 *	Softlimit	Hardlimit
 */
struct rlimit64 rlimits[RLIM_NLIMITS] = {
	/* max CPU time */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max file size */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max data size */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max stack */
#ifdef	sun4u
	(rlim64_t)DFLSSIZ,		(rlim64_t)RLIM64_INFINITY,
#else
	(rlim64_t)DFLSSIZ,		(rlim64_t)(uint_t)MAXSSIZ,
#endif
	/* max core file size */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
	/* max file descriptors */
	(rlim64_t)RLIM_FD_CUR,		(rlim64_t)RLIM_FD_MAX,
	/* max mapped memory */
	(rlim64_t)RLIM64_INFINITY,	(rlim64_t)RLIM64_INFINITY,
};

/*
 * Why this special map from infinity to actual values?
 * Infinity is a concept and not a number.  Code treating
 * RLIM_INFINITY as a number has caused problems with the
 * behaviour of setrlimit/getrlimit functions and limits future
 * extensions. These structures map the concept of infinity for
 * a resource to a system defined maximum.
 */
rlim64_t rlim_infinity_map[RLIM_NLIMITS] = {
	(rlim64_t)ULONG_MAX,		/* max CPU time */
	(rlim64_t)MAXOFFSET_T,		/* max file size */
	(rlim64_t)(size_t)ULONG_MAX,	/* max data size */
	(rlim64_t)(size_t)LONG_MAX,	/* max stack */
	(rlim64_t)MAXOFF_T,		/* max core file size */
	(rlim64_t)INT_MAX,		/* max file descriptors */
	(rlim64_t)ULONG_MAX,		/* max mapped memory */
};

#if defined(_SYSCALL32_IMPL) || defined(__lint)
/*
 * This map describes 32-bit "infinities".  It is more
 * of a kludge than it looks, because we can't easily implement
 * the address space size limits around exec.  Fortunately, this
 * is ok because the size of the 32-bit "infinite" address space matches
 * the maximum possible size of a 32-bit address space.
 *
 * Note that this map is only used when *interpreting* RLIM64_INFINITY
 * values for a 32-bit process.  The "native" rlim_infinity_map is
 * -always- used when manipulating the resource limits using the entire
 * getrlimit/setrlimit family of system calls -- so the entries in these
 * two maps are not at all independent!
 */
rlim64_t rlim_infinity_map_32[RLIM_NLIMITS] = {
	(rlim64_t)ULONG_MAX,		/* max CPU time */
	(rlim64_t)MAXOFFSET_T,		/* max file size */
	(rlim64_t)(size_t)UINT32_MAX,	/* max data size */
	(rlim64_t)(size_t)INT32_MAX,	/* max stack */
	(rlim64_t)MAXOFF32_T,		/* max core file size */
	(rlim64_t)INT_MAX,		/* max file descriptors */
	(rlim64_t)UINT32_MAX,		/* max mapped memory */
};
#endif	/* _SYSCALL32_IMPL || __lint */

/*
 * Streams tunables
 */
int	nstrpush = 9;
int	maxsepgcnt = 1;

/*
 * strmsgsz is the size for the maximum streams message a user can create.
 * for Release 4.0, a value of zero will indicate no upper bound.  This
 * parameter will disappear entirely in the next release.
 */

ssize_t	strmsgsz = 0x10000;
ssize_t	strctlsz = 1024;
int	rstchown = 1;		/* POSIX_CHOWN_RESTRICTED is enabled */
int	ngroups_max = NGROUPS_MAX_DEFAULT;

/*
 * generic scheduling stuff
 *
 * Configurable parameters for RT and TS are in the respective
 * scheduling class modules.
 */

pri_t maxclsyspri = MAXCLSYSPRI;
pri_t minclsyspri = MINCLSYSPRI;

int maxclass_sz = SA(MAX((sizeof (rtproc_t)), (sizeof (tsproc_t))));
int maxclass_szd = (SA(MAX((sizeof (rtproc_t)), (sizeof (tsproc_t)))) /
	sizeof (double));
char	sys_name[] = "SYS";
char	ts_name[] = "TS";
char	rt_name[] = "RT";

extern pri_t sys_init();
extern classfuncs_t sys_classfuncs;

sclass_t sclass[] = {
	"SYS",	sys_init,	&sys_classfuncs, STATIC_SCHED, 0,
	"",	NULL,	NULL,	NULL, 0,
	"",	NULL,	NULL,	NULL, 0,
	"",	NULL,	NULL,	NULL, 0,
	"",	NULL,	NULL,	NULL, 0,
	"",	NULL,	NULL,	NULL, 0,
};

int loaded_classes = 1;		/* for loaded classes */
kmutex_t class_lock;		/* lock for class[] */

int nclass = sizeof (sclass) / sizeof (sclass_t);
char initcls[] = "TS";
char *initclass = initcls;
char *defaultclass = initcls;
char *extraclass = NULL;

/*
 * SRM Resource Control Extension hook support.
 *
 * rce_ops must be initially NULL at boot. It is only set by
 * sched/SHR when it is ready to handle the calls to the various entry-
 * points defined in sys/rce.h. This happens during successful
 * loading of sched/SHR as a result of an /etc/system patch which changes
 * *initclass to "SHR".
 *
 * SRM may also change rce_ops during the call to SRM_START()
 * (see ../os/main.c), under testing conditions, or when SHR is
 * able to allow itself to be unloaded.  sched/SHR refuses to be
 * loaded or to set rce_ops if the version string in _srm_interface_version
 * doesn't match one that it knows how to support - a type safety
 * check that Solaris and SHR were built with equivalent sys/rce.h.
 */

volatile struct rce_interface *rce_ops = NULL;
const char _srm_interface_version[] = _SRM_INTERFACE_VERSION;

/*
 * Tunable system parameters.
 */

/*
 * The integers tune_* are done this way so that the tune
 * data structure may be "tuned" if necessary from the /etc/system
 * file. The tune data structure is initialized in param_init();
 */

tune_t tune;

/*
 * If freemem < t_getpgslow, then start to steal pages from processes.
 */
int tune_t_gpgslo = 25;

/*
 * Rate at which fsflush is run, in seconds.
 */
#define	DEFAULT_TUNE_T_FSFLUSHR	5
int tune_t_fsflushr = DEFAULT_TUNE_T_FSFLUSHR;

/*
 * The minimum available resident (not swappable) memory to maintain
 * in order to avoid deadlock.  In pages.
 */
int tune_t_minarmem = 25;

/*
 * The minimum available swappable memory to maintain in order to avoid
 * deadlock.  In pages.
 */
int tune_t_minasmem = 25;

int tune_t_flckrec = 512;	/* max # of active frlocks */

pgcnt_t pages_pp_maximum = 200;

int boothowto;			/* boot flags passed to kernel */
struct var v;			/* System Configuration Information */

/*
 * System Configuration Information
 */

#ifdef sparc
char hw_serial[11];		/* read from prom at boot time */
char architecture[] = "sparc";
char hw_provider[] = "Sun_Microsystems";
#endif
#ifdef i386
/*
 * On x86 machines, read hw_serial, hw_provider and srpc_domain from
 * /etc/bootrc at boot time.
 */
char architecture[] = "i386";
char hw_serial[11] = "0";
char hw_provider[SYS_NMLN] = "";
#endif
#ifdef __ia64
/*
 * On ia64 machines, read hw_serial, hw_provider and srpc_domain from
 * /etc/bootrc at boot time.
 */
char architecture[] = "ia64";
char hw_serial[11] = "0";
char hw_provider[SYS_NMLN] = "";
#endif
char srpc_domain[SYS_NMLN] = "";
char platform[SYS_NMLN] = "";	/* read from the devinfo root node */

/* Initialize isa_list */
char *isa_list = architecture;

#define	MIN_DEFAULT_MAXUSERS	8u
#define	MAX_DEFAULT_MAXUSERS	2048u
#define	MAX_MAXUSERS		4096u

void
param_calc(int platform_max_nprocs)
{
	/*
	 * Default to about one "user" per megabyte, taking into
	 * account both physical and virtual constraints.
	 * Note: 2^20 is a meg; shifting right by (20 - PAGESHIFT)
	 * converts pages to megs without integer overflow.
	 */
	if (maxusers == 0) {
		pgcnt_t physmegs = physmem >> (20 - PAGESHIFT);
		pgcnt_t virtmegs = vmem_size(heap_arena, VMEM_FREE) >> 20;
		maxusers = MIN(MAX(MIN(physmegs, virtmegs),
		    MIN_DEFAULT_MAXUSERS), MAX_DEFAULT_MAXUSERS);
	}
	if (maxusers > MAX_MAXUSERS) {
		maxusers = MAX_MAXUSERS;
		cmn_err(CE_NOTE, "maxusers limited to %d", MAX_MAXUSERS);
	}

	if (ngroups_max > NGROUPS_MAX_DEFAULT)
		cmn_err(CE_WARN,
		"ngroups_max of %d > 16, NFS AUTH_SYS will not work properly",
			ngroups_max);

#ifdef DEBUG
	/*
	 * The purpose of maxusers is to prevent memory overcommit.
	 * DEBUG kernels take more space, so reduce maxusers a bit.
	 */
	maxusers = (3 * maxusers) / 4;
#endif

	/*
	 * We need to dynamically change any variables now so that
	 * the setting of maxusers and pidmax propagate to the other
	 * variables that are dependent on them.
	 */
	if (reserved_procs == 0)
		reserved_procs = 5;
	if (pidmax < reserved_procs || pidmax > MAX_MAXPID)
		maxpid = MAX_MAXPID;
	else
		maxpid = pidmax;

	/*
	 * This allows platform-dependent code to constrain the maximum
	 * number of processes allowed in case there are e.g. VM limitations
	 * with how many contexts are available.
	 */
	if (max_nprocs == 0)
		max_nprocs = (10 + 16 * maxusers);
	if (platform_max_nprocs > 0 && max_nprocs > platform_max_nprocs)
		max_nprocs = platform_max_nprocs;
	if (max_nprocs > maxpid)
		max_nprocs = maxpid;

	if (maxuprc == 0)
		maxuprc = (max_nprocs - reserved_procs);
}

void
param_init(void)
{
	/*
	 * Set each individual element of struct var v to be the
	 * default value. This is done this way
	 * so that a user can set the assigned integer value in the
	 * /etc/system file *IF* tuning is needed.
	 */
	v.v_proc = max_nprocs;	/* v_proc - max # of processes system wide */
	v.v_maxupttl = max_nprocs - reserved_procs;
	v.v_maxsyspri = (int)maxclsyspri;  /* max global pri for sysclass */
	v.v_maxup = MIN(maxuprc, v.v_maxupttl); /* max procs per user */
	v.v_autoup = autoup;	/* v_autoup - delay for delayed writes */

	/*
	 * Set each individual element of struct tune to be the
	 * default value. Each struct element This is done this way
	 *  so that a user can set the assigned integer value in the
	 * /etc/system file *IF* tuning is needed.
	 */
	tune.t_gpgslo = tune_t_gpgslo;
	tune.t_fsflushr = tune_t_fsflushr;
	tune.t_minarmem = tune_t_minarmem;
	tune.t_minasmem = tune_t_minasmem;
	tune.t_flckrec = tune_t_flckrec;

	/*
	 * initialization for max file descriptors
	 */
	if (rlim_fd_cur > rlim_fd_max)
		rlim_fd_cur = rlim_fd_max;

	rlimits[RLIMIT_NOFILE].rlim_cur = rlim_fd_cur;
	rlimits[RLIMIT_NOFILE].rlim_max = rlim_fd_max;

	/*
	 * calculations needed if hz was set in /etc/system
	 */
	if (hires_tick)
		hz = hires_hz;

	tick_per_msec = hz / MILLISEC;
	msec_per_tick = MILLISEC / hz;
	usec_per_tick = MICROSEC / hz;
	nsec_per_tick = NANOSEC / hz;
	max_hres_adj = nsec_per_tick >> ADJ_SHIFT;
	cpu_decay_factor = ((1 << 20) * 100) / hz;
}

/*
 * Validate tuneable parameters following /etc/system processing,
 * but prior to param_init().
 */
void
param_check(void)
{
	if (ngroups_max < NGROUPS_UMIN || ngroups_max > NGROUPS_UMAX)
		ngroups_max = NGROUPS_MAX_DEFAULT;

	if (autoup <= 0) {
		autoup = DEFAULT_AUTOUP;
		cmn_err(CE_WARN, "autoup <= 0; defaulting to %d", autoup);
	}

	if (tune_t_fsflushr <= 0) {
		tune_t_fsflushr = DEFAULT_TUNE_T_FSFLUSHR;
		cmn_err(CE_WARN, "tune_t_fsflushr <= 0; defaulting to %d",
		    tune_t_fsflushr);
	}

	if (jump_pid < 0 || jump_pid >= pidmax) {
		jump_pid = 0;
		cmn_err(CE_WARN, "jump_pid < 0 or >= pidmax; ignored");
	}

	if (snoop_interval < SNOOP_INTERVAL_MIN) {
		snoop_interval = SNOOP_INTERVAL_DEFAULT;
		cmn_err(CE_WARN, "snoop_interval < minimum (%d); defaulting"
		    " to %d", SNOOP_INTERVAL_MIN, SNOOP_INTERVAL_DEFAULT);
	}
}
