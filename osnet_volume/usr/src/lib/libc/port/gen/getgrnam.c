/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* Copyright (c) 1991-1996, 1992 Sun Microsystems, Inc. 
 *	  All Rights Reserved  
 */	

#pragma ident	"@(#)getgrnam.c	1.19	96/11/14 SMI"        /* SVr4.0 1.16  */

/*	3.0 SID #	1.2	*/

/*LINTLIBRARY*/
#pragma weak getgrnam	= _getgrnam
#pragma weak getgrgid	= _getgrgid
#pragma weak getgrent	= _getgrent
#pragma weak fgetgrent	= _fgetgrent

#include "synonyms.h"
#include <sys/types.h>
#include <grp.h>
#include <nss_dbdefs.h>
#include <stdio.h>

#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 */

/*
 * Don't free this, even on an endgrent(), because bitter experience shows
 *   that there's production code that does getXXXbyYYY(), then endXXXent(),
 *   and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define GETBUF()	\
	NSS_XbyY_ALLOC(&buffer, sizeof (struct group), NSS_BUFLEN_GROUP)
	/* === ?? set ENOMEM on failure?  */

struct group *
getgrgid(gid_t gid)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? 0 : getgrgid_r(gid, b->result, b->buffer, b->buflen));
}

struct group *
getgrnam(const char *nam)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? 0 : getgrnam_r(nam ,b->result, b->buffer, b->buflen));
}

struct group *
getgrent(void)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? 0 : getgrent_r(b->result, b->buffer, b->buflen));
}

struct group *
fgetgrent(FILE *f)
{
	nss_XbyY_buf_t	*b = GETBUF();

	return (b == 0 ? 0 : fgetgrent_r(f, b->result, b->buffer, b->buflen));
}

#endif	NSS_INCLUDE_UNSAFE
