/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)tsort.c	1.27	99/09/08 SMI"

/*
 * Utilities to handle shared object dependency graph.
 *
 * The algorithms used in this file are taken from the following book:
 *	Algorithms in C
 *		Robert Sedgewick
 *		Addison-Wesley Publishing company
 *		ISBN 0-201-51425-7
 * 	From the following chapters:
 *		Chapter 29 Elementary Graph Algorithms
 *		Chapter 32 Directed Graph
 */
#include	"_synonyms.h"

#include	<sys/types.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<dlfcn.h>
#include	<signal.h>
#include	<locale.h>
#include	<string.h>
#include	<libintl.h>
#include	"_rtld.h"
#include	"msg.h"
#include	"debug.h"


/*
 * Structure for maintaining sorting state.
 */
typedef struct {
	Rt_map **	s_lmpa;		/* link-map[] (returned to caller) */
	Rt_map *	s_lmp;		/* originating link-map */
	Rt_map **	s_stack;	/* strongly connected component stack */
	List 		s_scc;		/* cyclic list */
	List		s_queue;	/* depth queue for cyclic components */
	int		s_sndx;		/* present stack index */
	int 		s_lndx1;	/* present link-map index for */
	int 		s_lndx2;	/* 	std and INITFIRST components */
	int		s_num;		/* number of objects to sort */
} Sort;


/*
 * qsort(3c) comparison functions.
 */
static int
f_compare(const void * lmpp1, const void * lmpp2)
{
	Rt_map *	lmp1 = *((Rt_map **)lmpp1);
	Rt_map *	lmp2 = *((Rt_map **)lmpp2);

	if (IDX(lmp1) > IDX(lmp2))
		return (1);
	if (IDX(lmp1) < IDX(lmp2))
		return (-1);
	return (0);
}

static int
r_compare(const void * lmpp1, const void * lmpp2)
{
	Rt_map *	lmp1 = *((Rt_map **)lmpp1);
	Rt_map *	lmp2 = *((Rt_map **)lmpp2);

	if (IDX(lmp1) > IDX(lmp2))
		return (-1);
	if (IDX(lmp1) < IDX(lmp2))
		return (1);
	return (0);
}

/*
 * This routine is called when cyclic dependency is detected between strongly
 * connected components.  The nodes within the cycle are breadth-first sorted.
 */
static int
sort_scc(Sort * sort, int fndx, int flag)
{
	int	lndx;
	int (*	fptr)(const void *, const void *);

	/*
	 * If this is the first cyclic dependency traverse the new objects that
	 * have been added to the link-map list and for each object establish
	 * a unique depth index.  We build this dynamically as we have no idea
	 * of the number of objects that will be inspected (logic matches that
	 * used by dlsym() to traverse lazy dependencies).
	 */
	if (sort->s_queue.head == 0) {
		Rt_map *	lmp = sort->s_lmp;
		int		idx = 1;
		Listnode *	lnp;

		if (list_append(&sort->s_queue, lmp) == 0)
			return (0);

		for (LIST_TRAVERSE(&sort->s_queue, lnp, lmp)) {
			Listnode *	_lnp;
			Rt_map *	_lmp;

			IDX(lmp) = idx++;

			for (LIST_TRAVERSE(&EDEPENDS(lmp), _lnp, _lmp)) {
				if (IDX(_lmp))
					continue;

				if (list_append(&sort->s_queue, _lmp) == 0)
					return (0);
			}
		}
	}

	/*
	 * Establish the sort comparison routine and the the last sort index.
	 */
	if (flag & RT_SORT_REV) {
		fptr = r_compare;
		lndx = sort->s_lndx1;
	} else {
		fptr = f_compare;
		lndx = sort->s_lndx2;
	}

	DBG_CALL(Dbg_tsort_printscc(sort->s_lmpa, fndx, lndx, flag, 1));
	qsort(&(sort->s_lmpa[fndx]), lndx - fndx, sizeof (Rt_map *), fptr);
	DBG_CALL(Dbg_tsort_printscc(sort->s_lmpa, fndx, lndx, flag, 0));

	return (1);
}

/*
 * Insert a link-map on the link-map array.  Two pointers are maintained so that
 * INITFIRST objects can be manipulated correctly; during .init processing
 * (RT_SORT_REV) any INITFIRST objects are collected at the beginning of the
 * array, and during .fini (RT_SORT_FWD) they are collected at the end.
 */
