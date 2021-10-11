/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _LIST_H
#define	_LIST_H

#pragma ident	"@(#)list.h	1.14	96/05/14 SMI"


/*
 * Includes
 */

#include "expr.h"
#include "set.h"

#include <tnf/tnfctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declarations
 */
void list_expr(spec_t *speclist_p, expr_t *expr_p);
void list_set(spec_t *speclist_p, char *setname_p);
void list_values(spec_t *speclist_p);

char *list_getattrs(tnfctl_probe_t *ref_p);

#ifdef __cplusplus
}
#endif

#endif /* _LIST_H */
