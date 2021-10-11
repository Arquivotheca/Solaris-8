/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAP_H
#define	_LDAP_H

#pragma ident	"@(#)ldap.h	1.3	97/11/12 SMI"


/*
 * Copyright (c) 1990 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

/* #ifndef _LDAP_H */
/* #define	_LDAP_H */

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef WINSOCK
#include "msdos.h"
#include <winsock.h>
#endif

#if !defined(NEEDPROTOS) && defined(__STDC__)
#define	NEEDPROTOS	1
#endif

#define	LDAP_PORT	389
#define	LDAP_VERSION1	1
#define	LDAP_VERSION2	2
#define	LDAP_VERSION	LDAP_VERSION2

#define	COMPAT20
#define	COMPAT30
#if defined(COMPAT20) || defined(COMPAT30)
#define	COMPAT
#endif

#define	LDAP_MAX_ATTR_LEN	100

/* debugging stuff */
#ifdef LDAP_DEBUG
extern int	ldap_debug;
#ifdef LDAP_SYSLOG
extern int	ldap_syslog;
extern int	ldap_syslog_level;
#endif
#define	LDAP_DEBUG_TRACE	0x001
#define	LDAP_DEBUG_PACKETS	0x002
#define	LDAP_DEBUG_ARGS		0x004
#define	LDAP_DEBUG_CONNS	0x008
#define	LDAP_DEBUG_BER		0x010
#define	LDAP_DEBUG_FILTER	0x020
#define	LDAP_DEBUG_CONFIG	0x040
#define	LDAP_DEBUG_ACL		0x080
#define	LDAP_DEBUG_STATS	0x100
#define	LDAP_DEBUG_STATS2	0x200
#define	LDAP_DEBUG_SHELL	0x400
#define	LDAP_DEBUG_PARSE	0x800
#define	LDAP_DEBUG_ANY		0xffff

#ifdef LDAP_SYSLOG
#define	Debug(level, fmt, arg1, arg2, arg3)	\
	{ \
		if (ldap_debug & level) \
			fprintf(stderr, fmt, arg1, arg2, arg3); \
		if (ldap_syslog & level) \
			syslog(ldap_syslog_level, fmt, arg1, arg2, arg3); \
	}
#else /* LDAP_SYSLOG */
#ifndef WINSOCK
#define	Debug(level, fmt, arg1, arg2, arg3) \
		if (ldap_debug & level) \
			fprintf(stderr, fmt, arg1, arg2, arg3);
#else /* !WINSOCK */
extern void Debug(int level, char *fmt, void *arg1, void *arg2, void *arg3);
#endif /* !WINSOCK */
#endif /* LDAP_SYSLOG */
#else /* LDAP_DEBUG */
#define	Debug(level, fmt, arg1, arg2, arg3)
#endif /* LDAP_DEBUG */

/*
 * specific LDAP instantiations of BER types we know about
 */

/* general stuff */
#define	LDAP_TAG_MESSAGE	0x30	/* tag is 16 + constructed bit */
#define	OLD_LDAP_TAG_MESSAGE	0x10	/* forgot the constructed bit  */
#define	LDAP_TAG_MSGID		0x02

/* possible operations a client can invoke */
#define	LDAP_REQ_BIND			0x60	/* application + constructed */
#define	LDAP_REQ_UNBIND			0x42	/* application + primitive   */
#define	LDAP_REQ_SEARCH			0x63	/* application + constructed */
#define	LDAP_REQ_MODIFY			0x66	/* application + constructed */
#define	LDAP_REQ_ADD			0x68	/* application + constructed */
#define	LDAP_REQ_DELETE			0x4a	/* application + primitive   */
#define	LDAP_REQ_MODRDN			0x6c	/* application + constructed */
#define	LDAP_REQ_COMPARE		0x6e	/* application + constructed */
#define	LDAP_REQ_ABANDON		0x50	/* application + primitive   */

