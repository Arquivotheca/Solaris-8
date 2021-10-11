/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mema_test_config.c	1.2	99/08/31 SMI"

#include <stddef.h>
#include <stdlib.h>
#include <sys/param.h>
#include <config_admin.h>
#include <memory.h>
#include "mema_test.h"

extern mtest_func_t memory_test_normal;
extern mtest_func_t memory_test_quick;
extern mtest_func_t memory_test_extended;

/*
 * Default test is first entry in the table (MTEST_DEFAULT_TEST).
 */
struct mtest_table_ent mtest_table[] = {
	{"normal",	memory_test_normal},
	{"quick",	memory_test_quick},
	{"extended",	memory_test_extended},
};

static char **opt_array;

char **
mtest_build_opts(int *maxerr_idx)
{
	if (opt_array == NULL) {
		int nopts;
		/*
		 * Test "type" options here, max_errors should be the
		 * last one.
		 */
		nopts = sizeof (mtest_table) / sizeof (mtest_table[0]);
		*maxerr_idx = nopts;

		/*
		 * One extra option for "max_errors"
		 */
		opt_array = (char **)malloc((nopts + 2) * sizeof (*opt_array));
		if (opt_array != NULL) {
			int i;

			for (i = 0; i < nopts; i++)
				opt_array[i] = (char *)mtest_table[i].test_name;

			opt_array[nopts] = "max_errors";
			opt_array[nopts + 1] = NULL;
		}
	}
	*maxerr_idx = sizeof (mtest_table) / sizeof (mtest_table[0]);
	return (opt_array);
}
