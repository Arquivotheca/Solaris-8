#ident	"@(#)aspppls.c	1.9	99/10/26 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <termios.h>
#include <unistd.h>

#include "fifo.h"

int errno;

static void fail(const char *, ...);

/*
 * Aspppls is the login service for asynchronous PPP.  Typically, aspppls is
 * the login shell for /etc/passwd entries associated with PPP logins.  When
 * invoked, it connects to the asynchronous PPP daemon, aspppd, via a named
 * pipe FIFO and passes the login user name and a file descriptor associated
 * with the remote system to aspppd.  It then waits until aspppd closes the
 * FIFO, thus indicating that the PPP session has terminated.
 */

void
main(void)
{
	char		buf[256];
	struct strbuf	data;
	register int	fd;		/* FIFO to aspppd */
	struct pollfd	fds;
	union fifo_msgs	*fifo_msg;
	register char	*s;		/* user name string */
	register int	timeout;	/* poll timeout */
	struct termios	tios;

	/* we'll need the root privilege only when we open the FIFO */
	(void) seteuid(getuid());

	/* turn off echoing, signals, etc. */

	if (tcgetattr(STDIN_FILENO, &tios) < 0)
	    fail("tcgetattr failed\n");
	tios.c_lflag = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tios) < 0)
	    fail("tcsetattr failed\n");

	/*
	 * Simple protocol, send:
	 *
	 * 	user name and trailing null
	 *	file descriptor (could be stdin or stdout) that remote end
	 *		connected on
	 */

	if ((s = cuserid(NULL)) == NULL)
	    if ((s = getlogin()) == NULL)
		fail("Login name not found\n");

	/* we need root privilege to open FIFO file */
	(void) seteuid((uid_t)0);

	if ((fd = open(FIFO_PATH, O_WRONLY)) < 0)
	    fail("Can't open %s\n", FIFO_PATH);

	/* revert to non-privileged user after opening the FIFO file */
	if (setuid(getuid()) < 0)
	    fail("Couldn't revert to non-privileged user\n");

	data.buf = buf;
	data.len = sizeof (uname_t);
	fifo_msg = (union fifo_msgs *)data.buf;
	fifo_msg->msg = FIFO_UNAME;
	strcpy(fifo_msg->uname.uname, s);
	if (putmsg(fd, NULL, &data, 0) < 0)
	    fail("Putmsg failed\n");

	if (ioctl(fd, I_SENDFD, STDIN_FILENO) < 0)
	    fail("I_SENDFD failed\n");

	/* Now just wait until aspppd closes the connection */

	fds.fd = fd;
	fds.events = 0;
	timeout = -1;
	switch (poll(&fds, 1, timeout)) {
	case -1:
		fail("poll failed\n");
		break;
	case 0:
		fail("Timeout (%d milleseconds) occured\n", timeout);
		break;
	default:
		if ((fds.revents & POLLHUP) == POLLHUP)	/* expected result */
		    break;

		if ((fds.revents & POLLERR) == POLLERR)
		    fail("poll error\n");
		else fail("Unexpected poll result\n");
		break;
	}
}

static void
fail(const char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	(void) vfprintf(stderr, fmt, args);
	va_end(args);
	if (errno)
	    (void) fprintf(stderr, "Error %d: %s\n", errno, strerror(errno));
	exit(EXIT_FAILURE);
}
