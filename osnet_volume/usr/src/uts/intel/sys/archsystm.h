/*
 * Copyright (c) 1993,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#pragma ident	"@(#)archsystm.h	1.17	99/11/20 SMI"

/*
 * A selection of ISA-dependent interfaces
 */

#include <sys/regset.h>
#include <vm/seg_enum.h>
#include <vm/page.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern greg_t getfp(void);
extern int getpil(void);

extern void loadldt(int);
extern int cr0(void);
extern void setcr0(int);
extern int cr2(void);
extern int dr6(void);
extern void setdr6(int);
extern int dr7(void);

extern void sti(void);

extern void tenmicrosec(void);
extern void spinwait(int millis);

extern void restore_int_flag(int);
extern int clear_int_flag(void);

extern void int20(void);

extern unsigned char inb(int port);
extern unsigned short inw(int port);
extern unsigned long inl(int port);
extern void repinsb(int port, unsigned char *addr, int count);
extern void repinsw(int port, unsigned short *addr, int count);
extern void repinsd(int port, unsigned long *addr, int count);
extern void outb(int port, unsigned char value);
extern void outw(int port, unsigned short value);
extern void outl(int port, unsigned long value);
extern void repoutsb(int port, unsigned char *addr, int count);
extern void repoutsw(int port, unsigned short *addr, int count);
extern void repoutsd(int port, unsigned long *addr, int count);

extern void pc_reset(void);
extern void reset(void);
extern int goany(void);

extern void setgregs(klwp_t *, gregset_t);
extern void getgregs(klwp_t *, gregset_t);
extern void setfpregs(klwp_t *, fpregset_t *);
extern void getfpregs(klwp_t *, fpregset_t *);

struct fpu_ctx;

extern void fp_free(struct fpu_ctx *, int);
extern void fp_save(struct fpu_ctx *);
extern void fp_restore(struct fpu_ctx *);
extern void fp_null();

extern int instr_size(struct regs *, caddr_t *, enum seg_rw);

extern void realsigprof(int, int);

extern int enable_cbcp; /* patchable in /etc/system */

#ifdef __ia64
extern caddr_t ia64devmap(pfn_t, pgcnt_t, uint_t);
#else
extern caddr_t i86devmap(pfn_t, pgcnt_t, uint_t);
#endif
extern page_t *page_numtopp_alloc(pfn_t pfnum);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ARCHSYSTM_H */
