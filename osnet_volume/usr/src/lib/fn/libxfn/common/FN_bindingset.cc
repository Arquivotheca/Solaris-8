/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_bindingset.cc	1.5	96/03/31 SMI"

#include "FN_bindingset.hh"
#include "BindingSet.hh"

class FN_bindingset_rep {
public:
	Set bindings;

	FN_bindingset_rep();
	FN_bindingset_rep(const FN_bindingset_rep&);
	~FN_bindingset_rep();

	FN_bindingset_rep& operator=(const FN_bindingset_rep&);
};

FN_bindingset_rep::FN_bindingset_rep()
{
}

FN_bindingset_rep::FN_bindingset_rep(
					const FN_bindingset_rep& r)
	: bindings(r.bindings)
{
}

FN_bindingset_rep::~FN_bindingset_rep()
{
}

FN_bindingset_rep& FN_bindingset_rep::operator=(
					const FN_bindingset_rep& r)
{
	if (&r != this) {
		bindings = r.bindings;
	}
	return (*this);
}


FN_bindingset::FN_bindingset(FN_bindingset_rep* r)
	: rep(r)
{
}

FN_bindingset_rep *
FN_bindingset::get_rep(const FN_bindingset& s)
{
	return (s.rep);
}

FN_bindingset::FN_bindingset()
{
	rep = new FN_bindingset_rep();
}

FN_bindingset::~FN_bindingset()
{
	delete rep;
}

// copy and assignment
FN_bindingset::FN_bindingset(const FN_bindingset& s)
{
	rep = new FN_bindingset_rep(*get_rep(s));
}

FN_bindingset &
FN_bindingset::operator=(const FN_bindingset& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

// get reference for specified name
const FN_ref* FN_bindingset::get_ref(const FN_string& name) const
{
	const BindingSetItem *i;

	if (i = (const BindingSetItem *)(rep->bindings.get(
	    (const void *)&name)))
		return (&(i->ref));
	return (0);
}

// get count of bindings
unsigned FN_bindingset::count() const
{
	return (rep->bindings.count());
}

// get first binding (points iter_pos after binding)
const FN_string *
FN_bindingset::first(void *&iter_pos, const FN_ref *&ref) const
{
	const BindingSetItem *i;

	if (i = (const BindingSetItem*)(rep->bindings.first(iter_pos))) {
		ref = &(i->ref);
		return (&(i->name));
	} else
		return (0);
}

// get binding after iter_pos (points iter_pos after binding)
const FN_string *
FN_bindingset::next(void *&iter_pos, const FN_ref *&ref) const
{
	const BindingSetItem *i;

	if (i = (const BindingSetItem*)(rep->bindings.next(iter_pos))) {
		ref = &(i->ref);
		return (&(i->name));
	} else
		return (0);
}

// add binding to set (fails if name already in set && exclusive nonzero)
int FN_bindingset::add(
	const FN_string& name,
	const FN_ref& ref,
	unsigned int exclusive)
{
	BindingSetItem *n;

	if ((n = new BindingSetItem(name, ref)) == 0)
		return (0);
	return (rep->bindings.add(n, exclusive));
}

// remove binding from set
int
FN_bindingset::remove(const FN_string &name)
{
	return (rep->bindings.remove((const void *)&name));
}
