/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PARSE_H
#define	_PARSE_H

#pragma ident	"@(#)parse.h	1.6	98/07/26 SMI"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _print_queue {
	ns_bsd_addr_t *binding;
	char *status;
	enum { RAW, IDLE, PRINTING, FAULTED, DISABLED } state;
	job_t **jobs;
} print_queue_t;

extern print_queue_t *parse_bsd_queue(ns_bsd_addr_t *binding, char *data,
					int len);
#ifdef __cplusplus
}
#endif

#endif	/* _PARSE_H */
