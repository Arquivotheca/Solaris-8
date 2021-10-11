/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _SPEC_H
#define	_SPEC_H

#pragma ident	"@(#)spec.h	1.24	97/10/29 SMI"

/*
 * Includes
 */

#include <stdio.h>
#include <libgen.h>
#include <sys/types.h>

#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typedefs
 */

typedef enum spec_type {
	SPEC_EXACT,
	SPEC_REGEXP


}			   spec_type_t;


typedef struct spec {
	queue_node_t	qn;
	char		   *str;
	spec_type_t	 type;
	char		   *regexp_p;

}			   spec_t;

typedef void
(*spec_attr_fun_t) (spec_t * spec, char *attr, char *value, void *calldatap);
typedef void
(*spec_val_fun_t) (spec_t * spec, char *value, void *calldatap);


/*
 * Globals
 */


/*
 * Declarations
 */

spec_t * spec(char *str_p, spec_type_t type);
void spec_destroy(spec_t * list_p);
void spec_print(FILE * stream, spec_t * list_p);
spec_t * spec_list(spec_t * list_p, spec_t * item_p);
void spec_attrtrav(spec_t * spec_p, char *attrs,
	spec_attr_fun_t fun, void *calldata_p);
void spec_valtrav(spec_t * spec_p, char *valstr,
	spec_val_fun_t fun, void *calldata_p);
spec_t *spec_dup(spec_t * spec_p);

#ifdef __cplusplus
}
#endif

#endif /* _SPEC_H */
