/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_DEBUG_H
#define	_DEBUG_H

#pragma ident	"@(#)debug.h	1.76	99/09/14 SMI"

/*
 * Global include file for linker debugging.
 *
 * ld(1) and ld.so carry out all diagnostic debugging calls via dlopen'ing
 * the library liblddbg.so.  Thus debugging is always enabled.  The utility
 * elfdump() is explicitly dependent upon this library.  There are two
 * categories of routines defined in this library:
 *
 *  o	Debugging routines that have specific linker knowledge, and test the
 *	class of debugging allowable before proceeding, start with the `Dbg_'
 *	prefix.
 *
 *  o	Lower level routines that provide generic ELF structure interpretation
 *	start with the `Elf_' prefix.  These latter routines are the only
 *	routines used by the elfdump() utility.
 */

#include <libelf.h>
#include "sgs.h"
#include "rtld.h"
#include "machdep.h"
#include "gelf.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Define any interface flags.  These flags direct the debugging routine to
 * generate different diagnostics, thus the strings themselves are maintained
 * in this library.
 */
#define	DBG_SUP_ENVIRON		1
#define	DBG_SUP_CMDLINE		2
#define	DBG_SUP_DEFAULT		3

#define	DBG_SUP_START		1
#define	DBG_SUP_ATEXIT		2
#define	DBG_SUP_FILE		3
#define	DBG_SUP_SECTION		4

#define	DBG_CONF_IGNORE		1	/* Configuration processing errors */
#define	DBG_CONF_VERSION	2
#define	DBG_CONF_PRCFAIL	3
#define	DBG_CONF_CORRUPT	4

#define	DBG_CONF_EDLIBPATH	0x0100	/* Configuration features available */
#define	DBG_CONF_ESLIBPATH	0x0200
#define	DBG_CONF_ADLIBPATH	0x0400
#define	DBG_CONF_ASLIBPATH	0x0800
#define	DBG_CONF_DIRCFG		0x1000
#define	DBG_CONF_OBJALT		0x2000
#define	DBG_CONF_MEMRESV	0x4000
#define	DBG_CONF_FEATMSK	0xff00

#define	DBG_ORDER_INFO_RANGE	1	/* sh_info out of range */
#define	DBG_ORDER_INFO_ORDER	2	/* sh_info also ordered */
#define	DBG_ORDER_LINK_OUTRANGE	3	/* sh_link out of range */
#define	DBG_ORDER_FLAGS		4	/* sh_flags do not match */
#define	DBG_ORDER_CYCLIC	5	/* sh_link cyclic */
#define	DBG_ORDER_LINK_ERROR	6	/* One of sh_linked has an error */

#define	DBG_DLSYM_DEF		0
#define	DBG_DLSYM_NEXT		1
#define	DBG_DLSYM_DEFAULT	2

/*
 * Recoverable input errors.
 * If a dependency lookup finds an ELF, it might reject it for one
 * of the following reasons and continue the search.
 */
#define	DBG_IFL_ERR_NONE	0
#define	DBG_IFL_ERR_MACH	1	/* machine type mismatch */
#define	DBG_IFL_ERR_CLASS	2	/* class (32-bit/64-bit) mismatch */
#define	DBG_IFL_ERR_LIBVER	3	/* elf library version mismatch */
#define	DBG_IFL_ERR_DATA	4	/* data format (MSG/LSB) mismatch */
#define	DBG_IFL_ERR_FLAGS	5	/* bad e_flags field */
#define	DBG_IFL_ERR_BADMATCH	6	/* e_flags mismatch */
#define	DBG_IFL_ERR_HALFLAG	7	/* requires HaL extensions */
#define	DBG_IFL_ERR_TYPE	8	/* bad Elf type */
#define	DBG_IFL_ERR_US3		9	/* Sun UltraSPARC III required */

/*
 * Define our setup entry point.
 */
extern	int		Dbg_setup(const char *);

/*
 * Define a user macro to invoke debugging.  The `dbg_mask' variable acts as a
 * suitable flag, and can be set to collect the return value from Dbg_setup().
 */
