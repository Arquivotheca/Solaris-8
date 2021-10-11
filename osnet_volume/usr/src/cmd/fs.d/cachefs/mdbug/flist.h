/*
 *
 *			flist.h
 *
 *   Defines a simple fixed size stack oriented list class
 *
 */
#pragma ident   "@(#)flist.h 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

/*
 * .LIBRARY Base
 * .FILE flist.cxx
 * .FILE flist.h
 * .NAME flist - simple list manager class
 *
 * .DESCRIPTION
 * The flist class is a simple list manager class designed specifically
 * for use by the mdbug package.
 * There is no destuctor for the class, so when an flist object is
 * deleted any objects still on the list are forgotten.
 */

#ifndef FLIST_H
#define	FLIST_H

#define	FLIST_SIZE  10	/* The number of items that will fit on list. */

typedef struct flist_object {
	char	*f_items[FLIST_SIZE];	/* Pointers to the items. */
	int	 f_index;		/* Index of item returned by next(). */
	int	 f_count;		/* Number of items on list. */

} flist_object_t;

flist_object_t *flist_create();
void	 flist_destroy(flist_object_t *flist_object_p);
void	 fl_push(flist_object_t *flist_object_p, void *);
void	 fl_pop(flist_object_t *flist_object_p);
void	*fl_top(flist_object_t *flist_object_p);
void	*fl_next(flist_object_t *flist_object_p);
void	 fl_clear(flist_object_t *flist_object_p);
int	 fl_space(flist_object_t *flist_object_p);
#endif /* FLIST_H */
