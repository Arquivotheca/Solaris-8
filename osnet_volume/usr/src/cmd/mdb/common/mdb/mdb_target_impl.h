/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_TARGET_IMPL_H
#define	_MDB_TARGET_IMPL_H

#pragma ident	"@(#)mdb_target_impl.h	1.2	99/11/19 SMI"

#include <mdb/mdb_target.h>
#include <mdb/mdb_module.h>
#include <mdb/mdb_list.h>
#include <mdb/mdb_gelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

/*
 * Target Operations
 *
 * This ops vector implements the set of primitives which can be used by
 * the debugger to interact with the target, and encompasses most of the
 * calls found in <mdb/mdb_target.h>.  The remainder of the target interface
 * is implemented by common code using these primitives.
 */

typedef struct mdb_tgt_ops {
	int (*t_setflags)(mdb_tgt_t *, int);	/* Set additional flags */
	int (*t_setcontext)(mdb_tgt_t *, void *); /* Set target context */

	void (*t_activate)(mdb_tgt_t *);	/* Activation callback */
	void (*t_deactivate)(mdb_tgt_t *);	/* Deactivation callback */
	void (*t_destroy)(mdb_tgt_t *);		/* Destructor */

	const char *(*t_name)(mdb_tgt_t *);	/* Return target name */
	const char *(*t_isa)(mdb_tgt_t *);	/* Return ISA string */
	const char *(*t_platform)(mdb_tgt_t *);	/* Return platform name */
	int (*t_uname)(mdb_tgt_t *, struct utsname *); /* Return uname */

	ssize_t (*t_aread)(mdb_tgt_t *,
		mdb_tgt_as_t, void *, size_t, mdb_tgt_addr_t);

	ssize_t (*t_awrite)(mdb_tgt_t *,
		mdb_tgt_as_t, const void *, size_t, mdb_tgt_addr_t);

	ssize_t (*t_vread)(mdb_tgt_t *, void *, size_t, uintptr_t);
	ssize_t (*t_vwrite)(mdb_tgt_t *, const void *, size_t, uintptr_t);
	ssize_t (*t_pread)(mdb_tgt_t *, void *, size_t, physaddr_t);
	ssize_t (*t_pwrite)(mdb_tgt_t *, const void *, size_t, physaddr_t);
	ssize_t (*t_fread)(mdb_tgt_t *, void *, size_t, uintptr_t);
	ssize_t (*t_fwrite)(mdb_tgt_t *, const void *, size_t, uintptr_t);
	ssize_t (*t_ioread)(mdb_tgt_t *, void *, size_t, ioaddr_t);
	ssize_t (*t_iowrite)(mdb_tgt_t *, const void *, size_t, ioaddr_t);

	int (*t_vtop)(mdb_tgt_t *, mdb_tgt_as_t, uintptr_t, physaddr_t *);

	int (*t_lookup_by_name)(mdb_tgt_t *,
		const char *, const char *, GElf_Sym *);

	int (*t_lookup_by_addr)(mdb_tgt_t *,
		uintptr_t, uint_t, char *, size_t, GElf_Sym *);

	int (*t_symbol_iter)(mdb_tgt_t *,
		const char *, uint_t, uint_t, mdb_tgt_sym_f *, void *);

	int (*t_mapping_iter)(mdb_tgt_t *, mdb_tgt_map_f *, void *);
	int (*t_object_iter)(mdb_tgt_t *, mdb_tgt_map_f *, void *);

	const mdb_map_t *(*t_addr_to_map)(mdb_tgt_t *, uintptr_t);
	const mdb_map_t *(*t_name_to_map)(mdb_tgt_t *, const char *);

	int (*t_thread_iter)(mdb_tgt_t *, mdb_tgt_thread_f *, void *);
	int (*t_cpu_iter)(mdb_tgt_t *, mdb_tgt_cpu_f *, void *);

	int (*t_thr_status)(mdb_tgt_t *, mdb_tgt_tid_t, mdb_tgt_status_t *);
	int (*t_cpu_status)(mdb_tgt_t *, mdb_tgt_cpuid_t, mdb_tgt_status_t *);
	int (*t_status)(mdb_tgt_t *, mdb_tgt_status_t *);

	int (*t_run)(mdb_tgt_t *, int, const struct mdb_arg *);
	int (*t_step)(mdb_tgt_t *, mdb_tgt_tid_t);
	int (*t_continue)(mdb_tgt_t *, mdb_tgt_status_t *);
	int (*t_call)(mdb_tgt_t *, uintptr_t, int, const struct mdb_arg *);

	int (*t_add_brkpt)(mdb_tgt_t *, uintptr_t, void *);
	int (*t_add_pwapt)(mdb_tgt_t *, physaddr_t, size_t, uint_t, void *);
	int (*t_add_vwapt)(mdb_tgt_t *, uintptr_t, size_t, uint_t, void *);
	int (*t_add_iowapt)(mdb_tgt_t *, ioaddr_t, size_t, uint_t, void *);
	int (*t_add_ixwapt)(mdb_tgt_t *, ulong_t, ulong_t, void *);
	int (*t_add_sysenter)(mdb_tgt_t *, int, void *);
	int (*t_add_sysexit)(mdb_tgt_t *, int, void *);
	int (*t_add_signal)(mdb_tgt_t *, int, void *);
	int (*t_add_object_load)(mdb_tgt_t *, void *);
	int (*t_add_object_unload)(mdb_tgt_t *, void *);

	int (*t_getareg)(mdb_tgt_t *, mdb_tgt_tid_t, const char *,
		mdb_tgt_reg_t *);
	int (*t_putareg)(mdb_tgt_t *, mdb_tgt_tid_t, const char *,
		mdb_tgt_reg_t);

	int (*t_stack_iter)(mdb_tgt_t *, const mdb_tgt_gregset_t *,
		mdb_tgt_stack_f *, void *);

} mdb_tgt_ops_t;

