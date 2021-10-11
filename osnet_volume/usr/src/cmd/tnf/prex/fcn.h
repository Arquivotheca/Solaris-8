/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _FCN_H
#define	_FCN_H

#pragma ident  "@(#)fcn.h	1.24	97/10/29 SMI"

/*
 * Includes
 */

#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typedefs
 */

typedef struct fcn {
	queue_node_t	qn;
	char		*name_p;
	char		*entry_name_p;
} fcn_t;


/*
 * Declarations
 */

void fcn(char *name_p, char *func_entry_p);
void fcn_list(void);
fcn_t *fcn_find(char *name_p);
char *fcn_findname(const char * const entry_p);

#ifdef __cplusplus
}
#endif

#endif /* _FCN_H */
