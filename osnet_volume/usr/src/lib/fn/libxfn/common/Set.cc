/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Set.cc	1.5	98/11/10 SMI"

#include <string.h>	/* mem*() */

#include "Set.hh"


#define	MOD	19


SetItem::~SetItem()
{
}

Set::Set(int case_sense)
{
	item_count = 0;
	// first_item = 0;
	if ((item_tab = new SetItem *[MOD])) {
		memset(item_tab, 0, sizeof (item_tab[0]) * MOD);
		item_tab_mod = MOD;
	}
	p_case_sense = case_sense;
}

void
Set::__delete_all()
{
	SetItem *p, *np;
	int	i;

	if (item_tab) {
		for (i = 0; i < item_tab_mod; ++i) {
			for (p = item_tab[i]; p; p = np) {
				np = p->next;
				delete p;
			}
		}
		delete[] item_tab;
	}
}

Set::~Set()
{
	__delete_all();
}

/*
 * head is set to the prev node or start of hash chain if not found.
 */

SetItem *
Set::__locate(SetItem *n, SetItem **&head, unsigned long &bucket)
{
	SetItem		*i, *p;

	bucket = n->key_hash() % item_tab_mod;
	head = &item_tab[bucket];

	for (p = 0, i = *head; i; i = i->next) {
		if (i->item_match(*n, p_case_sense)) {
			if (p)
				head = &p->next;
			return (i);
		}
		p = i;
	}
	return (0);
}

/*
 * this won't return the correct head if the set is empty
 * don't use this for selecting a bucket for insert.
 */

SetItem *
Set::__locate(const void *key, SetItem **&head)
{
	SetItem		*n, *i, *p;
	int	j;

	for (j = 0; j < item_tab_mod; ++j) {
		if ((n = item_tab[j]))
			break;
	}
	if (j == item_tab_mod) {
		head = 0;
		return (0);
	}

	head = &item_tab[n->key_hash(key) % item_tab_mod];

	for (p = 0, i = *head; i; i = i->next) {
		if (i->key_match(key, p_case_sense)) {
			if (p)
				head = &p->next;
			return (i);
		}
		p = i;
	}
	return (0);
}

void
Set::__copy(const Set& s)
{
	SetItem *p, *pn, *n;

	item_count = s.item_count;
	int	b;

	if ((item_tab = new SetItem *[MOD])) {
		memset(item_tab, 0, sizeof (item_tab[0]) * MOD);
		item_tab_mod = MOD;
	}

	if (item_tab == 0 || s.item_tab == 0)
		return;

	for (b = 0; b < s.item_tab_mod; ++b) {
		if (s.item_tab[b]) {
			for (p = s.item_tab[b], pn = n = 0; p;
			    p = p->next, pn = n) {
				n = p->copy();
				if (pn)
					pn->next = n;
				else
					item_tab[b] = n;
			}
			n->next = 0;
		} else
			item_tab[b] = 0;
	}
}

Set::Set(const Set& s)
{
	__copy(s);
}

Set &
Set::operator=(const Set& s)
{
	if (&s != this) {
		__delete_all();
		__copy(s);
	}
	return (*this);
}

unsigned
Set::count() const
{
	return (item_count);
}

const SetItem *
Set::first(void *&iter_pos)
{
	SetItem	*n;

	if (item_tab) {
		n = 0;
		for (int j = 0; j < item_tab_mod; ++j) {
			if ((n = item_tab[j]))
				break;
		}
		iter_pos = n;
		return ((SetItem *)n);
	}
	return (0);
}

const SetItem *
Set::next(void *&iter_pos)
{
	SetItem *p;

	if (iter_pos == 0)
		return (0);

	SetItem		**h;
	unsigned long	bucket;

	/*
	 * Walk all nodes on all lists.
	 */

	p = (SetItem *)iter_pos;
	if (p->next) {
		iter_pos = p->next;
		return (p->next);
	}
	/*
	 * When we hit the last node on a list, jump to head of next list.
	 * We need to figure out which list the current node is on so that
	 * we can select the next list.
	 */
	__locate(p, h, bucket);
	while (++bucket < item_tab_mod) {
		if (item_tab[bucket]) {
			iter_pos = item_tab[bucket];
			return (item_tab[bucket]);
		}
	}
	return (0);
}

SetItem *
Set::get(const void *key)
{
	SetItem	**h;

	return (__locate(key, h));
}

int
Set::add(SetItem* n, unsigned int exclusive)
{
	SetItem *i, **h;

	if (exclusive == FN_OP_EXCLUSIVE) {
		unsigned long	bucket;

		if (__locate(n, h, bucket)) {
			delete n;
			return (0);
		}
		n->next = *h;
		*h = n;
	} else {
		// add replace (supercede)
		unsigned long	bucket;

		if ((i = __locate(n, h, bucket))) {
			n->next = i->next;
			*h = n;
			delete i;
			return (1);
		}
		n->next = *h;
		*h = n;
	}
	item_count++;
	return (1);
}

int
Set::remove(const void *key)
{
	SetItem *i;
	SetItem	**h;

	if ((i = __locate(key, h))) {
		*h = i->next;
		--item_count;
		delete i;
		return (1);
	}
	return (0);
}
