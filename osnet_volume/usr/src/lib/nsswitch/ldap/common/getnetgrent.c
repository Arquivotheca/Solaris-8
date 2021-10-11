/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getnetgrent.c 1.1     99/07/07 SMI"

#include <syslog.h>
#include "ldap_common.h"

/* netgroup attributes filters */
#define	_N_NAME			"cn"
#define	_N_TRIPLE		"nisnetgrouptriple"
#define	_N_MEMBER		"membernisnetgroup"

#define	ISWILD(a)		(((a).argc == 0) || ((a).argv == NULL) || \
				    ((a).argv[0] == NULL)) ? "*" : (a).argv[0]
#define	ISNULL(a)		(a == NULL ? "<NULL>" : a)
#define	_F_GETNETGRENT		\
	"(&(objectClass=nisNetGroup)(nisnetgrouptriple=(%s,%s,%s)))"

#define	_F_GETMEMBERGRENT	\
	"(&(objectClass=nisNetGroup)(|(membernisnetgroup=%s)"

#define	_F_GETMEMBER		"(membernisnetgroup=%s)"
#define	_F_SETMEMBER		"(&(objectClass=nisNetGroup)(cn=%s))"
#define	_F_SETGRMEMBERENT	"(&(objectClass=nisNetGroup)(|(cn=%s)"
#define	_F_SETGRMEMBERENTSIMPLE	"(&(objectClass=nisNetGroup)(cn=%s)"
#define	_F_SETGRMEMBER		"(cn=%s)"



/* netgroup constants */
#define	_DEPTH_FIRST_SEARCH	0
#define	_BREADTH_FIRST_SEARCH	1

static const char *netgr_attrs[] = {
	_N_NAME,
	_N_TRIPLE,
	_N_MEMBER,
	(char *)NULL
};

struct line_buf {
	char *str;
	int len;
	int alloc;
};

static struct netgroupnam	*_ngt_next(struct netgrouptab *ngt);
static void			_ngt_insert(struct netgrouptab *ngt,
				    const char *groups, size_t len,
				    int search);
static void			_ngt_init(struct netgrouptab *ngt);



static struct netgroupnam *
_ngt_next(struct netgrouptab *ngt)
{
	struct netgroupnam *first = (struct netgroupnam *)NULL;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: _ngt_next]\n");
#endif	DEBUG

	if ((first = ngt->nt_first) == 0) {
		return (NULL);
	}
	if ((ngt->nt_first = first->ng_next) == 0)
		ngt->nt_last = &ngt->nt_first;
	ngt->nt_total--;

	return (first);
}


/*
 *
 */

static void
_ngt_insert(struct netgrouptab *ngt, const char *groups,
		size_t len, int search)
{

	struct netgroupnam	*cptr = (struct netgroupnam *)NULL;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: _ngt_insert]\n");
#endif	DEBUG

	if (len < 1) {
		return;
	}

	/* create new struct netgroupnam */

#define	dummy	((struct netgroupnam *)0)

	if ((cptr = (struct netgroupnam *)
	    malloc(len + 1 + (char *)&dummy->ng_name[0] -
	    (char *)dummy)) == 0)
		return;

	(void) memcpy(cptr->ng_name, groups, len);
	cptr->ng_name[len] = 0;

	/* insert in list at start (depth-first) or end (breadth-first) */

	if (search == _BREADTH_FIRST_SEARCH || ngt->nt_first == 0) {
		cptr->ng_next = 0;
		*ngt->nt_last = cptr;
		ngt->nt_last = &cptr->ng_next;
	} else {
		cptr->ng_next = ngt->nt_first;
		ngt->nt_first = cptr;
	}
	ngt->nt_total++;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: _ngt_insert]\n");
	(void) fprintf(stdout, "\tnt_total: %d\n", ngt->nt_total);
#endif	DEBUG
}


/*
 *
 */
static void
_ngt_destroy_netgroupnam(struct netgroupnam *ptr)
{

	if (ptr != NULL) {
		free(ptr);
	}
}


/*
 *
 */

