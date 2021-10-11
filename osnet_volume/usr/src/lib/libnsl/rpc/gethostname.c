/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)gethostname.c	1.10	94/10/19 SMI"

#include <sys/utsname.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <string.h>

#ifndef i386
extern int _uname();
#endif

/*
 * gethostname bsd compatibility
 */
gethostname(hname, hlen)
	char *hname;
	int hlen;
{
	struct utsname u;

	trace2(TR_gethostname, 0, hlen);
#ifdef i386
	if (_nuname(&u) < 0) {
#else
	if (_uname(&u) < 0) {
#endif /* i386 */
		trace1(TR_gethostname, 1);
		return (-1);
	}
	strncpy(hname, u.nodename, hlen);
	trace1(TR_gethostname, 1);
	return (0);
}
