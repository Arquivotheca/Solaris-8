/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prsort.c	1.2	99/09/08 SMI"

#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "prstat.h"
#include "prutil.h"
#include "prsort.h"

void
list_alloc(lp_list_t *lp, int nptrs)
{
	if (nptrs > 0) {
		lp->lp_nptrs = nptrs;
		lp->lp_ptrs = Zalloc(sizeof (void *) * (nptrs + 1));
	}
}

void
list_free(lp_list_t *lp)
{
	if (lp && lp->lp_ptrs) {
		free(lp->lp_ptrs);
		lp->lp_ptrs = NULL;
	}
}

/*
 * Sorting routines
 */

static ulong_t
get_cpu_from_psinfo(void *lwp)
{
	return ((ulong_t)
	    FRC2PCT((((lwp_info_t *)lwp)->li_info.pr_lwp.pr_pctcpu)*1000));
}

static ulong_t
get_cpu_from_usage(void *lwp)
{
	lwp_info_t *p = (lwp_info_t *)lwp;
	float cpu = 0;
	cpu += p->li_usr;
	cpu += p->li_sys;
	cpu *= 1000;
	return ((ulong_t)cpu);
}

static ulong_t
get_time(void *lwp)
{
	return ((ulong_t)TIME2SEC(((lwp_info_t *)lwp)->li_info.pr_lwp.pr_time));
}

static ulong_t
get_size(void *lwp)
{
	return ((ulong_t)((lwp_info_t *)lwp)->li_info.pr_size);
}

static ulong_t
get_rssize(void *lwp)
{
	return ((ulong_t)((lwp_info_t *)lwp)->li_info.pr_rssize);
}

static ulong_t
get_pri(void *lwp)
{
	return ((ulong_t)((lwp_info_t *)lwp)->li_info.pr_lwp.pr_pri);
}

static ulong_t
get_ulwp_key(void *ulwp)
{
	return (((ulwp_info_t *)ulwp)->ui_key);
}

void
list_set_keyfunc(char *arg, optdesc_t *opt, lp_list_t *list)
{
	if (list == NULL)
		return;

	list->lp_sortorder = opt->o_sortorder;
	if (arg == NULL) {	/* special case for ulwp_infos */
		list->lp_func = get_ulwp_key;
		return;
	}
	if (strcmp("cpu", arg) == 0) {
		if (opt->o_outpmode & OPT_USAGE)
			list->lp_func = get_cpu_from_usage;
		else
			list->lp_func = get_cpu_from_psinfo;
		return;
	}
	if (strcmp("time", arg) == 0) {
		list->lp_func = get_time;
		return;
	}
	if (strcmp("size", arg) == 0) {
		list->lp_func = get_size;
		return;
	}
	if (strcmp("rss", arg) == 0) {
		list->lp_func = get_rssize;
		return;
	}
	if (strcmp("pri", arg) == 0) {
		list->lp_func = get_pri;
		return;
	}
	Die(gettext("invalid sort key -- %s\n"), arg);
}

ulong_t
get_keyval(lp_list_t *lp, void *ptr)
{
	return (lp->lp_func(ptr));
}

static int
compare_keys(lp_list_t *lp, ulong_t key1, ulong_t key2)
{
	if (key1 == key2)
		return (0);
	if (key1 < key2)
		return (1 * lp->lp_sortorder);
	else
		return (-1 * lp->lp_sortorder);
}

static void
list_insert(lp_list_t *lp, void *ptr)
{
	int i, j;
	long k1, k2;

	for (i = 0; i < lp->lp_cnt; i++) {	/* insert in the middle */
		k1 = get_keyval(lp, ptr);
		k2 = get_keyval(lp, lp->lp_ptrs[i]);
		if (compare_keys(lp, k1, k2) >= 0) {
			for (j = lp->lp_cnt-1; j >= i; j--)
				lp->lp_ptrs[j+1] = lp->lp_ptrs[j];
			lp->lp_ptrs[i] = ptr;
			if (lp->lp_cnt < lp->lp_nptrs)
				lp->lp_cnt++;
			return;
		}
	}
	if (i+1 <= lp->lp_nptrs) {		/* insert at the tail */
		lp->lp_ptrs[lp->lp_cnt] = ptr;
		lp->lp_cnt++;
	}
}

static void
list_preinsert(lp_list_t *lp, void *ptr)
{
	ulong_t	k1, k2;

	if (lp->lp_cnt < lp->lp_nptrs) {	/* just add */
		list_insert(lp, ptr);
		return;
	}
	k1 = get_keyval(lp, lp->lp_ptrs[lp->lp_cnt-1]);
	k2 = get_keyval(lp, ptr);
	if (compare_keys(lp, k1, k2) >= 0)	/* skip insertion */
		return;
	k1 = get_keyval(lp, lp->lp_ptrs[0]);
	if (compare_keys(lp, k2, k1) >= 0) {	/* add at the head */
		list_insert(lp, ptr);
		return;
	}
	list_insert(lp, ptr);
}


void
sort_lwps(lp_list_t *lp, lwp_info_t *lwp_head)
{
	lwp_info_t *lwp = lwp_head;

	(void) memset(lp->lp_ptrs, 0, sizeof (void *) * lp->lp_nptrs);
	lp->lp_cnt = 0;
	while (lwp) {
		list_preinsert(lp, (void *)lwp);
		lwp = lwp->li_next;
	}
}

void
sort_ulwps(lp_list_t *lp, ulwp_info_t *ulwp_head)
{
	ulwp_info_t *ulwp = ulwp_head;

	(void) memset(lp->lp_ptrs, 0, sizeof (void *) * lp->lp_nptrs);
	lp->lp_cnt = 0;
	while (ulwp) {
		list_preinsert(lp, (void *)ulwp);
		ulwp = ulwp->ui_next;
	}
}
