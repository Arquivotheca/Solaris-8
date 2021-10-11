/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fdetach.c	1.9	96/11/15 SMI"	/* SVr4.0 1.2	*/

/*LINTLIBRARY*/

/*
 * Detach a STREAMS-based file descriptor from an object in the
 * file system name space.
 */


#pragma weak fdetach = _fdetach
#include "synonyms.h"
#include <sys/mount.h>

int
fdetach(const char *path)
{

	return (umount(path));
}
