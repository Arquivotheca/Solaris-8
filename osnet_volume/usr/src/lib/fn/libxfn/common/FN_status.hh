/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_STATUS_HH
#define	_XFN_FN_STATUS_HH

#pragma ident	"@(#)FN_status.hh	1.4	96/03/31 SMI"

#include <xfn/FN_status.h>
#include <xfn/FN_ref.hh>
#include <xfn/FN_composite_name.hh>
#include <xfn/FN_string.hh>

class FN_status_rep;

class FN_status {
public:
	FN_status();
	~FN_status();

	// copy and assignment
	FN_status(const FN_status&);
	FN_status(unsigned int status_code);
	FN_status& operator=(const FN_status&);

	// get operation's statuscode
	unsigned int code(void) const;

	// get reference operation resolved to
	const FN_ref* resolved_ref(void) const;

	// get name remaining to be resolved
	const FN_composite_name* remaining_name(void) const;

	// get name resolved so far
	const FN_composite_name* resolved_name(void) const;

	// get dignostic message
	const FN_string *diagnostic_message(void) const;

	// get operation's link statuscode
	unsigned int link_code(void) const;

	// get reference operation resolved to by link
	const FN_ref* link_resolved_ref(void) const;

	// get name remaining to be resolved
	const FN_composite_name* link_remaining_name(void) const;

	// get name resolved so far
	const FN_composite_name* link_resolved_name(void) const;

	// get dignostic message
	const FN_string *link_diagnostic_message(void) const;

	FN_string *description(unsigned int detail = 0,
	    unsigned int *more_detail = 0) const;

	// test for success
	int is_success(void) const;

	// set all
	int set(unsigned code,
		const FN_ref* resolved_reference = 0,
		const FN_composite_name* resolved_name = 0,
		const FN_composite_name* remaining_name = 0);

	// set success and clear resolved_reference/remaining_name
	int set_success(void);

	// set code only
	int set_code(unsigned int code);

	// set resolved_reference only
	int set_resolved_ref(const FN_ref*);

	// set remaining_name only
	int set_remaining_name(const FN_composite_name*);

	// set resolved_name only
	int set_resolved_name(const FN_composite_name*);

	// set dignostic message
	int set_diagnostic_message(const FN_string *);

	// %%% extension set all link fields
	int set_link(unsigned link_code,
		const FN_ref* link_resolved_reference = 0,
		const FN_composite_name* link_resolved_name = 0,
		const FN_composite_name* link_remaining_name = 0);

	int set_link_code(unsigned int code);

	// set link resolved_reference only
	int set_link_resolved_ref(const FN_ref*);

	// set link remaining_name only
	int set_link_remaining_name(const FN_composite_name*);

	// set link resolved_name only
	int set_link_resolved_name(const FN_composite_name*);

	// set link dignostic message
	int set_link_diagnostic_message(const FN_string *);

	// append to remaining_name
	int append_remaining_name(const FN_composite_name&);

	// append to resolved_name
	int append_resolved_name(const FN_composite_name&);

	// move prefix from remaining to resolved part and set resolved_ref
	int advance_by_name(const FN_composite_name& prefix,
			    const FN_ref& resolved_ref);

    protected:
	// %%% should have single representation, exposing implementation here
	FN_status_rep* rep;
	FN_status_rep* link_rep;
	static FN_status_rep *get_rep(const FN_status&);
	static FN_status_rep *get_link_rep(const FN_status&);
	FN_status(FN_status_rep *rep, FN_status_rep *link_rep);
};

#endif /* _XFN_FN_STATUS_HH */
