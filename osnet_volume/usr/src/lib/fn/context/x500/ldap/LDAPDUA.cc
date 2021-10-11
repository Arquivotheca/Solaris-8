/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)LDAPDUA.cc	1.4	97/11/07 SMI"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>	// isspace()
#include <sys/time.h>	// struct timeval
#include "LDAPDUA.hh"


/*
 * X.500 Directory User Agent over the LDAP API
 */


static int		ldap_initialized = 0;
static LDAP		*ld = 0;
static struct timeval	timeout = {300, 0}; // searches timeout after 5 minutes
static long		ttl = 10 * 60; // cache entries time-to-live: 10 minutes

#define	X500_CONF		"/etc/fn/x500.conf"
#define	LDAP_SERVERS_TAG	"ldap-servers:"
#define	LDAP_DEFAULT_SERVERS	"localhost ldap"

static char	*select_ref_attrs[] = {
	"objectReferenceString",
	// "objectReferenceId",
	// "objectReferenceAddresses",
	"objectClass",
	"presentationAddress",
	0
};

static char	*select_ref_next[] = {
	"nNSReferenceString",
	// "nNSReferenceId",
	// "nNSReferenceAddresses",
	0
};


LDAPDUA::LDAPDUA(
	int	&err
)
{
	char	*bind_dn = "";
	char	*bind_pw = "";

	// connect to LDAP server
	if (ld == 0) {

		FILE	*fp = 0;
		char	buf[BUFSIZ];
		char	*servers = 0;
		char	*cp;
		int	offset;

		// parse configuration file
		if ((fp = fopen(X500_CONF, "r")) != NULL) {

			while (fgets(buf, sizeof (buf), fp) != NULL) {

				if (buf[0] == '#')
					continue;

				if (servers = strstr(buf, LDAP_SERVERS_TAG)) {
					servers +=
					    sizeof (LDAP_SERVERS_TAG) - 1;
					while (isspace(*servers))
						servers++;

					// strip spaces and newline
					offset = 0;
					for (cp = servers; *cp; cp++) {
						if (isspace(*cp) &&
						    isspace(*(cp + 1))) {
							offset++;
						} else if (*cp == '\n') {
							*(cp - offset) = '\0';
							break;
						} else {
							*(cp - offset) = *cp;
						}
					}
					break;
				}
			}
			fclose(fp);
		} else {
			servers = 0;
			x500_trace("[LDAPDUA] error opening: %s\n", X500_CONF);
		}
		servers = (servers && *servers) ? servers : NULL;

		if ((ld = ldap_open(servers, LDAP_PORT)) == NULL) {
			err = 1;
			x500_trace("[LDAPDUA] LDAP initialization error\n");
			x500_trace("[LDAPDUA] cannot connect to LDAP-server\n");
			return;

		} else {
			x500_trace("[LDAPDUA] LDAP initialized OK\n");
			x500_trace("[LDAPDUA] connected to LDAP-server\n");
		}
	}

	// bind to LDAP server
	if (ldap_initialized == 0) {

		if (ldap_bind_s(ld, bind_dn, bind_pw, (int)LDAP_AUTH_SIMPLE) !=
		    LDAP_SUCCESS) {

			x500_trace("[LDAPDUA] cannot bind to LDAP-server\n");
			err = 1;
			return;

		} else {
			x500_trace("[LDAPDUA] anonymous bind %s\n",
			    "(no credentials)");

			// only dereference aliases during name resolution
			ld->ld_deref = LDAP_DEREF_FINDING;

			// discard cached results after 10 minutes
			if (ldap_enable_cache(ld, ttl, 0) == 0) {
				x500_trace("[LDAPDUA] timeout: %ld sec, "
				    "cache: %ld sec\n", timeout.tv_sec, ttl);
			} else {
				x500_trace("[LDAPDUA] error creating cache\n");
			}
		}

	} else {
		// already connected and bound to LDAP server
		x500_trace("[LDAPDUA] already connected to LDAP-server\n");
	}
	ldap_initialized++;
}


LDAPDUA::~LDAPDUA(
)
{
	if (ldap_initialized == 1) {
		if (ldap_unbind_s(ld) == LDAP_SUCCESS) {
			x500_trace("[~LDAPDUA] %s\n",
			    "disconnected from LDAP-server");
			x500_trace("[~LDAPDUA] LDAP shutdown OK\n");
		} else {
			x500_trace("[~LDAPDUA] LDAP shutdown error\n");
		}
		ld = 0;
	} else {
		x500_trace("[~LDAPDUA]\n");
	}
	ldap_initialized--;
}


