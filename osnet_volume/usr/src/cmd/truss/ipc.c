/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ipc.c	1.28	99/05/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <stropts.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/termio.h>
#include <libproc.h>
#include "ramdata.h"
#include "proto.h"

/*
 * Routines related to interprocess communication
 * among the truss processes which are controlling
 * multiple traced processes.
 */

/*
 * Function prototypes for static routines in this module.
 */
static	void	Ecritical(int);
static	void	Xcritical(int);
static	void	msleep(unsigned int);

/*
 * Ensure everyone keeps out of each other's way
 * while writing lines of trace output.
 */
void
Flush()
{
	/*
	 * Except for regions bounded by Eserialize()/Xserialize(),
	 * this is the only place anywhere in the program where a
	 * write() to the trace output file takes place, so here
	 * is where we detect errors writing to the output.
	 */

	errno = 0;

	Ecritical(0);
	(void) fflush(stdout);
	if (slowmode)
		(void) ioctl(fileno(stdout), TCSBRK, 1);
	Xcritical(0);

	if (ferror(stdout) && errno)	/* error on write(), probably EPIPE */
		interrupt = TRUE;		/* post an interrupt */
}

/*
 * Eserialize() and Xserialize() are used to bracket
 * a region which may produce large amounts of output,
 * such as showargs()/dumpargs().
 */

void
Eserialize()
{
	/* serialize output */
	Ecritical(0);
}

void
Xserialize()
{
	(void) fflush(stdout);
	if (slowmode)
		(void) ioctl(fileno(stdout), TCSBRK, 1);
	Xcritical(0);
}

/*
 * Enter critical region ---
 * Wait on mutex, lock out other processes.
 */
static void
Ecritical(int num)
{
	int rv = _lwp_mutex_lock(&Cp->mutex[num]);

	if (rv != 0) {
		char mnum[2];
		mnum[0] = '0' + num;
		mnum[1] = '\0';
		errno = rv;
		perror(command);
		errmsg("cannot grab mutex #", mnum);
	}
}

/*
 * Exit critical region ---
 * Release other processes waiting on mutex.
 */
static void
Xcritical(int num)
{
	int rv = _lwp_mutex_unlock(&Cp->mutex[num]);

	if (rv != 0) {
		char mnum[2];
		mnum[0] = '0' + num;
		mnum[1] = '\0';
		errno = rv;
		perror(command);
		errmsg("cannot release mutex #", mnum);
	}
}

/*
 * Sleep for the specified number of milliseconds.
 */
static void
msleep(unsigned int msec)
{
	if (msec)
		(void) poll(NULL, 0UL, (int)msec);
}

/*
 * Add process to set of those being traced.
 */
void
procadd(pid_t spid)
{
	int i;
	int j = -1;

	if (Cp == NULL)
		return;

	Ecritical(1);
	for (i = 0; i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]); i++) {
		if (Cp->tpid[i] == 0) {
			if (j == -1)	/* remember first vacant slot */
				j = i;
			if (Cp->spid[i] == 0)	/* this slot is better */
				break;
		}
	}
	if (i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]))
		j = i;
	if (j >= 0) {
		Cp->tpid[j] = getpid();
		Cp->spid[j] = spid;
	}
	Xcritical(1);
}

/*
 * Delete process from set of those being traced.
 */
void
procdel()
{
	int i;
	pid_t tpid;

	if (Cp == NULL)
		return;

	tpid = getpid();

	Ecritical(1);
	for (i = 0; i < sizeof (Cp->tpid) / sizeof (Cp->tpid[0]); i++) {
		if (Cp->tpid[i] == tpid) {
			Cp->tpid[i] = 0;
			break;
		}
	}
	Xcritical(1);
}

/*
 * Check for open of a /proc/nnnnn file.
 * Return 0 if this is not an open of a /proc file.
 * Return 1 if the process opened itself.
 * Return 2 if the process opened another process in truss's
 * set of controlled processes, after notifying and waiting
 * for the other controlling truss process to terminate.
 */
