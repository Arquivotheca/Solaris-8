/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)util.c	1.1	99/08/13 SMI"

/* LINTLIBRARY */

#include	<stdio.h>
#include	"rtc.h"
#include	"msg.h"

/*
 * Messaging support - funnel everything through _dgettext() as this provides
 * a stub binding to libc, or a real binding to libintl.
 */
extern char *	_dgettext(const char *, const char *);

const char *
_libcrle_msg(Msg mid)
{
	return (_dgettext(MSG_ORIG(MSG_SUNW_OST_SGS), MSG_ORIG(mid)));
}
