/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ID32_H
#define	_SYS_ID32_H

#pragma ident	"@(#)id32.h	1.1	99/09/13 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void	id32_init(void);
extern uint32_t	id32_alloc(void *, int);
extern void	id32_free(uint32_t);
extern void	*id32_lookup(uint32_t);

#endif /* KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ID32_H */
