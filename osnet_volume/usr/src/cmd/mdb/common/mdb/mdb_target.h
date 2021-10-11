/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_TARGET_H
#define	_MDB_TARGET_H

#pragma ident	"@(#)mdb_target.h	1.1	99/08/11 SMI"

#include <sys/utsname.h>
#include <sys/types.h>
#include <gelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Forward declaration of the target structure: the target itself is defined in
 * mdb_tgt_impl.h and is opaque with respect to callers of this interface.
 */

struct mdb_tgt;
struct mdb_arg;

typedef struct mdb_tgt mdb_tgt_t;

/*
 * Target Constructors
 *
 * These functions are used to create a complete debugger target.  The
 * constructor is passed as an argument to mdb_tgt_create().
 */

extern int mdb_value_tgt_create(mdb_tgt_t *, int, const char *[]);
extern int mdb_kvm_tgt_create(mdb_tgt_t *, int, const char *[]);
extern int mdb_proc_tgt_create(mdb_tgt_t *, int, const char *[]);
extern int mdb_kproc_tgt_create(mdb_tgt_t *, int, const char *[]);

/*
 * Targets are created by calling mdb_tgt_create() with an optional set of
 * target flags, an argument list, and a target constructor (see above):
 */

#define	MDB_TGT_F_RDWR		0x1	/* Open for writing (else read-only) */
#define	MDB_TGT_F_FORCE		0x2	/* Force open (even if non-exclusive) */
#define	MDB_TGT_F_PRELOAD	0x4	/* Preload all symbol tables */
#define	MDB_TGT_F_NOLOAD	0x8	/* Do not do load-object processing */
#define	MDB_TGT_F_ALL		0xf	/* Mask of all valid flags */

typedef int mdb_tgt_ctor_f(mdb_tgt_t *, int, const char *[]);

extern mdb_tgt_t *mdb_tgt_create(mdb_tgt_ctor_f *, int, int, const char *[]);
extern void mdb_tgt_destroy(mdb_tgt_t *);

extern int mdb_tgt_getflags(mdb_tgt_t *);
extern int mdb_tgt_setflags(mdb_tgt_t *, int);
extern int mdb_tgt_setcontext(mdb_tgt_t *, void *);

/*
 * Targets are activated and de-activated by the debugger framework.  An
 * activation occurs after construction when the target becomes the current
 * target in the debugger.  A target is de-activated prior to its destructor
 * being called by mdb_tgt_destroy, or when another target is activated.
 * These callbacks are suitable for loading support modules and other tasks.
 */
extern void mdb_tgt_activate(mdb_tgt_t *);

/*
 * Convenience functions for accessing miscellaneous target information.
 */
extern const char *mdb_tgt_name(mdb_tgt_t *);
extern const char *mdb_tgt_isa(mdb_tgt_t *);
extern const char *mdb_tgt_platform(mdb_tgt_t *);
extern int mdb_tgt_uname(mdb_tgt_t *, struct utsname *);

/*
 * Address Space Interface
 *
 * Each target can provide access to a set of address spaces, which may include
 * a primary virtual address space, a physical address space, an object file
 * address space (where virtual addresses are converted to file offsets in an
 * object file), and an I/O port address space.  Additionally, the target can
 * provide access to alternate address spaces, which are identified by the
 * opaque mdb_tgt_as_t type.  If the 'as' parameter to mdb_tgt_aread or
 * mdb_tgt_awrite are one of the listed constants, these calls are equivalent
 * to mdb_tgt_{v|p|f|io}read or write.
 */

typedef void *		mdb_tgt_as_t;		/* Opaque address space id */
typedef uint64_t	mdb_tgt_addr_t;		/* Generic unsigned address */

typedef uint64_t	physaddr_t;		/* Physical memory address */
typedef uintptr_t	ioaddr_t;		/* I/o port address */

#define	MDB_TGT_AS_VIRT	((mdb_tgt_as_t)-1L)	/* Virtual address space */
#define	MDB_TGT_AS_PHYS	((mdb_tgt_as_t)-2L)	/* Physical address space */
#define	MDB_TGT_AS_FILE	((mdb_tgt_as_t)-3L)	/* Object file address space */
#define	MDB_TGT_AS_IO	((mdb_tgt_as_t)-4L)	/* I/o address space */

