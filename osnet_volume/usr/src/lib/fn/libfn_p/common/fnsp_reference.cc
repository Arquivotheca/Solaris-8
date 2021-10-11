/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)fnsp_reference.cc	1.11	97/04/29 SMI"


#include <string.h>
#include <malloc.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

#include <xfn/fn_p.hh>
#include <xfn/fn_xdr.hh>

// FNSP reference type strings

static const FN_identifier
FNSP_organization_reftype((unsigned char *)"onc_fn_organization");

static const FN_identifier
FNSP_hostname_reftype((unsigned char *)"onc_fn_hostname");

static const FN_identifier
FNSP_username_reftype((unsigned char *)"onc_fn_username");

static const FN_identifier
FNSP_site_reftype((unsigned char *)"onc_fn_site");

static const FN_identifier
FNSP_service_reftype((unsigned char *)"onc_fn_service");

static const FN_identifier
FNSP_nsid_reftype((unsigned char *)"onc_fn_nsid");

static const FN_identifier
FNSP_user_reftype((unsigned char *)"onc_fn_user");

static const FN_identifier
FNSP_host_reftype((unsigned char *)"onc_fn_host");

static const FN_identifier
FNSP_generic_reftype((unsigned char *)"onc_fn_generic");

static const FN_identifier
FNSP_null_reftype((unsigned char *)"onc_fn_null");

static const FN_identifier
FNSP_enterprise_reftype((unsigned char *)"onc_fn_enterprise");

static const FN_identifier
FNSP_printername_reftype((unsigned char *) "onc_fn_printername");

static const FN_identifier
FNSP_printer_reftype((unsigned char *) "onc_printers");

// Layout of FNSP address: (bytes)
//	+---+---+---+---+------------------------------------- ... ----------+
//	|ctx|rep|ver|   | object name or serialzed reference (for null refs) |
//	+---+---+---+---+------------------------------------- ... ----------+

static const int ctx_type_posn = 0;
static const int repr_type_posn = 1;
static const int version_posn = 2;
static const int addr_contents_posn = 4;
static const int minimum_addr_size = addr_contents_posn;

static inline unsigned
extract_ctx_type(const unsigned char *addrbuf)
{
	return (addrbuf[ctx_type_posn]);
}


static inline unsigned
extract_repr_type(const unsigned char *addrbuf)
{
	return (addrbuf[repr_type_posn]);
}

static inline unsigned
extract_version(const unsigned char *addrbuf)
{
	return (addrbuf[version_posn]);
}

static inline const unsigned char *
extract_addr_contents(const unsigned char *addrbuf)
{
	return (&(addrbuf[addr_contents_posn]));
}


unsigned
FNSP_address_context_type(const FN_ref_addr &addr)
{
	unsigned ctx_type = 0;

	if (addr.length() > minimum_addr_size) {
		ctx_type = extract_ctx_type(
		    (const unsigned char *)addr.data());
	}

	return (ctx_type);
}


unsigned
FNSP_address_repr_type(const FN_ref_addr &addr)
{
	unsigned repr_type = FNSP_normal_repr;

	if (addr.length() > minimum_addr_size) {
		repr_type = extract_repr_type(
		    (const unsigned char *)addr.data());
	}

	return (repr_type);
}

unsigned
FNSP_address_version(const FN_ref_addr &addr)
{
	unsigned version = 0;

	if (addr.length() > minimum_addr_size) {
		version = extract_version((const unsigned char *)addr.data());
	}

	return (version);
}

void *
FNSP_address_compose(unsigned context_type, unsigned repr_type,
    const void *contents, int oldsize, int &size,
    unsigned version)
{
	unsigned char *addrbuf;
	int i;

	size = oldsize + addr_contents_posn;
	addrbuf = new unsigned char[size];

	if (addrbuf == 0)
		return (0);

	addrbuf[ctx_type_posn] = context_type;
	addrbuf[repr_type_posn] = repr_type;
	addrbuf[version_posn] = version;
	for (i = version_posn+1; i < addr_contents_posn; i++)
		addrbuf[i] = 0;
	memcpy((void *)&(addrbuf[addr_contents_posn]), contents, oldsize);

	return ((void *)addrbuf);
}


const void *
FNSP_address_decompose(const void *contents, int oldsize, int &size,
    unsigned *ctx_type, unsigned *impl_type,
    unsigned *version)
{
	if (oldsize <= minimum_addr_size)
		return (0);

	const unsigned char *addrbuf = (const unsigned char *)contents;

	if (ctx_type)
		*ctx_type = extract_ctx_type(addrbuf);
	if (impl_type)
		*impl_type = extract_repr_type(addrbuf);
	if (version)
		*version = extract_version(addrbuf);

	size = oldsize - addr_contents_posn;

	return ((const void *) extract_addr_contents(addrbuf));
}


/* ******************** FNSP reference types ************************** */