/*
 * If the specified entry exists then return a reference to it.
 */
FN_ref *
LDAPDUA::lookup(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("LDAPDUA::lookup(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// test for reference attributes
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*",
	    select_ref_attrs, 0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::lookup: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::lookup: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}

	// build reference

	FN_ref		*ref;
	unsigned char	*dn;

	dn = ldap_dn_to_string(base_dn, 0, err);
	delete [] base_dn;
	if (err != FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	if (! (ref = ldap_entry_to_ref(ld, dn, result, err))) {
		ldap_msgfree(result);
		return (0);
	}
	delete [] dn;
	ldap_msgfree(result);
	err = FN_SUCCESS;

	return (ref);
}


/*
 * If the specified entry holds a reference then return that reference.
 */
FN_ref *
LDAPDUA::lookup_next(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("LDAPDUA::lookup_next(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// test for external reference attribute
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*",
	    select_ref_next, 0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::lookup_next: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::lookup_next: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;

	// build reference
	FN_ref		*ref = 0;
	FN_attrset	*attrs = 0;
	LDAPMessage	*entry;
	char		**values;

	if (entry = ldap_first_entry(ld, result)) {

		// handle nNSReferenceString attribute
		if (values = ldap_get_values(ld, entry, "nNSReferenceString")) {

			if (! (ref = string_ref_to_ref(
			    (unsigned char *)values[0], strlen(values[0]),
			    &attrs, err))) {
				ldap_value_free(values);
				return (0);
			}
			ldap_value_free(values);

			if (attrs) {
				// %%% retrieve additional attributes
				// %%% append attributes to reference

				x500_trace("LDAPDUA::lookup_next: %s\n",
				    "additional reference attributes ignored");
			}
		} else {
			// %%% handle nNSReferenceId and nNSReferenceAddresses

			if (values = ldap_get_values(ld, entry,
			    "nNSReferenceId")) {
				ldap_value_free(values);
				err = FN_E_OPERATION_NOT_SUPPORTED;
				return (0);
			} else {
				err = FN_E_NAME_NOT_FOUND;
				ldap_msgfree(result);
				return (0);
			}
		}
	}

	ldap_msgfree(result);
	err = FN_SUCCESS;

	return (ref);
}


/*
 * Query the specified entry for its subordinates using ldap_search_st().
 */
FN_namelist *
LDAPDUA::list_names(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("LDAPDUA::list_names(\"%s\")\n", name.str());

	char		*base_dn;
	char		*attrs[2];
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// request a single attribute
	attrs[0] = "objectClass";
	attrs[1] = 0;

	// locate the children of the specified entry
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_ONELEVEL, "objectClass=*",
	    attrs, 1, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::list_names: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::list_names: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;

	// build set of names
	FN_nameset	*name_set;

	if ((name_set = new FN_nameset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, result, name_set)) != FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	ldap_msgfree(result);

	return (new FN_namelist_svc(name_set));
}


/*
 * Query the specified entry for its subordinates (and their reference
 * attributes) using ldap_search_st()
 */
FN_bindinglist *
LDAPDUA::list_bindings(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("LDAPDUA::list_bindings(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// locate the children of the specified entry
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_ONELEVEL, "objectClass=*",
	    select_ref_attrs, 0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::list_bindings: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::list_bindings: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;

	// build set of bindings
	FN_bindingset	*binding_set;

	if ((binding_set = new FN_bindingset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, result, binding_set)) !=
	    FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	ldap_msgfree(result);

	return (new FN_bindinglist_svc(binding_set));
}


/*
 * Add the supplied reference to the specified entry using ldap_modify_s()
 */
int
LDAPDUA::bind_next(
	const FN_string	&name,
	const FN_ref	&ref,
	unsigned int	exclusive
)
{
	x500_trace("LDAPDUA::bind_next(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name)))
		return (FN_E_ILLEGAL_NAME);

	// build modification
	LDAPMod		*mods[2];
	LDAPMod		replace_ref;
	char		*ref_value[2];
	int		len;

	mods[0] = &replace_ref;
	mods[1] = 0;

	replace_ref.mod_op = LDAP_MOD_REPLACE;
	replace_ref.mod_type = "nNSReferenceString";
	replace_ref.mod_values = ref_value;

	if (! (ref_value[0] = (char *)ref_to_string_ref(&ref, &len))) {
		delete [] base_dn;
		return (FN_E_MALFORMED_REFERENCE);
	}
	ref_value[1] = 0;

	// only add new reference attribute if none already exists
	if (exclusive) {
		char	*no_attrs = 0;

		ldap_disable_cache(ld);
		// test for presence of nNSReferenceString attribute
		if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE,
		    "nNSReferenceString=*", &no_attrs, 0, &timeout, &result) !=
		    LDAP_SUCCESS) {


			if (ld->ld_errno == LDAP_NO_SUCH_ATTRIBUTE) {
				x500_trace("LDAPDUA::bind_next: %s\n",
				    "no binding attribute");
			} else {
				x500_trace("LDAPDUA::bind_next: error: %s\n",
				    ldap_err2string(ld->ld_errno));
				if (ld->ld_matched)
					x500_trace("LDAPDUA::%s \"%s\"\n",
					    "bind_next: matched:",
					    ld->ld_matched);
				delete [] base_dn;
				ldap_msgfree(result);
				return (ldap_error_to_xfn(ld->ld_errno));
			}
		} else {
			// if binding already present, return error
			if (ldap_count_entries(ld, result) > 0) {
				x500_trace("LDAPDUA::bind_next: %s\n",
				    "binding already present");
				delete [] base_dn;
				ldap_msgfree(result);
				return (FN_E_NAME_IN_USE);
			} else {
				x500_trace("LDAPDUA::bind_next: %s\n",
				    "no binding present");
			}
		}
		ldap_msgfree(result);
	}

	// add the reference attribute
	if (ldap_modify_s(ld, base_dn, mods) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::bind_next: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::bind_next: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		delete [] ref_value[0];
		return (ldap_error_to_xfn(ld->ld_errno));
	}
	x500_trace("LDAPDUA::bind_next: binding added\n");
	ldap_uncache_entry(ld, base_dn);
	delete [] base_dn;
	delete [] ref_value[0];

	return (FN_SUCCESS);
}


/*
 * Remove the specified entry using ldap_delete_s()
 */
int
LDAPDUA::unbind(
	const FN_string	&name
)
{
	x500_trace("LDAPDUA::unbind(\"%s\")\n", name.str());

	char		*base_dn;

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name)))
		return (FN_E_ILLEGAL_NAME);

	// remove the entry
	if (ldap_delete_s(ld, base_dn) != LDAP_SUCCESS) {

		if (ld->ld_errno != LDAP_NO_SUCH_OBJECT) {
			x500_trace("LDAPDUA::unbind: error: %s\n",
			    ldap_err2string(ld->ld_errno));
			if (ld->ld_matched)
				x500_trace("LDAPDUA::unbind: matched: \"%s\"\n",
				    ld->ld_matched);
			delete [] base_dn;
			return (ldap_error_to_xfn(ld->ld_errno));
		}

		// if parent entry exists, return success
		LDAPMessage	*result;
		unsigned char	*parent_dn = get_next_ldap_rdn(base_dn);
		char		*no_attrs = 0;

		x500_trace("LDAPDUA::unbind: test if parent exists (%s)\n",
			    parent_dn);

		ldap_disable_cache(ld);
		if (ldap_search_st(ld, (char *)parent_dn, LDAP_SCOPE_BASE,
		    "objectClass=*", &no_attrs, 0, &timeout, &result) !=
		    LDAP_SUCCESS) {

			x500_trace("LDAPDUA::unbind: error: %s\n",
			    ldap_err2string(ld->ld_errno));
			if (ld->ld_matched)
				x500_trace("LDAPDUA::unbind: matched: \"%s\"\n",
				    ld->ld_matched);
			delete [] base_dn;
			ldap_msgfree(result);
			return (FN_E_NAME_NOT_FOUND);
		} else {
			x500_trace("LDAPDUA::unbind: found parent entry\n");
		}
		ldap_msgfree(result);
	}
	x500_trace("LDAPDUA::unbind: entry removed\n");
	delete [] base_dn;

	return (FN_SUCCESS);
}


