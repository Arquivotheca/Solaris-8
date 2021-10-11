/*
 * Copyright (c) 1994,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STABS_H
#define	_SYS_STABS_H

#pragma ident	"@(#)stabs.h	1.3	99/06/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "pat.h"

#define	MAXLINE	1024

/* Hash tables for stabs info ???. */

#define	BUCKETS	128
#define	HASH(NUM)	((int)(NUM & (BUCKETS - 1)))

struct	tdesc	*hash_table[BUCKETS];
struct	tdesc	*name_table[BUCKETS];

/* Structures for holding user's specification for how to display a struct. */

struct node {	/* per struct */
	char		 *struct_name;	/* struct name */
	char		 *prefix;	/* member prefix; remove from names */
	char		 *name;		/* name to use */
	struct child	 *child;	/* NULL-term list of child nodes */
	struct child	**last_child_link; /* ptr to last link in child list */
};

struct child {	/* per struct member */
	pat_handle_t	 pattern;	/* spec for members to apply to */
	char		*format;	/* format to display it in */
	char		*label;		/* what to call it */
	struct child	*next;		/* next child in list */
};

/* Data structures to contain info derived from the stabs. */

enum type {
	INTRINSIC,
	POINTER,
	ARRAY,
	FUNCTION,
	STRUCT,
	UNION,
	ENUM,
	FORWARD,
	TYPEOF,
	CONST,
	VOLATILE
};

struct tdesc {
	char	*name;
	struct	tdesc *next;
	enum	type type;
	int	size;
	union {
		struct	tdesc *tdesc;	/* *, f , to */
		struct	ardef *ardef;	/* ar */
		struct	mlist *members;	/* s, u */
		struct  elist *emem; /* e */
	} data;
	int	id;
	struct tdesc *hash;
};

struct elist {
	char	*name;
	int	number;
	struct elist *next;
};

struct element {
	struct tdesc *index_type;
	int	range_start;
	int	range_end;
};

struct ardef {
	struct tdesc	*contents;
	struct element	*indices;
};

struct mlist {	/* describes a struct/union member */
	int	offset;
	int	size;
	char	*name;
	struct	mlist *next;
	struct	tdesc *fdesc;		/* s, u */
};

#define	ALLOC(t)		((t *)malloc(sizeof (t)))

struct	tdesc *lookupname();

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STABS_H */
