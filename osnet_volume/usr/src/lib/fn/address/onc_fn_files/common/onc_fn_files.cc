/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)onc_fn_files.cc	1.4	97/11/12 SMI"


#include <string.h>
#include <ctype.h>
#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <xfn/fn_p.hh>

extern FN_string *
fns_default_address_description(const FN_ref_addr &,
				unsigned detail,
				unsigned *more_detail,
				const FN_string *msg);

static const FN_identifier
FNSP_files_address_type((unsigned char *) "onc_fn_files");

static int
FNSP_files_address_p(const FN_ref_addr &addr)
{
	return ((*addr.type()) == FNSP_files_address_type);
}

static inline char *
context_name(unsigned context_type)
{
	char *answer;
	switch (context_type) {
	case FNSP_organization_context:
		answer = "organization";
		break;
	case FNSP_enterprise_context:
		answer = "enterprise root";
		break;
	case FNSP_hostname_context:
		answer = "hostname";
		break;
	case FNSP_username_context:
		answer = "username";
		break;
	case FNSP_site_context:
		answer = "site";
		break;
	case FNSP_service_context:
		answer = "service";
		break;
	case FNSP_nsid_context:
		answer = "namespace id";
		break;
	case FNSP_user_context:
		answer = "user";
		break;
	case FNSP_host_context:
		answer = "host";
		break;
	case FNSP_generic_context:
		answer = "generic";
		break;
	case FNSP_null_context:
		answer = "null";
		break;
	default:
		answer = "unknown";
	}
	return (answer);
}


// level of detail between
// summary = 0  (default)
// and
// complete = 10
//
// more detail, if 0, is ignored.  if non-zero, it
// is set to specify the next level of detail available
// (beyond the level specified in 'detail').  if no more
// detail is available (beyond 'detail'), it will be set
// to 'detail'.
//
// For FNSP addresses,
//	(detail)	0	1	2	>2
// --------------------------------------------------------------------------
// address type		X	X	X	X
// context type		X	X	X	X (dump
// representation type		X	X	X format)
// address length			X	X
// version			X	X	X
// directory/object name	X	X	X
//
// for null reference:		X	X	X
// (contents is another reference)
//

extern "C"
FN_string_t *
Donc_fn_files(const FN_ref_addr *addrt,
	unsigned detail, unsigned *more_detail)
{
	const FN_ref_addr addr = *(const FN_ref_addr *)addrt;
	const void *contents;
	int contents_size;
	unsigned context_type, repr_type, version;
	FN_ref *nns_ref = 0;
	FN_string *nns_desc;
	int is_FNSP_addr = FNSP_files_address_p(addr);
	unsigned FN_status = FN_SUCCESS;

	if (is_FNSP_addr)
		contents = FNSP_address_decompose(addr.data(),
		    addr.length(),
		    contents_size,
		    &context_type,
		    &repr_type,
		    &version);

	// detail > 2
	if (!is_FNSP_addr || contents == 0 || detail > 2) {
		if (more_detail)
			*more_detail = detail;

		char *msg = 0;
		if (!is_FNSP_addr)
			msg = "Not a FNSP address";
		else if (contents == 0)
			msg = "Cannot decode FNSP address";

		if (msg) {
			FN_string msg_str((unsigned char *)msg);
			return ((FN_string_t *)
				fns_default_address_description(addr, 2, 0,
				&msg_str));
		} else {
			return ((FN_string_t *)
			    fns_default_address_description(addr, 2, 0, 0));
		}
	}

	const FN_identifier *tp = addr.type();
	const unsigned char *addr_type_str = 0;
	unsigned int cs_status;
	int size = 200;
	if (tp) {
		size += tp->length(); // type string
		addr_type_str = tp->str(&cs_status);
	}
	if (contents)
		size += contents_size;

	char *buf = new char[size];
	char *bp;

	sprintf(buf, "Address type: %s\n",
		addr_type_str ? (char *)addr_type_str : "");
	bp = buf + strlen(buf);

	if (detail > 1) {
                sprintf(bp, "  length: %ld\n", addr.length());

		bp = buf + strlen(buf);
	}

	sprintf(bp, "  context type: %s\n", context_name(context_type));
	bp = buf + strlen(buf);

	if (detail > 0) {
		sprintf(bp, "  representation: %s\n  version: %d\n",
			repr_type == FNSP_merged_repr ? "merged" : "normal",
			version);
		bp = buf + strlen(buf);
	}

	if (detail > 0) {
		switch (context_type) {
		case FNSP_organization_context:
		case FNSP_hostname_context:
		case FNSP_username_context:
		case FNSP_site_context:
		case FNSP_service_context:
		case FNSP_nsid_context:
		case FNSP_generic_context:
		case FNSP_user_context:
		case FNSP_host_context:
			sprintf(bp, "  internal name: ");
			bp = buf + strlen(buf);
			FN_string *iname;
			iname = FNSP_decode_internal_name(
			    (const char *)contents, contents_size);
			if (iname) {
				contents_size = iname->bytecount();
				strncpy(bp,
					(char *)(iname->str(&cs_status)),
					contents_size);
				bp[contents_size] = '\n';
				bp += (contents_size+1);
			}
			break;

		case FNSP_null_context:
			sprintf(bp, "  contains the following reference:\n[\n");
			bp = buf + strlen(buf);
			nns_ref = FN_ref_xdr_deserialize(
			    (const char *) contents, contents_size, FN_status);
			nns_desc = nns_ref->description(detail, more_detail);
			break;
		}
	}

	// Currently, we only have 0, 1, 2, > 2 levels of detail
	if (more_detail) {
		*more_detail = detail+1;
	}

	*bp = 0;

	if (nns_ref) {
		// Append nns description to answer;
		FN_string s1((unsigned char *)buf);
		FN_string s2((unsigned char *)"]\n");
		FN_string *ans1 =
		    new FN_string(&cs_status, &s1, nns_desc, &s2, 0);
		delete nns_desc;
		delete buf;
		delete nns_ref;
		return ((FN_string_t *)(ans1));
	} else {
		FN_string *ans2 = new FN_string((unsigned char *)buf);
		delete buf;
		return ((FN_string_t *)(ans2));
	}
}
