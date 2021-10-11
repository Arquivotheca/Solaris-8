/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	CACHEMGR_H
#define	CACHEMGR_H

#pragma ident	"@(#)cachemgr.h	1.1	99/07/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "ns_sldap.h"
#include "ns_internal.h"
#include "ns_cache_door.h"
#include "cachemgr_door.h"

#define	LOGFILE		"/var/ldap/cachemgr.log"
#define	KILLCACHEMGR	"/var/lib/ldap/ldap_cachemgr -K"
#define	MAXBITSIZE	30
#define	MAXDEBUG	10
#define	DEFAULTTTL	3600		/* 1 hour */

typedef	union {
	ldap_data_t	data;
	char		space[BUFFERSIZE];
} dataunion;


extern char * getcacheopt(char * s);
extern void logit(char * format, ...);
extern void do_update(ldap_call_t * in);
extern int load_admin_defaults(admin_t * ptr, int will_become_server);
extern int getldap_init(void);
extern void getldap_revalidate(void);
extern int getldap_uidkeepalive(int keep, int interval);
extern int getldap_invalidate(void);
extern void getldap_lookup(ldap_return_t *out, ldap_call_t * in);
extern void getldap_refresh(void);
extern int cachemgr_set_dl(admin_t * ptr, int value);
extern int cachemgr_set_ttl(ldap_stat_t * cache, char * name, int value);
extern int get_clearance(int callnumber);
extern int release_clearance(int callnumber);
#ifdef SLP
extern void discover();
#endif SLP

#ifdef __cplusplus
}
#endif

#endif /* _CACHEMGR_H */