static void
_ngt_init(struct netgrouptab *ngt)
{

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: _ngt_init]\n");
#endif	DEBUG

	(void) memset((void *)ngt, 0, sizeof (*ngt));
	ngt->nt_first = NULL;
	ngt->nt_last = &ngt->nt_first;
}


/*
 *
 *
 */
static void
_ngt_destroy_table(struct netgrouptab *ngt) {
	struct netgroupnam	*netgroup;

	while ((netgroup = _ngt_next(ngt)) != NULL) {
#ifdef DEBUG
		(void) fprintf(stdout, "\n[getnetgrent.c: "
			    "_ngt_destroy_table]\n");
#endif DEBUG
		_ngt_destroy_netgroupnam(netgroup);
	}
	_ngt_init(ngt);
}


static int
_process_in_entries(ns_ldap_result_t *result, struct netgrouptab *table,
		    char *group)
{
	int		i;
	ns_ldap_attr_t	*attrptr;
	ns_ldap_entry_t	*entry;

#ifdef	DEBUG
		(void) fprintf(stdout,
			"\n[getnetgrent.c: process_In_entries]\n");
#endif	DEBUG

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		return (-1);
	}
	for (entry = result->entry; entry != NULL; entry = entry->next) {
	    for (i = 0; i < entry->attr_count; i++) {
		attrptr = entry->attr_pair[i];
		if (attrptr == NULL) {
		    return (-1);
		}
		if (strcasecmp(attrptr->attrname, _N_NAME) == 0) {
		    if (strcasecmp(attrptr->attrvalue[0], group) == 0) {
			return (1);
		    } else {
			(void) _ngt_insert(table, attrptr->attrvalue[0],
				(int)strlen(attrptr->attrvalue[0]),
				_BREADTH_FIRST_SEARCH);
		    }
		    continue;
		}
		if (strcasecmp(attrptr->attrname, _N_TRIPLE) == 0) {
		    continue;
		}
		if (strcasecmp(attrptr->attrname, _N_MEMBER) == 0) {
		    continue;
		}
	    }
	}

	return (0);
}


static int
_process_set_entries(ns_ldap_result_t *result, struct netgrouptab *lookuptable,
			struct netgrouptab *memberlist)
{
	int		i, j;
	ns_ldap_attr_t	*attrptr;
	ns_ldap_entry_t	*entry;

#ifdef	DEBUG
		(void) fprintf(stdout,
			"\n[getnetgrent.c: process_set_entries]\n");
#endif	DEBUG

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		return (-1);
	}
	for (entry = result->entry; entry != NULL; entry = entry->next) {
	    for (i = 0; i < entry->attr_count; i++) {
		attrptr = entry->attr_pair[i];
		if (attrptr == NULL) {
		    return (-1);
		}
		if (strcasecmp(attrptr->attrname, _N_NAME) == 0) {
		    continue;
		}
		if (strcasecmp(attrptr->attrname, _N_TRIPLE) == 0) {
		    (void) _ngt_insert(memberlist,
		    attrptr->attrvalue[0], (int)strlen(attrptr->attrvalue[0]),
			_BREADTH_FIRST_SEARCH);
		    continue;
		}
		if (strcasecmp(attrptr->attrname, _N_MEMBER) == 0) {
		    for (j = 0; j < attrptr->value_count; j++) {
			(void) _ngt_insert(lookuptable, attrptr->attrvalue[j],
			    (int)strlen(attrptr->attrvalue[j]),
			    _BREADTH_FIRST_SEARCH);
		    }
		    continue;
		}
	    }
	}

	return (0);
}


static int
_print2buf(struct line_buf *line, char *toprint)
{
	int	toprintlen = 0;
	char	*str;

	if (line == NULL)
		return (-1);

	/* has print buffer line been exhausted */
	if ((toprintlen = strlen(toprint)) + line->len > (line->alloc - 1)) {
		do {
			if (line->alloc == 0) {
				str = (char *)calloc(BUFSIZ, sizeof (char));
			} else {
				str = (char *)realloc(line->str,
						((line->alloc) + BUFSIZ));
			}

			/*
			 * caller should note that we are freeing memory here
			 * itself in case of failure??
			 */

			if (str == NULL) {
				syslog(LOG_ERR, "print2buf:  out of memory");
				if (line->str)
					free(line->str);
				line->str = NULL;
				line->alloc = 0;
				line->len = 0;
				return (-1);
			}
			line->str = str;
			line->alloc += BUFSIZ;

		} while (toprintlen > line->alloc);
	}
	/* now add new 'toprint' data to buffer */
	(void) strcat(line->str, toprint);
	line->len += toprintlen;
	return (0);
}