extern ssize_t mdb_tgt_aread(mdb_tgt_t *, mdb_tgt_as_t,
	void *, size_t, mdb_tgt_addr_t);

extern ssize_t mdb_tgt_awrite(mdb_tgt_t *, mdb_tgt_as_t,
	const void *, size_t, mdb_tgt_addr_t);

extern ssize_t mdb_tgt_vread(mdb_tgt_t *, void *, size_t, uintptr_t);
extern ssize_t mdb_tgt_vwrite(mdb_tgt_t *, const void *, size_t, uintptr_t);
extern ssize_t mdb_tgt_pread(mdb_tgt_t *, void *, size_t, physaddr_t);
extern ssize_t mdb_tgt_pwrite(mdb_tgt_t *, const void *, size_t, physaddr_t);
extern ssize_t mdb_tgt_fread(mdb_tgt_t *, void *, size_t, uintptr_t);
extern ssize_t mdb_tgt_fwrite(mdb_tgt_t *, const void *, size_t, uintptr_t);
extern ssize_t mdb_tgt_ioread(mdb_tgt_t *, void *, size_t, uintptr_t);
extern ssize_t mdb_tgt_iowrite(mdb_tgt_t *, const void *, size_t, uintptr_t);

/*
 * Convert an address-space's virtual address to the corresponding
 * physical address (only useful for kernel targets):
 */
extern int mdb_tgt_vtop(mdb_tgt_t *, mdb_tgt_as_t, uintptr_t, physaddr_t *);

/*
 * Convenience functions for reading and writing null-terminated
 * strings from any of the target address spaces:
 */
extern ssize_t mdb_tgt_readstr(mdb_tgt_t *, mdb_tgt_as_t,
	char *, size_t, mdb_tgt_addr_t);

extern ssize_t mdb_tgt_writestr(mdb_tgt_t *, mdb_tgt_as_t,
	const char *, mdb_tgt_addr_t);

/*
 * Symbol Table Interface
 *
 * Each target can provide access to one or more symbol tables, which can be
 * iterated over, or used to lookup symbols by either name or address.  The
 * target can support a primary executable and primary dynamic symbol table,
 * a symbol table for its run-time link-editor, and symbol tables for one or
 * more loaded objects.
 */

/*
 * Reserved object names for mdb_tgt_lookup_by_name():
 */
#define	MDB_TGT_OBJ_EXEC	((const char *)0L)	/* Executable symbols */
#define	MDB_TGT_OBJ_RTLD	((const char *)1L)	/* Ldso/krtld symbols */
#define	MDB_TGT_OBJ_EVERY	((const char *)-1L)	/* All known symbols */

extern int mdb_tgt_lookup_by_name(mdb_tgt_t *, const char *,
	const char *, GElf_Sym *);

/*
 * Flag bit passed to mdb_tgt_lookup_by_addr():
 */
#define	MDB_TGT_SYM_FUZZY	0	/* Match closest address */
#define	MDB_TGT_SYM_EXACT	1	/* Match exact address only */

#define	MDB_TGT_SYM_NAMLEN	1024	/* Recommended max symbol name length */

extern int mdb_tgt_lookup_by_addr(mdb_tgt_t *, uintptr_t, uint_t,
	char *, size_t, GElf_Sym *);

/*
 * Callback function prototype for mdb_tgt_symbol_iter():
 */
typedef int mdb_tgt_sym_f(void *, const GElf_Sym *, const char *);

/*
 * Values for selecting symbol tables with mdb_tgt_symbol_iter():
 */
#define	MDB_TGT_SYMTAB		1	/* Normal symbol table (.symtab) */
#define	MDB_TGT_DYNSYM		2	/* Dynamic symbol table (.dynsym) */

/*
 * Values for selecting symbols of interest by binding and type.  These flags
 * can be used to construct a bitmask to pass to mdb_tgt_symbol_iter():
 */
