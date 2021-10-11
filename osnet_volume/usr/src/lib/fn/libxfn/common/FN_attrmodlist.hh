/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_ATTRMODLIST_HH
#define	_XFN_FN_ATTRMODLIST_HH

#pragma ident	"@(#)FN_attrmodlist.hh	1.3	96/03/31 SMI"

#include <xfn/FN_attrmodlist.h>

#include <xfn/FN_string.hh>
#include <xfn/FN_identifier.hh>
#include <xfn/FN_attribute.hh>
#include <xfn/FN_attrset.hh>

#include <xfn/misc_codes.h>

class FN_attrmodlist_rep;

class FN_attrmodlist {
    public:
	FN_attrmodlist();
	~FN_attrmodlist();

	// copy and assignment
	FN_attrmodlist(const FN_attrmodlist&);
	FN_attrmodlist& operator=(const FN_attrmodlist&);
	static FN_attrmodlist *from_attrset(unsigned int modop,
	    const FN_attrset &attrs);

	// get count of attrs
	unsigned int count(void) const;

	// to obtain the  first attribute
	const FN_attribute *first(void *& inter_pos,
	    unsigned int &first_mod_op) const;

	// to obtain the successive attributes
	const FN_attribute *next(void *& inter_pos,
	    unsigned int &mod_op) const;

	// add attr to set
	int add(unsigned int mod_op, const FN_attribute &attr);

    protected:
	FN_attrmodlist_rep *rep;
	static FN_attrmodlist_rep *get_rep(const FN_attrmodlist&);
	FN_attrmodlist(FN_attrmodlist_rep *);
};

#endif /* _XFN_FN_ATTRMODLIST_HH */
