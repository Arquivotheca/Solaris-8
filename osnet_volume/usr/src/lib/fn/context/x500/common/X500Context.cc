/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)X500Context.cc	1.3	97/11/12 SMI"


#include <string.h>
#include <stdio.h>
#include <synch.h>	// mutex_t
#include <sys/stat.h>	// stat()
#include <ctype.h>	// isspace()
#include "X500Context.hh"
#include "XDSDUA.hh"
#include "LDAPDUA.hh"


/*
 * The X.500 context
 */


#define	X500_CONF		"/etc/fn/x500.conf"
#define	X500_ACCESS_TAG		"x500-access:"
#define	XDS_TAG			"xds"

enum access_protocol		X500Context::x500_access = NONE;

static const FN_string		slash((const unsigned char *)"/");

// serialize DUA calls
static mutex_t			x500_dua_mutex = DEFAULTMUTEX;

// XDS/XOM shared object

#if defined(__sparcv9)
static char	*LIBXOMXDS_SO = "/opt/SUNWxds/lib/sparcv9/libxomxds.so";
#else
static char	*LIBXOMXDS_SO = "/opt/SUNWxds/lib/libxomxds.so";
#endif

X500Context::X500Context(
	const FN_ref_addr	&addr,
	const FN_ref		&ref,
	unsigned int		auth,
	int			&err
) : FN_ctx_svc(auth)
{
	struct stat	statbuf;

	if (x500_access == NONE) {

		FILE	*fp = 0;
		char	*access = 0;
		char	buf[BUFSIZ];

		if ((fp = fopen(X500_CONF, "r")) != NULL) {

			while (fgets(buf, sizeof (buf), fp) != NULL) {

				if (buf[0] == '#')
					continue;

				if (access = strstr(buf, X500_ACCESS_TAG)) {
					access += sizeof (X500_ACCESS_TAG) - 1;
					while (isspace(*access))
						access++;
					break;
				}
			}
			fclose(fp);
			if (access &&
			    (strncmp(access, XDS_TAG, sizeof (XDS_TAG) - 1) ==
			    0)) {
				x500_access = DAP_ACCESS;
			} else {
				x500_access = LDAP_ACCESS;
			}
		} else {
			x500_access = LDAP_ACCESS;
			x500_trace("[X500Context] error opening %s\n",
			    X500_CONF);
		}
	}

	mutex_lock(&x500_dua_mutex);

	if ((x500_access == DAP_ACCESS) && (stat(LIBXOMXDS_SO, &statbuf) == 0))
		x500_dua = new XDSDUA(err);
	else
		x500_dua = new LDAPDUA(err);

	mutex_unlock(&x500_dua_mutex);

	if (addr.length() > 0)
		context_prefix = new FN_string((unsigned char *)addr.data(),
		    addr.length());
	else
		context_prefix = 0;

	self_reference = new FN_ref(ref);

	x500_trace("[X500Context:%s] %s\n",
	    context_prefix ? (char *)context_prefix->str() : "/",
	    authoritative ? "authoritative" : "non-authoritative");
}


X500Context::~X500Context(
)
{

	x500_trace("[~X500Context:%s]\n",
	    context_prefix ? (char *)context_prefix->str() : "/");

	mutex_lock(&x500_dua_mutex);

	delete x500_dua;

	mutex_unlock(&x500_dua_mutex);

	delete self_reference;
	delete context_prefix;
}


/*
 *	C O N T E X T    I N T E R F A C E
 */

FN_string *
X500Context::xfn_name_to_x500(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	unsigned int	status;

	if (context_prefix) {
		if (name.is_empty())
			x500_name = new FN_string(*context_prefix);
		else
			x500_name = new FN_string(&status, context_prefix,
			    &slash, &name, 0);
	} else {
		x500_name = new FN_string(&status, &slash, &name, 0);
	}

	if (! x500_name) {
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);
		return (0);
	}

	return (x500_name);
}


