/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_BUCKET_H
#define	_BUCKET_H

#pragma ident	"@(#)bucket.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct bucketlist {
	struct bucket *bl_bucket;
	struct bucketlist *bl_next;
};

typedef struct bucket {
	char *b_name;
	struct bucket *b_parent;
	struct bucketlist *b_uncles;
	struct bucket *b_thread;
	int b_has_locals;	/* Should contain ``local:*;'' */
	int b_was_printed;	/* For loop detection. */
	int b_weak;		/* Weak interface. */
	table_t *b_interface_table;
} bucket_t;


/* Bucket interfaces, general. */
extern void create_lists(void);
extern void delete_lists(void);
extern void print_bucket(const bucket_t *);
extern void print_all_buckets(void);

/* Transformation interfaces. */
extern void sort_buckets(void);
extern void thread_trees(void);
extern void add_local(void);

/* Composite interfaces for insertion. */
extern int add_by_name(const char *, const Interface *);
extern int add_parent(const char *, const char *, int);
extern int add_uncle(const char *, const char *, int);

/* Output Interfaces, iterators */
extern bucket_t *first_list(void);
extern bucket_t *next_list(void);
extern bucket_t *first_from_list(const bucket_t *);
extern bucket_t *next_from_list(void);

/* Output Interfaces, extraction. */
extern char **parents_of(const bucket_t *);

extern void set_weak(const char *, int);

typedef struct {
	int h_hash;
	char *h_version_name;
	bucket_t *h_bucket;
} hashmap_t;

#ifdef	__cplusplus
}
#endif

#endif /* _BUCKET_H */
