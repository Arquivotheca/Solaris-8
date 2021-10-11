/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ramdata.c	1.19	98/02/17 SMI"	/* SVr4.0 1.3	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <libproc.h>
#include "ramdata.h"
#include "proto.h"

/*
 * ramdata.c -- read/write data definitions are collected here.
 */

char	*command = NULL;	/* name of command ("truss") */
int	length = 0;		/* length of printf() output so far */
pid_t	child = 0;		/* pid of fork()ed child process */
char	pname[32] = "";		/* formatted pid of controlled process */
int	interrupt = FALSE;	/* interrupt signal was received */
int	sigusr1 = FALSE;	/* received SIGUSR1 (release process) */
pid_t	created = 0;		/* if process was created, its process id */
int	Errno = 0;		/* errno for controlled process's syscall */
long	Rval1 = 0;		/* rval1 from syscall */
long	Rval2 = 0;		/* rval2 from syscall */
uid_t	Euid = 0;		/* truss's effective uid */
uid_t	Egid = 0;		/* truss's effective gid */
uid_t	Ruid = 0;		/* truss's real uid */
uid_t	Rgid = 0;		/* truss's real gid */
prcred_t credentials = { 0 };	/* traced process credentials */
int	istty = FALSE;		/* TRUE iff output is a tty */

int	Fflag = 0;		/* option flags from getopt() */
int	fflag = FALSE;
int	cflag = FALSE;
int	aflag = FALSE;
int	eflag = FALSE;
int	iflag = FALSE;
int	lflag = FALSE;
int	tflag = FALSE;
int	pflag = FALSE;
int	sflag = FALSE;
int	mflag = FALSE;
int	oflag = FALSE;
int	vflag = FALSE;
int	xflag = FALSE;
int	hflag = FALSE;

int	dflag = FALSE;
int	Dflag = FALSE;
struct tstamp *tstamp = NULL;
int	nstamps = 0;

sysset_t trace = { 0 };		/* sys calls to trace */
sysset_t traceeven = { 0 };	/* sys calls to trace even if not reported */
sysset_t verbose = { 0 };	/* sys calls to be verbose about */
sysset_t rawout = { 0 };	/* sys calls to show in raw mode */
sigset_t signals = { 0 };	/* signals to trace */
fltset_t faults = { 0 };	/* faults to trace */

fileset_t readfd = { 0 };	/* read() file descriptors to dump */
fileset_t writefd = { 0 };	/* write() file descriptors to dump */

struct counts *Cp = NULL;	/* for counting: malloc() or shared memory */

struct dynlib *Dyn = NULL;	/* for tracing functions in shared libraries */
struct dynpat *Dynpat = NULL;
struct dynpat *Lastpat = NULL;
struct bkpt **bpt_hashtable = NULL;	/* breakpoint hash table */
struct callstack *callstack = NULL;	/* the callstack array */
u_int	nstack = 0;			/* number of detected stacks */
rd_agent_t *Rdb_agent = NULL;		/* handle for librtld_db */
td_thragent_t *Thr_agent = NULL;	/* handle for libthread_db */
int	has_libthread = FALSE;	/* if TRUE, libthread.so.1 is present */

timestruc_t sysbegin = { 0 };	/* initial value of stime */
timestruc_t syslast = { 0 };	/* most recent value of stime */
timestruc_t usrbegin = { 0 };	/* initial value of utime */
timestruc_t usrlast = { 0 };	/* most recent value of utime */

pid_t	ancestor = 0;		/* top-level parent process id */
int	descendent = FALSE;	/* TRUE iff descendent of top level */

long	sys_args[9] = { 0 };	/* the arguments to last syscall */
int	sys_nargs = -1;		/* number of arguments to last syscall */
int	sys_indirect = FALSE;	/* if TRUE, this is an indirect system call */

char	sys_name[12] = "sys#ddd"; /* name of unknown system call */
char	raw_sig_name[SIG2STR_MAX+4] = "SIGxxxxx"; /* name of known signal */
char	sig_name[12] = "SIG#dd"; /* name of unknown signal */
char	flt_name[12] = "FLT#dd"; /* name of unknown fault */

char	*sys_path = NULL;	/* first pathname given to syscall */
size_t	sys_psize = 0;		/* sizeof(*sys_path) */
int	sys_valid = FALSE;	/* pathname was fetched and is valid */

char	*sys_string = NULL;	/* buffer for formatted syscall string */
size_t	sys_ssize = 0;		/* sizeof(*sys_string) */
size_t	sys_leng = 0;		/* strlen(sys_string) */

char	*exec_string = NULL;	/* copy of sys_string for exec() only */
char	exec_pname[32] = "";	/* formatted pid for exec() only */
id_t	exec_lwpid = 0;		/* lwpid that performed the exec */

char	*str_buffer = NULL;	/* fetchstring() buffer */
size_t	str_bsize = 0;		/* sizeof(*str_buffer) */

char iob_buf[2*IOBSIZE+8] = { 0 }; /* where prt_iob() leaves its stuff */

char	code_buf[128] = { 0 };	/* for symbolic arguments, e.g., ioctl codes */

int	ngrab = 0;		/* number of pid's to grab */
pid_t	*grab = NULL;		/* process id's to grab */

struct ps_prochandle *Proc = NULL;	/* global reference to process */
int	data_model = 0;		/* PR_MODEL_LP64 or PR_MODEL_ILP32 */

int	recur = 0;		/* show_strioctl() -- to prevent recursion */

long	pagesize = 0;		/* bytes per page; should be per-process */

int	exit_called = FALSE;	/* _exit() syscall was seen */
int	slowmode = FALSE;	/* always wait for tty output to drain */

sysset_t syshang = { 0 };	/* sys calls to make process hang */
sigset_t sighang = { 0 };	/* signals to make process hang */
fltset_t flthang = { 0 };	/* faults to make process hang */
int	Tflag = FALSE;
int	Sflag = FALSE;
int	Mflag = FALSE;
