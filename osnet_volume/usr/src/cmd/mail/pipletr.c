/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pipletr.c	1.5	92/07/17 SMI" 

#include "mail.h"

dowait(pidval)
pid_t	pidval;
{
	register pid_t w;
	int status;
	void (*istat)(), (*qstat)();

	/*
		Parent temporarily ignores signals so it will remain 
		around for command to finish
	*/
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);

	while ((w = wait(&status)) != pidval && w != CERROR);
	if (w == CERROR) {
		status = -errno;
		signal(SIGINT, istat);
		signal(SIGQUIT, qstat);
		return (status);
	}

	signal(SIGINT, istat);
	signal(SIGQUIT, qstat);
	status = ((status>>8)&0xFF);  		/* extract 8 high order bits */
	return (status);
}

/*
	invoke shell to execute command waiting for command to terminate
		s	-> command string
	return:
		status	-> command exit status
*/
systm(s)
char *s;
{
	pid_t	pid;

	/*
		Spawn the shell to execute command, however, since the 
		mail command runs setgid mode reset the effective group 
		id to the real group id so that the command does not
		acquire any special privileges
	*/
	if ((pid = fork()) == CHILD) {
		setuid(my_uid);
		setgid(my_gid);
#ifdef SVR3
		execl("/bin/sh", "sh", "-c", s, (char*)NULL);
#else
		execl("/usr/bin/sh", "sh", "-c", s, (char*)NULL);
#endif
		exit(127);
	}
	return (dowait(pid));
}
