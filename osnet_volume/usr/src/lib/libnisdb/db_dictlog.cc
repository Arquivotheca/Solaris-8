/*
 *	db_dictlog.cc
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_dictlog_entry.cc	1.5	92/09/21 SMI"

#include <stdio.h>

#include <malloc.h>
#include <string.h>
#ifdef TDRPC
#include <sysent.h>
#endif
#include <unistd.h>

#include "db_headers.h"
#include "db_dictlog.h"


/*
 * Constructor:  Create a log entry using the given parameters.  Note that
 * pointers to db_query and entry_object are simply assigned, not copied.
 */
db_dictlog_entry::db_dictlog_entry(int a, vers * v, char *tname,
				    table_obj *obj)
{
	action = a;
	aversion.assign(v);
	table_name = tname;
	table_object = obj;
	next = NULL;
	bversion.assign(v);
}

db_dictlog_entry::~db_dictlog_entry()
{
/* we might not have allocated these ourselves, so we cannot delete them */
}

/* prints a line from the journal */
void
db_dictlog_entry::print()
{
	switch (action) {
	case DB_ADD_TABLE:
	    printf ("add: ");
	    break;
	case DB_REMOVE_TABLE:
	    printf ("remove: ");
	    break;
	default:
	    printf ("action(%d): ", action);
	    break;
	}

	aversion.print(stdout);
	putchar(' ');
	if (table_name != NULL)
		printf ("table %s\n", table_name);
	else
		printf("no table!\n");
	bversion.print(stdout);
	putchar('\n');
}

static void
free_table_entry(table_obj* obj)
{
	if (obj == NULL)
		return;

	if (obj->ta_type != NULL)
		free(obj->ta_type);

	table_col* tcs = obj->ta_cols.ta_cols_val;
	int i;
	for (i = 0; i < obj->ta_cols.ta_cols_len; i++) {
		if (tcs[i].tc_name != NULL)
			delete tcs[i].tc_name;
	}
	if (tcs != NULL)
		delete tcs;
	if (obj->ta_path != NULL)
		free(obj->ta_path);
	free(obj);
}

static void
delete_log_entry(db_dictlog_entry *lentry)
{
	char *tname;
	table_obj *obj;
	if (lentry) {
		if ((tname = lentry->get_table_name())) {
			delete tname;
		}
		if ((obj = lentry->get_table_object())) {
		    free_table_entry(obj);
		}
		delete lentry;
	}
}

/*
 * Execute given function 'func' on log.
 * function takes as arguments: pointer to log entry, character pointer to
 * another argument, and pointer to an integer, which is used as a counter.
 * 'func' should increment this value for each successful application.
 * The log is traversed until either 'func' returns FALSE, or when the log
 * is exhausted.  The second argument to 'execute_on_log' is passed as the
 * second argument to 'func'.  The third argument, 'clean' determines whether
 * the log entry is deleted after the function has been applied.
 * Returns the number of times that 'func' incremented its third argument.
 */
int
db_dictlog::execute_on_log(bool_t (*func) (db_dictlog_entry *,
					    char *, int *),
					    char* dict, bool_t clean)
{
	db_dictlog_entry    *j;
	int count = 0;
	bool_t done = FALSE;

	if (open() == FALSE) {   // open log
		return (0);
	}
	while (!done) {
		j = get();
		if (j == NULL)
			break;
		if ((*func)(j, dict, &count) == FALSE) done = TRUE;
		if (clean) delete_log_entry(j);
	}

	close();
	return (count);
}

static bool_t
print_log_entry(db_dictlog_entry *j, char*, int *count)
{
	j->print();
	++ *count;
	return (TRUE);
}

/* Print contents of log file to stdout */
int
db_dictlog::print()
{
	return (execute_on_log(&(print_log_entry), NULL));
}

/*
 * Return the next element in current log; return NULL if end of log or error.
 * Log must have been opened for READ.
 */
db_dictlog_entry
*db_dictlog::get()
{
	db_dictlog_entry *j;
	if (mode != PICKLE_READ)
		return (NULL);

	j = new db_dictlog_entry;

	if (j == NULL) return (NULL);
	if (xdr_db_dictlog_entry(&(xdr), j) == FALSE) {
		delete_log_entry (j);
/*    WARNING("Could not sucessfully finish reading log"); */
		return (NULL);
	}
	if (j->sane())
		return (j);
	else {
		WARNING("truncated log entry found");
		delete_log_entry(j);
		return (NULL);
	}
}

/* Append given log entry to log. */
int
db_dictlog::append(db_dictlog_entry *j)
{
	int status;
	if (mode != PICKLE_APPEND)
		return (-1);

	/* xdr returns TRUE if successful, FALSE otherwise */
	status = ((xdr_db_dictlog_entry(&(xdr), j)) ? 0 : -1);
	if (status < 0) {
		WARNING("db_dictlog: could not write log entry");
		return (status);
	}

	status = fflush(file);
	if (status < 0) {
		WARNING("db_dictlog: could not flush log entry to disk");
		return (status);
	}

	status = fsync(fileno(file));
	if (status < 0) {
		WARNING("db_dictlog: could not sync log entry to disk");
	}

	return (status);
}
