/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Kernel Run-Time Linker/Loader private interfaces.
 */

#ifndef	_SYS_KOBJ_IMPL_H
#define	_SYS_KOBJ_IMPL_H

#pragma ident	"@(#)kobj_impl.h	1.25	99/05/04 SMI"

/*
 * Boot/aux vector attributes.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	BA_DYNAMIC	0
#define	BA_PHDR		1
#define	BA_PHNUM	2
#define	BA_PHENT	3
#define	BA_ENTRY	4
#define	BA_PAGESZ	5
#define	BA_LPAGESZ	6
#define	BA_LDELF	7
#define	BA_LDSHDR	8
#define	BA_LDNAME	9
#define	BA_BSS		10
#define	BA_IFLUSH	11
#define	BA_CPU		12
#define	BA_MMU		13
#define	BA_GOTADDR	14
#define	BA_NEXTGOT	15
#define	BA_NUM		16

typedef union {
	unsigned long ba_val;
	void *ba_ptr;
} val_t;

/*
 * Segment info.
 */
struct proginfo {
	uint_t size;
	uint_t align;
};

/*
 * Implementation-specific flags.
 */
#define		KOBJ_EXEC	0x04	/* executable (unix module)	*/
#define		KOBJ_INTERP	0x08	/* the interpreter module	*/
#define		KOBJ_PRIM	0x10	/* a primary kernel module	*/
#define		KOBJ_RESOLVED	0x20	/* fully resolved		*/

/*
 * kobj_notify_add() data notification structure
 */
typedef struct kobj_notify_list {
	unsigned int		kn_version;	/* interface vers */
	void			(*kn_func)(unsigned int, struct modctl *);
					/* notification func */
	unsigned int		kn_type;	/* notification type */
	struct kobj_notify_list *kn_prev;
	struct kobj_notify_list *kn_next;
} kobj_notify_list;

#define		KOBJ_NVERSION_NONE	0	/* kn_version */
#define		KOBJ_NVERSION_CURRENT	1
#define		KOBJ_NVERSION_NUM	2

/*
 * TOKENS for kobj_notify_list->kn_type
 *
 * krtld can provide notification to external clients on the
 * following events.
 */
#define		KOBJ_NOTIFY_MODLOAD	1	/* loading of modules */
#define		KOBJ_NOTIFY_MODUNLOAD	2	/* unloading of modules */
#define		KOBJ_NOTIFY_MAX		2

#define	ALIGN(x, a)	((a) == 0 ? (uintptr_t)(x) : \
	(((uintptr_t)(x) + (uintptr_t)(a) - 1l) & ~((uintptr_t)(a) - 1l)))



#ifdef	DEBUG
#define	KOBJ_DEBUG
#endif

#ifdef KOBJ_DEBUG
/*
 * Debugging flags.
 */
#define	D_DEBUG			0x001	/* general debugging */
#define	D_SYMBOLS		0x002	/* debug symbols */
#define	D_RELOCATIONS		0x004	/* debug relocations */
#define	D_BINDPRI		0x008	/* primary binding */
#define	D_BOOTALLOC		0x010	/* mem allocated from boot */
#define	D_LOADING		0x020	/* section loading */
#define	D_PRIMARY		0x040	/* debug primary mods only */
#define	D_LTDEBUG		0x080	/* light debugging */

extern int kobj_debug;		/* different than moddebug */
#endif

/*
 * Flags for kobj memory allocation.
 */
#define	KM_WAIT			0x0		/* wait for it */
#define	KM_NOWAIT		0x1		/* return immediately */
#define	KM_TEMP			0x1000	/* use boot memory in standalone mode */


struct bootops;

#if defined(__ia64)
extern char *gpptr;
extern vmem_t *sdata_arena;			/* module sdata & sbss arena */
extern Elf64_Addr lookup_funcdesc(struct module *, char *, int);
#endif

extern struct modctl_list *primaries;

extern void kobj_init(void *romvec, void *dvec,
	struct bootops *bootvec, val_t *bootaux);
extern int kobj_notify_add(kobj_notify_list *);
extern int kobj_notify_remove(kobj_notify_list *);
extern int do_relocations(struct module *);
extern int do_relocate(struct module *, char *, Word, int, int, Addr);
extern void (*_kobj_printf)(void *, const char *fmt, ...);
extern struct bootops *ops;
extern void exitto(caddr_t);
extern void kobj_sync_instruction_memory(caddr_t, size_t);
extern uint_t kobj_gethashsize(uint_t);
extern void * kobj_mod_alloc(struct module *, size_t, int, reloc_dest_t *);
extern void mach_alloc_funcdesc(struct module *);
extern uint_t kobj_hash_name(char *);
extern caddr_t kobj_segbrk(caddr_t *, size_t, size_t, caddr_t);
extern int get_progbits_size(struct module *, struct proginfo *,
	struct proginfo *, struct proginfo *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KOBJ_IMPL_H */