#define	MDB_TGT_BIND_LOCAL	0x0001	/* Local (static-scope) symbols */
#define	MDB_TGT_BIND_GLOBAL	0x0002	/* Global symbols */
#define	MDB_TGT_BIND_WEAK	0x0004	/* Weak binding symbols */

#define	MDB_TGT_BIND_ANY	0x0007	/* Any of the above */

#define	MDB_TGT_TYPE_NOTYPE	0x0100	/* Symbol has no type */
#define	MDB_TGT_TYPE_OBJECT	0x0200	/* Symbol refers to data */
#define	MDB_TGT_TYPE_FUNC	0x0400	/* Symbol refers to text */
#define	MDB_TGT_TYPE_SECT	0x0800	/* Symbol refers to a section */
#define	MDB_TGT_TYPE_FILE	0x1000	/* Symbol refers to a source file */

#define	MDB_TGT_TYPE_ANY	0x1f00	/* Any of the above */

extern int mdb_tgt_symbol_iter(mdb_tgt_t *, const char *, uint_t, uint_t,
	mdb_tgt_sym_f *, void *);

/*
 * Convenience functions for reading and writing at the address specified
 * by a given object file and symbol name:
 */
extern ssize_t mdb_tgt_readsym(mdb_tgt_t *, mdb_tgt_as_t, void *, size_t,
	const char *, const char *);

extern ssize_t mdb_tgt_writesym(mdb_tgt_t *, mdb_tgt_as_t, const void *, size_t,
	const char *, const char *);

/*
 * Virtual Address Mapping and Load Object interface
 *
 * These interfaces allow the caller to iterate through the various virtual
 * address space mappings, or only those mappings corresponding to load objects.
 */

#define	MDB_TGT_MAPSZ		256	/* Maximum length of mapping name */

#define	MDB_TGT_MAP_R		0x01	/* Mapping is readable */
#define	MDB_TGT_MAP_W		0x02	/* Mapping is writeable */
#define	MDB_TGT_MAP_X		0x04	/* Mapping is executable */
#define	MDB_TGT_MAP_SHMEM	0x08	/* Mapping is shared memory */
#define	MDB_TGT_MAP_STACK	0x10	/* Mapping is a stack of some kind */
#define	MDB_TGT_MAP_HEAP	0x20	/* Mapping is a heap of some kind */
#define	MDB_TGT_MAP_ANON	0x40	/* Mapping is anonymous memory */

typedef struct mdb_map {
	char map_name[MDB_TGT_MAPSZ];	/* Name of mapped object */
	uintptr_t map_base;		/* Virtual address of base of mapping */
	size_t map_size;		/* Size of mapping in bytes */
	uint_t map_flags;		/* Flags (see above) */
} mdb_map_t;

typedef int mdb_tgt_map_f(void *, const mdb_map_t *, const char *);

extern int mdb_tgt_mapping_iter(mdb_tgt_t *, mdb_tgt_map_f *, void *);
extern int mdb_tgt_object_iter(mdb_tgt_t *, mdb_tgt_map_f *, void *);

extern const mdb_map_t *mdb_tgt_addr_to_map(mdb_tgt_t *, uintptr_t);
extern const mdb_map_t *mdb_tgt_name_to_map(mdb_tgt_t *, const char *);

/*
 * Callback types for mdb_tgt_thread_iter() and mdb_tgt_cpu_iter():
 */
typedef void * mdb_tgt_tid_t;		/* Opaque thread identifier */
typedef void * mdb_tgt_cpuid_t;		/* Opaque CPU identifier */

typedef int mdb_tgt_thread_f(void *, mdb_tgt_tid_t, const char *);
typedef int mdb_tgt_cpu_f(void *, mdb_tgt_cpuid_t, const char *);

/*
 * CPU and Thread Interface
 *
 * These target functions allow the caller to iterate over the threads of
 * control and CPUs accessible from a given target.  Depending on the type
 * of target, the CPUs may represent actual physical processors, or some
 * virtual notion of a processor (such as a userland LWP).  Each thread or
 * CPU is identified only by an opaque value meaningful to the target.
 */