static void
_print2buf_init(struct line_buf *line)
{
	(void) memset((char *)line, 0, sizeof (*line));
	line->str = NULL;
	line->alloc = 0;
	line->len = 0;
}

static void
_print2buf_destroy(struct line_buf *line)
{
	free(line->str);
	line->str = NULL;
	line->alloc = 0;
	line->len = 0;
}

/*
 * domain comparing routine
 * 	n1: See if n1 is n2 or an ancestor of it
 * 	n2: (in string terms, n1 is a suffix of n2)
 * Returns ZERO for success, -1 for failure.
 */
static int
domcmp(const char *n1, const char *n2)
{
#define	PASS	0
#define	FAIL	-1

	size_t		l1, l2;

	if ((n1 == NULL) || (n2 == NULL))
		return (FAIL);

	l1 = strlen(n1);
	l2 = strlen(n2);

	/* Turn a blind eye to the presence or absence of trailing periods */
	if (l1 != 0 && n1[l1 - 1] == '.') {
		--l1;
	}
	if (l2 != 0 && n2[l2 - 1] == '.') {
		--l2;
	}
	if (l1 > l2) {		/* Can't be a suffix */
		return (FAIL);
	} else if (l1 == 0) {	/* Trivially a suffix; */
				/* (do we want this case?) */
		return (PASS);
	}
	/* So 0 < l1 <= l2 */
	if (l1 < l2 && n2[l2 - l1 - 1] != '.') {
		return (FAIL);
	}
	if (strncasecmp(n1, &n2[l2 - l1], l1) == 0) {
		return (PASS);
	} else {
		return (FAIL);
	}
}


static int split_triple(char *, char **, char **, char **);

/*
 * check the domain part of the triples.
 *	-1 = fails to match, 0 = match
 */
static int
match_domain_triple(struct nss_innetgr_args *ia, ns_ldap_result_t *result)
{
	int	ndomains, ndomains_init;
	char	**pdomains, **pdomains_init;
	char	**attr;
	char	*triple = NULL;
	char	*tuser, *thost, *tdomain;
	ns_ldap_entry_t	*entry;

	if ((ndomains_init = ia->arg[NSS_NETGR_DOMAIN].argc) == 0)
		/* assume wildard */
		return (0);
	pdomains_init = (char **)ia->arg[NSS_NETGR_DOMAIN].argv;
	if (pdomains_init == NULL || *pdomains_init == NULL)
		/* assume wildard */
		return (0);

	for (entry = result->entry; entry != NULL; entry = entry->next) {
		attr = __ns_ldap_getAttr(entry, _N_TRIPLE);
		if (attr == NULL || *attr == NULL)
			continue;
		while (*attr) {
			pdomains = pdomains_init;
			ndomains = ndomains_init;
			if ((triple = strdup(*attr)) == NULL)
				continue;
			if (split_triple(triple, &thost, &tuser, &tdomain)
							!= 0) {
				free(triple);
				continue;
			}
			if ((tdomain == NULL) || (strlen(tdomain)) == 0) {
				/* assume wildcard */
				free(triple);
				return (0);
			}
			do {
				if (domcmp(tdomain, *pdomains++) == 0) {
					/* matched */
					free(triple);
					return (0);
				}
			} while (--ndomains != 0);
			attr++;
			free(triple);
		}
	}
	return (-1);
}


static nss_status_t __netgr_in(ldap_backend_ptr, void *, char *);