int
checkproc(struct ps_prochandle *Pr, char *path)
{
	const pstatus_t *Psp = Pstatus(Pr);
	int what = Psp->pr_lwp.pr_what;		/* SYS_open or SYS_open64 */
	int err = Psp->pr_lwp.pr_errno;
	int pid;
	int i;
	const char *dirname;
	char *next;
	char *sp1;
	char *sp2;
	prgreg_t pc;

	/*
	 * A bit heuristic ...
	 * Test for the cases:
	 *	1234
	 *	1234/as
	 *	1234/ctl
	 *	1234/lwp/24/lwpctl
	 *	.../1234
	 *	.../1234/as
	 *	.../1234/ctl
	 *	.../1234/lwp/24/lwpctl
	 * Insert a '\0', if necessary, so the path becomes ".../1234".
	 *
	 * Along the way, watch out for /proc/self and /proc/1234/lwp/agent
	 */
	if ((sp1 = strrchr(path, '/')) == NULL)		/* last component */
		/* EMPTY */;
	else if (isdigit(*(sp1+1))) {
		sp1 += strlen(sp1);
		while (--sp1 > path && isdigit(*sp1))
			;
		if (*sp1 != '/')
			return (0);
	} else if (strcmp(sp1+1, "as") == 0 ||
	    strcmp(sp1+1, "ctl") == 0) {
		*sp1 = '\0';
	} else if (strcmp(sp1+1, "lwpctl") == 0) {
		/*
		 * .../1234/lwp/24/lwpctl
		 * ............   ^-- sp1
		 */
		if (sp1-6 >= path && strncmp(sp1-6, "/agent", 6) == 0)
			sp1 -= 6;
		else {
			while (--sp1 > path && isdigit(*sp1))
				;
		}
		if (*sp1 != '/' ||
		    (sp1 -= 4) <= path ||
		    strncmp(sp1, "/lwp", 4) != 0)
			return (0);
		*sp1 = '\0';
	} else if (strcmp(sp1+1, "self") != 0) {
		return (0);
	}

	if ((sp2 = strrchr(path, '/')) == NULL)
		dirname = path;
	else
		dirname = sp2 + 1;

	if (strcmp(dirname, "self") == 0) {
		pid = Psp->pr_pid;
	} else if ((pid = strtol(dirname, &next, 10)) < 0 ||
	    *next != '\0') {	/* dirname not a number */
		if (sp1 != NULL)
			*sp1 = '/';
		return (0);
	}
	if (sp2 == NULL)
		dirname = ".";
	else {
		*sp2 = '\0';
		dirname = path;
	}

	if (!Pisprocdir(Pr, dirname) ||	/* file not in a /proc directory */
	    pid == getpid() ||		/* process opened truss's /proc file */
	    pid == 0) {			/* process opened process 0 */
		if (sp1 != NULL)
			*sp1 = '/';
		if (sp2 != NULL)
			*sp2 = '/';
		return (0);
	}
	if (sp1 != NULL)
		*sp1 = '/';
	if (sp2 != NULL)
		*sp2 = '/';

	/*
	 * Process did open a /proc file ---
	 */
	if (pid == Psp->pr_pid) {	/* process opened its own /proc file */
		/*
		 * In SunOS 5.6 and beyond, self-opens always succeed.
		 */
		return (1);
	} else {		/* send signal to controlling truss process */
		for (i = 0; i < sizeof (Cp->tpid)/sizeof (Cp->tpid[0]); i++) {
			if (Cp->spid[i] == pid) {
				pid = Cp->tpid[i];
				break;
			}
		}
		if (i >= sizeof (Cp->tpid) / sizeof (Cp->tpid[0]))
			return (0);	/* don't attempt retry of open() */

		/* wait for controlling process to terminate */
		while (pid && Cp->tpid[i] == pid) {
			if (kill(pid, SIGUSR1) == -1)
				break;
			msleep(1000);
		}
		Ecritical(1);
		if (Cp->tpid[i] == 0)
			Cp->spid[i] = 0;
		Xcritical(1);
	}

	if (err == 0 && pr_close(Pr, Psp->pr_lwp.pr_rval1) == 0)
		err = 1;
	if (err) {	/* prepare to reissue the open() system call */
		(void) Pgetareg(Pr, R_PC, &pc);
#if sparc
		if (sys_indirect) {
			(void) Pputareg(Pr, R_G1, (prgreg_t)SYS_syscall);
			(void) Pputareg(Pr, R_O0, (prgreg_t)what);
			for (i = 0; i < 5; i++)
				(void) Pputareg(Pr, R_O1+i, sys_args[i]);
		} else {
			(void) Pputareg(Pr, R_G1, (prgreg_t)what);
			for (i = 0; i < 6; i++)
				(void) Pputareg(Pr, R_O0+i, sys_args[i]);
		}
		(void) Pputareg(Pr, R_nPC, pc);
#elif i386
		(void) Pputareg(Pr, EAX, (prgreg_t)what);
#elif __ia64
		/* XXX Merced -- fix me */
		(void) Pputareg(Pr, EAX, (prgreg_t)what);
#else
#error "unrecognized architecture"
#endif
		(void) Pputareg(Pr, R_PC, pc - sizeof (syscall_t));
		return (2);
	}

	return (0);
}
