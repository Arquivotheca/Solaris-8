/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_SEARCH_FILTER_HH
#define	_XFN_FN_SEARCH_FILTER_HH

#pragma ident	"@(#)FN_search_filter.hh	1.1	96/03/31 SMI"

#include <stdarg.h>  /* for var args */
#include <stddef.h>  /* for size_t */

#include <xfn/FN_search_filter.h>

class FN_search_filter_rep;


class FN_search_filter {
public:
	/* constructors and destructors */

	FN_search_filter(unsigned int &status, const unsigned char *estr, ...);
	FN_search_filter(unsigned int &status, const unsigned char *estr,
	    va_list args);
	FN_search_filter(unsigned int &status, const unsigned char *estr,
	    FN_search_filter_type *filter_types, void **filter_args);

	~FN_search_filter();

	// copy and assignment
	FN_search_filter(const FN_search_filter&);
	FN_search_filter& operator=(const FN_search_filter&);


	// accessors
	const unsigned char *filter_expression() const;

	const void **filter_arguments(size_t *num_args) const;
	const FN_search_filter_type *filter_argument_types(size_t *num_args)
	    const;

    protected:
	FN_search_filter_rep* rep;
	static FN_search_filter_rep* get_rep(const FN_search_filter&);
	FN_search_filter(FN_search_filter_rep*);

};

#endif /* _XFN_FN_SEARCH_FILTER_HH */