static nss_status_t
netgr_in(ldap_backend_ptr be, void *a)
{
	struct nss_innetgr_args	*ia = (struct nss_innetgr_args *)a;
	int	i;
	nss_status_t	rc = (nss_status_t)NSS_NOTFOUND;

	ia->status = NSS_NETGR_NO;
	for (i = 0; i < ia->groups.argc; i++) {
		if ((rc = __netgr_in(be, a, ia->groups.argv[i]))
					== NSS_SUCCESS) {
			return (rc);
		}
	}
	return (rc);
}

/*
 * __netgr_in checks only checks the netgroup specified in ngroup
 */
static nss_status_t
__netgr_in(ldap_backend_ptr be, void *a, char *netgrname)
{
	struct nss_innetgr_args	*ia = (struct nss_innetgr_args *)a;
	char			searchfilter[SEARCHFILTERLEN];
	struct netgrouptab	table;
	struct netgroupnam	*netgroup;
	int			first;
	static struct line_buf  searchstring;
	nss_status_t		status;
	char			name[80];
	nss_XbyY_args_t		argb;
	static nss_XbyY_buf_t	*nb;


#ifdef DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: netgr_in]\n");
	(void) fprintf(stdout, "\tmachine: argc[%d]='%s' user: "
			    "argc[%d]='%s',\n\tdomain:argc[%d]='%s' "
			    "netgroup: argc[%d]='%s'\n",
			    NSS_NETGR_MACHINE,
			    ISWILD(ia->arg[NSS_NETGR_MACHINE]),
			    NSS_NETGR_USER,
			    ISWILD(ia->arg[NSS_NETGR_USER]),
			    NSS_NETGR_DOMAIN,
			    ISWILD(ia->arg[NSS_NETGR_DOMAIN]),
			    NSS_NETGR_N,
			    ISWILD(ia->arg[NSS_NETGR_N]));
	(void) fprintf(stdout, "\tgroups='%s'\n", netgrname);
#endif DEBUG

	ia->status = NSS_NETGR_NO;

	if (snprintf(searchfilter, SEARCHFILTERLEN, _F_GETNETGRENT,
		    ISWILD(ia->arg[NSS_NETGR_MACHINE]),
		    ISWILD(ia->arg[NSS_NETGR_USER]),
		    "*") < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	NSS_XbyY_ALLOC(&nb, sizeof (struct nss_getnetgrent_args),
				NSS_BUFLEN_NETGROUP);
	NSS_XbyY_INIT(&argb, nb->result, nb->buffer, nb->buflen, 0);


	status = (nss_status_t)_nss_ldap_nocb_lookup(be, &argb,
		_NETGROUP, searchfilter, NULL);

	if (status != (nss_status_t)NS_LDAP_SUCCESS)
		return ((nss_status_t)status);

	(void) _ngt_init(&table);

	if (match_domain_triple(ia, be->result) == -1) {
		/* domaint part of triple not matching */
		return ((nss_status_t)NSS_NOTFOUND);
	}

	switch (_process_in_entries(be->result, &table, netgrname)) {
		case	1:
			/* found */
				(void) __ns_ldap_freeResult(&be->result);
				ia->status = NSS_NETGR_FOUND;
				return ((nss_status_t)NSS_SUCCESS);
		case	-1:
			/* error */
				(void) __ns_ldap_freeResult(&be->result);
				return ((nss_status_t)NSS_NOTFOUND);
		default	:
			/* continue */
				break;
	}

	do {
		first = 0;
		while ((netgroup = _ngt_next(&table)) != NULL) {
			(void) strcpy(name, netgroup->ng_name);
			_ngt_destroy_netgroupnam(netgroup);
			if (first == 0) {
				first = 1;
				_print2buf_init(&searchstring);
				if (snprintf(searchfilter, SEARCHFILTERLEN,
				    _F_GETMEMBERGRENT, name) < 0)
					return ((nss_status_t)NSS_NOTFOUND);
			} else {
				if (snprintf(searchfilter, SEARCHFILTERLEN,
				    _F_GETMEMBER, name) < 0)
					return ((nss_status_t)NSS_NOTFOUND);
			}

			if (_print2buf(&searchstring, searchfilter) != 0) {
				return ((nss_status_t)NSS_NOTFOUND);
			}
		}

		if (_print2buf(&searchstring, "))") != 0) {
				return ((nss_status_t)NSS_NOTFOUND);
		}

		status = (nss_status_t)_nss_ldap_nocb_lookup(be, &argb,
			_NETGROUP, searchstring.str, NULL);
		(void) _print2buf_destroy(&searchstring);
		if (status != (nss_status_t)NS_LDAP_SUCCESS) {
			return ((nss_status_t)status);
		}
		switch (_process_in_entries(be->result, &table,
			    netgrname)) {
			case	1:
				(void) __ns_ldap_freeResult(&be->result);
				ia->status = NSS_NETGR_FOUND;
				return ((nss_status_t)NSS_SUCCESS);
			case	-1:
				(void) __ns_ldap_freeResult(&be->result);
				return ((nss_status_t)NSS_NOTFOUND);
			default	:
				(void) __ns_ldap_freeResult(&be->result);
				break;
		}
	/*CONSTANTCONDITION*/
	} while (1);
	return ((nss_status_t)NSS_NOTFOUND);
}


/*
 *
 */

static nss_status_t
getnetgr_ldap_setent(ldap_backend_ptr be, void *a)
{
	struct nss_setnetgrent_args	*args =
				(struct nss_setnetgrent_args *) a;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: getnetgr_ldap_setent]\n");
#endif	DEBUG

	if (be->netgroup != NULL) {
		/* is this another set on the same netgroup */
		if (strcasecmp(be->netgroup, args->netgroup) == 0) {
			be->next_member = be->all_members.nt_first;
			return ((nss_status_t)NSS_SUCCESS);
		}
	}

	return (NSS_NOTFOUND);
}


