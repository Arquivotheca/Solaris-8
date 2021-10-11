/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mach.h	1.4	96/12/04 SMI"


#ifndef _MACH_DOT_H
#define	_MACH_DOT_H
#include <sys/reg.h>
#include <sys/types.h>

#if defined(sparc)

#define	GETARG0(regset)		regset->lr_rego0
#define	GETARG1(regset)		regset->lr_rego1
#define	GETARG2(regset)		regset->lr_rego2
#define	GETARG3(regset)		regset->lr_rego3
#define	GETARG4(regset)		regset->lr_rego4
#define	GETARG5(regset)		regset->lr_rego5

#define	GETFRAME(regset)	regset->lr_rego6
#define	GETPREVPC(regset)	regset->lr_rego7

#elif defined(i386)

#define	GETARG0(regset)		(((ulong_t *)regset->lr_esp)[1])
#define	GETARG1(regset)		(((ulong_t *)regset->lr_esp)[2])
#define	GETARG2(regset)		(((ulong_t *)regset->lr_esp)[3])
#define	GETARG3(regset)		(((ulong_t *)regset->lr_esp)[4])
#define	GETARG4(regset)		(((ulong_t *)regset->lr_esp)[5])
#define	GETARG5(regset)		(((ulong_t *)regset->lr_esp)[6])

#define	GETFRAME(regset)	(regset->lr_ebp)
#define	GETPREVPC(regset)	(*(uintptr_t *)regset->lr_esp)
#else
#error	unsupported architecture!
#endif

#endif
