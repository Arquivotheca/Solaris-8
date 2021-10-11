/*
 *	db_entry.cc
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)db_entry.cc	1.8	93/04/23 SMI"


#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "db_headers.h"
#include "db_table.h"  /* must come before db_entry */
#include "db_entry.h"

#define	PRINT_WIDTH 32

void
print_entry(entryp location, entry_object *e)
{
	printf("entry at location %d: \n", location);

	if (e == NULL) {
		printf("\tnull object\n");
		return;
	}

	int size = e->en_cols.en_cols_len, i, j, col_width;
	entry_col * entry = e->en_cols.en_cols_val;

	printf("\ttype: %s\n", e->en_type ? e->en_type : "none");
	printf("\tnumber of columns: %d\n", size);

	for (i = 0; i < size; i++) {
		printf("\t\t%d: flags=0x%x, length=%d, value=",
			i, entry[i].ec_flags, entry[i].ec_value.ec_value_len);
		col_width = ((entry[i].ec_value.ec_value_len > PRINT_WIDTH) ?
				PRINT_WIDTH : entry[i].ec_value.ec_value_len);
		for (j = 0; j < col_width; j++) {
			if (entry[i].ec_value.ec_value_val[j] < 32) {
				putchar('^');
				putchar(entry[i].ec_value.ec_value_val[j]+32);
			} else {
				putchar(entry[i].ec_value.ec_value_val[j]);
			}
		}

		putchar('\n');
	}
}

entry_object*
new_entry(entry_object *old)
{
	entry_object* newobj = new entry_object;
	if (newobj == NULL)
	    FATAL("new_entry:: cannot allocate space", DB_MEMORY_LIMIT);

	if (copy_entry(old, newobj))
		return (newobj);
	else {
	    delete newobj;
	    return (NULL);
	}
}

bool_t
copy_entry(entry_object * old, entry_object *nb)
{
	int tlen, j, i;
	int num_cols = 0;
	entry_col *cols, *newcols = NULL;

	if (old == NULL) return FALSE;

	if (old->en_type == NULL)
		nb->en_type = NULL;
	else {
		nb->en_type = strdup(old->en_type);
		if (nb->en_type == NULL)
			FATAL(
			    "copy_entry: cannot allocate space for entry type",
			    DB_MEMORY_LIMIT);
	}

	num_cols = old->en_cols.en_cols_len;
	cols = old->en_cols.en_cols_val;
	if (num_cols == 0)
		nb->en_cols.en_cols_val = NULL;
	else {
		newcols = new entry_col[num_cols];
		if (newcols == NULL) {
			if (nb->en_type)
			delete nb->en_type;
			FATAL("copy_entry: cannot allocate space for columns",
				DB_MEMORY_LIMIT);
		}
		for (j = 0; j < num_cols; j++) {
			newcols[j].ec_flags = cols[j].ec_flags;
			tlen = newcols[j].ec_value.ec_value_len =
				cols[j].ec_value.ec_value_len;
			newcols[j].ec_value.ec_value_val = new char[ tlen ];
			if (newcols[j].ec_value.ec_value_val == NULL) {
				// cleanup space already allocated
				if (nb->en_type)
					delete nb->en_type;
				for (i = 0; i < j; i++)
					delete newcols[i].ec_value.ec_value_val;
				delete newcols;
				FATAL(
			"copy_entry: cannot allocate space for column value",
			DB_MEMORY_LIMIT);
			}
			memcpy(newcols[j].ec_value.ec_value_val,
				cols[j].ec_value.ec_value_val,
				tlen);
		}
	}
	nb->en_cols.en_cols_len = num_cols;
	nb->en_cols.en_cols_val = newcols;
	return (TRUE);
}

void
free_entry(entry_object * obj)
{
	int i;
	int num_cols;
	entry_col *cols;

	if (obj != NULL) {
		num_cols = obj->en_cols.en_cols_len;
		cols = obj->en_cols.en_cols_val;
		for (i = 0; i < num_cols; i++)
			if (cols[i].ec_value.ec_value_val != NULL)
				delete cols[i].ec_value.ec_value_val;
		if (cols)
			delete cols;
		if (obj->en_type)
			delete obj->en_type;
		delete obj;
	}
}
