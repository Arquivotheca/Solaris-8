/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DD_MISC_H
#define	_DD_MISC_H

#pragma ident	"@(#)dd_misc.h	1.4	99/10/12 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DAEMON_FNAME	"in.dhcpd"

struct data_store {
	int code;	/* code passed to dd_* to access this store */
	char *name;	/* presentable string identifier ("Files", "NIS+") */
};

struct ip_interface {
	char name[IFNAMSIZ];
	struct in_addr addr;
	struct in_addr mask;
};

extern struct data_store **dd_data_stores(void);
extern void dd_free_data_stores(struct data_store **);
extern int dd_create_links(void);
extern int dd_check_links(void);
extern int dd_remove_links(void);
extern int dd_startup(boolean_t);
extern int dd_signal(char *, int);
extern pid_t dd_getpid(char *);
extern struct ip_interface **dd_get_interfaces(void);

#ifdef	__cplusplus
}
#endif

#endif	/* !_DD_MISC_H */
