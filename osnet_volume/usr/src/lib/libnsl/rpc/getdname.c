/*
 * Copyright (c) 1986-1991,1992-1993,1997,1999 by Sun Microsystems Inc.
 */

#ident	"@(#)getdname.c	1.15	99/07/19 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)getdname.c 1.5 89/04/18 Copyr 1989 Sun Micro";
#endif

/*
 * getdname.c
 *	Gets and sets the domain name of the system
 */

#include "rpc_mt.h"
#include <stdio.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/time.h>
#include <syslog.h>

#ifdef _NSL_RPC_ABI
/* For internal use only when building the libnsl RPC routines */
#define	sysinfo	_abi_sysinfo
#endif

#ifndef SI_SRPC_DOMAIN
#define	use_file
#endif

#ifdef use_file
char DOMAIN[] = "/etc/domain";
#endif

int setdomainname();
extern char *calloc();

#ifdef use_file
static char *domainname;
#endif

extern mutex_t	dname_lock;

int
getdomainname(name, namelen)
	char *name;
	int namelen;
{
#ifdef use_file
	FILE *domain_fd;
	char *line;

	trace2(TR_getdomainname, 0, namelen);

	mutex_lock(&dname_lock);
	if (domainname) {
		strncpy(name, domainname, namelen);
		mutex_unlock(&dname_lock);
		trace1(TR_getdomainname, 1);
		return (0);
	}

	domainname = (char *)calloc(1, 256);
	if (domainname == NULL) {
		syslog(LOG_ERR, "getdomainname : out of memory.");
		mutex_unlock(&dname_lock);
		trace1(TR_getdomainname, 1);
		return (-1);
	}

	if ((domain_fd = fopen(DOMAIN, "r")) == NULL) {

		mutex_unlock(&dname_lock);
		trace1(TR_getdomainname, 1);
		return (-1);
	}
	if (fscanf(domain_fd, "%s", domainname) == NULL) {
		fclose(domain_fd);
		mutex_unlock(&dname_lock);
		trace1(TR_getdomainname, 1);
		return (-1);
	}
	fclose(domain_fd);
	(void) strncpy(name, domainname, namelen);
	mutex_unlock(&dname_lock);
	trace1(TR_getdomainname, 1);
	return (0);
#else
	int sysinfostatus;

	trace2(TR_getdomainname, 0, namelen);
	sysinfostatus = sysinfo(SI_SRPC_DOMAIN, name, namelen);

	trace1(TR_getdomainname, 1);
	return ((sysinfostatus < 0) ? -1 : 0);
#endif
}

setdomainname(domain, len)
	char *domain;
	int len;
{
#ifdef use_file

	FILE *domain_fd;

	trace2(TR_setdomainname, 0, len);

	mutex_lock(&dname_lock);
	if (domainname)
		free(domainname);

	if ((domain_fd = fopen(DOMAIN, "w")) == NULL) {
		mutex_unlock(&dname_lock);
		trace1(TR_setdomainname, 1);
		return (-1);
	}
	if (fputs(domain, domain_fd) == NULL) {
		mutex_unlock(&dname_lock);
		trace1(TR_setdomainname, 1);
		return (-1);
	}
	fclose(domain_fd);
	domainname = (char *)calloc(1, 256);
	if (domainname == NULL) {
		syslog(LOG_ERR, "setdomainname : out of memory.");
		mutex_unlock(&dname_lock);
		trace1(TR_setdomainname, 1);
		return (-1);
	}
	(void) strncpy(domainname, domain, len);
	mutex_unlock(&dname_lock);
	trace1(TR_setdomainname, 1);
	return (0);
#else
	int sysinfostatus;

	trace2(TR_setdomainname, 0, len);
	sysinfostatus = sysinfo(SI_SET_SRPC_DOMAIN,
				domain, len + 1); /* add null */
	trace1(TR_setdomainname, 1);
	return ((sysinfostatus < 0) ? -1 : 0);
#endif
}