const FN_identifier *
FNSP_reftype_from_ctxtype(unsigned context_type)
{
	const FN_identifier *answer = 0;

	switch (context_type) {
	case FNSP_organization_context:
		answer = &FNSP_organization_reftype;
		break;
	case FNSP_enterprise_context:
		answer = &FNSP_enterprise_reftype;
		break;
	case FNSP_hostname_context:
		answer = &FNSP_hostname_reftype;
		break;
	case FNSP_username_context:
		answer = &FNSP_username_reftype;
		break;
	case FNSP_site_context:
		answer = &FNSP_site_reftype;
		break;
	case FNSP_service_context:
		answer = &FNSP_service_reftype;
		break;
	case FNSP_nsid_context:
		answer = &FNSP_nsid_reftype;
		break;
	case FNSP_user_context:
		answer = &FNSP_user_reftype;
		break;
	case FNSP_host_context:
		answer = &FNSP_host_reftype;
		break;
	case FNSP_generic_context:
		answer = &FNSP_generic_reftype;
		break;
	case FNSP_null_context:
		answer = &FNSP_null_reftype;
		break;
	case FNSP_printername_context:
		answer = &FNSP_printername_reftype;
		break;
	case FNSP_printer_object:
		answer = &FNSP_printer_reftype;
		break;
	default:
		answer = 0;
	}

	return (answer);
}



/* ********************* FNSP references ******************************* */

FN_string *
FNSP_decode_internal_name(const char *encoded_buf, int esize)
{
	char *decoded_buf = 0;
	XDR xdrs;
	bool_t status;

	xdrmem_create(&xdrs, (caddr_t) encoded_buf, esize, XDR_DECODE);
	status = xdr_wrapstring(&xdrs, &decoded_buf);

	if (status == FALSE)
		return (0);

	FN_string *answer = new FN_string((unsigned char *)decoded_buf);

	free(decoded_buf);
	return (answer);
}

char *
FNSP_encode_internal_name(const FN_string &internal_name, int &out_size)
{
	XDR	xdrs;
	char	*encode_buf;
	bool_t 	status;
	const unsigned char *dstr = internal_name.str();

	// Calculate size and allocate space for result
	int esize = (int) xdr_sizeof((xdrproc_t)xdr_wrapstring, &dstr);
	encode_buf = (char *)malloc(esize);
	if (encode_buf == NULL)
		return (0);

	// XDR structure into buffer
	xdrmem_create(&xdrs, encode_buf, esize, XDR_ENCODE);
	status = xdr_wrapstring(&xdrs, (char **)&dstr);
	if (status == FALSE) {
		free(encode_buf);
		return (0);
	}

	out_size = esize;
	return (encode_buf);
}

// Construct a reference using given reference type and
// given contents as address.

static FN_ref *
FNSP_reference(const FN_identifier &addrType, const FN_identifier &refType,
    const void *contents, unsigned clen)
{
	FN_ref *ref = new FN_ref(refType);
	FN_ref_addr *address;

	address = new FN_ref_addr(addrType, clen, contents);

	if (address == 0) {
		delete ref;
		return (0);
	}
	ref->append_addr(*address);
	delete address;

	return (ref);
}

// Construct a reference using given reference type, and
// <contextType, reprType, contents> as address
//	+---+---+----------+
//	| c | r | contents |
//	+---+---+----------+
// where c = context type (byte)
//	r = representation type (byte)

static FN_ref *
FNSP_reference(const FN_identifier &addrType, const FN_identifier &refType,
    const void *contents, unsigned clen, unsigned contextType,
    unsigned reprType, unsigned version)
{
	void *addrbuf;  // use char so that we can set each byte
	int newlen;
	FN_ref *answer;

	addrbuf = FNSP_address_compose(contextType, reprType, contents, clen,
	    newlen, version);

	answer = FNSP_reference(addrType, refType, addrbuf, newlen);

	delete[] addrbuf;
	return (answer);
}


// Construct a reference using given reference type, and
// <contextTYpe, reprType, internal name> as address

FN_ref *
FNSP_reference(const FN_identifier &addrType, const FN_identifier &refType,
    const FN_string &iname,
    unsigned contextType, unsigned reprType, unsigned version)
{
	int esize;
	char *einame = FNSP_encode_internal_name(iname, esize);
	FN_ref *answer;

	answer = FNSP_reference(addrType, refType, (void *)einame, esize,
	    contextType, reprType, version);
	free(einame);
	return (answer);
}


// Construct a reference using FNSP_reference_type, and
// <contextType, FNSP_normal_repr, internal_name> as address

FN_ref *
FNSP_reference(const FN_identifier &addrType, const FN_string &iname,
    unsigned contextType,
    unsigned reprType, unsigned version)
{
	const FN_identifier *reftype = FNSP_reftype_from_ctxtype(contextType);

	if (reftype == 0)
		return (0);  // FNSP_reference should set status
	else
		return (FNSP_reference(addrType, *reftype, iname, contextType,
		    reprType, version));
}


#ifdef FNS_NULLREF
/* ************************ Null context references *************** */

