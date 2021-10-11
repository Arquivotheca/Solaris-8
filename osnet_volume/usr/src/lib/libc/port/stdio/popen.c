/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)popen.c	1.36	99/11/03 SMI"	/* SVr4.0 1.29 */

/*LINTLIBRARY*/
#pragma weak pclose = _pclose
#pragma weak popen = _popen

#include "synonyms.h"
#include "shlib.h"
#include "mtlib.h"
#include "file64.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include "stdiom.h"
#include "mse.h"
#include "libc.h"

#define	tst(a, b) (*mode == 'r'? (b) : (a))
#define	RDR	0
#define	WTR	1

#define	BIN_SH "/bin/sh"
#define	BIN_KSH "/bin/ksh"
#define	SH "sh"
#define	KSH "ksh"
#define	SHFLG "-c"

#ifndef	_LP64
#define	MAX_FD (1 << (NBBY * (unsigned)sizeof (_lastbuf->_file))) /* now 256 */
#endif	/*	_LP64	*/

extern	int __xpg4;	/* defined in _xpg4.c; 0 if not xpg4-compiled program */

static mutex_t popen_lock = DEFAULTMUTEX;

typedef struct node {
	pid_t	pid;
	int	fd;
	struct	node	*next;
} node_t;

node_t  *head = NULL;

static	void close_all_fds(void);


FILE *
popen(const char *cmd, const char *mode)
{
	int	p[2];
	int *poptr;
	int myside, yourside, pid;
	FILE	*iop;

	if (pipe(p) < 0)
		return (NULL);

#ifndef	_LP64
	/* check that the fd's are in range for a struct FILE */
	if ((p[WTR] >= MAX_FD) || (p[RDR] >= MAX_FD)) {
		(void) close(p[WTR]);
		(void) close(p[RDR]);
		return (NULL);
	}
#endif	/*	_LP64	*/

	myside = tst(p[WTR], p[RDR]);
	yourside = tst(p[RDR], p[WTR]);
	if ((pid = vfork()) == 0) {
		/* myside and yourside reverse roles in child */
		int	stdio;

		/* close all pipes from other popen's */
		close_all_fds();

		stdio = tst(0, 1);
		(void) close(myside);
		if (yourside != stdio) {
			(void) close(stdio);
			(void) fcntl(yourside, F_DUPFD, stdio);
			(void) close(yourside);
		}

		if (__xpg4 == 0) {	/* not XPG4 */
			if (access(BIN_SH, X_OK))
				_exit(127);
			(void) execl(BIN_SH, SH, SHFLG, cmd, (char *)0);
		} else {
			if (access(BIN_KSH, X_OK))	/* XPG4 Requirement */
				_exit(127);
			(void) execl(BIN_KSH, KSH, SHFLG, cmd, (char *)0);
		}
		_exit(1);
	}
	(void) close(yourside);
	if (pid == -1) {
		(void) close(myside);
		return (NULL);
	}

	if (_insert(pid, myside) == -1) {
		(void) close(myside);
		return (NULL);
	}
	if ((iop = fdopen(myside, mode)) == NULL) {
		(void) close(myside);
		return (NULL);
	}
	_SET_ORIENTATION_BYTE(iop);
	return (iop);
}

int
pclose(FILE *ptr)
{
	int f;
	pid_t	pid;
	int status;

	pid = _delete(fileno(ptr));

	/* mark this pipe closed */
	(void) fclose(ptr);

	if (pid == -1)
		return (-1);

	while (waitpid(pid, &status, _WNOCHLD) < 0) {
		/* If waitpid fails with EINTR, restart the waitpid call */
		if (errno != EINTR) {
			status = -1;
			break;
		}
	}

	return (status);
}


int
_insert(pid_t pid, int fd)
{
	node_t	*prev;
	node_t	*curr;
	node_t	*new;

	(void) _mutex_lock(&popen_lock);

	for (prev = curr = head; curr != NULL; curr = curr->next) {
		prev = curr;
	}

	if ((new = malloc(sizeof (node_t))) == NULL) {
		(void) _mutex_unlock(&popen_lock);
		return (-1);
	}

	new->pid = pid;
	new->fd = fd;
	new->next = NULL;

	if (head == NULL)
		head = new;
	else
		prev->next = new;

	(void) _mutex_unlock(&popen_lock);

	return (0);
}


pid_t
_delete(int fd)
{
	node_t	*prev;
	node_t	*curr;
	pid_t	pid;

	(void) _mutex_lock(&popen_lock);

	for (prev = curr = head; curr != NULL; curr = curr->next) {
		if (curr->fd == fd) {
			if (curr == head)
				head = curr->next;
			else
				prev->next = curr->next;

			pid = curr->pid;
			free(curr);
			(void) _mutex_unlock(&popen_lock);
			return (pid);
		}
		prev = curr;
	}

	(void) _mutex_unlock(&popen_lock);

	return (-1);
}

static void
close_all_fds(void)
{
	node_t	*curr;

	(void) _mutex_lock(&popen_lock);

	for (curr = head; curr != NULL; curr = curr->next) {
		(void) close(curr->fd);
	}

	(void) _mutex_unlock(&popen_lock);
}
