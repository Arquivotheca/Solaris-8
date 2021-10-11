/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prfile.c	1.2	99/09/08 SMI"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include "prtable.h"
#include "prutil.h"
#include "prfile.h"

#define	FDS_TABLE_SIZE	1024

static fd_t *fd_tbl = NULL;
static int fd_max;
static int fd_cnt;
static int fd_cnt_cur;
static int fd_cnt_old;
static fds_t *fds_tbl[FDS_TABLE_SIZE];

void
fd_init(int n)
{
	fd_max = n;
	fd_cnt = fd_cnt_cur = fd_cnt_old = 0;
	fd_tbl = Zalloc(sizeof (fd_t) * n);
	(void) memset(fds_tbl, 0, sizeof (fds_t *) * FDS_TABLE_SIZE);
}

void
fd_exit()
{
	if (fd_tbl)
		free(fd_tbl);
}

void
fd_close(fd_t *fdp)
{
	if (fdp) {
		if (fdp->fd_fd >= 0 && fdp->fd_name[0] != '\0') {
			(void) close(fdp->fd_fd);
			fd_cnt--;
		}

		(void) memset(fdp, 0, sizeof (fd_t));
		fdp->fd_fd = -1;
	}
}

void
fd_closeall()
{
	fd_t *fdp = fd_tbl;
	int i;

	for (i = 0; i < fd_max; i++) {
		fd_close(fdp);
		fdp++;
	}
}

static void
fd_recycle()
{
	fd_t *fdp = fd_tbl;
	int counter;
	int i;

	counter = abs(fd_cnt_old - fd_cnt) + NUM_RESERVED_FD;

	for (i = 0; i < fd_max; i++, fdp++) {

		if (fdp->fd_fd == -1)
			continue;	/* skip recycled ones */

		if (fdp->fd_name[0] != '\0') {	/* file has name */
			(void) close(fdp->fd_fd);
			fd_cnt--;
			counter--;
			fdp->fd_fd = -1;
		}

		if (counter == 0)
			break;
	}
}

fd_t *
fd_open(char *name, int flags, fd_t *fdp)
{
	fd_t *fdp_new;
	int fd;

	if (fd_cnt > fd_max - NUM_RESERVED_FD)
		fd_recycle();

	if (fdp != NULL) {
		if ((strcmp(fdp->fd_name, name) == 0) && (fdp->fd_fd >= 0)) {
			fd_cnt_cur++;
			return (fdp);
		}
	}

again:	fd = open(name, flags);

	if (fd == -1) {
		if ((errno == EMFILE) || (errno == ENFILE)) {
			fd_recycle();
			goto again;
		}
		fdp_new = NULL;
	} else {
		fdp_new = &fd_tbl[fd];
		fdp_new->fd_fd = fd;
		fdp_new->fd_flags = flags;
		(void) strcpy(fdp_new->fd_name, name);
		fd_cnt++;
		fd_cnt_cur++;
	}
	return (fdp_new);
}

int
fd_getfd(fd_t *fdp)
{
	return (fdp->fd_fd);
}

void
fd_update()
{
	fd_cnt_old = fd_cnt_cur;
	fd_cnt_cur = 0;
}

fds_t *
fds_get(pid_t pid)
{
	fds_t *fdsp;
	int hash = pid % FDS_TABLE_SIZE;

	for (fdsp = fds_tbl[hash]; fdsp; fdsp = fdsp->fds_next)
		if (fdsp->fds_pid == pid)	/* searching for pid */
			return (fdsp);

	fdsp = Zalloc(sizeof (fds_t));	/* adding new if pid was not found */
	fdsp->fds_pid = pid;
	fdsp->fds_next = fds_tbl[hash];
	fds_tbl[hash] = fdsp;
	return (fdsp);
}

void
fds_rm(pid_t pid)
{
	fds_t *fds;
	fds_t *fds_prev = NULL;
	int hash = pid % FDS_TABLE_SIZE;

	for (fds = fds_tbl[hash]; fds && fds->fds_pid != pid;
	    fds = fds->fds_next)	/* finding pid */
		fds_prev = fds;

	if (fds) {			/* if pid was found */

		fd_close(fds->fds_psinfo);
		fd_close(fds->fds_usage);
		fd_close(fds->fds_lpsinfo);
		fd_close(fds->fds_lusage);

		if (fds_prev)
			fds_prev->fds_next = fds->fds_next;
		else
			fds_tbl[hash] = fds->fds_next;

		free(fds);
	}
}
