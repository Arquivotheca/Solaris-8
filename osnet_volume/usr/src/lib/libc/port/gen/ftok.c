/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ftok.c	1.14	96/11/15 SMI"	/* SVr4.0 1.4.1.5	*/

/*LINTLIBRARY*/

#pragma weak ftok = _ftok
#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>

key_t
ftok(const char *path, char id)
{
	struct stat64 st;

	return (stat64(path, &st) < 0 ? (key_t)-1 :
	    (key_t)((key_t)id << 24 |
				((uint32_t)minor(st.st_dev)&0x0fff) <<
				12 | ((uint32_t)st.st_ino&0x0fff)));
}
