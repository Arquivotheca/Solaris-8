/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CACHEMGR_DOOR_H
#define	_CACHEMGR_DOOR_H

#pragma ident	"@(#)cachemgr_door.h	1.1	99/07/07 SMI"

/*
 * Definitions for server side of doors-based name service caching
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include "ns_cache_door.h"

typedef struct admin {
	ldap_stat_t	ldap_stat;
	int		debug_level;
	int		ret_stats;
	char		logfile[MAXPATHLEN];
} admin_t;


extern int __ns_ldap_trydoorcall(ldap_data_t **dptr, int *ndata, int *adata);

#ifdef __cplusplus
}
#endif

#endif /* _CACHEMGR_DOOR_H */
