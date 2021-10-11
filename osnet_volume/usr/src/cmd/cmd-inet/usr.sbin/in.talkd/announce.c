/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)announce.c	1.6	97/05/07 SMI"	/* SVr4.0 1.3	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


#include "ctl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>


extern char hostname[];
int nofork = 0;		/* to be set from the debugger */

static int announce_proc(CTL_MSG *request, char *remote_machine);
static void print_mesg(FILE *tf, CTL_MSG *request, char *remote_machine);

extern void print_error(char *string);

/*
 * Because the tty driver insists on attaching a terminal-less
 * process to any terminal that it writes on, we must fork a child
 * to protect ourselves
 */

int
announce(CTL_MSG *request, char *remote_machine)
{
	pid_t pid, val;
	int status;

	if (nofork) {
		return (announce_proc(request, remote_machine));
	}

	if (pid = fork()) {

		/* we are the parent, so wait for the child */
		if (pid == (pid_t)-1) {
			/* the fork failed */
			return (FAILED);
		}

		do {
			val = wait(&status);
			if (val == (pid_t)-1) {
				if (errno == EINTR) {
					continue;
				} else {
					/* shouldn't happen */
					print_error("wait");
					return (FAILED);
				}
			}
		} while (val != pid);

		if ((status & 0377) > 0) {
				/* we were killed by some signal */
			return (FAILED);
		}

		/* Get the second byte, this is the exit/return code */
		return ((status>>8)&0377);
	} else {
		/* we are the child, go and do it */
		_exit(announce_proc(request, remote_machine));
	}
}

#define	TTY_BUFSZ	32

/*
 * See if the user is accepting messages. If so, announce that
 * a talk is requested.
 */
static int
announce_proc(CTL_MSG *request, char *remote_machine)
{
	char full_tty[TTY_BUFSZ];
	FILE *tf;
	struct stat stbuf;
	int fd;
	struct passwd *p;

	(void) snprintf(full_tty, TTY_BUFSZ, "/dev/%s", request->r_tty);
	p = getpwnam(request->r_name);

	if (p == 0 || access(full_tty, 0) != 0) {
		return (FAILED);
	}

	/* fopen uses O_CREAT|O_TRUNC, we don't want that */
	if ((fd = open(full_tty, O_WRONLY|O_NONBLOCK)) == -1) {
		return (PERMISSION_DENIED);
	}
	/* must be tty */
	if (!isatty(fd)) {
		close(fd);
		return (PERMISSION_DENIED);
	}

	/*
	 * open gratuitously attaches the talkd to any tty it opens, so
	 * disconnect us from the tty before we catch a signal
	 */
	(void) setsid();

	if (fstat(fd, &stbuf) < 0 || stbuf.st_uid != p->pw_uid) {
		close(fd);
		return (PERMISSION_DENIED);
	}

	if ((stbuf.st_mode&020) == 0) {
		close(fd);
		return (PERMISSION_DENIED);
	}

	if ((tf = fdopen(fd, "w")) == NULL) {
		close(fd);
		return (PERMISSION_DENIED);
	}

	print_mesg(tf, request, remote_machine);
	fclose(tf);
	return (SUCCESS);
}

#define	max(a, b) ((a) > (b) ? (a) : (b))
#define	N_LINES	5

/*
 * The value should be such that it should be able to accomodate
 * HOST_NAME_LENGTH and other chars in sprintf() below
 */
#define	N_CHARS	300

/*
 * build a block of characters containing the message.
 * It is sent blank filled and in a single block to
 * try to keep the message in one piece if the recipient
 * in in vi at the time
 */
static void
print_mesg(FILE *tf, CTL_MSG *request, char *remote_machine)
{
	struct timeval clock;
	struct tm *localclock;
	char line_buf[N_LINES][N_CHARS];
	int sizes[N_LINES];
	char big_buf[N_LINES*N_CHARS];
	char *bptr, *lptr;
	int i, j, max_size;

	/*
	 * The multiplier 5 below is chosen to accomodate the case where
	 * a character can be !isascii() as well as iscntrl()
	 */
	char l_username[(NAME_SIZE * 5) + 1];
	char r_machinename[(HOST_NAME_LENGTH * 5) + 1];
	int len, k;

	i = 0;
	max_size = 0;

	gettimeofday(&clock, (struct timezone *) NULL);
	localclock = localtime(&clock.tv_sec);

	sprintf(line_buf[i], " ");

	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;

	sprintf(line_buf[i], "Message from Talk_Daemon@%s at %d:%02d ...",
	    hostname, localclock->tm_hour, localclock->tm_min);

	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;

	memset(l_username, '\0', sizeof (l_username));
	len = strlen(request->l_name);
	k = 0;
	for (j = 0; ((j < len) && (k < NAME_SIZE)); j++) {
		if (!isprint(request->l_name[j])) {
			if (!isascii(request->l_name[j])) {
				l_username[k++] = 'M';
				l_username[k++] = '-';
				l_username[k++] = toascii(request->l_name[j]);
			}
			if (iscntrl(request->l_name[j])) {
				l_username[k++] = '^';
				/* add decimal 64 to the control character */
				l_username[k++] = request->l_name[j] + 0100;
			}
		}
		else
			l_username[k++] = request->l_name[j];
	}

	memset(r_machinename, '\0', sizeof (r_machinename));
	len = strlen(remote_machine);
	k = 0;
	for (j = 0; ((j < len) && (k < HOST_NAME_LENGTH)); j++) {
		if (!isprint(remote_machine[j])) {
			if (!isascii(remote_machine[j])) {
				r_machinename[k++] = 'M';
				r_machinename[k++] = '-';
				r_machinename[k++] = toascii(remote_machine[j]);
			}
			if (iscntrl(remote_machine[j])) {
				r_machinename[k++] = '^';
				/* add decimal 64 to the control character */
				r_machinename[k++] = remote_machine[j] + 0100;
			}
		}
		else
			r_machinename[k++] = remote_machine[j];
	}

	sprintf(line_buf[i], "talk: connection requested by %s@%s.",
	    l_username, r_machinename);

	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;

	sprintf(line_buf[i], "talk: respond with:  talk %s@%s",
	    l_username, r_machinename);

	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;

	sprintf(line_buf[i], " ");

	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;

	bptr = big_buf;
	*(bptr++) = '';	/* send something to wake them up */
	*(bptr++) = '\r';	/* add a \r in case of raw mode */
	*(bptr++) = '\n';
	for (i = 0; i < N_LINES; i++) {
		/* copy the line into the big buffer */
		lptr = line_buf[i];
		while (*lptr != '\0') {
			*(bptr++) = *(lptr++);
		}

		/* pad out the rest of the lines with blanks */
		for (j = sizes[i]; j < max_size + 2; j++) {
			*(bptr++) = ' ';
		}

		*(bptr++) = '\r';	/* add a \r in case of raw mode */
		*(bptr++) = '\n';
	}
	*bptr = '\0';

	fprintf(tf, big_buf);
	fflush(tf);
	(void) setsid();
}
