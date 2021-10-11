/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_attrset.cc	1.4 94/08/02 SMI"

#include "FN_attrset.hh"

#include "AttrSet.hh"


class FN_attrset_rep {
public:
	Set attrs;

	FN_attrset_rep();
	FN_attrset_rep(const FN_attrset_rep&);
	~FN_attrset_rep();

	FN_attrset_rep& operator=(const FN_attrset_rep&);
};

FN_attrset_rep::FN_attrset_rep()
{
}

FN_attrset_rep::FN_attrset_rep(const FN_attrset_rep& r)
	: attrs(r.attrs)
{
}

FN_attrset_rep::~FN_attrset_rep()
{
}

FN_attrset_rep &
FN_attrset_rep::operator=(const FN_attrset_rep& r)
{
	if (&r != this) {
		attrs = r.attrs;
	}
	return (*this);
}


FN_attrset::FN_attrset(FN_attrset_rep* r)
	: rep(r)
{
}

FN_attrset_rep *
FN_attrset::get_rep(const FN_attrset& s)
{
	return (s.rep);
}

FN_attrset::FN_attrset()
{
	rep = new FN_attrset_rep();
}

FN_attrset::~FN_attrset()
{
	delete rep;
}

// copy and assignment
FN_attrset::FN_attrset(const FN_attrset& s)
{
	rep = new FN_attrset_rep(*get_rep(s));
}

FN_attrset &
FN_attrset::operator=(const FN_attrset& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

// get value for specified attr
const FN_attribute *FN_attrset::get(const FN_identifier &attr) const
{
	const AttrSetItem *i;

	if (i = (const AttrSetItem *)rep->attrs.get((const void *)&attr))
		return (&i->at);
	return (0);
}

// get count of attr-values
unsigned FN_attrset::count() const
{
	return (rep->attrs.count());
}

// get first attr (points iter_pos after attr)
const FN_attribute *FN_attrset::first(void *&iter_pos) const
{
	const AttrSetItem *i;

	if (i = (const AttrSetItem*)rep->attrs.first(iter_pos)) {
		return (&i->at);
	} else
		return (0);
}

// get attr after iter_pos (points iter_pos after attr)
const FN_attribute *FN_attrset::next(void *&iter_pos) const
{
	const AttrSetItem *i;

	if (i = (const AttrSetItem*)rep->attrs.next(iter_pos)) {
		return (&i->at);
	} else
		return (0);
}

// add attr to set (fails if attr already in set and exclusive nonzero)
int
FN_attrset::add(const FN_attribute &attr, unsigned int exclusive)
{
	AttrSetItem *n;

	if ((n = new AttrSetItem(attr)) == 0)
		return (0);
	return (rep->attrs.add(n, exclusive));
}

// remove attr from set
int
FN_attrset::remove(const FN_identifier &id)
{
	return (rep->attrs.remove((const void *)&id));
}
