/*
 * Copyright (c) 1985-1994, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.12	95/07/15 SMI"

#include <sys/types.h>
#include <sys/elf.h>
#include <sys/param.h>
#include <sys/platnames.h>
#include <sys/boot_redirect.h>

#include "cbootblk.h"

static int
bbopen(char *pathname, void *arg)
{
#ifdef DEBUG
	puts("** Try: "); puts(pathname); puts("\n");
#endif
	return (openfile(arg, pathname));
}

void
main(void *ptr)
{
	unsigned long load;
	char *dev;
	int fd, count, once = 0;
	char letter[2];
	static char fullpath[MAXPATHLEN];

	fw_init(ptr);
	dev = getbootdevice(0);
retry:
	fd = open_platform_file(fscompname, bbopen, dev, fullpath, 0);
	if (fd != -1) {
#ifdef DEBUG
		puts("** ELF load ");
		puts(dev); puts(" "); puts(fullpath); puts("\n");
#endif
		load = read_elf_file(fd, fullpath);
		(void) closefile(fd);
		exitto((void *)load, ptr);
		/*NOTREACHED*/
	}

	/*
	 * PSARC/1994/396: Try for a slice redirection file.
	 */
	if (once == 0 &&
	    (fd = openfile(dev, BOOT_REDIRECT)) != -1) {
		once = 1;
		seekfile(fd, (off_t)0);
		count = readfile(fd, letter, 1);
		(void) closefile(fd);
		if (count == 1) {
			letter[1] = '\0';
			dev = getbootdevice(letter);
#ifdef DEBUG
			puts("** Redirection device ");
			puts(dev); puts("\n");
#endif
			goto retry;
			/*NOTREACHED*/
		}
	}

#ifdef notdef
	/*
	 * Finally, try for the old program.
	 * XXX Delete this before beta!
	 */
	(void) strcpy(fullpath, fscompname);
	if ((fd = openfile(dev, fullpath)) != -1) {
		puts("Warning: loading boot program from /");
		puts(" (should come from /platform)\n");
		load = read_elf_file(fd, fullpath);
		(void) closefile(fd);
		exitto((void *)load, ptr);
		/*NOTREACHED*/
	}
#endif
	puts("bootblk: can't find the boot program\n");
}