extern	int		dbg_mask;

#define	DBG_CALL(func)	if (dbg_mask) func

/*
 * Print routine, this must be supplied by the application.
 */
/*PRINTFLIKE1*/
extern	void		dbg_print(const char *, ...);


/*
 * External interface routines.  These are linker specific.
 */
#ifdef _ELF64
#define	Dbg_file_analyze	Dbg_file_analyze64
#define	Dbg_file_aout		Dbg_file_aout64
#define	Dbg_file_archive	Dbg_file_archive64
#define	Dbg_file_bind_entry	Dbg_file_bind_entry64
#define	Dbg_file_bind_needed	Dbg_file_bind_needed64
#define	Dbg_file_bind_title	Dbg_file_bind_title64
#define	Dbg_file_config_dis	Dbg_file_config_dis64
#define	Dbg_file_config_obj	Dbg_file_config_obj64
#define	Dbg_file_delete		Dbg_file_delete64
#define	Dbg_file_dlclose	Dbg_file_dlclose64
#define	Dbg_file_dldump		Dbg_file_dldump64
#define	Dbg_file_dlopen		Dbg_file_dlopen64
#define	Dbg_file_elf		Dbg_file_elf64
#define	Dbg_file_filter		Dbg_file_filter64
#define	Dbg_file_fixname	Dbg_file_fixname64
#define	Dbg_file_generic	Dbg_file_generic64
#define	Dbg_file_ldso		Dbg_file_ldso64
#define	Dbg_file_lazyload	Dbg_file_lazyload64
#define	Dbg_file_needed		Dbg_file_needed64
#define	Dbg_file_nl		Dbg_file_nl64
#define	Dbg_file_output		Dbg_file_output64
#define	Dbg_file_preload	Dbg_file_preload64
#define	Dbg_file_prot		Dbg_file_prot64
#define	Dbg_file_reuse		Dbg_file_reuse64
#define	Dbg_file_skip		Dbg_file_skip64
#define	Dbg_file_unused		Dbg_file_unused64
#define	Dbg_got_display		Dbg_got_display64
#define	Dbg_map_atsign		Dbg_map_atsign64
#define	Dbg_map_dash		Dbg_map_dash64
#define	Dbg_map_ent		Dbg_map_ent64
#define	Dbg_map_equal		Dbg_map_equal64
#define	Dbg_map_parse		Dbg_map_parse64
#define	Dbg_map_pipe		Dbg_map_pipe64
#define	Dbg_map_seg		Dbg_map_seg64
#define	Dbg_map_size_new	Dbg_map_size_new64
#define	Dbg_map_size_old	Dbg_map_size_old64
#define	Dbg_map_sort_fini	Dbg_map_sort_fini64
#define	Dbg_map_sort_orig	Dbg_map_sort_orig64
#define	Dbg_map_symbol		Dbg_map_symbol64
#define	Dbg_map_version		Dbg_map_version64
#define	Dbg_move_mventry	Dbg_move_mventry64
#define	Dbg_move_mventry2	Dbg_move_mventry264
#define	Dbg_move_actsctadj	Dbg_move_actsctadj64
#define	Dbg_move_outsctadj	Dbg_move_outsctadj64
#define	Dbg_move_psyminfo	Dbg_move_psyminfo64
#define	Dbg_reloc_disable_lazy	Dbg_reloc_disable_lazy64
#define	Dbg_reloc_in		Dbg_reloc_in64
#define	Dbg_reloc_out		Dbg_reloc_out64
#define	Dbg_reloc_proc		Dbg_reloc_proc64
#define	Dbg_reloc_ars_entry	Dbg_reloc_ars_entry64
#define	Dbg_reloc_ors_entry	Dbg_reloc_ors_entry64
#define	Dbg_reloc_doact		Dbg_reloc_doact64
#define	Dbg_reloc_dooutrel	Dbg_reloc_dooutrel64
#define	Dbg_reloc_reg_apply	Dbg_reloc_reg_apply64
#define	Dbg_sec_added		Dbg_sec_added64
#define	Dbg_sec_created		Dbg_sec_created64
#define	Dbg_sec_discarded	Dbg_sec_discarded64
#define	Dbg_sec_in		Dbg_sec_in64
#define	Dbg_sec_order_list	Dbg_sec_order_list64
#define	Dbg_sec_order_error	Dbg_sec_order_error64
#define	Dbg_seg_entry		Dbg_seg_entry64
#define	Dbg_seg_list		Dbg_seg_list64
#define	Dbg_seg_os		Dbg_seg_os64
#define	Dbg_seg_title		Dbg_seg_title64
#define	Dbg_syminfo_entry	Dbg_syminfo_entry64
#define	Dbg_syminfo_entry_title	Dbg_syminfo_entry_title64
#define	Dbg_syminfo_title	Dbg_syminfo_title64
#define	Dbg_syms_ar_checking	Dbg_syms_ar_checking64
#define	Dbg_syms_ar_entry	Dbg_syms_ar_entry64
#define	Dbg_syms_ar_resolve	Dbg_syms_ar_resolve64
#define	Dbg_syms_created	Dbg_syms_created64
#define	Dbg_syms_discarded	Dbg_syms_discarded64
#define	Dbg_syms_entered	Dbg_syms_entered64
#define	Dbg_syms_entry		Dbg_syms_entry64
#define	Dbg_syms_global		Dbg_syms_global64
#define	Dbg_syms_new		Dbg_syms_new64
#define	Dbg_syms_nl		Dbg_syms_nl64
#define	Dbg_syms_old		Dbg_syms_old64
#define	Dbg_syms_process	Dbg_syms_process64
#define	Dbg_syms_reduce		Dbg_syms_reduce64
#define	Dbg_syms_reloc		Dbg_syms_reloc64
#define	Dbg_syms_resolved	Dbg_syms_resolved64
#define	Dbg_syms_resolving1	Dbg_syms_resolving164
#define	Dbg_syms_resolving2	Dbg_syms_resolving264
#define	Dbg_syms_sec_entry	Dbg_syms_sec_entry64
#define	Dbg_syms_sec_title	Dbg_syms_sec_title64
#define	Dbg_syms_spec_title	Dbg_syms_spec_title64
#define	Dbg_syms_up_title	Dbg_syms_up_title64
#define	Dbg_syms_updated	Dbg_syms_updated64
#define	Dbg_syms_dlsym		Dbg_syms_dlsym64
#define	Dbg_syms_lookup_aout	Dbg_syms_lookup_aout64
#define	Dbg_syms_lookup		Dbg_syms_lookup64
#define	Dbg_audit_interface	Dbg_audit_interface64
#define	Dbg_audit_lib		Dbg_audit_lib64
#define	Dbg_audit_object	Dbg_audit_object64
#define	Dbg_audit_symval	Dbg_audit_symval64
#define	Dbg_audit_version	Dbg_audit_version64
#define	Dbg_ver_avail_entry	Dbg_ver_avail_entry64
#define	Dbg_ver_desc_entry	Dbg_ver_desc_entry64
#endif	/* _ELF64 */

