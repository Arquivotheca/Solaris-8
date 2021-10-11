/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bfuld.c	1.1	99/01/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/mman.h>

#define	ISTRLEN	16

char istr[ISTRLEN + 1] = "/usr/lib/ld.so.1";
char bstr[ISTRLEN + 1] = "/tmp/bfulib/bf.1";

int
main(int argc, char **argv)
{
	int i, f, fd;
	size_t size;
	char *map;

	for (f = 1; f < argc; f++) {
		fd = open(argv[f], O_RDWR);
		size = lseek(fd, 0, SEEK_END);
		map = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		for (i = 0; i < size - ISTRLEN - 1; i++)
			if (bcmp(&map[i], istr, ISTRLEN) == 0)
				bcopy(bstr, &map[i], ISTRLEN);
		msync(map, size, MS_SYNC);
		munmap(map, size);
		close(fd);
	}
	return (0);
}
