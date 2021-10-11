/*
 * Copyright (c) 1994,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)values-xpg4.c	1.2	96/11/26 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
/*
 * Setting thie value to 1 enables XPG4 mode for APIs
 * which have differing runtime behaviour from XPG3 to XPG4.
 * See usr/src/lib/libc/port/gen/xpg4.c for the default value.
 */

int __xpg4 = 1;
