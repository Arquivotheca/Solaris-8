/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)libc_open.c 1.7     98/05/13 SMI"

#pragma	weak _open = _libc_open

#include "synonyms.h"

#include <sys/mkdev.h>
#include <limits.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/ptms.h>

#if !defined(_LP64)
#pragma	weak _open64 = _libc_open64
extern int __open64(char *fname, int fmode, int cmode);
#endif

extern	int __xpg4; /* defined in port/gen/xpg4.c; 0 if not xpg4/xpg4v2 */

extern int __open(char *fname, int fmode, int cmode);

static void push_module(int fd);
static int isptsfd(int fd);
static void itoa(int i, char *ptr);

int
_libc_open(char *fname, int fmode, int cmode)
{
	int 	fd;

	/*
	 * XPG4v2 requires that open of a slave pseudo terminal device
	 * provides the process with an interface that is identical to
	 * the terminal interface. For a more detailed discussion,
	 * see bugid 4025044.
	 */

	fd = __open(fname, fmode, cmode);
	if (__xpg4 == 1) {
		if ((fd != -1) && isptsfd(fd))
			push_module(fd);
	}
	return (fd);
}

#if !defined(_LP64)
/*
 * The 32-bit APIs to large files require this interposition.
 * The 64-bit APIs just fall back to _libc_open() above.
 */
int
_libc_open64(char *fname, int fmode, int cmode)
{
	int 	fd;

	/*
	 * XPG4v2 requires that open of a slave pseudo terminal device
	 * provides the process with an interface that is identical to
	 * the terminal interface. For a more detailed discussion,
	 * see bugid 4025044.
	 */

	fd = __open64(fname, fmode, cmode);
	if (__xpg4 == 1) {
		if ((fd != -1) && isptsfd(fd))
			push_module(fd);
	}
	return (fd);
}
#endif	/* !_LP64 */

/*
 * Checks if the file matches an entry in the /dev/pts directory
 */
static int
isptsfd(int fd)
{
	static char buf[TTYNAME_MAX];
	struct stat64 fsb, stb;

	if (fstat64(fd, &fsb) != 0 ||
	    (fsb.st_mode & S_IFMT) != S_IFCHR)
		return (0);

	strcpy(buf, "/dev/pts/");
	itoa(minor(fsb.st_rdev), buf+strlen(buf));

	if (stat64(buf, &stb) != 0 ||
	    (fsb.st_mode & S_IFMT) != S_IFCHR)
		return (0);

	return (stb.st_rdev == fsb.st_rdev);
}

/*
 * Converts a number to a string (null terminated).
 */
static void
itoa(int i, char *ptr)
{
	int dig = 0;
	int tempi;

	tempi = i;
	do {
		dig++;
		tempi /= 10;
	} while (tempi);

	ptr += dig;
	*ptr = '\0';
	while (--dig >= 0) {
		*(--ptr) = i % 10 + '0';
		i /= 10;
	}
}

/*
 * Push modules to provide tty semantics
 */
static void
push_module(int fd)
{
	struct strioctl istr;

	istr.ic_cmd = PTSSTTY;
	istr.ic_len = 0;
	istr.ic_timout = 0;
	istr.ic_dp = NULL;
	if (ioctl(fd, I_STR, &istr) != -1) {
		(void) ioctl(fd, __I_PUSH_NOCTTY, "ptem");
		(void) ioctl(fd, __I_PUSH_NOCTTY, "ldterm");
		(void) ioctl(fd, __I_PUSH_NOCTTY, "ttcompat");
		istr.ic_cmd = PTSSTTY;
		istr.ic_len = 0;
		istr.ic_timout = 0;
		istr.ic_dp = NULL;
		(void) ioctl(fd, I_STR, &istr);
	}
}