extern	void		Dbg_args_files(int, char *);
extern	void		Dbg_args_flags(int, int);
extern	void		Dbg_bind_global(const char *, caddr_t, caddr_t, uint_t,
				const char *, caddr_t, caddr_t, const char *);
extern	void		Dbg_bind_profile(uint_t, uint_t);
extern	void		Dbg_bind_weak(const char *, caddr_t, caddr_t,
				const char *);
extern	void		Dbg_ent_print(Half, List * len, Boolean);
extern	void		Dbg_file_analyze(const char *, int);
extern	void		Dbg_file_aout(const char *, unsigned long,
				unsigned long, unsigned long);
extern	void		Dbg_file_archive(const char *, int);
extern	void		Dbg_file_bind_entry(Rt_map *, Rt_map *);
extern	void		Dbg_file_bind_needed(Rt_map *);
extern	void		Dbg_file_bind_title(int);
extern	void		Dbg_file_config_dis(const char *, int);
extern	void		Dbg_file_config_obj(const char *, const char *,
				const char *);
extern	void		Dbg_file_delete(const char *);
extern	void		Dbg_file_dlclose(const char *, int);
extern	void		Dbg_file_dldump(const char *, const char *, int);
extern	void		Dbg_file_dlopen(const char *, const char *, int);
extern	void		Dbg_file_elf(const char *, unsigned long,
				unsigned long, unsigned long, unsigned long,
				unsigned long, unsigned int, Lmid_t);
