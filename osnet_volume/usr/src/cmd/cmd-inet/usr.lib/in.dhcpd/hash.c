#ident	"@(#)hash.c	1.6	96/07/08 SMI"

/*
 * Copyright 1988, 1991 by Carnegie Mellon University
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carnegie Mellon University not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

/*
 * Generalized hash table ADT
 *
 * Provides multiple, dynamically-allocated, variable-sized hash tables on
 * various data and keys.
 *
 * This package attempts to follow some of the coding conventions suggested
 * by Bob Sidebotham and the AFS Clean Code Committee of the
 * Information Technology Center at Carnegie Mellon.
 */

#include <stdlib.h>
#include <string.h>
#include "hash.h"

#define	TRUE		1
#define	FALSE		0
#define	NULL		0

/*
 * This can be changed to make internal routines visible to debuggers, etc.
 */
#define	PRIVATE static

/*
 * Hash table initialization routine.
 *
 * This routine creates and intializes a hash table of size "tablesize"
 * entries.  Successful calls return a pointer to the hash table (which must
 * be passed to other hash routines to identify the hash table).  Failed
 * calls return NULL.
 */
hash_tbl *
hash_Init(unsigned tablesize)
{
	register hash_tbl *hashtblptr;
	register unsigned totalsize;

	if (tablesize != 0) {
		totalsize = sizeof (hash_tbl) + sizeof (hash_member *) * \
		    (tablesize - 1);
		if ((hashtblptr = (hash_tbl *)malloc(totalsize)) != NULL) {
		    (void) memset((char *)hashtblptr, 0, totalsize);
		    hashtblptr->size = tablesize; /* Success! */
		    hashtblptr->bucketnum = 0;
		    hashtblptr->member = (hashtblptr->table)[0];
		}
	} else
		hashtblptr = NULL;	/* Disallow zero-length tables */
	return (hashtblptr);		/* NULL if failure */
}

/*
 * Recursively frees an entire linked list of bucket members (used in the
 * open hashing scheme).  Does nothing if the passed pointer is NULL.
 */
PRIVATE void
hashi_FreeMember(hash_member *bucketptr, void (*free_data)())
{
	if (bucketptr) {
		/* Free next member if necessary */
		hashi_FreeMember(bucketptr->next, free_data);
		(*free_data)(bucketptr->data);
		free((char *) bucketptr);
	}
}

/*
 * This routine re-initializes the hash table.  It frees all the allocated
 * memory and resets all bucket pointers to NULL.
 */
void
hash_Reset(hash_tbl *hashtable, void (*free_data)())
{
	hash_member	**bucketptr;
	unsigned	i;

	bucketptr = hashtable->table;
	for (i = 0; i < hashtable->size; i++) {
		hashi_FreeMember(*bucketptr, free_data);
		*bucketptr++ = NULL;
	}
	hashtable->bucketnum = 0;
	hashtable->member = (hashtable->table)[0];
}

/*
 * Generic hash function to calculate a hash code from the given string.
 *
 * For each byte of the string, this function left-shifts the value in an
 * accumulator and then adds the byte into the accumulator.  The contents of
 * the accumulator is returned after the entire string has been processed.
 * It is assumed that this result will be used as the "hashcode" parameter in
 * calls to other functions in this package.  These functions automatically
 * adjust the hashcode for the size of each hashtable.
 *
 * This algorithm probably works best when the hash table size is a prime
 * number.
 *
 * Hopefully, this function is better than the previous one which returned
 * the sum of the squares of all the bytes.  I'm still open to other
 * suggestions for a default hash function.  The programmer is more than
 * welcome to supply his/her own hash function as that is one of the design
 * features of this package.
 */
unsigned
hash_HashFunction(unsigned char *string, register unsigned len)
{
	register unsigned accum;

	for (accum = 0; len != 0; len--) {
		accum <<= 1;
		accum += (unsigned) (*string++ & 0xFF);
	}
	return (accum);
}

/*
 * Returns TRUE if at least one entry for the given key exists; FALSE
 * otherwise.
 */
static int
hash_Exists(hash_tbl *hashtable, unsigned hashcode, int (*compare)(),
    hash_datum *key)
{
	register hash_member *memberptr;

	memberptr = (hashtable->table)[hashcode % (hashtable->size)];
	while (memberptr) {
		if ((*compare)(key, memberptr->data))
			return (TRUE); /* Entry does exist */
		memberptr = memberptr->next;
	}
	return (FALSE);	/* Entry does not exist */
}

/*
 * Insert the data item "element" into the hash table using "hashcode"
 * to determine the bucket number, and "compare" and "key" to determine
 * its uniqueness.
 *
 * If the insertion is successful 0 is returned.  If a matching entry
 * already exists in the given bucket of the hash table, or some other error
 * occurs, -1 is returned and the insertion is not done.
 */
int
hash_Insert(hash_tbl *hashtable, unsigned hashcode, int (*compare)(),
    hash_datum *key, hash_datum *element)
{
	hash_member *memberptr, *temp;

	hashcode %= hashtable->size;
	if (hash_Exists(hashtable, hashcode, compare, key))
		return (-1); /* At least one entry already exists */

	memberptr = (hashtable->table)[hashcode];
	if ((temp = (hash_member *)malloc(sizeof (hash_member))) != NULL) {
		(hashtable->table)[hashcode] = temp;
		temp->data = element;
		temp->next = memberptr;
		return (0); /* Success */
	}
	return (-1); /* malloc failed! */
}

/*
 * Locate and return the data entry associated with the given key.
 *
 * If the data entry is found, a pointer to it is returned.  Otherwise,
 * NULL is returned.
 */
hash_datum *
hash_Lookup(hash_tbl *hashtable, unsigned hashcode, int (*compare)(),
    hash_datum *key)
{
	hash_member *memberptr;

	memberptr = (hashtable->table)[hashcode % (hashtable->size)];
	while (memberptr) {
		if ((*compare)(key, memberptr->data))
			return (memberptr->data);
		memberptr = memberptr->next;
	}
	return (NULL);
}
