/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_nameset.cc	1.5	94/08/04 SMI"

#include <xfn/FN_nameset.hh>

#include "NameSet.hh"


class FN_nameset_rep {
public:
	Set names;

	FN_nameset_rep();
	FN_nameset_rep(const FN_nameset_rep &);
	~FN_nameset_rep();

	FN_nameset_rep &operator=(const FN_nameset_rep &);
};

FN_nameset_rep::FN_nameset_rep()
{
}

FN_nameset_rep::FN_nameset_rep(const FN_nameset_rep &r)
	: names(r.names)
{
}

FN_nameset_rep::~FN_nameset_rep()
{
}

FN_nameset_rep &
FN_nameset_rep::operator=(const FN_nameset_rep &r)
{
	if (&r != this) {
		names = r.names;
	}
	return (*this);
}


FN_nameset::FN_nameset(FN_nameset_rep *r)
	: rep(r)
{
}

FN_nameset_rep *
FN_nameset::get_rep(const FN_nameset &s)
{
	return (s.rep);
}

FN_nameset::FN_nameset()
{
	rep = new FN_nameset_rep();
}

FN_nameset::~FN_nameset()
{
	delete rep;
}

// copy and assignment
FN_nameset::FN_nameset(const FN_nameset &s)
{
	rep = new FN_nameset_rep(*get_rep(s));
}

FN_nameset &
FN_nameset::operator=(const FN_nameset &s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

// get count of names
unsigned
FN_nameset::count() const
{
	return (rep->names.count());
}

// get first name (points iter_pos after name)
const FN_string *
FN_nameset::first(void *&iter_pos) const
{
	const NameSetItem *i;

	if (i = (const NameSetItem *)(rep->names.first(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// get name following iter_pos (points iter_pos after name)
const FN_string *
FN_nameset::next(void *&iter_pos) const
{
	const NameSetItem *i;

	if (i = (const NameSetItem *)(rep->names.next(iter_pos)))
		return (&(i->name));
	else
		return (0);
}

// add name to set (replace name if already in set && exclusive)
// add name to set (fails if name already in set && exclusive == 0)
int
FN_nameset::add(const FN_string &name,
    unsigned int exclusive)
{
	NameSetItem *n;

	if ((n = new NameSetItem(name)) == 0)
		return (0);
	return (rep->names.add(n, exclusive));
}

// remove name from set
int
FN_nameset::remove(const FN_string &name)
{
	return (rep->names.remove((const void *)&name));
}
