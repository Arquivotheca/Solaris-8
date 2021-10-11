/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _BSD_FUNCTIONS_H
#define	_BSD_FUNCTIONS_H

#pragma ident	"@(#)bsd-functions.h	1.6	98/07/26 SMI"

#ifdef __cplusplus
extern "C" {
#endif

extern int bsd_queue(ns_bsd_addr_t *binding, int format, int ac, char *av[]);
extern void clear_screen();

#ifdef __cplusplus
}
#endif


#endif /* _BSD_FUNCTIONS_H */
