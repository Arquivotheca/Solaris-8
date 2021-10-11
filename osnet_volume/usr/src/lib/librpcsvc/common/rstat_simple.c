#pragma ident	"@(#)rstat_simple.c	1.6	97/05/29 SMI" 

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <rpc/rpc.h>
#include <rpcsvc/rstat.h>

enum clnt_stat
rstat(host, statp)
	char *host;
	struct statstime *statp;
{
	return (rpc_call(host, RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
			xdr_void, (char *) NULL,
			xdr_statstime, (char *) statp, (char *) NULL));
}

havedisk(host)
	char *host;
{
	int32_t have;

	if (rpc_call(host, RSTATPROG, RSTATVERS_TIME, RSTATPROC_HAVEDISK,
			xdr_void, (char *) NULL,
			xdr_int, (char *) &have, (char *) NULL) != 0)
		return (-1);
	else
		return (have);
}


