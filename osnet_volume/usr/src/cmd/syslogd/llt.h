/*
 * Copyright(c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LLT_H
#define	_LLT_H

#pragma ident	"@(#)llt.h	1.2	99/04/19 SMI"

typedef struct ll {
	struct ll *n;
} ll_t;

typedef struct llh {
	ll_t *front;
	ll_t **back;
} llh_t;

void   ll_init(llh_t *head);
void   ll_enqueue(llh_t *head, ll_t *data);
void   ll_mapf(llh_t *head, void (*func)(void *));
ll_t * ll_peek(llh_t *head);
ll_t * ll_dequeue(llh_t *head);
ll_t * ll_traverse(llh_t *ptr, int (*func)(void *, void *), void *user);
int    ll_check(llh_t *head);

#endif /* _LLT_H */
