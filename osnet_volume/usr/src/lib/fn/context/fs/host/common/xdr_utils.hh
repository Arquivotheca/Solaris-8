/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XDR_UTILS_HH
#define	_XDR_UTILS_HH

#pragma ident	"@(#)xdr_utils.hh	1.1	94/12/05 SMI"


#include <xfn/xfn.hh>


// Maximum size of XDR-encoded data in an address of a file system reference.
#define	ADDRESS_SIZE 1040


// Read the hostname and directory out of an onc_fn_fs_host address.
// The format of the address is either "hostname:directory" or "hostname".
// In the latter case, the directory is taken to be "/".  Return 0 on error.

int fs_host_decode_addr(const FN_ref_addr &addr, FN_composite_name *&dir,
    char *&hostname);


// Construct a reference from a host name and directory name.
// Return NULL on error.

FN_ref *fs_host_encode_ref(const char *hostname, const FN_composite_name *dir);


#endif	// _XDR_UTILS_HH
