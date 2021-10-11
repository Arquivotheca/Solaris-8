/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)getservent.c 1.2     99/10/22 SMI"

#include <ctype.h>
#include <netdb.h>
#include "ldap_common.h"

/* services attributes filters */
#define	_S_NAME			"cn"
#define	_S_PORT			"ipserviceport"
#define	_S_PROTOCOL		"ipserviceprotocol"
#define	_F_GETSERVBYNAME	"(&(objectClass=ipService)(cn=%s))"
#define	_F_GETSERVBYNAMEPROTO	\
	"(&(objectClass=ipService)(cn=%s)(ipServiceProtocol=%s))"
#define	_F_GETSERVBYPORT	"(&(objectClass=ipService)(ipServicePort=%ld))"
#define	_F_GETSERVBYPORTPROTO	\
	"(&(objectClass=ipService)(ipServicePort=%ld)(ipServiceProtocol=%s))"

static const char *services_attrs[] = {
	_S_NAME,
	_S_PORT,
	_S_PROTOCOL,
	(char *)NULL
};


/*
 * _nss_ldap_services2ent is the data marshaling method for the services
 * getXbyY * (e.g., getbyname(), getbyport(), getent()) backend processes.
 * This method is called after a successful ldap search has been performed.
 * This method will parse the ldap search values into *serv = (struct
 * servent *)argp->buf.result which the frontend process expects. Three error
 * conditions are expected and returned to nsswitch.
 */

static int
_nss_ldap_services2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		i, j;
	int		nss_result;
	int		buflen = (int)0;
	int		firstime = (int)1;
	unsigned long	len = 0L;
	char		**mp;
	char		*buffer = (char *)NULL;
	char		*ceiling = (char *)NULL;
	struct servent *serv = (struct servent *)NULL;
	ns_ldap_result_t	*result = be->result;
	ns_ldap_attr_t	*attrptr;

	buffer = (char *)argp->buf.buffer;
	buflen = (size_t)argp->buf.buflen;
	serv = (struct servent *)argp->buf.result;
	ceiling = buffer + buflen;
#ifdef DEBUG
	(void) fprintf(stderr, "[getservent.c: _nss_ldap_services2ent]\n");
#endif DEBUG

	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(argp->buf.buffer, 0, buflen);

	attrptr = getattr(result, 0);
	if (attrptr == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_srvs2ent;
	}

	for (i = 0; i < result->entry->attr_count; i++) {
		attrptr = getattr(result, i);
		if (attrptr == NULL) {
			nss_result = (int)NSS_STR_PARSE_PARSE;
			goto result_srvs2ent;
		}
		if (strcasecmp(attrptr->attrname, _S_NAME) == 0) {
			for (j = 0; j < attrptr->value_count; j++) {
				if (firstime) {
					/* service name */
					len = strlen(attrptr->attrvalue[j]);
					if (len < 1 ||
					    (attrptr->attrvalue[j] == '\0')) {
						nss_result =
						    (int)NSS_STR_PARSE_PARSE;
						goto result_srvs2ent;
					}
					serv->s_name = buffer;
					buffer += len + 1;
					if (buffer >= ceiling) {
						nss_result =
						    (int)NSS_STR_PARSE_ERANGE;
						goto result_srvs2ent;
					}
					(void) strcpy(serv->s_name,
						    attrptr->attrvalue[j]);

					/* alias list */
					mp = serv->s_aliases =
						(char **)ROUND_UP(buffer,
						sizeof (char **));
					buffer = (char *)serv->s_aliases +
						sizeof (char *) *
						(attrptr->value_count + 1);
					buffer = (char *)ROUND_UP(buffer,
						sizeof (char **));
					if (buffer >= ceiling) {
						nss_result =
						    (int)NSS_STR_PARSE_ERANGE;
						goto result_srvs2ent;
					}
					firstime = (int)0;
					continue;
				}
				/* alias list */
				len = strlen(attrptr->attrvalue[j]);
				if (len < 1 ||
				    (attrptr->attrvalue[j] == '\0')) {
					nss_result = (int)NSS_STR_PARSE_PARSE;
					goto result_srvs2ent;
				}
				*mp = buffer;
				buffer += len + 1;
				if (buffer >= ceiling) {
					nss_result = (int)NSS_STR_PARSE_ERANGE;
					goto result_srvs2ent;
				}
				(void) strcpy(*mp++, attrptr->attrvalue[j]);
				continue;
			}
		}

		if (strcasecmp(attrptr->attrname, _S_PORT) == 0) {
			len = strlen(attrptr->attrvalue[0]);
			if (len == 0 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_srvs2ent;
			}
			serv->s_port =
				    htons((u_short)atoi(attrptr->attrvalue[0]));
			continue;
		}

		if (strcasecmp(attrptr->attrname, _S_PROTOCOL) == 0) {
			/* protocol name */
			len = strlen(attrptr->attrvalue[0]);
			if (len < 1 || (attrptr->attrvalue[0] == '\0')) {
				nss_result = (int)NSS_STR_PARSE_PARSE;
				goto result_srvs2ent;
			}
			serv->s_proto = buffer;
			buffer += len + 1;
			if (buffer >= ceiling) {
				nss_result = (int)NSS_STR_PARSE_ERANGE;
				goto result_srvs2ent;
			}
			(void) strcpy(serv->s_proto, attrptr->attrvalue[0]);
			continue;
		}
	}

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getservent.c: _nss_ldap_services2ent]\n");
	(void) fprintf(stdout, "        s_name: [%s]\n", serv->s_name);
	if (mp != NULL) {
		for (mp = serv->s_aliases; *mp != NULL; mp++)
			(void) fprintf(stdout, "     s_aliases: [%s]\n", *mp);
	}
	(void) fprintf(stdout, "        s_port: [%d]\n", serv->s_port);
	(void) fprintf(stdout, "    s_protocol: [%s]\n", serv->s_proto);