extern	void		Dbg_file_filter(const char *, const char *);
extern	void		Dbg_file_fixname(const char *, const char *);
extern	void		Dbg_file_generic(Ifl_desc *);
extern	void		Dbg_file_lazyload(const char *, const char *,
				const char *);
extern	void		Dbg_file_ldso(const char *, unsigned long,
				unsigned long, unsigned long, unsigned long);
extern	void		Dbg_file_needed(const char *, const char *);
extern	void		Dbg_file_nl(void);
extern	void		Dbg_file_output(Ofl_desc *);
extern	void		Dbg_file_preload(const char *);
extern	void		Dbg_file_prot(const char *, int);
extern	void		Dbg_file_reuse(const char *, const char *);
extern	void		Dbg_file_rejected(const char *, int, uint_t);
extern	void		Dbg_file_skip(const char *, const char *);
extern	void		Dbg_file_unused(const char *);
extern	void		Dbg_got_display(Gottable *, Ofl_desc *);
extern	void		Dbg_libs_audit(const char *, const char *);
extern	void		Dbg_libs_ignore(const char *);
extern	void		Dbg_libs_init(List *, List *);
extern	void		Dbg_libs_l(const char *, const char *);
extern	void		Dbg_libs_path(const char *);
extern	void		Dbg_libs_req(const char *, const char *, const char *);
extern	void		Dbg_libs_update(List *, List *);
extern	void		Dbg_libs_yp(const char *);
extern	void		Dbg_libs_ylu(const char *, const char *, int);
extern	void		Dbg_libs_find(const char *);
extern	void		Dbg_libs_found(const char *);
extern	void		Dbg_libs_dpath(const char *);
extern	void		Dbg_libs_rpath(const char *, const char *);
extern	void		Dbg_map_atsign(Boolean);
extern	void		Dbg_map_dash(const char *, Sdf_desc *);
extern	void		Dbg_map_ent(Boolean, Ent_desc *, Ofl_desc *);
extern	void		Dbg_map_equal(Boolean);
extern	void		Dbg_map_parse(const char *);
extern	void		Dbg_map_pipe(Sg_desc *, const char *, const Word);
extern	void		Dbg_map_seg(Half, int, Sg_desc *);
extern	void		Dbg_map_size_new(const char *);
extern	void		Dbg_map_size_old(Ehdr *, Sym_desc *);
extern	void		Dbg_map_sort_fini(Sg_desc *);
extern	void		Dbg_map_sort_orig(Sg_desc *);
extern	void		Dbg_map_symbol(Ehdr *, Sym_desc *);
extern	void		Dbg_map_version(const char *, const char *, int);
extern 	void		Dbg_move_actsctadj(Sym_desc *);
extern 	void		Dbg_move_adjexpandreloc(ulong_t, const char *);
extern 	void		Dbg_move_adjmovereloc(ulong_t, ulong_t, const char *);
extern 	void		Dbg_move_expanding(unsigned const char *);
extern 	void		Dbg_move_input1(const char *);
extern 	void		Dbg_move_movedata(const char *);
extern 	void		Dbg_move_mventry(int, Move *, Sym_desc *);
extern 	void		Dbg_move_mventry2(Move *);
extern 	void		Dbg_move_outmove(unsigned const char *);
extern 	void		Dbg_move_outsctadj(Sym_desc *);
extern 	void		Dbg_move_parexpn(const char *);
extern 	void		Dbg_move_psyminfo(Ofl_desc *);
extern 	void		Dbg_move_sections(const char *, ulong_t);
extern 	void		Dbg_move_sunwbss(void);
extern	void		Dbg_move_title(int);
#if	defined(_ELF64)
extern	void		Dbg_pltpad_bindto64(const char *, const char *, Addr);
extern	void		Dbg_pltpad_boundto64(const char *, Addr, const char *,
				const char *);
