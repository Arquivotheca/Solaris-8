/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ns_common.c 1.7	99/11/11 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <libintl.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdir.h>
#include <lber.h>
#include <ldap.h>

#include "ns_sldap.h"
#include "ns_internal.h"

#define	UDP	"/dev/udp"
#define	MAXIFS	32

struct ifinfo {
	struct in_addr addr, netmask;
};

extern unsigned int _sleep(unsigned int);
static int __s_api_getDefaultAuth(Auth_t ** authp, int * cookie,
		const char *domain, ns_ldap_error_t **errorp);
static char ** __s_api_parseDN(char *val, char *database);
static char ** __s_api_parseServer(char **vals);
static char ** __s_api_sortServerNet(char **srvlist);
static char ** __s_api_sortServerPref(char **srvlist, char **preflist,
		boolean_t flag);
static int __s_api_isDN(char *userDN);


/*
 * FUNCTION:	__s_api_setOption
 *
 *		set given search options (flags) on ld
 *
 * RETURNS:	NS_LDAP_SUCCESS, NS_LDAP_CONFIG, NS_LDAP_INTERNAL
*/
static int
__s_api_setOption(
	LDAP * ld,
	const int flags,
	ns_ldap_error_t	**errorp)
{
void		**paramVal = NULL;
char		errstr[MAXERROR];
int		iflags = 0;
int		rc = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_setOption START\n");
#endif

	/* if the NS_LDAP_NOREF or NS_LDAP_FOLLOWREF is set */
	/* this will take precendence over the values specified */
	/* in the configuration file */

	rc = __ns_ldap_getParam(NULL, NS_LDAP_SEARCH_REF_P, &paramVal, errorp);
	if (rc != NS_LDAP_SUCCESS)
		return (rc);

	if ((flags & NS_LDAP_NOREF) || (flags & NS_LDAP_FOLLOWREF))
		iflags = flags;
	else
		iflags = ((* (int *)(*paramVal)) | flags);

	(void) __ns_ldap_freeParam(&paramVal);

	if ((iflags & NS_LDAP_NOREF) && !(iflags & NS_LDAP_FOLLOWREF)) {
	    if (ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)
		!= LDAP_SUCCESS) {
			sprintf(errstr, gettext("ldap_set_option failed."));
			MKERROR(*errorp, NS_LDAP_INTERNAL,
				strdup(errstr), NS_LDAP_INTERNAL);
			return (NS_LDAP_INTERNAL);
		}
	} else if ((iflags & NS_LDAP_FOLLOWREF) && !(iflags & NS_LDAP_NOREF)) {
	    if (ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON)
		!= LDAP_SUCCESS) {
			sprintf(errstr, gettext("ldap_set_option failed."));
			MKERROR(*errorp, NS_LDAP_INTERNAL,
				strdup(errstr), NS_LDAP_INTERNAL);
			return (NS_LDAP_INTERNAL);
		}
	} else if ((iflags & NS_LDAP_FOLLOWREF) && (iflags & NS_LDAP_NOREF))
		return (NS_LDAP_INVALID_PARAM);

	return (NS_LDAP_SUCCESS);
}


