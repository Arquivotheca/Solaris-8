/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_svc.cc	1.12	96/06/27 SMI"

#include <string.h>
#include <xfn/fn_spi.hh>
#include "fn_links.hh"

static int
set_status_resolved_name(const FN_composite_name &n, FN_status &s)
{
	void *ip;
	FN_composite_name *res = new FN_composite_name();
	const FN_string *name = n.first(ip);
	const FN_string *remaining_first = 0;

	const FN_composite_name *rem = s.remaining_name();
	if (rem) {
		void *ip1;
		remaining_first = rem->first(ip1);
		while ((name) &&
		    (remaining_first->compare(*name))) {
			res->append_comp(*name);
			name = n.next(ip);
		}
		s.set_resolved_name(res);
	}
	delete res;
	return (1);
}

FN_ref*
FN_ctx_svc::lookup(const FN_composite_name &n, FN_status &s)
{
	s.set_success(); // clear all fields
	FN_status_psvc ps(s);
	FN_ref *ret = p_lookup(n, 0, ps);

	fn_process_link(s, authoritative, FN_SUCCESS, &ret);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_lookup(rn, 0, ps);
		delete rc;

		fn_process_link(s, authoritative, FN_SUCCESS, &ret);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_ref*
FN_ctx_svc::lookup_link(const FN_composite_name &n, FN_status &s)
{
	s.set_success(); // clear all fields
	FN_status_psvc ps(s);
	FN_ref *ret = p_lookup(n, FN_SPI_LEAVE_TERMINAL_LINK, ps);

	fn_process_link(s, authoritative, FN_SUCCESS, &ret);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_lookup(rn, FN_SPI_LEAVE_TERMINAL_LINK, ps);
		delete rc;
		fn_process_link(s, authoritative, FN_SUCCESS, &ret);
	}

	set_status_resolved_name(n, s);
	return (ret);
}

