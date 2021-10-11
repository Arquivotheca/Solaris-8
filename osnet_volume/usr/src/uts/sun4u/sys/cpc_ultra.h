/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPC_ULTRA_H
#define	_SYS_CPC_ULTRA_H

#pragma ident	"@(#)cpc_ultra.h	1.1	99/08/15 SMI"

#include <sys/inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

extern void ultra_setpcr(uint64_t);
extern void ultra_setpic(uint64_t);
extern uint64_t ultra_getpic(void);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CPC_ULTRA_H */
