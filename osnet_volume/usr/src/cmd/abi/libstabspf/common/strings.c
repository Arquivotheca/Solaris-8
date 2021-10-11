/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strings.c	1.1	99/05/14 SMI"

#include <string.h>
#include <stdlib.h>
#include "stabspf_impl.h"

/*
 * The string table contains an array of chars. If it is not big enough
 * it is grown AS A WHOLE as needed.
 *
 * NOTE:
 *	The base of the string table can grow (realloc()) at any time
 *	so a stroffset_t must always be used to access a string.
 *
 *	A string offset (stroffset_t) value of zero is invalid.  Actually,
 *	the offset is from the base of the string table so valid
 *	offsets are greater than sizeof (struct string_table_hdr).
 */
typedef struct string_table {
	struct string_table_hdr {
		size_t	sth_used;	/* Number of used chars. */
		/* Number of allocated chars in the string portion. */
		size_t	sth_have;
		/* Number of bytes in the WHOLE string table. */
		size_t	sth_size;
	} st_hdr;
	char	st_strtab[1];	/* Place holder for first character. */
} stringt_t;

/*
 * In order to minimize reallocations, tune these values to what is typically
 * needed.
 *
 * The space required for the unique type strings in libc, libdl, libnsl and
 * libsocket, which are needed for an app like ksh, requires a string table
 * just under 40k.
 */
#define	STRING_TABLE_START	40960	/* Initial size of string table. */
#define	STRING_TABLE_GROW	8192	/* Grow string table by ... */

/* The global string table. */
static stringt_t *global_string_table;

/* Handy macros to access string table information directly. */
#define	st_have	st_hdr.sth_have
#define	st_used	st_hdr.sth_used
#define	st_size	st_hdr.sth_size


/*
 * stringt_new() - Create a new string table.
 */
static stabsret_t
stringt_new(stringt_t **stringt)
{
	stringt_t *st;

	if ((st = calloc(1, STRING_TABLE_START)) == NULL) {
		return (STAB_NOMEM);
	}

	st->st_size = STRING_TABLE_START;
	st->st_have = STRING_TABLE_START - sizeof (struct string_table_hdr);

	*stringt = st;
	return (STAB_SUCCESS);
}

/* stringt_destroy_string_table() - Free the given string table. */
static void
stringt_destroy_string_table(stringt_t **stringt)
{
	free(*stringt);
	*stringt = NULL;
}

/*
 * stringt_grow() - Grow the give string table.
 *
 * Argument <hint> is the size of the string that is driving the growth.
 * We must ensure that our new size can at least fit this string.
 */
static stabsret_t
stringt_grow(stringt_t **stringt, size_t hint)
{
	stringt_t *st = *stringt;
	stringt_t *new_st;
	char *new_strs;
	size_t new_have;
	size_t new_size;

	/* Just in case hint is bigger than STRING_TABLE_GROW. */
	if (hint < STRING_TABLE_GROW) {
		hint = STRING_TABLE_GROW;
	} else {
		hint += STRING_TABLE_GROW;
	}

	new_have = st->st_have + hint;
	new_size = st->st_size + hint;

	if ((new_st = realloc(st, new_size)) == NULL) {
		return (STAB_NOMEM);
	}

	st = new_st;

	/* Zero the new section */
	new_strs = (char *)new_st + st->st_size;
	(void) memset(new_strs, 0, hint);

	/* Update the string table information. */
	st->st_have = new_have;
	st->st_size = new_size;

	/* Update the string table. */
	*stringt = st;
	return (STAB_SUCCESS);
}

/*
 * stringt_new_str_in_table() - Copy a new string into the string table and
 *	assign <*offset> to its position.
 *
 * NOTE:
 *	If <slen> is not given (== 0) then <s> must be null terminated.
 */
static stabsret_t
stringt_new_str_in_table(stringt_t **stringt, const char *s, size_t slen,
    stroffset_t *offset)
{
	size_t new_used;
	stringt_t *st;
	char *new_str;
	stabsret_t ret;

	/* Get slen if not given assumes null terminated. */
	if (slen == 0) {
		slen = strlen(s);
	}

	/* Create string table if not there. */
	if (*stringt == NULL &&
	    (ret = stringt_new(stringt)) != STAB_SUCCESS) {
		return (ret);
	}
	st = *stringt;

	/* Add 1 for the null at the end. */
	new_used = st->st_used + slen + 1;

	if (new_used >= st->st_have) {
	    /* Add 1 for begining and 1 for the null at the end. */
	    ret = stringt_grow(stringt, slen + 2);
	    if (ret != STAB_SUCCESS) {
		return (ret);
	    }
	    /* Update st */
	    st = *stringt;
	}


	/* Copy into string table and nul terminate the copy. */
	new_str = st->st_strtab + st->st_used + 1;
	(void) memcpy(new_str, s, slen);
	new_str[slen] = '\0';

	/*
	 * Offset is from the beginning of the string table.
	 * this makes an offset of zero invalid.
	 */
	*offset = sizeof (struct string_table_hdr) + st->st_used + 1;
	st->st_used = new_used;

	return (STAB_SUCCESS);
}

/* stringt_new_str() - Add a new string to the global string table */
stabsret_t
stringt_new_str(const char *s, size_t slen, stroffset_t *offset)
{
	return (stringt_new_str_in_table(&global_string_table, s, slen,
	    offset));
}

/*
 * string_offset2ptr() - Get a 'char *' from the global string table.
 *
 * NOTE:
 * 	The symbol tables can be relocated at ANY time.  This is the only
 *	way to get a 'char *' from a stroffset_t.
 */
stabsret_t
string_offset2ptr(stroffset_t offset, char **ptr)
{
	if (offset <= sizeof (struct string_table_hdr) ||
	    offset > global_string_table->st_have) {
		return (STAB_FAIL);
	}

	/* Pointer calculated from base of string table. */
	*ptr = (char *)global_string_table + offset;

	return (STAB_SUCCESS);
}
/* stringt_create_table() - Create the global_keypair_table. */
stabsret_t
stringt_create_table(void)
{
	stabsret_t ret;

	if (global_string_table == NULL) {
		ret = stringt_new(&global_string_table);
	} else {
		ret = STAB_FAIL;
	}

	return (ret);
}

/* stringt_destroy_table() - Free the global_keypair_table. */
void
stringt_destroy_table(void)
{
	stringt_destroy_string_table(&global_string_table);
}

void
stringt_report(void)
{
	(void) fprintf(stderr, "==== string_table: size = %.2f K ====\n",
	    (float)(global_string_table->st_size / 1024.0));
}
