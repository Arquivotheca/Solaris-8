/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_writes.c	1.4	99/04/01 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <libintl.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lber.h>
#include <ldap.h>

#include "ns_sldap.h"
#include "ns_internal.h"

static void
free_mods(LDAPMod **mods)
{
	LDAPMod **ptr = mods;

	while (*ptr) {
		free(*ptr);
		ptr++;
	}
	free(mods);
}


/*ARGSUSED*/
int
__ns_ldap_addAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	LDAPMod		**mods;
	LDAP		*ld;
	char		**servers = NULL;
	char		errstr[MAXERROR];
	int		rc = 0;
	int		count = 0;
	int		i = 0;
	ns_ldap_attr_t	**aptr = (ns_ldap_attr_t **)attr;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_addAttr START\n");
#endif
	*errorp = NULL;

	/* Sanity check */
	if ((attr == NULL) || (*attr == NULL) ||
		(dn == NULL) || (cred == NULL))
			return (NS_LDAP_INVALID_PARAM);
	/* count number of attributes */
	while (*aptr++)
		count++;

	mods = (LDAPMod **)calloc((count + 1), sizeof (LDAPMod *));
	if (mods == NULL) {
		return (NS_LDAP_MEMORY);
	}
	for (i = 0; i < count && attr[i] != NULL; i++) {
		if ((mods[i] = (LDAPMod *)malloc(sizeof (LDAPMod))) == NULL) {
			free_mods(mods);
			return (NS_LDAP_MEMORY);
		}

		mods[i]->mod_op = LDAP_MOD_ADD;
		mods[i]->mod_type = attr[i]->attrname;
		mods[i]->mod_values = attr[i]->attrvalue;
	}

	mods[count] = NULL;

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		goto cleanup;
	}

	rc = __s_api_getConnection(servers, 0, cred,
			&ld, &connectionId, errorp);

	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;

	/* Perform the modfiy operation. */
	rc = ldap_modify_ext_s(ld, (char *)dn, mods, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
		int	errno;
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &errno);
		sprintf(errstr, gettext(ldap_err2string(errno)));
		MKERROR(*errorp, errno, errstr, NULL);
		rc = NS_LDAP_INTERNAL;
		goto cleanup;
	}
	rc = NS_LDAP_SUCCESS;
cleanup:
	free_mods(mods);
	if (servers)
		__s_api_free2dArray(servers);
	if (connectionId > -1)
		DropConnection(connectionId, 0);
	return (rc);
}


/*ARGSUSED*/
int
__ns_ldap_delAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	LDAPMod		**mods;
	LDAP		*ld;
	char		**servers = NULL;
	char		errstr[MAXERROR];
	int		rc = 0;
	int		i = 0;
	int		count = 0;
	ns_ldap_attr_t	**aptr = (ns_ldap_attr_t **)attr;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_delAttr START\n");
#endif
	*errorp = NULL;

	/* Sanity check */
	if ((attr == NULL) || (*attr == NULL) ||
		(dn == NULL) || (cred == NULL))
			return (NS_LDAP_INVALID_PARAM);
	/* count number of attributes */
	while (*aptr++)
		count++;

	mods = (LDAPMod **)calloc((count + 1), sizeof (LDAPMod *));
	if (mods == NULL) {
		return (NS_LDAP_MEMORY);
	}
	for (i = 0; i < count && attr[i] != NULL; i++) {
		if ((mods[i] = (LDAPMod *)malloc(sizeof (LDAPMod))) == NULL) {
			free_mods(mods);
			return (NS_LDAP_MEMORY);
		}

		mods[i]->mod_op = LDAP_MOD_DELETE;
		mods[i]->mod_type = attr[i]->attrname;
		mods[i]->mod_values = attr[i]->attrvalue;
	}

	mods[count] = NULL;

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		goto cleanup;
	}

	rc = __s_api_getConnection(servers, 0, cred,
			&ld, &connectionId, errorp);

	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;


	/* Perform the modfiy operation. */
	rc = ldap_modify_ext_s(ld, (char *)dn, mods, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
		int	errno = 0;
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &errno);
		sprintf(errstr, gettext(ldap_err2string(errno)));
		MKERROR(*errorp, errno, errstr, NULL);
		rc = NS_LDAP_INTERNAL;
		goto cleanup;
	}
	rc = NS_LDAP_SUCCESS;
cleanup:
	free_mods(mods);
	if (servers)
		__s_api_free2dArray(servers);
	if (connectionId > -1)
		DropConnection(connectionId, 0);
	return (rc);
}

