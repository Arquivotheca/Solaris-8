/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_REF_HH
#define	_XFN_FN_REF_HH

#pragma ident	"@(#)FN_ref.hh	1.3	96/03/31 SMI"

#include <xfn/FN_ref.h>
#include <xfn/FN_string.hh>
#include <xfn/FN_identifier.hh>
#include <xfn/FN_composite_name.hh>
#include <xfn/FN_ref_addr.hh>

class FN_ref_rep;

class FN_ref {
    public:
	FN_ref(const FN_identifier& type);
	~FN_ref();

	// copy and assignment
	FN_ref(const FN_ref&);
	FN_ref& operator=(const FN_ref&);

	const FN_identifier* type(void) const;

	// get count of addresses
	unsigned int addrcount(void) const;

	// get first address (points iter_pos after address)
	const FN_ref_addr* first(void*& iter_pos) const;

	// get address following iter_pos (points iter_pos after address)
	const FN_ref_addr* next(void*& iter_pos) const;

	// get description of reference
	FN_string *description(unsigned int detail = 0,
				unsigned int *more_detail = 0) const;

	// prepend address to list
	int prepend_addr(const FN_ref_addr&);
	// append address to list
	int append_addr(const FN_ref_addr&);

	// insert address before iter_pos
	int insert_addr(void*& iter_pos, const FN_ref_addr&);
	// delete address before iter_pos
	int delete_addr(void*& iter_pos);
	// delete all addresses
	int delete_all(void);

	// link-related operations
	// create link
	static FN_ref* create_link(const FN_composite_name& link_name);

	// is this a link?
	int is_link(void) const;

	// get link name
	FN_composite_name* link_name(void) const;

    protected:
	FN_ref_rep* rep;
	static FN_ref_rep* get_rep(const FN_ref&);
	FN_ref(FN_ref_rep*);

    private:
	FN_ref();
};

#endif /* _XFN_FN_REF_HH */
