/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)List.cc	1.4 98/02/13 SMI"

#include "List.hh"

ListItem::~ListItem()
{
}

List::List()
{
	item_count = 0;
	first_item = last_item = 0;
}

void
List::__delete_all()
{
	ListItem *p, *np;
	for (p = first_item; p; p = np) {
		np = p->next;
		delete p;
	}
}

List::~List()
{
	__delete_all();
}

void
List::__copy(const List &l)
{
	ListItem *p, *pn, *n;

	item_count = l.item_count;
	for (p = l.first_item, pn = n = 0; p; p = p->next, pn = n) {
		n = p->copy();
		n->prev = pn;
		if (pn)
			pn->next = n;
		else
			first_item = n;
	}
	if (n)
		n->next = 0;
	else
		first_item = 0;
	last_item = n;
}

List::List(const List &l)
{
	__copy(l);
}

List &
List::operator=(const List &l)
{
	if (&l != this) {
		__delete_all();
		__copy(l);
	}
	return (*this);
}

unsigned
List::count()
{
	return (item_count);
}

// set point following first item (return first item)
const ListItem *
List::first(void *&iter_pos)
{
	iter_pos = first_item;
	return ((ListItem *)iter_pos);
}

// set point preceeding last item (return last item)
const ListItem *
List::last(void *&iter_pos)
{
	if (last_item == 0) {
		iter_pos = 0;
		return (0);
	}
	iter_pos = ((ListItem *)last_item)->prev;
	return ((ListItem *)last_item);
}

// set point following next item (return next item)
const ListItem *
List::next(void *&iter_pos)
{
	ListItem *p;

	if (iter_pos == 0) {
		iter_pos = first_item;
		return ((ListItem *)iter_pos);
	} else if (p = ((ListItem *)iter_pos)->next) {
		iter_pos = p;
		return (p);
	} else
		return (0);
}

// set point preceeding prev item (return prev item)
const ListItem *
List::prev(void *&iter_pos)
{
	ListItem *p;

	if (iter_pos == 0)
		return (0);
	p = (ListItem *)iter_pos;
	iter_pos = p->prev;
	return (p);
}

List::prepend_item(ListItem *n)
{
	n->prev = 0;
	n->next = first_item;
	if (item_count == 0)
		first_item = last_item = n;
	else {
		first_item->prev = n;
		first_item = n;
	}
	item_count++;
	return (1);
}

List::append_item(ListItem *n)
{
	n->next = 0;
	n->prev = last_item;
	if (item_count == 0)
		first_item = last_item = n;
	else {
		last_item->next = n;
		last_item = n;
	}
	item_count++;
	return (1);
}

List::insert_item(void *&iter_pos, ListItem *n)
{
	if (iter_pos == 0)
		prepend_item(n);
	else if (iter_pos == last_item)
		append_item(n);
	else {
		n->next = ((ListItem *)iter_pos)->next;
		((ListItem *)iter_pos)->next->prev = n;
		n->prev = (ListItem *)iter_pos;
		((ListItem *)iter_pos)->next = n;
		item_count++;
	}
	iter_pos = n;
	return (1);
}

List::delete_item(void *&iter_pos)
{
	ListItem *i;

	if (item_count == 0)
		return (0);
	if (iter_pos == 0)
		return (0);
	if (iter_pos == first_item) {
		i = first_item;
		first_item = i->next;
		if (first_item)
			first_item->prev = 0;
		else
			last_item = 0;
		iter_pos = 0;
	} else if (iter_pos == last_item) {
		i = last_item;
		last_item = i->prev;
		if (last_item)
			last_item->next = 0;
		else
			first_item = 0;
		iter_pos = last_item;
	} else {
		i = (ListItem *)iter_pos;
		i->prev->next = i->next;
		i->next->prev = i->prev;
		iter_pos = i->prev;
	}
	delete i;
	item_count--;
	return (1);
}

List::delete_all()
{
	__delete_all();
	item_count = 0;
	first_item = last_item = 0;
	return (1);
}
