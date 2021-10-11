#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)sldap.spec	1.1	99/07/07 SMI"
#
# lib/libsldap/spec/sldap.spec

function	__getldapaliasbyname
include		"../../common/ns_sldap.h"
declaration	int __getldapaliasbyname( \
			char *alias, \
			char *answer, \
			size_t ans_len)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_list
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_list( \
			const char *database, \
			const char *filter, \
			const char * const *attribute, \
			const char *domain, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_result_t ** result, \
			ns_ldap_error_t ** errorp, \
			int (*callback)(const ns_ldap_entry_t *entry, \
				const void *userdata), \
			const void *userdata)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_addAttr
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_addAttr( \
			const char *dn, \
			const ns_ldap_attr_t * const *attr, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_delAttr
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_delAttr( \
			const char *dn, \
			const ns_ldap_attr_t * const *attr, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_repAttr
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_repAttr( \
			const char *dn, \
			const ns_ldap_attr_t * const *attr, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_addEntry
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_addEntry( \
			const char *dn, \
			const ns_ldap_entry_t *entry, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_delEntry
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_delEntry( \
			const char *dn, \
			const Auth_t *cred, \
			const int flags, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_firstEntry
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_firstEntry( \
			const char *database, \
			const char *filter, \
			const char * const *attribute, \
			const char *domain, \
			const Auth_t *cred, \
			const int flags, \
			void **cookie, \
			ns_ldap_result_t ** result, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_nextEntry
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_nextEntry( \
			void *cookie, \
			ns_ldap_result_t ** result, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_endEntry
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_endEntry( \
			void **cookie, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_freeResult
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_freeResult( \
			ns_ldap_result_t **result)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_freeError
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_freeError( \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_freeAuth
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_freeAuth( \
			Auth_t **authp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_uid2dn
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_uid2dn( \
			const char *uid, \
			const char *domain, \
			char **userDN, \
			const char *cred, \
			ns_ldap_error_t ** errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_host2dn
include		"../../common/ns_sldap.h"
declaration	int  __ns_ldap_host2dn( \
			const char *host, \
			const char *domain, \
			char **hostDN, \
			const char *cred, \
			ns_ldap_error_t ** errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_auth
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_auth( \
			const Auth_t *cred, \
			const char *domain, \
			const int flag, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_err2str
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_err2str( \
			int err, \
			char **strmsg)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_getParam
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_getParam( \
			const char *domain, \
			const ParamIndexType type, \
			void ***data, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_setParam
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_setParam( \
			const char *domain, \
			const ParamIndexType type, \
			const void *data, \
			ns_ldap_error_t **errorp)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_freeParam
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_freeParam(void ***data)
version		SUNWprivate_1.0
exception	$return == -1
end

function	__ns_ldap_getAttr
include		"../../common/ns_sldap.h"
declaration	char **__ns_ldap_getAttr( \
			const ns_ldap_entry_t *entry, \
			const char *attrname); 
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_setServer
include		"../../common/ns_sldap.h"
declaration	void __ns_ldap_setServer( \
			int set); 
version		SUNWprivate_1.0
end

function	__ns_ldap_LoadConfiguration
include		"../../common/ns_sldap.h"
declaration	ns_ldap_error_t *__ns_ldap_LoadConfiguration( \
			char *domainname); 
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_cache_destroy
include		"../../common/ns_sldap.h"
declaration	void __ns_ldap_cache_destroy( \
			);
version		SUNWprivate_1.0
end

function	__ns_ldap_LoadDoorInfo
include		"../../common/ns_sldap.h"
declaration	ns_ldap_error_t *__ns_ldap_LoadDoorInfo( \
			LineBuf *configinfo, \
			char *domainname);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_DumpConfiguration
include		"../../common/ns_sldap.h"
declaration	ns_ldap_error_t *__ns_ldap_DumpConfiguration( \
			char *filename);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_DumpLdif
include		"../../common/ns_sldap.h"
declaration	ns_ldap_error_t *__ns_ldap_DumpLdif( \
			char *filename);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_download
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_download( \
			const char *profilename, \
			char *serveraddr, \
			char *basedn);
version		SUNWprivate_1.0
exception	$return == 1
end

function	__ns_ldap_dump_profile
include		"../../common/ns_sldap.h"
declaration	int __ns_ldap_dump_profile( \
			DomainParamType *ptr);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_trydoorcall
include		"../../common/ns_cache_door.h"
declaration	int __ns_ldap_trydoorcall( \
			ldap_data_t **dptr, \
			int *ndata, \
			int *adata);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_print_config
include		"../../common/ns_sldap.h"
declaration	ns_ldap_error_t *__ns_ldap_print_config(int verbose);
version		SUNWprivate_1.0
exception	$return == NULL
end

function	__ns_ldap_default_config
include		"../../common/ns_sldap.h"
declaration	void __ns_ldap_default_config();
version		SUNWprivate_1.0
end

function	__ns_ldap_cache_ping
include		"../../common/ns_sldap.h"
declaration	void __ns_ldap_cache_ping();
version		SUNWprivate_1.0
end