/*
 * FUNCTION:	__s_api_getConnection_ext
 *
 *	Bind to a server from the server list and return the connection.
 *
 *	This function is different from __s_api_getConnection as:
 *	1) can be used for rebinding.
 *	2) can be used for HARD LOOKUP in which case it doesn't return without
 *	a connection unless there is a config, authentication or memory error.
 *	3) Doesn't need to have a credential.
 *
 * RETURN VALUES:
 *
 * NS_LDAP_SUCCESS	If connection was made successfully.
 * NS_LDAP_INVALID_PARAM If any invalid arguments were passed to the function.
 * NS_LDAP_CONFIG	If there are any config errors.
 * NS_LDAP_MEMORY	Memory errors.
 * NS_LDAP_INTERNAL	If there was a ldap error.
 *
 * INPUT:
 *
 * servers	List of servers to bind to
 * domain	domain
 * flags		If containing NS_LDAP_HARD the function will not return
 *		until it has a connection unless there is a
 *		authentication problem.
 *		flags could contain various options for the connection.
 * auth		Credentials for bind. This could be NULL in which case
 *		a default cred built from the config module is used.
 * cookie	The last server a connection was made to. This is
 *		used for rebinding.
 * OUTPUT:
 *
 * ld		A pointer to connection.
 * connectionId	connectionId used by connect module
 * cookie	set to the server index which the connect was made to.
 * errorp	Set if there are any INTERNAL, or CONFIG error.
*/
int
__s_api_getConnection_ext(
	char **servers,			/* server list */
	const char * domain,		/* domain */
	const int flags,
	const Auth_t * auth,		/* credentials for bind */
	LDAP ** ld,
	int *connectionId,
	int *cookie,
	ns_ldap_error_t **errorp)
{

	ns_ldap_error_t	*interErr;
	Auth_t		*authp = NULL;
	Auth_t		*newauth = NULL;
	char		*temptr;
	char		errmsg[MAXERROR];
	int		authCookie = 0;
	int		InvalidMethod = 0;
	int		i = 0;
	int		lastServer = 0;
	int		FirstTime = 1;
	int		rc = 0;
	int		sec = 0, times_thru = 0;


	if (servers == NULL || *servers == NULL)
		return (NS_LDAP_INVALID_PARAM);

	if (cookie)
		lastServer = *cookie;

	if (auth == NULL) {

		while ((rc = __s_api_getDefaultAuth(&authp, &authCookie, domain,
			errorp)) == NS_LDAP_INVALID_PARAM);
		/*
		NS_LDAP_INVALID_PARAM is overloaded to indicate that
		the default cred for a given auth method was imcomplete
		*/

		if (rc == NS_LDAP_OP_FAILED) {
			sprintf(errmsg,
			gettext("Auth method is not correctly provided!"));
			MKERROR(*errorp, NS_CONFIG_FILE, strdup(errmsg),
				NS_LDAP_CONFIG);
			return (NS_LDAP_CONFIG);
		} else if (rc != NS_LDAP_SUCCESS)
			return (rc);
	} else {
		authp = (Auth_t *)calloc(1, sizeof (Auth_t));
		if (!authp) {
			return (NS_LDAP_MEMORY);
		}
		if ((auth->type == NS_LDAP_AUTH_SIMPLE) ||
			(auth->type == NS_LDAP_AUTH_SASL_CRAM_MD5)) {

			if (!(__s_api_isDN(auth->cred.unix_cred.userID))) {
				char *userDN;
				rc = __ns_ldap_uid2dn(
					auth->cred.unix_cred.userID,
					domain, &userDN, NULL, errorp);
				if (rc != NS_LDAP_SUCCESS) {
					free(authp);
					return (rc);
				}
				authp->cred.unix_cred.userID = userDN;
			} else
				authp->cred.unix_cred.userID =
					strdup(auth->cred.unix_cred.userID);
			authp->type = auth->type;
			authp->security = auth->security;
			authp->cred.unix_cred.passwd =
				strdup(auth->cred.unix_cred.passwd);
		}
	} /* end of else */

start:	i = 0;
	if (i == (lastServer - 1))
		i++;
	temptr = servers[i];
	FirstTime = 1;

	while (temptr != NULL) {

		if (*errorp)
			__ns_ldap_freeError(errorp);

		*ld =  MakeConnection(temptr, (Auth_t *)authp, 0,
			connectionId, errorp);

		if (*ld && !*errorp) {
			if (cookie != NULL)
				*cookie = i+1;
			rc = NS_LDAP_SUCCESS;
			goto done;

		} else if (*errorp) {
		    switch ((*errorp)->status) {
			case LDAP_INVALID_CREDENTIALS:
			case LDAP_INVALID_DN_SYNTAX:
			case LDAP_INSUFFICIENT_ACCESS:
				__ns_ldap_freeAuth(&authp);
				return (NS_LDAP_OP_FAILED);

			case LDAP_AUTH_METHOD_NOT_SUPPORTED:
			case LDAP_INAPPROPRIATE_AUTH:
			case LDAP_NO_SUCH_OBJECT:
				if (auth == NULL) {
					__ns_ldap_freeAuth(&authp);
					while ((rc =
						__s_api_getDefaultAuth(&newauth,
						&authCookie, domain, &interErr))
						== NS_LDAP_INVALID_PARAM);
					if (rc == NS_LDAP_SUCCESS) {
						authp = newauth;
						/* try to connect again */
						continue;
					}
					if (rc == NS_LDAP_OP_FAILED) {
						InvalidMethod = 1;
					} else {
						__ns_ldap_freeError(errorp);
						*errorp = interErr;
						return (rc);
					}
				} else
					InvalidMethod = 1;
				break;
			default:
				InvalidMethod = 0;
				rc = NS_LDAP_INTERNAL;
				break;
		    }

		} else if (!*ld && !*errorp) {
			rc = NS_LDAP_MEMORY;
			goto done;
		}
		i++;
		if (i == (lastServer - 1))
			i++;
		if (FirstTime && (servers[i] == NULL) &&
			(lastServer - 1 >= 0)) {
				FirstTime = 0;
				i = lastServer - 1;
		}
		temptr = servers[i];
	}
	if (InvalidMethod) /* errorp is already set */
		return (NS_LDAP_INTERNAL);
	if (flags & NS_LDAP_HARD) {
		times_thru++;
		sec = 2 << times_thru;
		if (sec >= LDAPMAXHARDLOOKUPTIME) {
			sec = LDAPMAXHARDLOOKUPTIME;
			times_thru = 0;
		}
		_sleep(sec);
		authCookie = 0;
		goto start;
	}
done:
	__ns_ldap_freeAuth(&authp);
	if (rc == NS_LDAP_SUCCESS && flags)
		if ((rc = __s_api_setOption(*ld, flags, errorp)) !=
			NS_LDAP_SUCCESS) {
			DropConnection(*connectionId, 0);
			return (rc);
		}
	return (rc);
}


