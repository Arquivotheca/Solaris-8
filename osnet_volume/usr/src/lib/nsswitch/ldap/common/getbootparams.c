/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getbootparams.c 1.3     99/10/22 SMI"

#include "ldap_common.h"

/* bootparams attributes filters */
#define	_B_HOSTNAME		"cn"
#define	_B_PARAMETER		"bootparameter"
#define	_F_GETBOOTPARAMBYNAME	"(&(objectClass=bootableDevice)(cn=%s))"

static const char *bootparams_attrs[] = {
	_B_HOSTNAME,
	_B_PARAMETER,
	(char *)NULL
};

/*
 * _nss_ldap_bootparams2ent is the data marshaling method for the
 * bootparams getXbyY (e.g., getbyname()) backend processes. This
 * method is called after a successful ldap search has been performed.
 * This method will parse the ldap search values into argp->buf.buffer
 * Three error conditions are expected and returned to nsswitch.
 *
 * A host's bootparameters are returned on one line separated by white
 * space. Slapd stores each boot parameter as a separate entry. If more
 * than one bootparameter is available, a white space separated buffer
 * must be constructed and returned.
 */

static int
_nss_ldap_bootparams2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i, j, nss_result;
	int		buflen = (int)0;
	int		firstime = (int)1;
	unsigned long	len = 0L;
	char		*cp = (char *)NULL;
	char		*buffer = (char *)NULL;
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;

	buffer = argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;

	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_bp2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_bp2ent;
		}
		if (strcasecmp(attrptr->attrname, _B_PARAMETER) == 0) {
			for (j = 0; j < attrptr->value_count; j++) {
				len = strlen(attrptr->attrvalue[j]);
				if (len < 1 ||
				    (attrptr->attrvalue[j] == '\0')) {
					*buffer = 0;
					nss_result = (int)NSS_STR_PARSE_PARSE;
					goto result_bp2ent;
				}
				if (len > buflen) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_bp2ent;
				}
				if (firstime) {
					(void) strcpy(buffer,
					    attrptr->attrvalue[j]);
					firstime = (int)0;
				} else {
					if ((cp = strrchr(buffer, '\0'))
					    != NULL)
						*cp = ' ';
					(void) strcat(buffer,
					    attrptr->attrvalue[j]);
				}
			}
		}
	}

#ifdef DEBUG
	(void) fprintf(stdout, "\n[bootparams_getbyname.c: "
		    "_nss_ldap_bootparams2ent]\n");
	(void) fprintf(stdout, " bootparameter: [%s]\n", buffer);
#endif DEBUG

result_bp2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}

/*
 * getbyname gets bootparameters by host name. This function constructs an
 * ldap search filter using the host name invocation parameter and the
 * getbootparambyname search filter defined. Once the filter is
 * constructed, we search for matching entries and marshal the data
 * results into argp->buf.buffer for the frontend process. The function
 * _nss_ldap_bootparams2ent performs the data marshaling.
 *
 * RFC 2307, An Approach for Using LDAP as a Network Information Service,
 * indicates that dn's be fully qualified. Host name searches will be on
 * fully qualified host names (e.g., foo.bar.sun.com).
 */

static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	char		hostname[MAXHOSTNAMELEN];
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

	(void) strcpy(hostname, argp->key.name);
	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETBOOTPARAMBYNAME, hostname) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_BOOTPARAMS, searchfilter, NULL));
}


static ldap_backend_op_t bootparams_ops[] = {
	_nss_ldap_destr,
	getbyname
};


/*
 * _nss_ldap_bootparams_constr is where life begins. This function calls
 * the generic ldap constructor function to define and build the abstract
 * data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_bootparams_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

	return ((nss_backend_t *)_nss_ldap_constr(bootparams_ops,
		sizeof (bootparams_ops)/sizeof (bootparams_ops[0]),
		_BOOTPARAMS, bootparams_attrs, _nss_ldap_bootparams2ent));
}
