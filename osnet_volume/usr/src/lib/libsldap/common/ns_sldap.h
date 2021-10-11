/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef	_NS_SLDAP_H
#define	_NS_SLDAP_H

#pragma ident	"@(#)ns_sldap.h	1.2	99/11/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <lber.h>
#include <ldap.h>

/*
 * Version
 */
#define	NS_LDAP_VERSION		"1.0"

/*
 * Flags
 */
#define	NS_LDAP_HARD		0x001
#define	NS_LDAP_ALL_RES		0x002

/* Search Referral Option */
typedef enum SearchRef {
	NS_LDAP_FOLLOWREF	= 0x004,
	NS_LDAP_NOREF		= 0x008
} SearchRef_t;

typedef enum ScopeType {
	NS_LDAP_SCOPE_BASE	= 0x010,
	NS_LDAP_SCOPE_ONELEVEL	= 0x020,
	NS_LDAP_SCOPE_SUBTREE	= 0x040
} ScopeType_t;

/*
 * BE VERY CAREFUL. DO NOT USE FLAG NS_LDAP_KEEP_CONN UNLESS YOU MUST
 * IN libsldap.so.1 THERE IS NO CONNECTION GARBAGE COLLECTION AND IF
 * THIS FLAG GETS USED THERE MIGHT BE A CONNECTION LEAK. CURRENTLY THIS
 * IS ONLY SUPPORTED FOR LIST AND INTENDED FOR APPLICATIONS LIKE AUTOMOUNTER
*/

#define	NS_LDAP_KEEP_CONN	0x080

/*
 * Authentication Information
 */
typedef enum AuthType {
	NS_LDAP_AUTH_NONE,
	NS_LDAP_AUTH_SIMPLE,
	NS_LDAP_AUTH_SASL_CRAM_MD5,
	NS_LDAP_AUTH_SASL_GSSAPI,
	NS_LDAP_AUTH_SASL_SPNEGO,
	NS_LDAP_AUTH_TLS
} AuthType_t;

typedef enum SecType {
	NS_LDAP_SEC_NONE,
	NS_LDAP_SEC_SASL_INTEGRITY,
	NS_LDAP_SEC_SASL_PRIVACY,
	NS_LDAP_SEC_TLS
} SecType_t;

typedef enum PrefOnly {
	NS_LDAP_PREF_FALSE	= 0,
	NS_LDAP_PREF_TRUE	= 1
} PrefOnly_t;

typedef struct UnixCred {
	char	*userID;	/* Unix ID number */
	char	*passwd;	/* password */
} UnixCred_t;

typedef struct CertCred {
	char	*path;		/* certificate path */
	char	*passwd;	/* password */
} CertCred_t;

typedef struct Auth {
	AuthType_t	type;
	SecType_t	security;
	union {
		UnixCred_t	unix_cred;
		CertCred_t	cert_cred;
	} cred;
} Auth_t;


typedef struct LineBuf {
	char *str;
	int len;
	int alloc;
} LineBuf;

/*
 * Configuration Information
 */

typedef enum {
	NS_LDAP_FILE_VERSION_P,
	NS_LDAP_BINDDN_P,
	NS_LDAP_BINDPASSWD_P,
	NS_LDAP_SERVERS_P,
	NS_LDAP_SEARCH_BASEDN_P,
	NS_LDAP_AUTH_P,
	NS_LDAP_TRANSPORT_SEC_P,
	NS_LDAP_SEARCH_REF_P,
	NS_LDAP_DOMAIN_P,
	NS_LDAP_EXP_P,
	NS_LDAP_CERT_PATH_P,
	NS_LDAP_CERT_PASS_P,
	NS_LDAP_SEARCH_DN_P,
	NS_LDAP_SEARCH_SCOPE_P,
	NS_LDAP_SEARCH_TIME_P,
	NS_LDAP_SERVER_PREF_P,
	NS_LDAP_PREF_ONLY_P,
	NS_LDAP_CACHETTL_P,
	NS_LDAP_PROFILE_P
/*
 * If you add to this structure, please make sure
 * that you update the DomainParamType structure as well
 * in order to have the array size set properly
 */

} ParamIndexType;

typedef void** ConfigParamValType;

typedef struct DomainParamType {
	char *	domainName;
	ConfigParamValType	configParamList[NS_LDAP_PROFILE_P + 1];
	struct DomainParamType *next;		/* next entry */
} DomainParamType;


/*
 * __ns_ldap_*() return codes
 */
typedef enum {
	NS_LDAP_SUCCESS,	/* success, no info in errorp */
	NS_LDAP_OP_FAILED,	/* failed operation, no info in errorp */
	NS_LDAP_NOTFOUND,	/* entry not found, no info in errorp */
	NS_LDAP_MEMORY,		/* memory failure, no info in errorp */
	NS_LDAP_CONFIG,		/* config problem, detail in errorp */
	NS_LDAP_PARTIAL,	/* partial result, detail in errorp */
	NS_LDAP_INTERNAL,	/* LDAP error, detail in errorp */
	NS_LDAP_INVALID_PARAM	/* LDAP error, no info in errorp */
} ns_ldap_return_code;

