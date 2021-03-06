/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SORT_FIELDS_H
#define	_SORT_FIELDS_H

#pragma ident	"@(#)fields.h	1.3	99/04/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <alloca.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <widec.h>

#include "types.h"
#include "utility.h"

#define	FCV_REALLOC	0x1
#define	FCV_FAIL	0x2

#define	INITIAL_COLLATION_SIZE	1024

#define	COLL_NONUNIQUE	0x0
#define	COLL_UNIQUE	0x1
#define	COLL_DATA_ONLY	0x2
#define	COLL_REVERSE	0x4

extern void field_initialize(sort_t *);

extern field_t *field_new(sort_t *);
extern void field_delete(field_t *);
extern void field_add_to_chain(field_t **, field_t *);

extern ssize_t field_convert_alpha(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);
extern ssize_t field_convert_month(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);
extern ssize_t field_convert_numeric(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);

extern int collated(line_rec_t *, line_rec_t *, ssize_t, flag_t);
extern ssize_t field_convert(field_t *, line_rec_t *, flag_t, vchar_t);

extern ssize_t field_convert_alpha_wide(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);
extern ssize_t field_convert_month_wide(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);
extern ssize_t field_convert_numeric_wide(field_t *, line_rec_t *, vchar_t,
    ssize_t, ssize_t, ssize_t);

extern int collated_wide(line_rec_t *, line_rec_t *, ssize_t, flag_t);
extern ssize_t field_convert_wide(field_t *, line_rec_t *, flag_t, vchar_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _SORT_FIELDS_H */
