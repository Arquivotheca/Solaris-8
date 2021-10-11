/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getprotoent.c 1.2     99/10/22 SMI"

#include <ctype.h>
#include <netdb.h>
#include "ldap_common.h"

/* protocols attributes filters */
#define	_P_NAME			"cn"
#define	_P_PROTO		"ipprotocolnumber"
#define	_F_GETPROTOBYNAME	"(&(objectClass=ipProtocol)(cn=%s))"
#define	_F_GETPROTOBYNUMBER	\
	"(&(objectClass=ipProtocol)(ipProtocolNumber=%d))"

static const char *protocols_attrs[] = {
	_P_NAME,
	_P_PROTO,
	(char *)NULL
};


/*
 * _nss_ldap_protocols2ent is the data marshaling method for the protocols
 * getXbyY * (e.g., getbyname(), getbynumber(), getent()) backend processes.
 * This method is called after a successful ldap search has been performed.
 * This method will parse the ldap search values into *proto = (struct
 * protoent *)argp->buf.result which the frontend process expects. Three error
 * conditions are expected and returned to nsswitch.
 */

static int
_nss_ldap_protocols2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i, j;
	int		nss_result;
	int		buflen = (int)0;
	int		firstime = (int)1;
	unsigned long	len = 0L;
	char		*cp, **mp;
	char		*buffer = (char *)NULL;
	char		*ceiling = (char *)NULL;
	struct protoent	*proto = (struct protoent *)NULL;
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;

	buffer = (char *)argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_pls2ent;
	}
	proto = (struct protoent *)argp->buf.result;
	ceiling = buffer + buflen;

	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(argp->buf.buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_pls2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_pls2ent;
		}
		if (strcasecmp(attrptr->attrname, _P_NAME) == 0) {
			for (j = 0; j < attrptr->value_count; j++) {
				if (firstime) {
					/* protocol name */
					len = strlen(attrptr->attrvalue[j]);
					if (len < 1 ||
					    (attrptr->attrvalue[j] == '\0')) {
						nss_result =
						    (int)NSS_STR_PARSE_PARSE;
						goto result_pls2ent;
					}
					proto->p_name = buffer;
					buffer += len + 1;
					if (buffer >= ceiling) {
						nss_result =
						    (int)NSS_STR_PARSE_ERANGE;
						goto result_pls2ent;
					}
					(void) strcpy(proto->p_name,
						    attrptr->attrvalue[j]);
					mp = proto->p_aliases =
						(char **)ROUND_UP(buffer,
						sizeof (char **));
					buffer = (char *)proto->p_aliases +
						sizeof (char *) *
						(attrptr->value_count + 1);
					buffer = (char *)ROUND_UP(buffer,
						sizeof (char **));
					if (buffer >= ceiling) {
						nss_result =
						    (int)NSS_STR_PARSE_ERANGE;
						goto result_pls2ent;
					}
					firstime = (int)0;
				}
				/* alias list */
				len = strlen(attrptr->attrvalue[j]);
				if (len < 1 ||
				    (attrptr->attrvalue[j] == '\0')) {
					nss_result = (int)NSS_STR_PARSE_PARSE;
					goto result_pls2ent;
				}
				*mp = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_pls2ent;
				}
				/* uppercase matching protocol name */
				if (strcmp(proto->p_name,
				    attrptr->attrvalue[j]) != 0) {
					for (cp = attrptr->attrvalue[j];
					    *cp; cp++)
						*cp = toupper(*cp);
				}
				(void) strcpy(*mp++, attrptr->attrvalue[j]);
				continue;
			}
		}
		if (strcasecmp(attrptr->attrname, _P_PROTO) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len == 0 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_pls2ent;
			}
			errno = 0;
			proto->p_proto = (int)strtol(attrptr->attrvalue[0],
					    (char **)NULL, 10);
			if (errno != 0) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_pls2ent;
			}
			continue;
		}
	}

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getprotoent.c: _nss_ldap_protocols2ent]\n");
	(void) fprintf(stdout, "        p_name: [%s]\n", proto->p_name);
	if (mp != NULL) {
		for (mp = proto->p_aliases; *mp != NULL; mp++)
			(void) fprintf(stdout, "     p_aliases: [%s]\n", *mp);
	}
	(void) fprintf(stdout, "       p_proto: [%d]\n", proto->p_proto);
#endif DEBUG

result_pls2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


/*
 * getbyname gets struct protoent values by protocol name. This
 * function constructs an ldap search filter using the protocol
 * name invocation parameter and the getprotobyname search filter
 * defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into *proto = (struct *
 * protoent *)argp->buf.result. The function _nss_ldap_protocols2ent
 * performs the data marshaling.
 */

static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		searchfilter[SEARCHFILTERLEN];

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETPROTOBYNAME, argp->key.name) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_PROTOCOLS, searchfilter, NULL));
}


/*
 * getbynumber gets struct protoent values by protocol number. This
 * function constructs an ldap search filter using the protocol
 * name invocation parameter and the getprotobynumber search filter
 * defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into *proto = (struct *
 * protoent *)argp->buf.result. The function _nss_ldap_protocols2ent
 * performs the data marshaling.
 */

static nss_status_t
getbynumber(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char		numstr[12];
	char		searchfilter[SEARCHFILTERLEN];

	if (snprintf(numstr, sizeof (numstr), " %d", argp->key.number) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	if (snprintf(searchfilter, SEARCHFILTERLEN,
		_F_GETPROTOBYNUMBER, argp->key.number) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_PROTOCOLS, searchfilter, NULL));
}

static ldap_backend_op_t proto_ops[] = {
	_nss_ldap_destr,
	_nss_ldap_endent,
	_nss_ldap_setent,
	_nss_ldap_getent,
	getbyname,
	getbynumber
};


/*
 * _nss_ldap_protocols_constr is where life begins. This function calls
 * the generic ldap constructor function to define and build the abstract
 * data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_protocols_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

	return ((nss_backend_t *)_nss_ldap_constr(proto_ops,
		sizeof (proto_ops)/sizeof (proto_ops[0]), _PROTOCOLS,
		protocols_attrs, _nss_ldap_protocols2ent));
}
