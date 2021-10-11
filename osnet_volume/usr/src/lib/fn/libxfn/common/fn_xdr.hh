/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_XDR_HH
#define	_XFN_FN_XDR_HH

#pragma ident	"@(#)fn_xdr.hh	1.4	96/03/31 SMI"

#include <rpc/types.h>
#include <rpc/xdr.h>

#include <xfn/xfn.hh>

/*
 * xdr the FN_ref into an XDR stream
 * %%% NOTE: should probably use FN_ref_t here.
 */

bool_t xdr_FN_ref(XDR *xdrs, FN_ref **ref);

/*
 * serialize the NSReference into a buffer using and return a pointer to
 * the buffer.
 * Uses XDR to to serialize
 * %%% NOTE: should probably use FN_ref_t here.
 */

char *FN_ref_xdr_serialize(const FN_ref &ref, int &bufsize);

/*
 * deserialize the given buffer and create an FN_ref. Return a pointer
 * the FN_ref.
 * Expects the buffer to contain a serialized FN_ref that has
 * been serialized using NS_Referece_xdr_serialize
 * %%% NOTE: should probably use FN_ref_t here.
 */

FN_ref *FN_ref_xdr_deserialize(const char *buf, const int bufsize,
					    unsigned &status);

/* Function to operate on attributes */
bool_t xdr_FN_attr(XDR *xdrs, FN_attrset**);

/* serialize the attributes */
char *FN_attr_xdr_serialize(const FN_attrset& attrs, int &bufsize);

/* deserialize the attributes */
FN_attrset *FN_attr_xdr_deserialize(const char *buf,
				    const int bufsize,
				    unsigned &status);

#endif /* _XFN_FN_XDR_HH */
