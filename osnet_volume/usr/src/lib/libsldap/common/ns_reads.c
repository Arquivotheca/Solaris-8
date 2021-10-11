/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_reads.c 1.7	99/11/17 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <libintl.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "ns_sldap.h"
#include "ns_internal.h"


static int __s_api_getList(LDAP *ld, int flag, char *searchBase,
		int searchScope, const char *filter,
		const char * const *attribute, ns_ldap_error_t **error,
		int (*callback)(const ns_ldap_entry_t *entry,
		const void *userdata), const void *userdata,
		ns_ldap_result_t ** Rresult);

int
__ns_ldap_list(
	const char *database,
	const char *filter,
	const char * const *attribute,
	const char *domain,
	const Auth_t *auth,
	const int flags,
	ns_ldap_result_t **rResult, /* return result entries */
	ns_ldap_error_t **errorp,
	int (*callback)(const ns_ldap_entry_t *entry, const void *userdata),
	const void *userdata)
{

	ns_ldap_result_t *allResult = NULL; /* result entries for all dns */
	ns_ldap_result_t *result = NULL;   /* result entries per dn */
	ConnectionID	connectionId = -1;
	LDAP		*ld = NULL;
	char		*searchBase = NULL;
	char		**servers = NULL;
	char		**dns = NULL;
	int		connectionCookie = 0;
	int		getListFlag = 0;
	int		searchScope = 0;
	int		done = 0;
	int		rc = 0;
	int		i = 0;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_list START\n");
#endif

	*rResult = NULL;
	*errorp = NULL;

	searchScope = flags & (NS_LDAP_SCOPE_BASE | NS_LDAP_SCOPE_ONELEVEL |
			NS_LDAP_SCOPE_SUBTREE);
	rc = __s_api_getSearchScope(&searchScope, domain, errorp);
	if (rc != NS_LDAP_SUCCESS)
		return (rc);

	rc = __s_api_getDNs(&dns, NULL, database, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		if (dns)
			__s_api_free2dArray(dns);
		return (rc);
	}

	if ((rc = __s_api_getServers(&servers, NULL, errorp)) !=
	    NS_LDAP_SUCCESS) {
		goto cleanup;
	}

	rc = __s_api_getConnection_ext(servers, domain, flags, auth,
		&ld, &connectionId, &connectionCookie, errorp);

	if (rc != NS_LDAP_SUCCESS) {
		if (rc == NS_LDAP_OP_FAILED) /* Authentication failure  */
			rc = NS_LDAP_INTERNAL;
		goto cleanup;
	}

	rc = __s_api_isCtrlSupported(ld, LDAP_CONTROL_VLVREQUEST, errorp);
	if (rc == NS_LDAP_SUCCESS) {
		getListFlag = VLVCTRLFLAG;
	} else if (rc == NS_LDAP_OP_FAILED) {
		rc = __s_api_isCtrlSupported(ld, LDAP_CONTROL_SIMPLE_PAGE,
			errorp);
		if (rc == NS_LDAP_SUCCESS) {
			getListFlag = SIMPLEPAGECTRLFLAG;
		} else if (rc == NS_LDAP_OP_FAILED) {
			getListFlag = 0;
		} else
			goto cleanup;
	} else
		goto cleanup;

	while (((searchBase = dns[i]) != NULL) && !done) {

		rc = __s_api_getList(ld, getListFlag, searchBase,
			    searchScope, filter,
			    attribute, errorp,
			    callback, userdata,
			    &result);

		if (rc == NS_LDAP_INTERNAL) {
			if (flags & NS_LDAP_HARD) {
				__ns_ldap_freeError(errorp);
				DropConnection(connectionId, 0);
				if (result)
					__ns_ldap_freeResult(&result);
				rc = __s_api_getConnection_ext(servers, domain,
					flags, auth, &ld, &connectionId,
					&connectionCookie, errorp);

				if (rc != NS_LDAP_SUCCESS) {
					if (rc == NS_LDAP_OP_FAILED)
						rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}

				rc = __s_api_isCtrlSupported(ld,
					LDAP_CONTROL_VLVREQUEST,
					errorp);
				if (rc == NS_LDAP_SUCCESS) {
					getListFlag = VLVCTRLFLAG;
				} else if (rc == NS_LDAP_OP_FAILED) {
					rc = __s_api_isCtrlSupported(ld,
						LDAP_CONTROL_SIMPLE_PAGE,
						errorp);
					if (rc == NS_LDAP_SUCCESS) {
						getListFlag =
							SIMPLEPAGECTRLFLAG;
					} else if (rc == NS_LDAP_OP_FAILED) {
						getListFlag = 0;
					} else
						goto cleanup;
				} else
					goto cleanup;

				/* go to top of while look and search again */
				continue;
			}
			goto cleanup;

		} else if (rc == NS_LDAP_MEMORY) {
			goto cleanup;

		} else if (rc == NS_LDAP_PARTIAL && callback) {

		/*
		    NS_LDAP_PARTIAL returned by getList indicates that callback
		    function doesn't want anymore data and user wants to abort
		    search.
		*/
			rc = NS_LDAP_SUCCESS;
			goto cleanup;

		} else if (rc == NS_LDAP_OP_FAILED && callback) {

			/* callback returned a wrong error */
			rc = NS_LDAP_OP_FAILED;
			goto cleanup;

		}

		if (flags & NS_LDAP_ALL_RES) {
			if (result) {
				if (allResult == NULL)
					allResult = result;

				else {
				/* append result entries to allResult */

					ns_ldap_entry_t *temptr;
					for (temptr = allResult->entry;
					    temptr->next != NULL;
					    temptr = temptr->next);

					temptr->next = result->entry;
					allResult->entries_count +=
					result->entries_count;
					free(result);
				}
			}

		} else {
			if (result != NULL)
				allResult = result;
			if (rc == NS_LDAP_SUCCESS)
				done = 1;
		}
		i++;
	}

	rc = NS_LDAP_SUCCESS;

	if (!callback && allResult == NULL) {
		rc = NS_LDAP_NOTFOUND;
	} else if (callback) {
		if (*errorp && (*errorp)->status == LDAP_NO_SUCH_OBJECT)
			rc = NS_LDAP_NOTFOUND;
	}

cleanup:
	if (connectionId > -1)
		DropConnection(connectionId, flags);
	if (dns)
		__s_api_free2dArray(dns);
	if (servers)
		__s_api_free2dArray(servers);
	*rResult = allResult;
	return (rc);
}