static void
insert(Rt_map * tlmp, Sort * sort, int flag)
{
	int	first;

	if (FLAGS(tlmp) & FLG_RT_INITFRST)
		first = 1;
	else
		first = 0;

	if (((flag == RT_SORT_REV) && (first == 1)) ||
	    ((flag == RT_SORT_FWD) && (first == 0))) {
		if (sort->s_lndx1 > sort->s_lndx2) {
			(void) memmove(&(sort->s_lmpa[(sort->s_lndx2) + 1]),
			    &(sort->s_lmpa[sort->s_lndx2]),
			    ((sort->s_lndx1 - sort->s_lndx2) *
			    sizeof (Rt_map *)));
		}
		(sort->s_lndx1)++;
		sort->s_lmpa[(sort->s_lndx2)++] = tlmp;
	} else
		sort->s_lmpa[(sort->s_lndx1)++] = tlmp;
}

/*
 * Take elements off of the stack and move them to the link-map array. Typically
 * this routine just pops one strongly connented component (individual link-map)
 * at a time.  When a cyclic dependency has been detected the stack will contain
 * more than just the present object to process, and will trigger the later call
 * to sort_scc() to sort these elements.
 */
static int
visit(Rt_map * lmp, Sort * sort, int flag)
{
	List *		scc;
	int		num, tracing = LIST(lmp)->lm_flags & LML_TRC_ENABLE;
	Rt_map *	tlmp;

	if (tracing) {
		if ((scc = calloc(1, sizeof (List))) == NULL)
			return (0);
		if (list_append(&sort->s_scc, scc) == NULL)
			return (0);
	}

	num = sort->s_lndx1 - sort->s_lndx2;
	do {
		tlmp = sort->s_stack[--(sort->s_sndx)];
		insert(tlmp, sort, flag);

		SORTVAL(sort->s_stack[sort->s_sndx]) = sort->s_num;

		/*
		 * If tracing, save the strongly connected component.
		 */
		if (tracing) {
			if (list_append(scc, tlmp) == NULL)
				return (0);
		}
	} while (tlmp != lmp);

	/*
	 * Determine if there are cyclic dependencies to process.
	 */
	num += sort->s_lndx2;
	if (sort->s_lndx1 > (num + 1)) {
		if (sort_scc(sort, num, flag) == 0)
			return (0);
	}
	return (1);
}

/*
 * Reverse topological search (used to fire .init's).
 */
static int
rt_visit(Rt_map * lmp, Lm_list * lml, Sort * sort, int * id)
{
	int 		min;
	Listnode *	lnp;
	Rt_map *	dlmp;

	min = SORTVAL(lmp) = ++(*id);
	sort->s_stack[(sort->s_sndx)++] = lmp;

	for (LIST_TRAVERSE(&EDEPENDS(lmp), lnp, dlmp)) {
		int	_min;

		/*
		 * Only collect objects that belong to the callers link-map
		 * list and haven't already been collected.
		 */
		if ((LIST(dlmp) != lml) || (FLAGS(dlmp) & FLG_RT_INITCLCT))
			continue;

		if ((_min = SORTVAL(dlmp)) == 0) {
			if ((_min = rt_visit(dlmp, lml, sort, id)) == 0)
				return (0);
		}
		if (_min < min)
			min = _min;
	}
	for (LIST_TRAVERSE(&IDEPENDS(lmp), lnp, dlmp)) {
		int	_min;

		/*
		 * Only collect objects that belong to the callers link-map
		 * list and haven't already been collected.
		 */
		if ((LIST(dlmp) != lml) || (FLAGS(dlmp) & FLG_RT_INITCLCT))
			continue;

		if ((_min = SORTVAL(dlmp)) == 0) {
			if ((_min = rt_visit(dlmp, lml, sort, id)) == 0)
				return (0);
		}
		if (_min < min)
			min = _min;
	}

	FLAGS(lmp) |= FLG_RT_INITCLCT;

	if (min == SORTVAL(lmp)) {
		if (visit(lmp, sort, RT_SORT_REV) == 0)
			return (0);
	}
	return (min);
}

/*
 * Forward topological search (used to fire .fini's).
 */
