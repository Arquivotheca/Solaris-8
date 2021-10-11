/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_DISASM_IMPL_H
#define	_MDB_DISASM_IMPL_H

#pragma ident	"@(#)mdb_disasm_impl.h	1.1	99/08/11 SMI"

/*
 * Disassembler Implementation
 *
 * Each disassembler provides a string name (for selection with $V or -V),
 * a brief description, and the set of operations defined in mdb_dis_ops_t.
 * Currently the interface defined here is very primitive, but we hope to
 * greatly enhance it in the future if we have a two-pass disassembler.
 */

#include <mdb/mdb_disasm.h>
#include <mdb/mdb_module.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct mdb_dis_ops {
	void (*dis_destroy)(mdb_disasm_t *);
	mdb_tgt_addr_t (*dis_ins2str)(mdb_disasm_t *, mdb_tgt_t *,
	    mdb_tgt_as_t, char *, mdb_tgt_addr_t);
} mdb_dis_ops_t;

struct mdb_disasm {
	const char *dis_name;		/* Disassembler name */
	const char *dis_desc;		/* Brief description */
	mdb_module_t *dis_module;	/* Backpointer to containing module */
	const mdb_dis_ops_t *dis_ops;	/* Pointer to ops vector */
	void *dis_data;			/* Private storage */
};

#ifdef _MDB

#ifdef __sparc
extern mdb_dis_ctor_f sparc1_create;
extern mdb_dis_ctor_f sparc2_create;
extern mdb_dis_ctor_f sparc4_create;
extern mdb_dis_ctor_f sparcv8_create;
extern mdb_dis_ctor_f sparcv9_create;
extern mdb_dis_ctor_f sparcv9plus_create;
#endif	/* __sparc */

#ifdef __i386
extern mdb_dis_ctor_f ia32_create;
#endif	/* __i386 */

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_DISASM_IMPL_H */
