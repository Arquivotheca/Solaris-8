/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNS_SYMBOL_HH
#define	_FNS_SYMBOL_HH

#pragma ident	"@(#)fns_symbol.hh	1.4	96/06/14 SMI"

extern void *
fns_link_symbol(const char *function_name, const char *module_name);

extern void
fns_legal_C_identifier(char *outstr, const char *instr, size_t len);

#endif /* _FNS_SYMBOL_HH */
