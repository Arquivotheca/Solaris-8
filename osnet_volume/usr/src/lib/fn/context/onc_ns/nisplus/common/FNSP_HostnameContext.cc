/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_HostnameContext.cc	1.12	99/10/13 SMI"


#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>

#include <xfn/fn_p.hh>
#include <FNSP_Syntax.hh>
#include "FNSP_nisplus_address.hh"

#include "FNSP_HostnameContext.hh"
#include "fnsp_internal.hh"
#include "fnsp_nisplus_root.hh"
#include "fnsp_hostuser.hh"
#include "fnsp_search.hh"

// Not declared in any public header file.
extern "C" char *inet_ntoa_r(const struct in_addr, char []);

static FN_attribute *construct_attr(const FN_identifier &, struct hostent *,
    const FN_string &orgname, unsigned int &status);

static char *
construct_query(const FN_attrset &attrs, unsigned int &status,
	FN_attribute *&rest_aliases, FN_attribute *&rest_addrs);

static int add_fn_alias(FN_attribute &, const FN_string &name,
    const FN_string &org);

static void qualify(FN_string &name, const FN_string &orgname);

#define	MAXMSGLEN 256	// maximum size of diagnostic message
#define	IP_SIZE (16*4)	// maximum size of dotted IPv6 string ("109.104.40.71")

static const FN_string
    FNSP_hostname_attribute((unsigned char *)"_host_attribute");

// Names of attributes containing host information.
static const FN_identifier attr_name((unsigned char *)"onc_host_name");
static const FN_identifier attr_aliases((unsigned char *)"onc_host_aliases");
static const FN_identifier
    attr_addrs((unsigned char *)"onc_host_ip_addresses");

// ASCII syntax for attribute values.
static const FN_identifier ascii((unsigned char *)"fn_attr_syntax_ascii");

// NSIDs.  Used to construct host aliases.
static const FN_string nsid_host((unsigned char *)"host");
static const FN_string nsid_org((unsigned char *)"org");


static inline FN_string *
FNSP_hostname_attribute_internal_name(FN_string &domain)
{
	return (FNSP_compose_ctx_tablename(FNSP_hostname_attribute, domain));
}

FNSP_HostnameContext::FNSP_HostnameContext(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth)
: FNSP_HUContext(from_addr, from_ref, FNSP_host_context, auth)
{
	builtin_attrs.add(FN_attribute(attr_name, ascii));
	builtin_attrs.add(FN_attribute(attr_aliases, ascii));
	builtin_attrs.add(FN_attribute(attr_addrs, ascii));
}

FNSP_HostnameContext::FNSP_HostnameContext(const FN_ref &from_ref,
    unsigned int auth)
: FNSP_HUContext(from_ref, FNSP_host_context, auth)
{
	builtin_attrs.add(FN_attribute(attr_name, ascii));
	builtin_attrs.add(FN_attribute(attr_aliases, ascii));
	builtin_attrs.add(FN_attribute(attr_addrs, ascii));
}

