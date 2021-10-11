/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)tmutmp.c	1.14	98/10/22 SMI"	/* SVr4.0 1.10 */

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 */

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<string.h>
#include	<memory.h>
#include	<utmpx.h>
#include	"sac.h"
#include	<security/pam_appl.h>

extern	char	Scratch[];
extern	void	log();
extern	time_t	time();
extern	char	*lastname();

/*
 * account - create a utmpx record for service
 *
 */

int
account(line)
char	*line;
{
	struct utmpx utmpx;			/* prototype utmpx entry */
	struct utmpx *up = &utmpx;		/* and a pointer to it */
	extern	char *Tag;

	(void) memset(up, '\0', sizeof (utmpx));
	up->ut_user[0] = '.';
	(void) strncpy(&up->ut_user[1], Tag, sizeof (up->ut_user)-1);
	(void) strncpy(up->ut_line, lastname(line), sizeof (up->ut_line));
	up->ut_pid = getpid();
	up->ut_type = USER_PROCESS;
	up->ut_id[0] = 't';
	up->ut_id[1] = 'm';
	up->ut_id[2] = SC_WILDC;
	up->ut_id[3] = SC_WILDC;
	up->ut_exit.e_termination = 0;
	up->ut_exit.e_exit = 0;
	(void) time(&up->ut_tv.tv_sec);
	if (makeutx(up) == NULL) {
		(void) sprintf(Scratch, "makeutx for pid %d failed",
			up->ut_pid);
		log(Scratch);
		return (-1);
	}
	return (0);
}

/*
 * checkut_line	- check if a login is active on the requested device
 */
int
checkut_line(line)
char *line;
{
	struct utmpx *u;
	char buf[33], ttyn[33];
	int rvalue = 0;

	(void) strncpy(buf, lastname(line), sizeof (u->ut_line));
	buf[sizeof (u->ut_line)] = '\0';

	setutxent();
	while ((u = getutxent()) != NULL) {
		if (u->ut_type == USER_PROCESS) {
			strncpy(ttyn, u->ut_line, sizeof (u->ut_line));
			ttyn[sizeof (u->ut_line)] = '\0';
			if (strcmp(buf, ttyn) == 0) {
				rvalue = 1;
				break;
			}
		}
	}

	return (rvalue);
}


void
cleanut(pid, status)
	pid_t	pid;
	int	status;
{
	pam_handle_t *pamh;
	struct utmpx *up;
	struct utmpx ut;
	char user[33], ttyn[33], rhost[258];

	setutxent();
	while (up = getutxent()) {
		if (up->ut_pid == pid) {
			if (up->ut_type == DEAD_PROCESS) {
				/* Cleaned up elsewhere. */
				break;
			}

			strncpy(user, up->ut_user, sizeof (up->ut_user));
			user[sizeof (up->ut_user)] = '\0';
			strncpy(ttyn, up->ut_line, sizeof (up->ut_line));
			ttyn[sizeof (up->ut_line)] = '\0';
			strncpy(rhost, up->ut_host, sizeof (up->ut_host));
			rhost[sizeof (up->ut_host)] = '\0';

			if (pam_start("ttymon", user, NULL, &pamh)
							== PAM_SUCCESS) {
				(void) pam_set_item(pamh, PAM_TTY, ttyn);
				(void) pam_set_item(pamh, PAM_RHOST, rhost);
				(void) pam_close_session(pamh, 0);
				(void) pam_end(pamh, PAM_SUCCESS);
			}


			up->ut_type = DEAD_PROCESS;
			up->ut_exit.e_termination = WTERMSIG(status);
			up->ut_exit.e_exit = WEXITSTATUS(status);
			(void) time(&up->ut_tv.tv_sec);

			if (modutx(up) == NULL) {
				/*
				 * Since modutx failed we'll
				 * write out the new entry
				 * ourselves.
				 */
				(void) pututxline(up);
				updwtmpx("wtmpx", up);
			}
			break;
		}
	}
	endutxent();
}

/*
 * getty_account	- This is a copy of old getty account routine.
 *			- This is only called if ttymon is invoked as getty.
 *			- It tries to find its own INIT_PROCESS entry in utmpx
 *			- and change it to LOGIN_PROCESS
 */
void
getty_account(line)
char *line;
{
	pid_t ownpid;
	struct utmpx *u;

	ownpid = getpid();

	setutxent();
	while ((u = getutxent()) != NULL) {

		if (u->ut_type == INIT_PROCESS && u->ut_pid == ownpid) {
			(void) strncpy(u->ut_line, lastname(line),
				sizeof (u->ut_line));
			(void) strncpy(u->ut_user, "LOGIN",
					sizeof (u->ut_user));
			u->ut_type = LOGIN_PROCESS;

			/* Write out the updated entry. */
			(void) pututxline(u);
			break;
		}
	}

	/* create wtmpx entry also */
	if (u != NULL)
		updwtmpx("/etc/wtmpx", u);

	endutxent();
}
