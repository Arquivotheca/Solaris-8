/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STABS_H
#define	_SYS_STABS_H

#pragma ident	"@(#)stabs.h	1.12	98/12/04 SMI"

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	MAXLINE	8192

#define	BUCKETS	128

struct node {
	char *name;
	char *format;
	char *format2;
	struct child *child;
};

struct	child {
	char *name;
	char *format;
	struct child *next;
};

#define	HASH(NUM)		((int)(NUM & (BUCKETS - 1)))

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
	VOLATILE,
	CONST
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

struct mlist {
	int	offset;
	int	size;
	char	*name;
	struct	mlist *next;
	struct	tdesc *fdesc;		/* s, u */
};

struct model_info {
	char *name;
	size_t pointersize;
	size_t charsize;
	size_t shortsize;
	size_t intsize;
	size_t longsize;
};

extern struct tdesc *lookupname(char *);
extern void parse_input(void);
extern char *convert_format(char *format, char *dfault);
extern struct child *find_child(struct node *np, char *w);
extern char *uc(const char *s);

extern boolean_t error;
extern struct model_info *model;

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_STABS_H */
