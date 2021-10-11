/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)msgid.c	1.1	98/09/30 SMI"

#include <stdio.h>
#include <sys/strlog.h>

char msg[BUFSIZ];

int
main(void)
{
	uint32_t msgid;

	while (fgets(msg, BUFSIZ, stdin) != NULL) {
		STRLOG_MAKE_MSGID(msg, msgid);
		printf("%u %s", msgid, msg);
	}
	return (0);
}
