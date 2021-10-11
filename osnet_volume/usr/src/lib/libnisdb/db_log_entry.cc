/*
 *	db_log_entry.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_log_entry.cc	1.6	93/04/23 SMI"

#include <stdio.h>

#include "db_headers.h"
#include "db_log_entry.h"
#include "db_table.h"

extern void print_entry(entryp, entry_object *);

/*
 * Constructor:  Create a log entry using the given parameters.  Note that
 * pointers to db_query and entry_object are simply assigned, not copied.
 */
db_log_entry::db_log_entry(db_action a, vers * v, db_query *q,
			    entry_object *obj)
{
	action = a;
	aversion.assign(v);
	query = q;
	object = obj;
	next = NULL;
	bversion.assign(v);
}

db_log_entry::~db_log_entry()
{
/* we might not have allocated these ourselves, so we cannot delete them */
//	if (query) delete query;
//	if (object) free_entry(object);
}

/* prints a line from the journal */
void
db_log_entry::print()
{
	switch (action) {
	case DB_ADD:
	    printf("add: ");
	    break;
	case DB_REMOVE:
	    printf("remove: ");
	    break;
	default:
	    printf("action(%d): ", action);
	    break;
	}

	aversion.print(stdout);
	putchar(' ');
	if (query != NULL)
		query->print();
	else
		printf("no query!\n");

	if (object != NULL) {
		print_entry(0, object);
	} else {
		printf("no object\n");
	}
	bversion.print(stdout);
	putchar('\n');
}
