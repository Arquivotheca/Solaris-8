/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_P32TON_H
#define	_P32TON_H

#pragma ident	"@(#)P32ton.h	1.2	99/11/01 SMI"

#include <sys/types.h>
#include <sys/time_impl.h>
#include <sys/regset.h>
#include <sys/signal.h>
#include <sys/auxv.h>
#include <procfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _LP64

void timestruc_32_to_n(const timestruc32_t *, timestruc_t *);
void stack32_to_n(const stack32_t *, stack_t *);
void sigaction_32_to_n(const struct sigaction32 *, struct sigaction *);
void siginfo_32_to_n(const siginfo32_t *, siginfo_t *);
void auxv_32_to_n(const auxv32_t *, auxv_t *);
void rwindow_32_to_n(const struct rwindow32 *, struct rwindow *);
void gwindows_32_to_n(const gwindows32_t *, gwindows_t *);

void prgregset_32_to_n(const prgreg32_t *, prgreg_t *);
void prfpregset_32_to_n(const prfpregset32_t *, prfpregset_t *);
void lwpstatus_32_to_n(const lwpstatus32_t *, lwpstatus_t *);
void pstatus_32_to_n(const pstatus32_t *, pstatus_t *);
void lwpsinfo_32_to_n(const lwpsinfo32_t *, lwpsinfo_t *);
void psinfo_32_to_n(const psinfo32_t *, psinfo_t *);

#endif /* _LP64 */

#ifdef	__cplusplus
}
#endif

#endif	/* _P32TON_H */