static int
__s_api_getList(
	LDAP *ld,
	int flag,
	char *searchBase,
	int searchScope,
	const char *filter,
	const char * const *attribute,
	ns_ldap_error_t **error,
	int (*callback)(const ns_ldap_entry_t *entry, const void *userdata),
	const void *userdata,
	ns_ldap_result_t ** Rresult)
{

	ns_ldap_result_t	*result = NULL;
	ns_ldap_entry_t	*curEntry = NULL;
	LDAPMessage	*resultMsg = NULL;
	char		*dn = NULL;
	char		errstr[MAXERROR];
	int		rc = 0;
	int		msgId = 0;
	int		nEntries = 0;
	int		ldaperrno = 0;
	int		nAttrs = 0;
	int		finished = 0;
	LDAPControl	*ctrls[3];
	LDAPControl	**retCtrls = NULL;
	LDAPsortkey	**sortkeylist;
	LDAPControl	*sortctrl = NULL;
	LDAPControl	*vlvctrl = NULL;
	LDAPVirtualList	vlist;
	struct berval	*cookie = NULL;
	unsigned long index = 1;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getList START\n");
#endif
	ctrls[0] = NULL;
	ctrls[1] = NULL;
	ctrls[2] = NULL;

	/*  SEARCH */
	if (flag == VLVCTRLFLAG) {
		ldap_create_sort_keylist(&sortkeylist, SORTKEYLIST);
		rc = ldap_create_sort_control(ld, sortkeylist, 1, &sortctrl);
		if (rc != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		vlist.ldvlist_before_count = 0;
		vlist.ldvlist_after_count = LISTPAGESIZE-1;
		vlist.ldvlist_attrvalue = NULL;
		vlist.ldvlist_extradata = NULL;

	} else if (flag == SIMPLEPAGECTRLFLAG) {
		cookie = (struct berval *) calloc(1, sizeof (struct berval));
		if (cookie == NULL)
			return (NS_LDAP_MEMORY);
		cookie->bv_val = "";
		cookie->bv_len = 0;
	}


	while (!finished) {

	if (flag == VLVCTRLFLAG) {
		vlist.ldvlist_index = index;
		vlist.ldvlist_size = 0;
		if (vlvctrl)
			ldap_control_free(vlvctrl);
		vlvctrl = NULL;
		if ((rc = ldap_create_virtuallist_control(ld,
			&vlist, &vlvctrl)) != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		ctrls[0] = sortctrl;
		ctrls[1] = vlvctrl;
		ctrls[2] = NULL;
		rc = ldap_search_ext(ld, searchBase, searchScope,
			(char *) filter, (char **) attribute, 0,
			ctrls, NULL, 0, 0, &msgId);

	} else if (flag == SIMPLEPAGECTRLFLAG) {

		if (ctrls[0] != NULL)
			ldap_control_free(ctrls[0]);
		if ((rc = ldap_create_page_control(ld, LISTPAGESIZE, cookie,
				(char) 0, &ctrls[0])) != LDAP_SUCCESS) {
			/*
			 * XXX ber_bvfree causes memory corrupton.
			 *  Need to investigate
			 * ber_bvfree(cookie);
			*/
			free(cookie);
			cookie = NULL;
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		ctrls[1] = NULL;
		rc = ldap_search_ext(ld, searchBase, searchScope,
			(char *) filter, (char **) attribute, 0,
			ctrls, NULL, 0, 0, &msgId);

	} else {
		finished = 1;
		msgId = ldap_search(ld, searchBase, searchScope, (char *)filter,
			(char **)attribute, 0);

		/* process search error */

		if (msgId == -1) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
	}

	/* RESULT */

	while ((rc = ldap_result(ld, msgId, 0, (struct timeval *)NULL,
		&resultMsg)) == LDAP_RES_SEARCH_ENTRY) {

	    LDAPMessage	*e;
	    BerElement	*ber;
	    char	*attr;
	    if (nEntries == 0 && result == NULL) {

		if ((result = (ns_ldap_result_t *)
			calloc(1, sizeof (ns_ldap_result_t))) == NULL) {
				rc = NS_LDAP_MEMORY;
				goto cleanup;
			}
	    }

	    /* copy entry to result structure */

	    for (e = ldap_first_entry(ld, resultMsg); e != NULL;
		e = ldap_next_entry(ld, e)) {
		int j, k;
		char **vals;
		if (nEntries == 0) {

			result->entry = (ns_ldap_entry_t *)
			    calloc(1, sizeof (ns_ldap_entry_t));
			if (result->entry == NULL) {
				rc = NS_LDAP_MEMORY;
				goto cleanup;
			}
			curEntry = result->entry;

		} else {

			curEntry->next = (ns_ldap_entry_t *)
			    calloc(1, sizeof (ns_ldap_entry_t));
			if (curEntry->next == NULL) {
				rc = NS_LDAP_MEMORY;
				goto cleanup;
			}
			curEntry = curEntry->next;
		}
		curEntry->next = NULL;

		nEntries++;
		result->entries_count++;

		nAttrs = 1;	/* starts with 1 for the DN attribute */
		for (attr = ldap_first_attribute(ld, e, &ber);
			attr != NULL;
			attr = ldap_next_attribute(ld, e, ber)) {
			nAttrs++;
			ldap_memfree(attr);
		}
		ber_free(ber, 0);

		curEntry->attr_count = nAttrs;

		if ((curEntry->attr_pair = (ns_ldap_attr_t **)
		    calloc(curEntry->attr_count,
			sizeof (ns_ldap_attr_t *))) == NULL) {
				rc = NS_LDAP_MEMORY;
				goto cleanup;
			}

		/* DN attribute */
		dn = ldap_get_dn(ld, e);
		if ((curEntry->attr_pair[0] = (ns_ldap_attr_t *)
				calloc(1, sizeof (ns_ldap_attr_t))) == NULL) {
			ldap_memfree(dn);
			free(curEntry->attr_pair);
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}
		if ((curEntry->attr_pair[0]->attrname = strdup("dn")) == NULL) {
			ldap_memfree(dn);
			free(curEntry->attr_pair[0]);
			free(curEntry->attr_pair);
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}
		curEntry->attr_pair[0]->value_count = 1;
		if ((curEntry->attr_pair[0]->attrvalue = (char **)
				calloc(2, sizeof (char *))) == NULL) {
			ldap_memfree(dn);
			free(curEntry->attr_pair[0]);
			free(curEntry->attr_pair[0]->attrname);
			free(curEntry->attr_pair);
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}
		if ((curEntry->attr_pair[0]->attrvalue[0] = strdup(dn))
							== NULL) {
			ldap_memfree(dn);
			free(curEntry->attr_pair[0]);
			free(curEntry->attr_pair[0]->attrname);
			free(curEntry->attr_pair[0]->attrvalue);
			free(curEntry->attr_pair);
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}
		ldap_memfree(dn);

		/* other attributes */
		for (attr = ldap_first_attribute(ld, e, &ber), j = 1;
			attr != NULL && j != nAttrs;
			attr = ldap_next_attribute(ld, e, ber), j++) {
		    /* allocate new attr name */

		    if ((curEntry->attr_pair[j] = (ns_ldap_attr_t *)
			calloc(1, sizeof (ns_ldap_attr_t))) == NULL) {
				rc = NS_LDAP_MEMORY;
				ber_free(ber, 0);
				goto cleanup;
			}

		    if ((curEntry->attr_pair[j]->attrname = (char *)
			calloc(strlen(attr) + 1, sizeof (char))) == NULL) {
				rc = NS_LDAP_MEMORY;
				ber_free(ber, 0);
				goto cleanup;
			}

		    strcpy(curEntry->attr_pair[j]->attrname, attr);

		    if ((vals = ldap_get_values(ld, e, attr)) != NULL) {

			if (curEntry->attr_pair[j]->value_count =
				ldap_count_values(vals)) {

			    if ((curEntry->attr_pair[j]->attrvalue = (char **)
				calloc(curEntry->attr_pair[j]->value_count+1,
				sizeof (char *))) == NULL) {
					rc = NS_LDAP_MEMORY;
					ber_free(ber, 0);
					goto cleanup;
				}
			}

			for (k = 0;
			    k < curEntry->attr_pair[j]->value_count; k++) {

			    if ((curEntry->attr_pair[j]->attrvalue[k] =
				(char *)calloc(strlen(vals[k])+1,
				sizeof (char))) == NULL) {
					rc = NS_LDAP_MEMORY;
					ber_free(ber, 0);
					goto cleanup;
				}

			    strcpy(curEntry->attr_pair[j]->attrvalue[k],
				vals[k]);
			}

			curEntry->attr_pair[j]->attrvalue[k] = NULL;
			ldap_value_free(vals);
		    }

		    ldap_memfree(attr);
		}
		ber_free(ber, 0);
	    }

	    if (callback && result != NULL) {
		ns_ldap_entry_t * curEntry;
		rc = 0;

		for (curEntry = result->entry;
			curEntry != NULL;
			curEntry = curEntry->next) {

			rc = callback(curEntry, userdata);

			if (rc == NS_LDAP_CB_DONE) {
				/* callback doesn't want any more data */
				__ns_ldap_freeResult(&result);
				rc = NS_LDAP_PARTIAL;
				goto cleanup;
			} else if (rc != NS_LDAP_CB_NEXT) {
				/* invalid return code */
				__ns_ldap_freeResult(&result);
				rc = NS_LDAP_OP_FAILED;
				goto cleanup;

			}
		}
		__ns_ldap_freeResult(&result);
		nEntries = 0;
	    }
	    if (resultMsg)
		    ldap_msgfree(resultMsg);
	} /* end of result while loop */


	if (rc == -1) {

		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
		sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
		MKERROR(*error, ldaperrno, strdup(errstr), NULL);
		__ns_ldap_freeResult(&result);
		rc = NS_LDAP_INTERNAL;
		goto cleanup;

	} else if (finished && (rc == LDAP_RES_SEARCH_RESULT) &&
		(!result) && !callback) {
		rc = NS_LDAP_NOTFOUND;
		goto cleanup;

	} else if (finished && !result && !callback) {
		rc = NS_LDAP_NOTFOUND;
		goto cleanup;
	}

	if (flag == VLVCTRLFLAG) {
		int errCode;

		if ((rc = ldap_parse_result(ld, resultMsg, &errCode,
			NULL, NULL, NULL,
			&retCtrls, 0)) != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		if (retCtrls) {
			unsigned long target_posp = 0;
			unsigned long list_size = 0;

			if (ldap_parse_virtuallist_control(ld, retCtrls,
				&target_posp, &list_size, &errCode)
				== LDAP_SUCCESS) {
				index = target_posp + LISTPAGESIZE;
				if (index >= list_size) {
					finished = 1;
				}
			}
			ldap_controls_free(retCtrls);
			retCtrls = NULL;
		}
		else
			finished = 1;

	} else if (flag = SIMPLEPAGECTRLFLAG) {
		int errCode;

		if ((rc = ldap_parse_result(ld, resultMsg, &errCode,
			NULL, NULL, NULL,
			&retCtrls, 0)) != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*error, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		if (retCtrls) {
			unsigned int count = 0;

			if (cookie) {
				free(cookie);
				cookie = NULL;
			}

			if (ldap_parse_page_control(ld, retCtrls,
				&count, &cookie) == LDAP_SUCCESS) {
				if ((cookie == NULL) ||
					(cookie->bv_val == NULL) ||
					(*cookie->bv_val == NULL))
					finished = 1;
			}
			ldap_controls_free(retCtrls);
			retCtrls = NULL;
		}
		else
			finished = 1;
	}

	} /* end of while loop for search_ext */
	rc = NS_LDAP_SUCCESS;
cleanup:
	if (resultMsg)
		ldap_msgfree(resultMsg);

	*Rresult = result;
	if (ctrls[0] != NULL)
		ldap_control_free(ctrls[0]);
	if (ctrls[1] != NULL)
		ldap_control_free(ctrls[1]);
	/*
	 * XXX ber_bvfree causes memory corrupton.
	 *  Need to investigate
	 * if (cookie)
	 *	ber_bvfree(cookie);
	*/
	if (cookie)
		free(cookie);
	return (rc);

}

static int
__s_api_getEntry(
	LDAP * ld,
	LDAPMessage * e,
	ns_ldap_error_t **error,
	ns_ldap_result_t **result)
{

	ns_ldap_entry_t	*curEntry = NULL;
	BerElement	*ber;
	char		*attr = NULL;
	char		**vals = NULL;
	char		*dn;
	int		nAttrs = 0;
	int		j, k = 0;

#ifdef DEBUG
	fprintf(stderr, "__s_api_getEntry START\n");
#endif

	*result = NULL;
	*error = NULL;

	if (e == NULL) {
		return (NS_LDAP_INVALID_PARAM);
	}

	if ((*result = (ns_ldap_result_t *)
		calloc(1, sizeof (ns_ldap_result_t))) == NULL) {
			return (NS_LDAP_MEMORY);
	}
	(*result)->entry = (ns_ldap_entry_t *)
			    calloc(1, sizeof (ns_ldap_entry_t));
	if ((*result)->entry == NULL) {
		free (*result);
		return (NS_LDAP_MEMORY);
	}
	curEntry = (*result)->entry;

	curEntry->next = NULL;

	(*result)->entries_count++;

	nAttrs = 1;  /* start with 1 for the DN attr */
	for (attr = ldap_first_attribute(ld, e, &ber);
		attr != NULL;
		attr = ldap_next_attribute(ld, e, ber)) {
			nAttrs++;
			ldap_memfree(attr);
	}
	ber_free(ber, 0);

	curEntry->attr_count = nAttrs;

	if ((curEntry->attr_pair = (ns_ldap_attr_t **)
		calloc(curEntry->attr_count,
		    sizeof (ns_ldap_attr_t *))) == NULL) {
			return (NS_LDAP_MEMORY);
	}
	/* DN attribute */
	dn = ldap_get_dn(ld, e);
	if ((curEntry->attr_pair[0] = (ns_ldap_attr_t *)
		calloc(1, sizeof (ns_ldap_attr_t))) == NULL) {
			ldap_memfree(dn);
			free(curEntry->attr_pair);
			return (NS_LDAP_MEMORY);
		}

	if ((curEntry->attr_pair[0]->attrname = strdup("dn")) == NULL) {
		ldap_memfree(dn);
		free(curEntry->attr_pair[0]);
		free(curEntry->attr_pair);
		return (NULL);
	}
	curEntry->attr_pair[0]->value_count = 1;
	if ((curEntry->attr_pair[0]->attrvalue = (char **)
		calloc(2, sizeof (char *))) == NULL) {
		ldap_memfree(dn);
		free(curEntry->attr_pair[0]);
		free(curEntry->attr_pair[0]->attrname);
		free(curEntry->attr_pair);
		return (NS_LDAP_MEMORY);
	}
	if ((curEntry->attr_pair[0]->attrvalue[0] = strdup(dn))
			== NULL) {
		ldap_memfree(dn);
		free(curEntry->attr_pair[0]);
		free(curEntry->attr_pair[0]->attrname);
		free(curEntry->attr_pair[0]->attrvalue);
		free(curEntry->attr_pair);
		return (NS_LDAP_MEMORY);
	}
	ldap_memfree(dn);

	/* other attributes */
	for (attr = ldap_first_attribute(ld, e, &ber), j = 1;
		attr != NULL && j != nAttrs;
		attr = ldap_next_attribute(ld, e, ber), j++) {
	    /* allocate new attr name */

	    if ((curEntry->attr_pair[j] = (ns_ldap_attr_t *)
		calloc(1, sizeof (ns_ldap_attr_t))) == NULL) {
		    ber_free(ber, 0);
		    return (NS_LDAP_MEMORY);
	    }

	    if ((curEntry->attr_pair[j]->attrname = (char *)
		calloc(strlen(attr) + 1, sizeof (char))) == NULL) {
		    ber_free(ber, 0);
		    return (NS_LDAP_MEMORY);
	    }

	    strcpy(curEntry->attr_pair[j]->attrname, attr);

	    if ((vals = ldap_get_values(ld, e, attr)) != NULL) {

		if (curEntry->attr_pair[j]->value_count =
			ldap_count_values(vals)) {

		    if ((curEntry->attr_pair[j]->attrvalue = (char **)
			calloc(curEntry->attr_pair[j]->value_count+1,
			sizeof (char *))) == NULL) {
			    ber_free(ber, 0);
			    return (NS_LDAP_MEMORY);
		    }
		}

		for (k = 0;
		    k < curEntry->attr_pair[j]->value_count; k++) {

		    if ((curEntry->attr_pair[j]->attrvalue[k] =
			(char *)calloc(strlen(vals[k])+1,
			sizeof (char))) == NULL) {
			    ber_free(ber, 0);
			    return (NS_LDAP_MEMORY);
		    }
		    strcpy(curEntry->attr_pair[j]->attrvalue[k],
			vals[k]);
		}

		curEntry->attr_pair[j]->attrvalue[k] = NULL;
		ldap_value_free(vals);
	    }

	    ldap_memfree(attr);
	}

	ber_free(ber, 0);
	return (NS_LDAP_SUCCESS);
}


int
__ns_ldap_firstEntry(const char *database,
		    const char *filter,
		    const char * const *attribute,
		    const char *domain,
		    const Auth_t *auth,
		    const int flags,
		    void **cookie,
		    ns_ldap_result_t **result,
		    ns_ldap_error_t ** errorp)
{
	ConnectionID	connectionId = -1;
	ns_ldap_cookie_t	*cookieInfo;
	LDAPMessage	*resultMsg;
	LDAPMessage	*e;
	LDAP		*ld = NULL;
	char		*searchBase = NULL;
	char		**servers = NULL;
	char		**dns = NULL;
	char		errstr[MAXERROR];
	int		searchScope = 0;
	int		done = 0;
	int		rc = 0;
	int		ldaperrno = 0;
	int		i = 0;
	int		connectionCookie = 0;
	int		msgId;
	int		*intptr = NULL;
	int		ctrlFlag;
	LDAPControl	*ctrls[3] = { NULL, NULL, NULL };
	LDAPsortkey	**sortkeylist;
	LDAPControl	*sortctrl = NULL;
	LDAPControl	*vlvctrl = NULL;
	LDAPVirtualList	vlist;
	unsigned long index = 1;
	struct berval	*ctrlCookie;

#ifdef DEBUG
		fprintf(stderr, "__ns_ldap_firstEntry START\n");
#endif
	searchScope = flags & (NS_LDAP_SCOPE_BASE | NS_LDAP_SCOPE_ONELEVEL |
			NS_LDAP_SCOPE_SUBTREE);
	if ((rc = __s_api_getSearchScope(&searchScope, domain,
					errorp)) != NS_LDAP_SUCCESS)
		return (rc);

	if ((rc = __s_api_getDNs(&dns, NULL, database, errorp)) !=
	    NS_LDAP_SUCCESS)
		return (rc);

	rc = __s_api_getServers(&servers, NULL, errorp);
	if (rc != NS_LDAP_SUCCESS) {
		__s_api_free2dArray(dns);
		return (rc);
	}


	rc = __s_api_getConnection_ext(servers, domain, flags, auth,
		&ld, &connectionId, &connectionCookie, errorp);

	if (rc != NS_LDAP_SUCCESS) {
		if (rc == NS_LDAP_OP_FAILED)
			rc = NS_LDAP_INTERNAL;
		goto cleanup;
	}

	rc = __s_api_isCtrlSupported(ld, LDAP_CONTROL_VLVREQUEST, errorp);
	if (rc == NS_LDAP_SUCCESS) {
		ctrlFlag = VLVCTRLFLAG;
	} else if (rc == NS_LDAP_OP_FAILED) {
		rc = __s_api_isCtrlSupported(ld, LDAP_CONTROL_SIMPLE_PAGE,
			errorp);
		if (rc == NS_LDAP_SUCCESS) {
			ctrlFlag = SIMPLEPAGECTRLFLAG;
		} else if (rc == NS_LDAP_OP_FAILED) {
			ctrlFlag = 0;
		} else
			goto cleanup;
	} else
		goto cleanup;

	if (ctrlFlag == VLVCTRLFLAG) {
		ldap_create_sort_keylist(&sortkeylist, SORTKEYLIST);
		vlist.ldvlist_before_count = 0;
		vlist.ldvlist_after_count = ENUMPAGESIZE-1;
		vlist.ldvlist_attrvalue = NULL;
		vlist.ldvlist_extradata = NULL;

	} else if (ctrlFlag == SIMPLEPAGECTRLFLAG) {
		ctrlCookie = (struct berval *)
			calloc(1, sizeof (struct berval));
		if (ctrlCookie == NULL)
			return (NS_LDAP_MEMORY);
		ctrlCookie->bv_val = "";
		ctrlCookie->bv_len = 0;
	}
	done = 0;
	i = 0;
	while (((searchBase = dns[i]) != NULL) && !done) {

		*errorp = NULL;

		if (ctrlFlag == VLVCTRLFLAG) {
		if (sortctrl)
			ldap_control_free(sortctrl);
		rc = ldap_create_sort_control(ld, sortkeylist, 1, &sortctrl);
		if (rc != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		vlist.ldvlist_index = index;
		vlist.ldvlist_size = 0;

		if (vlvctrl)
			ldap_control_free(vlvctrl);
		vlvctrl = NULL;
		if ((rc = ldap_create_virtuallist_control(ld,
			&vlist, &vlvctrl)) != LDAP_SUCCESS) {
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		ctrls[0] = sortctrl;
		ctrls[1] = vlvctrl;
		ctrls[2] = NULL;
		rc = ldap_search_ext(ld, searchBase, searchScope,
			(char *) filter, (char **) attribute, 0,
			ctrls, NULL, 0, 0, &msgId);

		} else if (ctrlFlag == SIMPLEPAGECTRLFLAG) {

		if (ctrls[0] != NULL)
			ldap_controls_free(ctrls);
		if ((rc = ldap_create_page_control(ld, ENUMPAGESIZE, ctrlCookie,
				(char) 0, &ctrls[0])) != LDAP_SUCCESS) {
			/*
			 *  XXX ber_bvfree causes memory corrupton.
			 *  Need to investigate
			 * ber_bvfree(ctrlCookie);
			*/
			free(ctrlCookie);
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}
		ctrls[1] = NULL;
		rc = ldap_search_ext(ld, searchBase, searchScope,
			(char *) filter, (char **) attribute, 0,
			ctrls, NULL, 0, 0, &msgId);
		} else {
			msgId = ldap_search(ld, searchBase, searchScope,
			    (char *)filter, (char **)attribute,
			    0);
		}

		if (msgId == -1) {
			if (flags & NS_LDAP_HARD) {
				DropConnection(connectionId, 0);
				rc = __s_api_getConnection_ext(servers, domain,
					flags, auth, &ld, &connectionId,
					&connectionCookie, errorp);
				if (rc != NS_LDAP_SUCCESS) {
					if (rc == NS_LDAP_OP_FAILED)
						rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}
				continue;
				/* go to top of while loop and search again */
			}
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
			rc = NS_LDAP_INTERNAL;
			goto cleanup;
		}

		/*
		 * Poll the server for the results of the
		 * search operation. Passing LDAP_MSG_ONE
		 * indicates that you want to receive the
		 * entries one at a time, as they come in.
		 */

		rc = ldap_result(ld, msgId, LDAP_MSG_ONE, NULL,
				&resultMsg);

		switch (rc) {
		case -1:

		/* An error occurred. */
		if (flags & NS_LDAP_HARD) {
			DropConnection(connectionId, 0);
			ldap_msgfree(resultMsg);
			rc = __s_api_getConnection_ext(servers, domain,
				flags, auth, &ld, &connectionId,
				&connectionCookie, errorp);
			if (rc != NS_LDAP_SUCCESS) {
				if (rc == NS_LDAP_OP_FAILED)
					rc = NS_LDAP_INTERNAL;
				goto cleanup;
			}
			continue;
		}
		ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
		sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
		MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
		rc = NS_LDAP_INTERNAL;
		goto cleanup;

		case LDAP_RES_SEARCH_ENTRY:

		done = 1;

		e = ldap_first_entry(ld, resultMsg);
		rc = __s_api_getEntry(ld, e, errorp, result);

		if (*result == NULL)
			goto cleanup;

		/* else */

		intptr = (int *) calloc(1, sizeof (int));
		if (intptr == NULL) {
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}

		cookieInfo = calloc(1, sizeof (ns_ldap_cookie_t));
		if (cookieInfo == NULL) {
			rc = NS_LDAP_MEMORY;
			goto cleanup;
		}
		cookieInfo->msgId = msgId;

		if (flags & NS_LDAP_ALL_RES) {
			cookieInfo->searchScope = searchScope;
			cookieInfo->filter = strdup(filter);
			cookieInfo->attribute =
				__s_api_cp2dArray((char **)attribute);

			if (ctrlFlag == VLVCTRLFLAG) {
				cookieInfo->ctrlCookie = NULL;
				cookieInfo->vlvIndex = index;
				cookieInfo->currentdn = strdup(dns[i]);
				cookieInfo->flag = VLVCTRLFLAG;
				cookieInfo->dns =
					__s_api_cp2dArray(&dns[++i]);
			} else if (ctrlFlag == SIMPLEPAGECTRLFLAG) {
				cookieInfo->ctrlCookie = ctrlCookie;
				cookieInfo->currentdn = strdup(dns[i]);
				cookieInfo->flag = SIMPLEPAGECTRLFLAG;
				cookieInfo->dns =
					__s_api_cp2dArray(&dns[++i]);
			} else {
				cookieInfo->ctrlCookie = NULL;
				cookieInfo->currentdn = NULL;
				cookieInfo->flag = 0;
				cookieInfo->dns =
					__s_api_cp2dArray(&dns[++i]);
			}

		} else if (ctrlFlag == VLVCTRLFLAG) {
			cookieInfo->searchScope = searchScope;
			cookieInfo->filter = strdup(filter);
			cookieInfo->ctrlCookie = NULL;
			cookieInfo->vlvIndex = index;
			cookieInfo->currentdn = strdup(dns[i]);
			cookieInfo->flag = VLVCTRLFLAG;
			cookieInfo->dns = NULL;
			cookieInfo->attribute =
				__s_api_cp2dArray((char **)attribute);

		} else if (ctrlFlag == SIMPLEPAGECTRLFLAG) {
			cookieInfo->searchScope = searchScope;
			cookieInfo->filter = strdup(filter);
			cookieInfo->ctrlCookie = ctrlCookie;
			cookieInfo->currentdn = strdup(dns[i]);
			cookieInfo->flag = SIMPLEPAGECTRLFLAG;
			cookieInfo->dns = NULL;
			cookieInfo->attribute =
				__s_api_cp2dArray((char **)attribute);
		} else {
			cookieInfo->ctrlCookie = NULL;
			cookieInfo->dns = NULL;
			cookieInfo->flag = 0;
			cookieInfo->currentdn = NULL;
		}

		__s_api_setCookieInfo(connectionId, cookieInfo);
		*intptr = connectionId;
		*cookie = (void *) intptr;

		break;

		case LDAP_RES_SEARCH_REFERENCE:
			/*
			 * The server sent a search reference
			 * encountered during the search operation.
			 * Try the next dn (same as LDAP_RES_SEARCH_RESULT).
			 */

		case LDAP_RES_SEARCH_RESULT:
			/*
			 * Final result received from the server.
			 * check if error==NOT FOUND to return it
			 */
			i++;
			if (dns[i] == NULL) {
				/* No more DNs to check */
				rc = NS_LDAP_NOTFOUND;
				goto cleanup;
			}
			break;
		case 0:
			/*
			 * Timeout
			 */
			i++;
			if (dns[i] == NULL) {
				/* No more DNs to check */
				rc = NS_LDAP_NOTFOUND;
				goto cleanup;
			}
			break;

		default:
			break;
		} /* end of switch */

		ldap_msgfree(resultMsg);

	} /* end of while */

	rc = NS_LDAP_SUCCESS;

cleanup:
	if ((rc != NS_LDAP_SUCCESS) && (connectionId > -1))
		DropConnection(connectionId, 0);
	if (dns)
		__s_api_free2dArray(dns);
	if (servers)
		__s_api_free2dArray(servers);
	if (ctrls[0] != NULL)
		ldap_controls_free(ctrls);
	return (rc);
}

int
__ns_ldap_nextEntry(
	void *cookie,
	ns_ldap_result_t **result,
	ns_ldap_error_t ** errorp)
{

	ConnectionID	connectionId = -1;
	ns_ldap_cookie_t	*cookieInfo;
	LDAPMessage	*resultMsg;
	LDAPMessage	*e;
	LDAP 		*ld = NULL;
	int		rc = 0;
	int		ldaperrno = 0;
	int		i = 0;
	int		done = 0;
	char		**dns;
	char		errstr[MAXERROR];
	int		*c;
	int		entriesLeft = 0;
	int		count = 0;
	LDAPControl	*ctrls[3] = { NULL, NULL, NULL };
	LDAPsortkey	**sortkeylist;
	LDAPControl	*sortctrl = NULL;
	LDAPControl	*vlvctrl = NULL;
	LDAPControl	**retCtrls = NULL;
	LDAPVirtualList	vlist;
	unsigned long index = 1;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_nextEntry START\n");
#endif
	if (cookie == NULL)
		return (NS_LDAP_INVALID_PARAM);

	*result = NULL;
	*errorp = NULL;
	c = (int *)cookie;
	connectionId = (ConnectionID) *c;

	rc = __s_api_getCookieInfo(connectionId, &cookieInfo, &ld);
	if (rc == -1)
		return (NS_LDAP_INVALID_PARAM);

	if (cookieInfo->flag == VLVCTRLFLAG) {
		ldap_create_sort_keylist(&sortkeylist, SORTKEYLIST);
	}
	while (!done) {

	    rc = ldap_result(ld, cookieInfo->msgId, LDAP_MSG_ONE, NULL,
		&resultMsg);

	    switch (rc) {
		case -1:
#ifdef DEBUG
			fprintf(stderr,
			"__ns_ldap_nextEntry: ldap_result returned -1\n");
#endif
			/* An error occurred. */
			ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &ldaperrno);
			sprintf(errstr, gettext(ldap_err2string(ldaperrno)));
			MKERROR(*errorp, ldaperrno, strdup(errstr), NULL);
			return (NS_LDAP_INTERNAL);

		case LDAP_RES_SEARCH_ENTRY:

			done = 1;
			e = ldap_first_entry(ld, resultMsg);
			rc = __s_api_getEntry(ld, e, errorp, result);

			if ((*result) != NULL) {
#ifdef DEBUG
				__s_api_printResult(*result);
#endif
				__s_api_setCookieInfo(connectionId, cookieInfo);
				*c = connectionId;
			} else {
#ifdef DEBUG
				fprintf(stderr,
				"__ns_ldap_nextEntry: no result\n");
#endif
				ldap_msgfree(resultMsg);
				__s_api_free2dArray(cookieInfo->dns);
				__s_api_free2dArray(cookieInfo->attribute);
				free(cookieInfo->filter);
				free(cookieInfo);
				DropConnection(connectionId, 0);
				return (NS_LDAP_INTERNAL);
			}
			break;

		case LDAP_RES_SEARCH_REFERENCE:
			/*
			 * The server sent a search reference
			 * encountered during the search operation.
			 * Do same as case LDAP_RES_SEARCH_RESULT
			*/

#ifdef DEBUG
			fprintf(stderr,
			"__ns_ldap_nextEntry: REFERENCE returned\n");
#endif
		case LDAP_RES_SEARCH_RESULT:
			/* Final result received from the server. */
			/* check if error == NO OBJECT FOUND to return it */

			if (cookieInfo->flag == VLVCTRLFLAG) {
				int errCode;

				if ((rc = ldap_parse_result(ld, resultMsg,
					&errCode, NULL, NULL, NULL,
					&retCtrls, 0)) != LDAP_SUCCESS) {
					ldap_get_option(ld,
						LDAP_OPT_ERROR_NUMBER,
						&ldaperrno);
					sprintf(errstr,
					gettext(ldap_err2string(ldaperrno)));
					MKERROR(*errorp, ldaperrno,
						strdup(errstr), NULL);
					rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}
				if (retCtrls) {
					unsigned long target_posp = 0;
					unsigned long list_size = 0;

					if (ldap_parse_virtuallist_control(ld,
						retCtrls, &target_posp,
						&list_size, &errCode)
						== LDAP_SUCCESS) {
						index = target_posp +
							ENUMPAGESIZE;
						if (index >= list_size) {
							entriesLeft = 0;
						} else
							entriesLeft = 1;
					}
					ldap_controls_free(retCtrls);
					retCtrls = NULL;
				}
				else
					goto cleanup;

			} else if (cookieInfo->flag == SIMPLEPAGECTRLFLAG) {
				rc = __s_api_parsePageControl(ld, resultMsg,
					&count, &cookieInfo->ctrlCookie,
					errorp);
				if (rc == NS_LDAP_OP_FAILED) {
					cookieInfo->currentdn = NULL;
					entriesLeft = 0;
				} else if (rc == NS_LDAP_SUCCESS)
					entriesLeft = 1;
				else
					goto cleanup;
			}
			ldap_msgfree(resultMsg);

			if (cookieInfo->flag == VLVCTRLFLAG) {
				if (!entriesLeft) {
					if ((cookieInfo->dns != NULL) &&
					(cookieInfo->dns[0] != NULL)) {
						free(cookieInfo->currentdn);
						cookieInfo->currentdn =
						strdup(cookieInfo->dns[0]);
						dns = (char **)
							__s_api_cp2dArray(
							&(cookieInfo->dns[1]));
						__s_api_free2dArray(
							cookieInfo->dns);
						cookieInfo->dns = dns;
					} else {
						free(cookieInfo->currentdn);
						cookieInfo->currentdn = NULL;
						__s_api_free2dArray(
							cookieInfo->dns);
						__s_api_free2dArray(
							cookieInfo->attribute);
						free(cookieInfo->filter);
						free(cookieInfo);
						DropConnection(connectionId,
							DEL_CONNECTION);
						return (NS_LDAP_NOTFOUND);
					}
				}
				if (ctrls[0] != NULL)
					ldap_control_free(ctrls[0]);
				if (sortctrl)
					ldap_control_free(sortctrl);
				rc = ldap_create_sort_control(ld,
					sortkeylist, 1, &sortctrl);
				if (rc != LDAP_SUCCESS) {

					ldap_get_option(ld,
						LDAP_OPT_ERROR_NUMBER,
						&ldaperrno);
					sprintf(errstr,
					gettext(ldap_err2string(ldaperrno)));
					MKERROR(*errorp, ldaperrno,
						strdup(errstr), NULL);
					rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}
				index = cookieInfo->vlvIndex + ENUMPAGESIZE;
				cookieInfo->vlvIndex = index;
				vlist.ldvlist_before_count = 0;
				vlist.ldvlist_after_count = ENUMPAGESIZE-1;
				vlist.ldvlist_attrvalue = NULL;
				vlist.ldvlist_index = index;
				vlist.ldvlist_size = 0;
				vlist.ldvlist_extradata = NULL;
				if (vlvctrl)
					ldap_control_free(vlvctrl);
				vlvctrl = NULL;
				if ((rc = ldap_create_virtuallist_control(ld,
					&vlist, &vlvctrl)) != LDAP_SUCCESS) {
					ldap_get_option(ld,
						LDAP_OPT_ERROR_NUMBER,
						&ldaperrno);
					sprintf(errstr,
					gettext(ldap_err2string(ldaperrno)));
					MKERROR(*errorp, ldaperrno,
						strdup(errstr), NULL);
					rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}
				if (vlvctrl == NULL) {
				}
				ctrls[0] = sortctrl;
				ctrls[1] = vlvctrl;
				ctrls[2] = NULL;
				rc = ldap_search_ext(ld,
					cookieInfo->currentdn,
					cookieInfo->searchScope,
					cookieInfo->filter,
					cookieInfo->attribute, 0,
					ctrls, NULL, 0, 0, &cookieInfo->msgId);

			} else if (cookieInfo->flag == SIMPLEPAGECTRLFLAG) {

				if (!entriesLeft) {
					if ((cookieInfo->dns != NULL) &&
					(cookieInfo->dns[0] != NULL)) {
						free(cookieInfo->currentdn);
						cookieInfo->currentdn =
						strdup(cookieInfo->dns[0]);
						dns = (char **)
							__s_api_cp2dArray(
							&(cookieInfo->dns[1]));
						__s_api_free2dArray(
							cookieInfo->dns);
						cookieInfo->dns = dns;
					} else {
						free(cookieInfo->currentdn);
						cookieInfo->currentdn = NULL;
						__s_api_free2dArray(
							cookieInfo->dns);
						__s_api_free2dArray(
							cookieInfo->attribute);
						free(cookieInfo->filter);
						free(cookieInfo);
						DropConnection(connectionId,
							DEL_CONNECTION);
						return (NS_LDAP_NOTFOUND);
					}
				}
				if (ctrls[0] != NULL)
					ldap_control_free(ctrls[0]);
				if ((rc = ldap_create_page_control(ld,
					ENUMPAGESIZE,
					cookieInfo->ctrlCookie,
					(char) 0, &ctrls[0])) != LDAP_SUCCESS) {
					ldap_get_option(ld,
						LDAP_OPT_ERROR_NUMBER,
						&ldaperrno);
					sprintf(errstr,
						gettext(
						ldap_err2string(ldaperrno)));
					MKERROR(*errorp, ldaperrno,
						strdup(errstr), NULL);
					rc = NS_LDAP_INTERNAL;
					goto cleanup;
				}
				ctrls[1] = NULL;
				rc = ldap_search_ext(ld,
					cookieInfo->currentdn,
					cookieInfo->searchScope,
					cookieInfo->filter,
					cookieInfo->attribute, 0,
					ctrls, NULL, 0, 0, &cookieInfo->msgId);

			} else if ((cookieInfo->dns != NULL) &&
					(cookieInfo->dns[i] != NULL)) {
				cookieInfo->msgId = ldap_search(ld,
						cookieInfo->dns[i],
						cookieInfo->searchScope,
						cookieInfo->filter,
						cookieInfo->attribute, 0);
				i++;
				dns = (char **)
					__s_api_cp2dArray(
						&(cookieInfo->dns[i]));
				__s_api_free2dArray(
					cookieInfo->dns);
				cookieInfo->dns = dns;
			} else {
				__s_api_free2dArray(cookieInfo->dns);
				__s_api_free2dArray(cookieInfo->attribute);
				free(cookieInfo->filter);
				free(cookieInfo);
				DropConnection(connectionId, DEL_CONNECTION);
				return (NS_LDAP_NOTFOUND);
			}
			break;

		default:
#ifdef DEBUG
			fprintf(stderr,
			"__ns_ldap_nextEntry: ldap_result returns %d\n", rc);
#endif
			break;

	    }
	}
cleanup:
	if (ctrls[0] != NULL)
		ldap_control_free(ctrls[0]);
	return (NS_LDAP_SUCCESS);
}


int
__ns_ldap_endEntry(
	void **cookie,
	ns_ldap_error_t ** errorp)
{
	ConnectionID		connectionId = -1;
	ns_ldap_cookie_t	*cookieInfo;
	LDAP			*ld = NULL;
	int			rc = 0;
	int			*c;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_endEntry START\n");
#endif

	if (cookie == NULL)
		return (NS_LDAP_INVALID_PARAM);

	*errorp = NULL;
	c = (int *)*cookie;
	connectionId = (ConnectionID)(*c);

	rc = __s_api_getCookieInfo(connectionId, &cookieInfo, &ld);

	if (rc == -1) {
		free(*cookie);
		*cookie = NULL;
		return (NS_LDAP_INVALID_PARAM);
	}

	__s_api_free2dArray(cookieInfo->dns);
	__s_api_free2dArray(cookieInfo->attribute);
	if (cookieInfo->currentdn)
		free(cookieInfo->currentdn);
	if (cookieInfo->ctrlCookie)
		free(cookieInfo->ctrlCookie);
	free(cookieInfo->filter);
	free(cookieInfo);
	DropConnection(connectionId, 0);
	free(*cookie);
	*cookie = NULL;
	return (NS_LDAP_SUCCESS);
}


int
__ns_ldap_freeResult(ns_ldap_result_t **result)
{

	ns_ldap_entry_t	*curEntry = NULL;
	ns_ldap_entry_t	*delEntry = NULL;
	int		i, j, k = 0;
	ns_ldap_result_t	*res = *result;

#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_freeResult START\n");
#endif
	if (res == NULL)
		return (NS_LDAP_INVALID_PARAM);

	if (res->entry != NULL)
		curEntry = res->entry;

	for (i = 0; i < res->entries_count; i++) {

		for (j = 0; j < curEntry->attr_count; j++) {

			free(curEntry->attr_pair[j]->attrname);
			for (k = 0;
			    (k < curEntry->attr_pair[j]->value_count) &&
			    (curEntry->attr_pair[j]->attrvalue[k]);
				k++) {
			    free(curEntry->attr_pair[j]->attrvalue[k]);
			}
			free(curEntry->attr_pair[j]->attrvalue);
			free(curEntry->attr_pair[j]);
		}
		free(curEntry->attr_pair);
		delEntry = curEntry;
		curEntry = curEntry->next;
		free(delEntry);
	}

	free(res);
	*result = NULL;
	return (NS_LDAP_SUCCESS);
}

int
__ns_ldap_auth(const Auth_t *auth,
		    const char *domain,
		    const int flags,
		    ns_ldap_error_t **errorp)
{

	ConnectionID	connectionId = -1;
	LDAP		*ld;
	char 		**servers = NULL;
	int		rc = 0;
	int		connectionCookie = 0;


#ifdef DEBUG
	fprintf(stderr, "__ns_ldap_auth START\n");
#endif

	*errorp = NULL;
	if (!auth)
		return (NS_LDAP_INVALID_PARAM);
	if ((rc = __s_api_getServers(&servers, NULL, errorp)) !=
	    NS_LDAP_SUCCESS) {
		return (rc);
	}

	rc = __s_api_getConnection_ext(servers, domain, flags, auth,
		&ld, &connectionId, &connectionCookie, errorp);
	if (rc == NS_LDAP_OP_FAILED && *errorp)
		__ns_ldap_freeError(errorp);

	if (connectionId > -1)
		DropConnection(connectionId, 0);
	__s_api_free2dArray(servers);
	return (rc);
}

char **
__ns_ldap_getAttr(const ns_ldap_entry_t *entry, const char *attrname)
{
	int	i;

	if (entry == NULL)
		return (NULL);
	for (i = 0; i < entry->attr_count; i++) {
		if (strcasecmp(entry->attr_pair[i]->attrname, attrname) == NULL)
			return (entry->attr_pair[i]->attrvalue);
	}
	return (NULL);
}


/*ARGSUSED*/
int
__ns_ldap_uid2dn(const char *uid,
		const char *domain,
		char **userDN,
		const char *cred,	/* cred is ignored */
		ns_ldap_error_t **errorp)
{
	ns_ldap_result_t	*result = NULL;
	char		*filter;
	char		errstr[MAXERROR];
	char		**value;
	int		rc = 0;
	int		i = 0;

	*errorp = NULL;
	*userDN = NULL;
	if ((uid == NULL) || (uid[0] == '\0'))
		return (NS_LDAP_INVALID_PARAM);

	while (uid[i] != '\0') {
		if (uid[i] == '=') {
			*userDN = strdup(uid);
			return (NS_LDAP_SUCCESS);
		}
		i++;
	}
	i = 0;
	while ((uid[i] != '\0') && (isdigit(uid[i])))
		i++;
	if (uid[i] == '\0') {
		filter = (char *)malloc(strlen(UIDNUMFILTER) + strlen(uid) + 1);
		if (filter == NULL) {
			*userDN = NULL;
			return (NS_LDAP_MEMORY);
		}
		sprintf(filter, UIDNUMFILTER, uid);
	} else {
		filter = (char *)malloc(strlen(UIDFILTER) + strlen(uid) + 1);
		if (filter == NULL) {
			*userDN = NULL;
			return (NS_LDAP_MEMORY);
		}
		sprintf(filter, UIDFILTER, uid);
	}

	rc = __ns_ldap_list("passwd", filter,
			    NULL, NULL, NULL, 0,
			    &result, errorp, NULL, NULL);
	free(filter);
	if (rc != NS_LDAP_SUCCESS) {
		if (result)
			__ns_ldap_freeResult(&result);
		return (rc);
	}
	if (result->entries_count > 1) {
		__ns_ldap_freeResult(&result);
		*userDN = NULL;
		sprintf(errstr,
			gettext("Too many entries are returned for %s"), uid);
		MKERROR(*errorp, NS_LDAP_INTERNAL, strdup(errstr), NULL);
		return (NS_LDAP_INTERNAL);
	}

	value = __ns_ldap_getAttr(result->entry, "dn");
	*userDN = strdup(value[0]);
	__ns_ldap_freeResult(&result);
	return (NS_LDAP_SUCCESS);
}


/*ARGSUSED*/
int
__ns_ldap_host2dn(const char *host,
		const char *domain,
		char **hostDN,
		const char *cred,	/* cred is ignored */
		ns_ldap_error_t **errorp)
{
	ns_ldap_result_t	*result = NULL;
	char		*filter;
	char		errstr[MAXERROR];
	char		**value;
	int		rc;

	*errorp = NULL;
	*hostDN = NULL;
	if ((host == NULL) || (host[0] == '\0'))
		return (NS_LDAP_INVALID_PARAM);

	filter = (char *)malloc(strlen(HOSTFILTER) + strlen(host) + 1);
	if (filter == NULL) {
		return (NS_LDAP_MEMORY);
	}
	sprintf(filter,	HOSTFILTER, host);

	rc = __ns_ldap_list("hosts", filter, NULL, NULL, NULL, 0, &result,
				errorp, NULL, NULL);
	free(filter);
	if (rc != NS_LDAP_SUCCESS) {
		if (result)
			__ns_ldap_freeResult(&result);
		return (rc);
	}

	if (result->entries_count > 1) {
		__ns_ldap_freeResult(&result);
		*hostDN = NULL;
		sprintf(errstr,
			gettext("Too many entries are returned for %s"), host);
		MKERROR(*errorp, NS_LDAP_INTERNAL, strdup(errstr), NULL);
		return (NS_LDAP_INTERNAL);
	}

	value = __ns_ldap_getAttr(result->entry, "dn");
	*hostDN = strdup(value[0]);
	__ns_ldap_freeResult(&result);
	return (NS_LDAP_SUCCESS);
}
