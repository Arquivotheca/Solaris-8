/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)stubs_i386.c	1.10	99/08/19 SMI"

#include <sys/bootconf.h>

extern struct bootops *bootops;

mountroot()
{
	return (0);
}

open(name, mode)
	char *name;
	int mode;
{
	return (BOP_OPEN(bootops, name, mode));
}

read(fd, buf, count)
	int fd;
	char *buf;
	int count;
{
	return (BOP_READ(bootops, fd, buf, count));
}

/* ARGSUSED */
close(int fd)
{
	return (0);
}

reopen()
{
	return (0);
}

/* ARGSUSED */
lseek(int fd, int pos, int whence)
{
	/*
	 * Current version of standalone lseek() uses high and low offsets
	 * for the second and third parameters, apparently anticipating
	 * long long offsets.  That's completely bogus for kadb, since
	 * kadb gets its seek offsets from ELF header fields that are
	 * limited by the ABI to 32 bits.  Rather than change lseek()s
	 * all over kadb, the swindle is done here.
	 */
	return (BOP_SEEK(bootops, fd, 0, pos));
}

void
exitto(go2)
	int (*go2)();
{
	extern void _exitto();
	_exitto(go2);
}
