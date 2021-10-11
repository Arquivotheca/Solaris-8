/*
 * Copyright (c) 1992 - 1996, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_utils.cc	1.17	99/05/26 SMI"

#include <xfn/xfn.hh>
#include <stdlib.h> // malloc, free
#include <string.h> // memcpy, str*
#include <ctype.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "fnsp_utils.hh"


// Maximum size of XDR-encoded data in an address of a file system reference.
#define	FS_ADDR_SZ 1040

static char *
xdr_decode(const void *, size_t);

static size_t
xdr_encode(const char *, void *buf, size_t bufsz);

// For NIS and /etc files, the attribute map 
#define THISORGUNIT_ATTR_MAP	"fns_org"

FN_attrset *
FNSP_get_create_params_from_attrs(const FN_attrset *attrs,
    unsigned int &context_type,
    unsigned int &repr_type,
    FN_identifier **ref_type,
    unsigned int default_context_type)
{
	// default settings
	context_type = default_context_type;
	repr_type = FNSP_normal_repr;
	if (ref_type != NULL)
		*ref_type = NULL;

	if (attrs == NULL || attrs->count() == 0) {
		return (NULL);
	}

	FN_attrset *attr_copy = new FN_attrset (*attrs);
	if (attr_copy == NULL)
		return (NULL);

	static FN_identifier
		CTXTYPE_ATTR_ID((const unsigned char *)"fn_context_type");
	static FN_identifier
		REFTYPE_ATTR_ID((const unsigned char *)"fn_reference_type");

	// Extract context type information
	const FN_attribute *attr = attrs->get(CTXTYPE_ATTR_ID);
	if (attr) {
		void *ip;
		const FN_attrvalue *ctx_val = attr->first(ip);
		if (ctx_val) {
			unsigned int *cval =
			    (unsigned int *)(ctx_val->contents());
			context_type = *cval;
		}
		attr_copy->remove(CTXTYPE_ATTR_ID);
	}

	// Extract reference type information
	attr = attrs->get(REFTYPE_ATTR_ID);
	if (attr) {
		attr_copy->remove(REFTYPE_ATTR_ID);

		if (ref_type == NULL)
			return (attr_copy);

		void *ip;
		const FN_attrvalue *ref_val = attr->first(ip);

		if (ref_val == NULL)
			return (attr_copy);

		size_t len = ref_val->length();
		char *buf = (char *)malloc(len);

		memcpy(buf, ref_val->contents(), len);

		FN_identifier_t *reftype_val = (FN_identifier_t *)buf;
		reftype_val->contents = buf + sizeof (FN_identifier_t);

		*ref_type = new FN_identifier(*reftype_val);
		free(buf);
	}

	return (attr_copy);
}

// To obtained selected attributes form the complete set

FN_attrset *
FNSP_get_selected_attrset(const FN_attrset &wholeset,
    const FN_attrset &selection)
{
	// Filter out attributes not requested
	void *ip;
	const FN_identifier *id;
	const FN_attribute *attr, *new_attr;
	FN_attrset *subset = new FN_attrset;

	if (subset == NULL)  // allocation problems
		return (NULL);

	for (attr = selection.first(ip);
	    attr != NULL;
	    attr = selection.next(ip)) {
		id = attr->identifier();
		if (id != NULL && (new_attr = wholeset.get(*id))) {
			subset->add(*new_attr);
		}
	}

	if (subset->count() == 0) {
		delete subset;
		subset = NULL;
	}

	return (subset);
}

// returns 1 if all values in 'subset' is in 'wholeset'; 0 otherwise

int
FNSP_is_attr_subset(const FN_attribute &wholeset,
	const FN_attribute &subset)
{
	FN_attribute wholecopy(wholeset);
	const FN_attrvalue *val;
	void *ip;

	// determine each value in 'subset' is already in 'wholeset' by
	// attempting to add-exclusive value to 'wholeset'.  If add succeeds,
	// this means the value was not there before.
	for (val = subset.first(ip); val != NULL; val = subset.next(ip)) {
		if (wholecopy.add(*val, FN_OP_EXCLUSIVE) == 1)
			return (0);
	}

	return (1);
}

// Returns true if all attributes in needles are in haystack,
// and the attribute values in needles are in haystack
int
FNSP_is_attrset_subset(const FN_attrset &haystack,
	const FN_attrset &needles)
{
	const FN_attribute *needle, *hattr;
	void *ip;

	for (needle = needles.first(ip);
	    needle != 0;
	    needle = needles.next(ip)) {
		// If needle is not in haystack, or if it is in, but the
		// attribute values of needle are not in the corresponding
		// attribute in haystack, we've failed
		if ((hattr = haystack.get(*(needle->identifier()))) == NULL ||
		    FNSP_is_attr_subset(*hattr, *needle) == 0)
			return (0);
	}
	return (1);
}

// Returns 0 is not host or user context type
// Returns 1 if user context and
// Returns 2 if host context
// Returns 3 if thisorgunit context
int
FNSP_is_hostuser_ctx_type(const FNSP_Address &parent,
    const FN_string &atomic_name)
{
	unsigned ctx_type = parent.get_context_type();
	if (atomic_name.is_empty()) {
		if (ctx_type == FNSP_user_context)
			return (1);
		else if (ctx_type == FNSP_host_context)
			return (2);
	} else if (ctx_type == FNSP_username_context)
		return (1);
	else if (ctx_type == FNSP_hostname_context)
		return (2);

	// Check for this orgunit context
	const FN_string tablename = parent.get_table_name();
	if (strncmp((char *) tablename.str(), THISORGUNIT_ATTR_MAP,
	    strlen(THISORGUNIT_ATTR_MAP)) == 0)
		return (3);

	return (0);
}

int
FNSP_does_builtin_attrset_exist(const FNSP_Address &parent,
    const FN_string &atomic_name)
{
	unsigned ctx_type = parent.get_context_type();
	if (atomic_name.is_empty()) {
		if ((ctx_type == FNSP_user_context) ||
		    (ctx_type == FNSP_host_context))
			return (1);
	} else if ((ctx_type == FNSP_username_context) ||
	    (ctx_type == FNSP_hostname_context))
		return (1);
	return (0);
}

unsigned
FNSP_check_builtin_attrset(const FNSP_Address &addr,
    const FN_string &atomic_name, const FN_attrset &attrset)
{
	// Check for builtin attributes
	if (FNSP_does_builtin_attrset_exist(addr, atomic_name) == 0)
		return (FN_SUCCESS);

	if (FNSP_is_attrset_in_builtin_attrset(attrset))
		return (FN_E_OPERATION_NOT_SUPPORTED);
	else
		return (FN_SUCCESS);
}

unsigned
FNSP_check_builtin_attribute(const FNSP_Address &addr,
    const FN_string &atomic_name, const FN_attribute &attr)
{
	// Check for builtin attributes
	if (FNSP_does_builtin_attrset_exist(addr, atomic_name) == 0)
		return (FN_SUCCESS);

	if (FNSP_is_attribute_in_builtin_attrset(attr))
		return (FN_E_OPERATION_NOT_SUPPORTED);
	else
		return (FN_SUCCESS);
}


// Host builtint attributes
static const FN_identifier
    host_attr_name((unsigned char *) "onc_host_name");
static const FN_identifier
    host_attr_aliases((unsigned char *) "onc_host_aliases");
static const FN_identifier
    host_attrs((unsigned char *) "onc_host_ip_addresses");
static const FN_identifier
    ascii((unsigned char *)"fn_attr_syntax_ascii");

// Operations for builtin attribtues
FN_attrset *
FNSP_get_host_builtin_attrset(const char *hostname,
    const char *hostentry, unsigned &status)
{
	status = FN_SUCCESS;

	// Add attr_name
	FN_attribute attr_name(host_attr_name, ascii);
	FN_string string_name((unsigned char *) hostname);
	if (attr_name.add(string_name) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// Ip address
	FN_attribute attr_ip(host_attrs, ascii);
	char ip_addr[256];
	const char *host_name;
	host_name = strpbrk(hostentry, " \t\n");
	if (host_name == 0)
		host_name = (char *) hostentry + strlen(hostentry);
	strncpy(ip_addr, hostentry, (host_name - hostentry));
	ip_addr[host_name - hostentry] = '\0';
	FN_string string_ip((unsigned char *) ip_addr);
	if (attr_ip.add(string_ip) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// Host name aliases, parse the hostentry
	FN_attribute attr_aliases(host_attr_aliases, ascii);
	char alias_name[1024];
	const char *next_ptr;
	FN_string *string_alias;
	// host_name = strpbrk(hostentry, " \t");
	while ((host_name) && (*host_name != '\0')) {
		while ((*host_name == ' ') ||
		    (*host_name == '\t'))
			host_name++;
		// Copy hostname
		next_ptr = strpbrk(host_name, " \t\0");
		if (next_ptr == NULL)
			break;
		strncpy(alias_name, host_name,
		    (next_ptr - host_name));
		alias_name[next_ptr - host_name] = '\0';
		host_name = next_ptr;
		string_alias = new
		    FN_string((unsigned char *) alias_name);
		if (attr_aliases.add(*string_alias) == 0) {
			status = FN_E_INSUFFICIENT_RESOURCES;
			delete string_alias;
			return (0);
		}
		delete string_alias;
	}

	FN_attrset *builtin = new FN_attrset;
	if ((!builtin) ||
	    (builtin->add(attr_name) == 0) ||
	    (builtin->add(attr_ip) == 0) ||
	    (builtin->add(attr_aliases) == 0)) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		if (builtin)
			delete builtin;
		return (0);
	}

	return (builtin);
}

// User builtin attributes
static const FN_identifier
    user_attr_passwd((unsigned char *) "onc_unix_passwd");
static const FN_identifier
    user_attr_shadow((unsigned char *) "onc_unix_shadow");

FN_attrset *
FNSP_get_user_builtin_attrset(const char *passwdentry,
    const char *shadowentry, unsigned &status)
{
	status = FN_SUCCESS;
	FN_string *string_name;

	// Construct the attr_passwd attribute
	FN_attribute attr_passwd(user_attr_passwd, ascii);
	string_name = new FN_string((unsigned char *) passwdentry);
	if (attr_passwd.add(*string_name) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		delete string_name;
		return (0);
	}
	delete string_name;

	// Construct the attr_shadow attribute
	FN_attribute attr_shadow(user_attr_shadow, ascii);
	string_name = new FN_string((unsigned char *) shadowentry);
	if (attr_shadow.add(*string_name) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		delete string_name;
		return (0);
	}
	delete string_name;

	FN_attrset *builtin = new FN_attrset;
	if ((!builtin) ||
	    (builtin->add(attr_passwd) == 0) ||
	    (builtin->add(attr_shadow) == 0)) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		if (builtin)
			delete builtin;
		return (0);
	}
	return (builtin);
}

static const FN_identifier mail_attribute((unsigned char *) "mail");

FN_attribute *
FNSP_get_builtin_mail_attribute(const char *mailentry,
    unsigned &status)
{
	FN_attribute *attribute = new FN_attribute(mail_attribute, ascii);
	FN_string string_name((unsigned char *) mailentry);
	attribute->add(string_name);
	status = FN_SUCCESS;
	return (attribute);
}

int
FNSP_is_attribute_in_builtin_attrset(const FN_attribute &attr)
{
	const FN_identifier *attr_id = attr.identifier();
	const FN_identifier *attr_syntax = attr.syntax();

	if ((*attr_syntax) != ascii)
		return (0);
	if (((*attr_id) == host_attr_name) ||
	    ((*attr_id) == host_attr_aliases) ||
	    ((*attr_id) == host_attrs) ||
	    ((*attr_id) == user_attr_passwd) ||
	    ((*attr_id) == user_attr_shadow))
		return (1);
	else
		return (0);
}

int
FNSP_is_attrset_in_builtin_attrset(const FN_attrset &attrset)
{
	const FN_attribute *attr;
	void *ip;

	for (attr = attrset.first(ip);
	    attr != 0;
	    attr = attrset.next(ip)) {
		if (FNSP_is_attribute_in_builtin_attrset(*attr))
			return (1);
	}
	return (0);
}

unsigned
FNSP_remove_builtin_attrset(FN_attrset &attrset)
{
	attrset.remove(host_attr_name);
	attrset.remove(host_attr_aliases);
	attrset.remove(host_attrs);
	attrset.remove(user_attr_passwd);
	attrset.remove(user_attr_shadow);
	return (FN_SUCCESS);
}


// Given a line from a passwd file or map, return the user's home directory.

#define	HOMEDIR_FIELD 6	// position of home directory in a passwd entry

FN_string *
FNSP_homedir_from_passwd_entry(const char *pw_entry, unsigned &status)
{
	if (pw_entry[0] == ':' || pw_entry[0] == '\0') {
		status = FN_E_CONFIGURATION_ERROR;
		return (NULL);
	}
	const char *start;
	const char *end = pw_entry;
	int i;
	for (i = 0; i < HOMEDIR_FIELD; i++) {
		start = end + 1;
		end = strchr(start, ':');
		if (end == NULL) {
			status = FN_E_CONFIGURATION_ERROR;
			return (NULL);
		}
	}
	FN_string *homedir =
	    new FN_string((const unsigned char *)start, end - start);
	if (homedir == NULL) {
		status = FN_E_INSUFFICIENT_RESOURCES;
	}
	return (homedir);
}


// Check if ref is a user's "fs" binding.  If it is, replace the
// address data -- which contains the username -- with the name of
// the user's home directory.  If _fn_fs_user_pure has been defined with
// a nonzero value in the main program (interposing on the definition
// below), return the ref unmodified.

int _fn_fs_user_pure = 0;

void
FNSP_process_user_fs(
    const FNSP_Address &ctx_addr,
    FN_ref &ref,
    FNSP_get_homedir_fn get_homedir,
    unsigned &status)
{
	static const FN_identifier
	    FS_REFTYPE((const unsigned char *)"onc_fn_fs");
	static const FN_identifier
	    USER_FS_ADDRTYPE((const unsigned char *)"onc_fn_fs_user");

	status = FN_SUCCESS;

	if (_fn_fs_user_pure || *ref.type() != FS_REFTYPE) {
		return;
	}
	const FN_ref_addr *addr;
	void *iter;
	for (addr = ref.first(iter); addr != NULL; addr = ref.next(iter)) {
		if (*addr->type() == USER_FS_ADDRTYPE) {
			break;
		}
	}
	if (addr == NULL) {
		return;
	}
	// Found an address of type USER_FS_ADDRTYPE.

	char *username_str = xdr_decode(addr->data(), addr->length());
	if (username_str == NULL) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return;
	}
	FN_string
	    username((unsigned char *)username_str, strlen(username_str));
	free(username_str);
	FN_string *home = get_homedir(ctx_addr, username, status);
	if (home == NULL) {
		status = FN_SUCCESS;	// Return address as-is.
		return;
	}
	// XDR-encode the home directory name.
	char home_xdr[FS_ADDR_SZ];
	size_t home_xdr_sz;
	const char *home_str = (const char *)home->str();
	if (home_str == NULL ||
	    (home_xdr_sz = xdr_encode(home_str, home_xdr, FS_ADDR_SZ)) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		delete home;
		return;
	}
	delete home;
	FN_ref_addr newaddr(USER_FS_ADDRTYPE, home_xdr_sz, home_xdr);
	ref.delete_addr(iter);
	if (ref.insert_addr(iter, newaddr) != 1) {
		status = FN_E_INSUFFICIENT_RESOURCES;
	}
}

static char *
xdr_decode(const void *encoded_buf, size_t bufsz)
{
	XDR xdr;
	char *decoded_buf = NULL;

	xdrmem_create(&xdr, (caddr_t)encoded_buf, bufsz, XDR_DECODE);
	bool_t status = xdr_wrapstring(&xdr, &decoded_buf);
	xdr_destroy(&xdr);
	return (status ? decoded_buf : NULL);
}

static size_t
xdr_encode(const char *str, void *buf, size_t bufsz)
{
	XDR xdr;

	xdrmem_create(&xdr, (caddr_t)buf, bufsz, XDR_ENCODE);
	bool_t status = xdr_wrapstring(&xdr, (char **)&str);
	size_t encoded_sz = xdr_getpos(&xdr);
	xdr_destroy(&xdr);
	return (status ? encoded_sz : 0);
}

// Routines to uuencode and uudecode binary data
#define	ENC(c)	(((c) & 077) + 0x20)
#define	isvalid(octet)	(octet <= 0x40)
static char DEC(char c)
{
	if (c < 0x20)
		return (0xff);
	else
		return (((unsigned char)((c) - 0x20)) & 0x3f);
}

char *
FNSP_encode_binary(const void *refbuf, int len, unsigned &status)
{
	char *in_buf, *out_buf, *optr, *iptr;
	status = FN_E_INSUFFICIENT_RESOURCES;

	out_buf = (char *) malloc(sizeof (char)*(135 * (size_t) len/100 + 4));
	optr = out_buf;
	in_buf = (char *) malloc(sizeof (char)*(len + 6));
	iptr = in_buf;
	if ((optr == 0) || (iptr == 0))
		return (0);
	else
		status = FN_SUCCESS;

	memset((void *) iptr, 0, sizeof (char)*(len + 6));
	memcpy((void *) iptr, refbuf, len);
	memset((void *) optr, 0, sizeof (char)*(135 * (size_t) len/100 + 4));
	for (int i = 0; i < len; i += 3) {
		*(optr++) = ENC(*iptr >> 2);
		*(optr++) = ENC((*iptr << 4) & 060 |
		    (*(iptr + 1) >> 4) & 017);
		*(optr++) = ENC((*(iptr + 1) << 2) & 074 |
		    (*(iptr + 2) >> 6) & 03);
		*(optr++) = ENC(*(iptr + 2) & 077);
		iptr += 3;
	}
	*optr = '\0';
	free (in_buf);
	return (out_buf);
}

static int
FNSP_outdec(unsigned char *out, unsigned char *in, int n)
{
	unsigned char   b0 = DEC(*(in++));
	unsigned char   b1 = DEC(*(in++));
	unsigned char   b2 = DEC(*(in++));
	unsigned char   b3 = DEC(*in);

	if (!isvalid(b0)) {
		return (*(in-3));
	}
	if (!isvalid(b1)) {
		return (*(in-2));
	}

	*(out++) = (b0 << 2) | (b1 >> 4);

	if (n >= 2) {
		if (!isvalid(b2)) {
			return (*(in - 1));
		}
		*(out++) = (b1 << 4) | (b2 >> 2);
		if (n >= 3) {
			if (!isvalid(b3)) {
				return (*in);
			}
			*out = (b2 << 6) | b3;
		}
	}
	return (0x20);
}

void *
FNSP_decode_binary(const char *data, int len, unsigned &status)
{
	char *ipb, *opb, ch;
	int octets;
	char *buf = (char *) malloc(sizeof (char)*(135 * (size_t) len/100 + 4));
	if (buf == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	memset((void *) buf, 0, sizeof (char)*(135 * (size_t) len/100 + 4));
	memcpy((buf + 1), data, sizeof (char)*strlen(data));

	octets = len;
	opb = &buf[0];
	ipb = &buf[1];
	while (octets > 0) {
		if ((ch = FNSP_outdec((unsigned char *) opb,
		    (unsigned char *) ipb, octets)) != 0x20) {
			// Invalid character
			status = FN_E_CONFIGURATION_ERROR;
			free(buf);
			return (0);
		}
		ipb += 4;
		opb += 3;
		octets -= 3;
	}
	status = FN_SUCCESS;
	return (buf);
}

// Default operations
int FNSP_is_statisfying_search_filter(const FN_attrset *all,
   const FN_search_filter *filter)
{
	return (0);
}

FN_attrset *FNSP_to_simple_search_attrset(const FN_search_filter *filter)
{
	return (0);
}
