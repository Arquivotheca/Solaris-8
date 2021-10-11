/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_KSYMS_H
#define	_SYS_KSYMS_H

#pragma ident	"@(#)ksyms.h	1.9	99/04/26 SMI"

#include <sys/kobj.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern krwlock_t ksyms_lock;
extern vmem_t *ksyms_arena;

extern size_t ksyms_snapshot(void (*)(const void *, void *, size_t),
	void *, size_t);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KSYMS_H */
