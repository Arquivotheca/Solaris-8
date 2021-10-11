/*
 *
 *			all.h
 *
 * Include file for the cfsd_all class.
 */

#ident   "@(#)cfsd_all.h 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#ifndef CFSD_ALL
#define	CFSD_ALL

/* get define for _SYS_NMLN */
#include <sys/utsname.h>

typedef struct cfsd_all_object {
	char			 i_machname[SYS_NMLN]; /* machine name */
	cfsd_cache_object_t	*i_cachelist;		 /* list of caches */
	int			 i_cachecount;		 /* # of objs on list */
	mutex_t			 i_lock;		 /* synchro lock */
	int			 i_nextcacheid;		 /* for cache ids */
	int			 i_modify;		 /* changed when mod */
#ifdef HOARD_CLASS
	cfsd_hoard		*i_hoardp;		 /* hoarding class */
#endif

} cfsd_all_object_t;

cfsd_all_object_t *cfsd_all_create(void);
void cfsd_all_destroy(cfsd_all_object_t *cfsd_all_object_p);

void all_lock(cfsd_all_object_t *all_object_p);
void all_unlock(cfsd_all_object_t *all_object_p);

cfsd_cache_object_t *all_cachelist_at(cfsd_all_object_t *all_object_p,
    size_t index);
void all_cachelist_add(cfsd_all_object_t *all_object_p,
    cfsd_cache_object_t *cache_object_p);
cfsd_cache_object_t *all_cachelist_find(cfsd_all_object_t *all_object_p,
    const char *namep);

void all_cachefstab_update(cfsd_all_object_t *all_object_p);

#endif /* CFSD_ALL */
