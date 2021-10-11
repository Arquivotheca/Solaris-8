/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)hash.h	1.3	97/11/23 SMI"

/*
  @(#)SSM2 hash.h 1.2 90/11/13 
*/

/*
 * File: hash.h
 *
 * Copyright (C) 1990 Sun Microsystems Inc.
 * All Rights Reserved.
 */


/*
 *    Change Log
 * ============================================================================
 * Author      Date       Change 
 * barts     13 Nov 90	  Created.
 *
 */

#ifndef _hash_h
#define _hash_h

typedef struct hash_entry {
  struct hash_entry 
    * next_entry,
    * right_entry,
    * left_entry;
  char *       	key;
  char * 	data;
} hash_entry;

typedef struct hash {
  size_t	size;
  hash_entry ** table;
  hash_entry * 	start;   
  enum hash_type { String_Key = 0 , Integer_Key = 1} hash_type;
} hash;

hash * 		make_hash(size_t size);
hash * 		make_ihash(size_t size);
char ** 	get_hash(hash * tbl, char * key);
char **		find_hash(hash * tbl, const char * key);
char *		del_hash(hash * tbl, const char * key);
size_t 		operate_hash(hash * tbl, void (*ptr)(), const char * usr_arg);
void 		destroy_hash(hash * tbl, int (*ptr)(), const char * usr_arg);

#endif /* _hash_h */









