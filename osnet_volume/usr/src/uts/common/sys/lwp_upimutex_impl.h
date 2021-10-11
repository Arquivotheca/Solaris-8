/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_UPIMUTEX_H
#define	_SYS_UPIMUTEX_H

#pragma ident	"@(#)lwp_upimutex_impl.h	1.2	99/10/25 SMI"

#include <sys/thread.h>
#include <sys/lwp.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct upimutex upimutex_t;
typedef struct upib upib_t;

struct upib {
	kmutex_t upib_lock;
	upimutex_t *upib_first;
};

struct upimutex {
	struct _kthread *upi_owner;  /* owner */
	int	upi_waiter; /* wait bit */
	upib_t *upi_upibp; /* point back to upib bucket in hash chain */
	lwp_mutex_t *upi_vaddr;   /* virtual address, i.e. user lock ptr */
	lwpchan_t  upi_lwpchan;  /* lwpchan of virtual address */
	upimutex_t *upi_nextchain; /* next in hash chain */
	upimutex_t *upi_nextowned; /* list of mutexes owned by lwp */
};

#define	UPILWPCHAN_BITS		9
#define	UPILWPCHAN_TABSIZ	(1 << UPILWPCHAN_BITS)
#define	UPIMUTEX_TABSIZE	UPILWPCHAN_TABSIZ
#define	UPILWPCHAN_HASH(lwpchan)	\
	(((uintptr_t)((lwpchan).lc_wchan0)^(uintptr_t)((lwpchan).lc_wchan)) ^ \
	(((uintptr_t)((lwpchan).lc_wchan0)^(uintptr_t)((lwpchan).lc_wchan)) >> \
	UPILWPCHAN_BITS)) & (UPIMUTEX_TABSIZE - 1)
#define	UPI_CHAIN(lwpchan)	upimutextab[UPILWPCHAN_HASH((lwpchan))]

#define	UPIMUTEX_TRY	1
#define	UPIMUTEX_BLOCK	0

#ifdef _KERNEL
extern void upimutex_cleanup();
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_UPIMUTEX_H */