/*ARGSUSED1*/
static nss_status_t
getnetgr_ldap_endent(ldap_backend_ptr be, void *a)
{

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: getnetgr_ldap_endent]\n");
#endif	DEBUG

	be->next_member = NULL;
	if (be->netgroup != NULL) {
		free(be->netgroup);
		be->netgroup = NULL;
	}

	return ((nss_status_t)NSS_NOTFOUND);
}


/*ARGSUSED1*/
static nss_status_t
getnetgr_ldap_destr(ldap_backend_ptr be, void *a)
{

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: getnetgr_ldap_destr]\n");
#endif	DEBUG

	_ngt_destroy_table(&be->all_members);
	(void) _clean_ldap_backend(be);
	return ((nss_status_t)NSS_NOTFOUND);
}

static int
split_triple(char *triple, char **hostname, char **username, char **domain)
{
	int	i, syntax_err;
	char	*splittriple[3];
	char	*p = triple;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: split_triple]\n");
#endif	DEBUG

	if (triple == NULL)
		return (-1);

	p++;
	syntax_err = 0;
	for (i = 0; i < 3; i++) {
		char	*start;
		char	*limit;
		const char	*terminators = ",) \t";

		if (i == 2) {
			/* Don't allow comma */
			terminators++;
		}
		while (isspace(*p)) {
			p++;
		}
		start = p;
		limit = strpbrk(start, terminators);
		if (limit == 0) {
			syntax_err++;
			break;
		}
		p = limit;
		while (isspace(*p)) {
			p++;
		}
		if (*p == terminators[0]) {
			/*
			 * Successfully parsed this name and
			 * the separator after it (comma or
			 * right paren); leave p ready for
			 * next parse.
			 */
			p++;
			if (start == limit) {
				/* Wildcard */
				splittriple[i] = 0;
			} else {
				*limit = '\0';
				splittriple[i] = start;
			}
		} else {
			syntax_err++;
			break;
		}
	}

	if (syntax_err != 0)
		return (-1);

	*hostname = splittriple[0];
	*username = splittriple[1];
	*domain = splittriple[2];

	return (0);
}


