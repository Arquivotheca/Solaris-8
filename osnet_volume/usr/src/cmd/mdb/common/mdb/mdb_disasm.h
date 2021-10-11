/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_DISASM_H
#define	_MDB_DISASM_H

#pragma ident	"@(#)mdb_disasm.h	1.1	99/08/11 SMI"

#include <mdb/mdb_target.h>
#include <mdb/mdb_modapi.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

/*
 * Forward declaration of the disassembler structure: the internals are defined
 * in mdb_disasm_impl.h and is opaque with respect to callers of this interface.
 */

struct mdb_disasm;
typedef struct mdb_disasm mdb_disasm_t;

/*
 * Disassemblers are created by calling mdb_dis_create() with a disassembler
 * constructor function.  A constructed disassembler can be selected (made
 * the current disassembler) by invoking mdb_dis_select().
 */

typedef int mdb_dis_ctor_f(mdb_disasm_t *);

extern int mdb_dis_select(const char *);
extern mdb_disasm_t *mdb_dis_create(mdb_dis_ctor_f *);
extern void mdb_dis_destroy(mdb_disasm_t *);

/*
 * Currently each disassembler only supports a instruction-to-string operation:
 */
extern mdb_tgt_addr_t mdb_dis_ins2str(mdb_disasm_t *, mdb_tgt_t *,
    mdb_tgt_as_t, char *, mdb_tgt_addr_t);

/*
 * Builtin dcmds for selecting and listing disassemblers:
 */
extern int cmd_dismode(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int cmd_disasms(uintptr_t, uint_t, int, const mdb_arg_t *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_DISASM_H */
