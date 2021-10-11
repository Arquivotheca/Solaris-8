/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_attrmodlist.cc	1.4	96/03/31 SMI"

#include "FN_attrmodlist.hh"

#include "AttrModList.hh"


class FN_attrmodlist_rep {
public:
	List attrs;

	FN_attrmodlist_rep();
	FN_attrmodlist_rep(const FN_attrmodlist_rep&);
	~FN_attrmodlist_rep();

	FN_attrmodlist_rep& operator=(const FN_attrmodlist_rep&);
};

FN_attrmodlist_rep::FN_attrmodlist_rep()
{
}

FN_attrmodlist_rep::FN_attrmodlist_rep(const FN_attrmodlist_rep& r)
: attrs(r.attrs)
{
}

FN_attrmodlist_rep::~FN_attrmodlist_rep()
{
}

FN_attrmodlist_rep &
FN_attrmodlist_rep::operator=(const FN_attrmodlist_rep& r)
{
	if (&r != this) {
		attrs = r.attrs;
	}
	return (*this);
}


FN_attrmodlist::FN_attrmodlist(FN_attrmodlist_rep* r)
: rep(r)
{
}

FN_attrmodlist_rep *
FN_attrmodlist::get_rep(const FN_attrmodlist& s)
{
	return (s.rep);
}

FN_attrmodlist::FN_attrmodlist()
{
	rep = new FN_attrmodlist_rep();
}

FN_attrmodlist::~FN_attrmodlist()
{
	delete rep;
}

FN_attrmodlist *
FN_attrmodlist::from_attrset(unsigned int mod_op, const FN_attrset &attrs)
{
	FN_attrmodlist *modlist = new FN_attrmodlist();
	if (modlist == 0)
		return (0);

	const FN_attribute *attr;
	void *ip;
	for (attr = attrs.first(ip); attr != NULL; attr = attrs.next(ip)) {
		if (modlist->add(mod_op, *attr) == 0) {
			delete modlist;
			return (0);
		}
	}
	return (modlist);
}

// copy and assignment
FN_attrmodlist::FN_attrmodlist(const FN_attrmodlist& s)
{
	rep = new FN_attrmodlist_rep(*get_rep(s));
}

FN_attrmodlist &
FN_attrmodlist::operator=(const FN_attrmodlist& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

// get count of attr-values
unsigned FN_attrmodlist::count() const
{
	return (rep->attrs.count());
}

// get first attr (points iter_pos after attr)
const FN_attribute *FN_attrmodlist::first(void *&iter_pos,
    unsigned int &first_mod_op) const
{
	const AttrModListItem *i;

	if (i = (const AttrModListItem*)rep->attrs.first(iter_pos)) {
		first_mod_op = i->attr_mod_op;
		return (&i->attribute);
	} else
	    return (0);
}

// get attr after iter_pos (points iter_pos after attr)
const FN_attribute *FN_attrmodlist::next(void *&iter_pos,
    unsigned int &mod_op) const
{
	const AttrModListItem *i;

	if (i = (const AttrModListItem*)rep->attrs.next(iter_pos)) {
		mod_op = i->attr_mod_op;
		return (&i->attribute);
	} else
	    return (0);
}

// add attr to set (fails if attr already in set and exclusive nonzero)
int FN_attrmodlist::add(unsigned int mod_op, const FN_attribute &attr)
{
	AttrModListItem *n;

	if ((n = new AttrModListItem(attr, mod_op)) == 0)
	    return (0);
	return (rep->attrs.append_item(n));
}
