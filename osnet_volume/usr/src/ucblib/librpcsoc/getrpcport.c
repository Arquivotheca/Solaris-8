#ident	"@(#)getrpcport.c	1.6	98/08/10 SMI"

/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>

extern u_short pmap_getport(struct sockaddr_in *, rpcprog_t,
			rpcvers_t, rpcprot_t);

u_short
getrpcport(char *host, rpcprog_t prognum, rpcvers_t versnum, rpcprot_t proto)
{
	struct sockaddr_in addr;
	struct hostent *hp;

	if ((hp = gethostbyname(host)) == NULL)
		return (0);
	memcpy((char *) &addr.sin_addr, hp->h_addr, hp->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port =  0;
	return (pmap_getport(&addr, prognum, versnum, proto));
}
