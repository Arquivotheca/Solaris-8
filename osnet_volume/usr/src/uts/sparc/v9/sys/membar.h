/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MEMBAR_H
#define	_SYS_MEMBAR_H

#pragma ident	"@(#)membar.h	1.3	97/09/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)
extern void membar_ldld(void);
extern void membar_stld(void);
extern void membar_ldst(void);
extern void membar_stst(void);

extern void membar_ldld_ldst(void);
extern void membar_ldld_stld(void);
extern void membar_ldld_stst(void);

extern void membar_stld_ldld(void);
extern void membar_stld_ldst(void);
extern void membar_stld_stst(void);

extern void membar_ldst_ldld(void);
extern void membar_ldst_stld(void);
extern void membar_ldst_stst(void);

extern void membar_stst_ldld(void);
extern void membar_stst_stld(void);
extern void membar_stst_ldst(void);

extern void membar_lookaside(void);
extern void membar_memissue(void);
extern void membar_sync(void);
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEMBAR_H */
