#ident	"@(#)fds.c	1.14	94/02/17 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#include <malloc.h>
#include <poll.h>
#include <sys/types.h>

#include "fds.h"
#include "log.h"
#include "path.h"
#include "ppp.h"

struct pollfd		*fds = NULL;
unsigned long		nfds = 0;
static unsigned long	max_fds = 8;	/* doubled at initialization */
static void		(**callbacks)() = NULL;

static void		create_fds(void);

static void
create_fds(void)
{
	int		i;
	static int	old_fds = 0;
	void		*ptr;

	max_fds *= 2;
	ptr = realloc((void *)fds, max_fds * sizeof (struct pollfd));
	if (ptr == NULL)
	    fail("create_fds: no memory for fds array\n");
	fds = (struct pollfd *)ptr;

	for (i = old_fds; i < max_fds; ++i)
	    fds[i].fd = -1;
	old_fds = max_fds;

	ptr = realloc((void *)callbacks, max_fds * sizeof (void *));
	if (ptr == NULL)
	    fail("create_fds: no memory for callbacks array\n");
	callbacks = (void(**)())ptr;

	log(42, "create_fds: %lu elements created\n", max_fds);
}

void
add_to_fds(int fd, short events, void (*callback)())
{
	int i;

	if (fd < 0) {
		log(42, "add_to_fds: bogus fd=%d\n", fd);
		return;
	}

	for (i = 0; i < nfds; ++i)
	    if (fds[i].fd == fd) {
		    log(42, "add_to_fds: cannot add copy of fd[%d].fd=%d\n",
			i, fd);
		    return;
	    }

	if (nfds == 0 || nfds >= max_fds)
		create_fds();

	fds[nfds].fd = fd;
	fds[nfds].events = events;
	fds[nfds].revents = 0;
	callbacks[nfds] = callback;

	log(42, "add_to_fds: fds[%d].{fd, events, callback}="
	    "{%d, 0x%x, 0x%x (%d)}\n", nfds, fd, events, callback, callback);

	nfds++;
}

void
delete_from_fds(int fd)
{
	int	i, j;

	for (i = 0; i < nfds; ++i)
		if (fds[i].fd == fd) {
			log(42, "delete_from_fds: fds[%d].fd=%d\n", i, fd);
			for (j = i + 1; j < nfds; ++j, ++i) {
				fds[i] = fds[j];
				callbacks[i] = callbacks[j];
			}
			fds[--nfds].fd = -1;
			return;
		}

	log(42, "fds: attempted removal of non-existent fd\n");
}

void
change_callback(int index, void (*callback)())
{
	callbacks[index] = callback;
}

void
(*do_callback(int index))(int)
{
	return (callbacks[index]);
}

boolean_t
expected_event(struct pollfd fd)
{
	if ((fd.revents & fd.events) == fd.revents)
		return (B_TRUE);
	else
		return (B_FALSE);
}

void
log_bad_event(struct pollfd fd)
{
	static struct pollfd	save_fd;
	static int		count = 0;
	struct path		*p;

	log(42, "log_bad_event: fds.{fd, events, revents}="
	    "{%d, 0x%x, 0x%x}\n", fd.fd, fd.events, fd.revents);

	if (count > 0) {
		if (fd.fd == save_fd.fd &&
		fd.revents == save_fd.revents &&
		fd.events == save_fd.events) {
			if (count++ > 5) {
				log(42, "log_bad_event: possible loop "
				    "terminated\n");
				if ((p = get_path_by_fd(fd.fd)) == NULL)
				    fail("log_bad_event: path not found\n");
				terminate_path(p);
				count = 0;
				return;
			}
		} else
			count = 0;
	}
	if (count++ == 0)
		save_fd = fd;
}