static nss_status_t
getnetgr_ldap_getent(ldap_backend_ptr be, void *a)
{
	struct nss_getnetgrent_args *args = (struct nss_getnetgrent_args *) a;
	struct netgroupnam		*netgroupentry;

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: getnetgr_ldap_getent]\n");
#endif	DEBUG
	if ((netgroupentry = be->next_member) == NULL) {
		args->status = NSS_NETGR_NO;
	} else {
		char	*buffer = args->buffer;
		int	buflen = args->buflen;
		int	len;
		char	*hostname, *username, *domain;
		char	*tosplit = NULL;

		/* initialize as to not return garbage */
		args->retp[NSS_NETGR_MACHINE] = NULL;
		args->retp[NSS_NETGR_USER] = NULL;
		args->retp[NSS_NETGR_DOMAIN] = NULL;
		tosplit = strdup(netgroupentry->ng_name);
		if (split_triple(tosplit, &hostname, &username,
			&domain) < 0) {
			args->status = NSS_NETGR_NO;
			free(tosplit);
			tosplit = NULL;
			return ((nss_status_t)args->status);
		}
		if (hostname != NULL) {
			args->retp[NSS_NETGR_MACHINE] = buffer;
			len = strlen(hostname) + 1;
			(void) memcpy(buffer, hostname, len);
			buffer += len;
			buflen -= len;
			args->status = NSS_NETGR_FOUND;
		}
		if (username != NULL) {
			args->retp[NSS_NETGR_USER] = buffer;
			len = strlen(username) + 1;
			(void) memcpy(buffer, username, len);
			buffer += len;
			buflen -= len;
			args->status = NSS_NETGR_FOUND;
		}
		if (domain != NULL) {
			args->retp[NSS_NETGR_DOMAIN] = buffer;
			len = strlen(domain) + 1;
			(void) memcpy(buffer, domain, len);
			buffer += len;
			buflen -= len;
			args->status = NSS_NETGR_FOUND;
		}

		free(tosplit);
		tosplit = NULL;
		be->next_member = netgroupentry->ng_next;
	}
	return ((nss_status_t)NSS_SUCCESS);
}


static ldap_backend_op_t getnetgroup_ops[] = {
	getnetgr_ldap_destr,
	getnetgr_ldap_endent,
	getnetgr_ldap_setent,
	getnetgr_ldap_getent,
};

/*
 *
 */

static nss_status_t
netgr_set(ldap_backend_ptr be, void *a)
{
	struct nss_setnetgrent_args	*args =
				(struct nss_setnetgrent_args *) a;
	char				searchfilter[NETGROUPFILTERLEN];
	struct netgrouptab		lookuptable;
	struct netgroupnam		*netgroup;
	int				first;
	static struct line_buf		searchstring;
	nss_status_t			status;
	char				name[80];
	nss_XbyY_args_t			argb;
	static nss_XbyY_buf_t		*nb;
	ldap_backend_ptr		get_be;
	int				totalmembers;
	int				res;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: netgr_set]\n");
	(void) fprintf(stdout,
		"\targs->netgroup: %s\n", ISNULL(args->netgroup));
