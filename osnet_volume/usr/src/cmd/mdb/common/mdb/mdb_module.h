/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_MODULE_H
#define	_MDB_MODULE_H

#pragma ident	"@(#)mdb_module.h	1.1	99/08/11 SMI"

#include <mdb/mdb_argvec.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_target.h>
#include <mdb/mdb_disasm.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

typedef struct mdb_module {
	mdb_nv_t mod_dcmds;		/* Module dcmds hash */
	mdb_nv_t mod_walkers;		/* Module walkers hash */
	const char *mod_name;		/* Module name */
	void *mod_hdl;			/* Module object handle */
	mdb_modinfo_t *mod_info;	/* Module information */
	const mdb_modinfo_t *(*mod_init)(void);	/* Module load callback */
	void (*mod_fini)(void);		/* Module unload callback */
	mdb_tgt_ctor_f *mod_tgt_ctor;	/* Module target constructor */
	mdb_dis_ctor_f *mod_dis_ctor;	/* Module disassembler constructor */
	struct mdb_module *mod_prev;	/* Previous module on dependency list */
	struct mdb_module *mod_next;	/* Next module on dependency list */
} mdb_module_t;

typedef struct mdb_idcmd {
	const char *idc_name;		/* Backpointer to variable name */
	const char *idc_usage;		/* Usage message */
	const char *idc_descr;		/* Description */
	mdb_dcmd_f *idc_funcp;		/* Command function */
	void (*idc_help)(void);		/* Help function */
	mdb_module_t *idc_modp;		/* Backpointer to module */
	mdb_var_t *idc_var;		/* Backpointer to global variable */
} mdb_idcmd_t;

typedef struct mdb_iwalker {
	const char *iwlk_name;		/* Walk type name */
	char *iwlk_descr;		/* Walk description */
	int (*iwlk_init)(struct mdb_walk_state *);	/* Walk constructor */
	int (*iwlk_step)(struct mdb_walk_state *);	/* Walk iterator */
	void (*iwlk_fini)(struct mdb_walk_state *);	/* Walk destructor */
	void *iwlk_init_arg;		/* Walk constructor argument */
	mdb_module_t *iwlk_modp;	/* Backpointer to module */
	mdb_var_t *iwlk_var;		/* Backpointer to global variable */
} mdb_iwalker_t;

#define	MDB_MOD_LOCAL	0x0		/* Load module RTLD_LOCAL */
#define	MDB_MOD_GLOBAL	0x1		/* Load module RTLD_GLOBAL */
#define	MDB_MOD_SILENT	0x2		/* Remain silent if no module found */
#define	MDB_MOD_FORCE	0x4		/* Forcibly interpose module defs */
#define	MDB_MOD_BUILTIN	0x8		/* Module is compiled into debugger */

extern mdb_module_t *mdb_module_load(const char *, int);
extern void mdb_module_load_all(void);

extern int mdb_module_unload(const char *);
extern void mdb_module_unload_all(void);

extern int mdb_module_add_dcmd(mdb_module_t *, const mdb_dcmd_t *, int);
extern int mdb_module_remove_dcmd(mdb_module_t *, const char *);

extern int mdb_module_add_walker(mdb_module_t *, const mdb_walker_t *, int);
extern int mdb_module_remove_walker(mdb_module_t *, const char *);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_MODULE_H */
