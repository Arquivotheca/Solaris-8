/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_HASH_HH
#define	_XFN_HASH_HH

#pragma ident	"@(#)hash.hh	1.1	94/08/04 SMI"

#include <stddef.h>

extern unsigned long get_hashval(const void *p, size_t len);
extern unsigned long get_hashval_nocase(const char *p, size_t len);

#endif // _XFN_HASH_HH
