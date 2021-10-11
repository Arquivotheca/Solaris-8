/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 *              PROPRIETARY NOTICE(Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *              Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *      (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *              All rights reserved.
 */

#pragma ident	"@(#)bzero.c 1.7	98/06/22 SMI"	/* SVr4.0 1.1   */

/*LINTLIBRARY*/

#include <string.h>

extern void *_memset(void *, int, size_t);

/*
 * Set an array of n chars starting at sp to zero.
 */

void
bzero(char *sp, int len)
{
	if (len > 0)
		(void) _memset(sp, 0, (size_t)len);
}