static int
ft_visit(Rt_map * lmp, Lm_list * lml, Sort * sort, int * id, int flag)
{
	int 		min;
	Listnode *	lnp;
	Rt_map *	clmp;

	min = SORTVAL(lmp) = ++(*id);
	sort->s_stack[(sort->s_sndx)++] = lmp;

	for (LIST_TRAVERSE(&CALLERS(lmp), lnp, clmp)) {
		int	_min;

		/*
		 * Only collect objects that belong to the callers link-map
		 * list, that have had their .init fired, and haven't already
		 * been collected.
		 */
		if ((LIST(clmp) != lml) || (!((FLAGS(clmp) &
		    (FLG_RT_INITDONE | FLG_RT_FINICLCT)) == FLG_RT_INITDONE)))
			continue;

		if ((flag & RT_SORT_DELETE) &&
		    (!(FLAGS(clmp) & FLG_RT_DELETE)))
			continue;

		if ((_min = SORTVAL(clmp)) == 0) {
			if ((_min = ft_visit(clmp, lml, sort, id, flag)) == 0)
				return (0);
		}
		if (_min < min)
			min = _min;
	}

	FLAGS(lmp) |= FLG_RT_FINICLCT;

	if (min == SORTVAL(lmp)) {
		if (visit(lmp, sort, RT_SORT_FWD) == 0)
			return (0);
	}
	return (min);
}


/*
 * Reverse LD_BREATH search (used to fire .init's the old fashioned way).
 */
static void
rb_visit(Rt_map * lmp, Sort * sort)
{
	Rt_map *	nlmp;

	if ((nlmp = (Rt_map *)NEXT(lmp)) != 0)
		rb_visit(nlmp, sort);

	/*
	 * Only collect objects that haven't already been collected.
	 */
	if (!(FLAGS(lmp) & FLG_RT_INITCLCT)) {
		sort->s_lmpa[(sort->s_lndx1)++] = lmp;
		FLAGS(lmp) |= FLG_RT_INITCLCT;
	}
}

/*
 * Forward LD_BREATH search (used to fire .fini's the old fashioned way).
 */
static void
fb_visit(Rt_map * lmp, Sort * sort, int flag)
{
	while (lmp) {
		/*
		 * If we're called from dlclose() then we only collect those
		 * objects marked for deletion.
		 */
		if (!(flag & RT_SORT_DELETE) || (FLAGS(lmp) & FLG_RT_DELETE)) {
			/*
			 * Only collect objects that have had their .init fired,
			 * and haven't already been collected.
			 */
			if ((FLAGS(lmp) &
			    (FLG_RT_INITDONE | FLG_RT_FINICLCT)) ==
			    (FLG_RT_INITDONE)) {
				sort->s_lmpa[(sort->s_lndx1)++] = lmp;
				FLAGS(lmp) |= FLG_RT_FINICLCT;
			}
		}
		lmp = (Rt_map *)NEXT(lmp);
	}
}

/*
 * Find corresponding strongly connected component structure.
 */
static List *
trace_find_scc(Sort * sort, Rt_map * lmp)
{
	Listnode *	lnp1, * lnp2;
	List *		scc;
	Rt_map *	nlmp;

	for (LIST_TRAVERSE(&(sort->s_scc), lnp1, scc)) {
		for (LIST_TRAVERSE(scc, lnp2, nlmp)) {
			if (lmp == nlmp)
				return (scc);
		}
	}
	return (NULL);
}

/*
 * Print out the .init dependency information (ldd).
 */
static void
trace_sort(Sort * sort)
{
	int 		ndx = 0;
	List *		scc;
	Listnode *	lnp1, * lnp2;
	Rt_map *	lmp1, * lmp2, * lmp3;

	(void) printf(MSG_ORIG(MSG_STR_NL));

	while ((lmp1 = sort->s_lmpa[ndx++]) != NULL) {
		if (INIT(lmp1) == 0)
			continue;

		if (rtld_flags & RT_FL_BREADTH) {
			(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_01),
			    NAME(lmp1));
			continue;
		}

		if (((scc = trace_find_scc(sort, lmp1)) != NULL) &&
		    (scc->head == scc->tail)) {
			(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_01),
			    NAME(lmp1));
			continue;
		}

		(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_02), NAME(lmp1));
		for (LIST_TRAVERSE(&CALLERS(lmp1), lnp1, lmp2)) {
			for (LIST_TRAVERSE(scc, lnp2, lmp3)) {
				if (lmp2 != lmp3)
					continue;

				(void) printf(MSG_ORIG(MSG_LDD_FMT_FILE),
				    NAME(lmp3));
			}
		}
		(void) printf(MSG_ORIG(MSG_LDD_FMT_END));
	}
}

