/* LINTLIBRARY */
/* PROTOLIB1 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc. 
 * All rights reserved. 
 */ 

#pragma ident	"@(#)lintsup.c	1.1	98/08/28 SMI"

/*
 * Supplimental definitions for lint that help us avoid
 * options like `-x' that filter out things we want to
 * know about as well as things we don't.
 */

/*
 * The public interfaces are allowed to be "declared
 * but not used".
 */
#include <malloc.h>
#include <link.h>


/*
 * These are from libc.so.1, but are not in it's lint
 * library.
 */
int	__threaded;
char *	_dgettext(const char *, const char *);
