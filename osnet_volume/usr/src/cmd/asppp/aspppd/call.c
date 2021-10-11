#ident	"@(#)call.c	1.25	96/10/30 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <stropts.h>
#include <wait.h>

#include "call.h"
#include "aspppd.h"
#include "fds.h"
#include "ipd_ioctl.h"
#include "log.h"
#include "path.h"
#include "ppp.h"
#include "uucp_glue.h"

static void	call(char *, int);
static void	recv_fd(int);

void
place_call(ipd_con_dis_t dst)
{
	struct path	*p;
	int		pipe_fd[2];

	if ((p = get_path_by_addr(dst)) == NULL) {
		log(0, "place_call: can't find path for %s%d\n",
		    (dst.iftype == IPD_MTP) ? "ipd" : "ipdptp", dst.ifunit);
		return;
	}

	if (pipe(pipe_fd) < 0)
	    fail("place_call: pipe failed\n");

	p->state = dialing;

	/* bug 1262630. SIGHUP caught while establishing conn. */
	conn_cntr++;

	if ((p->pid = fork()) < 0)
	    fail("place_call: fork failed\n");
	if (p->pid == 0) {
		log(42, "place_call: child is %d\n", getpid());
		if (close(pipe_fd[1]) < 0)
		    fail("place_call: close(pipe_fd[1]) failed\n");
		call(p->uucp.system_name, pipe_fd[0]);
		/* never returns, but just in case */
		fail("place_call: call returned unexpectedly\n");
	}

	if (close(pipe_fd[0]) < 0)
	    fail("place_call: close(pipe_fd[0]) failed\n");
	p->s = pipe_fd[1];
	add_to_fds(p->s, (POLLIN | POLLHUP), recv_fd);
}

static void
recv_fd(int index)
{
	int			flags;
	struct path		*p;
	struct strrecvfd	recvfd;
	int			stat;

	if ((p = get_path_by_fd(fds[index].fd)) == NULL)
	    fail("recv_fd: can't find path\n");

	delete_from_fds(p->s);	/* pipe to child process */

	if (waitpid(p->pid, &stat, NULL) < 0)
	    fail("recv_fd: waitpid failed\n");
	p->pid = -1;

	if (WIFEXITED(stat) == 0 || WEXITSTATUS(stat) != 0) {
		log(42, "recv_fd: call to %s failed\n", p->uucp.system_name);
		terminate_path(p);

		/* bug 1262630 */
		conn_cntr--;

		return;
	}

	if (ioctl(p->s, I_RECVFD, &recvfd) < 0)
	    fail("recv_fd: I_RECVFD failed\n");

	if (close(p->s) < 0)
	    fail("recv_fd: pipe close failed\n");

	p->state = connected;
	p->s = recvfd.fd;
	flags = fcntl(p->s, F_GETFL) | O_NONBLOCK;
	if (fcntl(p->s, F_SETFL, flags) < 0)
	    fail("recv_fd: fcntl failed\n");
	add_to_fds(p->s, POLLOUT, start_ppp);
}

static void
call(char *sysname, int fd)
{
	uucp_prolog("ppp");

	Debug = debug;

	Cn = conn(sysname);

	if (Cn < 0) {
		if (close(fd) < 0)
		    fail("call: first close on fd=%d failed\n", fd);
		_exit(EXIT_FAILURE);	/* Don't execute atexit handlers */
	}

	uucp_epilog();

	if (ioctl(fd, I_SENDFD, Cn) < 0)
	    fail("call: I_SENDFD failed\n");

	if (close(fd) < 0)
	    fail("call: second close on fd=%d failed\n", fd);

	cleanup(EXIT_SUCCESS);
}