/*
 * FUNCTION:	__s_api_getConnection
 *
 *	Bind to a server from the server list and return the connection.
 *	This function is not used for rebinding and HARD lookup.
 *
 * RETURN VALUES:
 *
 * NS_LDAP_SUCCESS	If connection was made successfully.
 * NS_LDAP_INVALID_PARAM	If servers or auth are null.
 * NS_LDAP_CONFIG	If there are any config errors.
 * NS_LDAP_MEMORY	If memory errors.
 * NS_LDAP_INTERNAL	If there was a ldap error.
 * NS_LDAP_OP_FAILED	If there is a authentication error.
 *
 * INPUT:
 *
 * servers	List of servers to bind to
 * flags	flags could contain various options for the connection.
 * auth		Credentials for bind. Must be provided.
 * OUTPUT:
 *
 * ld		A pointer to connection.
 * connectionId	connectionId used by connect module
 * errorp	Set if there are any INTERNAL, or CONFIG error.
*/
int
__s_api_getConnection(
	char **servers,
	int flags,
	const Auth_t * auth,
	LDAP **ld,
	int *connectionId,
	ns_ldap_error_t **errorp)
{

	Auth_t		*authp = NULL;
	char		*temptr;
	int		noanswer = 1;
	int		i = 0;
	int		rc = 0;


#ifdef DEBUG
	fprintf(stderr, "__s_api_getConnection START\n");
#endif
	if (servers == NULL || *servers == NULL || auth == NULL)
		return (NS_LDAP_INVALID_PARAM);

	if (((auth->type == NS_LDAP_AUTH_SIMPLE) ||
		(auth->type == NS_LDAP_AUTH_SASL_CRAM_MD5)) &&
		!(__s_api_isDN(auth->cred.unix_cred.userID))) {

		char *userDN;
		authp = (Auth_t *)calloc(1, sizeof (Auth_t));
		if (!authp) {
			return (NS_LDAP_MEMORY);
		}
		rc = __ns_ldap_uid2dn(auth->cred.unix_cred.userID,
			NULL, &userDN, NULL, errorp);
		if (rc != NS_LDAP_SUCCESS) {
			__ns_ldap_freeAuth(&authp);
			return (rc);
		}
		authp->type = auth->type;
		authp->security = auth->security;
		authp->cred.unix_cred.passwd =
			strdup(auth->cred.unix_cred.passwd);
		authp->cred.unix_cred.userID = userDN;
	}

	i = 0;
	temptr = servers[i];
	while (noanswer && temptr != NULL) {

		if (*errorp)
			__ns_ldap_freeError(errorp);

		if (authp)
			*ld =  MakeConnection(temptr, (Auth_t *)authp, 0,
				connectionId, errorp);
		else
			*ld =  MakeConnection(temptr, (Auth_t *)auth, 0,
				connectionId, errorp);

		if (!*ld && !*errorp) { /* memory error */
			__ns_ldap_freeAuth(&authp);
			return (NS_LDAP_MEMORY);
		} else if (*errorp) {
			switch ((*errorp)->status) {
				case LDAP_INVALID_CREDENTIALS:
				case LDAP_INVALID_DN_SYNTAX:
				case LDAP_INSUFFICIENT_ACCESS:
					__ns_ldap_freeAuth(&authp);
					return (NS_LDAP_INTERNAL);
			}
		} else {
			rc = NS_LDAP_SUCCESS;
			noanswer = 0;
		}
		temptr = servers[++i];
	}

	if (noanswer)
		rc = NS_LDAP_INTERNAL;

	__ns_ldap_freeAuth(&authp);

	if (rc == NS_LDAP_SUCCESS && flags) {
		if ((rc = __s_api_setOption(*ld, flags, errorp))
			!=  NS_LDAP_SUCCESS)
				DropConnection(*connectionId, 0);
				return (rc);
	}
	return (rc);
}

/*
 * FUNCTION:	s_api_printResult
 *	Given a ns_ldap_result structure print it.
*/
int
__s_api_printResult(ns_ldap_result_t * result)
{

	ns_ldap_entry_t	*curEntry;
	int		i, j, k = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_printResult START\n");
#endif
	printf("--------------------------------------\n");
	if (result == NULL) {
		printf("No result\n");
		return (0);
	}
	printf("entries_count %d\n", result->entries_count);
	curEntry = result->entry;
	for (i = 0; i < result->entries_count; i++) {

	    printf("entry %d has attr_count = %d \n", i, curEntry->attr_count);
	    for (j = 0; j < curEntry->attr_count; j++) {
		printf("entry %d has attr_pair[%d] = %s \n", i, j,
		curEntry->attr_pair[j]->attrname);
		for (k = 0; k < 20 && curEntry->attr_pair[j]->attrvalue[k]; k++)
		printf("entry %d has attr_pair[%d]->attrvalue[%d] = %s \n",
		i, j, k, curEntry->attr_pair[j]->attrvalue[k]);
	    }
	    printf("\n--------------------------------------\n");
	    curEntry = curEntry->next;
	}
	return (1);
}

/*
 * FUNCTION:	__s_api_getSearchScope
 *
 *	Retrieve the search scope for ldap search from the config module.
 *
 * RETURN VALUES:	NS_LDAP_SUCCESS, NS_LDAP_CONFIG
 * INPUT:		domain
 * OUTPUT:		searchScope, errorp
*/
int
__s_api_getSearchScope(
	int *searchScope,
	const char *domain,
	ns_ldap_error_t **errorp)
{

	char		errmsg[MAXERROR];
	void		**paramVal = NULL;
	int		rc = 0;
	int		scope = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getSearchScope START\n");
#endif
	if (*searchScope == 0) {
		if ((rc = __ns_ldap_getParam((char *)domain,
			NS_LDAP_SEARCH_SCOPE_P,
			&paramVal, errorp)) != NS_LDAP_SUCCESS) {
			return (rc);
		}
		scope = * (int *)(*paramVal);
	} else {
		scope = *searchScope;
	}

	switch (scope) {

		case	NS_LDAP_SCOPE_ONELEVEL:
			*searchScope = LDAP_SCOPE_ONELEVEL;
			break;
		case	NS_LDAP_SCOPE_BASE:
			*searchScope = LDAP_SCOPE_BASE;
			break;
		case	NS_LDAP_SCOPE_SUBTREE:
			*searchScope = LDAP_SCOPE_SUBTREE;
			break;
		default:
			(void) __ns_ldap_freeParam(&paramVal);
			sprintf(errmsg, gettext("Invalid search scope!"));
			MKERROR(*errorp, NS_CONFIG_FILE, strdup(errmsg),
				NS_LDAP_CONFIG);
			return (NS_LDAP_CONFIG);
	}
	(void) __ns_ldap_freeParam(&paramVal);

	return (NS_LDAP_SUCCESS);
}

