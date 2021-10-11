/*
 * Copyright (c) 1994,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)xpg4.c	1.2	96/11/26 SMI"
/*LINTLIBRARY*/

/*
 * __xpg4 == 0 by default. The xpg4 cc driver will add an object
 * file that contains int __xpg4 = 1". The symbol interposition
 * provided by the linker will allow libc to find that symbol
 * instead.
 */

int __xpg4 = 0;
