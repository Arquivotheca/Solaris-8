/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_COMPOSITE_NAME_HH
#define	_XFN_FN_COMPOSITE_NAME_HH

#pragma ident	"@(#)FN_composite_name.hh	1.3	96/03/31 SMI"

#include <xfn/FN_string.hh>
#include <xfn/FN_composite_name.h>

class FN_composite_name_rep;

class FN_composite_name {
    public:
	/*
	 * Constructors and destructors.
	 */
	FN_composite_name();
	~FN_composite_name();

	/*
	 * Routines for FN_string.
	 */
	FN_composite_name(const FN_string&);

	// copy and assignment
	FN_composite_name(const FN_composite_name&);
	FN_composite_name& operator=(const FN_composite_name&);

	// convert to/from string representation
	// this is called fn_string_from_composite_name in the xfn C spec
	FN_string *string(unsigned int *status = 0) const;

	// %%% not in specs
	FN_composite_name& operator=(const FN_string&);
	FN_composite_name(const unsigned char *);

	// syntactic comparison; %%% not in specs
	operator==(const FN_composite_name&) const;
	operator!=(const FN_composite_name&) const;
#if 0
	operator<(const FN_composite_name&) const;
	operator>(const FN_composite_name&) const;
	operator<=(const FN_composite_name&) const;
	operator>=(const FN_composite_name&) const;
#endif
	// test for empty name (single emtpy string component)
	int is_empty(void) const;

	unsigned int count(void) const;

	// name equality (string equality)
	int is_equal(const FN_composite_name &,
		    unsigned int *status = 0) const;

	// get first component (points iter_pos after name)
	const FN_string* first(void*& iter_pos) const;
	// test for prefix (points iter_pos after prefix)
	int is_prefix(const FN_composite_name&,
		    void*& iter_pos,
		    unsigned int *status = 0) const;
	// get last component (points iter_pos before name)
	const FN_string* last(void*& iter_pos) const;
	// test for suffix (points iter_pos before suffix)
	int is_suffix(const FN_composite_name&,
		    void*& iter_pos,
		    unsigned int *status = 0) const;

	// get component following iter_pos (points iter_pos after component)
	const FN_string* next(void*& iter_pos) const;
	// get component before iter_pos (points iter_pos before component)
	const FN_string* prev(void*& iter_pos) const;

	// get copy of name from first component through iter_pos
	FN_composite_name* prefix(const void *iter_pos) const;
	// get copy of name from iter_pos through last component
	FN_composite_name* suffix(const void *iter_pos) const;

	// prepend component/name to name
	int prepend_comp(const FN_string&);
	// append component/name to name
	int append_comp(const FN_string&);

	int prepend_name(const FN_composite_name&);
	int append_name(const FN_composite_name&);

	// insert component/name before iter_pos
	int insert_comp(void*& iter_pos, const FN_string&);
	int insert_name(void*& iter_pos, const FN_composite_name&);

	// delete component before iter_pos
	int delete_comp(void*& iter_pos);

    protected:
	FN_composite_name_rep* rep;
	static FN_composite_name_rep* get_rep(const FN_composite_name&);
	FN_composite_name(FN_composite_name_rep*);
};

#endif /* _XFN_FN_COMPOSITE_NAME_HH */
