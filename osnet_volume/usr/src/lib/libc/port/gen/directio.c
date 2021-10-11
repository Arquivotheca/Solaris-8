/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)directio.c 1.2	96/10/15 SMI"

/* LINTLIBRARY */

#include <sys/types.h>
#include <unistd.h>
#include <sys/filio.h>

/*
 * directio() allows an application to provide advise to the
 * filesystem to optimize read and write performance.
 */

int
directio(int fildes, int advice)
{
	return (ioctl(fildes, _FIODIRECTIO, advice));
}
