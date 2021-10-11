/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)inet_ipaddr_string.cc	1.4 94/11/24 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include "inet_ipaddr_string.hh"

#define	IPADDR_TYPE "inet_ipaddr_string"

// Construct a reference (with no type) containing list of ip addresses
FN_ref*
inet_ipaddr_string(const struct hostent *hent)
{
	if (hent == 0 || hent->h_addrtype != AF_INET || hent->h_length <= 0)
		return (0);

	FN_ref* ip_ref = new FN_ref((unsigned char *) "");

	if (ip_ref == 0)
		return (0);
	// out of memory

	int howmany = hent->h_length;
	int i;
	FN_ref_addr *ia;
	for (i = 0; i < howmany; i++) {
		ia = new FN_ref_addr((const unsigned char *)IPADDR_TYPE,
		    strlen(hent->h_addr_list[i]),
		    hent->h_addr_list[i]);
		if (ia) {
			ip_ref->append_addr(*ia);
			delete ia;
		} else
			break;   // out of memory
	}
	return (ip_ref);
}
