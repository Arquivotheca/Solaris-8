/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hash.h	1.1	94/12/05 SMI"

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
  int 		size;
  hash_entry ** table;
  hash_entry * 	start;   
  enum hash_type { String_Key = 0 , Integer_Key = 1} hash_type;
} hash;

hash * 		make_hash();
hash * 		make_ihash();
char ** 	get_hash();
char **		find_hash();
char *		del_hash();
int  		operate_hash();
void 		destroy_hash();

#endif /* _hash_h */
