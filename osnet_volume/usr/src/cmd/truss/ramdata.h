/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ramdata.h	1.21	98/02/17 SMI"	/* SVr4.0 1.3	*/

/*
 * ramdata.h -- read/write data declarations.
 */

#include <signal.h>
#include <synch.h>
#include <thread_db.h>

/*
 * Set type for possible filedescriptors.
 */
#define	NOFILES_MAX	(8 * 1024)
typedef struct {
	uint32_t word[(NOFILES_MAX+31)/32];
} fileset_t;

/*
 * Previous stop state enumeration (used by signalled() and requested()).
 */
#define	SLEEPING	1
#define	JOBSIG		2
#define	JOBSTOP		3

/*
 * Simple convenience.
 */
#ifdef	TRUE
#undef	TRUE
#endif
#ifdef	FALSE
#undef	FALSE
#endif
#define	TRUE	1
#define	FALSE	0

extern	char	*command;	/* name of command ("truss") */
extern	int	length;		/* length of printf() output so far */
extern	pid_t	child;		/* pid of fork()ed child process */
extern	char	pname[32];	/* formatted pid of controlled process */
extern	int	interrupt;	/* interrupt signal was received */
extern	int	sigusr1;	/* received SIGUSR1 (release process) */
extern	pid_t	created;	/* if process was created, its process id */
extern	int	Errno;		/* errno for controlled process's syscall */
extern	long	Rval1;		/* rval1 (%r0) for syscall */
extern	long	Rval2;		/* rval2 (%r1) for syscall */
extern	uid_t	Euid;		/* truss's effective uid */
extern	uid_t	Egid;		/* truss's effective gid */
extern	uid_t	Ruid;		/* truss's real uid */
extern	uid_t	Rgid;		/* truss's real gid */
extern	prcred_t credentials;	/* traced process credentials */
extern	int	istty;		/* TRUE iff output is a tty */

extern	int	Fflag;		/* option flags from getopt() */
extern	int	fflag;
extern	int	cflag;
extern	int	aflag;
extern	int	eflag;
extern	int	iflag;
extern	int	lflag;
extern	int	tflag;
extern	int	pflag;
extern	int	sflag;
extern	int	mflag;
extern	int	oflag;
extern	int	vflag;
extern	int	xflag;
extern	int	hflag;

extern	int	dflag;
extern	int	Dflag;
struct tstamp {
	id_t	lwpid;
	int	sec;
	int	fraction;
};
extern	struct tstamp *tstamp;
extern	int	nstamps;

extern	sysset_t trace;		/* sys calls to trace */
extern	sysset_t traceeven;	/* sys calls to trace even if not reported */
extern	sysset_t verbose;	/* sys calls to be verbose about */
extern	sysset_t rawout;	/* sys calls to show in raw mode */
extern	sigset_t signals;	/* signals to trace */
extern	fltset_t faults;	/* faults to trace */
extern	fileset_t readfd;	/* read() file descriptors to dump */
extern	fileset_t writefd;	/* write() file descriptors to dump */

struct counts {		/* structure for keeping counts */
	long sigcount[PRMAXSIG+1];	/* signals count [0..PRMAXSIG] */
	long fltcount[PRMAXFAULT+1];	/* faults count [0..MAXFAULT] */
	long syscount[PRMAXSYS+1];	/* sys calls count [0..PRMAXSYS] */
	long syserror[PRMAXSYS+1];	/* sys calls returning error */
	timestruc_t systime[PRMAXSYS+1]; /* time spent in sys call */
	timestruc_t systotal;		/* total time spent in kernel */
	timestruc_t usrtotal;		/* total time spent in user mode */
	timestruc_t basetime;		/* base time for timestamps */
		/* the following is for internal control */
	lwp_mutex_t mutex[2];	/* mutexes for multi-process synchronization */
	pid_t tpid[1000];	/* truss process pid */
	pid_t spid[1000];	/* subject process pid */
};

extern	struct counts *Cp;	/* for counting: malloc() or shared memory */

struct bkpt {		/* to describe one function's entry point */
	struct bkpt *next;	/* hash table linked list */
	char	*sym_name;	/* function name */
	struct dynlib *dyn;	/* enclosing library */
	uintptr_t addr;		/* function address, breakpointed */
	u_long	instr;		/* original instruction at addr */
	int	flags;		/* see below */
};
#define	BPT_HANG	0x01	/* leave stopped and abandoned when called */
#define	BPT_EXCLUDE	0x02	/* function found but is being excluded */
#define	BPT_INTERNAL	0x04	/* trace internal calls on this function */
#define	BPT_ACTIVE	0x08	/* function breakpoint is set in process */
#define	BPT_PREINIT	0x10	/* PREINIT event in ld.so.1 */
#define	BPT_POSTINIT	0x20	/* POSTINIT event in ld.so.1 */
#define	BPT_DLACTIVITY	0x40	/* DLACTIVITY event in ld.so.1 */