/*
 * FUNCTION:	__s_api_getDefaultAuth
 *
 *	Constructs a credential for authentication using the config module.
 *
 * RETURN VALUES:
 *
 * NS_LDAP_SUCCESS	If successful
 * NS_LDAP_CONFIG	If there are any config errors.
 * NS_LDAP_MEMORY	Memory errors.
 * NS_LDAP_OP_FAILED	If there are no more authentication methods so can
 *			not build a new authp.
 * NS_LDAP_INVALID_PARAM This overloaded return value means that some of the
 *			necessary fields of a cred for a given auth method
 *			are not provided.
 * INPUT:
 *
 * domain	domain
 * cookie	last authentication method used in the authentication method
 *		list. The cookie was set last time getDefaultAuth was called.
 * OUTPUT:
 *
 * cookie		Index of the current authentication method.
 * errorp		Set if there are any INTERNAL or CONFIG error.
*/
static int
__s_api_getDefaultAuth(
	Auth_t ** authp,
	int * cookie,
	const char *domain,
	ns_ldap_error_t **errorp)
{

	void		**paramVal = NULL;
	void		**temptr = NULL;
	char		*modparamVal;
	char		errmsg[MAXERROR];
	int		i = 0;
	int		authtype;
	int		typeNo = 0;
	int		rc = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getDefaultAuth START\n");
#endif
	if ((rc = __ns_ldap_getParam((char *)domain, NS_LDAP_AUTH_P,
		&paramVal, errorp)) != NS_LDAP_SUCCESS) {
		return (rc);
	}

	if (paramVal == NULL) {
		sprintf(errmsg, gettext("No auth method provided."));
		MKERROR(*errorp, NS_CONFIG_FILE, strdup(errmsg),
			NS_LDAP_CONFIG);
		return (NS_LDAP_CONFIG);
	}
	for (temptr = paramVal; *temptr != NULL; temptr++)
		typeNo++;

	if (*cookie >= typeNo) {
		*authp = NULL;
		*cookie = 0;
		(void) __ns_ldap_freeParam(&paramVal);
		return (NS_LDAP_OP_FAILED);
	}

	*authp = (Auth_t *)calloc(1, sizeof (Auth_t));
	if ((*authp) == NULL) {
		(void) __ns_ldap_freeParam(&paramVal);
		return (NS_LDAP_MEMORY);
	}

	authtype = *(int *)(paramVal[*cookie]);
	switch (authtype) {

		case    NS_LDAP_AUTH_NONE:
			(*authp)->type = NS_LDAP_AUTH_NONE;
			break;
		case    NS_LDAP_AUTH_SIMPLE:
		case    NS_LDAP_AUTH_SASL_CRAM_MD5:
			(*authp)->type = (authtype == NS_LDAP_AUTH_SIMPLE) ?
				NS_LDAP_AUTH_SIMPLE :
				NS_LDAP_AUTH_SASL_CRAM_MD5;

			(void) __ns_ldap_freeParam(&paramVal);
			if ((rc = __ns_ldap_getParam((char *)domain,
				NS_LDAP_TRANSPORT_SEC_P,
				&paramVal, errorp)) != NS_LDAP_SUCCESS) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				return (rc);
			}

			if (paramVal == NULL) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				(*cookie)++;
				return (NS_LDAP_INVALID_PARAM);
			}
			switch (* (int *)(*paramVal)) {
				case    NS_LDAP_SEC_NONE:
					(*authp)->security = NS_LDAP_SEC_NONE;
					break;
				case	NS_LDAP_SEC_TLS:
					(*authp)->security = NS_LDAP_SEC_TLS;
					break;
				default:
					(void) __ns_ldap_freeParam(&paramVal);
					__ns_ldap_freeAuth(authp);
					*authp = NULL;
					(*cookie)++;
					return (NS_LDAP_INVALID_PARAM);
			}

			(void) __ns_ldap_freeParam(&paramVal);
			if ((rc = __ns_ldap_getParam((char *)domain,
				NS_LDAP_BINDDN_P,
				&paramVal, errorp)) != NS_LDAP_SUCCESS) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				return (rc);
			}

			if (paramVal == NULL) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				(*cookie)++;
				return (NS_LDAP_INVALID_PARAM);
			}
			i = strlen((char *)*paramVal);
			if (((*authp)->cred.unix_cred.userID = (char *)
			    calloc(i + 1, sizeof (char))) == NULL) {
				(void) __ns_ldap_freeParam(&paramVal);
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				return (NS_LDAP_MEMORY);
			}

			strncpy((*authp)->cred.unix_cred.userID,
				(char *)*paramVal, i);
			(*authp)->cred.unix_cred.userID[i] = '\0';
			(void) __ns_ldap_freeParam(&paramVal);

			if ((rc = __ns_ldap_getParam((char *)domain,
				NS_LDAP_BINDPASSWD_P,
				&paramVal, errorp)) != NS_LDAP_SUCCESS) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				return (rc);
			}

			if (paramVal == NULL || *paramVal == NULL) {
				if (paramVal)
					(void) __ns_ldap_freeParam(&paramVal);
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				(*cookie)++;
				return (NS_LDAP_INVALID_PARAM);
			}

			modparamVal = dvalue((char *)*paramVal);
			if (modparamVal == NULL) {
				(void) __ns_ldap_freeParam(&paramVal);
				__ns_ldap_freeAuth(authp);

				(*cookie)++;
				return (NS_LDAP_INVALID_PARAM);
			}
			i = strlen((char *)modparamVal);
			if (0 == i) {
				(void) __ns_ldap_freeParam(&paramVal);
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				(*cookie)++;
				return (NS_LDAP_INVALID_PARAM);
			}

			if (((*authp)->cred.unix_cred.passwd = (char *)
				calloc(i + 1, sizeof (char))) == NULL) {
				__ns_ldap_freeAuth(authp);
				*authp = NULL;
				(void) __ns_ldap_freeParam(&paramVal);
				free(modparamVal);
				return (NS_LDAP_MEMORY);
			}

			strncpy((*authp)->cred.unix_cred.passwd,
				(char *)modparamVal, i);
			free(modparamVal);
			(*authp)->cred.unix_cred.passwd[i] = '\0';
			break;
		default:
			(void) __ns_ldap_freeParam(&paramVal);
			sprintf(errmsg, gettext("Auth method not supported."));
			MKERROR(*errorp, NS_CONFIG_FILE, strdup(errmsg),
				NS_LDAP_CONFIG);
			__ns_ldap_freeAuth(authp);
			*authp = NULL;
			(*cookie)++;
			return (NS_LDAP_CONFIG);
	}
	(void) __ns_ldap_freeParam(&paramVal);
	(*cookie)++;
	return (NS_LDAP_SUCCESS);
}

