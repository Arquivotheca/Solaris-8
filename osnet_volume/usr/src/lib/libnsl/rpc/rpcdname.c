
#ident	"@(#)rpcdname.c	1.16	99/07/19 SMI"

/*
 * Copyright (c) 1986-1991,1992-1994,1997,1999 by Sun Microsystems Inc.
 */

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)rpcdname.c 1.2 89/03/10 Copyr 1989 Sun Micro";
#endif

/*
 * rpcdname.c
 * Gets the default domain name
 */
#include "rpc_mt.h"
#include <sys/types.h>
#include <sys/time.h>
#include <rpc/trace.h>
#include <string.h>
#include <syslog.h>

extern int getdomainname();
extern char *strdup();
static char *default_domain = 0;

static char *
get_default_domain()
{
	char temp[256];
	extern mutex_t dname_lock;

/* VARIABLES PROTECTED BY dname_lock: default_domain */

	trace1(TR_get_default_domain, 0);
	mutex_lock(&dname_lock);
	if (default_domain) {
		mutex_unlock(&dname_lock);
		trace1(TR_get_default_domain, 1);
		return (default_domain);
	}
	if (getdomainname(temp, (size_t) sizeof (temp)) < 0) {
		mutex_unlock(&dname_lock);
		trace1(TR_get_default_domain, 1);
		return (0);
	}
	if ((int) strlen(temp) > 0) {
		default_domain = strdup(temp);
		if (default_domain == NULL) {
			syslog(LOG_ERR, "get_default_domain : strdup failed.");
			mutex_unlock(&dname_lock);
			trace1(TR_get_default_domain, 1);
			return (0);
		}
	}
	mutex_unlock(&dname_lock);
	trace1(TR_get_default_domain, 1);
	return (default_domain);
}

/*
 * This is a wrapper for the system call getdomainname which returns a
 * ypclnt.h error code in the failure case.  It also checks to see that
 * the domain name is non-null, knowing that the null string is going to
 * get rejected elsewhere in the yp client package.
 */
int
__rpc_get_default_domain(domain)
	char **domain;
{
	trace1(TR___rpc_get_default_domain, 0);
	if ((*domain = get_default_domain()) != 0) {
		trace1(TR___rpc_get_default_domain, 1);
		return (0);
	}
	trace1(TR___rpc_get_default_domain, 1);
	return (-1);
}
