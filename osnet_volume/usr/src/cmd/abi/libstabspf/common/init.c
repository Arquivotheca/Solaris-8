/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)init.c	1.1	99/05/14 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <libintl.h>
#include "stabspf_impl.h"

/* CSTYLED */
#pragma	init (stabspf_init)

/*
 * Allocate Information required.
 */

static void
stabspf_init(void)
{
	/* Go for the largest hash size available. */
	if (hash_create_table(-1UL) != STAB_SUCCESS) {
		(void) fputs(gettext("FAIL: create hash table\n"), stderr);
		exit(EXIT_FAILURE);
	}
	if (keypair_create_table() != STAB_SUCCESS) {
		(void) fputs(gettext("FAIL: create keypair table\n"), stderr);
		exit(EXIT_FAILURE);
	}
	if (ttable_create_table() != STAB_SUCCESS) {
		(void) fputs(gettext("FAIL: create types table\n"), stderr);
		exit(EXIT_FAILURE);
	}
	if (stringt_create_table() != STAB_SUCCESS) {
		(void) fputs(gettext("FAIL: create string table\n"), stderr);
		exit(EXIT_FAILURE);
	}

}