/*
 * FUNCTION:	__ns_ldap_freeAuth
 *
 *	Frees all the memory associated with a authentication structure.
 *
 * RETURN VALUES:	NS_LDAP_INVALID_PARAM, NS_LDAP_SUCCESS, NS_LDAP_CONFIG
 * INPUT:		authp
*/
int
__ns_ldap_freeAuth(Auth_t ** authp)
{
	Auth_t *ap;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_freeAuth START\n");
#endif
	if (authp == NULL || *authp == NULL)
		return (NS_LDAP_INVALID_PARAM);

	ap = *authp;
	if (ap->cred.unix_cred.userID) {
		memset(ap->cred.unix_cred.userID, 0,
			strlen(ap->cred.unix_cred.userID));
		free(ap->cred.unix_cred.userID);
	}

	if (ap->cred.unix_cred.passwd) {
		memset(ap->cred.unix_cred.passwd, 0,
			strlen(ap->cred.unix_cred.passwd));
		free(ap->cred.unix_cred.passwd);
	}

	free(ap);
	*authp = NULL;
	return (NS_LDAP_SUCCESS);
}

/*
 * FUNCTION:	__s_api_getDNs
 *
 *	Retrieves the list of search DNS from the config module for the given
 *	domain and database.
 *
 * RETURN VALUES:	NS_LDAP_SUCCESS, NS_LDAP_MEMORY, NS_LDAP_CONFIG
 * INPUT:		domain, database
 * OUTPUT:		DN, error
*/
int
__s_api_getDNs(
	char *** DN,
	const char *domain,
	const char *database,
	ns_ldap_error_t ** error)
{

	void	**paramVal = NULL;
	void	**temptr = NULL;
	char	**dns = NULL;
	int	rc = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getDNs START\n");
#endif
	if ((rc = __ns_ldap_getParam((char *)domain, NS_LDAP_SEARCH_DN_P,
	    &paramVal, error)) != NS_LDAP_SUCCESS) {
		return (rc);
	}

	if (database && paramVal) {
		for (temptr = paramVal; *temptr != NULL; temptr++) {
			dns = __s_api_parseDN((char *)(*temptr),
			    (char *)database);
			if (dns != NULL)
				break;
		}
	}

	if (dns == NULL) {
		(void) __ns_ldap_freeParam(&paramVal);
		if ((rc = __ns_ldap_getParam((char *)domain,
		    NS_LDAP_SEARCH_BASEDN_P,
		    &paramVal, error)) != NS_LDAP_SUCCESS) {
			return (rc);
		}
		if (!paramVal) {
			char errmsg[MAXERROR];

			sprintf(errmsg, gettext("BaseDN not defined"));
			MKERROR(*error, NS_CONFIG_FILE, strdup(errmsg),
			    NS_LDAP_CONFIG);
			return (NS_LDAP_CONFIG);
		}

		dns = (char **)calloc(2, sizeof (char *));
		if (dns == NULL) {
			(void) __ns_ldap_freeParam(&paramVal);
			return (NS_LDAP_MEMORY);
		}
		dns[1] = NULL;

		if (database == NULL) {
			dns[0] = strdup((char *)*paramVal);
		} else if ((strcasecmp(database, "passwd") == 0) ||
		    (strcasecmp(database, "shadow") == 0) ||
		    (strcasecmp(database, "user_attr") == 0) ||
		    (strcasecmp(database, "audit_user") == 0)) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 11,
			    sizeof (char));
			strcpy(dns[0], "ou=people,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "group") == 0) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 10,
			    sizeof (char));
			strcpy(dns[0], "ou=group,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "rpc") == 0) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 8,
			    sizeof (char));
			strcpy(dns[0], "ou=rpc,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "protocols") == 0) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 14,
			    sizeof (char));
			strcpy(dns[0], "ou=protocols,");
			strcat(dns[0], (char *)*paramVal);

		} else if ((strcasecmp(database, "networks") == 0) ||
		    (strcasecmp(database, "netmasks") == 0)) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 13,
			    sizeof (char));
			strcpy(dns[0], "ou=networks,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "netgroup") == 0) {

			dns[0] = (char *) calloc(strlen((char *)*paramVal) + 13,
			    sizeof (char));
			strcpy(dns[0], "ou=netgroup,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "aliases") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 12,
			    sizeof (char));
			strcpy(dns[0], "ou=aliases,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "Hosts") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 10,
			    sizeof (char));
			strcpy(dns[0], "ou=Hosts,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "Services") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 13,
			    sizeof (char));
			strcpy(dns[0], "ou=Services,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "bootparams") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 15,
			    sizeof (char));
			strcpy(dns[0], "ou=ethers,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "ethers") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 11,
			    sizeof (char));
			strcpy(dns[0], "ou=ethers,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "auth_attr") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 20,
			    sizeof (char));
			strcpy(dns[0], "ou=SolarisAuthAttr,");
			strcat(dns[0], (char *)*paramVal);

		} else if ((strcasecmp(database, "prof_attr") == 0) ||
		    (strcasecmp(database, "exec_attr") == 0)) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 20,
			    sizeof (char));
			strcpy(dns[0], "ou=SolarisProfAttr,");
			strcat(dns[0], (char *)*paramVal);

		} else if (strcasecmp(database, "profile") == 0) {

			dns[0] = (char *)calloc(strlen((char *)*paramVal) + 15,
			    sizeof (char));
			strcpy(dns[0], "ou=profile,");
			strcat(dns[0], (char *)*paramVal);

		} else {
			char *p = (char *)*paramVal;
			char *buffer = NULL;

			if (strchr(database, '=') == NULL) {
				buffer = (char *)malloc(strlen(database) +
				    strlen(p) + 13);
				sprintf(buffer,
				    "nismapname=%s,%s", database, p);
			} else {
				buffer = (char *)malloc(strlen(database) +
				    strlen(p) + 2);
				sprintf(buffer, "%s,%s", database, p);
			}
			dns[0] = buffer;
		}
	}

	(void) __ns_ldap_freeParam(&paramVal);
	*DN = dns;
	return (NS_LDAP_SUCCESS);

}
/*
 * FUNCTION:	__s_api_parseDN
 *
 *	Parse a special formated list(val) into an array of char *.
 *
 * RETURN VALUE:	A char * pointer to the new list of dns.
 * INPUT:		val, database
*/
static char **
__s_api_parseDN(
	char *val,
	char *database)
{

	size_t		len = 0;
	char		**retVal = NULL;
	char		*temptr = val;
	int		index = 0;
	int 		valNo = 0;
	int		valSize = 0;
	int		i;

#ifdef DEBUG
	fprintf(stderr, "__s_api_parseDN START\n");
#endif
	if ((len = strlen(val)) < 1)
		return (NULL);
	if (strncmp((const char *)val, (const char *)database,
		(size_t)strlen(database)))
		return (NULL);
	if (val[strlen(database)] != ':')
		return (NULL);
	while (*(++temptr) != NULL)
		if (*temptr == '(')
			valNo++;

	retVal = (char **)calloc(valNo +1, sizeof (char *));
	if (retVal == NULL)
		return (NULL);

	temptr = val;
	index = 0;

	for (i = 0; (i < valNo) && (index < len); i++) {
		valSize = 0;
		for (; temptr != NULL && * temptr != '('; index++, temptr++)
			;
		index++;
		temptr++;
		while (temptr != NULL && * temptr != ')') {
		    temptr++;
		    valSize++;
		}
		retVal[i] = (char *)calloc(valSize + 1, sizeof (char));
		if (retVal[i] == NULL) {
			__s_api_free2dArray(retVal);
			return (NULL);
		}
		strncpy(retVal[i], val+index, valSize);
		retVal[i][valSize] = '\0';
		index += valSize;
	}

	retVal[i] = NULL;
	return (retVal);
}