FNSP_HostnameContext*
FNSP_HostnameContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref, unsigned int auth,
    FN_status &stat)
{
	FNSP_HostnameContext *answer = new FNSP_HostnameContext(from_addr,
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

int
FNSP_HostnameContext::check_for_config_error(const FN_string &name,
    FN_status_csvc& cs)
{
	unsigned status;
	char diagmsg[MAXMSGLEN];
	FN_string *home_org = FNSP_find_host_entry(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(), status);

	if (home_org) {
		if (home_org->compare(*my_orgname,
		    FN_STRING_CASE_INSENSITIVE) == 0)
			sprintf(diagmsg,
"\nEntry for %s exists in hosts table but does not have associated context.",
			    name.str());
		else
			sprintf(diagmsg,
"\nHost entry for %s is in domain %s but looking for context in domain %s",
			    name.str(), home_org->str(), my_orgname->str());

		FN_string dmsg((const unsigned char *)diagmsg);
		cs.set_code(FN_E_CONFIGURATION_ERROR);
		cs.set_diagnostic_message(&dmsg);

		delete home_org;
	} else {
		// cannot find host entry either.  No problem here.
		return (0);
	}
	return (1);
}


FNSP_Address*
FNSP_HostnameContext::get_attribute_context(const FN_string & /* name */,
    unsigned &status, unsigned int local_auth)
{
	FNSP_Address *target_ctx = 0;
	FN_string *target_name;

	if (target_name = FNSP_hostname_attribute_internal_name(*my_orgname)) {
		target_ctx = new FNSP_nisplus_address(*target_name,
		    FNSP_hostname_context, FNSP_normal_repr,
		    authoritative|local_auth);
		if (target_ctx == 0)
			status = FN_E_INSUFFICIENT_RESOURCES;
		delete target_name;
		status = FN_SUCCESS;
	} else
		status = FN_E_NAME_NOT_FOUND;

	return (target_ctx);
}



// Operations on builtin attributes.

FN_attribute *
FNSP_HostnameContext::builtin_attr_get(const FN_string &name,
    const FN_identifier &attrname, FN_status_csvc &cs)
{
	struct hostent *he = get_hostent(name, cs);
	if (he == 0) {
		return (0);
	}
	unsigned int status;
	FN_attribute *attr = construct_attr(attrname, he, *my_orgname, status);
	if (attr == 0) {
		cs.set_error(status, *my_reference, name);
	}
	FNSP_free_hostent(he);
	return (attr);
}

FN_attrset *
FNSP_HostnameContext::builtin_attr_get_all(const FN_string &name,
    FN_status_csvc &cs)
{
	FN_attrset *answer = new FN_attrset();
	if (answer == 0) {
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *my_reference, name);
		return (0);
	}

	struct hostent *he = get_hostent(name, cs);
	if (he == 0) {
		return (0);
	}
	unsigned int status = FN_SUCCESS;

	const FN_attribute *bi;
	void *iter;
	for (bi = builtin_attrs.first(iter);
	    bi != 0;
	    bi = builtin_attrs.next(iter)) {
		FN_attribute *attr =
		    construct_attr(*bi->identifier(), he, *my_orgname, status);
		if (attr == 0) {
			break;
		}
		if (answer->add(*attr) == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			delete attr;
			break;
		}
		delete attr;
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

// Get the hostent structure for the named host.  Ensure that home
// organization (based on the corresponding hosts.org_dir table) matches
// the organization name.  The hostent structure should be freed by the
// caller using FNSP_free_hostent().
//
struct hostent *
FNSP_HostnameContext::get_hostent(const FN_string &name, FN_status_csvc &cs)
{
	unsigned int status;
	struct hostent *he;
	FN_string *home_org = FNSP_find_host_entry(*my_orgname, name,
	    ns_impl->my_address->get_access_flags(), status, &he);
	if (home_org == 0) {
		cs.set_error(status, *my_reference, name);
		return (0);
	}
	if (home_org->compare(*my_orgname, FN_STRING_CASE_INSENSITIVE) != 0) {
		char diagmsg[MAXMSGLEN];
		sprintf(diagmsg,
		    "\nHost entry for %s is in domain %s "
		    "but looking for context in domain %s",
		    name.str(), home_org->str(), my_orgname->str());
		FN_string dmsg((const unsigned char *)diagmsg);
		cs.set_code(FN_E_CONFIGURATION_ERROR);
		cs.set_diagnostic_message(&dmsg);
		delete home_org;
		return (0);
	}
	delete home_org;
	cs.set_success();
	return (he);
}


static FN_attribute *
construct_attr(const FN_identifier &attrname, struct hostent *he,
    const FN_string &orgname, unsigned int &status)
{
	FN_attribute *attr = new FN_attribute(attrname, ascii);
	if (attr == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
	} else {
		status = FN_SUCCESS;
		FN_string name((unsigned char *)he->h_name);
		if (attrname == attr_name) {
			qualify(name, orgname);
			if (attr->add(name) == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
			}
		} else if (attrname == attr_aliases) {
			char **ap;
			for (ap = he->h_aliases; *ap != 0; ap++) {
				FN_string alias((unsigned char *)*ap);
				qualify(alias, orgname);
				if (attr->add(alias) == 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					break;
				}
			}
			if (add_fn_alias(*attr, name, orgname) == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
			}
		} else if (attrname == attr_addrs) {
			char **addrs;
			for (addrs = he->h_addr_list; *addrs != 0; addrs++) {
				struct in_addr addr;
				memcpy(&addr.s_addr, (*addrs), sizeof(addr.s_addr));
				char ip_str[IP_SIZE];
				inet_ntoa_r(addr, ip_str);
				if (attr->add((unsigned char *)ip_str) == 0) {
					status = FN_E_INSUFFICIENT_RESOURCES;
					break;
				}
			}
		} else {
			status = FN_E_NO_SUCH_ATTRIBUTE;
		}
	}
	if (status == FN_SUCCESS) {
		return (attr);
	} else {
		delete attr;
		return (0);
	}
}


// Add value of the form "host/spin" or "org/ssi.eng/host/spin"
// to an attr_aliases attribute.  Return nonzero on success.
static int
add_fn_alias(FN_attribute &aliases, const FN_string &name,
    const FN_string &orgname)
{
	FN_composite_name alias(name);
	if (alias.prepend_comp(nsid_host) == 0) {
		return (0);
	}
	if (!FNSP_local_domain_p(orgname)) {
		FN_string *org = FNSP_strip_root_name(orgname);
		if (org == 0 ||
		    alias.prepend_comp(*org) == 0 ||
		    alias.prepend_comp(nsid_org) == 0) {
			delete org;
			return (0);
		}
		delete org;
	}
	FN_string *alias_string = alias.string();
	if (alias_string == 0 ||
	    aliases.add(*alias_string) == 0) {
		delete alias_string;
		return (0);
	}
	delete alias_string;
	return (1);
}


// Qualify a simple host name in a given organization so that NIS+
// name expansion will produce the correct fully-qualified name.
// eg:  In the ssi.bar.sun.com domain,
//	qualify("myhost", "foo.bar.sun.com.") produces "myhost.foo".

static void
qualify(FN_string &hostname, const FN_string &orgname)
{
	static FN_string dot((unsigned char *)".");
	FN_string *qualified_org = FNSP_short_orgname(orgname);
	if (qualified_org != 0 &&
	    !qualified_org->is_empty()) {
		hostname = FN_string(0, &hostname, &dot, qualified_org);
	}
	delete qualified_org;
}

FN_searchset *
FNSP_HostnameContext::builtin_attr_search(const FN_attrset &attrs,
    FN_status_csvc &cstat)
{
	void *ip;
	FN_attribute *alias_check;
	FN_attribute *addr_check;
	unsigned int status;
	char *query = construct_query(attrs, status, alias_check, addr_check);
	FN_searchset *answer;

	if (query == NULL) {
		if (status == FN_SUCCESS)
			cstat.set_success();
		else
			cstat.set_error(status, *my_reference,
					(const unsigned char *)"");
		return (NULL);
	}

	answer = FNSP_search_host_table(*ns_impl->my_address, *my_orgname,
	    query, status);

	free(query);

	if ((alias_check != NULL || addr_check != NULL) && answer != NULL) {
		FN_searchset *tmp = answer;
		const FN_string *name;
		answer = new FN_searchset;

		if (answer == NULL) {
			cstat.set_error(FN_E_INSUFFICIENT_RESOURCES,
			    *my_reference, (const unsigned char *)"");
			return (NULL);
		}

		// further filter those with the correct alias(es)/addr(s)
		for (name = tmp->first(ip);
		    name != NULL;
		    name = tmp->next(ip)) {
			if ((alias_check == NULL ||
			    builtin_attr_exists(*name, attr_aliases,
				*alias_check, cstat)) &&
			    (addr_check == NULL ||
			    builtin_attr_exists(*name, attr_addrs, *addr_check,
				cstat))) {
				answer->add(*name);
			}
		}

		if (answer->count() == 0) {
			delete answer;
			answer = NULL;
		}

		delete tmp;
	} else {
		if (status == FN_SUCCESS)
			cstat.set_success();
		else {
			cstat.set_error(status, *my_reference,
			    (const unsigned char *)"");
		}
	}

	delete addr_check;
	delete alias_check;
	return (answer);
}

// Construct NIS+ index name that will identify entries
// with builtin attributes (values) named in 'attrs'.
// (the part of the NIS+ name between '[' and ']').
//
// e.g 	cname=mufasa,addr=109.104.40.38
// e.g. name=mufasa40,cname=mufasa
// e.g. name=jupiter
//
// All but the first value of a multivalued alias/addrs attribute
// is used in generating the query.  The rest are recorded
// for postprocessing by the caller of construct_query.
//
// Set rest_aliases to aliases in attrs not yet used
// Set rest_addrs to addresses in attrs not yet used

#define	FNS_MAXNAMELEN	1024
static char *
construct_query(const FN_attrset &attrs, unsigned int &status,
	FN_attribute *&rest_aliases, FN_attribute *&rest_addrs)
{
	char buf[FNS_MAXNAMELEN + 1];
	int all_false = 0, first_time = 1;
	void *ip, *vip;
	const FN_attribute *attr;
	const FN_attrvalue *attr_val;

	rest_aliases = NULL;
	rest_addrs = NULL;
	buf[0] = '\0';

	for (attr = attrs.first(ip); attr != NULL; attr = attrs.next(ip)) {
		attr_val = attr->first(vip);
		if (attr_val == NULL) {
			// checking for existence of attribute.
			// this is true for all entries for builtin attributes
			continue;
		}
		if (first_time == 0)
			strcat(buf, ",");  // add separator

		first_time = 0;
		if (*(attr->identifier()) == attr_name) {
			if (attr->valuecount() > 1) {
				all_false = 1;
				break;
			}
			strcat(buf, "cname=\"");
		} else if (*(attr->identifier()) == attr_addrs) {
			if (attr->valuecount() > 1) {
				// more than one address specified
				// save rest for postprocessing
				rest_addrs = new FN_attribute(*attr);
				rest_addrs->remove(*attr_val);
			}
			strcat(buf, "addr=\"");
		} else if (*(attr->identifier()) == attr_aliases) {
			// If alias is composite name, take last
			// component and look in cname column;
			// otherwise, look in name column
			FN_string alias_str((const unsigned char *)
					    attr_val->contents(),
					    attr_val->length());
			FN_composite_name alias_cname(alias_str);
			void *cip;

			if (alias_cname.count() > 1) {
				const FN_string *last = alias_cname.last(cip);
				if (last == NULL) {
					// something is wrong;
					all_false = 1;
					break;
				}

				strcat(buf, "cname=\"");
				strcat(buf, (const char *)(last->str()));
				strcat(buf, "\"");

				// save all values for postprocessing
				rest_aliases = new FN_attribute(*attr);
				continue;
			}

			// default behaviour: look in 'name' column
			strcat(buf, "name=\"");
			if (attr->valuecount() > 1) {
				rest_aliases = new FN_attribute(*attr);
				rest_aliases->remove(*attr_val);
			}
		}

		strncat(buf, (const char *)(attr_val->contents()),
			attr_val->length());
		strcat(buf, "\"");
	}

	status = FN_SUCCESS;
	if (all_false) {
		// given impossible query
		delete rest_aliases;
		delete rest_addrs;
		rest_aliases = NULL;
		rest_addrs = NULL;
		return (NULL);
	}

	return (strdup(buf));
}
