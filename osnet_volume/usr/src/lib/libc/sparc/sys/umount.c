/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 * This file is created to make the trap value
 * obsolete for umount() system call. This is an
 * effort to support forcible unmount interface
 * through umount2() system call.
 */

#ident	"@(#)umount.c	1.2	99/07/21 SMI"

#ifdef __STDC__
#pragma weak umount = _umount
#endif

/*LINTLIBRARY*/
#include "synonyms.h"
#include "libc.h"

int
umount(const char *path)
{
	return (umount2(path, 0));
}