/*ARGSUSED*/
int
__ns_ldap_repAttr(
	const char *dn,
	const ns_ldap_attr_t * const *attr,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	LDAPMod		**mods;
	LDAP		*ld;
	char		**servers;
	char		errstr[MAXERROR];
	int		rc = 0;
	int		i = 0;
	int		count = 0;
	ns_ldap_attr_t    **aptr = (ns_ldap_attr_t **)attr;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_repAttr START\n");
#endif
	*errorp = NULL;

	/* Sanity check */
	if ((attr == NULL) || (*attr == NULL) ||
		(dn == NULL) || (cred == NULL))
			return (NS_LDAP_INVALID_PARAM);

	/* count number of attributes */
	while (*aptr++)
		count++;

	mods = (LDAPMod **)calloc((count + 1), sizeof (LDAPMod *));
	if (mods == NULL) {
		return (NS_LDAP_MEMORY);
	}
	for (i = 0; i < count && attr[i] != NULL; i++) {
		if ((mods[i] = (LDAPMod *)malloc(sizeof (LDAPMod))) == NULL) {
			free_mods(mods);
			return (NS_LDAP_MEMORY);
		}

		mods[i]->mod_op = LDAP_MOD_REPLACE;
		mods[i]->mod_type = attr[i]->attrname;
		mods[i]->mod_values = attr[i]->attrvalue;
	}

	mods[count] = NULL;

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;

	rc = __s_api_getConnection(servers, 0, cred,
			&ld, &connectionId, errorp);
	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;

	/* Perform the modfiy operation. */
	rc = ldap_modify_ext_s(ld, (char *)dn, mods, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
		int	errno;
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &errno);
		sprintf(errstr, gettext(ldap_err2string(errno)));
		MKERROR(*errorp, errno, errstr, NULL);
		free(mods[0]);
		free(mods);
		__s_api_free2dArray(servers);
		return (NS_LDAP_INTERNAL);
	}

	rc = NS_LDAP_SUCCESS;
cleanup:
	free_mods(mods);
	if (servers)
		__s_api_free2dArray(servers);
	if (connectionId > -1)
		DropConnection(connectionId, 0);
	return (rc);
}


/*ARGSUSED*/
int
__ns_ldap_addEntry(
	const char *dn,
	const ns_ldap_entry_t *entry,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	LDAPMod		**mods = NULL;
	LDAP		*ld = NULL;
	char		**servers = NULL;
	char		errstr[MAXERROR];
	int		nAttr = 0;
	int		rc = 0;
	int		i = 0;
	int		j = 0;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_addEntry START\n");
#endif

	if ((entry == NULL) ||
		(dn == NULL) ||
		(cred == NULL))
			return (NS_LDAP_INVALID_PARAM);
	*errorp = NULL;

	/* Construct array of LDAPMod representing attributes of new entry. */

	nAttr = entry->attr_count;
	mods = (LDAPMod **)calloc((nAttr + 1), sizeof (LDAPMod *));
	if (mods == NULL) {
		return (NS_LDAP_MEMORY);
	}

	for (i = 0; i < nAttr; i++) {
		if ((mods[i] = (LDAPMod *)malloc(sizeof (LDAPMod))) == NULL) {
			for (j = 0; j < i; j++)
				free(mods[j]);
			free(mods);
			return (NS_LDAP_MEMORY);
		}
		mods[i]->mod_op = 0;
		mods[i]->mod_type = (entry->attr_pair[i]->attrname);
		mods[i]->mod_values = (entry->attr_pair[i]->attrvalue);
	}
	mods[nAttr] = NULL;

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;

	rc = __s_api_getConnection(servers, 0, cred,
			&ld, &connectionId, errorp);
	if (rc != NS_LDAP_SUCCESS)
		goto cleanup;

	/* Perform the add operation. */
	rc = ldap_add_ext_s(ld, (char *)dn, mods, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
		int	errno;
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &errno);
		sprintf(errstr, gettext(ldap_err2string(errno)));
		MKERROR(*errorp, errno, errstr, NULL);
		rc = NS_LDAP_INTERNAL;
		goto cleanup;
	}

	rc = NS_LDAP_SUCCESS;

cleanup:
	for (i = 0; i < nAttr; i++) {
		free(mods[i]);
	}
	free(mods);
	__s_api_free2dArray(servers);
	if (connectionId > -1)
		DropConnection(connectionId, 0);
	return (rc);
}


/*ARGSUSED*/
int
__ns_ldap_delEntry(
	const char *dn,
	const Auth_t *cred,
	const int flags,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	LDAP		*ld;
	char		**servers;
	char		errstr[MAXERROR];
	int		rc;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_delEntry START\n");
#endif
	if ((dn == NULL) ||
		(cred == NULL))
		return (NS_LDAP_INVALID_PARAM);

	*errorp = NULL;

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS)
		return (rc);

	rc = __s_api_getConnection(servers, 0, cred,
			&ld, &connectionId, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		__s_api_free2dArray(servers);
		return (rc);
	}

	/* Perform the delete operation. */
	rc = ldap_delete_ext_s(ld, (char *)dn, NULL, NULL);
	if (rc != LDAP_SUCCESS) {
		int	errno;
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &errno);
		sprintf(errstr, gettext(ldap_err2string(errno)));
		MKERROR(*errorp, errno, errstr, NULL);
		DropConnection(connectionId, 0);
		__s_api_free2dArray(servers);
		return (NS_LDAP_INTERNAL);
	}
	__s_api_free2dArray(servers);
	DropConnection(connectionId, 0);
	return (NS_LDAP_SUCCESS);
}
