/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _EXPR_H
#define	_EXPR_H

#pragma ident  "@(#)expr.h	1.24	97/10/29 SMI"

/*
 * Includes
 */

#include <stdio.h>

#include "queue.h"
#include "spec.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typedefs
 */

typedef struct expr {
	queue_node_t	qn;
	spec_t		 *left_p;
	spec_t		 *right_p;

} expr_t;


/*
 * Declarations
 */

expr_t * expr(spec_t * left_p, spec_t * right_p);
void expr_destroy(expr_t * list_p);
expr_t * expr_list(expr_t * list_p, expr_t * item_p);
void expr_print(FILE * stream, expr_t * list_p);
boolean_t expr_match(expr_t * expr_p, const char *attrs);
expr_t * expr_dup(expr_t * list_p);

#ifdef __cplusplus
}
#endif

#endif /* _EXPR_H */