extern int mdb_tgt_thread_iter(mdb_tgt_t *, mdb_tgt_thread_f *, void *);
extern int mdb_tgt_cpu_iter(mdb_tgt_t *, mdb_tgt_cpu_f *, void *);

/*
 * Execution Control Interface
 *
 * For in-situ debugging, we provide a relatively simple interface for target
 * execution control.  The target can be continued or single-stepped.  Upon
 * continue, the target's internal list of software event specifiers determines
 * what types of events will cause the target to stop.  The specifier list can
 * be manipulated via the functions provided below.  We currently support the
 * following types of traced events: breakpoints, watchpoints, system call
 * entry, system call exit, signals, and object load/unload.  Once the target
 * has stopped, the status of the representative thread is returned (this
 * status can also be obtained via mdb_tgt_status()).
 */

typedef struct mdb_tgt_status {
	mdb_tgt_tid_t st_tid;		/* Id of thread in question */
	uint_t st_flags;		/* Status flags (see below) */
	uint_t st_reason;		/* Reason for stop (see below) */
	int st_event;			/* Event id, if stopped on event */
} mdb_tgt_status_t;

/*
 * Status flags (st_flags):
 */
#define	MDB_TGT_STOPPED		0x0001	/* Thread is stopped */
#define	MDB_TGT_ISTOP		0x0002	/* Stopped on event of interest */
#define	MDB_TGT_DSTOP		0x0004	/* Stopped due to stop directive */
#define	MDB_TGT_STEP		0x0008	/* Single-step directive in effect */

/*
 * Reasons for thread stop (st_reason), if stopped:
 */
#define	MDB_TGT_REQUESTED	1	/* Stopped because of stop request */
#define	MDB_TGT_FAULTED		2	/* Stopped because of fault */
#define	MDB_TGT_SUSPENDED	3	/* Suspended (not on CPU) */

extern int mdb_tgt_thr_status(mdb_tgt_t *, mdb_tgt_tid_t, mdb_tgt_status_t *);
extern int mdb_tgt_cpu_status(mdb_tgt_t *, mdb_tgt_cpuid_t, mdb_tgt_status_t *);
extern int mdb_tgt_status(mdb_tgt_t *, mdb_tgt_status_t *);

extern int mdb_tgt_run(mdb_tgt_t *, int, const struct mdb_arg *);
extern int mdb_tgt_step(mdb_tgt_t *, mdb_tgt_tid_t);
extern int mdb_tgt_continue(mdb_tgt_t *, mdb_tgt_status_t *);
extern int mdb_tgt_call(mdb_tgt_t *, uintptr_t, int, const struct mdb_arg *);

/*
 * Iterating through the specifier list yields the integer id and private data
 * pointer.  The same callback is invoked as each event is destroyed by
 * mdb_tgt_sespec_destroy, or when an individual event is deleted via
 * mdb_tgt_sespec_delete.
 */
typedef int mdb_tgt_sespec_f(void *, int, void *);

/*
 * Basic manipulation of software event specifier list is performed by invoking
 * one of the mdb_tgt_add_* family of functions to add a new event specifier,
 * or by deleting an event using its id number.  Each mdb_tgt_add_* function
 * takes an additional void * parameter for the caller to associate private
 * data with the event specifier.  This data can be retrieved using
 * mdb_tgt_sespec_data().
 */

extern char *mdb_tgt_sespec_info(mdb_tgt_t *, int, char *, size_t);
extern void *mdb_tgt_sespec_data(mdb_tgt_t *, int);

extern int mdb_tgt_sespec_iter(mdb_tgt_t *, mdb_tgt_sespec_f *, void *);
extern int mdb_tgt_sespec_delete(mdb_tgt_t *, int, mdb_tgt_sespec_f *, void *);

/*
 * Breakpoints can be set at a specified virtual address:
 */
extern int mdb_tgt_add_brkpt(mdb_tgt_t *, uintptr_t, void *);

/*
 * Watchpoints can be set at physical, virtual, or i/o space addresses, and
 * can be for read, write, or execute.  Additionally, some architectures may
 * support the notion of instruction execute watchpoints, specified by an
 * opcode bitmask.
 */
