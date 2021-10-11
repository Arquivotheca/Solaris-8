/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ident	"@(#)_libc_gettext.h	1.5	97/05/29 SMI"

/* Header file for _libc_gettext() macro. */
#if !defined(TEXT_DOMAIN)	/* Should be defined thru -D flag. */
#	define	TEXT_DOMAIN	"SYS_TEST"
#endif

char * _dgettext(const char *, const char *);
#define _libc_gettext(msg_id)	_dgettext(TEXT_DOMAIN, msg_id)