/*
 * Sort the dependency
 */
Rt_map **
tsort(Rt_map * lmp, int num, int flag)
{
	Listnode *	lnp1, * lnp2;
	Rt_map *	_lmp;
	Lm_list *	lml = LIST(lmp);
	int 		id = 0, tracing = lml->lm_flags & LML_TRC_ENABLE;
	Sort		sort = { 0 };

	if (flag & RT_SORT_REV)
		lml->lm_init = 0;

	if (num == 0)
		return (0);

	/*
	 * Allocate memory for link-map list array.  Calloc the array to insure
	 * all elements are zero, we might find that no objects need processing.
	 */
	sort.s_lmp = lmp;
	sort.s_num = num + 1;
	if ((sort.s_lmpa = calloc(sort.s_num,  sizeof (Rt_map *))) == NULL)
		return ((Rt_map **)S_ERROR);

	/*
	 * A breadth first search is easy, simply add each object to the
	 * link-map array.
	 */
	if (rtld_flags & RT_FL_BREADTH) {
		if (flag & RT_SORT_REV)
			rb_visit(lmp, &sort);
		else
			fb_visit(lmp, &sort, flag);

		/*
		 * If tracing (only meaningfull for RT_SORT_REV) print out the
		 * sorted dependencies.
		 */
		if (tracing)
			trace_sort(&sort);

		return (sort.s_lmpa);
	}

	/*
	 * We need to topologically sort the dependencies.
	 */
	if ((sort.s_stack = malloc(sort.s_num * sizeof (Rt_map *))) == NULL)
		return ((Rt_map **)S_ERROR);

	for (_lmp = lmp; _lmp; _lmp = (Rt_map *)NEXT(_lmp)) {
		if (flag & RT_SORT_REV) {
			/*
			 * Only collect objects that haven't already been
			 * collected.
			 */
			if (FLAGS(_lmp) & FLG_RT_INITCLCT)
				continue;

			if (rt_visit(_lmp, lml, &sort, &id) == 0)
				return ((Rt_map **)S_ERROR);

		} else if (!(flag & RT_SORT_DELETE) ||
		    (FLAGS(_lmp) & FLG_RT_DELETE)) {
			/*
			 * Only collect objects that have had their .init fired,
			 * and haven't already been collected.
			 */
			if (!((FLAGS(_lmp) &
			    (FLG_RT_INITDONE | FLG_RT_FINICLCT)) ==
			    FLG_RT_INITDONE))
				continue;

			if (ft_visit(_lmp, lml, &sort, &id, flag) == 0)
				return ((Rt_map **)S_ERROR);
		}
	}

	/*
	 * If tracing (only meaningfull for RT_SORT_REV), print out the sorted
	 * dependencies.
	 */
	if (tracing)
		trace_sort(&sort);

	/*
	 * Clean any temporary structures prior to return.  The caller is
	 * responsible for freeing the lmpa.
	 */
	if (sort.s_stack)
		free(sort.s_stack);

	/*
	 * Traverse the link-maps collected on the sort queue and delete the
	 * depth index.  They may be traversed again to sort other components
	 * either for .inits and almost certainly for .finis.
	 */
	if (sort.s_queue.head) {
		Listnode *	lnp, * _lnp = 0;

		for (LIST_TRAVERSE(&sort.s_queue, lnp, _lmp)) {
			IDX(_lmp) = 0;
			if (_lnp)
				free(_lnp);
			_lnp = lnp;
		}
		if (_lnp)
			free(_lnp);

		sort.s_queue.head = sort.s_queue.tail = 0;
	}

	lnp1 = sort.s_scc.head;
	while (lnp1) {
		Listnode *f1;
		List *scc = (List *)lnp1->data;

		lnp2 = scc->head;
		while (lnp2) {
			Listnode *f2;

			f2 = lnp2;
			lnp2 = lnp2->next;
			free(f2);
		}

		f1 = lnp1;
		lnp1 = lnp1->next;
		free(f1->data);
		free(f1);
	}
	return (sort.s_lmpa);
}
