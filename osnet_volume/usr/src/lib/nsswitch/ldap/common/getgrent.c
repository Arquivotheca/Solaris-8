/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getgrent.c 1.2     99/10/22 SMI"

#include <grp.h>
#include "ldap_common.h"

/* Group attributes filters */
#define	_G_NAME		"cn"
#define	_G_GID		"gidnumber"
#define	_G_PASSWD	"userpassword"
#define	_G_MEM		"memberuid"

#define	_F_GETGRNAM	"(&(objectClass=posixGroup)(cn=%s))"
#define	_F_GETGRGID	"(&(objectClass=posixGroup)(gidNumber=%ld))"
#define	_F_GETGRMEM	"(&(objectClass=posixGroup)(memberUid=%s))"

static const char *gr_attrs[] = {
	_G_NAME,
	_G_GID,
	_G_PASSWD,
	_G_MEM,
	(char *)NULL
};


/*
 * _nss_ldap_group2ent is the data marshaling method for the group getXbyY
 * (e.g., getgrnam(), getgrgid(), getgrent()) backend processes. This method
 * is called after a successful ldap search has been performed. This method
 * will parse the ldap search values into struct group = argp->buf.buffer
 * which the frontend process expects. Three error conditions are expected
 * and returned to nsswitch.
 */

static int
_nss_ldap_group2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i, j;
	int		nss_result;
	int		buflen = (int)0;
	int		firstime = (int)1;
	unsigned long	len = 0L;
	char		**mp = NULL;
	char		*val = (char *)NULL;
	char		*buffer = (char *)NULL;
	char		*ceiling = (char *)NULL;
	struct group	*grp = (struct group *)NULL;
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_grp2ent;
	}
	grp = (struct group *)argp->buf.result;
	ceiling = buffer + buflen;
	mp = grp->gr_mem = (char **)NULL;

	/* initialize no group password */
	grp->gr_passwd = (char *)NULL;
	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(argp->buf.buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_grp2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_grp2ent;
		}
		if (strcasecmp(attrptr->attrname, _G_NAME) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_grp2ent;
			}
			grp->gr_name = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_grp2ent;
			}
			(void) strcpy(grp->gr_name, attrptr->attrvalue[0]);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _G_PASSWD) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len == 0 || (attrptr->attrvalue[0] == '\0')) {
				len = 1; val = "";
			} else {
				val = attrptr->attrvalue[0];
			}
			grp->gr_passwd = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_grp2ent;
			}
			(void) strcpy(grp->gr_passwd, val);
			continue;
		}
		if (strcasecmp(attrptr->attrname, _G_GID) == 0) {
			if (strlen(attrptr->attrvalue[0]) == 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_grp2ent;
			}
			errno = 0;
			grp->gr_gid = (gid_t)strtol(attrptr->attrvalue[0],
						    (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_grp2ent;
			}
			continue;
		}
		if (strcasecmp(attrptr->attrname, _G_MEM) == 0) {
			for (j = 0; j < attrptr->value_count; j++) {
				if (firstime) {
					mp = grp->gr_mem =
						    (char **)ROUND_UP(buffer,
						    sizeof (char **));
					buffer = (char *)grp->gr_mem +
						    sizeof (char *) *
						    (attrptr->value_count + 1);
					buffer = (char *)ROUND_UP(buffer,
						    sizeof (char **));
					if (buffer >= ceiling) {
						nss_result =
						    (int)NSS_STR_PARSE_ERANGE;
						goto result_grp2ent;
					}
					firstime = (int)0;
				}
				len = strlen(attrptr->attrvalue[j]);
				if (len < 1 ||
				    (attrptr->attrvalue[j] == '\0')) {
					nss_result = (int)NSS_STR_PARSE_PARSE;
					goto result_grp2ent;
				}
				*mp = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_grp2ent;
				}
				(void) strcpy(*mp++, attrptr->attrvalue[j]);
				continue;
			}
		}
	}
	if (mp != NULL)
		*mp = NULL;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getgrent.c: _nss_ldap_group2ent]\n");
	(void) fprintf(stdout, "       gr_name: [%s]\n", grp->gr_name);
	if (grp->gr_passwd != (char *)NULL)
		(void) fprintf(stdout, "     gr_passwd: [%s]\n",
			    grp->gr_passwd);
	(void) fprintf(stdout, "        gr_gid: [%ld]\n", grp->gr_gid);
	if (mp != NULL) {
		for (mp = grp->gr_mem; *mp != NULL; mp++)
			(void) fprintf(stdout, "        gr_mem: [%s]\n", *mp);
	}
#endif DEBUG

result_grp2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


