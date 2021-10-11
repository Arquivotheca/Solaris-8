/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ref.cc	1.7	97/10/16 SMI"

#include <stdlib.h>
#include <xfn/FN_string.hh>
#include <xfn/FN_ref.hh>
#include <xfn/FN_identifier.hh>
#include <xfn/FN_status.hh>
#include <xfn/FN_ref_addr.hh>

#include "AddressList.hh"

#include <string.h>	/* for str*() */
#include <stdio.h>	/* for sprintf */

// Global declarations for operators "new" and "delete"
// This is to remove dependency on libC
void * operator new(size_t size)
{
        return (malloc(size));
}

void operator delete(void *ptr)
{
        if (ptr)
                free(ptr);
}

class FN_ref_rep {
public:
	FN_identifier type;
	List addrs;

	FN_ref_rep(const FN_identifier &type);
	FN_ref_rep(const FN_ref_rep &);
	~FN_ref_rep();

	FN_ref_rep &operator=(const FN_ref_rep &);
};

FN_ref_rep::FN_ref_rep(const FN_identifier &t)
	: type(t)
{
}

FN_ref_rep::FN_ref_rep(const FN_ref_rep &r)
	: type(r.type), addrs(r.addrs)
{
}

FN_ref_rep::~FN_ref_rep()
{
}

FN_ref_rep &
FN_ref_rep::operator=(const FN_ref_rep &r)
{
	if (&r != this) {
		type = r.type;
		addrs = r.addrs;
	}
	return (*this);
}


FN_ref::FN_ref(FN_ref_rep *r)
	: rep(r)
{
}

FN_ref_rep *
FN_ref::get_rep(const FN_ref &r)
{
	return (r.rep);
}

// default constructor disallowed
FN_ref::FN_ref()
{
}

// constructor
FN_ref::FN_ref(const FN_identifier &t)
{
	rep = new FN_ref_rep(t);
}

FN_ref::~FN_ref()
{
	delete rep;
}

// copy and assignment
FN_ref::FN_ref(const FN_ref &r)
{
	rep = new FN_ref_rep(*get_rep(r));
}

FN_ref &
FN_ref::operator=(const FN_ref &r)
{
	if (&r != this) {
		*rep = *get_rep(r);
	}
	return (*this);
}

const FN_identifier *
FN_ref::type() const
{
	return (&(rep->type));
}

// get count of addresses
unsigned int
FN_ref::addrcount() const
{
	return (rep->addrs.count());
}

// get first address (points iter_pos after address)
const FN_ref_addr *
FN_ref::first(void *&iter_pos) const
{
	const AddressListItem *i;
	if (i = (const AddressListItem *)(rep->addrs.first(iter_pos)))
		return (&(i->addr));
	else
		return (0);
}

// get address following iter_pos (points iter_pos after address)
const FN_ref_addr *
FN_ref::next(void *&iter_pos) const
{
	const AddressListItem *i;
	if (i = (const AddressListItem *)(rep->addrs.next(iter_pos)))
		return (&(i->addr));
	else
		return (0);
}

/*
 * %%% should probably enhance description to handle FN_identifier
 */

// get description of reference
FN_string *
FN_ref::description(unsigned int d, unsigned int *md) const
{
	// d>=0 --> reference type + address descriptions(d)

	const FN_identifier *tp;
	const unsigned char *ref_type_str = 0;

	tp = type();

	size_t size = 26; // overhead
	if (tp) {
		size += tp->length(); // type string
		ref_type_str = tp->str();
	}
	unsigned char *buf = new unsigned char[size];
	sprintf((char *)buf, "Reference type: %s\n",
		ref_type_str ? (char *)ref_type_str : "");
	FN_string *ret = new FN_string(buf);
	delete[] buf;

	void *ip;
	const FN_ref_addr *a;
	unsigned amd;
	FN_string *newret;

	if (md)
		*md = d;

	for (a = first(ip); a; a = next(ip)) {
	    FN_string *adesc = a->description(d, &amd);
	    if (adesc) {
		    newret = new FN_string(0, ret, adesc, (FN_string *)0);
		    delete adesc;
		    if (newret == 0)
			    break;
		    delete ret;
		    ret = newret;
		    if (md && (*md == d || (amd > d && amd < *md)))
			    *md = amd;
	    }
	}

	return (ret);
}

// prepend address to list
int
FN_ref::prepend_addr(const FN_ref_addr &a)
{
	AddressListItem *n;
	if ((n = new AddressListItem(a)) == 0)
		return (0);
	return (rep->addrs.prepend_item(n));
}

// append address to list
int
FN_ref::append_addr(const FN_ref_addr &a)
{
	AddressListItem *n;
	if ((n = new AddressListItem(a)) == 0)
		return (0);
	return (rep->addrs.append_item(n));
}

// insert address before iter_pos
int
FN_ref::insert_addr(void *&iter_pos, const FN_ref_addr &a)
{
	AddressListItem *n;
	if ((n = new AddressListItem(a)) == 0)
		return (0);
	return (rep->addrs.insert_item(iter_pos, n));
}

// delete address before iter_pos
int
FN_ref::delete_addr(void *&iter_pos)
{
	return (rep->addrs.delete_item(iter_pos));
}

// delete all addresses
int
FN_ref::delete_all()
{
	return (rep->addrs.delete_all());
}


/*
 * a link reference is a FN_ref with a special reference type of FN_link_ref_id.
 * such a reference will contain exactly one addr of type FN_link_addr_id
 * what we encode in the addr is the 'str' (unsigned char*) version of
 * the composite name.
 */

static FN_identifier FN_link_ref_id((unsigned char *)"fn_link_ref");
static FN_identifier FN_link_addr_id((unsigned char *)"fn_link_addr");

FN_ref *
FN_ref::create_link(const FN_composite_name &link_name)
{
	FN_string		*link_str;
	const unsigned char	*link_cstr;
	FN_ref			*lk;

	link_str = link_name.string();
	if (link_str == 0)
		return (0);
	link_cstr = link_str->str();
	if (link_cstr == 0) {
		delete link_str;
		return (0);
	}
	FN_ref_addr addr(FN_link_addr_id,
	    strlen((const char *)link_cstr), link_cstr);
	delete link_str;

	if ((lk = new FN_ref(FN_link_ref_id)) == 0)
		return (0);
	if (lk->append_addr(addr) == 0) {
		delete lk;
		return (0);
	}
	return (lk);
}

static const void *
FN_ref_get_link(const FN_ref &ref, size_t *length = 0)
{
	const FN_ref_addr	*a;
	const FN_identifier	*id;
	void			*iter_pos;

	id = ref.type();
	if (id == 0)
		return (0);
	if (FN_link_ref_id != *id)
		return (0);

	for (a = ref.first(iter_pos); a; a = ref.next(iter_pos)) {
		id = a->type();
		if (id == 0)
			return (0);
		if (FN_link_addr_id != *id)
			continue;
		if (length)
			*length = a->length();
		return (a->data());
	}
	return (0);
}

int
FN_ref::is_link(void) const
{
	const void *link_contents = FN_ref_get_link(*this);
	return (link_contents != 0);
}


FN_composite_name *
FN_ref::link_name(void) const
{
	size_t link_length;
	const void *link_contents = FN_ref_get_link(*this, &link_length);
	FN_composite_name	*name = 0;

	if (link_contents) {
		name = new FN_composite_name(FN_string(
		    (const unsigned char *)link_contents, link_length));
	}
	return (name);
}
