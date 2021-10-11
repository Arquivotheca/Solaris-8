/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_ACT_H
#define	_ACPI_ACT_H

#pragma ident	"@(#)acpi_act.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* action declarations */


/*
 * lex rules
 */

extern int empty_lex(struct parse_state *pstp);

extern int pkg_length_lex(struct parse_state *pstp);
extern int pkg_narrow_lex(struct parse_state *pstp);
extern int eof_lex(struct parse_state *pstp);

extern int method_flags_lex(struct parse_state *pstp);
extern int system_level_lex(struct parse_state *pstp);
extern int pblock_length_lex(struct parse_state *pstp);
extern int field_flags_lex(struct parse_state *pstp);
extern int sync_level_lex(struct parse_state *pstp);
extern int region_space_lex(struct parse_state *pstp);
extern int match_rel_lex(struct parse_state *pstp);

extern int byte_data_lex(struct parse_state *pstp);
extern int word_data_lex(struct parse_state *pstp);
extern int dword_data_lex(struct parse_state *pstp);

extern int nameseg_lex(struct parse_state *pstp);
extern int name_string_lex(struct parse_state *pstp);

extern int name_string_reduce(int rflags, struct parse_state *pstp);

extern int string_lex(struct parse_state *pstp);
extern int skip_lex(struct parse_state *pstp);
extern int byte_list_lex(struct parse_state *pstp);

extern int device_scope(int rflags, struct parse_state *pstp);
extern int power_scope(int rflags, struct parse_state *pstp);
extern int proc_scope(int rflags, struct parse_state *pstp);
extern int thermal_scope(int rflags, struct parse_state *pstp);
extern int scope_scope(int rflags, struct parse_state *pstp);

extern int if_cond(int rflags, struct parse_state *pstp);
extern int while_cond(int rflags, struct parse_state *pstp);


/*
 * reduce actions
 */

extern int null_action(int rflags, struct parse_state *pstp);
extern int rhs1_reduce(int rflags, struct parse_state *pstp);
extern int rhs2_reduce(int rflags, struct parse_state *pstp);
extern int list_free(int rflags, struct parse_state *pstp);

/* data */
extern int zero_reduce(int rflags, struct parse_state *pstp);
extern int one_reduce(int rflags, struct parse_state *pstp);
extern int ones_reduce(int rflags, struct parse_state *pstp);
extern int revision_reduce(int rflags, struct parse_state *pstp);
extern int buffer_reduce(int rflags, struct parse_state *pstp);
extern int package_reduce(int rflags, struct parse_state *pstp);

/* buffer fields */
extern int create_field_reduce(int rflags, struct parse_state *pstp);
extern int bit_field_reduce(int rflags, struct parse_state *pstp);
extern int byte_field_reduce(int rflags, struct parse_state *pstp);
extern int word_field_reduce(int rflags, struct parse_state *pstp);
extern int dword_field_reduce(int rflags, struct parse_state *pstp);

/* fields */
extern int access_field_reduce(int rflags, struct parse_state *pstp);
extern int named_field_reduce(int rflags, struct parse_state *pstp);
extern int res_field_reduce(int rflags, struct parse_state *pstp);
extern int field_list_reduce(int rflags, struct parse_state *pstp);
extern int bank_field_reduce(int rflags, struct parse_state *pstp);
extern int field_reduce(int rflags, struct parse_state *pstp);
extern int index_field_reduce(int rflags, struct parse_state *pstp);

/* object declarations */
extern int device_reduce(int rflags, struct parse_state *pstp);
extern int event_reduce(int rflags, struct parse_state *pstp);
extern int method_reduce(int rflags, struct parse_state *pstp);
extern int mutex_reduce(int rflags, struct parse_state *pstp);
extern int power_reduce(int rflags, struct parse_state *pstp);
extern int proc_reduce(int rflags, struct parse_state *pstp);
extern int region_reduce(int rflags, struct parse_state *pstp);
extern int thermal_reduce(int rflags, struct parse_state *pstp);

/* name space modifiers */
extern int alias_reduce(int rflags, struct parse_state *pstp);
extern int name_reduce(int rflags, struct parse_state *pstp);
extern int scope_reduce(int rflags, struct parse_state *pstp);

/* execution */
extern int debug_reduce(int rflags, struct parse_state *pstp);

extern int local0_reduce(int rflags, struct parse_state *pstp);
extern int local1_reduce(int rflags, struct parse_state *pstp);
extern int local2_reduce(int rflags, struct parse_state *pstp);
extern int local3_reduce(int rflags, struct parse_state *pstp);
extern int local4_reduce(int rflags, struct parse_state *pstp);
extern int local5_reduce(int rflags, struct parse_state *pstp);
extern int local6_reduce(int rflags, struct parse_state *pstp);
extern int local7_reduce(int rflags, struct parse_state *pstp);

