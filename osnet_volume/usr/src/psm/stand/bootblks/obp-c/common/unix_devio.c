/*
 * Copyright (c) 1991-1994, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_devio.c	1.4	94/12/10 SMI"

/*
 * Emulate the firmware on Unix
 * Only useful for testing the boot block code.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>

int
devbread(void *handle, void *buf, int blkno, int size)
{
	int fd = (int)handle;

	if (lseek(fd, (off_t) blkno * DEV_BSIZE, SEEK_SET) == -1)
		perror("lseek"), exit(1);
	return (read(fd, buf, size));
}

void *
devopen(char *devname)
{
	int fd;

	if ((fd = open(devname, O_RDONLY)) == -1) {
		perror(devname);
		return (NULL);
	}
	return ((void *)fd);
}

int
devclose(void *handle)
{
	int fd = (int)handle;

	return (close(fd));
}

#define	SZBUF	511	/* or 513? */

static void
usage(void)
{
	char mess[] = "Usage:\ta.out raw-device pathname\n";

	write(2, mess, strlen(mess) + 1);
	exit(1);
}

static void
openfail(void)
{
	char mess[] = "openfile failed\n";

	write(2, mess, strlen(mess) + 1);
	exit(2);
}

extern int openfile(char *, char *);
extern int readfile(int, char *, size_t);
extern int closefile(int);

int
main(int argc, char *argv[])
{
	char buf[SZBUF];
	int fd, count;

	if (argc != 3)
		usage();
	if ((fd = openfile(argv[1], argv[2])) == -1)
		openfail();
	while ((count = readfile(fd, buf, SZBUF)) != 0)
		write(1, buf, count);
	(void) closefile(fd);
	return (0);
}

/*
 * Sigh.  These shouldn't be needed.
 */
void
bcopy(void *from, void *to, size_t howmany)
{
	(void) memcpy(to, from, howmany);
}

void
bzero(void *addr, size_t howmany)
{
	(void) memset(addr, 0, howmany);
}