/*
 * Remove reference from the specified entry using ldap_modify_s()
 */
int
LDAPDUA::unbind_next(
	const FN_string	&name
)
{
	x500_trace("LDAPDUA::unbind_next(\"%s\")\n", name.str());

	char		*base_dn;
	int		err;

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name)))
		return (FN_E_ILLEGAL_NAME);

	// build modification
	LDAPMod		*mods[2];
	LDAPMod		remove_ref;

	mods[0] = &remove_ref;
	mods[1] = 0;

	remove_ref.mod_op = LDAP_MOD_DELETE;
	remove_ref.mod_type = "nNSReferenceString";
	remove_ref.mod_values = 0;

	// remove the reference attribute
	if (ldap_modify_s(ld, base_dn, mods) != LDAP_SUCCESS) {

		// if no binding present return success
		if (ld->ld_errno == LDAP_NO_SUCH_ATTRIBUTE) {
			x500_trace("LDAPDUA::unbind_next: %s\n",
			    "no binding present");
		} else {
			x500_trace("LDAPDUA::unbind_next: error: %s\n",
			    ldap_err2string(ld->ld_errno));
			if (ld->ld_matched)
				x500_trace("LDAPDUA::unbind_next: %s \"%s\"\n",
				    "matched:", ld->ld_matched);
			err = ldap_error_to_xfn(ld->ld_errno);
			delete [] base_dn;
			return (err);
		}
	}
	x500_trace("LDAPDUA::bind_next: binding removed\n");
	ldap_uncache_entry(ld, base_dn);
	delete [] base_dn;

	return (FN_SUCCESS);
}