/*
 * getbynam gets a group entry by name. This function constructs an ldap
 * search filter using the name invocation parameter and the getgrnam search
 * filter defined. Once the filter is constructed, we searche for a matching
 * entry and marshal the data results into struct group for the frontend
 * process. The function _nss_ldap_group2ent performs the data marshaling.
 */

static nss_status_t
getbynam(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getgrent.c: getbyname]\n");
#endif DBEUG
	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETGRNAM, argp->key.name) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_GROUP, searchfilter, NULL));
}


/*
 * getbygid gets a group entry by number. This function constructs an ldap
 * search filter using the name invocation parameter and the getgrgid search
 * filter defined. Once the filter is constructed, we searche for a matching
 * entry and marshal the data results into struct group for the frontend
 * process. The function _nss_ldap_group2ent performs the data marshaling.
 */

static nss_status_t
getbygid(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char searchfilter[SEARCHFILTERLEN];

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getgrent.c: getbygid]\n");
#endif DBEUG
	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETGRGID, (long)argp->key.uid) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_GROUP, searchfilter, NULL));
}


/*
 * getbymember returns all groups a user is defined in. This function
 * uses different architectural procedures than the other group backend
 * system calls because it's a private interface. This function constructs
 * an ldap search filter using the name invocation parameter. Once the
 * filter is constructed, we search for all matching groups counting
 * and storing each group name, gid, etc. Data marshaling is used for
 * group processing. The function _nss_ldap_group2ent() performs the
 * data marshaling.
 *
 * (const char *)argp->username;	(size_t)strlen(argp->username);
 * (gid_t)argp->gid_array;		(int)argp->maxgids;
 * (int)argp->numgids;
 */

static nss_status_t
getbymember(ldap_backend_ptr be, void *a)
{
	int			i, j, k;
	int			gcnt = (int)0;
	char			**groupvalue, **membervalue;
	nss_status_t		lstat;
	nss_XbyY_args_t		argb;
	static nss_XbyY_buf_t	*gb;
	struct nss_groupsbymem	*argp = (struct nss_groupsbymem *)a;
	char			searchfilter[SEARCHFILTERLEN];
	ns_ldap_result_t	*result;
	ns_ldap_entry_t		*curEntry;
	char			*username;
	gid_t			gid;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getgrent.c: getbymember]\n");
#endif DBEUG

	NSS_XbyY_ALLOC(&gb, sizeof (struct group), NSS_BUFLEN_GROUP);
	NSS_XbyY_INIT(&argb, gb->result, gb->buffer, gb->buflen, 0);

	if (strcmp(argp->username, "") == 0 ||
	    strcmp(argp->username, "root") == 0)
		return ((nss_status_t)NSS_NOTFOUND);

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETGRMEM, argp->username) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	gcnt = (int)argp->numgids;
	lstat = (nss_status_t)_nss_ldap_nocb_lookup(be, &argb,
		_GROUP, searchfilter, NULL);
	if (lstat != (nss_status_t)NS_LDAP_SUCCESS)
		return ((nss_status_t)lstat);
	if (be->result == NULL)
		return (NSS_NOTFOUND);
	username = (char *)argp->username;
	result = (ns_ldap_result_t *)be->result;
	curEntry = (ns_ldap_entry_t *)result->entry;
	for (i = 0; i < result->entries_count; i++) {
		membervalue = __ns_ldap_getAttr(curEntry, "memberUid");
		if (membervalue) {
			for (j = 0; membervalue[j]; j++) {
				if (strcmp(membervalue[j], username) == NULL) {
					groupvalue = __ns_ldap_getAttr(curEntry,
						"gidnumber");
					gid = (gid_t)strtol(groupvalue[0],
						(char **)NULL, 10);
					if (argp->numgids < argp->maxgids) {
					    for (k = 0; k < argp->numgids;
							k++) {
						if (argp->gid_array[k] == gid)
						    /* already exists */
						    break;
					    }
					    if (k == argp->numgids)
						argp->gid_array[argp->numgids++]
						    = gid;
					}
					break;
				}
			}
		}
		curEntry = curEntry->next;
	}

	__ns_ldap_freeResult((ns_ldap_result_t **)&be->result);
	NSS_XbyY_FREE(&gb);
	if (gcnt == argp->numgids)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)NSS_SUCCESS);
}

static ldap_backend_op_t gr_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbynam,
	getbygid,
	getbymember
};


/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_group_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

	return ((nss_backend_t *)_nss_ldap_constr(gr_ops,
		sizeof (gr_ops)/sizeof (gr_ops[0]), _GROUP, gr_attrs,
		_nss_ldap_group2ent));
}
