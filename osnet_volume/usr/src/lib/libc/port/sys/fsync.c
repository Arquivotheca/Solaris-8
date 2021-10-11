/*	Copyright (c) 1988 AT&T	*/
/*	All Rights Reserved	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
*/

#pragma ident	"@(#)fsync.c	1.6	96/12/03	SMI"
/*
 * fsync(int fd)
 *
 */
/*LINTLIBRARY*/
#include "synonyms.h"
#include <sys/types.h>
#include "libc.h"
#include "sys/file.h"

#pragma weak	_libc_fsync = _fsync

int
fsync(int fd)
{

	return (__fdsync(fd, FSYNC));
}