// Returns whether given context type could be a null context reference
static int
FNSP_valid_null_ctxtype_p(unsigned context_type)
{
	switch (context_type) {
	case FNSP_null_context:
		return (1);
	default:
		return (0);
	}
}

// Construct a reference for an NullContext, given the nns reference (source)
// that is to comprise its contents.
// Return reference containing address:
//			+---+---+------------------------------------+
// NullContext address: | A | N | serialized form of source reference|
//			+---+---+------------------------------------+
// A = [FNSP_null_context | FNSP_user_object | FNSP_host_object];
// Reference type of the reference is generated from the given context_type.
//

FN_ref *
FNSP_null_context_reference_from(const FN_identifier &addrType,
    const FN_ref &source, unsigned &status,
    unsigned context_type, unsigned version)
{
	char *source_buf;
	int source_size;
	FN_ref *answer = 0;
	const FN_identifier *reftype;

	if (FNSP_valid_null_ctxtype_p(context_type))
		reftype = FNSP_reftype_from_ctxtype(context_type);
	else {
		// cannot assign arbitrary reference types to a null reference
		status = FN_E_MALFORMED_REFERENCE;   // ??? appropriate error?
		return (0);
	}


	source_buf = FN_ref_xdr_serialize(source, source_size);

	if (source_buf == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	answer = FNSP_reference(addrType, *reftype,
	    (const void *)source_buf,
	    source_size,
	    context_type,
	    FNSP_normal_repr,
	    version);
	free(source_buf);

	status = (answer? FN_SUCCESS : FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

// Construct a reference from deserializing the reference stored as the
// address of the given null context reference.  The address selected is the
// first one with context type FNSP_null_context, FNSP_user_object,
// or FNSP_host_object.
// Given reference with address of form:
//	+---+---+-----------------------------+
//	| E | N | serialized form of reference|
//	+---+---+-----------------------------+
// Return a reference constructed by deserializing reference.
//

FN_ref *
FNSP_null_context_reference_to(const FN_ref &source, unsigned &status)
{
	const FN_ref_addr *addr;
	FN_ref *answer = 0;
	void *iterposn;
	int i, num_addrs, newsize;
	const void *addr_contents;
	unsigned ctx_type;

	addr = source.first(iterposn);
	num_addrs = source.addrcount();

	status = FN_E_MALFORMED_REFERENCE;   // default status

	for (i = 0; i < num_addrs; i++) {
		if (addr == 0)
			addr = source.next(iterposn);

		addr_contents = FNSP_address_decompose(addr->data(),
		    addr->length(), newsize, &ctx_type);
		if (addr_contents &&
		    FNSP_valid_null_ctxtype_p(ctx_type)) {
			answer = FN_ref_xdr_deserialize(
			    (const char *) addr_contents, newsize, status);
			break;
		}
	}

	return (answer);
}
#endif

static inline
address_has_internal_name_p(unsigned context_type)
{
	switch (context_type) {
	case FNSP_enterprise_context:
	case FNSP_organization_context:
	case FNSP_hostname_context:
	case FNSP_username_context:
	case FNSP_site_context:
	case FNSP_service_context:
	case FNSP_nsid_context:
	case FNSP_user_context:
	case FNSP_host_context:
	case FNSP_generic_context:
	case FNSP_printername_context:
	case FNSP_printer_object:
		return (1);
	case FNSP_null_context:
	default:
		return (0);
	}
}


// Return internal name found in the FNSP address
// +---+---+------------------------------+
// | c | r | v |  | encoded internal name |
// +---+---+------------------------------+
FN_string *
FNSP_address_to_internal_name(const FN_ref_addr &addr,
    unsigned *ctx_type, unsigned *impl_type,
    unsigned *vers)
{
	FN_string *answer = 0;
	unsigned context_type = 0;
	unsigned repr_type = 0;
	unsigned version = 0;

	const void *addr_contents;
	int newsize;
	addr_contents = FNSP_address_decompose(addr.data(),
	    addr.length(), newsize, &context_type, &repr_type,
	    &version);
	if (addr_contents &&
	    address_has_internal_name_p(context_type)) {
		answer = FNSP_decode_internal_name(
		    (const char *)addr_contents, newsize);
	}

	if (ctx_type)
		*ctx_type = context_type;
	if (impl_type)
		*impl_type = repr_type;
	if (vers)
		*vers = version;
	return (answer);
}


// Return internal name found in first address of FNSP reference
FN_string *
FNSP_reference_to_internal_name(const FN_ref &ref,
    unsigned *ctx_type, unsigned *impl_type,
    unsigned *vers)
{
	int howmany, i;
	const FN_ref_addr *addr;
	void * iterposn;
	FN_string *answer = 0;

	// ignore reference type
	howmany = ref.addrcount();

	addr = ref.first(iterposn);
	for (i = 0; i < howmany; i++) {
		answer = FNSP_address_to_internal_name(*addr, ctx_type,
		    impl_type, vers);
		if (answer)
			break;
		else
			addr = ref.next(iterposn);
	}
	return (answer);
}
