/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ext_searchset.cc	1.1	96/03/31 SMI"

#include "FN_ext_searchset.hh"
#include "ExtSearchSet.hh"

class FN_ext_searchset_rep {
public:
	Set search_hits;
	const int got_refs;
	const int got_attrs;

	FN_ext_searchset_rep(int r, int a);
	FN_ext_searchset_rep(const FN_ext_searchset_rep&);
	~FN_ext_searchset_rep();

	FN_ext_searchset_rep& operator=(const FN_ext_searchset_rep&);
};

FN_ext_searchset_rep::FN_ext_searchset_rep(int r, int a)
: got_refs(r), got_attrs(a)
{
}

FN_ext_searchset_rep::FN_ext_searchset_rep(const FN_ext_searchset_rep& r)
: search_hits(r.search_hits), got_refs(r.got_refs), got_attrs(r.got_attrs)
{
}

FN_ext_searchset_rep::~FN_ext_searchset_rep()
{
}

FN_ext_searchset_rep& FN_ext_searchset_rep::operator=(
    const FN_ext_searchset_rep& r)
{
	if (&r != this) {
		search_hits = r.search_hits;
	}
	return (*this);
}


FN_ext_searchset::FN_ext_searchset(FN_ext_searchset_rep* r)
	: rep(r)
{
}

FN_ext_searchset_rep *
FN_ext_searchset::get_rep(const FN_ext_searchset& s)
{
	return (s.rep);
}

FN_ext_searchset::FN_ext_searchset(int r, int a)
{
	rep = new FN_ext_searchset_rep(r, a);
}

FN_ext_searchset::~FN_ext_searchset()
{
	delete rep;
}

// copy and assignment
FN_ext_searchset::FN_ext_searchset(const FN_ext_searchset& s)
{
	rep = new FN_ext_searchset_rep(*get_rep(s));
}

FN_ext_searchset &
FN_ext_searchset::operator=(const FN_ext_searchset &s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

int
FN_ext_searchset::refs_filled()
{
	return (rep->got_refs);
}

int
FN_ext_searchset::attrs_filled()
{
	return (rep->got_attrs);
}
// get count of searchs
unsigned FN_ext_searchset::count() const
{
	return (rep->search_hits.count());
}

// get first search (points iter_pos after search)
const FN_composite_name *
FN_ext_searchset::first(void *&iter_pos, const FN_ref **ref,
	const FN_attrset **attrs, unsigned int *rel, void **curr_pos) const
{
	ExtSearchSetItem *i;

	if (i = (ExtSearchSetItem*)(rep->search_hits.first(iter_pos))) {
		if (ref)
			*ref = i->ref;
		if (attrs)
			*attrs = i->attrs;
		if (rel)
			*rel = i->relative;
		if (curr_pos)
			*curr_pos = (void *)i;

		return (&(i->name));
	} else
		return (0);
}

// get search after iter_pos (points iter_pos after search)
const FN_composite_name *
FN_ext_searchset::next(void *&iter_pos, const FN_ref **ref,
	const FN_attrset **attrs, unsigned int *rel, void **curr_pos) const
{
	ExtSearchSetItem *i;

	if (i = (ExtSearchSetItem*)(rep->search_hits.next(iter_pos))) {
		if (ref)
			*ref = i->ref;
		if (attrs)
			*attrs = i->attrs;
		if (rel)
			*rel = i->relative;
		if (curr_pos)
			*curr_pos = (void *)i;

		return (&(i->name));
	} else
		return (0);
}

// check whether given name is present in set
int FN_ext_searchset::present(const FN_composite_name& name) const
{
	const ExtSearchSetItem *i;

	if (i = (const ExtSearchSetItem *)(rep->search_hits.get(
	    (const void *)&name)))
		return (1);
	return (0);
}


// add search to set (fails if name already in set && exclusive nonzero)
int FN_ext_searchset::add(
	const FN_composite_name& name,
	const FN_ref *ref,
	const FN_attrset *attrs,
	unsigned int relative,
	unsigned int exclusive)
{
	ExtSearchSetItem *n;

	if ((n = new ExtSearchSetItem(name, ref, attrs, relative)) == 0)
		return (0);
	return (rep->search_hits.add(n, exclusive));
}

// remove search from set
int
FN_ext_searchset::remove(const FN_composite_name &name)
{
	return (rep->search_hits.remove((const void *)&name));
}


// set contents of search entry; note no copying involved


int FN_ext_searchset::set_ref(void *posn, FN_ref *ref)
{
	ExtSearchSetItem *i = (ExtSearchSetItem *)posn;

	if (i == 0)
		return (0);

	if (i->ref)
		delete i->ref;
	i->ref = ref;
	return (1);
}

int FN_ext_searchset::set_attrs(void *posn, FN_attrset *attrs)
{
	ExtSearchSetItem *i = (ExtSearchSetItem *)posn;

	if (i == 0)
		return (0);

	if (i->attrs)
		delete i->attrs;
	i->attrs = attrs;
	return (1);
}
