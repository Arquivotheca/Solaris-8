/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)import_def.c	1.7	96/11/27 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>

void * (* _libc_malloc)() = &malloc;
void * (* _libc_realloc)() = &realloc;
void * (* _libc_calloc)() = &calloc;
void (* _libc_free)() = &free;
