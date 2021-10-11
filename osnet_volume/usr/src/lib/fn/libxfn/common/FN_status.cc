/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_status.cc	1.11	96/03/31 SMI"

#include <libintl.h>

#include <xfn/FN_status.hh>
#include <xfn/FN_ref.hh>
#include <xfn/FN_composite_name.hh>

#include <xfn/misc_codes.h>


class FN_status_rep {
public:
	unsigned int code;
	FN_ref *resolved_reference;
	FN_composite_name *remaining_name;
	FN_composite_name *resolved_name;
	FN_string *diagnostic_message;

	FN_status_rep(unsigned int code = FN_SUCCESS,
	    FN_ref * = 0, FN_composite_name *remaining = 0,
	    FN_composite_name *resolved = 0);

	FN_status_rep(const FN_status_rep &);
	~FN_status_rep();

	FN_status_rep &operator=(const FN_status_rep &);
};

FN_status_rep::FN_status_rep(
	unsigned int c,
	FN_ref *ref,
	FN_composite_name *remaining,
	FN_composite_name *resolved)
	: code(c), resolved_reference(ref), remaining_name(remaining),
	    resolved_name(resolved)
{
	diagnostic_message = 0;
}


FN_status_rep::FN_status_rep(const FN_status_rep &r)
{
	code = r.code;
	if (r.resolved_reference)
		resolved_reference = new FN_ref(*r.resolved_reference);
	else
		resolved_reference = 0;
	if (r.remaining_name)
		remaining_name = new FN_composite_name(*r.remaining_name);
	else
		remaining_name = 0;
	if (r.resolved_name)
		resolved_name = new FN_composite_name(*r.resolved_name);
	else
		resolved_name = 0;
	if (r.diagnostic_message)
		diagnostic_message = new FN_string(*r.diagnostic_message);
	else
		diagnostic_message = 0;
}

FN_status_rep::~FN_status_rep()
{
	delete resolved_reference;
	delete remaining_name;
	delete resolved_name;
	delete diagnostic_message;
}

FN_status_rep &FN_status_rep::operator=(const FN_status_rep &r)
{
	if (&r != this) {
		code = r.code;
		delete resolved_reference;
		if (r.resolved_reference)
			resolved_reference = new FN_ref(*r.resolved_reference);
		else
			resolved_reference = 0;
		delete remaining_name;
		if (r.remaining_name)
			remaining_name = new FN_composite_name(
							*r.remaining_name);
		else
			remaining_name = 0;
		delete resolved_name;
		if (r.resolved_name)
			resolved_name = new FN_composite_name(
							*r.resolved_name);
		else
			resolved_name = 0;
		delete diagnostic_message;
		if (r.diagnostic_message)
		    diagnostic_message = new FN_string(*r.diagnostic_message);
		else
		    diagnostic_message = (FN_string *)0;
	}
	return (*this);
}


FN_status::FN_status(FN_status_rep *r, FN_status_rep* lr)
	: rep(r), link_rep(lr)
{
}

FN_status_rep *
FN_status::get_rep(const FN_status &s)
{
	return (s.rep);
}

FN_status_rep *
FN_status::get_link_rep(const FN_status &s)
{
	return (s.link_rep);
}

FN_status::FN_status(unsigned int status)
{
	rep = new FN_status_rep(status);
	link_rep = new FN_status_rep(status);
}

FN_status::FN_status()
{
	rep = new FN_status_rep();
	link_rep = new FN_status_rep();
}

FN_status::~FN_status()
{
	delete rep;
	delete link_rep;
}

// copy and assignment
FN_status::FN_status(const FN_status &s)
{
	rep = new FN_status_rep(*get_rep(s));

	FN_status_rep * lrep = get_link_rep(s);
	if (lrep)
	    link_rep = new FN_status_rep(*lrep);
	else
	    link_rep = 0;
}

FN_status &
FN_status::operator=(const FN_status &s)
{
	if (&s != this) {
		*rep = *get_rep(s);
		FN_status_rep *lrep = get_link_rep(s);
		if (lrep)
			*link_rep = *lrep;
	}
	return (*this);
}

// get operation's statuscode
unsigned int
FN_status::code() const
{
	return (rep->code);
}

// get reference operation resolved to
const FN_ref *
FN_status::resolved_ref() const
{
	return (rep->resolved_reference);
}

// get name remaining to be resolved
const FN_composite_name *
FN_status::remaining_name() const
{
	return (rep->remaining_name);
}

// get resolved name
const FN_composite_name *
FN_status::resolved_name() const
{
	return (rep->resolved_name);
}

// get operation's link statuscode
unsigned int
FN_status::link_code() const
{
	if (link_rep)
		return (link_rep->code);
	else
		return (0);
}

