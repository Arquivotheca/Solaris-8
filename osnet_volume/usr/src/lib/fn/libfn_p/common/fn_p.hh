
/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _FN_P_HH
#define	_FN_P_HH

#pragma ident	"@(#)fn_p.hh	1.9	96/03/31 SMI"

#include <xfn/xfn.hh>

/* define this for now to enable compiles */
#define	FNS_NULLREF


/* context types */

enum FNSP_context_types {
	FNSP_organization_context = 1,
	FNSP_hostname_context = 2,
	FNSP_username_context = 3,
	FNSP_site_context = 4,
	FNSP_service_context = 5,
	FNSP_nsid_context = 6,
	FNSP_user_context = 7,
	FNSP_host_context = 8,
	FNSP_generic_context = 9,
	FNSP_null_context = 10,
	FNSP_enterprise_context = 11,
	FNSP_printername_context = 12,
	FNSP_printer_object = 13
};

/* representation types */

enum FNSP_representation_types {
	FNSP_normal_repr = 0,
	FNSP_merged_repr = 1
};

extern const FN_identifier *FNSP_reftype_from_ctxtype(
    unsigned context_type);

/* Functions for generating FNSP references */

extern FN_ref *FNSP_reference(const FN_identifier &addrType,
    const FN_identifier& ref_type,
    const FN_string& internal_name,
    unsigned context_type, unsigned repr_type,
    unsigned version = 0);

extern FN_ref *FNSP_reference(const FN_identifier &addrType,
    const FN_string& internal_name,
    unsigned context_type,
    unsigned repr_type = FNSP_normal_repr,
    unsigned version = 0);

extern FN_string *FNSP_reference_to_internal_name(const FN_ref &ref,
    unsigned *ctx = 0,
    unsigned *repr = 0,
    unsigned *version = 0);

#ifdef FNS_NULLREF
extern FN_ref *FNSP_null_context_reference_to(const FN_ref& source,
    unsigned &status);

extern FN_ref *FNSP_null_context_reference_from(
    const FN_identifier &addrType,
    const FN_ref& source,
    unsigned &status,
    unsigned context_type = FNSP_null_context,
    unsigned version = 0);
#endif


/* Functions for manipulating FNSP address formats */

extern unsigned FNSP_address_context_type(const FN_ref_addr &);

extern unsigned FNSP_address_repr_type(const FN_ref_addr &);

extern unsigned FNSP_address_version(const FN_ref_addr &);

extern FN_string *FNSP_address_to_internal_name(const FN_ref_addr &addr,
    unsigned *ctx_type = 0,
    unsigned *impl_type = 0,
    unsigned *vers = 0);

extern const void *FNSP_address_decompose(const void *contents, int oldsize,
    int &size,
    unsigned *ctx_type, unsigned *impl_type = 0,
    unsigned *version = 0);

extern void *FNSP_address_compose(unsigned context_type, unsigned repr_type,
    const void *contents, int oldsize, int &size,
    unsigned version);

extern FN_string *FNSP_decode_internal_name(const char *encoded_buf,
    int esize);

extern char *FNSP_encode_internal_name(const FN_string &internal_name,
    int &out_size);

#endif /* _FN_P_HH */
