/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nscd_nischeck.c	1.5	99/10/11 SMI"

/*
 * Check permissions on NIS+ tables for security
 *
 * Usage: /usr/lib/nscd_nischeck <table>
 *
 * Emit 1 if table isn't readable by "nobody" eg everybody.
 */

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpcsvc/nis.h>
#include <unistd.h>
extern int 	optind;
extern char	*optarg;

int
check_col(struct nis_object *table, int col)
{
  	struct table_col *c;
	c = table->zo_data.objdata_u.ta_data.ta_cols.ta_cols_val + col;
	return (NIS_NOBODY(c->tc_rights, NIS_READ_ACC));
}

int
main(int argc, char **argv)
{
	nis_result *tab;
	nis_object *obj;
	char namebuf[64];

	if (argc != 2) {
		(void)fprintf(stderr, "usage: %s cache_name\n", argv[0]);
		leave(1);
	}

	sprintf(namebuf, "%s.org_dir", argv[1]);
	tab = nis_lookup(namebuf, EXPAND_NAME);
	if (tab->status != NIS_SUCCESS) {
		nis_perror(tab->status, namebuf);
		leave(2);
	}

	obj = tab->objects.objects_val;
	if (NIS_NOBODY(obj->zo_access, NIS_READ_ACC))
		leave(0);

	/*
	 *	Currently only makes sense for passwd
	 */

	if (strcmp(argv[1], "passwd") == 0) {
		leave(1);
	}

	leave(0);
}

leave(int n)
{
	if(getenv("NSCD_DEBUG"))
	    fprintf(stderr, "nscd_nischeck: exit(%d)\n", n);
	exit(n);
}
