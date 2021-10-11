/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ns_error.c	1.1	99/07/07 SMI"

#include <stdlib.h>
#include <libintl.h>
#include "ns_sldap.h"
#include "ns_internal.h"

struct ns_ldaperror {
	int	e_code;
	char	*e_reason;
};

static struct ns_ldaperror ns_ldap_errlist[] = {
	{NS_LDAP_SUCCESS,	NULL},
	{NS_LDAP_OP_FAILED,	NULL},
	{NS_LDAP_NOTFOUND,	NULL},
	{NS_LDAP_MEMORY,	NULL},
	{NS_LDAP_CONFIG,	NULL},
	{NS_LDAP_PARTIAL,	NULL},
	{NS_LDAP_INTERNAL,	NULL},
	{NS_LDAP_INVALID_PARAM,	NULL},
	{-1,			NULL}
};


void
ns_ldaperror_init()
{
	int 	i = 0;

	ns_ldap_errlist[i++].e_reason = gettext("Success");
	ns_ldap_errlist[i++].e_reason = gettext("Operation failed");
	ns_ldap_errlist[i++].e_reason = gettext("Object not found");
	ns_ldap_errlist[i++].e_reason = gettext("Memory failure");
	ns_ldap_errlist[i++].e_reason = gettext("LDAP configuration problem");
	ns_ldap_errlist[i++].e_reason = gettext("Partial result");
	ns_ldap_errlist[i++].e_reason = gettext("LDAP error");
	ns_ldap_errlist[i++].e_reason = gettext("Invalid parameter");
	ns_ldap_errlist[i++].e_reason = gettext("Unknown error");
}


int
__ns_ldap_err2str(int err, char **strmsg)
{
	int	i;

	for (i = 0; (ns_ldap_errlist[i].e_code != err) &&
			(ns_ldap_errlist[i].e_code != -1); i++) {
		/* empty for loop */
	}
	*strmsg = ns_ldap_errlist[i].e_reason;
	return (NS_LDAP_SUCCESS);
}


int
__ns_ldap_freeError(ns_ldap_error_t **errorp)
{
	ns_ldap_error_t *err = *errorp;
	if (err) {
		if (err->message)
			free(err->message);
		free(err);
	}
	*errorp = NULL;
	return (NS_LDAP_SUCCESS);
}
