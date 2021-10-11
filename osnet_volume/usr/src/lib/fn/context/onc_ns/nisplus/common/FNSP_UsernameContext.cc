/*
 * Copyright (c) 1992 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_UsernameContext.cc	1.8	97/08/15 SMI"


#include "FNSP_UsernameContext.hh"
#include <FNSP_Syntax.hh>
#include "FNSP_nisplus_address.hh"
#include <xfn/fn_p.hh>

#include "FNSP_nisplusImpl.hh"

#include "fnsp_internal.hh"
#include "fnsp_hostuser.hh"
#include "fnsp_utils.hh"

static FN_attribute *construct_attr(const FN_identifier &,
    const FN_string &passwd, const FN_string &shadow, unsigned int &status);

/* maximum size for diagnostic message */
#define	MAXMSGLEN 256

static const FN_string
    FNSP_username_attribute((unsigned char *)"_user_attribute");

// Name of attributes containing password and shadow information.
static const FN_identifier attr_passwd((unsigned char *)"onc_unix_passwd");
static const FN_identifier attr_shadow((unsigned char *)"onc_unix_shadow");

// ASCII syntax for attribute values.
static const FN_identifier ascii((unsigned char *)"fn_attr_syntax_ascii");


static inline FN_string *
FNSP_username_attribute_internal_name(FN_string &domain)
{
	return (FNSP_compose_ctx_tablename(FNSP_username_attribute, domain));
}

FNSP_UsernameContext::FNSP_UsernameContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FNSP_HUContext(from_addr, from_ref, FNSP_user_context, auth)
{
	builtin_attrs.add(FN_attribute(attr_passwd, ascii));
	builtin_attrs.add(FN_attribute(attr_shadow, ascii));
}

FNSP_UsernameContext::FNSP_UsernameContext(const FN_ref &from_ref,
    unsigned int auth)
: FNSP_HUContext(from_ref, FNSP_user_context, auth)
{
	builtin_attrs.add(FN_attribute(attr_passwd, ascii));
	builtin_attrs.add(FN_attribute(attr_shadow, ascii));
}

FNSP_UsernameContext*
FNSP_UsernameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_UsernameContext *answer = new FNSP_UsernameContext(from_addr,
	    from_ref, auth);

	if (answer && answer->my_reference && answer->ns_impl &&
	    answer->ns_impl->my_address)
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}

FNSP_UsernameContext::check_for_config_error(const FN_string &name,
    FN_status_csvc& cs)
{
	unsigned status;
	char diagmsg[MAXMSGLEN];

	FN_string *home_org = FNSP_find_user_entry(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(), status);

	if (home_org) {
		if (home_org->compare(*my_orgname,
		    FN_STRING_CASE_INSENSITIVE) == 0)
			sprintf(diagmsg,
"\nEntry for %s exists in passwd table but does not have associated context.",
			    name.str());
		else
			sprintf(diagmsg,
"\nPasswd entry for %s is in domain %s but looking for context in domain %s",
			    name.str(), home_org->str(), my_orgname->str());

		FN_string dmsg((const unsigned char *)diagmsg);
		cs.set_code(FN_E_CONFIGURATION_ERROR);
		cs.set_diagnostic_message(&dmsg);

		delete home_org;
	} else {
		// cannot find passwd entry either.  No problem here.
		return (0);
	}
	return (1);
}


FNSP_Address*
FNSP_UsernameContext::get_attribute_context(const FN_string & /* name */,
    unsigned &status, unsigned int local_auth)
{
	FNSP_Address *target_ctx = 0;
	FN_string *target_name;

	target_name = FNSP_username_attribute_internal_name(*my_orgname);
	if (target_name) {
		target_ctx = new FNSP_nisplus_address(*target_name,
		    FNSP_username_context, FNSP_normal_repr,
		    authoritative|local_auth);
		if (target_ctx == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
		delete target_name;
		status = FN_SUCCESS;
	} else
		status = FN_E_NAME_NOT_FOUND;

	return (target_ctx);
}


// Operations on builtin passwd attribute.

FN_attribute *
FNSP_UsernameContext::builtin_attr_get(const FN_string &name,
    const FN_identifier &attrname, FN_status_csvc &cs)
{
	FN_attribute *attr;
	unsigned int status;
	FN_string passwd;
	FN_string shadow;

	if (FNSP_find_passwd_shadow(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(), passwd, shadow, status)) {
		attr = construct_attr(attrname, passwd, shadow, status);
	}
	if (status == FN_SUCCESS) {
		cs.set_success();
		return (attr);
	} else {
		cs.set_error(status, *my_reference, name);
		return (0);
	}
}

FN_attrset *
FNSP_UsernameContext::builtin_attr_get_all(const FN_string &name,
    FN_status_csvc &cs)
{
	unsigned int status, stat;
	FN_string passwd;
	FN_string shadow;
	char *mailentry;
	FN_attribute *mailattribute = 0;

	if (!FNSP_find_passwd_shadow(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(), passwd, shadow, status)) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}

	FN_attrset *answer = FNSP_get_user_builtin_attrset(
	    (char *) passwd.str(), (char *) shadow.str(), status);

	if (FNSP_find_mailentry(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(),
	    mailentry, stat)) {
		mailattribute = FNSP_get_builtin_mail_attribute(
		    mailentry, stat);
		free (mailentry);
		if (answer)
			answer->add(*mailattribute);
		delete mailattribute;
	}

	if (status != FN_SUCCESS) {
		delete answer;
		answer = 0;
		cs.set_error(status, *my_reference, name);
	} else {
		cs.set_success();
	}
	return (answer);
}


static FN_attribute *
construct_attr(const FN_identifier &attrname, const FN_string &passwd,
    const FN_string &shadow, unsigned int &status)
{
	FN_attribute *attr = new FN_attribute(attrname, ascii);
	if (attr == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	const FN_string *val;
	if (attrname == attr_passwd) {
		val = &passwd;
	} else if (attrname == attr_shadow) {
		val = &shadow;
	} else {
		status = FN_E_NO_SUCH_ATTRIBUTE;
		delete attr;
		return (0);
	}

	if (attr->add(*val) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		delete attr;
		return (0);
	} else {
		status = FN_SUCCESS;
		return (attr);
	}
}

FN_searchset *
FNSP_UsernameContext::builtin_attr_search(const FN_attrset &,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_SEARCH_INVALID_OP, *my_reference,
	    (const unsigned char *)"");
	return (NULL);
}
