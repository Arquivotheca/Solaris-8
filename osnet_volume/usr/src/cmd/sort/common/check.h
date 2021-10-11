/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SORT_CHECK_H
#define	_SORT_CHECK_H

#pragma ident	"@(#)check.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <alloca.h>

#include "fields.h"
#include "main.h"
#include "streams.h"
#include "types.h"
#include "utility.h"

extern void check_if_sorted(sort_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SORT_CHECK_H */
