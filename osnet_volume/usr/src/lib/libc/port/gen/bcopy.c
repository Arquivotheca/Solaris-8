/*
 * Copyright (c) 1995-1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)bcopy.c	1.6	98/12/18 SMI"	/* SVr4.0 1.1	*/

/*
 *
 *		PROPRIETARY NOTICE(Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

/*LINTLIBRARY*/
#include "synonyms.h"

#include <sys/types.h>
#include <string.h>
#include <strings.h>

/*
 * Copy s1 to s2, always copy n bytes.
 * For overlapped copies it does the right thing.
 */
void
bcopy(const void *s1, void *s2, size_t len)
{
	(void) memmove(s2, s1, len);
}
