/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBDEVFSEVENT_H
#define	_LIBDEVFSEVENT_H

#pragma ident	"@(#)libdevfsevent.h	1.1	98/07/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/devfs_log_event.h>

/*
 * defines for libdevfsevent interface
 */
typedef struct log_event_request {
	char *event_class;
	char *event_type;
	struct log_event_request *next;
} log_event_request_t;

typedef struct event_filter	event_filter_t;	/* opaque structure */

event_filter_t *request_log_event(event_filter_t *, log_event_request_t *);
int cancel_log_event_request(event_filter_t *);
int get_log_event_tuple(log_event_tuple_t *, char **);
int get_log_event(event_filter_t *, char *, time_t *tstamp);
int log_event(int argc, log_event_tuple_t tuples[]);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBDEVFSEVENT_H */