FN_ref *
X500Context::c_lookup(
	const FN_string	&name,
	unsigned int	lookup_flags,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	FN_ref		*ref = 0;
	int		err;

	x500_trace("X500Context::c_lookup(\"%s\")\n", name.str());

	if (name.is_empty())
		return (new FN_ref(*self_reference));

	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (ref = x500_dua->lookup(*x500_name, authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	if ((ref) && (! (lookup_flags & FN_SPI_LEAVE_TERMINAL_LINK)) &&
	    ref->is_link()) {
		cs.set_continue(*ref, *self_reference);
		x500_trace("X500Context::c_lookup: XFN link encountered\n");
		delete ref;
		ref = 0;
	}

	return (ref);
}


FN_ref *
X500Context::c_lookup_nns(
	const FN_string	&name,
	unsigned int,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	FN_ref		*ref;
	int		err;

	x500_trace("X500Context::c_lookup_nns(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (ref = x500_dua->lookup_next(*x500_name, authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (ref);
}


FN_namelist *
X500Context::c_list_names(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	FN_namelist	*nl;
	int		err;
	FN_ref		*ref;

	x500_trace("X500Context::c_list_names(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (nl = x500_dua->list_names(*x500_name, authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (nl);
}


FN_namelist *
X500Context::c_list_names_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_list_names_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


FN_bindinglist *
X500Context::c_list_bindings(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	FN_bindinglist	*bl;
	int		err;
	FN_ref		*ref;

	x500_trace("X500Context::c_list_bindings(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (bl = x500_dua->list_bindings(*x500_name, authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (bl);
}


FN_bindinglist *
X500Context::c_list_bindings_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_list_bindings_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


int
X500Context::c_bind(
	const FN_string	&name,
	const FN_ref	&,
	unsigned int,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_bind(\"%s\")\n", name.str());

	FN_ref	*ref;

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	// cannot support in X.500 (use c_attr_bind() instead)
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference, name);
	return (0);
}


int
X500Context::c_bind_nns(
	const FN_string	&name,
	const FN_ref	&ref,
	unsigned int	exclusive,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*lref;

	x500_trace("X500Context::c_bind_nns(\"%s\")\n", name.str());

	if ((lref = c_lookup(name, 1, cs)) && (lref->is_link())) {
		cs.set_continue(*lref, *self_reference);
		delete lref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->bind_next(*x500_name, ref, exclusive)) ==
	    FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_unbind(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*ref;

	x500_trace("X500Context::c_unbind(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->unbind(*x500_name)) == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_unbind_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*ref;

	x500_trace("X500Context::c_unbind_nns(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->unbind_next(*x500_name)) == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_rename(
	const FN_string		&oldname,
	const FN_composite_name	&newname,
	unsigned int		exclusive,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*ref;

	x500_trace("X500Context::c_rename(\"%s\")\n", oldname.str());

	if ((ref = c_lookup(oldname, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	// X.500 only supports rename of leaf entries
	if (newname.count() != 1) {
		cs.set_error(FN_E_ILLEGAL_NAME, *self_reference, oldname);
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(oldname, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	FN_string	*name_string = newname.string();

	if ((status_code = x500_dua->rename(*x500_name, name_string,
	    exclusive)) == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, oldname);
	}
	delete name_string;

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_rename_nns(
	const FN_string		&oldname,
	const FN_composite_name	&,
	unsigned int,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_rename_nns(\"%s\")\n", oldname.str());

	FN_ref	*ref;

	if ((ref = c_lookup(oldname, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference, oldname);
	return (0);
}


FN_ref *
X500Context::c_create_subcontext(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_create_subcontext(\"%s\")\n", name.str());

	FN_ref	*ref;

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	// cannot support in X.500 (use c_attr_create_subcontext() instead)
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference, name);
	return (0);
}


FN_ref *
X500Context::c_create_subcontext_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_create_subcontext_nns(\"%s\")\n",
	    name.str());

	FN_ref	*ref;

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference, name);
	return (0);
}


int
X500Context::c_destroy_subcontext(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_destroy_subcontext(\"%s\")\n", name.str());

	return (c_unbind(name, cs));
}


int
X500Context::c_destroy_subcontext_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_destroy_subcontext_nns(\"%s\")\n",
	    name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


/*
 *	A T T R I B U T E    I N T E R F A C E
 */

FN_attribute *
X500Context::c_attr_get(
	const FN_string		&name,
	const FN_identifier	&id,
	unsigned int		follow_link,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		err;
	FN_attribute	*attr;
	FN_ref		*ref;

	x500_trace("X500Context::c_attr_get(\"%s\")\n", name.str());

	if (follow_link) {
		if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
			cs.set_continue(*ref, *self_reference);
			delete ref;
			return (0);
		}
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (attr = x500_dua->get_attr(*x500_name, id, follow_link,
	    authoritative, err))
		cs.set_success();
	else
		cs.set_error(err, *self_reference, name);

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (attr);
}


FN_attribute *
X500Context::c_attr_get_nns(
	const FN_string		&name,
	const FN_identifier	&,
	unsigned int,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_get_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


FN_attrset *
X500Context::c_attr_get_ids(
	const FN_string	&name,
	unsigned int	follow_link,
	FN_status_csvc	&cs
)
{
	FN_string	*x500_name;
	int		err;
	FN_attrset	*attrs;
	FN_ref		*ref;

	x500_trace("X500Context::c_attr_get_ids(\"%s\")\n", name.str());

	if (follow_link) {
		if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
			cs.set_continue(*ref, *self_reference);
			delete ref;
			return (0);
		}
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (attrs = x500_dua->get_attr_ids(*x500_name, follow_link,
	    authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (attrs);
}


FN_attrset *
X500Context::c_attr_get_ids_nns(
	const FN_string	&name,
	unsigned int,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_attr_get_ids_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


FN_valuelist *
X500Context::c_attr_get_values(
	const FN_string		&name,
	const FN_identifier	&id,
	unsigned int		follow_link,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_get_values(\"%s\")\n", name.str());

	FN_attribute		*attr = c_attr_get(name, id, follow_link, cs);
	FN_valuelist_svc	*vals;

	if (! attr)
		return (0);

	if (! (vals = new FN_valuelist_svc(attr))) {
		delete attr;
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);
	}

	return (vals);
}


FN_valuelist *
X500Context::c_attr_get_values_nns(
	const FN_string		&name,
	const FN_identifier	&,
	unsigned int,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_get_values_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


FN_multigetlist *
X500Context::c_attr_multi_get(
	const FN_string		&name,
	const FN_attrset	*ids,
	unsigned int		follow_link,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		err;
	FN_multigetlist	*attrs;
	FN_ref		*ref;

	x500_trace("X500Context::c_attr_multi_get(\"%s\")\n", name.str());

	if (follow_link) {
		if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
			cs.set_continue(*ref, *self_reference);
			delete ref;
			return (0);
		}
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (attrs = x500_dua->get_attrs(*x500_name, ids, follow_link,
	    authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (attrs);
}


FN_multigetlist *
X500Context::c_attr_multi_get_nns(
	const FN_string		&name,
	const FN_attrset	*,
	unsigned int,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_multi_get_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


int
X500Context::c_attr_modify(
	const FN_string		&name,
	unsigned int		mod_op,
	const FN_attribute	&attr,
	unsigned int		follow_link,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_modify(\"%s\")\n", name.str());

	FN_attrmodlist	mod;
	FN_attrmodlist	*unexmods;

	mod.add(mod_op, attr);

	return (c_attr_multi_modify(name, mod, follow_link, &unexmods, cs));
}


int
X500Context::c_attr_modify_nns(
	const FN_string		&name,
	unsigned int,
	const FN_attribute	&,
	unsigned int,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_modify_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


int
X500Context::c_attr_multi_modify(
	const FN_string		&name,
	const FN_attrmodlist	&mods,
	unsigned int		follow_link,
	FN_attrmodlist		**unexecuted_mods,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*ref;

	x500_trace("X500Context::c_attr_multi_modify(\"%s\")\n", name.str());

	if (follow_link) {
		if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
			cs.set_continue(*ref, *self_reference);
			delete ref;
			return (0);
		}
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->modify_attrs(*x500_name, mods, follow_link,
	    unexecuted_mods)) == FN_SUCCESS) {

		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_attr_multi_modify_nns(
	const FN_string		&name,
	const FN_attrmodlist	&,
	unsigned int,
	FN_attrmodlist		**,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_multi_modify_nns(\"%s\")\n",
	    name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


/*
 *	E X T E N D E D    A T T R I B U T E    I N T E R F A C E
 */

FN_searchlist *
X500Context::c_attr_search(
	const FN_string		&name,
	const FN_attrset	*match_attrs,
	unsigned int		return_ref,
	const FN_attrset	*return_attr_ids,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		err;
	FN_searchlist	*entries;
	FN_ref		*ref;

	x500_trace("X500Context::c_attr_search(\"%s\")\n", name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (entries = x500_dua->search_attrs(*x500_name, match_attrs,
	    return_ref, return_attr_ids, authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (entries);
}


FN_searchlist *
X500Context::c_attr_search_nns(
	const FN_string		&name,
	const FN_attrset	*,
	unsigned int,
	const FN_attrset	*,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_search_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


FN_ext_searchlist *
X500Context::c_attr_ext_search(
	const FN_string		&name,
	const FN_search_control	*control,
	const FN_search_filter	*filter,
	FN_status_csvc		&cs
)
{
	FN_string		*x500_name;
	int			err;
	FN_ext_searchlist	*entries;
	FN_ref			*ref;

	x500_trace("X500Context::c_attr_ext_search(\"%s\")\n", name.str());

	if (control && control->follow_links()) {
		if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
			cs.set_continue(*ref, *self_reference);
			delete ref;
			return (0);
		}
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if (entries = x500_dua->search_attrs_ext(*x500_name, control, filter,
	    authoritative, err)) {
		cs.set_success();
	} else {
		cs.set_error(err, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return (entries);
}


FN_ext_searchlist *
X500Context::c_attr_ext_search_nns(
	const FN_string		&name,
	const FN_search_control	*,
	const FN_search_filter	*,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_ext_search_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


int
X500Context::c_attr_bind(
	const FN_string		&name,
	const FN_ref		&ref,
	const FN_attrset	*attrs,
	unsigned int		exclusive,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	int		status_code;
	FN_ref		*lref;

	x500_trace("X500Context::c_attr_bind(\"%s\")\n", name.str());

	if ((lref = c_lookup(name, 1, cs)) && (lref->is_link())) {
		cs.set_continue(*lref, *self_reference);
		delete lref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->bind_attrs(*x500_name, &ref, attrs,
	    exclusive)) == FN_SUCCESS) {
		cs.set_success();
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? 1 : 0);
}


int
X500Context::c_attr_bind_nns(
	const FN_string		&name,
	const FN_ref		&ref,
	const FN_attrset	*attrs,
	unsigned int		exclusive,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_bind_nns(\"%s\")\n", name.str());

	if (! attrs) {
		return (c_bind_nns(name, ref, exclusive, cs));
	} else {
		cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference,
		    name);
		return (0);
	}
}


FN_ref *
X500Context::c_attr_create_subcontext(
	const FN_string		&name,
	const FN_attrset	*attrs,
	FN_status_csvc		&cs
)
{
	FN_string	*x500_name;
	FN_ref		*ref = 0;
	int		status_code;

	x500_trace("X500Context::c_attr_create_subcontext(\"%s\")\n",
	    name.str());

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}
	if (! (x500_name = xfn_name_to_x500(name, cs)))
		return (0);

	mutex_lock(&x500_dua_mutex);

	if ((status_code = x500_dua->bind_attrs(*x500_name, 0, attrs, 1)) ==
	    FN_SUCCESS) {
		if (ref = x500_dua->lookup(*x500_name, authoritative,
		    status_code)) {
			cs.set_success();
		} else {
			cs.set_error(status_code, *self_reference, name);
		}
	} else {
		cs.set_error(status_code, *self_reference, name);
	}

	mutex_unlock(&x500_dua_mutex);

	delete x500_name;
	return ((status_code == FN_SUCCESS) ? ref : 0);
}


FN_ref *
X500Context::c_attr_create_subcontext_nns(
	const FN_string		&name,
	const FN_attrset	*,
	FN_status_csvc		&cs
)
{
	x500_trace("X500Context::c_attr_create_subcontext_nns(\"%s\")\n",
	    name.str());

	FN_ref	*ref;

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *self_reference, name);
	return (0);
}


FN_ref *
X500Context::get_ref(
	FN_status	&s
) const
{
	x500_trace("X500Context::get_ref()\n");

	FN_ref	*ref;

	if ((ref = new FN_ref(*self_reference)) == 0) {
		s.set_code(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	s.set_success();
	return (ref);
}


FN_composite_name *
X500Context::equivalent_name(
	const FN_composite_name	&,
	const FN_string		&leading_name,
	FN_status		&s
)
{
	x500_trace("X500Context::equivalent_name(\"%s\")\n",
	    leading_name.str());

	// unsupported, for now
	s.set_code(FN_E_OPERATION_NOT_SUPPORTED);
	return (0);
}


/*
 * X.500 DN syntax: slash-separated, left-to-right, case-insensitive
 * e.g.  /C=US/O=Wiz/OU=Sales,L=West/CN=Manager
 */
FN_attrset *
X500Context::c_get_syntax_attrs(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_get_syntax_attrs(\"%s\")\n", name.str());

	FN_ref			*ref;

	if ((ref = c_lookup(name, 1, cs)) && (ref->is_link())) {
		cs.set_continue(*ref, *self_reference);
		delete ref;
		return (0);
	}

	static const FN_string	separator((unsigned char *)"/");
	static const FN_string	begin_quote((unsigned char *)"\"");
	static const FN_string	end_quote((unsigned char *)"\"");
	static const FN_string	escape((unsigned char *)"\\");
	static const FN_string	type_value_separator((unsigned char *)"=");
	static const FN_string	ava_separator((unsigned char *)",");

	static const FN_syntax_standard
				x500_syntax(FN_SYNTAX_STANDARD_DIRECTION_LTR,
				    FN_STRING_CASE_INSENSITIVE, &separator,
				    &begin_quote, &end_quote, &escape,
				    &type_value_separator, &ava_separator);

	FN_attrset		*syntax_attrs = x500_syntax.get_syntax_attrs();

	if (syntax_attrs)
		cs.set_success();
	else
		cs.set_error(FN_E_INSUFFICIENT_RESOURCES, *self_reference,
		    name);

	return (syntax_attrs);
}


FN_attrset *
X500Context::c_get_syntax_attrs_nns(
	const FN_string	&name,
	FN_status_csvc	&cs
)
{
	x500_trace("X500Context::c_get_syntax_attrs_nns(\"%s\")\n", name.str());

	FN_ref	*ref;

	if (ref = c_lookup_nns(name, 0, cs)) {
		cs.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}


/*
 * extract X.500 distinguished name
 * rule: extract first sequence of components containing '='
 */
FN_composite_name *
X500Context::p_component_parser(
	const FN_composite_name	&name,
	FN_composite_name	**rest,
	FN_status_psvc		&s
)
{
#ifdef DEBUG
	FN_string	*temp = name.string();

	x500_trace("X500Context::p_component_parser(\"%s\")\n", temp->str());

	delete temp;
#endif

	s.set_success();
	void			*position;
	FN_composite_name	*dn = new FN_composite_name();
	const FN_string		*component = name.first(position);
	const unsigned char	*c;
	const unsigned char	*n;

	// handle '_x500'
	if (name.is_empty()) {
		return (dn);
	}
	// handle '/'
	if (component->is_empty()) {
		component = name.next(position);
	}

	if (component) {
		c = component->str();


		if (c && (*c == '_')) {	// process any namespace identifiers
			n = c;

			x500_trace("X500Context::p_component_parser: %s\n", c);

			dn->append_comp(*component);
			component = name.next(position);

			if (component) {
				c = component->str();

				x500_trace("X500Context::%s: %s\n",
				    "p_component_parser", c);

				dn->append_comp(*component);
				component = name.next(position);
			}

			if (component &&
			    (strcmp((const char *)n, "_orgunit") == 0)) {
				c = component->str();

				while (c && (*c != '_')) {

					x500_trace("X500Context::%s: %s\n",
					    "p_component_parser", c);

					dn->append_comp(*component);
					component = name.next(position);
					c = (component ? component->str() : 0);
				}
			}
		}
	}

	while (c && strchr((const char *)c, '=')) {

		x500_trace("X500Context::p_component_parser: %s\n", c);

		dn->append_comp(*component);
		component = name.next(position);
		c = (component ? component->str() : 0);
	}

	if (rest) {
		if (component)
			name.prev(position);

		*rest = name.suffix(position);

#ifdef DEBUG
		if (*rest) {
			FN_string	*temp = (*rest)->string();

			x500_trace("X500Context::%s: %s: \"%s\"\n",
			    "p_component_parser", "rest", temp->str());

			delete temp;
		}
#endif
	}

	return (dn);
}
