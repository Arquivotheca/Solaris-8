#ident "@(#)stubs_sparc.c	1.1	93/07/14 SMI" /* from SunOS 4.1 */

/* Copyright (c) 1990 Sun Microsystems, Inc. */

#include <sys/bootconf.h>

extern struct bootops *bootops;

mountroot()
{
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

close()
{
}

reopen()
{
}

/* ARGSUSED3 */
lseek(fd, pos, whence)
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

exitto(go2)
	int (*go2)();
{
	_exitto(go2);
}
