/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_search_control.cc	1.1	96/03/31 SMI"

#include <xfn/FN_search_control.hh>
#include <xfn/FN_status.h>

class FN_search_control_rep {
public:
	unsigned int scope;
	unsigned int follow_links;
	unsigned int max_names;
	unsigned int return_ref;
	FN_attrset *return_attr_ids;

	FN_search_control_rep();
	FN_search_control_rep(unsigned int scope,
			    unsigned int follow_links,
			    unsigned int max_names,
			    unsigned int return_ref,
			    const FN_attrset *return_attr_ids,
			    unsigned int &status);
	~FN_search_control_rep();

	FN_search_control_rep& operator=(const FN_search_control_rep&);
};

FN_search_control_rep::FN_search_control_rep()
{
	/* default control settings */

	scope = FN_SEARCH_ONE_CONTEXT;	/* search named context */
	follow_links = 0;		/* do not follow links */
	max_names = 0;			/* no limit to number returned */
	return_ref = 0;			/* do not return reference */
	return_attr_ids = new FN_attrset();	/* do not return attrs */
}

FN_search_control_rep::FN_search_control_rep(unsigned int iscope,
    unsigned int ifollow_links,
    unsigned int imax_names,
    unsigned int ireturn_ref,
    const FN_attrset *ireturn_attr_ids,
    unsigned int &status)
{
	switch (iscope) {
	case FN_SEARCH_NAMED_OBJECT:
	case FN_SEARCH_ONE_CONTEXT:
	case FN_SEARCH_SUBTREE:
	case FN_SEARCH_CONSTRAINED_SUBTREE:
		status = FN_SUCCESS;
		scope = iscope;
		break;
	default:
		status = FN_E_SEARCH_INVALID_OPTION;
		return;
	}
	follow_links = ifollow_links;
	max_names = imax_names;
	return_ref = ireturn_ref;
	if (ireturn_attr_ids == 0)
		return_attr_ids = 0;
	else {
		return_attr_ids = new FN_attrset(*ireturn_attr_ids);
		if (return_attr_ids == 0)
			status = FN_E_SEARCH_INVALID_OPTION;
	}
}

FN_search_control_rep::~FN_search_control_rep()
{
	delete return_attr_ids;
}

FN_search_control_rep &
FN_search_control_rep::operator=(const FN_search_control_rep& r)
{
	if (&r != this) {
		scope = r.scope;
		follow_links = r.follow_links;
		max_names = r.max_names;
		return_ref = r.return_ref;
		if (r.return_attr_ids == 0)
			return_attr_ids = 0;
		else {
			return_attr_ids = new FN_attrset(*r.return_attr_ids);
		}
	}
	return (*this);
}


FN_search_control::FN_search_control(FN_search_control_rep* r)
	: rep(r)
{
}

FN_search_control_rep *
FN_search_control::get_rep(const FN_search_control& s)
{
	return (s.rep);
}


FN_search_control::FN_search_control()
{
	rep = new FN_search_control_rep();
}

FN_search_control::~FN_search_control()
{
	delete rep;
}


FN_search_control::FN_search_control(unsigned int iscope,
    unsigned int ifollow_links,
    unsigned int imax_names,
    unsigned int ireturn_ref,
    const FN_attrset *ireturn_attr_ids,
    unsigned int &status)
{
	rep = new FN_search_control_rep(iscope, ifollow_links, imax_names,
	    ireturn_ref, ireturn_attr_ids, status);
}

// copy and assignment
FN_search_control::FN_search_control(const FN_search_control& s)
{
	rep = new FN_search_control_rep(*get_rep(s));
}

FN_search_control &
FN_search_control::operator=(const FN_search_control& s)
{
	if (&s != this) {
		*rep = *get_rep(s);
	}
	return (*this);
}

unsigned int
FN_search_control::scope(void) const
{
	return (rep->scope);
}

unsigned int
FN_search_control::follow_links(void) const
{
	return (rep->follow_links);
}

unsigned int
FN_search_control::max_names(void) const
{
	return (rep->max_names);
}

unsigned int
FN_search_control::return_ref(void) const
{
	return (rep->return_ref);
}

const FN_attrset *
FN_search_control::return_attr_ids(void) const
{
	return (rep->return_attr_ids);
}