FN_namelist*
FN_ctx_svc::list_names(const FN_composite_name &n, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_namelist* ret = p_list_names(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_list_names(rn, ps);
		delete rc;
		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_bindinglist*
FN_ctx_svc::list_bindings(const FN_composite_name &n, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_bindinglist *ret = p_list_bindings(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_list_bindings(rn, ps);
		delete rc;
		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

int FN_ctx_svc::bind(const FN_composite_name &n,
    const FN_ref &r, unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_bind(n, r, f, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_bind(rn, r, f, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

int
FN_ctx_svc::unbind(const FN_composite_name &n, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_unbind(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_unbind(rn, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_ref*
FN_ctx_svc::create_subcontext(const FN_composite_name &n,
    FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_ref *ret = p_create_subcontext(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_create_subcontext(rn, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

int
FN_ctx_svc::destroy_subcontext(const FN_composite_name &n, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_destroy_subcontext(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_destroy_subcontext(rn, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

int
FN_ctx_svc::rename(const FN_composite_name &oldname,
    const FN_composite_name &newname, unsigned int flag, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_rename(oldname, newname, flag, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_rename(rn, newname, flag, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(oldname, s);
	return (ret);
}

FN_attrset*
FN_ctx_svc::get_syntax_attrs(const FN_composite_name &n,
    FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_attrset *ret = p_get_syntax_attrs(n, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_get_syntax_attrs(rn, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_attribute*
FN_ctx_svc::attr_get(const FN_composite_name &n,
    const FN_identifier &i, unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_attribute* ret = p_attr_get(n, i, f, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ? make_func_info_attr_get(&i, &ret) : NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_get(rn, i, f, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	delete packet;
	set_status_resolved_name(n, s);
	return (ret);
}

int
FN_ctx_svc::attr_modify(const FN_composite_name &n, unsigned int i,
    const FN_attribute &a, unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_attr_modify(n, i, a, f, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ?
	    make_func_info_attr_modify(&i, &a, &ret) : NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_modify(rn, i, a, f, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	set_status_resolved_name(n, s);
	delete packet;
	return (ret);
}

FN_valuelist *FN_ctx_svc::attr_get_values(const FN_composite_name &n,
    const FN_identifier &i, unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_valuelist *ret = p_attr_get_values(n, i, f, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ? make_func_info_attr_get_values(&i, &ret) :
	    NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_get_values(rn, i, f, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	set_status_resolved_name(n, s);
	delete packet;
	return (ret);
}

FN_attrset*
FN_ctx_svc::attr_get_ids(const FN_composite_name &n, unsigned int f,
    FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_attrset *ret = p_attr_get_ids(n, f, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ? make_func_info_attr_get_ids(&ret) : NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_get_ids(rn, f, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	set_status_resolved_name(n, s);
	delete packet;
	return (ret);
}

FN_multigetlist*
FN_ctx_svc::attr_multi_get(const FN_composite_name &n, const FN_attrset *a,
    unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_multigetlist *ret = p_attr_multi_get(n, a, f, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ? make_func_info_attr_multi_get(a, &ret) :
	    NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_multi_get(rn, a, f, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	set_status_resolved_name(n, s);
	delete packet;
	return (ret);
}

int
FN_ctx_svc::attr_multi_modify(const FN_composite_name &n,
    const FN_attrmodlist &m, unsigned int f, FN_attrmodlist **a, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_attr_multi_modify(n, m, f, a, ps);
	FN_ctx_func_info_t *packet =
	    ((ret == NULL && f) ?
	    make_func_info_attr_multi_modify(&m, a, &ret) : NULL);

	fn_attr_process_link(s, authoritative, f, packet);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_multi_modify(rn, m, f, a, ps);
		delete rc;

		fn_attr_process_link(s, authoritative, f, packet);
	}
	set_status_resolved_name(n, s);
	delete packet;
	return (ret);
}

int FN_ctx_svc::attr_bind(const FN_composite_name &n,
    const FN_ref &r, const FN_attrset *a, unsigned int f, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	int ret = p_attr_bind(n, r, a, f, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_bind(rn, r, a, f, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_ref*
FN_ctx_svc::attr_create_subcontext(const FN_composite_name &n,
    const FN_attrset *attrs, FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_ref *ret = p_attr_create_subcontext(n, attrs, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_create_subcontext(rn, attrs, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_searchlist *
FN_ctx_svc::attr_search(const FN_composite_name &n,
    const FN_attrset *a, unsigned int rf, const FN_attrset *ra,
    FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_searchlist *ret = p_attr_search(n, a, rf, ra, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_search(rn, a, rf, ra, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}

FN_ext_searchlist *
FN_ctx_svc::attr_ext_search(const FN_composite_name &n,
    const FN_search_control *ctl, const FN_search_filter *fil,
    FN_status &s)
{
	s.set_success();  // clear all fields
	FN_status_psvc ps(s);
	FN_ext_searchlist *ret = p_attr_ext_search(n, ctl, fil, ps);

	fn_process_link(s, authoritative);

	// loop on FN_E_SPI_CONTINUE (guaranteed to make progress or fail)
	while (ps.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *rc;
		FN_status rs;
		if (!(rc = from_ref(*ps.resolved_ref(), authoritative, rs))) {
			ps.set_code(rs.code());
			break;
		}
		FN_composite_name rn = *ps.remaining_name();
		ret = rc->p_attr_ext_search(rn, ctl, fil, ps);
		delete rc;

		fn_process_link(s, authoritative);
	}
	set_status_resolved_name(n, s);
	return (ret);
}


FN_ctx_svc::FN_ctx_svc() : authoritative(0)
{
	// should never be called; always use constructor with initializer
}

FN_ctx_svc::~FN_ctx_svc()
{
}

#include <stdio.h>
#include <sys/param.h>

extern void *
fns_link_symbol(const char *function_name, const char *module_name);

extern void
fns_legal_C_identifier(char *outstr, const char *instr, size_t len);

#define	FROM_REF_ADDR_PREFIX	'A'
#define	SVC_PREFIX		'S'

// cosntructor name for fn_ctx_svc_handle_from_ref_addr()
static inline void
get_constructor_func_name(char *fname, const char *addr_type, size_t len)
{
	fname[0] = FROM_REF_ADDR_PREFIX;
	fname[1] = SVC_PREFIX;
	fns_legal_C_identifier(&fname[2], addr_type, len);
}

// construct from a reference
FN_ctx_svc*
FN_ctx_svc::from_ref(const FN_ref &r, unsigned int auth, FN_status &s)
{
	// %%% need to support use of reference type for module
	// selection as well
	FN_ctx_svc_t *cp;
	FN_ctx_svc *answer;
	const FN_ref_addr *ap;
	const char *t_cstr;
	void *ip, *fh;
	char mname[MAXPATHLEN], fname[MAXPATHLEN];

	// prime status for case of no supported addresses
	s.set(FN_E_NO_SUPPORTED_ADDRESS);

	// look for supported addresses (and try them)
	for (ap = r.first(ip); ap; ap = r.next(ip)) {
		t_cstr = (const char *)(ap->type()->str());
		if (t_cstr == 0)
			continue;

		get_constructor_func_name(fname, t_cstr, ap->type()->length());

		// look in executable (and linked libraries)
		// and then look in loadable module -- done by fns_link_symbol
		strcpy(mname, "fn_ctx_");
		if (sizeof ("fn_ctx_") + strlen(t_cstr) > sizeof (mname))
			continue;
		strcat(mname, t_cstr);
		if (fh = fns_link_symbol(fname, mname)) {
			if (cp = (*((FN_ctx_svc_from_ref_addr_func)fh))
			    ((const FN_ref_addr_t *)ap, (const FN_ref_t *) &r,
			    auth, (FN_status_t *)&s)) {
				answer = (FN_ctx_svc*)cp;
				return (answer);
			}
			continue;
		}
	}
	// give up
	return (0);
}

// get initial context for FN_ctx_svc
FN_ctx_svc*
FN_ctx_svc::from_initial(unsigned int auth, FN_status &s)
{
	void *fh;
	FN_ctx_svc_t *cp;
	FN_ctx_svc *answer;
	FN_status_t *st = (FN_status_t *)&s;

	// look in executable (and linked libraries)
	// and then look in loadable module -- done by fns_link_symbol
	if (fh = fns_link_symbol("initial__fn_ctx_svc_handle_from_initial",
	    "fn_ctx_initial")) {
		cp = ((*((FN_ctx_svc_from_initial_func)fh))(auth, st));
		answer = (FN_ctx_svc*)cp;
		return (answer);
	}
	// configuration error
	s.set(FN_E_CONFIGURATION_ERROR);
	return ((FN_ctx_svc*)0);
}

// A subclass of FN_ctx_svc may provide alternate implementation for these
// By default, these are no-ops.
FN_ctx_svc_data_t *
FN_ctx_svc::p_get_ctx_svc_data()
{
	return ((FN_ctx_svc_data_t *)0);
}

int
FN_ctx_svc::p_set_ctx_svc_data(FN_ctx_svc_data_t *)
{
	return (0);
}
