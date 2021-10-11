// ------------------------------------------------------------
//
//			flist.h
//
//   Defines a simple fixed size stack oriented list class
//

#pragma ident   "@(#)flist.h 1.1     94/10/28 SMI"
// Copyright (c) 1994 by Sun Microsystems, Inc.

// .LIBRARY Base
// .FILE flist.cxx
// .FILE flist.h
// .NAME flist - simple list manager class

// .DESCRIPTION
// The flist class is a simple list manager class designed specifically
// for use by the mdbug package.
// There is no destuctor for the class, so when an flist object is
// deleted any objects still on the list are forgotten.

#ifndef FLIST_H
#define	FLIST_H

const int FLIST_SIZE = 10;	// The number of items that will fit on list.

class flist
{
private:
	char	*f_items[FLIST_SIZE];	// Pointers to the items.
	int	 f_index;		// Index of item returned by next().
	int	 f_count;		// Number of items on list.

public:
	flist();			// Constructor
	void	 fl_push(void *);	// Place on top of list.
	void	 fl_pop();		// Remove top of list.
	void	*fl_top();		// Return top item on list.
	void	*fl_next();		// Return next item on list.
	void	 fl_clear();		// Removes and frees all items on list.
	int	 fl_count();		// Return number of items on list.
	int	 fl_space();		// Return amount of free space on list.
};

// ------------------------------------------------------------
//
//		fl_count
//
// Description:
// Arguments:
// Returns:
//	Returns the number of items on the list.
// Errors:
// Preconditions:

inline int
flist::fl_count()
{
	return (f_count);
}

// ------------------------------------------------------------
//
//		fl_space
//
// Description:
// Arguments:
// Returns:
//	Returns the number of free slots on the list.
// Errors:
// Preconditions:

inline int
flist::fl_space()
{
	return (FLIST_SIZE - f_count);
}

#endif /* FLIST_H */
