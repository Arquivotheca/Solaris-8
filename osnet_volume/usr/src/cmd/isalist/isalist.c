/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)isalist.c	1.4	96/10/07 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/systeminfo.h>

/*ARGSUSED*/
main(int argc, char *argv[])
{

	char buffer[BUFSIZ];
	char *buf = buffer;
	int ret = 0;
	size_t bufsize = BUFSIZ;

	ret = sysinfo(SI_ISALIST, buf, bufsize);
	if (ret == -1) {
			perror("isalist");
			exit(1);
	} else if (ret > bufsize) {

		/* We lost some because our buffer wasn't big enuf */
		buf = malloc(bufsize = ret);
		if (buf == NULL) {
			errno = ENOMEM;
			perror("isalist");
			exit(1);
		}
		ret = sysinfo(SI_ISALIST, buf, bufsize);
		if (ret == -1) {
			perror("isalist");
			exit(1);
		}
	}
	printf("%s\n", buf);
	return (0);


}
