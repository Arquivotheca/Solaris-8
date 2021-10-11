/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _LIST_H
#define _LIST_H

#pragma ident	"@(#)list.h	1.5	98/07/22 SMI"

#include <sys/va_list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	 VFUNC_T	int (*)(void *, __va_list)	/* for casting */
#define	 COMP_T		int (*)(void *, void *)		/* for casting */

extern void **list_append(void **, void *);
extern void **list_append_unique(void **, void *, int (*)(void *, void*));
extern void **list_concatenate(void **, void **);
extern void * list_locate(void **, int (*)(void *, void *), void *);
extern int list_iterate(void **, int (*)(void *, __va_list), ...);

#ifdef __cplusplus
}
#endif

#endif /* _LIST_H */
