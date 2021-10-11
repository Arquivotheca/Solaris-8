/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)printResult.c 1.1     99/07/07 SMI"

#include <stdio.h>
#include "../../../lib/libsldap/common/ns_sldap.h"

void
_printEntry(ns_ldap_entry_t * entry) {
	int	j, k;
	char	*cp;
	for (j = 0; j < entry->attr_count; j++) {
		cp = entry->attr_pair[j]->attrname;
		if (j == 0) {
			fprintf(stdout, "%s: %s\n", cp,
				entry->attr_pair[j]->attrvalue[0]);
		} else {
			for (k = 0; (k < entry->attr_pair[j]->value_count) &&
			    (entry->attr_pair[j]->attrvalue[k]); k++)
				fprintf(stdout, "\t%s: %s\n", cp,
					entry->attr_pair[j]->attrvalue[k]);
		}
	}
}


void
_printResult(ns_ldap_result_t * result) {
	int i, j, k;
	ns_ldap_entry_t *curEntry;

	if (result == NULL) {
		return;
	}
	curEntry = result->entry;
	for (i = 0; i < result->entries_count; i++) {
		if (i != 0)
			fprintf(stdout, "\n");
		_printEntry(curEntry);
		curEntry = curEntry->next;
	}
}