#endif DEBUG

	if (args->netgroup == NULL)
		return ((nss_status_t)NSS_NOTFOUND);

	be->netgroup = strdup(args->netgroup);
	be->next_member = NULL;
	(void) memset(searchfilter, 0, sizeof (searchfilter));
	if (snprintf(searchfilter, NETGROUPFILTERLEN,
		_F_SETMEMBER, be->netgroup) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	NSS_XbyY_ALLOC(&nb, sizeof (struct nss_getnetgrent_args),
				NSS_BUFLEN_NETGROUP);
	NSS_XbyY_INIT(&argb, nb->result, nb->buffer, nb->buflen, 0);

	status = (nss_status_t)_nss_ldap_nocb_lookup(be, &argb,
		_NETGROUP, searchfilter, NULL);

	if (status != (nss_status_t)NS_LDAP_SUCCESS)
		return ((nss_status_t)status);

	(void) _ngt_init(&lookuptable);
	(void) _ngt_init(&be->all_members);

	if (_process_set_entries(be->result, &lookuptable,
				    &be->all_members) == -1) {
		(void) __ns_ldap_freeResult(&be->result);
		return ((nss_status_t)NSS_NOTFOUND);
	}

	do {
		first = 0;
		totalmembers = lookuptable.nt_total;
		while ((netgroup = _ngt_next(&lookuptable)) != NULL) {
			(void) strcpy(name, netgroup->ng_name);
			_ngt_destroy_netgroupnam(netgroup);
			if (first == 0) {
				first = 1;
				_print2buf_init(&searchstring);
				if (totalmembers > 1) {
					if (snprintf(searchfilter,
					    NETGROUPFILTERLEN,
					    _F_SETGRMEMBERENT, name) < 0)
					    return ((nss_status_t)NSS_NOTFOUND);
				} else {
					if (snprintf(searchfilter,
					    NETGROUPFILTERLEN,
					    _F_SETGRMEMBERENTSIMPLE, name) < 0)
					    return ((nss_status_t)NSS_NOTFOUND);
				}
			} else {
				if (snprintf(searchfilter,
				    NETGROUPFILTERLEN,
				    _F_SETGRMEMBER, name) < 0)
					return ((nss_status_t)NSS_NOTFOUND);
			}

			if (_print2buf(&searchstring, searchfilter) != 0) {
				return ((nss_status_t)NSS_NOTFOUND);
			}
		}

		if (totalmembers > 1) {
			res = _print2buf(&searchstring, "))");
		} else {
			res = _print2buf(&searchstring, ")");
		}
		if (res != 0) {
				return ((nss_status_t)NSS_NOTFOUND);
		}

		status = (nss_status_t)_nss_ldap_nocb_lookup(be, &argb,
			_NETGROUP, searchstring.str, NULL);

		(void) _print2buf_destroy(&searchstring);

		if (status != (nss_status_t)NS_LDAP_SUCCESS) {
			return ((nss_status_t)status);
		}


		if (_process_set_entries(be->result, &lookuptable,
			    &be->all_members) == -1) {
			(void) __ns_ldap_freeResult(&be->result);
			return ((nss_status_t)NSS_NOTFOUND);
		}
	} while (lookuptable.nt_total > 0);

	/* now allocate and return iteration backend structure */
	if ((get_be = (ldap_backend_ptr) malloc(sizeof (*get_be))) == 0)
		return (NSS_UNAVAIL);
	get_be->ops = getnetgroup_ops;
	get_be->nops = sizeof (getnetgroup_ops) / sizeof (getnetgroup_ops[0]);
	get_be->tablename = strdup(_NETGROUP);
	get_be->attrs = netgr_attrs;
	get_be->result = NULL;
	get_be->ldapobj2ent = NULL;
	get_be->setcalled = 1;
	get_be->filter = NULL;
	get_be->enumcookie = NULL;
	get_be->netgroup = strdup(be->netgroup);
	(void) memcpy((void *)&get_be->all_members,
		(void *)&be->all_members, sizeof (struct netgrouptab));
	get_be->next_member = be->all_members.nt_first;
	args->iterator = (nss_backend_t *) get_be;

	(void) __ns_ldap_freeResult(&be->result);

	return (NSS_SUCCESS);
}


/*ARGSUSED1*/
static nss_status_t
netgr_ldap_destr(ldap_backend_ptr be, void *a)
{

#ifdef	DEBUG
	(void) fprintf(stdout, "\n[getnetgrent.c: netgr_ldap_destr]\n");
#endif	DEBUG

	if (be->netgroup != NULL) {
		free(be->netgroup);
		be->netgroup = NULL;
	}

	(void) _clean_ldap_backend(be);

	return ((nss_status_t)NSS_NOTFOUND);
}




static ldap_backend_op_t netgroup_ops[] = {
	netgr_ldap_destr,
	0,
	0,
	0,
	netgr_in,		/*	innetgr()	*/
	netgr_set		/*	setnetgrent()	*/
};


/*
 * _nss_ldap_netgroup_constr is where life begins. This function calls the
 * generic ldap constructor function to define and build the abstract data
 * types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_netgroup_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

#ifdef	DEBUG
	(void) fprintf(stdout,
		    "\n[getnetgrent.c: _nss_ldap_netgroup_constr]\n");
#endif	DEBUG

	return ((nss_backend_t *)_nss_ldap_constr(netgroup_ops,
		sizeof (netgroup_ops)/sizeof (netgroup_ops[0]), _NETGROUP,
		netgr_attrs, NULL));
}