// get link reference operation resolved to
const FN_ref *
FN_status::link_resolved_ref() const
{
	if (link_rep)
		return (link_rep->resolved_reference);
	else
		return (0);
}

// get link name remaining to be resolved
const FN_composite_name *
FN_status::link_remaining_name() const
{
	if (link_rep)
		return (link_rep->remaining_name);
	else
		return (0);
}

// get link resolved name
const FN_composite_name *
FN_status::link_resolved_name() const
{
	if (link_rep)
		return (link_rep->resolved_name);
	else
		return (0);
}

// test for statuscode success
int
FN_status::is_success() const
{
	return (rep->code == FN_SUCCESS);
}

static const char *statuscode_string_mapping[] = {
	"Unknown Error",
	"Success", /* FN_SUCCESS */
	"Link Error", /* FN_E_LINK_ERROR */
	"Configuration Error", /* FN_E_CONFIGURATION_ERROR */
	"Name Not Found", /* FN_E_NAME_NOT_FOUND */
	"Not a Context", /* FN_E_NOT_A_CONTEXT */
	"Link Loop Limit Reached", /* FN_E_LINK_LOOP_LIMIT */
	"Malformed Link", /* FN_E_MALFORMED_LINK */
	"Illegal Name", /* FN_E_ILLEGAL_NAME */
	"No Permission", /* FN_E_CTX_NO_PERMISSION */
	"Name in Use", /* FN_E_NAME_IN_USE */
	"Operation not Supported", /* FN_E_OPERATION_NOT_SUPPORTED */
	"Communication Failure", /* FN_E_COMMUNICATION_FAILURE */
	"Unavailable", /* FN_E_CTX_UNAVAILABLE */
	"No Supported Address", /* FN_E_NO_SUPPORTED_ADDRESS */
	"Bad Reference", /* FN_E_MALFORMED_REFERENCE */
	"Authentication Failure", /* FN_E_AUTHENTICATION_FAILURE */
	"Insufficient Resources", /* FN_E_INSUFFICIENT_RESOURCES */
	"Context not Empty", /* FN_E_CTX_NOT_EMPTY */
	"No Such Attribute", /* FN_E_NO_SUCH_ATTRIBUTE */
	"Invalid Attribute Identifier", /* FN_E_INVALID_ATTR_IDENTIFIER */
	"Invalid Attribute Value", /* FN_E_INVALID_ATTR_VALUE */
	"Too Many Attribute Values", /* FN_E_TOO_MANY_ATTR_VALUES */
	"Attribute Value Required", /* FN_E_ATTR_VALUE_REQUIRED */
	"Attribute No Permission", /* FN_E_ATTR_NO_PERMISSION */
	"Partial Result Returned", /* FN_E_PARTIAL_RESULT */
	"Invalid Enumeration Handle", /* FN_E_INVALID_ENUM_HANDLE */
	"Syntax Not Supported", /* FN_E_SYNTAX_NOT_SUPPORTED */
	"Invalid Syntax Attributes", /* FN_E_INVALID_SYNTAX_ATTRS */
	"Incompatible Code Sets", /* FN_E_INCOMPATIBLE_CODE_SETS */
	"Continue Operation Using Status Values", /* FN_E_CONTINUE */
	"Unspecified Error", /* FN_E_UNSPECIFIED_ERROR */
	"No Equivalent Name", /* FN_E_NO_EQUIVALENT_NAME */
	"Attribute Identifier in Use", /* FN_E_ATTR_IN_USE */
	"Invalid Search Filter", /* FN_E_SEARCH_INVALID_FILTER */
	"Invalid Operator in Search Filter", /* FN_E_SEARCH_INVALID_OP */
	"Invalid Search Option", /* FN_E_SEARCH_INVALID_OPTION */
	"Incompatible Locales" /* FN_E_INCOMPATIBLE_LOCALES */
};

#if 0
"Context not Found"; /*  FN_E_CTX_NOT_FOUND */
#endif

#define	NUM_STATUS_CODES \
	(sizeof (statuscode_string_mapping) / \
	    sizeof (statuscode_string_mapping[0]))

static FN_string __statuscode_string(unsigned sc)
{
	const char *str;

	if (sc < FN_SUCCESS || sc > NUM_STATUS_CODES)
		str = statuscode_string_mapping[0];
	else
		str = statuscode_string_mapping[sc];

	return (FN_string((const unsigned char *)gettext(str)));
}


