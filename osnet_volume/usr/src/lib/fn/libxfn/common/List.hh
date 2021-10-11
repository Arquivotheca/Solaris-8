/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LIST_HH
#define	_LIST_HH

#pragma ident	"@(#)List.hh	1.3	96/03/31 SMI"

// generic list (stores subclasses of ListItem)

// for now this is a double-linked list.

class ListItem {
    public:
	virtual ~ListItem();
	virtual ListItem* copy() = 0;

	ListItem *next, *prev;
};

class List {
    public:
	List();
	~List();
	List(const List&);
	List& operator=(const List&);
	unsigned count();
	const ListItem* first(void*& iter_pos);
	const ListItem* last(void*& iter_pos);
	const ListItem* next(void*& iter_pos);
	const ListItem* prev(void*& iter_pos);
	prepend_item(ListItem*);
	append_item(ListItem*);
	insert_item(void*& iter_pos, ListItem*);
	delete_item(void*& iter_pos);
	delete_all();

    private:
	void __delete_all();
	void __copy(const List&);

	unsigned item_count;
	ListItem* first_item;
	ListItem* last_item;
};

#endif /* _LIST_HH */