struct mdb_sespec;

typedef struct mdb_se_ops {
	char *(*se_info)(struct mdb_tgt *, struct mdb_sespec *, char *, size_t);
	int (*se_compare)(struct mdb_tgt *, struct mdb_sespec *,
		const mdb_tgt_status_t *);
	int (*se_activate)(struct mdb_tgt *, struct mdb_sespec *);
	int (*se_inactivate)(struct mdb_tgt *, struct mdb_sespec *);
	void (*se_destroy)(struct mdb_tgt *, struct mdb_sespec *);
} mdb_se_ops_t;

typedef struct mdb_sespec {
	mdb_list_t se_list;		/* List forward/back pointers */
	const mdb_se_ops_t *se_ops;	/* Ops vector for insert/delete */
	int se_flags;			/* Flags (see below) */
	int se_id;			/* Unique identifier */
	void *se_data;			/* Private storage for ops vector */
	void *se_cookie;		/* Private storage for caller */
} mdb_sespec_t;

typedef struct mdb_xdata {
	mdb_list_t xd_list;		/* List forward/back pointers */
	const char *xd_name;		/* Buffer name */
	const char *xd_desc;		/* Buffer description */
	ssize_t (*xd_copy)(mdb_tgt_t *, void *, size_t); /* Copy routine */
} mdb_xdata_t;

struct mdb_tgt {
	mdb_list_t t_tgtlist;		/* List forward/back pointers */
	mdb_list_t t_selist;		/* Head of sespec list */
	mdb_list_t t_xdlist;		/* Head of xdata list */
	mdb_module_t *t_module;		/* Backpointer to containing module */
	void *t_pshandle;		/* Proc service handle (if not tgt) */
	const mdb_tgt_ops_t *t_ops;	/* Pointer to target ops vector */
	void *t_data;			/* Private storage for implementation */
	uint_t t_flags;			/* Mode flags (see <mdb_target.h>) */
	id_t t_seseq;			/* Sequence number for next sespec id */
};

/*
 * Special functions which targets can use to fill ops vector slots:
 */
extern long mdb_tgt_notsup();		/* Return -1, set errno to ENOTSUP */
extern void *mdb_tgt_null();		/* Return NULL, set errno to ENOTSUP */
extern long mdb_tgt_nop();		/* Return 0 for success */

/*
 * Utility structures for target implementations:
 */
#define	MDB_TGT_R_PRIV		0x01	/* Privileged register */
#define	MDB_TGT_R_EXPORT	0x02	/* Export register as a variable */
#define	MDB_TGT_R_ALIAS		0x04	/* Alias for another register name */

typedef struct mdb_tgt_regdesc {
	const char *rd_name;		/* Register string name */
	ushort_t rd_num;		/* Register index number */
	ushort_t rd_flags;		/* Register flags (see above) */
} mdb_tgt_regdesc_t;

/*
 * Utility functions for target implementations:
 */
extern int mdb_tgt_xdata_insert(mdb_tgt_t *, const char *, const char *,
	ssize_t (*)(mdb_tgt_t *, void *, size_t));
extern int mdb_tgt_xdata_delete(mdb_tgt_t *, const char *);
extern int mdb_tgt_sym_match(const GElf_Sym *, uint_t);
extern void mdb_tgt_elf_export(mdb_gelf_file_t *);

/*
 * In the initial version of MDB, the data model property is not part of the
 * public API.  However, I am providing this as a hidden part of the ABI as
 * one way we can handle the situation.  If this turns out to be the right
 * decision, we can document it later without having to rev the API version.
 */
#define	MDB_TGT_MODEL_UNKNOWN	0	/* Unknown data model */
#define	MDB_TGT_MODEL_ILP32	1	/* Target data model is ILP32 */
#define	MDB_TGT_MODEL_LP64	2	/* Target data model is LP64 */

#ifdef _LP64
#define	MDB_TGT_MODEL_NATIVE	MDB_TGT_MODEL_LP64
#else
#define	MDB_TGT_MODEL_NATIVE	MDB_TGT_MODEL_ILP32
#endif

extern int mdb_prop_datamodel;

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_TARGET_IMPL_H */