struct dynlib {		/* structure for tracing functions */
	struct dynlib *next;
	char	*lib_name;	/* full library name */
	char	*match_name;	/* library name used in name matching */
	char	*prt_name;	/* library name for printing */
	int	built;		/* if true, bkpt list has been built */
	uintptr_t base;		/* library's mapping base */
	size_t	size;		/* library's mapping size */
};

struct dynpat {		/* structure specifying patterns for dynlib's */
	struct dynpat *next;
	const char **libpat;	/* array of patterns for library names */
	const char **sympat;	/* array of patterns for symbol names */
	int	nlibpat;	/* number of library patterns */
	int	nsympat;	/* number of symbol patterns */
	char	flag;		/* 0 or BPT_HANG */
	char	exclude_lib;	/* if true, exclude these libraries */
	char	exclude;	/* if true, exclude these functions */
	char	internal;	/* if true, trace internal calls */
	struct dynlib *Dp;	/* set to the dynlib instance when searching */
};

extern	struct dynlib *Dyn;	/* for tracing functions in shared libraries */
extern	struct dynpat *Dynpat;
extern	struct dynpat *Lastpat;
extern	struct bkpt **bpt_hashtable;	/* breakpoint hash table */

struct callstack {
	struct callstack *next;
	uintptr_t stkbase;	/* stkbase < stkend */
	uintptr_t stkend;	/* stkend == base + size */
	prgreg_t tref;		/* %g7 (sparc) or %gs (intel) */
	id_t	tid;		/* thread-id */
	u_int	ncall;		/* number of elements in stack */
	u_int	maxcall;	/* max elements in stack (malloc'd) */
	struct {
		uintptr_t sp;		/* %sp for function call */
		uintptr_t pc;		/* value of the return %pc */
		struct bkpt *fcn;	/* name of function called */
	} *stack;		/* pointer to the call stack info */
};

extern	struct callstack *callstack;	/* the callstack list */
extern	u_int	nstack;			/* number of detected stacks */
extern	rd_agent_t *Rdb_agent;		/* handle for librtld_db */
extern	td_thragent_t *Thr_agent;	/* handle for libthread_db */
extern	int	has_libthread;	/* if TRUE, libthread.so.1 is present */

extern	timestruc_t sysbegin;	/* initial value of stime */
extern	timestruc_t syslast;	/* most recent value of stime */
extern	timestruc_t usrbegin;	/* initial value of utime */
extern	timestruc_t usrlast;	/* most recent value of utime */

extern	pid_t	ancestor;	/* top-level parent process id */
extern	int	descendent;	/* TRUE iff descendent of top level */

extern	long	sys_args[9];	/* the arguments to last syscall */
extern	int	sys_nargs;	/* number of arguments to last syscall */
extern	int	sys_indirect;	/* if TRUE, this is an indirect system call */

extern	char	sys_name[12];	/* name of unknown system call */
extern	char	raw_sig_name[SIG2STR_MAX+4]; /* name of known signal */
extern	char	sig_name[12];	/* name of unknown signal */
extern	char	flt_name[12];	/* name of unknown fault */

extern	char	*sys_path;	/* first pathname given to syscall */
extern	size_t	sys_psize;	/* sizeof(*sys_path) */
extern	int	sys_valid;	/* pathname was fetched and is valid */

extern	char	*sys_string;	/* buffer for formatted syscall string */
extern	size_t	sys_ssize;	/* sizeof(*sys_string) */
extern	size_t	sys_leng;	/* strlen(sys_string) */

extern	char	*exec_string;	/* copy of sys_string for exec() only */
extern	char	exec_pname[32];	/* formatted pid for exec() only */
extern	id_t	exec_lwpid;	/* lwpid that performed the exec */

extern	char	*str_buffer;	/* fetchstring() buffer */
extern	size_t	str_bsize;	/* sizeof(*str_buffer) */

#define	IOBSIZE	12		/* number of bytes shown by prt_iob() */
extern	char iob_buf[2*IOBSIZE+8];	/* where prt_iob() leaves its stuff */

extern	char	code_buf[128];	/* for symbolic arguments, e.g., ioctl codes */

extern	int	ngrab;		/* number of pid's to grab */
extern	pid_t	*grab;		/* process id's to grab */

extern	struct ps_prochandle *Proc;	/* global reference to process */
extern	int	data_model;	/* PR_MODEL_LP64 or PR_MODEL_ILP32 */

extern	int	recur;		/* show_strioctl() -- to prevent recursion */

extern	long	pagesize;	/* bytes per page; should be per-process */

extern	int	exit_called;	/* _exit() syscall was seen */
extern	int	slowmode;	/* always wait for tty output to drain */

extern	sysset_t syshang;	/* sys calls to make process hang */
extern	sigset_t sighang;	/* signals to make process hang */
extern	fltset_t flthang;	/* faults to make process hang */
extern	int	Tflag;
extern	int	Sflag;
extern	int	Mflag;
