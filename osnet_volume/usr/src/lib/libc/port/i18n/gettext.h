/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBC_PORT_I18N_GETTEXT_H
#define	_LIBC_PORT_I18N_GETTEXT_H

#pragma	ident  "@(#)gettext.h 1.1     99/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct domain_binding {
	char	*domain;	/* domain name */
	char	*binding;	/* binding directory */
	struct domain_binding	*next;
} Dbinding;

/*
 * this structure is used for preserving nlspath templates before
 * passing them to bindtextdomain():
 */
typedef struct nlstmp {
	char	pathname[MAXPATHLEN];	/* the full pathname to file */
	struct nlstmp	*next;		/* link to the next entry */
} Nlstmp;

typedef struct msg_node {
	char	*path;			/* name of message catalog */
	struct msg_info	*msg_file_info;	/* information of msg file */
	struct msg_struct	*msg_list;	/* message list */
	char	*msg_ids;		/* actual message ids */
	char	*msg_strs;		/* actual message strs */
	struct msg_node	*next;	/* link to the next */
} Msg_node;

typedef struct nls_node {
	char	*domain;		/* key: domain name */
	char	*locale;		/* key: locale name */
	char	*nlspath;		/* key: NLSPATH */
	char	*ppaths;		/* value: expanded path */
	int	count;				/* value: # of components */
	struct nls_node	*next;	/* link to the next */
} Nls_node;

typedef struct cache_node {
	unsigned int	hashid;	/* hashed valued for the locale name */
	Msg_node	*m_node;	/* link to Msg_node */
	Nls_node	*n_node;	/* link to Nls_node */
	struct cache_node	*next;	/* link to the next */
} Cache_node;

typedef struct {
	char	*cur_domain;	/* current domain */
	Dbinding	*dbind;		/* domain binding */
	Cache_node	*c_node;	/* link to the cache node */
	Msg_node	*c_m_node;	/* link to the current Msg_node */
	Nls_node	*c_n_node;	/* link to the current Nls_node */
} Gettext_t;

#define	DEFAULT_DOMAIN		"messages"
#define	DEFAULT_BINDING		_DFLT_LOC_PATH
#define	MSGFILESUFFIX		".mo"
#define	MSGFILESUFFIXLEN	3

#define	CURRENT_DOMAIN(gt)	(gt)->cur_domain
#define	FIRSTBIND(gt)	(gt)->dbind

#define	ALLFREE \
	{ \
		Nlstmp	*tp, *tq; \
		tp = nlstmp; \
		while (tp) { \
			tq = tp->next; \
			free(tp); \
			tp = tq; \
		} \
		if (nnp->locale) \
			free(nnp->locale); \
		if (nnp->domain) \
			free(nnp->domain); \
		if (ppaths) \
			free(ppaths); \
		if (lang) \
			free(lang); \
		if (scnp) \
			free(cnp); \
		free(nnp); \
	}


#define	GET_HASHID(str, hashid) \
	{ \
		unsigned char	*__cp = (unsigned char *)str; \
		unsigned char	__c; \
		unsigned int	__id = 0; \
		while ((__c = *__cp++) != '\0') \
			if (__c >= ' ') \
				__id = (__id >> 5) + (__id << 27) + __c; \
		hashid = (__id % 899981) + 100000; \
	}

#define	SIGNAL_HOLD \
	(void) sighold(SIGINT); (void) sighold(SIGQUIT);\
	(void) sighold(SIGTERM)

#define	SIGNAL_RELEASE \
	(void) sigrelse(SIGTERM); (void) sigrelse(SIGQUIT);\
	(void) sigrelse(SIGINT)

#define	INIT_GT(def) \
	if (!global_gt) { \
		global_gt = (Gettext_t *)calloc(1, sizeof (Gettext_t)); \
		if (global_gt) \
			global_gt->cur_domain = (char *)default_domain; \
		else { \
			(void) _mutex_unlock(&gt_lock); \
			return ((def)); \
		} \
	}

#define	STRLEN(len, s) \
	s0 = (char *)(s); while (*s0++ != '\0'); (len) = s0 - ((s) + 1)

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIBC_PORT_I18N_GETTEXT_H */
