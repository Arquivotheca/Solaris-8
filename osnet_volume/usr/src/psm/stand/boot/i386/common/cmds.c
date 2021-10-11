/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)cmds.c	1.4	96/03/23 SMI"

/* boot shell miscellaneous built-in commands */

#include <sys/bsh.h>
#include <sys/types.h>
#include <sys/salib.h>


/* echo_cmd() - implements echo command - displays args */

void
echo_cmd(argc, argv)
	int	argc;
	char	*argv[];
{
	int newline_flag;

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n') {
		newline_flag = 0;
		--argc;
		++argv;
	} else
		newline_flag = 1;

	while (--argc > 0) {
		printf("%s%s", *++argv, (argc > 1) ? " " : "");
	}
	if (newline_flag)
		printf("\n");
}
