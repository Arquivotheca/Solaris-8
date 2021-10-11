/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)exec.c	1.1	99/01/11 SMI"

/*
 *	cscope - interactive C symbol cross-reference
 *
 *	process execution functions
 */

#include "global.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <varargs.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>

#define	getdtablesize()	_NFILE

#define	MAXARGS	100	/* maximum number of arguments to executed command */

pid_t	childpid;	/* child's process ID */

static	SIGTYPE	(*oldsigquit)();	/* old value of quit signal */
static	SIGTYPE	(*oldsigtstp)();	/* old value of terminal stop signal */

static pid_t myfork(void);
static int join(pid_t p);

/*
 * execute forks and executes a program or shell script, waits for it to
 * finish, and returns its exit code.
 */

/*VARARGS0*/
int
execute(va_alist)	/* note: "exec" is already defined on u370 */
va_dcl
{
	va_list	ap;
	char	*args[MAXARGS + 1];
	int	exitcode;
	int	i;
	char	msg[MSGLEN + 1];
	char	*path;
	pid_t	p;

	/* fork and exec the program or shell script */
	va_start(ap);
	exitcurses();
	if ((p = myfork()) == 0) {

		/* close all files except stdin, stdout, and stderr */
		for (i = 3; i < getdtablesize() && close(i) == 0; ++i) {
			;
		}
		/* execute the program or shell script */
		path = va_arg(ap, char *);
		for (i = 0; i < MAXARGS &&
		    (args[i] = va_arg(ap, char *)) != NULL; ++i) {
		}
		args[i] = NULL;			/* in case MAXARGS reached */
		args[0] = basename(args[0]);
		(void) execvp(path, args);	/* returns only on failure */
		(void) sprintf(msg, "\ncscope: cannot execute %s", path);
		(void) perror(msg);	/* display the reason */
		askforreturn();	 /* wait until the user sees the message */
		exit(1);		/* exit the child */
	} else {
		exitcode = join(p);	/* parent */
	}
	va_end(ap);
	if (noacttimeout) {
		(void) fprintf(stderr,
		    "cscope: no activity time out--exiting\n");
		myexit(SIGALRM);
	}
	entercurses();
	return (exitcode);
}

/* myfork acts like fork but also handles signals */

static pid_t
myfork(void)
{
	pid_t	p;		/* process number */

	oldsigtstp = signal(SIGTSTP, SIG_DFL);
	/* the parent ignores the interrupt and quit signals */
	if ((p = fork()) > 0) {
		childpid = p;
		oldsigquit = signal(SIGQUIT, SIG_IGN);
	}
	/* so they can be used to stop the child */
	else if (p == 0) {
		(void) signal(SIGINT, SIG_DFL);
		(void) signal(SIGQUIT, SIG_DFL);
		(void) signal(SIGHUP, SIG_DFL);	/* restore hangup default */
	}
	/* check for fork failure */
	if (p == -1) {
		myperror("Cannot fork");
	}
	return (p);
}

/* join is the compliment of fork */

static int
join(pid_t p)
{
	int	status;
	pid_t	w;

	/* wait for the correct child to exit */
	do {
		w = wait(&status);
	} while (p != -1 && w != p);
	childpid = 0;

	/* restore signal handling */
	(void) signal(SIGQUIT, oldsigquit);
	(void) signal(SIGTSTP, oldsigtstp);
	/* return the child's exit code */
	return (status >> 8);
}
