/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_conv.h	1.7	98/08/31 SMI"

#ifndef	_CONV_DOT_H
#define	_CONV_DOT_H

/*
 * Local include file for conversion library.
 */
#include	"conv.h"

/*
 * Various values that can't be matched to a symbolic definition will be
 * converted to a numeric string.  Each function that may require this
 * fallback maintains its own static string buffer, as many conversion
 * routines may be called for one final diagnostic.
 *
 * Most strings are printed as a 10.10s, but the string size is big
 * enough for any 32 bit value.
 */
#define	STRSIZE		12
#define	STRSIZE64	24

extern const char *	conv_invalid_str(char *, Lword, int);

#endif
