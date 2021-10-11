/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_vername.c	1.2	95/09/01 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Return a character string in buf,buflen representing the running
 * version of the firmware. Systems that have no concept of such a
 * string may return the string "unknown".
 *
 * Return the actual length of the string, including the NULL terminator.
 * Copy at most buflen bytes into the caller's buffer, always providing
 * NULL termination.
 *
 * Returns the actual length of the string, plus copies data in the callers
 * buf copying at most buflen bytes.  Returns -1 if an internal error occurs.
 */

int
prom_version_name(char *buf, int buflen)
{
	char temp[24];
	unsigned int mon_id = (unsigned int)prom_mon_id();

	*(buf + buflen - 1) = (char)0;	/* Force NULL termination */

	/*
	 * On OBP systems, the obp.op_mon_id field in the romvec contains
	 * contains the running version of the prom. The field contains
	 * two short integers, representing a major and minor version number.
	 */

	(void) prom_sprintf(temp, "OpenBoot %d.%d", mon_id >> 16,
	    mon_id & 0xffff);
	(void) prom_strncpy(buf, temp, buflen - 1);
	return (prom_strlen(temp) + 1);
}
