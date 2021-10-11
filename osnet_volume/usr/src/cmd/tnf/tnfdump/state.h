/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef	_STATE_H
#define	_STATE_H

#pragma ident	"@(#)state.h	1.2	95/07/31 SMI"

#include <sys/time.h>
#include <tnf/tnf.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * data structures for table of record pointers:
 *	time: timestamp of record
 *	record: handle on record
 */
typedef struct entry {
	hrtime_t	time;
	tnf_datum_t	record;
} entry_t;

void print_c_header		(void);
void describe_c_record		(tnf_datum_t);
void print_sorted_events(void);

void describe_scalar		(tnf_datum_t);

void fail			(int, char *, ...);

/* routines for manipulating table of records */
void table_insert		(entry_t *);
void table_sort			(void);
void table_print		(void (*print_elem)(entry_t *));
int table_get_num_elements(void);
entry_t *table_get_entry_indexed(int);

#ifdef __cplusplus
}
#endif

#endif /* _STATE_H */
