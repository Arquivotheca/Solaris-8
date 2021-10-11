/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAP_COMMON_H
#define	_LDAP_COMMON_H

#pragma ident	"@(#)ldap_common.h	1.2	99/08/18 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <nss_dbdefs.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <lber.h>
#include <ldap.h>
#include <pwd.h>
#include "ns_sldap.h"

#define	_ALIASES		"aliases"
#define	_AUTOMOUNT		"automount"
#define	_AUTHATTR		"auth_attr"
#define	_AUUSER			"audit_user"
#define	_BOOTPARAMS		"bootparams"
#define	_DEFAULT		"default"
#define	_ETHERS			"ethers"
#define	_EXECATTR		"exec_attr"
#define	_GROUP			"group"
#define	_HOSTS			"hosts"
#define	_HOSTS6			"hosts"
#define	_NETGROUP		"netgroup"
#define	_NETMASKS		"netmasks"
#define	_NETWORKS		"networks"
#define	_PASSWD			"passwd"
#define	_PROFATTR		"prof_attr"
#define	_PROTOCOLS		"protocols"
#define	_PUBLICKEY		"publickey"
#define	_RPC			"rpc"
#define	_SENDMAILVARS		"sendmailvars"
#define	_SERVICES		"services"
#define	_SHADOW			"shadow"
#define	_USERATTR		"user_attr"

#define	DOTTEDSUBDOMAIN(string) \
	((string != NULL) && (strchr(string, '.') != NULL))
#define	SEARCHFILTERLEN		256
#define	NETGROUPFILTERLEN	4096


struct netgroupnam {
	struct netgroupnam	*ng_next;
	char			ng_name[1];
};

struct netgrouptab {
	struct netgroupnam	*nt_first;
	struct netgroupnam	**nt_last;
	int			nt_total;
};

/*
 * Superset the nss_backend_t abstract data type. This ADT has
 * been extended to include ldap associated data structures.
 */

typedef struct ldap_backend *ldap_backend_ptr;
typedef nss_status_t (*ldap_backend_op_t)(ldap_backend_ptr, void *);
typedef int (*fnf)(ldap_backend_ptr be, nss_XbyY_args_t *argp);

struct ldap_backend {
	ldap_backend_op_t	*ops;
	nss_dbop_t		nops;
	char			*tablename;
	void			*enumcookie;
	char			*filter;
	int			setcalled;
	const char		**attrs;
	ns_ldap_result_t	*result;
	fnf			ldapobj2ent;
	char			*netgroup;
	char			*toglue;
	struct netgrouptab	all_members;
	struct netgroupnam	*next_member;
};

extern nss_status_t	_nss_ldap_destr(ldap_backend_ptr be, void *a);
extern nss_status_t	_nss_ldap_endent(ldap_backend_ptr be, void *a);
extern nss_status_t	_nss_ldap_setent(ldap_backend_ptr be, void *a);
extern nss_status_t	_nss_ldap_getent(ldap_backend_ptr be, void *a);
nss_backend_t		*_nss_ldap_constr(ldap_backend_op_t ops[], int nops,
			char *tablename, const char **attrs, fnf ldapobj2ent);
extern nss_status_t	_nss_ldap_nocb_lookup(ldap_backend_ptr be,
			nss_XbyY_args_t *argp, char *database,
			char *searchfilter, char *domain);
extern nss_status_t	_nss_ldap_lookup(ldap_backend_ptr be,
			nss_XbyY_args_t *argp, char *database,
			char *searchfilter, char *domain);
extern void		_clean_ldap_backend(ldap_backend_ptr be);

extern ns_ldap_attr_t *getattr(ns_ldap_result_t *result, int i);
extern char *_strip_dn_cononical_name(char *name);
extern const char *_strip_quotes(char *ipaddress);
extern int __nss2herrno(nss_status_t nsstat);
extern int propersubdomain(char *domain, char *subdomain);
extern int chophostdomain(char *string, char *host, char *domain);

#ifdef DEBUG
extern int printresult(ns_ldap_result_t * result);
#endif DEBUG

#ifdef	__cplusplus
}
#endif

#endif	/* _LDAP_COMMON_H */
