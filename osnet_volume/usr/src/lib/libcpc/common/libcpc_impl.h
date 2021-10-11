/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBCPC_IMPL_H
#define	_LIBCPC_IMPL_H

#pragma ident	"@(#)libcpc_impl.h	1.1	99/08/15 SMI"

#include <inttypes.h>
#include <sys/types.h>
#include <sys/cpc_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*PRINTFLIKE2*/
extern void __cpc_error(const char *fn, const char *fmt, ...);

extern const char *__cpc_reg_to_name(int cpuver, int regno, uint8_t bits);
extern int __cpc_name_to_reg(int cpuver, int regno,
    const char *name, uint8_t *bits);

extern int __cpc(int cmd, id_t lwpid, cpc_event_t *data, int flags);

extern uint_t __cpc_workver;

#define	CPUDRV				"/devices/pseudo/cpc@0"
#define	CPUDRV_SHARED			CPUDRV":shared"

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBCPC_IMPL_H */