/*
 * __s_api_get_local_interfaces
 *
 * Returns an pointer to an array of addresses and netmasks of all interfaces
 * configured on the system.
 *
 * NOTE: This function is very IPv4 centric.
 */
static struct ifinfo *
__s_api_get_local_interfaces()
{
	struct ifconf		ifc;
	struct ifreq		ifreq, *ifr;
	struct ifinfo		*localinfo;
	struct in_addr		netmask;
	struct sockaddr_in	*sin;
	char			*buf = NULL;
	int			fd = 0;
	int			numifs = 0;
	int			i, n = 0;

	if ((fd = open(UDP, O_RDONLY)) < 0)
		return ((struct ifinfo *)NULL);

	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
	}

	buf = (char *)malloc(numifs * sizeof (struct ifreq));
	if (buf == NULL) {
		(void) close(fd);
		return ((struct ifinfo *)NULL);
	}
	ifc.ifc_len = numifs * (int)sizeof (struct ifreq);
	ifc.ifc_buf = buf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0) {
		(void) close(fd);
		free(buf);
		return ((struct ifinfo *)NULL);
	}
	ifr = (struct ifreq *)buf;
	numifs = ifc.ifc_len/(int)sizeof (struct ifreq);
	localinfo = (struct ifinfo *)malloc((numifs + 1) *
	    sizeof (struct ifinfo));
	if (localinfo == NULL) {
		(void) close(fd);
		free(buf);
		return ((struct ifinfo *)NULL);
	}

	for (i = 0, n = numifs; n > 0; n--, ifr++) {
		u_int ifrflags;

		ifreq = *ifr;
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifreq) < 0)
			continue;

		ifrflags = ifreq.ifr_flags;
		if (((ifrflags & IFF_UP) == 0) ||
		    (ifr->ifr_addr.sa_family != AF_INET))
			continue;

		if (ioctl(fd, SIOCGIFNETMASK, (char *)&ifreq) < 0)
			continue;
		netmask = ((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;

		if (ioctl(fd, SIOCGIFADDR, (char *)&ifreq) < 0)
			continue;

		sin = (struct sockaddr_in *)&ifreq.ifr_addr;

		localinfo[i].addr = sin->sin_addr;
		localinfo[i].netmask = netmask;
		i++;
	}
	localinfo[i].addr.s_addr = 0;

	free(buf);
	(void) close(fd);
	return (localinfo);
}


/*
 * __s_api_samenet(char *, struct ifinfo *)
 *
 * Returns 1 if address is on the same subnet of the array of addresses
 * passed in.
 *
 * NOTE: This function is very IPv4 centric.
 */
static int
__s_api_sameNet(char *addr, struct ifinfo *ifs)
{
	int		answer = 0;

	if (addr && ifs) {
		char		*addr_raw;
		unsigned long	iaddr;
		int		i;

		if ((addr_raw = strdup(addr)) != NULL) {
			char	*s;

			/* Remove port number. */
			if ((s = strchr(addr_raw, ':')) != NULL)
				*s = '\0';

			iaddr = inet_addr(addr_raw);

			/* Loop through interface list to find match. */
			for (i = 0; ifs[i].addr.s_addr != 0; i++) {
				if ((iaddr & ifs[i].netmask.s_addr) ==
				    (ifs[i].addr.s_addr &
				    ifs[i].netmask.s_addr))
					answer++;
			}
			free(addr_raw);
		}
	}

	return (answer);
}