/* version 3.0 compatibility stuff */
#define	LDAP_REQ_UNBIND_30		0x62
#define	LDAP_REQ_DELETE_30		0x6a
#define	LDAP_REQ_ABANDON_30		0x70

/*
 * old broken stuff for backwards compatibility - forgot application tag
 * and constructed/primitive bit
 */
#define	OLD_LDAP_REQ_BIND		0x00
#define	OLD_LDAP_REQ_UNBIND		0x02
#define	OLD_LDAP_REQ_SEARCH		0x03
#define	OLD_LDAP_REQ_MODIFY		0x06
#define	OLD_LDAP_REQ_ADD		0x08
#define	OLD_LDAP_REQ_DELETE		0x0a
#define	OLD_LDAP_REQ_MODRDN		0x0c
#define	OLD_LDAP_REQ_COMPARE		0x0e
#define	OLD_LDAP_REQ_ABANDON		0x10

/* possible result types a server can return */
#define	LDAP_RES_BIND			0x61	/* application + constructed */
#define	LDAP_RES_SEARCH_ENTRY		0x64	/* application + constructed */
#define	LDAP_RES_SEARCH_RESULT		0x65	/* application + constructed */
#define	LDAP_RES_MODIFY			0x67	/* application + constructed */
#define	LDAP_RES_ADD			0x69	/* application + constructed */
#define	LDAP_RES_DELETE			0x6b	/* application + constructed */
#define	LDAP_RES_MODRDN			0x6d	/* application + constructed */
#define	LDAP_RES_COMPARE		0x6f	/* application + constructed */
#define	LDAP_RES_ANY			(-1)

/* old broken stuff for backwards compatibility */
#define	OLD_LDAP_RES_BIND		0x01
#define	OLD_LDAP_RES_SEARCH_ENTRY	0x04
#define	OLD_LDAP_RES_SEARCH_RESULT	0x05
#define	OLD_LDAP_RES_MODIFY		0x07
#define	OLD_LDAP_RES_ADD		0x09
#define	OLD_LDAP_RES_DELETE		0x0b
#define	OLD_LDAP_RES_MODRDN		0x0d
#define	OLD_LDAP_RES_COMPARE		0x0f

/* authentication methods available */
#define	LDAP_AUTH_NONE		0x00	/* no authentication		  */
#define	LDAP_AUTH_SIMPLE	0x80	/* context specific + primitive   */
#define	LDAP_AUTH_KRBV4		0xff	/* means do both of the following */
#define	LDAP_AUTH_KRBV41	0x81	/* context specific + primitive   */
#define	LDAP_AUTH_KRBV42	0x82	/* context specific + primitive   */

/* 3.0 compatibility auth methods */
#define	LDAP_AUTH_SIMPLE_30	0xa0	/* context specific + constructed */
#define	LDAP_AUTH_KRBV41_30	0xa1	/* context specific + constructed */
#define	LDAP_AUTH_KRBV42_30	0xa2	/* context specific + constructed */

/* old broken stuff */
#define	OLD_LDAP_AUTH_SIMPLE	0x00
#define	OLD_LDAP_AUTH_KRBV4	0x01
#define	OLD_LDAP_AUTH_KRBV42	0x02

/* filter types */
#define	LDAP_FILTER_AND		0xa0	/* context specific + constructed */
#define	LDAP_FILTER_OR		0xa1	/* context specific + constructed */
#define	LDAP_FILTER_NOT		0xa2	/* context specific + constructed */
#define	LDAP_FILTER_EQUALITY	0xa3	/* context specific + constructed */
#define	LDAP_FILTER_SUBSTRINGS	0xa4	/* context specific + constructed */
#define	LDAP_FILTER_GE		0xa5	/* context specific + constructed */
#define	LDAP_FILTER_LE		0xa6	/* context specific + constructed */
#define	LDAP_FILTER_PRESENT	0x87	/* context specific + primitive   */
#define	LDAP_FILTER_APPROX	0xa8	/* context specific + constructed */

