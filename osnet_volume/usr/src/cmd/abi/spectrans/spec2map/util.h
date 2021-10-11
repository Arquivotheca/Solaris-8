/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _UTIL_H
#define	_UTIL_H

#pragma ident	"@(#)util.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* String tables */
typedef struct table_head_t {
	int nelem;
	int used;
	char *elements[1];	/* Actually elements[nelem] */
} table_t;

#define	TABLE_INITIAL	50
#define	TABLE_INCREMENT	50


extern char *get_stringtable(table_t *, int);
extern int in_stringtable(table_t *, const char *);
extern int in_stringset(char *, char *);
extern void print_stringtable(table_t *);
extern void sort_stringtable(table_t *);

/* Caveat: never discard return value: see note in .c file. */
extern table_t *add_to_stringtable(table_t *, char *);

extern table_t *create_stringtable(int);
extern table_t *free_stringtable(table_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _UTIL_H */
