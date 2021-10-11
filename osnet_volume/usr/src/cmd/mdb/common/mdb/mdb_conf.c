/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_conf.c	1.2	99/11/19 SMI"

#include <sys/param.h>
#include <sys/systeminfo.h>
#include <sys/isa_defs.h>
#include <sys/utsname.h>
#include <strings.h>

static const char _mdb_version[] = "mdb 1.0";

const char *
mdb_conf_version(void)
{
	return (_mdb_version);
}

const char *
mdb_conf_platform(void)
{
	static char platbuf[MAXNAMELEN];

	if (sysinfo(SI_PLATFORM, platbuf, MAXNAMELEN) != -1)
		return (platbuf);

	return ("unknown");
}

const char *
mdb_conf_isa(void)
{
#ifdef	__sparc
#ifdef	__sparcv9
	return ("sparcv9");
#else	/* __sparcv9 */
	return ("sparc");
#endif	/* __sparcv9 */
#else	/* __sparc */
#ifdef	__i386
	return ("i386");
#else	/* __i386 */
#error	"unknown ISA"
#endif	/* __i386 */
#endif	/* __sparc */
}

void
mdb_conf_uname(struct utsname *utsp)
{
	bzero(utsp, sizeof (struct utsname));
	(void) uname(utsp);
}