#endif
extern	void		Dbg_reloc_apply(unsigned long long, unsigned long long);
extern	void		Dbg_reloc_reg_apply(unsigned long long,
				unsigned long long);
extern	void		Dbg_reloc_in(Half, Word, void *, const char *,
			    const char *);
extern	void		Dbg_reloc_out(Half, Word, void *, const char *,
			    const char *);
extern	void		Dbg_reloc_proc(Os_desc *, Is_desc *, Is_desc *);
extern	void		Dbg_reloc_ars_entry(Half, Rel_desc *);
extern	void		Dbg_reloc_ors_entry(Half, Rel_desc *);
extern	void		Dbg_reloc_disable_lazy(Half, const char *,
				const char *, Word);
extern	void		Dbg_reloc_doactiverel(void);
extern	void		Dbg_reloc_doact(Half, Word, Xword, Xword, const char *,
			    Os_desc *);
extern	void		Dbg_reloc_dooutrel(Rel_desc *);
extern	void		Dbg_reloc_end(const char *);
extern	void		Dbg_reloc_run(const char *, uint_t);
extern	void		Dbg_sec_added(Os_desc *, Sg_desc *);
extern	void		Dbg_sec_created(Os_desc *, Sg_desc *);
extern	void		Dbg_sec_discarded(Is_desc *, Is_desc *);
extern	void		Dbg_sec_in(Is_desc *);
extern	void		Dbg_sec_order_list(Ofl_desc *, int);
extern	void		Dbg_sec_order_error(Ifl_desc *, Word, int);
extern	void		Dbg_seg_entry(Half, int, Sg_desc *);
extern	void		Dbg_seg_list(Half, List *);
extern	void		Dbg_seg_os(Ofl_desc *, Os_desc *, int);
extern	void		Dbg_seg_title(void);
extern	void		Dbg_support_action(const char *, const char *, int,
			    const char *);
extern	void		Dbg_support_load(const char *, const char *);
extern	void		Dbg_support_req(const char *, int);
extern	void		Dbg_syms_ar_checking(Xword, Elf_Arsym *, const char *);
extern	void		Dbg_syms_ar_entry(Xword, Elf_Arsym *);
extern	void		Dbg_syms_ar_resolve(Xword, Elf_Arsym *, const char *,
			    int);
extern	void		Dbg_syms_ar_title(const char *, int);
extern	void		Dbg_syms_created(const char *);
extern	void		Dbg_syms_discarded(Sym_desc *, Is_desc *);
extern	void		Dbg_syms_entered(Ehdr *, Sym *, Sym_desc *);
extern	void		Dbg_syms_entry(Xword, Sym_desc *);
extern	void		Dbg_syms_global(Xword, const char *);
extern	void		Dbg_syms_lazy_rescan(const char *);
extern	void		Dbg_syms_new(Ehdr *, Sym *, Sym_desc *);
extern	void		Dbg_syms_nl(void);
extern	void		Dbg_syms_old(Ehdr *, Sym_desc *);
extern	void		Dbg_syms_process(Ifl_desc *);
extern	void		Dbg_syms_reduce(Ehdr *, Sym_desc *);
extern	void		Dbg_syms_reloc(Ehdr *, Sym_desc *, Boolean);
extern	void		Dbg_syms_resolved(Ehdr *, Sym_desc *);
extern	void		Dbg_syms_resolving1(Xword, const char *, int, int);
extern	void		Dbg_syms_resolving2(Ehdr *, Sym *, Sym *, Sym_desc *,
			    Ifl_desc *);
extern	void		Dbg_syms_sec_entry(int, Sg_desc *, Os_desc *);
extern	void		Dbg_syms_sec_title(void);
extern	void		Dbg_syms_spec_title(void);
extern	void		Dbg_syms_up_title(Ehdr *);
extern	void		Dbg_syms_updated(Ehdr *, Sym_desc *, const char *);
extern	void		Dbg_syms_dlsym(const char *, const char *, int);
extern	void		Dbg_syms_lookup_aout(const char *);
extern	void		Dbg_syms_lookup(const char *, const char *,
				const char *);
