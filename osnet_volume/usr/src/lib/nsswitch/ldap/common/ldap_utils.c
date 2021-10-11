/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ldap_utils.c 1.2     99/10/13 SMI"

#include <sys/systeminfo.h>
#include "ldap_common.h"


#ifdef DEBUG
/*
 * Debugging routine for printing the value of a result
 * structure
 */
int
printresult(ns_ldap_result_t * result)
{
	int		i, j, k;
	ns_ldap_entry_t	*curEntry;

	printf("--------------------------------------\n");
	printf("entries_count %d\n", result->entries_count);
	curEntry = result->entry;
	for (i = 0; i < result->entries_count; i++) {
		printf("entry %d has attr_count = %d \n",
		    i, curEntry->attr_count);
		for (j = 0; j < curEntry->attr_count; j++) {
			printf("entry %d has attr_pair[%d] = %s \n",
			    i, j, curEntry->attr_pair[j]->attrname);
			for (k = 0;
			    (k < curEntry->attr_pair[j]->value_count) &&
			    (curEntry->attr_pair[j]->attrvalue[k]);
			    k++)
				printf("entry %d has "
				    "attr_pair[%d]->attrvalue[%d] = %s \n",
				    i, j, k,
				    curEntry->attr_pair[j]->attrvalue[k]);
		}
		printf("\n--------------------------------------\n");
		curEntry = curEntry->next;
	}
	return (1);
}
#endif


/*
 *
 */

ns_ldap_attr_t *
getattr(ns_ldap_result_t *result, int i)
{
	ns_ldap_entry_t	*entry;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[ldap_utils.c: getattr]\n");
#endif DEBUG

	if (result != NULL) {
		entry = result->entry;
	} else {
		return (NULL);
	}
	if (result->entries_count == 0) {
		return (NULL);
	} else {
		return (entry->attr_pair[i]);
	}
}


/*
 * _strip_dn_cononical_name() is passed the ldap descriptor and the
 * current search entry and stips out a cononical name from it's
 * distinguished named.  An error returns (char *)NULL, otherwise, a
 * string containing a cononical name is returned.  The format of a
 * distinguished name is
 * 	dn: cn=hostname+ipHostNumber="109.34.54.76", ou= ...
 *	dn: echo+IpServiceProtocol=udp, ou= ...
 */

char *
_strip_dn_cononical_name(char *name)
{
	char	*cp = (char *) NULL;

	/* look for + sign */
	if ((cp = strchr(name, '+')) == NULL)
		return ((char *)name);
	*cp = '\0';
	/* look for = sign */
	if ((cp = strchr(name, '=')) == NULL)
		return ((char *)name);
	*cp++ = '\0';

	return (cp);
}



/*
 * 	"109.34.54.76" -> 109.34.54.76
 */

const char *
_strip_quotes(char *ipaddress)
{
	char	*cp = (char *) NULL;

	/* look for first " */
	if ((cp = strchr(ipaddress, '"')) == NULL)
		return ((char *)ipaddress);
	ipaddress++;
	/* look for last " */
	if ((cp = strchr(ipaddress, '"')) == NULL)
		return ((char *)ipaddress);
	*cp++ = '\0';

	return (ipaddress);
}


/*
 * This is a copy of a routine in libnsl/nss/netdir_inet.c.  It is
 * here because /etc/lib/nss_ldap.so.1 cannot call routines in
 * libnsl.  Care should be taken to keep the two copies in sync.
 */

int
__nss2herrno(nss_status_t nsstat)
{
	switch (nsstat) {
		case NSS_SUCCESS:
			return (0);
		case NSS_NOTFOUND:
			return (HOST_NOT_FOUND);
		case NSS_TRYAGAIN:
			return (TRY_AGAIN);
		case NSS_UNAVAIL:
			return (NO_RECOVERY);
	}
	/* NOTREACHED */
}