extern int arg0_reduce(int rflags, struct parse_state *pstp);
extern int arg1_reduce(int rflags, struct parse_state *pstp);
extern int arg2_reduce(int rflags, struct parse_state *pstp);
extern int arg3_reduce(int rflags, struct parse_state *pstp);
extern int arg4_reduce(int rflags, struct parse_state *pstp);
extern int arg5_reduce(int rflags, struct parse_state *pstp);
extern int arg6_reduce(int rflags, struct parse_state *pstp);

extern int method_call0_reduce(int rflags, struct parse_state *pstp);
extern int method_call1_reduce(int rflags, struct parse_state *pstp);
extern int method_call2_reduce(int rflags, struct parse_state *pstp);
extern int method_call3_reduce(int rflags, struct parse_state *pstp);
extern int method_call4_reduce(int rflags, struct parse_state *pstp);
extern int method_call5_reduce(int rflags, struct parse_state *pstp);
extern int method_call6_reduce(int rflags, struct parse_state *pstp);
extern int method_call7_reduce(int rflags, struct parse_state *pstp);

extern int method_body_reduce(int rflags, struct parse_state *pstp);

/* type 1 operators */
extern int break_reduce(int rflags, struct parse_state *pstp);
extern int breakpoint_reduce(int rflags, struct parse_state *pstp);
extern int fatal_reduce(int rflags, struct parse_state *pstp);
extern int else_reduce(int rflags, struct parse_state *pstp);
extern int else_clause_reduce(int rflags, struct parse_state *pstp);
extern int return_reduce(int rflags, struct parse_state *pstp);
extern int sleep_reduce(int rflags, struct parse_state *pstp);
extern int stall_reduce(int rflags, struct parse_state *pstp);
extern int while_reduce(int rflags, struct parse_state *pstp);

extern int load_reduce(int rflags, struct parse_state *pstp);
extern int notify_reduce(int rflags, struct parse_state *pstp);
extern int release_reduce(int rflags, struct parse_state *pstp);
extern int reset_reduce(int rflags, struct parse_state *pstp);
extern int signal_reduce(int rflags, struct parse_state *pstp);
extern int unload_reduce(int rflags, struct parse_state *pstp);

/* type 2 operators, package and buffer in data section */
extern int add_reduce(int rflags, struct parse_state *pstp);
extern int subtract_reduce(int rflags, struct parse_state *pstp);
extern int increment_reduce(int rflags, struct parse_state *pstp);
extern int decrement_reduce(int rflags, struct parse_state *pstp);
extern int divide_reduce(int rflags, struct parse_state *pstp);
extern int multiply_reduce(int rflags, struct parse_state *pstp);

extern int and_reduce(int rflags, struct parse_state *pstp);
extern int nand_reduce(int rflags, struct parse_state *pstp);
extern int or_reduce(int rflags, struct parse_state *pstp);
extern int nor_reduce(int rflags, struct parse_state *pstp);
extern int xor_reduce(int rflags, struct parse_state *pstp);
extern int not_reduce(int rflags, struct parse_state *pstp);
extern int shift_left_reduce(int rflags, struct parse_state *pstp);
extern int shift_right_reduce(int rflags, struct parse_state *pstp);
extern int left_bit_reduce(int rflags, struct parse_state *pstp);
extern int right_bit_reduce(int rflags, struct parse_state *pstp);

extern int land_reduce(int rflags, struct parse_state *pstp);
extern int lnand_reduce(int rflags, struct parse_state *pstp);
extern int lor_reduce(int rflags, struct parse_state *pstp);
extern int lnor_reduce(int rflags, struct parse_state *pstp);
extern int lnot_reduce(int rflags, struct parse_state *pstp);
extern int lnz_reduce(int rflags, struct parse_state *pstp);
extern int leq_reduce(int rflags, struct parse_state *pstp);
extern int lne_reduce(int rflags, struct parse_state *pstp);
extern int lgt_reduce(int rflags, struct parse_state *pstp);
extern int lle_reduce(int rflags, struct parse_state *pstp);
extern int llt_reduce(int rflags, struct parse_state *pstp);
extern int lge_reduce(int rflags, struct parse_state *pstp);

extern int cond_ref_reduce(int rflags, struct parse_state *pstp);
extern int deref_reduce(int rflags, struct parse_state *pstp);
extern int ref_reduce(int rflags, struct parse_state *pstp);

extern int index_reduce(int rflags, struct parse_state *pstp);
extern int match_reduce(int rflags, struct parse_state *pstp);

extern int from_bcd_reduce(int rflags, struct parse_state *pstp);
extern int to_bcd_reduce(int rflags, struct parse_state *pstp);

extern int acquire_reduce(int rflags, struct parse_state *pstp);
extern int concat_reduce(int rflags, struct parse_state *pstp);
extern int sizeof_reduce(int rflags, struct parse_state *pstp);
extern int store_reduce(int rflags, struct parse_state *pstp);
extern int type_reduce(int rflags, struct parse_state *pstp);
extern int wait_reduce(int rflags, struct parse_state *pstp);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_ACT_H */
