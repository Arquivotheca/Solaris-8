/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_SYNTAX_STANDARD_H
#define	_XFN_FN_SYNTAX_STANDARD_H

#pragma ident	"@(#)FN_syntax_standard.h	1.5	96/03/31 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <malloc.h>

typedef struct {
	unsigned long code_set;
	unsigned long lang_terr;
} FN_attr_syntax_locale_info_t;

typedef struct {
	size_t	num_locales;
	FN_attr_syntax_locale_info_t *locales;
} FN_attr_syntax_locales_t;

enum {
	FN_SYNTAX_STANDARD_DIRECTION_FLAT,
	FN_SYNTAX_STANDARD_DIRECTION_LTR,  /* left-to-right */
	FN_SYNTAX_STANDARD_DIRECTION_RTL   /* right-to-left */
};

typedef struct {
	unsigned int direction;
	unsigned int string_case;
	FN_string_t *component_separator;
	FN_string_t *begin_quote;
	FN_string_t *end_quote;
	FN_string_t *begin_quote2;
	FN_string_t *end_quote2;
	FN_string_t *escape;
	FN_string_t *type_separator;
	FN_string_t *ava_separator;
	FN_attr_syntax_locales_t *locales;
} FN_syntax_standard_t;

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_SYNTAX_STANDARD_H */
