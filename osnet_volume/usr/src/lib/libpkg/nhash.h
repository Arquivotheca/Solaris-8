/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ident	"@(#)nhash.h	1.4	93/03/31 SMI"

#ifndef NULL
#define	NULL	0
#endif	/* NULL */

typedef struct item_t {
    void *key;
    int	  keyl;
    void *data;
    int	  datal;
} Item;

#define	Null_Item ((Item *) NULL)

typedef struct bucket_t {
	int nent;
	int nalloc;
	Item **itempp;
} Bucket;

typedef struct cache_t {
	int	hsz;
	int	bsz;
	Bucket *bp;
	int (*hfunc)(void *, int, int);
	int (*cfunc)(void *, void *, int);
} Cache;

#ifdef _KERNEL
#define	malloc	bkmem_alloc
#endif	/* _KERNEL */

extern int init_cache(Cache **cp, int hsz, int bsz,
	    int (*hfunc)(void *, int, int), int (*cfunc)(void *, void *, int));
extern int add_cache(Cache *cp, Item *itemp);
extern Item *lookup_cache(Cache *cp, void *datap, int datalen);