/*
 * Rename the specified leaf entry to the supplied name using ldap_modrdn_s()
 * (new RDN must be an atomic name)
 */
int
LDAPDUA::rename(
	const FN_string	&name,
	const FN_string	*newname,
	unsigned int
)
{
	x500_trace("LDAPDUA::rename(\"%s\",\"%s\")\n", name.str(),
		newname->str());

	char		*base_dn;
	char		*new_rdn;
	int		err;

	// convert names to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name)))
		return (FN_E_ILLEGAL_NAME);

	if (! (new_rdn = (char *)newname->str())) {
		delete [] base_dn;
		return (FN_E_ILLEGAL_NAME);
	}

	// rename the specified entry

	if (ldap_modrdn_s(ld, base_dn, new_rdn) != LDAP_SUCCESS) {
		x500_trace("LDAPDUA::rename: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::rename: matched: \"%s\"\n",
			    ld->ld_matched);
		err = ldap_error_to_xfn(ld->ld_errno);
		delete [] base_dn;
		return (err);
	}
	x500_trace("LDAPDUA::rename: entry renamed\n");
	delete [] base_dn;

	return (FN_SUCCESS);
}


/*
 * Query the specified entry for the requested attribute using ldap_search_st().
 */
FN_attribute *
LDAPDUA::get_attr(
	const FN_string		&name,
	const FN_identifier	&id,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("LDAPDUA::get_attr(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;
	char		*selection[2];

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// convert id to selected attribute
	if (! (selection[0] = (char *)id_to_ldap_attr(id, err)))
		return (0);
	selection[1] = NULL;

	// retrieve the requested attribute
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*",
	    selection, 0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::get_attr: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::get_attr: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;

	// build atribute
	FN_attribute	*attr = 0;
	LDAPMessage	*entry;
	char		*attribute;
	struct berval	**values;
	BerElement	*ber;
	int		i;

	for (entry = ldap_first_entry(ld, result);
	    entry != NULL;
	    entry = ldap_next_entry(ld, entry)) {

		for (attribute = ldap_first_attribute(ld, entry, &ber);
		    attribute != NULL;
		    attribute = ldap_next_attribute(ld, entry, ber)) {

			if (values = ldap_get_values_len(ld, entry,
			    attribute)) {

				// %%% handle non-ASCII attribute values

				attr = new FN_attribute(FN_identifier(
				    FN_ID_STRING, strlen(attribute), attribute),
				    ascii);

				for (i = 0; values[i] != NULL; i++) {
					attr->add(FN_attrvalue(
					    values[i]->bv_val,
					    (size_t)(values[i]->bv_len)));
				}
				x500_trace("LDAPDUA::get_attr: "
				    "retrieved %d attribute value%s\n", i,
				    (i == 1) ? "" : "s");
				ldap_value_free_len(values);
			}
		}
	}
	ldap_msgfree(result);
	err = FN_SUCCESS;

	return (attr);
}


/*
 * Query the specified entry for all its attribute types using ds_read().
 */
FN_attrset *
LDAPDUA::get_attr_ids(
	const FN_string		&name,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("LDAPDUA::get_attr_ids(\"%s\")\n", name.str());

	char		*base_dn;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	x500_trace("LDAPDUA::get_attr_ids: requesting all attribute IDs\n");

	// retrieve the requested attributes
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*", NULL,
	    1, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::get_attr_ids: error: %s\n",
			ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::get_attr_ids: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;

	// build set of attribute IDs
	FN_attrset	*attr_set;

	if ((attr_set = new FN_attrset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, result, attr_set)) != FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	ldap_msgfree(result);

	return (attr_set);
}


/*
 * Query the specified entry for the requested attributes using ldap_search_st()
 */
FN_multigetlist *
LDAPDUA::get_attrs(
	const FN_string		&name,
	const FN_attrset	*ids,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("LDAPDUA::get_attrs(\"%s\")\n", name.str());

	char		*base_dn;
	char		**selection;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// convert ids to list of selected attributes
	if (ids) {
		if (! (selection = (char **)ids_to_ldap_attrs(ids, 0, err)))
			return (0);
	} else {
		selection = NULL;
		x500_trace("LDAPDUA::get_attrs: requesting all attributes\n");
	}

	// retrieve the requested attributes
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*",
	    selection, 0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::get_attrs: error: %s\n",
			ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::get_attrs: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		delete [] selection;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;
	delete [] selection;

	// build set of attributes
	FN_attrset	*attr_set;

	if ((attr_set = new FN_attrset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, result, attr_set)) != FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	ldap_msgfree(result);

	return (new FN_multigetlist_svc(attr_set));
}


/*
 * Modify the specified entry using ldap_modify_s() or ldap_add_s().
 */
int
LDAPDUA::modify_attrs(
	const FN_string		&name,
	const FN_attrmodlist	&mods,
	unsigned int,
	FN_attrmodlist		**unexmods
)
{
	x500_trace("LDAPDUA::modify_attrs(\"%s\")\n", name.str());

	char			*base_dn;
	LDAPMod			**mod_list;
	LDAPMessage		*result;
	LDAPMessage		*entry;
	int			err;
	int			rc;

	*unexmods = 0;

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		return (FN_E_ILLEGAL_NAME);
	}

	x500_trace("LDAPDUA::modify_attrs: retrieve X.500 attributes\n");

	ldap_disable_cache(ld);
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_BASE, "objectClass=*", NULL,
	    0, &timeout, &result) != LDAP_SUCCESS) {

		if (ld->ld_errno != LDAP_NO_SUCH_OBJECT) {
			x500_trace("LDAPDUA::modify_attrs: error: %s\n",
				ldap_err2string(ld->ld_errno));
			if (ld->ld_matched)
				x500_trace("LDAPDUA::modify_attrs: %s \"%s\"\n",
				    "matched:", ld->ld_matched);
			delete [] base_dn;
			err = ldap_error_to_xfn(ld->ld_errno);
			ldap_msgfree(result);
			return (err);
		} else {
			entry = 0;	// not present
		}
	} else {
		entry = ldap_first_entry(ld, result);
	}

	// convert mods to LDAP modification format
	if (! (mod_list = mods_to_ldap_mods(mods, ld, entry, err))) {
		x500_trace("LDAPDUA::modify_attrs: %s\n",
		    "no modifications performed");
		delete [] base_dn;
		ldap_msgfree(result);
		return (err);
	}
	ldap_msgfree(result);

	// make the specified modifications
	if (entry)
	    rc = ldap_modify_s(ld, base_dn, mod_list);
	else
	    rc = ldap_add_s(ld, base_dn, mod_list);

	if (rc != LDAP_SUCCESS) {
		x500_trace("LDAPDUA::modify_attrs: error: %s\n",
		    ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::modify_attrs: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		cleanup_ldap_mods(mod_list);
		return (ldap_error_to_xfn(ld->ld_errno));
	}
	delete [] base_dn;
	cleanup_ldap_mods(mod_list);
	ldap_uncache_entry(ld, base_dn);

	x500_trace("LDAPDUA::modify_attrs: modification(s) completed\n");

	return (FN_SUCCESS);
}


