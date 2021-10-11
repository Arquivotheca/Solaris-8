/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_REF_ADDR_HH
#define	_XFN_FN_REF_ADDR_HH

#pragma ident	"@(#)FN_ref_addr.hh	1.3	96/03/31 SMI"

#include <xfn/FN_ref_addr.h>
#include <xfn/FN_string.hh>
#include <xfn/FN_identifier.hh>

class FN_ref_addr_rep;

class FN_ref_addr {
public:
	FN_ref_addr(const FN_identifier &type,
		size_t len,
		const void *data);
	~FN_ref_addr();

	// copy and assignment
	FN_ref_addr(const FN_ref_addr &);
	FN_ref_addr &operator=(const FN_ref_addr &);

	// address type
	const FN_identifier *type(void) const;

	// length of address data in octets
	size_t length(void) const;

	// address data
	const void *data(void) const;

	// get description of address
	FN_string *description(unsigned int detail = 0,
	    unsigned int *more_detail = 0) const;

protected:
	FN_ref_addr_rep	*rep;
	static FN_ref_addr_rep *get_rep(const FN_ref_addr &);
	FN_ref_addr(FN_ref_addr_rep *);
private:
	FN_ref_addr();
};

#endif /* _XFN_FN_REF_ADDR_HH */
