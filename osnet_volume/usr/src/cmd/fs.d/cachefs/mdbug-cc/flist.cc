// ------------------------------------------------------------
//
//		flist.cxx
//
// Defines the flist class.

#pragma ident   "@(#)flist.cc 1.1     94/10/28 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

#include <stdio.h>
#include <stdlib.h>
#include "flist.h"

// ------------------------------------------------------------
//
//		flist
//
// Description:
//	Constructor for the flist class.
// Arguments:
// Returns:
// Preconditions:

flist::flist()
{
	f_count = 0;
	f_index = 0;
}

// ------------------------------------------------------------
//
//		fl_push
//
// Description:
//	Adds the specified pointer to the top of the list
//	if there is room.  If there is no more room then
//	nothing happens.
// Arguments:
// Returns:
// Preconditions:

void flist::fl_push(void *ptr)
{
	if (f_count < FLIST_SIZE) {
		f_items[f_count] = (char *)ptr;
		f_count++;
	}
}

// ------------------------------------------------------------
//
//		fl_pop
//
// Description:
//	Removes the top item from the list.
//	No action is taken if the list is empty.
// Arguments:
// Returns:
// Preconditions:

void
flist::fl_pop()
{
	if (f_count > 0)
		f_count--;
}

// ------------------------------------------------------------
//
//		fl_top
//
// Description:
//	Returns the top item on the list.
//	Sets the internal state so that a following call to
//	next() will return the second item on the list.
//	Returns NULL if the list is empty.
// Arguments:
// Returns:
// Preconditions:

void *
flist::fl_top()
{
	f_index = f_count;
	return (fl_next());
}

// ------------------------------------------------------------
//
//		fl_next
//
// Description:
//	Returns the next item on the list.  NULL if there
//	is no next item.
// Arguments:
// Returns:
// Preconditions:

void *
flist::fl_next()
{
	if (f_index > 0) {
		f_index--;
		return (f_items[ f_index ]);
	} else {
		return (NULL);
	}
}

// ------------------------------------------------------------
//
//		fl_clear
//
// Description:
//	Removes all items from the list and frees them.
// Arguments:
// Returns:
// Preconditions:

void
flist::fl_clear()
{
	void *p1;
	while ((p1 = fl_top()) != NULL) {
		free(p1);
		fl_pop();
	}
}