/*
 * FUNCTION:	__s_api_getServers
 *
 *	Retrieve a list of ldap servers from the config module.
 *
 * RETURN VALUE:	NS_LDAP_SUCCESS, NS_LDAP_CONFIG, NS_LDAP_MEMORY
 * INPUT:		domain
 * OUTPUT:		servers, error
*/
int
__s_api_getServers(
		char *** servers,
		const char *domain,
		ns_ldap_error_t ** error)
{
	void	**paramVal = NULL;
	char	errmsg[MAXERROR];
	char	**sortServers = NULL;
	char	**netservers = NULL;
	int	rc = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getServers START\n");
#endif
	if ((rc = __ns_ldap_getParam((char *)domain, NS_LDAP_SERVERS_P,
			&paramVal, error)) != NS_LDAP_SUCCESS)
		return (rc);

	if (paramVal == NULL) {
		sprintf(errmsg, gettext("No server list"));
		MKERROR(*error, NS_CONFIG_FILE, strdup(errmsg), NS_LDAP_CONFIG);
		return (NS_LDAP_CONFIG);
	}

	/* Get server address(es) and go through them. */
	*servers = __s_api_parseServer((char **)paramVal);

	/* Sort servers based on network. */
	netservers = __s_api_sortServerNet(*servers);
	if (netservers) {
		free(*servers);
		*servers = netservers;
	}

	/* Get preferred server list and sort servers based on that. */
	(void) __ns_ldap_freeParam(&paramVal);
	paramVal = NULL;
	if ((rc = __ns_ldap_getParam((char *)domain,
			NS_LDAP_SERVER_PREF_P,
			&paramVal, error)) != NS_LDAP_SUCCESS) {
		return (rc);
	}

	if (paramVal != NULL) {
		char **prefServers;
		void ** val;

		if ((rc =  __ns_ldap_getParam((char *)domain,
			NS_LDAP_PREF_ONLY_P,
			&val, error)) != NS_LDAP_SUCCESS)
			return (rc);

		prefServers = __s_api_parseServer((char **)paramVal);
		if (prefServers) {
			if (val != NULL && (*val) != NULL &&
					*(int *)val[0] == 1)
				sortServers = __s_api_sortServerPref(*servers,
					prefServers, B_FALSE);
			else
				sortServers = __s_api_sortServerPref(*servers,
					prefServers, B_TRUE);
			if (sortServers) {
				free(*servers);
				free(prefServers);
				*servers = sortServers;
			}
		}
		(void) __ns_ldap_freeParam(&val);
	}
	(void) __ns_ldap_freeParam(&paramVal);

	return (NS_LDAP_SUCCESS);

}

/*
 * FUNCTION:	__s_api_parseServer
*/
static char **
__s_api_parseServer(char **vals)
{

	char		**retVal = NULL;
	char		**temptr = NULL;
	int		valNo = 0;
	int		i = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_parseServers START\n");
#endif

	if (vals == NULL)
		return (NULL);
	for (temptr = vals; *temptr != NULL; temptr++)
		valNo++;

	retVal = (char **)calloc(valNo +1, sizeof (char *));
	if (retVal == NULL)
		return (NULL);

	for (temptr = vals, i = 0;
		*temptr != NULL && i < valNo; temptr++, i++) {
		retVal[i] = strdup(*temptr);
		if (retVal[i] == NULL) {
			__s_api_free2dArray(retVal);
			return (NULL);
		}
	}
	retVal[i] = NULL;
	return (retVal);
}

/*
 * FUNCTION:	__s_api_sortServerNet
 *	Sort the serverlist based on the distance from client
*/
static char **
__s_api_sortServerNet(char **srvlist)
{
	int		count = 0;
	int		all = 0;
	struct ifinfo	*ifs = __s_api_get_local_interfaces();
	char		**tsrvs;
	char		**psrvs, **retsrvs;

	/* Sanity check. */
	if (srvlist == NULL || srvlist[0] == NULL)
		return (NULL);

	/* Count the number of servers to sort. */
	for (count = 0; srvlist[count] != NULL; count++);
	count++;

	/* Make room for the returned list of servers. */
	retsrvs = (char **)calloc(count, sizeof (char *));
	if (retsrvs == NULL) {
		free(ifs);
		return (NULL);
	}

	retsrvs[count - 1] = NULL;

	/* Make a temporary list of servers. */
	psrvs = (char **)calloc(count, sizeof (char *));
	if (psrvs == NULL) {
		free(ifs);
		free(retsrvs);
		return (NULL);
	}

	/* Filter servers on the same subnet */
	tsrvs = srvlist;
	while (*tsrvs) {
		if (__s_api_sameNet(*tsrvs, ifs)) {
			psrvs[all] = *tsrvs;
			retsrvs[all++] = *(tsrvs);
		}
		tsrvs++;
	}

	/* Filter remaining servers. */
	tsrvs = srvlist;
	while (*tsrvs) {
		char	**ttsrvs = psrvs;

		while (*ttsrvs) {
			if (strcmp(*tsrvs, *ttsrvs) == 0)
				break;
			ttsrvs++;
		}

		if (*ttsrvs == NULL)
			retsrvs[all++] = *(tsrvs);
		tsrvs++;
	}

	free(ifs);
	free(psrvs);

	return (retsrvs);
}

/*
 * FUNCTION:	__s_api_sortServerPref
 *	Sort the serverlist based on the preffered server list
*/
static char **
__s_api_sortServerPref(char **srvlist, char **preflist, boolean_t flag)
{
	int		count = 0;
	int		all = 0;
	char		**tsrvs;
	char		**retsrvs;

	/* Sanity check. */
	if (srvlist == NULL || srvlist[0] == NULL)
		return (NULL);

	/* Count the number of servers to sort. */
	for (count = 0; srvlist[count] != NULL; count++);
	count++;

	/* Make room for the returned list of servers */
	retsrvs = (char **)calloc(count, sizeof (char *));
	if (retsrvs == NULL)
		return (NULL);

	/* Is there work to do? */
	if (preflist == NULL || preflist[0] == NULL) {
		tsrvs = srvlist;
		while (*tsrvs)
			retsrvs[all++] = *(tsrvs++);
		return (retsrvs);
	}

	/* Throw out preferred servers not on server list */
	tsrvs = preflist;
	while (*tsrvs) {
		char	**ttsrvs = srvlist;

		while (*ttsrvs) {
			if (strcmp(*tsrvs, *(ttsrvs)) == 0)
				break;
			ttsrvs++;
		}
		if (*ttsrvs != NULL)
			retsrvs[all++] = *tsrvs;
		tsrvs++;
	}

	/*
	 * If PREF_ONLY is false, we append the non-preferred servers
	 * to bottom of list.
	 */
	if (flag == B_TRUE) {

		tsrvs = srvlist;
		while (*tsrvs) {
			char	**ttsrvs = preflist;

			while (*ttsrvs) {
				if (strcmp(*tsrvs, *ttsrvs) == 0)
					break;
				ttsrvs++;
			}
			if (*ttsrvs == NULL)
				retsrvs[all++] = *tsrvs;
			tsrvs++;
		}
	}

	return (retsrvs);
}


