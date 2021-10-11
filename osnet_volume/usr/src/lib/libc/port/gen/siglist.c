/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)siglist.c	1.14	97/01/08 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <signal.h>

#undef	_sys_nsig
#undef	_sys_siglist
#define	OLDNSIG	34

const int _sys_nsig = OLDNSIG;

static const char	STR_SIG_UNK[]	= "UNKNOWN SIGNAL";
static const char	STR_SIGHUP[]	= "Hangup";
static const char	STR_SIGINT[]	= "Interrupt";
static const char	STR_SIGQUIT[]	= "Quit";
static const char	STR_SIGILL[]	= "Illegal Instruction";
static const char	STR_SIGTRAP[]	= "Trace/Breakpoint Trap";
static const char	STR_SIGABRT[]	= "Abort";
static const char	STR_SIGEMT[]	= "Emulation Trap";
static const char	STR_SIGFPE[]	= "Arithmetic Exception";
static const char	STR_SIGKILL[]	= "Killed";
static const char	STR_SIGBUS[]	= "Bus Error";
static const char	STR_SIGSEGV[]	= "Segmentation Fault";
static const char	STR_SIGSYS[]	= "Bad System Call";
static const char	STR_SIGPIPE[]	= "Broken Pipe";
static const char	STR_SIGALRM[]	= "Alarm Clock";
static const char	STR_SIGTERM[]	= "Terminated";
static const char	STR_SIGUSR1[]	= "User Signal 1";
static const char	STR_SIGUSR2[]	= "User Signal 2";
static const char	STR_SIGCLD[]	= "Child Status Changed";
static const char	STR_SIGPWR[]	= "Power-Fail/Restart";
static const char	STR_SIGWINCH[]	= "Window Size Change";
static const char	STR_SIGURG[]	= "Urgent Socket Condition";
static const char	STR_SIGPOLL[]	= "Pollable Event";
static const char	STR_SIGSTOP[]	= "Stopped (signal)";
static const char	STR_SIGTSTP[]	= "Stopped (user)";
static const char	STR_SIGCONT[]	= "Continued";
static const char	STR_SIGTTIN[]	= "Stopped (tty input)";
static const char	STR_SIGTTOU[]	= "Stopped (tty output)";
static const char	STR_SIGVTALRM[]	= "Virtual Timer Expired";
static const char	STR_SIGPROF[]	= "Profiling Timer Expired";
static const char	STR_SIGXCPU[]	= "Cpu Limit Exceeded";
static const char	STR_SIGXFSZ[]	= "File Size Limit Exceeded";
static const char	STR_SIGWAITING[]	= "No runnable lwp";
static const char	STR_SIGLWP[]	= "Inter-lwp signal";

const char *_sys_siglist[OLDNSIG] = {
	STR_SIG_UNK,	STR_SIGHUP,	STR_SIGINT,	STR_SIGQUIT,
	STR_SIGILL,	STR_SIGTRAP,	STR_SIGABRT,	STR_SIGEMT,
	STR_SIGFPE,	STR_SIGKILL,	STR_SIGBUS,	STR_SIGSEGV,
	STR_SIGSYS,	STR_SIGPIPE,	STR_SIGALRM,	STR_SIGTERM,
	STR_SIGUSR1,	STR_SIGUSR2,	STR_SIGCLD,	STR_SIGPWR,
	STR_SIGWINCH,	STR_SIGURG,	STR_SIGPOLL,	STR_SIGSTOP,
	STR_SIGTSTP,	STR_SIGCONT,	STR_SIGTTIN,	STR_SIGTTOU,
	STR_SIGVTALRM,	STR_SIGPROF,	STR_SIGXCPU,	STR_SIGXFSZ,
	STR_SIGWAITING,	STR_SIGLWP,
};

static const char *_sys_siglist_data[NSIG] = {
	STR_SIG_UNK,	STR_SIGHUP,	STR_SIGINT,	STR_SIGQUIT,
	STR_SIGILL,	STR_SIGTRAP,	STR_SIGABRT,	STR_SIGEMT,
	STR_SIGFPE,	STR_SIGKILL,	STR_SIGBUS,	STR_SIGSEGV,
	STR_SIGSYS,	STR_SIGPIPE,	STR_SIGALRM,	STR_SIGTERM,
	STR_SIGUSR1,	STR_SIGUSR2,	STR_SIGCLD,	STR_SIGPWR,
	STR_SIGWINCH,	STR_SIGURG,	STR_SIGPOLL,	STR_SIGSTOP,
	STR_SIGTSTP,	STR_SIGCONT,	STR_SIGTTIN,	STR_SIGTTOU,
	STR_SIGVTALRM,	STR_SIGPROF,	STR_SIGXCPU,	STR_SIGXFSZ,
	STR_SIGWAITING,	STR_SIGLWP,
		"Checkpoint Freeze",		/* SIGFREEZE	*/
		"Checkpoint Thaw",		/* SIGTHAW	*/
		"Thread Cancellation",		/* SIGCANCEL	*/
		"Resource Lost",		/* SIGLOST	*/
		"First Realtime Signal",	/* SIGRTMIN	*/
		"Second Realtime Signal",	/* SIGRTMIN+1	*/
		"Third Realtime Signal",	/* SIGRTMIN+2	*/
		"Fourth Realtime Signal",	/* SIGRTMIN+3	*/
		"Fourth Last Realtime Signal",	/* SIGRTMAX-3	*/
		"Third Last Realtime Signal",	/* SIGRTMAX-2	*/
		"Second Last Realtime Signal",	/* SIGRTMAX-1	*/
		"Last Realtime Signal"		/* SIGRTMAX	*/
};

const int	_sys_siglistn = sizeof (_sys_siglist_data) / sizeof (char *);
const char	**_sys_siglistp = _sys_siglist_data;