/*
 * Perform a simple search at the specified entry and return the requested
 * attributes and/or references using ldap_search_st().
 */
FN_searchlist *
LDAPDUA::search_attrs(
	const FN_string		&name,
	const FN_attrset	*match_attrs,
	unsigned int		return_ref,
	const FN_attrset	*attr_ids,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("LDAPDUA::search_attrs(\"%s\")\n", name.str());

	char		*base_dn;
	char		*filter;
	char		**selection;
	LDAPMessage	*result;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// convert attributes to filter
	if (! (filter = (char *)attrs_to_ldap_filter(match_attrs, err))) {
		delete [] base_dn;
		return (0);
	}

	if (attr_ids) {
		// convert attribute IDs to list of selected attributes
		// (append reference attributes, if necessary)
		if (! (selection = (char **)ids_to_ldap_attrs(attr_ids,
		    return_ref ? select_ref_attrs : 0, err))) {
			delete [] base_dn;
			delete [] filter;
			return (0);
		}
	} else {
		selection = NULL;
		x500_trace("LDAPDUA::search_attrs: %s\n",
		    "requesting all attributes");
	}

	// retrieve the requested attributes and/or references
	if (ldap_search_st(ld, base_dn, LDAP_SCOPE_ONELEVEL, filter, selection,
	    0, &timeout, &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::search_attrs: error: %s\n",
			ldap_err2string(ld->ld_errno));
		if (ld->ld_matched)
			x500_trace("LDAPDUA::search_attrs: matched: \"%s\"\n",
			    ld->ld_matched);
		delete [] base_dn;
		delete [] filter;
		delete [] selection;
		err = ldap_error_to_xfn(ld->ld_errno);
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;
	delete [] filter;
	delete [] selection;

	// build set of entries with attributes and/or references
	FN_searchset	*search_set;

	if ((search_set = new FN_searchset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, result, return_ref, search_set)) !=
	    FN_SUCCESS) {
		ldap_msgfree(result);
		return (0);
	}
	ldap_msgfree(result);

	return (new FN_searchlist_svc(search_set));
}