/*
 * Detailed error code for NS_LDAP_CONFIG
 */
typedef enum {
	NS_CONFIG_SYNTAX,	/* syntax error */
	NS_CONFIG_NODEFAULT,	/* no default value */
	NS_CONFIG_NOTLOADED,	/* configuration not loaded */
	NS_CONFIG_NOTALLOW,	/* operation requested not allowed */
	NS_CONFIG_FILE,		/* configuration file problem */
	NS_CONFIG_NODOMAIN,	/* no info for domain specified */
	NS_CONFIG_DOORERROR	/* error with door */
} ns_ldap_config_return_code;

/*
 * Detailed error code for NS_LDAP_PARTIAL
 */
typedef enum {
	NS_PARTIAL_TIMEOUT,	/* partial results due to timeout */
	NS_PARTIAL_OTHER	/* error encountered */
} ns_ldap_partial_return_code;

/*
 * Simplified LDAP Naming API result structure
 */
typedef struct ns_ldap_error {
	int	status;			/* LDAP error code */
	char	*message;		/* LDAP error message */
} ns_ldap_error_t;

typedef struct	 ns_ldap_attr {
	char	*attrname;			/* attribute name */
	u_int	value_count;
	char	**attrvalue;			/* attribute values */
} ns_ldap_attr_t;

typedef struct ns_ldap_entry {
	u_int		attr_count;		/* number of attributes */
	ns_ldap_attr_t	**attr_pair;		/* attributes pairs */
	struct ns_ldap_entry *next;		/* next entry */
} ns_ldap_entry_t;

typedef struct ns_ldap_result {
	u_int	entries_count;		/* number of entries */
	ns_ldap_entry_t	*entry;		/* data */
} ns_ldap_result_t;

/*
 * return values for the callback fucntion in __ns_ldap_list()
 */
#define	NS_LDAP_CB_NEXT	0	/* get the next entry */
#define	NS_LDAP_CB_DONE	1	/* done */

/*
 * Simplified LDAP Naming APIs
 */
int __ns_ldap_list(
	const char *database,
	const char *filter,
	const char * const *attribute,
	const char *domain,
	const Auth_t *cred,
	const int flags,
	ns_ldap_result_t ** result,
	ns_ldap_error_t ** errorp,
	int (*callback)(const ns_ldap_entry_t *entry, const void *userdata),
	const void *userdata);

int  __ns_ldap_addAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t **errorp);

int __ns_ldap_delAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t **errorp);

int  __ns_ldap_repAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t **errorp);

int  __ns_ldap_addEntry(
	const char *dn,
	const ns_ldap_entry_t *entry,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t **errorp);

int __ns_ldap_delEntry(
	const char *dn,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t **errorp);

int __ns_ldap_firstEntry(
	const char *database,
	const char *filter,
	const char * const *attribute,
	const char *domain,
	const Auth_t *cred,
	const int flags,
	void **cookie,
	ns_ldap_result_t ** result,
	ns_ldap_error_t **errorp);

int  __ns_ldap_nextEntry(
	void *cookie,
	ns_ldap_result_t ** result,
	ns_ldap_error_t **errorp);

int  __ns_ldap_endEntry(
	void **cookie,
	ns_ldap_error_t **errorp);

int __ns_ldap_freeResult(
	ns_ldap_result_t **result);

int __ns_ldap_freeError(
	ns_ldap_error_t **errorp);

int  __ns_ldap_uid2dn(
	const char *uid,
	const char *domain,
	char **userDN,
	const char *cred,
	ns_ldap_error_t ** errorp);

int  __ns_ldap_host2dn(
	const char *host,
	const char *domain,
	char **hostDN,
	const char *cred,
	ns_ldap_error_t ** errorp);

int __ns_ldap_auth(
	const Auth_t *cred,
	const char *domain,
	const int flag,
	ns_ldap_error_t **errorp);

int __ns_ldap_freeAuth(
	Auth_t **authp);

int __ns_ldap_err2str(
	int err,
	char **strmsg);

int __ns_ldap_setParam(
	const char *domain,
	const ParamIndexType type,
	const void *data,
	ns_ldap_error_t **errorp);

int __ns_ldap_getParam(
	const char *domain,
	const ParamIndexType type,
	void ***data,
	ns_ldap_error_t **errorp);

int __ns_ldap_freeParam(
	void ***data);

char **__ns_ldap_getAttr(
	const ns_ldap_entry_t *entry,
	const char *attrname);

#ifdef __cplusplus
}
#endif

#endif /* _NS_SLDAP_H */