#define	MDB_TGT_WA_R		0x1	/* Read watchpoint */
#define	MDB_TGT_WA_W		0x2	/* Write watchpoint */
#define	MDB_TGT_WA_X		0x4	/* Execute watchpoint */

extern int mdb_tgt_add_pwapt(mdb_tgt_t *, physaddr_t, size_t, uint_t, void *);
extern int mdb_tgt_add_vwapt(mdb_tgt_t *, uintptr_t, size_t, uint_t, void *);
extern int mdb_tgt_add_iowapt(mdb_tgt_t *, ioaddr_t, size_t, uint_t, void *);
extern int mdb_tgt_add_ixwapt(mdb_tgt_t *, ulong_t, ulong_t, void *);

/*
 * For user process debugging, tracepoints can be set on entry or exit from
 * a system call, or on receipt of a signal:
 */
extern int mdb_tgt_add_sysenter(mdb_tgt_t *, int, void *);
extern int mdb_tgt_add_sysexit(mdb_tgt_t *, int, void *);
extern int mdb_tgt_add_signal(mdb_tgt_t *, int, void *);

/*
 * Tracepoints can also be set on object load or unload.  For the kernel, this
 * corresponds to a module load or unload.  For user process debugging, this
 * corresponds to a dlopen(3X) or dlclose(3X) call.
 */
extern int mdb_tgt_add_object_load(mdb_tgt_t *, void *);
extern int mdb_tgt_add_object_unload(mdb_tgt_t *, void *);

/*
 * Machine Register Interface
 *
 * The machine registers for a given thread can be manipulated using the
 * getareg and putareg interface; the caller must know the naming convention
 * for registers for the given target architecture.  For the purposes of
 * this interface, we declare the register container to be the largest
 * current integer container.
 */

typedef uint64_t mdb_tgt_reg_t;

extern int mdb_tgt_getareg(mdb_tgt_t *, mdb_tgt_tid_t,
	const char *, mdb_tgt_reg_t *);

extern int mdb_tgt_putareg(mdb_tgt_t *, mdb_tgt_tid_t,
	const char *, mdb_tgt_reg_t);

/*
 * Stack Interface
 *
 * The target stack interface provides the ability to iterate backward through
 * the frames of an execution stack.  For the purposes of this interface, the
 * mdb_tgt_gregset (general purpose register set) is an opaque type: there must
 * be an implicit contract between the target implementation and any debugger
 * modules that must interpret the contents of this structure.  The callback
 * function is provided with the only elements of a stack frame which we can
 * reasonably abstract: the virtual address corresponding to a program counter
 * value, and an array of arguments passed to the function call represented by
 * this frame.  The rest of the frame is presumed to be contained within the
 * mdb_tgt_gregset_t, and is architecture-specific.
 */

typedef struct mdb_tgt_gregset mdb_tgt_gregset_t;

typedef int mdb_tgt_stack_f(void *, uintptr_t, uint_t, const long *,
	const mdb_tgt_gregset_t *);

extern int mdb_tgt_stack_iter(mdb_tgt_t *, const mdb_tgt_gregset_t *,
	mdb_tgt_stack_f *, void *);

/*
 * External Data Interface
 *
 * The external data interface provides each target with the ability to export
 * a set of named buffers that contain data which is associated with the
 * target, but is somehow not accessible through one of its address spaces and
 * does not correspond to a machine register.  A process credential is an
 * example of such a buffer: the credential is associated with the given
 * process, but is stored in the kernel (not the process's address space) and
 * thus is not accessible through any other target interface.  Since it is
 * exported via /proc, the user process target can export this information as a
 * named buffer for target-specific dcmds to consume.
 */

typedef int mdb_tgt_xdata_f(void *, const char *, const char *, size_t);

extern int mdb_tgt_xdata_iter(mdb_tgt_t *, mdb_tgt_xdata_f *, void *);
extern ssize_t mdb_tgt_getxdata(mdb_tgt_t *, const char *, void *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_TARGET_H */
