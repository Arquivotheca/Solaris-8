/*
 * Copyright (c) 1990, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CURS_GETTEXT_H
#define	_CURS_GETTEXT_H

#pragma ident		"@(#)_curs_gettext.h 1.4	97/08/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Header file for _curs_gettext() macro. */
#if !defined(TEXT_DOMAIN)	/* Should be defined thru -D flag. */
#	define	TEXT_DOMAIN	"SYS_TEST"
#endif

char *_dgettext(const char *, const char *);
#define	_curs_gettext(msg_id)	_dgettext(TEXT_DOMAIN, msg_id)

#ifdef	__cplusplus
}
#endif

#endif	/* _CURS_GETTEXT_H */