/* 3.0 compatibility filter types */
#define	LDAP_FILTER_PRESENT_30	0xa7	/* context specific + constructed */

/* old broken stuff */
#define	OLD_LDAP_FILTER_AND		0x00
#define	OLD_LDAP_FILTER_OR		0x01
#define	OLD_LDAP_FILTER_NOT		0x02
#define	OLD_LDAP_FILTER_EQUALITY	0x03
#define	OLD_LDAP_FILTER_SUBSTRINGS	0x04
#define	OLD_LDAP_FILTER_GE		0x05
#define	OLD_LDAP_FILTER_LE		0x06
#define	OLD_LDAP_FILTER_PRESENT		0x07
#define	OLD_LDAP_FILTER_APPROX		0x08

/* substring filter component types */
#define	LDAP_SUBSTRING_INITIAL	0x80	/* context specific */
#define	LDAP_SUBSTRING_ANY	0x81	/* context specific */
#define	LDAP_SUBSTRING_FINAL	0x82	/* context specific */

/* 3.0 compatibility substring filter component types */
#define	LDAP_SUBSTRING_INITIAL_30	0xa0	/* context specific */
#define	LDAP_SUBSTRING_ANY_30		0xa1	/* context specific */
#define	LDAP_SUBSTRING_FINAL_30		0xa2	/* context specific */

/* old broken stuff */
#define	OLD_LDAP_SUBSTRING_INITIAL	0x00
#define	OLD_LDAP_SUBSTRING_ANY		0x01
#define	OLD_LDAP_SUBSTRING_FINAL	0x02

/* search scopes */
#define	LDAP_SCOPE_BASE		0x00
#define	LDAP_SCOPE_ONELEVEL	0x01
#define	LDAP_SCOPE_SUBTREE	0x02

/* for modifications */
typedef struct ldapmod {
	int		mod_op;
#define	LDAP_MOD_ADD		0x00
#define	LDAP_MOD_DELETE		0x01
#define	LDAP_MOD_REPLACE	0x02
#define	LDAP_MOD_BVALUES	0x80
	char		*mod_type;
	union {
		char		**modv_strvals;
		struct berval	**modv_bvals;
	} mod_vals;
#define	mod_values	mod_vals.modv_strvals
#define	mod_bvalues	mod_vals.modv_bvals
	struct ldapmod	*mod_next;
} LDAPMod;

/*
 * possible error codes we can return
 */

#define	LDAP_SUCCESS			0x00
#define	LDAP_OPERATIONS_ERROR		0x01
#define	LDAP_PROTOCOL_ERROR		0x02
#define	LDAP_TIMELIMIT_EXCEEDED		0x03
#define	LDAP_SIZELIMIT_EXCEEDED		0x04
#define	LDAP_COMPARE_FALSE		0x05
#define	LDAP_COMPARE_TRUE		0x06
#define	LDAP_STRONG_AUTH_NOT_SUPPORTED	0x07
#define	LDAP_STRONG_AUTH_REQUIRED	0x08
#define	LDAP_PARTIAL_RESULTS		0x09

#define	LDAP_NO_SUCH_ATTRIBUTE		0x10
#define	LDAP_UNDEFINED_TYPE		0x11
#define	LDAP_INAPPROPRIATE_MATCHING	0x12
#define	LDAP_CONSTRAINT_VIOLATION	0x13
#define	LDAP_TYPE_OR_VALUE_EXISTS	0x14
#define	LDAP_INVALID_SYNTAX		0x15

#define	LDAP_NO_SUCH_OBJECT		0x20
#define	LDAP_ALIAS_PROBLEM		0x21
#define	LDAP_INVALID_DN_SYNTAX		0x22
#define	LDAP_IS_LEAF			0x23
#define	LDAP_ALIAS_DEREF_PROBLEM	0x24

#define	NAME_ERROR(n)	((n & 0xf0) == 0x20)