static FN_string *
describe_part(
	const FN_composite_name *remaining_name,
	const FN_string *diag_msg,
	const FN_ref *resolved_ref,
	unsigned int d,
	unsigned int *md)
{
	FN_string *ret = 0;
	FN_string *newret;
	unsigned int status;

	if (remaining_name) {
		FN_string	*s = remaining_name->string(&status);
		FN_string	rname_str(*s);
		FN_string	sep((const unsigned char *)": '");
		FN_string	esep((const unsigned char *)"'");

		delete s;
		if (ret) {
			newret = new FN_string(&status, ret, &sep, &rname_str,
			    &esep, (FN_string *)0);
			delete ret;
			ret = newret;
		} else
			ret = new FN_string(&status, &sep, &rname_str, &esep,
			    (FN_string *)0);
		// %%% check status
	}

	if (diag_msg) {
		if (ret) {
			newret = new FN_string(&status, ret, diag_msg,
			    (FN_string *)0);
			delete ret;
			ret = newret;
		} else
			ret = new FN_string(*diag_msg);
	}

	if (resolved_ref) {
		if (d == 0) {
			if (md)
				*md = 1;
		} else {
			unsigned refd, refmd;
			if (d < FN_DESCRIPTION_COMPLETE)
				refd = d-1;
			else
				refd = FN_DESCRIPTION_COMPLETE;
			FN_string *rref_str = resolved_ref->description(refd,
			    &refmd);
			FN_string sep((const unsigned char *)"\n");

			if (ret) {
				newret = new FN_string(&status, ret, &sep,
				    rref_str, (FN_string *)0);
				delete rref_str;
				delete ret;
				ret = newret;
			} else {
				ret = new FN_string(&status, &sep, rref_str,
				    (FN_string *)0);
			}
			// %%% check status
			if (md) {
				if (refmd == refd)
					*md = d;
				else
					*md = refmd+1;
			}
		}
	} else if (md)
		*md = d;

	return (ret);
}


// get description of status
FN_string *
FN_status::description(unsigned int d,	unsigned int *md) const
{
	// d>=0			--> code + name + diagnostic
	// complete>d>=1	--> + reference description(d-1)
	// d==complete		--> + reference description(complete)

	FN_string	*ret = new FN_string(__statuscode_string(code()));
	unsigned int	status;
	FN_string	*newret;

	FN_string *main_msg;
	if (code() == FN_E_NO_SUCH_ATTRIBUTE)
		main_msg = describe_part(0, diagnostic_message(),
		    resolved_ref(), d, md);
	else
		main_msg = describe_part(remaining_name(),
		    diagnostic_message(), resolved_ref(), d, md);

	if (main_msg) {
	    newret = new FN_string(&status, ret, main_msg, (FN_string *)0);
	    delete ret;
	    ret = newret;
	    delete main_msg;
	}

	if (code() == FN_E_LINK_ERROR) {
		FN_string *error =
		    new FN_string(__statuscode_string(link_code()));
		FN_string *link_msg = describe_part(link_remaining_name(),
		    link_diagnostic_message(), link_resolved_ref(), d, md);
		FN_string link_header((const unsigned char *)
		    " Link error information: ");
		newret = new FN_string(&status, ret, &link_header, error,
		    link_msg, (FN_string *)0);
		delete ret;
		ret = newret;
		delete error;
		delete link_msg;
	}

	return (ret);
}



// set all
int FN_status::set(
	unsigned int c,
	const FN_ref *ref,
	const FN_composite_name *resolved_name,
	const FN_composite_name *remaining_name)
{
	rep->code = c;
	if (rep->resolved_reference != ref) {
		delete rep->resolved_reference;
		if (ref)
			rep->resolved_reference = new FN_ref(*ref);
		else
			rep->resolved_reference = 0;
	}
	if (rep->remaining_name != remaining_name) {
		delete rep->remaining_name;
		if (remaining_name)
			rep->remaining_name =
			    new FN_composite_name(*remaining_name);
		else
			rep->remaining_name = 0;
	}
	if (rep->resolved_name != resolved_name) {
		delete rep->resolved_name;
		if (resolved_name)
			rep->resolved_name =
			    new FN_composite_name(*resolved_name);
		else
			rep->resolved_name = 0;
	}
	return (1);
}

// set code to success and clear
// resolved_reference/remaining_name/resolved_name

int
FN_status::set_success()
{
	rep->code = FN_SUCCESS;
	delete rep->resolved_reference;
	rep->resolved_reference = 0;
	delete rep->remaining_name;
	rep->remaining_name = 0;
	delete rep->resolved_name;
	rep->resolved_name = 0;
	delete rep->diagnostic_message;
	rep->diagnostic_message = 0;
	return (1);
}


// set statuscode only
int
FN_status::set_code(unsigned c)
{
	rep->code = c;
	return (1);
}

