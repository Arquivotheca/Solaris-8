#ident	"@(#)fifo.c	1.17	99/10/26 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stropts.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "aspppd.h"
#include "fds.h"
#include "fifo.h"
#include "ifconfig.h"
#include "log.h"
#include "path.h"
#include "ppp.h"

#define	C_MODE (S_IRUSR | S_IWUSR)

static void	delete_fifo_path(void);
static void	accept_fifo_connection(int);
static void	get_fifo_msg(int);

void
create_fifo(void)
{
	int		fd[2];
	int		tmpfd;

	if (pipe(fd) < 0)
	    fail("create_fifo: pipe failed\n");

	if (ioctl(fd[1], I_PUSH, "connld") < 0)
	    fail("cerate_fifo: couldn't push connld\n");

	delete_fifo_path();	/* just in case */

	if ((tmpfd = creat(FIFO_PATH, C_MODE)) < 0)
	    fail("create_fifo: creat failed\n");

	if (close(tmpfd) < 0)
	    fail("cerate_fifo: close failed\n");

	if (atexit(delete_fifo_path) != 0)
	    fail("cerate_fifo: atexit failed\n");

	if (fattach(fd[1], FIFO_PATH) < 0)
	    fail("cerate_fifo: fattach failed\n");

	add_to_fds(fd[0], POLLIN, accept_fifo_connection);
}

static void
delete_fifo_path(void)
{
	struct stat	sbuf;

	errno = 0;
	if (stat(FIFO_PATH, &sbuf) < 0) {
		if (errno != ENOENT)
		    fail("delete_fifo_path: stat failed\n");
	} else if (unlink(FIFO_PATH) < 0)
	    fail("delete_fifo_path: unlink failed\n");
}

static void
accept_fifo_connection(int index)
{
	struct strrecvfd	recvfd;

	if (ioctl(fds[index].fd, I_RECVFD, &recvfd) < 0)
	    fail("accept_fifo_connection: ioctl failed\n");

	add_to_fds(recvfd.fd, (POLLIN | POLLHUP), get_fifo_msg);
}

static void
get_fifo_msg(int index)
{
	struct strbuf		data;
	char			buf[256];
	union fifo_msgs		*fifo_msg;
	int			flags;
	char			*ifp;
	struct path		*p;
	struct strrecvfd	recvfd;

	if ((fds[index].revents & POLLHUP) == POLLHUP) {
		log(42, "get_fifo_msg: hangup detected\n");
		if (close(fds[index].fd) < 0)
		    fail("get_fifo_msg: close failed\n");
		delete_from_fds(fds[index].fd);
		return;
	}

	data.maxlen = sizeof (buf);
	data.buf = buf;
	fifo_msg = (union fifo_msgs *)data.buf;
	flags = 0;
	if (getmsg(fds[index].fd, NULL, &data, &flags) < 0)
	    fail("get_fifo_msg: getmsg failed\n");

	if (data.len <= 0)
	    fail("get_fifo_msg: invalid message format\n");

	log(42, "get_fifo_msg: msg %d received\n", fifo_msg->msg);
	switch (fifo_msg->msg) {
	case FIFO_UNAME:
		/* bug 1262630 */
		conn_cntr++;

		if ((p = get_path_by_name(fifo_msg->uname.uname)) == NULL) {
			log(0, "get_fifo_msg: can't find path with peer_system"
			    "_name %s\n", fifo_msg->uname.uname);
			if (close(fds[index].fd) < 0)
			    fail("get_fifo_msg: close failed\n");
			delete_from_fds(fds[index].fd);
			/* bug 1262630 */
			conn_cntr--;
		} else {
			/* ESC 504429, bugid 1237248. sgypsy@eng */
			if (p->state != inactive) {
			    log(1,
			    "Dialup path already used by another connection\n");
			    close(fds[index].fd);
			    delete_from_fds(fds[index].fd);
			    /* bug 1262630 */
			    conn_cntr--;
			    break;
			}

			/* reminder: next statement is blocking I/O */
			if (ioctl(fds[index].fd, I_RECVFD, &recvfd) < 0)
			    fail("get_fifo_msg: ioctl failed\n");

			if (p->inf.wild_card) {
				ifp = get_new_if();
				if (ifp == NULL) {    /* no interfaces ready */
					log(0, "get_fifo_msg: dynamic inter"
					    "face for %s not available\n",
					    fifo_msg->uname.uname);
					if (close(fds[index].fd) < 0)
					    fail("get_fifo_msg: close "
						"failed\n");
					delete_from_fds(fds[index].fd);
					/* bug 1262630 */
					conn_cntr--;
					break;
				}
				p->inf.ifunit = atoi(ifp+6);
			}
			p->cns_id = fds[index].fd;
			delete_from_fds(fds[index].fd);
			p->s = recvfd.fd;
			p->state = connected;
			flags = fcntl(p->s, F_GETFL) | O_NONBLOCK;
			if (fcntl(p->s, F_SETFL, flags) < 0)
			    fail("get_fifo_msg: fcntl failed\n");
			add_to_fds(p->s, POLLOUT, start_ppp);
		}
		break;
	case FIFO_RESTART:
		longjmp(restart, 2);
		break;
	case FIFO_DEBUG:
		debug = fifo_msg->debug.debug_level;
		log(debug, "get_fifo_msg: debug set to %d\n", debug);
		break;
	default:
		fail("get_fifo_msg: unrecognized FIFO message\n");
		break;
	}
}
