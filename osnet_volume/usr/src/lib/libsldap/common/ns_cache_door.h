/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_NS_CACHE_DOOR_H
#define	_NS_CACHE_DOOR_H

#pragma ident	"@(#)ns_cache_door.h	1.1	99/07/07 SMI"

/*
 * Definitions for client side of doors-based ldap caching
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <grp.h>
#include <pwd.h>


/*
 *	statistics & control structure
 */

typedef struct ldap_stat {
	int	ldap_numbercalls;	/* number of times called */
	int	ldap_ttl;		/* time to live for positive entries */
} ldap_stat_t;


/*
 * structure returned by server for all calls
 */

#define	BUFFERSIZE	8192
#define	OFFSET		36
typedef struct {
	int 		ldap_bufferbytesused;
	int 		ldap_return_code;
	int 		ldap_errno;

	union {
		char		config[BUFFERSIZE - OFFSET];
		ldap_stat_t 	stats;
		char 		buff[4];
	} ldap_u;

} ldap_return_t;

/*
 * calls look like this
 */

typedef struct {
	int ldap_callnumber;
	union {
		uid_t uid;
		gid_t gid;
		char domainname[sizeof (int)]; 	/* size is indeterminate */
		struct {
			int  a_type;
			int  a_length;
			char a_data[sizeof (int)];
		} addr;
	} ldap_u;
} ldap_call_t;
/*
 * how the client views the call process
 */

typedef union {
	ldap_call_t 		ldap_call;
	ldap_return_t 		ldap_ret;
	char 			ldap_buff[sizeof (int)];
} ldap_data_t;

#define	NULLCALL	0
#define	GETLDAPCONFIG	1

/*
 * administrative calls
 */

#define	KILLSERVER	7
#define	GETADMIN	8
#define	SETADMIN	9

/*
 * debug levels
 */

#define	DBG_OFF		0
#define	DBG_CANT_FIND	2
#define	DBG_NETLOOKUPS	4
#define	DBG_ALL		6

/*
 * Max size name we allow to be passed to avoid
 * buffer overflow problems
 */
#define	LDAPMAXNAMELEN	255

/*
 * defines for client-server interaction
 */

#define	LDAP_CACHE_DOOR_VERSION 1
#define	LDAP_CACHE_DOOR "/etc/.ldap_cache_door"
#define	LDAP_CACHE_DOOR_COOKIE ((void*)(0xdeadbeef^LDAP_CACHE_DOOR_VERSION))
#define	UPDATE_DOOR_COOKIE ((void*)(0xdeadcafe)

#define	SUCCESS		0
#define	NOTFOUND  	-1
#define	CREDERROR 	-2
#define	SERVERERROR 	-3
#define	NOSERVER 	-4

int
__ns_ldap_trydoorcall(ldap_data_t **dptr, int *ndata, int *adata);

#ifdef	__cplusplus
}
#endif


#endif	/* _NS_CACHE_DOOR_H */
