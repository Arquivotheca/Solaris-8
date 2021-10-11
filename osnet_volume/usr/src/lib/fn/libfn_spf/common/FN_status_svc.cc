/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#include <xfn/FN_status_svc.hh>

FN_status_psvc::FN_status_psvc(FN_status &s) : FN_status(get_rep(s),
							    get_link_rep(s))
{
	sp = &s;
}

FN_status_psvc::FN_status_psvc()
{
	sp = 0;
}

FN_status_psvc::~FN_status_psvc()
{
	if (sp) {
		rep = 0;
		link_rep = 0;
	}
}

FN_status_psvc::is_following_terminal_link() const
{
	return (code() == FN_E_SPI_FOLLOW_LINK && remaining_name() == 0);
}


// returns whether status indicates an SPI "continue" status
FN_status_psvc::is_continue() const
{
	switch (code()) {
	case FN_E_SPI_FOLLOW_LINK:
	case FN_E_SPI_CONTINUE:
		return (1);
	default:
		return (0);
	}
}

// set code and resolved_reference/remaining_name
int FN_status_psvc::set_error(unsigned int c,
    const FN_ref &ref,
    const FN_composite_name &remain)
{
	set_code(c);
	set_resolved_ref(&ref);
	set_remaining_name(&remain);
	// leaves resolved name unchanged
	return (1);
}

// set error code to FN_E_LINK_ERROR and copy codes from ls to *link* fields
int
FN_status_psvc::set_link_error(const FN_status &lstat)
{
	set_code(FN_E_LINK_ERROR);
	set_link_code(lstat.code());
	set_link_resolved_ref(lstat.resolved_ref());
	set_link_resolved_name(lstat.resolved_name());
	set_link_remaining_name(lstat.remaining_name());
	set_link_diagnostic_message(lstat.diagnostic_message());
	return (1);
}

int
FN_status_psvc::set_error_context(unsigned c,
    const FN_ctx &ctx,
    const FN_composite_name *remain_name)
{
	FN_status s;
	FN_ref *ref = ctx.get_ref(s);
	set_code(c);
	set_resolved_ref(ref);
	// leaves resolved name unchanged
	set_remaining_name(remain_name);
	if (ref)
		delete ref;
	return (1);
}


int
FN_status_psvc::set_continue(const FN_ref &rref,
    const FN_ref &cur_ref,
    const FN_composite_name *rn)
{
	if (rref.is_link()) {
		if (rn)
			set_error(FN_E_SPI_FOLLOW_LINK, rref, *rn);
		else
			set(FN_E_SPI_FOLLOW_LINK, &rref);

		// set context for relative link
		set_link_resolved_ref(&cur_ref);
	} else {
		if (rn)
			set_error(FN_E_SPI_CONTINUE, rref, *rn);
		else {
			set_code(FN_E_SPI_CONTINUE);
			set_resolved_ref(&rref);
		}
	}
	return (1);
}


int
FN_status_psvc::set_continue_context(const FN_ref &rref,
    const FN_ctx &curr_ctx,
    const FN_composite_name *rn)
{
	if (rref.is_link()) {
		if (rn)
			set_error(FN_E_SPI_FOLLOW_LINK, rref, *rn);
		else
			set(FN_E_SPI_FOLLOW_LINK, &rref);

		// set context for relative link
		FN_status s;
		FN_ref *cur_ref = curr_ctx.get_ref(s);
		set_link_resolved_ref(cur_ref);
		if (cur_ref)
			delete cur_ref;
	} else {
		if (rn)
			set_error(FN_E_SPI_CONTINUE, rref, *rn);
		else {
			set_code(FN_E_SPI_CONTINUE);
			set_resolved_ref(&rref);
		}
	}
	return (1);
}

/* ************** FN_status_cnsvc ****************************** */


FN_status_cnsvc::FN_status_cnsvc(FN_status &s)	: FN_status_psvc(s)
{
}

FN_status_cnsvc::FN_status_cnsvc() : FN_status_psvc()
{
}

FN_status_cnsvc::~FN_status_cnsvc()
{
}


// try nns associated with last resolved context
// by inserting leading null (to force follow nns) and passing up s_continue
int
FN_status_cnsvc::set_nns_continue()
{
	const FN_composite_name *rn = remaining_name();
	if (rn) {
		FN_composite_name newrn(*rn);
		newrn.prepend_comp((unsigned char *)"");
		FN_status_psvc::set_code(FN_E_SPI_CONTINUE);
		// resolved ref remains unchanged
		FN_status_psvc::set_remaining_name(&newrn);
	}
	return (1);
}


/* ************** FN_status_csvc ****************************** */

FN_status_csvc::FN_status_csvc(FN_status &s) : FN_status_cnsvc(s)
{
}

FN_status_csvc::FN_status_csvc() : FN_status_cnsvc()
{
}

FN_status_csvc::~FN_status_csvc()
{
}

FN_status_csvc::set_remaining_name(const FN_string* remaining_name)

{
	if (remaining_name) {
		FN_composite_name *cname = new FN_composite_name();
		cname->append_comp(*remaining_name);
		FN_status::set_remaining_name(cname);
		delete cname;
	} else
		FN_status::set_remaining_name(0);
	return (1);
}


// set code and resolved_reference/remaining_name
int
FN_status_csvc::set_error(unsigned c,
    const FN_ref &ref,
    const FN_string &name)
{
	FN_composite_name *cname = new FN_composite_name();
	cname->append_comp(name);
	FN_status_cnsvc::set_error(c, ref, *cname);
	delete cname;
	return (1);
}

// set resolved_reference to resolved_context's reference
int
FN_status_csvc::set_error_context(unsigned c,
    const FN_ctx &ctx,
    const FN_string *name)
{
	if (name) {
		FN_composite_name *cname = new FN_composite_name();
		cname->append_comp(*name);
		FN_status_cnsvc::set_error_context(c, ctx, cname);
		delete cname;
	} else
		FN_status_cnsvc::set_error_context(c, ctx, 0);
	return (1);
}

// set status to indicate continuation
int
FN_status_csvc::set_continue_context(const FN_ref &rref,
    const FN_ctx &curctx,
    const FN_string *name)
{
	if (name) {
		FN_composite_name *cname = new FN_composite_name();
		cname->append_comp(*name);
		FN_status_cnsvc::set_continue_context(rref, curctx, cname);
		delete cname;
	} else
		FN_status_cnsvc::set_continue_context(rref, curctx, 0);
	return (1);
}

int
FN_status_csvc::set_continue(const FN_ref &rref, const FN_ref &cur_ref,
    const FN_string *remain_name)
{

	if (remain_name) {
		FN_composite_name *cname = new FN_composite_name();
		cname->append_comp(*remain_name);
		FN_status_cnsvc::set_continue(rref, cur_ref, cname);
		delete cname;
	} else
		FN_status_cnsvc::set_continue(rref, cur_ref, 0);
	return (1);
}

/* ************** FN_status_asvc ****************************** */


FN_status_asvc::FN_status_asvc(FN_status &s) : FN_status_csvc(s)
{
}

FN_status_asvc::FN_status_asvc() : FN_status_csvc()
{
}

FN_status_asvc::~FN_status_asvc()
{
}

int
FN_status_asvc::set_error(unsigned c, const FN_ref *ref, const FN_string *name)
{
	FN_composite_name *cname = 0;

	if (name) {
		cname = new FN_composite_name();
		cname->append_comp(*name);
	}
	FN_status::set(c, ref, 0, cname);
	if (cname)
		delete cname;
	return (1);
}
