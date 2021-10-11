/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hash.c	1.1	94/12/05 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <synch.h>
#include <memory.h>
#include <getxby_door.h> 

static int    hash_string();

hash_t * 
make_hash(size)
int size;
{
	hash_t *ptr;

	ptr        = (hash_t *) malloc(sizeof(*ptr));
	ptr->size  =   size;
	ptr->table = (hash_entry_t **)
	    malloc( (unsigned) (sizeof(hash_entry_t *) * size) );
	(void)memset((char *) ptr->table, (char) 0, sizeof(hash_entry_t *)*size);
	ptr->start = NULL;
	ptr->hash_type = String_Key;
	return(ptr);
}

hash_t * 
make_ihash(size)
int size;
{
	hash_t * ptr;

	ptr        = (hash_t *) malloc(sizeof(*ptr));
	ptr->size  =   size;
	ptr->table = (hash_entry_t **) malloc((unsigned)
	    (sizeof(hash_entry_t *) * size) );
	(void)memset((char *) ptr->table, (char) 0,
	    sizeof(hash_entry_t *)*size);
	ptr->start = NULL;
	ptr->hash_type = Integer_Key;
	return(ptr);
}


char ** 
get_hash(hash_t *tbl, char *key)
{

	int bucket;
	hash_entry_t *tmp;
	hash_entry_t *new;

	if (tbl->hash_type == String_Key) {
		tmp = tbl->table[bucket = hash_string(key, tbl->size)];
	} else {
		tmp = tbl->table[bucket = abs((int)key) % tbl->size];
	}

	if (tbl->hash_type == String_Key) {
		while (tmp != NULL) {
			if (strcmp(tmp->key, key) == 0) {
				return(&tmp->data);
			}
			tmp = tmp->next_entry;
		}
	} else {
		while (tmp != NULL) {
			if (tmp->key == key) {
				return(&tmp->data);
			}
			tmp = tmp->next_entry;
		}
	}

	/*
	 * not found.... 
	 * insert new entry into bucket...
	 */

	new = (hash_entry_t *) malloc(sizeof(*new));
	new->key = ((tbl->hash_type == String_Key)?strdup(key):key);
	/*
	 * hook into chain from tbl...
	 */
	new->right_entry = NULL;
	new->left_entry = tbl->start;
	tbl->start = new;
	/*
	 * hook into bucket chain
	 */
	new->next_entry = tbl->table[bucket];
	tbl->table[bucket] = new;
	new->data = NULL;   /* so we know that it is new */
	return(&new->data);
}

char ** 
find_hash(hash_t *tbl, char *key)
{
	hash_entry_t 	*tmp;

	if (tbl->hash_type == String_Key) {
		tmp = tbl->table[hash_string(key, tbl->size)];
		for ( ;tmp != NULL; tmp = tmp->next_entry) {
			if (!strcmp(tmp->key, key)) {
				return(&tmp->data);
			}
		}
	} else {
		tmp = tbl->table[abs((int)key) % tbl->size];
		for ( ;tmp != NULL; tmp = tmp->next_entry) {
			if (tmp->key == key) {
				return(&tmp->data);
			}
		}
	}

	return(NULL);
}

char * 
del_hash(hash_t *tbl, char *key)
{
	int bucket;
	hash_entry_t * tmp, * prev = NULL;

	if (tbl->hash_type == String_Key) {
		bucket = hash_string(key, tbl->size);
	} else {
		bucket = abs((int)key) % tbl->size;
	}

	if ((tmp = tbl->table[bucket]) == NULL) {
		return(NULL);
	} else {
		if (tbl->hash_type == String_Key) {
			while (tmp != NULL) {
				if (!strcmp(tmp->key, key)) {
					break;  /* found item to delete ! */
				}
				prev = tmp;
				tmp  = tmp->next_entry;
			}
		} else {
			while (tmp != NULL) {
				if (tmp->key == key) {
					break;
				}
				prev = tmp;
				tmp  = tmp->next_entry;
			}
		}
		if (tmp == NULL) {
			return(NULL); /* not found */
		}
	}
	/*
	 * tmp now points to entry marked for deletion, prev to 
	 * item preceeding in bucket chain or NULL if tmp is first.
	 * remove from bucket chain first....
	 */
	if (tbl->hash_type == String_Key) {
		free(tmp->key);
	}
	if (prev!=NULL) {
		prev->next_entry = tmp->next_entry;
	} else {
		tbl->table[bucket] = tmp->next_entry;
	}
	/*
	 *now remove from tbl chain....
	 */
	if (tmp->right_entry != NULL) { /* not first in chain.... */
		tmp->right_entry->left_entry = (tmp->left_entry ? 
		    tmp->left_entry->right_entry: NULL);
	} else {
		tbl->start = (tmp->left_entry ?tmp->left_entry->right_entry:
		   NULL);
	}
	return(tmp->data);
}

int
operate_hash(hash_t *tbl, void (*ptr)(), char *usr_arg)
{
	hash_entry_t * tmp = tbl->start;
	int c = 0;

	while (tmp) {
		(*ptr)(tmp->data, usr_arg, tmp->key);
		tmp = tmp->left_entry;
		c++;
	}
	return(c);
}

int
operate_hash_addr(hash_t *tbl, void (*ptr)(), char *usr_arg)
{
	hash_entry_t * tmp = tbl->start;
	int c = 0;

	while (tmp) {
		(*ptr)(&(tmp->data), usr_arg, tmp->key);
		tmp = tmp->left_entry;
		c++;
	}
	return(c);
}

void 
destroy_hash(hash_t *tbl, int (*ptr)(), char *usr_arg)
{
	hash_entry_t * tmp = tbl->start, * prev;

	while (tmp) {
		if (ptr) {
			(*ptr)(tmp->data,usr_arg, tmp->key);
		}

		if (tbl->hash_type == String_Key) {
			free(tmp->key);
		}
		prev = tmp;
		tmp = tmp->left_entry;
		free((char *)prev);
	}
	free((char *)tbl->table);
	free(tbl);
}

static int 
hash_string(char *s, int modulo)
{
	unsigned result = 0;
	int i=1;

	while (*s != 0) {
		result += (*s++ << i++);
	}

	return(result % modulo);
}


