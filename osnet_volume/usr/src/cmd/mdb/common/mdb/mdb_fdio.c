/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_fdio.c	1.2	99/11/19 SMI"

/*
 * File Descriptor I/O Backend
 *
 * Simple backend to pass though io_ops to the corresponding system calls on
 * an underlying fd.  We provide functions to create fdio objects using file
 * descriptors, explicit file names, and path lookups.  We save the complete
 * filename so that mdb_iob_name can be used to report the complete filename
 * of an open macro file in syntax error messages.
 */

#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <mdb/mdb_modapi.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb.h>

typedef struct fd_data {
	char fd_name[MAXPATHLEN];	/* Save filename for error messages */
	int fd_fd;			/* File descriptor */
} fd_data_t;

static ssize_t
fdio_read(mdb_io_t *io, void *buf, size_t nbytes)
{
	fd_data_t *fdp = io->io_data;

	if (io->io_next == NULL)
		return (read(fdp->fd_fd, buf, nbytes));

	return (IOP_READ(io->io_next, buf, nbytes));
} 

static ssize_t
fdio_write(mdb_io_t *io, const void *buf, size_t nbytes)
{
	fd_data_t *fdp = io->io_data;

	if (io->io_next == NULL)
		return (write(fdp->fd_fd, buf, nbytes));

	return (IOP_WRITE(io->io_next, buf, nbytes));
}

static off64_t
fdio_seek(mdb_io_t *io, off64_t offset, int whence)
{
	fd_data_t *fdp = io->io_data;

	if (io->io_next == NULL)
		return (lseek64(fdp->fd_fd, offset, whence));

	return (IOP_SEEK(io->io_next, offset, whence));
}

static int
fdio_ctl(mdb_io_t *io, int req, void *arg)
{
	fd_data_t *fdp = io->io_data;

	if (io->io_next == NULL)
		return (ioctl(fdp->fd_fd, req, arg));

	return (IOP_CTL(io->io_next, req, arg));
}

static void
fdio_close(mdb_io_t *io)
{
	fd_data_t *fdp = io->io_data;

	(void) close(fdp->fd_fd);
	mdb_free(fdp, sizeof (fd_data_t));
}

static const char *
fdio_name(mdb_io_t *io)
{
	fd_data_t *fdp = io->io_data;

	if (io->io_next == NULL)
		return (fdp->fd_name);

	return (IOP_NAME(io->io_next));
}

mdb_io_t *
mdb_fdio_create_path(const char *path[], const char *fname,
    int flags, mode_t mode)
{
	int fd;

	if (path != NULL && strchr(fname, '/') == NULL) {
		char buf[MAXPATHLEN];
		int i;

		for (fd = -1, i = 0; path[i] != NULL; i++) {
			(void) mdb_iob_snprintf(buf, MAXPATHLEN, "%s/%s",
			    path[i], fname);

			if (access(buf, F_OK) == 0) {
				fd = open64(buf, flags, mode);
				fname = buf;
				break;
			}
		}

		if (fd == -1)
			(void) set_errno(ENOENT);
	} else
		fd = open64(fname, flags, mode);

	if (fd >= 0)
		return (mdb_fdio_create_named(fd, fname));

	return (NULL);
}

static const mdb_io_ops_t fdio_ops = {
	fdio_read,
	fdio_write,
	fdio_seek,
	fdio_ctl,
	fdio_close,
	fdio_name,
	no_io_link,
	no_io_unlink,
	no_io_attrstr
};

mdb_io_t *
mdb_fdio_create(int fd)
{
	mdb_io_t *io = mdb_alloc(sizeof (mdb_io_t), UM_SLEEP);
	fd_data_t *fdp = mdb_alloc(sizeof (fd_data_t), UM_SLEEP);

	switch (fd) {
	case STDIN_FILENO:
		(void) strcpy(fdp->fd_name, "(stdin)");
		break;
	case STDOUT_FILENO:
		(void) strcpy(fdp->fd_name, "(stdout)");
		break;
	case STDERR_FILENO:
		(void) strcpy(fdp->fd_name, "(stderr)");
		break;
	default:
		(void) mdb_iob_snprintf(fdp->fd_name, MAXPATHLEN, "fd %d", fd);
	}

	fdp->fd_fd = fd;

	io->io_ops = &fdio_ops;
	io->io_data = fdp;
	io->io_next = NULL;
	io->io_refcnt = 0;

	return (io);
}

mdb_io_t *
mdb_fdio_create_named(int fd, const char *name)
{
	mdb_io_t *io = mdb_fdio_create(fd);
	fd_data_t *fdp = io->io_data;

	(void) strncpy(fdp->fd_name, name, MAXPATHLEN);
	fdp->fd_name[MAXPATHLEN - 1] = '\0';

	return (io);
}
