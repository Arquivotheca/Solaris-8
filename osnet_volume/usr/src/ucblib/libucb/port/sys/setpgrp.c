/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setpgrp.c	1.2	97/06/16 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <unistd.h>

int
setpgrp(pid_t pid1, pid_t pid2)
{
	return (setpgid(pid1, pid2));
}