/*
 * FUNCTION:	__s_api_free2dArray
*/
int
__s_api_free2dArray(char ** inarray)
{

	char	**temptr;

	if (inarray == NULL)
		return (0);

	for (temptr = inarray; *temptr != NULL; temptr++) {
		free(*temptr);
	}
	free(inarray);
	return (NS_LDAP_SUCCESS);
}

/*
 * FUNCTION:	__s_api_cp2dArray
*/
char **
__s_api_cp2dArray(char **inarray)
{
	char	**newarray;
	char	**tarray, **ttarray;
	int	count;

	if (inarray == NULL)
		return (NULL);

	for (count = 0; inarray[count] != NULL; count++);

	newarray = (char **)calloc(count + 1, sizeof (char *));
	if (newarray == NULL)
		return (NULL);

	tarray = inarray;
	ttarray = newarray;
	while (*tarray)
		*(ttarray++) = strdup(*(tarray++));

	return (newarray);
}

/*
 * FUNCTION:	__s_api_isDN
 *	Determines if userDN is in DN syntax or not.
 * RETURNS:	1 if yes, 0 if not.
*/
static int
__s_api_isDN(char *userDN)
{
	if (userDN == NULL)
		return (1);
	if (strchr(userDN, '=') != NULL)
		return (1);
	return (0);
}

/*
 * FUNCTION:	__s_api_isCtrlSupported
 *	Determines if the passed control is supported by the LDAP sever.
 * RETURNS:	NS_LDAP_SUCCESS if yes, NS_LDAP_OP_FAIL if not.
*/
int
__s_api_isCtrlSupported(LDAP *ld, char *ctrlString, ns_ldap_error_t **error)
{
	LDAPMessage	*resultMsg;
	LDAPMessage	*e;
	BerElement	*ber;
	char		errmsg[MAXERROR];
	char		*attrs[2];
	char		*a;
	char		**vals;
	int		ldaperrno = 0;
	int		rc = 0;
	int		i = 0;

#ifdef DEBUG
		fprintf(stderr, "__s_api_isCtrlSupported START\n");
#endif
	/*  SEARCH */

	attrs[0] = "supportedControl";
	attrs[1] = NULL;

	rc = ldap_search_ext_s(ld, "", LDAP_SCOPE_BASE, "(objectclass=*)",
		attrs, 0, NULL, NULL, NULL, 0, &resultMsg);

	switch (rc) {
		/* If successful, the root DSE was found. */
		case LDAP_SUCCESS:
		break;
			/*
			If the root DSE was not found, the server does
			not comply with the LDAP v3 protocol.
			*/
		case LDAP_PARTIAL_RESULTS:
		case LDAP_NO_SUCH_OBJECT:
		case LDAP_OPERATIONS_ERROR:
		case LDAP_PROTOCOL_ERROR:
			sprintf(errmsg,
			gettext("Root DES not found. Not a LDAPv3 server."));
			MKERROR(*error, NS_LDAP_INTERNAL, strdup(errmsg),
				NS_LDAP_INTERNAL);
			return (NS_LDAP_INTERNAL);
		default:
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errmsg, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errmsg), NULL);
			return (NS_LDAP_INTERNAL);
	}

	if ((e = ldap_first_entry(ld, resultMsg)) != NULL) {
	    if ((a = ldap_first_attribute(ld, e, &ber)) != NULL) {
		if ((vals = ldap_get_values(ld, e, a)) != NULL) {
			for (i = 0; vals[i] != NULL; i++) {
				if (strstr(vals[i], ctrlString)) {
					ldap_value_free(vals);
					ldap_memfree(a);
					if (ber != NULL)
						ber_free(ber, 0);
					ldap_msgfree(resultMsg);
					return (NS_LDAP_SUCCESS);
				}
			}
			ldap_value_free(vals);
		}
		ldap_memfree(a);
	    }
	} else {
		sprintf(errmsg, gettext("ldap_search_ext_s: \
			unable to get root DSE:\n"));
		MKERROR(*error, NS_LDAP_INTERNAL, strdup(errmsg),
			NS_LDAP_INTERNAL);
		ldap_msgfree(resultMsg);
		return (NS_LDAP_INTERNAL);
	}

	if (ber != NULL)
		ber_free(ber, 0);

	ldap_msgfree(resultMsg);
	return (NS_LDAP_OP_FAILED);
}


int __s_api_parsePageControl(LDAP *ld, LDAPMessage *msg,
int *count, struct berval **ctrlCookie, ns_ldap_error_t **error)
{

	LDAPControl	**retCtrls = NULL;
	int		errCode;
	int		rc;
	int		ldaperrno;
	char		errstr[MAXERROR];

#ifdef DEBUG
		fprintf(stderr, "__s_api_parsePageControl START\n");
#endif
	*count = 0;
	if ((rc = ldap_parse_result(ld, msg,  &errCode, NULL, NULL, NULL,
			&retCtrls, 0)) != LDAP_SUCCESS) {

		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
		sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
		MKERROR(*error, ldaperrno, strdup(errstr), NULL);
		return (NS_LDAP_INTERNAL);
	}
	if (retCtrls) {
		if (ldap_parse_page_control(ld,
			retCtrls, (unsigned int *)count, ctrlCookie)
				== LDAP_SUCCESS) {
			if (((*ctrlCookie) == NULL) ||
				((*ctrlCookie)->bv_val == NULL) ||
				(*(*ctrlCookie)->bv_val == NULL)) {
					rc = NS_LDAP_OP_FAILED;
			} else {
				rc = NS_LDAP_SUCCESS;
			}
			ldap_controls_free(retCtrls);
			retCtrls = NULL;
		} else
			rc = NS_LDAP_OP_FAILED;
	} else
		rc = NS_LDAP_OP_FAILED;
	return (rc);
}
