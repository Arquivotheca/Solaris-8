/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _SET_H
#define	_SET_H

#pragma ident	"@(#)set.h	1.9	94/08/25 SMI"

/*
 * Includes
 */

#include <stdio.h>
#include <sys/types.h>

#include "queue.h"
#include "expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typedefs
 */

typedef struct set {
	queue_node_t	qn;
	char		   *setname_p;
	expr_t		 *exprlist_p;

}			   set_t;


/*
 * Declarations
 */

set_t		  *set(char *name, expr_t * exprlist_p);
void			set_list(void);
set_t		  *set_find(char *setname_p);
boolean_t	   set_match(set_t * set_p, const char *name, const char *keys);

#ifdef __cplusplus
}
#endif

#endif /* _SET_H */
