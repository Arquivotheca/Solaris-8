/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRSORT_H
#define	_PRSORT_H

#pragma ident	"@(#)prsort.h	1.2	99/09/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "prstat.h"

typedef ulong_t (*keyfunc_t)(void *);

/*
 * sorted list of pointers to lwps and ulwps
 */

typedef struct lp_list {
	int		lp_nptrs;	/* number of allocated pointers */
	int		lp_cnt;		/* number of used pointers */
	int		lp_sortorder;	/* sorting order for the list */
	keyfunc_t	lp_func;	/* pointer to key function */
	void		**lp_ptrs;	/* pointer to array of pointers */
} lp_list_t;

extern void list_alloc(lp_list_t *, int);
extern void list_free(lp_list_t *);
extern void list_set_keyfunc(char *, optdesc_t *, lp_list_t *);
extern ulong_t get_keyval(lp_list_t *, void *);
extern void sort_lwps(lp_list_t *, lwp_info_t *);
extern void sort_ulwps(lp_list_t *, ulwp_info_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRSORT_H */