extern	void		Dbg_tsort_printscc(Rt_map **, int, int, int, int);
extern	void		Dbg_audit_interface(const char *, const char *);
extern	void		Dbg_audit_lib(const char *);
extern	void		Dbg_audit_object(const char *, const char *);
extern	void		Dbg_audit_symval(const char *, const char *,
				const char *, Addr, Addr);
extern	void		Dbg_audit_version(const char *, ulong_t);
extern	void		Dbg_syminfo_entry(int, Syminfo *, Sym *,
				const char *, Dyn *);
extern	void		Dbg_syminfo_title(void);
extern	void		Dbg_util_call_fini(const char *);
extern	void		Dbg_util_call_init(const char *);
extern	void		Dbg_util_call_main(const char *);
extern	void		Dbg_util_nl(void);
extern	void		Dbg_util_str(const char *);
extern	void		Dbg_ver_avail_entry(Ver_index *, const char *);
extern	void		Dbg_ver_avail_title(const char *);
extern	void		Dbg_ver_desc_entry(Ver_desc *);
extern	void		Dbg_ver_def_title(const char *);
extern	void		Dbg_ver_need_title(const char *);
extern	void		Dbg_ver_need_entry(Half, const char *, const char *);
extern	void		Dbg_ver_nointerface(const char *);
extern	void		Dbg_ver_symbol(const char *);

/*
 * External interface routines. These are not linker specific and provide
 * generic routines for interpreting elf structures.
 */
#ifdef _ELF64
#define	Elf_phdr_entry		Gelf_phdr_entry
#define	Elf_shdr_entry		Gelf_shdr_entry
#define	Elf_sym_table_entry	Gelf_sym_table_entry
#else	/* elf32 */
extern	void		Elf_phdr_entry(Half, Elf32_Phdr *);
extern	void		Elf_shdr_entry(Half, Elf32_Shdr *);
extern	void		Elf_sym_table_entry(const char *, Elf32_Ehdr *,
			    Elf32_Sym *, Elf32_Word, const char *,
			    const char *);
#endif /* _ELF64 */

/*
 * These are used by both the Elf32 and Elf64 sides.
 */
extern	void		Gelf_phdr_entry(Half, GElf_Phdr *);
extern	void		Gelf_shdr_entry(Half, GElf_Shdr *);
extern	void		Gelf_sym_table_entry(const char *, GElf_Ehdr *,
			    GElf_Sym *, GElf_Word, const char *, const char *);
extern	void		Gelf_elf_data_title(void);
extern	void		Gelf_syminfo_entry(int, GElf_Syminfo *, const char *,
			    const char *);
extern	void		Gelf_sym_table_title(GElf_Ehdr *, const char *,
			    const char *);


#if	!defined(_ELF64)
extern	void		Gelf_dyn_print(GElf_Dyn *, int ndx, const char *, Half);
extern	void		Gelf_dyn_title(void);
extern	void		Gelf_elf_data(const char *, GElf_Addr, Elf_Data *,
			    const char *);
extern	void		Gelf_elf_header(GElf_Ehdr *);
extern	void		Gelf_got_title(GElf_Ehdr *, GElf_Word);
extern	void		Gelf_got_entry(GElf_Ehdr *, int, GElf_Addr, GElf_Xword,
			    GElf_Word, void *, const char *);
extern	void		Gelf_note_entry(GElf_Word *);
extern	void		Gelf_reloc_entry(const char *, GElf_Half, GElf_Word,
			    GElf_Rela *, const char *, const char *);
extern	void		Gelf_syminfo_entry_title(const char *);
extern	void		Gelf_ver_def_print(GElf_Verdef *, GElf_Xword,
			    const char *);
extern	void		Gelf_ver_need_print(GElf_Verneed *, GElf_Xword,
			    const char *);
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _DEBUG_H */
