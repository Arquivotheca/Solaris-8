/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PROTO_LDAP_H
#define	_PROTO_LDAP_H

#pragma ident	"@(#)proto-ldap.h	1.2	97/11/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * proto-ldap.h
 * function prototypes for ldap library
 */


#ifndef LIBIMPORT
#ifdef _WIN32
#define	LIBIMPORT	__declspec(dllimport)
#else /* _WIN32 */
#define	LIBIMPORT
#endif /* _WIN32 */
#endif /* LIBIMPORT */


/*
 * in abandon.c:
 */
LIBIMPORT int ldap_abandon(LDAP *ld, int msgid);

/*
 * in add.c:
 */
LIBIMPORT int ldap_add(LDAP *ld, char *dn, LDAPMod **attrs);
LIBIMPORT int ldap_add_s(LDAP *ld, char *dn, LDAPMod **attrs);

/*
 * in bind.c:
 */
LIBIMPORT int ldap_bind(LDAP *ld, char *who, char *passwd, int authmethod);
LIBIMPORT int ldap_bind_s(LDAP *ld, char *who, char *cred, int method);
LIBIMPORT int ldap_simple_bind(LDAP *ld, char *who, char *passwd);
LIBIMPORT int ldap_simple_bind_s(LDAP *ld, char *who, char *passwd);
LIBIMPORT int ldap_kerberos_bind_s(LDAP *ld, char *who);
LIBIMPORT int ldap_kerberos_bind1(LDAP *ld, char *who);
LIBIMPORT int ldap_kerberos_bind1_s(LDAP *ld, char *who);
LIBIMPORT int ldap_kerberos_bind2(LDAP *ld, char *who);
LIBIMPORT int ldap_kerberos_bind2_s(LDAP *ld, char *who);
LIBIMPORT int ldap_unbind(LDAP *ld);

#ifndef NO_CACHE
/*
 * in cache.c
 */
LIBIMPORT int ldap_enable_cache(LDAP *ld, time_t timeout, ssize_t maxmem);
LIBIMPORT void ldap_disable_cache(LDAP *ld);
LIBIMPORT void ldap_set_cache_options(LDAP *ld, unsigned int opts);
LIBIMPORT void ldap_destroy_cache(LDAP *ld);
LIBIMPORT void ldap_flush_cache(LDAP *ld);
LIBIMPORT void ldap_uncache_entry(LDAP *ld, char *dn);
LIBIMPORT void ldap_uncache_request(LDAP *ld, int msgid);
#endif /* !NO_CACHE */

/*
 * in compare.c:
 */
LIBIMPORT int ldap_compare(LDAP *ld, char *dn, char *attr, char *value);
LIBIMPORT int ldap_compare_s(LDAP *ld, char *dn, char *attr, char *value);

/*
 * in delete.c:
 */
LIBIMPORT int ldap_delete(LDAP *ld, char *dn);
LIBIMPORT int ldap_delete_s(LDAP *ld, char *dn);

/*
 * in error.c:
 */
LIBIMPORT int ldap_result2error(LDAP *ld, LDAPMessage *r, int freeit);
LIBIMPORT char *ldap_err2string(int err);
LIBIMPORT void ldap_perror(LDAP *ld, char *s);

/*
 * in modify.c:
 */
LIBIMPORT int ldap_modify(LDAP *ld, char *dn, LDAPMod **mods);
LIBIMPORT int ldap_modify_s(LDAP *ld, char *dn, LDAPMod **mods);

/*
 * in modrdn.c:
 */
LIBIMPORT int ldap_modrdn(LDAP *ld, char *dn, char *newrdn);
LIBIMPORT int ldap_modrdn_s(LDAP *ld, char *dn, char *newrdn);
LIBIMPORT int ldap_modrdn2(LDAP *ld, char *dn, char *newrdn,
	int deleteoldrdn);
LIBIMPORT int ldap_modrdn2_s(LDAP *ld, char *dn, char *newrdn,
	int deleteoldrdn);

/*
 * in open.c:
 */
LIBIMPORT LDAP *ldap_open(char *host, int port);
LIBIMPORT LDAP *ldap_init(char *defhost, int defport);

/*
 * in getentry.c:
 */
LIBIMPORT LDAPMessage *ldap_first_entry(LDAP *ld, LDAPMessage *chain);
LIBIMPORT LDAPMessage *ldap_next_entry(LDAP *ld, LDAPMessage *entry);
LIBIMPORT int ldap_count_entries(LDAP *ld, LDAPMessage *chain);

/*
 * in addentry.c
 */
LIBIMPORT LDAPMessage *ldap_delete_result_entry(LDAPMessage **list,
	LDAPMessage *e);
LIBIMPORT void ldap_add_result_entry(LDAPMessage **list, LDAPMessage *e);

/*
 * in getdn.c
 */
LIBIMPORT char *ldap_get_dn(LDAP *ld, LDAPMessage *entry);
LIBIMPORT char *ldap_dn2ufn(char *dn);
LIBIMPORT char **ldap_explode_dn(char *dn, int notypes);
LIBIMPORT char **ldap_explode_dns(char *dn);
LIBIMPORT int ldap_is_dns_dn(char *dn);

/*
 * in getattr.c
 */
