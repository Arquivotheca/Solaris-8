/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_SYNTAX_HH
#define	_FNSP_SYNTAX_HH

#pragma ident "@(#)FNSP_Syntax.hh	1.2 94/08/04 SMI"

#include <xfn/fn_spi.hh>  /* for FN_syntax_standard */

// function to return syntax information of FNSP contexts
extern const FN_syntax_standard* FNSP_Syntax(unsigned context_type);


#endif /* _FNSP_SYNTAX_HH */
