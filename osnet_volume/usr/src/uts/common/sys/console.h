/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CONSOLE_H
#define	_SYS_CONSOLE_H

#pragma ident	"@(#)console.h	1.19	98/10/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#include <sys/vnode.h>
#include <sys/taskq.h>
#include <sys/varargs.h>

extern void console_get_size(ushort_t *r, ushort_t *c,
	ushort_t *x, ushort_t *y);
/*PRINTFLIKE1*/
extern void console_printf(const char *, ...);
extern void console_vprintf(const char *, va_list);

extern vnode_t *console_vnode;
extern taskq_t *console_taskq;

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONSOLE_H */