#define	LDAP_INAPPROPRIATE_AUTH		0x30
#define	LDAP_INVALID_CREDENTIALS	0x31
#define	LDAP_INSUFFICIENT_ACCESS	0x32
#define	LDAP_BUSY			0x33
#define	LDAP_UNAVAILABLE		0x34
#define	LDAP_UNWILLING_TO_PERFORM	0x35
#define	LDAP_LOOP_DETECT		0x36

#define	LDAP_NAMING_VIOLATION		0x40
#define	LDAP_OBJECT_CLASS_VIOLATION	0x41
#define	LDAP_NOT_ALLOWED_ON_NONLEAF	0x42
#define	LDAP_NOT_ALLOWED_ON_RDN		0x43
#define	LDAP_ALREADY_EXISTS		0x44
#define	LDAP_NO_OBJECT_CLASS_MODS	0x45
#define	LDAP_RESULTS_TOO_LARGE		0x46

#define	LDAP_OTHER			0x50
#define	LDAP_SERVER_DOWN		0x51
#define	LDAP_LOCAL_ERROR		0x52
#define	LDAP_ENCODING_ERROR		0x53
#define	LDAP_DECODING_ERROR		0x54
#define	LDAP_TIMEOUT			0x55
#define	LDAP_AUTH_UNKNOWN		0x56
#define	LDAP_FILTER_ERROR		0x57
#define	LDAP_USER_CANCELLED		0x58
#define	LDAP_PARAM_ERROR		0x59
#define	LDAP_NO_MEMORY			0x5a


/* default limit on nesting of referrals */
#define	LDAP_DEFAULT_REFHOPLIMIT	5

/*
 * This structure represents both ldap messages and ldap responses.
 * These are really the same, except in the case of search responses,
 * where a response has multiple messages.
 */

typedef struct ldapmsg {
	int		lm_msgid;	/* the message id */
	int		lm_msgtype;	/* the message type */
	BerElement	*lm_ber;	/* the ber encoded message contents */
	struct ldapmsg	*lm_chain;	/* for search - next msg in the resp */
	struct ldapmsg	*lm_next;	/* next response */
	unsigned long	lm_time;	/* used to maintain cache */
} LDAPMessage;
#define	NULLMSG	((LDAPMessage *) NULL)


#ifdef LDAP_REFERRALS
/*
 * structure used to track outstanding requests
 */
typedef struct ldapreq {
	int		lr_msgid;	/* the message id */
	int		lr_status;	/* status of request */
#define	LDAP_REQST_INPROGRESS	1
#define	LDAP_REQST_CHASINGREFS	2
#define	LDAP_REQST_NOTCONNECTED	3
#define	LDAP_REQST_WRITING	4
	int		lr_outrefcnt;	/* count of outstanding referrals */
	int		lr_origid;	/* original request's message id */
	int		lr_parentcnt;	/* count of parent requests */
	int		lr_res_msgtype;	/* result message type */
	int		lr_res_errno;	/* result LDAP errno */
	char		*lr_res_error;	/* result error string */
	char		*lr_res_matched; /* result matched DN string */
	BerElement	*lr_ber;	/* ber encoded request contents */
	struct ldapreq	*lr_parent;	/* request that spawned this referral */
	struct ldapreq	*lr_refnext;	/* next referral spawned */
	struct ldapreq	*lr_prev;	/* previous request */
	struct ldapreq	*lr_next;	/* next request */
} LDAPRequest;


/*
 * structure for tracking LDAP server host, ports, DNs, etc.
 */
typedef struct ldap_server {
	char			*lsrv_host;
	char			*lsrv_dn;	/* if NULL, use default */
	int			lsrv_port;
	struct ldap_server	*lsrv_next;
} LDAPServer;


/*
 * structure for representing an LDAP server connection
 */
