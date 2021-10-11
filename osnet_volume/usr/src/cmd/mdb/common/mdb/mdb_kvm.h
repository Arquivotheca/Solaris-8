/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_KVM_H
#define	_MDB_KVM_H

#pragma ident	"@(#)mdb_kvm.h	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <sys/machelf.h>
#include <kvm.h>

#include <mdb/mdb_target.h>
#include <mdb/mdb_list.h>
#include <mdb/mdb_gelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

typedef struct kt_module {
	mdb_list_t km_list;		/* List forward/back pointers */
	char *km_name;			/* Module name */
	void *km_data;			/* Data buffer (module->symspace) */
	size_t km_datasz;		/* Size of km_data in bytes */
	void *km_symbuf;		/* Base of symbol table in km_data */
	char *km_strtab;		/* Base of string table in km_data */
	mdb_gelf_symtab_t *km_symtab;	/* Symbol table for module */
	uintptr_t km_symspace_va;	/* Kernel VA of krtld symspace */
	uintptr_t km_symtab_va;		/* Kernel VA of krtld symtab */
	uintptr_t km_strtab_va;		/* Kernel VA of krtld strtab */
	Shdr km_symtab_hdr;		/* Native .symtab section header */
	Shdr km_strtab_hdr;		/* Native .strtab section header */
	uintptr_t km_text_va;		/* Kernel VA of start of module text */
	size_t km_text_size;		/* Size of module text */
	size_t km_data_size;		/* Size of module data */
} kt_module_t;

typedef struct kt_data {
	ssize_t (*k_aread)();		/* Libkvm kvm_aread() routine */
	ssize_t (*k_awrite)();		/* Libkvm kvm_awrite() routine */
	ssize_t (*k_pread)();		/* Libkvm kvm_pread() routine */
	ssize_t (*k_pwrite)();		/* Libkvm kvm_pwrite() routine */
	const char *k_symfile;		/* Symbol table pathname */
	const char *k_kvmfile;		/* Core file pathname */
	kvm_t *k_cookie;		/* Cookie for libkvm routines */
	struct as *k_as;		/* Kernel VA of kas struct */
	mdb_io_t *k_fio;		/* File i/o backend */
	mdb_gelf_file_t *k_file;	/* ELF file object */
	mdb_gelf_symtab_t *k_symtab;	/* Standard symbol table */
	mdb_gelf_symtab_t *k_dynsym;	/* Dynamic symbol table */
	kt_module_t *k_modhead;		/* Head of list of modules */
	kt_module_t *k_modtail;		/* Tail of list of modules */
	mdb_nv_t k_modules;		/* Hash table of modules */
	mdb_list_t k_modlist;		/* List of modules in load order */
	char k_platform[MAXNAMELEN];	/* Platform string */
	const mdb_tgt_regdesc_t *k_rds;	/* Register description table */
	mdb_tgt_gregset_t *k_regs;	/* Representative register set */
	size_t k_regsize;		/* Size of k_regs in bytes */
	mdb_tgt_tid_t k_tid;		/* Pointer to representative thread */
	mdb_dcmd_f *k_dcmd_regs;	/* Dcmd to print registers */
	mdb_dcmd_f *k_dcmd_stack;	/* Dcmd to print stack trace */
	mdb_dcmd_f *k_dcmd_stackv;	/* Dcmd to print verbose stack trace */
	GElf_Sym k_intr_sym;		/* Kernel locore cmnint symbol */
	GElf_Sym k_trap_sym;		/* Kernel locore cmntrap symbol */
	int k_activated;		/* Set if kt_activate called */
} kt_data_t;

extern int kt_setflags(mdb_tgt_t *, int);
extern int kt_setcontext(mdb_tgt_t *, void *);

extern void kt_activate(mdb_tgt_t *);
extern void kt_deactivate(mdb_tgt_t *);
extern void kt_destroy(mdb_tgt_t *);

extern const char *kt_name(mdb_tgt_t *);
extern const char *kt_platform(mdb_tgt_t *);
extern int kt_uname(mdb_tgt_t *t, struct utsname *);

extern ssize_t kt_aread(mdb_tgt_t *, mdb_tgt_as_t,
    void *, size_t, mdb_tgt_addr_t);

extern ssize_t kt_awrite(mdb_tgt_t *, mdb_tgt_as_t,
    const void *, size_t, mdb_tgt_addr_t);

extern ssize_t kt_vread(mdb_tgt_t *, void *, size_t, uintptr_t);
extern ssize_t kt_vwrite(mdb_tgt_t *, const void *, size_t, uintptr_t);
extern ssize_t kt_pread(mdb_tgt_t *, void *, size_t, physaddr_t);
extern ssize_t kt_pwrite(mdb_tgt_t *, const void *, size_t, physaddr_t);
extern ssize_t kt_fread(mdb_tgt_t *, void *, size_t, uintptr_t);
extern ssize_t kt_fwrite(mdb_tgt_t *, const void *, size_t, uintptr_t);

extern int kt_vtop(mdb_tgt_t *, mdb_tgt_as_t, uintptr_t, physaddr_t *);

extern int kt_lookup_by_name(mdb_tgt_t *, const char *,
    const char *, GElf_Sym *);

extern int kt_lookup_by_addr(mdb_tgt_t *, uintptr_t,
    uint_t, char *, size_t, GElf_Sym *);

extern int kt_symbol_iter(mdb_tgt_t *, const char *, uint_t,
    uint_t, mdb_tgt_sym_f *, void *);

extern int kt_mapping_iter(mdb_tgt_t *, mdb_tgt_map_f *, void *);
extern int kt_object_iter(mdb_tgt_t *, mdb_tgt_map_f *, void *);

#ifdef sparc
extern void kt_sparcv9_init(mdb_tgt_t *);
extern void kt_sparcv7_init(mdb_tgt_t *);
#else	/* sparc */
extern void kt_ia32_init(mdb_tgt_t *);
#endif	/* sparc */

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_KVM_H */
