/*
 * Copyright (c) 1994,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ATOMIC_PRIM_H
#define	_SYS_ATOMIC_PRIM_H

#pragma ident	"@(#)atomic_prim.h	1.10	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

#if defined(_KERNEL)
extern void  atinc_cidx_extword(longlong_t *, longlong_t *, longlong_t);
extern int   atinc_cidx_word(int *, int);
extern short atinc_cidx_hword(short *, short);

extern void rwlock_word_init(uint_t *);
extern int  rwlock_word_enter(uint_t *, int);
extern void rwlock_word_exit(uint_t *, int);
extern void rwlock_hword_init(ushort_t *);
extern int  rwlock_hword_enter(ushort_t *, int);
extern void rwlock_hword_exit(ushort_t *, int);


#endif	/* _KERNEL */

#endif	_ASM

#define	WRITER_LOCK	0x0
#define	READER_LOCK	0x1

#define	HWORD_WLOCK	0xffff
#define	WORD_WLOCK	0xffffffff

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ATOMIC_PRIM_H */
