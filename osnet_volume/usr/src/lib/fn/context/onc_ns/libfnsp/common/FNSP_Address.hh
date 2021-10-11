/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ADDRESS_HH
#define	_FNSP_ADDRESS_HH

#pragma ident	"@(#)FNSP_Address.hh	1.5	97/08/18 SMI"

#include <xfn/fn_p.hh>

enum FNSP_implementation_type {
	FNSP_directory_impl,
	FNSP_single_table_impl,
	FNSP_shared_table_impl,	// shares table with entries
	FNSP_entries_impl,	// ctx implemented using only entries
	FNSP_null_impl
};

class FNSP_Address {
private:
	void init(const FN_string& contents, unsigned int auth);

protected:
	unsigned ctx_type;	// org | user | host ...
	unsigned repr_type;	// normal | merged
	FNSP_implementation_type impl_type;
	unsigned int access_flags;	// flags required for access
	FN_string internal_name;	// whole name
	FN_string index_name;		// index part, if any
	FN_string table_name;		// table part
					// internal_name if no ind
public:
	const FN_string& get_internal_name() const { return internal_name; }
	const FN_string& get_index_name() const { return index_name; }
	const FN_string& get_table_name() const { return table_name; }
	unsigned get_context_type() const { return ctx_type; }
	unsigned get_repr_type() const { return repr_type; }
	unsigned get_access_flags() const { return access_flags; }
	FNSP_implementation_type get_impl_type() const { return impl_type; }

	// used
	~FNSP_Address();
	FNSP_Address(const FN_ref_addr&, unsigned int auth);
	FNSP_Address(const FN_string&, unsigned ctx_type,
	    unsigned = FNSP_normal_repr, unsigned int auth = 0);
	FNSP_Address(const FNSP_Address *);

	// used by some constructors in Org and Flat
	// probably only for testing
	FNSP_Address(const FN_ref&, unsigned int auth = 0);
};

#endif /* _FNSP_ADDRESS_HH */
