/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ns_getalias.c 1.1     99/07/07 SMI"

#include <stdlib.h>
#include <libintl.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include "ns_sldap.h"
#include "ns_internal.h"

/*
 * getldaplaliasbyname() retrieves the aliases information from the LDAP server.
 * This is requires that the LDAP naming information (ie. LDAP_CLIENT_CACHE
 * file) is configured properly on the client machine.
 *
 * Return value:
 *      0 = success;
 *      1 = alias not found;
 *      -1 = other failure.  Contents in answer are undefined.
 */

#define	ALIAS_FILTER	"(&(objectclass=mailgroup)(|(cn=%s)(mail=%s)))"
#define	MAIL_CN		"cn"
#define	MAIL_ATTRIBUTE	"mail"
#define	MAIL_MEMBER	"mgrpRFC822MailMember"

int
__getldapaliasbyname(char *alias, char *answer, size_t ans_len)
{
	char		*database = "aliases";
	char		filter[BUFSIZE];
	char		*attribute[2];
	ns_ldap_result_t	*result = NULL;
	ns_ldap_error_t	*errorp = NULL;
	int		rc, i, j, len, comma;
	ns_ldap_entry_t	*entry = NULL;
	char		**attr_value = NULL;

	if (!alias || !*alias || !answer || ans_len == 0) {
		errno = EINVAL;
		return (-1);
	}

	answer[0] = '\0';

	/* get the aliases */
	if (sprintf(filter, ALIAS_FILTER, alias, alias) < 0) {
		errno = EINVAL;
		return (-1);
	}

	attribute[0] = MAIL_MEMBER;
	attribute[1] = NULL;

	/* should we do hardlookup */
	rc = __ns_ldap_list(database, (const char *)filter,
		(const char **)attribute, NULL, NULL, 0, &result,
		&errorp, NULL, NULL);

	if (rc == NS_LDAP_NOTFOUND) {
		errno = ENOENT;
		return (1);
	} else if (rc != NS_LDAP_SUCCESS) {
#ifdef DEBUG
		char *p;
		(void) __ns_ldap_err2str(rc, &p);
		if (errorp) {
			if (errorp->message)
				fprintf(stderr, "%s (%s)\n", p,
					errorp->message);
		} else
			fprintf(stderr, "%s\n", p);
#endif DEBUG
		__ns_ldap_freeError(&errorp);
		return (-1);
	}

	/* build the return value */
	answer[0] = '\0';
	len = 0;
	comma = 0;
	entry = result->entry;
	for (i = 0; i < result->entries_count; i++) {
		attr_value = __ns_ldap_getAttr(entry, MAIL_MEMBER);
		if (attr_value == NULL) {
			errno = ENOENT;
			return (-1);
		}
		for (j = 0; attr_value[j]; j++) {
			char	*tmp, *newhead;

			tmp = attr_value[j];
			while (*tmp == ' ' || *tmp == '\t' && *tmp != '\0')
				tmp++;
			newhead = tmp;
			while (*tmp != '\0') tmp++;
			while (*tmp == ' ' || *tmp == '\t' || *tmp == '\0' &&
			    tmp != newhead) {
				*tmp-- = '\0';
			}
			len = len + comma + strlen(newhead);
			if ((len + 1) > ans_len) {
				__ns_ldap_freeResult(&result);
				errno = EOVERFLOW;
				return (-1);
			}
			if (comma)
				strcat(answer, ",");
			else
				comma = 1;
			strcat(answer, newhead);
		}
	}

	__ns_ldap_freeResult(&result);
	errno = 0;
	return (0);
}
