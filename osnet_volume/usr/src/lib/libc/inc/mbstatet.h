/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#ifndef	_MBSTATET_H
#define	_MBSTATET_H

#pragma ident	"@(#)mbstatet.h	1.3	98/02/26 SMI"

typedef struct {
	void	*__lc_locale;	/* pointer to _LC_locale_t */
	void	*__state;		/* currently unused state flag */
	char	__consumed[8];	/* 8 bytes */
	char	__nconsumed;
	char	__fill[7];
} __mbstate_t;

#endif	/* _MBSTATET_H */
