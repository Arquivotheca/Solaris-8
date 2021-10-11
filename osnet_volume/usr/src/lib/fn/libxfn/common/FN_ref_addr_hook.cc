/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ref_addr_hook.cc	1.4	96/06/27 SMI"

#include <xfn/FN_ref_addr.hh>

#include "fns_symbol.hh"
#include <sys/param.h>	/* MAXPATHNAME */
#include <string.h>	/* strcpy, strcat */

extern FN_string *
fns_default_address_description(const FN_ref_addr &,
				unsigned detail,
				unsigned *more_detail,
				const FN_string *msg);

typedef FN_string_t *(*description_func)(const FN_ref_addr_t *,
    unsigned detail, unsigned *more_detail);

#define	FROM_REF_ADDR_DESC_PREFIX	'D'

static inline void
get_description_func_name(char *fname, const unsigned char *addr_type,
    size_t len)
{
	fname[0] = FROM_REF_ADDR_DESC_PREFIX;
	fns_legal_C_identifier(&fname[1], (const char *)addr_type, len);
}


// get description of address
FN_string *
FN_ref_addr::description(unsigned int d, unsigned int *md) const
{
	const FN_identifier	*tp;
	void			*fh;
	char			mname[MAXPATHLEN], fname[MAXPATHLEN];

	const FN_ref_addr_t	*myaddr = (FN_ref_addr_t *)this;
	const unsigned char	*addr_type_str;

	if ((tp = type()) && (addr_type_str = tp->str()) &&
	    (tp->length() + sizeof ("fn_ref_addr_")) < MAXPATHLEN) {
		get_description_func_name(fname, addr_type_str, tp->length());

		// look in executable (and linked libraries)
		// and then look in loadable module -- done by fns_link_symbol
		strcpy(mname, "fn_ref_addr_");
		strcat(mname, (char *) addr_type_str);
		if (fh = fns_link_symbol(fname, mname))
		    return ((FN_string *)
			    (*((description_func)fh))(myaddr, d, md));
	}

	return ((FN_string *)fns_default_address_description(*this, d, md, 0));
}
