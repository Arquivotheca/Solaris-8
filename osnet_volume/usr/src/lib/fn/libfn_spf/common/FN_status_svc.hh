/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_STATUS_SVC_HH
#define	_XFN_STATUS_SVC_HH

#pragma ident	"@(#)FN_status_svc.hh	1.6	94/09/21 SMI"

// Extended status object that has some useful operations used by
// clients of the framework

#include <xfn/xfn.hh>
#include <xfn/fn_spi.h>

class FN_status_psvc : public FN_status {
public:
	FN_status_psvc();
	virtual ~FN_status_psvc();

	// wrapper around FN_status
	FN_status_psvc(FN_status &);

	int is_following_terminal_link(void) const;

	int is_continue(void) const;

	int set_error(unsigned int code,
	    const FN_ref &resolved_reference,
	    const FN_composite_name &remaining_name);

	int set_link_error(const FN_status &);

	// set resolved_reference to resolved_context reference
	int set_error_context(unsigned code,
	    const FN_ctx &resolved_context,
	    const FN_composite_name *remaining_name = 0);

	// set continuation codes
	int set_continue(const FN_ref &resolved_ref,
	    const FN_ref &current_ref,
	    const FN_composite_name *r_name = 0);

	int set_continue_context(const FN_ref &resolved_ref,
	    const FN_ctx &current_context,
	    const FN_composite_name *r_name = 0);

	// get description of status
	FN_string* description(unsigned detail = 0,
	    unsigned *more_detail = 0) const;

protected:
	FN_status *sp;
};


class FN_status_cnsvc : public FN_status_psvc {
public:
	FN_status_cnsvc();
	virtual ~FN_status_cnsvc();

	// wrapper around FN_status
	FN_status_cnsvc(FN_status &);

	// set code to continue and insert null componennt
	// in front of remaining_name (to go to nns)
	int set_nns_continue();
};



// single component name service interface (name input is string)
class FN_status_csvc : public FN_status_cnsvc {
public:
	FN_status_csvc();
	virtual ~FN_status_csvc();

	// wrapper around FN_status
	FN_status_csvc(FN_status &);

	int set_remaining_name(const FN_string* remain_name);

	// set code and resolved_reference/remaining_name
	int set_error(unsigned code,
	    const FN_ref &resolved_reference,
	    const FN_string &remaining_name);

	// set resolved_reference to resolved_context reference
	int set_error_context(unsigned code,
	    const FN_ctx &resolved_context,
	    const FN_string *remaining_name = 0);

	// set continuation information
	int set_continue(const FN_ref &resolved_ref,
	    const FN_ref &current_ref,
	    const FN_string *remaining_name = 0);

	int set_continue_context(const FN_ref &resolved_ref,
	    const FN_ctx &current_context,
	    const FN_string *remaining_name = 0);
};

// atomic name service interface
class FN_status_asvc : public FN_status_csvc {
public:
	FN_status_asvc();
	virtual ~FN_status_asvc();

	// wrapper around FN_status
	FN_status_asvc(FN_status &);

	int set_error(unsigned code, const FN_ref *resolved_ref = 0,
	    const FN_string *remain_name = 0);
};

#endif /* _XFN_STATUS_SVC_HH */
