/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 */
#ifndef	_CACHE_DOT_H
#define	_CACHE_DOT_H

#pragma ident	"@(#)cache_a.out.h	1.5	98/08/28 SMI"

/*
 * ld.so directory caching
 */
#include	<sys/types.h>

/*
 * Shared object lookup performance in the run-time link editor is
 * enhanced through the use of caches for directories that the editor
 * searches.  A given "cache" describes the contents of a single directory,
 * and each cache entry contains the canonical name for a shared object
 * as well as its absolute pathname.
 *
 * Within a cache, "pointers" are really relative addresses to some absolute
 * address (often the base address of the containing database).
 */

/*
 * Relative pointer macros.
 */
#define	RELPTR(base, absptr) ((long)(absptr) - (long)(base))
#define	AP(base) ((caddr_t)base)

/*
 * Definitions for cache structures.
 */
#define	DB_HASH		11		/* number of hash buckets in caches */
#define	LD_CACHE_MAGIC 	0x041155	/* cookie to identify data structure */
#define	LD_CACHE_VERSION 0		/* version number of cache structure */

struct	dbe	{			/* element of a directory cache */
	long	dbe_next;		/* (rp) next element on this list */
	long	dbe_lop;		/* (rp) canonical name for object */
	long	dbe_name;		/* (rp) absolute name */
};

struct	db	{			/* directory cache database */
	long	db_name;		/* (rp) directory contained here */
	struct	dbe db_hash[DB_HASH];	/* hash buckets */
	caddr_t	db_chain;		/* private to database mapping */
};

struct dbf 	{			/* cache file image */
	long dbf_magic;			/* identifying cookie */
	long dbf_version;		/* version no. of these dbs */
	long dbf_machtype;		/* machine type */
	long dbf_db;		/* directory cache dbs */
};

/*
 * Structures used to describe and access a database.
 */
struct	dbd	{			/* data base descriptor */
	struct	dbd *dbd_next;		/* next one on this list */
	struct	db *dbd_db;		/* data base described by this */
};

struct	dd	{			/* directory descriptor */
	struct	dd *dd_next;		/* next one on this list */
	struct	db *dd_db;		/* data base described by this */
};

/*
 * Interfaces imported/exported by the lookup code.
 */

char	*ask_db();			/* ask db for highest minor number */
struct	db *lo_cache();			/* obtain cache for directory name */

#endif