/*
 * Perform an extended search below the specified entry and return the
 * requested attributes and/or references using ldap_search_st().
 *
 * NOTE: the XFN search-control 'follow_links' also controls whether
 *       X.500 aliases are dereferenced following name resolution.
 */
FN_ext_searchlist *
LDAPDUA::search_attrs_ext(
	const FN_string		&name,
	const FN_search_control	*control,
	const FN_search_filter	*sfilter,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("LDAPDUA::search_attrs_ext(\"%s\")\n", name.str());

	char		*base_dn;
	int		follow_links;
	int		scope;
	char		*filter;
	unsigned int	return_ref;
	char		**selection;
	LDAPMessage	*result;
	int		deref_setting;
	unsigned int	partial = 0;

	if (authoritative)
		ldap_disable_cache(ld);
	else
		ldap_enable_cache(ld, ttl, 0);

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	if (! (extract_search_controls(ld, control, scope, follow_links,
	    return_ref, selection, select_ref_attrs, err))) {
		delete [] base_dn;
		return (0);
	}

	// convert filter to LDAP filter string
	if (! (filter = (char *)filter_to_string(sfilter, err))) {
		delete [] base_dn;
		delete [] selection;
		return (0);
	}

	// overload the XFN 'follow_links' search-control
	//
	// if follow_links is zero:
	//	do not dereference X.500 aliases
	// if follow_links is non-zero:
	//	dereference X.500 aliases
	//
	deref_setting = ld->ld_deref;
	if (follow_links) {
		ld->ld_deref = LDAP_DEREF_ALWAYS;
	}

	// retrieve the requested attributes and/or references
	if (ldap_search_st(ld, base_dn, scope, filter, selection, 0, &timeout,
	    &result) != LDAP_SUCCESS) {

		x500_trace("LDAPDUA::search_attrs_ext: error: %s\n",
			ldap_err2string(ld->ld_errno));

		if (ld->ld_errno != LDAP_SIZELIMIT_EXCEEDED) {
			if (ld->ld_matched)
				x500_trace("LDAPDUA::%s matched: \"%s\"\n",
				    "search_attrs_ext:", ld->ld_matched);
			delete [] base_dn;
			delete [] selection;
			delete [] filter;
			err = ldap_error_to_xfn(ld->ld_errno);
			ldap_msgfree(result);
			ld->ld_deref = deref_setting;
			return (0);
		} else {
			partial = 1;
		}
	}
	delete [] selection;
	delete [] filter;
	ld->ld_deref = deref_setting;

	// build set of entries with attributes and/or references
	FN_ext_searchset	*ext_search_set;

	if ((ext_search_set = new FN_ext_searchset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		delete [] base_dn;
		ldap_msgfree(result);
		return (0);
	}

	if ((err = ldap_entries_to_set(ld, base_dn, result, return_ref,
	    ext_search_set)) != FN_SUCCESS) {
		delete [] base_dn;
		ldap_msgfree(result);
		return (0);
	}
	delete [] base_dn;
	ldap_msgfree(result);

	return (new FN_ext_searchlist_svc(
	    ext_search_set, partial ? FN_E_PARTIAL_RESULT : FN_SUCCESS));
}


/*
 * Create the specified entry with the supplied reference and/or attributes
 * using ldap_modify_s()
 */
int
LDAPDUA::bind_attrs(
	const FN_string		&name,
	const FN_ref		*ref,
	const FN_attrset	*attrs,
	unsigned int		exclusive
)
{
	x500_trace("LDAPDUA::bind_attrs(\"%s\")\n", name.str());

	FN_attrmodlist	*mods = 0;
	char		*base_dn;
	LDAPMod		**mod_list;
	int		err;

	// convert name to LDAP distinguished name format
	if (! (base_dn = (char *)string_to_ldap_dn(name))) {
		err = FN_E_ILLEGAL_NAME;
		return (0);
	}

	// convert reference and/or attributes to XFN modifications
	// convert XFN modifications to LDAP modification format
	if ((! (mods = attrs_and_ref_to_mods(attrs, ref, err))) &&
	    (! (mod_list = mods_to_ldap_mods(*mods, ld, 0, err)))) {
		x500_trace("LDAPDUA::bind_attrs: no modifications performed\n");
		delete [] base_dn;
		delete mods;
		return (err);
	}
	delete mods;

	// add the entry
	if (ldap_modify_s(ld, base_dn, mod_list) != LDAP_SUCCESS) {

		if (ld->ld_errno == LDAP_ALREADY_EXISTS) {

			if (! exclusive) {
				x500_trace("LDAPDUA::bind_attrs: %s\n",
				    "entry already exists");
				delete [] base_dn;
				cleanup_ldap_mods(mod_list);
				return (FN_E_NAME_IN_USE);
			}

			// %%% if attrs is 0 should add ref not replace entry

			// remove existing entry
			if (ldap_delete_s(ld, base_dn) != LDAP_SUCCESS) {
				x500_trace("LDAPDUA::bind_attrs: error: %s\n",
				    ldap_err2string(ld->ld_errno));
				if (ld->ld_matched)
					x500_trace("LDAPDUA::%s: \"%s\"\n",
					    "bind_attrs: matched",
					    ld->ld_matched);
				delete [] base_dn;
				cleanup_ldap_mods(mod_list);
				return (ldap_error_to_xfn(ld->ld_errno));
			}

			// add new entry
			if (ldap_modify_s(ld, base_dn, mod_list) !=
			    LDAP_SUCCESS) {
				x500_trace("LDAPDUA::bind_attrs: error: %s\n",
				    ldap_err2string(ld->ld_errno));
				if (ld->ld_matched)
					x500_trace("LDAPDUA::%s: \"%s\"\n",
					    "bind_attrs: matched",
					    ld->ld_matched);
				delete [] base_dn;
				cleanup_ldap_mods(mod_list);
				return (ldap_error_to_xfn(ld->ld_errno));
			}
		} else {
			x500_trace("LDAPDUA::bind_attrs: error: %s\n",
			    ldap_err2string(ld->ld_errno));
			if (ld->ld_matched)
				x500_trace("LDAPDUA::bind_attrs: %s \"%s\"\n",
				    "matched:", ld->ld_matched);
			delete [] base_dn;
			cleanup_ldap_mods(mod_list);
			return (ldap_error_to_xfn(ld->ld_errno));
		}
	}
	x500_trace("LDAPDUA::bind_next: modifications completed\n");
	delete [] base_dn;
	cleanup_ldap_mods(mod_list);
	ldap_uncache_entry(ld, base_dn);

	return (FN_SUCCESS);
}
