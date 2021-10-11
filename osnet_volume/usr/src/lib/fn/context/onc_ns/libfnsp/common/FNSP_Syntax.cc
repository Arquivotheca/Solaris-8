/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_Syntax.cc	1.6	96/03/31 SMI"

#include "FNSP_Syntax.hh"
#include <xfn/fn_p.hh>

/* ******************* FNSP Syntaxes ********************************** */

// Flat, case-sensitive.

static const FN_syntax_standard
    FNSP_flat_syntax(FN_SYNTAX_STANDARD_DIRECTION_FLAT,
    FN_STRING_CASE_SENSITIVE);

// Flat, case-insensitive.

static const FN_syntax_standard
FNSP_iflat_syntax(FN_SYNTAX_STANDARD_DIRECTION_FLAT,
    FN_STRING_CASE_INSENSITIVE);

static const FN_string bq((unsigned char *)"\"");
static const FN_string esc((unsigned char *)"\\");

// Dotted right-to-left (case insensitive).

static const FN_string dot_sep((unsigned char *)".");
static const FN_syntax_standard
    FNSP_dot_syntax(FN_SYNTAX_STANDARD_DIRECTION_RTL,
    FN_STRING_CASE_INSENSITIVE, &dot_sep, &esc, &bq);

// Slash-separated, left-to-right (case sensitive).

static const FN_string slash_sep((unsigned char *)"/");
static const FN_syntax_standard
    FNSP_slash_syntax(FN_SYNTAX_STANDARD_DIRECTION_LTR,
    FN_STRING_CASE_SENSITIVE, &slash_sep, &esc, &bq);

// Returns syntax associated with given FNSP context type

const FN_syntax_standard *
FNSP_Syntax(unsigned context_type)
{
	const FN_syntax_standard *answer = 0;

	switch (context_type) {
	case FNSP_organization_context:
	case FNSP_site_context:
		answer = &FNSP_dot_syntax;    // insensitive, right-to-left dot
		break;
	case FNSP_enterprise_context:
	case FNSP_hostname_context:
	case FNSP_user_context:
	case FNSP_host_context:
	case FNSP_nsid_context:
		answer = &FNSP_iflat_syntax;  // insensitive flat
		break;
	case FNSP_null_context:		// no syntax
	case FNSP_username_context:
		answer = &FNSP_flat_syntax;   // sensitive flat
		break;
	case FNSP_service_context:
	case FNSP_generic_context:
	case FNSP_printername_context:
	case FNSP_printer_object:
		answer = &FNSP_slash_syntax;  // sensitive, left-to-right slash
		break;
	default:
		answer = 0;
	}
	return (answer);
}
