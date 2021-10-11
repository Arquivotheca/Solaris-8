/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)util.c	1.1	99/01/25 SMI"

/*
 * util.c -- low-level utilities used by map*.c
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "xlator.h"
#include "util.h"
#include "errlog.h"

/*
 * String tables -- WARNING!  This uses realloc to recreate tables,
 *	so always assign table_t * return value to the current
 *	table pointer, lest the table address change in the
 *	called function.
 */
static char *strset(char *, char *);

table_t *
create_stringtable(int size)
{
	table_t *t;

	/* Solaris idiom: malloc && memset. TBD. */
	if ((t = calloc((size_t)1, (size_t)(sizeof (table_t) +
	    ((sizeof (char *)) * size)))) == NULL) {
		errlog(FATAL,
		    "\nOut of memory.\n"
		    "We wish to hold the whole sky,\n"
		    "But we never will.\n");
	}
	t->nelem = size;
	t->used = -1;
	return (t);
}


table_t *
add_to_stringtable(table_t *t, char *value)
{
	table_t *t2;

	int i;

	if (t == NULL) {
		seterrline(__LINE__, __FILE__, NULL, NULL);
		errlog(FATAL|PROGRAM, "programmer error: tried to add to "
			"a NULL table");
	}
	if (in_stringtable(t, value)) {
		return (t);
	}
	++t->used;
	if (t->used >= t->nelem) {
		if ((t2 = realloc(t, (size_t)(sizeof (table_t) +
		    ((sizeof (char *)) * (t->nelem + TABLE_INCREMENT)))))
		    == NULL) {
			print_stringtable(t);
			seterrline(__LINE__, __FILE__, NULL, NULL);
			errlog(FATAL|PROGRAM, "out of memory extending a "
				"string table");
		}
		t = t2;
		t->nelem += TABLE_INCREMENT;
		for (i = t->used; i < t->nelem; ++i) {
			t->elements[i] = NULL;
		}
	}
	t->elements[t->used] = strset(t->elements[t->used], value);
	return (t);
}

/*
 * free_stringtable -- really only mark it empty for reuse.
 */
table_t *
free_stringtable(table_t *t)
{

	if (t != NULL) {
		t->used = -1;
	}
	return (t);
}


char *
get_stringtable(table_t *t, int index)
{

	if (t == NULL) {
		return (NULL);
	} else if (index > t->used) {
		return (NULL);
	} else {
		return (t->elements[index]);
	}
}

int
in_stringtable(table_t *t, const char *value)
{
	int i;

	if (t == NULL) {
		return (0);
	}
	for (i = 0; i <= t->used; ++i) {
		if (strcmp(value, t->elements[i]) == 0)
			return (1);
	}
	return (0);
}


void
print_stringtable(table_t *t)
{
	int i;

	if (t == NULL)
		return;

	errlog(VERBOSE,
		"table size = %d elements out of %d elements/%d bytes\n",
		t->used + 1, t->nelem,
		sizeof (table_t) + (sizeof (char *) * t->nelem));

	for (i = 0; i <= t->used; ++i) {
		(void) fprintf(stderr, "\t%s\n",
			get_stringtable(t, i));
	}
}

static int
compare(const void *p, const void *q)
{
	return (strcmp((char *)p, (char *)q));
}

void
sort_stringtable(table_t *t)
{

	if (t && t->used > 0) {
		qsort((char *)t->elements, (size_t)t->used,
			sizeof (char *), compare);
	}
}


/*
 * strset -- update a dynamically-allocated string or die trying.
 */
/*ARGSUSED*/
static char *
strset(char *string, char *value)
{
	size_t vlen;

	assert(value != NULL, "passed a null value to strset");
	vlen = strlen(value);
	if (string == NULL) {
		/* It was never allocated, so allocate it. */
		if ((string = malloc(vlen + 1)) == NULL) {
			seterrline(__LINE__, __FILE__, NULL, NULL);
			errlog(FATAL|PROGRAM, "out of memory allocating a "
			    "string");
		}
	} else if (strlen(string) < vlen) {
		/* Reallocate bigger. */
		if ((string = realloc(string, vlen + 1)) == NULL) {
			seterrline(__LINE__, __FILE__, NULL, NULL);
			errlog(FATAL|PROGRAM, "out of memory reallocating"
			    "a string");
		}
	}
	(void) strcpy(string, value);
	return (string);
}
