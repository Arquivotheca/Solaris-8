/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)hash.cc	1.1 94/08/04 SMI"

#include "hash.hh"

#define	HASHSHIFT	(3)
#define	HASHMASK	(0x1f)


unsigned long
get_hashval(const void *p, size_t cp_len)
{
	const unsigned char	*cp;
	unsigned long		hv;

	cp = (const unsigned char *)p;
	hv = 0;
	while (cp_len > 0) {
		hv = (hv << HASHSHIFT) ^ hv;
		hv += *cp++;
		--cp_len;
	}
	// make results predictable regardless of sizeof (u_long)
	return (hv & 0xffffffff);
}

/*
 * same as above, but this one is case insensitive
 */

unsigned long
get_hashval_nocase(const char *p, size_t cp_len)
{
	const unsigned char	*cp;
	unsigned long		hv;

	cp = (const unsigned char *)p;
	hv = 0;
	while (cp_len > 0) {
		hv = (hv << HASHSHIFT) ^ hv;
		hv += *cp++ & HASHMASK;
		--cp_len;
	}
	// make results predictable regardless of sizeof (u_long)
	return (hv & 0xffffffff);
}
