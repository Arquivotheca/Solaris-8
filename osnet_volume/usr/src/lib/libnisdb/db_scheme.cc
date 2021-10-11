/*
 *	db_scheme.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_scheme.cc	1.10	95/11/06 SMI"

#include <string.h>
#include "db_headers.h"
#include "db_scheme.h"

/*
 *  Constructor:  create new scheme by making copy of 'orig'.
 * All items within old scheme are also copied (i.e. no shared pointers).
*/
db_scheme::db_scheme(db_scheme* orig)
{
	int numkeys, i;
	keys.keys_len = 0;
	keys.keys_val = NULL;

	if (orig == NULL) {
		WARNING("db_scheme::db_scheme: null original db_scheme");
		return;
	}

	numkeys = this->keys.keys_len = orig->keys.keys_len;
	db_key_desc * descols = this->keys.keys_val = new db_key_desc[numkeys];
	db_key_desc * srccols = orig->keys.keys_val;

	if (descols == NULL) {
		clear_columns(0);
		FATAL("db_scheme::db_scheme: cannot allocate space for columns",
		DB_MEMORY_LIMIT);
	}

	for (i = 0; i < numkeys; i++) {
		if (srccols[i].key_name == NULL) {
			clear_columns(i);
			WARNING("db_scheme::db_scheme: null column name");
			return;
		}
		descols[i].key_name = new item(srccols[i].key_name);
		if (descols[i].key_name == NULL) {
			clear_columns(i);
			FATAL(
		"db_scheme::db_scheme: cannot allocate space for column names",
		DB_MEMORY_LIMIT);
		}
		descols[i].key_flags = srccols[i].key_flags;
		descols[i].where = srccols[i].where;
		descols[i].store_type = srccols[i].store_type;
		descols[i].column_number = srccols[i].column_number;
	}
	this->max_columns = orig->max_columns;
	this->data = orig->data;
}

/* Constructor:  create new sheme by using information in 'zdesc'. */
db_scheme::db_scheme(table_obj *zdesc)
{
	keys.keys_len = 0;
	keys.keys_val = NULL;

	if (zdesc == NULL) {
		WARNING("db_scheme::db_scheme: null table obj");
		return;
	}

	max_columns = zdesc->ta_maxcol;

	/* find out how many searchable columns */
	int total_cols = zdesc->ta_cols.ta_cols_len;
	table_col * zcols = zdesc->ta_cols.ta_cols_val;
	int count = 0, i;

	if (zcols == NULL) {
		WARNING("db_scheme::db_scheme: no columns in nis table obj");
		return;
	}

	/* find out number of indices  */
	for (i = 0; i < total_cols; i++) {
		if (zcols[i].tc_flags&TA_SEARCHABLE)
			++count;
	}
	if (count == 0) {
		WARNING(
		"db_scheme::db_scheme: no searchable columns in nis table obj");
		return;
	}

	keys.keys_len = count;
	db_key_desc * scols = keys.keys_val = new db_key_desc[count];
	if (scols == NULL) {
		clear_columns(0);
		FATAL("db_scheme::db_scheme: cannot allocate space for keys",
			DB_MEMORY_LIMIT);
	}
	int keynum = 0;

	for (i = 0; i < total_cols; i++) {
		if (zcols[i].tc_flags&TA_SEARCHABLE) {
			if (zcols[i].tc_name == NULL) {
				clear_columns(keynum);
				WARNING(
	    "db_scheme::db_scheme: searchable column cannot have null name");
				return;
			}
			scols[keynum].key_name = new item(zcols[i].tc_name,
					strlen(zcols[i].tc_name));
			if (scols[keynum].key_name == NULL) {
				clear_columns(keynum);
				FATAL(
		    "db_scheme::db_scheme: cannot allocate space for key names",
		    DB_MEMORY_LIMIT);
			}
			scols[keynum].key_flags = zcols[i].tc_flags;
			scols[keynum].column_number = i;
			scols[keynum].where.max_len = NIS_MAXATTRVAL;
			scols[keynum].where.start_column = 0;
			/* don't care about position information for now */
			++keynum;	/* advance to next key number */
		}
	}
	if (keynum != count) {		/* something is wrong */
		clear_columns(keynum);
		WARNING(
	    "db_scheme::db_scheme: incorrect number of  searchable columns");
	}
}

void
db_scheme::clear_columns(int numkeys)
{
		int j;
		db_key_desc * cols = keys.keys_val;

		if (cols) {
			for (j = 0; j < numkeys; j++) {
				if (cols[j].key_name)
					delete cols[j].key_name;
			}
			delete cols;
			keys.keys_val = NULL;
		}
		keys.keys_len = 0;
}

/* Destructor:  delete all keys associated with scheme and scheme itself. */
db_scheme::~db_scheme()
{
	clear_columns(keys.keys_len);
}

/*
 * Predicate:  return whether given string is one of the index names
 * this scheme.  If so, return in 'result' the index's number.
*/
bool_t
db_scheme::find_index(char *purportedname, int *result)
{
	if (purportedname) {
		int i;
		int plen;
		plen = strlen(purportedname);

		for (i = 0; i < keys.keys_len; i++) {
			if (keys.keys_val[i].key_name->equal(purportedname,
								plen, TRUE)) {
				if (result) *result = i;
				return (TRUE);
			}
		}
	}
	return (FALSE);
}

/* Print out description of table. */
void
db_scheme::print()
{
	int i;

	for (i = 0; i < keys.keys_len; i++) {
		keys.keys_val[i].key_name->print();
		printf(
	"\tcolumn=%d, flags=0x%x, key record position=%d, max length=%d\n",
			keys.keys_val[i].column_number,
			keys.keys_val[i].key_flags,
			keys.keys_val[i].where.start_column,
			keys.keys_val[i].where.max_len);
		printf("\tdata record position=%d, max length=%d\n",
			data.where.start_column, data.where.max_len);
	}
	printf("\tmaximum number of columns=%d\n", max_columns);
}
