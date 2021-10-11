/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getspent.c	1.17	96/11/27 SMI"
/*LINTLIBRARY*/

#pragma weak getspent	= _getspent
#pragma weak getspnam	= _getspnam
#pragma weak fgetspent	= _fgetspent
	/* putspent() has been moved to putspent.c */

#include "synonyms.h"
#include "shlib.h"
#include <sys/types.h>
#include <shadow.h>
#include <nss_dbdefs.h>
#include <stdio.h>

#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 */

/*
 * Don't free this, even on an endspent(), because bitter experience shows
 *   that there's production code that does getXXXbyYYY(), then endXXXent(),
 *   and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define	GETBUF()	\
	NSS_XbyY_ALLOC(&buffer, (int)sizeof (struct spwd), NSS_BUFLEN_SHADOW)
	/* === ?? set ENOMEM on failure?  */

struct spwd *
getspnam(const char *nam)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? NULL : getspnam_r(nam, b->result, b->buffer, b->buflen));
}

struct spwd *
getspent(void)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? NULL : getspent_r(b->result, b->buffer, b->buflen));
}

struct spwd *
fgetspent(FILE *f)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? NULL : fgetspent_r(f, b->result, b->buffer, b->buflen));
}

#endif	NSS_INCLUDE_UNSAFE