// set resolved_reference only
int
FN_status::set_resolved_ref(const FN_ref *ref)
{
	if (rep->resolved_reference != ref) {
		delete rep->resolved_reference;
		if (ref)
			rep->resolved_reference = new FN_ref(*ref);
		else
			rep->resolved_reference = 0;
	}
	return (1);
}

// set remaining_name only
int
FN_status::set_remaining_name(const FN_composite_name *name)
{
	if (rep->remaining_name != name) {
		delete rep->remaining_name;
		if (name)
			rep->remaining_name = new FN_composite_name(*name);
		else
			rep->remaining_name = 0;
	}
	return (1);
}

// set resolved_name only
int
FN_status::set_resolved_name(const FN_composite_name *name)
{
	if (rep->resolved_name != name) {
		delete rep->resolved_name;
		if (name)
			rep->resolved_name = new FN_composite_name(*name);
		else
			rep->resolved_name = 0;
	}
	return (1);
}

// append to remaining_name
int
FN_status::append_remaining_name(const FN_composite_name &name)
{
	if (rep->remaining_name)
		rep->remaining_name->append_name(name);
	else
		rep->remaining_name = new FN_composite_name(name);
	return (1);
}

// append to resolved_name
int
FN_status::append_resolved_name(const FN_composite_name &name)
{
	if (rep->resolved_name)
		rep->resolved_name->append_name(name);
	else
		rep->resolved_name = new FN_composite_name(name);
	return (1);
}

// move prefix from remaining to resolved part and set resolved_re
int
FN_status::advance_by_name(const FN_composite_name &prefix,
    const FN_ref &resolved_ref)
{
	void *iter_pos, *ip;
	unsigned int status;
	FN_composite_name pre = prefix;

	if (rep->remaining_name) {
		if (!rep->remaining_name->is_prefix(prefix, iter_pos, &status))
			return (0);

		if ((pre.first(ip)) && (rep->remaining_name->first(iter_pos))) {
			rep->remaining_name->delete_comp(iter_pos);
			while (pre.next(ip) &&
			    rep->remaining_name->first(iter_pos))
				rep->remaining_name->delete_comp(iter_pos);
			append_resolved_name(prefix);
		}
	} else
		return (0);

	set_resolved_ref(&resolved_ref);
	return (1);
}



// set link statuscode only
int
FN_status::set_link_code(unsigned int c)
{
	if (link_rep == 0)
		link_rep = new FN_status_rep();
	link_rep->code = c;
	return (1);
}

// set link resolved_reference only
int
FN_status::set_link_resolved_ref(const FN_ref *ref)
{
	if (link_rep == 0)
		link_rep = new FN_status_rep();
	if (link_rep->resolved_reference != ref) {
		delete link_rep->resolved_reference;
		if (ref)
			link_rep->resolved_reference = new FN_ref(*ref);
		else
			link_rep->resolved_reference = 0;
	}
	return (1);
}

// set link remaining_name only
int
FN_status::set_link_remaining_name(const FN_composite_name *name)
{
	if (link_rep == 0)
		link_rep = new FN_status_rep();
	if (link_rep->remaining_name != name) {
		delete link_rep->remaining_name;
		if (name)
			link_rep->remaining_name = new FN_composite_name(*name);
		else
			link_rep->remaining_name = 0;
	}
	return (1);
}

// set link resolved_name only
int
FN_status::set_link_resolved_name(const FN_composite_name *name)
{
	if (link_rep == 0)
		link_rep = new FN_status_rep();
	if (link_rep->resolved_name != name) {
		delete link_rep->resolved_name;
		if (name)
			link_rep->resolved_name = new FN_composite_name(*name);
		else
			link_rep->resolved_name = 0;
	}
	return (1);
}

// Diagnostic message interface
const FN_string *
FN_status::diagnostic_message(void) const
{
	if (rep)
		return (rep->diagnostic_message);
	else
		return (0);
}

const FN_string *
FN_status::link_diagnostic_message(void) const
{
	if (link_rep)
		return (link_rep->diagnostic_message);
	else
		return (0);
}

int
FN_status::set_diagnostic_message(const FN_string *msg)
{
	if (rep) {
		delete rep->diagnostic_message;
		if (msg)
			rep->diagnostic_message = new FN_string(*msg);
		else rep->diagnostic_message = 0;
		return (1);
	} else
		return (0);
}

int
FN_status::set_link_diagnostic_message(const FN_string *msg)
{
	if (link_rep) {
		delete link_rep->diagnostic_message;
		if (msg)
			link_rep->diagnostic_message = new FN_string(*msg);
		else link_rep->diagnostic_message = 0;
		return (1);
	} else
		return (0);
}
