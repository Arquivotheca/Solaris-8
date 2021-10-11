/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCP_HOSTCONF_H
#define	_DHCP_HOSTCONF_H

#pragma ident	"@(#)dhcp_hostconf.h	1.4	99/07/26 SMI"

#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>

/*
 * dhcp_hostconf.[ch] provide an API to the /etc/dhcp/<if>.dhc files.
 * see dhcp_hostconf.c for documentation on how to use the exported
 * functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	DHCP_HOSTCONF_MAGIC	0x44484301		/* hex "DHC1" */
#define	DHCP_HOSTCONF_PREFIX	"/etc/dhcp/"
#define	DHCP_HOSTCONF_SUFFIX	".dhc"
#define	DHCP_HOSTCONF_TMPL	DHCP_HOSTCONF_PREFIX DHCP_HOSTCONF_SUFFIX

extern char	*ifname_to_hostconf(const char *);
extern int	remove_hostconf(const char *);
extern int	read_hostconf(const char *, PKT_LIST **);
extern int	write_hostconf(const char *, PKT_LIST *, time_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_HOSTCONF_H */
