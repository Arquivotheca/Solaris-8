/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SERVER_DOOR_H
#define	_SERVER_DOOR_H

#pragma ident	"@(#)server_door.h	1.3	99/04/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for server side of doors-based name service caching
 */


typedef struct admin {
	nsc_stat_t	passwd;
	nsc_stat_t	group;
	nsc_stat_t	host;
	nsc_stat_t	node;
	nsc_stat_t	exec;
	nsc_stat_t	prof;
	nsc_stat_t	user;
	int		debug_level;
	int		avoid_nameservice;
		/* set to true for disconnected op */
	int		ret_stats;	/* return status of admin calls */
	char		logfile[128];	/* debug file for logging */
} admin_t;


extern struct group *_uncached_getgrgid_r(gid_t, struct group *, char *, int);

extern struct group *_uncached_getgrnam_r(const char *, struct group *,
    char *, int);

extern struct passwd *_uncached_getpwuid_r(uid_t, struct passwd *, char *, int);

extern struct passwd *_uncached_getpwnam_r(const char *, struct passwd *,
    char *, int);

extern struct hostent  *_uncached_gethostbyname_r(const char *,
    struct hostent *, char *, int, int *h_errnop);

extern struct hostent  *_uncached_gethostbyaddr_r(const char *, int, int,
    struct hostent *, char *, int, int *h_errnop);

extern struct hostent  *_uncached_getipnodebyname(const char *,
    struct hostent *, char *, int, int *h_errnop);

extern struct hostent  *_uncached_getipnodebyaddr(const char *, int, int,
    struct hostent *, char *, int, int *h_errnop);

extern int _nsc_trydoorcall(nsc_data_t **dptr, int *ndata, int *adata);

#ifdef	__cplusplus
}
#endif

#endif	/* _SERVER_DOOR_H */