#endif DEBUG

result_srvs2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


/*
 * getbyname gets struct servent values by service name. This
 * function constructs an ldap search filter using the service
 * name invocation parameter and the getservbyname search filter
 * defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into *serv = (struct servent *)
 * argp->buf.result. The function _nss_ldap_services2ent performs
 * the data marshaling.
 */

static nss_status_t
getbyname(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	const char	*proto = argp->key.serv.proto;
	char		searchfilter[SEARCHFILTERLEN];

	if (proto == NULL) {
		if (snprintf(searchfilter, SEARCHFILTERLEN,
		    _F_GETSERVBYNAME, argp->key.serv.serv.name) < 0)
			return ((nss_status_t)NSS_NOTFOUND);
	} else {
		if (snprintf(searchfilter, SEARCHFILTERLEN,
		    _F_GETSERVBYNAMEPROTO,
		    argp->key.serv.serv.name, proto) < 0)
			return ((nss_status_t)NSS_NOTFOUND);
	}

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_SERVICES, searchfilter, NULL));
}


/*
 * getbyport gets struct servent values by service port. This
 * function constructs an ldap search filter using the service
 * name invocation parameter and the getservbyport search filter
 * defined. Once the filter is constructed, we search for a matching
 * entry and marshal the data results into *serv = (struct servent *)
 * argp->buf.result. The function _nss_ldap_services2ent performs
 * the data marshaling.
 */

static nss_status_t
getbyport(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	const char	*proto = argp->key.serv.proto;
	char		portstr[12];
	char		searchfilter[SEARCHFILTERLEN];

	if (snprintf(portstr, sizeof (portstr), " %d",
	    ntohs((u_short)argp->key.serv.serv.port)) < 0)
		return ((nss_status_t)NSS_NOTFOUND);

	if (proto == NULL) {
		if (snprintf(searchfilter, SEARCHFILTERLEN, _F_GETSERVBYPORT,
		    strtol(portstr, (char **)NULL, 10)) < 0)
			return ((nss_status_t)NSS_NOTFOUND);
	} else {
		if (snprintf(searchfilter, SEARCHFILTERLEN,
		    _F_GETSERVBYPORTPROTO,
		    strtol(portstr, (char **)NULL, 10), proto) < 0)
			return ((nss_status_t)NSS_NOTFOUND);
	}

	return ((nss_status_t)_nss_ldap_lookup(be, argp,
		_SERVICES, searchfilter, NULL));
}

static ldap_backend_op_t serv_ops[] = {
    _nss_ldap_destr,
    _nss_ldap_endent,
    _nss_ldap_setent,
    _nss_ldap_getent,
    getbyname,
    getbyport
};


/*
 * _nss_ldap_services_constr is where life begins. This function calls
 * the generic ldap constructor function to define and build the
 * abstract data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_services_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

	return ((nss_backend_t *)_nss_ldap_constr(serv_ops,
		sizeof (serv_ops)/sizeof (serv_ops[0]), _SERVICES,
		services_attrs, _nss_ldap_services2ent));
}
