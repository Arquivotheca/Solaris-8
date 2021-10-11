/*
 * Copyright (c) 1993 - 1995 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_SEARCH_CONTROL_HH
#define	_XFN_FN_SEARCH_CONTROL_HH

#pragma ident	"@(#)FN_search_control.hh	1.1	96/03/31 SMI"

#include <xfn/FN_attrset.hh>
#include <xfn/FN_search_control.h>

class FN_search_control_rep;

class FN_search_control {
    public:
	/*
	 * Constructors and destructors.
	 */
	FN_search_control();

	FN_search_control(unsigned int scope,
	    unsigned int follow_links,
	    unsigned int max_names,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    unsigned int &status);
	~FN_search_control();

	// copy and assignment
	FN_search_control(const FN_search_control&);
	FN_search_control& operator=(const FN_search_control&);

	unsigned int scope(void) const;
	unsigned int follow_links(void) const;
	unsigned int max_names(void) const;
	unsigned int return_ref(void) const;
	const FN_attrset *return_attr_ids(void) const;

    protected:
	FN_search_control_rep* rep;
	static FN_search_control_rep* get_rep(const FN_search_control&);
	FN_search_control(FN_search_control_rep*);
};

#endif /* _XFN_FN_SEARCH_CONTROL_HH */
