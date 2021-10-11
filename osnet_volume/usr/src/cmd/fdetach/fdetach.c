/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fdetach.c	1.2	92/07/14 SMI"
#include <stdio.h>
#include <errno.h>
/*
 * Unmount a STREAM from the command line.
 */

main(argc, argv)
	int argc;
	char **argv;
{
	if (argv[1] == NULL)
	{
		printf("usage: fdetach pathname\n");
		exit(-1);
	}
	if (fdetach(argv[1]) < 0)
	{
		perror("fdetach");
		exit(-1);
	}
	exit(0);
}
