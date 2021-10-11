/*
 *
 *		flist.c
 *
 * Defines the flist class.
 */
#pragma ident   "@(#)flist.c 1.1     96/02/23 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#include <stdio.h>
#include <stdlib.h>
#include "flist.h"
#include "mdbug.h"

/*
 *
 *		flist_create
 *
 * Description:
 *	Constructor for the flist class.
 * Arguments:
 * Returns:
 * Preconditions:
 */
flist_object_t *
flist_create()
{
	flist_object_t *flist_object_p;

	flist_object_p = (flist_object_t *)calloc(sizeof (flist_object_t), 1);

	if (flist_object_p == NULL)
		doabort();

	flist_object_p->f_count = 0;
	flist_object_p->f_index = 0;
	return (flist_object_p);
}

/*
 *
 *		flist_destroy
 *
 * Description:
 *	Destructor for the flist class.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
flist_destroy(flist_object_t *flist_object_p)
{
	free(flist_object_p);
}
/*
 *
 *		fl_push
 *
 * Description:
 *	Adds the specified pointer to the top of the list
 *	if there is room.  If there is no more room then
 *	nothing happens.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fl_push(flist_object_t *flist_object_p, void *ptr)
{
	if (flist_object_p->f_count < FLIST_SIZE) {
		flist_object_p->f_items[flist_object_p->f_count] = (char *)ptr;
		flist_object_p->f_count++;
	}
}

/*
 *
 *		fl_pop
 *
 * Description:
 *	Removes the top item from the list.
 *	No action is taken if the list is empty.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fl_pop(flist_object_t *flist_object_p)
{
	if (flist_object_p->f_count > 0)
		flist_object_p->f_count--;
}

/*
 *
 *		fl_top
 *
 * Description:
 *	Returns the top item on the list.
 *	Sets the internal state so that a following call to
 *	next() will return the second item on the list.
 *	Returns NULL if the list is empty.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void *
fl_top(flist_object_t *flist_object_p)
{
	flist_object_p->f_index = flist_object_p->f_count;
	return (fl_next(flist_object_p));
}

/*
 *
 *		fl_next
 *
 * Description:
 *	Returns the next item on the list.  NULL if there
 *	is no next item.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void *
fl_next(flist_object_t *flist_object_p)
{
	if (flist_object_p->f_index > 0) {
		flist_object_p->f_index--;
		return (flist_object_p->f_items[ flist_object_p->f_index ]);
	} else {
		return (NULL);
	}
}

/*
 *
 *		fl_clear
 *
 * Description:
 *	Removes all items from the list and frees them.
 * Arguments:
 * Returns:
 * Preconditions:
 */
void
fl_clear(flist_object_t *flist_object_p)
{
	void *p1;
	while ((p1 = fl_top(flist_object_p)) != NULL) {
		free(p1);
		fl_pop(flist_object_p);
	}
}
/*
 *
 *		fl_space
 *
 * Description:
 * Arguments:
 * Returns:
 *	Returns the number of free slots on the list.
 * Errors:
 * Preconditions:
 */
int
fl_space(flist_object_t *flist_object_p)
{
	return (FLIST_SIZE - flist_object_p->f_count);
}