LIBIMPORT char *ldap_first_attribute(LDAP *ld, LDAPMessage *entry,
	BerElement **ber);
LIBIMPORT char *ldap_next_attribute(LDAP *ld, LDAPMessage *entry,
	BerElement *ber);

/*
 * in getvalues.c
 */
LIBIMPORT char **ldap_get_values(LDAP *ld, LDAPMessage *entry, char *target);
LIBIMPORT struct berval **ldap_get_values_len(LDAP *ld, LDAPMessage *entry,
	char *target);
LIBIMPORT int ldap_count_values(char **vals);
LIBIMPORT int ldap_count_values_len(struct berval **vals);
LIBIMPORT void ldap_value_free(char **vals);
LIBIMPORT void ldap_value_free_len(struct berval **vals);

/*
 * in result.c:
 */
LIBIMPORT int ldap_result(LDAP *ld, int msgid, int all,
	struct timeval *timeout, LDAPMessage **result);
LIBIMPORT int ldap_msgfree(LDAPMessage *lm);
LIBIMPORT int ldap_msgdelete(LDAP *ld, int msgid);

/*
 * in search.c:
 */
LIBIMPORT int ldap_search(LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly);
LIBIMPORT int ldap_search_s(LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly, LDAPMessage **res);
LIBIMPORT int ldap_search_st(LDAP *ld, char *base, int scope, char *filter,
    char **attrs, int attrsonly, struct timeval *timeout, LDAPMessage **res);

/*
 * in ufn.c
 */
LIBIMPORT int ldap_ufn_search_c(LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res, int (*cancelproc)(void *cl),
	void *cancelparm);
LIBIMPORT int ldap_ufn_search_ct(LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res, int (*cancelproc)(void *cl),
	void *cancelparm, char *tag1, char *tag2, char *tag3);
LIBIMPORT int ldap_ufn_search_s(LDAP *ld, char *ufn, char **attrs,
	int attrsonly, LDAPMessage **res);
LIBIMPORT LDAPFiltDesc *ldap_ufn_setfilter(LDAP *ld, char *fname);
LIBIMPORT void ldap_ufn_setprefix(LDAP *ld, char *prefix);
LIBIMPORT int ldap_ufn_timeout(void *tvparam);


/*
 * in unbind.c
 */
LIBIMPORT int ldap_unbind_s(LDAP *ld);


/*
 * in getfilter.c
 */
LIBIMPORT LDAPFiltDesc *ldap_init_getfilter(char *fname);
LIBIMPORT LDAPFiltDesc *ldap_init_getfilter_buf(char *buf, ssize_t buflen);
LIBIMPORT LDAPFiltInfo *ldap_getfirstfilter(LDAPFiltDesc *lfdp, char *tagpat,
	char *value);
LIBIMPORT LDAPFiltInfo *ldap_getnextfilter(LDAPFiltDesc *lfdp);
LIBIMPORT void ldap_setfilteraffixes(LDAPFiltDesc *lfdp, char *prefix,
	char *suffix);
LIBIMPORT void ldap_build_filter(char *buf, size_t buflen,
	char *pattern, char *prefix, char *suffix, char *attr,
	char *value, char **valwords);

/*
 * in free.c
 */
LIBIMPORT void ldap_getfilter_free(LDAPFiltDesc *lfdp);
LIBIMPORT void ldap_mods_free(LDAPMod **mods, int freemods);

/*
 * in friendly.c
 */
LIBIMPORT char *ldap_friendly_name(char *filename, char *uname,
	FriendlyMap **map);
LIBIMPORT void ldap_free_friendlymap(FriendlyMap **map);


/*
 * in cldap.c
 */
LIBIMPORT LDAP *cldap_open(char *host, int port);
LIBIMPORT void cldap_close(LDAP *ld);
LIBIMPORT int cldap_search_s(LDAP *ld, char *base, int scope, char *filter,
	char **attrs, int attrsonly, LDAPMessage **res, char *logdn);
LIBIMPORT void cldap_setretryinfo(LDAP *ld, int tries, time_t timeout);


/*
 * in sort.c
 */
LIBIMPORT int ldap_sort_entries(LDAP *ld, LDAPMessage **chain, char *attr,
	int (*cmp)());
LIBIMPORT int ldap_sort_values(LDAP *ld, char **vals, int (*cmp)());
LIBIMPORT int ldap_sort_strcasecmp(char **a, char **b);


/*
 * in charset.c
 */
#ifdef STR_TRANSLATION
LIBIMPORT void ldap_set_string_translators(LDAP *ld,
	BERTranslateProc encode_proc, BERTranslateProc decode_proc);
#endif /* STR_TRANSLATION */

#ifdef LDAP_CHARSET_8859
LIBIMPORT int ldap_t61_to_8859(char **bufp, unsigned int *buflenp,
	int free_input);
LIBIMPORT int ldap_8859_to_t61(char **bufp, unsigned int *buflenp,
	int free_input);
#endif /* LDAP_CHARSET_8859 */


#ifdef WINSOCK
/*
 * in msdos/winsock/wsa.c
 */
LIBIMPORT void ldap_memfree(void *p);
#endif /* WINSOCK */


#ifdef	__cplusplus
}
#endif

#endif	/* _PROTO_LDAP_H */
