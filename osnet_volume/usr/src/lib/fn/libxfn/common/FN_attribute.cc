/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_attribute.cc	1.3 94/08/02 SMI"

#include <xfn/FN_attribute.hh>

#include "AttrValSet.hh"

class FN_attribute_rep {
public:
	FN_identifier attr_id;
	FN_identifier attr_syntax;
	Set attr_vals;

	FN_attribute_rep();
	FN_attribute_rep(const FN_attribute_rep&);
	FN_attribute_rep(const FN_identifier&, const FN_identifier&);
	~FN_attribute_rep();

	FN_attribute_rep& operator=(const FN_attribute_rep&);
};

FN_attribute_rep::FN_attribute_rep()
{
}

FN_attribute_rep::FN_attribute_rep(const FN_attribute_rep& r)
: attr_id(r.attr_id), attr_syntax(r.attr_syntax), attr_vals(r.attr_vals)
{
}


FN_attribute_rep::FN_attribute_rep(const FN_identifier &id,
    const FN_identifier &syntax)
	: attr_id(id), attr_syntax(syntax), attr_vals()
{
}

FN_attribute_rep::~FN_attribute_rep()
{
}

FN_attribute_rep &
FN_attribute_rep::operator=(const FN_attribute_rep& r)
{
	if (&r != this) {
	    attr_id = r.attr_id;
	    attr_syntax = r.attr_syntax;
	    attr_vals = r.attr_vals;
	}
	return (*this);
}


FN_attribute::FN_attribute(FN_attribute_rep* r)
	: rep(r)
{
}

FN_attribute_rep *
FN_attribute::get_rep(const FN_attribute& s)
{
	return (s.rep);
}


FN_attribute::FN_attribute()
{
	rep = new FN_attribute_rep();
}

FN_attribute::~FN_attribute()
{
	delete rep;
}


FN_attribute::FN_attribute(const FN_identifier &id,
    const FN_identifier &syntax)
{
	rep = new FN_attribute_rep(id, syntax);
}

const FN_identifier *
FN_attribute::identifier(void) const
{
	return (&(rep->attr_id));
}

const FN_identifier *
FN_attribute::syntax(void) const
{
	return (&(rep->attr_syntax));
}


// copy and assignment
FN_attribute::FN_attribute(const FN_attribute& s)
{
	rep = new FN_attribute_rep(*get_rep(s));
}

FN_attribute &
FN_attribute::operator=(const FN_attribute& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

// get count of values
unsigned int FN_attribute::valuecount() const
{
	return (rep->attr_vals.count());
}

// get first value (points iter_pos after value)
const FN_attrvalue *
FN_attribute::first(void *&iter_pos) const
{
	const AttrValSetItem* i;

	if (i = (const AttrValSetItem*)(rep->attr_vals.first(iter_pos)))
		return (&(i->attr_val));
	else
		return (0);
}

// get value following iter_pos (points iter_pos after value)
const FN_attrvalue *
FN_attribute::next(void *&iter_pos) const
{
	const AttrValSetItem* i;

	if (i = (const AttrValSetItem*)(rep->attr_vals.next(iter_pos)))
		return (&(i->attr_val));
	else
		return (0);
}

// add value to set (replace value if already in set && exclusive)
int
FN_attribute::add(const FN_attrvalue &av, unsigned int exclusive)
{
	AttrValSetItem *n;

	if ((n = new AttrValSetItem(av)) == 0)
		return (0);
	return (rep->attr_vals.add(n, exclusive));
}

// remove value from set
int
FN_attribute::remove(const FN_attrvalue &av)
{
	return (rep->attr_vals.remove((const void *)&av));
}

int
FN_attribute::operator==(const FN_attribute &attr) const
{
	const FN_identifier *id1;
	const FN_identifier *id2;

	id1 = identifier();
	id2 = attr.identifier();

	if (*id1 != *id2)
	    return (0);

	id1 = syntax();
	id2 = attr.syntax();

	unsigned int count, i;
	if ((count = valuecount()) != attr.valuecount())
	    return (0);

	void *iter1;
	void *iter2;
	const FN_attrvalue *val1 = first(iter1);
	const FN_attrvalue *val2 = attr.first(iter2);

	for (i = 0; val1 && val2 && i < count; i++) {
		if (*val1 != *val2)
		    return (0);
		val1 = next(iter1);
		val2 = attr.next(iter2);
	}
	return (1);
}

int FN_attribute::operator!=(const FN_attribute& attr) const
{
	return (!(*this == attr));
}
