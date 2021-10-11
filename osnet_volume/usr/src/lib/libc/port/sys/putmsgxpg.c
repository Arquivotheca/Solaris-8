/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)putmsgxpg.c	1.4	98/02/27 SMI"
/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
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
 *	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 *
 */
/*LINTLIBRARY*/
#include	"synonyms.h"
#include 	<sys/types.h>
#include	<stropts.h>

#pragma	weak	_libc_xpg4_putmsg= ___xpg4_putmsg
#pragma	weak	_libc_xpg4_putpmsg= ___xpg4_putpmsg

int
___xpg4_putmsg(int fildes, const struct strbuf *ctlptr,
		const struct strbuf *dataptr, int flags)
{
	return (putmsg(fildes, ctlptr, dataptr, flags|MSG_XPG4));
}

int
___xpg4_putpmsg(int fildes, const struct strbuf *ctlptr,
		const struct strbuf * dataptr, int band, int flags)
{
	return (putpmsg(fildes, ctlptr, dataptr, band, flags|MSG_XPG4));
}
