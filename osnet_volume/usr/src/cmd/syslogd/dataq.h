/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DATAQ_H
#define	_DATAQ_H

#pragma ident	"@(#)dataq.h	1.1	96/11/04 SMI"

#include "llt.h"
typedef struct dataq_data {
	ll_t list;
	void *data;
} dataq_data_t;

typedef struct dataq_waiter {
	ll_t list;
	pthread_cond_t cv;
	int wakeup;
} dataq_waiter_t;

typedef struct dataq {
	pthread_mutex_t lock;
	int num_data;
	int num_waiters;
	llh_t data;
	llh_t waiters;
} dataq_t;
int dataq_init(dataq_t *ptr);
int dataq_enqueue(dataq_t *dataq, void *in);
int dataq_dequeue(dataq_t *dataq, void **outptr);
int dataq_destroy(dataq_t *dataq);

#endif	/* _DATAQ_H */
