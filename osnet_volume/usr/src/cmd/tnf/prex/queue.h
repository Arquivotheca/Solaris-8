/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _QUEUE_H
#define	_QUEUE_H

#pragma ident	"@(#)queue.h	1.24	97/10/29 SMI"

/*
 * Includes
 */

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typedefs
 */

typedef struct queue_node queue_node_t;
struct queue_node {
	queue_node_t   *next_p;
	queue_node_t   *prev_p;
};


/*
 * Declarations
 */

boolean_t	   queue_isempty(queue_node_t * q);
queue_node_t   *queue_prepend(queue_node_t * h, queue_node_t * q);
queue_node_t   *queue_append(queue_node_t * h, queue_node_t * q);
void			queue_init(queue_node_t * q);
queue_node_t   *queue_next(queue_node_t * h, queue_node_t * q);
queue_node_t   *queue_remove(queue_node_t * q);

#ifdef __cplusplus
}
#endif

#endif	/* _QUEUE_H */
