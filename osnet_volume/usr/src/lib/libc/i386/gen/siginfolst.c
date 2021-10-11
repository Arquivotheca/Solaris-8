/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)siginfolst.c	1.5    96/12/09 SMI"	/* SVr4.0 1.3   */

#include <signal.h>
#include <siginfo.h>

#undef _sys_siginfolist
#define	OLDNSIG 34

const char * _sys_traplist[NSIGTRAP] = {
	"breakpoint trap",
	"trace trap",
	"read access trap",
	"write access trap",
	"execute access trap"
};

const char * _sys_illlist[NSIGILL] = {
	"illegal opcode",
	"illegal operand",
	"illegal addressing mode",
	"illegal trap",
	"privileged instruction",
	"privileged register",
	"co-processor",
	"bad stack"
};

const char * _sys_fpelist[NSIGFPE] = {
	"integer divide by zero",
	"integer overflow",
	"floating point divide by zero",
	"floating point overflow",
	"floating point underflow",
	"floating point inexact result",
	"invalid floating point operation",
	"subscript out of range"
};

const char * _sys_segvlist[NSIGSEGV] = {
	"address not mapped to object",
	"invalid permissions",
};

const char * _sys_buslist[NSIGBUS] = {
	"invalid address alignment",
	"non-existent physical address",
	"object specific"
};

const char * _sys_cldlist[NSIGCLD] = {
	"child has exited",
	"child was killed",
	"child has coredumped",
	"traced child has trapped",
	"child has stopped",
	"stopped child has continued"
};

const char * _sys_polllist[NSIGPOLL] = {
	"input available",
	"output possible",
	"message available",
	"I/O error",
	"high priority input available",
	"device disconnected"
};

struct siginfolist _sys_siginfolist[OLDNSIG-1] = {
	0,		0,			/* SIGHUP */
	0,		0,			/* SIGINT */
	0,		0,			/* SIGQUIT */
	NSIGILL,	(char **)_sys_illlist,	/* SIGILL */
	NSIGTRAP,	(char **)_sys_traplist,	/* SIGTRAP */
	0,		0,			/* SIGABRT */
	0,		0,			/* SIGEMT */
	NSIGFPE,	(char **)_sys_fpelist,	/* SIGFPE */
	0,		0,			/* SIGKILL */
	NSIGBUS,	(char **)_sys_buslist,	/* SIGBUS */
	NSIGSEGV,	(char **)_sys_segvlist,	/* SIGSEGV */
	0,		0,			/* SIGSYS */
	0,		0,			/* SIGPIPE */
	0,		0,			/* SIGALRM */
	0,		0,			/* SIGTERM */
	0,		0,			/* SIGUSR1 */
	0,		0,			/* SIGUSR2 */
	NSIGCLD,	(char **)_sys_cldlist,	/* SIGCLD */
	0,		0,			/* SIGPWR */
	0,		0,			/* SIGWINCH */
	0,		0,			/* SIGURG */
	NSIGPOLL,	(char **)_sys_polllist,	/* SIGPOLL */
	0,		0,			/* SIGSTOP */
	0,		0,			/* SIGTSTP */
	0,		0,			/* SIGCONT */
	0,		0,			/* SIGTTIN */
	0,		0,			/* SIGTTOU */
	0,		0,			/* SIGVTALRM */
	0,		0,			/* SIGPROF */
	0,		0,			/* SIGXCPU */
	0,		0,			/* SIGXFSZ */
	0,		0,			/* SIGWAITING */
	0,		0,			/* SIGLWP */
};

static const struct siginfolist _sys_siginfolist_data[NSIG-1] = {
	0,		0,			/* SIGHUP */
	0,		0,			/* SIGINT */
	0,		0,			/* SIGQUIT */
	NSIGILL,	(char **)_sys_illlist,	/* SIGILL */
	NSIGTRAP,	(char **)_sys_traplist,	/* SIGTRAP */
	0,		0,			/* SIGABRT */
	0,		0,			/* SIGEMT */
	NSIGFPE,	(char **)_sys_fpelist,	/* SIGFPE */
	0,		0,			/* SIGKILL */
	NSIGBUS,	(char **)_sys_buslist,	/* SIGBUS */
	NSIGSEGV,	(char **)_sys_segvlist,	/* SIGSEGV */
	0,		0,			/* SIGSYS */
	0,		0,			/* SIGPIPE */
	0,		0,			/* SIGALRM */
	0,		0,			/* SIGTERM */
	0,		0,			/* SIGUSR1 */
	0,		0,			/* SIGUSR2 */
	NSIGCLD,	(char **)_sys_cldlist,	/* SIGCLD */
	0,		0,			/* SIGPWR */
	0,		0,			/* SIGWINCH */
	0,		0,			/* SIGURG */
	NSIGPOLL,	(char **)_sys_polllist,	/* SIGPOLL */
	0,		0,			/* SIGSTOP */
	0,		0,			/* SIGTSTP */
	0,		0,			/* SIGCONT */
	0,		0,			/* SIGTTIN */
	0,		0,			/* SIGTTOU */
	0,		0,			/* SIGVTALRM */
	0,		0,			/* SIGPROF */
	0,		0,			/* SIGXCPU */
	0,		0,			/* SIGXFSZ */
	0,		0,			/* SIGWAITING */
	0,		0,			/* SIGLWP */
	0,		0,			/* SIGFREEZE */
	0,		0,			/* SIGTHAW */
	0,		0,			/* SIGCANCEL */
	0,		0,			/* SIGLOST */
	0,		0,			/* SIGRTMIN */
	0,		0,			/* SIGRTMIN+1 */
	0,		0,			/* SIGRTMIN+2 */
	0,		0,			/* SIGRTMIN+3 */
	0,		0,			/* SIGRTMAX-3 */
	0,		0,			/* SIGRTMAX-2 */
	0,		0,			/* SIGRTMAX-1 */
	0,		0,			/* SIGRTMAX */
};

const struct siginfolist *_sys_siginfolistp = _sys_siginfolist_data;