typedef struct ldap_conn {
	Sockbuf			*lconn_sb;
	int			lconn_refcnt;
	unsigned long		lconn_lastused;	/* time */
	int			lconn_status;
#define	LDAP_CONNST_NEEDSOCKET		1
#define	LDAP_CONNST_CONNECTING		2
#define	LDAP_CONNST_CONNECTED		3
	LDAPServer		*lconn_server;
	struct ldap_conn	*lconn_next;
} LDAPConn;
#endif /* LDAP_REFERRALS */


/*
 * structure for client cache
 */
#define	LDAP_CACHE_BUCKETS	31	/* cache hash table size */
typedef struct ldapcache {
	LDAPMessage	lc_buckets[LDAP_CACHE_BUCKETS];	/* hash table */
	LDAPMessage	*lc_requests;			/* unfulfilled reqs */
	time_t		lc_timeout;			/* request timeout */
	ssize_t		lc_maxmem;			/* memory to use */
	ssize_t		lc_memused;			/* memory in use */
	int		lc_enabled;			/* enabled? */
	unsigned int	lc_options;			/* options */
#define	LDAP_CACHE_OPT_CACHENOERRS	0x00000001
#define	LDAP_CACHE_OPT_CACHEALLERRS	0x00000002
}  LDAPCache;
#define	NULLLDCACHE ((LDAPCache *)NULL)

/*
 * structures for ldap getfilter routines
 */

typedef struct ldap_filt_info {
	char			*lfi_filter;
	char			*lfi_desc;
	int			lfi_scope;	/* LDAP_SCOPE_BASE, etc */
	int			lfi_isexact;	/* exact match filter? */
	struct ldap_filt_info	*lfi_next;
} LDAPFiltInfo;

typedef struct ldap_filt_list {
    char			*lfl_tag;
    char			*lfl_pattern;
    char			*lfl_delims;
    LDAPFiltInfo		*lfl_ilist;
    struct ldap_filt_list	*lfl_next;
} LDAPFiltList;


#define	LDAP_FILT_MAXSIZ	1024

typedef struct ldap_filt_desc {
	LDAPFiltList		*lfd_filtlist;
	LDAPFiltInfo		*lfd_curfip;
	LDAPFiltInfo		lfd_retfi;
	char			lfd_filter[ LDAP_FILT_MAXSIZ ];
	char			*lfd_curval;
	char			*lfd_curvalcopy;
	char			**lfd_curvalwords;
	char			*lfd_filtprefix;
	char			*lfd_filtsuffix;
} LDAPFiltDesc;


/*
 * structure representing an ldap connection
 */

typedef struct ldap {
	Sockbuf		ld_sb;		/* socket descriptor & buffer */
	char		*ld_host;
	int		ld_version;
	char		ld_lberoptions;
	int		ld_deref;
#define	LDAP_DEREF_NEVER	0
#define	LDAP_DEREF_SEARCHING	1
#define	LDAP_DEREF_FINDING	2
#define	LDAP_DEREF_ALWAYS	3

	time_t		ld_timelimit;
	int		ld_sizelimit;
#define	LDAP_NO_LIMIT		0

	LDAPFiltDesc	*ld_filtd;	/* from getfilter for ufn searches */
	char		*ld_ufnprefix;	/* for incomplete ufn's */

	int		ld_errno;
	char		*ld_error;
	char		*ld_matched;
	int		ld_msgid;

	/* do not mess with these */
#ifdef LDAP_REFERRALS
	LDAPRequest	*ld_requests;	/* list of outstanding requests */
#else /* LDAP_REFERRALS */
	LDAPMessage	*ld_requests;	/* list of outstanding requests */
#endif /* LDAP_REFERRALS */
	LDAPMessage	*ld_responses;	/* list of outstanding responses */
	int		*ld_abandoned;	/* array of abandoned requests */
	char		ld_attrbuffer[LDAP_MAX_ATTR_LEN];
	LDAPCache	*ld_cache;	/* non-null if cache is initialized */
	char		*ld_cldapdn;	/* DN used in connectionless search */

	/* it is OK to change these next four values directly */
	int		ld_cldaptries;	/* connectionless search retry count */
	time_t		ld_cldaptimeout; /* time between retries */
	int		ld_refhoplimit;	/* limit on referral nesting */
	unsigned int	ld_options;	/* boolean options */
#ifdef LDAP_DNS
#define	LDAP_OPT_DNS		0x00000001	/* use DN & DNS */
#endif /* LDAP_DNS */
#ifdef LDAP_REFERRALS
#define	LDAP_OPT_REFERRALS	0x00000002	/* chase referrals */
#endif /* LDAP_REFERRALS */

	/* do not mess with the rest though */
	char		*ld_defhost;	/* full name of default server */
	int		ld_defport;	/* port of default server */
	BERTranslateProc ld_lber_encode_translate_proc;
	BERTranslateProc ld_lber_decode_translate_proc;
#ifdef LDAP_REFERRALS
	LDAPConn	*ld_defconn;	/* default connection */
	LDAPConn	*ld_conns;	/* list of server connections */
	void		*ld_selectinfo;	/* platform specifics for select */
#endif /* LDAP_REFERRALS */
} LDAP;

