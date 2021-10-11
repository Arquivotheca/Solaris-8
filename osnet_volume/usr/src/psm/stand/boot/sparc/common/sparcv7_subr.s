/*
 * Copyright (c) 1986, 1991, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)sparcv7_subr.s	1.12	96/05/17 SMI" /* from SunOS 4.1 */

#include <sys/asm_linkage.h>
#include <sys/psr.h>

#define PSR_PIL_BIT 8

#define RAISE(level) \
	mov     %psr, %o0; \
	and     %o0, PSR_PIL, %g1; \
	cmp     %g1, (level << PSR_PIL_BIT); \
	bl,a    1f; \
	andn    %o0, PSR_PIL, %g1; \
	retl; \
	nop; \
1:      or      %g1, (level << PSR_PIL_BIT), %g1; \
	mov     %g1, %psr; \
	nop; \
	retl; \
	nop;

#define SETPRI(level) \
	mov     %psr, %o0; \
	andn    %o0, PSR_PIL, %g1; \
	or      %g1, (level << PSR_PIL_BIT), %g1; \
	mov     %g1, %psr; \
	nop; \
	retl; \
	nop;

#if defined(lint)

/* ARGSUSED */
int
splimp(void)
{ return (0); }

#else	/* lint */

	ENTRY(splimp)
	RAISE(6)
	SET_SIZE(splimp)

#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
int
splnet(void)
{ return (0); }

#else	/* lint */

	ENTRY(splnet)
	RAISE(1)
	SET_SIZE(splnet)

#endif	/* lint */

#ifdef	notdef
#if defined(lint)

/* ARGSUSED */
int
spl5(void)
{ return (0); }

/* ARGSUSED */
int
spl6(void)
{ return (0); }

#else

	ENTRY2(spl6,spl5)
	SETPRI(12)
	SET_SIZE(spl5)
	SET_SIZE(spl6)

#endif	/* lint */
#endif	/* notdef */

#if defined(lint)

/* ARGSUSED */
int
splx(int level)
{ return (0); }

#else	/* lint */

	ENTRY(splx)
	and     %o0, PSR_PIL, %g1
	mov     %psr, %o0
	andn    %o0, PSR_PIL, %g2
	or      %g2, %g1, %g2
	mov     %g2, %psr
	nop
	retl
	nop
	SET_SIZE(splx)

#endif	/* lint */