/*
 * structure for ldap friendly mapping routines
 */

typedef struct friendly {
	char	*f_unfriendly;
	char	*f_friendly;
} FriendlyMap;


/*
 * handy macro to check whether LDAP struct is set up for CLDAP or not
 */
#define	LDAP_IS_CLDAP(ld)	(ld->ld_sb.sb_naddr > 0)


#ifndef NEEDPROTOS
extern LDAP *ldap_open();
extern LDAP *ldap_init();
#ifdef STR_TRANSLATION
extern void ldap_set_string_translators();
#endif /* STR_TRANSLATION */
extern LDAPMessage *ldap_first_entry();
extern LDAPMessage *ldap_next_entry();
extern char *ldap_get_dn();
extern char *ldap_dn2ufn();
extern char **ldap_explode_dn();
extern char *ldap_first_attribute();
extern char *ldap_next_attribute();
extern char **ldap_get_values();
extern struct berval **ldap_get_values_len();
extern void ldap_value_free();
extern void ldap_value_free_len();
extern int ldap_count_values();
extern int ldap_count_values_len();
extern char *ldap_err2string();
extern void ldap_getfilter_free();
extern LDAPFiltDesc *ldap_init_getfilter();
extern LDAPFiltDesc *ldap_init_getfilter_buf();
extern LDAPFiltInfo *ldap_getfirstfilter();
extern LDAPFiltInfo *ldap_getnextfilter();
extern void ldap_setfilteraffixes();
extern void ldap_build_filter();
extern void ldap_flush_cache();
extern void ldap_set_cache_options();
extern void ldap_uncache_entry();
extern void ldap_uncache_request();
extern char *ldap_friendly_name();
extern void ldap_free_friendlymap();
extern LDAP *cldap_open();
extern void cldap_setretryinfo();
extern void cldap_close();
extern LDAPFiltDesc *ldap_ufn_setfilter();
extern int ldap_ufn_timeout();
extern int ldap_sort_entries();
extern int ldap_sort_values();
extern int ldap_sort_strcasecmp();

#if defined(ultrix) || defined(VMS) || defined(nextstep)
extern char *strdup();
#endif

#else /* NEEDPROTOS */
#if !defined(MACOS) && !defined(DOS) && !defined(_WIN32) && !defined(WINSOCK)
#include <sys/time.h>
#endif
#if defined(WINSOCK) && !defined(_WIN32)
#include "proto-ld.h"
#else
#include "proto-ldap.h"
#endif

#ifdef VMS
extern char *strdup(const char *s);
#endif
#if defined(ultrix) || defined(nextstep)
extern char *strdup();
#endif

#endif /* NEEDPROTOS */

#ifdef	__cplusplus
}
#endif

#endif	/* _LDAP_H */
